/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "nspr.h"
#include "nsPSMShimLayer.h"

#ifdef XP_UNIX
#include <sys/stat.h>
#include <unistd.h>
#include "private/pprio.h"  /* for PR_Socket */
#endif

#define PSM_TIMEOUT_IN_SEC 300

#define NSPSMSHIMMAXFD 50


static PRIntervalTime gTimeout  = PR_INTERVAL_NO_TIMEOUT;

CMT_SocketFuncs nsPSMShimTbl =
{
    nsPSMShimGetSocket,
    nsPSMShimConnect,
    nsPSMShimVerifyUnixSocket,
    nsPSMShimSend,
    nsPSMShimSelect,
    nsPSMShimReceive,
    nsPSMShimShutdown,
    nsPSMShimClose
};


CMTSocket
nsPSMShimGetSocket(int unixSock)
{
    PRStatus   rv;
    PRFileDesc *fd;
    CMSocket   *sock;
    PRSocketOptionData sockopt;

    /*
    if (PR_INTERVAL_NO_WAIT == gTimeout) 
    {
        gTimeout  = PR_SecondsToInterval(PSM_TIMEOUT_IN_SEC);
    }
    */

    if (unixSock)
    {
#ifndef XP_UNIX        
        return NULL;
#else
        fd = PR_Socket(PR_AF_LOCAL, PR_SOCK_STREAM, 0);
        PR_ASSERT(fd);
#endif
    }
    else
    {
        fd = PR_NewTCPSocket();
        PR_ASSERT(fd);

        /* disable Nagle algorithm delay for control sockets */
        sockopt.option = PR_SockOpt_NoDelay;
        sockopt.value.no_delay = PR_TRUE;   
        rv = PR_SetSocketOption(fd, &sockopt);
        PR_ASSERT(PR_SUCCESS == rv);
    }

    sock = (CMSocket *)PR_Malloc(sizeof(CMSocket));

    if (sock == NULL)
      return sock;
    
    sock->fd     = fd;
    sock->isUnix = unixSock;

    memset(&sock->netAddr, 0, sizeof(PRNetAddr));
    
    return (CMTSocket)sock;
}

CMTStatus
nsPSMShimConnect(CMTSocket sock, short port, char *path)
{
    CMTStatus   rv = CMTSuccess;
    PRStatus    err;
    PRErrorCode errcode;
    PRSocketOptionData   sockopt;
    PRBool      nonBlocking;
    CMSocket    *cmSock = (CMSocket *)sock;
    
    if (!sock) return CMTFailure;

    if (cmSock->isUnix)
    {
#ifndef XP_UNIX
        return CMTFailure;
#else
        int pathLen;
        if (!path)
        {
            return CMTFailure;
        }

        /* check buffer overrun */
        pathLen = strlen(path)+1;
        
        pathLen = pathLen < sizeof(cmSock->netAddr.local.path)
                ? pathLen : sizeof(cmSock->netAddr.local.path);
  
        memcpy(&cmSock->netAddr.local.path, path, pathLen);
        cmSock->netAddr.local.family = PR_AF_LOCAL;
#endif
    }
    else  /* cmSock->isUnix */
    {
        cmSock->netAddr.inet.family = PR_AF_INET;
        cmSock->netAddr.inet.port   = PR_htons(port);
        cmSock->netAddr.inet.ip     = PR_htonl(PR_INADDR_LOOPBACK);
    }

    /* Save non-blocking status */
    sockopt.option = PR_SockOpt_Nonblocking;
    err = PR_GetSocketOption(cmSock->fd, &sockopt);
    PR_ASSERT(PR_SUCCESS == err);

    nonBlocking = sockopt.value.non_blocking;

    /* make connect blocking for now */
    sockopt.option = PR_SockOpt_Nonblocking;
    sockopt.value.non_blocking = PR_FALSE;   
    err = PR_SetSocketOption(cmSock->fd, &sockopt);
    PR_ASSERT(PR_SUCCESS == err);

    err = PR_Connect( cmSock->fd, &cmSock->netAddr, PR_INTERVAL_MAX );
    
    if (err == PR_FAILURE)    
    {
        errcode = PR_GetError();
        
        if (PR_IS_CONNECTED_ERROR != errcode)
            rv = CMTFailure;
    }    
    
    /* restore nonblock status */
    if (nonBlocking) {
      sockopt.option = PR_SockOpt_Nonblocking;
      sockopt.value.non_blocking = nonBlocking;  
      err = PR_SetSocketOption(cmSock->fd, &sockopt);
      PR_ASSERT(PR_SUCCESS == err);
    }

    return rv;
}

CMTStatus
nsPSMShimVerifyUnixSocket(CMTSocket sock)
{
#ifndef XP_UNIX
    return CMTFailure;
#else

    int  rv;
    CMSocket  *cmSock;
    struct stat statbuf;

    cmSock = (CMSocket *)sock;
        
    if (!cmSock || !cmSock->isUnix) 
        return CMTFailure;
 
    rv = stat(cmSock->netAddr.local.path, &statbuf);
    if (rv < 0 || statbuf.st_uid != geteuid() )
    {
        PR_Close(cmSock->fd);
        cmSock->fd = NULL;
        PR_Free(cmSock);
        return CMTFailure;
    }
    return CMTSuccess;   
#endif
}

CMInt32
nsPSMShimSend(CMTSocket sock, void *buffer, size_t length)
{
    CMSocket *cmSock = (CMSocket *)sock;

    if (!sock) return CMTFailure;
    
   return PR_Send(cmSock->fd, buffer, length, 0, gTimeout);
}

CMInt32
nsPSMShimReceive(CMTSocket sock, void *buffer, size_t bufSize)
{
    CMSocket *cmSock = (CMSocket *)sock;
    
    if (!sock) return CMTFailure;

    return PR_Recv(cmSock->fd, buffer, bufSize, 0, gTimeout);
}


CMTSocket
nsPSMShimSelect(CMTSocket *socks, int numsocks, int poll)
{
    CMSocket       **sockArr = (CMSocket **)socks;
    PRPollDesc       readPDs[NSPSMSHIMMAXFD];
    PRIntervalTime   timeout;
    PRInt32          cnt;
    int              i;

    if (!socks) return NULL;

    memset(readPDs, 0, sizeof(readPDs));

    PR_ASSERT(NSPSMSHIMMAXFD >= numsocks);

    for (i=0; i<numsocks; i++)
    {
        readPDs[i].fd       = sockArr[i]->fd;
        readPDs[i].in_flags = PR_POLL_READ;
    }

    timeout = poll ? PR_INTERVAL_NO_WAIT : PR_INTERVAL_NO_TIMEOUT;

    cnt = PR_Poll(readPDs, numsocks, timeout);

    /* Figure out which socket was selected */
    if (cnt > 0)
    {
        for (i=0; i<numsocks; i++)
        {
            if (readPDs[i].out_flags & PR_POLL_READ)
            {
            return (CMTSocket)sockArr[i];
            }
        }
    }
    return NULL;
}


CMTStatus
nsPSMShimShutdown(CMTSocket sock)
{
    CMSocket *cmSock = (CMSocket*)sock;
    PRStatus rv;

    if (!sock) return CMTFailure;

    rv = PR_Shutdown(cmSock->fd, PR_SHUTDOWN_SEND);
    return (PR_SUCCESS == rv) ? CMTSuccess : CMTFailure;
}

CMTStatus
nsPSMShimClose(CMTSocket sock)
{
    CMSocket *cmSock = (CMSocket*)sock;
    PRStatus rv      = PR_SUCCESS;
    PR_ASSERT(cmSock);

    if (!sock) return CMTFailure;

    rv = PR_Close(cmSock->fd);
    cmSock->fd = NULL;

    PR_Free(cmSock);

    return (PR_SUCCESS == rv) ? CMTSuccess : CMTFailure;
}
