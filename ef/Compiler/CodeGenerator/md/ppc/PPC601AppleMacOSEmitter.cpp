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
//	PPC601AppleMacOSEmitter.cp
//
//	Scott M. Silver
//	Peter Desantis
// 	Laurent Morichetti
//	
//	Subclass of InstructionEmitter which handles emitting for the ppc601


#include "PPC601AppleMacOSEmitter.h"
#include "PPC601AppleMacOS_Support.h"
#include "Instruction.h"
#include "ppc601-macos.nad.burg.h"
#include "ControlNodes.h"
#include "SysCalls.h"
#include "AssembledInstructions.h"
#include "PPCInstructions.h"
#include "PPCCalls.h"

// Use the PowerPC disassembler that comes with the mac if possible
#if defined(DEBUG) && defined(__MWERKS__)
#include <ConditionalMacros.h>
#if defined(GENERATINGPOWERPC) && GENERATINGPOWERPC
#define USING_MAC_PPC_DISASSEMBLER
#ifdef __TYPES__	// save old types
	#define __OLD_TYPES__
#endif
#define __TYPES__
#include "Disassembler.h"
#define MAC_PPC_DISASSEMBLER_OPTIONS (Disassemble_PowerPC32|Disassemble_RsvBitsErr|Disassemble_FieldErr|Disassemble_Extended|Disassemble_BasicComm|Disassemble_CRBits|Disassemble_CRFltBits|Disassemble_BranchBO|Disassemble_TrapTO)
#ifndef __OLD_TYPES__
	#undef __TYPES__
#endif
#endif
#endif


const Uint8	kR3Color = 0;		// for return values <= 32 bits
const Uint8	kR4Color = 1;		// for return values > 32 bits

const Uint8 kFR1Color = 0;		// for return values <= 64 bits

const Uint8	kJitGlobalsColor = 29;	// used for global values
const Uint8	kJitGlobalsRegister = 13;
const Uint8	kStackRegister = 13;



#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊEmitter ¥
#endif


#define BINARY_PRIMITIVE_IX(inRuleKind, inOp) \
	case inRuleKind: 	\
		genArith_IX(inPrimitive, inOp);	\
		break;
		
#define BINARY_PRIMITIVE_ID(inRuleKind, inOp, inOpX) \
	case inRuleKind: 	\
		genArith_ID(inPrimitive, inOp, inOpX);	\
		break;

#define UNDEFINED_RULE(inRuleKind) \
	case inRuleKind: 	\
		assert(false);	\
		break;

#define BINARY_PRIMITIVE_SET(inRuleRoot, inOpX, inOpD) \
	BINARY_PRIMITIVE_IX(inRuleRoot##_I, inOpX);	
	
void PPCEmitter::
emitPrimitive(Primitive& inPrimitive, NamedRule inRule)
{		
	switch (inRule)
	{
	    case emConst_I:
	    case emConst_A:
			emit_Const_I(inPrimitive);
			break;
	    case emConst_F:
			emit_Const_F(inPrimitive);
	    	break;
		case emResult_I: case emResult_A:
			emit_Result_I(inPrimitive);
			break;
		case emResult_F:
			emit_Result_F(inPrimitive);
			break;
		case emIfLt: case emIfULt:
			genBranch(inPrimitive, condLt);
			break;
		case emIfEq: case emIfUEq:
			genBranch(inPrimitive, condEq);
			break;
		case emIfLe: case emIfULe: 
			genBranch(inPrimitive, condLe);
			break;
		case emIfGt: case emIfUGt:
			genBranch(inPrimitive, condGt);
			break;
		case emIfLgt: case emIfNe: 
			genBranch(inPrimitive, condLgt);
			break;
		case emIfGe: case emIfUGe:       	
			genBranch(inPrimitive, condGe);
			break;
		BINARY_PRIMITIVE_SET(emAnd, xfAnd, dfAndi);
		BINARY_PRIMITIVE_SET(emOr, xfOr, dfOri);
		BINARY_PRIMITIVE_SET(emXor, xfXor, dfXori);
		BINARY_PRIMITIVE_SET(emAdd, xfAdd, dfAddi);
		case emAdd_A: case emAddU_A:
			genArith_IX(inPrimitive, xfAdd);
			break;
//		case emAddI_A:
//			genArith_ID(inPrimitive, dfAddi, xfAdd);
//			break;
//		case emAddR_A:
//			genArith_RD(inPrimitive, dfAddi, xfAdd, false);
//			break;
//		case emAddRU_A:
//			genArith_RD(inPrimitive, dfAddi, xfAdd, true);
//			break;
		case emSub_I:
			genArithReversed_IX(inPrimitive, xfSubf);
			break;
///		case emSubR_I:
//		emit_SubR(inPrimitive, false);
//			break;			
//		case emSubAI_I:
//			emit_SubI(inPrimitive);
//			break;
///		case emSubAR_I:
//		emit_SubR(inPrimitive, false);
//			break;
//		case emSub_A: case emSubU_A:
//			genArithReversed_IX(inPrimitive, xfSubf);
//			break;
//		case emSubR_A:
//			emit_SubR(inPrimitive, false);
//			break;
//		case emSubUR_A:
//			emit_SubR(inPrimitive, true);
//			break;
		BINARY_PRIMITIVE_SET(emMul, xfMullw, dfMulli);		
		case emDiv_I:   	
			emit_Div_I(inPrimitive);
			break;
		case emDivE_I:   	
			emit_DivE_I(inPrimitive);
			break;
//		case emDivI_I:  	
//			emit_DivI_I(inPrimitive);
//			break;
//		case emDivR_I:  	
//			emit_DivR_I(inPrimitive);
//			break;
//		case emDivRE_I:    	
//			emit_DivRE_I(inPrimitive);
//			break;
		case emDivU_I:  	
			emit_DivU_I(inPrimitive);
			break;
		case emDivUE_I:   	
			emit_DivUE_I(inPrimitive);
			break;
//		case emDivUI_I:     	
//			emit_DivUI_I(inPrimitive);
//			break;
//		case emDivUR_I:     	
//			emit_DivUR_I(inPrimitive);
//			break;
//		case emDivURE_I:    	
//			emit_DivURE_I(inPrimitive);
//			break;
		case emModE_I:
			emit_ModE_I(inPrimitive);
			break;
		case emShl_I:
			{
				Instruction& shiftLeft = *new(mPool) XFormXY(&inPrimitive, mPool, 2, 1, xfSlw, pfNil);
				shiftLeft.standardUseDefine(*this);
			}
			break;
//		case emShlI_I:
//			emit_ShlI_I(inPrimitive);
//			break;
//
		case emChkNull:
			{
				TrapD&	newInsn = *new(mPool) TrapD(&inPrimitive, mPool, condEq, false, 0);
			
				newInsn.standardUseDefine(*this);
			}
			break;
//		case emLimitR:
//			emit_LimitR(inPrimitive);
//			break;
		case emLimit:
			emit_Limit(inPrimitive, 0);
			break;
		case emLd_IRegisterIndirect:	case emLd_ARegisterIndirect:
			emit_Ld_IRegisterIndirect(inPrimitive);
			break;
		case emLd_I: case emLd_A:
			{
				LdD_&	newInsn = *new(mPool) LdD_(&inPrimitive, mPool, dfLwz, 0);

				newInsn.standardUseDefine(*this);
			}
			break;	
		case emSt_I:
			{
				StD& newInsn = *new(mPool) StD(&inPrimitive, mPool, dfStw, 0); 
				newInsn.standardUseDefine(*this);
			}	
			break;
//		case emStI_I:
//			emit_StI_I(inPrimitive);
//			break;
//		case emStI_IRegisterIndirect:
//			emit_StI_IRegisterIndirect(inPrimitive);
//			break;
		case emSt_IRegisterIndirect:
			emit_St_IRegisterIndirect(inPrimitive);
			break;
		case emDynamicCall:
			{
				CallD_& newInsn = *new CallD_(&inPrimitive, mPool, CallD_::numberOfArguments(inPrimitive), CallD_::hasReturnValue(inPrimitive), *this);
			}
			break;
		case emStaticCall:
			{
				Call_& newInsn = *new Call_(&inPrimitive, mPool, Call_::numberOfArguments(inPrimitive), Call_::hasReturnValue(inPrimitive), *this);
			}
			break;
		case emSysCallEV:
			{
				CallS_V& newInsn = *new CallS_V(&inPrimitive, mPool, CallS_V::numberOfArguments(inPrimitive), CallS_V::hasReturnValue(inPrimitive), *this, PrimSysCall::cast(inPrimitive).sysCall.function);
			}
			break;
		case emSysCallEC:
			{
				CallS_C& newInsn = *new CallS_C(&inPrimitive, mPool, CallS_C::numberOfArguments(inPrimitive), CallS_C::hasReturnValue(inPrimitive), *this, PrimSysCall::cast(inPrimitive).sysCall.function);
			}
			break;
		case emSysCallE:
			{
				CallS_& newInsn = *new CallS_(&inPrimitive, mPool, CallS_::numberOfArguments(inPrimitive), CallS_::hasReturnValue(inPrimitive), *this, PrimSysCall::cast(inPrimitive).sysCall.function);
			}
			break;
		case emCmp_I:
			emit_Cmp_I(inPrimitive);
			break;
//		case emCmpI_I:
//			emit_CmpI_I(inPrimitive);
//			break;
		case emFAdd_F:
			{
				Instruction& insn = sAFormInfos[afFadd].creatorFunction(inPrimitive, mPool, afFadd);
				useProducer(inPrimitive.nthInputVariable(0), insn, 0, vrcFloatingPoint);
				useProducer(inPrimitive.nthInputVariable(1), insn, 1, vrcFloatingPoint);
				defineProducer(inPrimitive, insn, 0, vrcFloatingPoint);
			}
			break;
//		case emFAddI_F:
//			{
//				VirtualRegister&	contantVR = genLoadConstant_F(inPrimitive, inPrimitive.nthInputConstant(1).f);
//				Instruction& insn = sAFormInfos[afFadd].creatorFunction(inPrimitive, mPool, afFadd);
//				useProducer(inPrimitive.nthInputVariable(0), insn, 0, vrcFloatingPoint);
//				useTemporaryVR(insn, contantVR, 1, vrcFloatingPoint);
	//			defineProducer(inPrimitive, insn, 0, vrcFloatingPoint);
	//		}
//			break;
		default:
			//assert(false)
			;
	}
}


void PPCEmitter::
emit_Const_I(Primitive& inPrimitive)
{
	Int32	constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.i;
	
	// can't use genLoadConstant_I here because we need to assign the VR to this producer
	// (and this producer could already have a VR assigned to it)
	if ((constant <= 32767) && (constant >= -32768))
	{
		ArithIZeroInputD&	lo16 = *new(mPool) ArithIZeroInputD(&inPrimitive, mPool, dfAddi, (Uint16) constant);
		defineProducer(inPrimitive, lo16, 0);	
	}
	else
	{
		// if we need to load a constant larger that 16 bits, then we use an addis, ori combo
		// we need to make the input of the the ori depend on the output of the addis so that
		// those instructions come in uninterrupted pairs (because we don't support defining half 
		// of a VirtualRegister)
		ArithIZeroInputD&	hi16 = *new(mPool) ArithIZeroInputD(&inPrimitive, mPool, dfAddis, (Uint16)((Uint32) constant >> 16));
		VirtualRegister& 	constantVr = *defineProducer(inPrimitive, hi16, 0);

		LogicalID&	lo16 = *new(mPool) LogicalID(&inPrimitive, mPool, dfOri, (Uint16)((Uint32) constant));
		useTemporaryVR(lo16, constantVr, 0);
		redefineTemporary(lo16, constantVr, 0);
	}
}

void PPCEmitter::
emit_Const_F(Primitive& inPrimitive) 
{
	Flt32	constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.f;
	
	Uint16	offset;	
	mAccumulatorTOC.addData(&constant, sizeof(constant), offset);
	
	Instruction&	ldConstant = *new(mPool) LdD_RTOC(&inPrimitive, mPool, dfLfs, offset);
	defineProducer(inPrimitive, ldConstant, 0, vrcFloatingPoint);		
}

void PPCEmitter::
emit_StI_I(Primitive& inPrimitive)
{
 	VirtualRegister&	constantReg = genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(2).i);
	StD& 				storeInsn = *new(mPool) StD(&inPrimitive, mPool, dfStw, 0); 

	useTemporaryVR(storeInsn, constantReg, 1);
	useProducer(inPrimitive.nthInputVariable(0), storeInsn, 0);
	trespass("Not implemented yet");
	//defineProducer(inPrimitive, storeInsn, 0);
	//defineProducer(nthOutputProducer(inPrimitive, 1), storeInsn, 0);
}
 
void PPCEmitter::
emit_St_IRegisterIndirect(Primitive& inPrimitive)
{
	Int16 offset;
	
	// Need to get immediate arguement from producer's primitive
	DataNode* addrSource = &inPrimitive.nthInput(1).getVariable();

	if(!extractS16(PrimConst::cast(addrSource->nthInputVariable(1)).value, offset))
		assert(false);
		
	StD& storeInsn = *new(mPool) StD(&inPrimitive, mPool, dfStw, offset); 
	useProducer(inPrimitive.nthInputVariable(0), storeInsn, 0);		// <- store
	useProducer(addrSource->nthInputVariable(0), storeInsn, 1);		// <- address base
	useProducer(inPrimitive.nthInputVariable(2), storeInsn, 2);		// <- value 
	defineProducer(inPrimitive, storeInsn, 0); 	// -> store
}


void PPCEmitter::
emit_ShlI_I(Primitive& inPrimitive)
{
	uint8 shiftBy;

	extractU8(inPrimitive.nthInputConstant(1), shiftBy);
	
	MFormInAOnly& newInsn = *new(mPool) MFormInAOnly(&inPrimitive, mPool, mfRlwinm, shiftBy, 0, 31 - shiftBy, pfNil);	// slwi ra, rs, shiftBy
	newInsn.standardUseDefine(*this);
}


void PPCEmitter::
emit_Div_I(Primitive& inPrimitive)
{
	ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivw, pfNil);
		
	useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
	useProducer(inPrimitive.nthInputVariable(1), divInsn, 1);
	defineProducer(inPrimitive, divInsn, 0);
}

void PPCEmitter::
emit_DivE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);	
	emit_Div_I(inPrimitive);
}


void PPCEmitter::
emit_DivI_I(Primitive& inPrimitive)
{
	Value& immediate = inPrimitive.nthInputConstant(1);
	
	if(isPowerOfTwo(immediate.i))
	{
		uint8 shiftBy = 31 - leadingZeros(immediate.i);
		
		XInAOnly&	shiftRight = *new(mPool) XInAOnly(&inPrimitive, mPool, xfSrawi, pfNil, shiftBy);			
		shiftRight.standardUseDefine(*this);
	} 
	else 
	{	
		VirtualRegister&	divisor = genLoadConstant_I(inPrimitive, immediate.i);		
		ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivw, pfNil);
		
		useTemporaryVR(divInsn, divisor, 1);
		useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
		defineProducer(inPrimitive, divInsn, 0);
	}
}

void PPCEmitter::
emit_DivR_I(Primitive& inPrimitive)
{
	Value& immediate = inPrimitive.nthInputConstant(0);		// remember it's reversed
	
	if(isPowerOfTwo(immediate.i))
	{
		uint8 shiftBy = 31 - leadingZeros(immediate.i);
		
		XInAOnly&	shiftRight = *new(mPool) XInAOnly(&inPrimitive, mPool, xfSrawi, pfNil, shiftBy);			
		useProducer(inPrimitive.nthInputVariable(1), shiftRight, 0);
		defineProducer(inPrimitive, shiftRight, 0);
	} 
	else 
	{	
		VirtualRegister&	divisor = genLoadConstant_I(inPrimitive, immediate.i);		
		ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivw, pfNil);
		
		useTemporaryVR(divInsn, divisor, 1);
		useProducer(inPrimitive.nthInputVariable(1), divInsn, 0);
		defineProducer(inPrimitive, divInsn, 0);
	}
}


void PPCEmitter::
emit_DivRE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);
	emit_DivR_I(inPrimitive);
}

void PPCEmitter::
emit_DivU_I(Primitive& inPrimitive)
{
	ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivwu, pfNil);
		
	useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
	useProducer(inPrimitive.nthInputVariable(1), divInsn, 0);
	defineProducer(inPrimitive, divInsn, 0);
}

void PPCEmitter::
emit_DivUE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);

	emit_DivU_I(inPrimitive);	
}

void PPCEmitter::
emit_DivUI_I(Primitive& inPrimitive)
{
	Value& immediate = inPrimitive.nthInputConstant(1);
	
	if(isPowerOfTwo(immediate.i))
	{
		Uint8 shiftBy = 31 - leadingZeros(immediate.i);
		
		MFormInAOnly&	shiftRight = *new(mPool) MFormInAOnly(&inPrimitive, mPool, mfRlwinm, 32-shiftBy, shiftBy, 31, pfNil);	// srwi ra, rs, shiftBy
		shiftRight.standardUseDefine(*this);
	} 
	else 
	{	
		VirtualRegister&	divisor = genLoadConstant_I(inPrimitive, immediate.i);		
		ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivwu, pfNil);
		
		useTemporaryVR(divInsn, divisor, 1);
		useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
		defineProducer(inPrimitive, divInsn, 0);
	}
}

void PPCEmitter::
emit_DivUR_I(Primitive& inPrimitive)
{
	Value& immediate = inPrimitive.nthInputConstant(0);
	
	if(isPowerOfTwo(immediate.i))
	{
		uint8 shiftBy = 31 - leadingZeros(immediate.i);
		
		MFormInAOnly&	shiftRight = *new(mPool) MFormInAOnly(&inPrimitive, mPool, mfRlwinm, 32-shiftBy, shiftBy, 31, pfNil);	// srwi ra, rs, shiftBy
		useProducer(inPrimitive.nthInputVariable(1), shiftRight, 0);
		defineProducer(inPrimitive.nthInputVariable(0), shiftRight, 0);
	} 
	else 
	{	
		VirtualRegister&	divisor = genLoadConstant_I(inPrimitive, immediate.i);		
		ArithX&				divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivwu, pfNil);
		
		useTemporaryVR(divInsn, divisor, 1);
		useProducer(inPrimitive.nthInputVariable(1), divInsn, 0);
		defineProducer(inPrimitive, divInsn, 0);
	}
}

void PPCEmitter::
emit_DivURE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);

	emit_DivURE_I(inPrimitive);
}


void PPCEmitter::
emit_Mod_I(Primitive& inPrimitive)
{
	//	divw	rt, ta, rb		# rt = (ra-R)/rb
	//	mullw	rt, rt, rb		# rt = ra-R
	//	subf	rt, rt, rta		# rt = R
	ArithX&	divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivw, pfNil);
	useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
	useProducer(inPrimitive.nthInputVariable(1), divInsn, 1);
	VirtualRegister& tempVr = defineTemporary(divInsn, 0);

	ArithX& mullInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfMullw, pfNil);
	useProducer(inPrimitive.nthInputVariable(1), mullInsn, 1);
	useTemporaryVR(mullInsn, tempVr, 0);
	redefineTemporary(mullInsn, tempVr, 0);
	
	ArithX& subInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfSubf, pfNil);
	useTemporaryVR(subInsn, tempVr, 0);
	useProducer(inPrimitive.nthInputVariable(0), subInsn, 1);
	defineProducer(inPrimitive, subInsn, 0);	
}

void PPCEmitter::			
emit_ModE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);
	emit_Mod_I(inPrimitive);
}
#if 0

void PPCEmitter::
emit_ModI_I(Primitive& inPrimitive)
{
	genMod_I(inPrimitive);
}

void PPCEmitter::
emit_ModR_I(Primitive& inPrimitive)
{
	genMod_I(inPrimitive);
}

void PPCEmitter::
emit_ModRE_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);
	genMod_I(inPrimitive);
}

void PPCEmitter::
emit_ModU_I(Primitive& inPrimitive)
{
	genThrowIfDivisorZero(inPrimitive);
	genMod_I(inPrimitive);
}

//ta = dividiend
//rb = divisor

void PPCEmitter::
genMod_I(Primitive& inPrimitive)
{
	// FIX-ME? Could reduce register pressure here by 
	// folding the constant in the mullw to kill rb one instruction earlier
	//	divw	rt, ta, rb		# rt = (ra-R)/rb
	//	mullw	rt, rt, rb		# rt = ra-R
	//	subf	rt, rt, rta		# rt = R
	ArithX&	divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivw, pfNil);
	VirtualRegister*	dividendVr;
	VirtualRegister*	divisorVr;
	
	if (inPrimitive.nthInputIsConstant(0))
		dividendVr = &genLoadConstant_I(inPrimitive.nthInputConstant(0).i)
	else
		dividendVr = useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
		
	if (inPrimitive.nthInputIsConstant(1))
		divisorVr = &genLoadConstant_I(inPrimitive.nthInputConstant(1).i)
	else
		divisorVr = useProducer(inPrimitive.nthInputVariable(1), divInsn, 1);
	
	VirtualRegister& tempVr = defineTemporary(divInsn, 0);

	ArithX& mullInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfMullw, pfNil);
	useTemporaryVR(mullInsn, *divisorVr, 1);
	useTemporaryVR(mullInsn, tempVr, 0);
	redefineTemporary(mullInsn, tempVr, 0);
	
	ArithX& subInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfSubf, pfNil);
	useTemporaryVR(subInsn, tempVr, 0);
	useTemporaryVR(subInsn, *dividendVr, 1);
	defineProducer(inPrimitive, subInsn, 0);	
}

void PPCEmitter::
genModU_I(Primitive& inPrimitive)
{
	// FIX-ME? Could reduce register pressure here by 
	// folding the constant in the mullw to kill rb one instruction earlier
	//	divw	rt, ta, rb		# rt = (ra-R)/rb
	//	mullw	rt, rt, rb		# rt = ra-R
	//	subf	rt, rt, rta		# rt = R
	ArithX&	divInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfDivwu, pfNil);
	VirtualRegister*	dividendVr;
	VirtualRegister*	divisorVr;
	
	if (inPrimitive.nthInputIsConstant(0))
		dividendVr = &genLoadConstant_I(inPrimitive.nthInputConstant(0).i)
	else
		dividendVr = useProducer(inPrimitive.nthInputVariable(0), divInsn, 0);
		
	if (inPrimitive.nthInputIsConstant(1))
		divisorVr = &genLoadConstant_I(inPrimitive.nthInputConstant(1).i)
	else
		divisorVr = useProducer(inPrimitive.nthInputVariable(1), divInsn, 1);
	
	VirtualRegister& tempVr = defineTemporary(divInsn, 0);

	ArithX& mullInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfMullw, pfNil);
	useTemporaryVR(mullInsn, *divisorVr, 1);
	useTemporaryVR(mullInsn, tempVr, 0);
	redefineTemporary(mullInsn, tempVr, 0);
	
	ArithX& subInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfSubf, pfNil);
	useTemporaryVR(subInsn, tempVr, 0);
	useTemporaryVR(subInsn, *dividendVr, 1);
	defineProducer(inPrimitive, subInsn, 0);	
}
#endif

void PPCEmitter::
emit_Ld_IRegisterIndirect(Primitive& inPrimitive)
{		
	Int16 constant;
	DataNode& immSource = inPrimitive.nthInput(1).getVariable(); // immediate arguement from producer's primitive

	if(extractS16(immSource.nthInputConstant(1), constant))
	{
		LdD_&	newInsn = *new(mPool) LdD_(&inPrimitive, mPool, dfLwz, constant);

		useProducer(inPrimitive.nthInputVariable(0), newInsn, 0);		// M to poLd_I
		useProducer(immSource.nthInputVariable(0), newInsn, 1);		// Vint to poAdd_A
		defineProducer(inPrimitive, newInsn, 0);	// Vint from poLd_I
	}
	else
	{
		assert(false);
//		emitPrimitive(immSource);
//		emitPrimitive(inPrimitive);
	}
}

void PPCEmitter::
emit_LimitR(Primitive& inPrimitive)
{
	Int16 constant;
	
	if (extractS16(inPrimitive.nthInputConstant(0), constant))
	{
		TrapD&	newInsn = *new(mPool) TrapD(&inPrimitive, mPool, condLe, false, constant);
		useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);
		inPrimitive.setInstructionRoot(&newInsn);
	}
	else
	{
		// fix-me handle immediate values larger than the use of a single instruction allows
		assert(false);
	}
}


void PPCEmitter::
emit_Limit(Primitive& inPrimitive, PPCInsnFlags inFlags)
{
	TrapX&	newInsn = *new(mPool) TrapX(&inPrimitive, mPool, condGe, false, inFlags);
	newInsn.standardUseDefine(*this);
}


void PPCEmitter::
emit_Cmp_I(Primitive& inPrimitive)
{
	CmpIX&	newInsn = *new(mPool) CmpIX(&inPrimitive, mPool, xfCmp, 0);
	newInsn.standardUseDefine(*this);
}


void PPCEmitter::
emit_CmpI_I(Primitive& inPrimitive)
{
	Int16 constant;
	
	if (extractS16(inPrimitive.nthInputConstant(1), constant))
	{
		CmpID&	newInsn = *new(mPool) CmpID(&inPrimitive, mPool, dfCmpi, constant);
		newInsn.standardUseDefine(*this);
	}
	else
	{
		VirtualRegister&	constantVr = genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(1).i);
		CmpIX&				newInsn = *new(mPool) CmpIX(&inPrimitive, mPool, xfCmp, 0);

		useProducer(inPrimitive.nthInputVariable(0), newInsn, 0);
		useTemporaryVR(newInsn, constantVr, 1);
		defineProducer(inPrimitive, newInsn, 0);
	}
}

void PPCEmitter::
emit_Result_I(Primitive& inPrimitive)
{
	VirtualRegister*	returnVr;	
						
	if (inPrimitive.nthInputIsConstant(0))
	{
		// if the result is a constant load it into a register
		returnVr = &genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(0).i);
	}
	else
	{
		// otherwise create a buffer copy instruction between the result 
		// and the precolored register
		Copy_I&				copyInsn = *new(mPool) Copy_I(&inPrimitive, mPool);
		
		returnVr = &defineTemporary(copyInsn, 0);
		useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0);
	}
	
	// integer results (and _A go in r3)
	returnVr->preColorRegister(kR3Color);
	
	// create a special instruction for the RegisterAllocator which says this result
	// is used elsewhere (but not in this body of code).
	InsnExternalUse& externalUse = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);
	useTemporaryVR(externalUse, *returnVr, 0);
	inPrimitive.setInstructionRoot(&externalUse);
}

void PPCEmitter::
emit_Result_F(Primitive& inPrimitive)
{
	VirtualRegister*	returnVr;	
						
	if (inPrimitive.nthInputIsConstant(0))
	{
		// if the result is a constant load it into a register
		returnVr = &genLoadConstant_F(inPrimitive, inPrimitive.nthInputConstant(0).f);
	}
	else
	{
		// otherwise create a buffer copy instruction between the result 
		// and the precolored register
		XInBOnly&				copyInsn = *new(mPool) XInBOnly(&inPrimitive, mPool, xfFmr, pfNil);
		
		useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0, vrcFloatingPoint);
		returnVr = &defineTemporary(copyInsn, 0, vrcFloatingPoint);
	}
	
	// integer results (and _A go in r3)
	returnVr->preColorRegister(1);
	
	// create a special instruction for the RegisterAllocator which says this result
	// is used elsewhere (but not in this body of code).
	InsnExternalUse& externalUse = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);
	useTemporaryVR(externalUse, *returnVr, 0, vrcFloatingPoint);
	inPrimitive.setInstructionRoot(&externalUse);
}


#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ Utilities ¥
#endif

void PPCEmitter::
genLoadIZero(Primitive& inPrimitive, PPCInsnFlags /*inFlags*/)
{
	LdD_&	newInsn = *new(mPool) LdD_(&inPrimitive, mPool, dfLwz, 0);

	newInsn.standardUseDefine(*this);
}


void PPCEmitter::
genBranch(Primitive& inPrimitive, Condition2 cond)
{
	ControlNode& controlIf = *inPrimitive.getContainer();
	BranchCB&	newInsn = *new(mPool) BranchCB(&inPrimitive, mPool, controlIf.getTrueSuccessor().getTarget(), cond, false, false);

	newInsn.standardUseDefine(*this);
}

// need to do an add
// with a negated constant
void PPCEmitter::
emit_SubI(Primitive& inPrimitive)
{
	Int16 constant;
	
	// perform an addi with -constant if we're in range
	if (extractS16(inPrimitive.nthInputConstant(1), constant))
	{
		ArithID&	newInsn = *new(mPool) ArithID(&inPrimitive, mPool, dfAddi, -constant);

		newInsn.standardUseDefine(*this);
	}
	else
	{
		// otherwise load the constant, and do a subf
		VirtualRegister&	constantReg = genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(1).i);
		
		ArithX&	opInsn = *new(mPool) ArithX(&inPrimitive, mPool, xfSubf, pfNil);
		
		useTemporaryVR(opInsn, constantReg, 1);
		useProducer(inPrimitive.nthInputVariable(0), opInsn, 0);
		defineProducer(inPrimitive, opInsn, 0);
	}
}

void PPCEmitter::
emit_SubR(Primitive& inPrimitive, bool inUnsigned)
{
	// first negate the Vint edge
	Instruction&	negate = *new(mPool) XInAOnly(&inPrimitive, mPool, xfNeg, pfNil, 0);
	useProducer(inPrimitive.nthInputVariable(1), negate, 0);
	defineProducer(inPrimitive, negate, 0);
	
	// then perform the appropriate add
	genArith_RD(inPrimitive, dfAddi, xfAdd, inUnsigned);
}

void PPCEmitter::
genArith_ID(Primitive& inPrimitive, DFormKind inKind, XFormKind inXKind)
{
	Int16 constant;
	
	if (extractS16(inPrimitive.nthInputConstant(1), constant))
	{
		ArithID&	newInsn = *new(mPool) ArithID(&inPrimitive, mPool, inKind, constant);

		newInsn.standardUseDefine(*this);
	}
	else
	{
		VirtualRegister&	constantReg = genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(1).i);
		
		ArithX&	opInsn = *new(mPool) ArithX(&inPrimitive, mPool, inXKind, pfNil);
		
		useTemporaryVR(opInsn, constantReg, 0);
		useProducer(inPrimitive.nthInputVariable(0), opInsn, 1);
		defineProducer(inPrimitive, opInsn, 0);
	}
}

// this is when the constant is in the first position,  not in the second
void PPCEmitter::
genArith_RD(Primitive& inPrimitive, DFormKind inKind, XFormKind inXKind, bool inIsUnsigned)
{
	Int16 constant;
	
	// use a normal D form Arithmetic if the value fits in a signed word, or
	// it's unsigned and the constant is > 0 if it were treated as signed
	if (extractS16(inPrimitive.nthInputConstant(0), constant) && ((!inIsUnsigned) || (inIsUnsigned && constant >= 0)))
	{
		ArithID&	newInsn = *new(mPool) ArithID(&inPrimitive, mPool, inKind, constant);
		
		useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);
		defineProducer(inPrimitive, newInsn, 0);
	}
	else
	{
		VirtualRegister&	constantReg = genLoadConstant_I(inPrimitive, inPrimitive.nthInputConstant(0).i);
		
		ArithX&	opInsn = *new(mPool) ArithX(&inPrimitive, mPool, inXKind, pfNil);
		
		useTemporaryVR(opInsn, constantReg, 0);
		useProducer(inPrimitive.nthInputVariable(1), opInsn, 1);
		defineProducer(inPrimitive, opInsn, 0);
	}
}



void PPCEmitter::
genArith_IX(Primitive& inPrimitive, XFormKind inKind)
{
	ArithX&	newInsn = *new(mPool) ArithX(&inPrimitive, mPool, inKind, pfNil);
	
	newInsn.standardUseDefine(*this);
}

// this is for reversed operands in the native instruction
void PPCEmitter::
genArithReversed_IX(Primitive& inPrimitive, XFormKind inKind)
{
	ArithX&	newInsn = *new(mPool) ArithX(&inPrimitive, mPool, inKind, pfNil);

	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);
	defineProducer(inPrimitive, newInsn, 0);
}


VirtualRegister& PPCEmitter::
genLoadConstant_I(DataNode& inPrimitive, Int32 inConstant)
{
	VirtualRegister*	vr;
	
	if ((inConstant <= 32767) && (inConstant >= -32768))
	{
		ArithIZeroInputD&	lo16 = *new(mPool) ArithIZeroInputD(&inPrimitive, mPool, dfAddi, inConstant);
		vr = &defineTemporary(lo16, 0);		
	}
	else
	{
		// if we need to load a constant larger that 16 bits, then we use an addis, ori combo
		ArithIZeroInputD&	hi16 = *new(mPool) ArithIZeroInputD(&inPrimitive, mPool, dfAddis, (Uint16)(((Uint32) inConstant) >> 16));
		VirtualRegister& 	constantVr = defineTemporary(hi16, 0);

		LogicalID&	lo16 = *new(mPool) LogicalID(&inPrimitive, mPool, dfOri, (Uint16)((Uint32) inConstant));
		useTemporaryVR(lo16, constantVr, 0);
		redefineTemporary(lo16, constantVr, 0);
		
		vr = &constantVr;
	}

	return (*vr);
}


VirtualRegister& PPCEmitter::
genLoadConstant_F(DataNode& inPrimitive, Flt32 inConstant)
{
	Uint16 offset;
	void *d = mAccumulatorTOC.addData(&inConstant, sizeof(inConstant), offset);
	assert(d);
	
	Instruction&	ldConstant = *new(mPool) LdD_RTOC(&inPrimitive, mPool, dfLfs, offset);
	return (defineTemporary(ldConstant, 0, vrcFloatingPoint));		
}

void PPCEmitter::
genThrowIfDivisorZero(Primitive& inPrimitive)
{
	// Check the divisor != 0
	TrapD&	trapInsn = *new(mPool) TrapD(&inPrimitive, mPool, condEq, false, 0);
	useProducer(inPrimitive.nthInputVariable(1), trapInsn, 0);
	inPrimitive.setInstructionRoot(&trapInsn);
}

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊFormatting Support ¥
#endif
	
Instruction& PPCEmitter::
emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget)
{	
	BranchI&	newInsn = *new(mPool) BranchI(&inDataNode, mPool, inTarget, false, false);
	
	return (newInsn);
}

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊRegisterAllocator Support ¥
#endif

bool PPCEmitter::
emitCopyAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr)
{
	ArithID&	newInsn = *new(mPool) ArithID(&inDataNode, mPool, dfAddi, 0);
	
	newInsn.addUse(0, fromVr);
	newInsn.addDefine(0, toVr);
#ifdef DEBUG
	toVr.setDefiningInstruction(newInsn);
#endif
	newInsn.insertAfter(*where);
	return false;
}

void PPCEmitter::
emitStoreAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& storedReg, VirtualRegister& stackReg)
{
	StD_SpillRegister& newInsn = *new(mPool) StD_SpillRegister(&inDataNode, mPool, stackReg, storedReg);

	newInsn.insertAfter(*where);
}

void PPCEmitter::
emitLoadAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& loadedReg, VirtualRegister& stackReg)
{
	LdD_SpillRegister& newInsn = *new(mPool) LdD_SpillRegister(&inDataNode, mPool, stackReg, loadedReg);
#ifdef DEBUG
	loadedReg.setDefiningInstruction(newInsn);
#endif
	newInsn.insertAfter(*where);
}

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊDebug Support ¥
#endif

#ifdef DEBUG
void* 
disassemble1(LogModuleObject &f, void* inFrom)
{
#ifdef USING_MAC_PPC_DISASSEMBLER
	char mnemonic[100];
	char operand[100];
	char comment[100];
	
	ppcDisassembler((unsigned long *)inFrom, 0, MAC_PPC_DISASSEMBLER_OPTIONS, mnemonic, operand, comment, NULL, NULL);		
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%p: %-10s %-40s %-40s\n", inFrom, mnemonic, operand, comment));
	
	return ((char*) inFrom + 4);
#else
	f;
	inFrom = NULL;
	return NULL;
#endif
}
#endif

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊLinkage Support ¥
#endif

void PPCEmitter::
emitArguments(ControlNode::BeginExtra& inBeginNode)
{
	Uint16			curParamWords;			// number of words of argument space used
	Uint16			curFloatingPointArg;	// number of floating point arguments

	if (inBeginNode.nArguments == 0)
		return;

	InsnExternalDefine& defineInsn = *new(mPool) InsnExternalDefine(&inBeginNode[0], mPool, inBeginNode.nArguments * 2);
	
	Uint8 curArgIdx;	// current index into the arguments in ControlNode::BeginExtra
	
	curParamWords = 0;
	curFloatingPointArg = 1;
	for (curArgIdx = 0; curArgIdx < inBeginNode.nArguments && curParamWords < 8; curArgIdx++)
	{
		PrimArg&	curArg = inBeginNode[curArgIdx];

		switch (curArg.getOperation())
		{
			case poArg_I:
			case poArg_A:
			{
				if (curArg.hasConsumers())
				{
					Copy_I& 	copyInsn =  *new(mPool) Copy_I(&curArg, mPool);

					// hook up this vr to the external define FIX-ME
					VirtualRegister&	inputVr = defineTemporary(defineInsn, curArgIdx);
					inputVr.preColorRegister(curParamWords);

					// emit the copy
					useTemporaryVR(copyInsn, inputVr, 0);
					defineProducer(curArg, copyInsn, 0);
				}	
				curParamWords++;
				break;
			}
			case poArg_L:
			{
				assert(false);
				if (curParamWords < 6)
				{
					Copy_I& 	copyInsn = *new(mPool) Copy_I(&curArg, mPool);
					
					// hook up this vr to the external define FIX-ME
					VirtualRegister&	inputVr = defineTemporary(defineInsn, curArgIdx);

					useTemporaryVR(copyInsn, inputVr, 0);
					defineProducer(curArg, copyInsn, 0);
					
					//inputVr.preColorRegister(curParamWords);
					//inputVr.preColorRegister(curParamWords);
				}
				else if (curParamWords < 7)
				{
					// make the external define create this argument
					VirtualRegister&	inputVr = defineTemporary(defineInsn, curArgIdx);	// specify 32bit fixed point register
					inputVr.preColorRegister(6);
					
					// copy the hiword argument in r10 (color=6) to a new vr
					Copy_I& 	copyInsn = *new(mPool) Copy_I(&curArg, mPool);
					useTemporaryVR(copyInsn, inputVr, 0);		
					
					VirtualRegister& copyVr64 = mVRAllocator.newVirtualRegister();		// FIX-ME want to specify a "64" bit vr
					defineProducerWithExistingVirtualRegister(copyVr64, curArg, copyInsn, 0);
					
					// copy the loword argument from the appropriate stackslot to a new vr
					LdD_FixedSource<kStackRegister>&		loadParam = *new(mPool) LdD_FixedSource<kStackRegister>(&curArg, mPool, dfLwz, 24 + 32);
					
					// fix-me add use or change to special stack relative load
					defineProducer(curArg, loadParam, 0);	// specify loword of 32 bit register
				}
				curParamWords+=2;
				break;
			}
			case poArg_F:
			{
				XInBOnly&				copyInsn = *new(mPool) XInBOnly(&curArg, mPool, xfFmr, pfNil);
				
				// emit the copy
				// hook up this vr to the external define
				VirtualRegister&	inputVr = defineTemporary(defineInsn, curArgIdx, vrcFloatingPoint);
				inputVr.preColorRegister(curFloatingPointArg);
				
				useTemporaryVR(copyInsn, inputVr, 0, vrcFloatingPoint);
				defineProducer(curArg, copyInsn, 0, vrcFloatingPoint);
	
				curParamWords++;
				curFloatingPointArg++;
				break;
			}
			case poArg_D:
				assert(false);
				break;
				
			case poArg_M:
				break;

			default:
				assert(0);
		}
	}	

	// do appropriate loads for each argument
	Uint8 stackArgIdx;
	for (stackArgIdx = curArgIdx; stackArgIdx < inBeginNode.nArguments; stackArgIdx++)
	{
		PrimArg&	curArg = inBeginNode[stackArgIdx];

		switch (curArg.getOperation())
		{
			case poArg_I:
			case poArg_A:
			{
				if (curArg.hasConsumers())
				{
					LdD_FixedSource<kStackRegister>&	loadParam = *new(mPool) LdD_FixedSource<kStackRegister>(&curArg, mPool, dfLwz, 24 + curParamWords * 4);
				
					useTemporaryOrder(loadParam, defineTemporaryOrder(defineInsn, stackArgIdx), 0);
					defineProducer(curArg, loadParam, 0);	
				}
				curParamWords++;
				break;
			}
			case poArg_L:
				assert(false);
			case poArg_F:
			{
#if 0
				LoadSP&		loadParam = *new(mPool) LoadSP(&curArg, mPool, 24 + curParamWords * 4, dfLfs);
				
				defineProducer(curArg, loadParam, 0);	
#endif
				curParamWords++;
				break;
			}
			case poArg_D:
				assert(false);
			default:
				assert(false);
		}
	}
	
	// continue counting from before
	// make all the rest of the defines as type none
	for (;curArgIdx < inBeginNode.nArguments * 2; curArgIdx++)
		defineInsn.getInstructionDefineBegin()[curArgIdx].kind = udNone;
	
#if 0
	MacDebugger&	macDebugger = *new(mPool) MacDebugger(&inBeginNode.initialMemory, *this);
	inBeginNode.initialMemory.setInstructionRoot(&macDebugger);
#endif
}


const Uint32 upper16 = 0xffff0000;
const Uint32 lower16 = 0x0000ffff;


void PPCFormatter::
calculatePrologEpilog(Method& /*inMethod*/, Uint32& outPrologSize, Uint32& outEpilogSize)
{
	Uint8		usedNonVolatileGPR = 10; // mVRAllocator.nUsedCalleeSavedRegisters;
	Uint8		usedNonVolatileFPR = 0;
	Uint32		localsWords = 0;		// localSize_words
	const Uint32 kLinkAreaWords	= 2;
	Uint32		totalRegisterWords;
	
	assert(usedNonVolatileGPR <= 14);
	assert(usedNonVolatileFPR <= 13); 

	mSaveGPRwords = usedNonVolatileGPR;
	mSaveFPRwords = usedNonVolatileFPR * 2;	// 2 words per FPR

	// total amount of space needed to save non-volatile registers
	totalRegisterWords = mSaveGPRwords + mSaveFPRwords;
	if(totalRegisterWords < 8)
		totalRegisterWords = 8;

	// figure total size of frame
	mFrameSizeWords = mEmitter.mMaxArgWords + kLinkAreaWords + localsWords + totalRegisterWords;  
	mFrameSizeWords = (mFrameSizeWords + 3  & ~3);		// align to 4 byte boundary

	Uint32 prologSize_words;
	Uint32 epilogSize_words;
	
	// Number of instructions
	if((mFrameSizeWords * 4) > 32768)  // We need 3 instructions to create the stack frame
	{
		// Prolog needs 4 instruction to save CR and LR
		// and 1 instruction to save each FPR and GPR
		// and 3 instructions to create the stack frame
		prologSize_words = 7 + usedNonVolatileFPR + usedNonVolatileGPR;

		// Epilog needs 4 instruction to restore CR and LR
		// and 1 instruction to restore each FPR and GPR
		// and 3 instruction to restore the stack frame
		// and 1 instruction to return (branch to lr)
		epilogSize_words = prologSize_words + 1; 
	} 
	else 
	{                        
		// We need 1 instruction to create the stack frame
		// Prolog needs 4 instruction to save CR and LR
		// and 1 instruction to save each FPR and GPR
		// and 1 instruction to create the stack frame
		prologSize_words = 1 + 2 + /*2*/ + usedNonVolatileFPR + usedNonVolatileGPR;

		// Epilog needs 4 instruction to restore CR and LR
		// and 1 instruction to restore each FPR and GPR
		// and 1 instruction to restore the stack frame
		// and 1 instruction to return (branch to lr)
		epilogSize_words = prologSize_words + 1;  
	}    

	// now convert from words to bytes
	outPrologSize = prologSize_words * 4;
	outEpilogSize = epilogSize_words * 4;
}


void PPCFormatter::	
formatPrologToMemory(void* inWhere)
{
  Uint32* nextInstruction = (Uint32*)inWhere;

#if 0
  /* 
     Write instructions to save CR
     mfcr r0
     stw r0, 4(r1)
  */
  *nextInstruction++ = mfcrR0;
  *nextInstruction++ = stwR04R1;
#endif

  /*
    Write instructions to save LR
    mflr r0
    stw r0, 8(r1)
  */
  *nextInstruction++ = mflrR0; 
  *nextInstruction++ = stwR08R1;

  /*
    Save non-volatile FPRs
    non volatile floating point registers are in the range fr14 - fr31
    the saved block of non-volatile registers must start at fr31 and move up to fr 14
   
    for(i = 0; i < #FPR to be saved; i++)
         stfd fr(31-x), (-8 * (i+1))(r1)
  */  
  Uint16 i;
 
  for(i = 0; i < (mSaveFPRwords/2); i++)
	{
      Uint32 fpr = 31-i;
      fpr = fpr<<21;
      Int32 off = (-8 * (i+1));
      Uint32 offset = off & lower16;
      
      Uint32 stfd =  stfd_FR_offR1 | fpr | offset; 
      
      *nextInstruction++ = stfd;
    }
  
  /*
    Save non-volatile GPRs
    non volatile general purpose registers are in the range r13 - r31
    the saved block of non-volatile registers must start at r31 and move up to fr 13
    
    for(i = 0; i < #GPR to be saved; i++)
          stw r(31-x), (-4 * (i+1)) - (4 * fprSize_words)) (r1)
  */  
  for(i = 0; i < (mSaveGPRwords); i++)
    {
      Uint32 gpr = 31-i;
      gpr = gpr<<21;
      Int32 off = (-4 * (i+1)) - (4 * mSaveFPRwords);
      Uint32 offset = off & lower16;
      
      Uint32 stw =  stw_R_offR1 | gpr | offset; 
      
      *nextInstruction++ = stw;
    }

  /*
    Create stack frame
    
    if(frameSize_bytes > 32768)
        lis   r12, upper 16-bits of -frameSize_bytes
        ori   r12, lower 16-bits of -frameSize_bytes
        stwuz r1, r1, r12stwu r1, -frameSize_bytes(r1)
    else
        stwu r1, -frameSize_bytes(r1)
  */

  Int32 frameSize_bytes = -4*mFrameSizeWords;
  if(frameSize_bytes < -32768)
    {
     
      Uint32 sfUpper = upper16 & frameSize_bytes;
      sfUpper = sfUpper>>16;
      Uint32 sfLower = lower16 & frameSize_bytes;
      
      uint lis =  lisR12_imm | sfUpper;
      uint ori =  oriR12R12_imm | sfLower;
   
      *nextInstruction++ = lis;
      *nextInstruction++ = ori;
      *nextInstruction =  stwuxR1R1R12 ;
   
    } else {
      Uint32 offset = frameSize_bytes & lower16;
      uint stwv = stwuR1_offR1 | offset;
      *nextInstruction = stwv;
    }

#ifdef DEBUG
  nextInstruction++;
  Uint32 numberInstructions = nextInstruction - (Uint32*)inWhere;
 // assert((numberInstructions * 4) == mPrologSize_bytes);	FIX-ME
#endif
}



void PPCFormatter::	
formatEpilogToMemory(void* inWhere)
{
  Uint32* nextInstruction = (Uint32*)inWhere;

  /*
    Return to parent's stack frame
    
    if(frameSize_bytes > 32768)
        lis   r12, upper 16-bits of frameSize_bytes
        ori   r12, lower 16-bits of frameSize_bytes
        addi r1, r1, r12
    else
        addi r1, r1, frameSize_bytes
  */
  
  const Uint32 frameSize_bytes = 4*mFrameSizeWords;
  if(frameSize_bytes > 32768)
    {     
      Uint32 sfUpper = upper16 & frameSize_bytes;
      sfUpper = sfUpper>>16;
      Uint32 sfLower = lower16 & frameSize_bytes;    
      
      uint lis =  lisR12_imm | sfUpper;
      uint ori =  oriR12R12_imm | sfLower;
   
      *nextInstruction++ = lis;
      *nextInstruction++ = ori;
      *nextInstruction++ = addR1R1R12;
    } 
    else 
    {
      Uint32 offset = lower16 & frameSize_bytes;
      Uint32 addi = addiR1R1_imm | offset;
      *nextInstruction++ = addi;
    }
 
#if 0
  /* 
     restore CR
     lwz r0, 4(r1)
     mtcr r0
  */
  *nextInstruction++ = lwzR04R1; 
  *nextInstruction++ = mtcrR0;
#endif

  /*
    restore LR
    lwz r0, 8(r1)
    mtlr r0
  */
  *nextInstruction++ = lwzR08R1;
  *nextInstruction++ = kMtlrR0;

  /*
    Restore non-volatile FPRs
    for(i = 0; i < #FPR to be saved; i++)
         lfd fr(31-x), (-8 * (i+1))(r1)
  */  
  Uint16 i;
 
  for(i = 0; i < (mSaveFPRwords/2); i++)
    {
      Uint32 fpr = 31-i;
      fpr = fpr<<21;
      Int32 off = (-8 * (i+1));
      Uint32 offset = off & lower16;
      
      Uint32 lfd =  lfd_FR_offR1 | fpr | offset; 
      
      *nextInstruction++ = lfd;
    }
  
  
  /*
    Save non-volatile GPRs
    for(i = 0; i < #GPR to be saved; i++)
          lwz r(31-x), (-4 * (i+1)) - (4 * fprSize_words)) (r1)
  */  
  for(i = 0; i < (mSaveGPRwords); i++)
    {
      Uint32 gpr = 31-i;
      gpr = gpr<<21;
      Int32 off = (-4 * (i+1)) - (4 * mSaveFPRwords);
      Uint32 offset = off & lower16;
      
      Uint32 lwz =  lwz_R_offR1 | gpr | offset; 
      
      *nextInstruction++ = lwz;
    }
	
	*nextInstruction = kBlr;

#ifdef DEBUG
  nextInstruction++;
  Uint32 numberInstructions = nextInstruction - (Uint32*)inWhere;
  //assert((numberInstructions * 4) == mEpilogSize_bytes);		// FIX-ME
#endif
}

#ifdef XP_MAC
#include "StringUtils.h"

class PPCTraceInfo
{
	// a trace back table is a PPCTraceInfo
	// aligned on a 4 byte boundary
public:
	PPCTraceInfo(const char* inSymbol) :
		symbol(inSymbol) 
	{ 
		symbolLen = PL_strlen(symbol); 
		magic[0] = 0;
		magic[1] = 0x00092040;
		magic[2] = 0;
	}
	
	Uint32 					magic[3];		// some flags and other unknown stuff
	Uint32					functionLen;	// length of function in bytes
	Uint16 					symbolLen;		// length of following symbol	
	TemporaryStringCopy		symbol;			// symbol to write out

	size_t
	getFormattedSize()
	{
		size_t size = offsetof(PPCTraceInfo, symbol) + symbolLen;
		
		// align 4
		return ((size + 3) & ~3);
	}		
	
	void
	formatToMemory(void* inStart, Uint32 inFunctionLength)
	{
		Uint32 x[] = {0, 0x00092041, 0x80030000, 0x00000078, 0x00202E5F, 0x5F63745F, 0x5F313954, 0x656D706F, 0x72617279, 0x53747269, 0x6E67436F, 0x70794650, 0x43630000};
		
		unsigned char* 	output = static_cast<unsigned char*>(inStart);
		char *cp;
		
		for (cp = symbol; *cp; cp++) 
		{
			if (!(('A' <= *cp && *cp <= 'Z') || ('a' <= *cp && *cp <= 'z')))
				*cp = 'x';
		}		
		
		functionLen = inFunctionLength;
		output += offsetof(PPCTraceInfo, symbol);
		memcpy(output, symbol, symbolLen);
		
		symbolLen = (symbolLen + 1) & ~1;
		output -= offsetof(PPCTraceInfo, symbol);
		memcpy(output, &magic[0], offsetof(PPCTraceInfo, symbol));
	}
};


#endif // XP_MAC

#include "FieldOrMethod.h"
#include "NativeFormatter.h"

void PPCFormatter::
beginFormatting(const FormattedCodeInfo& inInfo)
{
	Int16	offsetInRealTOC;
	
	// determine offset we need to add to the offsets into the accumulator toc
	mRealTOC = &acquireTOC(mEmitter.mAccumulatorTOC.getNext() - mEmitter.mAccumulatorTOC.mBegin);
	
	void *d = mRealTOC->addData(mEmitter.mAccumulatorTOC.mBegin, mEmitter.mAccumulatorTOC.getNext() - mEmitter.mAccumulatorTOC.mBegin, offsetInRealTOC);
	assert(d);
	mRealTOCOffset = offsetInRealTOC + mRealTOC->mGlobalPtr - mRealTOC->mBegin;
	
	// grab ptr to beginning of stuff that's not code
	mNextFreePostMethod = (Uint32*) ((Uint8*) inInfo.methodStart + inInfo.bodySize + inInfo.preMethodSize);
}



void PPCFormatter::
calculatePrePostMethod(Method& inMethod, Uint32& outPreMethodSize, Uint32& outPostMethodSize) 
{ 
	outPreMethodSize = 0;
	outPostMethodSize = mEmitter.mCrossTOCPtrGlCount * kCrossTocPtrGlBytes;
		
#ifdef FIX_ME
	PPCTraceInfo	traceBack(inMethod.toString());

	outPostMethodSize += traceBack.getFormattedSize(); //sTraceInfo.size(inMethod.toString());
#else
	inMethod;
#endif
}

void PPCFormatter::
formatPostMethodToMemory(void* inWhere, const FormattedCodeInfo& inInfo)
{
#ifdef XP_MAC
#if 0
	PPCTraceInfo	traceBack(inInfo.method->toString());
	
	traceBack.formatToMemory(inWhere, inInfo.bodySize);
#endif
	inWhere;
	inInfo;
#else
	inWhere;
	inInfo;
#endif
}



Uint8* PPCFormatter::
createTransitionVector(const FormattedCodeInfo& inInfo)
{
	TVector	newTVector = {inInfo.methodStart, (void*) mRealTOC->mGlobalPtr};	
	Uint16	offset;
	
	void*	tvectorPtr = mRealTOC->addData(&newTVector, sizeof(newTVector), offset);	
	
	assert(tvectorPtr);		// FIX-ME handle case where TOC blows up
	
	return (Uint8*)tvectorPtr;
}



