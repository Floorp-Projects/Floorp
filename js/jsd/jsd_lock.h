/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

extern JSDStaticLock*
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
