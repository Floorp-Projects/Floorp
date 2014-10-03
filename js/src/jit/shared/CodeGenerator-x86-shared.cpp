/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/CodeGenerator-x86-shared.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include "jsmath.h"

#include "jit/IonFrames.h"
#include "jit/IonLinker.h"
#include "jit/JitCompartment.h"
#include "jit/RangeAnalysis.h"
#include "vm/TraceLogging.h"

#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Abs;
using mozilla::FloatingPoint;
using mozilla::FloorLog2;
using mozilla::NegativeInfinity;
using mozilla::SpecificNaN;

using JS::GenericNaN;

namespace js {
namespace jit {

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

bool
CodeGeneratorX86Shared::generatePrologue()
{
    MOZ_ASSERT(!gen->compilingAsmJS());

    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameSize());

    return true;
}

bool
CodeGeneratorX86Shared::generateEpilogue()
{
    MOZ_ASSERT(!gen->compilingAsmJS());

    masm.bind(&returnLabel_);

#ifdef JS_TRACE_LOGGING
    if (gen->info().executionMode() == SequentialExecution) {
        if (!emitTracelogStopEvent(TraceLogger::IonMonkey))
            return false;
        if (!emitTracelogScriptStop())
            return false;
    }
#endif

    // Pop the stack we allocated at the start of the function.
    masm.freeStack(frameSize());
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.ret();
    return true;
}

bool
OutOfLineBailout::accept(CodeGeneratorX86Shared *codegen)
{
    return codegen->visitOutOfLineBailout(this);
}

void
CodeGeneratorX86Shared::emitBranch(Assembler::Condition cond, MBasicBlock *mirTrue,
                                   MBasicBlock *mirFalse, Assembler::NaNCond ifNaN)
{
    if (ifNaN == Assembler::NaN_IsFalse)
        jumpToBlock(mirFalse, Assembler::Parity);
    else if (ifNaN == Assembler::NaN_IsTrue)
        jumpToBlock(mirTrue, Assembler::Parity);

    if (isNextBlock(mirFalse->lir())) {
        jumpToBlock(mirTrue, cond);
    } else {
        jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
        jumpToBlock(mirTrue);
    }
}

bool
CodeGeneratorX86Shared::visitDouble(LDouble *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitFloat32(LFloat32 *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantFloat32(ins->getFloat(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitTestIAndBranch(LTestIAndBranch *test)
{
    const LAllocation *opd = test->input();

    // Test the operand
    masm.testl(ToRegister(opd), ToRegister(opd));
    emitBranch(Assembler::NonZero, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitTestDAndBranch(LTestDAndBranch *test)
{
    const LAllocation *opd = test->input();

    // ucomisd flags:
    //             Z  P  C
    //            ---------
    //      NaN    1  1  1
    //        >    0  0  0
    //        <    0  0  1
    //        =    1  0  0
    //
    // NaN is falsey, so comparing against 0 and then using the Z flag is
    // enough to determine which branch to take.
    masm.xorpd(ScratchDoubleReg, ScratchDoubleReg);
    masm.ucomisd(ToFloatRegister(opd), ScratchDoubleReg);
    emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitTestFAndBranch(LTestFAndBranch *test)
{
    const LAllocation *opd = test->input();
    // ucomiss flags are the same as doubles; see comment above
    masm.xorps(ScratchFloat32Reg, ScratchFloat32Reg);
    masm.ucomiss(ToFloatRegister(opd), ScratchFloat32Reg);
    emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitBitAndAndBranch(LBitAndAndBranch *baab)
{
    if (baab->right()->isConstant())
        masm.testl(ToRegister(baab->left()), Imm32(ToInt32(baab->right())));
    else
        masm.testl(ToRegister(baab->left()), ToRegister(baab->right()));
    emitBranch(Assembler::NonZero, baab->ifTrue(), baab->ifFalse());
    return true;
}

void
CodeGeneratorX86Shared::emitCompare(MCompare::CompareType type, const LAllocation *left, const LAllocation *right)
{
#ifdef JS_CODEGEN_X64
    if (type == MCompare::Compare_Object) {
        masm.cmpq(ToRegister(left), ToOperand(right));
        return;
    }
#endif

    if (right->isConstant())
        masm.cmpl(ToRegister(left), Imm32(ToInt32(right)));
    else
        masm.cmpl(ToRegister(left), ToOperand(right));
}

bool
CodeGeneratorX86Shared::visitCompare(LCompare *comp)
{
    MCompare *mir = comp->mir();
    emitCompare(mir->compareType(), comp->left(), comp->right());
    masm.emitSet(JSOpToCondition(mir->compareType(), comp->jsop()), ToRegister(comp->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareAndBranch(LCompareAndBranch *comp)
{
    MCompare *mir = comp->cmpMir();
    emitCompare(mir->compareType(), comp->left(), comp->right());
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), comp->jsop());
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareD(LCompareD *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->mir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareDouble(cond, lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareF(LCompareF *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->mir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareFloat(cond, lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitNotI(LNotI *ins)
{
    masm.cmpl(ToRegister(ins->input()), Imm32(0));
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitNotD(LNotD *ins)
{
    FloatRegister opd = ToFloatRegister(ins->input());

    // Not returns true if the input is a NaN. We don't have to worry about
    // it if we know the input is never NaN though.
    Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
    if (ins->mir()->operandIsNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.xorpd(ScratchDoubleReg, ScratchDoubleReg);
    masm.compareDouble(Assembler::DoubleEqualOrUnordered, opd, ScratchDoubleReg);
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitNotF(LNotF *ins)
{
    FloatRegister opd = ToFloatRegister(ins->input());

    // Not returns true if the input is a NaN. We don't have to worry about
    // it if we know the input is never NaN though.
    Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
    if (ins->mir()->operandIsNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.xorps(ScratchFloat32Reg, ScratchFloat32Reg);
    masm.compareFloat(Assembler::DoubleEqualOrUnordered, opd, ScratchFloat32Reg);
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->cmpMir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareDouble(cond, lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse(), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareFAndBranch(LCompareFAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->cmpMir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareFloat(cond, lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse(), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitAsmJSPassStackArg(LAsmJSPassStackArg *ins)
{
    const MAsmJSPassStackArg *mir = ins->mir();
    Address dst(StackPointer, mir->spOffset());
    if (ins->arg()->isConstant()) {
        masm.storePtr(ImmWord(ToInt32(ins->arg())), dst);
    } else {
        if (ins->arg()->isGeneralReg()) {
            masm.storePtr(ToRegister(ins->arg()), dst);
        } else {
            switch (mir->input()->type()) {
              case MIRType_Double:
              case MIRType_Float32:
                masm.storeDouble(ToFloatRegister(ins->arg()), dst);
                return true;
              // StackPointer is SimdStackAlignment-aligned and ABIArgGenerator guarantees stack
              // offsets are SimdStackAlignment-aligned.
              case MIRType_Int32x4:
                masm.storeAlignedInt32x4(ToFloatRegister(ins->arg()), dst);
                return true;
              case MIRType_Float32x4:
                masm.storeAlignedFloat32x4(ToFloatRegister(ins->arg()), dst);
                return true;
              default: break;
            }
            MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected mir type in AsmJSPassStackArg");
        }
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitOutOfLineLoadTypedArrayOutOfBounds(OutOfLineLoadTypedArrayOutOfBounds *ool)
{
    if (ool->dest().isFloat()) {
        if (ool->isFloat32Load())
            masm.loadConstantFloat32(float(GenericNaN()), ool->dest().fpu());
        else
            masm.loadConstantDouble(GenericNaN(), ool->dest().fpu());
    } else {
        Register destReg = ool->dest().gpr();
        masm.mov(ImmWord(0), destReg);
    }
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86Shared::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_.used()) {
        // All non-table-based bailouts will go here.
        masm.bind(&deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        masm.push(Imm32(frameSize()));

        JitCode *handler = gen->jitRuntime()->getGenericBailoutHandler(gen->info().executionMode());
        masm.jmp(ImmPtr(handler->raw()), Relocation::JITCODE);
    }

    return true;
}

class BailoutJump {
    Assembler::Condition cond_;

  public:
    explicit BailoutJump(Assembler::Condition cond) : cond_(cond)
    { }
#ifdef JS_CODEGEN_X86
    void operator()(MacroAssembler &masm, uint8_t *code) const {
        masm.j(cond_, ImmPtr(code), Relocation::HARDCODED);
    }
#endif
    void operator()(MacroAssembler &masm, Label *label) const {
        masm.j(cond_, label);
    }
};

class BailoutLabel {
    Label *label_;

  public:
    explicit BailoutLabel(Label *label) : label_(label)
    { }
#ifdef JS_CODEGEN_X86
    void operator()(MacroAssembler &masm, uint8_t *code) const {
        masm.retarget(label_, ImmPtr(code), Relocation::HARDCODED);
    }
#endif
    void operator()(MacroAssembler &masm, Label *label) const {
        masm.retarget(label_, label);
    }
};

template <typename T> bool
CodeGeneratorX86Shared::bailout(const T &binder, LSnapshot *snapshot)
{
    if (!encode(snapshot))
        return false;

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                  frameClass_.frameSize() == masm.framePushed());

#ifdef JS_CODEGEN_X86
    // On x64, bailout tables are pointless, because 16 extra bytes are
    // reserved per external jump, whereas it takes only 10 bytes to encode a
    // a non-table based bailout.
    if (assignBailoutId(snapshot)) {
        binder(masm, deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE);
        return true;
    }
#endif

    // We could not use a jump table, either because all bailout IDs were
    // reserved, or a jump table is not optimal for this frame size or
    // platform. Whatever, we will generate a lazy bailout.
    //
    // All bailout code is associated with the bytecodeSite of the block we are
    // bailing out from.
    InlineScriptTree *tree = snapshot->mir()->block()->trackedTree();
    OutOfLineBailout *ool = new(alloc()) OutOfLineBailout(snapshot);
    if (!addOutOfLineCode(ool, BytecodeSite(tree, tree->script()->code())))
        return false;

    binder(masm, ool->entry());
    return true;
}

bool
CodeGeneratorX86Shared::bailoutIf(Assembler::Condition condition, LSnapshot *snapshot)
{
    return bailout(BailoutJump(condition), snapshot);
}

bool
CodeGeneratorX86Shared::bailoutIf(Assembler::DoubleCondition condition, LSnapshot *snapshot)
{
    MOZ_ASSERT(Assembler::NaNCondFromDoubleCondition(condition) == Assembler::NaN_HandledByCond);
    return bailoutIf(Assembler::ConditionFromDoubleCondition(condition), snapshot);
}

bool
CodeGeneratorX86Shared::bailoutFrom(Label *label, LSnapshot *snapshot)
{
    MOZ_ASSERT(label->used() && !label->bound());
    return bailout(BailoutLabel(label), snapshot);
}

bool
CodeGeneratorX86Shared::bailout(LSnapshot *snapshot)
{
    Label label;
    masm.jump(&label);
    return bailoutFrom(&label, snapshot);
}

bool
CodeGeneratorX86Shared::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    masm.push(Imm32(ool->snapshot()->snapshotOffset()));
    masm.jmp(&deoptLabel_);
    return true;
}

bool
CodeGeneratorX86Shared::visitMinMaxD(LMinMaxD *ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());
#ifdef DEBUG
    FloatRegister output = ToFloatRegister(ins->output());
    MOZ_ASSERT(first == output);
#endif

    Label done, nan, minMaxInst;

    // Do a ucomisd to catch equality and NaNs, which both require special
    // handling. If the operands are ordered and inequal, we branch straight to
    // the min/max instruction. If we wanted, we could also branch for less-than
    // or greater-than here instead of using min/max, however these conditions
    // will sometimes be hard on the branch predictor.
    masm.ucomisd(first, second);
    masm.j(Assembler::NotEqual, &minMaxInst);
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN())
        masm.j(Assembler::Parity, &nan);

    // Ordered and equal. The operands are bit-identical unless they are zero
    // and negative zero. These instructions merge the sign bits in that
    // case, and are no-ops otherwise.
    if (ins->mir()->isMax())
        masm.andpd(second, first);
    else
        masm.orpd(second, first);
    masm.jump(&done);

    // x86's min/max are not symmetric; if either operand is a NaN, they return
    // the read-only operand. We need to return a NaN if either operand is a
    // NaN, so we explicitly check for a NaN in the read-write operand.
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN()) {
        masm.bind(&nan);
        masm.ucomisd(first, first);
        masm.j(Assembler::Parity, &done);
    }

    // When the values are inequal, or second is NaN, x86's min and max will
    // return the value we need.
    masm.bind(&minMaxInst);
    if (ins->mir()->isMax())
        masm.maxsd(second, first);
    else
        masm.minsd(second, first);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86Shared::visitMinMaxF(LMinMaxF *ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());
#ifdef DEBUG
    FloatRegister output = ToFloatRegister(ins->output());
    MOZ_ASSERT(first == output);
#endif

    Label done, nan, minMaxInst;

    // Do a ucomiss to catch equality and NaNs, which both require special
    // handling. If the operands are ordered and inequal, we branch straight to
    // the min/max instruction. If we wanted, we could also branch for less-than
    // or greater-than here instead of using min/max, however these conditions
    // will sometimes be hard on the branch predictor.
    masm.ucomiss(first, second);
    masm.j(Assembler::NotEqual, &minMaxInst);
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN())
        masm.j(Assembler::Parity, &nan);

    // Ordered and equal. The operands are bit-identical unless they are zero
    // and negative zero. These instructions merge the sign bits in that
    // case, and are no-ops otherwise.
    if (ins->mir()->isMax())
        masm.andps(second, first);
    else
        masm.orps(second, first);
    masm.jump(&done);

    // x86's min/max are not symmetric; if either operand is a NaN, they return
    // the read-only operand. We need to return a NaN if either operand is a
    // NaN, so we explicitly check for a NaN in the read-write operand.
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN()) {
        masm.bind(&nan);
        masm.ucomiss(first, first);
        masm.j(Assembler::Parity, &done);
    }

    // When the values are inequal, or second is NaN, x86's min and max will
    // return the value we need.
    masm.bind(&minMaxInst);
    if (ins->mir()->isMax())
        masm.maxss(second, first);
    else
        masm.minss(second, first);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86Shared::visitAbsD(LAbsD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    // Load a value which is all ones except for the sign bit.
    masm.loadConstantDouble(SpecificNaN<double>(0, FloatingPoint<double>::kSignificandBits),
                            ScratchDoubleReg);
    masm.andpd(ScratchDoubleReg, input);
    return true;
}

bool
CodeGeneratorX86Shared::visitAbsF(LAbsF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    // Same trick as visitAbsD above.
    masm.loadConstantFloat32(SpecificNaN<float>(0, FloatingPoint<float>::kSignificandBits),
                             ScratchFloat32Reg);
    masm.andps(ScratchFloat32Reg, input);
    return true;
}

bool
CodeGeneratorX86Shared::visitClzI(LClzI *ins)
{
    Register input = ToRegister(ins->input());
    Register output = ToRegister(ins->output());

    // bsr is undefined on 0
    Label done, nonzero;
    if (!ins->mir()->operandIsNeverZero()) {
        masm.testl(input, input);
        masm.j(Assembler::NonZero, &nonzero);
        masm.move32(Imm32(32), output);
        masm.jump(&done);
    }

    masm.bind(&nonzero);
    masm.bsr(input, output);
    masm.xor32(Imm32(0x1F), output);
    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86Shared::visitSqrtD(LSqrtD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.sqrtsd(input, output);
    return true;
}

bool
CodeGeneratorX86Shared::visitSqrtF(LSqrtF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.sqrtss(input, output);
    return true;
}

bool
CodeGeneratorX86Shared::visitPowHalfD(LPowHalfD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));

    Label done, sqrt;

    if (!ins->mir()->operandIsNeverNegativeInfinity()) {
        // Branch if not -Infinity.
        masm.loadConstantDouble(NegativeInfinity<double>(), ScratchDoubleReg);

        Assembler::DoubleCondition cond = Assembler::DoubleNotEqualOrUnordered;
        if (ins->mir()->operandIsNeverNaN())
            cond = Assembler::DoubleNotEqual;
        masm.branchDouble(cond, input, ScratchDoubleReg, &sqrt);

        // Math.pow(-Infinity, 0.5) == Infinity.
        masm.xorpd(input, input);
        masm.subsd(ScratchDoubleReg, input);
        masm.jump(&done);

        masm.bind(&sqrt);
    }

    if (!ins->mir()->operandIsNeverNegativeZero()) {
        // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5). Adding 0 converts any -0 to 0.
        masm.xorpd(ScratchDoubleReg, ScratchDoubleReg);
        masm.addsd(ScratchDoubleReg, input);
    }

    masm.sqrtsd(input, input);

    masm.bind(&done);
    return true;
}

class OutOfLineUndoALUOperation : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    LInstruction *ins_;

  public:
    explicit OutOfLineUndoALUOperation(LInstruction *ins)
        : ins_(ins)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitOutOfLineUndoALUOperation(this);
    }
    LInstruction *ins() const {
        return ins_;
    }
};

bool
CodeGeneratorX86Shared::visitAddI(LAddI *ins)
{
    if (ins->rhs()->isConstant())
        masm.addl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.addl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot()) {
        if (ins->recoversInput()) {
            OutOfLineUndoALUOperation *ool = new(alloc()) OutOfLineUndoALUOperation(ins);
            if (!addOutOfLineCode(ool, ins->mir()))
                return false;
            masm.j(Assembler::Overflow, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Overflow, ins->snapshot()))
                return false;
        }
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitSubI(LSubI *ins)
{
    if (ins->rhs()->isConstant())
        masm.subl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.subl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot()) {
        if (ins->recoversInput()) {
            OutOfLineUndoALUOperation *ool = new(alloc()) OutOfLineUndoALUOperation(ins);
            if (!addOutOfLineCode(ool, ins->mir()))
                return false;
            masm.j(Assembler::Overflow, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Overflow, ins->snapshot()))
                return false;
        }
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitOutOfLineUndoALUOperation(OutOfLineUndoALUOperation *ool)
{
    LInstruction *ins = ool->ins();
    Register reg = ToRegister(ins->getDef(0));

    mozilla::DebugOnly<LAllocation *> lhs = ins->getOperand(0);
    LAllocation *rhs = ins->getOperand(1);

    MOZ_ASSERT(reg == ToRegister(lhs));
    MOZ_ASSERT_IF(rhs->isGeneralReg(), reg != ToRegister(rhs));

    // Undo the effect of the ALU operation, which was performed on the output
    // register and overflowed. Writing to the output register clobbered an
    // input reg, and the original value of the input needs to be recovered
    // to satisfy the constraint imposed by any RECOVERED_INPUT operands to
    // the bailout snapshot.

    if (rhs->isConstant()) {
        Imm32 constant(ToInt32(rhs));
        if (ins->isAddI())
            masm.subl(constant, reg);
        else
            masm.addl(constant, reg);
    } else {
        if (ins->isAddI())
            masm.subl(ToOperand(rhs), reg);
        else
            masm.addl(ToOperand(rhs), reg);
    }

    return bailout(ool->ins()->snapshot());
}

class MulNegativeZeroCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    LMulI *ins_;

  public:
    explicit MulNegativeZeroCheck(LMulI *ins)
      : ins_(ins)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitMulNegativeZeroCheck(this);
    }
    LMulI *ins() const {
        return ins_;
    }
};

bool
CodeGeneratorX86Shared::visitMulI(LMulI *ins)
{
    const LAllocation *lhs = ins->lhs();
    const LAllocation *rhs = ins->rhs();
    MMul *mul = ins->mir();
    MOZ_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    if (rhs->isConstant()) {
        // Bailout on -0.0
        int32_t constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::Signed : Assembler::Equal;
            masm.testl(ToRegister(lhs), ToRegister(lhs));
            if (!bailoutIf(bailoutCond, ins->snapshot()))
                    return false;
        }

        switch (constant) {
          case -1:
            masm.negl(ToOperand(lhs));
            break;
          case 0:
            masm.xorl(ToOperand(lhs), ToRegister(lhs));
            return true; // escape overflow check;
          case 1:
            // nop
            return true; // escape overflow check;
          case 2:
            masm.addl(ToOperand(lhs), ToRegister(lhs));
            break;
          default:
            if (!mul->canOverflow() && constant > 0) {
                // Use shift if cannot overflow and constant is power of 2
                int32_t shift = FloorLog2(constant);
                if ((1 << shift) == constant) {
                    masm.shll(Imm32(shift), ToRegister(lhs));
                    return true;
                }
            }
            masm.imull(Imm32(ToInt32(rhs)), ToRegister(lhs));
        }

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;
    } else {
        masm.imull(ToOperand(rhs), ToRegister(lhs));

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;

        if (mul->canBeNegativeZero()) {
            // Jump to an OOL path if the result is 0.
            MulNegativeZeroCheck *ool = new(alloc()) MulNegativeZeroCheck(ins);
            if (!addOutOfLineCode(ool, mul))
                return false;

            masm.testl(ToRegister(lhs), ToRegister(lhs));
            masm.j(Assembler::Zero, ool->entry());
            masm.bind(ool->rejoin());
        }
    }

    return true;
}

class ReturnZero : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    Register reg_;

  public:
    explicit ReturnZero(Register reg)
      : reg_(reg)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitReturnZero(this);
    }
    Register reg() const {
        return reg_;
    }
};

bool
CodeGeneratorX86Shared::visitReturnZero(ReturnZero *ool)
{
    masm.mov(ImmWord(0), ool->reg());
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86Shared::visitUDivOrMod(LUDivOrMod *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
    MOZ_ASSERT(rhs != edx);
    MOZ_ASSERT_IF(output == eax, ToRegister(ins->remainder()) == edx);

    ReturnZero *ool = nullptr;

    // Put the lhs in eax.
    if (lhs != eax)
        masm.mov(lhs, eax);

    // Prevent divide by zero.
    if (ins->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (ins->mir()->isTruncated()) {
            if (!ool)
                ool = new(alloc()) ReturnZero(output);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    // Zero extend the lhs into edx to make (edx:eax), since udiv is 64-bit.
    masm.mov(ImmWord(0), edx);
    masm.udiv(rhs);

    // If the remainder is > 0, bailout since this must be a double.
    if (ins->mir()->isDiv() && !ins->mir()->toDiv()->canTruncateRemainder()) {
        Register remainder = ToRegister(ins->remainder());
        masm.testl(remainder, remainder);
        if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
            return false;
    }

    // Unsigned div or mod can return a value that's not a signed int32.
    // If our users aren't expecting that, bail.
    if (!ins->mir()->isTruncated()) {
        masm.testl(output, output);
        if (!bailoutIf(Assembler::Signed, ins->snapshot()))
            return false;
    }

    if (ool) {
        if (!addOutOfLineCode(ool, ins->mir()))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitMulNegativeZeroCheck(MulNegativeZeroCheck *ool)
{
    LMulI *ins = ool->ins();
    Register result = ToRegister(ins->output());
    Operand lhsCopy = ToOperand(ins->lhsCopy());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT_IF(lhsCopy.kind() == Operand::REG, lhsCopy.reg() != result.code());

    // Result is -0 if lhs or rhs is negative.
    masm.movl(lhsCopy, result);
    masm.orl(rhs, result);
    if (!bailoutIf(Assembler::Signed, ins->snapshot()))
        return false;

    masm.mov(ImmWord(0), result);
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86Shared::visitDivPowTwoI(LDivPowTwoI *ins)
{
    Register lhs = ToRegister(ins->numerator());
    mozilla::DebugOnly<Register> output = ToRegister(ins->output());

    int32_t shift = ins->shift();
    bool negativeDivisor = ins->negativeDivisor();
    MDiv *mir = ins->mir();

    // We use defineReuseInput so these should always be the same, which is
    // convenient since all of our instructions here are two-address.
    MOZ_ASSERT(lhs == output);

    if (!mir->isTruncated() && negativeDivisor) {
        // 0 divided by a negative number must return a double.
        masm.testl(lhs, lhs);
        if (!bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
    }

    if (shift != 0) {
        if (!mir->isTruncated()) {
            // If the remainder is != 0, bailout since this must be a double.
            masm.testl(lhs, Imm32(UINT32_MAX >> (32 - shift)));
            if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
                return false;
        }

        // Adjust the value so that shifting produces a correctly rounded result
        // when the numerator is negative. See 10-1 "Signed Division by a Known
        // Power of 2" in Henry S. Warren, Jr.'s Hacker's Delight.
        if (mir->canBeNegativeDividend()) {
            Register lhsCopy = ToRegister(ins->numeratorCopy());
            MOZ_ASSERT(lhsCopy != lhs);
            if (shift > 1)
                masm.sarl(Imm32(31), lhs);
            masm.shrl(Imm32(32 - shift), lhs);
            masm.addl(lhsCopy, lhs);
        }

        masm.sarl(Imm32(shift), lhs);
        if (negativeDivisor)
            masm.negl(lhs);
    } else if (shift == 0 && negativeDivisor) {
        // INT32_MIN / -1 overflows.
        masm.negl(lhs);
        if (!mir->isTruncated() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitDivOrModConstantI(LDivOrModConstantI *ins) {
    Register lhs = ToRegister(ins->numerator());
    Register output = ToRegister(ins->output());
    int32_t d = ins->denominator();

    // This emits the division answer into edx or the modulus answer into eax.
    MOZ_ASSERT(output == eax || output == edx);
    MOZ_ASSERT(lhs != eax && lhs != edx);
    bool isDiv = (output == edx);

    // The absolute value of the denominator isn't a power of 2 (see LDivPowTwoI
    // and LModPowTwoI).
    MOZ_ASSERT((Abs(d) & (Abs(d) - 1)) != 0);

    // We will first divide by Abs(d), and negate the answer if d is negative.
    // If desired, this can be avoided by generalizing computeDivisionConstants.
    ReciprocalMulConstants rmc = computeDivisionConstants(Abs(d));

    // As explained in the comments of computeDivisionConstants, we first compute
    // X >> (32 + shift), where X is either (rmc.multiplier * n) if the multiplier
    // is non-negative or (rmc.multiplier * n) + (2^32 * n) otherwise. This is the
    // desired division result if n is non-negative, and is one less than the result
    // otherwise.
    masm.movl(Imm32(rmc.multiplier), eax);
    masm.imull(lhs);
    if (rmc.multiplier < 0)
        masm.addl(lhs, edx);
    masm.sarl(Imm32(rmc.shiftAmount), edx);

    // We'll subtract -1 instead of adding 1, because (n < 0 ? -1 : 0) can be
    // computed with just a sign-extending shift of 31 bits.
    if (ins->canBeNegativeDividend()) {
        masm.movl(lhs, eax);
        masm.sarl(Imm32(31), eax);
        masm.subl(eax, edx);
    }

    // After this, edx contains the correct truncated division result.
    if (d < 0)
        masm.negl(edx);

    if (!isDiv) {
        masm.imull(Imm32(-d), edx, eax);
        masm.addl(lhs, eax);
    }

    if (!ins->mir()->isTruncated()) {
        if (isDiv) {
            // This is a division op. Multiply the obtained value by d to check if
            // the correct answer is an integer. This cannot overflow, since |d| > 1.
            masm.imull(Imm32(d), edx, eax);
            masm.cmpl(lhs, eax);
            if (!bailoutIf(Assembler::NotEqual, ins->snapshot()))
                return false;

            // If lhs is zero and the divisor is negative, the answer should have
            // been -0.
            if (d < 0) {
                masm.testl(lhs, lhs);
                if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                    return false;
            }
        } else if (ins->canBeNegativeDividend()) {
            // This is a mod op. If the computed value is zero and lhs
            // is negative, the answer should have been -0.
            Label done;

            masm.cmpl(lhs, Imm32(0));
            masm.j(Assembler::GreaterThanOrEqual, &done);

            masm.testl(eax, eax);
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;

            masm.bind(&done);
        }
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitDivI(LDivI *ins)
{
    Register remainder = ToRegister(ins->remainder());
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    MDiv *mir = ins->mir();

    MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
    MOZ_ASSERT(rhs != edx);
    MOZ_ASSERT(remainder == edx);
    MOZ_ASSERT(output == eax);

    Label done;
    ReturnZero *ool = nullptr;

    // Put the lhs in eax, for either the negative overflow case or the regular
    // divide case.
    if (lhs != eax)
        masm.mov(lhs, eax);

    // Handle divide by zero.
    if (mir->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (mir->canTruncateInfinities()) {
            // Truncated division by zero is zero (Infinity|0 == 0)
            if (!ool)
                ool = new(alloc()) ReturnZero(output);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    // Handle an integer overflow exception from -2147483648 / -1.
    if (mir->canBeNegativeOverflow()) {
        Label notmin;
        masm.cmpl(lhs, Imm32(INT32_MIN));
        masm.j(Assembler::NotEqual, &notmin);
        masm.cmpl(rhs, Imm32(-1));
        if (mir->canTruncateOverflow()) {
            // (-INT32_MIN)|0 == INT32_MIN and INT32_MIN is already in the
            // output register (lhs == eax).
            masm.j(Assembler::Equal, &done);
        } else {
            MOZ_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Equal, ins->snapshot()))
                return false;
        }
        masm.bind(&notmin);
    }

    // Handle negative 0.
    if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
        Label nonzero;
        masm.testl(lhs, lhs);
        masm.j(Assembler::NonZero, &nonzero);
        masm.cmpl(rhs, Imm32(0));
        if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
            return false;
        masm.bind(&nonzero);
    }

    // Sign extend the lhs into edx to make (edx:eax), since idiv is 64-bit.
    if (lhs != eax)
        masm.mov(lhs, eax);
    masm.cdq();
    masm.idiv(rhs);

    if (!mir->canTruncateRemainder()) {
        // If the remainder is > 0, bailout since this must be a double.
        masm.testl(remainder, remainder);
        if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
            return false;
    }

    masm.bind(&done);

    if (ool) {
        if (!addOutOfLineCode(ool, mir))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitModPowTwoI(LModPowTwoI *ins)
{
    Register lhs = ToRegister(ins->getOperand(0));
    int32_t shift = ins->shift();

    Label negative;

    if (ins->mir()->canBeNegativeDividend()) {
        // Switch based on sign of the lhs.
        // Positive numbers are just a bitmask
        masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
    }

    masm.andl(Imm32((uint32_t(1) << shift) - 1), lhs);

    if (ins->mir()->canBeNegativeDividend()) {
        Label done;
        masm.jump(&done);

        // Negative numbers need a negate, bitmask, negate
        masm.bind(&negative);

        // Unlike in the visitModI case, we are not computing the mod by means of a
        // division. Therefore, the divisor = -1 case isn't problematic (the andl
        // always returns 0, which is what we expect).
        //
        // The negl instruction overflows if lhs == INT32_MIN, but this is also not
        // a problem: shift is at most 31, and so the andl also always returns 0.
        masm.negl(lhs);
        masm.andl(Imm32((uint32_t(1) << shift) - 1), lhs);
        masm.negl(lhs);

        // Since a%b has the same sign as b, and a is negative in this branch,
        // an answer of 0 means the correct result is actually -0. Bail out.
        if (!ins->mir()->isTruncated() && !bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
        masm.bind(&done);
    }
    return true;

}

class ModOverflowCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    Label done_;
    LModI *ins_;
    Register rhs_;

  public:
    explicit ModOverflowCheck(LModI *ins, Register rhs)
      : ins_(ins), rhs_(rhs)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitModOverflowCheck(this);
    }
    Label *done() {
        return &done_;
    }
    LModI *ins() const {
        return ins_;
    }
    Register rhs() const {
        return rhs_;
    }
};

bool
CodeGeneratorX86Shared::visitModOverflowCheck(ModOverflowCheck *ool)
{
    masm.cmpl(ool->rhs(), Imm32(-1));
    if (ool->ins()->mir()->isTruncated()) {
        masm.j(Assembler::NotEqual, ool->rejoin());
        masm.mov(ImmWord(0), edx);
        masm.jmp(ool->done());
    } else {
        if (!bailoutIf(Assembler::Equal, ool->ins()->snapshot()))
            return false;
       masm.jmp(ool->rejoin());
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitModI(LModI *ins)
{
    Register remainder = ToRegister(ins->remainder());
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());

    // Required to use idiv.
    MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
    MOZ_ASSERT(rhs != edx);
    MOZ_ASSERT(remainder == edx);
    MOZ_ASSERT(ToRegister(ins->getTemp(0)) == eax);

    Label done;
    ReturnZero *ool = nullptr;
    ModOverflowCheck *overflow = nullptr;

    // Set up eax in preparation for doing a div.
    if (lhs != eax)
        masm.mov(lhs, eax);

    // Prevent divide by zero.
    if (ins->mir()->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (ins->mir()->isTruncated()) {
            if (!ool)
                ool = new(alloc()) ReturnZero(edx);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    Label negative;

    // Switch based on sign of the lhs.
    if (ins->mir()->canBeNegativeDividend())
        masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);

    // If lhs >= 0 then remainder = lhs % rhs. The remainder must be positive.
    {
        // Check if rhs is a power-of-two.
        if (ins->mir()->canBePowerOfTwoDivisor()) {
            MOZ_ASSERT(rhs != remainder);

            // Rhs y is a power-of-two if (y & (y-1)) == 0. Note that if
            // y is any negative number other than INT32_MIN, both y and
            // y-1 will have the sign bit set so these are never optimized
            // as powers-of-two. If y is INT32_MIN, y-1 will be INT32_MAX
            // and because lhs >= 0 at this point, lhs & INT32_MAX returns
            // the correct value.
            Label notPowerOfTwo;
            masm.mov(rhs, remainder);
            masm.subl(Imm32(1), remainder);
            masm.branchTest32(Assembler::NonZero, remainder, rhs, &notPowerOfTwo);
            {
                masm.andl(lhs, remainder);
                masm.jmp(&done);
            }
            masm.bind(&notPowerOfTwo);
        }

        // Since lhs >= 0, the sign-extension will be 0
        masm.mov(ImmWord(0), edx);
        masm.idiv(rhs);
    }

    // Otherwise, we have to beware of two special cases:
    if (ins->mir()->canBeNegativeDividend()) {
        masm.jump(&done);

        masm.bind(&negative);

        // Prevent an integer overflow exception from -2147483648 % -1
        Label notmin;
        masm.cmpl(lhs, Imm32(INT32_MIN));
        overflow = new(alloc()) ModOverflowCheck(ins, rhs);
        masm.j(Assembler::Equal, overflow->entry());
        masm.bind(overflow->rejoin());
        masm.cdq();
        masm.idiv(rhs);

        if (!ins->mir()->isTruncated()) {
            // A remainder of 0 means that the rval must be -0, which is a double.
            masm.testl(remainder, remainder);
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    masm.bind(&done);

    if (overflow) {
        if (!addOutOfLineCode(overflow, ins->mir()))
            return false;
        masm.bind(overflow->done());
    }

    if (ool) {
        if (!addOutOfLineCode(ool, ins->mir()))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitBitNotI(LBitNotI *ins)
{
    const LAllocation *input = ins->getOperand(0);
    MOZ_ASSERT(!input->isConstant());

    masm.notl(ToOperand(input));
    return true;
}

bool
CodeGeneratorX86Shared::visitBitOpI(LBitOpI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    switch (ins->bitop()) {
        case JSOP_BITOR:
            if (rhs->isConstant())
                masm.orl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.orl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITXOR:
            if (rhs->isConstant())
                masm.xorl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.xorl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITAND:
            if (rhs->isConstant())
                masm.andl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.andl(ToOperand(rhs), ToRegister(lhs));
            break;
        default:
            MOZ_CRASH("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitShiftI(LShiftI *ins)
{
    Register lhs = ToRegister(ins->lhs());
    const LAllocation *rhs = ins->rhs();

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        switch (ins->bitop()) {
          case JSOP_LSH:
            if (shift)
                masm.shll(Imm32(shift), lhs);
            break;
          case JSOP_RSH:
            if (shift)
                masm.sarl(Imm32(shift), lhs);
            break;
          case JSOP_URSH:
            if (shift) {
                masm.shrl(Imm32(shift), lhs);
            } else if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                masm.testl(lhs, lhs);
                if (!bailoutIf(Assembler::Signed, ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_CRASH("Unexpected shift op");
        }
    } else {
        MOZ_ASSERT(ToRegister(rhs) == ecx);
        switch (ins->bitop()) {
          case JSOP_LSH:
            masm.shll_cl(lhs);
            break;
          case JSOP_RSH:
            masm.sarl_cl(lhs);
            break;
          case JSOP_URSH:
            masm.shrl_cl(lhs);
            if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                masm.testl(lhs, lhs);
                if (!bailoutIf(Assembler::Signed, ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_CRASH("Unexpected shift op");
        }
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitUrshD(LUrshD *ins)
{
    Register lhs = ToRegister(ins->lhs());
    MOZ_ASSERT(ToRegister(ins->temp()) == lhs);

    const LAllocation *rhs = ins->rhs();
    FloatRegister out = ToFloatRegister(ins->output());

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        if (shift)
            masm.shrl(Imm32(shift), lhs);
    } else {
        MOZ_ASSERT(ToRegister(rhs) == ecx);
        masm.shrl_cl(lhs);
    }

    masm.convertUInt32ToDouble(lhs, out);
    return true;
}

MoveOperand
CodeGeneratorX86Shared::toMoveOperand(const LAllocation *a) const
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    return MoveOperand(StackPointer, ToStackOffset(a));
}

class OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    MTableSwitch *mir_;
    CodeLabel jumpLabel_;

    bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    explicit OutOfLineTableSwitch(MTableSwitch *mir)
      : mir_(mir)
    {}

    MTableSwitch *mir() const {
        return mir_;
    }

    CodeLabel *jumpLabel() {
        return &jumpLabel_;
    }
};

bool
CodeGeneratorX86Shared::visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool)
{
    MTableSwitch *mir = ool->mir();

    masm.align(sizeof(void*));
    masm.bind(ool->jumpLabel()->src());
    if (!masm.addCodeLabel(*ool->jumpLabel()))
        return false;

    for (size_t i = 0; i < mir->numCases(); i++) {
        LBlock *caseblock = skipTrivialBlocks(mir->getCase(i))->lir();
        Label *caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl;
        masm.writeCodePointer(cl.dest());
        cl.src()->bind(caseoffset);
        if (!masm.addCodeLabel(cl))
            return false;
    }

    return true;
}

bool
CodeGeneratorX86Shared::emitTableSwitchDispatch(MTableSwitch *mir, Register index, Register base)
{
    Label *defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

    // Lower value with low value
    if (mir->low() != 0)
        masm.subl(Imm32(mir->low()), index);

    // Jump to default case if input is out of range
    int32_t cases = mir->numCases();
    masm.cmpl(index, Imm32(cases));
    masm.j(AssemblerX86Shared::AboveOrEqual, defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first
    // generate the case entries (we don't yet know their offsets in the
    // instruction stream).
    OutOfLineTableSwitch *ool = new(alloc()) OutOfLineTableSwitch(mir);
    if (!addOutOfLineCode(ool, mir))
        return false;

    // Compute the position where a pointer to the right case stands.
    masm.mov(ool->jumpLabel()->dest(), base);
    Operand pointer = Operand(base, index, ScalePointer);

    // Jump to the right case
    masm.jmp(pointer);

    return true;
}

bool
CodeGeneratorX86Shared::visitMathD(LMathD *math)
{
    FloatRegister lhs = ToFloatRegister(math->lhs());
    Operand rhs = ToOperand(math->rhs());

    MOZ_ASSERT(ToFloatRegister(math->output()) == lhs);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.addsd(rhs, lhs);
        break;
      case JSOP_SUB:
        masm.subsd(rhs, lhs);
        break;
      case JSOP_MUL:
        masm.mulsd(rhs, lhs);
        break;
      case JSOP_DIV:
        masm.divsd(rhs, lhs);
        break;
      default:
        MOZ_CRASH("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitMathF(LMathF *math)
{
    FloatRegister lhs = ToFloatRegister(math->lhs());
    Operand rhs = ToOperand(math->rhs());

    MOZ_ASSERT(ToFloatRegister(math->output()) == lhs);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.addss(rhs, lhs);
        break;
      case JSOP_SUB:
        masm.subss(rhs, lhs);
        break;
      case JSOP_MUL:
        masm.mulss(rhs, lhs);
        break;
      case JSOP_DIV:
        masm.divss(rhs, lhs);
        break;
      default:
        MOZ_CRASH("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitFloor(LFloor *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchDoubleReg;
    Register output = ToRegister(lir->output());

    Label bailout;

    if (AssemblerX86Shared::HasSSE41()) {
        // Bail on negative-zero.
        masm.branchNegativeZero(input, output, &bailout);
        if (!bailoutFrom(&bailout, lir->snapshot()))
            return false;

        // Round toward -Infinity.
        masm.roundsd(input, scratch, X86Assembler::RoundDown);

        if (!bailoutCvttsd2si(scratch, output, lir->snapshot()))
            return false;
    } else {
        Label negative, end;

        // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
        masm.xorpd(scratch, scratch);
        masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &negative);

        // Bail on negative-zero.
        masm.branchNegativeZero(input, output, &bailout);
        if (!bailoutFrom(&bailout, lir->snapshot()))
            return false;

        // Input is non-negative, so truncation correctly rounds.
        if (!bailoutCvttsd2si(input, output, lir->snapshot()))
            return false;

        masm.jump(&end);

        // Input is negative, but isn't -0.
        // Negative values go on a comparatively expensive path, since no
        // native rounding mode matches JS semantics. Still better than callVM.
        masm.bind(&negative);
        {
            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            if (!bailoutCvttsd2si(input, output, lir->snapshot()))
                return false;

            // Test whether the input double was integer-valued.
            masm.convertInt32ToDouble(output, scratch);
            masm.branchDouble(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }

        masm.bind(&end);
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitFloorF(LFloorF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloat32Reg;
    Register output = ToRegister(lir->output());

    Label bailout;

    if (AssemblerX86Shared::HasSSE41()) {
        // Bail on negative-zero.
        masm.branchNegativeZeroFloat32(input, output, &bailout);
        if (!bailoutFrom(&bailout, lir->snapshot()))
            return false;

        // Round toward -Infinity.
        masm.roundss(input, scratch, X86Assembler::RoundDown);

        if (!bailoutCvttss2si(scratch, output, lir->snapshot()))
            return false;
    } else {
        Label negative, end;

        // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
        masm.xorps(scratch, scratch);
        masm.branchFloat(Assembler::DoubleLessThan, input, scratch, &negative);

        // Bail on negative-zero.
        masm.branchNegativeZeroFloat32(input, output, &bailout);
        if (!bailoutFrom(&bailout, lir->snapshot()))
            return false;

        // Input is non-negative, so truncation correctly rounds.
        if (!bailoutCvttss2si(input, output, lir->snapshot()))
            return false;

        masm.jump(&end);

        // Input is negative, but isn't -0.
        // Negative values go on a comparatively expensive path, since no
        // native rounding mode matches JS semantics. Still better than callVM.
        masm.bind(&negative);
        {
            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            if (!bailoutCvttss2si(input, output, lir->snapshot()))
                return false;

            // Test whether the input double was integer-valued.
            masm.convertInt32ToFloat32(output, scratch);
            masm.branchFloat(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }

        masm.bind(&end);
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitCeil(LCeil *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchDoubleReg;
    Register output = ToRegister(lir->output());

    Label bailout, lessThanMinusOne;

    // Bail on ]-1; -0] range
    masm.loadConstantDouble(-1, scratch);
    masm.branchDouble(Assembler::DoubleLessThanOrEqualOrUnordered, input,
                      scratch, &lessThanMinusOne);

    // Test for remaining values with the sign bit set, i.e. ]-1; -0]
    masm.movmskpd(input, output);
    masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
    if (!bailoutFrom(&bailout, lir->snapshot()))
        return false;

    if (AssemblerX86Shared::HasSSE41()) {
        // x <= -1 or x > -0
        masm.bind(&lessThanMinusOne);
        // Round toward +Infinity.
        masm.roundsd(input, scratch, X86Assembler::RoundUp);
        return bailoutCvttsd2si(scratch, output, lir->snapshot());
    }

    // No SSE4.1
    Label end;

    // x >= 0 and x is not -0.0, we can truncate (resp. truncate and add 1) for
    // integer (resp. non-integer) values.
    // Will also work for values >= INT_MAX + 1, as the truncate
    // operation will return INT_MIN and there'll be a bailout.
    if (!bailoutCvttsd2si(input, output, lir->snapshot()))
        return false;
    masm.convertInt32ToDouble(output, scratch);
    masm.branchDouble(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

    // Input is not integer-valued, add 1 to obtain the ceiling value
    masm.addl(Imm32(1), output);
    // if input > INT_MAX, output == INT_MAX so adding 1 will overflow.
    if (!bailoutIf(Assembler::Overflow, lir->snapshot()))
        return false;
    masm.jump(&end);

    // x <= -1, truncation is the way to go.
    masm.bind(&lessThanMinusOne);
    if (!bailoutCvttsd2si(input, output, lir->snapshot()))
        return false;

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorX86Shared::visitCeilF(LCeilF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloat32Reg;
    Register output = ToRegister(lir->output());

    Label bailout, lessThanMinusOne;

    // Bail on ]-1; -0] range
    masm.loadConstantFloat32(-1.f, scratch);
    masm.branchFloat(Assembler::DoubleLessThanOrEqualOrUnordered, input,
                     scratch, &lessThanMinusOne);

    // Test for remaining values with the sign bit set, i.e. ]-1; -0]
    masm.movmskps(input, output);
    masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
    if (!bailoutFrom(&bailout, lir->snapshot()))
        return false;

    if (AssemblerX86Shared::HasSSE41()) {
        // x <= -1 or x > -0
        masm.bind(&lessThanMinusOne);
        // Round toward +Infinity.
        masm.roundss(input, scratch, X86Assembler::RoundUp);
        return bailoutCvttss2si(scratch, output, lir->snapshot());
    }

    // No SSE4.1
    Label end;

    // x >= 0 and x is not -0.0, we can truncate (resp. truncate and add 1) for
    // integer (resp. non-integer) values.
    // Will also work for values >= INT_MAX + 1, as the truncate
    // operation will return INT_MIN and there'll be a bailout.
    if (!bailoutCvttss2si(input, output, lir->snapshot()))
        return false;
    masm.convertInt32ToFloat32(output, scratch);
    masm.branchFloat(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

    // Input is not integer-valued, add 1 to obtain the ceiling value
    masm.addl(Imm32(1), output);
    // if input > INT_MAX, output == INT_MAX so adding 1 will overflow.
    if (!bailoutIf(Assembler::Overflow, lir->snapshot()))
        return false;
    masm.jump(&end);

    // x <= -1, truncation is the way to go.
    masm.bind(&lessThanMinusOne);
    if (!bailoutCvttss2si(input, output, lir->snapshot()))
        return false;

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorX86Shared::visitRound(LRound *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister temp = ToFloatRegister(lir->temp());
    FloatRegister scratch = ScratchDoubleReg;
    Register output = ToRegister(lir->output());

    Label negativeOrZero, negative, end, bailout;

    // Branch to a slow path for non-positive inputs. Doesn't catch NaN.
    masm.xorpd(scratch, scratch);
    masm.branchDouble(Assembler::DoubleLessThanOrEqual, input, scratch, &negativeOrZero);

    // Input is positive. Add the biggest double less than 0.5 and
    // truncate, rounding down (because if the input is the biggest double less
    // than 0.5, adding 0.5 would undesirably round up to 1). Note that we have
    // to add the input to the temp register because we're not allowed to
    // modify the input register.
    masm.loadConstantDouble(GetBiggestNumberLessThan(0.5), temp);
    masm.addsd(input, temp);
    if (!bailoutCvttsd2si(temp, output, lir->snapshot()))
        return false;

    masm.jump(&end);

    // Input is negative, +0 or -0.
    masm.bind(&negativeOrZero);
    // Branch on negative input.
    masm.j(Assembler::NotEqual, &negative);

    // Bail on negative-zero.
    masm.branchNegativeZero(input, output, &bailout, /* maybeNonZero = */ false);
    if (!bailoutFrom(&bailout, lir->snapshot()))
        return false;

    // Input is +0
    masm.xor32(output, output);
    masm.jump(&end);

    // Input is negative.
    masm.bind(&negative);
    masm.loadConstantDouble(0.5, temp);

    if (AssemblerX86Shared::HasSSE41()) {
        // Add 0.5 and round toward -Infinity. The result is stored in the temp
        // register (currently contains 0.5).
        masm.addsd(input, temp);
        masm.roundsd(temp, scratch, X86Assembler::RoundDown);

        // Truncate.
        if (!bailoutCvttsd2si(scratch, output, lir->snapshot()))
            return false;

        // If the result is positive zero, then the actual result is -0. Bail.
        // Otherwise, the truncation will have produced the correct negative integer.
        masm.testl(output, output);
        if (!bailoutIf(Assembler::Zero, lir->snapshot()))
            return false;

    } else {
        masm.addsd(input, temp);

        // Round toward -Infinity without the benefit of ROUNDSD.
        {
            // If input + 0.5 >= 0, input is a negative number >= -0.5 and the result is -0.
            masm.compareDouble(Assembler::DoubleGreaterThanOrEqual, temp, scratch);
            if (!bailoutIf(Assembler::DoubleGreaterThanOrEqual, lir->snapshot()))
                return false;

            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            if (!bailoutCvttsd2si(temp, output, lir->snapshot()))
                return false;

            // Test whether the truncated double was integer-valued.
            masm.convertInt32ToDouble(output, scratch);
            masm.branchDouble(Assembler::DoubleEqualOrUnordered, temp, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }
    }

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorX86Shared::visitRoundF(LRoundF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister temp = ToFloatRegister(lir->temp());
    FloatRegister scratch = ScratchFloat32Reg;
    Register output = ToRegister(lir->output());

    Label negativeOrZero, negative, end, bailout;

    // Branch to a slow path for non-positive inputs. Doesn't catch NaN.
    masm.xorps(scratch, scratch);
    masm.branchFloat(Assembler::DoubleLessThanOrEqual, input, scratch, &negativeOrZero);

    // Input is non-negative. Add the biggest float less than 0.5 and truncate,
    // rounding down (because if the input is the biggest float less than 0.5,
    // adding 0.5 would undesirably round up to 1). Note that we have to add
    // the input to the temp register because we're not allowed to modify the
    // input register.
    masm.loadConstantFloat32(GetBiggestNumberLessThan(0.5f), temp);
    masm.addss(input, temp);

    if (!bailoutCvttss2si(temp, output, lir->snapshot()))
        return false;

    masm.jump(&end);

    // Input is negative, +0 or -0.
    masm.bind(&negativeOrZero);
    // Branch on negative input.
    masm.j(Assembler::NotEqual, &negative);

    // Bail on negative-zero.
    masm.branchNegativeZeroFloat32(input, output, &bailout);
    if (!bailoutFrom(&bailout, lir->snapshot()))
        return false;

    // Input is +0.
    masm.xor32(output, output);
    masm.jump(&end);

    // Input is negative.
    masm.bind(&negative);
    masm.loadConstantFloat32(0.5f, temp);

    if (AssemblerX86Shared::HasSSE41()) {
        // Add 0.5 and round toward -Infinity. The result is stored in the temp
        // register (currently contains 0.5).
        masm.addss(input, temp);
        masm.roundss(temp, scratch, X86Assembler::RoundDown);

        // Truncate.
        if (!bailoutCvttss2si(scratch, output, lir->snapshot()))
            return false;

        // If the result is positive zero, then the actual result is -0. Bail.
        // Otherwise, the truncation will have produced the correct negative integer.
        masm.testl(output, output);
        if (!bailoutIf(Assembler::Zero, lir->snapshot()))
            return false;

    } else {
        masm.addss(input, temp);
        // Round toward -Infinity without the benefit of ROUNDSS.
        {
            // If input + 0.5 >= 0, input is a negative number >= -0.5 and the result is -0.
            masm.compareFloat(Assembler::DoubleGreaterThanOrEqual, temp, scratch);
            if (!bailoutIf(Assembler::DoubleGreaterThanOrEqual, lir->snapshot()))
                return false;

            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            if (!bailoutCvttss2si(temp, output, lir->snapshot()))
                return false;

            // Test whether the truncated double was integer-valued.
            masm.convertInt32ToFloat32(output, scratch);
            masm.branchFloat(Assembler::DoubleEqualOrUnordered, temp, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }
    }

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorX86Shared::visitGuardShape(LGuardShape *guard)
{
    Register obj = ToRegister(guard->input());
    masm.cmpPtr(Operand(obj, JSObject::offsetOfShape()), ImmGCPtr(guard->mir()->shape()));

    return bailoutIf(Assembler::NotEqual, guard->snapshot());
}

bool
CodeGeneratorX86Shared::visitGuardObjectType(LGuardObjectType *guard)
{
    Register obj = ToRegister(guard->input());
    masm.cmpPtr(Operand(obj, JSObject::offsetOfType()), ImmGCPtr(guard->mir()->typeObject()));

    Assembler::Condition cond =
        guard->mir()->bailOnEquality() ? Assembler::Equal : Assembler::NotEqual;
    return bailoutIf(cond, guard->snapshot());
}

bool
CodeGeneratorX86Shared::visitGuardClass(LGuardClass *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadPtr(Address(obj, JSObject::offsetOfType()), tmp);
    masm.cmpPtr(Operand(tmp, types::TypeObject::offsetOfClasp()), ImmPtr(guard->mir()->getClass()));
    if (!bailoutIf(Assembler::NotEqual, guard->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorX86Shared::visitEffectiveAddress(LEffectiveAddress *ins)
{
    const MEffectiveAddress *mir = ins->mir();
    Register base = ToRegister(ins->base());
    Register index = ToRegister(ins->index());
    Register output = ToRegister(ins->output());
    masm.leal(Operand(base, index, mir->scale(), mir->displacement()), output);
    return true;
}

bool
CodeGeneratorX86Shared::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint
    // patching to occur. Otherwise, we could overwrite the invalidation
    // epilogue.
    for (size_t i = 0; i < sizeof(void *); i += Assembler::NopSize())
        masm.nop();

    masm.bind(&invalidate_);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    JitCode *thunk = gen->jitRuntime()->getInvalidationThunk();

    masm.call(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk should
    // pop the invalidated JS frame and return directly to its caller.
    masm.assumeUnreachable("Should have returned directly to its caller instead of here.");
    return true;
}

bool
CodeGeneratorX86Shared::visitNegI(LNegI *ins)
{
    Register input = ToRegister(ins->input());
    MOZ_ASSERT(input == ToRegister(ins->output()));

    masm.neg32(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitNegD(LNegD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));

    masm.negateDouble(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitNegF(LNegF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));

    masm.negateFloat(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitInt32x4(LInt32x4 *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantInt32x4(ins->getValue(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitFloat32x4(LFloat32x4 *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantFloat32x4(ins->getValue(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitInt32x4ToFloat32x4(LInt32x4ToFloat32x4 *ins)
{
    FloatRegister in = ToFloatRegister(ins->input());
    FloatRegister out = ToFloatRegister(ins->output());
    masm.convertInt32x4ToFloat32x4(in, out);
    return true;
}

bool
CodeGeneratorX86Shared::visitFloat32x4ToInt32x4(LFloat32x4ToInt32x4 *ins)
{
    FloatRegister in = ToFloatRegister(ins->input());
    FloatRegister out = ToFloatRegister(ins->output());
    masm.convertFloat32x4ToInt32x4(in, out);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdValueInt32x4(LSimdValueInt32x4 *ins)
{
    MSimdValueX4 *mir = ins->mir();
    MOZ_ASSERT(mir->type() == MIRType_Int32x4);

    FloatRegister output = ToFloatRegister(ins->output());
    if (AssemblerX86Shared::HasSSE41()) {
        masm.movd(ToRegister(ins->getOperand(0)), output);
        for (size_t i = 1; i < 4; ++i) {
            Register r = ToRegister(ins->getOperand(i));
            masm.pinsrd(i, r, output);
        }
        return true;
    }

    masm.reserveStack(Simd128DataSize);
    for (size_t i = 0; i < 4; ++i) {
        Register r = ToRegister(ins->getOperand(i));
        masm.store32(r, Address(StackPointer, i * sizeof(int32_t)));
    }
    masm.loadAlignedInt32x4(Address(StackPointer, 0), output);
    masm.freeStack(Simd128DataSize);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdValueFloat32x4(LSimdValueFloat32x4 *ins)
{
    MSimdValueX4 *mir = ins->mir();
    MOZ_ASSERT(mir->type() == MIRType_Float32x4);

    FloatRegister output = ToFloatRegister(ins->output());
    FloatRegister r0 = ToFloatRegister(ins->getOperand(0));
    MOZ_ASSERT(r0 == output); // defineReuseInput(0)

    FloatRegister r1 = ToFloatRegister(ins->getTemp(0));
    FloatRegister r2 = ToFloatRegister(ins->getOperand(2));
    FloatRegister r3 = ToFloatRegister(ins->getOperand(3));

    masm.unpcklps(r3, r1);
    masm.unpcklps(r2, r0);
    masm.unpcklps(r1, r0);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdSplatX4(LSimdSplatX4 *ins)
{
    FloatRegister output = ToFloatRegister(ins->output());

    MSimdSplatX4 *mir = ins->mir();
    MOZ_ASSERT(IsSimdType(mir->type()));
    JS_STATIC_ASSERT(sizeof(float) == sizeof(int32_t));

    switch (mir->type()) {
      case MIRType_Int32x4: {
        Register r = ToRegister(ins->getOperand(0));
        masm.movd(r, output);
        masm.pshufd(0, output, output);
        break;
      }
      case MIRType_Float32x4: {
        FloatRegister r = ToFloatRegister(ins->getOperand(0));
        MOZ_ASSERT(r == output);
        masm.shufps(0, r, output);
        break;
      }
      default:
        MOZ_CRASH("Unknown SIMD kind");
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitSimdExtractElementI(LSimdExtractElementI *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    SimdLane lane = ins->lane();
    if (lane == LaneX) {
        // The value we want to extract is in the low double-word
        masm.moveLowInt32(input, output);
    } else {
        uint32_t mask = MacroAssembler::ComputeShuffleMask(lane);
        masm.shuffleInt32(mask, input, ScratchSimdReg);
        masm.moveLowInt32(ScratchSimdReg, output);
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdExtractElementF(LSimdExtractElementF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());

    SimdLane lane = ins->lane();
    if (lane == LaneX) {
        // The value we want to extract is in the low double-word
        if (input != output)
            masm.moveFloat32(input, output);
    } else if (lane == LaneZ) {
        masm.moveHighPairToLowPairFloat32(input, output);
    } else {
        uint32_t mask = MacroAssembler::ComputeShuffleMask(lane);
        masm.shuffleFloat32(mask, input, output);
    }
    masm.canonicalizeFloat(output);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdInsertElementI(LSimdInsertElementI *ins)
{
    FloatRegister vector = ToFloatRegister(ins->vector());
    Register value = ToRegister(ins->value());
    FloatRegister output = ToFloatRegister(ins->output());
    MOZ_ASSERT(vector == output); // defineReuseInput(0)

    unsigned component = unsigned(ins->lane());

    // Note that, contrarily to float32x4, we cannot use movd if the inserted
    // value goes into the first component, as movd clears out the higher lanes
    // of the output.
    if (AssemblerX86Shared::HasSSE41()) {
        masm.pinsrd(component, value, output);
        return true;
    }

    masm.reserveStack(Simd128DataSize);
    masm.storeAlignedInt32x4(vector, Address(StackPointer, 0));
    masm.store32(value, Address(StackPointer, component * sizeof(int32_t)));
    masm.loadAlignedInt32x4(Address(StackPointer, 0), output);
    masm.freeStack(Simd128DataSize);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdInsertElementF(LSimdInsertElementF *ins)
{
    FloatRegister vector = ToFloatRegister(ins->vector());
    FloatRegister value = ToFloatRegister(ins->value());
    FloatRegister output = ToFloatRegister(ins->output());
    MOZ_ASSERT(vector == output); // defineReuseInput(0)

    if (ins->lane() == SimdLane::LaneX) {
        // As both operands are registers, movss doesn't modify the upper bits
        // of the destination operand.
        if (value != output)
            masm.movss(value, output);
        return true;
    }

    if (AssemblerX86Shared::HasSSE41()) {
        // The input value is in the low float32 of the 'value' FloatRegister.
        masm.insertps(value, output, masm.insertpsMask(SimdLane::LaneX, ins->lane()));
        return true;
    }

    unsigned component = unsigned(ins->lane());
    masm.reserveStack(Simd128DataSize);
    masm.storeAlignedFloat32x4(vector, Address(StackPointer, 0));
    masm.storeFloat32(value, Address(StackPointer, component * sizeof(int32_t)));
    masm.loadAlignedFloat32x4(Address(StackPointer, 0), output);
    masm.freeStack(Simd128DataSize);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdSignMaskX4(LSimdSignMaskX4 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // For Float32x4 and Int32x4.
    masm.movmskps(input, output);
    return true;
}

bool
CodeGeneratorX86Shared::visitSimdBinaryCompIx4(LSimdBinaryCompIx4 *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == lhs);

    MSimdBinaryComp::Operation op = ins->operation();
    switch (op) {
      case MSimdBinaryComp::greaterThan:
        masm.packedGreaterThanInt32x4(rhs, lhs);
        return true;
      case MSimdBinaryComp::equal:
        masm.packedEqualInt32x4(rhs, lhs);
        return true;
      case MSimdBinaryComp::lessThan:
        // scr := rhs
        if (rhs.kind() == Operand::FPREG)
            masm.moveAlignedInt32x4(ToFloatRegister(ins->rhs()), ScratchSimdReg);
        else
            masm.loadAlignedInt32x4(rhs, ScratchSimdReg);

        // scr := scr > lhs (i.e. lhs < rhs)
        // Improve by doing custom lowering (rhs is tied to the output register)
        masm.packedGreaterThanInt32x4(ToOperand(ins->lhs()), ScratchSimdReg);
        masm.moveAlignedInt32x4(ScratchFloat32Reg, lhs);
        return true;
      case MSimdBinaryComp::notEqual:
      case MSimdBinaryComp::greaterThanOrEqual:
      case MSimdBinaryComp::lessThanOrEqual:
        // These operations are not part of the spec. so are not implemented.
        break;
    }
    MOZ_CRASH("unexpected SIMD op");
}

bool
CodeGeneratorX86Shared::visitSimdBinaryCompFx4(LSimdBinaryCompFx4 *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == lhs);

    MSimdBinaryComp::Operation op = ins->operation();
    switch (op) {
      case MSimdBinaryComp::equal:
        masm.cmpps(rhs, lhs, 0x0);
        return true;
      case MSimdBinaryComp::lessThan:
        masm.cmpps(rhs, lhs, 0x1);
        return true;
      case MSimdBinaryComp::lessThanOrEqual:
        masm.cmpps(rhs, lhs, 0x2);
        return true;
      case MSimdBinaryComp::notEqual:
        masm.cmpps(rhs, lhs, 0x4);
        return true;
      case MSimdBinaryComp::greaterThanOrEqual:
        masm.cmpps(rhs, lhs, 0x5);
        return true;
      case MSimdBinaryComp::greaterThan:
        masm.cmpps(rhs, lhs, 0x6);
        return true;
    }
    MOZ_CRASH("unexpected SIMD op");
}

bool
CodeGeneratorX86Shared::visitSimdBinaryArithIx4(LSimdBinaryArithIx4 *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == lhs);

    MSimdBinaryArith::Operation op = ins->operation();
    switch (op) {
      case MSimdBinaryArith::Add:
        masm.packedAddInt32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Sub:
        masm.packedSubInt32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Mul:
        // we can do mul with a single instruction only if we have SSE4.1
        // using the PMULLD instruction.
      case MSimdBinaryArith::Div:
        // x86 doesn't have SIMD i32 div.
        break;
      case MSimdBinaryArith::Max:
        // we can do max with a single instruction only if we have SSE4.1
        // using the PMAXSD instruction.
        break;
      case MSimdBinaryArith::Min:
        // we can do max with a single instruction only if we have SSE4.1
        // using the PMINSD instruction.
        break;
    }
    MOZ_CRASH("unexpected SIMD op");
}

bool
CodeGeneratorX86Shared::visitSimdBinaryArithFx4(LSimdBinaryArithFx4 *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == lhs);

    MSimdBinaryArith::Operation op = ins->operation();
    switch (op) {
      case MSimdBinaryArith::Add:
        masm.packedAddFloat32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Sub:
        masm.packedSubFloat32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Mul:
        masm.packedMulFloat32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Div:
        masm.packedDivFloat32(rhs, lhs);
        return true;
      case MSimdBinaryArith::Max:
        // TODO: standardization of float32x4.min/max needs to define the
        // semantics for particular values: if both input operands are -0 or 0,
        // or one operand is NaN, the return value will be the "second operand"
        // in the instruction.
        // See also bug 1068028, which is about fixing semantics once the
        // specification is stable.
        masm.maxps(rhs, lhs);
        return true;
      case MSimdBinaryArith::Min:
        // See comment above.
        masm.minps(rhs, lhs);
        return true;
    }
    MOZ_CRASH("unexpected SIMD op");
}

bool
CodeGeneratorX86Shared::visitSimdBinaryBitwiseX4(LSimdBinaryBitwiseX4 *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    Operand rhs = ToOperand(ins->rhs());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == lhs);

    MSimdBinaryBitwise::Operation op = ins->operation();
    switch (op) {
      case MSimdBinaryBitwise::and_:
        masm.bitwiseAndX4(rhs, lhs);
        return true;
      case MSimdBinaryBitwise::or_:
        masm.bitwiseOrX4(rhs, lhs);
        return true;
      case MSimdBinaryBitwise::xor_:
        masm.bitwiseXorX4(rhs, lhs);
        return true;
    }
    MOZ_CRASH("unexpected SIMD bitwise op");
}

bool
CodeGeneratorX86Shared::visitSimdShift(LSimdShift *ins)
{
    FloatRegister vec = ToFloatRegister(ins->vector());
    FloatRegister out = ToFloatRegister(ins->output());
    MOZ_ASSERT(vec == out); // defineReuseInput(0);

    // TODO: If the shift count is greater than 31, this will just zero all
    // lanes by default for lsh and ursh, and set the count to 32 for rsh
    // (which will just extend the sign bit to all bits). Plain JS doesn't do
    // this: instead it only keeps the five low bits of the mask. Spec isn't
    // clear about that topic so this might need to be fixed. See also bug
    // 1068028.
    const LAllocation *val = ins->value();
    if (val->isConstant()) {
        Imm32 count(ToInt32(val));
        switch (ins->operation()) {
          case MSimdShift::lsh:
            masm.packedLeftShiftByScalar(count, out);
            return true;
          case MSimdShift::rsh:
            masm.packedRightShiftByScalar(count, out);
            return true;
          case MSimdShift::ursh:
            masm.packedUnsignedRightShiftByScalar(count, out);
            return true;
        }
        MOZ_CRASH("unexpected SIMD bitwise op");
    }

    MOZ_ASSERT(val->isRegister());
    FloatRegister tmp = ScratchFloat32Reg;
    masm.movd(ToRegister(val), tmp);

    switch (ins->operation()) {
      case MSimdShift::lsh:
        masm.packedLeftShiftByScalar(tmp, out);
        return true;
      case MSimdShift::rsh:
        masm.packedRightShiftByScalar(tmp, out);
        return true;
      case MSimdShift::ursh:
        masm.packedUnsignedRightShiftByScalar(tmp, out);
        return true;
    }
    MOZ_CRASH("unexpected SIMD bitwise op");
}

bool
CodeGeneratorX86Shared::visitSimdSelect(LSimdSelect *ins)
{
    FloatRegister mask = ToFloatRegister(ins->mask());
    FloatRegister onTrue = ToFloatRegister(ins->lhs());
    FloatRegister onFalse = ToFloatRegister(ins->rhs());

    MOZ_ASSERT(onTrue == ToFloatRegister(ins->output()));
    // The onFalse argument is not destroyed but due to limitations of the
    // register allocator its life ends at the start of the operation.
    masm.bitwiseAndX4(Operand(mask), onTrue);
    masm.bitwiseAndNotX4(Operand(onFalse), mask);
    masm.bitwiseOrX4(Operand(mask), onTrue);
    return true;
}

bool
CodeGeneratorX86Shared::visitForkJoinGetSlice(LForkJoinGetSlice *ins)
{
    MOZ_ASSERT(gen->info().executionMode() == ParallelExecution);
    MOZ_ASSERT(ToRegister(ins->forkJoinContext()) == ForkJoinGetSliceReg_cx);
    MOZ_ASSERT(ToRegister(ins->temp1()) == eax);
    MOZ_ASSERT(ToRegister(ins->temp2()) == edx);
    MOZ_ASSERT(ToRegister(ins->temp3()) == ForkJoinGetSliceReg_temp0);
    MOZ_ASSERT(ToRegister(ins->temp4()) == ForkJoinGetSliceReg_temp1);
    MOZ_ASSERT(ToRegister(ins->output()) == ForkJoinGetSliceReg_output);

    masm.call(gen->jitRuntime()->forkJoinGetSliceStub());
    return true;
}

JitCode *
JitRuntime::generateForkJoinGetSliceStub(JSContext *cx)
{
    MacroAssembler masm(cx);

    // We need two fixed temps. We need to fix eax for cmpxchg, and edx for
    // div.
    Register cxReg = ForkJoinGetSliceReg_cx, worker = cxReg;
    Register pool = ForkJoinGetSliceReg_temp0;
    Register bounds = ForkJoinGetSliceReg_temp1;
    Register output = ForkJoinGetSliceReg_output;

    MOZ_ASSERT(worker != eax && worker != edx);
    MOZ_ASSERT(pool != eax && pool != edx);
    MOZ_ASSERT(bounds != eax && bounds != edx);
    MOZ_ASSERT(output != eax && output != edx);

    Label stealWork, noMoreWork, gotSlice;
    Operand workerSliceBounds(Address(worker, ThreadPoolWorker::offsetOfSliceBounds()));

    // Clobber cx to load the worker.
    masm.push(cxReg);
    masm.loadPtr(Address(cxReg, ForkJoinContext::offsetOfWorker()), worker);

    // Load the thread pool, which is used in all cases below.
    masm.loadThreadPool(pool);

    {
        // Try to get a slice from the current thread.
        Label getOwnSliceLoopHead;
        masm.bind(&getOwnSliceLoopHead);

        // Load the slice bounds for the current thread.
        masm.loadSliceBounds(worker, bounds);

        // The slice bounds is a uint32 composed from two uint16s:
        // [ from          , to           ]
        //   ^~~~            ^~
        //   upper 16 bits | lower 16 bits
        masm.move32(bounds, output);
        masm.shrl(Imm32(16), output);

        // If we don't have any slices left ourselves, move on to stealing.
        masm.branch16(Assembler::Equal, output, bounds, &stealWork);

        // If we still have work, try to CAS [ from+1, to ].
        masm.move32(bounds, edx);
        masm.add32(Imm32(0x10000), edx);
        masm.move32(bounds, eax);
        masm.atomic_cmpxchg32(edx, workerSliceBounds, eax);
        masm.j(Assembler::NonZero, &getOwnSliceLoopHead);

        // If the CAS succeeded, return |from| in output.
        masm.jump(&gotSlice);
    }

    // Try to steal work.
    masm.bind(&stealWork);

    // It's not technically correct to test whether work-stealing is turned on
    // only during stub-generation time, but it's a DEBUG only thing.
    //
    // If stealing is off, stealWork falls through to noMoreWork.
    if (cx->runtime()->threadPool.workStealing()) {
        Label stealWorkLoopHead;
        masm.bind(&stealWorkLoopHead);

        // Check if we have work.
        masm.branch32(Assembler::Equal,
                      Address(pool, ThreadPool::offsetOfPendingSlices()),
                      Imm32(0), &noMoreWork);

        // Get an id at random. The following is an inline of
        // the 32-bit xorshift in ThreadPoolWorker::randomWorker().
        {
            // Reload the current worker.
            masm.loadPtr(Address(StackPointer, 0), cxReg);
            masm.loadPtr(Address(cxReg, ForkJoinContext::offsetOfWorker()), worker);

            // Perform the xorshift to get a random number in eax, using edx
            // as a temp.
            Address rngState(worker, ThreadPoolWorker::offsetOfSchedulerRNGState());
            masm.load32(rngState, eax);
            masm.move32(eax, edx);
            masm.shll(Imm32(ThreadPoolWorker::XORSHIFT_A), eax);
            masm.xor32(edx, eax);
            masm.move32(eax, edx);
            masm.shrl(Imm32(ThreadPoolWorker::XORSHIFT_B), eax);
            masm.xor32(edx, eax);
            masm.move32(eax, edx);
            masm.shll(Imm32(ThreadPoolWorker::XORSHIFT_C), eax);
            masm.xor32(edx, eax);
            masm.store32(eax, rngState);

            // Compute the random worker id by computing % numWorkers. Reuse
            // output as a temp.
            masm.move32(Imm32(0), edx);
            masm.move32(Imm32(cx->runtime()->threadPool.numWorkers()), output);
            masm.udiv(output);
        }

        // Load the worker from the workers array.
        masm.loadPtr(Address(pool, ThreadPool::offsetOfWorkers()), worker);
        masm.loadPtr(BaseIndex(worker, edx, ScalePointer), worker);

        // Try to get a slice from the designated victim worker.
        Label stealSliceFromWorkerLoopHead;
        masm.bind(&stealSliceFromWorkerLoopHead);

        // Load the slice bounds and decompose for the victim worker.
        masm.loadSliceBounds(worker, bounds);
        masm.move32(bounds, eax);
        masm.shrl(Imm32(16), eax);

        // If the victim worker has no more slices left, find another worker.
        masm.branch16(Assembler::Equal, eax, bounds, &stealWorkLoopHead);

        // If the victim worker still has work, try to CAS [ from, to-1 ].
        masm.move32(bounds, output);
        masm.sub32(Imm32(1), output);
        masm.move32(bounds, eax);
        masm.atomic_cmpxchg32(output, workerSliceBounds, eax);
        masm.j(Assembler::NonZero, &stealSliceFromWorkerLoopHead);

        // If the CAS succeeded, return |to-1| in output.
#ifdef DEBUG
        masm.atomic_inc32(Operand(Address(pool, ThreadPool::offsetOfStolenSlices())));
#endif
        // Copies lower 16 bits only.
        masm.movzwl(output, output);
    } else {
        masm.jump(&noMoreWork);
    }

    // If we successfully got a slice, decrement pool->pendingSlices_ and
    // return the slice.
    masm.bind(&gotSlice);
    masm.atomic_dec32(Operand(Address(pool, ThreadPool::offsetOfPendingSlices())));
    masm.pop(cxReg);
    masm.ret();

    // There's no more slices to give out, return a sentinel value.
    masm.bind(&noMoreWork);
    masm.move32(Imm32(ThreadPool::MAX_SLICE_ID), output);
    masm.pop(cxReg);
    masm.ret();

    Linker linker(masm);
    JitCode *code = linker.newCode<NoGC>(cx, OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(code, "ForkJoinGetSliceStub");
#endif

    return code;
}

} // namespace jit
} // namespace js
