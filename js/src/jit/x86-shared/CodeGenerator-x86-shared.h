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
        void accept(CodeGeneratorX86Shared* codegen) {
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
        wasm::TrapOffset trapOffset_;

      public:
        OutOfLineSimdFloatToIntCheck(Register temp, FloatRegister input, LInstruction *ins,
                                     wasm::TrapOffset trapOffset)
          : temp_(temp), input_(input), ins_(ins), trapOffset_(trapOffset)
        {}

        Register temp() const { return temp_; }
        FloatRegister input() const { return input_; }
        LInstruction* ins() const { return ins_; }
        wasm::TrapOffset trapOffset() const { return trapOffset_; }

        void accept(CodeGeneratorX86Shared* codegen) {
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
    virtual void visitDouble(LDouble* ins);
    virtual void visitFloat32(LFloat32* ins);
    virtual void visitMinMaxD(LMinMaxD* ins);
    virtual void visitMinMaxF(LMinMaxF* ins);
    virtual void visitAbsD(LAbsD* ins);
    virtual void visitAbsF(LAbsF* ins);
    virtual void visitClzI(LClzI* ins);
    virtual void visitCtzI(LCtzI* ins);
    virtual void visitPopcntI(LPopcntI* ins);
    virtual void visitPopcntI64(LPopcntI64* lir);
    virtual void visitSqrtD(LSqrtD* ins);
    virtual void visitSqrtF(LSqrtF* ins);
    virtual void visitPowHalfD(LPowHalfD* ins);
    virtual void visitAddI(LAddI* ins);
    virtual void visitAddI64(LAddI64* ins);
    virtual void visitSubI(LSubI* ins);
    virtual void visitSubI64(LSubI64* ins);
    virtual void visitMulI(LMulI* ins);
    virtual void visitMulI64(LMulI64* ins);
    virtual void visitDivI(LDivI* ins);
    virtual void visitDivPowTwoI(LDivPowTwoI* ins);
    virtual void visitDivOrModConstantI(LDivOrModConstantI* ins);
    virtual void visitModI(LModI* ins);
    virtual void visitModPowTwoI(LModPowTwoI* ins);
    virtual void visitBitNotI(LBitNotI* ins);
    virtual void visitBitOpI(LBitOpI* ins);
    virtual void visitBitOpI64(LBitOpI64* ins);
    virtual void visitShiftI(LShiftI* ins);
    virtual void visitShiftI64(LShiftI64* ins);
    virtual void visitUrshD(LUrshD* ins);
    virtual void visitTestIAndBranch(LTestIAndBranch* test);
    virtual void visitTestDAndBranch(LTestDAndBranch* test);
    virtual void visitTestFAndBranch(LTestFAndBranch* test);
    virtual void visitCompare(LCompare* comp);
    virtual void visitCompareAndBranch(LCompareAndBranch* comp);
    virtual void visitCompareD(LCompareD* comp);
    virtual void visitCompareDAndBranch(LCompareDAndBranch* comp);
    virtual void visitCompareF(LCompareF* comp);
    virtual void visitCompareFAndBranch(LCompareFAndBranch* comp);
    virtual void visitBitAndAndBranch(LBitAndAndBranch* baab);
    virtual void visitNotI(LNotI* comp);
    virtual void visitNotD(LNotD* comp);
    virtual void visitNotF(LNotF* comp);
    virtual void visitMathD(LMathD* math);
    virtual void visitMathF(LMathF* math);
    virtual void visitFloor(LFloor* lir);
    virtual void visitFloorF(LFloorF* lir);
    virtual void visitCeil(LCeil* lir);
    virtual void visitCeilF(LCeilF* lir);
    virtual void visitRound(LRound* lir);
    virtual void visitRoundF(LRoundF* lir);
    virtual void visitGuardShape(LGuardShape* guard);
    virtual void visitGuardObjectGroup(LGuardObjectGroup* guard);
    virtual void visitGuardClass(LGuardClass* guard);
    virtual void visitEffectiveAddress(LEffectiveAddress* ins);
    virtual void visitUDivOrMod(LUDivOrMod* ins);
    virtual void visitUDivOrModConstant(LUDivOrModConstant *ins);
    virtual void visitAsmJSPassStackArg(LAsmJSPassStackArg* ins);
    virtual void visitAsmJSPassStackArgI64(LAsmJSPassStackArgI64* ins);
    virtual void visitAsmSelect(LAsmSelect* ins);
    virtual void visitWasmReinterpret(LWasmReinterpret* lir);
    virtual void visitMemoryBarrier(LMemoryBarrier* ins);
    virtual void visitWasmAddOffset(LWasmAddOffset* lir);
    virtual void visitWasmTruncateToInt32(LWasmTruncateToInt32* lir);
    virtual void visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop* lir);
    virtual void visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect* lir);
    virtual void visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir);
    virtual void visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir);
    virtual void visitCopySignD(LCopySignD* lir);
    virtual void visitCopySignF(LCopySignF* lir);
    virtual void visitRotateI64(LRotateI64* lir);

    void visitOutOfLineLoadTypedArrayOutOfBounds(OutOfLineLoadTypedArrayOutOfBounds* ool);

    void visitNegI(LNegI* lir);
    void visitNegD(LNegD* lir);
    void visitNegF(LNegF* lir);

    void visitOutOfLineWasmTruncateCheck(OutOfLineWasmTruncateCheck* ool);

    // SIMD operators
    void visitSimdValueInt32x4(LSimdValueInt32x4* lir);
    void visitSimdValueFloat32x4(LSimdValueFloat32x4* lir);
    void visitSimdSplatX16(LSimdSplatX16* lir);
    void visitSimdSplatX8(LSimdSplatX8* lir);
    void visitSimdSplatX4(LSimdSplatX4* lir);
    void visitSimd128Int(LSimd128Int* ins);
    void visitSimd128Float(LSimd128Float* ins);
    void visitInt32x4ToFloat32x4(LInt32x4ToFloat32x4* ins);
    void visitFloat32x4ToInt32x4(LFloat32x4ToInt32x4* ins);
    void visitFloat32x4ToUint32x4(LFloat32x4ToUint32x4* ins);
    void visitSimdReinterpretCast(LSimdReinterpretCast* lir);
    void visitSimdExtractElementB(LSimdExtractElementB* lir);
    void visitSimdExtractElementI(LSimdExtractElementI* lir);
    void visitSimdExtractElementU2D(LSimdExtractElementU2D* lir);
    void visitSimdExtractElementF(LSimdExtractElementF* lir);
    void visitSimdInsertElementI(LSimdInsertElementI* lir);
    void visitSimdInsertElementF(LSimdInsertElementF* lir);
    void visitSimdSwizzleI(LSimdSwizzleI* lir);
    void visitSimdSwizzleF(LSimdSwizzleF* lir);
    void visitSimdShuffleX4(LSimdShuffleX4* lir);
    void visitSimdShuffle(LSimdShuffle* lir);
    void visitSimdUnaryArithIx16(LSimdUnaryArithIx16* lir);
    void visitSimdUnaryArithIx8(LSimdUnaryArithIx8* lir);
    void visitSimdUnaryArithIx4(LSimdUnaryArithIx4* lir);
    void visitSimdUnaryArithFx4(LSimdUnaryArithFx4* lir);
    void visitSimdBinaryCompIx16(LSimdBinaryCompIx16* lir);
    void visitSimdBinaryCompIx8(LSimdBinaryCompIx8* lir);
    void visitSimdBinaryCompIx4(LSimdBinaryCompIx4* lir);
    void visitSimdBinaryCompFx4(LSimdBinaryCompFx4* lir);
    void visitSimdBinaryArithIx16(LSimdBinaryArithIx16* lir);
    void visitSimdBinaryArithIx8(LSimdBinaryArithIx8* lir);
    void visitSimdBinaryArithIx4(LSimdBinaryArithIx4* lir);
    void visitSimdBinaryArithFx4(LSimdBinaryArithFx4* lir);
    void visitSimdBinarySaturating(LSimdBinarySaturating* lir);
    void visitSimdBinaryBitwise(LSimdBinaryBitwise* lir);
    void visitSimdShift(LSimdShift* lir);
    void visitSimdSelect(LSimdSelect* ins);
    void visitSimdAllTrue(LSimdAllTrue* ins);
    void visitSimdAnyTrue(LSimdAnyTrue* ins);

    template <class T, class Reg> void visitSimdGeneralShuffle(LSimdGeneralShuffleBase* lir, Reg temp);
    void visitSimdGeneralShuffleI(LSimdGeneralShuffleI* lir);
    void visitSimdGeneralShuffleF(LSimdGeneralShuffleF* lir);

    // Out of line visitors.
    void visitOutOfLineBailout(OutOfLineBailout* ool);
    void visitOutOfLineUndoALUOperation(OutOfLineUndoALUOperation* ool);
    void visitMulNegativeZeroCheck(MulNegativeZeroCheck* ool);
    void visitModOverflowCheck(ModOverflowCheck* ool);
    void visitReturnZero(ReturnZero* ool);
    void visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool);
    void visitOutOfLineSimdFloatToIntCheck(OutOfLineSimdFloatToIntCheck* ool);
    void generateInvalidateEpilogue();

    // Generating a result.
    template<typename S, typename T>
    void atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType, const S& value,
                                    const T& mem, Register temp1, Register temp2, AnyRegister output);

    // Generating no result.
    template<typename S, typename T>
    void atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType, const S& value, const T& mem);

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

    void accept(CodeGeneratorX86Shared* codegen);

    LSnapshot* snapshot() const {
        return snapshot_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_x86_shared_CodeGenerator_x86_shared_h */
