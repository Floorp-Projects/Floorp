/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __XP_SOCK_h_
#define __XP_SOCK_h_

#include "xp_core.h"
#include "xp_error.h"

#ifdef XP_UNIX

#ifdef AIXV3
#include <sys/signal.h>
#include <sys/select.h>
#endif /* AIXV3 */

#include <sys/types.h>
#include <string.h>

#include <errno.h>   
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifndef __hpux 
#include <arpa/inet.h> 
#endif /* __hpux */
#include <netdb.h>
#endif /* XP_UNIX */

#ifdef XP_MAC
#include "macsocket.h"
#define SOCKET_BUFFER_SIZE 4096
#endif /* XP_MAC */

#ifdef XP_OS2 /* IBM-VPB050196 */
#	include "os2sock.h"
#	ifdef XP_OS2_DOUGSOCK
#		include "dsfunc.h"
#	endif
#	define SOCKET_BUFFER_SIZE 4096
#endif

#ifdef XP_WIN
#include "winsock.h"
#define SOCKET_BUFFER_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

extern int dupsocket(int foo); /* always fails */

#ifdef __cplusplus
}
#endif

#ifndef EPIPE
#define EPIPE ECONNRESET
#endif

#undef BOOLEAN
#define BOOLEAN char
#endif /* XP_WIN */

#ifndef SOCKET_ERRNO
#define SOCKET_ERRNO    XP_GetError()
#endif

/************************************************************************/

#ifdef XP_UNIX

/* Network i/o wrappers */
#define XP_SOCKET int
#define XP_SOCK_ERRNO errno
#define XP_SOCK_SOCKET socket
#define XP_SOCK_CONNECT connect
#define XP_SOCK_ACCEPT accept
#define XP_SOCK_BIND bind
#define XP_SOCK_LISTEN listen
#define XP_SOCK_SHUTDOWN shutdown
#define XP_SOCK_IOCTL ioctl
#define XP_SOCK_RECV recv
#define XP_SOCK_RECVFROM recvfrom
#define XP_SOCK_RECVMSG recvmsg
#define XP_SOCK_SEND send
#define XP_SOCK_SENDTO sendto
#define XP_SOCK_SENDMSG sendmsg
#define XP_SOCK_READ read
#define XP_SOCK_WRITE write
#define XP_SOCK_READV readv
#define XP_SOCK_WRITEV writev
#define XP_SOCK_GETPEERNAME getpeername
#define XP_SOCK_GETSOCKNAME getsockname
#define XP_SOCK_GETSOCKOPT getsockopt
#define XP_SOCK_SETSOCKOPT setsockopt
#define XP_SOCK_CLOSE close
#define XP_SOCK_DUP dup

#endif /* XP_UNIX */

/*IBM-DSR072296 - now using WinSock 1.1 support in OS/2 Merlin instead of DOUGSOCK...*/
#if defined(XP_WIN) || ( defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK) )
#define XP_SOCKET SOCKET
#define XP_SOCK_ERRNO WSAGetLastError()

#define XP_SOCK_SOCKET socket
#define XP_SOCK_CONNECT connect
#define XP_SOCK_ACCEPT accept
#define XP_SOCK_BIND bind
#define XP_SOCK_LISTEN listen
#define XP_SOCK_SHUTDOWN shutdown
#define XP_SOCK_IOCTL ioctlsocket
#define XP_SOCK_RECV recv
#define XP_SOCK_RECVFROM recvfrom
#define XP_SOCK_RECVMSG recvmsg
#define XP_SOCK_SEND send
#define XP_SOCK_SENDTO sendto
#define XP_SOCK_SENDMSG sendmsg
#define XP_SOCK_READ(s,b,l) recv(s,b,l,0)
#define XP_SOCK_WRITE(s,b,l) send(s,b,l,0)
#define XP_SOCK_READV readv
#define XP_SOCK_WRITEV writev
#define XP_SOCK_GETPEERNAME getpeername
#define XP_SOCK_GETSOCKNAME getsockname
#define XP_SOCK_GETSOCKOPT getsockopt
#define XP_SOCK_SETSOCKOPT setsockopt
#define XP_SOCK_CLOSE closesocket
#define XP_SOCK_DUP dupsocket

#endif /* XP_WIN/ XP_OS2 && not DOUGSOCK */

#if defined(XP_OS2) && defined(XP_OS2_DOUGSOCK)

/* Network i/o wrappers */
#define XP_SOCKET int
#define XP_SOCK_ERRNO sock_errno()
#define XP_SOCK_SOCKET socket
#define XP_SOCK_CONNECT connect
#define XP_SOCK_ACCEPT accept
#define XP_SOCK_BIND bind
#define XP_SOCK_LISTEN listen
#define XP_SOCK_SHUTDOWN shutdown
#define XP_SOCK_IOCTL ioctl
#define XP_SOCK_RECV receiveAndMakeReadSocketActive
#define XP_SOCK_RECVFROM recvfrom
#define XP_SOCK_RECVMSG recvmsg
#define XP_SOCK_SEND send
#define XP_SOCK_SENDTO sendto
#define XP_SOCK_SENDMSG sendmsg
#define XP_SOCK_READ(s,b,l) receiveAndMakeReadSocketActive(s,b,l,0)
#define XP_SOCK_WRITE(s,b,l) send(s,b,l,0)
#define XP_SOCK_READV readv
#define XP_SOCK_WRITEV writev
#define XP_SOCK_GETPEERNAME getpeername
#define XP_SOCK_GETSOCKNAME getsockname
#define XP_SOCK_GETSOCKOPT getsockopt
#define XP_SOCK_SETSOCKOPT setsockopt
#define XP_SOCK_CLOSE closeAndRemoveSocketFromPostList
#define XP_SOCK_DUP dupsocket 

#endif /*XP_OS2 with DOUGSOCK*/ 

#ifdef XP_MAC
/*
	Remap unix sockets into GUSI
*/
#define XP_SOCKET int
#define XP_SOCK_ERRNO errno
#define XP_SOCK_SOCKET macsock_socket
#define XP_SOCK_CONNECT macsock_connect
#define XP_SOCK_ACCEPT macsock_accept
#define XP_SOCK_BIND macsock_bind
#define XP_SOCK_LISTEN macsock_listen
#define XP_SOCK_SHUTDOWN macsock_shutdown
#define XP_SOCK_IOCTL macsock_ioctl
#define XP_SOCK_RECV(s,b,l,f) XP_SOCK_READ(s,b,l)
#define XP_SOCK_SEND(s,b,l,f) XP_SOCK_WRITE(s,b,l)
#define XP_SOCK_READ macsock_read
#define XP_SOCK_WRITE macsock_write
#define XP_SOCK_GETPEERNAME macsock_getpeername
#define XP_SOCK_GETSOCKNAME macsock_getsockname
#define XP_SOCK_GETSOCKOPT macsock_getsockopt
#define XP_SOCK_SETSOCKOPT macsock_setsockopt
#define XP_SOCK_CLOSE macsock_close
#define XP_SOCK_DUP macsock_dup

#endif /* XP_MAC */

#endif /* __XP_SOCK_h_ */
