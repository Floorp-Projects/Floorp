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
#ifndef LOCALENV_H
#define LOCALENV_H

#include "Fundamentals.h"


// Information common to all environments in a graph
struct CommonEnv
{
	Pool &envPool;								// Memory pool from which to allocate
	const Uint32 nLocals;						// Number of words of local variables
	const Uint32 stackBase;						// Index of first stack temporary
	const Uint32 stackSize;						// Number of words of local stack space
	const Uint32 nEnvSlots;						// nMemoryBindings + nLocals + stackSize

	CommonEnv(Pool &envPool, Uint32 nMemoryBindings, Uint32 nLocals, Uint32 stackSize):
		envPool(envPool), nLocals(nLocals), stackBase(nMemoryBindings + nLocals), stackSize(stackSize),
		nEnvSlots(nMemoryBindings + nLocals + stackSize) {}
};


// A single environment containings bindings of class Binding.
// Class Common, which should be derived from CommonEnv, contains information
// common to all environments in a graph.
template<class Binding, class Common, Uint32 nMemoryBindings>
class LocalEnv
{
  protected:
	const Common &commonEnv;					// Backpointer to information shared among all LocalEnvs in a graph
  private:
	Binding *bindings;							// Array of:
												//     nMemoryBindings memory bindings
												//	   nLocals local variable bindings
												//     stackSize stack temporaries' bindings
												//   (the bindings may be nil).
	Uint32 sp;									// Stack pointer (index within bindings array of first unused temporary)
												//   Bindings at indices 0..sp-1 are valid; others are ignored

  public:
	explicit LocalEnv(const Common &commonEnv);
	LocalEnv(const LocalEnv &env): commonEnv(env.commonEnv) {copyEnv(env);}
	void operator=(const LocalEnv &env) {assert(!bindings); copyEnv(env);}
	void move(LocalEnv &env);

	void init();

  protected:
	void copyEnv(const LocalEnv &env);
	Binding *bindingsBegin() const {return bindings;}	 // Return the first valid binding
	Binding *bindingsEnd() const {return bindings + sp;} // Return the last valid binding + 1
	Binding *bindingsMemoryEnd() const {return bindings + nMemoryBindings;} // Return the last memory binding + 1
  public:

	Pool &envPool() const {return commonEnv.envPool;}
	Binding &memory() const {return bindings[0];}
	Binding &local(Uint32 n) const {assert(n < commonEnv.nLocals); return bindings[nMemoryBindings + n];}
	Binding &stackNth(Uint32 n) const {assert(n > 0 && sp >= commonEnv.stackBase + n); return bindings[sp - n];}
	Binding *stackTopN(Uint32 DEBUG_ONLY(n)) const {assert(sp >= commonEnv.stackBase + n); return bindings + sp;}

	// Stack operations
	Uint32 getSP() const {return sp - commonEnv.stackBase;}
	void dropAll() {sp = commonEnv.stackBase;}
	void drop(uint n) {assert(sp >= commonEnv.stackBase + n); sp -= n;}
	void raise(uint n) {assert(sp + n <= commonEnv.nEnvSlots); sp += n;}
	Binding &pop() {assert(sp > commonEnv.stackBase); return bindings[--sp];} // Note: Result invalidated by next push!
	Binding &push() {assert(sp < commonEnv.nEnvSlots); return bindings[sp++];}
	Binding &pop2();
	Binding &push2();
	Binding &pop1or2(bool two);
	Binding &push1or2(bool two);

  protected:
	bool compatible(const LocalEnv &env) {return bindings && env.bindings && &commonEnv == &env.commonEnv && sp == env.sp;}
};


// --- INLINES ----------------------------------------------------------------

//
// Construct a new LocalEnv for the method described by commonEnv.
// The LocalEnv cannot be used unless init is called first or another LocalEnv
// is copied to this one.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline LocalEnv<Binding, Common, nMemoryBindings>::LocalEnv(const Common &commonEnv):
	commonEnv(commonEnv),
	bindings(0)
{}


//
// Destructively move the given LocalEnv (which must have been initialized)
// to this LocalEnv, which must be uninitialized.  The given LocalEnv is
// left uninitialized.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline void LocalEnv<Binding, Common, nMemoryBindings>::move(LocalEnv &env)
{
	assert(!bindings && env.bindings);
	bindings = env.bindings;
	sp = env.sp;
  #ifdef DEBUG
	env.bindings = 0;
  #endif
}


//
// Pop and return a long or double binding from the stack.  The two words on the top of
// the stack must contain a long or double value.
// Note that the returned reference will be invalidated by the next push.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline Binding &LocalEnv<Binding, Common, nMemoryBindings>::pop2()
{
	assert(sp > commonEnv.stackBase+1 && bindings[sp-1].isSecondWord());
	return bindings[sp -= 2];
}


//
// Push a long or double binding outo the stack.  The return value is a reference to
// a new stack slot; the caller must initialize it to refer to the long or double's first word.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline Binding &LocalEnv<Binding, Common, nMemoryBindings>::push2()
{
	assert(sp < commonEnv.nEnvSlots-1);
	Binding *b = &bindings[sp];
	b[1].defineSecondWord();
	sp += 2;
	return *b;
}


//
// Do pop() if two is false or pop2() if two is true.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline Binding &LocalEnv<Binding, Common, nMemoryBindings>::pop1or2(bool two)
{
	assert(sp > commonEnv.stackBase+two && (!two || bindings[sp-1].isSecondWord()));
	if (two)
		--sp;
	return bindings[--sp];
}


//
// Do push() if two is false or push2() if two is true.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
inline Binding &LocalEnv<Binding, Common, nMemoryBindings>::push1or2(bool two)
{
	assert(sp < commonEnv.nEnvSlots-two);
	Binding &b = bindings[sp++];
	if (two)
		bindings[sp++].defineSecondWord();
	return b;
}


// --- TEMPLATES --------------------------------------------------------------


//
// Initialize every entry in this LocalEnv to be empty and the stack to have no
// temporaries.  This LocalEnv must not have been initialized before.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
void LocalEnv<Binding, Common, nMemoryBindings>::init()
{
	assert(!bindings);
	Binding *lb = new(commonEnv.envPool) Binding[commonEnv.nEnvSlots];
	bindings = lb;
	Uint32 stackBase = commonEnv.stackBase;
	sp = stackBase;
	Binding *lbEnd = lb + stackBase;
	while (lb != lbEnd)
		lb++->clear();
}


//
// Assign a copy of the given LocalEnv (which must have been initialized)
// to this LocalEnv, which is known to be uninitialized.
//
template<class Binding, class Common, Uint32 nMemoryBindings>
void LocalEnv<Binding, Common, nMemoryBindings>::copyEnv(const LocalEnv &env)
{
	assert(env.bindings);
	Binding *lbDst = new(commonEnv.envPool) Binding[commonEnv.nEnvSlots];
	copy(env.bindingsBegin(), env.bindingsEnd(), lbDst);
	bindings = lbDst;
	sp = env.sp;
}

#endif
