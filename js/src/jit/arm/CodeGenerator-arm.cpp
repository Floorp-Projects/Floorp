/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm/CodeGenerator-arm.h"

#include "mozilla/MathAlgorithms.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"

#include "jit/CodeGenerator.h"
#include "jit/JitCompartment.h"
#include "jit/JitFrames.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "js/Conversions.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jsscriptinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;
using mozilla::NegativeInfinity;
using JS::GenericNaN;
using JS::ToInt32;

// shared
CodeGeneratorARM::CodeGeneratorARM(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

Register64
CodeGeneratorARM::ToOperandOrRegister64(const LInt64Allocation input)
{
    return ToRegister64(input);
}

void
CodeGeneratorARM::emitBranch(Assembler::Condition cond, MBasicBlock* mirTrue, MBasicBlock* mirFalse)
{
    if (isNextBlock(mirFalse->lir())) {
        jumpToBlock(mirTrue, cond);
    } else {
        jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
        jumpToBlock(mirTrue);
    }
}

void
OutOfLineBailout::accept(CodeGeneratorARM* codegen)
{
    codegen->visitOutOfLineBailout(this);
}

void
CodeGeneratorARM::visitTestIAndBranch(LTestIAndBranch* test)
{
    const LAllocation* opd = test->getOperand(0);
    MBasicBlock* ifTrue = test->ifTrue();
    MBasicBlock* ifFalse = test->ifFalse();

    // Test the operand
    masm.ma_cmp(ToRegister(opd), Imm32(0));

    if (isNextBlock(ifFalse->lir())) {
        jumpToBlock(ifTrue, Assembler::NonZero);
    } else if (isNextBlock(ifTrue->lir())) {
        jumpToBlock(ifFalse, Assembler::Zero);
    } else {
        jumpToBlock(ifFalse, Assembler::Zero);
        jumpToBlock(ifTrue);
    }
}

void
CodeGeneratorARM::visitCompare(LCompare* comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->mir()->compareType(), comp->jsop());
    const LAllocation* left = comp->getOperand(0);
    const LAllocation* right = comp->getOperand(1);
    const LDefinition* def = comp->getDef(0);

    if (right->isConstant())
        masm.ma_cmp(ToRegister(left), Imm32(ToInt32(right)));
    else if (right->isRegister())
        masm.ma_cmp(ToRegister(left), ToRegister(right));
    else
        masm.ma_cmp(ToRegister(left), Operand(ToAddress(right)));
    masm.ma_mov(Imm32(0), ToRegister(def));
    masm.ma_mov(Imm32(1), ToRegister(def), cond);
}

void
CodeGeneratorARM::visitCompareAndBranch(LCompareAndBranch* comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->cmpMir()->compareType(), comp->jsop());
    const LAllocation* left = comp->left();
    const LAllocation* right = comp->right();

    if (right->isConstant())
        masm.ma_cmp(ToRegister(left), Imm32(ToInt32(right)));
    else if (right->isRegister())
        masm.ma_cmp(ToRegister(left), ToRegister(right));
    else
        masm.ma_cmp(ToRegister(left), Operand(ToAddress(right)));
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

bool
CodeGeneratorARM::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_.used()) {
        // All non-table-based bailouts will go here.
        masm.bind(&deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        masm.ma_mov(Imm32(frameSize()), lr);

        JitCode* handler = gen->jitRuntime()->getGenericBailoutHandler();
        masm.branch(handler);
    }

    return !masm.oom();
}

void
CodeGeneratorARM::bailoutIf(Assembler::Condition condition, LSnapshot* snapshot)
{
    encode(snapshot);

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                  frameClass_.frameSize() == masm.framePushed());

    if (assignBailoutId(snapshot)) {
        uint8_t* bailoutTable = Assembler::BailoutTableStart(deoptTable_->raw());
        uint8_t* code = bailoutTable + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE;
        masm.ma_b(code, condition);
        return;
    }

    // We could not use a jump table, either because all bailout IDs were
    // reserved, or a jump table is not optimal for this frame size or
    // platform. Whatever, we will generate a lazy bailout.
    InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
    OutOfLineBailout* ool = new(alloc()) OutOfLineBailout(snapshot, masm.framePushed());

    // All bailout code is associated with the bytecodeSite of the block we are
    // bailing out from.
    addOutOfLineCode(ool, new(alloc()) BytecodeSite(tree, tree->script()->code()));

    masm.ma_b(ool->entry(), condition);
}

void
CodeGeneratorARM::bailoutFrom(Label* label, LSnapshot* snapshot)
{
    if (masm.bailed())
        return;

    MOZ_ASSERT_IF(!masm.oom(), label->used());
    MOZ_ASSERT_IF(!masm.oom(), !label->bound());

    encode(snapshot);

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                  frameClass_.frameSize() == masm.framePushed());

    // On ARM we don't use a bailout table.
    InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
    OutOfLineBailout* ool = new(alloc()) OutOfLineBailout(snapshot, masm.framePushed());

    // All bailout code is associated with the bytecodeSite of the block we are
    // bailing out from.
    addOutOfLineCode(ool, new(alloc()) BytecodeSite(tree, tree->script()->code()));

    masm.retarget(label, ool->entry());
}

void
CodeGeneratorARM::bailout(LSnapshot* snapshot)
{
    Label label;
    masm.ma_b(&label);
    bailoutFrom(&label, snapshot);
}

void
CodeGeneratorARM::visitOutOfLineBailout(OutOfLineBailout* ool)
{
    ScratchRegisterScope scratch(masm);
    masm.ma_mov(Imm32(ool->snapshot()->snapshotOffset()), scratch);
    masm.ma_push(scratch); // BailoutStack::padding_
    masm.ma_push(scratch); // BailoutStack::snapshotOffset_
    masm.ma_b(&deoptLabel_);
}

void
CodeGeneratorARM::visitMinMaxD(LMinMaxD* ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());

    MOZ_ASSERT(first == ToFloatRegister(ins->output()));

    if (ins->mir()->isMax())
        masm.maxDouble(second, first, true);
    else
        masm.minDouble(second, first, true);
}

void
CodeGeneratorARM::visitMinMaxF(LMinMaxF* ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());

    MOZ_ASSERT(first == ToFloatRegister(ins->output()));

    if (ins->mir()->isMax())
        masm.maxFloat32(second, first, true);
    else
        masm.minFloat32(second, first, true);
}

void
CodeGeneratorARM::visitAbsD(LAbsD* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    masm.ma_vabs(input, input);
}

void
CodeGeneratorARM::visitAbsF(LAbsF* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    MOZ_ASSERT(input == ToFloatRegister(ins->output()));
    masm.ma_vabs_f32(input, input);
}

void
CodeGeneratorARM::visitSqrtD(LSqrtD* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.ma_vsqrt(input, output);
}

void
CodeGeneratorARM::visitSqrtF(LSqrtF* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.ma_vsqrt_f32(input, output);
}

void
CodeGeneratorARM::visitAddI(LAddI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);

    if (rhs->isConstant())
        masm.ma_add(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCC);
    else if (rhs->isRegister())
        masm.ma_add(ToRegister(lhs), ToRegister(rhs), ToRegister(dest), SetCC);
    else
        masm.ma_add(ToRegister(lhs), Operand(ToAddress(rhs)), ToRegister(dest), SetCC);

    if (ins->snapshot())
        bailoutIf(Assembler::Overflow, ins->snapshot());
}

void
CodeGeneratorARM::visitAddI64(LAddI64* lir)
{
    const LInt64Allocation lhs = lir->getInt64Operand(LAddI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LAddI64::Rhs);

    MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

    if (IsConstant(rhs)) {
        masm.add64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
        return;
    }

    masm.add64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
}

void
CodeGeneratorARM::visitSubI(LSubI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);

    if (rhs->isConstant())
        masm.ma_sub(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCC);
    else if (rhs->isRegister())
        masm.ma_sub(ToRegister(lhs), ToRegister(rhs), ToRegister(dest), SetCC);
    else
        masm.ma_sub(ToRegister(lhs), Operand(ToAddress(rhs)), ToRegister(dest), SetCC);

    if (ins->snapshot())
        bailoutIf(Assembler::Overflow, ins->snapshot());
}

void
CodeGeneratorARM::visitSubI64(LSubI64* lir)
{
    const LInt64Allocation lhs = lir->getInt64Operand(LSubI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LSubI64::Rhs);

    MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

    if (IsConstant(rhs)) {
        masm.sub64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
        return;
    }

    masm.sub64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
}

void
CodeGeneratorARM::visitMulI(LMulI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);
    MMul* mul = ins->mir();
    MOZ_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    if (rhs->isConstant()) {
        // Bailout when this condition is met.
        Assembler::Condition c = Assembler::Overflow;
        // Bailout on -0.0
        int32_t constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            masm.ma_cmp(ToRegister(lhs), Imm32(0));
            bailoutIf(bailoutCond, ins->snapshot());
        }
        // TODO: move these to ma_mul.
        switch (constant) {
          case -1:
            masm.ma_rsb(ToRegister(lhs), Imm32(0), ToRegister(dest), SetCC);
            break;
          case 0:
            masm.ma_mov(Imm32(0), ToRegister(dest));
            return; // Escape overflow check;
          case 1:
            // Nop
            masm.ma_mov(ToRegister(lhs), ToRegister(dest));
            return; // Escape overflow check;
          case 2:
            masm.ma_add(ToRegister(lhs), ToRegister(lhs), ToRegister(dest), SetCC);
            // Overflow is handled later.
            break;
          default: {
            bool handled = false;
            if (constant > 0) {
                // Try shift and add sequences for a positive constant.
                if (!mul->canOverflow()) {
                    // If it cannot overflow, we can do lots of optimizations.
                    Register src = ToRegister(lhs);
                    uint32_t shift = FloorLog2(constant);
                    uint32_t rest = constant - (1 << shift);
                    // See if the constant has one bit set, meaning it can be
                    // encoded as a bitshift.
                    if ((1 << shift) == constant) {
                        masm.ma_lsl(Imm32(shift), src, ToRegister(dest));
                        handled = true;
                    } else {
                        // If the constant cannot be encoded as (1 << C1), see
                        // if it can be encoded as (1 << C1) | (1 << C2), which
                        // can be computed using an add and a shift.
                        uint32_t shift_rest = FloorLog2(rest);
                        if ((1u << shift_rest) == rest) {
                            masm.as_add(ToRegister(dest), src, lsl(src, shift-shift_rest));
                            if (shift_rest != 0)
                                masm.ma_lsl(Imm32(shift_rest), ToRegister(dest), ToRegister(dest));
                            handled = true;
                        }
                    }
                } else if (ToRegister(lhs) != ToRegister(dest)) {
                    // To stay on the safe side, only optimize things that are a
                    // power of 2.

                    uint32_t shift = FloorLog2(constant);
                    if ((1 << shift) == constant) {
                        // dest = lhs * pow(2,shift)
                        masm.ma_lsl(Imm32(shift), ToRegister(lhs), ToRegister(dest));
                        // At runtime, check (lhs == dest >> shift), if this
                        // does not hold, some bits were lost due to overflow,
                        // and the computation should be resumed as a double.
                        masm.as_cmp(ToRegister(lhs), asr(ToRegister(dest), shift));
                        c = Assembler::NotEqual;
                        handled = true;
                    }
                }
            }

            if (!handled) {
                if (mul->canOverflow())
                    c = masm.ma_check_mul(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), c);
                else
                    masm.ma_mul(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest));
            }
          }
        }
        // Bailout on overflow.
        if (mul->canOverflow())
            bailoutIf(c, ins->snapshot());
    } else {
        Assembler::Condition c = Assembler::Overflow;

        if (mul->canOverflow())
            c = masm.ma_check_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest), c);
        else
            masm.ma_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest));

        // Bailout on overflow.
        if (mul->canOverflow())
            bailoutIf(c, ins->snapshot());

        if (mul->canBeNegativeZero()) {
            Label done;
            masm.ma_cmp(ToRegister(dest), Imm32(0));
            masm.ma_b(&done, Assembler::NotEqual);

            // Result is -0 if lhs or rhs is negative.
            masm.ma_cmn(ToRegister(lhs), ToRegister(rhs));
            bailoutIf(Assembler::Signed, ins->snapshot());

            masm.bind(&done);
        }
    }
}

void
CodeGeneratorARM::visitMulI64(LMulI64* lir)
{
    const LInt64Allocation lhs = lir->getInt64Operand(LMulI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LMulI64::Rhs);

    MOZ_ASSERT(ToRegister64(lhs) == ToOutRegister64(lir));

    if (IsConstant(rhs)) {
        int64_t constant = ToInt64(rhs);
        switch (constant) {
          case -1:
            masm.neg64(ToRegister64(lhs));
            return;
          case 0:
            masm.xor64(ToRegister64(lhs), ToRegister64(lhs));
            return;
          case 1:
            // nop
            return;
          case 2:
            masm.add64(ToRegister64(lhs), ToRegister64(lhs));
            return;
          default:
            if (constant > 0) {
                // Use shift if constant is power of 2.
                int32_t shift = mozilla::FloorLog2(constant);
                if (int64_t(1) << shift == constant) {
                    masm.lshift64(Imm32(shift), ToRegister64(lhs));
                    return;
                }
            }
            Register temp = ToTempRegisterOrInvalid(lir->temp());
            masm.mul64(Imm64(constant), ToRegister64(lhs), temp);
        }
    } else {
        Register temp = ToTempRegisterOrInvalid(lir->temp());
        masm.mul64(ToOperandOrRegister64(rhs), ToRegister64(lhs), temp);
    }
}

void
CodeGeneratorARM::divICommon(MDiv* mir, Register lhs, Register rhs, Register output,
                             LSnapshot* snapshot, Label& done)
{
    if (mir->canBeNegativeOverflow()) {
        // Handle INT32_MIN / -1;
        // The integer division will give INT32_MIN, but we want -(double)INT32_MIN.

        // Sets EQ if lhs == INT32_MIN.
        masm.ma_cmp(lhs, Imm32(INT32_MIN));
        // If EQ (LHS == INT32_MIN), sets EQ if rhs == -1.
        masm.ma_cmp(rhs, Imm32(-1), Assembler::Equal);
        if (mir->canTruncateOverflow()) {
            if (mir->trapOnError()) {
                masm.ma_b(wasm::JumpTarget::IntegerOverflow, Assembler::Equal);
            } else {
                // (-INT32_MIN)|0 = INT32_MIN
                Label skip;
                masm.ma_b(&skip, Assembler::NotEqual);
                masm.ma_mov(Imm32(INT32_MIN), output);
                masm.ma_b(&done);
                masm.bind(&skip);
            }
        } else {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Equal, snapshot);
        }
    }

    // Handle divide by zero.
    if (mir->canBeDivideByZero()) {
        masm.ma_cmp(rhs, Imm32(0));
        if (mir->canTruncateInfinities()) {
            if (mir->trapOnError()) {
                masm.ma_b(wasm::JumpTarget::IntegerDivideByZero, Assembler::Equal);
            } else {
                // Infinity|0 == 0
                Label skip;
                masm.ma_b(&skip, Assembler::NotEqual);
                masm.ma_mov(Imm32(0), output);
                masm.ma_b(&done);
                masm.bind(&skip);
            }
        } else {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Equal, snapshot);
        }
    }

    // Handle negative 0.
    if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
        Label nonzero;
        masm.ma_cmp(lhs, Imm32(0));
        masm.ma_b(&nonzero, Assembler::NotEqual);
        masm.ma_cmp(rhs, Imm32(0));
        MOZ_ASSERT(mir->fallible());
        bailoutIf(Assembler::LessThan, snapshot);
        masm.bind(&nonzero);
    }
}

void
CodeGeneratorARM::visitDivI(LDivI* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register temp = ToRegister(ins->getTemp(0));
    Register output = ToRegister(ins->output());
    MDiv* mir = ins->mir();

    Label done;
    divICommon(mir, lhs, rhs, output, ins->snapshot(), done);

    if (mir->canTruncateRemainder()) {
        masm.ma_sdiv(lhs, rhs, output);
    } else {
        {
            ScratchRegisterScope scratch(masm);
            masm.ma_sdiv(lhs, rhs, temp);
            masm.ma_mul(temp, rhs, scratch);
            masm.ma_cmp(lhs, scratch);
        }
        bailoutIf(Assembler::NotEqual, ins->snapshot());
        masm.ma_mov(temp, output);
    }

    masm.bind(&done);
}

extern "C" {
    extern MOZ_EXPORT int64_t __aeabi_idivmod(int,int);
    extern MOZ_EXPORT int64_t __aeabi_uidivmod(int,int);
}

void
CodeGeneratorARM::visitSoftDivI(LSoftDivI* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    MDiv* mir = ins->mir();

    Label done;
    divICommon(mir, lhs, rhs, output, ins->snapshot(), done);

    masm.setupAlignedABICall();
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    if (gen->compilingAsmJS())
        masm.callWithABI(wasm::SymbolicAddress::aeabi_idivmod);
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, __aeabi_idivmod));

    // idivmod returns the quotient in r0, and the remainder in r1.
    if (!mir->canTruncateRemainder()) {
        MOZ_ASSERT(mir->fallible());
        masm.ma_cmp(r1, Imm32(0));
        bailoutIf(Assembler::NonZero, ins->snapshot());
    }

    masm.bind(&done);
}

void
CodeGeneratorARM::visitDivPowTwoI(LDivPowTwoI* ins)
{
    MDiv* mir = ins->mir();
    Register lhs = ToRegister(ins->numerator());
    Register output = ToRegister(ins->output());
    int32_t shift = ins->shift();

    if (shift == 0) {
        masm.ma_mov(lhs, output);
        return;
    }

    if (!mir->isTruncated()) {
        // If the remainder is != 0, bailout since this must be a double.
        {
            // The bailout code also needs the scratch register.
            // Here it is only used as a dummy target to set CC flags.
            ScratchRegisterScope scratch(masm);
            masm.as_mov(scratch, lsl(lhs, 32 - shift), SetCC);
        }
        bailoutIf(Assembler::NonZero, ins->snapshot());
    }

    if (!mir->canBeNegativeDividend()) {
        // Numerator is unsigned, so needs no adjusting. Do the shift.
        masm.as_mov(output, asr(lhs, shift));
        return;
    }

    // Adjust the value so that shifting produces a correctly rounded result
    // when the numerator is negative. See 10-1 "Signed Division by a Known
    // Power of 2" in Henry S. Warren, Jr.'s Hacker's Delight.
    ScratchRegisterScope scratch(masm);

    if (shift > 1) {
        masm.as_mov(scratch, asr(lhs, 31));
        masm.as_add(scratch, lhs, lsr(scratch, 32 - shift));
    } else {
        masm.as_add(scratch, lhs, lsr(lhs, 32 - shift));
    }

    // Do the shift.
    masm.as_mov(output, asr(scratch, shift));
}

void
CodeGeneratorARM::modICommon(MMod* mir, Register lhs, Register rhs, Register output,
                             LSnapshot* snapshot, Label& done)
{
    // 0/X (with X < 0) is bad because both of these values *should* be doubles,
    // and the result should be -0.0, which cannot be represented in integers.
    // X/0 is bad because it will give garbage (or abort), when it should give
    // either \infty, -\infty or NAN.

    // Prevent 0 / X (with X < 0) and X / 0
    // testing X / Y. Compare Y with 0.
    // There are three cases: (Y < 0), (Y == 0) and (Y > 0).
    // If (Y < 0), then we compare X with 0, and bail if X == 0.
    // If (Y == 0), then we simply want to bail. Since this does not set the
    // flags necessary for LT to trigger, we don't test X, and take the bailout
    // because the EQ flag is set.
    // If (Y > 0), we don't set EQ, and we don't trigger LT, so we don't take
    // the bailout.
    if (mir->canBeDivideByZero() || mir->canBeNegativeDividend()) {
        masm.ma_cmp(rhs, Imm32(0));
        masm.ma_cmp(lhs, Imm32(0), Assembler::LessThan);
        if (mir->isTruncated()) {
            if (mir->trapOnError()) {
                masm.ma_b(wasm::JumpTarget::IntegerDivideByZero, Assembler::Equal);
            } else {
                // NaN|0 == 0 and (0 % -X)|0 == 0
                Label skip;
                masm.ma_b(&skip, Assembler::NotEqual);
                masm.ma_mov(Imm32(0), output);
                masm.ma_b(&done);
                masm.bind(&skip);
            }
        } else {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Equal, snapshot);
        }
    }
}

void
CodeGeneratorARM::visitModI(LModI* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Register callTemp = ToRegister(ins->callTemp());
    MMod* mir = ins->mir();

    // Save the lhs in case we end up with a 0 that should be a -0.0 because lhs < 0.
    masm.ma_mov(lhs, callTemp);

    Label done;
    modICommon(mir, lhs, rhs, output, ins->snapshot(), done);

    masm.ma_smod(lhs, rhs, output);

    // If X%Y == 0 and X < 0, then we *actually* wanted to return -0.0.
    if (mir->canBeNegativeDividend()) {
        if (mir->isTruncated()) {
            // -0.0|0 == 0
        } else {
            MOZ_ASSERT(mir->fallible());
            // See if X < 0
            masm.ma_cmp(output, Imm32(0));
            masm.ma_b(&done, Assembler::NotEqual);
            masm.ma_cmp(callTemp, Imm32(0));
            bailoutIf(Assembler::Signed, ins->snapshot());
        }
    }

    masm.bind(&done);
}

void
CodeGeneratorARM::visitSoftModI(LSoftModI* ins)
{
    // Extract the registers from this instruction.
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Register callTemp = ToRegister(ins->callTemp());
    MMod* mir = ins->mir();
    Label done;

    // Save the lhs in case we end up with a 0 that should be a -0.0 because lhs < 0.
    MOZ_ASSERT(callTemp.code() > r3.code() && callTemp.code() < r12.code());
    masm.ma_mov(lhs, callTemp);

    // Prevent INT_MIN % -1;
    // The integer division will give INT_MIN, but we want -(double)INT_MIN.
    if (mir->canBeNegativeDividend()) {
        // Sets EQ if lhs == INT_MIN
        masm.ma_cmp(lhs, Imm32(INT_MIN));
        // If EQ (LHS == INT_MIN), sets EQ if rhs == -1
        masm.ma_cmp(rhs, Imm32(-1), Assembler::Equal);
        if (mir->isTruncated()) {
            // (INT_MIN % -1)|0 == 0
            Label skip;
            masm.ma_b(&skip, Assembler::NotEqual);
            masm.ma_mov(Imm32(0), output);
            masm.ma_b(&done);
            masm.bind(&skip);
        } else {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Equal, ins->snapshot());
        }
    }

    modICommon(mir, lhs, rhs, output, ins->snapshot(), done);

    masm.setupAlignedABICall();
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    if (gen->compilingAsmJS())
        masm.callWithABI(wasm::SymbolicAddress::aeabi_idivmod);
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, __aeabi_idivmod));

    // If X%Y == 0 and X < 0, then we *actually* wanted to return -0.0
    if (mir->canBeNegativeDividend()) {
        if (mir->isTruncated()) {
            // -0.0|0 == 0
        } else {
            MOZ_ASSERT(mir->fallible());
            // See if X < 0
            masm.ma_cmp(r1, Imm32(0));
            masm.ma_b(&done, Assembler::NotEqual);
            masm.ma_cmp(callTemp, Imm32(0));
            bailoutIf(Assembler::Signed, ins->snapshot());
        }
    }
    masm.bind(&done);
}

void
CodeGeneratorARM::visitModPowTwoI(LModPowTwoI* ins)
{
    Register in = ToRegister(ins->getOperand(0));
    Register out = ToRegister(ins->getDef(0));
    MMod* mir = ins->mir();
    Label fin;
    // bug 739870, jbramley has a different sequence that may help with speed
    // here.
    masm.ma_mov(in, out, SetCC);
    masm.ma_b(&fin, Assembler::Zero);
    masm.ma_rsb(Imm32(0), out, LeaveCC, Assembler::Signed);
    masm.ma_and(Imm32((1 << ins->shift()) - 1), out);
    masm.ma_rsb(Imm32(0), out, SetCC, Assembler::Signed);
    if (mir->canBeNegativeDividend()) {
        if (!mir->isTruncated()) {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Zero, ins->snapshot());
        } else {
            // -0|0 == 0
        }
    }
    masm.bind(&fin);
}

void
CodeGeneratorARM::visitModMaskI(LModMaskI* ins)
{
    Register src = ToRegister(ins->getOperand(0));
    Register dest = ToRegister(ins->getDef(0));
    Register tmp1 = ToRegister(ins->getTemp(0));
    Register tmp2 = ToRegister(ins->getTemp(1));
    MMod* mir = ins->mir();
    masm.ma_mod_mask(src, dest, tmp1, tmp2, ins->shift());
    if (mir->canBeNegativeDividend()) {
        if (!mir->isTruncated()) {
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Zero, ins->snapshot());
        } else {
            // -0|0 == 0
        }
    }
}

void
CodeGeneratorARM::visitBitNotI(LBitNotI* ins)
{
    const LAllocation* input = ins->getOperand(0);
    const LDefinition* dest = ins->getDef(0);
    // This will not actually be true on arm. We can not an imm8m in order to
    // get a wider range of numbers
    MOZ_ASSERT(!input->isConstant());

    masm.ma_mvn(ToRegister(input), ToRegister(dest));
}

void
CodeGeneratorARM::visitBitOpI(LBitOpI* ins)
{
    const LAllocation* lhs = ins->getOperand(0);
    const LAllocation* rhs = ins->getOperand(1);
    const LDefinition* dest = ins->getDef(0);
    // All of these bitops should be either imm32's, or integer registers.
    switch (ins->bitop()) {
      case JSOP_BITOR:
        if (rhs->isConstant())
            masm.ma_orr(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        else
            masm.ma_orr(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        break;
      case JSOP_BITXOR:
        if (rhs->isConstant())
            masm.ma_eor(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        else
            masm.ma_eor(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        break;
      case JSOP_BITAND:
        if (rhs->isConstant())
            masm.ma_and(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        else
            masm.ma_and(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        break;
      default:
        MOZ_CRASH("unexpected binary opcode");
    }
}

void
CodeGeneratorARM::visitShiftI(LShiftI* ins)
{
    Register lhs = ToRegister(ins->lhs());
    const LAllocation* rhs = ins->rhs();
    Register dest = ToRegister(ins->output());

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        switch (ins->bitop()) {
          case JSOP_LSH:
            if (shift)
                masm.ma_lsl(Imm32(shift), lhs, dest);
            else
                masm.ma_mov(lhs, dest);
            break;
          case JSOP_RSH:
            if (shift)
                masm.ma_asr(Imm32(shift), lhs, dest);
            else
                masm.ma_mov(lhs, dest);
            break;
          case JSOP_URSH:
            if (shift) {
                masm.ma_lsr(Imm32(shift), lhs, dest);
            } else {
                // x >>> 0 can overflow.
                masm.ma_mov(lhs, dest);
                if (ins->mir()->toUrsh()->fallible()) {
                    masm.ma_cmp(dest, Imm32(0));
                    bailoutIf(Assembler::LessThan, ins->snapshot());
                }
            }
            break;
          default:
            MOZ_CRASH("Unexpected shift op");
        }
    } else {
        // The shift amounts should be AND'ed into the 0-31 range since arm
        // shifts by the lower byte of the register (it will attempt to shift by
        // 250 if you ask it to).
        masm.ma_and(Imm32(0x1F), ToRegister(rhs), dest);

        switch (ins->bitop()) {
          case JSOP_LSH:
            masm.ma_lsl(dest, lhs, dest);
            break;
          case JSOP_RSH:
            masm.ma_asr(dest, lhs, dest);
            break;
          case JSOP_URSH:
            masm.ma_lsr(dest, lhs, dest);
            if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                masm.ma_cmp(dest, Imm32(0));
                bailoutIf(Assembler::LessThan, ins->snapshot());
            }
            break;
          default:
            MOZ_CRASH("Unexpected shift op");
        }
    }
}

void
CodeGeneratorARM::visitUrshD(LUrshD* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register temp = ToRegister(ins->temp());

    const LAllocation* rhs = ins->rhs();
    FloatRegister out = ToFloatRegister(ins->output());

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        if (shift)
            masm.ma_lsr(Imm32(shift), lhs, temp);
        else
            masm.ma_mov(lhs, temp);
    } else {
        masm.ma_and(Imm32(0x1F), ToRegister(rhs), temp);
        masm.ma_lsr(temp, lhs, temp);
    }

    masm.convertUInt32ToDouble(temp, out);
}

void
CodeGeneratorARM::visitClzI(LClzI* ins)
{
    Register input = ToRegister(ins->input());
    Register output = ToRegister(ins->output());

    masm.clz32(input, output, /* knownNotZero = */ false);
}

void
CodeGeneratorARM::visitCtzI(LCtzI* ins)
{
    Register input = ToRegister(ins->input());
    Register output = ToRegister(ins->output());

    masm.ctz32(input, output, /* knownNotZero = */ false);
}

void
CodeGeneratorARM::visitPopcntI(LPopcntI* ins)
{
    Register input = ToRegister(ins->input());
    Register output = ToRegister(ins->output());

    Register tmp = ToRegister(ins->temp());

    masm.popcnt32(input, output, tmp);
}

void
CodeGeneratorARM::visitPowHalfD(LPowHalfD* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    ScratchDoubleScope scratch(masm);

    Label done;

    // Masm.pow(-Infinity, 0.5) == Infinity.
    masm.loadConstantDouble(NegativeInfinity<double>(), scratch);
    masm.compareDouble(input, scratch);
    masm.ma_vneg(scratch, output, Assembler::Equal);
    masm.ma_b(&done, Assembler::Equal);

    // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5).
    // Adding 0 converts any -0 to 0.
    masm.loadConstantDouble(0.0, scratch);
    masm.ma_vadd(scratch, input, output);
    masm.ma_vsqrt(output, output);

    masm.bind(&done);
}

MoveOperand
CodeGeneratorARM::toMoveOperand(LAllocation a) const
{
    if (a.isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a.isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    int32_t offset = ToStackOffset(a);
    MOZ_ASSERT((offset & 3) == 0);
    return MoveOperand(StackPointer, offset);
}

class js::jit::OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorARM>
{
    MTableSwitch* mir_;
    Vector<CodeLabel, 8, JitAllocPolicy> codeLabels_;

    void accept(CodeGeneratorARM* codegen) {
        codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(TempAllocator& alloc, MTableSwitch* mir)
      : mir_(mir),
        codeLabels_(alloc)
    {}

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
CodeGeneratorARM::visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool)
{
    MTableSwitch* mir = ool->mir();

    size_t numCases = mir->numCases();
    for (size_t i = 0; i < numCases; i++) {
        LBlock* caseblock = skipTrivialBlocks(mir->getCase(numCases - 1 - i))->lir();
        Label* caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl = ool->codeLabel(i);
        cl.target()->bind(caseoffset);
        masm.addCodeLabel(cl);
    }
}

void
CodeGeneratorARM::emitTableSwitchDispatch(MTableSwitch* mir, Register index, Register base)
{
    // The code generated by this is utter hax.
    // The end result looks something like:
    // SUBS index, input, #base
    // RSBSPL index, index, #max
    // LDRPL pc, pc, index lsl 2
    // B default

    // If the range of targets in N through M, we first subtract off the lowest
    // case (N), which both shifts the arguments into the range 0 to (M - N)
    // with and sets the MInus flag if the argument was out of range on the low
    // end.

    // Then we a reverse subtract with the size of the jump table, which will
    // reverse the order of range (It is size through 0, rather than 0 through
    // size). The main purpose of this is that we set the same flag as the lower
    // bound check for the upper bound check. Lastly, we do this conditionally
    // on the previous check succeeding.

    // Then we conditionally load the pc offset by the (reversed) index (times
    // the address size) into the pc, which branches to the correct case. NOTE:
    // when we go to read the pc, the value that we get back is the pc of the
    // current instruction *PLUS 8*. This means that ldr foo, [pc, +0] reads
    // $pc+8. In other words, there is an empty word after the branch into the
    // switch table before the table actually starts. Since the only other
    // unhandled case is the default case (both out of range high and out of
    // range low) I then insert a branch to default case into the extra slot,
    // which ensures we don't attempt to execute the address table.
    Label* defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

    int32_t cases = mir->numCases();
    // Lower value with low value.
    masm.ma_sub(index, Imm32(mir->low()), index, SetCC);
    masm.ma_rsb(index, Imm32(cases - 1), index, SetCC, Assembler::NotSigned);
    // Inhibit pools within the following sequence because we are indexing into
    // a pc relative table. The region will have one instruction for ma_ldr, one
    // for ma_b, and each table case takes one word.
    AutoForbidPools afp(&masm, 1 + 1 + cases);
    masm.ma_ldr(DTRAddr(pc, DtrRegImmShift(index, LSL, 2)), pc, Offset, Assembler::NotSigned);
    masm.ma_b(defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first generate
    // the case entries (we don't yet know their offsets in the instruction
    // stream).
    OutOfLineTableSwitch* ool = new(alloc()) OutOfLineTableSwitch(alloc(), mir);
    for (int32_t i = 0; i < cases; i++) {
        CodeLabel cl;
        masm.writeCodePointer(cl.patchAt());
        masm.propagateOOM(ool->addCodeLabel(cl));
    }
    addOutOfLineCode(ool, mir);
}

void
CodeGeneratorARM::visitMathD(LMathD* math)
{
    FloatRegister src1 = ToFloatRegister(math->getOperand(0));
    FloatRegister src2 = ToFloatRegister(math->getOperand(1));
    FloatRegister output = ToFloatRegister(math->getDef(0));

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.ma_vadd(src1, src2, output);
        break;
      case JSOP_SUB:
        masm.ma_vsub(src1, src2, output);
        break;
      case JSOP_MUL:
        masm.ma_vmul(src1, src2, output);
        break;
      case JSOP_DIV:
        masm.ma_vdiv(src1, src2, output);
        break;
      default:
        MOZ_CRASH("unexpected opcode");
    }
}

void
CodeGeneratorARM::visitMathF(LMathF* math)
{
    FloatRegister src1 = ToFloatRegister(math->getOperand(0));
    FloatRegister src2 = ToFloatRegister(math->getOperand(1));
    FloatRegister output = ToFloatRegister(math->getDef(0));

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.ma_vadd_f32(src1, src2, output);
        break;
      case JSOP_SUB:
        masm.ma_vsub_f32(src1, src2, output);
        break;
      case JSOP_MUL:
        masm.ma_vmul_f32(src1, src2, output);
        break;
      case JSOP_DIV:
        masm.ma_vdiv_f32(src1, src2, output);
        break;
      default:
        MOZ_CRASH("unexpected opcode");
    }
}

void
CodeGeneratorARM::visitFloor(LFloor* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    Label bail;
    masm.floor(input, output, &bail);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::visitFloorF(LFloorF* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    Label bail;
    masm.floorf(input, output, &bail);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::visitCeil(LCeil* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    Label bail;
    masm.ceil(input, output, &bail);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::visitCeilF(LCeilF* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    Label bail;
    masm.ceilf(input, output, &bail);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::visitRound(LRound* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    FloatRegister tmp = ToFloatRegister(lir->temp());
    Label bail;
    // Output is either correct, or clamped. All -0 cases have been translated
    // to a clamped case.
    masm.round(input, output, &bail, tmp);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::visitRoundF(LRoundF* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    FloatRegister tmp = ToFloatRegister(lir->temp());
    Label bail;
    // Output is either correct, or clamped. All -0 cases have been translated
    // to a clamped case.
    masm.roundf(input, output, &bail, tmp);
    bailoutFrom(&bail, lir->snapshot());
}

void
CodeGeneratorARM::emitRoundDouble(FloatRegister src, Register dest, Label* fail)
{
    ScratchDoubleScope scratch(masm);

    masm.ma_vcvt_F64_I32(src, scratch);
    masm.ma_vxfer(scratch, dest);
    masm.ma_cmp(dest, Imm32(0x7fffffff));
    masm.ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    masm.ma_b(fail, Assembler::Equal);
}

void
CodeGeneratorARM::visitTruncateDToInt32(LTruncateDToInt32* ins)
{
    emitTruncateDouble(ToFloatRegister(ins->input()), ToRegister(ins->output()), ins->mir());
}

void
CodeGeneratorARM::visitTruncateFToInt32(LTruncateFToInt32* ins)
{
    emitTruncateFloat32(ToFloatRegister(ins->input()), ToRegister(ins->output()), ins->mir());
}

static const uint32_t FrameSizes[] = { 128, 256, 512, 1024 };

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    for (uint32_t i = 0; i < JS_ARRAY_LENGTH(FrameSizes); i++) {
        if (frameDepth < FrameSizes[i])
            return FrameSizeClass(i);
    }

    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(JS_ARRAY_LENGTH(FrameSizes));
}

uint32_t
FrameSizeClass::frameSize() const
{
    MOZ_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    MOZ_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

    return FrameSizes[class_];
}

ValueOperand
CodeGeneratorARM::ToValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorARM::ToOutValue(LInstruction* ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorARM::ToTempValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

void
CodeGeneratorARM::visitValue(LValue* value)
{
    const ValueOperand out = ToOutValue(value);

    masm.moveValue(value->value(), out);
}

void
CodeGeneratorARM::visitBox(LBox* box)
{
    const LDefinition* type = box->getDef(TYPE_INDEX);

    MOZ_ASSERT(!box->getOperand(0)->isConstant());

    // On x86, the input operand and the output payload have the same virtual
    // register. All that needs to be written is the type tag for the type
    // definition.
    masm.ma_mov(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
}

void
CodeGeneratorARM::visitBoxFloatingPoint(LBoxFloatingPoint* box)
{
    const LDefinition* payload = box->getDef(PAYLOAD_INDEX);
    const LDefinition* type = box->getDef(TYPE_INDEX);
    const LAllocation* in = box->getOperand(0);
    FloatRegister reg = ToFloatRegister(in);

    if (box->type() == MIRType::Float32) {
        ScratchFloat32Scope scratch(masm);
        masm.convertFloat32ToDouble(reg, scratch);
        masm.ma_vxfer(VFPRegister(scratch), ToRegister(payload), ToRegister(type));
    } else {
        masm.ma_vxfer(VFPRegister(reg), ToRegister(payload), ToRegister(type));
    }
}

void
CodeGeneratorARM::visitUnbox(LUnbox* unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox* mir = unbox->mir();
    Register type = ToRegister(unbox->type());

    if (mir->fallible()) {
        masm.ma_cmp(type, Imm32(MIRTypeToTag(mir->type())));
        bailoutIf(Assembler::NotEqual, unbox->snapshot());
    }
}

void
CodeGeneratorARM::visitDouble(LDouble* ins)
{
    const LDefinition* out = ins->getDef(0);
    masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
}

void
CodeGeneratorARM::visitFloat32(LFloat32* ins)
{
    const LDefinition* out = ins->getDef(0);
    masm.loadConstantFloat32(ins->getFloat(), ToFloatRegister(out));
}

Register
CodeGeneratorARM::splitTagForTest(const ValueOperand& value)
{
    return value.typeReg();
}

void
CodeGeneratorARM::visitTestDAndBranch(LTestDAndBranch* test)
{
    const LAllocation* opd = test->input();
    masm.ma_vcmpz(ToFloatRegister(opd));
    masm.as_vmrs(pc);

    MBasicBlock* ifTrue = test->ifTrue();
    MBasicBlock* ifFalse = test->ifFalse();
    // If the compare set the 0 bit, then the result is definately false.
    jumpToBlock(ifFalse, Assembler::Zero);
    // It is also false if one of the operands is NAN, which is shown as
    // Overflow.
    jumpToBlock(ifFalse, Assembler::Overflow);
    jumpToBlock(ifTrue);
}

void
CodeGeneratorARM::visitTestFAndBranch(LTestFAndBranch* test)
{
    const LAllocation* opd = test->input();
    masm.ma_vcmpz_f32(ToFloatRegister(opd));
    masm.as_vmrs(pc);

    MBasicBlock* ifTrue = test->ifTrue();
    MBasicBlock* ifFalse = test->ifFalse();
    // If the compare set the 0 bit, then the result is definately false.
    jumpToBlock(ifFalse, Assembler::Zero);
    // It is also false if one of the operands is NAN, which is shown as
    // Overflow.
    jumpToBlock(ifFalse, Assembler::Overflow);
    jumpToBlock(ifTrue);
}

void
CodeGeneratorARM::visitCompareD(LCompareD* comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.compareDouble(lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()));
}

void
CodeGeneratorARM::visitCompareF(LCompareF* comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.compareFloat(lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()));
}

void
CodeGeneratorARM::visitCompareDAndBranch(LCompareDAndBranch* comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    masm.compareDouble(lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse());
}

void
CodeGeneratorARM::visitCompareFAndBranch(LCompareFAndBranch* comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());
    masm.compareFloat(lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse());
}

void
CodeGeneratorARM::visitCompareB(LCompareB* lir)
{
    MCompare* mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation* rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Label notBoolean, done;
    masm.branchTestBoolean(Assembler::NotEqual, lhs, &notBoolean);
    {
        if (rhs->isConstant())
            masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
        else
            masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
        masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
        masm.jump(&done);
    }

    masm.bind(&notBoolean);
    {
        masm.move32(Imm32(mir->jsop() == JSOP_STRICTNE), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorARM::visitCompareBAndBranch(LCompareBAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation* rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Assembler::Condition cond = masm.testBoolean(Assembler::NotEqual, lhs);
    jumpToBlock((mir->jsop() == JSOP_STRICTEQ) ? lir->ifFalse() : lir->ifTrue(), cond);

    if (rhs->isConstant())
        masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
    else
        masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorARM::visitCompareBitwise(LCompareBitwise* lir)
{
    MCompare* mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    Label notEqual, done;
    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    masm.j(Assembler::NotEqual, &notEqual);
    {
        masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
        masm.emitSet(cond, output);
        masm.jump(&done);
    }
    masm.bind(&notEqual);
    {
        masm.move32(Imm32(cond == Assembler::NotEqual), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorARM::visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock* notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    jumpToBlock(notEqual, Assembler::NotEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorARM::visitBitAndAndBranch(LBitAndAndBranch* baab)
{
    if (baab->right()->isConstant())
        masm.ma_tst(ToRegister(baab->left()), Imm32(ToInt32(baab->right())));
    else
        masm.ma_tst(ToRegister(baab->left()), ToRegister(baab->right()));
    emitBranch(Assembler::NonZero, baab->ifTrue(), baab->ifFalse());
}

void
CodeGeneratorARM::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble* lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorARM::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32* lir)
{
    masm.convertUInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorARM::visitNotI(LNotI* ins)
{
    // It is hard to optimize !x, so just do it the basic way for now.
    masm.ma_cmp(ToRegister(ins->input()), Imm32(0));
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()));
}

void
CodeGeneratorARM::visitNotI64(LNotI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register output = ToRegister(lir->output());

    masm.ma_orr(input.low, input.high, output);
    masm.ma_cmp(output, Imm32(0));
    masm.emitSet(Assembler::Equal, output);
}

void
CodeGeneratorARM::visitNotD(LNotD* ins)
{
    // Since this operation is not, we want to set a bit if the double is
    // falsey, which means 0.0, -0.0 or NaN. When comparing with 0, an input of
    // 0 will set the Z bit (30) and NaN will set the V bit (28) of the APSR.
    FloatRegister opd = ToFloatRegister(ins->input());
    Register dest = ToRegister(ins->output());

    // Do the compare.
    masm.ma_vcmpz(opd);
    // TODO There are three variations here to compare performance-wise.
    bool nocond = true;
    if (nocond) {
        // Load the value into the dest register.
        masm.as_vmrs(dest);
        masm.ma_lsr(Imm32(28), dest, dest);
        // 28 + 2 = 30
        masm.ma_alu(dest, lsr(dest, 2), dest, OpOrr);
        masm.ma_and(Imm32(1), dest);
    } else {
        masm.as_vmrs(pc);
        masm.ma_mov(Imm32(0), dest);
        masm.ma_mov(Imm32(1), dest, Assembler::Equal);
        masm.ma_mov(Imm32(1), dest, Assembler::Overflow);
    }
}

void
CodeGeneratorARM::visitNotF(LNotF* ins)
{
    // Since this operation is not, we want to set a bit if the double is
    // falsey, which means 0.0, -0.0 or NaN. When comparing with 0, an input of
    // 0 will set the Z bit (30) and NaN will set the V bit (28) of the APSR.
    FloatRegister opd = ToFloatRegister(ins->input());
    Register dest = ToRegister(ins->output());

    // Do the compare.
    masm.ma_vcmpz_f32(opd);
    // TODO There are three variations here to compare performance-wise.
    bool nocond = true;
    if (nocond) {
        // Load the value into the dest register.
        masm.as_vmrs(dest);
        masm.ma_lsr(Imm32(28), dest, dest);
        // 28 + 2 = 30
        masm.ma_alu(dest, lsr(dest, 2), dest, OpOrr);
        masm.ma_and(Imm32(1), dest);
    } else {
        masm.as_vmrs(pc);
        masm.ma_mov(Imm32(0), dest);
        masm.ma_mov(Imm32(1), dest, Assembler::Equal);
        masm.ma_mov(Imm32(1), dest, Assembler::Overflow);
    }
}

void
CodeGeneratorARM::visitGuardShape(LGuardShape* guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.ma_ldr(DTRAddr(obj, DtrOffImm(ShapedObject::offsetOfShape())), tmp);
    masm.ma_cmp(tmp, ImmGCPtr(guard->mir()->shape()));

    bailoutIf(Assembler::NotEqual, guard->snapshot());
}

void
CodeGeneratorARM::visitGuardObjectGroup(LGuardObjectGroup* guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());
    MOZ_ASSERT(obj != tmp);

    masm.ma_ldr(DTRAddr(obj, DtrOffImm(JSObject::offsetOfGroup())), tmp);
    masm.ma_cmp(tmp, ImmGCPtr(guard->mir()->group()));

    Assembler::Condition cond =
        guard->mir()->bailOnEquality() ? Assembler::Equal : Assembler::NotEqual;
    bailoutIf(cond, guard->snapshot());
}

void
CodeGeneratorARM::visitGuardClass(LGuardClass* guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadObjClass(obj, tmp);
    masm.ma_cmp(tmp, Imm32((uint32_t)guard->mir()->getClass()));
    bailoutIf(Assembler::NotEqual, guard->snapshot());
}

void
CodeGeneratorARM::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint patching
    // to occur. Otherwise, we could overwrite the invalidation epilogue.
    for (size_t i = 0; i < sizeof(void*); i += Assembler::NopSize())
        masm.nop();

    masm.bind(&invalidate_);

    // Push the return address of the point that we bailed out at onto the stack.
    masm.Push(lr);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    JitCode* thunk = gen->jitRuntime()->getInvalidationThunk();

    masm.branch(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk
    // should pop the invalidated JS frame and return directly to its caller.
    masm.assumeUnreachable("Should have returned directly to its caller instead of here.");
}

void
CodeGeneratorARM::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorARM::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorARM::visitCompareExchangeTypedArrayElement(LCompareExchangeTypedArrayElement* lir)
{
    Register elements = ToRegister(lir->elements());
    AnyRegister output = ToAnyRegister(lir->output());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

    Register oldval = ToRegister(lir->oldval());
    Register newval = ToRegister(lir->newval());

    Scalar::Type arrayType = lir->mir()->arrayType();
    int width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * width);
        masm.compareExchangeToTypedIntArray(arrayType, dest, oldval, newval, temp, output);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        masm.compareExchangeToTypedIntArray(arrayType, dest, oldval, newval, temp, output);
    }
}

void
CodeGeneratorARM::visitAtomicExchangeTypedArrayElement(LAtomicExchangeTypedArrayElement* lir)
{
    Register elements = ToRegister(lir->elements());
    AnyRegister output = ToAnyRegister(lir->output());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

    Register value = ToRegister(lir->value());

    Scalar::Type arrayType = lir->mir()->arrayType();
    int width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * width);
        masm.atomicExchangeToTypedIntArray(arrayType, dest, value, temp, output);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        masm.atomicExchangeToTypedIntArray(arrayType, dest, value, temp, output);
    }
}

template<typename S, typename T>
void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const S& value, const T& mem, Register flagTemp,
                                             Register outTemp, AnyRegister output)
{
    MOZ_ASSERT(flagTemp != InvalidReg);
    MOZ_ASSERT_IF(arrayType == Scalar::Uint32, outTemp != InvalidReg);

    switch (arrayType) {
      case Scalar::Int8:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd8SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub8SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd8SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr8SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor8SignExtend(value, mem, flagTemp, output.gpr());
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Uint8:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd8ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub8ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd8ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr8ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor8ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Int16:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd16SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub16SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd16SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr16SignExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor16SignExtend(value, mem, flagTemp, output.gpr());
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Uint16:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd16ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub16ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd16ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr16ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor16ZeroExtend(value, mem, flagTemp, output.gpr());
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Int32:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd32(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub32(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd32(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr32(value, mem, flagTemp, output.gpr());
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor32(value, mem, flagTemp, output.gpr());
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Uint32:
        // At the moment, the code in MCallOptimize.cpp requires the output
        // type to be double for uint32 arrays.  See bug 1077305.
        MOZ_ASSERT(output.isFloat());
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicFetchAdd32(value, mem, flagTemp, outTemp);
            break;
          case AtomicFetchSubOp:
            masm.atomicFetchSub32(value, mem, flagTemp, outTemp);
            break;
          case AtomicFetchAndOp:
            masm.atomicFetchAnd32(value, mem, flagTemp, outTemp);
            break;
          case AtomicFetchOrOp:
            masm.atomicFetchOr32(value, mem, flagTemp, outTemp);
            break;
          case AtomicFetchXorOp:
            masm.atomicFetchXor32(value, mem, flagTemp, outTemp);
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        masm.convertUInt32ToDouble(outTemp, output.fpu());
        break;
      default:
        MOZ_CRASH("Invalid typed array type");
    }
}

template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Imm32& value, const Address& mem,
                                             Register flagTemp, Register outTemp,
                                             AnyRegister output);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Imm32& value, const BaseIndex& mem,
                                             Register flagTemp, Register outTemp,
                                             AnyRegister output);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Register& value, const Address& mem,
                                             Register flagTemp, Register outTemp,
                                             AnyRegister output);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Register& value, const BaseIndex& mem,
                                             Register flagTemp, Register outTemp,
                                             AnyRegister output);

// Binary operation for effect, result discarded.
template<typename S, typename T>
void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType, const S& value,
                                             const T& mem, Register flagTemp)
{
    MOZ_ASSERT(flagTemp != InvalidReg);

    switch (arrayType) {
      case Scalar::Int8:
      case Scalar::Uint8:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicAdd8(value, mem, flagTemp);
            break;
          case AtomicFetchSubOp:
            masm.atomicSub8(value, mem, flagTemp);
            break;
          case AtomicFetchAndOp:
            masm.atomicAnd8(value, mem, flagTemp);
            break;
          case AtomicFetchOrOp:
            masm.atomicOr8(value, mem, flagTemp);
            break;
          case AtomicFetchXorOp:
            masm.atomicXor8(value, mem, flagTemp);
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Int16:
      case Scalar::Uint16:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicAdd16(value, mem, flagTemp);
            break;
          case AtomicFetchSubOp:
            masm.atomicSub16(value, mem, flagTemp);
            break;
          case AtomicFetchAndOp:
            masm.atomicAnd16(value, mem, flagTemp);
            break;
          case AtomicFetchOrOp:
            masm.atomicOr16(value, mem, flagTemp);
            break;
          case AtomicFetchXorOp:
            masm.atomicXor16(value, mem, flagTemp);
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      case Scalar::Int32:
      case Scalar::Uint32:
        switch (op) {
          case AtomicFetchAddOp:
            masm.atomicAdd32(value, mem, flagTemp);
            break;
          case AtomicFetchSubOp:
            masm.atomicSub32(value, mem, flagTemp);
            break;
          case AtomicFetchAndOp:
            masm.atomicAnd32(value, mem, flagTemp);
            break;
          case AtomicFetchOrOp:
            masm.atomicOr32(value, mem, flagTemp);
            break;
          case AtomicFetchXorOp:
            masm.atomicXor32(value, mem, flagTemp);
            break;
          default:
            MOZ_CRASH("Invalid typed array atomic operation");
        }
        break;
      default:
        MOZ_CRASH("Invalid typed array type");
    }
}

template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Imm32& value, const Address& mem,
                                             Register flagTemp);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Imm32& value, const BaseIndex& mem,
                                             Register flagTemp);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Register& value, const Address& mem,
                                             Register flagTemp);
template void
CodeGeneratorARM::atomicBinopToTypedIntArray(AtomicOp op, Scalar::Type arrayType,
                                             const Register& value, const BaseIndex& mem,
                                             Register flagTemp);


template <typename T>
static inline void
AtomicBinopToTypedArray(CodeGeneratorARM* cg, AtomicOp op,
                        Scalar::Type arrayType, const LAllocation* value, const T& mem,
                        Register flagTemp, Register outTemp, AnyRegister output)
{
    if (value->isConstant())
        cg->atomicBinopToTypedIntArray(op, arrayType, Imm32(ToInt32(value)), mem, flagTemp, outTemp, output);
    else
        cg->atomicBinopToTypedIntArray(op, arrayType, ToRegister(value), mem, flagTemp, outTemp, output);
}

void
CodeGeneratorARM::visitAtomicTypedArrayElementBinop(LAtomicTypedArrayElementBinop* lir)
{
    MOZ_ASSERT(lir->mir()->hasUses());

    AnyRegister output = ToAnyRegister(lir->output());
    Register elements = ToRegister(lir->elements());
    Register flagTemp = ToRegister(lir->temp1());
    Register outTemp = lir->temp2()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp2());
    const LAllocation* value = lir->value();

    Scalar::Type arrayType = lir->mir()->arrayType();
    int width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address mem(elements, ToInt32(lir->index()) * width);
        AtomicBinopToTypedArray(this, lir->mir()->operation(), arrayType, value, mem, flagTemp, outTemp, output);
    } else {
        BaseIndex mem(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        AtomicBinopToTypedArray(this, lir->mir()->operation(), arrayType, value, mem, flagTemp, outTemp, output);
    }
}

template <typename T>
static inline void
AtomicBinopToTypedArray(CodeGeneratorARM* cg, AtomicOp op, Scalar::Type arrayType,
                        const LAllocation* value, const T& mem, Register flagTemp)
{
    if (value->isConstant())
        cg->atomicBinopToTypedIntArray(op, arrayType, Imm32(ToInt32(value)), mem, flagTemp);
    else
        cg->atomicBinopToTypedIntArray(op, arrayType, ToRegister(value), mem, flagTemp);
}

void
CodeGeneratorARM::visitAtomicTypedArrayElementBinopForEffect(LAtomicTypedArrayElementBinopForEffect* lir)
{
    MOZ_ASSERT(!lir->mir()->hasUses());

    Register elements = ToRegister(lir->elements());
    Register flagTemp = ToRegister(lir->flagTemp());
    const LAllocation* value = lir->value();
    Scalar::Type arrayType = lir->mir()->arrayType();
    int width = Scalar::byteSize(arrayType);

    if (lir->index()->isConstant()) {
        Address mem(elements, ToInt32(lir->index()) * width);
        AtomicBinopToTypedArray(this, lir->mir()->operation(), arrayType, value, mem, flagTemp);
    } else {
        BaseIndex mem(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        AtomicBinopToTypedArray(this, lir->mir()->operation(), arrayType, value, mem, flagTemp);
    }
}

void
CodeGeneratorARM::visitAsmSelect(LAsmSelect* ins)
{
    MIRType mirType = ins->mir()->type();

    Register cond = ToRegister(ins->condExpr());
    masm.ma_cmp(cond, Imm32(0));

    if (mirType == MIRType::Int32) {
        Register falseExpr = ToRegister(ins->falseExpr());
        Register out = ToRegister(ins->output());
        MOZ_ASSERT(ToRegister(ins->trueExpr()) == out, "true expr input is reused for output");
        masm.ma_mov(falseExpr, out, LeaveCC, Assembler::Zero);
        return;
    }

    FloatRegister out = ToFloatRegister(ins->output());
    MOZ_ASSERT(ToFloatRegister(ins->trueExpr()) == out, "true expr input is reused for output");

    FloatRegister falseExpr = ToFloatRegister(ins->falseExpr());

    if (mirType == MIRType::Double)
        masm.moveDouble(falseExpr, out, Assembler::Zero);
    else if (mirType == MIRType::Float32)
        masm.moveFloat32(falseExpr, out, Assembler::Zero);
    else
        MOZ_CRASH("unhandled type in visitAsmSelect!");
}

void
CodeGeneratorARM::visitAsmReinterpret(LAsmReinterpret* lir)
{
    MOZ_ASSERT(gen->compilingAsmJS());
    MAsmReinterpret* ins = lir->mir();

    MIRType to = ins->type();
    DebugOnly<MIRType> from = ins->input()->type();

    switch (to) {
      case MIRType::Int32:
        MOZ_ASSERT(from == MIRType::Float32);
        masm.ma_vxfer(ToFloatRegister(lir->input()), ToRegister(lir->output()));
        break;
      case MIRType::Float32:
        MOZ_ASSERT(from == MIRType::Int32);
        masm.ma_vxfer(ToRegister(lir->input()), ToFloatRegister(lir->output()));
        break;
      case MIRType::Double:
      case MIRType::Int64:
        MOZ_CRASH("not handled by this LIR opcode");
      default:
        MOZ_CRASH("unexpected AsmReinterpret");
    }
}

void
CodeGeneratorARM::emitWasmCall(LWasmCallBase* ins)
{
    MWasmCall* mir = ins->mir();

    if (UseHardFpABI() || mir->callee().which() != wasm::CalleeDesc::Builtin) {
        emitWasmCallBase(ins);
        return;
    }

    // The soft ABI passes floating point arguments in GPRs. Since basically
    // nothing is set up to handle this, the values are placed in the
    // corresponding VFP registers, then transferred to GPRs immediately
    // before the call. The mapping is sN <-> rN, where double registers
    // can be treated as their two component single registers.

    for (unsigned i = 0, e = ins->numOperands(); i < e; i++) {
        LAllocation* a = ins->getOperand(i);
        if (a->isFloatReg()) {
            FloatRegister fr = ToFloatRegister(a);
            if (fr.isDouble()) {
                uint32_t srcId = fr.singleOverlay().id();
                masm.ma_vxfer(fr, Register::FromCode(srcId), Register::FromCode(srcId + 1));
            } else {
                uint32_t srcId = fr.id();
                masm.ma_vxfer(fr, Register::FromCode(srcId));
            }
        }
    }

    emitWasmCallBase(ins);

    switch (mir->type()) {
      case MIRType::Double:
        masm.ma_vxfer(r0, r1, d0);
        break;
      case MIRType::Float32:
        masm.as_vxfer(r0, InvalidReg, VFPRegister(d0).singleOverlay(), Assembler::CoreToFloat);
        break;
      default:
        break;
    }
}

void
CodeGeneratorARM::visitWasmCall(LWasmCall* ins)
{
    emitWasmCall(ins);
}

void
CodeGeneratorARM::visitWasmCallI64(LWasmCallI64* ins)
{
    emitWasmCall(ins);
}

void
CodeGeneratorARM::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    const LAllocation* ptr = ins->ptr();

    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->accessType()) {
      case Scalar::Int8:    isSigned = true;  size =  8; break;
      case Scalar::Uint8:   isSigned = false; size =  8; break;
      case Scalar::Int16:   isSigned = true;  size = 16; break;
      case Scalar::Uint16:  isSigned = false; size = 16; break;
      case Scalar::Int32:
      case Scalar::Uint32:  isSigned = true;  size = 32; break;
      case Scalar::Float64: isFloat = true;   size = 64; break;
      case Scalar::Float32: isFloat = true;   size = 32; break;
      default: MOZ_CRASH("unexpected array type");
    }

    if (ptr->isConstant()) {
        MOZ_ASSERT(!mir->needsBoundsCheck());
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);
        if (isFloat) {
            VFPRegister vd(ToFloatRegister(ins->output()));
            if (size == 32)
                masm.ma_vldr(Address(HeapReg, ptrImm), vd.singleOverlay(), Assembler::Always);
            else
                masm.ma_vldr(Address(HeapReg, ptrImm), vd, Assembler::Always);
        }  else {
            masm.ma_dataTransferN(IsLoad, size, isSigned, HeapReg, Imm32(ptrImm),
                                  ToRegister(ins->output()), Offset, Assembler::Always);
        }
    } else {
        Register ptrReg = ToRegister(ptr);
        if (isFloat) {
            FloatRegister output = ToFloatRegister(ins->output());
            if (size == 32)
                output = output.singleOverlay();

            Assembler::Condition cond = Assembler::Always;
            if (mir->needsBoundsCheck()) {
                BufferOffset cmp = masm.as_cmp(ptrReg, Imm8(0));
                masm.append(wasm::BoundsCheck(cmp.getOffset()));

                size_t nanOffset = size == 32 ? wasm::NaN32GlobalDataOffset : wasm::NaN64GlobalDataOffset;
                masm.ma_vldr(Address(GlobalReg, nanOffset - AsmJSGlobalRegBias), output,
                             Assembler::AboveOrEqual);
                cond = Assembler::Below;
            }

            masm.ma_vldr(output, HeapReg, ptrReg, 0, cond);
        } else {
            Register output = ToRegister(ins->output());

            Assembler::Condition cond = Assembler::Always;
            if (mir->needsBoundsCheck()) {
                uint32_t cmpOffset = masm.as_cmp(ptrReg, Imm8(0)).getOffset();
                masm.append(wasm::BoundsCheck(cmpOffset));

                masm.ma_mov(Imm32(0), output, Assembler::AboveOrEqual);
                cond = Assembler::Below;
            }

            masm.ma_dataTransferN(IsLoad, size, isSigned, HeapReg, ptrReg, output, Offset, cond);
        }
    }
}

template <typename T>
void
CodeGeneratorARM::emitWasmLoad(T* lir)
{
    const MWasmLoad* mir = lir->mir();

    uint32_t offset = mir->offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptr());
    Scalar::Type type = mir->accessType();

    // Maybe add the offset.
    if (offset || type == Scalar::Int64) {
        Register ptrPlusOffset = ToRegister(lir->ptrCopy());
        if (offset)
            masm.ma_add(Imm32(offset), ptrPlusOffset);
        ptr = ptrPlusOffset;
    } else {
        MOZ_ASSERT(lir->ptrCopy()->isBogusTemp());
    }

    bool isSigned = type == Scalar::Int8 || type == Scalar::Int16 || type == Scalar::Int32 ||
                    type == Scalar::Int64;
    unsigned byteSize = mir->byteSize();

    memoryBarrier(mir->barrierBefore());

    if (mir->type() == MIRType::Int64) {
        Register64 output = ToOutRegister64(lir);
        if (type == Scalar::Int64) {
            MOZ_ASSERT(INT64LOW_OFFSET == 0);
            masm.ma_dataTransferN(IsLoad, 32, /* signed = */ false, HeapReg, ptr, output.low);
            masm.ma_add(Imm32(INT64HIGH_OFFSET), ptr);
            masm.ma_dataTransferN(IsLoad, 32, isSigned, HeapReg, ptr, output.high);
        } else {
            masm.ma_dataTransferN(IsLoad, byteSize * 8, isSigned, HeapReg, ptr, output.low);
            if (isSigned)
                masm.ma_asr(Imm32(31), output.low, output.high);
            else
                masm.ma_mov(Imm32(0), output.high);
        }
    } else {
        AnyRegister output = ToAnyRegister(lir->output());
        bool isFloat = output.isFloat();
        if (isFloat) {
            MOZ_ASSERT((byteSize == 4) == output.fpu().isSingle());
            ScratchRegisterScope scratch(masm);
            masm.ma_add(HeapReg, ptr, scratch);
            masm.ma_vldr(Address(scratch, 0), output.fpu());
        } else {
            masm.ma_dataTransferN(IsLoad, byteSize * 8, isSigned, HeapReg, ptr, output.gpr());
        }
    }

    memoryBarrier(mir->barrierAfter());
}

void
CodeGeneratorARM::visitWasmLoad(LWasmLoad* lir)
{
    emitWasmLoad(lir);
}

void
CodeGeneratorARM::visitWasmLoadI64(LWasmLoadI64* lir)
{
    emitWasmLoad(lir);
}

template<typename T>
void
CodeGeneratorARM::emitWasmUnalignedLoad(T* lir)
{
    const MWasmLoad* mir = lir->mir();

    uint32_t offset = mir->offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptrCopy());
    if (offset)
        masm.ma_add(Imm32(offset), ptr);

    // Add HeapReg to ptr, so we can use base+index addressing in the byte loads.
    masm.ma_add(HeapReg, ptr);

    unsigned byteSize = mir->byteSize();
    Scalar::Type type = mir->accessType();
    bool isSigned = type == Scalar::Int8 || type == Scalar::Int16 || type == Scalar::Int32 ||
                    type == Scalar::Int64;

    MIRType mirType = mir->type();

    Register tmp = ToRegister(lir->getTemp(1));

    Register low;
    if (IsFloatingPointType(mirType))
        low = ToRegister(lir->getTemp(2));
    else if (mirType == MIRType::Int64)
        low = ToOutRegister64(lir).low;
    else
        low = ToRegister(lir->output());

    MOZ_ASSERT(low != tmp);
    MOZ_ASSERT(low != ptr);

    memoryBarrier(mir->barrierBefore());

    masm.emitUnalignedLoad(isSigned, Min(byteSize, 4u), ptr, tmp, low);

    if (IsFloatingPointType(mirType)) {
        FloatRegister output = ToFloatRegister(lir->output());
        if (byteSize == 4) {
            MOZ_ASSERT(output.isSingle());
            masm.ma_vxfer(low, output);
        } else {
            MOZ_ASSERT(byteSize == 8);
            MOZ_ASSERT(output.isDouble());
            Register high = ToRegister(lir->getTemp(3));
            masm.emitUnalignedLoad(/* signed */ false, 4, ptr, tmp, high, /* offset */ 4);
            masm.ma_vxfer(low, high, output);
        }
    } else if (mirType == MIRType::Int64) {
        Register64 output = ToOutRegister64(lir);
        if (type == Scalar::Int64) {
            MOZ_ASSERT(byteSize == 8);
            masm.emitUnalignedLoad(isSigned, 4, ptr, tmp, output.high, /* offset */ 4);
        } else {
            MOZ_ASSERT(byteSize <= 4);
            // Propagate sign.
            if (isSigned)
                masm.ma_asr(Imm32(31), output.low, output.high);
            else
                masm.ma_mov(Imm32(0), output.high);
        }
    }

    memoryBarrier(mir->barrierAfter());
}

void
CodeGeneratorARM::visitWasmUnalignedLoad(LWasmUnalignedLoad* lir)
{
    emitWasmUnalignedLoad(lir);
}

void
CodeGeneratorARM::visitWasmUnalignedLoadI64(LWasmUnalignedLoadI64* lir)
{
    emitWasmUnalignedLoad(lir);
}

void
CodeGeneratorARM::visitWasmAddOffset(LWasmAddOffset* lir)
{
    MWasmAddOffset* mir = lir->mir();
    Register base = ToRegister(lir->base());
    Register out = ToRegister(lir->output());

    masm.ma_add(base, Imm32(mir->offset()), out, SetCC);
    masm.ma_b(wasm::JumpTarget::OutOfBounds, Assembler::CarrySet);
}

template <typename T>
void
CodeGeneratorARM::emitWasmStore(T* lir)
{
    const MWasmStore* mir = lir->mir();

    uint32_t offset = mir->offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptr());
    unsigned byteSize = mir->byteSize();
    Scalar::Type type = mir->accessType();

    // Maybe add the offset.
    if (offset || type == Scalar::Int64) {
        Register ptrPlusOffset = ToRegister(lir->ptrCopy());
        if (offset)
            masm.ma_add(Imm32(offset), ptrPlusOffset);
        ptr = ptrPlusOffset;
    } else {
        MOZ_ASSERT(lir->ptrCopy()->isBogusTemp());
    }

    memoryBarrier(mir->barrierBefore());

    if (type == Scalar::Int64) {
        MOZ_ASSERT(INT64LOW_OFFSET == 0);

        Register64 value = ToRegister64(lir->getInt64Operand(lir->ValueIndex));
        masm.ma_dataTransferN(IsStore, 32 /* bits */, /* signed */ false, HeapReg, ptr, value.low);
        masm.ma_add(Imm32(INT64HIGH_OFFSET), ptr);
        masm.ma_dataTransferN(IsStore, 32 /* bits */, /* signed */ true, HeapReg, ptr, value.high);
    } else {
        AnyRegister value = ToAnyRegister(lir->getOperand(lir->ValueIndex));
        if (value.isFloat()) {
            FloatRegister val = value.fpu();
            MOZ_ASSERT((byteSize == 4) == val.isSingle());
            ScratchRegisterScope scratch(masm);
            masm.ma_add(HeapReg, ptr, scratch);
            masm.ma_vstr(val, Address(scratch, 0));
        } else {
            bool isSigned = type == Scalar::Uint32 || type == Scalar::Int32; // see AsmJSStoreHeap;
            Register val = value.gpr();
            masm.ma_dataTransferN(IsStore, 8 * byteSize /* bits */, isSigned, HeapReg, ptr, val);
        }
    }

    memoryBarrier(mir->barrierAfter());
}

void
CodeGeneratorARM::visitWasmStore(LWasmStore* lir)
{
    emitWasmStore(lir);
}

void
CodeGeneratorARM::visitWasmStoreI64(LWasmStoreI64* lir)
{
    emitWasmStore(lir);
}

template<typename T>
void
CodeGeneratorARM::emitWasmUnalignedStore(T* lir)
{
    const MWasmStore* mir = lir->mir();

    uint32_t offset = mir->offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptrCopy());
    if (offset)
        masm.ma_add(Imm32(offset), ptr);

    // Add HeapReg to ptr, so we can use base+index addressing in the byte loads.
    masm.ma_add(HeapReg, ptr);

    MIRType mirType = mir->value()->type();

    memoryBarrier(mir->barrierAfter());

    Register val = ToRegister(lir->valueHelper());
    if (IsFloatingPointType(mirType)) {
        masm.ma_vxfer(ToFloatRegister(lir->getOperand(LWasmUnalignedStore::ValueIndex)), val);
    } else if (mirType == MIRType::Int64) {
        Register64 input = ToRegister64(lir->getInt64Operand(LWasmUnalignedStoreI64::ValueIndex));
        if (input.low != val)
            masm.ma_mov(input.low, val);
    }

    unsigned byteSize = mir->byteSize();
    masm.emitUnalignedStore(Min(byteSize, 4u), ptr, val);

    if (byteSize > 4) {
        // It's a double or an int64 load.
        // Load the high 32 bits when counter == 4.
        if (IsFloatingPointType(mirType)) {
            FloatRegister fp = ToFloatRegister(lir->getOperand(LWasmUnalignedStore::ValueIndex));
            MOZ_ASSERT(fp.isDouble());
            ScratchRegisterScope scratch(masm);
            masm.ma_vxfer(fp, scratch, val);
        } else {
            MOZ_ASSERT(mirType == MIRType::Int64);
            masm.ma_mov(ToRegister64(lir->getInt64Operand(LWasmUnalignedStoreI64::ValueIndex)).high, val);
        }
        masm.emitUnalignedStore(4, ptr, val, /* offset */ 4);
    }

    memoryBarrier(mir->barrierBefore());
}

void
CodeGeneratorARM::visitWasmUnalignedStore(LWasmUnalignedStore* lir)
{
    emitWasmUnalignedStore(lir);
}

void
CodeGeneratorARM::visitWasmUnalignedStoreI64(LWasmUnalignedStoreI64* lir)
{
    emitWasmUnalignedStore(lir);
}

void
CodeGeneratorARM::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    const LAllocation* ptr = ins->ptr();

    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->accessType()) {
      case Scalar::Int8:
      case Scalar::Uint8:   isSigned = false; size = 8; break;
      case Scalar::Int16:
      case Scalar::Uint16:  isSigned = false; size = 16; break;
      case Scalar::Int32:
      case Scalar::Uint32:  isSigned = true;  size = 32; break;
      case Scalar::Float64: isFloat  = true;  size = 64; break;
      case Scalar::Float32: isFloat = true;   size = 32; break;
      default: MOZ_CRASH("unexpected array type");
    }

    if (ptr->isConstant()) {
        MOZ_ASSERT(!mir->needsBoundsCheck());
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);
        if (isFloat) {
            VFPRegister vd(ToFloatRegister(ins->value()));
            Address addr(HeapReg, ptrImm);
            if (size == 32)
                masm.storeFloat32(vd, addr);
            else
                masm.storeDouble(vd, addr);
        } else {
            masm.ma_dataTransferN(IsStore, size, isSigned, HeapReg, Imm32(ptrImm),
                                  ToRegister(ins->value()), Offset, Assembler::Always);
        }
    } else {
        Register ptrReg = ToRegister(ptr);

        Assembler::Condition cond = Assembler::Always;
        if (mir->needsBoundsCheck()) {
            BufferOffset cmp = masm.as_cmp(ptrReg, Imm8(0));
            masm.append(wasm::BoundsCheck(cmp.getOffset()));

            cond = Assembler::Below;
        }

        if (isFloat) {
            FloatRegister value = ToFloatRegister(ins->value());
            if (size == 32)
                value = value.singleOverlay();

            masm.ma_vstr(value, HeapReg, ptrReg, 0, 0, Assembler::Below);
        } else {
            Register value = ToRegister(ins->value());
            masm.ma_dataTransferN(IsStore, size, isSigned, HeapReg, ptrReg, value, Offset, cond);
        }
    }
}

void
CodeGeneratorARM::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins)
{
    MAsmJSCompareExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    Scalar::Type vt = mir->accessType();
    const LAllocation* ptr = ins->ptr();
    Register ptrReg = ToRegister(ptr);
    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne);
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());

    masm.compareExchangeToTypedIntArray(vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        srcAddr, oldval, newval, InvalidReg,
                                        ToAnyRegister(ins->output()));
}

void
CodeGeneratorARM::visitAsmJSCompareExchangeCallout(LAsmJSCompareExchangeCallout* ins)
{
    const MAsmJSCompareExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    Register ptr = ToRegister(ins->ptr());
    Register oldval = ToRegister(ins->oldval());
    Register newval = ToRegister(ins->newval());
    Register tls = ToRegister(ins->tls());
    Register instance = ToRegister(ins->getTemp(0));
    Register viewType = ToRegister(ins->getTemp(1));

    MOZ_ASSERT(ToRegister(ins->output()) == ReturnReg);

    masm.loadPtr(Address(tls, offsetof(wasm::TlsData, instance)), instance);
    masm.ma_mov(Imm32(mir->accessType()), viewType);

    masm.setupAlignedABICall();
    masm.passABIArg(instance);
    masm.passABIArg(viewType);
    masm.passABIArg(ptr);
    masm.passABIArg(oldval);
    masm.passABIArg(newval);
    masm.callWithABI(wasm::SymbolicAddress::AtomicCmpXchg);
}

void
CodeGeneratorARM::visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins)
{
    MAsmJSAtomicExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    Scalar::Type vt = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register value = ToRegister(ins->value());
    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne);
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    masm.atomicExchangeToTypedIntArray(vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                       srcAddr, value, InvalidReg, ToAnyRegister(ins->output()));
}

void
CodeGeneratorARM::visitAsmJSAtomicExchangeCallout(LAsmJSAtomicExchangeCallout* ins)
{
    const MAsmJSAtomicExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    Register ptr = ToRegister(ins->ptr());
    Register value = ToRegister(ins->value());
    Register tls = ToRegister(ins->tls());
    Register instance = ToRegister(ins->getTemp(0));
    Register viewType = ToRegister(ins->getTemp(1));

    MOZ_ASSERT(ToRegister(ins->output()) == ReturnReg);

    masm.loadPtr(Address(tls, offsetof(wasm::TlsData, instance)), instance);
    masm.ma_mov(Imm32(mir->accessType()), viewType);

    masm.setupAlignedABICall();
    masm.passABIArg(instance);
    masm.passABIArg(viewType);
    masm.passABIArg(ptr);
    masm.passABIArg(value);
    masm.callWithABI(wasm::SymbolicAddress::AtomicXchg);
}

void
CodeGeneratorARM::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);
    MOZ_ASSERT(mir->hasUses());

    Scalar::Type vt = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register flagTemp = ToRegister(ins->flagTemp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne);

    if (value->isConstant()) {
        atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                   Imm32(ToInt32(value)), srcAddr, flagTemp, InvalidReg,
                                   ToAnyRegister(ins->output()));
    } else {
        atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                   ToRegister(value), srcAddr, flagTemp, InvalidReg,
                                   ToAnyRegister(ins->output()));
    }
}

void
CodeGeneratorARM::visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);
    MOZ_ASSERT(!mir->hasUses());

    Scalar::Type vt = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register flagTemp = ToRegister(ins->flagTemp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne);

    if (value->isConstant())
        atomicBinopToTypedIntArray(op, vt, Imm32(ToInt32(value)), srcAddr, flagTemp);
    else
        atomicBinopToTypedIntArray(op, vt, ToRegister(value), srcAddr, flagTemp);
}

void
CodeGeneratorARM::visitAsmJSAtomicBinopCallout(LAsmJSAtomicBinopCallout* ins)
{
    const MAsmJSAtomicBinopHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() == 0);

    Register ptr = ToRegister(ins->ptr());
    Register value = ToRegister(ins->value());
    Register tls = ToRegister(ins->tls());
    Register instance = ToRegister(ins->getTemp(0));
    Register viewType = ToRegister(ins->getTemp(1));

    masm.loadPtr(Address(tls, offsetof(wasm::TlsData, instance)), instance);
    masm.move32(Imm32(mir->accessType()), viewType);

    masm.setupAlignedABICall();
    masm.passABIArg(instance);
    masm.passABIArg(viewType);
    masm.passABIArg(ptr);
    masm.passABIArg(value);

    switch (mir->operation()) {
      case AtomicFetchAddOp:
        masm.callWithABI(wasm::SymbolicAddress::AtomicFetchAdd);
        break;
      case AtomicFetchSubOp:
        masm.callWithABI(wasm::SymbolicAddress::AtomicFetchSub);
        break;
      case AtomicFetchAndOp:
        masm.callWithABI(wasm::SymbolicAddress::AtomicFetchAnd);
        break;
      case AtomicFetchOrOp:
        masm.callWithABI(wasm::SymbolicAddress::AtomicFetchOr);
        break;
      case AtomicFetchXorOp:
        masm.callWithABI(wasm::SymbolicAddress::AtomicFetchXor);
        break;
      default:
        MOZ_CRASH("Unknown op");
    }
}

void
CodeGeneratorARM::visitAsmJSPassStackArg(LAsmJSPassStackArg* ins)
{
    const MAsmJSPassStackArg* mir = ins->mir();
    Address dst(StackPointer, mir->spOffset());
    if (ins->arg()->isConstant()) {
        masm.ma_storeImm(Imm32(ToInt32(ins->arg())), dst);
    } else {
        if (ins->arg()->isGeneralReg())
            masm.ma_str(ToRegister(ins->arg()), dst);
        else
            masm.ma_vstr(ToFloatRegister(ins->arg()), dst);
    }
}

void
CodeGeneratorARM::visitUDiv(LUDiv* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    Label done;
    generateUDivModZeroCheck(rhs, output, &done, ins->snapshot(), ins->mir());

    masm.ma_udiv(lhs, rhs, output);

    // Check for large unsigned result - represent as double.
    if (!ins->mir()->isTruncated()) {
        MOZ_ASSERT(ins->mir()->fallible());
        masm.ma_cmp(output, Imm32(0));
        bailoutIf(Assembler::LessThan, ins->snapshot());
    }

    // Check for non-zero remainder if not truncating to int.
    if (!ins->mir()->canTruncateRemainder()) {
        MOZ_ASSERT(ins->mir()->fallible());
        {
            ScratchRegisterScope scratch(masm);
            masm.ma_mul(rhs, output, scratch);
            masm.ma_cmp(scratch, lhs);
        }
        bailoutIf(Assembler::NotEqual, ins->snapshot());
    }

    if (done.used())
        masm.bind(&done);
}

void
CodeGeneratorARM::visitUMod(LUMod* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    Label done;
    generateUDivModZeroCheck(rhs, output, &done, ins->snapshot(), ins->mir());

    masm.ma_umod(lhs, rhs, output);

    // Check for large unsigned result - represent as double.
    if (!ins->mir()->isTruncated()) {
        MOZ_ASSERT(ins->mir()->fallible());
        masm.ma_cmp(output, Imm32(0));
        bailoutIf(Assembler::LessThan, ins->snapshot());
    }

    if (done.used())
        masm.bind(&done);
}

template<class T>
void
CodeGeneratorARM::generateUDivModZeroCheck(Register rhs, Register output, Label* done,
                                           LSnapshot* snapshot, T* mir)
{
    if (!mir)
        return;
    if (mir->canBeDivideByZero()) {
        masm.ma_cmp(rhs, Imm32(0));
        if (mir->isTruncated()) {
            if (mir->trapOnError()) {
                masm.ma_b(wasm::JumpTarget::IntegerDivideByZero, Assembler::Equal);
            } else {
                Label skip;
                masm.ma_b(&skip, Assembler::NotEqual);
                // Infinity|0 == 0
                masm.ma_mov(Imm32(0), output);
                masm.ma_b(done);
                masm.bind(&skip);
            }
        } else {
            // Bailout for divide by zero
            MOZ_ASSERT(mir->fallible());
            bailoutIf(Assembler::Equal, snapshot);
        }
    }
}

void
CodeGeneratorARM::visitSoftUDivOrMod(LSoftUDivOrMod* ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    MOZ_ASSERT(lhs == r0);
    MOZ_ASSERT(rhs == r1);
    MOZ_ASSERT(ins->mirRaw()->isDiv() || ins->mirRaw()->isMod());
    MOZ_ASSERT_IF(ins->mirRaw()->isDiv(), output == r0);
    MOZ_ASSERT_IF(ins->mirRaw()->isMod(), output == r1);

    Label done;
    MDiv* div = ins->mir()->isDiv() ? ins->mir()->toDiv() : nullptr;
    MMod* mod = !div ? ins->mir()->toMod() : nullptr;

    generateUDivModZeroCheck(rhs, output, &done, ins->snapshot(), div);
    generateUDivModZeroCheck(rhs, output, &done, ins->snapshot(), mod);

    masm.setupAlignedABICall();
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    if (gen->compilingAsmJS())
        masm.callWithABI(wasm::SymbolicAddress::aeabi_uidivmod);
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, __aeabi_uidivmod));

    // uidivmod returns the quotient in r0, and the remainder in r1.
    if (div && !div->canTruncateRemainder()) {
        MOZ_ASSERT(div->fallible());
        masm.ma_cmp(r1, Imm32(0));
        bailoutIf(Assembler::NonZero, ins->snapshot());
    }

    // Bailout for big unsigned results
    if ((div && !div->isTruncated()) || (mod && !mod->isTruncated())) {
        DebugOnly<bool> isFallible = (div && div->fallible()) || (mod && mod->fallible());
        MOZ_ASSERT(isFallible);
        masm.ma_cmp(output, Imm32(0));
        bailoutIf(Assembler::LessThan, ins->snapshot());
    }

    masm.bind(&done);
}

void
CodeGeneratorARM::visitEffectiveAddress(LEffectiveAddress* ins)
{
    const MEffectiveAddress* mir = ins->mir();
    Register base = ToRegister(ins->base());
    Register index = ToRegister(ins->index());
    Register output = ToRegister(ins->output());
    masm.as_add(output, base, lsl(index, mir->scale()));
    masm.ma_add(Imm32(mir->displacement()), output);
}

void
CodeGeneratorARM::visitWasmLoadGlobalVar(LWasmLoadGlobalVar* ins)
{
    const MWasmLoadGlobalVar* mir = ins->mir();
    unsigned addr = mir->globalDataOffset() - AsmJSGlobalRegBias;
    if (mir->type() == MIRType::Int32) {
        masm.ma_dtr(IsLoad, GlobalReg, Imm32(addr), ToRegister(ins->output()));
    } else if (mir->type() == MIRType::Float32) {
        VFPRegister vd(ToFloatRegister(ins->output()));
        masm.ma_vldr(Address(GlobalReg, addr), vd.singleOverlay());
    } else {
        MOZ_ASSERT(mir->type() == MIRType::Double);
        masm.ma_vldr(Address(GlobalReg, addr), ToFloatRegister(ins->output()));
    }
}

void
CodeGeneratorARM::visitWasmLoadGlobalVarI64(LWasmLoadGlobalVarI64* ins)
{
    const MWasmLoadGlobalVar* mir = ins->mir();
    unsigned addr = mir->globalDataOffset() - AsmJSGlobalRegBias;
    MOZ_ASSERT(mir->type() == MIRType::Int64);
    Register64 output = ToOutRegister64(ins);

    masm.ma_dtr(IsLoad, GlobalReg, Imm32(addr + INT64LOW_OFFSET), output.low);
    masm.ma_dtr(IsLoad, GlobalReg, Imm32(addr + INT64HIGH_OFFSET), output.high);
}

void
CodeGeneratorARM::visitWasmStoreGlobalVar(LWasmStoreGlobalVar* ins)
{
    const MWasmStoreGlobalVar* mir = ins->mir();
    MIRType type = mir->value()->type();

    unsigned addr = mir->globalDataOffset() - AsmJSGlobalRegBias;
    if (type == MIRType::Int32) {
        masm.ma_dtr(IsStore, GlobalReg, Imm32(addr), ToRegister(ins->value()));
    } else if (type == MIRType::Float32) {
        VFPRegister vd(ToFloatRegister(ins->value()));
        masm.ma_vstr(vd.singleOverlay(), Address(GlobalReg, addr));
    } else {
        MOZ_ASSERT(type == MIRType::Double);
        masm.ma_vstr(ToFloatRegister(ins->value()), Address(GlobalReg, addr));
    }
}

void
CodeGeneratorARM::visitWasmStoreGlobalVarI64(LWasmStoreGlobalVarI64* ins)
{
    const MWasmStoreGlobalVar* mir = ins->mir();
    unsigned addr = mir->globalDataOffset() - AsmJSGlobalRegBias;
    MOZ_ASSERT (mir->value()->type() == MIRType::Int64);
    Register64 input = ToRegister64(ins->value());

    masm.ma_dtr(IsStore, GlobalReg, Imm32(addr + INT64LOW_OFFSET), input.low);
    masm.ma_dtr(IsStore, GlobalReg, Imm32(addr + INT64HIGH_OFFSET), input.high);
}

void
CodeGeneratorARM::visitNegI(LNegI* ins)
{
    Register input = ToRegister(ins->input());
    masm.ma_neg(input, ToRegister(ins->output()));
}

void
CodeGeneratorARM::visitNegD(LNegD* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    masm.ma_vneg(input, ToFloatRegister(ins->output()));
}

void
CodeGeneratorARM::visitNegF(LNegF* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    masm.ma_vneg_f32(input, ToFloatRegister(ins->output()));
}

void
CodeGeneratorARM::memoryBarrier(MemoryBarrierBits barrier)
{
    // On ARMv6 the optional argument (BarrierST, etc) is ignored.
    if (barrier == (MembarStoreStore|MembarSynchronizing))
        masm.ma_dsb(masm.BarrierST);
    else if (barrier & MembarSynchronizing)
        masm.ma_dsb();
    else if (barrier == MembarStoreStore)
        masm.ma_dmb(masm.BarrierST);
    else if (barrier)
        masm.ma_dmb();
}

void
CodeGeneratorARM::visitMemoryBarrier(LMemoryBarrier* ins)
{
    memoryBarrier(ins->type());
}

void
CodeGeneratorARM::setReturnDoubleRegs(LiveRegisterSet* regs)
{
    MOZ_ASSERT(ReturnFloat32Reg.code_ == FloatRegisters::s0);
    MOZ_ASSERT(ReturnDoubleReg.code_ == FloatRegisters::s0);
    FloatRegister s1 = {FloatRegisters::s1, VFPRegister::Single};
    regs->add(ReturnFloat32Reg);
    regs->add(s1);
    regs->add(ReturnDoubleReg);
}

void
CodeGeneratorARM::visitWasmTruncateToInt32(LWasmTruncateToInt32* lir)
{
    auto input = ToFloatRegister(lir->input());
    auto output = ToRegister(lir->output());

    MWasmTruncateToInt32* mir = lir->mir();
    MIRType fromType = mir->input()->type();

    auto* ool = new(alloc()) OutOfLineWasmTruncateCheck(mir, input);
    addOutOfLineCode(ool, mir);

    // vcvt* converts NaN into 0, so check for NaNs here.
    {
        if (fromType == MIRType::Double)
            masm.compareDouble(input, input);
        else if (fromType == MIRType::Float32)
            masm.compareFloat(input, input);
        else
            MOZ_CRASH("unexpected type in visitWasmTruncateToInt32");

        masm.ma_b(ool->entry(), Assembler::VFP_Unordered);
    }

    ScratchDoubleScope scratchScope(masm);
    FloatRegister scratch = scratchScope.uintOverlay();

    // ARM conversion instructions clamp the value to ensure it fits within the
    // target's type bounds, so every time we see those, we need to check the
    // input.
    if (mir->isUnsigned()) {
        if (fromType == MIRType::Double)
            masm.ma_vcvt_F64_U32(input, scratch);
        else if (fromType == MIRType::Float32)
            masm.ma_vcvt_F32_U32(input, scratch);
        else
            MOZ_CRASH("unexpected type in visitWasmTruncateToInt32");

        masm.ma_vxfer(scratch, output);

        // int32_t(UINT32_MAX) == -1.
        masm.ma_cmp(output, Imm32(-1));
        masm.ma_cmp(output, Imm32(0), Assembler::NotEqual);
        masm.ma_b(ool->entry(), Assembler::Equal);

        masm.bind(ool->rejoin());
        return;
    }

    scratch = scratchScope.sintOverlay();

    if (fromType == MIRType::Double)
        masm.ma_vcvt_F64_I32(input, scratch);
    else if (fromType == MIRType::Float32)
        masm.ma_vcvt_F32_I32(input, scratch);
    else
        MOZ_CRASH("unexpected type in visitWasmTruncateToInt32");

    masm.ma_vxfer(scratch, output);
    masm.ma_cmp(output, Imm32(INT32_MAX));
    masm.ma_cmp(output, Imm32(INT32_MIN), Assembler::NotEqual);
    masm.ma_b(ool->entry(), Assembler::Equal);

    masm.bind(ool->rejoin());
}

void
CodeGeneratorARM::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister inputDouble = input;
    Register64 output = ToOutRegister64(lir);

    MWasmTruncateToInt64* mir = lir->mir();
    MIRType fromType = mir->input()->type();

    auto* ool = new(alloc()) OutOfLineWasmTruncateCheck(mir, input);
    addOutOfLineCode(ool, mir);

    ScratchDoubleScope scratchScope(masm);
    if (fromType == MIRType::Float32) {
        inputDouble = ScratchDoubleReg;
        masm.convertFloat32ToDouble(input, inputDouble);
    }

    masm.Push(input);

    masm.setupUnalignedABICall(output.high);
    masm.passABIArg(inputDouble, MoveOp::DOUBLE);
    if (lir->mir()->isUnsigned())
        masm.callWithABI(wasm::SymbolicAddress::TruncateDoubleToUint64);
    else
        masm.callWithABI(wasm::SymbolicAddress::TruncateDoubleToInt64);

    masm.Pop(input);

    masm.ma_cmp(output.high, Imm32(0x80000000));
    masm.ma_cmp(output.low, Imm32(0x00000000), Assembler::Equal);
    masm.ma_b(ool->entry(), Assembler::Equal);

    masm.bind(ool->rejoin());

    MOZ_ASSERT(ReturnReg64 == output);
}

void
CodeGeneratorARM::visitOutOfLineWasmTruncateCheck(OutOfLineWasmTruncateCheck* ool)
{
    MIRType fromType = ool->fromType();
    FloatRegister input = ool->input();

    ScratchDoubleScope scratchScope(masm);
    FloatRegister scratch;

    // Eagerly take care of NaNs.
    Label inputIsNaN;
    if (fromType == MIRType::Double)
        masm.branchDouble(Assembler::DoubleUnordered, input, input, &inputIsNaN);
    else if (fromType == MIRType::Float32)
        masm.branchFloat(Assembler::DoubleUnordered, input, input, &inputIsNaN);
    else
        MOZ_CRASH("unexpected type in visitOutOfLineWasmTruncateCheck");

    // Handle special values.
    Label fail;

    // By default test for the following inputs and bail:
    // signed:   ] -Inf, INTXX_MIN - 1.0 ] and [ INTXX_MAX + 1.0 : +Inf [
    // unsigned: ] -Inf, -1.0 ] and [ UINTXX_MAX + 1.0 : +Inf [
    // Note: we cannot always represent those exact values. As a result
    // this changes the actual comparison a bit.
    double minValue, maxValue;
    Assembler::DoubleCondition minCond = Assembler::DoubleLessThanOrEqual;
    Assembler::DoubleCondition maxCond = Assembler::DoubleGreaterThanOrEqual;
    if (ool->toType() == MIRType::Int64) {
        if (ool->isUnsigned()) {
            minValue = -1;
            maxValue = double(UINT64_MAX) + 1.0;
        } else {
            // In the float32/double range there exists no value between
            // INT64_MIN and INT64_MIN - 1.0. Making INT64_MIN the lower-bound.
            minValue = double(INT64_MIN);
            minCond = Assembler::DoubleLessThan;
            maxValue = double(INT64_MAX) + 1.0;
        }
    } else {
        if (ool->isUnsigned()) {
            minValue = -1;
            maxValue = double(UINT32_MAX) + 1.0;
        } else {
            if (fromType == MIRType::Float32) {
                // In the float32 range there exists no value between
                // INT32_MIN and INT32_MIN - 1.0. Making INT32_MIN the lower-bound.
                minValue = double(INT32_MIN);
                minCond = Assembler::DoubleLessThan;
            } else {
                minValue = double(INT32_MIN) - 1.0;
            }
            maxValue = double(INT32_MAX) + 1.0;
        }
    }

    if (fromType == MIRType::Double) {
        scratch = scratchScope.doubleOverlay();
        masm.loadConstantDouble(minValue, scratch);
        masm.branchDouble(minCond, input, scratch, &fail);

        masm.loadConstantDouble(maxValue, scratch);
        masm.branchDouble(maxCond, input, scratch, &fail);
    } else {
        MOZ_ASSERT(fromType == MIRType::Float32);
        scratch = scratchScope.singleOverlay();
        masm.loadConstantFloat32(float(minValue), scratch);
        masm.branchFloat(minCond, input, scratch, &fail);

        masm.loadConstantFloat32(float(maxValue), scratch);
        masm.branchFloat(maxCond, input, scratch, &fail);
    }

    // We had an actual correct value, get back to where we were.
    masm.ma_b(ool->rejoin());

    // Handle errors.
    masm.bind(&fail);
    masm.jump(wasm::JumpTarget::IntegerOverflow);

    masm.bind(&inputIsNaN);
    masm.jump(wasm::JumpTarget::InvalidConversionToInteger);
}

void
CodeGeneratorARM::visitInt64ToFloatingPointCall(LInt64ToFloatingPointCall* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    MInt64ToFloatingPoint* mir = lir->mir();
    MIRType toType = mir->type();

    // We are free to clobber all registers, since this is a call instruction.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(input.low);
    regs.take(input.high);
    Register temp = regs.takeAny();

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(input.high);
    masm.passABIArg(input.low);
    if (lir->mir()->isUnsigned())
        masm.callWithABI(wasm::SymbolicAddress::Uint64ToFloatingPoint, MoveOp::DOUBLE);
    else
        masm.callWithABI(wasm::SymbolicAddress::Int64ToFloatingPoint, MoveOp::DOUBLE);

    MOZ_ASSERT_IF(toType == MIRType::Double, output == ReturnDoubleReg);
    if (toType == MIRType::Float32) {
        MOZ_ASSERT(output == ReturnFloat32Reg);
        masm.convertDoubleToFloat32(ReturnDoubleReg, output);
    }
}

void
CodeGeneratorARM::visitCopySignF(LCopySignF* ins)
{
    FloatRegister lhs = ToFloatRegister(ins->getOperand(0));
    FloatRegister rhs = ToFloatRegister(ins->getOperand(1));
    FloatRegister output = ToFloatRegister(ins->getDef(0));

    Register lhsi = ToRegister(ins->getTemp(0));
    Register rhsi = ToRegister(ins->getTemp(1));

    masm.ma_vxfer(lhs, lhsi);
    masm.ma_vxfer(rhs, rhsi);

    // Clear lhs's sign.
    masm.ma_and(Imm32(INT32_MAX), lhsi, lhsi);

    // Keep rhs's sign.
    masm.ma_and(Imm32(INT32_MIN), rhsi, rhsi);

    // Combine.
    masm.ma_orr(lhsi, rhsi, rhsi);

    masm.ma_vxfer(rhsi, output);
}

void
CodeGeneratorARM::visitCopySignD(LCopySignD* ins)
{
    FloatRegister lhs = ToFloatRegister(ins->getOperand(0));
    FloatRegister rhs = ToFloatRegister(ins->getOperand(1));
    FloatRegister output = ToFloatRegister(ins->getDef(0));

    Register lhsi = ToRegister(ins->getTemp(0));
    Register rhsi = ToRegister(ins->getTemp(1));

    // Manipulate high words of double inputs.
    masm.as_vxfer(lhsi, InvalidReg, lhs, Assembler::FloatToCore, Assembler::Always, 1);
    masm.as_vxfer(rhsi, InvalidReg, rhs, Assembler::FloatToCore, Assembler::Always, 1);

    // Clear lhs's sign.
    masm.ma_and(Imm32(INT32_MAX), lhsi, lhsi);

    // Keep rhs's sign.
    masm.ma_and(Imm32(INT32_MIN), rhsi, rhsi);

    // Combine.
    masm.ma_orr(lhsi, rhsi, rhsi);

    // Reconstruct the output.
    masm.as_vxfer(lhsi, InvalidReg, lhs, Assembler::FloatToCore, Assembler::Always, 0);
    masm.ma_vxfer(lhsi, rhsi, output);
}

void
CodeGeneratorARM::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir)
{
    const LInt64Allocation& input = lir->getInt64Operand(0);
    Register output = ToRegister(lir->output());

    if (lir->mir()->bottomHalf())
        masm.move32(ToRegister(input.low()), output);
    else
        masm.move32(ToRegister(input.high()), output);
}

void
CodeGeneratorARM::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir)
{
    Register64 output = ToOutRegister64(lir);
    MOZ_ASSERT(ToRegister(lir->input()) == output.low);

    if (lir->mir()->isUnsigned())
        masm.ma_mov(Imm32(0), output.high);
    else
        masm.ma_asr(Imm32(31), output.low, output.high);
}

void
CodeGeneratorARM::visitDivOrModI64(LDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));
    Register64 output = ToOutRegister64(lir);

    MOZ_ASSERT(output == ReturnReg64);

    // All inputs are useAtStart for a call instruction. As a result we cannot
    // ask for a non-aliasing temp. Using the following to get such a temp.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(lhs.low);
    regs.take(lhs.high);
    if (lhs != rhs) {
        regs.take(rhs.low);
        regs.take(rhs.high);
    }
    Register temp = regs.takeAny();

    Label done;

    // Handle divide by zero.
    if (lir->canBeDivideByZero())
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, wasm::JumpTarget::IntegerDivideByZero);

    // Handle an integer overflow exception from INT64_MIN / -1.
    if (lir->canBeNegativeOverflow()) {
        Label notmin;
        masm.branch64(Assembler::NotEqual, lhs, Imm64(INT64_MIN), &notmin);
        masm.branch64(Assembler::NotEqual, rhs, Imm64(-1), &notmin);
        if (lir->mir()->isMod())
            masm.xor64(output, output);
        else
            masm.jump(wasm::JumpTarget::IntegerOverflow);
        masm.jump(&done);
        masm.bind(&notmin);
    }

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingAsmJS());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::ModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::DivI64);

    MOZ_ASSERT(ReturnReg64 == output);

    masm.bind(&done);
}

void
CodeGeneratorARM::visitUDivOrModI64(LUDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));

    MOZ_ASSERT(ToOutRegister64(lir) == ReturnReg64);

    // All inputs are useAtStart for a call instruction. As a result we cannot
    // ask for a non-aliasing temp. Using the following to get such a temp.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(lhs.low);
    regs.take(lhs.high);
    if (lhs != rhs) {
        regs.take(rhs.low);
        regs.take(rhs.high);
    }
    Register temp = regs.takeAny();

    // Prevent divide by zero.
    if (lir->canBeDivideByZero())
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, wasm::JumpTarget::IntegerDivideByZero);

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingAsmJS());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::UModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::UDivI64);
}

void
CodeGeneratorARM::visitCompareI64(LCompareI64* lir)
{
    MCompare* mir = lir->mir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register64 lhsRegs = ToRegister64(lhs);
    Register output = ToRegister(lir->output());

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    Assembler::Condition condition = JSOpToCondition(lir->jsop(), isSigned);
    Label done;

    masm.move32(Imm32(1), output);

    if (IsConstant(rhs)) {
        Imm64 imm = Imm64(ToInt64(rhs));
        masm.branch64(condition, lhsRegs, imm, &done);
    } else {
        Register64 rhsRegs = ToRegister64(rhs);
        masm.branch64(condition, lhsRegs, rhsRegs, &done);
    }

    masm.move32(Imm32(0), output);
    masm.bind(&done);
}

void
CodeGeneratorARM::visitCompareI64AndBranch(LCompareI64AndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register64 lhsRegs = ToRegister64(lhs);

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    Assembler::Condition condition = JSOpToCondition(lir->jsop(), isSigned);

    Label* trueLabel = getJumpLabelForBranch(lir->ifTrue());
    Label* falseLabel = getJumpLabelForBranch(lir->ifFalse());

    if (isNextBlock(lir->ifFalse()->lir())) {
        falseLabel = nullptr;
    } else if (isNextBlock(lir->ifTrue()->lir())) {
        condition = Assembler::InvertCondition(condition);
        trueLabel = falseLabel;
        falseLabel = nullptr;
    }

    if (IsConstant(rhs)) {
        Imm64 imm = Imm64(ToInt64(rhs));
        masm.branch64(condition, lhsRegs, imm, trueLabel, falseLabel);
    } else {
        Register64 rhsRegs = ToRegister64(rhs);
        masm.branch64(condition, lhsRegs, rhsRegs, trueLabel, falseLabel);
    }
}

void
CodeGeneratorARM::visitShiftI64(LShiftI64* lir)
{
    const LInt64Allocation lhs = lir->getInt64Operand(LShiftI64::Lhs);
    LAllocation* rhs = lir->getOperand(LShiftI64::Rhs);

    MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

    if (rhs->isConstant()) {
        int32_t shift = int32_t(rhs->toConstant()->toInt64() & 0x3F);
        switch (lir->bitop()) {
          case JSOP_LSH:
            if (shift)
                masm.lshift64(Imm32(shift), ToRegister64(lhs));
            break;
          case JSOP_RSH:
            if (shift)
                masm.rshift64Arithmetic(Imm32(shift), ToRegister64(lhs));
            break;
          case JSOP_URSH:
            if (shift)
                masm.rshift64(Imm32(shift), ToRegister64(lhs));
            break;
          default:
            MOZ_CRASH("Unexpected shift op");
        }
        return;
    }

    switch (lir->bitop()) {
      case JSOP_LSH:
        masm.lshift64(ToRegister(rhs), ToRegister64(lhs));
        break;
      case JSOP_RSH:
        masm.rshift64Arithmetic(ToRegister(rhs), ToRegister64(lhs));
        break;
      case JSOP_URSH:
        masm.rshift64(ToRegister(rhs), ToRegister64(lhs));
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
}

void
CodeGeneratorARM::visitBitOpI64(LBitOpI64* lir)
{
    const LInt64Allocation lhs = lir->getInt64Operand(LBitOpI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LBitOpI64::Rhs);

    MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

    switch (lir->bitop()) {
      case JSOP_BITOR:
        if (IsConstant(rhs))
            masm.or64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
        else
            masm.or64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
        break;
      case JSOP_BITXOR:
        if (IsConstant(rhs))
            masm.xor64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
        else
            masm.xor64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
        break;
      case JSOP_BITAND:
        if (IsConstant(rhs))
            masm.and64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
        else
            masm.and64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
        break;
      default:
        MOZ_CRASH("unexpected binary opcode");
    }
}

void
CodeGeneratorARM::visitRotateI64(LRotateI64* lir)
{
    MRotate* mir = lir->mir();
    LAllocation* count = lir->count();

    Register64 input = ToRegister64(lir->input());
    Register64 output = ToOutRegister64(lir);
    Register temp = ToTempRegisterOrInvalid(lir->temp());

    if (count->isConstant()) {
        int32_t c = int32_t(count->toConstant()->toInt64() & 0x3F);
        if (!c) {
            masm.move64(input, output);
            return;
        }
        if (mir->isLeftRotate())
            masm.rotateLeft64(Imm32(c), input, output, temp);
        else
            masm.rotateRight64(Imm32(c), input, output, temp);
    } else {
        if (mir->isLeftRotate())
            masm.rotateLeft64(ToRegister(count), input, output, temp);
        else
            masm.rotateRight64(ToRegister(count), input, output, temp);
    }
}

void
CodeGeneratorARM::visitAsmJSPassStackArgI64(LAsmJSPassStackArgI64* ins)
{
    const MAsmJSPassStackArg* mir = ins->mir();
    Address dst(StackPointer, mir->spOffset());
    if (IsConstant(ins->arg()))
        masm.store64(Imm64(ToInt64(ins->arg())), dst);
    else
        masm.store64(ToRegister64(ins->arg()), dst);
}

void
CodeGeneratorARM::visitAsmSelectI64(LAsmSelectI64* lir)
{
    Register cond = ToRegister(lir->condExpr());
    const LInt64Allocation falseExpr = lir->falseExpr();

    Register64 out = ToOutRegister64(lir);
    MOZ_ASSERT(ToRegister64(lir->trueExpr()) == out, "true expr is reused for input");

    masm.ma_cmp(cond, Imm32(0));
    if (falseExpr.low().isRegister()) {
        masm.ma_mov(ToRegister(falseExpr.low()), out.low, LeaveCC, Assembler::Equal);
        masm.ma_mov(ToRegister(falseExpr.high()), out.high, LeaveCC, Assembler::Equal);
    } else {
        masm.ma_ldr(ToAddress(falseExpr.low()), out.low, Offset, Assembler::Equal);
        masm.ma_ldr(ToAddress(falseExpr.high()), out.high, Offset, Assembler::Equal);
    }
}

void
CodeGeneratorARM::visitAsmReinterpretFromI64(LAsmReinterpretFromI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    masm.ma_vxfer(input.low, input.high, output);
}

void
CodeGeneratorARM::visitAsmReinterpretToI64(LAsmReinterpretToI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
    FloatRegister input = ToFloatRegister(lir->getOperand(0));
    Register64 output = ToOutRegister64(lir);

    masm.ma_vxfer(input, output.low, output.high);
}

void
CodeGeneratorARM::visitPopcntI64(LPopcntI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);
    Register temp = ToRegister(lir->getTemp(0));

    masm.popcnt64(input, output, temp);
}

void
CodeGeneratorARM::visitClzI64(LClzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);

    masm.clz64(input, output.low);
    masm.move32(Imm32(0), output.high);
}

void
CodeGeneratorARM::visitCtzI64(LCtzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);

    masm.ctz64(input, output.low);
    masm.move32(Imm32(0), output.high);
}

void
CodeGeneratorARM::visitTestI64AndBranch(LTestI64AndBranch* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));

    masm.ma_cmp(input.high, Imm32(0));
    jumpToBlock(lir->ifTrue(), Assembler::NonZero);
    masm.ma_cmp(input.low, Imm32(0));
    emitBranch(Assembler::NonZero, lir->ifTrue(), lir->ifFalse());
}
