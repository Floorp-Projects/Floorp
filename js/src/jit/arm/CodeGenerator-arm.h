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

    CodeGeneratorARM *thisFromCtor() {return this;}

  protected:
    // Label for the common return path.
    NonAssertingLabel returnLabel_;
    NonAssertingLabel deoptLabel_;
    // Ugh. This is not going to be pretty to move over. Stack slotted variables
    // are not useful on arm. It looks like this will need to return one of two
    // types.
    inline Operand ToOperand(const LAllocation &a) {
        if (a.isGeneralReg())
            return Operand(a.toGeneralReg()->reg());
        if (a.isFloatReg())
            return Operand(a.toFloatReg()->reg());
        return Operand(StackPointer, ToStackOffset(&a));
    }
    inline Operand ToOperand(const LAllocation *a) {
        return ToOperand(*a);
    }
    inline Operand ToOperand(const LDefinition *def) {
        return ToOperand(def->output());
    }

    MoveOperand toMoveOperand(const LAllocation *a) const;

    bool bailoutIf(Assembler::Condition condition, LSnapshot *snapshot);
    bool bailoutFrom(Label *label, LSnapshot *snapshot);
    bool bailout(LSnapshot *snapshot);

    template <typename T1, typename T2>
    bool bailoutCmpPtr(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot *snapshot) {
        masm.cmpPtr(lhs, rhs);
        return bailoutIf(c, snapshot);
    }
    bool bailoutTestPtr(Assembler::Condition c, Register lhs, Register rhs, LSnapshot *snapshot) {
        masm.testPtr(lhs, rhs);
        return bailoutIf(c, snapshot);
    }
    template <typename T1, typename T2>
    bool bailoutCmp32(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot *snapshot) {
        masm.cmp32(lhs, rhs);
        return bailoutIf(c, snapshot);
    }
    template <typename T1, typename T2>
    bool bailoutTest32(Assembler::Condition c, T1 lhs, T2 rhs, LSnapshot *snapshot) {
        masm.test32(lhs, rhs);
        return bailoutIf(c, snapshot);
    }
    bool bailoutIfFalseBool(Register reg, LSnapshot *snapshot) {
        masm.test32(reg, Imm32(0xFF));
        return bailoutIf(Assembler::Zero, snapshot);
    }

  protected:
    bool generatePrologue();
    bool generateEpilogue();
    bool generateOutOfLineCode();

    void emitRoundDouble(FloatRegister src, Register dest, Label *fail);

    // Emits a branch that directs control flow to the true block if |cond| is
    // true, and the false block if |cond| is false.
    void emitBranch(Assembler::Condition cond, MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    void testNullEmitBranch(Assembler::Condition cond, const ValueOperand &value,
                            MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        cond = masm.testNull(cond, value);
        emitBranch(cond, ifTrue, ifFalse);
    }
    void testUndefinedEmitBranch(Assembler::Condition cond, const ValueOperand &value,
                                 MBasicBlock *ifTrue, MBasicBlock *ifFalse)
    {
        cond = masm.testUndefined(cond, value);
        emitBranch(cond, ifTrue, ifFalse);
    }

    bool emitTableSwitchDispatch(MTableSwitch *mir, Register index, Register base);

  public:
    // Instruction visitors.
    virtual bool visitMinMaxD(LMinMaxD *ins);
    virtual bool visitAbsD(LAbsD *ins);
    virtual bool visitAbsF(LAbsF *ins);
    virtual bool visitSqrtD(LSqrtD *ins);
    virtual bool visitSqrtF(LSqrtF *ins);
    virtual bool visitAddI(LAddI *ins);
    virtual bool visitSubI(LSubI *ins);
    virtual bool visitBitNotI(LBitNotI *ins);
    virtual bool visitBitOpI(LBitOpI *ins);

    virtual bool visitMulI(LMulI *ins);

    virtual bool visitDivI(LDivI *ins);
    virtual bool visitSoftDivI(LSoftDivI *ins);
    virtual bool visitDivPowTwoI(LDivPowTwoI *ins);
    virtual bool visitModI(LModI *ins);
    virtual bool visitSoftModI(LSoftModI *ins);
    virtual bool visitModPowTwoI(LModPowTwoI *ins);
    virtual bool visitModMaskI(LModMaskI *ins);
    virtual bool visitPowHalfD(LPowHalfD *ins);
    virtual bool visitShiftI(LShiftI *ins);
    virtual bool visitUrshD(LUrshD *ins);

    virtual bool visitTestIAndBranch(LTestIAndBranch *test);
    virtual bool visitCompare(LCompare *comp);
    virtual bool visitCompareAndBranch(LCompareAndBranch *comp);
    virtual bool visitTestDAndBranch(LTestDAndBranch *test);
    virtual bool visitTestFAndBranch(LTestFAndBranch *test);
    virtual bool visitCompareD(LCompareD *comp);
    virtual bool visitCompareF(LCompareF *comp);
    virtual bool visitCompareDAndBranch(LCompareDAndBranch *comp);
    virtual bool visitCompareFAndBranch(LCompareFAndBranch *comp);
    virtual bool visitCompareB(LCompareB *lir);
    virtual bool visitCompareBAndBranch(LCompareBAndBranch *lir);
    virtual bool visitCompareV(LCompareV *lir);
    virtual bool visitCompareVAndBranch(LCompareVAndBranch *lir);
    virtual bool visitBitAndAndBranch(LBitAndAndBranch *baab);
    virtual bool visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir);
    virtual bool visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir);
    virtual bool visitNotI(LNotI *ins);
    virtual bool visitNotD(LNotD *ins);
    virtual bool visitNotF(LNotF *ins);

    virtual bool visitMathD(LMathD *math);
    virtual bool visitMathF(LMathF *math);
    virtual bool visitFloor(LFloor *lir);
    virtual bool visitFloorF(LFloorF *lir);
    virtual bool visitCeil(LCeil *lir);
    virtual bool visitCeilF(LCeilF *lir);
    virtual bool visitRound(LRound *lir);
    virtual bool visitRoundF(LRoundF *lir);
    virtual bool visitTruncateDToInt32(LTruncateDToInt32 *ins);
    virtual bool visitTruncateFToInt32(LTruncateFToInt32 *ins);

    // Out of line visitors.
    bool visitOutOfLineBailout(OutOfLineBailout *ool);
    bool visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool);

  protected:
    ValueOperand ToValue(LInstruction *ins, size_t pos);
    ValueOperand ToOutValue(LInstruction *ins);
    ValueOperand ToTempValue(LInstruction *ins, size_t pos);

    // Functions for LTestVAndBranch.
    Register splitTagForTest(const ValueOperand &value);

    bool divICommon(MDiv *mir, Register lhs, Register rhs, Register output, LSnapshot *snapshot,
                    Label &done);
    bool modICommon(MMod *mir, Register lhs, Register rhs, Register output, LSnapshot *snapshot,
                    Label &done);

  public:
    CodeGeneratorARM(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm);

  public:
    bool visitBox(LBox *box);
    bool visitBoxFloatingPoint(LBoxFloatingPoint *box);
    bool visitUnbox(LUnbox *unbox);
    bool visitValue(LValue *value);
    bool visitDouble(LDouble *ins);
    bool visitFloat32(LFloat32 *ins);

    bool visitGuardShape(LGuardShape *guard);
    bool visitGuardObjectType(LGuardObjectType *guard);
    bool visitGuardClass(LGuardClass *guard);

    bool visitNegI(LNegI *lir);
    bool visitNegD(LNegD *lir);
    bool visitNegF(LNegF *lir);
    bool visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins);
    bool visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins);
    bool visitAsmJSLoadHeap(LAsmJSLoadHeap *ins);
    bool visitAsmJSStoreHeap(LAsmJSStoreHeap *ins);
    bool visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins);
    bool visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins);
    bool visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins);
    bool visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins);
    bool visitAsmJSPassStackArg(LAsmJSPassStackArg *ins);

    bool visitForkJoinGetSlice(LForkJoinGetSlice *ins);

    bool generateInvalidateEpilogue();
  protected:
    void postAsmJSCall(LAsmJSCall *lir) {
        if (!UseHardFpABI() && lir->mir()->callee().which() == MAsmJSCall::Callee::Builtin) {
            switch (lir->mir()->type()) {
              case MIRType_Double:
                masm.ma_vxfer(r0, r1, d0);
                break;
              case MIRType_Float32:
                masm.as_vxfer(r0, InvalidReg, VFPRegister(d0).singleOverlay(),
                              Assembler::CoreToFloat);
                break;
              default:
                break;
            }
        }
    }

    bool visitEffectiveAddress(LEffectiveAddress *ins);
    bool visitUDiv(LUDiv *ins);
    bool visitUMod(LUMod *ins);
    bool visitSoftUDivOrMod(LSoftUDivOrMod *ins);

  public:
    // Unimplemented SIMD instructions
    bool visitSimdValueX4(LSimdValueX4 *lir) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitInt32x4(LInt32x4 *ins) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitFloat32x4(LFloat32x4 *ins) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitSimdExtractElementI(LSimdExtractElementI *ins) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitSimdExtractElementF(LSimdExtractElementF *ins) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitSimdBinaryArithIx4(LSimdBinaryArithIx4 *lir) { MOZ_ASSUME_UNREACHABLE("NYI"); }
    bool visitSimdBinaryArithFx4(LSimdBinaryArithFx4 *lir) { MOZ_ASSUME_UNREACHABLE("NYI"); }
};

typedef CodeGeneratorARM CodeGeneratorSpecific;

// An out-of-line bailout thunk.
class OutOfLineBailout : public OutOfLineCodeBase<CodeGeneratorARM>
{
  protected: // Silence Clang warning.
    LSnapshot *snapshot_;
    uint32_t frameSize_;

  public:
    OutOfLineBailout(LSnapshot *snapshot, uint32_t frameSize)
      : snapshot_(snapshot),
        frameSize_(frameSize)
    { }

    bool accept(CodeGeneratorARM *codegen);

    LSnapshot *snapshot() const {
        return snapshot_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_arm_CodeGenerator_arm_h */
