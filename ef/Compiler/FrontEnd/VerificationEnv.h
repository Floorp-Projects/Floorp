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
#ifndef VERIFICATIONENV_H
#define VERIFICATIONENV_H

#include "LocalEnv.h"
#include "Value.h"
#include "ErrorHandling.h"

struct BytecodeBlock;

class VerificationEnv
{
  public:
	typedef BytecodeBlock *Subroutine;			// A subroutine is represented by a pointer to its entry BytecodeBlock

	enum BindingKind
	{
		bkVoid,			// Undefined stack word
		bkReturn,		// jsr/ret return value
		bkInt,			// int value
		bkFloat,		// float value
		bkAddr,			// Object pointer value
		bkLong,			// First word of a long value
		bkDouble,		// First word of a double value
		bkSecondWord	// Second word of a long or double value
	};

  private:
	static const BindingKind valueKindBindingKinds[nValueKinds];
	static const BindingKind typeKindBindingKinds[nTypeKinds];
  public:
	static bool isOneWordKind(BindingKind kind) {return (uint)(kind - bkReturn) <= (uint)(bkAddr - bkReturn);}
	static bool isTwoWordKind(BindingKind kind) {return (uint)(kind - bkLong) <= (uint)(bkDouble - bkLong);}
	static bool isTwoOrSecondWordKind(BindingKind kind) {return (uint)(kind - bkLong) <= (uint)(bkSecondWord - bkLong);}
	static BindingKind valueKindToBindingKind(ValueKind kind) {return valueKindBindingKinds[kind];}
	static BindingKind typeKindToBindingKind(TypeKind kind) {return typeKindBindingKinds[kind];}

	class Binding
	{
		BindingKind kind;						// Kind of value held in this stack slot
		union {
			Subroutine subroutine;				// If kind is bkReturn, the subroutine that was called
		};

	  public:
		BindingKind getKind() {return kind;}
		bool hasKind(BindingKind k) const {return kind == k;}
		bool isOneWord() const {return isOneWordKind(kind);}
		bool isTwoWord() const {return isTwoWordKind(kind);}
		bool isTwoOrSecondWord() const {return isTwoOrSecondWordKind(kind);}

		bool operator==(const Binding &b2) const {return kind == b2.kind && (kind != bkReturn || subroutine == b2.subroutine);}
		bool operator!=(const Binding &b2) const {return !operator==(b2);}

		void setVoid() {kind = bkVoid;}
		void setReturn(Subroutine s) {kind = bkReturn; subroutine = s;}
		void setKind(BindingKind k) {assert(k != bkReturn); kind = k;}

		Subroutine getSubroutine() const {assert(kind == bkReturn); return subroutine;}
	};

	struct InitBinding: Binding
	{
		explicit InitBinding(BindingKind kind) {setKind(kind);}
	};

	struct Common: CommonEnv
	{
		Pool *bindingPool;						// Pool used for allocating arrays of bindings and 'modified' arrays inside Contexts; nil if none

		Common(Pool &envPool, Pool *bindingPool, Uint32 nLocals, Uint32 stackSize):
			CommonEnv(envPool, 0, nLocals, stackSize), bindingPool(bindingPool) {}

		void setBindingPool(Pool &pool) {assert(!bindingPool); bindingPool = &pool;}
		void clearBindingPool() {bindingPool = 0;}	// Trashes bindings arrays and 'modified' arrays inside Contexts
	};

  private:
	struct Context
	{
		Context *next;							// Link to next outer subroutine context or nil if none
		const Subroutine subroutine;			// Pointer to the first instruction of a subroutine that was called and not yet returned
	  private:
		char *modified;							// Array of nLocals+stackSize bytes; each is set if the corresponding local variable
												//   or stack temporary has been modified since entry to the subroutine;
	  public:									//   undefined if common.bindingPool is nil.
		bool retReachable;						// True if subroutine's ret is reachable from this block (without going through subroutine's jsr)
      #ifdef DEBUG
    	const Common &common;					// Backpointer to information shared among all VerificationEnvs in a graph
      #endif

		Context(Subroutine subroutine, const Common &common);
		Context(const Context &src, const Common &common);
		Context(const Context &src, const Common &common, Function1<Subroutine, Subroutine> &translator);

		void modify(Uint32 slot) {assert(common.bindingPool); modified[slot] = 1;}
		bool isModified(Uint32 slot) {assert(common.bindingPool); return modified[slot] != 0;}
		bool meet(const Context &src, Uint32 nSlots);

		static Context *find(Context *list, const Subroutine subroutine);
	};

	Common &common;								// Backpointer to information shared among all VerificationEnvs in a graph
	Binding *bindings;							// Array of:
												//	   nLocals local variable bindings
												//     stackSize stack temporaries' bindings;
												//   bindings==nil means no information is available yet.
												//   If common.bindingPool is nil, bindings is either nil or non-nil but does not point to anything.
	Uint32 sp;									// Stack pointer (index within bindings array of first unused temporary)
												//   Bindings at indices 0..sp-1 are valid; others are ignored
	Context *activeContexts;					// Linked list of all currently active subroutines from inner to outer

	static const InitBinding secondWordBinding;	// Always contains a bkSecondWord binding

  public:
	explicit VerificationEnv(Common &common): common(common), bindings(0) {}
	VerificationEnv(const VerificationEnv &env): common(env.common) {copyEnv(env);}
	VerificationEnv(const VerificationEnv &env, Function1<Subroutine, Subroutine> &translator);
	void operator=(const VerificationEnv &env) {assert(!live()); copyEnv(env);}
	void move(VerificationEnv &env);

	void initLive();
	bool live() const {return bindings != 0;}

  private:
	void copyEnv(const VerificationEnv &env);
	void setSlot(Uint32 slot, const Binding &binding);

  public:
	// Local variable operations
	const Binding &getLocal1(Uint32 n) const;
	const Binding &getLocal2(Uint32 n) const;
	void setLocal1(Uint32 n, const Binding &binding);
	void setLocal2(Uint32 n, const Binding &binding);

	// Stack operations
	Uint32 getSP() const {return sp - common.stackBase;}
	void dropAll() {sp = common.stackBase;}
	const Binding &pop1();
	const Binding &pop1(BindingKind bk);
	void pop2(Binding &binding1, Binding &binding2);
	const Binding &pop2(BindingKind bk);
	const Binding &pop1or2(BindingKind bk);
	void push1(const Binding &binding);
	void push1(BindingKind bk);
	void push2(const Binding &binding);
	void push2(BindingKind bk);
	void push2(const Binding &binding1, const Binding &binding2);
	void push1or2(BindingKind bk);

	void enterSubroutine(Subroutine s);
	void exitSubroutine(Subroutine s, const VerificationEnv &entryEnv);

	bool meet(const VerificationEnv &env);

	bool isRetReachable(Subroutine s);
	void setRetReachable(Subroutine s);
	bool mergeRetReachables(const VerificationEnv &env, Subroutine s);
};


// --- INLINES ----------------------------------------------------------------


//
// Destructively move the given VerificationEnv (which must have been initialized)
// to this VerificationEnv, which must be uninitialized.  The given VerificationEnv is
// left uninitialized.
//
inline void VerificationEnv::move(VerificationEnv &env)
{
	assert(!live() && env.live());
	bindings = env.bindings;
	sp = env.sp;
	activeContexts = env.activeContexts;
  #ifdef DEBUG
	env.bindings = 0;
	env.activeContexts = 0;
  #endif
}


//
// Return true if Subroutine s is one of the active contexts in this VerificationEnv
// and that context's retReachable flag is true.
//
inline bool VerificationEnv::isRetReachable(Subroutine s)
{
	assert(live());
	Context *c = Context::find(activeContexts, s);
	return c && c->retReachable;
}

#endif
