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

#ifndef jslock_h___
#define jslock_h___
/*
 * JS locking macros, used to protect globals only if JS_THREADSAFE is defined.
 */

#ifdef JS_THREADSAFE

#include "prlock.h"
#include "jscntxt.h"

#define JS_ACQUIRE_LOCK(l)      PR_Lock(l)
#define JS_RELEASE_LOCK(l)      PR_Unlock(l)

#define JS_LOCK_RUNTIME(rt)     js_lock_runtime(rt)
#define JS_UNLOCK_RUNTIME(rt)   js_unlock_runtime(rt)

extern void js_lock_runtime(JSRuntime *rt);
extern void js_unlock_runtime(JSRuntime *rt);
#ifdef DEBUG
#define JS_IS_RUNTIME_LOCKED(rt) js_is_runtime_locked(rt)

extern int js_is_runtime_locked(JSRuntime *rt);
#else
#define JS_IS_RUNTIME_LOCKED(rt) 1
#endif

#define JS_LOCK_VOID(cx, e)                                                   \
    PR_BEGIN_MACRO                                                            \
	JSRuntime *_rt = (cx)->runtime;                                       \
	JS_LOCK_RUNTIME_VOID(_rt, e);                                         \
    PR_END_MACRO

#else  /* !JS_THREADSAFE */

#define JS_ACQUIRE_LOCK(l)      ((void)0)
#define JS_RELEASE_LOCK(l)      ((void)0)

#define JS_LOCK_RUNTIME(rt)     ((void)0)
#define JS_UNLOCK_RUNTIME(rt)   ((void)0)
#define JS_IS_RUNTIME_LOCKED(rt) 1
#define JS_LOCK_VOID(cx, e)     JS_LOCK_RUNTIME_VOID((cx)->runtime, e)

#endif /* !JS_THREADSAFE */

#define JS_LOCK(cx)             JS_LOCK_RUNTIME((cx)->runtime)
#define JS_UNLOCK(cx)           JS_UNLOCK_RUNTIME((cx)->runtime)
#define JS_IS_LOCKED(cx)        JS_IS_RUNTIME_LOCKED((cx)->runtime)

#define JS_LOCK_RUNTIME_VOID(t,e) \
    (JS_LOCK_RUNTIME(t), (void)(e), JS_UNLOCK_RUNTIME(t))

#define JS_LOCK_AND_RETURN_TYPE(cx, type, call)                               \
    PR_BEGIN_MACRO                                                            \
	type _ret;                                                            \
	JS_LOCK_VOID(cx, _ret = call);                                        \
	return _ret;                                                          \
    PR_END_MACRO

#define JS_LOCK_AND_RETURN_BOOL(cx, call)                                     \
    JS_LOCK_AND_RETURN_TYPE(cx, JSBool, call)

#define JS_LOCK_AND_RETURN_STRING(cx, call)                                   \
    JS_LOCK_AND_RETURN_TYPE(cx, JSString *, call)

#endif /* jslock_h___ */
