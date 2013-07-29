/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/arm/CodeGenerator-arm.h"

#include "mozilla/MathAlgorithms.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"

#include "ion/CodeGenerator.h"
#include "ion/IonCompartment.h"
#include "ion/IonFrames.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/PerfSpewer.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"

#include "ion/shared/CodeGenerator-shared-inl.h"
#include "vm/Shape-inl.h"

using namespace js;
using namespace js::ion;

using mozilla::FloorLog2;

// shared
CodeGeneratorARM::CodeGeneratorARM(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

bool
CodeGeneratorARM::generatePrologue()
{
    if (gen->compilingAsmJS()) {
        masm.Push(lr);
        // Note that this automatically sets MacroAssembler::framePushed().
        masm.reserveStack(frameDepth_);
    } else {
        // Note that this automatically sets MacroAssembler::framePushed().
        masm.reserveStack(frameSize());
        masm.checkStackAlignment();
    }

    return true;
}

bool
CodeGeneratorARM::generateEpilogue()
{
    masm.bind(&returnLabel_); 
#if JS_TRACE_LOGGING
    masm.tracelogStop();
#endif
    if (gen->compilingAsmJS()) {
        // Pop the stack we allocated at the start of the function.
        masm.freeStack(frameDepth_);
        masm.Pop(pc);
        JS_ASSERT(masm.framePushed() == 0);
        //masm.as_bkpt();
    } else {
        // Pop the stack we allocated at the start of the function.
        masm.freeStack(frameSize());
        JS_ASSERT(masm.framePushed() == 0);
        masm.ma_pop(pc);
    }
    masm.dumpPool();
    return true;
}

void
CodeGeneratorARM::emitBranch(Assembler::Condition cond, MBasicBlock *mirTrue, MBasicBlock *mirFalse)
{
    LBlock *ifTrue = mirTrue->lir();
    LBlock *ifFalse = mirFalse->lir();
    if (isNextBlock(ifFalse)) {
        masm.ma_b(ifTrue->label(), cond);
    } else {
        masm.ma_b(ifFalse->label(), Assembler::InvertCondition(cond));
        if (!isNextBlock(ifTrue)) {
            masm.ma_b(ifTrue->label());
        }
    }
}


bool
OutOfLineBailout::accept(CodeGeneratorARM *codegen)
{
    return codegen->visitOutOfLineBailout(this);
}

bool
CodeGeneratorARM::visitTestIAndBranch(LTestIAndBranch *test)
{
    const LAllocation *opd = test->getOperand(0);
    LBlock *ifTrue = test->ifTrue()->lir();
    LBlock *ifFalse = test->ifFalse()->lir();

    // Test the operand
    masm.ma_cmp(ToRegister(opd), Imm32(0));

    if (isNextBlock(ifFalse)) {
        masm.ma_b(ifTrue->label(), Assembler::NonZero);
    } else if (isNextBlock(ifTrue)) {
        masm.ma_b(ifFalse->label(), Assembler::Zero);
    } else {
        masm.ma_b(ifFalse->label(), Assembler::Zero);
        masm.ma_b(ifTrue->label());
    }
    return true;
}

bool
CodeGeneratorARM::visitCompare(LCompare *comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->mir()->compareType(), comp->jsop());
    const LAllocation *left = comp->getOperand(0);
    const LAllocation *right = comp->getOperand(1);
    const LDefinition *def = comp->getDef(0);

    if (right->isConstant())
        masm.ma_cmp(ToRegister(left), Imm32(ToInt32(right)));
    else
        masm.ma_cmp(ToRegister(left), ToOperand(right));
    masm.ma_mov(Imm32(0), ToRegister(def));
    masm.ma_mov(Imm32(1), ToRegister(def), NoSetCond, cond);
    return true;
}

bool
CodeGeneratorARM::visitCompareAndBranch(LCompareAndBranch *comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->mir()->compareType(), comp->jsop());
    if (comp->right()->isConstant())
        masm.ma_cmp(ToRegister(comp->left()), Imm32(ToInt32(comp->right())));
    else
        masm.ma_cmp(ToRegister(comp->left()), ToOperand(comp->right()));
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;

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

        IonCompartment *ion = GetIonContext()->compartment->ionCompartment();
        IonCode *handler = ion->getGenericBailoutHandler();

        masm.branch(handler);
    }

    return true;
}

bool
CodeGeneratorARM::bailoutIf(Assembler::Condition condition, LSnapshot *snapshot)
{
    if (!encode(snapshot))
        return false;

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    JS_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                 frameClass_.frameSize() == masm.framePushed());

    if (assignBailoutId(snapshot)) {
        uint8_t *code = deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE;
        masm.ma_b(code, Relocation::HARDCODED, condition);
        return true;
    }

    // We could not use a jump table, either because all bailout IDs were
    // reserved, or a jump table is not optimal for this frame size or
    // platform. Whatever, we will generate a lazy bailout.
    OutOfLineBailout *ool = new OutOfLineBailout(snapshot, masm.framePushed());
    if (!addOutOfLineCode(ool))
        return false;

    masm.ma_b(ool->entry(), condition);

    return true;
}
bool
CodeGeneratorARM::bailoutFrom(Label *label, LSnapshot *snapshot)
{
    JS_ASSERT(label->used());
    JS_ASSERT(!label->bound());

    CompileInfo &info = snapshot->mir()->block()->info();
    switch (info.executionMode()) {
      case ParallelExecution: {
        // in parallel mode, make no attempt to recover, just signal an error.
        OutOfLineAbortPar *ool = oolAbortPar(ParallelBailoutUnsupported,
                                             snapshot->mir()->block(),
                                             snapshot->mir()->pc());
        masm.retarget(label, ool->entry());
        return true;
      }
      case SequentialExecution:
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    if (!encode(snapshot))
        return false;

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    JS_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                 frameClass_.frameSize() == masm.framePushed());
    // This involves retargeting a label, which I've declared is always going
    // to be a pc-relative branch to an absolute address!
    // With no assurance that this is going to be a local branch, I am wary to
    // implement this.  Moreover, If it isn't a local branch, it will be large
    // and possibly slow.  I believe that the correct way to handle this is to
    // subclass label into a fatlabel, where we generate enough room for a load
    // before the branch
#if 0
    if (assignBailoutId(snapshot)) {
        uint8_t *code = deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE;
        masm.retarget(label, code, Relocation::HARDCODED);
        return true;
    }
#endif
    // We could not use a jump table, either because all bailout IDs were
    // reserved, or a jump table is not optimal for this frame size or
    // platform. Whatever, we will generate a lazy bailout.
    OutOfLineBailout *ool = new OutOfLineBailout(snapshot, masm.framePushed());
    if (!addOutOfLineCode(ool)) {
        return false;
    }

    masm.retarget(label, ool->entry());

    return true;
}

bool
CodeGeneratorARM::bailout(LSnapshot *snapshot)
{
    Label label;
    masm.ma_b(&label);
    return bailoutFrom(&label, snapshot);
}

bool
CodeGeneratorARM::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    masm.ma_mov(Imm32(ool->snapshot()->snapshotOffset()), ScratchRegister);
    masm.ma_push(ScratchRegister);
    masm.ma_push(ScratchRegister);
    masm.ma_b(&deoptLabel_);
    return true;
}

bool
CodeGeneratorARM::visitMinMaxD(LMinMaxD *ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());
    FloatRegister output = ToFloatRegister(ins->output());

    JS_ASSERT(first == output);

    Assembler::Condition cond = ins->mir()->isMax()
        ? Assembler::VFP_LessThanOrEqual
        : Assembler::VFP_GreaterThanOrEqual;
    Label nan, equal, returnSecond, done;

    masm.compareDouble(first, second);
    masm.ma_b(&nan, Assembler::VFP_Unordered); // first or second is NaN, result is NaN.
    masm.ma_b(&equal, Assembler::VFP_Equal); // make sure we handle -0 and 0 right.
    masm.ma_b(&returnSecond, cond);
    masm.ma_b(&done);

    // Check for zero.
    masm.bind(&equal);
    masm.compareDouble(first, InvalidFloatReg);
    masm.ma_b(&done, Assembler::VFP_NotEqualOrUnordered); // first wasn't 0 or -0, so just return it.
    // So now both operands are either -0 or 0.
    if (ins->mir()->isMax()) {
        masm.ma_vadd(second, first, first); // -0 + -0 = -0 and -0 + 0 = 0.
    } else {
        masm.ma_vneg(first, first);
        masm.ma_vsub(first, second, first);
        masm.ma_vneg(first, first);
    }
    masm.ma_b(&done);

    masm.bind(&nan);
    masm.loadStaticDouble(&js_NaN, output);
    masm.ma_b(&done);

    masm.bind(&returnSecond);
    masm.ma_vmov(second, output);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorARM::visitAbsD(LAbsD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));
    masm.ma_vabs(input, input);
    return true;
}

bool
CodeGeneratorARM::visitSqrtD(LSqrtD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));
    masm.ma_vsqrt(input, input);
    return true;
}

bool
CodeGeneratorARM::visitAddI(LAddI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);

    if (rhs->isConstant())
        masm.ma_add(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCond);
    else
        masm.ma_add(ToRegister(lhs), ToOperand(rhs), ToRegister(dest), SetCond);

    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;

    return true;
}

bool
CodeGeneratorARM::visitSubI(LSubI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);

    if (rhs->isConstant())
        masm.ma_sub(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCond);
    else
        masm.ma_sub(ToRegister(lhs), ToOperand(rhs), ToRegister(dest), SetCond);

    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorARM::visitMulI(LMulI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);
    MMul *mul = ins->mir();
    JS_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    if (rhs->isConstant()) {
        // Bailout when this condition is met.
        Assembler::Condition c = Assembler::Overflow;
        // Bailout on -0.0
        int32_t constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            masm.ma_cmp(ToRegister(lhs), Imm32(0));
            if (!bailoutIf(bailoutCond, ins->snapshot()))
                return false;
        }
        // TODO: move these to ma_mul.
        switch (constant) {
          case -1:
            masm.ma_rsb(ToRegister(lhs), Imm32(0), ToRegister(dest), SetCond);
            break;
          case 0:
            masm.ma_mov(Imm32(0), ToRegister(dest));
            return true; // escape overflow check;
          case 1:
            // nop
            masm.ma_mov(ToRegister(lhs), ToRegister(dest));
            return true; // escape overflow check;
          case 2:
            masm.ma_add(ToRegister(lhs), ToRegister(lhs), ToRegister(dest), SetCond);
            // Overflow is handled later.
            break;
          default: {
            bool handled = false;
            if (!mul->canOverflow()) {
                // If it cannot overflow, we can do lots of optimizations
                Register src = ToRegister(lhs);
                uint32_t shift = FloorLog2(constant);
                uint32_t rest = constant - (1 << shift);
                // See if the constant has one bit set, meaning it can be encoded as a bitshift
                if ((1 << shift) == constant) {
                    masm.ma_lsl(Imm32(shift), src, ToRegister(dest));
                    handled = true;
                } else {
                    // If the constant cannot be encoded as (1<<C1), see if it can be encoded as
                    // (1<<C1) | (1<<C2), which can be computed using an add and a shift
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
                    // At runtime, check (lhs == dest >> shift), if this does not hold,
                    // some bits were lost due to overflow, and the computation should
                    // be resumed as a double.
                    masm.as_cmp(ToRegister(lhs), asr(ToRegister(dest), shift));
                    c = Assembler::NotEqual;
                    handled = true;
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
        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(c, ins->snapshot()))
            return false;
    } else {
        Assembler::Condition c = Assembler::Overflow;

        //masm.imull(ToOperand(rhs), ToRegister(lhs));
        if (mul->canOverflow())
            c = masm.ma_check_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest), c);
        else
            masm.ma_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest));

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(c, ins->snapshot()))
            return false;

        if (mul->canBeNegativeZero()) {
            Label done;
            masm.ma_cmp(ToRegister(dest), Imm32(0));
            masm.ma_b(&done, Assembler::NotEqual);

            // Result is -0 if lhs or rhs is negative.
            masm.ma_cmn(ToRegister(lhs), ToRegister(rhs));
            if (!bailoutIf(Assembler::Signed, ins->snapshot()))
                return false;

            masm.bind(&done);
        }
    }

    return true;
}

bool
CodeGeneratorARM::divICommon(MDiv *mir, Register lhs, Register rhs, Register output,
                             LSnapshot *snapshot, Label &done)
{
    if (mir->canBeNegativeOverflow()) {
        // Handle INT32_MIN / -1;
        // The integer division will give INT32_MIN, but we want -(double)INT32_MIN.
        masm.ma_cmp(lhs, Imm32(INT32_MIN)); // sets EQ if lhs == INT32_MIN
        masm.ma_cmp(rhs, Imm32(-1), Assembler::Equal); // if EQ (LHS == INT32_MIN), sets EQ if rhs == -1
        if (mir->isTruncated()) {
            // (-INT32_MIN)|0 = INT32_MIN
            Label skip;
            masm.ma_b(&skip, Assembler::NotEqual);
            masm.ma_mov(Imm32(INT32_MIN), output);
            masm.ma_b(&done);
            masm.bind(&skip);
        } else {
            JS_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Equal, snapshot))
                return false;
        }
    }

    // 0/X (with X < 0) is bad because both of these values *should* be doubles, and
    // the result should be -0.0, which cannot be represented in integers.
    // X/0 is bad because it will give garbage (or abort), when it should give
    // either \infty, -\infty or NAN.

    // Prevent 0 / X (with X < 0) and X / 0
    // testing X / Y.  Compare Y with 0.
    // There are three cases: (Y < 0), (Y == 0) and (Y > 0)
    // If (Y < 0), then we compare X with 0, and bail if X == 0
    // If (Y == 0), then we simply want to bail.  Since this does not set
    // the flags necessary for LT to trigger, we don't test X, and take the
    // bailout because the EQ flag is set.
    // if (Y > 0), we don't set EQ, and we don't trigger LT, so we don't take the bailout.
    if (mir->canBeDivideByZero() || mir->canBeNegativeZero()) {
        masm.ma_cmp(rhs, Imm32(0));
        masm.ma_cmp(lhs, Imm32(0), Assembler::LessThan);
        if (mir->isTruncated()) {
            // Infinity|0 == 0 and -0|0 == 0
            Label skip;
            masm.ma_b(&skip, Assembler::NotEqual);
            masm.ma_mov(Imm32(0), output);
            masm.ma_b(&done);
            masm.bind(&skip);
        } else {
            JS_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Equal, snapshot))
                return false;
        }
    }

    return true;
}

bool
CodeGeneratorARM::visitDivI(LDivI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register temp = ToRegister(ins->getTemp(0));
    Register output = ToRegister(ins->output());
    MDiv *mir = ins->mir();

    Label done;
    if (!divICommon(mir, lhs, rhs, output, ins->snapshot(), done))
        return false;

    if (mir->isTruncated()) {
        masm.ma_sdiv(lhs, rhs, output);
    } else {
        masm.ma_sdiv(lhs, rhs, ScratchRegister);
        masm.ma_mul(ScratchRegister, rhs, temp);
        masm.ma_cmp(lhs, temp);
        if (!bailoutIf(Assembler::NotEqual, ins->snapshot()))
            return false;
        masm.ma_mov(ScratchRegister, output);
    }

    masm.bind(&done);

    return true;
}

extern "C" {
    extern int __aeabi_idivmod(int,int);
    extern int __aeabi_uidivmod(int,int);
}

bool
CodeGeneratorARM::visitSoftDivI(LSoftDivI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    MDiv *mir = ins->mir();

    Label done;
    if (!divICommon(mir, lhs, rhs, output, ins->snapshot(), done))
        return false;

    masm.setupAlignedABICall(2);
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, __aeabi_idivmod));
    // idivmod returns the quotient in r0, and the remainder in r1.
    if (!mir->isTruncated()) {
        JS_ASSERT(mir->fallible());
        masm.ma_cmp(r1, Imm32(0));
        if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
            return false;
    }

    masm.bind(&done);

    return true;
}

bool
CodeGeneratorARM::visitDivPowTwoI(LDivPowTwoI *ins)
{
    Register lhs = ToRegister(ins->numerator());
    Register output = ToRegister(ins->output());
    int32_t shift = ins->shift();

    if (shift != 0) {
        if (!ins->mir()->isTruncated()) {
            // If the remainder is != 0, bailout since this must be a double.
            masm.as_mov(ScratchRegister, lsl(lhs, 32 - shift), SetCond);
            if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
                return false;
        }

        // Adjust the value so that shifting produces a correctly rounded result
        // when the numerator is negative. See 10-1 "Signed Division by a Known
        // Power of 2" in Henry S. Warren, Jr.'s Hacker's Delight.
        // Note that we wouldn't need to do this adjustment if we could use
        // Range Analysis to find cases when the value is never negative.
        if (shift > 1) {
            masm.as_mov(ScratchRegister, asr(lhs, 31));
            masm.as_add(ScratchRegister, lhs, lsr(ScratchRegister, 32 - shift));
        } else
            masm.as_add(ScratchRegister, lhs, lsr(lhs, 32 - shift));

        // Do the shift.
        masm.as_mov(output, asr(ScratchRegister, shift));
    } else {
        masm.ma_mov(lhs, output);
    }

    return true;
}

bool
CodeGeneratorARM::modICommon(MMod *mir, Register lhs, Register rhs, Register output,
                             LSnapshot *snapshot, Label &done)
{
    // 0/X (with X < 0) is bad because both of these values *should* be doubles, and
    // the result should be -0.0, which cannot be represented in integers.
    // X/0 is bad because it will give garbage (or abort), when it should give
    // either \infty, -\infty or NAN.

    // Prevent 0 / X (with X < 0) and X / 0
    // testing X / Y.  Compare Y with 0.
    // There are three cases: (Y < 0), (Y == 0) and (Y > 0)
    // If (Y < 0), then we compare X with 0, and bail if X == 0
    // If (Y == 0), then we simply want to bail.  Since this does not set
    // the flags necessary for LT to trigger, we don't test X, and take the
    // bailout because the EQ flag is set.
    // if (Y > 0), we don't set EQ, and we don't trigger LT, so we don't take the bailout.
    masm.ma_cmp(rhs, Imm32(0));
    masm.ma_cmp(lhs, Imm32(0), Assembler::LessThan);
    if (mir->isTruncated()) {
        // NaN|0 == 0 and (0 % -X)|0 == 0
        Label skip;
        masm.ma_b(&skip, Assembler::NotEqual);
        masm.ma_mov(Imm32(0), output);
        masm.ma_b(&done);
        masm.bind(&skip);
    } else {
        JS_ASSERT(mir->fallible());
        if (!bailoutIf(Assembler::Equal, snapshot))
            return false;
    }

    return true;
}

bool
CodeGeneratorARM::visitModI(LModI *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Register callTemp = ToRegister(ins->getTemp(0));
    MMod *mir = ins->mir();

    // save the lhs in case we end up with a 0 that should be a -0.0 because lhs < 0.
    masm.ma_mov(lhs, callTemp);

    Label done;
    if (!modICommon(mir, lhs, rhs, output, ins->snapshot(), done))
        return false;

    masm.ma_smod(lhs, rhs, output);

    // If X%Y == 0 and X < 0, then we *actually* wanted to return -0.0
    if (mir->isTruncated()) {
        // -0.0|0 == 0
    } else {
        JS_ASSERT(mir->fallible());
        // See if X < 0
        masm.ma_cmp(output, Imm32(0));
        masm.ma_b(&done, Assembler::NotEqual);
        masm.ma_cmp(callTemp, Imm32(0));
        if (!bailoutIf(Assembler::Signed, ins->snapshot()))
            return false;
    }

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorARM::visitSoftModI(LSoftModI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Register callTemp = ToRegister(ins->getTemp(2));
    MMod *mir = ins->mir();
    Label done;
    // save the lhs in case we end up with a 0 that should be a -0.0 because lhs < 0.
    JS_ASSERT(callTemp.code() > r3.code() && callTemp.code() < r12.code());
    masm.ma_mov(lhs, callTemp);

    // Prevent INT_MIN % -1;
    // The integer division will give INT_MIN, but we want -(double)INT_MIN.
    if (mir->canBeNegativeDividend()) {
        masm.ma_cmp(lhs, Imm32(INT_MIN)); // sets EQ if lhs == INT_MIN
        masm.ma_cmp(rhs, Imm32(-1), Assembler::Equal); // if EQ (LHS == INT_MIN), sets EQ if rhs == -1
        if (mir->isTruncated()) {
            // (INT_MIN % -1)|0 == 0
            Label skip;
            masm.ma_b(&skip, Assembler::NotEqual);
            masm.ma_mov(Imm32(0), output);
            masm.ma_b(&done);
            masm.bind(&skip);
        } else {
            JS_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Equal, ins->snapshot()))
                return false;
        }
    }

    if (!modICommon(mir, lhs, rhs, output, ins->snapshot(), done))
        return false;

    masm.setupAlignedABICall(2);
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, __aeabi_idivmod));

    // If X%Y == 0 and X < 0, then we *actually* wanted to return -0.0
    if (mir->isTruncated()) {
        // -0.0|0 == 0
    } else {
        JS_ASSERT(mir->fallible());
        // See if X < 0
        masm.ma_cmp(r1, Imm32(0));
        masm.ma_b(&done, Assembler::NotEqual);
        masm.ma_cmp(callTemp, Imm32(0));
        if (!bailoutIf(Assembler::Signed, ins->snapshot()))
            return false;
    }
    masm.bind(&done);
    return true;
}

bool
CodeGeneratorARM::visitModPowTwoI(LModPowTwoI *ins)
{
    Register in = ToRegister(ins->getOperand(0));
    Register out = ToRegister(ins->getDef(0));
    MMod *mir = ins->mir();
    Label fin;
    // bug 739870, jbramley has a different sequence that may help with speed here
    masm.ma_mov(in, out, SetCond);
    masm.ma_b(&fin, Assembler::Zero);
    masm.ma_rsb(Imm32(0), out, NoSetCond, Assembler::Signed);
    masm.ma_and(Imm32((1<<ins->shift())-1), out);
    masm.ma_rsb(Imm32(0), out, SetCond, Assembler::Signed);
    if (!mir->isTruncated()) {
        JS_ASSERT(mir->fallible());
        if (!bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
    } else {
        // -0|0 == 0
    }
    masm.bind(&fin);
    return true;
}

bool
CodeGeneratorARM::visitModMaskI(LModMaskI *ins)
{
    Register src = ToRegister(ins->getOperand(0));
    Register dest = ToRegister(ins->getDef(0));
    Register tmp = ToRegister(ins->getTemp(0));
    MMod *mir = ins->mir();
    masm.ma_mod_mask(src, dest, tmp, ins->shift());
    if (!mir->isTruncated()) {
        JS_ASSERT(mir->fallible());
        if (!bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
    } else {
        // -0|0 == 0
    }
    return true;
}
bool
CodeGeneratorARM::visitBitNotI(LBitNotI *ins)
{
    const LAllocation *input = ins->getOperand(0);
    const LDefinition *dest = ins->getDef(0);
    // this will not actually be true on arm.
    // We can not an imm8m in order to get a wider range
    // of numbers
    JS_ASSERT(!input->isConstant());

    masm.ma_mvn(ToRegister(input), ToRegister(dest));
    return true;
}

bool
CodeGeneratorARM::visitBitOpI(LBitOpI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);
    // all of these bitops should be either imm32's, or integer registers.
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
        MOZ_ASSUME_UNREACHABLE("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorARM::visitShiftI(LShiftI *ins)
{
    Register lhs = ToRegister(ins->lhs());
    const LAllocation *rhs = ins->rhs();
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
                if (ins->mir()->toUrsh()->canOverflow()) {
                    masm.ma_cmp(dest, Imm32(0));
                    if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
                        return false;
                }
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    } else {
        // The shift amounts should be AND'ed into the 0-31 range since arm
        // shifts by the lower byte of the register (it will attempt to shift
        // by 250 if you ask it to).
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
            if (ins->mir()->toUrsh()->canOverflow()) {
                // x >>> 0 can overflow.
                masm.ma_cmp(dest, Imm32(0));
                if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    }

    return true;
}

bool
CodeGeneratorARM::visitUrshD(LUrshD *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register temp = ToRegister(ins->temp());

    const LAllocation *rhs = ins->rhs();
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
    return true;
}

bool
CodeGeneratorARM::visitPowHalfD(LPowHalfD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());

    Label done;

    // Masm.pow(-Infinity, 0.5) == Infinity.
    masm.ma_vimm(js_NegativeInfinity, ScratchFloatReg);
    masm.compareDouble(input, ScratchFloatReg);
    masm.ma_vneg(ScratchFloatReg, output, Assembler::Equal);
    masm.ma_b(&done, Assembler::Equal);

    // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5). Adding 0 converts any -0 to 0.
    masm.ma_vimm(0.0, ScratchFloatReg);
    masm.ma_vadd(ScratchFloatReg, input, output);
    masm.ma_vsqrt(output, output);

    masm.bind(&done);
    return true;
}

typedef MoveResolver::MoveOperand MoveOperand;

MoveOperand
CodeGeneratorARM::toMoveOperand(const LAllocation *a) const
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    JS_ASSERT((ToStackOffset(a) & 3) == 0);
    int32_t offset = ToStackOffset(a);
    
    // The way the stack slots work, we assume that everything from depth == 0 downwards is writable
    // however, since our frame is included in this, ensure that the frame gets skipped
    if (gen->compilingAsmJS())
        offset -= AlignmentMidPrologue;

    return MoveOperand(StackPointer, offset);
}

class js::ion::OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorARM>
{
    MTableSwitch *mir_;
    Vector<CodeLabel, 8, IonAllocPolicy> codeLabels_;

    bool accept(CodeGeneratorARM *codegen) {
        return codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(MTableSwitch *mir)
      : mir_(mir)
    {}

    MTableSwitch *mir() const {
        return mir_;
    }

    bool addCodeLabel(CodeLabel label) {
        return codeLabels_.append(label);
    }
    CodeLabel codeLabel(unsigned i) {
        return codeLabels_[i];
    }
};

bool
CodeGeneratorARM::visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool)
{
    MTableSwitch *mir = ool->mir();

    int numCases = mir->numCases();
    for (size_t i = 0; i < numCases; i++) {
        LBlock *caseblock = mir->getCase(numCases - 1 - i)->lir();
        Label *caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl = ool->codeLabel(i);
        cl.src()->bind(caseoffset);
        if (!masm.addCodeLabel(cl))
            return false;
    }

    return true;
}

bool
CodeGeneratorARM::emitTableSwitchDispatch(MTableSwitch *mir, const Register &index,
                                          const Register &base)
{
    // the code generated by this is utter hax.
    // the end result looks something like:
    // SUBS index, input, #base
    // RSBSPL index, index, #max
    // LDRPL pc, pc, index lsl 2
    // B default

    // If the range of targets in N through M, we first subtract off the lowest
    // case (N), which both shifts the arguments into the range 0 to (M-N) with
    // and sets the MInus flag if the argument was out of range on the low end.

    // Then we a reverse subtract with the size of the jump table, which will
    // reverse the order of range (It is size through 0, rather than 0 through
    // size).  The main purpose of this is that we set the same flag as the lower
    // bound check for the upper bound check.  Lastly, we do this conditionally
    // on the previous check succeeding.

    // Then we conditionally load the pc offset by the (reversed) index (times
    // the address size) into the pc, which branches to the correct case.
    // NOTE: when we go to read the pc, the value that we get back is the pc of
    // the current instruction *PLUS 8*.  This means that ldr foo, [pc, +0]
    // reads $pc+8.  In other words, there is an empty word after the branch into
    // the switch table before the table actually starts.  Since the only other
    // unhandled case is the default case (both out of range high and out of range low)
    // I then insert a branch to default case into the extra slot, which ensures
    // we don't attempt to execute the address table.
    Label *defaultcase = mir->getDefault()->lir()->label();

    int32_t cases = mir->numCases();
    // Lower value with low value
    masm.ma_sub(index, Imm32(mir->low()), index, SetCond);
    masm.ma_rsb(index, Imm32(cases - 1), index, SetCond, Assembler::Unsigned);
    AutoForbidPools afp(&masm);
    masm.ma_ldr(DTRAddr(pc, DtrRegImmShift(index, LSL, 2)), pc, Offset, Assembler::Unsigned);
    masm.ma_b(defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first
    // generate the case entries (we don't yet know their offsets in the
    // instruction stream).
    OutOfLineTableSwitch *ool = new OutOfLineTableSwitch(mir);
    for (int32_t i = 0; i < cases; i++) {
        CodeLabel cl;
        masm.writeCodePointer(cl.dest());
        if (!ool->addCodeLabel(cl))
            return false;
    }
    if (!addOutOfLineCode(ool))
        return false;

    return true;
}

bool
CodeGeneratorARM::visitMathD(LMathD *math)
{
    const LAllocation *src1 = math->getOperand(0);
    const LAllocation *src2 = math->getOperand(1);
    const LDefinition *output = math->getDef(0);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.ma_vadd(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
        break;
      case JSOP_SUB:
        masm.ma_vsub(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
        break;
      case JSOP_MUL:
        masm.ma_vmul(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
        break;
      case JSOP_DIV:
        masm.ma_vdiv(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorARM::visitFloor(LFloor *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    Label bail;
    masm.floor(input, output, &bail);
    if (!bailoutFrom(&bail, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorARM::visitRound(LRound *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    FloatRegister tmp = ToFloatRegister(lir->temp());
    Label bail;
    // Output is either correct, or clamped.  All -0 cases have been translated to a clamped
    // case.a
    masm.round(input, output, &bail, tmp);
    if (!bailoutFrom(&bail, lir->snapshot()))
        return false;
    return true;
}

void
CodeGeneratorARM::emitRoundDouble(const FloatRegister &src, const Register &dest, Label *fail)
{
    masm.ma_vcvt_F64_I32(src, ScratchFloatReg);
    masm.ma_vxfer(ScratchFloatReg, dest);
    masm.ma_cmp(dest, Imm32(0x7fffffff));
    masm.ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    masm.ma_b(fail, Assembler::Equal);
}

bool
CodeGeneratorARM::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    return emitTruncateDouble(ToFloatRegister(ins->input()), ToRegister(ins->output()));
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
    JS_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    JS_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

    return FrameSizes[class_];
}

ValueOperand
CodeGeneratorARM::ToValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorARM::ToOutValue(LInstruction *ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorARM::ToTempValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

bool
CodeGeneratorARM::visitValue(LValue *value)
{
    const ValueOperand out = ToOutValue(value);

    masm.moveValue(value->value(), out);
    return true;
}

bool
CodeGeneratorARM::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame   = value->getOperand(0);
    const ValueOperand out     = ToOutValue(value);

    const ptrdiff_t frameOffset = value->mir()->frameOffset();

    masm.loadValue(Address(ToRegister(frame), frameOffset), out);
    return true;
}

bool
CodeGeneratorARM::visitBox(LBox *box)
{
    const LDefinition *type = box->getDef(TYPE_INDEX);

    JS_ASSERT(!box->getOperand(0)->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.ma_mov(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
    return true;
}

bool
CodeGeneratorARM::visitBoxDouble(LBoxDouble *box)
{
    const LDefinition *payload = box->getDef(PAYLOAD_INDEX);
    const LDefinition *type = box->getDef(TYPE_INDEX);
    const LAllocation *in = box->getOperand(0);

    //masm.as_vxfer(ToRegister(payload), ToRegister(type),
    //              VFPRegister(ToFloatRegister(in)), Assembler::FloatToCore);
    masm.ma_vxfer(VFPRegister(ToFloatRegister(in)), ToRegister(payload), ToRegister(type));
    return true;
}

bool
CodeGeneratorARM::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();
    Register type = ToRegister(unbox->type());

    if (mir->fallible()) {
        masm.ma_cmp(type, Imm32(MIRTypeToTag(mir->type())));
        if (!bailoutIf(Assembler::NotEqual, unbox->snapshot()))
            return false;
    }
    return true;
}

bool
CodeGeneratorARM::visitDouble(LDouble *ins)
{

    const LDefinition *out = ins->getDef(0);

    masm.ma_vimm(ins->getDouble(), ToFloatRegister(out));
    return true;
}

Register
CodeGeneratorARM::splitTagForTest(const ValueOperand &value)
{
    return value.typeReg();
}

bool
CodeGeneratorARM::visitTestDAndBranch(LTestDAndBranch *test)
{
    const LAllocation *opd = test->input();
    masm.ma_vcmpz(ToFloatRegister(opd));
    masm.as_vmrs(pc);

    LBlock *ifTrue = test->ifTrue()->lir();
    LBlock *ifFalse = test->ifFalse()->lir();
    // If the compare set the  0 bit, then the result
    // is definately false.
    masm.ma_b(ifFalse->label(), Assembler::Zero);
    // it is also false if one of the operands is NAN, which is
    // shown as Overflow.
    masm.ma_b(ifFalse->label(), Assembler::Overflow);
    if (!isNextBlock(ifTrue))
        masm.ma_b(ifTrue->label());
    return true;
}

bool
CodeGeneratorARM::visitCompareD(LCompareD *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.compareDouble(lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()));
    return true;
}

bool
CodeGeneratorARM::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());
    masm.compareDouble(lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse());
    return true;
}

bool
CodeGeneratorARM::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

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
    return true;
}

bool
CodeGeneratorARM::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    if (mir->jsop() == JSOP_STRICTEQ)
        masm.branchTestBoolean(Assembler::NotEqual, lhs, lir->ifFalse()->lir()->label());
    else
        masm.branchTestBoolean(Assembler::NotEqual, lhs, lir->ifTrue()->lir()->label());

    if (rhs->isConstant())
        masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
    else
        masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGeneratorARM::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
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
    return true;
}

bool
CodeGeneratorARM::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
              mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    Label *notEqual;
    if (cond == Assembler::Equal)
        notEqual = lir->ifFalse()->lir()->label();
    else
        notEqual = lir->ifTrue()->lir()->label();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    masm.j(Assembler::NotEqual, notEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGeneratorARM::visitBitAndAndBranch(LBitAndAndBranch *baab)
{
    if (baab->right()->isConstant())
        masm.ma_tst(ToRegister(baab->left()), Imm32(ToInt32(baab->right())));
    else
        masm.ma_tst(ToRegister(baab->left()), ToRegister(baab->right()));
    emitBranch(Assembler::NonZero, baab->ifTrue(), baab->ifFalse());
    return true;
}

bool
CodeGeneratorARM::visitUInt32ToDouble(LUInt32ToDouble *lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorARM::visitNotI(LNotI *ins)
{
    // It is hard to optimize !x, so just do it the basic way for now.
    masm.ma_cmp(ToRegister(ins->input()), Imm32(0));
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorARM::visitNotD(LNotD *ins)
{
    // Since this operation is not, we want to set a bit if
    // the double is falsey, which means 0.0, -0.0 or NaN.
    // when comparing with 0, an input of 0 will set the Z bit (30)
    // and NaN will set the V bit (28) of the APSR.
    FloatRegister opd = ToFloatRegister(ins->input());
    Register dest = ToRegister(ins->output());

    // Do the compare
    masm.ma_vcmpz(opd);
    bool nocond = true;
    if (nocond) {
        // Load the value into the dest register
        masm.as_vmrs(dest);
        masm.ma_lsr(Imm32(28), dest, dest);
        masm.ma_alu(dest, lsr(dest, 2), dest, op_orr); // 28 + 2 = 30
        masm.ma_and(Imm32(1), dest);
    } else {
        masm.as_vmrs(pc);
        masm.ma_mov(Imm32(0), dest);
        masm.ma_mov(Imm32(1), dest, NoSetCond, Assembler::Equal);
        masm.ma_mov(Imm32(1), dest, NoSetCond, Assembler::Overflow);
#if 0
        masm.as_vmrs(ToRegister(dest));
        // Mask out just the two bits we care about.  If neither bit is set,
        // the dest is already zero
        masm.ma_and(Imm32(0x50000000), dest, dest, Assembler::SetCond);
        // If it is non-zero, then force it to be 1.
        masm.ma_mov(Imm32(1), dest, NoSetCond, Assembler::NotEqual);
#endif
    }
    return true;
}

bool
CodeGeneratorARM::visitLoadSlotV(LLoadSlotV *load)
{
    const ValueOperand out = ToOutValue(load);
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    masm.loadValue(Address(base, offset), out);
    return true;
}

bool
CodeGeneratorARM::visitLoadSlotT(LLoadSlotT *load)
{
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    if (load->mir()->type() == MIRType_Double)
        masm.loadInt32OrDouble(Operand(base, offset), ToFloatRegister(load->output()));
    else
        masm.ma_ldr(Operand(base, offset + NUNBOX32_PAYLOAD_OFFSET), ToRegister(load->output()));
    return true;
}

bool
CodeGeneratorARM::visitStoreSlotT(LStoreSlotT *store)
{

    Register base = ToRegister(store->slots());
    int32_t offset = store->mir()->slot() * sizeof(js::Value);

    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();

    if (store->mir()->needsBarrier())
        emitPreBarrier(Address(base, offset), store->mir()->slotType());

    if (valueType == MIRType_Double) {
        masm.ma_vstr(ToFloatRegister(value), Operand(base, offset));
        return true;
    }

    // Store the type tag if needed.
    if (valueType != store->mir()->slotType())
        masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), Operand(base, offset));

    // Store the payload.
    if (value->isConstant())
        masm.storePayload(*value->toConstant(), Operand(base, offset));
    else
        masm.storePayload(ToRegister(value), Operand(base, offset));

    return true;
}

bool
CodeGeneratorARM::visitLoadElementT(LLoadElementT *load)
{
    Register base = ToRegister(load->elements());
    if (load->mir()->type() == MIRType_Double) {
        FloatRegister fpreg = ToFloatRegister(load->output());
        if (load->index()->isConstant()) {
            Address source(base, ToInt32(load->index()) * sizeof(Value));
            if (load->mir()->loadDoubles())
                masm.loadDouble(source, fpreg);
            else
                masm.loadInt32OrDouble(source, fpreg);
        } else {
            Register index = ToRegister(load->index());
            if (load->mir()->loadDoubles())
                masm.loadDouble(BaseIndex(base, index, TimesEight), fpreg);
            else
                masm.loadInt32OrDouble(base, index, fpreg);
        }
    } else {
        if (load->index()->isConstant()) {
            Address source(base, ToInt32(load->index()) * sizeof(Value));
            masm.load32(source, ToRegister(load->output()));
        } else {
            masm.ma_ldr(DTRAddr(base, DtrRegImmShift(ToRegister(load->index()), LSL, 3)),
                        ToRegister(load->output()));
        }
    }
    JS_ASSERT(!load->mir()->needsHoleCheck());
    return true;
}

void
CodeGeneratorARM::storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                                    const Register &elements, const LAllocation *index)
{
    if (index->isConstant()) {
        Address dest = Address(elements, ToInt32(index) * sizeof(Value));
        if (valueType == MIRType_Double) {
            masm.ma_vstr(ToFloatRegister(value), Operand(dest));
            return;
        }

        // Store the type tag if needed.
        if (valueType != elementType)
            masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), dest);

        // Store the payload.
        if (value->isConstant())
            masm.storePayload(*value->toConstant(), dest);
        else
            masm.storePayload(ToRegister(value), dest);
    } else {
        Register indexReg = ToRegister(index);
        if (valueType == MIRType_Double) {
            masm.ma_vstr(ToFloatRegister(value), elements, indexReg);
            return;
        }

        // Store the type tag if needed.
        if (valueType != elementType)
            masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), elements, indexReg);

        // Store the payload.
        if (value->isConstant())
            masm.storePayload(*value->toConstant(), elements, indexReg);
        else
            masm.storePayload(ToRegister(value), elements, indexReg);
    }
}

bool
CodeGeneratorARM::visitGuardShape(LGuardShape *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.ma_ldr(DTRAddr(obj, DtrOffImm(JSObject::offsetOfShape())), tmp);
    masm.ma_cmp(tmp, ImmGCPtr(guard->mir()->shape()));

    return bailoutIf(Assembler::NotEqual, guard->snapshot());
}

bool
CodeGeneratorARM::visitGuardObjectType(LGuardObjectType *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.ma_ldr(DTRAddr(obj, DtrOffImm(JSObject::offsetOfType())), tmp);
    masm.ma_cmp(tmp, ImmGCPtr(guard->mir()->typeObject()));

    Assembler::Condition cond =
        guard->mir()->bailOnEquality() ? Assembler::Equal : Assembler::NotEqual;
    return bailoutIf(cond, guard->snapshot());
}

bool
CodeGeneratorARM::visitGuardClass(LGuardClass *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadObjClass(obj, tmp);
    masm.ma_cmp(tmp, Imm32((uint32_t)guard->mir()->getClass()));
    if (!bailoutIf(Assembler::NotEqual, guard->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorARM::visitImplicitThis(LImplicitThis *lir)
{
    Register callee = ToRegister(lir->callee());
    const ValueOperand out = ToOutValue(lir);

    // The implicit |this| is always |undefined| if the function's environment
    // is the current global.
    masm.ma_ldr(DTRAddr(callee, DtrOffImm(JSFunction::offsetOfEnvironment())), out.typeReg());
    masm.ma_cmp(out.typeReg(), ImmGCPtr(&gen->info().script()->global()));

    // TODO: OOL stub path.
    if (!bailoutIf(Assembler::NotEqual, lir->snapshot()))
        return false;

    masm.moveValue(UndefinedValue(), out);
    return true;
}

typedef bool (*InterruptCheckFn)(JSContext *);
static const VMFunction InterruptCheckInfo = FunctionInfo<InterruptCheckFn>(InterruptCheck);

bool
CodeGeneratorARM::visitInterruptCheck(LInterruptCheck *lir)
{
    OutOfLineCode *ool = oolCallVM(InterruptCheckInfo, lir, (ArgList()), StoreNothing());
    if (!ool)
        return false;

    void *interrupt = (void*)&gen->compartment->rt->interrupt;
    masm.load32(AbsoluteAddress(interrupt), lr);
    masm.ma_cmp(lr, Imm32(0));
    masm.ma_b(ool->entry(), Assembler::NonZero);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorARM::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint
    // patching to occur. Otherwise, we could overwrite the invalidation
    // epilogue.
    for (size_t i = 0; i < sizeof(void *); i+= Assembler::nopSize())
        masm.nop();

    masm.bind(&invalidate_);

    // Push the return address of the point that we bailed out at onto the stack
    masm.Push(lr);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    IonCode *thunk = GetIonContext()->compartment->ionCompartment()->getInvalidationThunk();

    masm.branch(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk should
    // pop the invalidated JS frame and return directly to its caller.
    masm.breakpoint();
    return true;
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // Can always use the scratch register on ARM.
    addState->dispatchScratch = ScratchRegister;
}

template <class U>
Register
getBase(U *mir)
{
    switch (mir->base()) {
      case U::Heap: return HeapReg;
      case U::Global: return GlobalReg;
    }
    return InvalidReg;
}

bool
CodeGeneratorARM::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
CodeGeneratorARM::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
CodeGeneratorARM::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    const MAsmJSLoadHeap *mir = ins->mir();
    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->viewType()) {
      case ArrayBufferView::TYPE_INT8:    isSigned = true; size = 8; break;
      case ArrayBufferView::TYPE_UINT8:   isSigned = false; size = 8; break;
      case ArrayBufferView::TYPE_INT16:   isSigned = true; size = 16; break;
      case ArrayBufferView::TYPE_UINT16:  isSigned = false; size = 16; break;
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT32:  isSigned = true;  size = 32; break;
      case ArrayBufferView::TYPE_FLOAT64: isFloat = true;   size = 64; break;
      case ArrayBufferView::TYPE_FLOAT32:
        isFloat = true;
        size = 32;
        break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }
    Register index = ToRegister(ins->ptr());
    BufferOffset bo = masm.ma_BoundsCheck(index);
    if (isFloat) {
        VFPRegister vd(ToFloatRegister(ins->output()));
        if (size == 32) {
            masm.ma_vldr(vd.singleOverlay(), HeapReg, index, 0, Assembler::Zero);
            masm.as_vcvt(vd, vd.singleOverlay(), false, Assembler::Zero);
        } else {
            masm.ma_vldr(vd, HeapReg, index, 0, Assembler::Zero);
        }
        masm.ma_vmov(NANReg, ToFloatRegister(ins->output()), Assembler::NonZero);
    }  else {
        masm.ma_dataTransferN(IsLoad, size, isSigned, HeapReg, index,
                              ToRegister(ins->output()), Offset, Assembler::Zero);
        masm.ma_mov(Imm32(0), ToRegister(ins->output()), NoSetCond, Assembler::NonZero);
    }
    return gen->noteBoundsCheck(bo.getOffset());
}

bool
CodeGeneratorARM::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    const MAsmJSStoreHeap *mir = ins->mir();
    bool isSigned;
    int size;
    bool isFloat = false;
    switch (mir->viewType()) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_UINT8:   isSigned = false; size = 8; break;
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_UINT16:  isSigned = false; size = 16; break;
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT32:  isSigned = true;  size = 32; break;
      case ArrayBufferView::TYPE_FLOAT64: isFloat = true;   size = 64; break;
      case ArrayBufferView::TYPE_FLOAT32:
        isFloat = true;
        size = 32;
        break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }
    Register index = ToRegister(ins->ptr());

    BufferOffset bo = masm.ma_BoundsCheck(index);
    if (isFloat) {
        VFPRegister vd(ToFloatRegister(ins->value()));
        if (size == 32) {
            masm.storeFloat(vd, HeapReg, index, Assembler::Zero);
        } else {
            masm.ma_vstr(vd, HeapReg, index, 0, Assembler::Zero);
        }
    }  else {
        masm.ma_dataTransferN(IsStore, size, isSigned, HeapReg, index,
                              ToRegister(ins->value()), Offset, Assembler::Zero);
    }
    return gen->noteBoundsCheck(bo.getOffset());
}

bool
CodeGeneratorARM::visitAsmJSPassStackArg(LAsmJSPassStackArg *ins)
{
    const MAsmJSPassStackArg *mir = ins->mir();
    Operand dst(StackPointer, mir->spOffset());
    if (ins->arg()->isConstant()) {
        //masm.as_bkpt();
        masm.ma_storeImm(Imm32(ToInt32(ins->arg())), dst);
    } else {
        if (ins->arg()->isGeneralReg())
            masm.ma_str(ToRegister(ins->arg()), dst);
        else
            masm.ma_vstr(ToFloatRegister(ins->arg()), dst);
    }

    return true;
}

bool
CodeGeneratorARM::visitUDiv(LUDiv *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    masm.ma_udiv(lhs, rhs, output);
    return true;
}

bool
CodeGeneratorARM::visitUMod(LUMod *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());
    Label notzero;
    Label done;

    masm.ma_cmp(rhs, Imm32(0));
    masm.ma_b(&notzero, Assembler::NonZero);
    masm.ma_mov(Imm32(0), output);
    masm.ma_b(&done);

    masm.bind(&notzero);
    masm.ma_umod(lhs, rhs, output);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorARM::visitSoftUDivOrMod(LSoftUDivOrMod *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    JS_ASSERT(lhs == r0);
    JS_ASSERT(rhs == r1);
    JS_ASSERT(ins->mirRaw()->isAsmJSUDiv() || ins->mirRaw()->isAsmJSUMod());
    JS_ASSERT_IF(ins->mirRaw()->isAsmJSUDiv(), output == r0);
    JS_ASSERT_IF(ins->mirRaw()->isAsmJSUMod(), output == r1);

    Label afterDiv;

    masm.ma_cmp(rhs, Imm32(0));
    Label notzero;
    masm.ma_b(&notzero, Assembler::NonZero);
    masm.ma_mov(Imm32(0), output);
    masm.ma_b(&afterDiv);
    masm.bind(&notzero);

    masm.setupAlignedABICall(2);
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, __aeabi_uidivmod));

    masm.bind(&afterDiv);
    return true;
}

bool
CodeGeneratorARM::visitEffectiveAddress(LEffectiveAddress *ins)
{
    const MEffectiveAddress *mir = ins->mir();
    Register base = ToRegister(ins->base());
    Register index = ToRegister(ins->index());
    Register output = ToRegister(ins->output());
    masm.as_add(output, base, lsl(index, mir->scale()));
    masm.ma_add(Imm32(mir->displacement()), output);
    return true;
}

bool
CodeGeneratorARM::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    const MAsmJSLoadGlobalVar *mir = ins->mir();
    unsigned addr = mir->globalDataOffset();
    if (mir->type() == MIRType_Int32)
        masm.ma_dtr(IsLoad, GlobalReg, Imm32(addr), ToRegister(ins->output()));
    else
        masm.ma_vldr(Operand(GlobalReg, addr), ToFloatRegister(ins->output()));
    return true;
}

bool
CodeGeneratorARM::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    const MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double);
    unsigned addr = mir->globalDataOffset();
    if (mir->value()->type() == MIRType_Int32)
        masm.ma_dtr(IsStore, GlobalReg, Imm32(addr), ToRegister(ins->value()));
    else
        masm.ma_vstr(ToFloatRegister(ins->value()), Operand(GlobalReg, addr));
    return true;
}

bool
CodeGeneratorARM::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    const MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register tmp = ToRegister(ins->temp());
    Register out = ToRegister(ins->output());
    unsigned addr = mir->globalDataOffset();
    masm.ma_mov(Imm32(addr), tmp);
    masm.as_add(tmp, tmp, lsl(index, 2));
    masm.ma_ldr(DTRAddr(GlobalReg, DtrRegImmShift(tmp, LSL, 0)), out);

    return true;
}

bool
CodeGeneratorARM::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    const MAsmJSLoadFFIFunc *mir = ins->mir();

    masm.ma_ldr(Operand(GlobalReg, mir->globalDataOffset()), ToRegister(ins->output()));

    return true;
}

bool
CodeGeneratorARM::visitNegI(LNegI *ins)
{
    Register input = ToRegister(ins->input());
    masm.ma_neg(input, ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorARM::visitNegD(LNegD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    masm.ma_vneg(input, ToFloatRegister(ins->output()));
    return true;
}
