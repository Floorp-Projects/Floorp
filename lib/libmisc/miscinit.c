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

#include "sec.h"
#include "ssl.h"
#ifdef SOURCE_KIT
#include "sslimpl.h"
#else /* SOURCE_KIT */
#include "impl.h"
#endif /* SOURCE_KIT */
#include "nspr.h"
#ifdef SOURCE_KIT
#ifdef FORTEZZA
/* Sigh for FortezzaGlobalinit() */
#include "fortezza.h"
#endif /* FORTEZZA */
#endif /* SOURCE_KIT */

extern int XP_ERRNO_EWOULDBLOCK;

static int sec_inited = 0;
void SEC_Init(void)
{
    /* PR_Init() must be called before SEC_Init() */
#if !defined(SERVER_BUILD)
    XP_ASSERT(PR_Initialized() == PR_TRUE);
#endif
    if (sec_inited)
	return;

    SEC_RNGInit();
#ifdef SOURCE_KIT
    SSL_InitHashLock();
    SSL3_Init();
#ifdef FORTEZZA
    FortezzaGlobalInit();
#endif /* FORTEZZA */
#endif /* SOURCE_KIT */
    sec_inited = 1;
}

#ifdef SOURCE_KIT

static PRIntervalTime sec_io_timeout = PR_INTERVAL_NO_TIMEOUT;

void SEC_Set_IO_Timeout(PRUint32 timeout)
{
    sec_io_timeout = PR_SecondsToInterval(timeout);
}

static void SEC_Map_Error()
{
    int err, block;

    err = _PR_MD_GET_SOCKET_ERROR();
    block = ((err == _MD_EWOULDBLOCK) || (err == _MD_EAGAIN) ||
             (sec_io_timeout == PR_INTERVAL_NO_WAIT && err == _MD_ETIMEDOUT));
    if (block)
	err = XP_ERRNO_EWOULDBLOCK;
    XP_SetError(err);
}

int SEC_Send(int s, const void *buf, int len, int flags)
{
    int rv;

#if defined(__sun) && defined(SYSV)
    rv = _PR_MD_SEND(s, buf, len, flags, sec_io_timeout);
#else
    rv = _PR_MD_SEND(s, buf, len, 0, sec_io_timeout);
#endif
    if (rv < 0)
	SEC_Map_Error();
    return rv;
}

int SEC_Recv(int s, void *buf, int len, int flags)
{
    int rv;

    rv = _PR_MD_RECV(s, buf, len, flags, sec_io_timeout);
    if (rv < 0)
	SEC_Map_Error();
    return rv;
}

#endif /* SOURCE_KIT */
