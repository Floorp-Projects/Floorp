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
    // ugh.  this is not going to be pretty to move over.
    // stack slotted variables are not useful on arm.
    // it looks like this will need to return one of two types.
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

    MoveResolver::MoveOperand toMoveOperand(const LAllocation *a) const;

    bool bailoutIf(Assembler::Condition condition, LSnapshot *snapshot);
    bool bailoutFrom(Label *label, LSnapshot *snapshot);
    bool bailout(LSnapshot *snapshot);

  protected:
    bool generatePrologue();
    bool generateEpilogue();
    bool generateOutOfLineCode();

    void emitRoundDouble(const FloatRegister &src, const Register &dest, Label *fail);

    // Emits a branch that directs control flow to the true block if |cond| is
    // true, and the false block if |cond| is false.
    void emitBranch(Assembler::Condition cond, MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    bool emitTableSwitchDispatch(MTableSwitch *mir, const Register &index, const Register &base);

  public:
    // Instruction visitors.
    virtual bool visitMinMaxD(LMinMaxD *ins);
    virtual bool visitAbsD(LAbsD *ins);
    virtual bool visitSqrtD(LSqrtD *ins);
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
    virtual bool visitCompareD(LCompareD *comp);
    virtual bool visitCompareDAndBranch(LCompareDAndBranch *comp);
    virtual bool visitCompareB(LCompareB *lir);
    virtual bool visitCompareBAndBranch(LCompareBAndBranch *lir);
    virtual bool visitCompareV(LCompareV *lir);
    virtual bool visitCompareVAndBranch(LCompareVAndBranch *lir);
    virtual bool visitBitAndAndBranch(LBitAndAndBranch *baab);
    virtual bool visitUInt32ToDouble(LUInt32ToDouble *lir);
    virtual bool visitNotI(LNotI *ins);
    virtual bool visitNotD(LNotD *ins);

    virtual bool visitMathD(LMathD *math);
    virtual bool visitFloor(LFloor *lir);
    virtual bool visitRound(LRound *lir);
    virtual bool visitTruncateDToInt32(LTruncateDToInt32 *ins);

    // Out of line visitors.
    bool visitOutOfLineBailout(OutOfLineBailout *ool);
    bool visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool);

  protected:
    ValueOperand ToValue(LInstruction *ins, size_t pos);
    ValueOperand ToOutValue(LInstruction *ins);
    ValueOperand ToTempValue(LInstruction *ins, size_t pos);

    // Functions for LTestVAndBranch.
    Register splitTagForTest(const ValueOperand &value);

    void storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                           const Register &elements, const LAllocation *index);

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
    bool visitOsrValue(LOsrValue *value);
    bool visitDouble(LDouble *ins);

    bool visitLoadSlotV(LLoadSlotV *load);
    bool visitLoadSlotT(LLoadSlotT *load);
    bool visitStoreSlotT(LStoreSlotT *load);

    bool visitLoadElementT(LLoadElementT *load);

    bool visitGuardShape(LGuardShape *guard);
    bool visitGuardObjectType(LGuardObjectType *guard);
    bool visitGuardClass(LGuardClass *guard);
    bool visitImplicitThis(LImplicitThis *lir);

    bool visitInterruptCheck(LInterruptCheck *lir);

    bool visitNegI(LNegI *lir);
    bool visitNegD(LNegD *lir);
    bool visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins);
    bool visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins);
    bool visitAsmJSLoadHeap(LAsmJSLoadHeap *ins);
    bool visitAsmJSStoreHeap(LAsmJSStoreHeap *ins);
    bool visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins);
    bool visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins);
    bool visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins);
    bool visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins);

    bool visitAsmJSPassStackArg(LAsmJSPassStackArg *ins);

    bool generateInvalidateEpilogue();
  protected:
    void postAsmJSCall(LAsmJSCall *lir) {
#if  !defined(JS_CPU_ARM_HARDFP)
        if (lir->mir()->type() == MIRType_Double) {
            masm.ma_vxfer(r0, r1, d0);
        }
#endif
}

    bool visitEffectiveAddress(LEffectiveAddress *ins);
    bool visitUDiv(LUDiv *ins);
    bool visitUMod(LUMod *ins);
    bool visitSoftUDivOrMod(LSoftUDivOrMod *ins);
};

typedef CodeGeneratorARM CodeGeneratorSpecific;

// An out-of-line bailout thunk.
class OutOfLineBailout : public OutOfLineCodeBase<CodeGeneratorARM>
{
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
