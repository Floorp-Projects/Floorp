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

#ifdef JS_THREADSAFE

/*
 * JS locking stubs.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include "jspubtd.h"
#include "prthread.h"
#include "prlog.h"
#include "jscntxt.h"
#include "jsscope.h"
#include "jspubtd.h"
#include "jslock.h"

static PRLock *_global_lock;

static void
js_LockGlobal() 
{
  PR_Lock(_global_lock);
}

static void
js_UnlockGlobal() 
{
  PR_Unlock(_global_lock);
}

#define AtomicRead(W) (W)
#define AtomicAddBody(P,I)\
    prword n;\
    do {\
	n = AtomicRead(*(P));\
    } while (!js_CompareAndSwap(P, n, n + I));

#if defined(_WIN32) && !defined(NSPR_LOCK)
#pragma warning( disable : 4035 )

PR_INLINE int
js_CompareAndSwap(prword *w, prword ov, prword nv)
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

#elif defined(SOLARIS) && !defined(NSPR_LOCK)

#ifndef ULTRA_SPARC
PR_INLINE prword
js_AtomicRead(prword *p)
{
    prword n;
    while ((n = *p) == -1)
	;
    return n;
}

#undef AtomicRead
#define AtomicRead(W) js_AtomicRead(&(W))

static PRLock *_counter_lock;
#define UsingCounterLock 1

#undef AtomicAddBody
#define AtomicAddBody(P,I) \
    PR_Lock(_counter_lock);\
    *(P) += I;\
    PR_Unlock(_counter_lock);

#endif /* !ULTRA_SPARC */

PR_INLINE int
js_CompareAndSwap(prword *w, prword ov, prword nv)
{
#if defined(__GNUC__)
    unsigned int res;
#ifndef ULTRA_SPARC
    PR_ASSERT(nv >= 0);
    asm volatile ("
swap [%1],%4
1:  tst %4
bneg,a 1b
swap [%1],%4
cmp %2,%4
be,a 2f
swap [%1],%3
swap [%1],%4
ba 3f
mov 0,%0
2:  mov 1,%0
3:" 
		  : "=r" (res) 
		  : "r" (w), "r" (ov), "r" (nv), "r" (-1));
#else /* ULTRA_SPARC */
    PR_ASSERT(ov != nv);
    asm volatile ("
cas [%1],%2,%3
cmp %2,%3
be,a 1f
mov 1,%0
mov 0,%0
1:" 
		  : "=r" (res) 
		  : "r" (w), "r" (ov), "r" (nv));
#endif /* ULTRA_SPARC */
    return (int)res;
#else /* !__GNUC__ */
    extern int compare_and_swap(prword*,prword,prword);
#ifndef ULTRA_SPARC
    PR_ASSERT(nv >= 0);
#else
    PR_ASSERT(ov != nv);
#endif    
    return compare_and_swap(w,ov,nv);
#endif
}

#elif defined(AIX) && !defined(NSPR_LOCK)

PR_INLINE int
js_CompareAndSwap(prword *w, prword ov, prword nv)
{
/*
  int i = -1;

  lwarx reg,0,w
  cmpw reg,ov
  bne no
  stwcx. nv,0,w
  beq yes
  no:
  li res,0
  yes:
  */
    extern int compare_and_swap(int*,int*,int);

    return compare_and_swap(w,&ov,nv);
}

#else

static PRLock *_counter_lock;
#define UsingCounterLock 1

#undef AtomicAddBody
#define AtomicAddBody(P,I) \
    PR_Lock(_counter_lock);\
    *(P) += I;\
    PR_Unlock(_counter_lock);

static PRLock *_compare_and_swap_lock;
#define UsingCompareAndSwapLock 1

PR_INLINE int
js_CompareAndSwap(prword *w, prword ov, prword nv)
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

PR_INLINE void
js_AtomicAdd(prword *p, prword i)
{
    AtomicAddBody(p,i);
}

prword
js_CurrentThreadId()
{
    return CurrentThreadId();
}

void
js_NewLock(JSThinLock *p)
{
#ifdef NSPR_LOCK
    p->owner = 0;
    p->fat = (JSFatLock*)JS_NEW_LOCK();
#else
    memset(p, 0, sizeof(JSThinLock));
#endif
}

void
js_DestroyLock(JSThinLock *p)
{
#ifdef NSPR_LOCK
    p->owner = 0xdeadbeef;
    JS_DESTROY_LOCK(((JSLock*)p->fat));
#else
    PR_ASSERT(p->owner == 0);
    PR_ASSERT(p->fat == NULL);
#endif
}

static void js_Dequeue(JSThinLock *);

PR_INLINE jsval
js_GetSlotWhileLocked(JSContext *cx, JSObject *obj, uint32 slot) 
{
    jsval v;
#ifndef NSPR_LOCK
    JSScope *scp = (JSScope *)obj->map;
    JSThinLock *p = &scp->lock;
    prword me = cx->thread;
#endif

    PR_ASSERT(obj->slots && slot < obj->map->freeslot);
#ifndef NSPR_LOCK
    PR_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&p->owner, 0, me)) {
	if (scp == (JSScope *)obj->map) {
	    v = obj->slots[slot];
	    if (!js_CompareAndSwap(&p->owner, me, 0)) {
		scp->count = 1;
		js_UnlockObj(cx,obj);
	    }
	    return v;
	}
	if (!js_CompareAndSwap(&p->owner, me, 0))
	    js_Dequeue(p);
    }
    else if (Thin_RemoveWait(AtomicRead(p->owner)) == me) {
	return obj->slots[slot];
    }
#endif
    js_LockObj(cx,obj);
    v = obj->slots[slot];
    js_UnlockObj(cx,obj);
    return v;
}

PR_INLINE void
js_SetSlotWhileLocked(JSContext *cx, JSObject *obj, uint32 slot, jsval v) 
{
#ifndef NSPR_LOCK
    JSScope *scp = (JSScope *)obj->map;
    JSThinLock *p = &scp->lock;
    prword me = cx->thread;
#endif

    PR_ASSERT(obj->slots && slot < obj->map->freeslot);
#ifndef NSPR_LOCK
    PR_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&p->owner, 0, me)) {
	if (scp == (JSScope *)obj->map) {
	    obj->slots[slot] = v;
	    if (!js_CompareAndSwap(&p->owner, me, 0)) {
		scp->count = 1;
		js_UnlockObj(cx,obj);
	    }
	    return;
	}
	if (!js_CompareAndSwap(&p->owner, me, 0))
	    js_Dequeue(p);
    }
    else if (Thin_RemoveWait(AtomicRead(p->owner)) == me) {
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
    PR_ASSERT(fl);
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

static int 
js_SuspendThread(JSThinLock *p)
{
    JSFatLock *fl = p->fat;
    PRStatus stat;
    if (fl == NULL)
	return 1;
    PR_Lock(fl->slock);
    fl->susp++;
    if (p->fat == NULL || fl->susp<1) {
		PR_Unlock(fl->slock);
		return 1;
    }
    stat = PR_WaitCondVar(fl->svar,PR_INTERVAL_NO_TIMEOUT); 
    if (stat == PR_FAILURE) {
		fl->susp--;
		return 0;
    }
    PR_Unlock(fl->slock);
    return 1;
}

static void
js_ResumeThread(JSThinLock *p)
{
    JSFatLock *fl = p->fat;
    PRStatus stat;
    PR_Lock(fl->slock);
    fl->susp--; 
    stat = PR_NotifyCondVar(fl->svar);
    PR_ASSERT(stat != PR_FAILURE);
    PR_Unlock(fl->slock);
}

static JSFatLock * 
listOfFatlocks(int l) 
{
    JSFatLock *m;
    JSFatLock *m0;
	int i;

    PR_ASSERT(l>0);
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

static JSFatLockTable _fl_table;

JSFatLock *
allocateFatlock() 
{
  JSFatLock *m;

  if (_fl_table.free == NULL) {
#ifdef DEBUG
      printf("Ran out of fat locks!\n");
#endif
      _fl_table.free = listOfFatlocks(10);
  }
  m = _fl_table.free;
  _fl_table.free = m->next;
  _fl_table.free->prev = NULL;
  m->susp = 0;
  m->next = _fl_table.taken;
  m->prev = NULL;
  if (_fl_table.taken != NULL)
	_fl_table.taken->prev = m;
  _fl_table.taken = m;
  return m;
}

void 
deallocateFatlock(JSFatLock *m) 
{
  if (m == NULL)
	return;
  if (m->prev != NULL)
	m->prev->next = m->next;
  if (m->next != NULL)
	m->next->prev = m->prev;
  if (m == _fl_table.taken)
	_fl_table.taken = m->next;
  m->next = _fl_table.free;
  _fl_table.free = m;
}

JS_PUBLIC_API(int)
js_SetupLocks(int l)
{
  if (l > 10000)       /* l equals number of initially allocated fat locks */
#ifdef DEBUG
    printf("Number %d very large in js_SetupLocks()!\n",l);
#endif
  if (_global_lock)
    return 1;
  _global_lock = PR_NewLock();
  PR_ASSERT(_global_lock);
#ifdef UsingCounterLock
  _counter_lock = PR_NewLock();
  PR_ASSERT(_counter_lock);
#endif
#ifdef UsingCompareAndSwapLock
  _compare_and_swap_lock = PR_NewLock();
  PR_ASSERT(_compare_and_swap_lock);
#endif
  _fl_table.free = listOfFatlocks(l);
  _fl_table.taken = NULL;
  return 1;
}

JS_PUBLIC_API(void)
js_CleanupLocks()
{
  if (_global_lock != NULL) {
    deleteListOfFatlocks(_fl_table.free);
    _fl_table.free = NULL;
    deleteListOfFatlocks(_fl_table.taken);
    _fl_table.taken = NULL;
    PR_DestroyLock(_global_lock);
    _global_lock = NULL;
#ifdef UsingCounterLock
    PR_DestroyLock(_counter_lock);
    _counter_lock = NULL;
#endif
#ifdef UsingCompareAndSwapLock
    PR_DestroyLock(_compare_and_swap_lock);
    _compare_and_swap_lock = NULL;
#endif
  }
}

JS_PUBLIC_API(void)
js_InitContextForLocking(JSContext *cx)
{
	cx->thread = CurrentThreadId();
	PR_ASSERT(Thin_GetWait(cx->thread) == 0);
}

/*

  It is important that emptyFatlock() clears p->fat in the empty case
  while holding p->slock. This serializes the access of p->fat wrt
  js_SuspendThread(), which also requires p->slock.  There would
  otherwise be a race condition as follows.  Thread A is about to
  clear p->fat, having woken up after being suspended in a situation
  where no other thread has yet suspended on p. However, suppose
  thread B is about to suspend on p, having just released the global
  lock, and is currently calling js_SuspendThread(). In the unfortunate
  case where A precedes B ever so slightly, A will think that it
  should clear p->fat (being currently empty but not for long), at the
  same time as B is about to suspend on p->fat.  Now, A deallocates
  p->fat while B is about to suspend on it. Thus, the suspension of B
  is lost and B will not be properly activated.

  Using p->slock as below (and correspondingly in js_SuspendThread()),
  js_SuspendThread() will notice that p->fat is empty, and hence return
  immediately. 

  */

int
emptyFatlock(JSThinLock *p) 
{
  JSFatLock *fl = p->fat;
  int i;
  
  if (fl == NULL)
	return 1;
  PR_Lock(fl->slock);
  i = fl->susp;
  if (i < 1) {
	fl->susp = -1;
	deallocateFatlock(fl);
	p->fat = NULL;
  }
  PR_Unlock(fl->slock);
  return i < 1;
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

  Furthermore, when enqueueing (after the compare-and-swap has failed
  to lock the object), a global spin lock is grabbed which serializes
  changes to an object, and prevents the race condition when a lock is
  released simultaneously with a thread suspending on it (these two
  operations are serialized using the spin lock). Thus, a thread is
  enqueued if the lock is still held by another thread, and otherwise
  the lock is grabbed using compare-and-swap. When dequeueing, the
  lock is released, and one of the threads suspended on the lock is
  notified.  If other threads still are waiting, the wait bit is kept, and if
  not, the fat lock is deallocated (in emptyFatlock()).

*/

static void
js_Enqueue(JSThinLock *p, prword me)
{
  prword o, n;

  js_LockGlobal();
  while (1) {
	o = AtomicRead(p->owner);
	n = Thin_SetWait(o);
	if (o != 0 && js_CompareAndSwap(&p->owner,o,n)) {
	  if (p->fat == NULL)
		p->fat = allocateFatlock();
#ifdef DEBUG_
	  printf("Going to suspend (%x) on {%x,%x}\n",me,p,p->fat);
#endif
	  js_UnlockGlobal();
	  js_SuspendThread(p);
	  js_LockGlobal();
#ifdef DEBUG_
	  printf("Waking up (%x) on {%x,%x,%d}\n",me,p,p->fat,(p->fat ? p->fat->susp : 42));
#endif
	  if (emptyFatlock(p))
		me = Thin_RemoveWait(me);
	  else
		me = Thin_SetWait(me);
	}
	else if (js_CompareAndSwap(&p->owner,0,me)) {
	  js_UnlockGlobal();
	  return;
	}
  }
}

static void
js_Dequeue(JSThinLock *p)
{
  js_LockGlobal();
  PR_ASSERT(Thin_GetWait(AtomicRead(p->owner)));
  p->owner = 0;			/* release it */
  PR_ASSERT(p->fat);
  js_ResumeThread(p);
  js_UnlockGlobal();
}

PR_INLINE void
js_Lock(JSThinLock *p, prword me)
{
#ifdef DEBUG_
    printf("\nT%x about to lock %x\n",me,p);
#endif
    PR_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&p->owner, 0, me))
		return;
    if (Thin_RemoveWait(AtomicRead(p->owner)) != me)
		js_Enqueue(p, me);
#ifdef DEBUG
    else
	PR_ASSERT(0);
#endif
}

PR_INLINE void
js_Unlock(JSThinLock *p, prword me) 
{
#ifdef DEBUG_
    printf("\nT%x about to unlock %p\n",me,p);
#endif
    PR_ASSERT(me == CurrentThreadId());
    if (js_CompareAndSwap(&p->owner, me, 0))
	return;
    if (Thin_RemoveWait(AtomicRead(p->owner)) == me)
	js_Dequeue(p);
#ifdef DEBUG
    else
	PR_ASSERT(0);
#endif
}

void
js_LockRuntime(JSRuntime *rt)
{
    prword me = CurrentThreadId();
    JSThinLock *p;

    PR_ASSERT(Thin_RemoveWait(AtomicRead(rt->rtLock.owner)) != me);
    p = &rt->rtLock;
    JS_LOCK0(p,me);
}

void
js_UnlockRuntime(JSRuntime *rt)
{
    prword me = CurrentThreadId();
    JSThinLock *p;

    PR_ASSERT(Thin_RemoveWait(AtomicRead(rt->rtLock.owner)) == me);
    p = &rt->rtLock;
    JS_UNLOCK0(p,me);
}

PR_INLINE void
js_LockScope1(JSContext *cx, JSScope *scope, prword me)
{
    JSThinLock *p;

    if (Thin_RemoveWait(AtomicRead(scope->lock.owner)) == me) {
	PR_ASSERT(scope->count > 0);
	scope->count++;
    } else {
	p = &scope->lock;
	JS_LOCK0(p,me);
	PR_ASSERT(scope->count == 0);
	scope->count = 1;
    }
}

void
js_LockScope(JSContext *cx, JSScope *scope)
{
    PR_ASSERT(cx->thread == CurrentThreadId());
    js_LockScope1(cx,scope,cx->thread);
}

void
js_UnlockScope(JSContext *cx, JSScope *scope)
{
    prword me = cx->thread;
    JSThinLock *p;

    PR_ASSERT(scope->count > 0);
    if (Thin_RemoveWait(AtomicRead(scope->lock.owner)) != me) {
	PR_ASSERT(0);
	return;
    }
    if (--scope->count == 0) {
	p = &scope->lock;
	JS_UNLOCK0(p,me);
    }
}

void
js_TransferScopeLock(JSContext *cx, JSScope *oldscope, JSScope *newscope)
{
    prword me;
    JSThinLock *p;

    PR_ASSERT(JS_IS_SCOPE_LOCKED(newscope));
    /*
     * If the last reference to oldscope went away, newscope needs no lock
     * state update.
     */
    if (!oldscope) {
	return;
    }
    PR_ASSERT(JS_IS_SCOPE_LOCKED(oldscope));
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
    p = &oldscope->lock;
    me = cx->thread;
    JS_UNLOCK0(p,me);
}

void
js_LockObj(JSContext *cx, JSObject *obj)
{
    JSScope *scope;
    prword me = cx->thread;
    PR_ASSERT(me == CurrentThreadId());
    for (;;) {
		scope = (JSScope *) obj->map;
		js_LockScope1(cx, scope, me);

		/* If obj still has this scope, we're done. */
		if (scope == (JSScope *) obj->map)
			return;

		/* Lost a race with a mutator; retry with obj's new scope. */
		js_UnlockScope(cx, scope);
    }
}

void
js_UnlockObj(JSContext *cx, JSObject *obj)
{
    js_UnlockScope(cx, (JSScope *) obj->map);
}

#ifdef DEBUG
JSBool
js_IsRuntimeLocked(JSRuntime *rt)
{
    return CurrentThreadId() == Thin_RemoveWait(AtomicRead(rt->rtLock.owner));
}

JSBool
js_IsObjLocked(JSObject *obj)
{
    JSObjectMap *map = obj->map;

    return MAP_IS_NATIVE(map) && CurrentThreadId() == Thin_RemoveWait(AtomicRead(((JSScope *)map)->lock.owner));
}

JSBool
js_IsScopeLocked(JSScope *scope)
{
    return CurrentThreadId() == Thin_RemoveWait(AtomicRead(scope->lock.owner));
}
#endif

#endif /* JS_THREADSAFE */
