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

#ifndef nspr_locks_h___
#define nspr_locks_h___

/* many people in libnet [mkautocf.c ...] (and possibly others) get
 * NSPR20 for free by including nslocks.h.  To minimize changes during
 * the javaectomy effort, we are including this file (where previously
 * it was only included if java was included.
 */
#include "prmon.h"
#ifdef NSPR20
#ifdef XP_MAC
#include "prpriv.h"	/* for MonitorEntryCount */
#else
#include "private/prpriv.h"
#endif
#endif /* NSPR20 */

#if defined(JAVA)

XP_BEGIN_PROTOS
extern PRMonitor* libnet_asyncIO;
XP_END_PROTOS

#define LIBNET_LOCK()		PR_EnterMonitor(libnet_asyncIO)
#define LIBNET_UNLOCK()		PR_ExitMonitor(libnet_asyncIO)
#define LIBNET_IS_LOCKED()	PR_InMonitor(libnet_asyncIO)

#else /* !JAVA */

#define LIBNET_LOCK()
#define LIBNET_UNLOCK()
#define LIBNET_IS_LOCKED() 1

#endif /* JAVA */

#endif /* nspr_locks_h___ */
