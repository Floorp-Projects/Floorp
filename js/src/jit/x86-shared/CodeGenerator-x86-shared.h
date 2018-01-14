/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_shared_CodeGenerator_x86_shared_h
#define jit_x86_shared_CodeGenerator_x86_shared_h

#include "jit/shared/CodeGenerator-shared.h"

namespace js {
namespace jit {

class OutOfLineBailout;
class OutOfLineUndoALUOperation;
class OutOfLineLoadTypedArrayOutOfBounds;
class MulNegativeZeroCheck;
class ModOverflowCheck;
class ReturnZero;
class OutOfLineTableSwitch;

class CodeGeneratorX86Shared : public CodeGeneratorShared
{
    friend class MoveResolverX86;

    CodeGeneratorX86Shared* thisFromCtor() {
        return this;
    }

    template <typename T>
    void bailout(const T& t, LSnapshot* snapshot);

  protected:
    // Load a NaN or zero into a register for an out of bounds AsmJS or static
    // typed array load.
    class OutOfLineLoadTypedArrayOutOfBounds : public OutOfLineCodeBase<CodeGeneratorX86Shared>
    {
        AnyRegister dest_;
        Scalar::Type viewType_;
      public:
        OutOfLineLoadTypedArrayOutOfBounds(AnyRegister dest, Scalar::Type viewType)
          : dest_(dest), viewType_(viewType)
        {}

        AnyRegister dest() const { return dest_; }
        Scalar::Type viewType() const { return viewType_; }
        void accept(CodeGeneratorX86Shared* codegen) override {
            codegen->visitOutOfLineLoadTypedArrayOutOfBounds(this);
        }
    };

    // Additional bounds check for vector Float to Int conversion, when the
    // undefined pattern is seen. Might imply a bailout.
    class OutOfLineSimdFloatToIntCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared>
    {
        Register temp_;
        FloatRegister input_;
        LInstruction* ins_;
        wasm::BytecodeOffset bytecodeOffset_;

      public:
        OutOfLineSimdFloatToIntCheck(Register temp, FloatRegister input, LInstruction *ins,
                                     wasm::BytecodeOffset bytecodeOffset)
          : temp_(temp), input_(input), ins_(ins), bytecodeOffset_(bytecodeOffset)
        {}

        Register temp() const { return temp_; }
        FloatRegister input() const { return input_; }
        LInstruction* ins() const { return ins_; }
        wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

        void accept(CodeGeneratorX86Shared* codegen) override {
            codegen->visitOutOfLineSimdFloatToIntCheck(this);
        }
    };

  public:
    NonAssertingLabel deoptLabel_;

    Operand ToOperand(const LAllocation& a);
    Operand ToOperand(const LAllocation* a);
    Operand ToOperand(const LDefinition* def);

#ifdef JS_PUNBOX64
    Operand ToOperandOrRegister64(const LInt64Allocation input);
#else
    Register64 ToOperandOrRegister64(const LInt64Allocation input);
#endif

    MoveOperand toMoveOperand(LAllocation a) const;

    void bailoutIf(Assembler::Condition condition, LSnapshot* snapshot);
    void bailoutIf(Assembler::DoubleCondition condition, LSnapshot* snapshot);
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
    void bailoutCvttsd2si(FloatRegister src, Register dest, LSnapshot* snapshot) {
        // vcvttsd2si returns 0x80000000 on failure. Test for it by
        // subtracting 1 and testing overflow. The other possibility is to test
        // equality for INT_MIN after a comparison, but 1 costs fewer bytes to
        // materialize.
        masm.vcvttsd2si(src, dest);
        masm.cmp32(dest, Imm32(1));
        bailoutIf(Assembler::Overflow, snapshot);
    }
    void bailoutCvttss2si(FloatRegister src, Register dest, LSnapshot* snapshot) {
        // Same trick as explained in the above comment.
        masm.vcvttss2si(src, dest);
        masm.cmp32(dest, Imm32(1));
        bailoutIf(Assembler::Overflow, snapshot);
    }

  protected:
    bool generateOutOfLineCode();

    void emitCompare(MCompare::CompareType type, const LAllocation* left, const LAllocation* right);

    // Emits a branch that directs control flow to the true block if |cond| is
    // true, and the false block if |cond| is false.
    void emitBranch(Assembler::Condition cond, MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                    Assembler::NaNCond ifNaN = Assembler::NaN_HandledByCond);
    void emitBranch(Assembler::DoubleCondition cond, MBasicBlock* ifTrue, MBasicBlock* ifFalse);

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

    void emitSimdExtractLane8x16(FloatRegister input, Register output, unsigned lane,
                                 SimdSign signedness);
    void emitSimdExtractLane16x8(FloatRegister input, Register output, unsigned lane,
                                 SimdSign signedness);
    void emitSimdExtractLane32x4(FloatRegister input, Register output, unsigned lane);

  public:
    CodeGeneratorX86Shared(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

  public:
    // Instruction visitors.
    virtual void visitDouble(LDouble* ins) override;
    virtual void visitFloat32(LFloat32* ins) override;
    virtual void visitMinMaxD(LMinMaxD* ins) override;
    virtual void visitMinMaxF(LMinMaxF* ins) override;
    virtual void visitAbsD(LAbsD* ins) override;
    virtual void visitAbsF(LAbsF* ins) override;
    virtual void visitClzI(LClzI* ins) override;
    virtual void visitCtzI(LCtzI* ins) override;
    virtual void visitPopcntI(LPopcntI* ins) override;
    virtual void visitPopcntI64(LPopcntI64* lir) override;
    virtual void visitSqrtD(LSqrtD* ins) override;
    virtual void visitSqrtF(LSqrtF* ins) override;
    virtual void visitPowHalfD(LPowHalfD* ins) override;
    virtual void visitAddI(LAddI* ins) override;
    virtual void visitAddI64(LAddI64* ins) override;
    virtual void visitSubI(LSubI* ins) override;
    virtual void visitSubI64(LSubI64* ins) override;
    virtual void visitMulI(LMulI* ins) override;
    virtual void visitMulI64(LMulI64* ins) override;
    virtual void visitDivI(LDivI* ins) override;
    virtual void visitDivPowTwoI(LDivPowTwoI* ins) override;
    virtual void visitDivOrModConstantI(LDivOrModConstantI* ins) override;
    virtual void visitModI(LModI* ins) override;
    virtual void visitModPowTwoI(LModPowTwoI* ins) override;
    virtual void visitBitNotI(LBitNotI* ins) override;
    virtual void visitBitOpI(LBitOpI* ins) override;
    virtual void visitBitOpI64(LBitOpI64* ins) override;
    virtual void visitShiftI(LShiftI* ins) override;
    virtual void visitShiftI64(LShiftI64* ins) override;
    virtual void visitUrshD(LUrshD* ins) override;
    virtual void visitTestIAndBranch(LTestIAndBranch* test) override;
    virtual void visitTestDAndBranch(LTestDAndBranch* test) override;
    virtual void visitTestFAndBranch(LTestFAndBranch* test) override;
    virtual void visitCompare(LCompare* comp) override;
    virtual void visitCompareAndBranch(LCompareAndBranch* comp) override;
    virtual void visitCompareD(LCompareD* comp) override;
    virtual void visitCompareDAndBranch(LCompareDAndBranch* comp) override;
    virtual void visitCompareF(LCompareF* comp) override;
    virtual void visitCompareFAndBranch(LCompareFAndBranch* comp) override;
    virtual void visitBitAndAndBranch(LBitAndAndBranch* baab) override;
    virtual void visitNotI(LNotI* comp) override;
    virtual void visitNotD(LNotD* comp) override;
    virtual void visitNotF(LNotF* comp) override;
    virtual void visitMathD(LMathD* math) override;
    virtual void visitMathF(LMathF* math) override;
    virtual void visitFloor(LFloor* lir) override;
    virtual void visitFloorF(LFloorF* lir) override;
    virtual void visitCeil(LCeil* lir) override;
    virtual void visitCeilF(LCeilF* lir) override;
    virtual void visitRound(LRound* lir) override;
    virtual void visitRoundF(LRoundF* lir) override;
    virtual void visitNearbyInt(LNearbyInt* lir) override;
    virtual void visitNearbyIntF(LNearbyIntF* lir) override;
    virtual void visitGuardShape(LGuardShape* guard) override;
    virtual void visitGuardObjectGroup(LGuardObjectGroup* guard) override;
    virtual void visitGuardClass(LGuardClass* guard) override;
    virtual void visitEffectiveAddress(LEffectiveAddress* ins) override;
    virtual void visitUDivOrMod(LUDivOrMod* ins) override;
    virtual void visitUDivOrModConstant(LUDivOrModConstant *ins) override;
    virtual void visitWasmStackArg(LWasmStackArg* ins) override;
    virtual void visitWasmStackArgI64(LWasmStackArgI64* ins) override;
    virtual void visitWasmSelect(LWasmSelect* ins) override;
    virtual void visitWasmReinterpret(LWasmReinterpret* lir) override;
    virtual void visitMemoryBarrier(LMemoryBarrier* ins) override;
    virtual void visitWasmAddOffset(LWasmAddOffset* lir) override;
    virtual void visitWasmTruncateToInt32(LWasmTruncateToInt32* lir) override;
    virtual void visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop* lir) override;
    virtual void visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect* lir) override;
    virtual void visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir) override;
    virtual void visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir) override;
    virtual void visitCopySignD(LCopySignD* lir) override;
    virtual void visitCopySignF(LCopySignF* lir) override;
    virtual void visitRotateI64(LRotateI64* lir) override;

    void visitOutOfLineLoadTypedArrayOutOfBounds(OutOfLineLoadTypedArrayOutOfBounds* ool);

    void visitNegI(LNegI* lir) override;
    void visitNegD(LNegD* lir) override;
    void visitNegF(LNegF* lir) override;

    void visitOutOfLineWasmTruncateCheck(OutOfLineWasmTruncateCheck* ool) override;

    // SIMD operators
    void visitSimdValueInt32x4(LSimdValueInt32x4* lir) override;
    void visitSimdValueFloat32x4(LSimdValueFloat32x4* lir) override;
    void visitSimdSplatX16(LSimdSplatX16* lir) override;
    void visitSimdSplatX8(LSimdSplatX8* lir) override;
    void visitSimdSplatX4(LSimdSplatX4* lir) override;
    void visitSimd128Int(LSimd128Int* ins) override;
    void visitSimd128Float(LSimd128Float* ins) override;
    void visitInt32x4ToFloat32x4(LInt32x4ToFloat32x4* ins) override;
    void visitFloat32x4ToInt32x4(LFloat32x4ToInt32x4* ins) override;
    void visitFloat32x4ToUint32x4(LFloat32x4ToUint32x4* ins) override;
    void visitSimdReinterpretCast(LSimdReinterpretCast* lir) override;
    void visitSimdExtractElementB(LSimdExtractElementB* lir) override;
    void visitSimdExtractElementI(LSimdExtractElementI* lir) override;
    void visitSimdExtractElementU2D(LSimdExtractElementU2D* lir) override;
    void visitSimdExtractElementF(LSimdExtractElementF* lir) override;
    void visitSimdInsertElementI(LSimdInsertElementI* lir) override;
    void visitSimdInsertElementF(LSimdInsertElementF* lir) override;
    void visitSimdSwizzleI(LSimdSwizzleI* lir) override;
    void visitSimdSwizzleF(LSimdSwizzleF* lir) override;
    void visitSimdShuffleX4(LSimdShuffleX4* lir) override;
    void visitSimdShuffle(LSimdShuffle* lir) override;
    void visitSimdUnaryArithIx16(LSimdUnaryArithIx16* lir) override;
    void visitSimdUnaryArithIx8(LSimdUnaryArithIx8* lir) override;
    void visitSimdUnaryArithIx4(LSimdUnaryArithIx4* lir) override;
    void visitSimdUnaryArithFx4(LSimdUnaryArithFx4* lir) override;
    void visitSimdBinaryCompIx16(LSimdBinaryCompIx16* lir) override;
    void visitSimdBinaryCompIx8(LSimdBinaryCompIx8* lir) override;
    void visitSimdBinaryCompIx4(LSimdBinaryCompIx4* lir) override;
    void visitSimdBinaryCompFx4(LSimdBinaryCompFx4* lir) override;
    void visitSimdBinaryArithIx16(LSimdBinaryArithIx16* lir) override;
    void visitSimdBinaryArithIx8(LSimdBinaryArithIx8* lir) override;
    void visitSimdBinaryArithIx4(LSimdBinaryArithIx4* lir) override;
    void visitSimdBinaryArithFx4(LSimdBinaryArithFx4* lir) override;
    void visitSimdBinarySaturating(LSimdBinarySaturating* lir) override;
    void visitSimdBinaryBitwise(LSimdBinaryBitwise* lir) override;
    void visitSimdShift(LSimdShift* lir) override;
    void visitSimdSelect(LSimdSelect* ins) override;
    void visitSimdAllTrue(LSimdAllTrue* ins) override;
    void visitSimdAnyTrue(LSimdAnyTrue* ins) override;

    template <class T, class Reg> void visitSimdGeneralShuffle(LSimdGeneralShuffleBase* lir, Reg temp);
    void visitSimdGeneralShuffleI(LSimdGeneralShuffleI* lir) override;
    void visitSimdGeneralShuffleF(LSimdGeneralShuffleF* lir) override;

    // Out of line visitors.
    void visitOutOfLineBailout(OutOfLineBailout* ool);
    void visitOutOfLineUndoALUOperation(OutOfLineUndoALUOperation* ool);
    void visitMulNegativeZeroCheck(MulNegativeZeroCheck* ool);
    void visitModOverflowCheck(ModOverflowCheck* ool);
    void visitReturnZero(ReturnZero* ool);
    void visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool);
    void visitOutOfLineSimdFloatToIntCheck(OutOfLineSimdFloatToIntCheck* ool);
    void generateInvalidateEpilogue();

    void setReturnDoubleRegs(LiveRegisterSet* regs);

    void canonicalizeIfDeterministic(Scalar::Type type, const LAllocation* value);
};

// An out-of-line bailout thunk.
class OutOfLineBailout : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    LSnapshot* snapshot_;

  public:
    explicit OutOfLineBailout(LSnapshot* snapshot)
      : snapshot_(snapshot)
    { }

    void accept(CodeGeneratorX86Shared* codegen) override;

    LSnapshot* snapshot() const {
        return snapshot_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_x86_shared_CodeGenerator_x86_shared_h */
