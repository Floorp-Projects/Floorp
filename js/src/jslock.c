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

#ifdef JS_THREADSAFE

/*
 * JS locking stubs.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include "jspubtd.h"
#include "prthread.h"
#include "pratom.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jscntxt.h"
#include "jsscope.h"
#include "jspubtd.h"
#include "jslock.h"

#ifndef NSPR_LOCK
#include <memory.h>
#endif

static PRLock **_global_locks;
static int _nr_of_globals=1;

static void
js_LockGlobal(void *id)
{
    int i = ((int)id/4)%_nr_of_globals;
    PR_Lock(_global_locks[i]);
}

static void
js_UnlockGlobal(void *id)
{
    int i = ((int)id/4)%_nr_of_globals;
    PR_Unlock(_global_locks[i]);
}

#define ReadWord(W) (W)
#define AtomicAddBody(P,I)                                                    \
    jsword n;                                                                 \
    do {                                                                      \
	n = ReadWord(*(P));                                                   \
    } while (!js_CompareAndSwap(P, n, n + I))

/* Exclude Alpha NT. */
#if defined(_WIN32) && defined(_M_IX86) && !defined(NSPR_LOCK)
#pragma warning( disable : 4035 )

JS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    __asm {
		mov eax,ov
		mov ecx, nv
		mov ebx, w
		lock cmpxchg [ebx], ecx
		sete al
		and eax,1h
	}
}

#elif defined(__GNUC__) && defined(__i386__) && !defined(NSPR_LOCK)

/* Note: This fails on 386 cpus, cmpxchgl is a >= 486 instruction */
JS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    unsigned int res;

    __asm__ __volatile__ (
                          "lock\n"
                          "cmpxchgl %2, (%1)\n"
                          "sete %%al\n"
                          "andl $1, %%eax\n"
                          : "=a" (res)
                          : "r" (w), "r" (nv), "a" (ov) 
                          : "cc", "memory");
    return res;
}

#elif defined(SOLARIS) && defined(sparc) && !defined(NSPR_LOCK)

#ifndef ULTRA_SPARC
JS_INLINE jsword
js_ReadWord(jsword *p)
{
    jsword n;
    while ((n = *p) == -1)
	;
    return n;
}

#undef ReadWord
#define ReadWord(W) js_ReadWord(&(W))

static PRLock *_counter_lock;
#define UsingCounterLock 1

#undef AtomicAddBody
#define AtomicAddBody(P,I)                                                    \
    PR_Lock(_counter_lock);                                                   \
    *(P) += I;                                                                \
    PR_Unlock(_counter_lock)

#endif /* !ULTRA_SPARC */

JS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
#if defined(__GNUC__)
    unsigned int res;
#ifndef ULTRA_SPARC
    JS_ASSERT(nv != -1);
    asm volatile ("\
stbar\n\
swap [%1],%4\n\
1:  cmp %4,-1\n\
be,a 1b\n\
swap [%1],%4\n\
cmp %2,%4\n\
be,a 2f\n\
swap [%1],%3\n\
swap [%1],%4\n\
ba 3f\n\
mov 0,%0\n\
2:  mov 1,%0\n\
3:"
		  : "=r" (res)
		  : "r" (w), "r" (ov), "r" (nv), "r" (-1));
#else /* ULTRA_SPARC */
    JS_ASSERT(ov != nv);
    asm volatile ("\
stbar\n\
cas [%1],%2,%3\n\
cmp %2,%3\n\
be,a 1f\n\
mov 1,%0\n\
mov 0,%0\n\
1:"
		  : "=r" (res)
		  : "r" (w), "r" (ov), "r" (nv));
#endif /* ULTRA_SPARC */
    return (int)res;
#else /* !__GNUC__ */
    extern int compare_and_swap(jsword*,jsword,jsword);
#ifndef ULTRA_SPARC
    JS_ASSERT(nv != -1);
#else
    JS_ASSERT(ov != nv);
#endif
    return compare_and_swap(w,ov,nv);
#endif
}

#elif defined(AIX) && !defined(NSPR_LOCK)
#include <sys/atomic_op.h>

JS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    return !_check_lock((atomic_p)w,ov,nv);
}

#else

static PRLock *_counter_lock;
#define UsingCounterLock 1

#undef AtomicAddBody
#define AtomicAddBody(P,I)                                                    \
    PR_Lock(_counter_lock);                                                   \
    *(P) += I;                                                                \
    PR_Unlock(_counter_lock)

static PRLock *_compare_and_swap_lock;
#define UsingCompareAndSwapLock 1

JS_INLINE int
js_CompareAndSwap(jsword *w, jsword ov, jsword nv)
{
    int res = 0;

    PR_Lock(_compare_and_swap_lock);
    if (*w == ov) {
      *w = nv;
      res = 1;
    }
    PR_Unlock(_compare_and_swap_lock);
    return res;
}

#endif /* arch-tests */

JS_INLINE void
js_AtomicAdd(jsword *p, jsword i)
{
    AtomicAddBody(p,i);
}

jsword
js_CurrentThreadId()
{
    return CurrentThreadId();
}

void
js_NewLock(JSThinLock *tl)
{
#ifdef NSPR_LOCK
    tl->owner = 0;
    tl->fat = (JSFatLock*)JS_NEW_LOCK();
#else
    memset(tl, 0, sizeof(JSThinLock));
#endif
}

void
js_DestroyLock(JSThinLock *tl)
{
#ifdef NSPR_LOCK
    tl->owner = 0xdeadbeef;
    JS_DESTROY_LOCK(((JSLock*)tl->fat));
#else
    JS_ASSERT(tl->owner == 0);
    JS_ASSERT(tl->fat == NULL);
#endif
}

static void js_Dequeue(JSThinLock *);

JS_INLINE jsval
js_GetSlotWhileLocked(JSContext *cx, JSObject *obj, uint32 slot)
{
    jsval v;
#ifndef NSPR_LOCK
    JSScope *scope = OBJ_SCOPE(obj);
    JSThinLock *tl = &scope->lock;
    jsword me = cx->thread;
#endif

    JS_ASSERT(obj->slots && slot < obj->map->freeslot);
#ifndef NSPR_LOCK
    JS_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&tl->owner, 0, me)) {
	if (scope == OBJ_SCOPE(obj)) {
	    v = obj->slots[slot];
	    if (!js_CompareAndSwap(&tl->owner, me, 0)) {
		scope->count = 1;
		js_UnlockObj(cx,obj);
	    }
	    return v;
	}
	if (!js_CompareAndSwap(&tl->owner, me, 0))
	    js_Dequeue(tl);
    }
    else if (Thin_RemoveWait(ReadWord(tl->owner)) == me) {
	return obj->slots[slot];
    }
#endif
    js_LockObj(cx,obj);
    v = obj->slots[slot];
    js_UnlockObj(cx,obj);
    return v;
}

JS_INLINE void
js_SetSlotWhileLocked(JSContext *cx, JSObject *obj, uint32 slot, jsval v)
{
#ifndef NSPR_LOCK
    JSScope *scope = OBJ_SCOPE(obj);
    JSThinLock *tl = &scope->lock;
    jsword me = cx->thread;
#endif

    JS_ASSERT(obj->slots && slot < obj->map->freeslot);
#ifndef NSPR_LOCK
    JS_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&tl->owner, 0, me)) {
	if (scope == OBJ_SCOPE(obj)) {
	    obj->slots[slot] = v;
	    if (!js_CompareAndSwap(&tl->owner, me, 0)) {
		scope->count = 1;
		js_UnlockObj(cx,obj);
	    }
	    return;
	}
	if (!js_CompareAndSwap(&tl->owner, me, 0))
	    js_Dequeue(tl);
    }
    else if (Thin_RemoveWait(ReadWord(tl->owner)) == me) {
	obj->slots[slot] = v;
	return;
    }
#endif
    js_LockObj(cx,obj);
    obj->slots[slot] = v;
    js_UnlockObj(cx,obj);
}

static JSFatLock *
mallocFatlock()
{
    JSFatLock *fl = (JSFatLock *)malloc(sizeof(JSFatLock)); /* for now */
    if (!fl) return NULL;
    fl->susp = 0;
    fl->next = NULL;
    fl->prev = NULL;
    fl->slock = PR_NewLock();
    fl->svar = PR_NewCondVar(fl->slock);
    return fl;
}

static void
freeFatlock(JSFatLock *fl)
{
    PR_DestroyLock(fl->slock);
    PR_DestroyCondVar(fl->svar);
    free(fl);
}

static JSFatLock *
listOfFatlocks(int l)
{
    JSFatLock *m;
    JSFatLock *m0;
    int i;

    JS_ASSERT(l>0);
    m0 = m = mallocFatlock();
    for (i=1; i<l; i++) {
        m->next = mallocFatlock();
        m = m->next;
    }
    return m0;
}

static void
deleteListOfFatlocks(JSFatLock *m)
{
    JSFatLock *m0;
    for (; m; m=m0) {
	m0 = m->next;
	freeFatlock(m);
    }
}

static JSFatLockTable* _fl_tables;

static JSFatLock *
allocateFatlock(void *id)
{
    JSFatLock *m;

    int i = ((int)id/4)%_nr_of_globals;
    if (_fl_tables[i].free == NULL) {
  #ifdef DEBUG
        printf("Ran out of fat locks!\n");
  #endif
        _fl_tables[i].free = listOfFatlocks(10);
    }
    m = _fl_tables[i].free;
    _fl_tables[i].free = m->next;
    _fl_tables[i].free->prev = NULL;
    m->susp = 0;
    m->next = _fl_tables[i].taken;
    m->prev = NULL;
    if (_fl_tables[i].taken != NULL)
        _fl_tables[i].taken->prev = m;
    _fl_tables[i].taken = m;
    return m;
}

static void
deallocateFatlock(JSFatLock *m, void *id)
{
    int i = ((int)id/4)%_nr_of_globals;
    if (m == NULL)
	return;
    if (m->prev != NULL)
	m->prev->next = m->next;
    if (m->next != NULL)
	m->next->prev = m->prev;
    if (m == _fl_tables[i].taken)
	_fl_tables[i].taken = m->next;
    m->next = _fl_tables[i].free;
    _fl_tables[i].free = m;
}

int
js_SetupLocks(int l, int g)
{
    int i;

    if (_global_locks)
        return 1;
    if (l > 10000 || l < 0)       /* l equals number of initially allocated fat locks */
#ifdef DEBUG
        printf("Bad number %d in js_SetupLocks()!\n",l);
#endif
    if (g > 100 || g < 0)       /* g equals number of global locks */
#ifdef DEBUG
        printf("Bad number %d in js_SetupLocks()!\n",l);
#endif
    _nr_of_globals = g;
    _global_locks = (PRLock**)malloc(_nr_of_globals * sizeof(PRLock*));
    JS_ASSERT(_global_locks != NULL);
    for (i=0; i<_nr_of_globals; i++) {
        _global_locks[i] = PR_NewLock();
        JS_ASSERT(_global_locks[i] != NULL);
    }
#ifdef UsingCounterLock
    _counter_lock = PR_NewLock();
    JS_ASSERT(_counter_lock);
#endif
#ifdef UsingCompareAndSwapLock
    _compare_and_swap_lock = PR_NewLock();
    JS_ASSERT(_compare_and_swap_lock);
#endif
    _fl_tables = (JSFatLockTable*)malloc(_nr_of_globals * sizeof(JSFatLockTable));
    JS_ASSERT(_fl_tables != NULL);
    for (i=0; i<_nr_of_globals; i++) {
        _fl_tables[i].free = listOfFatlocks(l);
        _fl_tables[i].taken = NULL;
    }
    return 1;
}

/* pull in the cleanup function from jsdtoa.c */
extern void js_FinishDtoa(void);

void
js_CleanupLocks()
{
    int i;

    if (_global_locks != NULL) {
        for (i=0; i<_nr_of_globals; i++) {
            PR_DestroyLock(_global_locks[i]);
            deleteListOfFatlocks(_fl_tables[i].free);
            _fl_tables[i].free = NULL;
            deleteListOfFatlocks(_fl_tables[i].taken);
            _fl_tables[i].taken = NULL;
        }
        free(_fl_tables);
        _fl_tables = NULL;
        free(_global_locks);
        _global_locks = NULL;
#ifdef UsingCounterLock
        PR_DestroyLock(_counter_lock);
        _counter_lock = NULL;
#endif
#ifdef UsingCompareAndSwapLock
        PR_DestroyLock(_compare_and_swap_lock);
        _compare_and_swap_lock = NULL;
#endif
        js_FinishDtoa();
    }
}

JS_PUBLIC_API(void)
js_InitContextForLocking(JSContext *cx)
{
    cx->thread = CurrentThreadId();
    JS_ASSERT(Thin_GetWait(cx->thread) == 0);
}

/*
  Fast locking and unlocking is implemented by delaying the
  allocation of a system lock (fat lock) until contention. As long as
  a locking thread A runs uncontended, the lock is represented solely
  by storing A's identity in the object being locked.

  If another thread B tries to lock the object currently locked by A,
  B is enqueued into a fat lock structure (which might have to be
  allocated and pointed to by the object), and suspended using NSPR
  conditional variables (wait).  A wait bit (Bacon bit) is set in the
  lock word of the object, signalling to A that when releasing the
  lock, B must be dequeued and notified.

  The basic operation of the locking primitives (js_Lock(),
  js_Unlock(), js_Enqueue(), and js_Dequeue()) is
  compare-and-swap. Hence, when locking into p, if
  compare-and-swap(p,NULL,me) succeeds this implies that p is
  unlocked.  Similarly, when unlocking p, if
  compare-and-swap(p,me,NULL) succeeds this implies that p is
  uncontended (no one is waiting because the wait bit is not set).

  When dequeueing, the lock is released, and one of the threads
  suspended on the lock is notified.  If other threads still are
  waiting, the wait bit is kept (in js_Enqueue), and if not, the fat
  lock is deallocated.

  The functions js_Enqueue, js_Dequeque, js_SuspendThread, and js_ResumeThread
  are serialized using a global lock. For further
  scalability an array of global locks is used, which is index modulo the
  thin lock pointer.
*/

/* (i) global lock is held
   (ii) fl->susp >= 0
*/
static int
js_SuspendThread(JSThinLock *tl)
{
    JSFatLock *fl;
    JSStatus stat;

    if (tl->fat == NULL)
        fl = tl->fat = allocateFatlock(tl);
    else
        fl = tl->fat;
    JS_ASSERT(fl->susp >= 0);
    fl->susp++;
    PR_Lock(fl->slock);
    js_UnlockGlobal(tl);
    stat = (JSStatus)PR_WaitCondVar(fl->svar,PR_INTERVAL_NO_TIMEOUT);
    JS_ASSERT(stat != JS_FAILURE);
    PR_Unlock(fl->slock);
    js_LockGlobal(tl);
    fl->susp--;
    if (fl->susp == 0) {
        deallocateFatlock(fl,tl);
        tl->fat = NULL;
    }
    return tl->fat == NULL;
}

/* (i) global lock is held
   (ii) fl->susp > 0
*/
static void
js_ResumeThread(JSThinLock *tl)
{
    JSFatLock *fl = tl->fat;
    JSStatus stat;

    JS_ASSERT(fl != NULL);
    JS_ASSERT(fl->susp > 0);
    PR_Lock(fl->slock);
    js_UnlockGlobal(tl);
    stat = (JSStatus)PR_NotifyCondVar(fl->svar);
    JS_ASSERT(stat != JS_FAILURE);
    PR_Unlock(fl->slock);
}

static void
js_Enqueue(JSThinLock *tl, jsword me)
{
    jsword o, n;

    js_LockGlobal(tl);
    while (1) {
	o = ReadWord(tl->owner);
	n = Thin_SetWait(o);
	if (o != 0 && js_CompareAndSwap(&tl->owner,o,n)) {
	    if (js_SuspendThread(tl))
                me = Thin_RemoveWait(me);
	    else
		me = Thin_SetWait(me);
	}
	else if (js_CompareAndSwap(&tl->owner,0,me)) {
            js_UnlockGlobal(tl);
	    return;
	}
    }
}

static void
js_Dequeue(JSThinLock *tl)
{
    int o;

    js_LockGlobal(tl);
    o = ReadWord(tl->owner);
    JS_ASSERT(Thin_GetWait(o) != 0);
    JS_ASSERT(tl->fat != NULL);
    if (!js_CompareAndSwap(&tl->owner,o,0)) /* release it */
	JS_ASSERT(0);
    js_ResumeThread(tl);
}

JS_INLINE void
js_Lock(JSThinLock *tl, jsword me)
{
    JS_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&tl->owner, 0, me))
	return;
    if (Thin_RemoveWait(ReadWord(tl->owner)) != me)
	js_Enqueue(tl, me);
#ifdef DEBUG
    else
	JS_ASSERT(0);
#endif
}

JS_INLINE void
js_Unlock(JSThinLock *tl, jsword me)
{
    JS_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&tl->owner, me, 0))
	return;
    if (Thin_RemoveWait(ReadWord(tl->owner)) == me)
	js_Dequeue(tl);
#ifdef DEBUG
    else
	JS_ASSERT(0);
#endif
}

void
js_LockRuntime(JSRuntime *rt)
{
    PR_Lock(rt->rtLock);
#ifdef DEBUG
    rt->rtLockOwner = CurrentThreadId();
#endif
}

void
js_UnlockRuntime(JSRuntime *rt)
{
#ifdef DEBUG
    rt->rtLockOwner = 0;
#endif
    PR_Unlock(rt->rtLock);
}

static JS_INLINE void
js_LockScope1(JSContext *cx, JSScope *scope, jsword me)
{
    JSThinLock *tl;

    if (Thin_RemoveWait(ReadWord(scope->lock.owner)) == me) {
	JS_ASSERT(scope->count > 0);
	scope->count++;
    } else {
	tl = &scope->lock;
	JS_LOCK0(tl,me);
	JS_ASSERT(scope->count == 0);
	scope->count = 1;
    }
}

void
js_LockScope(JSContext *cx, JSScope *scope)
{
    JS_ASSERT(cx->thread == CurrentThreadId());
    js_LockScope1(cx,scope,cx->thread);
}

void
js_UnlockScope(JSContext *cx, JSScope *scope)
{
    jsword me = cx->thread;
    JSThinLock *tl;

    JS_ASSERT(scope->count > 0);
    if (Thin_RemoveWait(ReadWord(scope->lock.owner)) != me) {
	JS_ASSERT(0);
	return;
    }
    if (--scope->count == 0) {
	tl = &scope->lock;
	JS_UNLOCK0(tl,me);
    }
}

void
js_TransferScopeLock(JSContext *cx, JSScope *oldscope, JSScope *newscope)
{
    jsword me;
    JSThinLock *tl;

    JS_ASSERT(JS_IS_SCOPE_LOCKED(newscope));

    /*
     * If the last reference to oldscope went away, newscope needs no lock
     * state update.
     */
    if (!oldscope)
	return;
    JS_ASSERT(JS_IS_SCOPE_LOCKED(oldscope));

    /*
     * Transfer oldscope's entry count to newscope, as it will be unlocked
     * now via JS_UNLOCK_OBJ(cx,obj) calls made while we unwind the C stack
     * from the current point (under js_GetMutableScope).
     */
    newscope->count = oldscope->count;

    /*
     * Reset oldscope's lock state so that it is completely unlocked.
     */
    oldscope->count = 0;
    tl = &oldscope->lock;
    me = cx->thread;
    JS_UNLOCK0(tl,me);
}

void
js_LockObj(JSContext *cx, JSObject *obj)
{
    JSScope *scope;
    jsword me = cx->thread;
    JS_ASSERT(me == CurrentThreadId());
    for (;;) {
        scope = OBJ_SCOPE(obj);
        js_LockScope1(cx, scope, me);

        /* If obj still has this scope, we're done. */
        if (scope == OBJ_SCOPE(obj))
            return;

        /* Lost a race with a mutator; retry with obj's new scope. */
        js_UnlockScope(cx, scope);
    }
}

void
js_UnlockObj(JSContext *cx, JSObject *obj)
{
    js_UnlockScope(cx, OBJ_SCOPE(obj));
}

#ifdef DEBUG
JSBool
js_IsRuntimeLocked(JSRuntime *rt)
{
    return CurrentThreadId() == rt->rtLockOwner;
}

JSBool
js_IsObjLocked(JSObject *obj)
{
    JSScope *scope = OBJ_SCOPE(obj);

    return MAP_IS_NATIVE(&scope->map) &&
           CurrentThreadId() == Thin_RemoveWait(ReadWord(scope->lock.owner));
}

JSBool
js_IsScopeLocked(JSScope *scope)
{
    return CurrentThreadId() == Thin_RemoveWait(ReadWord(scope->lock.owner));
}
#endif

#undef ReadWord
#endif /* JS_THREADSAFE */
