/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "Monitor.h"
#include "Exceptions.h"
#include "SysCallsRuntime.h"
#include "prprf.h"

/*
	Author: Bjorn Carlson (bjorn@netscape.com)

	This is the new implementation:
	Each object has one state word in which we can place information
	regarding the status of the lock. This might later be a word
	shared with other parts of the system such as the gc. At this point, we are 
	assuming that we can use three bits. Hence;

  Obj: ------------
	  | type       |
	   ------------
	  | hash/gc bb|
	   ------------

	where the lock state is captured in the two bits 'bb'.

	The lock state is as follows:
	case 01: 
		Obj is unlocked and the remaining 30 bits are used as a hash code (shifted by 2).

	case 11:
		Obj is unlocked and the remaining 30 bits index an entry in the hash code
		vector. If the code is 0, the object has not yet received its hash code 
		(strings are assigned a code lazily).

	case 00:
		Obj is locked and the remaining 30 bits point to a stack frame entry containing
		the old contents of the object. As a special case we use the word 0 for signalling
		that the object is currently busy, which we use when updating its hash code.

	case 10:
		Obj is locked, some thread is waiting, and the remaining 30 bits point to
		the stack as above.

	For the specific algorithms for reading and writing hash codes, and locking and
	unlocking monitors, see below.

	When a thread is created (from inside java.lang.Thread, or
	externally), the initial function should be Thread::run, so that appropriate
	initialization takes place.

	When a thread dies (is interrupted), I try to catch this in the calls to
	NSPR which fails in such a case. At failure, I try to take the appropriate
	action. A thread may die, however, waiting for I/O or hanging off some NSPR 
	lock outside our monitors.

  */

Monitors<MonitorObject>* monitors;

inline bool Monitor::cmpAndSwap(Int32* w, Int32 ov, Int32 nv)
{
	bool r=false;
#if defined(_WIN32) && !defined(__GNUC__)
	__asm   {
		mov eax,ov
		mov ecx, nv
		mov ebx, w
		lock cmpxchg [ebx], ecx
		sete r
	}
#elif defined(__i386__)
    /* This actually only works 486 and better...  */
    {
      Int32 readval;

      __asm__ __volatile__ ("lock; cmpxchgl %3, %1; sete %0"
                            : "=q" (r), "=m" (*w), "=a" (readval)
                            : "r" (nv), "m" (*w), "0" (r), "2" (ov));
    }
#else
	ov;
	PR_fprintf(PR_STDOUT,"compare and swap NYI!!\n");
	*w = nv;
    r = true;
#endif
	return r;
}

inline Int32 Monitor::pushStack() { // for now
	return monitors->pushStack(); 
	// should be something like:
	// __asm { 
	//     mov ecx,v
	//     mov [esp]Context.lock,ecx
	// }
}

inline Int32 Monitor::popStack() { // for now
	return monitors->popStack();
	// should be something like:
	// __asm { 
	//     mov eax,[esp]Context.lock
	// }
}

inline bool Monitor::inStack(Int32 v) {
	return monitors->inStack(v);
	// Use #ifdef HAVE_STACK_GROWING_UP
	// and something like:
	// p = beginning of current stack;
	// __asm { 
	//     cmp esp,v
	//     setb r1
	//     mov ecx,p
	//     cmp ecx,v
	//     seta r2
	//     mov eax,r1
	//     and eax,r2
	// }
}

void Monitor::staticInit() 
{
	monitors = new Monitors<MonitorObject>(10);
	threadInit();
}

void Monitor::destroy() 
{
	delete monitors;
}

inline void Monitor::threadInit() 
{ 
	monitors->threadInit(); 
}

// locks the monitor part of object o.
/*	the algorithm is as follows:
	get a pointer to the state slot of the object, w, and the stack entry, sp, in which to 
	save the old value of *w. Now;
	1. if *w is 0, then the monitor is busy and we spin.
	2. if *w is unlocked (an odd number), then we optimistically save *w to sp, and
	try to swap sp into w. If this succeeds, we now own the monitor. If it fails, we
	loop back to 1.
	3. if *w is locked, we check if we own the lock, by checking if the pointer stored in 
	*w (v) points into our stack. If it does, we save 0 onto the stack, and return. If it
	doesn't, we enqueue ourselves onto the suspension list of the monitor. For more on this,
	see enqueue() (there is a special case when the lock is released while we are trying to
	suspend, which is dealt with in enqueue()). The variable 'wl' is used for keeping the
	waiting bit set when returning from enqueue, if we see that more threads are waiting when
	we wake up. Hence, or:ing 'wl' with 'sp' will keep the bit set if needed. When we return
	from enqueue() we loop back to 1.
 */
void Monitor::lock(JavaObject &o)
{
	Int32* w;
	Int32 v,sp,wl=0;
	
#ifdef DEBUG_
	PR_fprintf(PR_STDOUT,"\nT%x trying to lock %x\n",PR_GetCurrentThread(),&o);
#endif
	w = &o.state;
	sp = pushStack();
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		if ((v&Unlocked) != 0) { // unlocked
			// write to stack entry
			*(Int32*)sp = v;
			sp |= wl;
			if (cmpAndSwap(w,v,sp))
				return;
			continue; // try again
		}
		v &= ~LockMask;
		if (inStack(v)) { // I own it
			*(Int32*)sp = 0;
			return;
		}
		wl = monitors->enqueue(o);
	}
}

// unlocking a monitor which we must own.
// The algorithm is:
/*
	1. Pop the stack entry corresponding to this lock. If 0, then return.
	2. Otherwise, check the state *w of the object to see if it is busy, and
	if so, spin. Otherwise, we check to see if we own the lock. If not, an
	exception is thrown.
	3. finally, we try to swap back top into w, comparing with the pointer v. If 
	this fails, it is either because the monitor is busy, or the waiting bit is set
	(the waiting bit resides outside the bitpattern for the pointer, and hence masking
	the pointer with ~LockMask, will remove the waiting bit from 'v'). Then, dequeue()
	is called, which will resume a thread if one exists, and release the monitor.
 */
void Monitor::unlock(JavaObject &o)
{
	Int32* w;
	Int32 v,top;

#ifdef DEBUG_
	PR_fprintf(PR_STDOUT,"\nT%x unlocking %x\n",PR_GetCurrentThread(),&o);
#endif
	w = &o.state;
	top = popStack();
	if (top == 0)
		return;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		v &= ~LockMask;
		if (!inStack(v)) {
			sysThrowNamedException("java/lang/IllegalMonitorStateException"); // doesn't return
			return;
		}
		if (!cmpAndSwap(w,v,top))
			monitors->dequeue(o,top);
		return;
	}
}

// locks the monitor part of object o for wait():s sake.
// This is a replica of lock() but where we don't save to the stack (done
// previously since we used to own this lock), and we don't check if we own
// the monitor because we don't (having just been wakened inside wait()).
// For more, see wait().
void Monitor::wlock(JavaObject &o, Int32 sp)
{
	Int32* w;
	Int32 v,wl=0;
	
#ifdef DEBUG_
	PR_fprintf(PR_STDOUT,"\nT%x trying to w-lock %x\n",PR_GetCurrentThread(),&o);
#endif
	w = &o.state;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		if ((v&Unlocked) != 0) { // unlocked
			sp |= wl;
			if (cmpAndSwap(w,v,sp))
				return;
			continue; // try again
		}
		wl = monitors->enqueue(o);
	}
}

// waits for someone to notify o within 'tim'. If we don't own the monitor,
// an exception is thrown.
void Monitor::wait(JavaObject &o, Int64 tim)
{
	Int32* w;
	Int32 v, t;

	if (tim < 0) {
		sysThrowNamedException("java/lang/IllegalArgumentException");
		return;
	}
	t = (Int32)tim;
	w = &o.state;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		v &= ~LockMask;
		if (!inStack(v)) {
			sysThrowNamedException("java/lang/IllegalMonitorStateException");			
			return;
		}
#ifdef DEBUG
		PR_fprintf(PR_STDOUT,"About to wait for %d\n",t);
#endif
		monitors->wait(o,v,t);
#ifdef DEBUG
		PR_fprintf(PR_STDOUT,"finished waiting\n");
#endif
		return;
	}
}

// notifies someone waiting on o
void Monitor::notify(JavaObject &o)
{
	Int32* w;
	Int32 v;

	w = &o.state;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		v &= ~LockMask;
		if (!inStack(v)) {
			sysThrowNamedException("java/lang/IllegalMonitorStateException");
			return;
		}
		monitors->notify(o);
		return;
	}
}

// notifies everyone waiting on o
void Monitor::notifyAll(JavaObject &o)
{
	Int32* w;
	Int32 v;

	w = &o.state;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		v &= ~LockMask;
		if (!inStack(v)) {
			sysThrowNamedException("java/lang/IllegalMonitorStateException");
			return;
		}
		monitors->notifyAll(o);
		return;
	}
}

// reading a hash code is done as follows:
/*
	1. get a pointer to the object state slot, w.
	2. spin, if the monitor is busy.
	3. if the monitor is locked by someone then;
	3.1 if someone else owns it, make the monitor busy, and
	read from the other thread's stack entry or the object itself (if it was
	released while we are trying to read it). Then restore w back to non-busy.
	3.2 if we own it, read from our stack.
	4. if the hash value of the object is extended, get the value from the table.
	Otherwise,  shift it appropriately, and return that value.
 */
Int32 Monitor::hashRead(JavaObject& obj) {
	Int32* w;
	Int32 v, ov;

	w = &obj.state;
	while (true) {
		v = *w;
		if (v == 0)
			continue;
		if ((v&Unlocked) == 0) {  // locked by someone
			v &= ~LockMask;
			if (!inStack(v)) {    // some other thread
				while ((ov = exchange(w,0)) == 0) // make it busy
					;
				if ((ov&Unlocked) == 0) 
					v = *(Int32*)(ov & ~LockMask);
				else 
					v = ov;
				*w = ov;
			}
			else                  // current thread
				v = *(Int32*)v;
		}
		if ((v&XHash) == 0)
			return v>>KeyShift;
		else
			return monitors->hashGet(v);
	}
}

// writing a hash code is done as:
/*
	1. decide if we need to allocate an extended code, and if so,
	allocate an entry optimistically.
	2. make the monitor busy.
	3. if the monitor is locked:
	3.1. if the value is still unset, update the stack entry with the new code.
	3.2 If set, deallocate the new code since we don't need it then.
	4. if unlocked, then update the old value. 
	5. finally, write back the old value.
 */
void Monitor::hashWrite(JavaObject& obj, Int32 k) {
	Int32* w;
	Int32 ov, v;

	w = &obj.state;
	if (k >= (1<<KeySize))
		k = monitors->hashAdd(k) | XHash;
	else
		k = (k<<KeyShift) | IHash;
	while ((ov = exchange(w,0)) == 0) // make it busy
		;
	if ((ov&Unlocked) == 0) { // locked by someone
		v = (ov & ~LockMask);
		if ((*(Int32*)v & ~LockMask) == 0) // still unwritten
			*(Int32*)v = k;
		else  // someone else beat us to it
			monitors->hashRemove(k);
	}
	else // unlocked
		ov = k;
	*w = ov;
}

Int32 Monitor::hashCode(JavaObject& obj) {
	Int32 v = hashRead(obj);
	if (v == 0) {
		v = monitors->nextHash();
		hashWrite(obj,v);
	}
	return v;
}

// Monitors::
template<class M>
Monitors<M>::Monitors(int l) : pool(), cacheNext(0), taken(NULL), stateNext(1), hashNext(1), hashVecTop(0), hashVecSize(64) {
	int i;
	PRStatus stat;

	if (l > 10000) 
		sysThrowNamedException("java/lang/IllegalArgumentException");
	free = newList(l);
	_lock = PR_NewLock();
	table = new PointerHashTable<M*>(pool);
	for (i=0; i<cacheSize; i+=2) {
		keyCache[i] = 0;
		keyCache[i+1] = 0;
	}
	hashVec = new(pool) Int32[hashVecSize];
	hashVec[hashVecTop++] = 0; // default
	stat = PR_NewThreadPrivateIndex(&thr_stack,NULL);
	assert(stat != PR_FAILURE);
	stat = PR_NewThreadPrivateIndex(&thr_top,NULL);
	assert(stat != PR_FAILURE);
}

template<class M>
Monitors<M>::~Monitors() {
	delete table;
	PR_DestroyLock(_lock);
}

template<class M>
M* Monitors<M>::allocate(JavaObject& o) {
	M* m;
	if (free == NULL) 
		free = newList(10);
	m = free;
	m->init(o);
	free = free->next;
	m->next = taken;
	m->prev = NULL;
	if (taken != NULL)
		taken->prev = m;
	taken = m;
	return m;
}

template<class M>
void Monitors<M>::deallocate(M* m) {
	if (m->prev != NULL)
		m->prev->next = m->next;
	if (m->next != NULL)
		m->next->prev = m->prev;
	if (m == taken)
		taken = m->next;
	m->next = free;
	free = m;
}

template<class M>
M* Monitors<M>::newList(int l) {
	M* m;
	M* m0;
	assert(l>0);
	m0 = m = new(pool) M;
	for (int i=1; i<l; i++) {
		m->next = new(pool) M;
		m = m->next;
	}
	return m0;
}

template<class M>
int Monitors<M>::enqueue(JavaObject& o) {
	M* m;

	// enqueue does ALL the deallocation of fat monitors, so that if a monitor
	// is released in unlock()/dequeue() we deallocate it when waking up.
	// we must deal with two cases: 
	// (i) we must suspend, and thus we might need to allocate a monitor object
	// (ii) the object was just released, and we thus go back to lock() and try again
	Int32* w = &o.state;
	Int32 v = *w;
	if (v!=0 && // not busy
		(v&Monitor::Unlocked) == 0 && // still locked
		Monitor::cmpAndSwap(w,v,v|Monitor::Waiting)) { // swap in waiting bit
		lock(); // now allocate a fat monitor
		if ((m = getObject(o)) == NULL) {
			m = allocate(o);
			addObject(o,m);
		}
		unlock();
#ifdef DEBUG
		PR_fprintf(PR_STDOUT,"\n T%x going to sleep on %x (%d)\n",PR_GetCurrentThread(),w,*w&Monitor::LockMask);
#endif
		// puts current thread to sleep. Tries locking again at wakeup.
		// suspend and resume are sticky, so that if the lock was released while we
		// were trying to suspend, we return immediately (see MonitorObject::suspend()).
		if (!m->suspend()) { // if failure, it was interrupted
			if (m->empty()) { // no more state. Deallocate monitor
				deallocate(m);
				removeObject(o);
			}
			sysThrowNamedException("java/lang/InterruptedException");
			return 0; // never reached
		}
#ifdef DEBUG
		PR_fprintf(PR_STDOUT,"\n T%x waking up at %x (%d)\n",PR_GetCurrentThread(),w,*w&Monitor::LockMask);
#endif
		v = 0;
		if (m->empty()) { // no more state. Deallocate monitor
			deallocate(m);
			removeObject(o);
		}
		else if (m->isSuspended())
			v = Monitor::Waiting; // return waiting bit to lock() to preserve it
		return v;
	}
	return 0;
}

// dequeue looks for a fat monitor for o, and if one is found, it resumes it.
// This sticks, so that a concurrent suspend will return immediately if the resume
// beats it. Finally, the monitor is released by putting back the old value in,
// which was picked up from the stack ('top' in unlock()).
template<class M>
void Monitors<M>::dequeue(JavaObject& o,Int32 v) {
	M* m;
	Int32 ov;
	Int32* w = &o.state;

	do
		ov = *w;
	while (ov == 0 || !Monitor::cmpAndSwap(w,ov,v)); // release it
	lock();
	if ((m = getObject(o)) != NULL) {
		m->resume(); // wake a suspended thread
	}
	else if ((ov & Monitor::Waiting) != 0) {
		// this is if enqueue() didn't make the lock() and suspend() before we got called
		m = allocate(o);
		addObject(o,m);
		m->resume();
	}
	unlock();
}

// wait() allocates a fat monitor if necessary. Then reads back the old
// value from the stack, releases the monitor, resumes a suspended thread,
// puts itself to sleep (which might return immediately if there is a pending
// resume). At wakeup, it tries to reacquire the lock, using the still valid stack pointer.
template<class M>
void Monitors<M>::wait(JavaObject& o, Int32 sp, Int32 tim) {
	M* m;
	Int32 ov, v;
	Int32* w;

	w = &o.state;
	v = *(Int32*)sp;
	do
		ov = *w;
	while (ov == 0 || !Monitor::cmpAndSwap(w,ov,v)); // release it
	lock();
	if ((m = getObject(o)) == NULL) {
		m = allocate(o);
		addObject(o,m);
	}
	unlock();
	m->resume(); // resume a suspended thread
	if (!m->wait(tim)) { // if failure, it was interrupted
		if (m->empty()) { // no more state. Deallocate monitor
			deallocate(m);
			removeObject(o);
		}
		sysThrowNamedException("java/lang/InterruptedException");
		return; // never reached
	}
	Monitor::wlock(o,sp);
}

template<class M>
void Monitors<M>::notify(JavaObject& o) {
	M* m;

	lock();
	if ((m = getObject(o)) == NULL) {
		m = allocate(o);
		addObject(o,m);
	}
	unlock();
	m->notify();
	return;
}

template<class M>
void Monitors<M>::notifyAll(JavaObject& o) {
	M* m;

	lock();
	if ((m = getObject(o)) == NULL) {
		m = allocate(o);
		addObject(o,m);
	}
	unlock();
	m->notifyAll();
	return;
}

template<class M>
void Monitors<M>::addObject(JavaObject& o, M* m) {
	Int32* w = &o.state;
	keyCache[cacheNext] = &o.state;
	valCache[cacheNext] = m;
	cacheNext = (cacheNext+1)%cacheSize;
	table->add(reinterpret_cast<void*>(w), m);
}

template<class M>
M* Monitors<M>::getObject(JavaObject& o) {
	M* m;
	int i;
	Int32* w = &o.state;

	for (i=0; i<cacheSize; i+=2) {
		if (keyCache[i] == w)
			return valCache[i];
		if (keyCache[i+1] == w)
			return valCache[i+1];
	}
	if (table->get(reinterpret_cast<void*>(w),&m))
		return m;
	else
		return NULL;
}

template<class M>
void Monitors<M>::removeObject(JavaObject& o) {
	int i;
	Int32* w = &o.state;

	for (i=0; i<cacheSize; i+=2) {
		if (keyCache[i] == w) {
			keyCache[i] = 0;
			break;
		}
		if (keyCache[i+1] == w) {
			keyCache[i+1] = 0;
			break;
		}
	}
	table->remove(reinterpret_cast<void*>(w));
}

template<class M>
Int32 Monitors<M>::hashGet(Int32 i) {
	lock();
	Int32 k = hashVec[i>>Monitor::KeyShift];
	unlock();
	return k;
}

template<class M>
Int32 Monitors<M>::hashAdd(Int32 k) {
	assert(k >= (1<<Monitor::KeySize));
	lock();
	Int32 i = hashVecTop++;
	if (i == hashVecSize) {
		Int32* oldVec = hashVec;
		hashVec = new(pool) Int32[hashVecSize+hashVecSize];
		for ( ; i-- > 0; )
			hashVec[i] = oldVec[i];
		i = hashVecSize;
		hashVecSize *= 2;
	}
	unlock();
	hashVec[i] = k;
	return i<<Monitor::KeyShift;
}

template<class M>
void Monitors<M>::hashRemove(Int32 i) {
	lock();
	if ((i>>Monitor::KeyShift) == hashVecTop-1)
		hashVecTop--;
	unlock();
}

template<class M>
Int32 Monitors<M>::nextHash() {
	lock();
	int hN = hashNext++;
	unlock();
	return hN;
}

// These are only in here until we have the stack entries for monitors
// in place.
template<class M>
Int32 Monitors<M>::pushStack() {
	Int32* a = (Int32*)PR_GetThreadPrivate(thr_stack);
	int i = (int)PR_GetThreadPrivate(thr_top);

	if (i==256)
		return 0;
	PR_SetThreadPrivate(thr_top,(void*)(i+1));
	return (Int32)&a[i];
}

template<class M>
Int32 Monitors<M>::popStack() {
	Int32* a = (Int32*)PR_GetThreadPrivate(thr_stack);
	int i = (int)PR_GetThreadPrivate(thr_top);
	PR_SetThreadPrivate(thr_top,(void*)(i-1));
	return a[i-1];
}

template<class M>
inline bool Monitors<M>::inStack(Int32 sp) { // for now
	Int32* a = (Int32*)PR_GetThreadPrivate(thr_stack);
	return (Int32)&a[0] <= sp && sp < (Int32)&a[256];
}
