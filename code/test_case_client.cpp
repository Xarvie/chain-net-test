#include "SystemReader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#define FD_SETSIZE  1024
#include<WinSock2.h>
#include <MSWSock.h>
#include<Windows.h>

#else
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#endif

#include <thread>
#include <vector>
#include <chrono>

#define NUM 8

#include <mutex>


std::mutex lock;

#define MAXFD 50

std::vector<std::thread> threads;

static int closeSocket(uint64_t fd) {
#ifdef OS_WINDOWS
    return closesocket(fd);
#else
    return close((int)fd);
#endif
}


inline int getSockError() {
#ifdef WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}


inline int IsEagain() {
    int err = getSockError();
#if defined(OS_WINDOWS)
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == WSAEWOULDBLOCK)
        return 1;
#endif
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
        return 1;
    return 0;
}

int main() {
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

#else
    signal(SIGPIPE, SIG_IGN);
#endif

    for (int x = 0; x < 20; x++) {
        threads.emplace_back([] {
            int ret = 0;
            uint64_t sock[MAXFD];
            for (int i = 0; i < MAXFD; i++) {
                sock[i] = socket(AF_INET, SOCK_STREAM, 0);
                if (sock[i] <= 0) {
                    std::cout << "socket err" << std::endl;
                }
                int reuse = 1;
                setsockopt(sock[i], SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(reuse));


#if defined(OS_WINDOWS)
                char nodelay = 1;
#else
                int nodelay = 1;
#endif
                if (setsockopt(sock[i], IPPROTO_TCP, TCP_NODELAY, &nodelay,
                               sizeof(nodelay)) < 0)
                    printf("err: nodelay");
#if defined(OS_WINDOWS)
                //unsigned long ul = 1;
                //ret = ioctlsocket(sock[i], FIONBIO, (unsigned long *) &ul);
                //if (ret == SOCKET_ERROR)
                //    printf("err: ioctlsocket");
#else
                //int flags = fcntl(sock[i], F_GETFL, 0);
                //if (flags < 0) printf("err: fcntl");

                //ret = fcntl(sock[i], F_SETFL, flags | O_NONBLOCK);
                //if (ret < 0) printf("err: fcntl");
#endif

                struct sockaddr_in srvAddr;
                srvAddr.sin_family = AF_INET;
                srvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
                srvAddr.sin_port = htons(9876);

                ret = connect(sock[i], (struct sockaddr *) &srvAddr, sizeof(srvAddr));

            }
            char hello[] = "\n";
            for (int i = 0; i < MAXFD;) {
                int ret = send(sock[i], hello, 1, 0);
                if (ret != 1) {
                    if (IsEagain()) {
                        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    std::cout << "a-" << getSockError() << std::endl;
                    closeSocket(sock[i]);
                    sock[i] = 0;
                }
                i++;
            }


            while (true) {
                fd_set fdRead;
                fd_set fdWrite;
                fd_set fdExp;


                FD_ZERO(&fdRead);
                FD_ZERO(&fdWrite);
                FD_ZERO(&fdExp);
                uint64_t maxSock = 0;

                for (int i = 0; i < MAXFD; i++) {
                    if (sock[i] == 0)
                        continue;
                    FD_SET(sock[i], &fdRead);
                    //FD_SET(sock[i], &fdWrite);
                    if (maxSock < sock[i]) {
                        maxSock = sock[i];
                    }
                }
                char buf[300];
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                int ret = select((int) maxSock + 1, &fdRead, &fdWrite, &fdExp, NULL);
                if (ret > 0) {
                    for (int i = 0; i < MAXFD; i++) {
                        uint64_t fd = sock[i];
                        if (FD_ISSET(fd, &fdRead)) {
                            int ret = recv(fd, buf, 1, MSG_WAITALL);

                            if (ret < 1) {
                                if (IsEagain()) {
                                    continue;
                                } else {

                                    std::cout << "b-" << getSockError() << std::endl;
                                    closeSocket(sock[i]);
                                    sock[i] = 0;
                                }
                            }
                            if (1 > send(fd, hello, 1, 0)) {
                                if (IsEagain()) {
                                    continue;
                                } else {

                                    std::cout << "c-" << getSockError() << std::endl;
                                    closeSocket(sock[i]);
                                    sock[i] = 0;
                                }
                            }


                        }
                    }
                }
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(1100000));

    return 0;
}