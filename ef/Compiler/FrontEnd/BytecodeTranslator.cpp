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

#include "BytecodeTranslator.h"
#include "PrimitiveBuilders.h"
#include "MemoryAccess.h"
#include "FieldOrMethod.h"

static const UnaryBuilder *const unaryBuilders[] =
{
	&builder_Neg_II,		// bcINeg
	&builder_Neg_LL,		// bcLNeg
	&builder_Neg_FF,		// bcFNeg
	&builder_Neg_DD,		// bcDNeg
	0,						// bcIShl
	0,						// bcLShl
	0,						// bcIShr
	0,						// bcLShr
	0,						// bcIUShr
	0,						// bcLUShr
	0,						// bcIAnd
	0,						// bcLAnd
	0,						// bcIOr
	0,						// bcLOr
	0,						// bcIXor
	0,						// bcLXor
	0,						// bcIInc
	&builder_Conv_IL,		// bcI2L
	&builder_Conv_IF,		// bcI2F
	&builder_Conv_ID,		// bcI2D
	&builder_Conv_LI,		// bcL2I
	&builder_Conv_LF,		// bcL2F
	&builder_Conv_LD,		// bcL2D
	&builder_Conv_FI,		// bcF2I
	&builder_Conv_FL,		// bcF2L
	&builder_Conv_FD,		// bcF2D
	&builder_Conv_DI,		// bcD2I
	&builder_Conv_DL,		// bcD2L
	&builder_Conv_DF,		// bcD2F
	&builder_ExtB_II,		// bcInt2Byte
	&builder_ExtC_II,		// bcInt2Char
	&builder_ExtS_II		// bcInt2Short
};

static const BinaryBuilder *const binaryBuilders[] =
{
	&builder_AddCoal_III,	// bcIAdd
	&builder_AddCoal_LLL,	// bcLAdd
	&builder_Add_FFF,		// bcFAdd
	&builder_Add_DDD,		// bcDAdd
	&builder_SubCoal_III,	// bcISub
	&builder_SubCoal_LLL,	// bcLSub
	&builder_Sub_FFF,		// bcFSub
	&builder_Sub_DDD,		// bcDSub
	&builder_Mul_III,		// bcIMul
	&builder_Mul_LLL,		// bcLMul
	&builder_Mul_FFF,		// bcFMul
	&builder_Mul_DDD,		// bcDMul
	&builder_Div_III,		// bcIDiv
	&builder_Div_LLL,		// bcLDiv
	&builder_Div_FFF,		// bcFDiv
	&builder_Div_DDD,		// bcDDiv
	&builder_Mod_III,		// bcIRem
	&builder_Mod_LLL,		// bcLRem
	&builder_Rem_FFF,		// bcFRem
	&builder_Rem_DDD,		// bcDRem
	0,						// bcINeg
	0,						// bcLNeg
	0,						// bcFNeg
	0,						// bcDNeg
	&builder_Shl_III,		// bcIShl
	&builder_Shl_LIL,		// bcLShl
	&builder_Shr_III,		// bcIShr
	&builder_Shr_LIL,		// bcLShr
	&builder_UShr_III,		// bcIUShr
	&builder_UShr_LIL,		// bcLUShr
	&builder_And_III,		// bcIAnd
	&builder_And_LLL,		// bcLAnd
	&builder_Or_III,		// bcIOr
	&builder_Or_LLL,		// bcLOr
	&builder_Xor_III,		// bcIXor
	&builder_Xor_LLL,		// bcLXor
	0,						// bcIInc
	0,						// bcI2L
	0,						// bcI2F
	0,						// bcI2D
	0,						// bcL2I
	0,						// bcL2F
	0,						// bcL2D
	0,						// bcF2I
	0,						// bcF2L
	0,						// bcF2D
	0,						// bcD2I
	0,						// bcD2L
	0,						// bcD2F
	0,						// bcInt2Byte
	0,						// bcInt2Char
	0,						// bcInt2Short
	&builder_Cmp3_LLI,		// bcLCmp
	&builder_Cmp3L_FFI,		// bcFCmpL
	&builder_Cmp3G_FFI,		// bcFCmpG
	&builder_Cmp3L_DDI,		// bcDCmpL
	&builder_Cmp3G_DDI		// bcDCmpG
};

static const ComparisonBuilder<Int32> *const unaryComparisonBuilders[] =
{
	&builder_CmpEq_IIC,		// bcIfEq
	&builder_CmpEq_IIC,		// bcIfNe
	&builder_Cmp_IIC,		// bcIfLt
	&builder_Cmp_IIC,		// bcIfGe
	&builder_Cmp_IIC,		// bcIfGt
	&builder_Cmp_IIC		// bcIfLe
};

static const BinaryBuilder *const binaryComparisonBuilders[] =
{
	&builder_CmpEq_IIC,		// bcIf_ICmpEq
	&builder_CmpEq_IIC,		// bcIf_ICmpNe
	&builder_Cmp_IIC,		// bcIf_ICmpLt
	&builder_Cmp_IIC,		// bcIf_ICmpGe
	&builder_Cmp_IIC,		// bcIf_ICmpGt
	&builder_Cmp_IIC,		// bcIf_ICmpLe
	&builder_CmpUEq_AAC,	// bcIf_ACmpEq
	&builder_CmpUEq_AAC		// bcIf_ACmpNe
};


static const Condition2 bytecodeConditionals[] =
{
	condEq,		// bcIfEq
	condNe,		// bcIfNe
	condLt,		// bcIfLt
	condGe,		// bcIfGe
	condGt,		// bcIfGt
	condLe,		// bcIfLe
	condEq,		// bcIf_ICmpEq
	condNe,		// bcIf_ICmpNe
	condLt,		// bcIf_ICmpLt
	condGe,		// bcIf_ICmpGe
	condGt,		// bcIf_ICmpGt
	condLe,		// bcIf_ICmpLe
	condEq,		// bcIf_ACmpEq
	condNe,		// bcIf_ACmpNe
	cond0,		// bcGoto
	cond0,		// bcJsr
	cond0,		// bcRet
	cond0,		// bcTableSwitch
	cond0,		// bcLookupSwitch
	cond0,		// bcIReturn
	cond0,		// bcLReturn
	cond0,		// bcFReturn
	cond0,		// bcDReturn
	cond0,		// bcAReturn
	cond0,		// bcReturn
	cond0,		// bcGetStatic
	cond0,		// bcPutStatic
	cond0,		// bcGetField
	cond0,		// bcPutField
	cond0,		// bcInvokeVirtual
	cond0,		// bcInvokeSpecial
	cond0,		// bcInvokeStatic
	cond0,		// bcInvokeInterface
	cond0,		// bc_unused_
	cond0,		// bcNew
	cond0,		// bcNewArray
	cond0,		// bcANewArray
	cond0,		// bcArrayLength
	cond0,		// bcAThrow
	cond0,		// bcCheckCast
	cond0,		// bcInstanceOf
	cond0,		// bcMonitorEnter
	cond0,		// bcMonitorExit
	cond0,		// bcWide
	cond0,		// bcMultiANewArray
	condEq,		// bcIfNull
	condNe		// bcIfNonnull
};


const TypeKind arrayAccessTypeKinds[] =
{
	tkInt,		// bcIALoad, bcIAStore
	tkLong,		// bcLALoad, bcLAStore
	tkFloat,	// bcFALoad, bcFAStore
	tkDouble,	// bcDALoad, bcDAStore
	tkObject,	// bcAALoad, bcAAStore
	tkByte,		// bcBALoad, bcBAStore
	tkChar,		// bcCALoad, bcCAStore
	tkShort		// bcSALoad, bcSAStore
};


const LoadBuilder *const loadBuilders[] =
{
	0,						// tkVoid
	&builder_LdU_AB,		// tkBoolean
	&builder_LdU_AB,		// tkUByte
	&builder_LdS_AB,		// tkByte
	&builder_LdU_AH,		// tkChar
	&builder_LdS_AH,		// tkShort
	&builder_Ld_AI,			// tkInt
	&builder_Ld_AL,			// tkLong
	&builder_Ld_AF,			// tkFloat
	&builder_Ld_AD,			// tkDouble
	&builder_Ld_AA,			// tkObject
	&builder_Ld_AA,			// tkSpecial
	&builder_Ld_AA,			// tkArray
	&builder_Ld_AA			// tkInterface
};


const StoreBuilder *const storeBuilders[] =
{
	0,						// tkVoid
	&builder_St_AB,			// tkBoolean
	&builder_St_AB,			// tkUByte
	&builder_St_AB,			// tkByte
	&builder_St_AH,			// tkChar
	&builder_St_AH,			// tkShort
	&builder_St_AI,			// tkInt
	&builder_St_AL,			// tkLong
	&builder_St_AF,			// tkFloat
	&builder_St_AD,			// tkDouble
	&builder_St_AA,			// tkObject
	&builder_St_AA,			// tkSpecial
	&builder_St_AA,			// tkArray
	&builder_St_AA			// tkInterface
};


static const SysCall *const newArrayCalls[natLimit - natMin] =
{
	&scNewBooleanArray,	// natBoolean
	&scNewCharArray,	// natChar
	&scNewFloatArray,	// natFloat
	&scNewDoubleArray,	// natDouble
	&scNewByteArray,	// natByte
	&scNewShortArray,	// natShort
	&scNewIntArray,		// natInt
	&scNewLongArray		// natLong
};


static const SysCall *const typeToArrayCalls[nPrimitiveTypeKinds] =
{
	NULL,               // tkVoid
	&scNewBooleanArray,	// tkBoolean
	&scNewByteArray,	// tkUByte
	&scNewByteArray,	// tkByte
	&scNewCharArray,	// tkChar
	&scNewShortArray,	// tkShort
	&scNewIntArray,		// tkInt
	&scNewLongArray,	// tkLong
	&scNewFloatArray,	// tkFloat
	&scNewDoubleArray	// tkDouble
};


// ----------------------------------------------------------------------------
// BytecodeTranslator::BlockTranslator


//
// Create a new, empty control node to hold the rest of this BytecodeBlock and set cn
// to be that control node.  Make edge e be the one predecessor (so far) of the
// new control node.
//
void BytecodeTranslator::BlockTranslator::appendControlNode(ControlEdge &e)
{
	ControlNode &nextControlNode = getControlGraph().newControlNode();
	nextControlNode.addPredecessor(e);
	cn = &nextControlNode;
}


//
// Designate this ControlNode to be an Exc, Throw, or AExc node, depending on the given
// ControlKind.  This node will have zero (Throw) or one (Exc or AExc) normal successors
// (whose targets are not initialized -- the caller will have to do that) followed by as
// many exceptional successors as there are possible handlers for an exception of class
// exceptionClass thrown from within this BytecodeBlock.  The exceptional successor edges are
// fully initialized, as is the ExceptionExtra node's handlerFilters array.
//
// If the ControlNode will be an Exc or Throw node, tryPrimitive must be a primitive
// located inside this node that can throw an exception; if the ControlNode will be an
// AExc node, tryPrimitive must be nil.
//
// translationEnv's stack is ignored.
// This routine assumes ownership of translationEnv if canOwnEnv is true (thereby sometimes eliminating
// an unnecessary copy).
//
void BytecodeTranslator::BlockTranslator::genException(ControlKind ck, Primitive *tryPrimitive, const Class &exceptionClass,
													   bool canOwnEnv) const
{
	assert(cn && hasExceptionOutgoingEdges(ck));
	Uint32 nNormalSuccessors = ck != ckThrow;
	Uint32 maxNHandlers = block.handlersEnd - block.successorsEnd + 1;
	const Class **filters = new(primitivePool) const Class *[maxNHandlers];
	ControlEdge *successors = new(primitivePool) ControlEdge[nNormalSuccessors + maxNHandlers];

	// Copy the list of handlers from the BytecodeBlock to the ExceptionExtra.
	// Remove any handlers that can't apply.
	const Class **srcFilter = block.exceptionClasses;
	const Class **dstFilter = filters;
	BasicBlock **srcSuccessor = block.successorsEnd;
	ControlEdge *dstSuccessor = successors + nNormalSuccessors;
	BasicBlock *successor;
	while (--maxNHandlers) {
		const Class *filter = *srcFilter++;
		successor = *srcSuccessor++;

		assert(filter->implements(Standard::get(cThrowable)));
		if (filter == &Standard::get(cThrowable))
			goto haveCatchAll;
		if (exceptionClass.implements(*filter) || filter->implements(exceptionClass)) {
			*dstFilter++ = filter;
			bt.linkPredecessor(*successor, *dstSuccessor++, getEnv(), false);
		}
	}
	// We haven't reached a catch-all handler, so direct remaining exceptions to
	// the End block, which indicates that they are propagated out of this function.
	successor = bt.bytecodeGraph.endBlock;

  haveCatchAll:
	// Fill in the catch-all handler.
	*dstFilter++ = &Standard::get(cThrowable);
	bt.linkPredecessor(*successor, *dstSuccessor, getEnv(), canOwnEnv);

	cn->setControlException(ck, dstFilter - filters, filters, tryPrimitive, successors);
}


//
// Generate an unconditional throw of the given exception, which is guaranteed not to
// have a null value.  The exception's class must be a subclass of exceptionClass, which
// must be a subclass of Throwable; exceptionClass is not checked for integrity.
//
// Set cn to nil to indicate that no more control nodes should be generated.
//
// This routine assumes ownership of translationEnv if canOwnEnv is true (thereby sometimes eliminating
// an unnecessary copy).
//
void BytecodeTranslator::BlockTranslator::genThrowCommon(const VariableOrConstant &exception, const Class &exceptionClass,
														 bool canOwnEnv)
{
	// asharma - fix me
	Primitive &prim = makeSysCall(scThrow, 0, 1, &exception, 0, 0, *cn, 0);
	genException(ckThrow, &prim, exceptionClass, canOwnEnv);
	cn = 0;
}


//
// Generate an unconditional throw of the given exception, which is a system-defined
// instance of Throwable or one of its subclasses; exceptionObject is not checked for
// integrity.  Set cn to nil to indicate that no more control nodes should be generated.
//
// This routine assumes ownership of translationEnv if canOwnEnv is true (thereby sometimes eliminating
// an unnecessary copy).
//
void BytecodeTranslator::BlockTranslator::genSimpleThrow(cobj exceptionObject, bool canOwnEnv)
{
	VariableOrConstant exception;
	exception.setConstant(objectAddress(exceptionObject));
	genThrowCommon(exception, exceptionObject->getClass(), canOwnEnv);
}


//
// Generate an exc node that indicates that prim might throw an exception
// of the given class or one of its subclasses; exceptionClass must be Throwable
// or one of its subclasses and is not checked for integrity.
// prim is the primitive inside cn that can throw exceptions.  There must be no
// other primitives in cn that rely on prim's outgoing data edge, if any.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock;
// that control node is always empty.
//
// This routine does not assume ownership of translationEnv.
//
void BytecodeTranslator::BlockTranslator::genPossibleThrow(Primitive &prim, const Class &exceptionClass)
{
	genException(ckExc, &prim, exceptionClass, false);
	appendControlNode(cn->getNormalSuccessor());
}


//
// Make cn into a basic block control node.  Allocate its ControlExtra record
// and link it to the successor BasicBlock.
// Set cn to nil to indicate that no more control nodes should be generated.
//
// If the basic block would be empty and have only one predecessor, eliminate it
// and instead link the successor to the predecessor.
//
void BytecodeTranslator::BlockTranslator::createBasicBlock(BasicBlock &successor)
{
	ControlEdge *edge;

	if (cn->empty() && cn->getPredecessors().lengthIs(1)) {
		// Eliminate this basic block and recycle its ControlNode.
		edge = &cn->getPredecessors().first();
		assert(block.getGeneratedControlNodes());
		if (block.getFirstControlNode() == cn)
			block.getFirstControlNode() = 0;
		edge->clearTarget();	// Clear cn's predecessor linked list.
		getControlGraph().recycle(*cn);
	} else {
		cn->setControlBlock();
		edge = &cn->getNormalSuccessor();
	}
	bt.linkPredecessor(successor, *edge, *translationEnv, true);
	cn = 0;
}


//
// Make cn into an IF control node.  Allocate its ControlExtra record and
// link it to the successor BasicBlocks.  c contains the condition variable.
// If c satisfies cond2 (or reverse(cond2) if reverseCond2 is true), the IF
// will branch to the true successor; otherwise, it will branch to the false
// successor.  On exit set cn to nil.
//
void BytecodeTranslator::BlockTranslator::createIf(Condition2 cond2, 
												   bool reverseCond2,
												   VariableOrConstant &c,
												   BasicBlock &successorFalse,
												   BasicBlock &successorTrue,
												   Uint32 bci)
{
	if (reverseCond2)
		cond2 = reverse(cond2);

	bool constResult;
	DataNode *cp = partialComputeComparison(cond2, c, constResult);
	if (cp) {
		// The value of the selector is not known at compile time.
		cn->setControlIf(cond2, *cp, bci);
		bt.linkPredecessor(successorTrue, cn->getTrueSuccessor(), *translationEnv, false);
		bt.linkPredecessor(successorFalse, cn->getFalseSuccessor(), *translationEnv, true);
		cn = 0;
	} else
		// The comparison result is known at compile time.
		createBasicBlock(constResult ? successorTrue : successorFalse);
}


//
// Same as createIf except that if the selector is false, continue processing this
// BytecodeBlock using the new control node in cn.  If the selector is true, go to
// successorTrue as in createIf.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the selector is known to always be true.
//
void 
BytecodeTranslator::BlockTranslator::createContinuedIf(Condition2 cond2, 
													   bool reverseCond2, 
													   VariableOrConstant &c,
													   BasicBlock &successorTrue, 
													   Uint32 bci)   
{
	if (reverseCond2)
		cond2 = reverse(cond2);

	bool constResult;
	DataNode *cp = partialComputeComparison(cond2, c, constResult);
	if (cp) {
		// The value of the selector is not known at compile time.
		cn->setControlIf(cond2, *cp, bci);
		bt.linkPredecessor(successorTrue, cn->getTrueSuccessor(), *translationEnv, false);
		appendControlNode(cn->getFalseSuccessor());
	} else if (constResult)
		// The condition is always true.
		createBasicBlock(successorTrue);
}


//
// Create control and data flow nodes to make sure that arg (which must have
// pointer kind) is non-null.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genNullGuard(const VariableOrConstant &arg, Uint32 bci)
{
	Primitive *prim;

	switch (makeChkNull(arg, prim, cn, bci)) {
		case chkAlwaysOK:
			break;
		case chkNeverOK:
			genSimpleThrow(&Standard::get(oNullPointerException), true);
			break;
		default:
			genPossibleThrow(*prim, Standard::get(cNullPointerException));
	}
}


//
// Create control and data flow nodes for a tableswitch bytecode.
// bc points immediately after the tableswitch opcode.
//
// Set cn to nil to indicate that no more control nodes should be generated.
//
void BytecodeTranslator::BlockTranslator::genTableSwitch(const bytecode *bc,
														 Uint32 bci)
{
	VariableOrConstant arg1;
	VariableOrConstant arg2;
	VariableOrConstant arg3;
	TranslationEnv &env = *translationEnv;

	env.pop().extract(arg1, bci);
	bc = BytecodeGraph::switchAlign(bc) + 4;	// Skip past padding and default target.
	Int32 low = readBigSWord(bc);
	Int32 high = readBigSWord(bc + 4);
	bc += 8;
	assert(low <= high);
	Uint32 nCases = high - low + 1;
	bc += nCases * 4;
	assert(bc == block.bytecodesEnd);
	assert(block.nSuccessors() == nCases + 1);

	// Do the range check; jump to the default case if out of range.
	builder_AddCoal_III.specializeArgConst(arg1, -low, arg2, cn);
	Primitive *prim = builder_CmpU_IIC.specializeArgConst(arg2, (Int32)nCases, arg3, cn);
	createContinuedIf(condGe, prim != 0, arg3, block.getSuccessor(0), bci);

	BasicBlock **successorsEnd = block.successorsEnd;
	BasicBlock **nextSuccessor = block.successorsBegin + 1;
	if (cn)
		if (arg2.isConstant())
			// The switch argument is a constant, so go directly to the corresponding target.
			createBasicBlock(block.getSuccessor(1 + arg2.getConstant().i - low));
		else {
			// The value of the switch argument is not known at compile time.
			cn->setControlSwitch(arg2.getVariable(), nCases, bci);
			ControlEdge *ce = cn->getSwitchSuccessors();
			while (nextSuccessor != successorsEnd)
				bt.linkPredecessor(**nextSuccessor++, *ce++, env, false);
			cn = 0;
		}
}


//
// Create control and data flow nodes for a lookupswitch bytecode.
// bc points immediately after the lookupswitch opcode.
//
// Set cn to nil to indicate that no more control nodes should be generated.
//
void BytecodeTranslator::BlockTranslator::genLookupSwitch(const bytecode *bc,
														  Uint32 bci)
{
	VariableOrConstant arg;

	translationEnv->pop().extract(arg, bci);
	bc = BytecodeGraph::switchAlign(bc) + 4;	// Skip past padding and default target.
	Uint32 nPairs = readBigUWord(bc);
	bc += 4;
	assert(block.nSuccessors() == nPairs + 1);
	if (nPairs == 0)
		createBasicBlock(block.getSuccessor(0));
	else {
		BasicBlock **nextSuccessor = block.successorsBegin + 1;
		while (true) {
			VariableOrConstant result;
			Int32 match = readBigSWord(bc);
			bc += 8;
			Primitive *prim = builder_CmpEq_IIC.specializeArgConst(arg, match, result, cn);
			nPairs--;
			if (nPairs) {
				createContinuedIf(condEq, prim != 0, result,
								  **nextSuccessor++, bci);
				if (!cn) {
					// We're done early.
					bc += nPairs * 8;
					break;
				}
			} else {
				createIf(condEq, prim != 0, result, block.getSuccessor(0),
						 **nextSuccessor, bci);
				break;
			}
		}
	}
	assert(bc == block.bytecodesEnd);
}


//
// Create data flow nodes to read the type of an object.  The object pointer should
// already have been checked to make sure it is not null.
//
inline void 
BytecodeTranslator::BlockTranslator::genReadObjectType(const VariableOrConstant &object, VariableOrConstant &type, Uint32 bci) const {
	assert(objectTypeOffset == 0);
	builder_Ld_AA.makeLoad(0, object, type, false, true, cn, bci);
}


//
// Create data flow nodes for a getstatic bytecode.
// cpi is the constant table index of the static variable entry.
//
void BytecodeTranslator::BlockTranslator::genGetStatic(ConstantPoolIndex cpi, Uint32 bci) const
{
	ValueKind vk;
	addr address;
	bool isVolatile;
	bool isConstant;
	TypeKind tk = classFileSummary.lookupStaticField(cpi, vk, address, isVolatile, isConstant);
	TranslationEnv &env = *translationEnv;

	DataNode *memory = &env.memory().extract(bci);
	VariableOrConstant result;
	loadBuilders[tk]->makeLoad(&memory, address, result, isVolatile,
							   isConstant, cn, bci);
	env.memory().define(*memory);
	env.push1or2(isDoublewordKind(vk)).define(result, cn, envPool);
}


//
// Create data flow nodes for a putstatic bytecode.
// cpi is the constant table index of the static variable entry.
//
void BytecodeTranslator::BlockTranslator::genPutStatic(ConstantPoolIndex cpi,
													   Uint32 bci) const
{
	ValueKind vk;
	addr address;
	bool isVolatile;
	bool isConstant;
	TypeKind tk = classFileSummary.lookupStaticField(cpi, vk, address, isVolatile, isConstant);
	TranslationEnv &env = *translationEnv;

	if (isConstant)
		verifyError(VerifyError::writeToConst);
	VariableOrConstant a;
	a.setConstant(address);
	VariableOrConstant value;
	env.pop1or2(isDoublewordKind(vk)).extract(value, bci);
	Primitive &memoryOut = storeBuilders[tk]->makeStore(env.memory().extract(bci), a, value,
														isVolatile, cn, bci);
	env.memory().define(memoryOut);
}


//
// Create control and data flow nodes for a getfield bytecode.
// cpi is the constant table index of the field entry.
//
// Set cn to a control node to use for the next statement of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genGetField(ConstantPoolIndex cpi,
													  Uint32 bci)
{
	ValueKind vk;
	Uint32 offset;
	bool isVolatile;
	bool isConstant;
	TypeKind tk = classFileSummary.lookupInstanceField(cpi, vk, offset, isVolatile, isConstant);
	TranslationEnv &env = *translationEnv;

	VariableOrConstant objectAddr;
	env.pop().extract(objectAddr, bci);
	genNullGuard(objectAddr, bci);
	if (cn) {
		// Get the address of the field.
		VariableOrConstant fieldAddr;
		builder_Add_AIA.specializeArgConst(objectAddr, (Int32)offset, fieldAddr, cn);

		// Emit the load instruction.
		DataNode *memory = &env.memory().extract(bci);
		VariableOrConstant result;
		loadBuilders[tk]->makeLoad(&memory, fieldAddr, result, isVolatile, isConstant, cn, bci);
		env.memory().define(*memory);
		env.push1or2(isDoublewordKind(vk)).define(result, cn, envPool);
	}
}


//
// Create control and data flow nodes for a putfield bytecode.
// cpi is the constant table index of the field entry.
//
// Set cn to a control node to use for the next statement of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genPutField(ConstantPoolIndex cpi,
													  Uint32 bci)
{
	ValueKind vk;
	Uint32 offset;
	bool isVolatile;
	bool isConstant;
	TypeKind tk = classFileSummary.lookupInstanceField(cpi, vk, offset, isVolatile, isConstant);
	TranslationEnv &env = *translationEnv;

	if (isConstant)
		verifyError(VerifyError::writeToConst);
	VariableOrConstant value;
	VariableOrConstant objectAddr;
	env.pop1or2(isDoublewordKind(vk)).extract(value, bci);
	env.pop().extract(objectAddr, bci);
	genNullGuard(objectAddr, bci);
	if (cn) {
		// Get the address of the field.
		VariableOrConstant fieldAddr;
		builder_Add_AIA.specializeArgConst(objectAddr, (Int32)offset, fieldAddr, cn);

		// Emit the store instruction.
		Primitive &memoryOut =
		storeBuilders[tk]->makeStore(env.memory().extract(bci), fieldAddr,
									 value, isVolatile, cn, bci);
		env.memory().define(memoryOut);
	}
}


//
// Create control and data flow nodes to read the length of an array.
// Make sure that arrayAddr isn't nil and return the array length in arrayLength.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genReadArrayLength(const VariableOrConstant &arrayAddr, VariableOrConstant &arrayLength, Uint32 bci)
{
	genNullGuard(arrayAddr, bci);
	if (cn) {
		// Get the address of the field.
		VariableOrConstant fieldAddr;
		builder_Add_AIA.specializeArgConst(arrayAddr, arrayLengthOffset, fieldAddr, cn);

		// Emit the load instruction.
		builder_Ld_AI.makeLoad(0, fieldAddr, arrayLength, false, true, cn, bci);
	}
}


//
// Create control and data flow nodes to make sure that the object can be
// written into the object array, which should already have been checked to
// make sure it is not null.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genArrayCastGuard(const VariableOrConstant &array, const VariableOrConstant &object)
{
	assert(object.hasKind(vkAddr) && array.hasKind(vkAddr));
	// We can write nil into any object array so we don't bother issuing a check
	// if we know that we're writing a nil.
	if (!object.isConstant() || !object.getConstant().a) {
		// Do we know the actual types of both the object and the array?
		Type *objectType = object.getDynamicType();
		if (objectType) {
			Type *arrayType = array.getDynamicType();
			if (arrayType) {
				// Yes.  Perform the check statically.
				assert(arrayType->typeKind == tkArray);
				if (!implements(*objectType, static_cast<Array *>(arrayType)->componentType))
					genSimpleThrow(&Standard::get(oArrayStoreException), true);
				return;
			}
		}

		VariableOrConstant args[2];
		args[0] = array;
		args[1] = object;
		// asharma - fix me
		Primitive &prim = makeSysCall(scCheckArrayStore, 0, 2, args, 0, 0, *cn, 0);
		genPossibleThrow(prim, Standard::get(cArrayStoreException));
	}
}


//
// Create control and data flow nodes for an array read or write bytecode.
// If write is true, the access is a write; otherwise it's a read.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genArrayEltAccess(TypeKind tk, bool
															write, Uint32 bci)
{
	VariableOrConstant value;
	VariableOrConstant index;
	VariableOrConstant length;	// Array length
	VariableOrConstant arrayAddrAndOffset[2];	// [0] is array address; [1] is index times element size
	TranslationEnv &env = *translationEnv;

	if (write)
		env.pop1or2(isDoublewordKind(tk)).extract(value, bci);
	env.pop().extract(index, bci);
	env.pop().extract(arrayAddrAndOffset[0], bci);
	genReadArrayLength(arrayAddrAndOffset[0], length, bci);

	if (cn) {
		// Check bounds
		Primitive *prim;
		CheckResult chkRes = makeLimit(index, length, prim, cn);
		if (chkRes == chkNeverOK)
			genSimpleThrow(&Standard::get(oArrayIndexOutOfBoundsException), true);
		else {
			if (chkRes != chkAlwaysOK)
				genPossibleThrow(*prim, Standard::get(cArrayIndexOutOfBoundsException));

			// Check cast if needed
			assert(isPrimitiveKind(tk) || tk == tkObject);
			if (write && tk == tkObject)
				genArrayCastGuard(arrayAddrAndOffset[0], value);

			if (cn) {
				// Get the address of the element.
				VariableOrConstant elementAddr;
				builder_Shl_III.specializeArgConst(index, getLgTypeKindSize(tk), arrayAddrAndOffset[1], cn);
				builder_Add_AIA.makeBinaryGeneric(arrayAddrAndOffset, elementAddr, cn);
				builder_AddCoal_AIA.specializeArgConst(elementAddr, arrayEltsOffset(tk), elementAddr, cn);

				DataNode *memory = &env.memory().extract(bci);
				if (write)
					// Emit the store instruction.
					env.memory().define(storeBuilders[tk]->makeStore(*memory, elementAddr, value, false, cn, bci));
				else {
					// Emit the load instruction.
					loadBuilders[tk]->makeLoad(&memory, elementAddr, value, false, false, cn, bci);
					env.push1or2(isDoublewordKind(tk)).define(value, cn, envPool);
				}
			}
		}
	}
}


//
// Create data flow nodes to check if a reference with class pointed
// at by the type argument is assignable to another reference of type
// targetInterface, which is either an interface type or the type of
// an array of interfaces.  type should be a variable, not a constant.
//
// results should be an array of two VariableOrConstants.  On exit
// results[0] and results[1] will be initialized to two integer variables
// that, if equal, indicate that the interface check succeeds and,
// if not equal, indicate that the interface check fails.
// 
void BytecodeTranslator::BlockTranslator::genCheckInterfaceAssignability(VariableOrConstant &type, const Type &targetInterface,
																		 VariableOrConstant *results, Uint32 bci) const
{
	assert(cn && type.isVariable());

    // Get the address of the class-specific interface assignability table used
    // to determine whether an instance of that class can be assigned to a
    // given interface (or interface array) type.
    VariableOrConstant vPointerToAssignableTableAddr;
    builder_Add_AIA.specializeArgConst(type, offsetof(Type, interfaceAssignableTable), vPointerToAssignableTableAddr, cn);
    VariableOrConstant vAssignableTableAddr;
    builder_Ld_AA.makeLoad(0, vPointerToAssignableTableAddr, vAssignableTableAddr, false, true, cn, bci);
    
    // Compute the address of the interface assignability table entry
    // corresponding to the given interface.
    Uint32 interfaceNumber = targetInterface.getInterfaceNumber();
    VariableOrConstant vAssignableTableEntryAddr;
    builder_Add_AIA.specializeArgConst(vAssignableTableAddr, interfaceNumber * sizeof(Uint16), vAssignableTableEntryAddr, cn);
    
    // Load the selected table entry, which will contain a value that we must match against
    VariableOrConstant &vAssignableValue = results[0];
    VariableOrConstant &vMatchValue = results[1];
    builder_LdU_AH.makeLoad(0, vAssignableTableEntryAddr, vAssignableValue, false, true, cn, bci);
    
    // Load the class-specific match value for comparison with the value found in the table
    VariableOrConstant vAddrOfAssignableMatchValue;
	builder_Add_AIA.specializeVarConst(type.getVariable(), offsetof(Type, assignableMatchValue), vAddrOfAssignableMatchValue, cn);
    builder_LdU_AH.makeLoad(0, vAddrOfAssignableMatchValue, vMatchValue, false, true, cn, bci);
}


//
// Create control and data flow nodes for a checkcast bytecode that checks
// that object arg is a member of type t.  Type t is guaranteed not to be Object,
// and arg is guaranteed to be nonnull.
//
// Set cn to a new, empty node to use for the next portion of this BytecodeBlock or
// to nil if an exception is always thrown.
//
void BytecodeTranslator::BlockTranslator::genCheckCastNonNull(const Type &t, const VariableOrConstant &arg, Uint32 bci)
{
	assert(!isPrimitiveKind(t.typeKind));

	VariableOrConstant type;
	genReadObjectType(arg, type, bci);

	if (type.isConstant()) {
		// We determined the type of arg at compile time.  Do nothing if it a subtype of t;
		// otherwise, always throw ClassCastException.
		if (!implements(addressAsType(type.getConstant().a), t))
			genSimpleThrow(&Standard::get(oClassCastException), true);
	} else {
		DataNode &dp = type.getVariable();
		Primitive *prim;

        if (t.final)
			// We're testing against a final type.  Emit a simple equality test
			// that ensures that arg is the same as the given final type.
			prim = makeChkCastA(dp, t, cn);
        else if ((t.typeKind == tkObject) ||
                 ((t.typeKind == tkArray) && (asArray(t).getElementType().typeKind != tkInterface))) {

			// We're testing against an object or an array type that is not an array of
			// interfaces.  Emit a vtable-based test that passes the given type and its subtypes.
			// The given type and each of its subtypes will have a pointer to type t
			// in the ventry at index nVTableSlots-1.

			// Make sure that we have a sufficient number of vEntries.
			VariableOrConstant nVEntriesAddr;
			builder_Add_AIA.specializeVarConst(dp, offsetof(Type, nVTableSlots), nVEntriesAddr, cn);
			VariableOrConstant nVEntries;
			builder_Ld_AI.makeLoad(0, nVEntriesAddr, nVEntries, false, true, cn, bci);
			Primitive *limPrim;
			CheckResult chkRes = makeLimCast(nVEntries, t.nVTableSlots, limPrim, cn);
			if (chkRes == chkNeverOK) {
				genSimpleThrow(&Standard::get(oClassCastException), true);
				return;
			}
			if (chkRes != chkAlwaysOK)
				genPossibleThrow(*limPrim, Standard::get(cClassCastException));

			// Get the value of the vEntry at index nVTableSlots-1.
			VariableOrConstant vEntryAddr;
			builder_Add_AIA.specializeVarConst(dp, vTableIndexToOffset(t.nVTableSlots - 1), vEntryAddr, cn);
			VariableOrConstant vEntryValue;
			builder_Ld_AA.makeLoad(0, vEntryAddr, vEntryValue, false, true, cn, bci);

			// Emit a test to make sure that the vEntry value is the type we desire.
			prim = makeChkCastA(vEntryValue.getVariable(), t, cn);
		} else {
			// We're testing against an interface or an array whose element type is an interface.
			assert((t.typeKind == tkInterface) || ((t.typeKind == tkArray) && (asArray(t).getElementType().typeKind == tkInterface)));

			// Generate a runtime assignability check
			VariableOrConstant cmpArgs[2];
			genCheckInterfaceAssignability(type, t, cmpArgs, bci);
			prim = makeChkCastI(cmpArgs[0].getVariable(), cmpArgs[1].getVariable(), cn);
 		}
	    genPossibleThrow(*prim, Standard::get(cClassCastException));
	}
}


//
// Create control and data flow nodes for a checkcast bytecode that checks
// that object arg satisfies the type represented by cpi.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the checkcast always throws an exception.
//
void BytecodeTranslator::BlockTranslator::genCheckCast(ConstantPoolIndex cpi, const VariableOrConstant &arg, Uint32 bci)
{
	const Type &t = classFileSummary.lookupType(cpi);

	// A cast to the type Object always succeeds.
	if (&t != &Standard::get(cObject)) {
		// Try to statically check whether the object is null.
		bool constResult;
		VariableOrConstant cond;
		builder_CmpUEq_AAC.specializeArgConst(arg, nullAddr, cond, cn);
		DataNode *cp = partialComputeComparison(condEq, cond, constResult);
		if (cp) {
			// We have to check whether the object is null at runtime.
			ControlNode *cnIf = cn;
			cnIf->setControlIf(condEq, *cp, bci);

			// Create a new ControlNode to hold the check code.
			appendControlNode(cnIf->getFalseSuccessor());
			genCheckCastNonNull(t, arg, bci);
			if (!cn)
				cn = &getControlGraph().newControlNode();

			// Attach the true arm of the if to the latest control node, which should
			// be empty at this point.
			assert(cn->empty());
			cn->addPredecessor(cnIf->getTrueSuccessor());
		} else
			// The object is either always or never null.  If it's always null, don't
			// bother emitting any code.
			if (!constResult)
				// The object is never null.
				genCheckCastNonNull(t, arg, bci);
	}
}


//
// Create control and data flow nodes for an instanceof bytecode that checks
// whether object arg is a member of type t and returns the int result (1 if arg is
// a member of type t, 0 if not) in result.  Type t is guaranteed not to be Object,
// and arg is guaranteed to be nonnull.
// cnFalse is either nil or a pointer to a control node that will store false in the
// result.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock.
// The outgoing cn will never be nil.
//
void BytecodeTranslator::BlockTranslator::genInstanceOfNonNull(const Type &t, const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *cnFalse, Uint32 bci)
{
	assert(!isPrimitiveKind(t.typeKind));

	VariableOrConstant type;
	genReadObjectType(arg, type, bci);

	if (type.isConstant())
		// We determined the type of arg at compile time.
		// Return the constant 0 or 1, as appropriate.
		result.setConstant((Int32)implements(addressAsType(type.getConstant().a), t));

	else {
		DataNode &dp = type.getVariable();
		VariableOrConstant args[2];
		args[0].setVariable(dp);
		args[1].setConstant(objectAddress(&t));

		if (t.final)
			// We're testing against a final type.  Emit a simple equality test
			// that checks whether arg is the same as the given final type.
			builder_Eq_AAI.makeBinaryGeneric(args, result, cn);

		else if ((t.typeKind == tkObject) ||
                 ((t.typeKind == tkArray) && (asArray(t).getElementType().typeKind != tkInterface))) {

            // We're testing against an object or an array type where the elements
            // of the array are not interfaces.
			// Emit a vtable-based test that passes the given type and its subtypes.
			// The given type and each of its subtypes will have a pointer to type t
			// in the ventry at index nVTableSlots-1.

			// Make sure that we have a sufficient number of vEntries.
			VariableOrConstant nVEntriesAddr;
			builder_Add_AIA.specializeVarConst(dp, offsetof(Type, nVTableSlots), nVEntriesAddr, cn);
			VariableOrConstant nVEntries;
			builder_Ld_AI.makeLoad(0, nVEntriesAddr, nVEntries, false, true, cn, bci);

			// Emit the comparison and if node.
			VariableOrConstant argCmp;
			Primitive *reverseCmp = builder_Cmp_IIC.specializeArgConst(nVEntries, t.nVTableSlots, argCmp, cn);
			Condition2 cond2 = reverseCmp ? condGt : condLt;
			bool constResult;
			DataNode *cp = partialComputeComparison(cond2, argCmp, constResult);
			bool needJoin = false;
			if (!cp) {
                // The result of the if is known at compile time.
                //   (This should not be possible, because we've already
                //   tested for a type known at compile-time, above, but
                //   this code might be purposeful in the future.)
				if (constResult) {
					// The if is always true, so the result is always false.
					result.setConstant((Int32)0);
					return;
				}
				// The if is always false, so don't emit the if.
			} else {
				ControlNode *cnIf = cn;
				cnIf->setControlIf(cond2, *cp, bci);

				// Create a new control node for the false branch of the if and store it in cn.
				appendControlNode(cnIf->getFalseSuccessor());
				if (!cnFalse) {
					cnFalse = &getControlGraph().newControlNode();
					needJoin = true;
				}
				cnFalse->addPredecessor(cnIf->getTrueSuccessor());
			}

			// Get the value of the vEntry at index nVTableSlots-1.
			VariableOrConstant vEntryAddr;
			builder_Add_AIA.specializeVarConst(dp, vTableIndexToOffset(t.nVTableSlots - 1), vEntryAddr, cn);
			builder_Ld_AA.makeLoad(0, vEntryAddr, args[0], false, true, cn, bci);

			// Emit a test to make sure that the vEntry value is the type we desire.
			builder_Eq_AAI.makeBinaryGeneric(args, result, cn);

			// If we created our own control node for the true branch of the if, we must now join it
			// with the control flow leaving the false branch.
			if (needJoin) {
				ControlNode *predecessors[2];
				args[0] = result;
				args[1].setConstant((Int32)0);
				predecessors[0] = cn;
				predecessors[1] = cnFalse;
				cn = &joinControlFlows(2, predecessors, getControlGraph());
				joinDataFlows(2, *cn, args, result);
			}

        } else {
			// We're testing against an interface or an array whose element type is an interface.
			assert((t.typeKind == tkInterface) || ((t.typeKind == tkArray) && (asArray(t).getElementType().typeKind == tkInterface)));

			VariableOrConstant cmpArgs[2];
			genCheckInterfaceAssignability(type, asInterface(t), cmpArgs, bci);
        	builder_Eq_III.makeBinaryGeneric(cmpArgs, result, cn);
        }
	}
}


//
// Create control and data flow nodes for an instanceof bytecode that checks
// whether object arg is a member of the type represented by cpi.  However,
// unlike checkcast, nil is not considered to be a member of any type.
// The result VariableOrConstant gets an int result that is 1 if arg is a member
// of the type or 0 if not.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock.
// The outgoing cn will never be nil.
//
void BytecodeTranslator::BlockTranslator::genInstanceOf(ConstantPoolIndex cpi, const VariableOrConstant &arg, VariableOrConstant &result, Uint32 bci)
{
	const Type &t = classFileSummary.lookupType(cpi);

	if (&t == &Standard::get(cObject))
		// "x instanceof Object" succeeds if and only if x is non-nil.
		builder_Ne0_AI.makeUnaryGeneric(arg, result, cn);
	else {
		// Try to statically check whether the object is null.
		bool constResult;
		VariableOrConstant cond;
		builder_CmpUEq_AAC.specializeArgConst(arg, nullAddr, cond, cn);
		DataNode *cp = partialComputeComparison(condEq, cond, constResult);
		if (cp) {
			// We have to check whether the object is null at runtime.
			ControlNode *cnIf = cn;
			cnIf->setControlIf(condEq, *cp, bci);

			// Create a new ControlNode to hold the instanceof code for the null case.
			VariableOrConstant args[2];
			ControlNode *predecessors[2];
			appendControlNode(cnIf->getTrueSuccessor());
			args[1].setConstant((Int32)0);
			predecessors[1] = cn;

			// Create a new ControlNode to hold the instanceof code for the non-null case.
			appendControlNode(cnIf->getFalseSuccessor());
			genInstanceOfNonNull(t, arg, args[0], predecessors[1], bci);
			predecessors[0] = cn;

			// Merge the outputs of the above two ControlNodes.
			cn = &joinControlFlows(2, predecessors, getControlGraph());
			joinDataFlows(2, *cn, args, result);
		} else
			// The object is either always or never null.
			if (constResult)
				// The object is always null.
				result.setConstant((Int32)0);
			else
				// The object is never null.
				genInstanceOfNonNull(t, arg, result, 0, bci);
	}
}


//
// Create control and data flow nodes for a system call that receives
// a memory argument together with the given arguments and outputs a new
// memory state together with one one-word result and can throw any exception.
// Push the result onto the environment.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock.
//
void BytecodeTranslator::BlockTranslator::genNewCommon(const SysCall &sysCall, uint nArgs, const VariableOrConstant *args, Uint32 bci)
{
	VariableOrConstant result;
	TranslationEnv &env = *translationEnv;
	DataNode *memory = &env.memory().extract(bci);
	Primitive &prim = makeSysCall(sysCall, &memory, nArgs, args, 1, &result, *cn, bci);
	env.memory().define(*memory);
	env.push().define(result.getVariable()); // OK because result cannot be a constant here.
	genPossibleThrow(prim, Standard::get(sysCall.exceptionClass));
}


//
// Create code to allocate a temporary array for storing all the dimensions
// of a multi-dimensional array, which is pushed onto the environment.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genTempArrayOfDim(Int32 dim, Uint32 bci)
{
	VariableOrConstant args[2], arg, *dimargs;
	Int32 i;
	TranslationEnv &env = *translationEnv;

	// Pull all the dimensions out of the environment.
	dimargs = new(primitivePool) VariableOrConstant[dim];
	for (i=dim-1; i>=0; i--) {
		env.pop().extract(dimargs[i], bci);
		// check for negative dimension here
		assert(dimargs[i].hasKind(vkInt));
		if (dimargs[i].isConstant() && dimargs[i].getConstant().i < 0) {
			genSimpleThrow(&Standard::get(oNegativeArraySizeException), true);
			return;
		}
	}
	args[0].setConstant(dim);  // the number of ints to allocate
	genNewCommon(*typeToArrayCalls[tkInt], 1, args, bci);
	env.pop().extract(args[0], bci); // array now stored in args[0]
	// Now loop through dimensions and generate code to fill the array.
	// We need to do the following:
	// for each argument (picked from the environment in reverse order), 
	// store it in the array using the code for iastore 
	// (genArrayEltAccess(tkInt, 1)).
	// We proceed by pushing the array, followed by i, and dimargs[i].
	// Then, generate a call to genArrayEltAccess().
	// Return the resulting control node.
	for (i=0; i<dim; i++) {
		env.push().define(args[0], cn, envPool); // arr. WASTE!
		arg.setConstant(i);
		env.push().define(arg, cn, envPool); // i.
		env.push().define(dimargs[i], cn, envPool); // ith dimension
		genArrayEltAccess(tkInt, 1, bci);
	}
	env.push().define(args[0], cn, envPool);
}


//
// Create control and data flow nodes for a monitor call that receives a memory
// argument together with the given argument (the pointer to the object being
// acquired or released) and outputs a new memory state.  Set the primitive's slot
// to unknown for now.
//
Primitive &BytecodeTranslator::BlockTranslator::genMonitor(PrimitiveOperation op, const VariableOrConstant &arg, Uint32 bci) const
{
	assert(cn);
	TranslationEnv &env = *translationEnv;
	Primitive &prim = makeMonitorPrimitive(op, env.memory().extract(bci), arg, PrimMonitor::unknownSlot, cn, bci);
	env.memory().define(prim);
	return prim;
}


//
// Generate an unconditional throw of the given exception.  If the exception is null,
// throw NullPointerException instead.  Set cn to nil to indicate that no more control
// nodes should be generated.
//
// This routine assumes ownership of translationEnv.
//
void BytecodeTranslator::BlockTranslator::genThrow(const VariableOrConstant &exception)
{
	// asharma - fix me
	genNullGuard(exception, 0);
	if (cn)
		// We don't know the class of the exception object here, so conservatively assume Throwable.
		// A more detailed dataflow analysis might narrow down the possibilities.
		genThrowCommon(exception, Standard::get(cThrowable), true);
}


//
// Generate a null check for the receiver of a virtual or interface call.
// Return the receiver.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead because the receiver is always null.
//
void BytecodeTranslator::BlockTranslator::genReceiverNullCheck(const Signature &sig, VariableOrConstant &receiver, Uint32 bci)
{
	translationEnv->stackNth(sig.nArgumentSlots()).extract(receiver, bci);
	genNullGuard(receiver, bci);
}


//
// Generate code to perform a virtual method lookup.  The receiver object
// is the "this" object and is guaranteed to be nonnull.  vIndex is the index
// (not offset!) of the virtual table entry.
// Store the looked-up function address in functionAddr.
//
void BytecodeTranslator::BlockTranslator::genVirtualLookup(const VariableOrConstant &receiver, Uint32 vIndex,
														   VariableOrConstant &functionAddr, Uint32 bci) const
{
	VariableOrConstant type;
	genReadObjectType(receiver, type, bci);

	// Get the address of the vEntry.
	VariableOrConstant vEntryAddr;
	builder_Add_AIA.specializeArgConst(type, vTableIndexToOffset(vIndex), vEntryAddr, cn);

	// Emit the load instruction.
	builder_Ld_AA.makeLoad(0, vEntryAddr, functionAddr, false, true, cn, bci);
}


//
// Generate code to perform an interface method lookup.  The receiver object
// is the "this" object and is guaranteed to be nonnull.  vIndex is the index
// (not offset!) of the virtual table entry.
// Store the looked-up function address in functionAddr.
//
void BytecodeTranslator::BlockTranslator::genInterfaceLookup(const VariableOrConstant &receiver, 
															 Uint32 interfaceNumber, 
															 Uint32 vIndex, 
															 const Method *interfaceMethod, 
															 VariableOrConstant &functionAddr, 
															 Uint32 bci) const
{
	VariableOrConstant type;
	genReadObjectType(receiver, type, bci);

    // Check for a receiver object of type known at compile-time.
    // In this case, we can statically determine the method within the
    // receiver's type that implements the interface method.
	if (type.isConstant()) {
        Type &receiverType = addressAsType(type.getConstant().a);
        ClassFileSummary *receiverSummary = NULL;

        if (receiverType.typeKind == tkObject) {
            receiverSummary = asClass(receiverType).summary;
        } else {
            assert(receiverType.typeKind == tkArray);
            /* Arrays never have interface methods */
            verifyError(VerifyError::noSuchMethod);
        }

        // Get the class method that implements the given interface method
        addr a;
        Uint32 vIndex;
        const Method *method;
        method = receiverSummary->lookupImplementationOfInterfaceMethod(interfaceMethod, vIndex, a);
        if (!method)
			verifyError(VerifyError::noSuchMethod);

        // Generate normal virtual function dispatch or set function address if it's
        //   known at compile-time.
        if (method)
            functionAddr.setConstant(a);
        else
            genVirtualLookup(receiver, vIndex, functionAddr, bci);

	} else {

		VariableOrConstant vVars[2];
		VariableOrConstant &vSubVTableOffset = vVars[1];
		VariableOrConstant &vEntryOffset = vVars[0];

		// Get the address of the class-specific interface table used to locate
		// the sub-vtable dedicated to the given interface within the class' vtable
		VariableOrConstant vPointerToInterfaceTableAddr;
		builder_Add_AIA.specializeArgConst(type, offsetof(Type, interfaceVIndexTable),
		                                   vPointerToInterfaceTableAddr, cn);
		VariableOrConstant vInterfaceTableAddr;
		builder_Ld_AA.makeLoad(0, vPointerToInterfaceTableAddr, vInterfaceTableAddr, false, true, cn, bci);

		// Compute the address of the interface table entry corresponding to the
		// given interface.
		VariableOrConstant vInterfaceTableEntryAddr;
		assert(sizeof(VTableOffset) == 4);
		builder_Add_AIA.specializeArgConst(vInterfaceTableAddr, interfaceNumber * sizeof(VTableOffset),
		                                   vInterfaceTableEntryAddr, cn);

		// Compute the offset into the class's VTable which corresponds to the base
		// of the sub-vtable for this interface
		builder_Ld_AI.makeLoad(0, vInterfaceTableEntryAddr, vSubVTableOffset, false, true, cn, bci);
		                                         
		// Get the offset of the vEntry within the interface's sub-vtable.
		builder_Add_AIA.specializeArgConst(type, vIndex * sizeof(VTableEntry), vEntryOffset, cn);

		// Compute the address of the vtable entry
		VariableOrConstant vVTableEntryAddr;
		builder_Add_AIA.makeBinaryGeneric(vVars, vVTableEntryAddr, cn);

		// Emit the load instruction to get the function's address from the vtable entry.
		builder_Ld_AA.makeLoad(0, vVTableEntryAddr, functionAddr, false, true, cn, bci);
	}
}

//
// Generate a function call.  opcode should be one of the four opcodes
// invokevirtual, invokespecial, invokestatic, or invokeinterface.
// cpi is the constant pool index of the method.
// If opcode is bcInvokeInterface, nArgs is the number of arguments.
//
// Set cn to a control node to use for the next portion of this BytecodeBlock or
// nil if the rest of this BytecodeBlock is dead.
//
void BytecodeTranslator::BlockTranslator::genInvoke(bytecode opcode, ConstantPoolIndex cpi, uint nArgs, Uint32 bci)
{
	Signature sig;
	VariableOrConstant receiver;
	VariableOrConstant function;
	const Method *method;
	TranslationEnv &env = *translationEnv;

	function.setConstant(nullAddr);
	switch (opcode) {
		case bcInvokeVirtual:
			{
				Uint32 vIndex;
				method = classFileSummary.lookupVirtualMethod(cpi, sig, vIndex, function.getConstant().a);
				genReceiverNullCheck(sig, receiver, bci);
				if (!cn)
					return;
				if (!method)
					genVirtualLookup(receiver, vIndex, function, bci);
			}
			break;
		case bcInvokeSpecial:
			{
				bool isInit;
				method = &classFileSummary.lookupSpecialMethod(cpi, sig, isInit, function.getConstant().a);
				genReceiverNullCheck(sig, receiver, bci);
				if (!cn)
					return;
			}
			break;
		case bcInvokeStatic:
			method = &classFileSummary.lookupStaticMethod(cpi, sig, function.getConstant().a);
			break;
		case bcInvokeInterface:
			{
				Uint32 vIndex, interfaceNumber;
				method = classFileSummary.lookupInterfaceMethod(cpi, sig, vIndex, interfaceNumber,
                                                                function.getConstant().a, nArgs);
				genReceiverNullCheck(sig, receiver, bci);
				if (!cn)
					return;
                assert(!method);
				genInterfaceLookup(receiver, interfaceNumber, vIndex, method, function, bci);
			}
			break;
		default:
			trespass("Bad invoke opcode");
			return;
	}

	VariableOrConstant result;
	VariableOrConstant args20[20];	// Hold arguments here if there are fewer than 20.
	VariableOrConstant *args = args20;
	uint nArguments = sig.nArguments;
	if (nArguments > 20)
		args = new(envPool) VariableOrConstant[nArguments];

	// Pop the arguments off the environment stack.
	const Type **argumentType = sig.argumentTypes + nArguments;
	VariableOrConstant *arg = args + nArguments;
	while (arg != args)
		env.pop1or2(isDoublewordKind((*--argumentType)->typeKind)).extract(*--arg, bci);

	// Create the call primitive.
	DataNode *memory = &env.memory().extract(bci);
	Primitive &prim = makeCall(sig, memory, function, method, args, &result, *cn, bci);
	env.memory().define(*memory);

	// Push the result onto the environment stack.
	if (sig.getNResults())
		env.push1or2(isDoublewordKind(result.getKind())).define(result, cn, envPool);
	genPossibleThrow(prim, Standard::get(cThrowable));
}


//
// Create control and data flow nodes for the primitives inside this BytecodeBlock.
// Use and modify translationEnv to keep track of the locals' and stack temporaries' values.
//
// Set cn to nil when done.
//
void BytecodeTranslator::BlockTranslator::genLivePrimitives()
{
	BytecodeGraph &bg = bt.bytecodeGraph;
	TranslationEnv &env = *translationEnv;
	const AttributeCode *code = (AttributeCode *) 
		bg.method.getMethodInfo().getAttribute("Code");
	AttributeLocalVariableTable *localVariableTable = 
		(AttributeLocalVariableTable *) code->getAttribute("LocalVariableTable");
	const bytecode *const bytecodesEnd = block.bytecodesEnd;
	const bytecode *bc = block.bytecodesBegin;

	while (bc != bytecodesEnd) {
		assert(bc < bytecodesEnd);
		bytecode opcode = *bc++;
		ValueKind vk;
		Value v;
		Uint32 i;
		ConstantPoolIndex cpi;
		Int32 j;
		TranslationBinding *tb;
		VariableOrConstant args[4];
		VariableOrConstant result;
		Primitive *prim;
		const Type *cl;
		IntervalMapping	*mapping;
		Uint32 bci = bc - bg.bytecodesBegin;

		switch (opcode) {

			case bcNop:
				break;

			case bcAConst_Null:
				env.push().definePtr(nullAddr, cn, envPool);
				break;
			case bcIConst_m1:
			case bcIConst_0:
			case bcIConst_1:
			case bcIConst_2:
			case bcIConst_3:
			case bcIConst_4:
			case bcIConst_5:
				env.push().defineInt(opcode - bcIConst_0, cn, envPool);
				break;
			case bcLConst_0:
				env.push2().defineLong(0, cn, envPool);
				break;
			case bcLConst_1:
				env.push2().defineLong(1, cn, envPool);
				break;
			case bcFConst_0:
				env.push().defineFloat(0.0, cn, envPool);
				break;
			case bcFConst_1:
				env.push().defineFloat(1.0, cn, envPool);
				break;
			case bcFConst_2:
				env.push().defineFloat(2.0, cn, envPool);
				break;
			case bcDConst_0:
				env.push2().defineDouble(0.0, cn, envPool);
				break;
			case bcDConst_1:
				env.push2().defineDouble(1.0, cn, envPool);
				break;
			case bcBIPush:
				env.push().defineInt(*(Int8 *)bc, cn, envPool);
				bc++;
				break;
			case bcSIPush:
				env.push().defineInt(readBigSHalfwordUnaligned(bc), cn, envPool);
				bc += 2;
				break;
			case bcLdc:
				cpi = *(Uint8 *)bc;
				bc++;
				goto pushConst1;
			case bcLdc_W:
				cpi = readBigUHalfwordUnaligned(bc);
				bc += 2;
			  pushConst1:
				vk = classFileSummary.lookupConstant(cpi, v);
				assert(isWordKind(vk));
				env.push().define(vk, v, cn, envPool);
				break;
			case bcLdc2_W:
				vk = classFileSummary.lookupConstant(readBigUHalfwordUnaligned(bc), v);
				bc += 2;
				assert(isDoublewordKind(vk));
				env.push2().define(vk, v, cn, envPool);
				break;

			case bcILoad:
			case bcFLoad:
			case bcALoad:
				i = *(Uint8 *)bc;
				bc++;
			  pushLoad1:
				env.push() = env.local(i);
				break;
			case bcLLoad:
			case bcDLoad:
				i = *(Uint8 *)bc;
				bc++;
			  pushLoad2:
				assert(env.local(i+1).isSecondWord());
				env.push2() = env.local(i);
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
				genArrayEltAccess(arrayAccessTypeKinds[opcode - bcIALoad], false, bci);
				goto checkDeadRestOfBlock;

			case bcIStore:
			case bcFStore:
			case bcAStore:
				i = *(Uint8 *)bc;
				bc++;
			  popStore1:
				// asharma - label this one
				if (localVariableTable) {
					DoublyLinkedList<IntervalMapping> &mappings =
						localVariableTable->getEntry(i)->mappings;
					IntervalMapping *prev;
				
					mapping = new IntervalMapping();	// Fix me!
					mapping->setStart((Uint32) (bc - bg.bytecodesBegin));
					if (!mappings.empty()) {
						prev = &mappings.get(mappings.end());
						prev->setEnd((Uint32) (bc - bg.bytecodesBegin));
					}
					mappings.addLast(*mapping);
					tb = (TranslationBinding *) &env.stackNth(1);
					if (tb->isDataEdge()) {
						tb->extract(bci).mapping = mapping;
					}
				}				
				env.local(i) = env.pop();
				break;
			case bcLStore:
			case bcDStore:
				i = *(Uint8 *)bc;
				bc++;
			  popStore2:
				// asharma label this one
				env.local(i) = env.pop2();
				env.local(i+1).defineSecondWord();
				break;
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
				goto popStore1;

			case bcIAStore:
			case bcLAStore:
			case bcFAStore:
			case bcDAStore:
			case bcAAStore:
			case bcBAStore:
			case bcCAStore:
			case bcSAStore:
				genArrayEltAccess(arrayAccessTypeKinds[opcode - bcIAStore], true, bci);
				goto checkDeadRestOfBlock;

			case bcPop:
				env.drop(1);
				break;
			case bcPop2:
				env.drop(2);
				break;
			case bcDup:
				tb = &env.stackNth(1);
				env.push() = *tb;
				break;
			case bcDup_x1:
				env.raise(1);
				tb = env.stackTopN(3);
				tb[-1] = tb[-2];
				tb[-2] = tb[-3];
				tb[-3] = tb[-1];
				break;
			case bcDup_x2:
				env.raise(1);
				tb = env.stackTopN(4);
				tb[-1] = tb[-2];
				tb[-2] = tb[-3];
				tb[-3] = tb[-4];
				tb[-4] = tb[-1];
				break;
			case bcDup2:
				env.raise(2);
				tb = env.stackTopN(4);
				tb[-1] = tb[-3];
				tb[-2] = tb[-4];
				break;
			case bcDup2_x1:
				env.raise(2);
				tb = env.stackTopN(5);
				tb[-1] = tb[-3];
				tb[-2] = tb[-4];
				tb[-3] = tb[-5];
				tb[-4] = tb[-1];
				tb[-5] = tb[-2];
				break;
			case bcDup2_x2:
				env.raise(2);
				tb = env.stackTopN(6);
				tb[-1] = tb[-3];
				tb[-2] = tb[-4];
				tb[-3] = tb[-5];
				tb[-4] = tb[-6];
				tb[-5] = tb[-1];
				tb[-6] = tb[-2];
				break;
			case bcSwap:
				{
					TranslationBinding tempBinding;
					tb = env.stackTopN(2);
					tempBinding = tb[-1];
					tb[-1] = tb[-2];
					tb[-2] = tempBinding;
				}
				break;

			case bcIAdd:
			case bcFAdd:
			case bcISub:
			case bcFSub:
			case bcIMul:
			case bcFMul:
			case bcFDiv:
			case bcFRem:
			case bcIShl:
			case bcIShr:
			case bcIUShr:
			case bcIAnd:
			case bcIOr:
			case bcIXor:
			case bcFCmpL:
			case bcFCmpG:
				env.pop().extract(args[1], bci);
				tb = env.stackTopN(1) - 1;
			  genGenericBinary:
				tb->extract(args[0], bci);
			  genExtractedBinary:
				binaryBuilders[opcode - bcIAdd]->makeBinaryGeneric(args, result, cn);
				tb->define(result, cn, envPool);
				break;
			case bcLAdd:
			case bcDAdd:
			case bcLSub:
			case bcDSub:
			case bcLMul:
			case bcDMul:
			case bcDDiv:
			case bcDRem:
			case bcLAnd:
			case bcLOr:
			case bcLXor:
				env.pop2().extract(args[1], bci);
			  genLongShift:
				tb = env.stackTopN(2) - 2;
				assert(tb[1].isSecondWord());
				goto genGenericBinary;
			case bcLShl:
			case bcLShr:
			case bcLUShr:
				env.pop().extract(args[1], bci);
				goto genLongShift;
			case bcLCmp:
			case bcDCmpL:
			case bcDCmpG:
				env.pop2().extract(args[1], bci);
				env.pop2().extract(args[0], bci);
				tb = &env.push();
				goto genExtractedBinary;

			case bcIDiv:
			case bcIRem:
				env.pop().extract(args[1], bci);
				tb = env.stackTopN(1) - 1;
				assert(args[1].hasKind(vkInt));
				if (args[1].isConstant() && args[1].getConstant().i == 0) {
					// If the second argument is always zero, this operation becomes an unconditional throw.
					genSimpleThrow(&Standard::get(oArithmeticException), true);
					// The rest of this BytecodeBlock is dead.
					return;
				}
			  genIntegerDivRem:
				tb->extract(args[0], bci);
				prim = binaryBuilders[opcode - bcIAdd]->makeBinaryGeneric(args, result, cn);
				tb->define(result, cn, envPool);
				if (prim && prim->canRaiseException())
					genPossibleThrow(*prim, Standard::get(cArithmeticException));
				break;
			case bcLDiv:
			case bcLRem:
				env.pop2().extract(args[1], bci);
				tb = env.stackTopN(2) - 2;
				assert(tb[1].isSecondWord());
				assert(args[1].hasKind(vkLong));
				if (args[1].isConstant() && args[1].getConstant().l == 0) {
					// If the second argument is always zero, this operation becomes an unconditional throw.
					genSimpleThrow(&Standard::get(oArithmeticException), true);
					// The rest of this BytecodeBlock is dead.
					return;
				}
				goto genIntegerDivRem;

			case bcINeg:
			case bcFNeg:
			case bcI2F:
			case bcF2I:
			case bcInt2Byte:
			case bcInt2Char:
			case bcInt2Short:
				tb = env.stackTopN(1) - 1;
			  genGenericUnary:
				tb->extract(args[0], bci);
			  genExtractedUnary:
				unaryBuilders[opcode - bcINeg]->makeUnaryGeneric(args[0], result, cn);
				tb->define(result, cn, envPool);
				break;
			case bcLNeg:
			case bcDNeg:
			case bcL2D:
			case bcD2L:
				tb = env.stackTopN(2) - 2;
				assert(tb[1].isSecondWord());
				goto genGenericUnary;
			case bcI2L:
			case bcI2D:
			case bcF2L:
			case bcF2D:
				env.pop().extract(args[0], bci);
				tb = &env.push2();
				goto genExtractedUnary;
			case bcL2I:
			case bcL2F:
			case bcD2I:
			case bcD2F:
				env.pop2().extract(args[0], bci);
				tb = &env.push();
				goto genExtractedUnary;

			case bcIInc:
				i = *(Uint8 *)bc;
				bc++;
				j = *(Int8 *)bc;
				bc++;
			  incLocal:
				// asharma label this one
				env.local(i).extract(args[0], bci);
				builder_AddCoal_III.specializeArgConst(args[0], j, result, cn);
				env.local(i).define(result, cn, envPool);
				break;

			case bcIfEq:
			case bcIfNe:
			case bcIfLt:
			case bcIfGe:
			case bcIfGt:
			case bcIfLe:
				env.pop().extract(args[0], bci);
				prim = unaryComparisonBuilders[opcode - bcIfEq]->specializeArgConst(args[0], 0, result, cn);
			  genIf:
				// We should have exactly two successors.
				createIf(bytecodeConditionals[opcode - bcIfEq], prim != 0, result, block.getSuccessor(0), block.getSuccessor(1), bci);
				// We should be at the end of the basic block.
				assert(bc + 2 == bytecodesEnd);
				return;
			case bcIf_ICmpEq:
			case bcIf_ICmpNe:
			case bcIf_ICmpLt:
			case bcIf_ICmpGe:
			case bcIf_ICmpGt:
			case bcIf_ICmpLe:
			case bcIf_ACmpEq:
			case bcIf_ACmpNe:
				env.pop().extract(args[1], bci);
				env.pop().extract(args[0], bci);
				prim = binaryComparisonBuilders[opcode - bcIf_ICmpEq]->makeBinaryGeneric(args, result, cn);
				goto genIf;

			case bcGoto_W:
				bc += 2;
			case bcGoto:
				bc += 2;
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;

			case bcJsr_W:
				bc += 2;
			case bcJsr:
				bc += 2;
				env.push().clear();	// Push the return address, which is not used since we eliminated all ret's already.
				// We should be at the end of the basic block.
				assert(bc == bytecodesEnd);
				break;

			case bcTableSwitch:
				genTableSwitch(bc, bci);
				return;
			case bcLookupSwitch:
				genLookupSwitch(bc, bci);
				return;

			case bcLReturn:
			case bcDReturn:
				env.pop2().extract(args[0], bci);
				goto genValueReturn;
			case bcIReturn:
			case bcFReturn:
			case bcAReturn:
				env.pop().extract(args[0], bci);
			  genValueReturn:
				vk = args[0].getKind();
				cn->setControlReturn(1, &vk, bci);
				cn->getReturnExtra()[0].getInput() = args[0];
				goto genReturn;
			case bcReturn:
				cn->setControlReturn(0, 0, bci);
			  genReturn:
				assert(block.nSuccessors() == 1);
				bt.linkPredecessor(*block.successorsBegin[0], cn->getReturnSuccessor(), env, true);
				cn = 0;
				assert(bc == bytecodesEnd);
				return;

			case bcGetStatic:
				genGetStatic(readBigUHalfwordUnaligned(bc), bci);
				bc += 2;
				break;
			case bcPutStatic:
				genPutStatic(readBigUHalfwordUnaligned(bc), bci);
				bc += 2;
				break;
			case bcGetField:
				genGetField(readBigUHalfwordUnaligned(bc), bci);
			  add2CheckDeadRestOfBlock:
				bc += 2;
				// Exit if the rest of this BytecodeBlock is dead.
			  checkDeadRestOfBlock:
				if (!cn) {
					// The rest of this BytecodeBlock is dead.
					assert(bc < bytecodesEnd);
					return;
				}
				break;
			case bcPutField:
				genPutField(readBigUHalfwordUnaligned(bc), bci);
				goto add2CheckDeadRestOfBlock;

			case bcInvokeVirtual:
			case bcInvokeSpecial:
			case bcInvokeStatic:
				genInvoke(opcode, readBigUHalfwordUnaligned(bc), 0, bci);
				goto add2CheckDeadRestOfBlock;
			case bcInvokeInterface:
				genInvoke(opcode, readBigUHalfwordUnaligned(bc), *(Uint8 *)(bc + 2), bci);
				bc += 4;
				goto checkDeadRestOfBlock;

			case bcNew:
				args[0].setConstant(objectAddress(&classFileSummary.lookupClass(readBigUHalfwordUnaligned(bc))));
				genNewCommon(scNew, 1, args, bci);
				goto add2CheckDeadRestOfBlock;

			case bcNewArray:
				i = *(Uint8 *)bc;
				bc++;
				i -= natMin;
				if (i >= natLimit - natMin)
					verifyError(VerifyError::badNewArrayType);
				env.pop().extract(args[0], bci);
				genNewCommon(*newArrayCalls[i], 1, args, bci);
				goto checkDeadRestOfBlock;

			case bcANewArray:
				args[0].setConstant(objectAddress(&classFileSummary.lookupType(readBigUHalfwordUnaligned(bc))));
				env.pop().extract(args[1], bci);
				genNewCommon(scNewObjectArray, 2, args, bci);
				goto add2CheckDeadRestOfBlock;

			case bcMultiANewArray:
				cl = &classFileSummary.lookupType(readBigUHalfwordUnaligned(bc));
				args[0].setConstant(objectAddress(cl)); // array type argument
				i = *(Uint8 *)(bc+2); // dimension
				bc += 3;
				switch (i) {
				case 1:
					cl = &asArray(*cl).getElementType();
					if (isPrimitiveKind(cl->typeKind)) { // converted to a base type
						env.pop().extract(args[0], bci); // number of elements
						genNewCommon(*typeToArrayCalls[cl->typeKind], 1, args, bci);
					}
					else { // converted to a class type
						env.pop().extract(args[1], bci); // number of elements
						genNewCommon(scNewObjectArray, 2, args, bci);
					}
					break;
				case 2:
					env.pop().extract(args[2], bci);  // first dimension
					env.pop().extract(args[1], bci);  // second dimension
					genNewCommon(scNew2DArray, 3, args, bci);
					break;
				case 3:
					env.pop().extract(args[3], bci);  // as above
					env.pop().extract(args[2], bci);
					env.pop().extract(args[1], bci);
					genNewCommon(scNew3DArray, 4, args, bci);
					break;
				default:
					// i>=4
					// generate temporary array to store all the dimensions in, and
					// push it onto the environment
					genTempArrayOfDim(i, bci);
					if (!cn)
						return; // temporary array construction will throw exception
					env.pop().extract(args[1], bci);
					genNewCommon(scNewNDArray, 2, args, bci);
					break;
				}			
				goto checkDeadRestOfBlock;

			case bcArrayLength:
				env.pop().extract(args[0], bci);
				genReadArrayLength(args[0], result, bci);
				if (!cn)
					return;	// The rest of this BytecodeBlock is dead.
				env.push().define(result, cn, envPool);
				break;

			case bcAThrow:
				env.pop().extract(args[0], bci);
				genThrow(args[0]);
				// The rest of this BytecodeBlock is dead, and we should be at the end of it.
				assert(bc == bytecodesEnd);
				return;

			case bcCheckCast:
				env.stackNth(1).extract(args[0], bci);
				genCheckCast(readBigUHalfwordUnaligned(bc), args[0], bci);
				goto add2CheckDeadRestOfBlock;

			case bcInstanceOf:
				env.pop().extract(args[0], bci);
				genInstanceOf(readBigUHalfwordUnaligned(bc), args[0], result, bci);
				assert(cn);	// instanceof can't throw exceptions.
				bc += 2;
				env.push().define(result, cn, envPool);
				break;

			case bcMonitorEnter:
				env.pop().extract(args[0], bci);
				genNullGuard(args[0], bci);
				if (cn)
					genPossibleThrow(genMonitor(poMEnter, args[0], bci), Standard::get(cError));
				else {
					// The rest of this BytecodeBlock is dead.
					assert(bc < bytecodesEnd);
					return;
				}
				break;

			case bcMonitorExit:
				env.pop().extract(args[0], bci);
				genMonitor(poMExit, args[0], bci);
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
					case bcAStore:
						goto popStore1;
					case bcLStore:
					case bcDStore:
						goto popStore2;

					case bcIInc:
						j = readBigSHalfwordUnaligned(bc);
						bc += 2;
						goto incLocal;

					// We should not see these here; they should have been taken out by now.
					// case bcRet:
					default:
						verifyError(VerifyError::badBytecode);
				}
				break;

			case bcIfNull:
			case bcIfNonnull:
				env.pop().extract(args[0], bci);
				prim = builder_CmpUEq_AAC.specializeArgConst(args[0], nullAddr, result, cn);
				goto genIf;

			case bcBreakpoint:
				// Ignore breakpoints.
				break;

			// We should not see these here; they should have been taken out by now.
			// case bcRet:
			default:
				verifyError(VerifyError::badBytecode);
		}
	}

	// We fell off the last opcode of this basic block of bytecodes.
	// We should have exactly one successor.
	assert(block.nSuccessors() == 1);
	createBasicBlock(block.getSuccessor(0));
}


//
// Adjust this BytecodeBlock's local environment to allow primitives to be generated
// for this BytecodeBlock before all of its predecessors have been generated.
// This means that local and stack variable bindings may have to be replaced by
// phantom phi nodes.  [This is inefficient, so this code should be revised to
// do a prepass figuring out exactly which phi nodes are needed.]
//
// Also create an aexc header node for the cycle.  Set cn to the ControlNode to use for
// the rest of this BytecodeBlock.
//
void BytecodeTranslator::BlockTranslator::genCycleHeader()
{
	translationEnv->anticipate(block);
	genException(ckAExc, 0, Standard::get(cThreadDeath), false);
	appendControlNode(cn->getNormalSuccessor());
}


// ----------------------------------------------------------------------------
// BytecodeTranslator


//
// If stackNormalization is either sn0, sn1, or sn2, pop all except the top
// zero, one, or two words from env, modifying it in place.
//
void BytecodeTranslator::normalizeEnv(TranslationEnv &env, BasicBlock::StackNormalization stackNormalization)
{
	TranslationBinding tempBinding;

	switch (stackNormalization) {
		case BasicBlock::snNoChange:
			return;
		case BasicBlock::sn0:
			break;
		case BasicBlock::sn1:
			tempBinding = env.pop();
			break;
		case BasicBlock::sn2:
			tempBinding = env.pop2();
			break;
	}
	env.dropAll();
	switch (stackNormalization) {
		case BasicBlock::sn0:
			break;
		case BasicBlock::sn1:
			env.push() = tempBinding;
			break;
		case BasicBlock::sn2:
			env.push2() = tempBinding;
			break;
		default:
			trespass("Bad stackNormalization");
	}
}


//
// Take notice that the control node(s) generated from BasicBlock block will have
// another predecessor, as given by controlEdge.  The controlEdge should be allocated
// out of the control graph's memory pool.
// Merge the BasicBlock's environment with predecessorEnv, adding phi nodes or
// phantom phi nodes as needed.  If canOwnEnv is true, the BasicBlock can assume
// ownership of predecessorEnv (thereby sometimes eliminating an unnecessary copy).
//
void BytecodeTranslator::linkPredecessor(BasicBlock &block, ControlEdge &controlEdge, TranslationEnv &predecessorEnv, bool canOwnEnv)
{
	BasicBlock::StackNormalization stackNormalization = block.stackNormalization;
	if (stackNormalization != BasicBlock::snNoChange && predecessorEnv.getSP() != (Uint32)(stackNormalization-BasicBlock::sn0))
		// We have to normalize the stack inside predecessorEnv.
		if (canOwnEnv)
			normalizeEnv(predecessorEnv, stackNormalization);
		else {
			TranslationEnv envCopy(predecessorEnv);
			normalizeEnv(envCopy, stackNormalization);
			linkPredecessor(block, controlEdge, envCopy, true);
			return;
		}

	ControlNode *first = block.getFirstControlNode();
	assert(block.getGeneratedControlNodes() == (first != 0));
	if (first) {
		// Only aexc nodes can be targets of backward edges.
		assert(block.hasIncomingBackEdges && first->hasControlKind(ckAExc));
		first->disengagePhis();
		first->addPredecessor(controlEdge);
	} else
		block.getPredecessors().addLast(controlEdge);

	if (block.getEnvInInitialized())
		block.getTranslationEnvIn().meet(predecessorEnv, block.hasKind(BasicBlock::bbEnd), block);
	else {
		if (canOwnEnv)
			block.getTranslationEnvIn().move(predecessorEnv);
		else
			block.getTranslationEnvIn() = predecessorEnv;
		block.getEnvInInitialized() = true;
	}
  #ifdef DEBUG
	if (first)
		first->reengagePhis();
  #endif
}



//
// Call finishedOnePredecessor on every successor.
//
void BytecodeTranslator::finishedOutgoingEdges(BasicBlock &block)
{
	BasicBlock **handlersEnd = block.handlersEnd;
	for (BasicBlock **bp = block.successorsBegin; bp != handlersEnd; bp++)
		finishedOnePredecessor(**bp);
}


//
// Create a new ControlNode for BasicBlock block.
// BasicBlock block must not be dead.
// Return the ControlNode to use for the rest of BasicBlock block.
// If mergeBlock is true and there is only one predecessor node that is a Block node,
// continue to use that Block node instead of making a new ControlNode.
//
ControlNode *BytecodeTranslator::createControlNode(BasicBlock &block, bool mergeBlock) const
{
	assert(!block.getGeneratedControlNodes());	// We shouldn't have generated anything for this node yet.

	if (mergeBlock && block.nPredecessors == block.getNSeenPredecessors() && block.getPredecessors().lengthIs(1)) {
		ControlNode &predNode = DoublyLinkedList<ControlEdge>::get(block.getPredecessors().begin()).getSource();
		if (predNode.hasControlKind(ckBlock)) {
			// This node has only one predecessor and it was a ckBlock node, so
			// merge this node and its predecessor now.
			predNode.unsetControlKind();
		  #ifdef DEBUG
			block.getGeneratedControlNodes() = true;
		  #endif
			return &predNode;
		}
	}

	ControlNode *cn = &controlGraph.newControlNode();
	block.getFirstControlNode() = cn;
	cn->movePredecessors(block.getPredecessors());
  #ifdef DEBUG
	block.getGeneratedControlNodes() = true;
  #endif
	return cn;
}


//
// Create control and data flow nodes out of the EndBlock.  genPrimitives must
// have been called on every other BasicBlock before calling this.
//
void BytecodeTranslator::genPrimitives(EndBlock &block) const
{
	// We should have seen all other blocks by now.
	assert(block.getNSeenPredecessors() == block.nPredecessors && !block.hasIncomingBackEdges);
	// The end block is never dead; either normal control flow or exceptions can always reach it.
	assert(block.getEnvInInitialized());

	assert(!block.getGeneratedControlNodes());	// We shouldn't have generated anything for this node yet.
	ControlNode &endNode = controlGraph.getEndNode();
	block.getFirstControlNode() = &endNode;
	block.getFirstControlNode()->movePredecessors(block.getPredecessors());
  #ifdef DEBUG
	block.getGeneratedControlNodes() = true;
  #endif

	// Initialize the finalMemory in the EndNode.
	// asharma - fix me
	endNode.getEndExtra().finalMemory.getInput().setVariable(block.getTranslationEnvIn().memory().extract(0));
}


//
// Create control and data flow nodes out of the CatchBlock.
//
void BytecodeTranslator::genPrimitives(CatchBlock &block) const
{
	// CatchBlocks cannot be targets of backward control edges.
	assert(block.getNSeenPredecessors() == block.nPredecessors);
	if (block.getEnvInInitialized()) {
		// This isn't a dead CatchBlock if some predecessor already jumps here.
		assert(block.getNSeenPredecessors());
		assert(block.nSuccessors() == 1);
		ControlNode *cn = createControlNode(block, false);
		// asharma fix me!
		block.getTranslationEnvIn().push().define(cn->setControlCatch(0));
		linkPredecessor(block.getHandler(), cn->getNormalSuccessor(), block.getTranslationEnvIn(), true);
	}

	// Adjust the nPredecessors counts of the successor BasicBlock.
	finishedOutgoingEdges(block);
}


//
// Create control and data flow nodes out of the BytecodeBlock.
//
void BytecodeTranslator::genPrimitives(BytecodeBlock &block) const
{
	assert(block.getNSeenPredecessors() <= block.nPredecessors);
	if (block.getNSeenPredecessors() != block.nPredecessors || block.getEnvInInitialized()) {
		// This isn't a dead BytecodeBlock if either some predecessor already jumps
		// here or we haven't examined all of the predecessor BasicBlocks yet.
		assert(block.getNSeenPredecessors());
		BlockTranslator translator(*this, block.getTranslationEnvIn(), block, createControlNode(block, true));

		if (block.getNSeenPredecessors() == block.nPredecessors)
			// We've seen all the predecessors already.  Modify envIn directly -- we won't
			// need it any more for merging additional predecessors.
			translator.genLivePrimitives();
		else {
			// If this block has unseen predecessors in the depth-first order,
			// then this block must be the header of a cycle.  Use a copy of envIn to
			// process this block's primitives.
			assert(block.hasIncomingBackEdges);
			translator.genCycleHeader();
			TranslationEnv cycleEnv(block.getTranslationEnvIn());
			translator.setEnv(cycleEnv);
			translator.genLivePrimitives();
			// cycleEnv must be live up to this point.
		}
		assert(translator.translatorDone());
	}

	// Adjust the nPredecessors counts of successor BasicBlocks.
	finishedOutgoingEdges(block);
}


//
// Create control and data flow nodes out of this BytecodeGraph.
//
void BytecodeTranslator::genRawPrimitiveGraph() const
{
	BasicBlock **blockArray = bytecodeGraph.getDFSList();
	for (Uint32 i = bytecodeGraph.getDFSListLength() - 1; i; i--) {
		BasicBlock *block = *blockArray++;
		assert(block->hasKind(BasicBlock::bbBytecode) || block->hasKind(BasicBlock::bbCatch));
		if (block->hasKind(BasicBlock::bbBytecode))
			genPrimitives(*static_cast<BytecodeBlock *>(block));
		else
			genPrimitives(*static_cast<CatchBlock *>(block));
	}
	assert((*blockArray)->hasKind(BasicBlock::bbEnd));
	genPrimitives(*static_cast<EndBlock *>(*blockArray));
}


//
// Add a synchronization primitive to acquire the syncHolder object's lock on
// method entry.  Return the exc control node that contains the MEnter primitive.
//
static ControlNode &addEnterNode(ControlGraph &cg, VariableOrConstant &syncHolder)
{
	Pool &pool = cg.pool;
	DoublyLinkedList<ControlEdge>::iterator where;
	ControlNode &beginNode = cg.getBeginNode();
	ControlNode &endNode = cg.getEndNode();
	ControlNode *secondNode;
	DataNode &beginMemory = beginNode.getBeginExtra().initialMemory;

	// Insert a new exc node (called enterNode) immediately after the begin node.
	// The exc node's normal outgoing edge will go to the node (secondNode) that used to
	// be the successor of the begin node.  The exc node will also have one outgoing exception
	// edge that goes directly to the end node.
	ControlNode &enterNode = insertControlNodeAfter(beginNode, secondNode, where); // Disengages secondNode's phi nodes.
	const Class **filter = new(pool) const Class *;
	filter[0] = &Standard::get(cThrowable);
	ControlEdge *enterSuccessors = new(pool) ControlEdge[2];
	// asharma - fix me
	Primitive &primEnter = makeMonitorPrimitive(poMEnter, beginMemory, syncHolder, 0, &enterNode, 0);
	enterNode.setControlException(ckExc, 1, filter, &primEnter, enterSuccessors);

	// Link the enterNode to its normal successor.
	secondNode->addPredecessor(enterSuccessors[0], where);
	secondNode->reengagePhis();

	// Link the enterNode's exception edge to the end node,
	// adjusting the end node's memory phi node as necessary.
	VariableOrConstant memoryOutVOC;
	memoryOutVOC.setVariable(primEnter);
	endNode.disengagePhis();
	endNode.addPredecessor(enterSuccessors[1]);
	addDataFlow(endNode, endNode.getEndExtra().finalMemory.getInput(), memoryOutVOC);
	endNode.reengagePhis();

	// Replace the function's incoming memory edge by the memory edge (primEnter) generated by the MEnter
	// everywhere except in the MEnter itself.
	const DoublyLinkedList<DataConsumer> &beginMemoryConsumers = beginMemory.getConsumers();
	for (DoublyLinkedList<DataConsumer>::iterator i = beginMemoryConsumers.begin(); !beginMemoryConsumers.done(i);) {
		DataConsumer &c = beginMemoryConsumers.get(i);
		i = beginMemoryConsumers.advance(i);
		if (&c.getNode() != &primEnter) {
			c.clearVariable();
			c.setVariable(primEnter);
		}
	}
	return enterNode;
}


//
// Add a synchronization primitive to release the syncHolder object's lock on
// normal method exit via the return node if there is one.
//
static void addNormalExitNode(ControlGraph &cg, VariableOrConstant &syncHolder)
{
	ControlNode *returnNode = cg.getReturnNode();
	if (returnNode) {
		ControlNode &endNode = cg.getEndNode();
		ControlEdge &returnEdge = returnNode->getReturnSuccessor();

		// Add an MExit primitive to the return node, consuming the return node's
		// current outgoing memory edge and producing a new memory edge.
		DataConsumer &finalMemory = endNode.getEndExtra().finalMemory.getInput();
		DataNode &returnMemory = followVariableBack(finalMemory, endNode, returnEdge).getVariable();
		// asharma - fix me
		Primitive &returnMemoryOut = makeMonitorPrimitive(poMExit, returnMemory, syncHolder, 0, returnNode, 0);

		// Fix up the end node's memory phi node (creating one if needed) to receive the
		// memory edge outgoing from our new MonitorExit primitive when control comes to the end
		// node from the return node.
		VariableOrConstant returnMemoryOutVOC;
		returnMemoryOutVOC.setVariable(returnMemoryOut);
		changeDataFlow(endNode, returnEdge, finalMemory, returnMemoryOutVOC);
	}
}


class ExceptionEdgeIteratee: public Function1<bool, ControlEdge &>
{
	ControlNode *enterNode;

  public:
	explicit ExceptionEdgeIteratee(ControlNode *enterNode): enterNode(enterNode) {}

	bool operator()(ControlEdge &e);
};


//
// Edge e is an edge pointing to the end node in a synchronized method.
// Return true if that method's synchronization lock should be released if
// control follows edge e.
// Given our implementation of addEnterNode and addNormalExitNode, this
// method returns false for edges originating in:
//   the return node,
//   the node at the beginning of the method that acquires the monitor, and
//   the node just before the return node that releases the monitor on normal exit.
// For all other edges this method returns true.
//
bool ExceptionEdgeIteratee::operator()(ControlEdge &e)
{
	ControlNode &source = e.getSource();
	return !(source.hasControlKind(ckReturn) || &source == enterNode);
}


//
// Add a synchronization primitive to release the syncHolder object's lock on
// exceptional method exit.
//
static void addExceptionalExitNode(ControlGraph &cg, VariableOrConstant &syncHolder, ControlNode &enterNode, Uint32 bci)
{
	ExceptionEdgeIteratee eei(&enterNode);
	ControlNode &endNode = cg.getEndNode();
	ControlNode *exitCatchNode = insertControlNodeBefore(endNode, eei); // Disengages endNode's phi nodes if returns non-nil.
	if (exitCatchNode) {
		// We have at least one exceptional method exit path that doesn't come from
		// the (already inserted) MEnter exc node.

		// Make the newly inserted node into a catch node that intercepts all exceptions
		// thrown out of this method except those coming from our new MEnter exc node.
		// Connect this catch node's outgoing edge directly to the end node for now.
		PrimCatch &primCatch = exitCatchNode->setControlCatch(bci);
		ControlEdge &exitCatchSuccessorEdge = exitCatchNode->getNormalSuccessor();
		endNode.addPredecessor(exitCatchSuccessorEdge);
		endNode.reengagePhis();

		// exitMemoryIn will be the memory input to the MonitorExit primitive we're
		// about to insert.
		DataConsumer &finalMemory = endNode.getEndExtra().finalMemory.getInput();
		DataNode &exitMemoryIn = followVariableBack(finalMemory, endNode, exitCatchSuccessorEdge).getVariable();

		// Insert a new throw node (called exitNode) immediately after the catch node.
		// The throw node will have an MExit primitive followed by a throw primitive that
		// rethrows the caught exception.  The throw node will have one outgoing exception
		// edge that goes directly to the end node.
		ControlNode *successor;
		DoublyLinkedList<ControlEdge>::iterator where;
		ControlNode &exitNode = insertControlNodeAfter(*exitCatchNode, successor, where); // Disengages endNode's phi nodes.
		assert(successor == &endNode);
		// asharma - fix me
		Primitive &exitMemoryOut = makeMonitorPrimitive(poMExit, exitMemoryIn, syncHolder, 0, &exitNode, 0);

		// Initialize the throw primitive to rethrow the caught exception.
		Pool &pool = cg.pool;
		const Class **filter = new(pool) const Class *;
		filter[0] = &Standard::get(cThrowable);
		ControlEdge *throwSuccessors = new(pool) ControlEdge;
		VariableOrConstant primCatchVOC;
		primCatchVOC.setVariable(primCatch);
		Primitive &throwPrim = makeSysCall(scThrow, 0, 1, &primCatchVOC, 0, 0, exitNode, bci);
		exitNode.setControlException(ckThrow, 1, filter, &throwPrim, throwSuccessors);

		// Link the throw node's outgoing edge to the end node at location where,
		// adjusting the end node's memory phi node as necessary.
		VariableOrConstant exitMemoryOutVOC;
		exitMemoryOutVOC.setVariable(exitMemoryOut);
		endNode.addPredecessor(throwSuccessors[0], where);
		endNode.reengagePhis();
		changeDataFlow(endNode, throwSuccessors[0], finalMemory, exitMemoryOutVOC);
	}
}


//
// Add synchronization primitives to acquire the syncHolder object's lock on method
// entry and release that lock on normal or exceptional method exit.  The ControlGraph
// should already have been generated for this method.
//
void BytecodeTranslator::addSynchronization(VariableOrConstant &syncHolder, Uint32 bci) const
{
	ControlGraph &cg = controlGraph;
	ControlNode &enterNode = addEnterNode(cg, syncHolder);
	addNormalExitNode(cg, syncHolder);
	addExceptionalExitNode(cg, syncHolder, enterNode, bci);
}


//
// Create a ControlGraph with data flow nodes out of this BytecodeGraph.
// Return the newly allocated ControlGraph or nil if an illegal bytecode was found.
// The BytecodeGraph must be complete and depthFirstSearch must have been called
// before calling this function; the usual way to do these is to call divideIntoBlocks.
//
ControlGraph *BytecodeTranslator::genPrimitiveGraph(BytecodeGraph &bytecodeGraph, Pool &primitivePool, Pool &tempPool)
{
	ControlGraph &cg = *new(primitivePool)
		ControlGraph(primitivePool, bytecodeGraph.nArguments, bytecodeGraph.argumentKinds, bytecodeGraph.isInstanceMethod, 1 /***** FIX ME *****/);

	BytecodeTranslator translator(bytecodeGraph, cg, tempPool);
	BasicBlock **block = bytecodeGraph.getDFSList();
	BasicBlock **dfsListEnd = block + bytecodeGraph.getDFSListLength();
	while (block != dfsListEnd)
		(*block++)->initTranslation(translator);

	BytecodeBlock *beginBlock = bytecodeGraph.beginBlock;
	if (beginBlock) {
		// Set up the locals and arguments
		TranslationEnv env(translator);

		env.init();
		ControlNode::BeginExtra &beginExtra = cg.getBeginNode().getBeginExtra();
		env.memory().define(beginExtra.initialMemory);
		uint nArgs = bytecodeGraph.nArguments;
		uint slotOffset = 0;
		const ValueKind *ak = bytecodeGraph.argumentKinds;
		for (uint n = 0; n != nArgs; n++) {
			env.local(slotOffset++).define(beginExtra[n]);
			if (isDoublewordKind(*ak++))
				env.local(slotOffset++).defineSecondWord();
		}
		translator.linkPredecessor(*beginBlock, cg.getBeginNode().getNormalSuccessor(), env, true);
		translator.finishedOnePredecessor(*beginBlock);
	}

	translator.genRawPrimitiveGraph();
	if (bytecodeGraph.isSynchronized) {
		VariableOrConstant sync;
		if (bytecodeGraph.isInstanceMethod)
			sync.setVariable(cg.getBeginNode().getBeginExtra()[0]);
		else
			sync.setConstant(objectAddress(bytecodeGraph.classFileSummary.getThisClass()));
		// asharma fix me!
		translator.addSynchronization(sync, 0);
	}
	return &cg;
}

