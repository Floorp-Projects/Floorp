/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef jslock_h__
#define jslock_h__

#include "jstypes.h"
#include "jsapi.h"
#include "jsprvtd.h"

#ifdef JS_THREADSAFE
# include "pratom.h"
# include "prlock.h"
# include "prcvar.h"
# include "prthread.h"
# include "prinit.h"
#endif

#ifdef JS_THREADSAFE

#if (defined(_WIN32) && defined(_M_IX86)) ||                                  \
    (defined(_WIN64) && (defined(_M_AMD64) || defined(_M_X64))) ||            \
    (defined(__i386) && (defined(__GNUC__) || defined(__SUNPRO_CC))) ||       \
    (defined(__x86_64) && (defined(__GNUC__) || defined(__SUNPRO_CC))) ||     \
    (defined(__sparc) && (defined(__GNUC__) || defined(__SUNPRO_CC))) ||      \
    (defined(__arm__) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)) ||      \
    defined(AIX) ||                                                           \
    defined(USE_ARM_KUSER)
# define JS_HAS_NATIVE_COMPARE_AND_SWAP 1
#else
# define JS_HAS_NATIVE_COMPARE_AND_SWAP 0
#endif

#if defined(JS_USE_ONLY_NSPR_LOCKS) || !JS_HAS_NATIVE_COMPARE_AND_SWAP
# define NSPR_LOCK 1
#else
# undef NSPR_LOCK
#endif

#define Thin_GetWait(W) ((jsword)(W) & 0x1)
#define Thin_SetWait(W) ((jsword)(W) | 0x1)
#define Thin_RemoveWait(W) ((jsword)(W) & ~0x1)

typedef struct JSFatLock JSFatLock;

typedef struct JSThinLock {
    jsword      owner;
    JSFatLock   *fat;
} JSThinLock;

#define CX_THINLOCK_ID(cx)       ((jsword)(cx)->thread())
#define CURRENT_THREAD_IS_ME(me) (((JSThread *)me)->id == js_CurrentThreadId())

typedef PRLock JSLock;

/*
 * Atomic increment and decrement for a reference counter, given jsrefcount *p.
 * NB: jsrefcount is int32, aka PRInt32, so that pratom.h functions work.
 */
#define JS_ATOMIC_INCREMENT(p)      PR_ATOMIC_INCREMENT((PRInt32 *)(p))
#define JS_ATOMIC_DECREMENT(p)      PR_ATOMIC_DECREMENT((PRInt32 *)(p))
#define JS_ATOMIC_ADD(p,v)          PR_ATOMIC_ADD((PRInt32 *)(p), (PRInt32)(v))
#define JS_ATOMIC_SET(p,v)          PR_ATOMIC_SET((PRInt32 *)(p), (PRInt32)(v))

#define js_CurrentThreadId()        PR_GetCurrentThread()
#define JS_NEW_LOCK()               PR_NewLock()
#define JS_DESTROY_LOCK(l)          PR_DestroyLock(l)
#define JS_ACQUIRE_LOCK(l)          PR_Lock(l)
#define JS_RELEASE_LOCK(l)          PR_Unlock(l)

#define JS_NEW_CONDVAR(l)           PR_NewCondVar(l)
#define JS_DESTROY_CONDVAR(cv)      PR_DestroyCondVar(cv)
#define JS_WAIT_CONDVAR(cv,to)      PR_WaitCondVar(cv,to)
#define JS_NO_TIMEOUT               PR_INTERVAL_NO_TIMEOUT
#define JS_NOTIFY_CONDVAR(cv)       PR_NotifyCondVar(cv)
#define JS_NOTIFY_ALL_CONDVAR(cv)   PR_NotifyAllCondVar(cv)

#define JS_LOCK(cx, tl)             js_Lock(cx, tl)
#define JS_UNLOCK(cx, tl)           js_Unlock(cx, tl)

#define JS_LOCK_RUNTIME(rt)         js_LockRuntime(rt)
#define JS_UNLOCK_RUNTIME(rt)       js_UnlockRuntime(rt)

extern void js_Lock(JSContext *cx, JSThinLock *tl);
extern void js_Unlock(JSContext *cx, JSThinLock *tl);
extern void js_LockRuntime(JSRuntime *rt);
extern void js_UnlockRuntime(JSRuntime *rt);
extern int js_SetupLocks(int,int);
extern void js_CleanupLocks();
extern void js_InitLock(JSThinLock *);
extern void js_FinishLock(JSThinLock *);

#ifdef DEBUG

#define JS_IS_RUNTIME_LOCKED(rt)        js_IsRuntimeLocked(rt)

extern JSBool js_IsRuntimeLocked(JSRuntime *rt);

#else

#define JS_IS_RUNTIME_LOCKED(rt)        0

#endif /* DEBUG */

#else  /* !JS_THREADSAFE */

#define JS_ATOMIC_INCREMENT(p)      (++*(p))
#define JS_ATOMIC_DECREMENT(p)      (--*(p))
#define JS_ATOMIC_ADD(p,v)          (*(p) += (v))
#define JS_ATOMIC_SET(p,v)          (*(p) = (v))

#define js_CurrentThreadId()        0
#define JS_NEW_LOCK()               NULL
#define JS_DESTROY_LOCK(l)          ((void)0)
#define JS_ACQUIRE_LOCK(l)          ((void)0)
#define JS_RELEASE_LOCK(l)          ((void)0)
#define JS_LOCK(cx, tl)             ((void)0)
#define JS_UNLOCK(cx, tl)           ((void)0)

#define JS_NEW_CONDVAR(l)           NULL
#define JS_DESTROY_CONDVAR(cv)      ((void)0)
#define JS_WAIT_CONDVAR(cv,to)      ((void)0)
#define JS_NOTIFY_CONDVAR(cv)       ((void)0)
#define JS_NOTIFY_ALL_CONDVAR(cv)   ((void)0)

#define JS_LOCK_RUNTIME(rt)         ((void)0)
#define JS_UNLOCK_RUNTIME(rt)       ((void)0)

#define JS_IS_RUNTIME_LOCKED(rt)        1

#endif /* !JS_THREADSAFE */

#define JS_LOCK_GC(rt)              JS_ACQUIRE_LOCK((rt)->gcLock)
#define JS_UNLOCK_GC(rt)            JS_RELEASE_LOCK((rt)->gcLock)
#define JS_AWAIT_GC_DONE(rt)        JS_WAIT_CONDVAR((rt)->gcDone, JS_NO_TIMEOUT)
#define JS_NOTIFY_GC_DONE(rt)       JS_NOTIFY_ALL_CONDVAR((rt)->gcDone)
#define JS_AWAIT_REQUEST_DONE(rt)   JS_WAIT_CONDVAR((rt)->requestDone,        \
                                                    JS_NO_TIMEOUT)
#define JS_NOTIFY_REQUEST_DONE(rt)  JS_NOTIFY_CONDVAR((rt)->requestDone)

#ifndef JS_SET_OBJ_INFO
#define JS_SET_OBJ_INFO(obj,f,l)        ((void)0)
#endif
#ifndef JS_SET_TITLE_INFO
#define JS_SET_TITLE_INFO(title,f,l)    ((void)0)
#endif

#ifdef JS_THREADSAFE

extern JSBool
js_CompareAndSwap(volatile jsword *w, jsword ov, jsword nv);

/* Atomically bitwise-or the mask into the word *w using compare and swap. */
extern void
js_AtomicSetMask(volatile jsword *w, jsword mask);

/*
 * Atomically bitwise-and the complement of the mask into the word *w using
 * compare and swap.
 */
extern void
js_AtomicClearMask(volatile jsword *w, jsword mask);

#define JS_ATOMIC_SET_MASK(w, mask) js_AtomicSetMask(w, mask)
#define JS_ATOMIC_CLEAR_MASK(w, mask) js_AtomicClearMask(w, mask)

#else

static inline JSBool
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    return (*w == ov) ? *w = nv, JS_TRUE : JS_FALSE;
}

#define JS_ATOMIC_SET_MASK(w, mask) (*(w) |= (mask))
#define JS_ATOMIC_CLEAR_MASK(w, mask) (*(w) &= ~(mask))

#endif

#ifdef __cplusplus

namespace js {

#ifdef JS_THREADSAFE
class AutoLock {
  private:
    JSLock *lock;

  public:
    AutoLock(JSLock *lock) : lock(lock) { JS_ACQUIRE_LOCK(lock); }
    ~AutoLock() { JS_RELEASE_LOCK(lock); }
};
# define JS_AUTO_LOCK_GUARD(name, l) AutoLock name((l));
#else
# define JS_AUTO_LOCK_GUARD(name, l)
#endif

class AutoAtomicIncrement {
    int32 *p;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoAtomicIncrement(int32 *p JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : p(p) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ATOMIC_INCREMENT(p);
    }

    ~AutoAtomicIncrement() {
        JS_ATOMIC_DECREMENT(p);
    }
};

} /* namespace js */

#endif

#endif /* jslock_h___ */
