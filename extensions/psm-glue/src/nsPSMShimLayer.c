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
#endif


#define NSPSMSHIMMAXFD 50

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


    if (unixSock)
    {
#ifndef XP_UNIX        
        return NULL;
#else
        fd = PR_OpenTCPSocket(AF_UNIX);
        PR_ASSERT(fd);
#endif
    }
    else
    {
        PRSocketOptionData sockopt;

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
    PRStatus    err;
    PRErrorCode errcode;
    CMTStatus   rv = CMTSuccess;
    CMSocket    *cmSock = (CMSocket *)sock;

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

    err = PR_Connect( cmSock->fd, &cmSock->netAddr, PR_INTERVAL_MAX );
    
    if (err == PR_FAILURE)    
    {
        errcode = PR_GetError();
        
        /* TODO: verify PR_INVALID_ARGUMENT_ERROR continue with connect */
        
        switch (errcode)
        {
            case PR_IS_CONNECTED_ERROR:
                rv = CMTSuccess;
                break;

            case PR_IN_PROGRESS_ERROR:
            case PR_IO_TIMEOUT_ERROR:
#ifdef WIN32
            case PR_WOULD_BLOCK_ERROR:            
            case PR_INVALID_ARGUMENT_ERROR:
#endif
            default:
                rv = CMTFailure;
                break;
        }
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
    CMSocket  *cmSock = (CMSocket *)sock;
    struct stat statbuf;

    if (!cmSock->isUnix) 
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

size_t
nsPSMShimSend(CMTSocket sock, void *buffer, size_t length)
{
    PRInt32   total;
    CMSocket *cmSock = (CMSocket *)sock;

    total = PR_Send(cmSock->fd, buffer, length, 0, PR_INTERVAL_NO_TIMEOUT);

    /* TODO: for now, return 0 if there's an error */
    return (total < 0) ? 0 : total;
}


CMTSocket
nsPSMShimSelect(CMTSocket *socks, int numsocks, int poll)
{
    CMSocket       **sockArr = (CMSocket **)socks;
    PRPollDesc       readPDs[NSPSMSHIMMAXFD];
    PRIntervalTime   timeout;
    PRInt32          cnt;
    int              i;

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

size_t
nsPSMShimReceive(CMTSocket sock, void *buffer, size_t bufSize)
{
    PRInt32   total;
    CMSocket *cmSock = (CMSocket *)sock;

    total = PR_Recv(cmSock->fd, buffer, bufSize, 0, PR_INTERVAL_NO_TIMEOUT);

    /* TODO: for now, return 0 if there's an error */
    return (total < 0) ? 0 : total;
}

CMTStatus
nsPSMShimShutdown(CMTSocket sock)
{
    CMSocket *cmSock = (CMSocket*)sock;
    PRStatus rv = PR_Shutdown(cmSock->fd, PR_SHUTDOWN_SEND);
    return (PR_SUCCESS == rv) ? CMTSuccess : CMTFailure;
}

CMTStatus
nsPSMShimClose(CMTSocket sock)
{
    CMSocket *cmSock = (CMSocket*)sock;
    PRStatus rv      = PR_SUCCESS;
    PR_ASSERT(cmSock);

    rv = PR_Close(cmSock->fd);
    cmSock->fd = NULL;

    /* TODO: release ref on control connection */
    PR_Free(cmSock);

    return (PR_SUCCESS == rv) ? CMTSuccess : CMTFailure;
}
