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

#ifdef JS_THREADSAFE

/*
 * JS locking stubs.
 */
#include <stdlib.h>
#include <string.h>
#include "jspubtd.h"
#include "jsutil.h"
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsscope.h"
#include "jsstr.h"

using namespace js;

#define ReadWord(W) (W)

#if !defined(__GNUC__)
# define __asm__ asm
# define __volatile__ volatile
#endif

/* Implement NativeCompareAndSwap. */

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma warning( disable : 4035 )
JS_BEGIN_EXTERN_C
extern long __cdecl
_InterlockedCompareExchange(long *volatile dest, long exchange, long comp);
JS_END_EXTERN_C
#pragma intrinsic(_InterlockedCompareExchange)

JS_STATIC_ASSERT(sizeof(jsword) == sizeof(long));

static JS_ALWAYS_INLINE int
NativeCompareAndSwapHelper(volatile jsword *w, jsword ov, jsword nv)
{
    _InterlockedCompareExchange((long*) w, nv, ov);
    __asm {
        sete al
    }
}

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    return (NativeCompareAndSwapHelper(w, ov, nv) & 1);
}

#elif defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64))
JS_BEGIN_EXTERN_C
extern long long __cdecl
_InterlockedCompareExchange64(long long *volatile dest, long long exchange, long long comp);
JS_END_EXTERN_C
#pragma intrinsic(_InterlockedCompareExchange64)

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    return _InterlockedCompareExchange64((long long *volatile)w, nv, ov) == ov;
}

#elif defined(XP_MACOSX) || defined(DARWIN)

#include <libkern/OSAtomic.h>

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    /* Details on these functions available in the manpage for atomic */
    return OSAtomicCompareAndSwapPtrBarrier(reinterpret_cast<void *>(ov),
                                            reinterpret_cast<void *>(nv),
                                            reinterpret_cast<void * volatile *>(w));
}

#elif defined(__i386) && (defined(__GNUC__) || defined(__SUNPRO_CC))

/* Note: This fails on 386 cpus, cmpxchgl is a >= 486 instruction */
static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                          "lock\n"
                          "cmpxchgl %2, (%1)\n"
                          "sete %%al\n"
                          "andl $1, %%eax\n"
                          : "=a" (res)
#ifdef __SUNPRO_CC
/* Different code for Sun Studio because of a bug of SS12U1 */
                          : "c" (w), "d" (nv), "a" (ov)
#else
                          : "r" (w), "r" (nv), "a" (ov)
#endif
                          : "cc", "memory");
    return (int)res;
}
#elif defined(__x86_64) && (defined(__GNUC__) || defined(__SUNPRO_CC))

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                          "lock\n"
                          "cmpxchgq %2, (%1)\n"
                          "sete %%al\n"
                          "movzbl %%al, %%eax\n"
                          : "=a" (res)
                          : "r" (w), "r" (nv), "a" (ov)
                          : "cc", "memory");
    return (int)res;
}

#elif defined(__sparc)
#if defined(__GNUC__)

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                  "membar #StoreLoad | #LoadLoad\n"
#if JS_BITS_PER_WORD == 32
                  "cas [%1],%2,%3\n"
#else
                  "casx [%1],%2,%3\n"
#endif
                  "membar #StoreLoad | #LoadLoad\n"
                  "cmp %2,%3\n"
                  "be,a 1f\n"
                  "mov 1,%0\n"
                  "mov 0,%0\n"
                  "1:"
                  : "=r" (res)
                  : "r" (w), "r" (ov), "r" (nv));
    return (int)res;
}

#elif defined(__SUNPRO_CC)

/* Implementation in lock_sparc*.il */
extern "C" int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv);

#endif

#elif defined(AIX)

#include <sys/atomic_op.h>

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    int res;
    JS_STATIC_ASSERT(sizeof(jsword) == sizeof(long));

    res = compare_and_swaplp((atomic_l)w, &ov, nv);
    if (res)
        __asm__("isync");
    return res;
}

#elif defined(USE_ARM_KUSER)

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=429387 for a
 * description of this ABI; this is a function provided at a fixed
 * location by the kernel in the memory space of each process.
 */
typedef int (__kernel_cmpxchg_t)(int oldval, int newval, volatile int *ptr);
#define __kernel_cmpxchg (*(__kernel_cmpxchg_t *)0xffff0fc0)

JS_STATIC_ASSERT(sizeof(jsword) == sizeof(int));

static JS_ALWAYS_INLINE int
NativeCompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    volatile int *vp = (volatile int *) w;
    PRInt32 failed = 1;

    /* Loop until a __kernel_cmpxchg succeeds. See bug 446169 */
    do {
        failed = __kernel_cmpxchg(ov, nv, vp);
    } while (failed && *vp == ov);
    return !failed;
}

#elif JS_HAS_NATIVE_COMPARE_AND_SWAP

#error "JS_HAS_NATIVE_COMPARE_AND_SWAP should be 0 if your platform lacks a compare-and-swap instruction."

#endif /* arch-tests */

#if JS_HAS_NATIVE_COMPARE_AND_SWAP

JSBool
js_CompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    return !!NativeCompareAndSwap(w, ov, nv);
}

#elif defined(NSPR_LOCK)

# ifdef __GNUC__
# warning "js_CompareAndSwap is implemented using NSPR lock"
# endif

JSBool
js_CompareAndSwap(volatile jsword *w, jsword ov, jsword nv)
{
    int result;
    static PRLock *CompareAndSwapLock = JS_NEW_LOCK();

    JS_ACQUIRE_LOCK(CompareAndSwapLock);
    result = (*w == ov);
    if (result)
        *w = nv;
    JS_RELEASE_LOCK(CompareAndSwapLock);
    return result;
}

#else /* !defined(NSPR_LOCK) */

#error "NSPR_LOCK should be on when the platform lacks native compare-and-swap."

#endif

void
js_AtomicSetMask(volatile jsword *w, jsword mask)
{
    jsword ov, nv;

    do {
        ov = *w;
        nv = ov | mask;
    } while (!js_CompareAndSwap(w, ov, nv));
}

void
js_AtomicClearMask(volatile jsword *w, jsword mask)
{
    jsword ov, nv;

    do {
        ov = *w;
        nv = ov & ~mask;
    } while (!js_CompareAndSwap(w, ov, nv));
}

#ifndef NSPR_LOCK

struct JSFatLock {
    int         susp;
    PRLock      *slock;
    PRCondVar   *svar;
    JSFatLock   *next;
    JSFatLock   **prevp;
};

typedef struct JSFatLockTable {
    JSFatLock   *free;
    JSFatLock   *taken;
} JSFatLockTable;

#define GLOBAL_LOCK_INDEX(id)   (((uint32)(jsuword)(id)>>2) & global_locks_mask)

static void
js_Dequeue(JSThinLock *);

static PRLock **global_locks;
static uint32 global_lock_count = 1;
static uint32 global_locks_log2 = 0;
static uint32 global_locks_mask = 0;

static void
js_LockGlobal(void *id)
{
    uint32 i = GLOBAL_LOCK_INDEX(id);
    PR_Lock(global_locks[i]);
}

static void
js_UnlockGlobal(void *id)
{
    uint32 i = GLOBAL_LOCK_INDEX(id);
    PR_Unlock(global_locks[i]);
}

#endif /* !NSPR_LOCK */

void
js_InitLock(JSThinLock *tl)
{
#ifdef NSPR_LOCK
    tl->owner = 0;
    tl->fat = (JSFatLock*)JS_NEW_LOCK();
#else
    PodZero(tl);
#endif
}

void
js_FinishLock(JSThinLock *tl)
{
#ifdef NSPR_LOCK
    tl->owner = 0xdeadbeef;
    if (tl->fat)
        JS_DESTROY_LOCK(((JSLock*)tl->fat));
#else
    JS_ASSERT(tl->owner == 0);
    JS_ASSERT(tl->fat == NULL);
#endif
}

#ifndef NSPR_LOCK

static JSFatLock *
NewFatlock()
{
    JSFatLock *fl = (JSFatLock *)js_malloc(sizeof(JSFatLock)); /* for now */
    if (!fl) return NULL;
    fl->susp = 0;
    fl->next = NULL;
    fl->prevp = NULL;
    fl->slock = PR_NewLock();
    fl->svar = PR_NewCondVar(fl->slock);
    return fl;
}

static void
DestroyFatlock(JSFatLock *fl)
{
    PR_DestroyLock(fl->slock);
    PR_DestroyCondVar(fl->svar);
    js_free(fl);
}

static JSFatLock *
ListOfFatlocks(int listc)
{
    JSFatLock *m;
    JSFatLock *m0;
    int i;

    JS_ASSERT(listc>0);
    m0 = m = NewFatlock();
    for (i=1; i<listc; i++) {
        m->next = NewFatlock();
        m = m->next;
    }
    return m0;
}

static void
DeleteListOfFatlocks(JSFatLock *m)
{
    JSFatLock *m0;
    for (; m; m=m0) {
        m0 = m->next;
        DestroyFatlock(m);
    }
}

static JSFatLockTable *fl_list_table = NULL;
static uint32          fl_list_table_len = 0;
static uint32          fl_list_chunk_len = 0;

static JSFatLock *
GetFatlock(void *id)
{
    JSFatLock *m;

    uint32 i = GLOBAL_LOCK_INDEX(id);
    if (fl_list_table[i].free == NULL) {
#ifdef DEBUG
        if (fl_list_table[i].taken)
            printf("Ran out of fat locks!\n");
#endif
        fl_list_table[i].free = ListOfFatlocks(fl_list_chunk_len);
    }
    m = fl_list_table[i].free;
    fl_list_table[i].free = m->next;
    m->susp = 0;
    m->next = fl_list_table[i].taken;
    m->prevp = &fl_list_table[i].taken;
    if (fl_list_table[i].taken)
        fl_list_table[i].taken->prevp = &m->next;
    fl_list_table[i].taken = m;
    return m;
}

static void
PutFatlock(JSFatLock *m, void *id)
{
    uint32 i;
    if (m == NULL)
        return;

    /* Unlink m from fl_list_table[i].taken. */
    *m->prevp = m->next;
    if (m->next)
        m->next->prevp = m->prevp;

    /* Insert m in fl_list_table[i].free. */
    i = GLOBAL_LOCK_INDEX(id);
    m->next = fl_list_table[i].free;
    fl_list_table[i].free = m;
}

#endif /* !NSPR_LOCK */

JSBool
js_SetupLocks(int listc, int globc)
{
#ifndef NSPR_LOCK
    uint32 i;

    if (global_locks)
        return JS_TRUE;
#ifdef DEBUG
    if (listc > 10000 || listc < 0) /* listc == fat lock list chunk length */
        printf("Bad number %d in js_SetupLocks()!\n", listc);
    if (globc > 100 || globc < 0)   /* globc == number of global locks */
        printf("Bad number %d in js_SetupLocks()!\n", listc);
#endif
    global_locks_log2 = JS_CeilingLog2(globc);
    global_locks_mask = JS_BITMASK(global_locks_log2);
    global_lock_count = JS_BIT(global_locks_log2);
    global_locks = (PRLock **) js_malloc(global_lock_count * sizeof(PRLock*));
    if (!global_locks)
        return JS_FALSE;
    for (i = 0; i < global_lock_count; i++) {
        global_locks[i] = PR_NewLock();
        if (!global_locks[i]) {
            global_lock_count = i;
            js_CleanupLocks();
            return JS_FALSE;
        }
    }
    fl_list_table = (JSFatLockTable *) js_malloc(i * sizeof(JSFatLockTable));
    if (!fl_list_table) {
        js_CleanupLocks();
        return JS_FALSE;
    }
    fl_list_table_len = global_lock_count;
    for (i = 0; i < global_lock_count; i++)
        fl_list_table[i].free = fl_list_table[i].taken = NULL;
    fl_list_chunk_len = listc;
#endif /* !NSPR_LOCK */
    return JS_TRUE;
}

void
js_CleanupLocks()
{
#ifndef NSPR_LOCK
    uint32 i;

    if (global_locks) {
        for (i = 0; i < global_lock_count; i++)
            PR_DestroyLock(global_locks[i]);
        js_free(global_locks);
        global_locks = NULL;
        global_lock_count = 1;
        global_locks_log2 = 0;
        global_locks_mask = 0;
    }
    if (fl_list_table) {
        for (i = 0; i < fl_list_table_len; i++) {
            DeleteListOfFatlocks(fl_list_table[i].free);
            fl_list_table[i].free = NULL;
            DeleteListOfFatlocks(fl_list_table[i].taken);
            fl_list_table[i].taken = NULL;
        }
        js_free(fl_list_table);
        fl_list_table = NULL;
        fl_list_table_len = 0;
    }
#endif /* !NSPR_LOCK */
}

#ifdef NSPR_LOCK

static JS_ALWAYS_INLINE void
ThinLock(JSThinLock *tl, jsword me)
{
    JS_ACQUIRE_LOCK((JSLock *) tl->fat);
    tl->owner = me;
}

static JS_ALWAYS_INLINE void
ThinUnlock(JSThinLock *tl, jsword /*me*/)
{
    tl->owner = 0;
    JS_RELEASE_LOCK((JSLock *) tl->fat);
}

#else

/*
 * Fast locking and unlocking is implemented by delaying the allocation of a
 * system lock (fat lock) until contention.  As long as a locking thread A
 * runs uncontended, the lock is represented solely by storing A's identity in
 * the object being locked.
 *
 * If another thread B tries to lock the object currently locked by A, B is
 * enqueued into a fat lock structure (which might have to be allocated and
 * pointed to by the object), and suspended using NSPR conditional variables
 * (wait).  A wait bit (Bacon bit) is set in the lock word of the object,
 * signalling to A that when releasing the lock, B must be dequeued and
 * notified.
 *
 * The basic operation of the locking primitives (js_Lock, js_Unlock,
 * js_Enqueue, and js_Dequeue) is compare-and-swap.  Hence, when locking into
 * the word pointed at by p, compare-and-swap(p, 0, A) success implies that p
 * is unlocked.  Similarly, when unlocking p, if compare-and-swap(p, A, 0)
 * succeeds this implies that p is uncontended (no one is waiting because the
 * wait bit is not set).
 *
 * When dequeueing, the lock is released, and one of the threads suspended on
 * the lock is notified.  If other threads still are waiting, the wait bit is
 * kept (in js_Enqueue), and if not, the fat lock is deallocated.
 *
 * The functions js_Enqueue, js_Dequeue, js_SuspendThread, and js_ResumeThread
 * are serialized using a global lock.  For scalability, a hashtable of global
 * locks is used, which is indexed modulo the thin lock pointer.
 */

/*
 * Invariants:
 * (i)  global lock is held
 * (ii) fl->susp >= 0
 */
static int
js_SuspendThread(JSThinLock *tl)
{
    JSFatLock *fl;
    PRStatus stat;

    if (tl->fat == NULL)
        fl = tl->fat = GetFatlock(tl);
    else
        fl = tl->fat;
    JS_ASSERT(fl->susp >= 0);
    fl->susp++;
    PR_Lock(fl->slock);
    js_UnlockGlobal(tl);
    stat = PR_WaitCondVar(fl->svar, PR_INTERVAL_NO_TIMEOUT);
    JS_ASSERT(stat != PR_FAILURE);
    PR_Unlock(fl->slock);
    js_LockGlobal(tl);
    fl->susp--;
    if (fl->susp == 0) {
        PutFatlock(fl, tl);
        tl->fat = NULL;
    }
    return tl->fat == NULL;
}

/*
 * (i)  global lock is held
 * (ii) fl->susp > 0
 */
static void
js_ResumeThread(JSThinLock *tl)
{
    JSFatLock *fl = tl->fat;
    PRStatus stat;

    JS_ASSERT(fl != NULL);
    JS_ASSERT(fl->susp > 0);
    PR_Lock(fl->slock);
    js_UnlockGlobal(tl);
    stat = PR_NotifyCondVar(fl->svar);
    JS_ASSERT(stat != PR_FAILURE);
    PR_Unlock(fl->slock);
}

static void
js_Enqueue(JSThinLock *tl, jsword me)
{
    jsword o, n;

    js_LockGlobal(tl);
    for (;;) {
        o = ReadWord(tl->owner);
        n = Thin_SetWait(o);
        if (o != 0 && NativeCompareAndSwap(&tl->owner, o, n)) {
            if (js_SuspendThread(tl))
                me = Thin_RemoveWait(me);
            else
                me = Thin_SetWait(me);
        }
        else if (NativeCompareAndSwap(&tl->owner, 0, me)) {
            js_UnlockGlobal(tl);
            return;
        }
    }
}

static void
js_Dequeue(JSThinLock *tl)
{
    jsword o;

    js_LockGlobal(tl);
    o = ReadWord(tl->owner);
    JS_ASSERT(Thin_GetWait(o) != 0);
    JS_ASSERT(tl->fat != NULL);
    if (!NativeCompareAndSwap(&tl->owner, o, 0)) /* release it */
        JS_ASSERT(0);
    js_ResumeThread(tl);
}

static JS_ALWAYS_INLINE void
ThinLock(JSThinLock *tl, jsword me)
{
    JS_ASSERT(CURRENT_THREAD_IS_ME(me));
    if (NativeCompareAndSwap(&tl->owner, 0, me))
        return;
    if (Thin_RemoveWait(ReadWord(tl->owner)) != me)
        js_Enqueue(tl, me);
#ifdef DEBUG
    else
        JS_ASSERT(0);
#endif
}

static JS_ALWAYS_INLINE void
ThinUnlock(JSThinLock *tl, jsword me)
{
    JS_ASSERT(CURRENT_THREAD_IS_ME(me));

    /*
     * Since we can race with the NativeCompareAndSwap in js_Enqueue, we need
     * to use a C_A_S here as well -- Arjan van de Ven 30/1/08
     */
    if (NativeCompareAndSwap(&tl->owner, me, 0))
        return;

    JS_ASSERT(Thin_GetWait(tl->owner));
    if (Thin_RemoveWait(ReadWord(tl->owner)) == me)
        js_Dequeue(tl);
#ifdef DEBUG
    else
        JS_ASSERT(0);   /* unbalanced unlock */
#endif
}

#endif /* !NSPR_LOCK */

void
js_Lock(JSContext *cx, JSThinLock *tl)
{
    ThinLock(tl, CX_THINLOCK_ID(cx));
}

void
js_Unlock(JSContext *cx, JSThinLock *tl)
{
    ThinUnlock(tl, CX_THINLOCK_ID(cx));
}

void
js_LockRuntime(JSRuntime *rt)
{
    PR_Lock(rt->rtLock);
#ifdef DEBUG
    rt->rtLockOwner = js_CurrentThreadId();
#endif
}

void
js_UnlockRuntime(JSRuntime *rt)
{
#ifdef DEBUG
    rt->rtLockOwner = NULL;
#endif
    PR_Unlock(rt->rtLock);
}

#ifdef DEBUG
JSBool
js_IsRuntimeLocked(JSRuntime *rt)
{
    return js_CurrentThreadId() == rt->rtLockOwner;
}
#endif /* DEBUG */
#endif /* JS_THREADSAFE */
