/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
 * Header for JavaScript Debugging support - Locking and threading functions
 */

#ifndef jsd_lock_h___
#define jsd_lock_h___

/* 
 * If you want to support threading and locking, define JSD_THREADSAFE and 
 * implement the functions below.
 */

/*
 * NOTE: These locks must be reentrant in the sense that they support
 * nested calls to lock and unlock.
 */

typedef struct JSDStaticLock JSDStaticLock;

extern void*
jsd_CreateLock();

extern void
jsd_Lock(JSDStaticLock* lock);

extern void
jsd_Unlock(JSDStaticLock* lock);

#ifdef DEBUG
extern JSBool
jsd_IsLocked(JSDStaticLock* lock);
#endif /* DEBUG */

extern void*
jsd_CurrentThread();

#endif /* jsd_lock_h___ */
