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
//	x86Win32Emitter.h
//
//	Peter DeSantis
//	Simon Holmes a Court
//

#ifndef X86_WIN32_EMITTER
#define X86_WIN32_EMITTER

#include "InstructionEmitter.h"
#include "VirtualRegister.h"
#include "ControlNodes.h"

#include "x86Opcode.h"

class x86Instruction;
class InsnFloatMemory;
class x86ArgListInstruction;
class InsnDoubleOpDir;

//-----------------------------------------------------------------------------------------------------------
inline Uint32 nthInputConstantUint32(/* const */ DataNode& inPrimitive, size_t inWhich)
{ 
    PrimConst* primConst = static_cast<PrimConst*>(&inPrimitive.nthInputVariable(inWhich));
    assert(primConst->hasCategory(pcConst));

	Uint32 constant;
	bool test = extractU32(primConst->value, constant);
	assert(test);
	return constant;
}

//-----------------------------------------------------------------------------------------------------------
// MemDSIParameters
// common operations performed by all MemDSI modes
class MemDSIParameters
{
	public:
		DataNode&  addImmPrimitive;
		DataNode&  addPrimitive;
		DataNode&  shiftPrimitive;

		DataNode& baseProducer;
		DataNode& indexProducer;

		uint32 displacement;
		uint32 scale;

		MemDSIParameters(DataNode& inNode) :		// should be load or store
			addImmPrimitive(inNode.nthInputVariable(1)),
			addPrimitive(addImmPrimitive.nthInputVariable(0)),
			shiftPrimitive(addPrimitive.nthInputVariable(1)),
			baseProducer(addPrimitive.nthInputVariable(0)),
			indexProducer(shiftPrimitive.nthInputVariable(0))
		{
			displacement = nthInputConstantUint32(addImmPrimitive, 1);
			scale = nthInputConstantUint32(shiftPrimitive, 1);
		}
};

//-----------------------------------------------------------------------------------------------------------
enum x86AddressModeType
{
	amNormal,
	amMemDisp,
	amMemDSI		
};

enum RawConditionCode
{
	rawLt,
	rawEq,
	rawLe,
	rawGt,
	rawLgt,
	rawGe
};

//-----------------------------------------------------------------------------------------------------------
// x86Win32Emitter
class x86Win32Emitter :
	public InstructionEmitter
{
	friend class x86Instruction;
	friend class x86Formatter;

public:
					x86Win32Emitter(Pool& inPool, VirtualRegisterManager& vrMan) : 
					  InstructionEmitter(inPool, vrMan)
					  { }

	void	emitPrimitive(Primitive& inPrimitive, NamedRule inRule);

	VirtualRegister&		emit_CopyOfInput(x86ArgListInstruction& inInsn, DataNode& inPrimitive, Uint8 inWhichInput, VirtualRegisterID inID = vidLow);

	void					emitArguments(ControlNode::BeginExtra& inBeginNode);
	bool					emitCopyAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& fromVr, VirtualRegister& toVr);
	void					emitLoadAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& loadedReg, VirtualRegister& stackReg);
	void					emitStoreAfter(DataNode& inDataNode, InstructionList::iterator where, VirtualRegister& storedReg, VirtualRegister& stackReg);
	virtual Instruction&	emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget);

    void            emit_CallReturnF(InsnUseXDefineYFromPool& callInsn, DataNode& callPrimitive, DataNode& returnValProducer);
    void            emit_CallReturnD(InsnUseXDefineYFromPool& callInsn, DataNode& callPrimitive, DataNode& returnValProducer);
    void            emit_ArgF(PrimArg& arg, InstructionDefine& order, int curStackOffset);
    void            emit_ArgD(PrimArg& arg, InstructionDefine& order, int curStackOffset);

private:
	void			emit_LoadAddress(Primitive& inPrimitive);

	// break
	void			emit_Break(Primitive& inPrimitive);

	// result
	void			emit_Result_I(Primitive& inPrimitive);
	void			emit_Result_L(Primitive& inPrimitive);
    void			emit_Result_F(Primitive& inPrimitive);
	void			emit_Result_D(Primitive& inPrimitive);

	void			emit_Ld_I_MemDisp(Primitive& inPrimitive);
	void			emit_LimitR(Primitive& inPrimitive);
	void			emit_Limit(Primitive& inPrimitive);

    void            emit_ExceptionCheck(Primitive& inPrimitive, x86ConditionCode condType, Uint32 constant,
                                        void (*throwExceptionFunction)());

    void            emit_LimCast(Primitive& inPrimitive);

    void            emit_ChkCast(Primitive& inPrimitive);
    void            emit_ChkCast_Const(Primitive& inPrimitive);
	void			emit_ChkNull(Primitive& inPrimitive);

	void			emit_LoadConstant_I(Primitive& inPrimitive);
	void			emit_LoadConstant_L(Primitive& inPrimitive);

	void			emit_LoadConstant_F(Primitive& inPrimitive);
	void			emit_LoadConstant_D(Primitive& inPrimitive);
    
    void 			genLdC_I(Primitive& inPrimitive);
	void 			genLd_I(Primitive& inPrimitive);

	void 			emit_Ld_L(Primitive& inPrimitive);
	void 			emit_St_L(Primitive& inPrimitive);

	// condition consumers
	void			emit_B(Primitive& inPrimitive, RawConditionCode rawCondType);
	void			emit_Cond(Primitive& inPrimitive, RawConditionCode rawCondType);

	// logical operator helpers
	void			genLogicI_I(Primitive& inPrimitive, x86ImmediateArithType iaType);
	void			genLogic_I(Primitive& inPrimitive, x86DoubleOpDirCode raType);
	void			emit_Logic_L(Primitive& inPrimitive, x86DoubleOpDirCode insnType);

	// and
	void			emit_AndI_I(Primitive& inPrimitive);
	void			emit_And_I(Primitive& inPrimitive);
	void			emit_And_L(Primitive& inPrimitive);

	// or
	void			emit_OrI_I(Primitive& inPrimitive);
	void			emit_Or_I(Primitive& inPrimitive);
	void			emit_Or_L(Primitive& inPrimitive);

	// xor
	void			emit_XorI_I(Primitive& inPrimitive);
	void			emit_Xor_I(Primitive& inPrimitive);
	void			emit_Xor_L(Primitive& inPrimitive);

	// shift helpers
	void			genShiftI_I(Primitive& inPrimitive, x86ExtendedType eByImmediate, x86ExtendedType eBy1);
	void			genShift_I(Primitive& inPrimitive, x86ExtendedType eByCl);

	// shl
	void			emit_ShlI_I(Primitive& inPrimitive);
	void			emit_Shl_I(Primitive& inPrimitive);
	void			emit_Shl_L(Primitive& inPrimitive);

	// sar
	void			emit_SarI_I(Primitive& inPrimitive);
	void			emit_Sar_I(Primitive& inPrimitive);
	void			emit_Sar_L(Primitive& inPrimitive);

	// shr
	void			emit_ShrI_I(Primitive& inPrimitive);
	void			emit_Shr_I(Primitive& inPrimitive);
	void			emit_Shr_L(Primitive& inPrimitive);

	// mul
	void			emit_Mul_I(Primitive& inPrimitive);
//	void			emit_MulI_I(Primitive& inPrimitive);	// not yet implemented
	void			emit_Mul_L(Primitive& inPrimitive);

	void			emit_Add_I(Primitive& inPrimitive);
	void			emit_AddI_I(Primitive& inPrimitive);
	void			emit_Add_L(Primitive& inPrimitive);

	void			emit_Arithmetic_L(Primitive& inPrimitive, x86DoubleOpDirCode insnTypeLo,
                                      x86DoubleOpDirCode insnTypeHi);

	void			emit_Sub_I(Primitive& inPrimitive);
	void			emit_Sub_L(Primitive& inPrimitive);

	void			emit_SubR_I(Primitive& inPrimitive);

	void			emit_Cmp_I(Primitive& inPrimitive);
	void			emit_CmpI_I(Primitive& inPrimitive);

    void            emit_3wayCmpL_L(Primitive& inPrimitive);
    void            emit_3wayCmpCL_L(Primitive& inPrimitive);

    void            emit_3wayCmpF_L(Primitive& inPrimitive);
    void            emit_3wayCmpF_G(Primitive& inPrimitive);
    void            emit_3wayCmpD_L(Primitive& inPrimitive);
    void            emit_3wayCmpD_G(Primitive& inPrimitive);
    void            emit_3wayCmpCF_L(Primitive& inPrimitive);
    void            emit_3wayCmpCF_G(Primitive& inPrimitive);
    void            emit_3wayCmpCD_L(Primitive& inPrimitive);
    void            emit_3wayCmpCD_G(Primitive& inPrimitive);

	void			emit_Limit_MemDisp(Primitive& inPrimitive);
	void			emit_LimitR_MemDisp(Primitive& inPrimitive);
	void			emit_CmpI_I_MemDSI(Primitive& inPrimitive);
	void			emit_Cmp_I_MemDSI(Primitive& inPrimitive);

	// div/mod
	void			emit_Div_L(Primitive& inPrimitive);
	void			emit_Mod_L(Primitive& inPrimitive);

	void			emit_Div_I(Primitive& inPrimitive);
	void			emit_DivU_I(Primitive& inPrimitive);
	void			emit_Mod_I(Primitive& inPrimitive);
	void			emit_ModU_I(Primitive& inPrimitive);

	void			emit_Div_I_MemDSI(Primitive& inPrimitive);
	void			emit_DivU_I_MemDSI(Primitive& inPrimitive);
	void			emit_Mod_I_MemDSI(Primitive& inPrimitive);
	void			emit_ModU_I_MemDSI(Primitive& inPrimitive);

	// div/mod helpers
	x86Instruction& genDivMod_FrontEnd(Primitive& inPrimitive, x86ExtendedType insnType);
	x86Instruction&	genDivMod_FrontEnd_MemDSI(Primitive& inPrimitive, x86ExtendedType insnType);
	x86Instruction&	genDivMod_FrontEnd_CInt(Primitive& inPrimitive, x86ExtendedType insnType);
	void			genDivBackEnd(x86Instruction& inInsn);
	void			genModBackEnd(x86Instruction& inInsn);

	// extract
	void			emit_Ext_I(Primitive& inPrimitive);
	void			emit_Ext_L(Primitive& inPrimitive);

    // Floating Point Utilities
    void            emit_BinaryFloat(Primitive& inPrimitive,
                                     x86FloatMemoryType binary_op, x86FloatMemoryType load_op, x86FloatMemoryType store_op,
                                     VRClass vrClass);
    void            emit_BinaryFloat32(Primitive& inPrimitive,
                                       x86FloatMemoryType binary_op);
    void            emit_BinaryFloat64(Primitive& inPrimitive,
                                       x86FloatMemoryType binary_op);
    void            emit_3wayCmpF(Primitive& inPrimitive, DataNode& first_operand, DataNode& second_operand,
                                  bool negate_result, x86FloatMemoryType load_op, x86FloatMemoryType cmpOp, VRClass vrClass);

    InsnDoubleOpDir& copyFromFloatToIntegerRegister(DataNode& inDataNode, InsnUseXDefineYFromPool& defInsn);
    InsnDoubleOpDir& copyFromIntegerRegisterToFloat(DataNode& inDataNode, InsnUseXDefineYFromPool& defInsn);

    // Floating Point
	void 			emit_FAdd_F(Primitive& inPrimitive);
	void 			emit_FAdd_D(Primitive& inPrimitive);
    void 			emit_FMul_F(Primitive& inPrimitive);
	void 			emit_FMul_D(Primitive& inPrimitive);
	void 			emit_FSub_F(Primitive& inPrimitive);
	void 			emit_FSub_D(Primitive& inPrimitive);    
    void 			emit_FDiv_F(Primitive& inPrimitive);
	void 			emit_FDiv_D(Primitive& inPrimitive);

    void            emit_FRem_F(Primitive& inPrimitive);
    void            emit_FRem_D(Primitive& inPrimitive);

	// convert
	void			emit_ConvI_L(Primitive& inPrimitive);
	void			emit_ConvL_I(Primitive& inPrimitive);

    void            emit_FConv(Primitive& inPrimitive);

	// load
	void			emit_Ld_I_MemDSI(Primitive& inPrimitive);
	void			emit_LdS_B(Primitive& inPrimitive);
	void			emit_LdU_B(Primitive& inPrimitive);
	void			emit_LdS_H(Primitive& inPrimitive);
	void			emit_LdU_H(Primitive& inPrimitive);

    void            emit_Ld_F(Primitive& inPrimitive);
    void            emit_Ld_D(Primitive& inPrimitive);

	// store
	void			emit_St_B(Primitive& inPrimitive);
	void			emit_St_H(Primitive& inPrimitive);
	void			emit_StI_I(Primitive& inPrimitive);
	void			emit_St_I(Primitive& inPrimitive);
	void			emit_St_I_MemDSI(Primitive& inPrimitive);
	void			emit_StI_I_MemDisp(Primitive& inPrimitive);
	void			emit_St_I_MemDisp(Primitive& inPrimitive);

    void			emit_St_F(Primitive& inPrimitive);
	void			emit_St_D(Primitive& inPrimitive);

	// catch
	void			emit_Catch(Primitive& inPrimitive);

	// switch
	void			emit_Switch(Primitive& inPrimitive);

	// monitors
	void			emit_MonitorEnter(Primitive& inPrimitive);
	void			emit_MonitorExit(Primitive& inPrimitive);
};

#endif	// X86_WIN32_EMITTER
