/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef mozjava_h___
#define mozjava_h___

#if defined(NSPR) || defined(JAVA)
#include "prthread.h"
#include "prmon.h"
#ifndef NSPR20
#include "prfile.h"
#include "prglobal.h"
#include "prevent.h"
#else
#include "prio.h"
#include "prtypes.h"
#include "plevent.h"
#endif /* NSPR20 */
#include "prlog.h"
#include "java.h"
#endif

#ifdef JAVA
extern PRMonitor *fdset_lock;
extern PRMonitor *pr_asyncIO;
extern PRThread *mozilla_thread;

#define LOCK_FDSET() PR_EnterMonitor(fdset_lock)
#define UNLOCK_FDSET() PR_ExitMonitor(fdset_lock)
#else
#define LOCK_FDSET()
#define UNLOCK_FDSET()
#endif

#endif /* mozjava_h___ */
