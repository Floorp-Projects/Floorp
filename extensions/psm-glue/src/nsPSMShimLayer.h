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

#ifndef _NSPSMSHIMLAYER_H_
#define _NSPSMSHIMLAYER_H_

#include "cmtcmn.h"
#include "prio.h"

PR_BEGIN_EXTERN_C

typedef struct CMSocket {
    PRFileDesc *fd;
    PRBool      isUnix;
    PRNetAddr   netAddr;
} CMSocket; 

PR_EXTERN(CMT_SocketFuncs) nsPSMShimTbl;

PR_EXTERN(CMTSocket)
nsPSMShimGetSocket(int unixSock);

PR_EXTERN(CMTStatus)
nsPSMShimConnect(CMTSocket sock, short port, char *path);

PR_EXTERN(CMTStatus)
nsPSMShimVerifyUnixSocket(CMTSocket sock);

PR_EXTERN(CMInt32)
nsPSMShimSend(CMTSocket sock, void *buffer, size_t length);

PR_EXTERN(CMTSocket)
nsPSMShimSelect(CMTSocket *socks, int numsocks, int poll);

PR_EXTERN(CMInt32)
nsPSMShimReceive(CMTSocket sock, void *buffer, size_t bufSize);

PR_EXTERN(CMTStatus)
nsPSMShimShutdown(CMTSocket sock);

PR_EXTERN(CMTStatus)
nsPSMShimClose(CMTSocket sock);

PR_END_EXTERN_C

#endif /* _NSPSMSHIMLAYER_H_ */
