/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */
#ifndef jslock_h__
#define jslock_h__

#ifdef JS_THREADSAFE

#include "jstypes.h"
#include "prlock.h"
#include "prcvar.h"
#include "jshash.h" /* Added by JSIFY */

#include "jsprvtd.h"    /* for JSScope, etc. */
#include "jspubtd.h"    /* for JSRuntime, etc. */

#define Thin_GetWait(W) ((jsword)(W) & 0x1)
#define Thin_SetWait(W) ((jsword)(W) | 0x1)
#define Thin_RemoveWait(W) ((jsword)(W) & ~0x1)

typedef struct JSFatLock {
    int susp;
    PRLock* slock;
    PRCondVar* svar;
    struct JSFatLock *next;
    struct JSFatLock *prev;
} JSFatLock;

typedef struct JSThinLock {
  jsword owner;
  JSFatLock *fat;
} JSThinLock;

typedef PRLock JSLock;

typedef struct JSFatLockTable {
    JSFatLock *free;
    JSFatLock *taken;
} JSFatLockTable;

#define JS_ATOMIC_ADDREF(p, i) js_AtomicAdd(p,i)

#define CurrentThreadId()           (jsword)PR_GetCurrentThread()
#define JS_CurrentThreadId()        js_CurrentThreadId()
#define JS_NEW_LOCK()               PR_NewLock()
#define JS_DESTROY_LOCK(l)          PR_DestroyLock(l)
#define JS_ACQUIRE_LOCK(l)          PR_Lock(l)
#define JS_RELEASE_LOCK(l)          PR_Unlock(l)
#define JS_LOCK0(P,M) js_Lock(P,M)
#define JS_UNLOCK0(P,M) js_Unlock(P,M)

#define JS_NEW_CONDVAR(l)           PR_NewCondVar(l)
#define JS_DESTROY_CONDVAR(cv)      PR_DestroyCondVar(cv)
#define JS_WAIT_CONDVAR(cv,to)      PR_WaitCondVar(cv,to)
#define JS_NO_TIMEOUT               PR_INTERVAL_NO_TIMEOUT
#define JS_NOTIFY_CONDVAR(cv)       PR_NotifyCondVar(cv)
#define JS_NOTIFY_ALL_CONDVAR(cv)   PR_NotifyAllCondVar(cv)

#ifdef DEBUG
#include "jsscope.h"

#define _SET_OBJ_INFO(obj,f,l)                                                \
    _SET_SCOPE_INFO(OBJ_SCOPE(obj),f,l)

#define _SET_SCOPE_INFO(scope,f,l)                                            \
    (JS_ASSERT(scope->count > 0 && scope->count <= 4),                        \
     scope->file[scope->count-1] = f,                                         \
     scope->line[scope->count-1] = l)
#endif /* DEBUG */

#define JS_LOCK_RUNTIME(rt)         js_LockRuntime(rt)
#define JS_UNLOCK_RUNTIME(rt)       js_UnlockRuntime(rt)
#define JS_LOCK_OBJ(cx,obj)         (js_LockObj(cx, obj),                    \
				      _SET_OBJ_INFO(obj,__FILE__,__LINE__))
#define JS_UNLOCK_OBJ(cx,obj)       js_UnlockObj(cx, obj)
#define JS_LOCK_SCOPE(cx,scope)     (js_LockScope(cx, scope),                \
				      _SET_SCOPE_INFO(scope,__FILE__,__LINE__))
#define JS_UNLOCK_SCOPE(cx,scope)   js_UnlockScope(cx, scope)
#define JS_TRANSFER_SCOPE_LOCK(cx, scope, newscope) js_TransferScopeLock(cx, scope, newscope)

extern jsword js_CurrentThreadId();
extern JS_INLINE void js_Lock(JSThinLock *, jsword);
extern JS_INLINE void js_Unlock(JSThinLock *, jsword);
extern int js_CompareAndSwap(jsword *, jsword, jsword);
extern void js_AtomicAdd(jsword*, jsword);
extern void js_LockRuntime(JSRuntime *rt);
extern void js_UnlockRuntime(JSRuntime *rt);
extern void js_LockObj(JSContext *cx, JSObject *obj);
extern void js_UnlockObj(JSContext *cx, JSObject *obj);
extern void js_LockScope(JSContext *cx, JSScope *scope);
extern void js_UnlockScope(JSContext *cx, JSScope *scope);
extern int js_SetupLocks(int,int);
extern void js_CleanupLocks();
extern JS_PUBLIC_API(void) js_InitContextForLocking(JSContext *);
extern void js_TransferScopeLock(JSContext *, JSScope *, JSScope *);
extern JS_PUBLIC_API(jsval) js_GetSlotWhileLocked(JSContext *, JSObject *, uint32);
extern JS_PUBLIC_API(void) js_SetSlotWhileLocked(JSContext *, JSObject *, uint32, jsval);
extern void js_NewLock(JSThinLock *);
extern void js_DestroyLock(JSThinLock *);

#ifdef DEBUG

#define JS_IS_RUNTIME_LOCKED(rt)    js_IsRuntimeLocked(rt)
#define JS_IS_OBJ_LOCKED(obj)       js_IsObjLocked(obj)
#define JS_IS_SCOPE_LOCKED(scope)   js_IsScopeLocked(scope)

extern JSBool js_IsRuntimeLocked(JSRuntime *rt);
extern JSBool js_IsObjLocked(JSObject *obj);
extern JSBool js_IsScopeLocked(JSScope *scope);

#else

#define JS_IS_RUNTIME_LOCKED(rt)    0
#define JS_IS_OBJ_LOCKED(obj)       1
#define JS_IS_SCOPE_LOCKED(scope)   1

#endif /* DEBUG */

#define JS_LOCK_OBJ_VOID(cx, obj, e)                                          \
    JS_BEGIN_MACRO                                                            \
	js_LockObj(cx, obj);                                                 \
	e;                                                                    \
	js_UnlockObj(cx, obj);                                               \
    JS_END_MACRO

#define JS_LOCK_VOID(cx, e)                                                   \
    JS_BEGIN_MACRO                                                            \
	JSRuntime *_rt = (cx)->runtime;                                       \
	JS_LOCK_RUNTIME_VOID(_rt, e);                                         \
    JS_END_MACRO

#if defined(JS_USE_ONLY_NSPR_LOCKS) || !(defined(_WIN32) || defined(SOLARIS) || defined(AIX))

#undef JS_LOCK0
#undef JS_UNLOCK0
#define JS_LOCK0(P,M) JS_ACQUIRE_LOCK(((JSLock*)(P)->fat)); (P)->owner = (M)
#define JS_UNLOCK0(P,M) (P)->owner = 0; JS_RELEASE_LOCK(((JSLock*)(P)->fat))
#define NSPR_LOCK 1

#endif /* arch-tests */

#else  /* !JS_THREADSAFE */

#define JS_ATOMIC_ADDREF(p,i)       (*(p) += i)

#define JS_CurrentThreadId() 0
#define JS_NEW_LOCK()               NULL
#define JS_DESTROY_LOCK(l)          ((void)0)
#define JS_ACQUIRE_LOCK(l)          ((void)0)
#define JS_RELEASE_LOCK(l)          ((void)0)
#define JS_LOCK0(P,M)                ((void)0)
#define JS_UNLOCK0(P,M)              ((void)0)

#define JS_NEW_CONDVAR(l)           NULL
#define JS_DESTROY_CONDVAR(cv)      ((void)0)
#define JS_WAIT_CONDVAR(cv,to)      ((void)0)
#define JS_NOTIFY_CONDVAR(cv)       ((void)0)
#define JS_NOTIFY_ALL_CONDVAR(cv)   ((void)0)

#define JS_LOCK_RUNTIME(rt)         ((void)0)
#define JS_UNLOCK_RUNTIME(rt)       ((void)0)
#define JS_LOCK_OBJ(cx,obj)         ((void)0)
#define JS_UNLOCK_OBJ(cx,obj)       ((void)0)
#define JS_LOCK_OBJ_VOID(cx,obj,e)  (e)
#define JS_LOCK_SCOPE(cx,scope)     ((void)0)
#define JS_UNLOCK_SCOPE(cx,scope)   ((void)0)
#define JS_TRANSFER_SCOPE_LOCK(c,o,n) ((void)0)

#define JS_IS_RUNTIME_LOCKED(rt)    1
#define JS_IS_OBJ_LOCKED(obj)       1
#define JS_IS_SCOPE_LOCKED(scope)   1
#define JS_LOCK_VOID(cx, e)         JS_LOCK_RUNTIME_VOID((cx)->runtime, e)

#endif /* !JS_THREADSAFE */

#define JS_LOCK_RUNTIME_VOID(rt,e)                                            \
    JS_BEGIN_MACRO                                                            \
	JS_LOCK_RUNTIME(rt);                                                  \
	e;                                                                    \
	JS_UNLOCK_RUNTIME(rt);                                                \
    JS_END_MACRO

#define JS_LOCK_GC(rt)              JS_ACQUIRE_LOCK((rt)->gcLock)
#define JS_UNLOCK_GC(rt)            JS_RELEASE_LOCK((rt)->gcLock)
#define JS_LOCK_GC_VOID(rt,e)       (JS_LOCK_GC(rt), (e), JS_UNLOCK_GC(rt))
#define JS_AWAIT_GC_DONE(rt)        JS_WAIT_CONDVAR((rt)->gcDone, JS_NO_TIMEOUT)
#define JS_NOTIFY_GC_DONE(rt)       JS_NOTIFY_ALL_CONDVAR((rt)->gcDone)
#define JS_AWAIT_REQUEST_DONE(rt)   JS_WAIT_CONDVAR((rt)->requestDone,        \
						    JS_NO_TIMEOUT)
#define JS_NOTIFY_REQUEST_DONE(rt)  JS_NOTIFY_CONDVAR((rt)->requestDone)

#define JS_LOCK(P,CX) JS_LOCK0(P,(CX)->thread)
#define JS_UNLOCK(P,CX) JS_UNLOCK0(P,(CX)->thread)

#ifndef _SET_OBJ_INFO
#define _SET_OBJ_INFO(obj,f,l)      ((void)0)
#endif
#ifndef _SET_SCOPE_INFO
#define _SET_SCOPE_INFO(scope,f,l)  ((void)0)
#endif

#endif /* jslock_h___ */
