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
#include "VerificationEnv.h"


const VerificationEnv::BindingKind VerificationEnv::valueKindBindingKinds[nValueKinds] =
{
	bkVoid,		// vkVoid
	bkInt,		// vkInt
	bkLong,		// vkLong
	bkFloat,	// vkFloat
	bkDouble,	// vkDouble
	bkAddr,		// vkAddr
	bkVoid,		// vkCond
	bkVoid,		// vkMemory
	bkVoid		// vkTuple
};


const VerificationEnv::BindingKind VerificationEnv::typeKindBindingKinds[nTypeKinds] =
{
	bkVoid,		// tkVoid
	bkInt,		// tkBoolean
	bkInt,		// tkUByte
	bkInt,		// tkByte
	bkInt,		// tkChar
	bkInt,		// tkShort
	bkInt,		// tkInt
	bkLong,		// tkLong
	bkFloat,	// tkFloat
	bkDouble,	// tkDouble
	bkAddr,		// tkObject
	bkAddr,		// tkSpecial
	bkAddr,		// tkArray
	bkAddr		// tkInterface
};


// ----------------------------------------------------------------------------
// VerificationEnv::Context


//
// Create a new, empty Context that holds information about which variables a subroutine
// has modified.  Clear the modified array (indicating that no variables have been accessed
// yet) and the link to the next context.
//
VerificationEnv::Context::Context(Subroutine subroutine, const Common &common):
	next(0),
	subroutine(subroutine),
	retReachable(false)
  #ifdef DEBUG
	, common(common)
  #endif
{
	assert(common.bindingPool);
	modified = new(*common.bindingPool) char[common.nEnvSlots];
	fill(modified, modified + common.nEnvSlots, 0);
}


//
// Create a copy of the given context.  Clear the link to the next context.
//
VerificationEnv::Context::Context(const Context &src, const Common &common):
	next(0),
	subroutine(src.subroutine),
	retReachable(src.retReachable)
  #ifdef DEBUG
	, common(common)
  #endif
{
	assert(common.bindingPool);
	modified = new(*common.bindingPool) char[common.nEnvSlots];
	copy(src.modified, src.modified + common.nEnvSlots, modified);
}


//
// Create a partial copy of the given context.  Do not copy the contents of the
// modified array.  Translate the Subroutine value using the given translator.
// Clear the link to the next context.
//
inline VerificationEnv::Context::Context(const Context &src, const Common &common, Function1<Subroutine, Subroutine> &translator):
	next(0),
	subroutine(translator(src.subroutine)),
	retReachable(src.retReachable)
  #ifdef DEBUG
	, common(common)
  #endif
{}


//
// Merge this context with the src context.  A slot in the merged context is
// set to the modified state if it was modified in either this or the src context.
// Return true if the resulting context differs from the original contents of this
// context.
//
bool VerificationEnv::Context::meet(const Context &src, Uint32 nSlots)
{
	assert(subroutine == src.subroutine && common.bindingPool);
	bool changed = false;
	char *dstModified = modified;
	const char *srcModified = src.modified;
	for (Uint32 slot = 0; slot != nSlots; slot++)
		if (srcModified[slot] && !dstModified[slot]) {
			dstModified[slot] = true;
			changed = true;
		}
	return changed;
}


//
// If a context corresponding to the given subroutine is in the linked list of
// contexts, return that context.  If not, return nil.  list can be nil.
//
VerificationEnv::Context *VerificationEnv::Context::find(Context *list, const Subroutine subroutine)
{
	while (list && list->subroutine != subroutine)
		list = list->next;
	return list;
}


// ----------------------------------------------------------------------------
// VerificationEnv


const VerificationEnv::InitBinding VerificationEnv::secondWordBinding(bkSecondWord);


//
// Assign a partial copy of the given VerificationEnv (which must have been initialized)
// to this VerificationEnv, which is known to be uninitialized.  Do not copy the
// bindings nor the modified arrays inside Contexts.  Translate all Subroutine values
// using the given translator.
//
VerificationEnv::VerificationEnv(const VerificationEnv &env, Function1<Subroutine, Subroutine> &translator):
	common(env.common)
{
	assert(env.live());

	// Copy the bindings pointer (which is now only relevant to indicate whether this
	// environment is live or not).
	bindings = env.bindings;
	sp = env.sp;

	// Copy the context hierarchy.
	activeContexts = 0;
	Context *srcContext = env.activeContexts;
	Context **dstContext = &activeContexts;
	while (srcContext) {
		*dstContext = new(common.envPool) Context(*srcContext, common, translator);
		dstContext = &(*dstContext)->next;
		srcContext = srcContext->next;
	}
}


//
// Make this VerificationEnv (which must not have been initialized) live,
// assigning bkVoid to every slot.
//
void VerificationEnv::initLive()
{
	assert(!live() && common.bindingPool);

	// Create and initialize the bindings.
	Binding *bdgs = new(*common.bindingPool) Binding[common.nEnvSlots];
	bindings = bdgs;
	Binding *bdgsEnd = bdgs + common.nEnvSlots;
	while (bdgs != bdgsEnd)
		bdgs++->setVoid();
	sp = common.stackBase;
	activeContexts = 0;
}


//
// Assign a copy of the given VerificationEnv (which must have been initialized)
// to this VerificationEnv, which is known to be uninitialized.
//
void VerificationEnv::copyEnv(const VerificationEnv &env)
{
	assert(env.live() && common.bindingPool);

	// Copy the bindings.
	bindings = new(*common.bindingPool) Binding[common.nEnvSlots];
	copy(env.bindings, env.bindings + common.nEnvSlots, bindings);
	sp = env.sp;

	// Copy the context hierarchy.
	activeContexts = 0;
	Context *srcContext = env.activeContexts;
	Context **dstContext = &activeContexts;
	while (srcContext) {
		*dstContext = new(common.envPool) Context(*srcContext, common);
		dstContext = &(*dstContext)->next;
		srcContext = srcContext->next;
	}
}


//
// Set the value of the given environment slot, adding that slot to
// the list of slots modified by all currently active subroutines.
//
void VerificationEnv::setSlot(Uint32 slot, const Binding &binding)
{
	assert(slot < common.nEnvSlots);
	bindings[slot] = binding;
	for (Context *c = activeContexts; c; c = c->next)
		c->modify(slot);
}


//
// Return the value of the one-word local variable in the given slot.
// Throw a verification error if the slot number is out of bounds or the value
// is undefined or a part of a two-word value.
//
const VerificationEnv::Binding &VerificationEnv::getLocal1(Uint32 n) const
{
	assert(live() && common.bindingPool);
	if (n >= common.nLocals)
		verifyError(VerifyError::noSuchLocal);
	Binding &b = bindings[n];
	if (!b.isOneWord())
		verifyError(VerifyError::badType);
	return b;
}


//
// Return the value of the two-word local variable in the given slot.
// Throw a verification error if the slot number is out of bounds or the value
// is undefined or not a two-word value.
//
const VerificationEnv::Binding &VerificationEnv::getLocal2(Uint32 n) const
{
	assert(live() && common.bindingPool);
	if (n+1 >= common.nLocals)
		verifyError(VerifyError::noSuchLocal);
	Binding &b = bindings[n];
	if (!b.isTwoWord())
		verifyError(VerifyError::badType);
	assert(bindings[n+1].hasKind(bkSecondWord));
	return b;
}


//
// Set the value of the one-word local variable in the given slot.
// Throw a verification error if the slot number is out of bounds.
// The given binding must have a one-word kind.
//
void VerificationEnv::setLocal1(Uint32 n, const Binding &binding)
{
	assert(live() && binding.isOneWord() && common.bindingPool);
	if (n >= common.nLocals)
		verifyError(VerifyError::noSuchLocal);
	Binding *b = &bindings[n];

	// If we're writing into an existing half of a doubleword,
	// invalidate the other half of that doubleword.
	BindingKind bk = b->getKind();
	if (isTwoOrSecondWordKind(bk))
		b[bk == bkSecondWord ? -1 : 1].setVoid();

	setSlot(n, binding);
}


//
// Set the value of the two-word local variable in the given slot.
// Throw a verification error if the slot number is out of bounds.
// The given binding must have a two-word kind.
//
void VerificationEnv::setLocal2(Uint32 n, const Binding &binding)
{
	assert(live() && binding.isTwoWord() && common.bindingPool);
	if (n+1 >= common.nLocals)
		verifyError(VerifyError::noSuchLocal);
	Binding *b = &bindings[n];

	// If we're writing into an existing half of a doubleword,
	// invalidate the other half of that doubleword.
	if (b[0].hasKind(bkSecondWord))
		b[-1].setVoid();
	if (b[1].isTwoWord())
		b[2].setVoid();

	setSlot(n, binding);
	setSlot(n+1, secondWordBinding);
}


//
// Pop and return a one-word value from the stack.
// Throw a verification error if the stack underflows or the value
// is undefined or a part of a two-word value.
// Note that the returned reference will be invalidated by the next push.
//
const VerificationEnv::Binding &VerificationEnv::pop1()
{
	assert(live() && common.bindingPool);
	if (sp == common.stackBase)
		verifyError(VerifyError::bytecodeStackUnderflow);
	Binding &b = bindings[--sp];
	if (!b.isOneWord())
		verifyError(VerifyError::badType);
	return b;
}


//
// Pop and return a one-word value from the stack.
// Throw a verification error if the stack underflows or the value
// is undefined or has a kind other than the given kind.
// Note that the returned reference will be invalidated by the next push.
//
const VerificationEnv::Binding &VerificationEnv::pop1(BindingKind bk)
{
	assert(live() && isOneWordKind(bk) && common.bindingPool);
	if (sp == common.stackBase)
		verifyError(VerifyError::bytecodeStackUnderflow);
	Binding &b = bindings[--sp];
	if (!b.hasKind(bk))
		verifyError(VerifyError::badType);
	return b;
}


//
// Pop and return a two-word value or two one-word values from the stack.
// Throw a verification error if the stack underflows or the value
// is undefined or a part of a two-word value that includes one of the two
// stack slots but not the other.
//
void VerificationEnv::pop2(Binding &binding1, Binding &binding2)
{
	assert(live() && common.bindingPool);
	if (sp <= common.stackBase+1)
		verifyError(VerifyError::bytecodeStackUnderflow);
	sp -= 2;
	Binding *b = &bindings[sp];
	BindingKind bk1 = b[0].getKind();
	BindingKind bk2 = b[1].getKind();
	if (!(isOneWordKind(bk1) && isOneWordKind(bk2) || isTwoWordKind(bk1)))
		verifyError(VerifyError::badType);
	assert(!isTwoWordKind(bk1) || bk2 == bkSecondWord);
	binding1 = b[0];
	binding2 = b[1];
}


//
// Pop and return a two-word value from the stack.
// Throw a verification error if the stack underflows or the value
// is undefined or has a kind other than the given kind.
// Note that the returned reference will be invalidated by the next push.
//
const VerificationEnv::Binding &VerificationEnv::pop2(BindingKind bk)
{
	assert(live() && isTwoWordKind(bk) && common.bindingPool);
	if (sp <= common.stackBase+1)
		verifyError(VerifyError::bytecodeStackUnderflow);
	sp -= 2;
	Binding *b = &bindings[sp];
	if (!b[0].hasKind(bk))
		verifyError(VerifyError::badType);
	assert(b[1].hasKind(bkSecondWord));
	return *b;
}


//
// Pop and return a one or two-word value from the stack, depending on bk.
// Throw a verification error if the stack underflows or the value
// is undefined or has a kind other than the given kind.
// Note that the returned reference will be invalidated by the next push.
//
const VerificationEnv::Binding &VerificationEnv::pop1or2(BindingKind bk)
{
	return isTwoWordKind(bk) ? pop2(bk) : pop1(bk);
}


//
// Push a one-word value onto the stack.
// Throw a verification error if the stack overflows.
// The given binding must have a one-word kind.
//
void VerificationEnv::push1(const Binding &binding)
{
	assert(live() && binding.isOneWord() && common.bindingPool);
	if (sp == common.nEnvSlots)
		verifyError(VerifyError::bytecodeStackOverflow);
	setSlot(sp++, binding);
}


//
// Push a one-word value that contains a new binding of the given kind onto the stack.
// Throw a verification error if the stack overflows.
//
void VerificationEnv::push1(BindingKind bk)
{
	// Note: bkAddr will be outlawed here once we start to distinguish among types of
	// pointers.
	assert(bk == bkVoid || bk == bkInt || bk == bkFloat || bk == bkAddr);
	InitBinding b(bk);
	push1(b);
}


//
// Push a two-word value onto the stack.
// Throw a verification error if the stack overflows.
// The given binding must have a two-word kind.
//
void VerificationEnv::push2(const Binding &binding)
{
	assert(live() && binding.isTwoWord() && common.bindingPool);
	if (sp+1 >= common.nEnvSlots)
		verifyError(VerifyError::bytecodeStackOverflow);
	setSlot(sp++, binding);
	setSlot(sp++, secondWordBinding);
}


//
// Push a two-word value that contains a new binding of the given kind onto the stack.
// Throw a verification error if the stack overflows.
//
void VerificationEnv::push2(BindingKind bk)
{
	assert(bk == bkLong || bk == bkDouble);
	InitBinding b(bk);
	push2(b);
}


//
// Push a two-word value or two one-word values onto the stack.
// Throw a verification error if the stack overflows.
// The given bindings must be the two words of a two-word value
// or both be one-word values.
//
void VerificationEnv::push2(const Binding &binding1, const Binding &binding2)
{
	assert(live() && (binding1.isTwoWord() && binding2.hasKind(bkSecondWord) ||
					  binding1.isOneWord() && binding2.isOneWord())
				  && common.bindingPool);
	if (sp+1 >= common.nEnvSlots)
		verifyError(VerifyError::bytecodeStackOverflow);
	setSlot(sp++, binding1);
	setSlot(sp++, binding2);
}


//
// Push a one or two-word value that contains a new binding of the
// given kind onto the stack.
// Throw a verification error if the stack overflows.
//
void VerificationEnv::push1or2(BindingKind bk)
{
	// Note: bkAddr will be outlawed here once we start to distinguish among types of
	// pointers.
	assert(bk == bkVoid || bk == bkInt || bk == bkFloat || bk == bkAddr || bk == bkLong || bk == bkDouble);
	InitBinding b(bk);
	if (isTwoWordKind(bk))
		push2(b);
	else
		push1(b);
}


//
// Push the context of the given subroutine onto the list of currently active
// contexts.  Initialize that context to record all bindings written to this
// environment after this enterSubroutine call.
//
// Throw a verification error if the given subroutine already is on the list
// of active contexts.  This rejects bytecode programs that call subroutines
// recursively.  It also rejects some "valid" bytecode programs that do not call
// subroutines recursively (for instance, if subroutine A exits via a jump
// instead of a ret and then calls subroutine A again), but, fortunately, the
// current definition of the Java language does not generate such programs.
// (Specifically, each Java try block has a single entry point and the finally
// handler -- say, subroutine A -- for that try block is outside that block.  In
// order for subroutine A to be called again, execution must proceed again
// through the entry point of the try block, and we know that at that point
// subroutine A is not one of the active contexts.)
//
void VerificationEnv::enterSubroutine(Subroutine s)
{
	assert(live() && s && common.bindingPool);
	if (Context::find(activeContexts, s))
		verifyError(VerifyError::jsrNestingError);

	Context *c = new(common.envPool) Context(s, common);
	c->next = activeContexts;
	activeContexts = c;
}


//
// Pop the context of the given subroutine from the list of currently active
// contexts, and replace environment bindings that were not modified by the
// subroutine by their contenst from entryEnv, which represents the environment
// as it was just before entry to the subroutine.  Also pop any contexts more
// recent than that of the given subroutine -- these subroutines apparently
// exited using a jump instead of a ret.
//
// Throw a verification error if the subroutine is not on the list of active
// contexts, which indicates that there is some program path along which this
// ret could be reached without this subroutine having been called first.
//
void VerificationEnv::exitSubroutine(Subroutine s, const VerificationEnv &entryEnv)
{
	assert(live() && s && entryEnv.live() && common.bindingPool);
	Context *c = Context::find(activeContexts, s);
	if (!c)
		verifyError(VerifyError::jsrNestingError);
	activeContexts = c->next;

	// Replace unmodified environment bindings.
	Uint32 nSlots = sp;
	const Binding *srcBindings = entryEnv.bindings;
	Binding *dstBindings = bindings;
	for (Uint32 slot = 0; slot != nSlots; slot++)
		if (!c->isModified(slot))
			dstBindings[slot] = srcBindings[slot];
}


//
// Intersect the given VerificationEnv (which must be live) into this
// VerificationEnv, which must also be live.  The two environments may have
// different sets of active subroutine contexts, in which case leave the maximal
// common set (for instance, intersecting a->b with b yields b, while
// intersecting a->b->d->e with a->c->d yields a->d).  Because each subroutine has
// only one entry point and cannot be recursive, we don't have to worry about
// cases such as intersecting a->b with b->a -- nesting of subroutines follows a
// partial order.
//
// Return true if this VerificationEnv changed.
//
// Throw a verification error if the environments have different stack depths.
//
bool VerificationEnv::meet(const VerificationEnv &env)
{
	assert(live() && env.live() && common.bindingPool);
	Uint32 nSlots = sp;
	if (nSlots != env.sp)
		verifyError(VerifyError::bytecodeStackDynamic);
	bool changed = false;

	// Merge context lists
	Context **dstContextPtr = &activeContexts;
	Context *srcContext = env.activeContexts;
	Context *dstContext;
	while ((dstContext = *dstContextPtr) != 0) {
		Context *c = Context::find(srcContext, dstContext->subroutine);
		if (c) {
			srcContext = c;
			changed |= dstContext->meet(*c, nSlots);
			dstContextPtr = &dstContext->next;
		} else {
			// Remove this destination context.
			*dstContextPtr = dstContext->next;
			changed = true;
		}
	}

	// Merge bindings
	const Binding *srcBinding = env.bindings;
	Binding *dstBinding = bindings;
	Binding *dstBindingsEnd = dstBinding + nSlots;
	while (dstBinding != dstBindingsEnd) {
		if (*dstBinding != *srcBinding && !dstBinding->hasKind(bkVoid)) {
			dstBinding->setVoid();
			changed = true;
		}
		srcBinding++;
		dstBinding++;
	}
	return changed;
}


//
// This VerificationEnv (which must be live) is the entry environment to a ret
// bytecode that returns from subroutine s.  That subroutine's context is one
// of the active contexts in this VerificationEnv.  Set that context's retReachable
// flag because clearly subroutine s's ret is reachable from here.
//
void VerificationEnv::setRetReachable(Subroutine s)
{
	assert(live());
	Context *c = Context::find(activeContexts, s);
	assert(c);
	c->retReachable = true;
}


//
// Merge the retReachable flags of env into this VerificationEnv.  env is the
// environment of a successor of this VerificationEnv's block.  If s is non-nil,
// this VerificationEnv's block is a jsr instruction that calls subroutine s,
// and env belongs to s's first block; in this case don't set this environment's
// retReachable flag for s's context because this environment is outside s.
//
// Return true if this VerificationEnv changed.
//
bool VerificationEnv::mergeRetReachables(const VerificationEnv &env, Subroutine s)
{
	assert(live() && env.live());
	bool changed = false;

	Context *srcContext = env.activeContexts;
	Context *dstContext = activeContexts;
	while (srcContext) {
		if (srcContext->retReachable) {
			Subroutine srcSubroutine = srcContext->subroutine;
			if (srcSubroutine != s) {
				dstContext = Context::find(dstContext, srcSubroutine);
				assert(dstContext);
				if (!dstContext->retReachable) {
					dstContext->retReachable = true;
					changed = true;
				}
			}
		}
		srcContext = srcContext->next;
	}
	return changed;
}
