/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef prpcos_h___
#define prpcos_h___
/*
 * PC definitions peculiar to Windows OS variants.
 */

#ifdef XP_PC

#include "prtypes.h"

#include <windows.h>
#include <setjmp.h>
#include <stdlib.h>

#define USE_SETJMP
#define DIRECTORY_SEPARATOR         '\\'
#define DIRECTORY_SEPARATOR_STR     "\\"
#define PATH_SEPARATOR              ';'

#ifdef _WIN32
#define GCPTR
#else
#define GCPTR __far
#endif

/*
** Routines for processing command line arguments
*/
PR_BEGIN_EXTERN_C

extern char *optarg;
extern int optind, opterr, optopt;
extern int getopt(int argc, char **argv, char *spec);

PR_END_EXTERN_C

#define gcmemcpy(a,b,c) memcpy(a,b,c)


/*
** Definitions of directory structures amd functions
** These definitions are from:
**      <dirent.h>
*/
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>          /* O_BINARY */


#ifndef _WIN32
#include <stdio.h>
#ifndef __WATCOMC__
#include <winsock.h>
#endif /* __WATCOMC__ */

#if !defined(stderr)
extern FILE *stderr;
#endif

PR_BEGIN_EXTERN_C

/*
** The following RTL routines are completely missing from WIN16
*/
extern int  printf(const char *, ...);
extern void perror(const char*);

/*
** The following RTL routines are unavailable for WIN16 DLLs
*/
#ifdef _WINDLL

extern void * malloc(size_t);
extern void * realloc(void*, size_t);
extern void * calloc(size_t, size_t);
extern void   free(void*);

/* XXX:  Need to include all of the winsock calls as well... */
extern void   exit(int);
extern size_t strftime(char *, size_t, const char *, const struct tm *);
extern int    sscanf(const char *, const char *, ...);

#endif /* _WINDLL */

PR_END_EXTERN_C
#endif /* ! _WIN32 */


typedef int PROSFD;

PR_BEGIN_EXTERN_C

#ifndef __WATCOMC__
#if defined(XP_PC) && !defined(_WIN32)

struct PRMethodCallbackStr {
    void*   (PR_CALLBACK_DECL *malloc) (size_t);
    void*   (PR_CALLBACK_DECL *realloc)(void *, size_t);
    void*   (PR_CALLBACK_DECL *calloc) (size_t, size_t);
    void    (PR_CALLBACK_DECL *free)   (void *);
    int     (PR_CALLBACK_DECL *gethostname)(char * name, int namelen);
    struct hostent * (PR_CALLBACK_DECL *gethostbyname)(const char * name);
    struct hostent * (PR_CALLBACK_DECL *gethostbyaddr)(const char * addr, int len, int type);
    char*   (PR_CALLBACK_DECL *getenv)(const char *varname);
    int     (PR_CALLBACK_DECL *putenv)(const char *assoc);
    int     (PR_CALLBACK_DECL *auxOutput)(const char *outputString);
    void    (PR_CALLBACK_DECL *exit)(int status);
    size_t  (PR_CALLBACK_DECL *strftime)(char *s, size_t len, const char *fmt, const struct tm *p);

    u_long  (PR_CALLBACK_DECL *ntohl)       (u_long netlong);
    u_short (PR_CALLBACK_DECL *ntohs)       (u_short netshort);
    int     (PR_CALLBACK_DECL *closesocket) (SOCKET s);
    int     (PR_CALLBACK_DECL *setsockopt)  (SOCKET s, int level, int optname, const char FAR * optval, int optlen);
    SOCKET  (PR_CALLBACK_DECL *socket)      (int af, int type, int protocol);
    int     (PR_CALLBACK_DECL *getsockname) (SOCKET s, struct sockaddr FAR *name, int FAR * namelen);
    u_long  (PR_CALLBACK_DECL *htonl)       (u_long hostlong);
    u_short (PR_CALLBACK_DECL *htons)       (u_short hostshort);
    unsigned long (PR_CALLBACK_DECL *inet_addr) (const char FAR * cp);
    int     (PR_CALLBACK_DECL *WSAGetLastError)(void);
    int     (PR_CALLBACK_DECL *connect)     (SOCKET s, const struct sockaddr FAR *name, int namelen);
    int     (PR_CALLBACK_DECL *recv)        (SOCKET s, char FAR * buf, int len, int flags);
    int     (PR_CALLBACK_DECL *ioctlsocket) (SOCKET s, long cmd, u_long FAR *argp);
    int     (PR_CALLBACK_DECL *recvfrom)    (SOCKET s, char FAR * buf, int len, int flags, struct sockaddr FAR *from, int FAR * fromlen);
    int     (PR_CALLBACK_DECL *send)        (SOCKET s, const char FAR * buf, int len, int flags);
    int     (PR_CALLBACK_DECL *sendto)      (SOCKET s, const char FAR * buf, int len, int flags, const struct sockaddr FAR *to, int tolen);
    SOCKET  (PR_CALLBACK_DECL *accept)      (SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen);
    int     (PR_CALLBACK_DECL *listen)      (SOCKET s, int backlog);
    int     (PR_CALLBACK_DECL *bind)        (SOCKET s, const struct sockaddr FAR *addr, int namelen);
};

#endif

extern PR_PUBLIC_API(void) PR_MDInit(struct PRMethodCallbackStr *);

#endif /* __WATCOMC__ */

PR_END_EXTERN_C

#endif /* XP_PC */
#endif /* prpcos_h___ */
