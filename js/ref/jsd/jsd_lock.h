/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsd_lock_h___
#define jsd_lock_h___

/* 
 * If you want to support threading and locking, define JSD_THREADSAFE and 
 * implement the functions below.
 */

/* All the NETSCAPE_INTERNAL stuff supports this locking */
#ifdef NETSCAPE_INTERNAL
#define JSD_THREADSAFE 1
#endif

/*
 * NOTE: These locks must be reentrant in the sense that they support
 * nested calls to lock and unlock.
 */

extern void*
jsd_CreateLock();

extern void
jsd_Lock(void* lock);

extern void
jsd_Unlock(void* lock);

#ifdef DEBUG
extern JSBool
jsd_IsLocked(void* lock);
#endif /* DEBUG */

extern void*
jsd_CurrentThread();

#endif /* jsd_lock_h___ */
