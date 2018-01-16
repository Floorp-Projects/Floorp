/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_CodeGenerator_x86_h
#define jit_x86_CodeGenerator_x86_h

#include "jit/x86-shared/CodeGenerator-x86-shared.h"
#include "jit/x86/Assembler-x86.h"

namespace js {
namespace jit {

class OutOfLineTruncate;
class OutOfLineTruncateFloat32;

class CodeGeneratorX86 : public CodeGeneratorX86Shared
{
  private:
    CodeGeneratorX86* thisFromCtor() {
        return this;
    }

  protected:
    ValueOperand ToValue(LInstruction* ins, size_t pos);
    ValueOperand ToOutValue(LInstruction* ins);
    ValueOperand ToTempValue(LInstruction* ins, size_t pos);

    template <typename T> void emitWasmLoad(T* ins);
    template <typename T> void emitWasmStore(T* ins);
    template <typename T> void emitWasmStoreOrExchangeAtomicI64(T* ins, uint32_t offset);

  public:
    CodeGeneratorX86(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

  public:
    void visitBox(LBox* box) override;
    void visitBoxFloatingPoint(LBoxFloatingPoint* box) override;
    void visitUnbox(LUnbox* unbox) override;
    void visitValue(LValue* value) override;
    void visitCompareB(LCompareB* lir) override;
    void visitCompareBAndBranch(LCompareBAndBranch* lir) override;
    void visitCompareBitwise(LCompareBitwise* lir) override;
    void visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir) override;
    void visitWasmUint32ToDouble(LWasmUint32ToDouble* lir) override;
    void visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir) override;
    void visitTruncateDToInt32(LTruncateDToInt32* ins) override;
    void visitTruncateFToInt32(LTruncateFToInt32* ins) override;
    void visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins) override;
    void visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins) override;
    void visitWasmLoad(LWasmLoad* ins) override;
    void visitWasmLoadI64(LWasmLoadI64* ins) override;
    void visitWasmStore(LWasmStore* ins) override;
    void visitWasmStoreI64(LWasmStoreI64* ins) override;
    void visitAsmJSLoadHeap(LAsmJSLoadHeap* ins) override;
    void visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) override;
    void visitWasmCompareExchangeHeap(LWasmCompareExchangeHeap* ins) override;
    void visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap* ins) override;
    void visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) override;
    void visitWasmAtomicBinopHeapForEffect(LWasmAtomicBinopHeapForEffect* ins) override;

    void visitWasmAtomicLoadI64(LWasmAtomicLoadI64* ins) override;
    void visitWasmAtomicStoreI64(LWasmAtomicStoreI64* ins) override;
    void visitWasmCompareExchangeI64(LWasmCompareExchangeI64* ins) override;
    void visitWasmAtomicExchangeI64(LWasmAtomicExchangeI64* ins) override;
    void visitWasmAtomicBinopI64(LWasmAtomicBinopI64* ins) override;

    void visitOutOfLineTruncate(OutOfLineTruncate* ool);
    void visitOutOfLineTruncateFloat32(OutOfLineTruncateFloat32* ool);

    void visitCompareI64(LCompareI64* lir) override;
    void visitCompareI64AndBranch(LCompareI64AndBranch* lir) override;
    void visitDivOrModI64(LDivOrModI64* lir) override;
    void visitUDivOrModI64(LUDivOrModI64* lir) override;
    void visitWasmSelectI64(LWasmSelectI64* lir) override;
    void visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir) override;
    void visitWasmReinterpretToI64(LWasmReinterpretToI64* lir) override;
    void visitExtendInt32ToInt64(LExtendInt32ToInt64* lir) override;
    void visitSignExtendInt64(LSignExtendInt64* ins) override;
    void visitWrapInt64ToInt32(LWrapInt64ToInt32* lir) override;
    void visitClzI64(LClzI64* lir) override;
    void visitCtzI64(LCtzI64* lir) override;
    void visitNotI64(LNotI64* lir) override;
    void visitWasmTruncateToInt64(LWasmTruncateToInt64* lir) override;
    void visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir) override;
    void visitTestI64AndBranch(LTestI64AndBranch* lir) override;
};

typedef CodeGeneratorX86 CodeGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_x86_CodeGenerator_x86_h */
