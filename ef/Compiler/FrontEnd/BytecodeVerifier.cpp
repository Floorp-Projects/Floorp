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

#include "BytecodeVerifier.h"
#include "FieldOrMethod.h"
#include "MemoryAccess.h"
#include "GraphUtils.h"


static const VerificationEnv::BindingKind localAccessKinds[] =
{
	VerificationEnv::bkInt,		// bcILoad,   bcIStore
	VerificationEnv::bkLong,	// bcLLoad,   bcLStore
	VerificationEnv::bkFloat,	// bcFLoad,   bcFStore
	VerificationEnv::bkDouble,	// bcDLoad,   bcDStore
	VerificationEnv::bkAddr,	// bcALoad,   bcAStore
	VerificationEnv::bkInt,		// bcILoad_0, bcIStore_0
	VerificationEnv::bkInt,		// bcILoad_1, bcIStore_1
	VerificationEnv::bkInt,		// bcILoad_2, bcIStore_2
	VerificationEnv::bkInt,		// bcILoad_3, bcIStore_3
	VerificationEnv::bkLong,	// bcLLoad_0, bcLStore_0
	VerificationEnv::bkLong,	// bcLLoad_1, bcLStore_1
	VerificationEnv::bkLong,	// bcLLoad_2, bcLStore_2
	VerificationEnv::bkLong,	// bcLLoad_3, bcLStore_3
	VerificationEnv::bkFloat,	// bcFLoad_0, bcFStore_0
	VerificationEnv::bkFloat,	// bcFLoad_1, bcFStore_1
	VerificationEnv::bkFloat,	// bcFLoad_2, bcFStore_2
	VerificationEnv::bkFloat,	// bcFLoad_3, bcFStore_3
	VerificationEnv::bkDouble,	// bcDLoad_0, bcDStore_0
	VerificationEnv::bkDouble,	// bcDLoad_1, bcDStore_1
	VerificationEnv::bkDouble,	// bcDLoad_2, bcDStore_2
	VerificationEnv::bkDouble,	// bcDLoad_3, bcDStore_3
	VerificationEnv::bkAddr,	// bcALoad_0, bcAStore_0
	VerificationEnv::bkAddr,	// bcALoad_1, bcAStore_1
	VerificationEnv::bkAddr,	// bcALoad_2, bcAStore_2
	VerificationEnv::bkAddr		// bcALoad_3, bcAStore_3
};


static const VerificationEnv::BindingKind arrayAccessKinds[] =
{
	VerificationEnv::bkInt,		// bcIALoad, bcIAStore, bcIReturn
	VerificationEnv::bkLong,	// bcLALoad, bcLAStore, bcLReturn
	VerificationEnv::bkFloat,	// bcFALoad, bcFAStore, bcFReturn
	VerificationEnv::bkDouble,	// bcDALoad, bcDAStore, bcDReturn
	VerificationEnv::bkAddr,	// bcAALoad, bcAAStore, bcAReturn
	VerificationEnv::bkInt,		// bcBALoad, bcBAStore
	VerificationEnv::bkInt,		// bcCALoad, bcCAStore
	VerificationEnv::bkInt		// bcSALoad, bcSAStore
};


static const VerificationEnv::BindingKind bytecodeSignatures[][3] =
{ // Argument 1                 Argument 2                 Result
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIAdd
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLAdd
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkFloat},		// bcFAdd
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkDouble},		// bcDAdd
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcISub
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLSub
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkFloat},		// bcFSub
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkDouble},		// bcDSub
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIMul
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLMul
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkFloat},		// bcFMul
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkDouble},		// bcDMul
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIDiv
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLDiv
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkFloat},		// bcFDiv
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkDouble},		// bcDDiv
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIRem
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLRem
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkFloat},		// bcFRem
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkDouble},		// bcDRem
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcINeg
	{VerificationEnv::bkLong,	VerificationEnv::bkVoid,   VerificationEnv::bkLong},		// bcLNeg
	{VerificationEnv::bkFloat,	VerificationEnv::bkVoid,   VerificationEnv::bkFloat},		// bcFNeg
	{VerificationEnv::bkDouble,	VerificationEnv::bkVoid,   VerificationEnv::bkDouble},		// bcDNeg
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIShl
	{VerificationEnv::bkLong,	VerificationEnv::bkInt,    VerificationEnv::bkLong},		// bcLShl
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIShr
	{VerificationEnv::bkLong,	VerificationEnv::bkInt,    VerificationEnv::bkLong},		// bcLShr
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIUShr
	{VerificationEnv::bkLong,	VerificationEnv::bkInt,    VerificationEnv::bkLong},		// bcLUShr
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIAnd
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLAnd
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIOr
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLOr
	{VerificationEnv::bkInt,	VerificationEnv::bkInt,    VerificationEnv::bkInt},			// bcIXor
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkLong},		// bcLXor
	{VerificationEnv::bkVoid,	VerificationEnv::bkVoid,   VerificationEnv::bkVoid},		// bcIInc
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkLong},		// bcI2L
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkFloat},		// bcI2F
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkDouble},		// bcI2D
	{VerificationEnv::bkLong,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcL2I
	{VerificationEnv::bkLong,	VerificationEnv::bkVoid,   VerificationEnv::bkFloat},		// bcL2F
	{VerificationEnv::bkLong,	VerificationEnv::bkVoid,   VerificationEnv::bkDouble},		// bcL2D
	{VerificationEnv::bkFloat,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcF2I
	{VerificationEnv::bkFloat,	VerificationEnv::bkVoid,   VerificationEnv::bkLong},		// bcF2L
	{VerificationEnv::bkFloat,	VerificationEnv::bkVoid,   VerificationEnv::bkDouble},		// bcF2D
	{VerificationEnv::bkDouble,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcD2I
	{VerificationEnv::bkDouble,	VerificationEnv::bkVoid,   VerificationEnv::bkLong},		// bcD2L
	{VerificationEnv::bkDouble,	VerificationEnv::bkVoid,   VerificationEnv::bkFloat},		// bcD2F
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcInt2Byte
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcInt2Char
	{VerificationEnv::bkInt,	VerificationEnv::bkVoid,   VerificationEnv::bkInt},			// bcInt2Short
	{VerificationEnv::bkLong,	VerificationEnv::bkLong,   VerificationEnv::bkInt},			// bcLCmp
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkInt},			// bcFCmpL
	{VerificationEnv::bkFloat,	VerificationEnv::bkFloat,  VerificationEnv::bkInt},			// bcFCmpG
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkInt},			// bcDCmpL
	{VerificationEnv::bkDouble,	VerificationEnv::bkDouble, VerificationEnv::bkInt}			// bcDCmpG
};


// ----------------------------------------------------------------------------
// SubroutineCallSiteList


//
// Return true if this Association is less than a2 in the "dictionary" ordering with
// the subroutine being the major key and callSite being the minor key.  A nil callSite
// is considered to be less than ony other callSite.
//
bool SubroutineCallSiteList::Association::operator<(const Association &a2) const
{
	// According to ANSI C++, we can't compare subroutine addresses directly because
	// they are not elements of the same array.  Instead, we compare their bytecodesBegin
	// values, which do point into the same array.
	const bytecode *begin1 = subroutine.bytecodesBegin;
	const bytecode *begin2 = a2.subroutine.bytecodesBegin;
	return begin1 < begin2 ||
		   begin1 == begin2 && a2.callSite && (!callSite || callSite->bytecodesBegin < a2.callSite->bytecodesBegin);
}


//
// Create an Iterator that will return all known call sites for the given subroutine.
//
SubroutineCallSiteList::Iterator::Iterator(SubroutineCallSiteList &csList, BytecodeBlock &subroutine):
	subroutine(subroutine)
{
	Association a(subroutine);
	Node *n = csList.tree.findAfter(a);
	if (n && &n->getKey().subroutine != &subroutine)
		n = 0;
	node = n;
}


//
// Advance the iterator to the next call site.
//
void SubroutineCallSiteList::Iterator::operator++()
{
	assert(node);
	Node *n = node->next();
	if (n && &n->getKey().subroutine != &subroutine)
		n = 0;
	node = n;
}


//
// Add callSite to the list of subroutine's call sites if it is not there already.
// Allocate additional storage from pool if needed.
//
void SubroutineCallSiteList::addAssociation(BytecodeBlock &subroutine, BytecodeBlock &callSite, Pool &pool)
{
	Association a(subroutine, callSite);
	Node *where;
	bool right;
	if (!tree.find(a, where, right))
		tree.attach(*new(pool) Node(a), where, right);
}


//
// Remove callSite to the list of subroutine's call sites.  callSite must have been
// previously added (and not yet removed) to the list of subroutine's call sites.
//
void SubroutineCallSiteList::removeAssociation(BytecodeBlock &subroutine, BytecodeBlock &callSite)
{
	Association a(subroutine, callSite);
	Node *n = tree.find(a);
	assert(n);
	tree.remove(*n);
}


//
// Return a callSite and its subroutine of one association present in this
// SubroutineCallSiteList.  At the same time remove that association from this
// SubroutineCallSiteList.  Also return a flag that is true if this was the only
// association for this subroutine.
// If this SubroutineCallSiteList is already empty, return nil.
//
BytecodeBlock *SubroutineCallSiteList::popAssociation(BytecodeBlock *&subroutine, bool &onlyOneCallSite)
{
	Node *n = tree.firstNode();
	if (!n) {
		subroutine = 0;
		return 0;
	}
	subroutine = &n->getKey().subroutine;
	BytecodeBlock *callSite = n->getKey().callSite;
	Node *n2 = n->next();
	onlyOneCallSite = !(n2 && &n2->getKey().subroutine == subroutine);
	tree.remove(*n);
	return callSite;
}


// ----------------------------------------------------------------------------
// BytecodeVerifier


//
// Initialize the BytecodeVerifier and set up all BasicBlocks in the bytecode
// graph for verification.
//
BytecodeVerifier::BytecodeVerifier(BytecodeGraph &bytecodeGraph, Pool &envPool):
	VerificationEnv::Common(envPool, 0, bytecodeGraph.nLocals, bytecodeGraph.stackSize),
	bytecodeGraph(bytecodeGraph),
	classFileSummary(bytecodeGraph.classFileSummary)
{
	BasicBlock **block = bytecodeGraph.getDFSList();
	BasicBlock **dfsListEnd = block + bytecodeGraph.getDFSListLength();
	while (block != dfsListEnd)
		(*block++)->initVerification(*this);
}


//
// If stackNormalization is either sn0, sn1, or sn2, pop all except the top
// zero, one, or two words from env, modifying it in place.
//
void BytecodeVerifier::normalizeEnv(VerificationEnv &env, BasicBlock::StackNormalization stackNormalization)
{
	VerificationEnv::Binding tempBinding1;
	VerificationEnv::Binding tempBinding2;

	switch (stackNormalization) {
		case BasicBlock::snNoChange:
			return;
		case BasicBlock::sn0:
			break;
		case BasicBlock::sn1:
			tempBinding1 = env.pop1();
			break;
		case BasicBlock::sn2:
			env.pop2(tempBinding1, tempBinding2);
			break;
	}
	env.dropAll();
	switch (stackNormalization) {
		case BasicBlock::sn0:
			break;
		case BasicBlock::sn1:
			env.push1(tempBinding1);
			break;
		case BasicBlock::sn2:
			env.push2(tempBinding1, tempBinding2);
			break;
		default:
			trespass("Bad stackNormalization");
	}
}


//
// Intersect block's current incoming environment with predecessorEnv, updating the
// block's incoming environment.  Return true if both conditions below are true:
//   the block's incoming environment changed, and
//   the block's generation is already equal to the given generation.
// When this method returns true, a block already examined on this pass through the
// depth-first list has changed, so another pass is needed.
//
// Set this block's recompute flag if its incoming environment changed.
//
// If canOwnEnv is true, the BasicBlock can assume ownership of predecessorEnv
// (thereby sometimes eliminating an unnecessary copy).
//
// Throw a verification error if the environments cannot be intersected.
//
bool BytecodeVerifier::predecessorChanged(BasicBlock &block, VerificationEnv &predecessorEnv, Uint32 generation, bool canOwnEnv)
{
	assert(predecessorEnv.live());
	BasicBlock::StackNormalization stackNormalization = block.stackNormalization;
	if (stackNormalization != BasicBlock::snNoChange && predecessorEnv.getSP() != (Uint32)(stackNormalization-BasicBlock::sn0))
		// We have to normalize the stack inside predecessorEnv.
		if (canOwnEnv)
			normalizeEnv(predecessorEnv, stackNormalization);
		else {
			VerificationEnv envCopy(predecessorEnv);
			normalizeEnv(envCopy, stackNormalization);
			return predecessorChanged(block, envCopy, generation, true);
		}

	bool changed = block.getGeneration() == generation;
	if (block.getVerificationEnvIn().live())
		if (block.getVerificationEnvIn().meet(predecessorEnv))
			block.getRecompute() = true;
		else
			changed = false;
	else {
		block.getRecompute() = true;
		if (canOwnEnv)
			block.getVerificationEnvIn().move(predecessorEnv);
		else
			block.getVerificationEnvIn() = predecessorEnv;
	}
	return changed;
}


//
// Call predecessorChanged on every BasicBlock between blocksBegin (inclusive)
// and blocksEnd (exclusive).  Return true if any predecessorChanged call returned
// true.  Of course, canOwnEnv applies only to the last call to predecessorChanged.
//
bool BytecodeVerifier::predecessorChanged(BasicBlock **blocksBegin, BasicBlock **blocksEnd, VerificationEnv &predecessorEnv,
										  Uint32 generation, bool canOwnEnv)
{
	bool changed = false;
	while (blocksBegin != blocksEnd) {
		BasicBlock *block = *blocksBegin++;
		changed |= predecessorChanged(*block, predecessorEnv, generation, blocksBegin == blocksEnd && canOwnEnv);
	}
	return changed;
}


//
// Compute the verificationEnvIn, subroutineHeader, and subroutineRet fields of
// the block.  Verify the block's stack and type discpiline.  Return true if
// calling predecessorChanged on at least one of this block's successors returned true.
//
bool BytecodeVerifier::propagateDataflow(CatchBlock &block, Uint32 generation)
{
	VerificationEnv env(block.getVerificationEnvIn());
	env.push1(VerificationEnv::bkAddr);
	return predecessorChanged(block.getHandler(), env, generation, true);
}


//
// Compute the verificationEnvIn, subroutineHeader, and subroutineRet fields of
// the block.  Verify the block's stack and type discipline.  Return true if
// calling predecessorChanged on at least one of this block's successors returned true.
//
bool BytecodeVerifier::propagateDataflow(BytecodeBlock &block, Uint32 generation)
{
	VerificationEnv env(block.getVerificationEnvIn());
	// Follow exception handlers from this block at the beginning of this block and
	// after each change of a local variable.
	bool changed = predecessorChanged(block.successorsEnd, block.handlersEnd, env, generation, false);

	const bytecode *const bytecodesEnd = block.bytecodesEnd;
	const bytecode *bc = block.bytecodesBegin;
	assert(bc != bytecodesEnd);
	while (bc != bytecodesEnd) {
		assert(bc < bytecodesEnd);
		bytecode opcode = *bc++;
		ValueKind vk;
		TypeKind tk;
		Value v;
		Uint32 i;
		ConstantPoolIndex cpi;
		const VerificationEnv::Binding *b;
		const VerificationEnv::BindingKind *bk;
		VerificationEnv::Binding b1;
		VerificationEnv::Binding b2;
		VerificationEnv::Binding b3;
		VerificationEnv::Binding b4;
		Signature sig;
		Uint32 offset;
		Uint32 interfaceNumber;
		addr address;
		bool isVolatile;
		bool isConstant;
		bool isInit;
		BytecodeBlock *sub;

		switch (opcode) {

			case bcNop:
				break;

			case bcAConst_Null:
				env.push1(VerificationEnv::bkAddr);
				break;
			case bcIConst_m1:
			case bcIConst_0:
			case bcIConst_1:
			case bcIConst_2:
			case bcIConst_3:
			case bcIConst_4:
			case bcIConst_5:
			  pushInt:
				env.push1(VerificationEnv::bkInt);
				break;
			case bcLConst_0:
			case bcLConst_1:
				env.push2(VerificationEnv::bkLong);
				break;
			case bcFConst_0:
			case bcFConst_1:
			case bcFConst_2:
				env.push1(VerificationEnv::bkFloat);
				break;
			case bcDConst_0:
			case bcDConst_1:
				env.push2(VerificationEnv::bkDouble);
				break;
			case bcBIPush:
				bc++;
				goto pushInt;
			case bcSIPush:
				bc += 2;
				goto pushInt;
			case bcLdc:
				cpi = *(Uint8 *)bc;
				bc++;
				goto pushConst1;
			case bcLdc_W:
				cpi = readBigUHalfwordUnaligned(bc);
				bc += 2;
			  pushConst1:
				vk = classFileSummary.lookupConstant(cpi, v);
				if (!isWordKind(vk))
					verifyError(VerifyError::badType);
				env.push1(VerificationEnv::valueKindToBindingKind(vk));
				break;
			case bcLdc2_W:
				vk = classFileSummary.lookupConstant(readBigUHalfwordUnaligned(bc), v);
				bc += 2;
				if (!isDoublewordKind(vk))
					verifyError(VerifyError::badType);
				env.push2(VerificationEnv::valueKindToBindingKind(vk));
				break;

			case bcILoad:
			case bcFLoad:
			case bcALoad:
				i = *(Uint8 *)bc;
				bc++;
			  pushLoad1:
				b = &env.getLocal1(i);
				if (!b->hasKind(localAccessKinds[opcode - bcILoad]))
					verifyError(VerifyError::badType);
				env.push1(*b);
				break;
			case bcLLoad:
			case bcDLoad:
				i = *(Uint8 *)bc;
				bc++;
			  pushLoad2:
				b = &env.getLocal2(i);
				if (!b->hasKind(localAccessKinds[opcode - bcILoad]))
					verifyError(VerifyError::badType);
				env.push2(*b);
				break;
			case bcILoad_0:
			case bcILoad_1:
			case bcILoad_2:
			case bcILoad_3:
				i = opcode - bcILoad_0;
				goto pushLoad1;
			case bcLLoad_0:
			case bcLLoad_1:
			case bcLLoad_2:
			case bcLLoad_3:
				i = opcode - bcLLoad_0;
				goto pushLoad2;
			case bcFLoad_0:
			case bcFLoad_1:
			case bcFLoad_2:
			case bcFLoad_3:
				i = opcode - bcFLoad_0;
				goto pushLoad1;
			case bcDLoad_0:
			case bcDLoad_1:
			case bcDLoad_2:
			case bcDLoad_3:
				i = opcode - bcDLoad_0;
				goto pushLoad2;
			case bcALoad_0:
			case bcALoad_1:
			case bcALoad_2:
			case bcALoad_3:
				i = opcode - bcALoad_0;
				goto pushLoad1;

			case bcIALoad:
			case bcLALoad:
			case bcFALoad:
			case bcDALoad:
			case bcAALoad:
			case bcBALoad:
			case bcCALoad:
			case bcSALoad:
				env.pop1(VerificationEnv::bkInt);
				env.pop1(VerificationEnv::bkAddr);
				env.push1or2(arrayAccessKinds[opcode - bcIALoad]);
				break;

			case bcIStore:
			case bcFStore:
				i = *(Uint8 *)bc;
				bc++;
			  popStore1:
				env.setLocal1(i, env.pop1(localAccessKinds[opcode - bcIStore]));
			  recheckHandlers:
				// We changed the local environment, so propagate the environment to the
				// exception handlers again.
				changed |= predecessorChanged(block.successorsEnd, block.handlersEnd, env, generation, false);
				break;
			case bcAStore:
				i = *(Uint8 *)bc;
				bc++;
			  popAStore1:
				b = &env.pop1();
				if (!(b->hasKind(VerificationEnv::bkAddr) || b->hasKind(VerificationEnv::bkReturn)))
					verifyError(VerifyError::badType);
				env.setLocal1(i, *b);
				goto recheckHandlers;
			case bcLStore:
			case bcDStore:
				i = *(Uint8 *)bc;
				bc++;
			  popStore2:
				env.setLocal2(i, env.pop2(localAccessKinds[opcode - bcIStore]));
				goto recheckHandlers;

			case bcIStore_0:
			case bcIStore_1:
			case bcIStore_2:
			case bcIStore_3:
				i = opcode - bcIStore_0;
				goto popStore1;
			case bcLStore_0:
			case bcLStore_1:
			case bcLStore_2:
			case bcLStore_3:
				i = opcode - bcLStore_0;
				goto popStore2;
			case bcFStore_0:
			case bcFStore_1:
			case bcFStore_2:
			case bcFStore_3:
				i = opcode - bcFStore_0;
				goto popStore1;
			case bcDStore_0:
			case bcDStore_1:
			case bcDStore_2:
			case bcDStore_3:
				i = opcode - bcDStore_0;
				goto popStore2;
			case bcAStore_0:
			case bcAStore_1:
			case bcAStore_2:
			case bcAStore_3:
				i = opcode - bcAStore_0;
				goto popAStore1;

			case bcIAStore:
			case bcLAStore:
			case bcFAStore:
			case bcDAStore:
			case bcAAStore:
			case bcBAStore:
			case bcCAStore:
			case bcSAStore:
				env.pop1or2(arrayAccessKinds[opcode - bcIAStore]);
				env.pop1(VerificationEnv::bkInt);
				env.pop1(VerificationEnv::bkAddr);
				break;

			case bcPop:
				env.pop1();
				break;
			case bcPop2:
				env.pop2(b1, b2);
				break;
			case bcDup:
				b1 = env.pop1();
				env.push1(b1);
				env.push1(b1);
				break;
			case bcDup_x1:
				b1 = env.pop1();
				b2 = env.pop1();
				env.push1(b1);
				env.push1(b2);
				env.push1(b1);
				break;
			case bcDup_x2:
				b1 = env.pop1();
				env.pop2(b2, b3);
				env.push1(b1);
				env.push2(b2, b3);
				env.push1(b1);
				break;
			case bcDup2:
				env.pop2(b1, b2);
				env.push2(b1, b2);
				env.push2(b1, b2);
				break;
			case bcDup2_x1:
				env.pop2(b1, b2);
				b3 = env.pop1();
				env.push2(b1, b2);
				env.push1(b3);
				env.push2(b1, b2);
				break;
			case bcDup2_x2:
				env.pop2(b1, b2);
				env.pop2(b3, b4);
				env.push2(b1, b2);
				env.push2(b3, b4);
				env.push2(b1, b2);
				break;
			case bcSwap:
				b1 = env.pop1();
				b2 = env.pop1();
				env.push1(b1);
				env.push1(b2);
				break;

			case bcIAdd:
			case bcLAdd:
			case bcFAdd:
			case bcDAdd:
			case bcISub:
			case bcLSub:
			case bcFSub:
			case bcDSub:
			case bcIMul:
			case bcLMul:
			case bcFMul:
			case bcDMul:
			case bcIDiv:
			case bcLDiv:
			case bcFDiv:
			case bcDDiv:
			case bcIRem:
			case bcLRem:
			case bcFRem:
			case bcDRem:
			case bcIShl:
			case bcLShl:
			case bcIShr:
			case bcLShr:
			case bcIUShr:
			case bcLUShr:
			case bcIAnd:
			case bcLAnd:
			case bcIOr:
			case bcLOr:
			case bcIXor:
			case bcLXor:
			case bcLCmp:
			case bcFCmpL:
			case bcFCmpG:
			case bcDCmpL:
			case bcDCmpG:
				bk = bytecodeSignatures[opcode - bcIAdd];
				env.pop1or2(bk[1]);
				goto popPushUnary;

			case bcINeg:
			case bcLNeg:
			case bcFNeg:
			case bcDNeg:
			case bcI2L:
			case bcI2F:
			case bcI2D:
			case bcL2I:
			case bcL2F:
			case bcL2D:
			case bcF2I:
			case bcF2L:
			case bcF2D:
			case bcD2I:
			case bcD2L:
			case bcD2F:
			case bcInt2Byte:
			case bcInt2Char:
			case bcInt2Short:
				bk = bytecodeSignatures[opcode - bcIAdd];
			  popPushUnary:
				env.pop1or2(bk[0]);
				env.push1or2(bk[2]);
				break;

			case bcIInc:
				i = *(Uint8 *)bc;
				bc += 2;
			  incLocal:
				if (!env.getLocal1(i).hasKind(VerificationEnv::bkInt))
					verifyError(VerifyError::badType);
				break;

			case bcIf_ICmpEq:
			case bcIf_ICmpNe:
			case bcIf_ICmpLt:
			case bcIf_ICmpGe:
			case bcIf_ICmpGt:
			case bcIf_ICmpLe:
				env.pop1(VerificationEnv::bkInt);
			case bcIfEq:
			case bcIfNe:
			case bcIfLt:
			case bcIfGe:
			case bcIfGt:
			case bcIfLe:
				env.pop1(VerificationEnv::bkInt);
				bc += 2;
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;
			case bcIf_ACmpEq:
			case bcIf_ACmpNe:
				env.pop1(VerificationEnv::bkAddr);
			case bcIfNull:
			case bcIfNonnull:
				env.pop1(VerificationEnv::bkAddr);
				bc += 2;
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;

			case bcGoto_W:
				bc += 2;
			case bcGoto:
				bc += 2;
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;

			case bcTableSwitch:
			case bcLookupSwitch:
				env.pop1(VerificationEnv::bkInt);
				bc = bytecodesEnd;
				break;

			case bcIReturn:
			case bcLReturn:
			case bcFReturn:
			case bcDReturn:
			case bcAReturn:
				env.pop1or2(arrayAccessKinds[opcode - bcIReturn]);
			case bcReturn:
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;

			case bcGetStatic:
				env.push1or2(VerificationEnv::typeKindToBindingKind(
						classFileSummary.lookupStaticField(readBigUHalfwordUnaligned(bc), vk, address, isVolatile, isConstant)));
				bc += 2;
				break;
			case bcPutStatic:
				env.pop1or2(VerificationEnv::typeKindToBindingKind(
						classFileSummary.lookupStaticField(readBigUHalfwordUnaligned(bc), vk, address, isVolatile, isConstant)));
				bc += 2;
				break;
			case bcGetField:
				env.pop1(VerificationEnv::bkAddr);
				env.push1or2(VerificationEnv::typeKindToBindingKind(
						classFileSummary.lookupInstanceField(readBigUHalfwordUnaligned(bc), vk, offset, isVolatile, isConstant)));
				bc += 2;
				break;
			case bcPutField:
				env.pop1or2(VerificationEnv::typeKindToBindingKind(
						classFileSummary.lookupInstanceField(readBigUHalfwordUnaligned(bc), vk, offset, isVolatile, isConstant)));
				env.pop1(VerificationEnv::bkAddr);
				bc += 2;
				break;

			case bcInvokeVirtual:
				classFileSummary.lookupVirtualMethod(readBigUHalfwordUnaligned(bc), sig, offset, address);
			  popPushInvoke:
				bc += 2;
				i = sig.nArguments;
				while (i--)
					env.pop1or2(VerificationEnv::typeKindToBindingKind(sig.argumentTypes[i]->typeKind));
				tk = sig.resultType->typeKind;
				if (tk != tkVoid)
					env.push1or2(VerificationEnv::typeKindToBindingKind(tk));
				break;
			case bcInvokeSpecial:
				classFileSummary.lookupSpecialMethod(readBigUHalfwordUnaligned(bc), sig, isInit, address);
				goto popPushInvoke;
			case bcInvokeStatic:
				classFileSummary.lookupStaticMethod(readBigUHalfwordUnaligned(bc), sig, address);
				goto popPushInvoke;
			case bcInvokeInterface:
				classFileSummary.lookupInterfaceMethod(readBigUHalfwordUnaligned(bc), sig, offset, interfaceNumber, address, *(Uint8 *)(bc + 2));
				bc += 2;
				goto popPushInvoke;

			case bcNew:
				classFileSummary.lookupClass(readBigUHalfwordUnaligned(bc));
				bc += 2;
				env.push1(VerificationEnv::bkAddr);
				break;

			case bcNewArray:
				i = *(Uint8 *)bc;
				bc++;
				i -= natMin;
				if (i >= natLimit - natMin)
					verifyError(VerifyError::badNewArrayType);
				env.pop1(VerificationEnv::bkInt);
				env.push1(VerificationEnv::bkAddr);
				break;

			case bcANewArray:
				classFileSummary.lookupType(readBigUHalfwordUnaligned(bc));
				bc += 2;
				env.pop1(VerificationEnv::bkInt);
				env.push1(VerificationEnv::bkAddr);
				break;

			case bcMultiANewArray:
				classFileSummary.lookupType(readBigUHalfwordUnaligned(bc));
				i = *(Uint8 *)(bc+2);	// Number of dimensions
				bc += 3;
				if (i == 0)
					verifyError(VerifyError::badNewArrayType);
				while (i--)
					env.pop1(VerificationEnv::bkInt);
				env.push1(VerificationEnv::bkAddr);
				break;

			case bcArrayLength:
				env.pop1(VerificationEnv::bkAddr);
				env.push1(VerificationEnv::bkInt);
				break;

			case bcAThrow:
				env.pop1(VerificationEnv::bkAddr);
				// The rest of this BytecodeBlock is dead, and we should be at the end of it.
				assert(bc == bytecodesEnd);
				break;

			case bcCheckCast:
				classFileSummary.lookupType(readBigUHalfwordUnaligned(bc));
				bc += 2;
				env.pop1(VerificationEnv::bkAddr);
				env.push1(VerificationEnv::bkAddr);
				break;

			case bcInstanceOf:
				classFileSummary.lookupType(readBigUHalfwordUnaligned(bc));
				bc += 2;
				env.pop1(VerificationEnv::bkAddr);
				env.push1(VerificationEnv::bkInt);
				break;

			case bcMonitorEnter:
			case bcMonitorExit:
				env.pop1(VerificationEnv::bkAddr);
				break;

			case bcWide:
				opcode = *bc++;
				i = readBigUHalfwordUnaligned(bc);
				bc += 2;
				switch (opcode) {
					case bcILoad:
					case bcFLoad:
					case bcALoad:
						goto pushLoad1;
					case bcLLoad:
					case bcDLoad:
						goto pushLoad2;

					case bcIStore:
					case bcFStore:
						goto popStore1;
					case bcLStore:
					case bcDStore:
						goto popStore2;
					case bcAStore:
						goto popAStore1;

					case bcIInc:
						bc += 2;
						goto incLocal;

					case bcRet:
						goto handleRet;

					default:
						verifyError(VerifyError::badBytecode);
				}
				break;

			case bcBreakpoint:
				// Ignore breakpoints.
				break;

			case bcJsr_W:
				bc += 2;
			case bcJsr:
				bc += 2;
				// We have two successors to a jsr bytecode's basic block.  The first is the
				// code that follows the jsr bytecode, while the second is the beginning of the
				// subroutine.
				// At this point we do not propagate the environment to the code that follows
				// the jst bytecode because we don't know the state of the registers, and, besides,
				// the subroutine might not return at all.  Instead, we only propagate the environment
				// into the subroutine and rely on ret to propagate it to the instructions after
				// all jsr's that could have called that subroutine.
				assert(block.hasSubkind(BytecodeBlock::skJsr));
				sub = &block.getSuccessor(0);
				env.enterSubroutine(sub);
				b1.setReturn(sub);
				env.push1(b1);

				subroutineCallSiteList.addAssociation(*sub, block, envPool);

				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				changed |= predecessorChanged(*sub, env, generation, true);
				return changed;

			case bcRet:
				i = *(Uint8 *)bc;
				bc++;
			  handleRet:
				b = &env.getLocal1(i);
				if (!b->hasKind(VerificationEnv::bkReturn))
					verifyError(VerifyError::badType);
				sub = b->getSubroutine();
				// We have to figure out the successors dynamically using the subroutine's address.
				assert(block.hasSubkind(BytecodeBlock::skRet) && block.nSuccessors() == 0);
				{
					// This can be an assert instead of an if/verifyError because if we really did have
					// two jsr's going to this ret, the return address local's BindingKind would become
					// bkVoid instead of bkReturn and we would catch this above.
					assert(!block.getSubroutineHeader() || block.getSubroutineHeader() == sub);
					block.getSubroutineHeader() = sub;
					BytecodeBlock *oldRet = sub->getSubroutineRet();
					if (oldRet && oldRet != &block)
						verifyError(VerifyError::multipleRet);
					sub->getSubroutineRet() = &block;

					for (SubroutineCallSiteList::Iterator iter(subroutineCallSiteList, *sub); iter.more(); ++iter) {
						BytecodeBlock &succ = *iter;
						assert(succ.hasSubkind(BytecodeBlock::skJsr));
						VerificationEnv succEnv(env);
						succEnv.exitSubroutine(sub, succ.getVerificationEnvIn());
						changed |= predecessorChanged(succ.getSuccessor(1), succEnv, generation, true);
					}
				}
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				return changed;

			default:
				verifyError(VerifyError::badBytecode);
		}
	}

	// Propagate dataflow information to all non-exceptional successors.
	changed |= predecessorChanged(block.successorsBegin, block.successorsEnd, env, generation, true);
	return changed;
}


//
// Compute the verificationEnvIn, subroutineHeader, and subroutineRet fields of
// each block in the bytecode graph.  This will verify the method's stack and
// type discpiline and match each ret (and wide ret) bytecode in the method with
// the entry address of the subroutine to which it corresponds.
// Set all retReachable fields of Context structures in the verification environments
// to false.
//
// depthFirstSearch should have been called on entry to this function.  The
// subroutineHeader and subroutineRet fields of each bytecode graph block should
// be nil on entry.
//
// This function uses the generation and recompute fields of bytecode graph blocks
// as temporaries.  It expects all recompute flags to be false on entry, and it
// leaves them that way on exit.
//
void BytecodeVerifier::computeDataflow()
{
	Pool bindingPool;
	setBindingPool(bindingPool);

	BasicBlock **dfsList = bytecodeGraph.getDFSList();
	BasicBlock **dfsListEnd = dfsList + bytecodeGraph.getDFSListLength() - 1;
	assert(bytecodeGraph.getDFSListLength() > 0);

	Uint32 generation = 0;

	// Set up the locals and arguments
	VerificationEnv env(*this);
	env.initLive();
	uint nArgs = bytecodeGraph.nArguments;
	uint slotOffset = 0;
	const ValueKind *ak = bytecodeGraph.argumentKinds;
	for (uint n = 0; n != nArgs; n++) {
		VerificationEnv::Binding b;
		b.setKind(VerificationEnv::valueKindToBindingKind(*ak++));
		if (b.isOneWord())
			env.setLocal1(slotOffset++, b);
		else {
			env.setLocal2(slotOffset, b);
			slotOffset += 2;
		}
	}

	bool changed = predecessorChanged(*bytecodeGraph.beginBlock, env, generation, true);
	while (changed) {
		generation++;
		changed = false;
		BasicBlock **blockPtr = dfsList;
		while (blockPtr != dfsListEnd) {
			BasicBlock *block = *blockPtr++;
			block->getGeneration() = generation;
			if (block->getRecompute()) {
				block->getRecompute() = false;
				assert(block->getVerificationEnvIn().live());
				assert(block->hasKind(BasicBlock::bbBytecode) || block->hasKind(BasicBlock::bbCatch));
				if (block->hasKind(BasicBlock::bbBytecode))
					changed |= propagateDataflow(*static_cast<BytecodeBlock *>(block), generation);
				else
					changed |= propagateDataflow(*static_cast<CatchBlock *>(block), generation);
			}
		}
	}

	assert((*dfsListEnd)->hasKind(BasicBlock::bbEnd));
	(*dfsListEnd)->getRecompute() = false;
  #ifdef DEBUG
	// There shouldn't be any blocks left to recompute.
	for (BasicBlock **blockPtr = dfsList; blockPtr != dfsListEnd+1; blockPtr++)
		assert(!(*blockPtr)->getRecompute());
  #endif
	clearBindingPool();
}


//
// Do two things:
//
// 1.  Compute the retReachable fields of Context structures in the verification environments
// in each block in the bytecode graph.  The rest of the environments should have been
// computed by computeDataflow.
//
// Let B be a block with one or more Contexts in its verification environment and let
// C be one of those Contexts and S be C's subroutine.  On output C's retReachable will
// be set if S's ret can be reached from B without going through S's jsr again.
// If C's retReachable is not set, we can consider B to not really be a part of S
// because there is no way to reach S's ret from B; instead, we consider S to have
// exited via a jump before it got to B.
//
// 2.  Convert each BytecodeBlocks with a jsr's for which there is no ret into a
// skJsrNoRet-subkind block.  That block will have one successor instead of two because
// the subroutine it calls cannot return.
//
// This function uses the generation fields of bytecode graph blocks as temporaries.
//
void BytecodeVerifier::computeRetReachables()
{
	BasicBlock **dfsList = bytecodeGraph.getDFSList();
	BasicBlock **dfsListEnd = dfsList + bytecodeGraph.getDFSListLength();

	bool changed = false;
	BasicBlock **blockPtr = dfsList;
	while (blockPtr != dfsListEnd) {
		BasicBlock *block = *blockPtr++;
		block->getGeneration() = 0;
		if (block->hasKind(BasicBlock::bbBytecode) && block->getVerificationEnvIn().live()) {
			BytecodeBlock::Subkind subkind = static_cast<BytecodeBlock *>(block)->subkind;
			if (subkind == BytecodeBlock::skRet) {
				block->getVerificationEnvIn().setRetReachable(block->getSubroutineHeader());
				block->getGeneration() = 1;
				changed = true;
			} else if (subkind == BytecodeBlock::skJsr) {
				assert(block->nSuccessors() == 2);
				if (!block->getSuccessor(0).getSubroutineRet()) {
					static_cast<BytecodeBlock *>(block)->transformToJsrNoRet();
					subroutineCallSiteList.removeAssociation(block->getSuccessor(0), *static_cast<BytecodeBlock *>(block));
				}
			}
		}
	}

	dfsListEnd--;	// Ignore the end block from now on.
	Uint32 generation = 1;
	while (changed) {
		Uint32 oldGeneration = generation;
		generation++;
		changed = false;
		// We're doing a backward analysis, so traverse the DFS list in reverse order.
		// This isn't quite as good as computing the DFS of the reverse graph, but it works;
		// figuring out the real DFS of the reverse graph would require reversing the graph's
		// edges and be quite messy.
		blockPtr = dfsListEnd;
		while (blockPtr != dfsList) {
			BasicBlock *block = *--blockPtr;
			// For each live block propagate retReachable information backward from successors
			// (including exceptional successors) that have changed in either this or the previous
			// generation.  Set the generation of any blocks that change as a result to the current
			// generation so that they can be propagated further.
			if (block->getVerificationEnvIn().live()) {
				BasicBlock **succPtr = block->successorsBegin;
				BasicBlock **succEnd = block->handlersEnd;
				BytecodeBlock *sub = 0;
				if (block->hasKind(BasicBlock::bbBytecode) && static_cast<BytecodeBlock *>(block)->hasSubkind(BytecodeBlock::skJsr))
					sub = &block->getSuccessor(0);
				while (succPtr != succEnd) {
					BasicBlock *succ = *succPtr++;
					if (succ->getGeneration() >= oldGeneration)
						if (block->getVerificationEnvIn().mergeRetReachables(succ->getVerificationEnvIn(), sub)) {
							changed = true;
							block->getGeneration() = generation;
						}
					sub = 0;
				}
			}
		}
	}
}


struct DuplicateHelper
{
	typedef BasicBlock *Successor;
	typedef BasicBlock *NodeRef;
	
	const Uint32 generation;					// currentGeneration from BytecodeVerifier::duplicateSubroutines

	explicit DuplicateHelper(Uint32 generation): generation(generation) {}

	static Successor *getSuccessorsBegin(NodeRef n) {return n->successorsBegin;}
	static Successor *getSuccessorsEnd(NodeRef n) {return n->handlersEnd;}
	static NodeRef getNodeRef(Successor s) {return s;}
};


struct DuplicatePass1Helper: DuplicateHelper
{
	BytecodeGraph &bytecodeGraph;				// The BytecodeGraph which we're examining
	BytecodeBlock *subroutine;					// Header of the subroutine whose body we're marking

	DuplicatePass1Helper(BytecodeGraph &bytecodeGraph, BytecodeBlock *subroutine, Uint32 generation):
		DuplicateHelper(generation), bytecodeGraph(bytecodeGraph), subroutine(subroutine) {}

	bool isMarked(NodeRef n) {return n->getGeneration() == generation || !n->getVerificationEnvIn().isRetReachable(subroutine);}
	void setMarked(NodeRef n);
};


//
// Make a clone of block n and store a pointer to that clone in n's clone field.
// Make the clone's successors and handlers point to the same blocks to which n's
// successors and handlers point.
// Also set n's generation to the current generation.
//
void DuplicatePass1Helper::setMarked(BasicBlock *n)
{
	assert(n->getGeneration() != generation);
	n->getGeneration() = generation;
	BasicBlock *clone;
	Pool &bytecodeGraphPool = bytecodeGraph.bytecodeGraphPool;
	if (n->hasKind(BasicBlock::bbBytecode))
		clone = new(bytecodeGraphPool) BytecodeBlock(bytecodeGraph, *static_cast<BytecodeBlock *>(n));
	else {
		assert(n->hasKind(BasicBlock::bbCatch));
		clone = new(bytecodeGraphPool) CatchBlock(bytecodeGraph, *static_cast<CatchBlock *>(n));
	}
	n->getClone() = clone;
}


struct DuplicatePass2Helper: DuplicateHelper, Function1<BytecodeBlock *, BytecodeBlock *>
{
	SubroutineCallSiteList &subroutineCallSiteList; // Reference to BytecodeVerifier::subroutineCallSiteList
	Pool &envPool;								// Reference to BytecodeVerifier::envPool

	DuplicatePass2Helper(SubroutineCallSiteList &subroutineCallSiteList, Pool &envPool, Uint32 generation):
		DuplicateHelper(generation), subroutineCallSiteList(subroutineCallSiteList), envPool(envPool) {}

	bool isMarked(NodeRef n) {return n->getGeneration() != generation;}
	void setMarked(NodeRef n);

	BytecodeBlock *operator()(BytecodeBlock *arg);
};


//
// This block has an already created clone saved in its clone field.
// For each of this block's successors and catch handlers s, if s points to
// a block in this subroutine (which we can tell by checking whether s's generation
// is greater than or equal to this DuplicatePass2Helper's generation), replace s
// by s's clone.
// If this block has subkind skJsr, update the subroutineCallSiteList to also include
// this block's clone, which is a newly created call site.
//
// Set this block's generation to this DuplicatePass2Helper's generation plus one to
// mark this block so that setMarked is not called on it again.
//
void DuplicatePass2Helper::setMarked(BasicBlock *n)
{
	assert(n->getGeneration() == generation);
	n->getGeneration() = generation + 1;
	BasicBlock *const clone = n->getClone();
	assert(clone);

	// Fix up the VerificationTemps.
	clone->initVerification(*n, *this);

	// Fix up the successor and handler pointers.
	BasicBlock **successor = clone->successorsBegin;
	BasicBlock **limit = clone->handlersEnd;
	while (successor != limit) {
		BasicBlock *s = *successor;
		assert(s);
		if (s->getGeneration() >= generation)
			*successor = s->getClone();
		successor++;
	}

	if (clone->hasKind(BasicBlock::bbBytecode)) {
		BytecodeBlock *const bytecodeClone = static_cast<BytecodeBlock *>(clone);

		// Fix up the catchBlock pointer.  If it's non-nil, it must point inside this subroutine.
		CatchBlock *c = bytecodeClone->catchBlock;
		if (c) {
			assert(c->getGeneration() >= generation);
			bytecodeClone->catchBlock = static_cast<CatchBlock *>(c->getClone());
		}

		// If clone has subkind skJsr, update the subroutineCallSiteList.
		if (bytecodeClone->hasSubkind(BytecodeBlock::skJsr)) {
			subroutineCallSiteList.addAssociation(bytecodeClone->getSuccessor(0), *bytecodeClone, envPool);
		}
	} else
		assert(clone->hasKind(BasicBlock::bbCatch));
}


//
// If arg points inside the subroutine currently being duplicated, return its clone;
// if arg is nil or points outside that subroutine, return it unchanged.
//
BytecodeBlock *DuplicatePass2Helper::operator()(BytecodeBlock *arg)
{
	if (arg && arg->getGeneration() >= generation)
		arg = static_cast<BytecodeBlock *>(arg->getClone());
	return arg;
}


//
// Duplicate each subroutine called by jsr's inside skJsr-kind (not skJsrNoRet-kind) blocks.
// This function relies on the bytecode graph being set up by computeRetReachables.
// Furthermore, no blocks should have been added to the graph since its last DFS search.
// On exit the graph will contain no unforwarded ret's and every jsr will be inside a
// skJsrNoRet-kind block.
//
// This function uses the generation and clone fields of bytecode graph blocks as temporaries.
//
void BytecodeVerifier::duplicateSubroutines()
{
	Pool searchPool;
	Uint32 nGraphBlocks = bytecodeGraph.getDFSListLength();	// Upper bound on the current number of blocks in the graph
	SearchStackEntry<BasicBlock *> *searchStack = 0;
	Uint32 searchStackSize = 0;

	BasicBlock **block = bytecodeGraph.getDFSList();
	BasicBlock **dfsListEnd = block + nGraphBlocks;
	while (block != dfsListEnd)
		(*block++)->getGeneration() = 0;

	Uint32 currentGeneration = 2;
	BytecodeBlock *callSite;
	BytecodeBlock *subroutine;
	bool onlyOneCallSite;
	while ((callSite = subroutineCallSiteList.popAssociation(subroutine, onlyOneCallSite)) != 0) {
		BytecodeBlock *subroutineRet = subroutine->getSubroutineRet();
		assert(subroutineRet && callSite->hasSubkind(BytecodeBlock::skJsr));

		if (!onlyOneCallSite) {
			// There is more than one jsr remaining to this subroutine, so we copy the entire
			// subroutine.
			if (nGraphBlocks > searchStackSize) {
				searchStackSize = nGraphBlocks * 2;
				searchStack = new(searchPool) SearchStackEntry<BasicBlock *>[searchStackSize];
			}
			{
				// Make a clone of every BasicBlock in the subroutine and store a pointer to that
				// clone in the original BasicBlock's clone field.  The clones' successors will still
				// point to the original blocks; we'll take care of that later.
				// To recognize which blocks we've cloned already we set the generation field of
				// every visited block in the subroutine to currentGeneration.
				DuplicatePass1Helper pass1Helper(bytecodeGraph, subroutine, currentGeneration);
				graphSimpleSearch(pass1Helper, static_cast<BasicBlock *>(subroutine), searchStackSize, searchStack);
			}

			// Now change all successor and catch block pointers in the cloned blocks point to
			// the clones of their successors/catch blocks if there are any.  Leave pointers to
			// blocks outside the subroutine as they are.  Change the generation of each block
			// whose clone we transform this way to currentGeneration+1 to mark it so we don't
			// transform it twice.
			DuplicatePass2Helper pass2Helper(subroutineCallSiteList, envPool, currentGeneration);
			graphSimpleSearch(pass2Helper, static_cast<BasicBlock *>(subroutine), searchStackSize, searchStack);

			// Change the subroutine pointer of our call site.
			assert(subroutine->getGeneration() >= currentGeneration && callSite->nSuccessors() == 2);
			callSite->successorsBegin[0] = subroutine->getClone();
			// Set up subroutineRet to point to the clone of the original ret.
			assert(subroutineRet->getGeneration() >= currentGeneration);
			subroutineRet = static_cast<BytecodeBlock *>(subroutineRet->getClone());

			// Advance the generation.
			currentGeneration += 2;
			// Throw an error if we went through 2^32 generations so far (unlikely, but it's better to be safe!).
			if (currentGeneration < 2)
				verifyError(VerifyError::jsrNestingError);
		}

		// At this point we've already copied the subroutine if needed.  Now we
		// just forward its ret to the successor of the jsr.
		subroutineRet->transformToForward(callSite->transformToJsrNoRet());
	}
}


//
// Inline all jsr-ret subroutines in this BytecodeGraph, eliminating all
// jsr and ret bytecodes.  depthFirstSearch should have been called on entry
// to this function, and it needs to be called again after this function exits.
//
void BytecodeVerifier::inlineSubroutines(BytecodeGraph &bytecodeGraph, Pool &tempPool)
{
	BytecodeVerifier verifier(bytecodeGraph, tempPool);

	verifier.computeDataflow();
	verifier.computeRetReachables();
	verifier.duplicateSubroutines();
	bytecodeGraph.invalidateDFSList();
}
