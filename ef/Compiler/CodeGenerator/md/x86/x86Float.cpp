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
//  x86Float.cpp - Floating-point code-generation for x86 processors
//

#include "x86Float.h"
#include "x86Win32Emitter.h"
#include "FloatUtils.h"

// Note: In comments below, TOS = Top of FPU stack

//====================================================================================================
// Register classes for float and double operands
// FIXME - For now, all float and double virtual registers are allocated on the stack
const VRClass vrcFloat = vrcStackSlot;
const VRClass vrcDouble = vrcStackSlot; // FIXME - Stack slots are temporarily 8-bytes, so they can hold a double


//====================================================================================================
// Floating-point instruction classes

/*--------------------------------------------------------------------------------
Floating-Point instructions where one operand is a memory location and the other is
the top of stack, e.g. binary operations like add, and load/store/conversion instructions.

oiBaseOpcode
	contains the opcode for the instruction
	
oiOpcodeInformation
    6           set if opcode extension is used
	[5 4 3]		contain Regfield opcode extension (if bit 6 is set)
	others		set to 0
*/

// Utility macros for setting up opcode table
#define NO_EXTENSION 8   // Register extensions range from 0 to 7, so this special code indicates that
                         //  there is no R/M byte

#define FLOAT_INFO(first_opcode, reg_extension)   \
    first_opcode, ((reg_extension != NO_EXTENSION) * ((1<< 6) | ((reg_extension) << 3)))

x86OpcodeInfo InsnFloatMemory::opcodeTable[] =
{
    {FLOAT_INFO(0xD9, 3), "fstp32"},        // TOS => 32-bit float memory.  Pop TOS.
    {FLOAT_INFO(0xDD, 3), "fstp64"},        // TOS => 64-bit double memory.  Pop TOS.
    {FLOAT_INFO(0xD9, 2), "fst32"},         // TOS => 32-bit float memory. (Don't pop FPU stack.)
    {FLOAT_INFO(0xDD, 2), "fst64"},         // TOS => 64-bit double memory.  (Don't pop FPU stack.)
    {FLOAT_INFO(0xDB, 3), "fistp32"},       // Round(TOS) => 32-bit int memory. Pop TOS.
    {FLOAT_INFO(0xDF, 7), "fistp64"},       // Round(TOS) => 64-bit long memory. Pop TOS.
    {FLOAT_INFO(0xD9, 0), "fld32"},         // 32-bit float memory => Push on FPU stack
    {FLOAT_INFO(0xDD, 0), "fld64"},         // 64-bit float memory => Push on FPU stack
    {FLOAT_INFO(0xDB, 0), "fild32"},        // 32-bit int memory => convert to FP and push on FPU stack
    {FLOAT_INFO(0xDF, 5), "fild64"},        // 64-bit long memory => convert to FP and push on FPU stack
    {FLOAT_INFO(0xD8, 0), "fadd32"},        // Add TOS and 32-bit float memory => replace TOS
    {FLOAT_INFO(0xDC, 0), "fadd64"},        // Add TOS and 64-bit double memory => replace TOS
    {FLOAT_INFO(0xD8, 1), "fmul32"},        // Multiply TOS and 32-bit float memory => replace TOS
    {FLOAT_INFO(0xDC, 1), "fmul64"},        // Multiply TOS and 64-bit double memory => replace TOS
    {FLOAT_INFO(0xD8, 4), "fsub32"},        // Subtract TOS from 32-bit float memory => replace TOS
    {FLOAT_INFO(0xDC, 4), "fsub64"},        // Subtract TOS from 64-bit double memory => replace TOS
    {FLOAT_INFO(0xD8, 5), "fsubr32"},       // Subtract 32-bit float memory from TOS => replace TOS
    {FLOAT_INFO(0xDC, 5), "fsubr64"},       // Subtract 64-bit double memory from TOS => replace TOS
    {FLOAT_INFO(0xD8, 6), "fdiv32"},        // Divide TOS by 32-bit float memory => replace TOS
    {FLOAT_INFO(0xDC, 6), "fdiv64"},        // Divide TOS by 64-bit double memory => replace TOS
    {FLOAT_INFO(0xD8, 3), "fcomp32"},       // Compare TOS to 32-bit float memory, setting FPU flags, pop TOS
    {FLOAT_INFO(0xDC, 3), "fcomp64"}        // Compare TOS to 64-bit double memory, setting FPU flags, pop TOS
};

void InsnFloatMemory::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter) 
{ 
	assert(opcodeInfo != NULL && iArgumentList != NULL); 
    Uint8 *opLocation = (Uint8*)inStart;

	// Format the opcode to memory
	*opLocation++ = opcodeInfo->oiBaseOpcode;

	// Find the location of the argument list and format it to memory
	iArgumentList->alFormatToMemory((void*)opLocation, inOffset, *this, inFormatter);

	// If the opcode has an opcode extension then OR it into the proper place.  ( the reg field of the modr/m byte.)
    Uint8 regFieldExtension = kRegfield_Mask & opcodeInfo->oiOpcodeInformation;
	*opLocation |= regFieldExtension;
}

/*----------------------------------------------------------------------------------------------------------
Floating-Point instructions where source and destination are implicitly on the FPU stack
  e.g. negation, comparison

oiBaseOpcode
	contains the first byte of the opcode for the instruction
	
oiOpcodeInformation
	contains the second byte of the opcode for the instruction
*/

x86OpcodeInfo InsnFloatReg::opcodeTable[] =
{
    {0xDF, 0xE9, "fucomip   st, st(1)"},    // Compare top two operands on FPU stack and set EFLAGS, pop FPU stack
    {0xD9, 0xE0, "fchs"},                   // Negate top of stack value
    {0xDF, 0xE0, "fnstsw   ax"}             // Copy FPU status register to AX
};

void InsnFloatReg::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/) 
{ 
	// Format the opcode to memory.  There is no argument list.
    Uint8* start = (Uint8*) inStart;
    *start++ = opcodeInfo->oiBaseOpcode;
    *start = opcodeInfo->oiOpcodeInformation;
}

//====================================================================================================
// Instruction generation utilities

InsnDoubleOpDir& x86Win32Emitter::
copyFromFloatToIntegerRegister(DataNode& inDataNode, InsnUseXDefineYFromPool& defInsn)
{
    VirtualRegister& vr = defineTemporary(defInsn, 0, vrcStackSlot);
	InsnDoubleOpDir& copyInsn = *new(mPool) InsnDoubleOpDir(&inDataNode, mPool, raCopyI, atRegAllocStackSlot, atRegDirect, 1, 1);
    useTemporaryVR(copyInsn, vr, 0);
    return copyInsn;
}

InsnDoubleOpDir& x86Win32Emitter::
copyFromIntegerRegisterToFloat(DataNode& inDataNode, InsnUseXDefineYFromPool& defInsn)
{
    VirtualRegister& vr = defineTemporary(defInsn, 0);
	InsnDoubleOpDir& copyInsn = *new(mPool) InsnDoubleOpDir(&inDataNode, mPool, raCopyI, atRegDirect, atRegAllocStackSlot, 1, 1);
    useTemporaryVR(copyInsn, vr, 0);
    return copyInsn;
}

//====================================================================================================
// Floating-point binary operations, i.e. add, subtract, multiply, divide, modulus
   
void x86Win32Emitter::
emit_BinaryFloat(Primitive& inPrimitive,
                 x86FloatMemoryType binary_op, x86FloatMemoryType load_op, x86FloatMemoryType store_op,
                 VRClass vrClass)
{
    // Fetch first operand of binary op from memory and push it on the FPU stack
    InsnFloatMemory &loadInsn = *new InsnFloatMemory(&inPrimitive, mPool, load_op, 1, 1);
    useProducer(inPrimitive.nthInputVariable(0), loadInsn, 0, vrClass);
	InstructionDefine& define1 = defineTemporaryOrder(loadInsn, 0);

    // Fetch second operand and perform binary operation, result replaces top of FPU stack
    InsnFloatMemory &binaryInsn = *new InsnFloatMemory(&inPrimitive, mPool, binary_op, 2, 1);
    useProducer(inPrimitive.nthInputVariable(1), binaryInsn, 0, vrClass);
	useTemporaryOrder(binaryInsn, define1, 1);
	InstructionDefine& define2 = defineTemporaryOrder(binaryInsn, 0);

    // Pop result of binary operation from FPU stack and store into memory
    InsnFloatMemory &storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, store_op, 1, 1);
	useTemporaryOrder(storeInsn, define2, 0);
    defineProducer(inPrimitive, storeInsn, 0, vrClass);                 // result
}

// Emit 32-bit float binary operation
void x86Win32Emitter::
emit_BinaryFloat32(Primitive& inPrimitive, x86FloatMemoryType binary_op)
{
    emit_BinaryFloat(inPrimitive, binary_op, fld32, fstp32, vrcFloat);
}

// Emit 64-bit float binary operation
void x86Win32Emitter::
emit_BinaryFloat64(Primitive& inPrimitive, x86FloatMemoryType binary_op)
{
    emit_BinaryFloat(inPrimitive, binary_op, fld64, fstp64, vrcDouble);
}

void x86Win32Emitter::
emit_FAdd_F(Primitive& inPrimitive) {
    emit_BinaryFloat32(inPrimitive, fadd32);
}

void x86Win32Emitter::
emit_FAdd_D(Primitive& inPrimitive) {
    emit_BinaryFloat64(inPrimitive, fadd64);
}

void x86Win32Emitter::
emit_FMul_F(Primitive& inPrimitive) {
    emit_BinaryFloat32(inPrimitive, fmul32);
}

void x86Win32Emitter::
emit_FMul_D(Primitive& inPrimitive) {
    emit_BinaryFloat64(inPrimitive, fmul64);
}

void x86Win32Emitter::
emit_FSub_F(Primitive& inPrimitive) {
    emit_BinaryFloat32(inPrimitive, fsub32);
}

void x86Win32Emitter::
emit_FSub_D(Primitive& inPrimitive) {
    emit_BinaryFloat64(inPrimitive, fsub64);
}

void x86Win32Emitter::
emit_FDiv_F(Primitive& inPrimitive) {
    emit_BinaryFloat32(inPrimitive, fdiv32);
}

void x86Win32Emitter::
emit_FDiv_D(Primitive& inPrimitive) {
    emit_BinaryFloat64(inPrimitive, fdiv64);
}

// FIXME - Modulus is wrapper around fmod function.  Should be changed to inline code.
void x86Win32Emitter::
emit_FRem_D(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)(void))&javaFMod);
}

// Wrapper around fmod() for 32-bit float operands instead of double operands
static Flt32 fmod32(Flt32 a, Flt32 b)
{
    return (Flt32)javaFMod(a, b);
}

void x86Win32Emitter::
emit_FRem_F(Primitive& inPrimitive)
{
	new(mPool) CallS_C(&inPrimitive, mPool, 2, true, *this, (void (*)(void))&fmod32);
}


//-----------------------------------------------------------------------------------------------------------
// Conversions to/from floating-point
//
// All conversions are two steps:
//   1) Load input operand onto FPU stack from memory, with possible conversion to floating-point type
//   2) Simultaneously convert and store from top of FPU stack into memory location, with possible
//      conversion to integer type.

void x86Win32Emitter::
emit_FConv(Primitive& inPrimitive)
{
    InsnFloatMemory *loadInsn;
    
    // Fetch input operand from memory and push it on the FPU stack
    switch (inPrimitive.nthInputVariable(0).getKind()) {
    case vkFloat:
        loadInsn = new InsnFloatMemory(&inPrimitive, mPool, fld32, 1, 1);
        useProducer(inPrimitive.nthInputVariable(0), *loadInsn, 0, vrcFloat);
        break;
        
    case vkDouble:
        loadInsn = new InsnFloatMemory(&inPrimitive, mPool, fld64, 1, 1);
        useProducer(inPrimitive.nthInputVariable(0), *loadInsn, 0, vrcDouble);
        break;
        
    case vkInt:
        {
            InsnDoubleOpDir& copyInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegDirect, atRegAllocStackSlot, 1, 1);
            useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0);
            VirtualRegister& tmp = defineTemporary(copyInsn, 0, vrcStackSlot);
            
            loadInsn = new InsnFloatMemory(&inPrimitive, mPool, fild32, 1, 1);
            useTemporaryVR(*loadInsn, tmp, 0, vrcStackSlot);
        }
        break;
        
    case vkLong:
        {
            InsnDoubleOpDir& copyInsnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegDirect, atRegAllocStackSlotHi32, 1, 1);
            useProducer(inPrimitive.nthInputVariable(0), copyInsnHi, 0, vrcInteger, vidHigh);
            VirtualRegister& tmp64 = defineTemporary(copyInsnHi, 0, vrcStackSlot);

            InsnDoubleOpDir& copyInsnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegDirect, atRegAllocStackSlot, 2, 1);
            useProducer(inPrimitive.nthInputVariable(0), copyInsnLo, 0, vrcInteger, vidLow);
            useTemporaryVR(copyInsnLo, tmp64, 1);
            InstructionDefine& orderStoreLoad = defineTemporaryOrder(copyInsnLo, 0);

            loadInsn = new InsnFloatMemory(&inPrimitive, mPool, fild64, 2, 1);
            useTemporaryVR(*loadInsn, tmp64, 0, vrcStackSlot);
            useTemporaryOrder(*loadInsn, orderStoreLoad, 1);
        }
        break;
	default:
		// Absence of default case generates gcc warnings.
		break;
    }
    
    InstructionDefine& order = defineTemporaryOrder(*loadInsn, 0);
    
    // Store value from top of FPU stack into memory
    ValueKind vk = inPrimitive.getKind();
    switch (vk) {
    case vkFloat:
        {
            // Pop result from FPU stack and store into memory as 32-bit float
            InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fstp32, 1, 1);
            useTemporaryOrder(storeInsn, order, 0);
            defineProducer(inPrimitive, storeInsn, 0, vrcFloat);
        }
        break;
        
    case vkDouble:
        {
            // Pop result from FPU stack and store into memory as 64-bit double
            InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fstp64, 1, 1);
            useTemporaryOrder(storeInsn, order, 0);
            defineProducer(inPrimitive, storeInsn, 0, vrcDouble);
        }
        break;
        
    case vkInt:
    case vkLong:
        {
            /* Rounding is controlled by the RC flag in the FPU.  Round-to-nearest is the desired rounding mode for
               all floating-point instructions *except* conversions from floating-point types to integer types, in
               which case round-towards-zero (truncation) is mandated.  Rather than temporarily changing the RC flag
               for all such conversions, we achieve the equivalent result by subtracting or adding 0.5 to the value
               before rounding, i.e.
        
                   truncate(x) <==> round(x + sign(x) * 0.5)
          
               FIXME - we still don't handle out-of-range inputs and NaNs per the Java spec.
            
            */
            
            // Store the 32-bit representation of the floating-point input operand into memory so that we can extract its sign.
            InsnFloatMemory& storeInsn1 = *new InsnFloatMemory(&inPrimitive, mPool, fst32, 1, 1);
            useTemporaryOrder(storeInsn1, order, 0);
            VirtualRegister& tmpVR1 = defineTemporary(storeInsn1, 0, vrcStackSlot);

            // Extract the sign bit of the input operand
            x86Instruction&	andInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaAndImm, 0x80000000, atRegAllocStackSlot, 1, 1);
            useTemporaryVR(andInsn, tmpVR1, 0, vrcStackSlot);
            redefineTemporary(andInsn, tmpVR1, 0, vrcStackSlot);
            
            // Generate 0.5 * sign(input)
            const float half = 0.5;
            x86Instruction&	orInsn = *new(mPool) x86Instruction(&inPrimitive, mPool, iaOrImm, *(Uint32*)&half, atRegAllocStackSlot, 1, 1);
            useTemporaryVR(orInsn, tmpVR1, 0, vrcStackSlot);
            redefineTemporary(orInsn, tmpVR1, 0, vrcStackSlot);

            // Subtract 0.5 * sign(input) from input operand
            InsnFloatMemory& subInsn = *new InsnFloatMemory(&inPrimitive, mPool, fsub32, 1, 1);
            useTemporaryVR(subInsn, tmpVR1, 0, vrcStackSlot);
            redefineTemporaryOrder(subInsn, order, 0);
            
            if (vk == vkInt) {
                // Pop result from FPU stack, convert to 32-bit integer, and store into memory
                
                InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fistp32, 1, 1);
                useTemporaryOrder(storeInsn, order, 0);
                
                // All transfers from the FPU must go through memory, so make a copy from the memory location
                //   to the integer register destination.
                InsnDoubleOpDir& copyInsn = copyFromFloatToIntegerRegister(inPrimitive, storeInsn);
                defineProducer(inPrimitive, copyInsn, 0);
                
            } else { // vkLong
                // Pop result from FPU stack, convert to 64-bit integer, and store into memory
                InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fistp64, 1, 1);
                useTemporaryOrder(storeInsn, order, 0);
                VirtualRegister& tmp64 = defineTemporary(storeInsn, 0, vrcStackSlot);  // 64-bit integer stack slot
                
                // Copy high 32-bits from memory to integer register
                InsnDoubleOpDir& copyInsnHi = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegAllocStackSlotHi32, atRegDirect, 1, 1);
                useTemporaryVR(copyInsnHi, tmp64, 0, vrcStackSlot);
                defineProducer(inPrimitive, copyInsnHi, 0, vrcInteger, vidHigh);
                
                // Copy low 32-bits from memory to integer register
                InsnDoubleOpDir& copyInsnLo = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegAllocStackSlot, atRegDirect, 1, 1);
                useTemporaryVR(copyInsnLo, tmp64, 0, vrcStackSlot);
                defineProducer(inPrimitive, copyInsnLo, 0, vrcInteger, vidLow);
            }
        }
        break;
        
	default:
		// Absence of default case generates gcc warnings.
        assert(0);
		break;
    }
}

//====================================================================================================
// Floating-point function-call glue

// Obtain the 32-bit float return value of a function call
void x86Win32Emitter::
emit_CallReturnF(InsnUseXDefineYFromPool& callInsn, DataNode& callPrimitive, DataNode& returnValProducer)
{
    InstructionDefine& define = defineTemporaryOrder(callInsn, 1);
                    
    // Pop result from FPU stack and store into memory as 32-bit float
    InsnFloatMemory& storeInsn = *new InsnFloatMemory(&callPrimitive, mPool, fstp32, 1, 1);
    useTemporaryOrder(storeInsn, define, 0);
    defineProducer(returnValProducer, storeInsn, 0, vrcFloat);
}

// Obtain the 64-bit double return value of a function call
void x86Win32Emitter::
emit_CallReturnD(InsnUseXDefineYFromPool& callInsn, DataNode& callPrimitive, DataNode& returnValProducer)
{
    InstructionDefine& define = defineTemporaryOrder(callInsn, 1);
                    
    // Pop result from FPU stack and store into memory as 64-bit double
    InsnFloatMemory& storeInsn = *new InsnFloatMemory(&callPrimitive, mPool, fstp64, 1, 1);
    useTemporaryOrder(storeInsn, define, 0);
    defineProducer(returnValProducer, storeInsn, 0, vrcDouble);
}

// Retrieve a 32-bit float argument from the call stack
void x86Win32Emitter::
emit_ArgF(PrimArg& arg, InstructionDefine& order, int curStackOffset)
{
    InsnDoubleOpDir& loadParam = *new(mPool) InsnDoubleOpDir(&arg, mPool, raLoadI, curStackOffset, atStackOffset, atRegDirect, 1, 1);
    useTemporaryOrder(loadParam, order, 0);
    InsnDoubleOpDir& copyInsn = copyFromIntegerRegisterToFloat(arg, loadParam);
    defineProducer(arg, copyInsn, 0, vrcFloat);
}

// Retrieve a 64-bit double argument from the call stack
void x86Win32Emitter::
emit_ArgD(PrimArg& arg, InstructionDefine& order, int curStackOffset)
{
    InsnFloatMemory& loadInsn = *new InsnFloatMemory(&arg, mPool, fld64, atStackOffset, curStackOffset, 1, 1);
    useTemporaryOrder(loadInsn, order, 0);
    redefineTemporaryOrder(loadInsn, order, 0);

    InsnFloatMemory& copyInsn = *new InsnFloatMemory(&arg, mPool, fstp64, 1, 1);
    useTemporaryOrder(copyInsn, order, 0);
    defineProducer(arg, copyInsn, 0, vrcDouble);
}
    
// Push float function return value on top of FPU stack
void x86Win32Emitter::
emit_Result_F(Primitive& inPrimitive)
{
    InsnFloatMemory &copyInsn = *new InsnFloatMemory(&inPrimitive, mPool, fld32, 1, 1);
	InsnExternalUse& extInsn = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);

	useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0, vrcFloat);
	InstructionDefine& define = defineTemporaryOrder(copyInsn, 0);
	useTemporaryOrder(extInsn, define, 0);

	inPrimitive.setInstructionRoot(&extInsn);
}

// Push double function return value on top of FPU stack
void x86Win32Emitter::
emit_Result_D(Primitive& inPrimitive)
{
    InsnFloatMemory &copyInsn = *new InsnFloatMemory(&inPrimitive, mPool, fld64, 1, 1);
	InsnExternalUse& extInsn = *new(mPool) InsnExternalUse(&inPrimitive, mPool, 1);

	useProducer(inPrimitive.nthInputVariable(0), copyInsn, 0, vrcDouble);
	InstructionDefine& define = defineTemporaryOrder(copyInsn, 0);
	useTemporaryOrder(extInsn, define, 0);

	inPrimitive.setInstructionRoot(&extInsn);
}

//====================================================================================================
// Comparisons

// Matches pattern: poCatL_I(poFCmp_F(Vfloat, Vfloat))
/* Emits code that follows this pattern:

        fld                 ; push second_arg on FPU stack
        fcomp   [ebp + xx]  ; Load first_arg, set integer condition flags and pop all args
        fnstsw  ax          ; Copy FPU status reg into AX
        sahf                ; Copy AX into EFLAGS status reg
        seta    al          ; al =  (first_arg > second_arg) ? 1 : 0;
        setb    bl          ; bl = ((first_arg < second_arg) || (first_arg == NAN) || (second_arg == NAN)) ? 1 : 0
        sub     ebx, eax    ; Result in lowest byte is -1, 0, or +1
        movsx   eax, bl     ; Sign-extend low byte to 32-bits

    (Some changes in operand usage will appear depending on the exact pattern of primitives being matched.)
*/

void x86Win32Emitter::
emit_3wayCmpF(Primitive& inPrimitive, DataNode &first_operand, DataNode &second_operand,
              bool negate_result, x86FloatMemoryType load_op, x86FloatMemoryType cmpOp, VRClass vrClass)
{
    // Push first operand on FPU stack
    InsnFloatMemory& loadInsn2 = *new InsnFloatMemory(&inPrimitive, mPool, load_op, 1, 1);
    useProducer(first_operand, loadInsn2, 0, vrClass);
    InstructionDefine& order = defineTemporaryOrder(loadInsn2, 0);

    // Set FPU status flags
    // FIXME - Should this define a condition-flag edge ?  There really should be separate
    //         edge types for integer and FPU condition codes.
    InsnFloatMemory& cmpInsn = *new InsnFloatMemory(&inPrimitive, mPool, cmpOp, 2, 1);
    useProducer(second_operand, cmpInsn, 0, vrClass);
    useTemporaryOrder(cmpInsn, order, 1);
    redefineTemporaryOrder(cmpInsn, order, 0);

    // Copy FPU status flags to AX register
    InsnFloatReg& copyFromStatusFlagsInsn = *new InsnFloatReg(&inPrimitive, mPool, fnstsw, 1, 1);
    useTemporaryOrder(copyFromStatusFlagsInsn, order, 0);
    VirtualRegister& FPUstatus = defineTemporary(copyFromStatusFlagsInsn, 0);
	FPUstatus.preColorRegister(x86GPRToColor[EAX]);

    // sahf instruction (copy from AX into integer status flags register)
    InsnNoArgs& sahfInsn = *new(mPool) InsnNoArgs(&inPrimitive, mPool, opSahf, 1, 1);
    useTemporaryVR(sahfInsn, FPUstatus, 0);
    sahfInsn.addDefine(0, udCond);

	// setnbe instruction
	InsnSet& setInsn1 = *new(mPool) InsnSet(&inPrimitive, mPool, ccJNBE, 1, 1);
    setInsn1.addUse(0, udCond);
	setInsn1.getInstructionUseBegin()[0].src = &sahfInsn;		    // condition edge

    // setb instruction
	InsnSet& setInsn2 = *new(mPool) InsnSet(&inPrimitive, mPool, ccJB, 1, 1);
    setInsn2.addUse(0, udCond);
	setInsn2.getInstructionUseBegin()[0].src = &sahfInsn;		    // condition edge

    VirtualRegister* tmpVR1, *tmpVR2;
    if (negate_result) {
        tmpVR2 = &defineTemporary(setInsn1, 0);         // (first_operand > second_operand) -> tmpVR2
        tmpVR1 = &defineTemporary(setInsn2, 0);         // ((first_operand < second_operand) ||
                                                        //  (first_operand == NAN) ||
                                                        //  (second_operand == NAN)) -> tmpVR1
    } else {
        tmpVR1 = &defineTemporary(setInsn1, 0);         // (first_operand > second_operand) -> tmpVR1
        tmpVR2 = &defineTemporary(setInsn2, 0);         // ((first_operand < second_operand) ||
                                                        //  (first_operand == NAN) ||
                                                        //  (second_operand == NAN)) -> tmpVR2
    }
    
    // FIXME - We must store result of SET instruction in either AL, BL, CL, or DL, but there's no
    // way to indicate this restriction to the register allocator so, for now, we hard-code the registers
	tmpVR1->preColorRegister(x86GPRToColor[EAX]);
	tmpVR2->preColorRegister(x86GPRToColor[EBX]);

    // sub instruction
    InsnDoubleOpDir& subInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raSub);
	useTemporaryVR(subInsn, *tmpVR1, 0);                             
    useTemporaryVR(subInsn, *tmpVR2, 1);
    redefineTemporary(subInsn, *tmpVR1, 0);             // tmpVR1 - tmpVR2 -> tmpVR1

    // Upper 24 bits are garbage.  Sign-extend byte to 32-bits using movs instruction
	InsnDoubleOp& extInsn = *new(mPool) InsnDoubleOp(&inPrimitive, mPool, opMovSxB, atRegDirect, atRegDirect, 1, 1);
	useTemporaryVR(extInsn, *tmpVR1, 0);  
	defineProducer(inPrimitive, extInsn, 0);			// exts(tmpVR1) -> result
}

void x86Win32Emitter::
emit_3wayCmpF_G(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(0), cmpPrimitive.nthInputVariable(1),
                  false, fld32, fcomp32, vrcFloat);
}

void x86Win32Emitter::
emit_3wayCmpF_L(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(1), cmpPrimitive.nthInputVariable(0),
                  true, fld32, fcomp32, vrcFloat);
}

void x86Win32Emitter::
emit_3wayCmpD_G(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(0), cmpPrimitive.nthInputVariable(1),
                  false, fld64, fcomp64, vrcDouble);
}

void x86Win32Emitter::
emit_3wayCmpD_L(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(1), cmpPrimitive.nthInputVariable(0),
                  true, fld64, fcomp64, vrcDouble);
}

void x86Win32Emitter::
emit_3wayCmpCF_G(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(1), cmpPrimitive.nthInputVariable(0),
                  false, fld32, fcomp32, vrcFloat);
}

void x86Win32Emitter::
emit_3wayCmpCF_L(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(0), cmpPrimitive.nthInputVariable(1),
                  true, fld32, fcomp32, vrcFloat);
}

void x86Win32Emitter::
emit_3wayCmpCD_G(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(1), cmpPrimitive.nthInputVariable(0),
                  false, fld64, fcomp64, vrcDouble);
}

void x86Win32Emitter::
emit_3wayCmpCD_L(Primitive& inPrimitive)
{
    Primitive& cmpPrimitive = Primitive::cast(inPrimitive.nthInputVariable(0));
    emit_3wayCmpF(inPrimitive, cmpPrimitive.nthInputVariable(0), cmpPrimitive.nthInputVariable(1),
                  true, fld64, fcomp64, vrcDouble);
}

//====================================================================================================
// Constants

// Generate 32-bit float constant
void x86Win32Emitter::
emit_LoadConstant_F(Primitive& inPrimitive)
{
	Uint32 constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.i;

	x86Instruction*	newInsn;

	if(constant == 0)
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, srmMoveImm0, 0, 0, 1);
	else
		newInsn = new(mPool) x86Instruction(&inPrimitive, mPool, ceMoveImm, constant, atRegDirect, 0, 1);

    defineProducer(inPrimitive, copyFromIntegerRegisterToFloat(inPrimitive, *newInsn), 0, vrcFloat);    // result
}

// Generate 64-bit double constant
// FIXME: Need to create an in-memory literal pool for storing double constants, rather than using immediate instructions
void x86Win32Emitter::
emit_LoadConstant_D(Primitive& inPrimitive)
{
	Flt64 constant = (*static_cast<const PrimConst *>(&inPrimitive)).value.d;

    // Store 64-bit double constant in literal pool
    // FIXME - We should have a literal pool for each method to store in-memory constants
    //         and which can be released when the method is discarded.  
    Flt64* literalPoolEntry = (Flt64*)malloc(sizeof(Flt64));
    *literalPoolEntry = constant;
    
    // Fetch from memory and temporarily push 64-bit double on the FPU stack
    InsnFloatMemory &loadInsn = *new InsnFloatMemory(&inPrimitive, mPool, fld64, atAbsoluteAddress, (Uint32)literalPoolEntry, 0, 1);
	InstructionDefine& order = defineTemporaryOrder(loadInsn, 0);

    // Pop 64-bit double from FPU stack and store into double variable
    InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fstp64, 1, 1);
    useTemporaryOrder(storeInsn, order, 0);
    defineProducer(inPrimitive, storeInsn, 0, vrcDouble);              // result
}

//====================================================================================================
// Floating-point memory operations

void x86Win32Emitter::
emit_Ld_F(Primitive& inPrimitive)
{
    // Load 32-bit float into an integer register
    InsnDoubleOpDir& loadInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raLoadI, atRegisterIndirect, atRegDirect);
	useProducer(inPrimitive.nthInputVariable(0), loadInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), loadInsn, 0);		// address

    // Store into 32-bit float variable
    InsnDoubleOpDir& storeInsn = copyFromIntegerRegisterToFloat(inPrimitive, loadInsn);
	defineProducer(inPrimitive, storeInsn, 0, vrcFloat);            // result
}

void x86Win32Emitter::
emit_Ld_D(Primitive& inPrimitive)
{
    // Fetch from memory and temporarily push 64-bit double on the FPU stack
    InsnFloatMemory &loadInsn = *new InsnFloatMemory(&inPrimitive, mPool, fld64, atRegisterIndirect, 2, 1);
	useProducer(inPrimitive.nthInputVariable(0), loadInsn, 1);		// memory edge
	useProducer(inPrimitive.nthInputVariable(1), loadInsn, 0);		// address
	InstructionDefine& order = defineTemporaryOrder(loadInsn, 0);

    // Pop 64-bit double from FPU stack
    InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fstp64, 1, 1);
    useTemporaryOrder(storeInsn, order, 0);
    defineProducer(inPrimitive, storeInsn, 0, vrcDouble);              // result
}

void x86Win32Emitter::
emit_St_F(Primitive& inPrimitive)	
{
    // Load 32-bit float into an integer register
	InsnDoubleOpDir& loadInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raCopyI, atRegAllocStackSlot, atRegDirect, 1, 1);
    useProducer(inPrimitive.nthInputVariable(2), loadInsn, 0);		// data
    VirtualRegister& floatVal = defineTemporary(loadInsn, 0);       // intermediate output
    
    // Store temporary integer register into indirect register destination
    InsnDoubleOpDir& storeInsn = *new(mPool) InsnDoubleOpDir(&inPrimitive, mPool, raStoreI, atRegisterIndirect, atRegDirect, 3, 1);
	useProducer(inPrimitive.nthInputVariable(1), storeInsn, 0);		// address
	useTemporaryVR(storeInsn, floatVal, 1);                 		// data
	useProducer(inPrimitive.nthInputVariable(0), storeInsn, 2);		// memory edge in
	defineProducer(inPrimitive, storeInsn, 0);						// memory edge out
}

void x86Win32Emitter::
emit_St_D(Primitive& inPrimitive)	
{
    // Temporarily push 64-bit double on the FPU stack
    InsnFloatMemory& loadInsn = *new InsnFloatMemory(&inPrimitive, mPool, fld64, 1, 1);
    useProducer(inPrimitive.nthInputVariable(2), loadInsn, 0, vrcDouble);    // data
    InstructionDefine& order = defineTemporaryOrder(loadInsn, 0);
    
    // Pop result from FPU stack and store into memory as 64-bit double
    InsnFloatMemory& storeInsn = *new InsnFloatMemory(&inPrimitive, mPool, fstp64, atRegisterIndirect, 3, 1);
    useProducer(inPrimitive.nthInputVariable(1), storeInsn, 0);		        // address
    useTemporaryOrder(storeInsn, order, 1);
    useProducer(inPrimitive.nthInputVariable(0), storeInsn, 2);		        // memory edge in
    defineProducer(inPrimitive, storeInsn, 0);						        // memory edge out
}
