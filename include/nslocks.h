/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */

#ifndef nspr_locks_h___
#define nspr_locks_h___

/* many people in libnet [mkautocf.c ...] (and possibly others) get
 * NSPR20 for free by including nslocks.h.  To minimize changes during
 * the javaectomy effort, we are including this file (where previously
 * it was only included if java was included.
 */
#include "prmon.h"
#ifdef XP_MAC
#include "prpriv.h"	/* for MonitorEntryCount */
#else
#include "private/prpriv.h"
#endif

#if defined(JAVA) || defined(NS_MT_SUPPORTED)

XP_BEGIN_PROTOS
extern PRMonitor* libnet_asyncIO;
XP_END_PROTOS

#define LIBNET_LOCK()		PR_EnterMonitor(libnet_asyncIO)
#define LIBNET_UNLOCK()		PR_ExitMonitor(libnet_asyncIO)
#define LIBNET_IS_LOCKED()	PR_InMonitor(libnet_asyncIO)

#else /* !JAVA && !NS_MT_SUPPORTED*/

#define LIBNET_LOCK()
#define LIBNET_UNLOCK()
#define LIBNET_IS_LOCKED() 1

#endif /* JAVA */

#endif /* nspr_locks_h___ */
