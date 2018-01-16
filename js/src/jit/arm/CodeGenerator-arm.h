/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_CodeGenerator_arm_h
#define jit_arm_CodeGenerator_arm_h

#include "jit/arm/Assembler-arm.h"
#include "jit/shared/CodeGenerator-shared.h"

namespace js {
namespace jit {

class OutOfLineBailout;
class OutOfLineTableSwitch;

class CodeGeneratorARM : public CodeGeneratorShared
{
    friend class MoveResolverARM;

    CodeGeneratorARM* thisFromCtor() {return this;}

  protected:
    NonAssertingLabel deoptLabel_;

    MoveOperand toMoveOperand(LAllocation a) const;

    void bailoutIf(Assembler::Condition condition, LSnapshot* snapshot);
    void bailoutFrom(Label* label, LSnapshot* snapshot);
    void bailout(LSnapshot* snapshot);

    template <typename T1, typename T2>
    void bailoutCmpPtr(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot* snapshot) {
        masm.cmpPtr(lhs, rhs);
        bailoutIf(c, snapshot);
    }
    void bailoutTestPtr(Assembler::Condition c, Register lhs, Register rhs, LSnapshot* snapshot) {
        masm.testPtr(lhs, rhs);
        bailoutIf(c, snapshot);
    }
    template <typename T1, typename T2>
    void bailoutCmp32(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot* snapshot) {
        masm.cmp32(lhs, rhs);
        bailoutIf(c, snapshot);
    }
    template <typename T1, typename T2>
    void bailoutTest32(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot* snapshot) {
        masm.test32(lhs, rhs);
        bailoutIf(c, snapshot);
    }
    void bailoutIfFalseBool(Register reg, LSnapshot* snapshot) {
        masm.test32(reg, Imm32(0xFF));
        bailoutIf(Assembler::Zero, snapshot);
    }

    template<class T>
    void generateUDivModZeroCheck(Register rhs, Register output, Label* done, LSnapshot* snapshot,
                                  T* mir);

  protected:
    bool generateOutOfLineCode();

    void emitRoundDouble(FloatRegister src, Register dest, Label* fail);

    // Emits a branch that directs control flow to the true block if |cond| is
    // true, and the false block if |cond| is false.
    void emitBranch(Assembler::Condition cond, MBasicBlock* ifTrue, MBasicBlock* ifFalse);

    void testNullEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                            MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        cond = masm.testNull(cond, value);
        emitBranch(cond, ifTrue, ifFalse);
    }
    void testUndefinedEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                                 MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        cond = masm.testUndefined(cond, value);
        emitBranch(cond, ifTrue, ifFalse);
    }
    void testObjectEmitBranch(Assembler::Condition cond, const ValueOperand& value,
                              MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        cond = masm.testObject(cond, value);
        emitBranch(cond, ifTrue, ifFalse);
    }
    void testZeroEmitBranch(Assembler::Condition cond, Register reg,
                            MBasicBlock* ifTrue, MBasicBlock* ifFalse)
    {
        MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        masm.cmpPtr(reg, ImmWord(0));
        emitBranch(cond, ifTrue, ifFalse);
    }

    void emitTableSwitchDispatch(MTableSwitch* mir, Register index, Register base);

    template <typename T>
    void emitWasmLoad(T* ins);
    template <typename T>
    void emitWasmUnalignedLoad(T* ins);
    template <typename T>
    void emitWasmStore(T* ins);
    template <typename T>
    void emitWasmUnalignedStore(T* ins);

  public:
    // Instruction visitors.
    virtual void visitMinMaxD(LMinMaxD* ins) override;
    virtual void visitMinMaxF(LMinMaxF* ins) override;
    virtual void visitAbsD(LAbsD* ins) override;
    virtual void visitAbsF(LAbsF* ins) override;
    virtual void visitSqrtD(LSqrtD* ins) override;
    virtual void visitSqrtF(LSqrtF* ins) override;
    virtual void visitAddI(LAddI* ins) override;
    virtual void visitSubI(LSubI* ins) override;
    virtual void visitBitNotI(LBitNotI* ins) override;
    virtual void visitBitOpI(LBitOpI* ins) override;

    virtual void visitMulI(LMulI* ins) override;

    virtual void visitDivI(LDivI* ins) override;
    virtual void visitSoftDivI(LSoftDivI* ins) override;
    virtual void visitDivPowTwoI(LDivPowTwoI* ins) override;
    virtual void visitModI(LModI* ins) override;
    virtual void visitSoftModI(LSoftModI* ins) override;
    virtual void visitModPowTwoI(LModPowTwoI* ins) override;
    virtual void visitModMaskI(LModMaskI* ins) override;
    virtual void visitPowHalfD(LPowHalfD* ins) override;
    virtual void visitShiftI(LShiftI* ins) override;
    virtual void visitShiftI64(LShiftI64* ins) override;
    virtual void visitUrshD(LUrshD* ins) override;

    virtual void visitClzI(LClzI* ins) override;
    virtual void visitCtzI(LCtzI* ins) override;
    virtual void visitPopcntI(LPopcntI* ins) override;

    virtual void visitTestIAndBranch(LTestIAndBranch* test) override;
    virtual void visitCompare(LCompare* comp) override;
    virtual void visitCompareAndBranch(LCompareAndBranch* comp) override;
    virtual void visitTestDAndBranch(LTestDAndBranch* test) override;
    virtual void visitTestFAndBranch(LTestFAndBranch* test) override;
    virtual void visitCompareD(LCompareD* comp) override;
    virtual void visitCompareF(LCompareF* comp) override;
    virtual void visitCompareDAndBranch(LCompareDAndBranch* comp) override;
    virtual void visitCompareFAndBranch(LCompareFAndBranch* comp) override;
    virtual void visitCompareB(LCompareB* lir) override;
    virtual void visitCompareBAndBranch(LCompareBAndBranch* lir) override;
    virtual void visitCompareBitwise(LCompareBitwise* lir) override;
    virtual void visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir) override;
    virtual void visitBitAndAndBranch(LBitAndAndBranch* baab) override;
    virtual void visitWasmUint32ToDouble(LWasmUint32ToDouble* lir) override;
    virtual void visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir) override;
    virtual void visitNotI(LNotI* ins) override;
    virtual void visitNotD(LNotD* ins) override;
    virtual void visitNotF(LNotF* ins) override;

    virtual void visitMathD(LMathD* math) override;
    virtual void visitMathF(LMathF* math) override;
    virtual void visitFloor(LFloor* lir) override;
    virtual void visitFloorF(LFloorF* lir) override;
    virtual void visitCeil(LCeil* lir) override;
    virtual void visitCeilF(LCeilF* lir) override;
    virtual void visitRound(LRound* lir) override;
    virtual void visitRoundF(LRoundF* lir) override;
    virtual void visitTruncateDToInt32(LTruncateDToInt32* ins) override;
    virtual void visitTruncateFToInt32(LTruncateFToInt32* ins) override;

    virtual void visitWrapInt64ToInt32(LWrapInt64ToInt32* lir) override;
    virtual void visitExtendInt32ToInt64(LExtendInt32ToInt64* lir) override;
    virtual void visitSignExtendInt64(LSignExtendInt64* ins) override;
    virtual void visitAddI64(LAddI64* lir) override;
    virtual void visitSubI64(LSubI64* lir) override;
    virtual void visitMulI64(LMulI64* lir) override;
    virtual void visitDivOrModI64(LDivOrModI64* lir) override;
    virtual void visitUDivOrModI64(LUDivOrModI64* lir) override;
    virtual void visitCompareI64(LCompareI64* lir) override;
    virtual void visitCompareI64AndBranch(LCompareI64AndBranch* lir) override;
    virtual void visitBitOpI64(LBitOpI64* lir) override;
    virtual void visitRotateI64(LRotateI64* lir) override;
    virtual void visitWasmStackArgI64(LWasmStackArgI64* lir) override;
    virtual void visitWasmSelectI64(LWasmSelectI64* lir) override;
    virtual void visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir) override;
    virtual void visitWasmReinterpretToI64(LWasmReinterpretToI64* lir) override;
    virtual void visitPopcntI64(LPopcntI64* ins) override;
    virtual void visitClzI64(LClzI64* ins) override;
    virtual void visitCtzI64(LCtzI64* ins) override;
    virtual void visitNotI64(LNotI64* ins) override;
    virtual void visitWasmTruncateToInt64(LWasmTruncateToInt64* ins) override;
    virtual void visitInt64ToFloatingPointCall(LInt64ToFloatingPointCall* lir) override;
    virtual void visitTestI64AndBranch(LTestI64AndBranch* lir) override;

    // Out of line visitors.
    void visitOutOfLineBailout(OutOfLineBailout* ool);
    void visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool);

  protected:
    ValueOperand ToValue(LInstruction* ins, size_t pos);
    ValueOperand ToOutValue(LInstruction* ins);
    ValueOperand ToTempValue(LInstruction* ins, size_t pos);

    Register64 ToOperandOrRegister64(const LInt64Allocation input);

    // Functions for LTestVAndBranch.
    Register splitTagForTest(const ValueOperand& value);

    void divICommon(MDiv* mir, Register lhs, Register rhs, Register output, LSnapshot* snapshot,
                    Label& done);
    void modICommon(MMod* mir, Register lhs, Register rhs, Register output, LSnapshot* snapshot,
                    Label& done);

  public:
    CodeGeneratorARM(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

  public:
    void visitBox(LBox* box) override;
    void visitBoxFloatingPoint(LBoxFloatingPoint* box) override;
    void visitUnbox(LUnbox* unbox) override;
    void visitValue(LValue* value) override;
    void visitDouble(LDouble* ins) override;
    void visitFloat32(LFloat32* ins) override;

    void visitGuardShape(LGuardShape* guard) override;
    void visitGuardObjectGroup(LGuardObjectGroup* guard) override;
    void visitGuardClass(LGuardClass* guard) override;

    void visitNegI(LNegI* lir) override;
    void visitNegD(LNegD* lir) override;
    void visitNegF(LNegF* lir) override;
    void visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins) override;
    void visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins) override;
    void visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop* lir) override;
    void visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect* lir) override;
    void visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir) override;
    void visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir) override;
    void visitWasmSelect(LWasmSelect* ins) override;
    void visitWasmReinterpret(LWasmReinterpret* ins) override;
    void emitWasmCall(LWasmCallBase* ins);
    void visitWasmLoad(LWasmLoad* ins) override;
    void visitWasmLoadI64(LWasmLoadI64* ins) override;
    void visitWasmUnalignedLoad(LWasmUnalignedLoad* ins) override;
    void visitWasmUnalignedLoadI64(LWasmUnalignedLoadI64* ins) override;
    void visitWasmAddOffset(LWasmAddOffset* ins) override;
    void visitWasmStore(LWasmStore* ins) override;
    void visitWasmStoreI64(LWasmStoreI64* ins) override;
    void visitWasmUnalignedStore(LWasmUnalignedStore* ins) override;
    void visitWasmUnalignedStoreI64(LWasmUnalignedStoreI64* ins) override;
    void visitAsmJSLoadHeap(LAsmJSLoadHeap* ins) override;
    void visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) override;
    void visitWasmCompareExchangeHeap(LWasmCompareExchangeHeap* ins) override;
    void visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap* ins) override;
    void visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) override;
    void visitWasmAtomicBinopHeapForEffect(LWasmAtomicBinopHeapForEffect* ins) override;
    void visitWasmStackArg(LWasmStackArg* ins) override;
    void visitWasmTruncateToInt32(LWasmTruncateToInt32* ins) override;
    void visitOutOfLineWasmTruncateCheck(OutOfLineWasmTruncateCheck* ool) override;
    void visitCopySignD(LCopySignD* ins) override;
    void visitCopySignF(LCopySignF* ins) override;

    void visitMemoryBarrier(LMemoryBarrier* ins) override;

    void generateInvalidateEpilogue();

    void setReturnDoubleRegs(LiveRegisterSet* regs);

    // Generating a result.
    template<typename S, typename T>
    void atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType, const S& value,
                                    const T& mem, Register flagTemp, Register outTemp,
                                    AnyRegister output);

    // Generating no result.
    template<typename S, typename T>
    void atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType, const S& value,
                                    const T& mem, Register flagTemp);

    void visitWasmAtomicLoadI64(LWasmAtomicLoadI64* lir) override;
    void visitWasmAtomicStoreI64(LWasmAtomicStoreI64* lir) override;
    void visitWasmCompareExchangeI64(LWasmCompareExchangeI64* lir) override;
    void visitWasmAtomicBinopI64(LWasmAtomicBinopI64* lir) override;
    void visitWasmAtomicExchangeI64(LWasmAtomicExchangeI64* lir) override;

  protected:
    void visitEffectiveAddress(LEffectiveAddress* ins) override;
    void visitUDiv(LUDiv* ins) override;
    void visitUMod(LUMod* ins) override;
    void visitSoftUDivOrMod(LSoftUDivOrMod* ins) override;

  public:
    // Unimplemented SIMD instructions
    void visitSimdSplatX4(LSimdSplatX4* lir) override { MOZ_CRASH("NYI"); }
    void visitSimd128Int(LSimd128Int* ins) override { MOZ_CRASH("NYI"); }
    void visitSimd128Float(LSimd128Float* ins) override { MOZ_CRASH("NYI"); }
    void visitSimdReinterpretCast(LSimdReinterpretCast* ins) override { MOZ_CRASH("NYI"); }
    void visitSimdExtractElementI(LSimdExtractElementI* ins) override { MOZ_CRASH("NYI"); }
    void visitSimdExtractElementF(LSimdExtractElementF* ins) override { MOZ_CRASH("NYI"); }
    void visitSimdGeneralShuffleI(LSimdGeneralShuffleI* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdGeneralShuffleF(LSimdGeneralShuffleF* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdSwizzleI(LSimdSwizzleI* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdSwizzleF(LSimdSwizzleF* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdBinaryCompIx4(LSimdBinaryCompIx4* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdBinaryCompFx4(LSimdBinaryCompFx4* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdBinaryArithIx4(LSimdBinaryArithIx4* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdBinaryArithFx4(LSimdBinaryArithFx4* lir) override { MOZ_CRASH("NYI"); }
    void visitSimdBinaryBitwise(LSimdBinaryBitwise* lir) override { MOZ_CRASH("NYI"); }
};

typedef CodeGeneratorARM CodeGeneratorSpecific;

// An out-of-line bailout thunk.
class OutOfLineBailout : public OutOfLineCodeBase<CodeGeneratorARM>
{
  protected: // Silence Clang warning.
    LSnapshot* snapshot_;
    uint32_t frameSize_;

  public:
    OutOfLineBailout(LSnapshot* snapshot, uint32_t frameSize)
      : snapshot_(snapshot),
        frameSize_(frameSize)
    { }

    void accept(CodeGeneratorARM* codegen) override;

    LSnapshot* snapshot() const {
        return snapshot_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_arm_CodeGenerator_arm_h */
