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
//
// File:	x86Win32Emitter.cpp
//
// Authors:	Peter DeSantis
//			Simon Holmes a Court
//

// Temporary
// Some CL args I use:
// -sys javasoft/sqe/tests/api/java/lang/Boolean/BooleanTests1
// -stage i -method insertElementAt (Ljava/lang/Object;I)V java/util/Vector
// -stage -noinvoke i -method cmpLongTestHarnessHelper (JJ)I TinyClass
// -l Output -l Package TinyClass
// -breakCompile TinyClass Sieve ([I)V -html -log x86ArgList 4 -log x86Emitter 4 -log x86Spill 4 -log FieldOrMethod 4 -l Package -sys TinyClass
// -sys -html -log x86Emitter 4 -log FieldOrMethod 4 -ta -be java.lang.String startsWith (Ljava/lang/String;I)Z TinyClass
// -sys -html -be suntest.quicktest.runtime.QuickTest assert (ZLjava/lang/String;)V javasoft/sqe/tests/api/java/lang/Integer/IntegerTests32

#include "x86Win32Emitter.h"
#include "x86Win32Instruction.h"
#include "x86StdCall.h"
#include "Primitives.h"
#include "SysCalls.h"
#include "x86SysCallsRuntime.h"
#include "JavaObject.h"

#include "x86-win32.nad.burg.h"
#include "ControlNodes.h"

#include "x86Win32Instruction.h"

#include "x86Win32ExceptionHandler.h"

UT_DEFINE_LOG_MODULE(x86Emitter);

//-----------------------------------------------------------------------------------------------------------
// Debugging


#ifdef DEBUG_LOG_VERBOSE
// for rule printing
	extern char* burm_string[];
	extern int burm_max_rule;
#endif

//-----------------------------------------------------------------------------------------------------------
// Emitter

// Structures/Enums

x86ArgumentType memDSIAddressingModes[] =
{
	atBaseIndexed,
	atScaledIndexedBy2,
	atScaledIndexedBy4,
	atScaledIndexedBy8
};

struct x86CondList
{
	x86ConditionCode ccSigned;
	x86ConditionCode ccUnsigned;
};

x86CondList condList[] =
{
	// signed, unsigned
	{ ccJL,	  ccJB	 },	// cLt
	{ ccJE,   ccJE	 },	// cEq
	{ ccJLE,  ccJBE	 },	// cLe
	{ ccJNLE, ccJNBE },	// cGt
	{ ccJNE,  ccJNE	 },	// cLgt
	{ ccJNL,  ccJNB	 },	// cGe
};

//-----------------------------------------------------------------------------------------------------------
void x86Win32Emitter::
emitPrimitive(Primitive& inPrimitive, NamedRule inRule)
{
#ifdef DEBUG_LOG_VERBOSE	// very noisy
	char* ruleName;
	if(inRule < burm_max_rule && (ruleName = burm_string[inRule]) != 0)
			printf("[%3d] %-50s ", inRule, ruleName);
	else
			printf("[%3d] %-50s ", inRule, NULL);
#endif

	// case statement corresponding to rules in the grammar
	switch (inRule)
	{
		//-----------------------------------------------------------------------
		// General
        case emConst_I:			emit_LoadConstant_I(inPrimitive);	break;
		case emConst_A:			emit_LoadAddress(inPrimitive);		break;
		case emConst_L:			emit_LoadConstant_L(inPrimitive);	break;
        case emConst_F:			emit_LoadConstant_F(inPrimitive);   break;
        case emConst_D:			emit_LoadConstant_D(inPrimitive);   break;

		case emResult_I:		emit_Result_I(inPrimitive);			break;
		case emResult_A:		emit_Result_I(inPrimitive);			break;
		case emResult_L:		emit_Result_L(inPrimitive);			break;
        case emResult_F:		emit_Result_F(inPrimitive);			break;
        case emResult_D:		emit_Result_D(inPrimitive);			break;

		case emBreak:			emit_Break(inPrimitive);			break;

		//-----------------------------------------------------------------------
		// Control Flow
	    case emIfLt:			emit_B(inPrimitive, rawLt);			break;
		case emIfEq:        	emit_B(inPrimitive, rawEq);			break;
		case emIfLe:			emit_B(inPrimitive, rawLe);			break;
		case emIfGt:			emit_B(inPrimitive, rawGt);			break;
		case emIfLgt:			emit_B(inPrimitive, rawLgt);		break;
		case emIfGe:    		emit_B(inPrimitive, rawGe);			break;

		case emIfULt:			emit_B(inPrimitive, rawLt);			break;
		case emIfUEq:			emit_B(inPrimitive, rawEq);			break;
		case emIfULe:			emit_B(inPrimitive, rawLe);			break;
		case emIfUGt:			emit_B(inPrimitive, rawGt);			break;
		case emIfNe:			emit_B(inPrimitive, rawLgt);		break;
		case emIfUGe:       	emit_B(inPrimitive, rawGe);			break;

		//-----------------------------------------------------------------------
		// Booleans
		case emLt_I:			emit_Cond(inPrimitive, rawLt);		break;
		case emEq_I:        	emit_Cond(inPrimitive, rawEq);		break;
		case emLe_I:			emit_Cond(inPrimitive, rawLe);		break;
		case emGt_I:			emit_Cond(inPrimitive, rawGt);		break;
		case emLgt_I:			emit_Cond(inPrimitive, rawLgt);		break;
		case emGe_I:			emit_Cond(inPrimitive, rawGe);		break;

		case emULt_I:			emit_Cond(inPrimitive, rawLt);		break;
		case emUEq_I:			emit_Cond(inPrimitive, rawEq);		break;
		case emULe_I:			emit_Cond(inPrimitive, rawLe);		break;
		case emUGt_I:			emit_Cond(inPrimitive, rawGt);		break;
		case emNe_I:			emit_Cond(inPrimitive, rawLgt);		break;
		case emUGe_I:			emit_Cond(inPrimitive, rawGe);		break;

		//-----------------------------------------------------------------------
		// Switch
		case emSwitch:			emit_Switch(inPrimitive);			break; 

		//-----------------------------------------------------------------------
		// Scalar and Aritmetic Logic

		// And
		case emAnd_I:			emit_And_I(inPrimitive);			break;
		case emAndI_I:			emit_AndI_I(inPrimitive);			break;

		case emAnd_L:			emit_And_L(inPrimitive);			break;

		// Or
		case emOr_I:			emit_Or_I(inPrimitive);				break;
		case emOrI_I:			emit_OrI_I(inPrimitive);			break;
		case emOr_L:			emit_Or_L(inPrimitive);				break;

		// Xor
		case emXor_I:			emit_Xor_I(inPrimitive);			break;
		case emXorI_I:			emit_XorI_I(inPrimitive);			break;

		case emXor_L:			emit_Xor_L(inPrimitive);			break;

		// Add
		case emAdd_I:			emit_Add_I(inPrimitive);			break;
		case emAdd_A:			emit_Add_I(inPrimitive);			break;

		case emAddI_I:			emit_AddI_I(inPrimitive);			break;	
		case emAddI_A:			emit_AddI_I(inPrimitive);			break;

		case emAdd_L:			emit_Add_L(inPrimitive);			break;

		// Sub
		case emSub_I:			emit_Sub_I(inPrimitive);			break;
		case emSub_A:			emit_Sub_I(inPrimitive);			break;

		case emSub_L:			emit_Sub_L(inPrimitive);			break;
		case emSubR_I:			emit_SubR_I(inPrimitive);			break;
		
		// Mul
		case emMul_I:			emit_Mul_I(inPrimitive);			break;
		case emMul_L:			emit_Mul_L(inPrimitive);			break;

		// Div
		case emDiv_I:			emit_Div_I(inPrimitive);			break;
		case emDivE_I:			emit_Div_I(inPrimitive);			break;

		case emDiv_I_MemDSI:	emit_Div_I_MemDSI(inPrimitive);		break;
		case emDivE_I_MemDSI:	emit_Div_I_MemDSI(inPrimitive);		break;

		case emDivU_I:			emit_DivU_I(inPrimitive);			break;
		case emDivUE_I:			emit_DivU_I(inPrimitive);			break;

		case emDivU_I_MemDSI:	emit_DivU_I_MemDSI(inPrimitive);	break;
		case emDivUE_I_MemDSI:	emit_DivU_I_MemDSI(inPrimitive);	break;

		case emDiv_L:			emit_Div_L(inPrimitive);			break;
		case emDivE_L:			emit_Div_L(inPrimitive);			break;

		// Mod
		case emMod_I:			emit_Mod_I(inPrimitive);			break;
		case emModE_I:			emit_Mod_I(inPrimitive);			break;

		case emMod_I_MemDSI:	emit_Mod_I_MemDSI(inPrimitive);		break;
		case emModE_I_MemDSI:	emit_Mod_I_MemDSI(inPrimitive);		break;

		case emModU_I:			emit_ModU_I(inPrimitive);			break;
		case emModUE_I:			emit_ModU_I(inPrimitive);			break;

		case emModU_I_MemDSI:	emit_ModU_I_MemDSI(inPrimitive);	break;
		case emModUE_I_MemDSI:	emit_ModU_I_MemDSI(inPrimitive);	break;

		case emMod_L:			emit_Mod_L(inPrimitive);			break;
		case emModE_L:			emit_Mod_L(inPrimitive);			break;

		// Shift
		case emShl_I:			emit_Shl_I(inPrimitive);			break;
		case emShlI_I:			emit_ShlI_I(inPrimitive);			break;

		case emShr_I:			emit_Sar_I(inPrimitive);			break;
		case emShrI_I:			emit_SarI_I(inPrimitive);			break;

		case emShrU_I:			emit_Shr_I(inPrimitive);			break;
		case emShrUI_I:			emit_ShrI_I(inPrimitive);			break;

		case emShl_L:			emit_Shl_L(inPrimitive);			break;
		case emShr_L:			emit_Sar_L(inPrimitive);			break;
		case emShrU_L:			emit_Shr_L(inPrimitive);			break;

		// Sign Extend
		case emExt_I:			emit_Ext_I(inPrimitive);			break;
		case emExt_L:			emit_Ext_L(inPrimitive);			break;
			
		//-----------------------------------------------------------------------
		// Floating Point Arithmetic

        case emFAdd_D:          emit_FAdd_D(inPrimitive);			break;
        case emFAdd_F:          emit_FAdd_F(inPrimitive);			break;

        case emFMul_D:          emit_FMul_D(inPrimitive);			break;
        case emFMul_F:          emit_FMul_F(inPrimitive);			break;
            
        case emFSub_D:          emit_FSub_D(inPrimitive);			break;
        case emFSub_F:          emit_FSub_F(inPrimitive);			break;
            
        case emFDiv_D:          emit_FDiv_D(inPrimitive);			break;
        case emFDiv_F:          emit_FDiv_F(inPrimitive);			break;

        case emFRem_D:          emit_FRem_D(inPrimitive);			break;
        case emFRem_F:          emit_FRem_F(inPrimitive);			break;
            
        //-----------------------------------------------------------------------
		// Integer conversion
		case emConvI_L:			emit_ConvI_L(inPrimitive);			break;
		case emConvL_I:			emit_ConvL_I(inPrimitive);			break;

        //-----------------------------------------------------------------------
		// Float conversion
        case emFConvI_F:
        case emFConvI_D:
        case emFConvL_F:
        case emFConvL_D:
        case emFConvF_I:
        case emFConvF_L:
        case emFConvF_D:
        case emFConvD_I:
        case emFConvD_L:
        case emFConvD_F:        emit_FConv(inPrimitive);			break;

		//-----------------------------------------------------------------------
		// Comparison
		case emCmp_I:			emit_Cmp_I(inPrimitive);			break;
		case emCmpU_I:			emit_Cmp_I(inPrimitive);			break;
		case emCmpU_A:			emit_Cmp_I(inPrimitive);			break;

		case emCmpI_I:			emit_CmpI_I(inPrimitive);			break; 
		case emCmpUI_I:			emit_CmpI_I(inPrimitive);			break; 

		case emCmpI_I_MemDSI:	emit_CmpI_I_MemDSI(inPrimitive);	break;
		case emCmp_I_MemDSI:	emit_Cmp_I_MemDSI(inPrimitive);		break;

		case em3wayCmpL_L:		emit_3wayCmpL_L(inPrimitive);		break;
        case em3wayCmpCL_L:		emit_3wayCmpCL_L(inPrimitive);		break;

        case em3wayCmpF_L:      emit_3wayCmpF_L(inPrimitive);		break;
        case em3wayCmpF_G:      emit_3wayCmpF_G(inPrimitive);		break;
        case em3wayCmpD_L:      emit_3wayCmpD_L(inPrimitive);		break;
        case em3wayCmpD_G:      emit_3wayCmpD_G(inPrimitive);		break;

        case em3wayCmpCF_L:     emit_3wayCmpCF_L(inPrimitive);		break;
        case em3wayCmpCF_G:     emit_3wayCmpCF_G(inPrimitive);		break;
        case em3wayCmpCD_L:     emit_3wayCmpCD_L(inPrimitive);		break;
        case em3wayCmpCD_G:     emit_3wayCmpCD_G(inPrimitive);		break;

		//-----------------------------------------------------------------------
		// Limit
		case emLimitR:			emit_LimitR(inPrimitive);			break;
		case emLimit:			emit_Limit(inPrimitive);			break;
		case emLimitR_MemDisp:	emit_LimitR_MemDisp(inPrimitive);	break;
		case emLimit_MemDisp:	emit_Limit_MemDisp(inPrimitive);	break;

		//-----------------------------------------------------------------------
		// Cast
		case emLimCast:			emit_LimCast(inPrimitive);			break;
		case emChkCastI_A:		emit_ChkCast_Const(inPrimitive);	break;
		case emChkCast_A:		emit_ChkCast(inPrimitive);			break;
		case emChkCast_I:		emit_ChkCast(inPrimitive);			break;

		//-----------------------------------------------------------------------
		// Memory
		case emLd_I:			genLd_I(inPrimitive);				break;
		case emLd_A:			genLd_I(inPrimitive);				break;
		case emLd_L:			emit_Ld_L(inPrimitive);				break;
        case emLd_F:            emit_Ld_F(inPrimitive);             break;
        case emLd_D:            emit_Ld_D(inPrimitive);             break;


		case emLd_I_MemDisp:	emit_Ld_I_MemDisp(inPrimitive);		break;
		case emLd_I_MemDSI:		emit_Ld_I_MemDSI(inPrimitive);		break;

		case emLdS_B:			emit_LdS_B(inPrimitive);			break;
		case emLdU_B:			emit_LdU_B(inPrimitive);			break;
		case emLdS_H:			emit_LdS_H(inPrimitive);			break;
		case emLdU_H:			emit_LdU_H(inPrimitive);			break;
            
		case emSt_I:			emit_St_I(inPrimitive);				break;
		case emSt_A:			emit_St_I(inPrimitive);				break;
		case emSt_L:			emit_St_L(inPrimitive);				break;
		case emStI_I:			emit_StI_I(inPrimitive);			break;

		case emSt_B:			emit_St_B(inPrimitive);				break;
		case emSt_H:			emit_St_H(inPrimitive);				break;

		case emStI_I_MemDisp:	emit_StI_I_MemDisp(inPrimitive);	break;
		case emSt_I_MemDisp:	emit_St_I_MemDisp(inPrimitive);		break;
		case emSt_I_MemDSI:		emit_St_I_MemDSI(inPrimitive);		break;

		case emSt_F:			emit_St_F(inPrimitive);				break;
        case emSt_D:			emit_St_D(inPrimitive);				break;

/*      FIXME: Implement performance optimizations

        case emLd_F_MemDisp:
        case emLd_F_MemDSI:
        case emLd_D_MemDisp:
        case emLd_D_MemDSI:     assert(0);                          break;
        
        case emStI_F_MemDisp:
        case emSt_F_MemDisp:
        case emSt_F_MemDSI:

        case emStI_D_MemDisp:
        case emSt_D_MemDisp:
        case emSt_D_MemDSI:
                                assert(0);                          break;
*/

		case emCatch:			emit_Catch(inPrimitive);			break;

		//-----------------------------------------------------------------------
		// Calls
		case emStaticCall:
			{
				new(mPool) Call_(&inPrimitive, mPool, Call_::numberOfArguments(inPrimitive), Call_::hasReturnValue(inPrimitive), *this);
				break;
			}
		case emDynamicCall:
			{
				new(mPool) CallD_(&inPrimitive, mPool, CallD_::numberOfArguments(inPrimitive), CallD_::hasReturnValue(inPrimitive), *this);
				break;
			}
		case emSysCall: case emSysCallE:
			{
				new(mPool) CallS_(		&inPrimitive, mPool, CallS_::numberOfArguments(inPrimitive), true, *this, 
										(void (*)()) static_cast<PrimSysCall*>(&inPrimitive)->sysCall.function);
				break;
			}
		case emSysCallC: case emSysCallEC:
			{
				new(mPool) CallS_C(		&inPrimitive, mPool,
										CallS_C::numberOfArguments(inPrimitive), true, *this,
										(void (*)()) static_cast<PrimSysCall*>(&inPrimitive)->sysCall.function); 
				break;
			}
		case emSysCallV: case emSysCallEV:
			{
				new(mPool)  CallS_V(	&inPrimitive, mPool,
										CallS_V::numberOfArguments(inPrimitive), true, *this,
										(void (*)()) static_cast<PrimSysCall*>(&inPrimitive)->sysCall.function); 
				break;
			}

		//-----------------------------------------------------------------------
		// Monitors
		case emMEnter:			emit_MonitorEnter(inPrimitive);			break;
		case emMExit:			emit_MonitorExit(inPrimitive);			break;

		//-----------------------------------------------------------------------
		// Misc
		case emChkNull:			emit_ChkNull(inPrimitive);			break;

		default:
#ifdef DEBUG_LOG_VERBOSE
			printf(" (unimplemented)");
#endif
			break;
	}

#ifdef DEBUG_LOG_VERBOSE
			printf("\n");
#endif
}
	
//-----------------------------------------------------------------------------------------------------------
// Utility
bool x86Win32Emitter::
emitCopyAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr)
{
	InsnDoubleOpDir& newInsn = newCopyInstruction(inDataNode, mPool);
	newInsn.addUse(0, fromVr);
	newInsn.addDefine(0, toVr);
	newInsn.insertAfter(*where);

#if DEBUG
	toVr.setDefiningInstruction(newInsn);
	newInsn.checkIntegrity();					// ensure that everthing makes sense here
#endif
	return false;
}

void x86Win32Emitter::
emitStoreAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& storedReg, VirtualRegister& stackReg)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inDataNode, mPool, raSaveReg, atRegDirect, atRegAllocStackSlot, 1, 1);
    newInsn.addUse(0, storedReg);	
	newInsn.addDefine(0, stackReg);
	newInsn.insertAfter(*where);

	UT_LOG(x86Emitter, PR_LOG_DEBUG, (" emitStoreAfter (insn %p)\n", &newInsn));
#if DEBUG
    newInsn.printDebug(UT_LOG_MODULE(x86Emitter));
	newInsn.checkIntegrity();					// ensure that everthing makes sense here
#endif
}

void x86Win32Emitter::
emitLoadAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& loadedReg, VirtualRegister& stackReg)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inDataNode, mPool, raLoadI, atRegAllocStackSlot, atRegDirect, 1, 1);
	newInsn.addUse(0, stackReg);
    newInsn.addDefine(0, loadedReg);
	newInsn.insertAfter(*where);

#ifdef DEBUG		// FIX why do you do this Laurent?
	loadedReg.setDefiningInstruction(newInsn);
#endif

	UT_LOG(x86Emitter, PR_LOG_DEBUG, (" emitLoadAfter  (insn %p)\n", &newInsn));
#ifdef DEBUG
    newInsn.printDebug(UT_LOG_MODULE(x86Emitter));
	newInsn.printArgs();
	newInsn.checkIntegrity();					// ensure that everthing makes sense here
#endif
}

Instruction& x86Win32Emitter::
emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget)
{	
	x86Instruction&	newInsn = *new(mPool) x86Instruction(&inDataNode, mPool, inTarget);
	return (newInsn);
}

// COMMENT ME
VirtualRegister& x86Win32Emitter::
emit_CopyOfInput(x86ArgListInstruction& /*inInsn*/, DataNode& inPrimitive, Uint8 inWhichInput, VirtualRegisterID inID)
{
	// FIX-ME assumes fixed point registers
	InsnDoubleOpDir& newInsn = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(inWhichInput), newInsn, 0, vrcInteger, inID);
	VirtualRegister &vrToBeOverwritten = *defineProducer(inPrimitive, newInsn, 0, vrcInteger, inID);
	return vrToBeOverwritten;
}

//-----------------------------------------------------------------------------------------------------------
// Arithmetic
/*
// Broken for now
void x86Win32Emitter::
emit_MulI_I(Primitive& inPrimitive)
{
	// FIX check for power of two
	assert(false);
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);
	x86Instruction& newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, sMulImm, constant, atRegDirect, atRegDirect, 1, 1);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 0);		
	defineProducer(inPrimitive, newInsn, 0);
}
*/

void x86Win32Emitter::
emit_Mul_I(Primitive& inPrimitive)
{	
	InsnDoubleOp&	newInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMul);
	newInsn.x86StandardUseDefine(*this);
}

void x86Win32Emitter::
emit_AddI_I(Primitive& inPrimitive)
{
	x86Instruction*	newInsn;
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);

	if(constant == 1) {							// inc
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceInc, atRegDirect, 1, 1);
		newInsn->x86StandardUseDefine(*this);
	} else if((int)constant == -1) {			// dec
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceDec, atRegDirect, 1, 1);
		newInsn->x86StandardUseDefine(*this);
	} else {									// add
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, iaAddImm, constant, atRegDirect, 1, 1);
		newInsn->x86StandardUseDefine(*this);
	}
}

void x86Win32Emitter::
emit_Add_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raAdd);
	newInsn.x86StandardUseDefine(*this);
}

void x86Win32Emitter::
emit_Sub_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raSub);
	newInsn.x86StandardUseDefine(*this);
}

// a - b = (-b) + a
void x86Win32Emitter::
emit_SubR_I(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 0);

	// negate
	x86Instruction&	negInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eNeg, atRegDirect, 1, 1 );
	VirtualRegister& vrOut = emit_CopyOfInput(negInsn, inPrimitive, 1);
	useProducer(inPrimitive, negInsn, 0);
	redefineTemporary(negInsn, vrOut, 0);

	// add constant
	// FIX later work out how to avoid emitting
	x86Instruction&	addInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaAddImm, constant, atRegDirect, 1, 1);
	useProducer(inPrimitive, addInsn, 0);
	defineProducer(inPrimitive, addInsn, 0);
	// FIX is this right
}

//-----------------------------------------------------------------------------------------------------------
// Integer Logical Operations

void x86Win32Emitter::emit_AndI_I(Primitive& inPrimitive)	{ genLogicI_I(inPrimitive, iaAndImm); }
void x86Win32Emitter::emit_OrI_I(Primitive& inPrimitive)	{ genLogicI_I(inPrimitive, iaOrImm);  }
void x86Win32Emitter::emit_XorI_I(Primitive& inPrimitive)	{ genLogicI_I(inPrimitive, iaXorImm); }

void x86Win32Emitter::emit_And_I(Primitive& inPrimitive)	{ genLogic_I(inPrimitive, raAnd); }
void x86Win32Emitter::emit_Or_I(Primitive& inPrimitive)		{ genLogic_I(inPrimitive, raOr);  }
void x86Win32Emitter::emit_Xor_I(Primitive& inPrimitive)	{ genLogic_I(inPrimitive, raXor); }

void x86Win32Emitter::
genLogicI_I(Primitive& inPrimitive, x86ImmediateArithType iaType) 
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);
	x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaType, constant, atRegDirect, 1, 1);
	newInsn.x86StandardUseDefine(*this);
}

void x86Win32Emitter::
genLogic_I(Primitive& inPrimitive, x86DoubleOpDirCode raType)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raType);
	newInsn.x86StandardUseDefine(*this);
}

//-----------------------------------------------------------------------------------------------------------
// Integer Shifts

void x86Win32Emitter::emit_SarI_I(Primitive& inPrimitive)	{ genShiftI_I(inPrimitive, eSarImm, eSar1); }
void x86Win32Emitter::emit_ShrI_I(Primitive& inPrimitive)	{ genShiftI_I(inPrimitive, eShrImm, eShr1); }
void x86Win32Emitter::emit_ShlI_I(Primitive& inPrimitive)	{ genShiftI_I(inPrimitive, eShlImm, eShl1); }

void x86Win32Emitter::emit_Sar_I(Primitive& inPrimitive)	{ genShift_I(inPrimitive, eSarCl); }
void x86Win32Emitter::emit_Shr_I(Primitive& inPrimitive)	{ genShift_I(inPrimitive, eShrCl); }
void x86Win32Emitter::emit_Shl_I(Primitive& inPrimitive)	{ genShift_I(inPrimitive, eShlCl); }

void x86Win32Emitter::genShiftI_I(Primitive& inPrimitive, x86ExtendedType eByImmediate, x86ExtendedType eBy1)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);
	if(constant == 1) 
	{
		x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eBy1, atRegDirect, 1, 1);
		newInsn.x86StandardUseDefine(*this);
	} 
	else 
	{
		x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eByImmediate, constant, atRegDirect, 1, 1);
		newInsn.x86StandardUseDefine(*this);
	}
}

void x86Win32Emitter::genShift_I(Primitive& inPrimitive, x86ExtendedType eByCl)
{
	x86Instruction&	shiftInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eByCl, atRegDirect, 2, 1 );

	// we kill one of our inputs, make copy
	emit_CopyOfInput(shiftInsn, inPrimitive, 0);

	// input and output arg 0 will have been copied to the outgoing edge
	// by emit_CopyOfInput
	useProducer(inPrimitive, shiftInsn, 0);
	defineProducer(inPrimitive, shiftInsn, 0);

	// now make a buffer copy of the shift by value so we can precolor 
	InsnDoubleOpDir& shiftByCopy = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(1), shiftByCopy, 0);
	VirtualRegister& shiftByVR = defineTemporary(shiftByCopy, 0);
	shiftByVR.preColorRegister(x86GPRToColor[ECX]);

	// setup last input of the shift instruction
	useTemporaryVR(shiftInsn, shiftByVR, 1);
};

//-----------------------------------------------------------------------------------------------------------
// 64 bit support
void x86Win32Emitter::emit_Add_L(Primitive& inPrimitive)	{ emit_Arithmetic_L(inPrimitive, raAdd, raAdc); }
void x86Win32Emitter::emit_Sub_L(Primitive& inPrimitive)	{ emit_Arithmetic_L(inPrimitive, raSub, raSbb); }

void x86Win32Emitter::
emit_Arithmetic_L(Primitive& inPrimitive, x86DoubleOpDirCode insnTypeLo, x86DoubleOpDirCode insnTypeHi)
{
	// low word arithmetic
	InsnDoubleOpDir& insnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, insnTypeLo, atRegDirect, atRegDirect, 2, 2);
	VirtualRegister& vrLo = emit_CopyOfInput(insnLo, inPrimitive, 0, vidLow);
	useProducer(inPrimitive, insnLo, 0, vrcInteger, vidLow);
	useProducer(inPrimitive.nthInputVariable(1), insnLo, 1, vrcInteger, vidLow);
	redefineTemporary(insnLo, vrLo, 0);
    insnLo.addDefine(1, udCond);

	// high word arithmetic with carry
	InsnDoubleOpDir& insnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, insnTypeHi, atRegDirect, atRegDirect, 3, 1);
	VirtualRegister& vrHi = emit_CopyOfInput(insnHi, inPrimitive, 0, vidHigh);
	useProducer(inPrimitive, insnHi, 0, vrcInteger, vidHigh);
	useProducer(inPrimitive.nthInputVariable(1), insnHi, 1, vrcInteger, vidHigh);
	insnHi.addUse(2, udCond);
	insnHi.getInstructionUseBegin()[2].src = &insnLo;	// FIX hack in lieu of temporary condition edge
	redefineTemporary(insnHi, vrHi, 0);
}


void x86Win32Emitter::emit_And_L(Primitive& inPrimitive)	{ emit_Logic_L(inPrimitive, raAnd); }
void x86Win32Emitter::emit_Or_L(Primitive& inPrimitive)		{ emit_Logic_L(inPrimitive, raOr);  }
void x86Win32Emitter::emit_Xor_L(Primitive& inPrimitive)	{ emit_Logic_L(inPrimitive, raXor); }

void x86Win32Emitter::
emit_Logic_L(Primitive& inPrimitive, x86DoubleOpDirCode insnType)
{
	// low word
	InsnDoubleOpDir& insnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, insnType);
	VirtualRegister& vrLo = emit_CopyOfInput(insnLo, inPrimitive, 0, vidLow);
	useProducer(inPrimitive, insnLo, 0, vrcInteger, vidLow);
	useProducer(inPrimitive.nthInputVariable(1), insnLo, 1, vrcInteger, vidLow);
	redefineTemporary(insnLo, vrLo, 0);

	// high word
	InsnDoubleOpDir& insnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, insnType);
	VirtualRegister& vrHi = emit_CopyOfInput(insnHi, inPrimitive, 0, vidHigh);
	useProducer(inPrimitive, insnHi, 0, vrcInteger, vidHigh);
	useProducer(inPrimitive.nthInputVariable(1), insnHi, 1, vrcInteger, vidHigh);
	redefineTemporary(insnHi, vrHi, 0);
}

#if defined(WIN32)
extern int64 __stdcall x86Mul64Bit(int64 a, int64 b);
extern int64 __stdcall x86Div64Bit(int64 a, int64 b);
extern int64 __stdcall x86Mod64Bit(int64 a, int64 b);
extern int64 __stdcall x86Shl64Bit(int64 a, int b);
extern uint64 __stdcall x86Shr64Bit(uint64 a, int b);
extern int64 __stdcall x86Sar64Bit(int64 a, int b);
extern int64 __stdcall x86ThreeWayCMP_L(int64 a, int64 b);
extern int64 __stdcall x86ThreeWayCMPC_L(int64 a, int64 b);
extern int64 __stdcall x86Extract64Bit(int64 a, int b);
#elif defined(LINUX) || defined(FREEBSD)
extern "C" {
extern void x86Mul64Bit(void);
extern void x86Div64Bit(void);
extern void x86Mod64Bit(void);
extern void x86Shl64Bit(void);
extern void x86Shr64Bit(void);
extern void x86Sar64Bit(void);
extern void x86ThreeWayCMP_L(void);
extern void x86ThreeWayCMPC_L(void) {trespass("Not implemented");}
extern void x86Extract64Bit(void);
};
#endif

#if !defined(WIN32) && !defined(LINUX) && !defined(FREEBSD)
static void x86Mul64Bit() {trespass("Not implemented");}
static void x86Div64Bit() {trespass("Not implemented");}
static void x86Mod64Bit() {trespass("Not implemented");}
static void x86Shl64Bit() {trespass("Not implemented");}
static void x86Shr64Bit() {trespass("Not implemented");}
static void x86Sar64Bit() {trespass("Not implemented");}
static void x86ThreeWayCMP_L() {trespass("Not implemented");}
static void x86ThreeWayCMPC_L() {trespass("Not implemented");}
static void x86Extract64Bit()	{trespass("Not implemented");}
#endif

void x86Win32Emitter::
emit_Mul_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Mul64Bit);
}

void x86Win32Emitter::
emit_Div_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Div64Bit);
}

void x86Win32Emitter::
emit_Mod_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Mod64Bit);
}

void x86Win32Emitter::
emit_3wayCmpL_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86ThreeWayCMP_L, &(inPrimitive.nthInputVariable(0)));
}

void x86Win32Emitter::
emit_3wayCmpCL_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86ThreeWayCMP_L, &(inPrimitive.nthInputVariable(0)));
}

void x86Win32Emitter::
emit_Shl_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Shl64Bit);
}

void x86Win32Emitter::
emit_Shr_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Shr64Bit);
}
 
void x86Win32Emitter::
emit_Sar_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Sar64Bit);
}

void x86Win32Emitter::
emit_Ext_L(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)())&x86Extract64Bit);
}

void x86Win32Emitter::			
emit_ConvL_I(Primitive& inPrimitive)
{
	// make a copy of the input and precolour it to EAX
	InsnDoubleOpDir& copyOfInput = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(0), copyOfInput, 0);
	VirtualRegister& vrIn = defineTemporary(copyOfInput, 0);
	vrIn.preColorRegister(x86GPRToColor[EAX]);

	// create CDQ instruction, use vrIn and define vrHi and vrLo
	InsnNoArgs& insnCdq = *new(mPool) InsnNoArgs(&inPrimitive, mPool, opCdq, 1, 2);
	useTemporaryVR(insnCdq, vrIn, 0);
	VirtualRegister& vrHi = defineTemporary(insnCdq, 0);
	VirtualRegister& vrLo = defineTemporary(insnCdq, 1);

	// precolour vrHi and vrLo
	vrHi.preColorRegister(x86GPRToColor[EDX]);
	vrLo.preColorRegister(x86GPRToColor[EAX]);
	
	// make copies of vrHi and vrLo and define the producer
	InsnDoubleOpDir& copyHi = newCopyInstruction(inPrimitive, mPool);
	useTemporaryVR(copyHi, vrHi, 0);
	defineProducer(inPrimitive, copyHi, 0, vrcInteger, vidHigh);

	InsnDoubleOpDir& copyLo = newCopyInstruction(inPrimitive, mPool);
	useTemporaryVR(copyLo, vrLo, 0);
	defineProducer(inPrimitive, copyLo, 0, vrcInteger, vidLow);
}

void x86Win32Emitter::			
emit_ConvI_L(Primitive& inPrimitive)
{
	// make a copy of the low register of the input
	InsnDoubleOpDir& copyOfInput = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(0), copyOfInput, 0, vrcInteger, vidLow);
	defineProducer(inPrimitive, copyOfInput, 0);
}

void x86Win32Emitter::
emit_Ld_L(Primitive& inPrimitive)
{
	InsnDoubleOpDir& insnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, atRegisterIndirect, atRegDirect, 2, 1);
	useProducer(inPrimitive.nthInputVariable(0), insnLo, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), insnLo, 0);		// address
	defineProducer(inPrimitive, insnLo, 0, vrcInteger, vidLow);		// -> lo

	InsnDoubleOpDir& insnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, 4, atRegisterIndirect, atRegDirect, 2, 1);
	useProducer(inPrimitive.nthInputVariable(0), insnHi, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), insnHi, 0);		// address
	defineProducer(inPrimitive, insnHi, 0, vrcInteger, vidHigh);	// -> hi
}

void x86Win32Emitter::
emit_St_L(Primitive& inPrimitive)
{
	InsnDoubleOpDir& insnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, atRegisterIndirect, atRegDirect, 2, 1);
	useProducer(inPrimitive.nthInputVariable(1), insnLo, 0);						// address
	useProducer(inPrimitive.nthInputVariable(2), insnLo, 1, vrcInteger, vidLow);	// <- lo
// FIX for now these are commented out -- see comment below
//	useProducer(inPrimitive.nthInputVariable(0), insnLo, 2);						// memory edge in
//	defineProducer(inPrimitive, insnLo, 0);											// memory edge out
	InstructionDefine& define = defineTemporaryOrder(insnLo, 0);

	// FIX for now we are using the memory edge out from lo as the memory edge in for hi.
	// This is because the memory edges don't have a clue about high/low registers
	InsnDoubleOpDir& insnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, 4, atRegisterIndirect, atRegDirect, 4, 1);
	useProducer(inPrimitive.nthInputVariable(1), insnHi, 0);						// address
	useProducer(inPrimitive.nthInputVariable(2), insnHi, 1, vrcInteger, vidHigh);	// <- hi
	useProducer(inPrimitive.nthInputVariable(0), insnHi, 2);						// memory edge in
	useTemporaryOrder(insnHi, define, 3);
	defineProducer(inPrimitive, insnHi, 0);											// memory edge out
}

//-----------------------------------------------------------------------------------------------------------
void x86Win32Emitter::
emit_Break(Primitive& inPrimitive)
{
	new(mPool) InsnNoArgs(&inPrimitive, mPool, opBreak, 0, 0);
}

//-----------------------------------------------------------------------------------------------------------
// Comparisons

void x86Win32Emitter::
emit_Cmp_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCmp);
	newInsn.standardUseDefine(*this);
}

void x86Win32Emitter::
emit_CmpI_I(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);

	if(constant == 0) 
	{
		x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, srmCmpImm0, 0, 1, 1 );
		newInsn.standardUseDefine(*this);
	} else {
		x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaCmpImm, constant, atRegDirect, 1, 1 );
		newInsn.standardUseDefine(*this);
	}
}

//-----------------------------------------------------------------------------------------------------------
// Div and Mod are very similar in X86, so we try to factor out as much as we can

// Normal Addressing Modes
void x86Win32Emitter::emit_Div_I(Primitive& p)			{ genDivBackEnd(genDivMod_FrontEnd(p, eIDiv)); }
void x86Win32Emitter::emit_DivU_I(Primitive& p)			{ genDivBackEnd(genDivMod_FrontEnd(p, eDiv )); }
void x86Win32Emitter::emit_Mod_I(Primitive& p)			{ genModBackEnd(genDivMod_FrontEnd(p, eIDiv)); }
void x86Win32Emitter::emit_ModU_I(Primitive& p)			{ genModBackEnd(genDivMod_FrontEnd(p, eDiv )); }

// Displaced, Scaled, Indexed Addressing Mode
void x86Win32Emitter::emit_Div_I_MemDSI(Primitive& p)	{ genDivBackEnd(genDivMod_FrontEnd_MemDSI(p, eIDiv)); }
void x86Win32Emitter::emit_DivU_I_MemDSI(Primitive& p)	{ genDivBackEnd(genDivMod_FrontEnd_MemDSI(p, eDiv )); }
void x86Win32Emitter::emit_Mod_I_MemDSI(Primitive& p)	{ genModBackEnd(genDivMod_FrontEnd_MemDSI(p, eIDiv)); }
void x86Win32Emitter::emit_ModU_I_MemDSI(Primitive& p)	{ genModBackEnd(genDivMod_FrontEnd_MemDSI(p, eDiv )); }

// does div, mod, divU, modU
//  div divides dividend in EAX:EDX by divisor in reg/mem
x86Instruction&  x86Win32Emitter::
genDivMod_FrontEnd(Primitive& inPrimitive, x86ExtendedType insnType)
{
 	// make a copy of the dividend (since it will be eventually overwritten)
	InsnDoubleOpDir& dividendCopy = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(0), dividendCopy, 0);
	VirtualRegister& vrEAXin = defineTemporary(dividendCopy, 0);
	vrEAXin.preColorRegister(x86GPRToColor[EAX]);

	// convert to quad word
	InsnNoArgs& insnCdq = *new(mPool) InsnNoArgs(&inPrimitive, mPool, opCdq, 1, 2);
	useTemporaryVR(insnCdq, vrEAXin, 0);	
	VirtualRegister& vrEAXOut = defineTemporary(insnCdq, 0);
	VirtualRegister& vrEDXOut = defineTemporary(insnCdq, 1);
	vrEAXOut.preColorRegister(x86GPRToColor[EAX]);
	vrEDXOut.preColorRegister(x86GPRToColor[EDX]);

	// divide
	x86Instruction& insnDivide = *new(mPool)x86Instruction(&inPrimitive, mPool, insnType, atRegDirect, 3, 2);
	useProducer(inPrimitive.nthInputVariable(1), insnDivide, 0);
	useTemporaryVR(insnDivide, vrEAXOut, 1); 
	useTemporaryVR(insnDivide, vrEDXOut, 2);

	return insnDivide;
}

// poDiv_I(Vint, MemDSI)
//    dividend / divisor
x86Instruction& x86Win32Emitter::
genDivMod_FrontEnd_MemDSI(Primitive& inPrimitive, x86ExtendedType insnType)
{
	// get the DSI parameters
	DataNode& loadPrimitive = inPrimitive.nthInputVariable(1);

	MemDSIParameters parms(loadPrimitive);
	assert(parms.scale <= 3);		// eventually MemDSI will be limited to scale <=3

 	// make a copy of the dividend (since it will be eventually overwritten)
	InsnDoubleOpDir& dividendCopy = newCopyInstruction(inPrimitive, mPool);
	useProducer(inPrimitive.nthInputVariable(0), dividendCopy, 0);
	VirtualRegister& vrEAXin = defineTemporary(dividendCopy, 0);
	vrEAXin.preColorRegister(x86GPRToColor[EAX]);		// input must be in EAX

	// convert to quad word
	InsnNoArgs&  insnCdq = *new(mPool) InsnNoArgs(&inPrimitive, mPool, opCdq, 1, 2);
	useTemporaryVR(insnCdq, vrEAXin, 0);	
	VirtualRegister& vrEAXOut = defineTemporary(insnCdq, 0);
	VirtualRegister& vrEDXOut = defineTemporary(insnCdq, 1);
	vrEAXOut.preColorRegister(x86GPRToColor[EAX]);
	vrEDXOut.preColorRegister(x86GPRToColor[EDX]);

	// divide
	x86Instruction& insnDivide = 
		*new(mPool)x86Instruction(&inPrimitive, mPool, insnType, memDSIAddressingModes[parms.scale], parms.displacement, 5, 2);

	useProducer(parms.baseProducer, insnDivide, 0);					// base
	useProducer(parms.indexProducer, insnDivide, 1);				// index
	useTemporaryVR(insnDivide, vrEAXOut, 2);						// EAX
	useTemporaryVR(insnDivide, vrEDXOut, 3);						// EDX
	useProducer(loadPrimitive.nthInputVariable(0), insnDivide, 4);	// memory edge in

	return insnDivide;
}

// backends for div and mod
void x86Win32Emitter::
genDivBackEnd(x86Instruction& inInsn)
{
	DataNode& primitive = *(inInsn.getPrimitive());
	VirtualRegister *vrEAX, *vrEDX;

	vrEAX = &defineTemporary(inInsn, 0);
	vrEAX->preColorRegister(x86GPRToColor[EAX]);

	InsnDoubleOpDir& insnCopy = newCopyInstruction(primitive, mPool);
	useTemporaryVR(insnCopy, *vrEAX, 0);
	defineProducer(primitive, insnCopy, 0);

	vrEDX = &defineTemporary(inInsn, 1);			// unused result
	vrEDX->preColorRegister(x86GPRToColor[EDX]);
}

void x86Win32Emitter::
genModBackEnd(x86Instruction& inInsn)
{
	DataNode& primitive = *(inInsn.getPrimitive());
	VirtualRegister *vrEAX, *vrEDX;

	vrEDX = &defineTemporary(inInsn, 1);
	vrEDX->preColorRegister(x86GPRToColor[EDX]);

	InsnDoubleOpDir& insnCopy = newCopyInstruction(primitive, mPool);
	useTemporaryVR(insnCopy, *vrEDX, 0);
	defineProducer(primitive, insnCopy, 0);

	vrEAX = &defineTemporary(inInsn, 0);			// unused result
	vrEAX->preColorRegister(x86GPRToColor[EAX]);
}

//-----------------------------------------------------------------------------------------------------------

void x86Win32Emitter::
emit_LoadConstant_I(Primitive& inPrimitive)
{
	Uint32 constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.i;

	x86Instruction*	newInsn;

	if(constant == 0)
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, srmMoveImm0, 0, 0, 1);
	else
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceMoveImm, constant, atRegDirect, 0, 1);

	defineProducer(inPrimitive, *newInsn, 0);	
}

void x86Win32Emitter::
emit_LoadConstant_L(Primitive& inPrimitive)
{
	Int64 constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.l;
	// FIX make better
	Int32 low = (Int32)((Uint64) constant & 0xFFFFFFFF);
	Int32 high = (Int32)((Uint64)constant >> 32);

	x86Instruction* loInsn;
	x86Instruction* hiInsn;

	if(low == 0)
		loInsn = new(mPool) x86Instruction(&inPrimitive, mPool, srmMoveImm0, 0, 0, 1);
	else
		loInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceMoveImm, low, atRegDirect, 0, 1);
	defineProducer(inPrimitive, *loInsn, 0, vrcInteger, vidLow);

	if(high == 0)
		hiInsn = new(mPool) x86Instruction(&inPrimitive, mPool, srmMoveImm0, 0, 0, 1);
	else
		hiInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceMoveImm, high, atRegDirect, 0, 1);
	defineProducer(inPrimitive, *hiInsn, 0, vrcInteger, vidHigh);
}

void x86Win32Emitter::
emit_Catch(Primitive& inPrimitive)
{
	// create a define instruction which tells the register allocator that the register
	// is defined elsewhere (in the exception support code)
	InsnExternalDefine& defineInsn = *new(mPool) InsnExternalDefine(&inPrimitive, mPool, 1);

	// define an outgoing edge which is a vr precolored to EAX
	VirtualRegister&	exceptionObj = defineTemporary(defineInsn, 0);	
	exceptionObj.preColorRegister(x86GPRToColor[ECX]); // FIX should be eax (hack to work around reg alloc bug

	// now create a buffer copy between the precolored EAX and define the outgoing
	// VR with the outgoing edge of the Catch primitive
	InsnDoubleOpDir& copyInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegDirect, atRegDirect, 1, 1);
	useTemporaryVR(copyInsn, exceptionObj, 0); 
	defineProducer(inPrimitive, copyInsn, 0);
}

void x86Win32Emitter::
emit_LoadAddress(Primitive& inPrimitive)
{
	addr a= (*static_cast<const PrimConst *>(&inPrimitive)).value.a;
	Uint32 constant = (Uint32)addressFunction(a);
	x86Instruction&	newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, ceMoveImm, constant, atRegDirect, 0, 1);
	defineProducer(inPrimitive, newInsn, 0);	
}

void x86Win32Emitter::
emit_Ext_I(Primitive& inPrimitive)
{
	Uint32 shiftAmount = 32 - nthInputConstantUint32(inPrimitive, 1);

	// shift left by shiftAmount
	x86Instruction&	shl = *new(mPool) x86Instruction(&inPrimitive, mPool, eShlImm, shiftAmount, atRegDirect, 1, 1);
	shl.x86StandardUseDefine(*this);

	// sar by shiftAmount
	x86Instruction&	sar = *new(mPool) x86Instruction(&inPrimitive, mPool, eSarImm, shiftAmount, atRegDirect, 1, 1);
	useProducer(inPrimitive, sar, 0);
	defineProducer(inPrimitive, sar, 0);
}

// Method:	emit_Switch
// Purpose:	emit a switch
void x86Win32Emitter::
emit_Switch(Primitive& inPrimitive)
{
	InsnSwitch& jmpInsn = *new InsnSwitch(&inPrimitive, mPool);
	jmpInsn.standardUseDefine(*this);
}

// Method:	getConditionCode
// Purpose:	check the condition edge we depend upon to see if it is signed or unsigned and
//			use it to select the x86 condition code
// Assumes:	condition edge is always the 0th input (currently an invariant)
inline x86ConditionCode getConditionCode(Primitive& inPrimitive, RawConditionCode rawCondType)
{
	x86ConditionCode condType;
	if(inPrimitive.nthInputVariable(0).isSignedCompare())
		condType = condList[rawCondType].ccSigned;
	else
		condType = condList[rawCondType].ccUnsigned;
	return condType;
}

// Method:	emit_B
// Purpose:	emit a conditional branch
void x86Win32Emitter::
emit_B(Primitive& inPrimitive, RawConditionCode rawCondType)
{
	// since the condition code depends on whether the comparison was signed or unsinged,
	// we need to determine whether the comparison primitive preceding this primitive was 
	// signed or unsigned and use that to select the appropriate condition code.
	x86ConditionCode condType = getConditionCode(inPrimitive, rawCondType);

	// conditional branch
	ControlNode& controlIf = *inPrimitive.getContainer();
	InsnCondBranch& newInsn = *new(mPool) InsnCondBranch(&inPrimitive, mPool, condType, controlIf);
	newInsn.standardUseDefine(*this);
}

// Method:	emit_Cond
// Purpose:	emit a condition flags -> boolean conversion
// Code must be in the form
// xor         eax,eax			WE EMIT
// cmp         ecx, value		(emitted by CMP primitive)
// sete        al				WE EMIT
// [copy	   al, tempVr]		WE EMIT (tempVR is in the outgoing edge)
//
// It is imperative that the instruction scheduler clears the destination before the comparison,
// otherwise the compare flags will be clobbered by the xor eax, eax
void x86Win32Emitter::
emit_Cond(Primitive& inPrimitive, RawConditionCode rawCondType)
{
	// see comment in emit_B
	x86ConditionCode condType = getConditionCode(inPrimitive, rawCondType);

	// clear the destination
	x86Instruction& clearInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, srmMoveImm0, 0, 0, 1);
	VirtualRegister& outputVR = defineTemporary(clearInsn,0);		
	outputVR.preColorRegister(x86GPRToColor[EAX]);

	// set instruction
	InsnSet& setInsn = *new(mPool) InsnSet(&inPrimitive, mPool, condType);
	useTemporaryVR(setInsn, outputVR, 0);							// register to store into
	useProducer(inPrimitive.nthInputVariable(0), setInsn, 1);		// condition edge
	redefineTemporary(setInsn, outputVR, 0);						// -> result

	// create buffer copy
	InsnDoubleOpDir& copyInsn = newCopyInstruction(inPrimitive, mPool);
	useTemporaryVR(copyInsn, outputVR, 0);
	defineProducer(inPrimitive, copyInsn, 0);
}

void x86Win32Emitter::
genLd_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	defineProducer(inPrimitive, newInsn, 0);						// output
}

void x86Win32Emitter::
emit_LdS_B(Primitive& inPrimitive)
{
	InsnDoubleOp& newInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMovSxB, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	defineProducer(inPrimitive, newInsn, 0);						// output
}

void x86Win32Emitter::
emit_LdU_B(Primitive& inPrimitive)
{
	InsnDoubleOp& newInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMovZxB, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	defineProducer(inPrimitive, newInsn, 0);						// output
}

void x86Win32Emitter::
emit_LdS_H(Primitive& inPrimitive)
{
	InsnDoubleOp& newInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMovSxH, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	defineProducer(inPrimitive, newInsn, 0);						// output
}

void x86Win32Emitter::
emit_LdU_H(Primitive& inPrimitive)
{
	InsnDoubleOp& newInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMovZxH, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	defineProducer(inPrimitive, newInsn, 0);						// output
}

void x86Win32Emitter::
genLdC_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, atRegisterIndirect, atRegDirect, 1, 1);
	defineProducer(inPrimitive, newInsn, 0);
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 0);

	/*	FIX pete -- why
		put a flag in for exceptions here!!!!
		use curIndex as the place to put it 	*/	
}

// poLimit(Vint, Vint)	throw if va >= vb  (ie ensure va < vb) OK
// form cmp r1, r2		throw if r1 >= r2	 r1 nb r2
// so skip if !nb ==> b
void x86Win32Emitter::
emit_Limit(Primitive& inPrimitive)
{
	// compare
	InsnDoubleOpDir& compare = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCmp);
	useProducer(inPrimitive.nthInputVariable(0), compare, 0);
	useProducer(inPrimitive.nthInputVariable(1), compare, 1);
	compare.addDefine(0, udCond);

	// branch
	// jump to exception handler >=   ie jnb
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, ccJB, (void (*)())sysThrowArrayIndexOutOfBoundsException);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &compare;
	inPrimitive.setInstructionRoot(&branch);
}

// Common code for limCast, chkCast and chkNull primitives
void x86Win32Emitter::
emit_ExceptionCheck(Primitive& inPrimitive, x86ConditionCode condType, Uint32 constant, void (*throwExceptionFunction)())
{
	x86Instruction&	trap = *new(mPool) x86Instruction(&inPrimitive, mPool, iaCmpImm, constant, atRegDirect, 1, 1);
	useProducer(inPrimitive.nthInputVariable(0), trap, 0);
	trap.addDefine(0, udCond);

	// branch
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, condType, throwExceptionFunction);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &trap;
	inPrimitive.setInstructionRoot(&branch);
}

// poChkCast_I(vint, Vint) or poChkCast_A(Vptr, Vptr)
void x86Win32Emitter::
emit_ChkCast(Primitive& inPrimitive)
{
	InsnDoubleOpDir& trap = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCmp);
	useProducer(inPrimitive.nthInputVariable(0), trap, 0);
	useProducer(inPrimitive.nthInputVariable(1), trap, 1);
	trap.addDefine(0, udCond);

	// branch
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, ccJE, (void (*)())sysThrowClassCastException);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &trap;
	inPrimitive.setInstructionRoot(&branch);
}

// poChkCast_A(Vptr, Cptr)
void x86Win32Emitter::
emit_ChkCast_Const(Primitive& inPrimitive)
{
    Uint32 classPtr = nthInputConstantUint32(inPrimitive, 1);
    emit_ExceptionCheck(inPrimitive, ccJE, classPtr, (void (*)())sysThrowClassCastException);
}

void x86Win32Emitter::
emit_LimCast(Primitive& inPrimitive)
{
    Uint32 numVTableEntries = nthInputConstantUint32(inPrimitive, 1);
    emit_ExceptionCheck(inPrimitive, ccJNB, numVTableEntries, (void (*)())sysThrowClassCastException);
}

void x86Win32Emitter::
emit_ChkNull(Primitive& inPrimitive)
{
    emit_ExceptionCheck(inPrimitive, ccJNE, 0, (void (*)())sysThrowNullPointerException);
}

void x86Win32Emitter::
emit_Result_I(Primitive& inPrimitive)
{
	InsnDoubleOpDir& copyInsn = newCopyInstruction(inPrimitive, mPool);
	InsnExternalUse& extInsn = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);

	useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0);
	VirtualRegister& resultReg = defineTemporary(copyInsn, 0);
	resultReg.preColorRegister(x86GPRToColor[EAX]);
	useTemporaryVR(extInsn, resultReg, 0);
	inPrimitive.setInstructionRoot(&extInsn);
}

void x86Win32Emitter::
emit_Result_L(Primitive& inPrimitive)
{
	// Low
	InsnDoubleOpDir& copyLo = newCopyInstruction(inPrimitive, mPool, 1, 2);
	InsnExternalUse& extLoInsn = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);

	useProducer(inPrimitive.nthInputVariable(0), copyLo, 0, vrcInteger, vidLow);
	VirtualRegister& vrLo = defineTemporary(copyLo, 0);
	vrLo.preColorRegister(x86GPRToColor[EAX]);

	useTemporaryVR(extLoInsn, vrLo, 0);

	// High
	InsnDoubleOpDir& copyHi = newCopyInstruction(inPrimitive, mPool, 2, 1);
	InsnExternalUse& extHiInsn = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);

	useProducer(inPrimitive.nthInputVariable(0), copyHi, 0, vrcInteger, vidHigh);
	VirtualRegister& vrHi = defineTemporary(copyHi, 0);
	vrHi.preColorRegister(x86GPRToColor[EDX]);

	useTemporaryVR(extHiInsn, vrHi, 0);
	useTemporaryOrder(copyHi, defineTemporaryOrder(copyLo, 1), 1);

	inPrimitive.setInstructionRoot(&extHiInsn);
}

// poLimit(poConst_I, Vint)	throw if poConst_I >= Vint OK
// form cmp reg, 10			throw if poConst_I <= Vint	jbe
// so skip if !be ==> jnbe
void x86Win32Emitter::
emit_LimitR(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);
	
	// compare
	x86Instruction&	compare = *new(mPool) x86Instruction(&inPrimitive, mPool, iaCmpImm, constant, atRegDirect, 1, 1 );
	useProducer(inPrimitive.nthInputVariable(1), compare, 0);
	compare.addDefine(0, udCond);

	// branch
	// reversed -- jump to exception handler if below or equal ie ccJBE (ok)
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, ccJNBE, (void (*)())sysThrowArrayIndexOutOfBoundsException);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &compare;
	inPrimitive.setInstructionRoot(&branch);
}

void x86Win32Emitter::
emit_St_I(Primitive& inPrimitive)	
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, atRegisterIndirect, atRegDirect, 3, 1);
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// adress
	useProducer(inPrimitive.nthInputVariable(2), newInsn, 1);		// data
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 2);		// memory edge in
	defineProducer(inPrimitive, newInsn, 0);						// memory edge out
}

void x86Win32Emitter::
emit_St_B(Primitive& inPrimitive)	
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreB, atRegisterIndirect, atRegDirect, 3, 1);
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address

	// FIX precolouring until register classes are introduced
	InsnDoubleOpDir& copyInsn = newCopyInstruction(inPrimitive, mPool);

	useProducer(inPrimitive.nthInputVariable(2), copyInsn, 0);		// data
	VirtualRegister& copyOfInput = defineTemporary(copyInsn, 0);

	copyOfInput.preColorRegister(x86GPRToColor[EAX]);

	useProducer(inPrimitive.nthInputVariable(0), newInsn, 2);		// memory edge in
	useTemporaryVR(newInsn, copyOfInput, 1);						// precolored register containing the data
	defineProducer(inPrimitive, newInsn, 0);						// memory edge out
}

void x86Win32Emitter::
emit_St_H(Primitive& inPrimitive)	
{
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreH, atRegisterIndirect, atRegDirect, 3, 1);
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// adress
	useProducer(inPrimitive.nthInputVariable(2), newInsn, 1);		// data
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 2);		// memory edge in
	defineProducer(inPrimitive, newInsn, 0);						// memory edge out
}

void x86Win32Emitter::
emit_StI_I(Primitive& inPrimitive)	
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 2);

	x86Instruction& newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eMoveImm, constant, atRegisterIndirect, 2, 1);
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 0);		// address
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge in
	defineProducer(inPrimitive, newInsn, 0);						// memory edge out
}

//-----------------------------------------------------------------------------------------------------------
// MemDisp
void x86Win32Emitter::
emit_St_I_MemDisp(Primitive& inPrimitive)
{
	DataNode& addSource = inPrimitive.nthInput(1).getVariable();
	Uint32 disp = nthInputConstantUint32(addSource, 1);

	// store
	InsnDoubleOpDir& newInsn = 
		*new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, disp, atRegisterIndirect, atRegDirect, 3, 1);
	useProducer(addSource.nthInputVariable(0), newInsn, 0);		// base address
	useProducer(inPrimitive.nthInputVariable(2), newInsn, 1);	// value to store
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 2);	// memory edge in
	defineProducer(inPrimitive, newInsn, 0);					// memory edge out
}

void x86Win32Emitter::
emit_StI_I_MemDisp(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 2);

	DataNode& addSource = inPrimitive.nthInput(1).getVariable();
	Uint32 disp = nthInputConstantUint32(addSource, 1);
	
	// store
	x86Instruction& newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, eMoveImm, constant, atRegisterIndirect, disp, 2, 1);
	useProducer(addSource.nthInputVariable(0), newInsn, 0);		// base address
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);	// memory edge in
	defineProducer(inPrimitive, newInsn, 0);					// memory edge out
}

void x86Win32Emitter::
emit_Ld_I_MemDisp(Primitive& inPrimitive)
{
	DataNode& addSource = inPrimitive.nthInput(1).getVariable();
	Uint32 disp = nthInputConstantUint32(addSource, 1);

	// load
	InsnDoubleOpDir& newInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, disp, atRegisterIndirect, atRegDirect);
	useProducer(addSource.nthInputVariable(0), newInsn, 0);			// base address
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 1);		// memory edge
	defineProducer(inPrimitive, newInsn, 0);						// loaded value
}

// poLimit(poConst_I, MemDisp)	throw if poConst_I >= MemDisp OK
// form cmp [eax + 4], 10		throw if MemDisp <= poConst_I	jbe
// so skip if !be ==> jnbe
void x86Win32Emitter::
emit_LimitR_MemDisp(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 0);
		
	DataNode& loadSource = inPrimitive.nthInput(1).getVariable();
	DataNode& addSource = loadSource.nthInput(1).getVariable();
	Uint32 displacement = nthInputConstantUint32(addSource, 1);
	
	x86Instruction&	compare = *new(mPool) x86Instruction(&inPrimitive, mPool, iaCmpImm, constant, atRegisterIndirect, displacement, 1, 1);
	useProducer(addSource.nthInputVariable(0), compare, 0);
	compare.addDefine(0, udCond);

	// reversed -- jump to exception handler if below ie ccJBE
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, ccJNBE, (void (*)())sysThrowArrayIndexOutOfBoundsException);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &compare;
	inPrimitive.setInstructionRoot(&branch);
}

// poLimit(Vint, MemDisp)	throw if Vint >= MemDisp   OK
// form  cmp eax, [reg + 4]	throw if Vint >= MemDisp jnb
// so skip if !nb ==> jb
void x86Win32Emitter::
emit_Limit_MemDisp(Primitive& inPrimitive)
{
	DataNode& loadSource = inPrimitive.nthInput(1).getVariable();
	DataNode& addSource = loadSource.nthInput(1).getVariable();
	Uint32 displacement = nthInputConstantUint32(addSource, 1);

	// compare
	InsnDoubleOpDir& compare = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCmp, displacement, atRegDirect, atRegisterIndirect);
	useProducer(inPrimitive.nthInputVariable(0), compare, 0);
	useProducer(addSource.nthInputVariable(0), compare, 1);
	compare.addDefine(0, udCond);

	// branch
	InsnSysCallCondBranch& branch = *new(mPool) InsnSysCallCondBranch(&inPrimitive, mPool, ccJB, (void (*)())sysThrowArrayIndexOutOfBoundsException);
	branch.addUse(0, udCond);
	branch.getInstructionUseBegin()->src = &compare;
	inPrimitive.setInstructionRoot(&branch);
}

//-----------------------------------------------------------------------------------------------------------
// MemDSI -- lots more factoring out to do

// poCmp_I(MemDSI, Vint)
void x86Win32Emitter::
emit_Cmp_I_MemDSI(Primitive& inPrimitive)
{
	DataNode& loadPrimitive = inPrimitive.nthInput(0).getVariable();
	MemDSIParameters parms(loadPrimitive);
	assert(parms.scale <= 3);		// eventually MemDSI will be limited to scale <=3

	InsnDoubleOpDir& newInsn = 
		*new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCmp, parms.displacement, memDSIAddressingModes[parms.scale], atRegDirect, 4, 1);
	useProducer(parms.baseProducer, newInsn, 0);							// <- base
	useProducer(parms.indexProducer, newInsn, 1);							// <- index
	useProducer(inPrimitive.nthInputVariable(1), newInsn, 2);				// <- compare value
	useProducer(loadPrimitive.nthInputVariable(0), newInsn, 3);				// <- memory
	defineProducer(inPrimitive, newInsn, 0);								// -> compare result
}

void x86Win32Emitter::
emit_CmpI_I_MemDSI(Primitive& inPrimitive)
{
	Uint32 constant = nthInputConstantUint32(inPrimitive, 1);

	DataNode& loadPrimitive = inPrimitive.nthInput(0).getVariable();
	MemDSIParameters parms(loadPrimitive);
	assert(parms.scale <= 3);		// eventually MemDSI will be limited to scale <=3

	x86Instruction& newInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaCmpImm, constant, memDSIAddressingModes[parms.scale], parms.displacement, 3, 1);
	useProducer(parms.baseProducer, newInsn, 0);							// <- base
	useProducer(parms.indexProducer, newInsn, 1);							// <- index
	useProducer(loadPrimitive.nthInputVariable(0), newInsn, 2);				// <- memory
	defineProducer(inPrimitive, newInsn, 0);								// -> compare result
}

void x86Win32Emitter::
emit_St_I_MemDSI(Primitive& inPrimitive)
{
	MemDSIParameters parms(inPrimitive);
	assert(parms.scale <= 3);		// eventually MemDSI will be limited to scale <=3

	InsnDoubleOpDir& newInsn = 
		*new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, parms.displacement, memDSIAddressingModes[parms.scale], atRegDirect, 4, 1);
	useProducer(parms.baseProducer, newInsn, 0);							// <- base
	useProducer(parms.indexProducer, newInsn, 1);							// <- index
	useProducer(inPrimitive.nthInputVariable(2), newInsn, 2);				// <- value reg
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 3);				// <- memory edge
	defineProducer(inPrimitive, newInsn, 0);								// -> memory edge
}

void x86Win32Emitter::
emit_Ld_I_MemDSI(Primitive& inPrimitive)
{
	MemDSIParameters parms(inPrimitive);
	assert(parms.scale <= 3);		// eventually MemDSI will be limited to scale <=3

	InsnDoubleOpDir& newInsn = 
		*new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, parms.displacement, memDSIAddressingModes[parms.scale], atRegDirect, 3, 1);

	useProducer(parms.baseProducer, newInsn, 0);							// <- base
	useProducer(parms.indexProducer, newInsn, 1);							// <- index
	useProducer(inPrimitive.nthInputVariable(0), newInsn, 2);				// <- memory edge
	defineProducer(inPrimitive, newInsn, 0);								// -> value
}

//-----------------------------------------------------------------------------------------------------------
// Monitors

void x86Win32Emitter::
emit_MonitorEnter(Primitive& inPrimitive)
{
	// temporary, until we get syscall guard frames working on linux
#ifdef _WIN32
	new(mPool) CallS_V(&inPrimitive, mPool, CallS_V::numberOfArguments(inPrimitive), true, *this, (void (*)())guardedsysMonitorEnter); 
#else
	new(mPool) CallS_V(&inPrimitive, mPool, CallS_V::numberOfArguments(inPrimitive), true, *this, (void (*)())sysMonitorEnter); 
#endif
}

void x86Win32Emitter::
emit_MonitorExit(Primitive& inPrimitive)
{
#ifdef _WIN32
	new(mPool) CallS_V(&inPrimitive, mPool, CallS_V::numberOfArguments(inPrimitive), true, *this, (void (*)())guardedsysMonitorExit);
#else
	new(mPool) CallS_V(&inPrimitive, mPool, CallS_V::numberOfArguments(inPrimitive), true, *this, (void (*)())sysMonitorExit);
#endif
}
