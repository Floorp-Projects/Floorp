/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef mozjava_h___
#define mozjava_h___

#include "prthread.h"
#include "prmon.h"
#include "prio.h"
#include "prtypes.h"
#include "plevent.h"
#include "prlog.h"
#ifdef JAVA
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
