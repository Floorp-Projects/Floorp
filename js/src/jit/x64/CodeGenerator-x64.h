/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x64_CodeGenerator_x64_h
#define jit_x64_CodeGenerator_x64_h

#include "jit/x86-shared/CodeGenerator-x86-shared.h"

namespace js {
namespace jit {

class CodeGeneratorX64 : public CodeGeneratorX86Shared
{
    CodeGeneratorX64* thisFromCtor() {
        return this;
    }

  protected:
    ValueOperand ToValue(LInstruction* ins, size_t pos);
    ValueOperand ToOutValue(LInstruction* ins);
    ValueOperand ToTempValue(LInstruction* ins, size_t pos);

    void storeUnboxedValue(const LAllocation* value, MIRType valueType,
                           Operand dest, MIRType slotType);
    void memoryBarrier(MemoryBarrierBits barrier);

    void loadSimd(Scalar::Type type, unsigned numElems, const Operand& srcAddr, FloatRegister out);
    void emitSimdLoad(LAsmJSLoadHeap* ins);
    void storeSimd(Scalar::Type type, unsigned numElems, FloatRegister in, const Operand& dstAddr);
    void emitSimdStore(LAsmJSStoreHeap* ins);
  public:
    CodeGeneratorX64(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

  public:
    void visitValue(LValue* value);
    void visitBox(LBox* box);
    void visitUnbox(LUnbox* unbox);
    void visitCompareB(LCompareB* lir);
    void visitCompareBAndBranch(LCompareBAndBranch* lir);
    void visitCompareBitwise(LCompareBitwise* lir);
    void visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir);
    void visitCompare64(LCompare64* lir);
    void visitCompare64AndBranch(LCompare64AndBranch* lir);
    void visitBitOpI64(LBitOpI64* lir);
    void visitShiftI64(LShiftI64* lir);
    void visitRotate64(LRotate64* lir);
    void visitAddI64(LAddI64* lir);
    void visitSubI64(LSubI64* lir);
    void visitMulI64(LMulI64* lir);
    void visitDivOrModI64(LDivOrModI64* lir);
    void visitUDivOrMod64(LUDivOrMod64* lir);
    void visitNotI64(LNotI64* lir);
    void visitClzI64(LClzI64* lir);
    void visitCtzI64(LCtzI64* lir);
    void visitPopcntI64(LPopcntI64* lir);
    void visitTruncateDToInt32(LTruncateDToInt32* ins);
    void visitTruncateFToInt32(LTruncateFToInt32* ins);
    void visitWrapInt64ToInt32(LWrapInt64ToInt32* lir);
    void visitExtendInt32ToInt64(LExtendInt32ToInt64* lir);
    void visitWasmTruncateToInt64(LWasmTruncateToInt64* lir);
    void visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir);
    void visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins);
    void visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins);
    void visitAsmSelectI64(LAsmSelectI64* ins);
    void visitAsmJSCall(LAsmJSCall* ins);
    void visitAsmJSLoadHeap(LAsmJSLoadHeap* ins);
    void visitAsmJSStoreHeap(LAsmJSStoreHeap* ins);
    void visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins);
    void visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins);
    void visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins);
    void visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins);
    void visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar* ins);
    void visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar* ins);
    void visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr* ins);
    void visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc* ins);
    void visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble* lir);
    void visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32* lir);
    void visitAsmReinterpretFromI64(LAsmReinterpretFromI64* lir);
    void visitAsmReinterpretToI64(LAsmReinterpretToI64* lir);

    void visitWasmTruncateToInt32(LWasmTruncateToInt32* lir);
};

typedef CodeGeneratorX64 CodeGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_x64_CodeGenerator_x64_h */
