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
#ifndef _EF_MONITORS
#define _EF_MONITORS

#include "JavaObject.h"
#include "HashTable.h"
#include "prtypes.h"
#include "pratom.h"
#include "prlock.h"
#include "prcvar.h"
#include "prthread.h"

struct PointerKeyOps {
  static bool equals(void* userKey, void* hashTableKey) {
    return (userKey == hashTableKey);
  }

  static void* copyKey(Pool &/*pool*/, void* userKey) {
    return userKey;
  }

  static Uint32 hashCode(void* userKey) {
    return reinterpret_cast<Uint32>(userKey);
  }
};

template<class N> 
class PointerHashTable : public HashTable<N, void*, PointerKeyOps> {
public:
  PointerHashTable(Pool &p) : HashTable<N, void*, PointerKeyOps>(p) { } 
};

struct MonitorArgStruct { // for debugging
	void (*f)(void*);
	void* arg;
};

class NS_EXTERN Monitor {
public:
	enum {
		KeySize=30,
		KeyShift=32-KeySize,
		KeyMask=0xFFFFFFFC, // '1...1100'
		LockMask=0x3,
		XHash=0x2,
		Waiting=0x2,  // w-flag when object is locked
		IHash=0x1,
		Unlocked=0x1
	};

	class Exception {};

	static inline bool cmpAndSwap(Int32* w, Int32 ov, Int32 nv);

	static inline Int32 exchange(Int32* w, Int32 v) {
		return (Int32)PR_AtomicSet((PRInt32*)w,(PRInt32)v);
	}

	static inline bool inStack(Int32 v);

	static inline Int32 pushStack();

	static inline Int32 popStack();

	static void threadInit();

	static void lock(JavaObject& o);

	static void unlock(JavaObject& o);

	static void wlock(JavaObject& o, Int32 sp);

	static void wait(JavaObject& o, Int64 tim);

	static inline void wait(JavaObject& o) {
		wait(o, PR_INTERVAL_NO_TIMEOUT);
	}

	static void notify(JavaObject& o);

	static void notifyAll(JavaObject& o);

	static void staticInit();

	static void destroy();

 	static inline void run(void* arg) { // for debugging
		MonitorArgStruct* mas = (MonitorArgStruct*)arg;
#if 0
		DWORD handler = (DWORD)NULL;//*win32HardwareThrow;
		__asm
		{
			push handler
			push fs:[0]
			mov fs:[0],esp
		}
		threadInit();
		mas->f(mas->arg);
		__asm {
			mov eax,[esp] // old fs:[0]
			mov fs:[0],eax
			add esp,8
		}
#else
		threadInit();
		mas->f(mas->arg);
#endif
	}

	static void hashWrite(JavaObject& obj, Int32 k);

	static Int32 hashRead(JavaObject& obj);

	static Int32 hashCode(JavaObject& obj);
};

struct MonitorObject {
	JavaObject* obj;
 	Int32 susp;
 	Int32 _wait;
 	PRLock* slock;
 	PRCondVar* svar;
 	PRLock* wlock;
 	PRCondVar* wvar;
public:
	class Exception {};
	MonitorObject* next;
	MonitorObject* prev;

 	MonitorObject() : obj(NULL), susp(0), _wait(0), next(NULL), prev(NULL) {
 		slock = PR_NewLock(); svar = PR_NewCondVar(slock);
 		wlock = PR_NewLock(); wvar = PR_NewCondVar(wlock);
	}

	~MonitorObject() { 
 		PR_DestroyLock(slock); PR_DestroyCondVar(svar);
 		PR_DestroyLock(wlock); PR_DestroyCondVar(wvar);
	}

 	inline void init(JavaObject& o) { 
		next=NULL; obj = &o; 
		susp = 0; _wait = 0;
	}

	inline bool empty() { 
		return _wait == 0 && susp == 0; 
	}

	inline bool isSuspended() { 
		return susp > 0; 
	}

	inline bool suspend() 
	{ 
		PRStatus stat;
		PR_Lock(slock);
		susp++; 
		if (susp<1) {
			PR_Unlock(slock);
			return true;
		}
		stat = PR_WaitCondVar(svar,PR_INTERVAL_NO_TIMEOUT); 
		if (stat == PR_FAILURE) {
			susp--;
			return false;
		}
		PR_Unlock(slock);
		return true;
	}

	inline void resume() 
	{ 
		PRStatus stat;
		PR_Lock(slock);
		susp--; 
		stat = PR_NotifyCondVar(svar);
		assert(stat != PR_FAILURE);
		PR_Unlock(slock);
	}

	inline bool wait(Int32 tim)
	{ 
		PRStatus stat;
		PR_Lock(wlock);
		_wait++; 
		if (_wait<1) {
			PR_Unlock(wlock);
			return true;
		}
		stat = PR_WaitCondVar(wvar,(tim == 0 ? PR_INTERVAL_NO_TIMEOUT : PR_MillisecondsToInterval(tim)));
		if (stat == PR_FAILURE) {
			_wait--;
			return false;
		}
		PR_Unlock(wlock);
		return true;
	}

	inline void notify() 
	{
		PRStatus stat;
		PR_Lock(wlock);
		_wait--; 
		stat = PR_NotifyCondVar(wvar); 
		assert(stat != PR_FAILURE);
		PR_Unlock(wlock);
	}

	inline void notifyAll() 
	{ 
		PR_Lock(wlock);
		_wait--;  // Is this right?
		PR_NotifyAllCondVar(wvar); 
		PR_Unlock(wlock);
	}
};

template<class M>
class Monitors {
private:
	PRUintn thr_stack; // temporary
	PRUintn thr_top;   // ditto
	PRLock* _lock;
	M* free;   // for allocation
	M* taken;  // for deallocation and cleanup
	M* newList(int l);
	Pool pool;
	PointerHashTable<M*>* table;
	enum { cacheSize=4 }; // must be even!!!
	Int32* keyCache[cacheSize];
	M* valCache[cacheSize];
	Int32* hashVec;
	int cacheNext;
	int stateNext;
	int hashNext;
	int hashVecTop;
	int hashVecSize;

	inline Int32* threadVec() {
		Int32* a;
		a = new(pool) Int32[256];
		for (int i=0; i<256; i++)
			a[i] = -1;	
		return a;
	}

	inline void threadInit() {
		PRStatus stat;
		
		stat = PR_SetThreadPrivate(thr_stack, (void *)threadVec());
		assert(stat != PR_FAILURE);
		stat = PR_SetThreadPrivate(thr_top, (void *)0);
		assert(stat != PR_FAILURE);
	}

	friend void Monitor::threadInit();

public:

	class Exception {};

	Monitors(int l);

	~Monitors();

	M* allocate(JavaObject& o);

	void deallocate(M* m);

	inline void lock() {
		PR_Lock(_lock);
	}

	inline void unlock() {
		PR_Unlock(_lock);
	}

	int enqueue(JavaObject& o);

	void dequeue(JavaObject& o, Int32 v);

	void wait(JavaObject& o, Int32 sp, Int32 tim);

	void notify(JavaObject& o);

	void notifyAll(JavaObject& o);

	M* getObject(JavaObject& o);

	void addObject(JavaObject& o, M* m);

	void removeObject(JavaObject& o);

	inline bool inStack(Int32 v);

	inline Int32 pushStack();

	inline Int32 popStack();
	
	Int32 hashGet(Int32 k);

	Int32 hashAdd(Int32 k);

	void hashRemove(Int32 k);

	Int32 nextHash();
};

extern Monitors<MonitorObject>* monitors;
#endif
