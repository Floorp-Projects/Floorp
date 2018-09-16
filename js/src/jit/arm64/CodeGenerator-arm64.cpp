/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/CodeGenerator-arm64.h"

#include "mozilla/MathAlgorithms.h"

#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;
using mozilla::NegativeInfinity;
using JS::GenericNaN;

// shared
CodeGeneratorARM64::CodeGeneratorARM64(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

bool
CodeGeneratorARM64::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode()) {
        return false;
    }

    if (deoptLabel_.used()) {
        // All non-table-based bailouts will go here.
        masm.bind(&deoptLabel_);

        // Store the frame size, so the handler can recover the IonScript.
        masm.Mov(x30, frameSize());

        TrampolinePtr handler = gen->jitRuntime()->getGenericBailoutHandler();
        masm.jump(handler);
    }

    return !masm.oom();
}

void
CodeGeneratorARM64::emitBranch(Assembler::Condition cond, MBasicBlock* mirTrue, MBasicBlock* mirFalse)
{
    if (isNextBlock(mirFalse->lir())) {
        jumpToBlock(mirTrue, cond);
    } else {
        jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
        jumpToBlock(mirTrue);
    }
}

void
OutOfLineBailout::accept(CodeGeneratorARM64* codegen)
{
    codegen->visitOutOfLineBailout(this);
}

void
CodeGenerator::visitTestIAndBranch(LTestIAndBranch* test)
{
    Register input = ToRegister(test->input());
    MBasicBlock* mirTrue = test->ifTrue();
    MBasicBlock* mirFalse = test->ifFalse();

    masm.test32(input, input);

    // Jump to the True block if NonZero.
    // Jump to the False block if Zero.
    if (isNextBlock(mirFalse->lir())) {
        jumpToBlock(mirTrue, Assembler::NonZero);
    } else {
        jumpToBlock(mirFalse, Assembler::Zero);
        if (!isNextBlock(mirTrue->lir())) {
            jumpToBlock(mirTrue);
        }
    }
}

void
CodeGenerator::visitCompare(LCompare* comp)
{
    const MCompare* mir = comp->mir();
    const MCompare::CompareType type = mir->compareType();
    const Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
    const Register leftreg = ToRegister(comp->getOperand(0));
    const LAllocation* right = comp->getOperand(1);
    const Register defreg = ToRegister(comp->getDef(0));

    if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
        masm.cmpPtrSet(cond, leftreg, ToRegister(right), defreg);
        return;
    }

    if (right->isConstant()) {
        masm.cmp32Set(cond, leftreg, Imm32(ToInt32(right)), defreg);
    } else {
        masm.cmp32Set(cond, leftreg, ToRegister(right), defreg);
    }
}

void
CodeGenerator::visitCompareAndBranch(LCompareAndBranch* comp)
{
    const MCompare* mir = comp->cmpMir();
    const MCompare::CompareType type = mir->compareType();
    const LAllocation* left = comp->left();
    const LAllocation* right = comp->right();

    if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
        masm.cmpPtr(ToRegister(left), ToRegister(right));
    } else if (right->isConstant()) {
        masm.cmp32(ToRegister(left), Imm32(ToInt32(right)));
    } else {
        masm.cmp32(ToRegister(left), ToRegister(right));
    }

    Assembler::Condition cond = JSOpToCondition(type, comp->jsop());
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void
CodeGeneratorARM64::bailoutIf(Assembler::Condition condition, LSnapshot* snapshot)
{
    encode(snapshot);

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                  frameClass_.frameSize() == masm.framePushed());

    // ARM64 doesn't use a bailout table.
    InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
    OutOfLineBailout* ool = new(alloc()) OutOfLineBailout(snapshot);
    addOutOfLineCode(ool, new(alloc()) BytecodeSite(tree, tree->script()->code()));

    masm.B(ool->entry(), condition);
}

void
CodeGeneratorARM64::bailoutFrom(Label* label, LSnapshot* snapshot)
{
    MOZ_ASSERT(label->used());
    MOZ_ASSERT(!label->bound());

    encode(snapshot);

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                  frameClass_.frameSize() == masm.framePushed());

    // ARM64 doesn't use a bailout table.
    InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
    OutOfLineBailout* ool = new(alloc()) OutOfLineBailout(snapshot);
    addOutOfLineCode(ool, new(alloc()) BytecodeSite(tree, tree->script()->code()));

    masm.retarget(label, ool->entry());
}

void
CodeGeneratorARM64::bailout(LSnapshot* snapshot)
{
    MOZ_CRASH("bailout");
}

void
CodeGeneratorARM64::visitOutOfLineBailout(OutOfLineBailout* ool)
{
    masm.push(Imm32(ool->snapshot()->snapshotOffset()));
    masm.B(&deoptLabel_);
}

void
CodeGenerator::visitMinMaxD(LMinMaxD* ins)
{
    MOZ_CRASH("visitMinMaxD");
}

void
CodeGenerator::visitMinMaxF(LMinMaxF* ins)
{
    MOZ_CRASH("visitMinMaxF");
}

void
CodeGenerator::visitAbsD(LAbsD* ins)
{
    MOZ_CRASH("visitAbsD");
}

void
CodeGenerator::visitAbsF(LAbsF* ins)
{
    MOZ_CRASH("visitAbsF");
}

void
CodeGenerator::visitSqrtD(LSqrtD* ins)
{
    MOZ_CRASH("visitSqrtD");
}

void
CodeGenerator::visitSqrtF(LSqrtF* ins)
{
    MOZ_CRASH("visitSqrtF");
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister
toWRegister(const T* a)
{
    return ARMRegister(ToRegister(a), 32);
}

// FIXME: Uh, is this a static function? It looks like it is...
template <typename T>
ARMRegister
toXRegister(const T* a)
{
    return ARMRegister(ToRegister(a), 64);
}

Operand
toWOperand(const LAllocation* a)
{
    if (a->isConstant()) {
        return Operand(ToInt32(a));
    }
    return Operand(toWRegister(a));
}

vixl::CPURegister
ToCPURegister(const LAllocation* a, Scalar::Type type)
{
    if (a->isFloatReg() && type == Scalar::Float64) {
        return ARMFPRegister(ToFloatRegister(a), 64);
    }
    if (a->isFloatReg() && type == Scalar::Float32) {
        return ARMFPRegister(ToFloatRegister(a), 32);
    }
    if (a->isGeneralReg()) {
        return ARMRegister(ToRegister(a), 32);
    }
    MOZ_CRASH("Unknown LAllocation");
}

vixl::CPURegister
ToCPURegister(const LDefinition* d, Scalar::Type type)
{
    return ToCPURegister(d->output(), type);
}

void
CodeGenerator::visitAddI(LAddI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);

    // Platforms with three-operand arithmetic ops don't need recovery.
    MOZ_ASSERT(!ins->recoversInput());

    if (ins->snapshot()) {
        masm.Adds(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
        bailoutIf(Assembler::Overflow, ins->snapshot());
    } else {
        masm.Add(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    }
}

void
CodeGenerator::visitSubI(LSubI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);

    // Platforms with three-operand arithmetic ops don't need recovery.
    MOZ_ASSERT(!ins->recoversInput());

    if (ins->snapshot()) {
        masm.Subs(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
        bailoutIf(Assembler::Overflow, ins->snapshot());
    } else {
        masm.Sub(toWRegister(dest), toWRegister(lhs), toWOperand(rhs));
    }
}

void
CodeGenerator::visitMulI(LMulI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);
    MMul* mul = ins->mir();
    MOZ_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    Register lhsreg = ToRegister(lhs);

    if (rhs->isConstant()) {
        // Bailout on -0.0.
        int32_t constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            masm.Cmp(toWRegister(lhs), Operand(0));
            bailoutIf(bailoutCond, ins->snapshot());
        }

        switch (constant) {
          case -1:
            masm.neg32(lhsreg);
            break;
          case 0:
            masm.Mov(ARMRegister(lhsreg, 32), wzr);
            return; // escape overflow check;
          case 1:
            // nop
            return; // escape overflow check;
          case 2:
            masm.add32(lhsreg, lhsreg);
            break;
          default:
            // Use shift if cannot overflow and constant is a power of 2
            if (!mul->canOverflow() && constant > 0) {
                int32_t shift = FloorLog2(constant);
                if ((1 << shift) == constant) {
                    masm.lshift32(Imm32(shift), lhsreg);
                    return;
                }
            }

            // Otherwise, just multiply.
            Label bailout;
            Label* onZero = mul->canBeNegativeZero() ? &bailout : nullptr;
            Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

            vixl::UseScratchRegisterScope temps(&masm.asVIXL());
            const Register scratch = temps.AcquireW().asUnsized();

            masm.move32(Imm32(constant), scratch);
            masm.mul32(lhsreg, scratch, ToRegister(dest), onOverflow, onZero);
            if (onZero || onOverflow) {
                bailoutFrom(&bailout, ins->snapshot());
            }
            return; // escape overflow check;
        }

        // Overflow check.
        if (mul->canOverflow()) {
            bailoutIf(Assembler::Overflow, ins->snapshot());
        }
    } else {
        Register rhsreg = ToRegister(rhs);

        Label bailout;
        // TODO: x64 (but not other platforms) have an OOL path for onZero.
        Label* onZero = mul->canBeNegativeZero() ? &bailout : nullptr;
        Label* onOverflow = mul->canOverflow() ? &bailout : nullptr;

        masm.mul32(lhsreg, rhsreg, ToRegister(dest), onOverflow, onZero);
        if (onZero || onOverflow) {
            bailoutFrom(&bailout, ins->snapshot());
        }
    }
}


void
CodeGenerator::visitDivI(LDivI* ins)
{
    MOZ_CRASH("visitDivI");

}

void
CodeGenerator::visitDivPowTwoI(LDivPowTwoI* ins)
{
    MOZ_CRASH("CodeGenerator::visitDivPowTwoI");
}

void
CodeGeneratorARM64::modICommon(MMod* mir, Register lhs, Register rhs, Register output,
                               LSnapshot* snapshot, Label& done)
{
    MOZ_CRASH("CodeGeneratorARM64::modICommon");
}

void
CodeGenerator::visitModI(LModI* ins)
{
    MOZ_CRASH("visitModI");
}

void
CodeGenerator::visitModPowTwoI(LModPowTwoI* ins)
{
    Register lhs = ToRegister(ins->getOperand(0));
    ARMRegister lhsw = toWRegister(ins->getOperand(0));
    ARMRegister outw = toWRegister(ins->output());

    int32_t shift = ins->shift();
    bool canBeNegative = !ins->mir()->isUnsigned() && ins->mir()->canBeNegativeDividend();

    Label negative;
    if (canBeNegative) {
        // Switch based on sign of the lhs.
        // Positive numbers are just a bitmask.
        masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
    }

    masm.And(outw, lhsw, Operand((uint32_t(1) << shift) - 1));

    if (canBeNegative) {
        Label done;
        masm.jump(&done);

        // Negative numbers need a negate, bitmask, negate.
        masm.bind(&negative);
        masm.Neg(outw, Operand(lhsw));
        masm.And(outw, outw, Operand((uint32_t(1) << shift) - 1));

        // Since a%b has the same sign as b, and a is negative in this branch,
        // an answer of 0 means the correct result is actually -0. Bail out.
        if (!ins->mir()->isTruncated()) {
            masm.Negs(outw, Operand(outw));
            bailoutIf(Assembler::Zero, ins->snapshot());
        } else {
            masm.Neg(outw, Operand(outw));
        }

        masm.bind(&done);
    }
}

void
CodeGenerator::visitModMaskI(LModMaskI* ins)
{
    MOZ_CRASH("CodeGenerator::visitModMaskI");
}

void
CodeGenerator::visitBitNotI(LBitNotI* ins)
{
    const LAllocation* input = ins->getOperand(0);
    const LDefinition* output = ins->getDef(0);
    masm.Mvn(toWRegister(output), toWOperand(input));
}

void
CodeGenerator::visitBitOpI(LBitOpI* ins)
{
    const ARMRegister lhs = toWRegister(ins->getOperand(0));
    const Operand rhs = toWOperand(ins->getOperand(1));
    const ARMRegister dest = toWRegister(ins->getDef(0));

    switch (ins->bitop()) {
      case JSOP_BITOR:
        masm.Orr(dest, lhs, rhs);
        break;
      case JSOP_BITXOR:
        masm.Eor(dest, lhs, rhs);
        break;
      case JSOP_BITAND:
        masm.And(dest, lhs, rhs);
        break;
      default:
        MOZ_CRASH("unexpected binary opcode");
    }
}

void
CodeGenerator::visitShiftI(LShiftI* ins)
{
    MOZ_CRASH("visitShiftI");
}

void
CodeGenerator::visitUrshD(LUrshD* ins)
{
    MOZ_CRASH("visitUrshD");
}

void
CodeGenerator::visitPowHalfD(LPowHalfD* ins)
{
    MOZ_CRASH("visitPowHalfD");
}

MoveOperand
CodeGeneratorARM64::toMoveOperand(const LAllocation a) const
{
    if (a.isGeneralReg()) {
        return MoveOperand(ToRegister(a));
    }
    if (a.isFloatReg()) {
        return MoveOperand(ToFloatRegister(a));
    }
    return MoveOperand(AsRegister(masm.getStackPointer()), ToStackOffset(a));
}

class js::jit::OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorARM64>
{
    MTableSwitch* mir_;
    Vector<CodeLabel, 8, JitAllocPolicy> codeLabels_;

    void accept(CodeGeneratorARM64* codegen) override {
        codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(TempAllocator& alloc, MTableSwitch* mir)
      : mir_(mir),
        codeLabels_(alloc)
    { }

    MTableSwitch* mir() const {
        return mir_;
    }

    bool addCodeLabel(CodeLabel label) {
        return codeLabels_.append(label);
    }
    CodeLabel codeLabel(unsigned i) {
        return codeLabels_[i];
    }
};

void
CodeGeneratorARM64::visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool)
{
    MOZ_CRASH("visitOutOfLineTableSwitch");
}

void
CodeGeneratorARM64::emitTableSwitchDispatch(MTableSwitch* mir, Register index_, Register base_)
{
    MOZ_CRASH("emitTableSwitchDispatch");
}

void
CodeGenerator::visitMathD(LMathD* math)
{
    MOZ_CRASH("visitMathD");
}

void
CodeGenerator::visitMathF(LMathF* math)
{
    MOZ_CRASH("visitMathF");
}

void
CodeGenerator::visitFloor(LFloor* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());

    Label bailout;
    masm.floor(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());
}

void
CodeGenerator::visitFloorF(LFloorF* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());

    Label bailout;
    masm.floorf(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());
}

void
CodeGenerator::visitCeil(LCeil* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());

    Label bailout;
    masm.ceil(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());
}

void
CodeGenerator::visitCeilF(LCeilF* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());

    Label bailout;
    masm.ceilf(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());
}

void
CodeGenerator::visitRound(LRound* lir)
{
    MOZ_CRASH("visitRound");
}

void
CodeGenerator::visitRoundF(LRoundF* lir)
{
    MOZ_CRASH("visitRoundF");
}

void
CodeGenerator::visitTrunc(LTrunc* lir)
{
    MOZ_CRASH("visitTrunc");
}

void
CodeGenerator::visitTruncF(LTruncF* lir)
{
    MOZ_CRASH("visitTruncF");
}

void
CodeGenerator::visitClzI(LClzI* lir)
{
    ARMRegister input = toWRegister(lir->input());
    ARMRegister output = toWRegister(lir->output());
    masm.Clz(output, input);
}

void
CodeGenerator::visitCtzI(LCtzI* lir)
{
    MOZ_CRASH("visitCtzI");
}

void
CodeGeneratorARM64::emitRoundDouble(FloatRegister src, Register dest, Label* fail)
{
    MOZ_CRASH("CodeGeneratorARM64::emitRoundDouble");
}

void
CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32* ins)
{
    MOZ_CRASH("visitTruncateDToInt32");
}

void
CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32* ins)
{
    MOZ_CRASH("visitTruncateFToInt32");
}

static const uint32_t FrameSizes[] = { 128, 256, 512, 1024 };

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(0);
}

uint32_t
FrameSizeClass::frameSize() const
{
    MOZ_CRASH("arm64 does not use frame size classes");
}

ValueOperand
CodeGeneratorARM64::ToValue(LInstruction* ins, size_t pos)
{
    return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand
CodeGeneratorARM64::ToTempValue(LInstruction* ins, size_t pos)
{
    MOZ_CRASH("CodeGeneratorARM64::ToTempValue");
}

void
CodeGenerator::visitValue(LValue* value)
{
    ValueOperand result = ToOutValue(value);
    masm.moveValue(value->value(), result);
}

void
CodeGenerator::visitBox(LBox* box)
{
    const LAllocation* in = box->getOperand(0);
    ValueOperand result = ToOutValue(box);

    masm.moveValue(TypedOrValueRegister(box->type(), ToAnyRegister(in)), result);
}

void
CodeGenerator::visitUnbox(LUnbox* unbox)
{
    MUnbox* mir = unbox->mir();

    if (mir->fallible()) {
        const ValueOperand value = ToValue(unbox, LUnbox::Input);
        Assembler::Condition cond;
        switch (mir->type()) {
          case MIRType::Int32:
            cond = masm.testInt32(Assembler::NotEqual, value);
            break;
          case MIRType::Boolean:
            cond = masm.testBoolean(Assembler::NotEqual, value);
            break;
          case MIRType::Object:
            cond = masm.testObject(Assembler::NotEqual, value);
            break;
          case MIRType::String:
            cond = masm.testString(Assembler::NotEqual, value);
            break;
          case MIRType::Symbol:
            cond = masm.testSymbol(Assembler::NotEqual, value);
            break;
          default:
            MOZ_CRASH("Given MIRType cannot be unboxed.");
        }
        bailoutIf(cond, unbox->snapshot());
    } else {
#ifdef DEBUG
        JSValueTag tag = MIRTypeToTag(mir->type());
        Label ok;

        ValueOperand input = ToValue(unbox, LUnbox::Input);
        ScratchTagScope scratch(masm, input);
        masm.splitTagForTest(input, scratch);
        masm.branchTest32(Assembler::Condition::Equal, scratch, Imm32(tag), &ok);
        masm.assumeUnreachable("Infallible unbox type mismatch");
        masm.bind(&ok);
#endif
    }

    ValueOperand input = ToValue(unbox, LUnbox::Input);
    Register result = ToRegister(unbox->output());
    switch (mir->type()) {
      case MIRType::Int32:
        masm.unboxInt32(input, result);
        break;
      case MIRType::Boolean:
        masm.unboxBoolean(input, result);
        break;
      case MIRType::Object:
        masm.unboxObject(input, result);
        break;
      case MIRType::String:
        masm.unboxString(input, result);
        break;
      case MIRType::Symbol:
        masm.unboxSymbol(input, result);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
}

void
CodeGenerator::visitDouble(LDouble* ins)
{
    ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 64);
    masm.Fmov(output, ins->getDouble());
}

void
CodeGenerator::visitFloat32(LFloat32* ins)
{
    ARMFPRegister output(ToFloatRegister(ins->getDef(0)), 32);
    masm.Fmov(output, ins->getFloat());
}

void
CodeGeneratorARM64::splitTagForTest(const ValueOperand& value, ScratchTagScope& tag)
{
    MOZ_CRASH("splitTagForTest");
}

void
CodeGenerator::visitTestDAndBranch(LTestDAndBranch* test)
{
    MOZ_CRASH("visitTestDAndBranch");
}

void
CodeGenerator::visitTestFAndBranch(LTestFAndBranch* test)
{
    MOZ_CRASH("visitTestFAndBranch");
}

void
CodeGenerator::visitCompareD(LCompareD* comp)
{
    const FloatRegister left = ToFloatRegister(comp->left());
    const FloatRegister right = ToFloatRegister(comp->right());
    ARMRegister output = toWRegister(comp->output());
    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    masm.compareDouble(cond, left, right);
    masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void
CodeGenerator::visitCompareF(LCompareF* comp)
{
    const FloatRegister left = ToFloatRegister(comp->left());
    const FloatRegister right = ToFloatRegister(comp->right());
    ARMRegister output = toWRegister(comp->output());
    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    masm.compareFloat(cond, left, right);
    masm.cset(output, Assembler::ConditionFromDoubleCondition(cond));
}

void
CodeGenerator::visitCompareDAndBranch(LCompareDAndBranch* comp)
{
    const FloatRegister left = ToFloatRegister(comp->left());
    const FloatRegister right = ToFloatRegister(comp->right());
    Assembler::DoubleCondition doubleCond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    Assembler::Condition cond = Assembler::ConditionFromDoubleCondition(doubleCond);

    masm.compareDouble(doubleCond, left, right);
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void
CodeGenerator::visitCompareFAndBranch(LCompareFAndBranch* comp)
{
    const FloatRegister left = ToFloatRegister(comp->left());
    const FloatRegister right = ToFloatRegister(comp->right());
    Assembler::DoubleCondition doubleCond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    Assembler::Condition cond = Assembler::ConditionFromDoubleCondition(doubleCond);

    masm.compareFloat(doubleCond, left, right);
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void
CodeGenerator::visitCompareB(LCompareB* lir)
{
    MOZ_CRASH("visitCompareB");
}

void
CodeGenerator::visitCompareBAndBranch(LCompareBAndBranch* lir)
{
    MOZ_CRASH("visitCompareBAndBranch");
}

void
CodeGenerator::visitCompareBitwise(LCompareBitwise* lir)
{
    MOZ_CRASH("visitCompareBitwise");
}

void
CodeGenerator::visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir)
{
    MOZ_CRASH("visitCompareBitwiseAndBranch");
}

void
CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch* baab)
{
    MOZ_CRASH("visitBitAndAndBranch");
}

void
CodeGenerator::visitWasmUint32ToDouble(LWasmUint32ToDouble* lir)
{
    MOZ_CRASH("visitWasmUint32ToDouble");
}

void
CodeGenerator::visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir)
{
    MOZ_CRASH("visitWasmUint32ToFloat32");
}

void
CodeGenerator::visitNotI(LNotI* ins)
{
    MOZ_CRASH("visitNotI");
}

//        NZCV
// NAN -> 0011
// ==  -> 0110
// <   -> 1000
// >   -> 0010
void
CodeGenerator::visitNotD(LNotD* ins)
{
    MOZ_CRASH("visitNotD");
}

void
CodeGenerator::visitNotF(LNotF* ins)
{
    MOZ_CRASH("visitNotF");
}

void
CodeGeneratorARM64::storeElementTyped(const LAllocation* value, MIRType valueType,
                                      MIRType elementType, Register elements,
                                      const LAllocation* index)
{
    MOZ_CRASH("CodeGeneratorARM64::storeElementTyped");
}

void
CodeGeneratorARM64::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint patching
    // to occur. Otherwise, we could overwrite the invalidation epilogue.
    for (size_t i = 0; i < sizeof(void*); i += Assembler::NopSize()) {
        masm.nop();
    }

    masm.bind(&invalidate_);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));

    TrampolinePtr thunk = gen->jitRuntime()->getInvalidationThunk();
    masm.call(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk
    // should pop the invalidated JS frame and return directly to its caller.
    masm.assumeUnreachable("Should have returned directly to its caller instead of here.");
}

template <class U>
Register
getBase(U* mir)
{
    switch (mir->base()) {
      case U::Heap: return HeapReg;
    }
    return InvalidReg;
}

void
CodeGenerator::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins)
{
    MOZ_CRASH("visitAsmJSLoadHeap");
}

void
CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    MOZ_CRASH("visitAsmJSStoreHeap");
}

void
CodeGenerator::visitWasmCompareExchangeHeap(LWasmCompareExchangeHeap* ins)
{
    MOZ_CRASH("visitWasmCompareExchangeHeap");
}

void
CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins)
{
    MOZ_CRASH("visitWasmAtomicBinopHeap");
}

void
CodeGenerator::visitWasmStackArg(LWasmStackArg* ins)
{
    MOZ_CRASH("visitWasmStackArg");
}

void
CodeGenerator::visitUDiv(LUDiv* ins)
{
    MOZ_CRASH("visitUDiv");
}

void
CodeGenerator::visitUMod(LUMod* ins)
{
    MOZ_CRASH("visitUMod");
}

void
CodeGenerator::visitEffectiveAddress(LEffectiveAddress* ins)
{
    MOZ_CRASH("visitEffectiveAddress");
}

void
CodeGenerator::visitNegI(LNegI* ins)
{
    MOZ_CRASH("visitNegI");
}

void
CodeGenerator::visitNegD(LNegD* ins)
{
    MOZ_CRASH("visitNegD");
}

void
CodeGenerator::visitNegF(LNegF* ins)
{
    MOZ_CRASH("visitNegF");
}

void
CodeGenerator::visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir)
{
    Register elements = ToRegister(lir->elements());
    AnyRegister output = ToAnyRegister(lir->output());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

    Register oldval = ToRegister(lir->oldval());
    Register newval = ToRegister(lir->newval());

    Scalar::Type arrayType = lir->mir()->arrayType();
    size_t width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * width);
        masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval, newval, temp, output);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval, newval, temp, output);
    }
}

void
CodeGenerator::visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir)
{
    Register elements = ToRegister(lir->elements());
    AnyRegister output = ToAnyRegister(lir->output());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

    Register value = ToRegister(lir->value());

    Scalar::Type arrayType = lir->mir()->arrayType();
    size_t width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * width);
        masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp, output);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp, output);
    }
}

void
CodeGenerator::visitAddI64(LAddI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitClzI64(LClzI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitCtzI64(LCtzI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitMulI64(LMulI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitNotI64(LNotI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitSubI64(LSubI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitPopcntI(LPopcntI*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitBitOpI64(LBitOpI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitShiftI64(LShiftI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitSoftDivI(LSoftDivI*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitSoftModI(LSoftModI*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmLoad(LWasmLoad*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitCopySignD(LCopySignD*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitCopySignF(LCopySignF*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitNearbyInt(LNearbyInt*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitPopcntI64(LPopcntI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitRotateI64(LRotateI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmStore(LWasmStore*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitCompareI64(LCompareI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitNearbyIntF(LNearbyIntF*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmSelect(LWasmSelect*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmLoadI64(LWasmLoadI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmStoreI64(LWasmStoreI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitMemoryBarrier(LMemoryBarrier*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitSoftUDivOrMod(LSoftUDivOrMod*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmAddOffset(LWasmAddOffset*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmSelectI64(LWasmSelectI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitSignExtendInt64(LSignExtendInt64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmReinterpret(LWasmReinterpret*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitWasmAtomicBinopHeapForEffect(LWasmAtomicBinopHeapForEffect*)
{
    MOZ_CRASH("NYI");
}

void
CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect*)
{
    MOZ_CRASH("NYI");
}
