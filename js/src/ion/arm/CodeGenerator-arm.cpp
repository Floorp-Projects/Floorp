/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CodeGenerator-arm.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "jsnum.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "ion/IonFrames.h"
#include "ion/MoveEmitter.h"
#include "ion/IonCompartment.h"

using namespace js;
using namespace js::ion;

// shared
CodeGeneratorARM::CodeGeneratorARM(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorShared(gen, graph),
    deoptLabel_(NULL)
{
}

bool
CodeGeneratorARM::generatePrologue()
{
    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameSize());

    // Allocate returnLabel_ on the heap, so we don't run it's destructor and
    // assert-not-bound in debug mode on compilation failure.
    returnLabel_ = new HeapLabel();

    return true;
}

bool
CodeGeneratorARM::generateEpilogue()
{
    masm.bind(returnLabel_);

    // Pop the stack we allocated at the start of the function.
    masm.freeStack(frameSize());
    JS_ASSERT(masm.framePushed() == 0);

    masm.ma_pop(pc);
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
CodeGeneratorARM::visitGoto(LGoto *jump)
{
    LBlock *target = jump->target()->lir();

    // Don't bother emitting a jump if we'll flow through to the next block.
    if (isNextBlock(target))
        return true;

    masm.ma_b(target->label());
    return true;
}

bool
CodeGeneratorARM::visitTestIAndBranch(LTestIAndBranch *test)
{
    const LAllocation *opd = test->getOperand(0);
    LBlock *ifTrue = test->ifTrue()->lir();
    LBlock *ifFalse = test->ifFalse()->lir();

    // Test the operand
    masm.ma_cmp(Imm32(0), ToRegister(opd));

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
CodeGeneratorARM::visitCompareI(LCompareI *comp)
{
    const LAllocation *left = comp->getOperand(0);
    const LAllocation *right = comp->getOperand(1);
    const LDefinition *def = comp->getDef(0);

    masm.ma_cmp(ToRegister(left), ToOperand(right));
    masm.ma_mov(Imm32(1), ToRegister(def));
    masm.ma_mov(Imm32(0), ToRegister(def),
                NoSetCond, Assembler::NotEqual);
    return true;
}

static inline Assembler::Condition
JSOpToCondition(JSOp op)
{
    switch (op) {
      case JSOP_LT:
        return Assembler::LessThan;
      case JSOP_LE:
        return Assembler::LessThanOrEqual;
      case JSOP_GT:
        return Assembler::GreaterThan;
      case JSOP_GE:
        return Assembler::GreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unrecognized comparison operation");
        return Assembler::Equal;
    }
}

bool
CodeGeneratorARM::visitCompareIAndBranch(LCompareIAndBranch *comp)
{
#if 0
    const LAllocation *left = comp->getOperand(0);
    const LAllocation *right = comp->getOperand(1);
    LBlock *ifTrue = comp->ifTrue()->lir();
    LBlock *ifFalse = comp->ifFalse()->lir();
    Assembler::Condition cond = comp->condition();

    // Compare the operands
    masm.ma_cmp(ToRegister(left), ToOperand(right));

    // Take advantage of block fallthrough when possible
    if (isNextBlock(ifFalse)) {
        masm.ma_b(ifTrue->label(), cond);
    } else if (isNextBlock(ifTrue)) {
        masm.ma_b(ifFalse->label(), Assembler::inverseCondition(cond));
    } else {
        masm.ma_b(ifTrue->label(), cond);
        masm.ma_b(ifFalse->label(), Assembler::Always);
    }
    return true;
#endif
    Assembler::Condition cond = JSOpToCondition(comp->jsop());
    masm.ma_cmp(ToRegister(comp->left()), ToOperand(comp->right()));
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;

}

bool
CodeGeneratorARM::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_) {
        // All non-table-based bailouts will go here.
        masm.bind(deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        masm.ma_mov(Imm32(frameSize()), lr);

        IonCompartment *ion = gen->cx->compartment->ionCompartment();
        IonCode *handler = ion->getGenericBailoutHandler(gen->cx);
        if (!handler)
            return false;

        masm.ma_b(handler->raw(), Relocation::CODE);
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
        uint8 *code = deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE;
        masm.ma_b(code, condition, Relocation::EXTERNAL);
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
    JS_ASSERT(label->used() && !label->bound());
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
        uint8 *code = deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE;
        masm.retarget(label, code, Relocation::EXTERNAL);
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
CodeGeneratorARM::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    masm.bind(ool->entry());

    if (!deoptLabel_)
        deoptLabel_ = new HeapLabel();
    masm.ma_mov(Imm32(ool->snapshot()->snapshotOffset()), ScratchRegister);
    masm.ma_push(ScratchRegister);
    masm.ma_push(ScratchRegister);
    masm.ma_b(deoptLabel_);
    return true;
}

bool
CodeGeneratorARM::visitAddI(LAddI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);

    if (rhs->isConstant()) {
        masm.ma_add(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCond);
    } else {
        masm.ma_add(ToRegister(lhs), ToOperand(rhs), ToRegister(dest), SetCond);
    }

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

    if (rhs->isConstant()) {
        masm.ma_sub(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), SetCond);
    } else {
        masm.ma_sub(ToRegister(lhs), ToOperand(rhs), ToRegister(dest), SetCond);
    }

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

    if (rhs->isConstant()) {
        // Bailout on -0.0
        int32 constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            masm.ma_cmp(Imm32(0), ToRegister(lhs));
            if (bailoutIf(bailoutCond, ins->snapshot()))
                    return false;
        }

        switch (constant) {
          case -1:
              masm.ma_rsb(ToRegister(lhs), Imm32(0), ToRegister(dest));
            break;
          case 0:
              masm.ma_mov(Imm32(0), ToRegister(dest));
            return true; // escape overflow check;
          case 1:
            // nop
            return true; // escape overflow check;
          case 2:
              masm.ma_lsl(Imm32(1), ToRegister(lhs), ToRegister(dest));
            break;
          default:
            if (!mul->canOverflow() && constant > 0) {
                // Use shift if cannot overflow and constant is power of 2
                int32 shift;
                JS_FLOOR_LOG2(shift, constant);
                if ((1 << shift) == constant) {
                    masm.ma_lsl(Imm32(shift), ToRegister(lhs), ToRegister(dest));
                    return true;
                }
            } else if (!mul->canOverflow()) {
                int32 shift;
                JS_FLOOR_LOG2(shift, -constant);
                if ((1<<shift) == -constant) {
                    // since lsl is actually a modifier, and not an instruction,
                    // we can emit mvn dest, op1 lsl 3 for op1 * -8
                    // although mvn is a bitwise negate, not an actual negate
                }
            }
            //masm.imull(Imm32(ToInt32(rhs)), ToRegister(lhs));
            JS_NOT_REACHED("need to implement emitinst for mul/mull");
        }

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;
    } else {
        //masm.imull(ToOperand(rhs), ToRegister(lhs));
        JS_NOT_REACHED("need to implement emitinst for mul/mull");

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;

        // Bailout on 0 (could be -0.0)
        if (mul->canBeNegativeZero()) {
            masm.ma_cmp(Imm32(0), ToRegister(lhs));
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    return true;
}

bool
CodeGeneratorARM::visitDivI(LDivI *ins)
{
    JS_NOT_REACHED("codegen for DIVI NYI");
#if 0
    Register remainder = ToRegister(ins->remainder());
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());

    JS_ASSERT(remainder == edx);
    JS_ASSERT(lhs == eax);

    // Prevent divide by zero.
    masm.testl(rhs, rhs);
    if (!bailoutIf(Assembler::Zero, ins->snapshot()))
        return false;

    // Prevent an integer overflow exception from -2147483648 / -1.
    Label notmin;
    masm.cmpl(lhs, Imm32(INT_MIN));
    masm.ma_b(&notmin, Assembler::NotEqual);
    masm.cmpl(rhs, Imm32(-1));
    if (!bailoutIf(Assembler::Equal, ins->snapshot()))
        return false;
    masm.bind(&notmin);

    // Prevent negative 0.
    Label nonzero;
    masm.testl(lhs, lhs);
    masm.ma_b(&nonzero, Assembler::NonZero);
    masm.cmpl(rhs, Imm32(0));
    if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
        return false;
    masm.bind(&nonzero);

    // Sign extend lhs (eax) to (eax:edx) since idiv is 64-bit.
    masm.cdq();
    masm.idiv(rhs);

    // If the remainder is > 0, bailout since this must be a double.
    masm.testl(remainder, remainder);
    if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
        return false;

#endif
    return true;
}

bool
CodeGeneratorARM::visitBitNot(LBitNot *ins)
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
CodeGeneratorARM::visitBitOp(LBitOp *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);
    // all of these bitops should be either imm32's, or integer registers.
    switch (ins->bitop()) {
      case JSOP_BITOR:
        if (rhs->isConstant()) {
            masm.ma_orr(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        } else {
            masm.ma_orr(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        }
        break;
      case JSOP_BITXOR:
        if (rhs->isConstant()) {
            masm.ma_eor(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        } else {
            masm.ma_eor(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        }
        break;
      case JSOP_BITAND:
        if (rhs->isConstant()) {
            masm.ma_and(Imm32(ToInt32(rhs)), ToRegister(lhs), ToRegister(dest));
        } else {
            masm.ma_and(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
        }
        break;
      default:
        JS_NOT_REACHED("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorARM::visitShiftOp(LShiftOp *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);
    const LDefinition *dest = ins->getDef(0);
    // TODO: the shift amounts should be AND'ed into the 0-31 range since
    // arm shifts by the lower byte of the register (it will attempt to shift by
    // 250 if you ask it to, and the result will probably not be what you want.
    switch (ins->bitop()) {
        case JSOP_LSH:
          if (rhs->isConstant()) {
                masm.ma_lsl(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs), ToRegister(dest));
          } else {
                masm.ma_lsl(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
          }
            break;
        case JSOP_RSH:
          if (rhs->isConstant()) {
                masm.ma_asr(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs), ToRegister(dest));
          } else {
                masm.ma_asr(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
          }
            break;
        case JSOP_URSH: {
            MUrsh *ursh = ins->mir()->toUrsh();
            if (rhs->isConstant()) {
                masm.ma_lsr(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs), ToRegister(dest));
            } else {
                masm.ma_lsr(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
            }

            // Note: this is an unsigned operation.
            // We don't have a UINT32 type, so we will emulate this with INT32
            // The bit representation of an integer from ToInt32 and ToUint32 are the same.
            // So the inputs are ok.
            // But we need to bring the output back again from UINT32 to INT32.
            // Both representation overlap each other in the positive numbers. (in INT32)
            // So there is only a problem when solution (in INT32) is negative.
            if (ursh->canOverflow()) {
                masm.ma_cmp(Imm32(0), ToRegister(lhs));
                if (!bailoutIf(Assembler::LessThan, ins->snapshot())) {
                    return false;
                }
            }
            break;
        }
        default:
            JS_NOT_REACHED("unexpected shift opcode");
            return false;
    }

    return true;
}

bool
CodeGeneratorARM::visitInteger(LInteger *ins)
{
    const LDefinition *def = ins->getDef(0);
    masm.ma_mov(Imm32(ins->getValue()), ToRegister(def));
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
    return MoveOperand(StackPointer, ToStackOffset(a));
}

bool
CodeGeneratorARM::visitMoveGroup(LMoveGroup *group)
{
    if (!group->numMoves())
        return true;

    MoveResolver &resolver = masm.moveResolver();

    for (size_t i = 0; i < group->numMoves(); i++) {
        const LMove &move = group->getMove(i);

        const LAllocation *from = move.from();
        const LAllocation *to = move.to();

        // No bogus moves.
        JS_ASSERT(*from != *to);
        JS_ASSERT(!from->isConstant());
        JS_ASSERT(from->isDouble() == to->isDouble());

        MoveResolver::Move::Kind kind = from->isDouble()
                                        ? MoveResolver::Move::DOUBLE
                                        : MoveResolver::Move::GENERAL;

        if (!resolver.addMove(toMoveOperand(from), toMoveOperand(to), kind)) {
            return false;
        }
    }

    if (!resolver.resolve())
        return false;

    MoveEmitter emitter(masm);
    emitter.emit(resolver);
    emitter.finish();

    return true;
}

bool
CodeGeneratorARM::visitTableSwitch(LTableSwitch *ins)
{
    MTableSwitch *mir = ins->mir();
    const LAllocation *input = ins->getOperand(0);

    // Put input in temp. register
    LDefinition *index = ins->getTemp(0);

    // Lower value with low value
    if (mir->low() != 0) {
        masm.ma_sub(ToRegister(input), Imm32(mir->low()), ToRegister(index));
    }

    // Jump to default case if input is out of range
    LBlock *defaultcase = mir->getDefault()->lir();
    int32 cases = mir->numCases();
    masm.ma_cmp(Imm32(cases), ToRegister(index));
    masm.ma_b(defaultcase->label(), Assembler::AboveOrEqual);

    // Create a label pointing to the jumptable
    // This gets patched after linking
    CodeLabel *label = new CodeLabel();
    if (!masm.addCodeLabel(label))
        return false;

    // Compute the pointer to the right case in the second temp. register
    LDefinition *base = ins->getTemp(1);

#if 0
    // I don't have CodeLabels implemented in any way shape or form, particularly
    // not for the purposes of moving.  This will have to wait.
    masm.ma_mov(label->dest(), ToRegister(base));
    masm.ma_add(ToRegister(base), Operand(lsl(ToRegister(index), 3)),ToRegister(base));

    Operand pointer = Operand(ToRegister(base), ToRegister(index), TimesEight);
    masm.lea(pointer, ToRegister(base));

    // Jump to the right case
    masm.jmp(ToOperand(base));

    // Create the jumptable,
    // Every jump statements get aligned on pointersize
    // That way there is always 2*pointersize between each jump statement.
    masm.align(1 << TimesFour);
    masm.bind(label->src());

    for (uint j=0; j<ins->mir()->numCases(); j++) {
        LBlock *caseblock = ins->mir()->getCase(j)->lir();

        masm.jmp(caseblock->label());
        masm.align(1 << TimesFour);
    }

    return true;
#endif
    JS_NOT_REACHED("what the deuce are tables");
    return false;
}

bool
CodeGeneratorARM::visitMathD(LMathD *math)
{
    const LAllocation *src1 = math->getOperand(1);
    const LAllocation *src2 = math->getOperand(0);
    const LDefinition *output = math->getDef(0);
    
    switch (math->jsop()) {
      case JSOP_ADD:
          masm.ma_vadd(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
        break;
      case JSOP_MUL:
          masm.ma_vmul(ToFloatRegister(src1), ToFloatRegister(src2), ToFloatRegister(output));
      default:
        JS_NOT_REACHED("unexpected opcode");
        return false;
    }
    return true;
}

// Checks whether a double is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
bool
CodeGeneratorARM::emitDoubleToInt32(const FloatRegister &src, const Register &dest, Label *fail)
{
    // we have three options:
    // 3) convert the floating point value to an integer, if it did not fit,
    //        then when we convert it *back* to  a float, it will have a
    //        different value, which we can test.
    masm.ma_vcvt_F64_I32(src, ScratchFloatReg);
    // move the value into the dest register.
    masm.ma_vxfer(ScratchFloatReg, dest);
    masm.ma_vcvt_I32_F64(ScratchFloatReg, ScratchFloatReg);
    masm.ma_vcmp_F64(ScratchFloatReg, src);
    // bail out if they aren't equal.
    masm.ma_b(fail, Assembler::NotEqual_Unordered);
    // guard for /= 0.
    return true;
}
    // 1) convert the floating point value to an integer, if it did not fit,
    //        then it was clamped to INT_MIN/INT_MAX, and we can test it.
    //        NOTE: if the value really was supposed to be INT_MAX / INT_MIN
    //        then it will be wrong.
    // 2) convert the floating point value to an integer, if it did not fit,
    //        then it set one or two bits in the fpcsr.  Check those.

void
CodeGeneratorARM::emitTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail)
{
    JS_NOT_REACHED("truncate Double NYI");
}
// "x86-only"

// The first two size classes are 128 and 256 bytes respectively. After that we
// increment by 512.
static const uint32 LAST_FRAME_SIZE = 512;
static const uint32 LAST_FRAME_INCREMENT = 512;
static const uint32 FrameSizes[] = { 128, 256, LAST_FRAME_SIZE };

FrameSizeClass
FrameSizeClass::FromDepth(uint32 frameDepth)
{
    for (uint32 i = 0; i < JS_ARRAY_LENGTH(FrameSizes); i++) {
        if (frameDepth < FrameSizes[i])
            return FrameSizeClass(i);
    }

    uint32 newFrameSize = frameDepth - LAST_FRAME_SIZE;
    uint32 sizeClass = (newFrameSize / LAST_FRAME_INCREMENT) + 1;

    return FrameSizeClass(JS_ARRAY_LENGTH(FrameSizes) + sizeClass);
}
uint32
FrameSizeClass::frameSize() const
{
    JS_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);

    if (class_ < JS_ARRAY_LENGTH(FrameSizes))
        return FrameSizes[class_];

    uint32 step = class_ - JS_ARRAY_LENGTH(FrameSizes);
    return LAST_FRAME_SIZE + step * LAST_FRAME_INCREMENT;
}

ValueOperand
CodeGeneratorARM::ToValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

bool
CodeGeneratorARM::visitValue(LValue *value)
{
    jsval_layout jv = JSVAL_TO_IMPL(value->value());

    LDefinition *type = value->getDef(TYPE_INDEX);
    LDefinition *payload = value->getDef(PAYLOAD_INDEX);

    masm.ma_mov(Imm32(jv.s.tag), ToRegister(type));
    if (value->value().isMarkable()) {
        masm.ma_mov(ImmGCPtr((gc::Cell *)jv.s.payload.ptr), ToRegister(payload));
    } else {
        masm.ma_mov(Imm32(jv.s.payload.u32), ToRegister(payload));
    }
    return true;
}

static inline JSValueTag
MIRTypeToTag(MIRType type)
{
    switch (type) {
      case MIRType_Boolean:
        return JSVAL_TAG_BOOLEAN;
      case MIRType_Int32:
        return JSVAL_TAG_INT32;
      case MIRType_String:
        return JSVAL_TAG_STRING;
      case MIRType_Object:
        return JSVAL_TAG_OBJECT;
      default:
        JS_NOT_REACHED("no payload...");
    }
    return JSVAL_TAG_NULL;
}

bool
CodeGeneratorARM::visitBox(LBox *box)
{
    const LAllocation *a = box->getOperand(0);
    const LDefinition *type = box->getDef(TYPE_INDEX);

    JS_ASSERT(!a->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    // this is different on ARM.  fix this accordingly.
    masm.ma_mov(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
    return true;
}

bool
CodeGeneratorARM::visitBoxDouble(LBoxDouble *box)
{
    const LDefinition *payload = box->getDef(PAYLOAD_INDEX);
    const LDefinition *type = box->getDef(TYPE_INDEX);
    const LAllocation *in = box->getOperand(0);

    masm.as_vxfer(ToRegister(payload), ToRegister(type),
                  VFPRegister(ToFloatRegister(in)), Assembler::FloatToCore);
    return true;
}

bool
CodeGeneratorARM::visitUnbox(LUnbox *unbox)
{
    LAllocation *type = unbox->getOperand(TYPE_INDEX);
    masm.ma_cmp(Imm32(MIRTypeToTag(unbox->type())), ToRegister(type));
    if (!bailoutIf(Assembler::NotEqual, unbox->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorARM::visitReturn(LReturn *ret)
{

#ifdef DEBUG
    LAllocation *type = ret->getOperand(TYPE_INDEX);
    LAllocation *payload = ret->getOperand(PAYLOAD_INDEX);

    JS_ASSERT(ToRegister(type) == JSReturnReg_Type);
    JS_ASSERT(ToRegister(payload) == JSReturnReg_Data);
#endif
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir() != *gen->graph().poBegin())
        masm.ma_b(returnLabel_);
    return true;
}

void
CodeGeneratorARM::linkAbsoluteLabels()
{
    JSScript *script = gen->info().script();
    IonCode *method = script->ion->method();

    for (size_t i = 0; i < deferredDoubles_.length(); i++) {
        DeferredDouble *d = deferredDoubles_[i];
        const Value &v = script->ion->getConstant(d->index());
        MacroAssembler::Bind(method, d->label(), &v);
    }
}

bool
CodeGeneratorARM::visitDouble(LDouble *ins)
{

    const LDefinition *out = ins->getDef(0);
    const LConstantIndex *cindex = ins->getOperand(0)->toConstantIndex();
    const Value &v = graph.getConstant(cindex->index());

    jsdpun dpun;
    dpun.d = v.toDouble();
    if ((dpun.s.lo) == 0) {
        if (dpun.s.hi == 0) {
            // to zero a register, load 1.0, then execute dN <- dN - dN
            VFPImm dblEnc(0x3FF00000);
            masm.as_vimm(ToFloatRegister(out), dblEnc);
            masm.as_vsub(ToFloatRegister(out), ToFloatRegister(out), ToFloatRegister(out));
            return true;
        }
        VFPImm dblEnc(dpun.s.hi);
        if (dblEnc.isValid()) {
            masm.as_vimm(ToFloatRegister(out), dblEnc);
            return true;
        }

    }
    JS_NOT_REACHED("immediate NYI");
    return false;
#if 0
    DeferredDouble *d = new DeferredDouble(cindex->index());
    if (!deferredDoubles_.append(d))
        return false;

    masm.movsd(d->label(), ToFloatRegister(out));
    return true;
#endif
}

bool
CodeGeneratorARM::visitUnboxDouble(LUnboxDouble *ins)
{
    const ValueOperand box = ToValue(ins, LUnboxDouble::Input);
    const LDefinition *result = ins->output();

    Assembler::Condition cond = masm.testDouble(Assembler::NotEqual, box);
    if (!bailoutIf(cond, ins->snapshot()))
        return false;
    masm.unboxDouble(box, ToFloatRegister(result));
    return true;
}
Register
CodeGeneratorARM::splitTagForTest(const ValueOperand &value)
{
    return value.typeReg();
}
Assembler::Condition
CodeGeneratorARM::testStringTruthy(bool truthy, const ValueOperand &value)
{
    Register string = value.payloadReg();
    //DTRAddr lengthAndFlags = DTRAddr(string, DtrOffImm(JSString::offsetOfLengthAndFlags()));

    size_t mask = (0xFFFFFFFF << JSString::LENGTH_SHIFT);
    masm.ma_dtr(IsLoad, string, Imm32(JSString::offsetOfLengthAndFlags()), ScratchRegister);
    masm.ma_tst(Imm32(mask), ScratchRegister);
    return truthy ? Assembler::NonZero : Assembler::Zero;
}


bool
CodeGeneratorARM::visitCompareD(LCompareD *comp)
{
    JS_NOT_REACHED("Codegen for CompareD NYI");
    return false;
}

bool
CodeGeneratorARM::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    JS_NOT_REACHED("Codegen for CompareDAndBranch NYI");
    return false;
}


bool
CodeGeneratorARM::visitStackArg(LStackArg *arg)
{
    ValueOperand val = ToValue(arg, 0);
    uint32 argslot = arg->argslot();
    int32 stack_offset = StackOffsetOfPassedArg(argslot);

    masm.ma_str(val.typeReg(), DTRAddr(StackPointer, DtrOffImm(stack_offset)));
    masm.ma_str(val.payloadReg(), DTRAddr(StackPointer, DtrOffImm(stack_offset+4)));
    return true;
}

bool
CodeGeneratorARM::visitCallGeneric(LCallGeneric *call)
{

    // Holds the function object.
    const LAllocation *obj = call->getFunction();
    Register objreg  = ToRegister(obj);

    // Holds the callee token. Initially undefined.
    const LAllocation *tok = call->getToken();
    Register tokreg  = ToRegister(tok);

    // Holds the function nargs. Initially undefined.
    const LAllocation *nargs = call->getNargsReg();
    Register nargsreg = ToRegister(nargs);

    uint32 callargslot  = call->argslot();
    uint32 unused_stack = StackOffsetOfPassedArg(callargslot);



    // Guard that objreg is actually a function object.
    masm.ma_ldr(DTRAddr(objreg, DtrOffImm(JSObject::offsetOfClassPointer())), tokreg);
    masm.ma_ldr(DTRAddr(tokreg, DtrOffImm(0)), tokreg);

    masm.ma_cmp(Imm32((uint32)&js::FunctionClass), tokreg);
    if (!bailoutIf(Assembler::NotEqual, call->snapshot()))
        return false;

    // Extract the function object.
    masm.ma_ldr(DTRAddr(objreg, DtrOffImm(offsetof(JSObject, privateData))),
                objreg);

    // Guard that objreg is a non-native function:
    // Non-native iff (obj->flags & JSFUN_KINDMASK >= JSFUN_INTERPRETED).
    masm.ma_ldr(DTRAddr(objreg, DtrOffImm(offsetof(JSFunction, flags))),
                tokreg);

    // theoreticaly the test is tokreg & JSFUN_KINDMASK >= JSFUN_INTERPRETED
    // however, JSFUN_KINDMASK is two bits, and JSFUN_INTERPRETED is the smallest
    // non-zero value that those two bits can take on (0b01).  In this case, the
    // comparison is equivalent to seeing if either of the bits is non-zero.
    masm.ma_tst(Imm32(JSFUN_KINDMASK), tokreg);

    if (!bailoutIf(Assembler::Zero, call->snapshot())) {
        return false;
    }

    // Save the calleeToken, which equals the function object.
    masm.ma_mov(objreg, tokreg);

    // Knowing that objreg is a non-native function, load the JSScript.
    masm.ma_ldr(DTRAddr(objreg, DtrOffImm(offsetof(JSFunction, u.i.script_))),
                objreg);
    masm.ma_ldr(DTRAddr(objreg, DtrOffImm(offsetof(JSScript, ion))),
                objreg);

    masm.ma_cmp(Imm32(0), objreg);
    // Bail if the callee has not yet been JITted.
    if (!bailoutIf(Assembler::Zero, call->snapshot()))
        return false;


    // Remember the size of the frame above this point, in case of bailout.
    JS_STATIC_ASSERT(IonFramePrefix::JSFrame == 0x0);
    uint32 stack_size = masm.framePushed() - unused_stack;
    uint32 size_descriptor = stack_size << IonFramePrefix::FrameTypeBits;



    // Nestle sp up to the argument vector.
    if (unused_stack) {
        masm.ma_add(StackPointer, Imm32(unused_stack), StackPointer);
    }
    // Construct the IonFramePrefix.
    masm.ma_push(tokreg);
    masm.push(Imm32(size_descriptor));

    // Call the function, padding with |undefined| in case of insufficient args.
    {
        Label thunk, rejoin;


        // Get the address of the argumentsRectifier code.
        IonCompartment *ion = gen->ionCompartment();

        IonCode *argumentsRectifier = ion->getArgumentsRectifier(gen->cx);
        if (!argumentsRectifier)
            return false;

        // Check whether the provided arguments satisfy target argc.
        masm.ma_ldrh(EDtrAddr(tokreg, EDtrOffImm(offsetof(JSFunction, nargs))),
                     nargsreg);

        masm.ma_cmp(Imm32(call->nargs()), nargsreg);
        masm.ma_b(&thunk, Assembler::Above);

        // No argument fixup needed. Call the function normally.
        masm.ma_ldr(DTRAddr(objreg, DtrOffImm(offsetof(IonScript, method_))), objreg);
        masm.ma_ldr(DTRAddr(objreg, DtrOffImm(IonCode::OffsetOfCode())), objreg);
        masm.ma_callIon(objreg);
        masm.ma_b(&rejoin);

        // Argument fixup needed. Create a frame with correct |nargs| and then call.
        masm.bind(&thunk);
        // r0 should be safe, since everything has been saved by the register
        // allocator
        masm.ma_mov(Imm32(call->nargs()), r0);
        masm.ma_mov(Imm32((int)argumentsRectifier->raw()), ScratchRegister);
        masm.ma_callIon(ScratchRegister);

        masm.bind(&rejoin);

    }
#if 0
    // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
    int prefix_garbage = 2 * sizeof(void *);
    int restore_diff = prefix_garbage - unused_stack;
    
    if (restore_diff > 0)
        masm.addPtr(Imm32(restore_diff), StackPointer);
    else if (restore_diff < 0)
        masm.subPtr(Imm32(-restore_diff), StackPointer);

    return true;
#endif
    return false;
}
