/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
