/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/*
 * This file contains declarations for some functions that are normally implemented
 * in NSPR, but which are stubbed out or changed in mini-NSPR (the NSPR subset
 * required to run JS).
 */
#ifndef prstubs_h___
#define prstubs_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef struct PRLock {int dummy;} PRLock;

PR_EXTERN(PRLock*) PR_NewLock(void);
PR_EXTERN(void) PR_DestroyLock(PRLock *lock);
PR_EXTERN(void) PR_Lock(PRLock *lock);
PR_EXTERN(PRStatus) PR_Unlock(PRLock *lock);

extern PRBool _pr_initialized;
extern void _PR_ImplicitInitialization(void);

#define PR_SetError(x, y)

/*
** Abort the process in a non-graceful manner. This will cause a core file,
** call to the debugger or other moral equivalent as well as causing the
** entire process to stop.
*/
PR_EXTERN(void) PR_Abort(void);

PR_END_EXTERN_C

#endif /* prstubs_h___ */
