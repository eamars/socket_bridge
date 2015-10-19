//
//  main.c
//  SocketBridge
//
//  Created by Ran Bao on 19/10/15.
//  Copyright © 2015 Ran Bao. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#define MAX_DATA_SZ 4096

#define handle_error_en(en, msg) \
do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct
{
    int client_to_server;
    int server_to_client;
    
}Connection;

void print_peer_information(int s)
{
    socklen_t len;
    struct sockaddr_storage addr;
    char ipstr[INET6_ADDRSTRLEN];
    int port;
    
    len = sizeof addr;
    getpeername(s, (struct sockaddr*)&addr, &len);
    
    // deal with both IPv4 and IPv6:
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } else { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }
    
    printf("Client: %s connected\n", ipstr);
}


int connect_to(const char *hostname, const char *port)
{
    struct addrinfo their_addrinfo; // server address info
    struct addrinfo *their_addr = NULL; // connector's address information
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    memset(&their_addrinfo, 0, sizeof(struct addrinfo));
    their_addrinfo.ai_family = AF_INET;        /* use an internet address */
    their_addrinfo.ai_socktype = SOCK_STREAM;  /* use TCP rather than datagram */
    
    getaddrinfo( hostname, port, &their_addrinfo, &their_addr);/* get IP info */
    
    int rc = connect(sockfd, their_addr->ai_addr, their_addr->ai_addrlen); //connect to server
    
    // free allocated memory
    free(their_addr);
    
    if(rc == -1) {
        handle_error("connect");
    }
    
    return sockfd;
}

int listen_on(int port)
{
    int rc;
    int sock_fd;
    struct sockaddr_in sa;
    
    // sock_fd is set to block
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(port);
    
    rc = bind(sock_fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc == -1)
    {
        handle_error("bind");
    }
    
    rc = listen(sock_fd, 5);
    if (rc == -1)
    {
        handle_error("listen");
    }
    
    return sock_fd;
}

int accept_connection(int s)
{
    int client_fd;
    struct sockaddr_in client_sock;
    socklen_t sz = sizeof(struct sockaddr_in);
    
    client_fd = accept(s, (struct sockaddr *)&client_sock, &sz);
    
    if (client_fd == -1)
    {
        handle_error("accept");
    }
    
    return client_fd;
    
}

void *handle_server_to_client(void *args)
{
    Connection *conn;
    char buffer[MAX_DATA_SZ];
    ssize_t sz;
    
    conn = (Connection *) args;
    memset(buffer, 0, MAX_DATA_SZ);
    
    while((sz = read(conn->server_to_client, buffer, MAX_DATA_SZ)) != 0)
    {
        // read message from server
        if (sz < 0)
        {
            handle_error("read from server to client");
        }
        else
        {
            // printf("Server: [%.*s]\n", (int)sz ,buffer);
            write(conn->client_to_server, buffer, sz);
        }
        memset(buffer, 0, MAX_DATA_SZ);
    }
    
    close(conn->server_to_client);
    
    return NULL;
}

void *handle_client_to_server(void *args)
{
    Connection *conn;
    char buffer[MAX_DATA_SZ];
    ssize_t sz;
    
    conn = (Connection *) args;
    memset(buffer, 0, MAX_DATA_SZ);
    
    while((sz = read(conn->client_to_server, buffer, MAX_DATA_SZ)) != 0)
    {
        // read message from server
        if (sz < 0)
        {
            handle_error("read from server to client");
        }
        else
        {
            // printf("Client: [%.*s]\n", (int)sz ,buffer);
            write(conn->server_to_client, buffer, sz);
        }
        memset(buffer, 0, MAX_DATA_SZ);
    }
    
    close(conn->client_to_server);
    
    return NULL;
}

void handle_request(int s, const char **argv)
{
    Connection conn;
    pthread_t threads[2];
    
    // assign fds
    conn.client_to_server = s;
    conn.server_to_client = connect_to(argv[2], argv[3]);
    
    // print client info
    print_peer_information(s);
    
    // create threads to handle each connections
    pthread_create(&threads[0], NULL, handle_client_to_server, &conn);
    pthread_create(&threads[1], NULL, handle_server_to_client, &conn);
    
    // wait for all threads terminates
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    return;
}

int main(int argc, const char * argv[])
{
    int server_fd;
    int client_fd;
    int pid;
    
    if (argc != 4)
    {
        printf("Usage: SocketBridge listen_port dest_addr dest_port");
        handle_error_en(1, "argc");
    }
    
    server_fd = listen_on(atoi(argv[1]));

    while (1)
    {
        client_fd = accept_connection(server_fd);
        
        pid = fork();
        if (pid < 0)
        {
            handle_error("fork");
        }
        else if (pid == 0)
        {
            handle_request(client_fd, argv);
        }
        else
        {
            close(client_fd);
        }
    }
}