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
#include "jsscope.h"
#include "jsscriptinlines.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "ion/IonFrames.h"
#include "ion/MoveEmitter.h"
#include "ion/IonCompartment.h"

#include "jsscopeinlines.h"

using namespace js;
using namespace js::ion;

class DeferredJumpTable : public DeferredData
{
    LTableSwitch *lswitch;
    BufferOffset off;
  public:
    DeferredJumpTable(LTableSwitch *lswitch, BufferOffset off_)
        : lswitch(lswitch), off(off_)
    { }

    void copy(IonCode *code, uint8 *ignore__) const {
        void **jumpData = (void **)(((char*)code->raw()) + off.getOffset());
        int numCases =  lswitch->mir()->numCases();
        // For every case write the pointer to the start in the table
        for (int j = 0; j < numCases; j++) {
            LBlock *caseblock = lswitch->mir()->getCase(numCases - 1 - j)->lir();
            Label *caseheader = caseblock->label();

            uint32 offset = caseheader->offset();
            *jumpData = (void *)(code->raw() + offset);
            jumpData++;
        }
    }
};


static inline Assembler::Condition
JSOpToCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::Equal;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::NotEqual;
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
    masm.checkStackAlignment();
    // Allocate returnLabel_ on the heap, so we don't run its destructor and
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

void
CodeGeneratorARM::emitSet(Assembler::Condition cond, const Register &dest)
{
    masm.ma_mov(Imm32(0), dest);
    masm.ma_mov(Imm32(1), dest, NoSetCond, cond);
}

bool
CodeGeneratorARM::visitCompareI(LCompareI *comp)
{
    const LAllocation *left = comp->getOperand(0);
    const LAllocation *right = comp->getOperand(1);
    const LDefinition *def = comp->getDef(0);

    if (right->isConstant())
        masm.ma_cmp(ToRegister(left), Imm32(ToInt32(right)));
    else
        masm.ma_cmp(ToRegister(left), ToOperand(right));
    masm.ma_mov(Imm32(0), ToRegister(def));
    masm.ma_mov(Imm32(1), ToRegister(def), NoSetCond, JSOpToCondition(comp->jsop()));
    return true;
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
    if (right->isConstant())
        masm.ma_cmp(ToRegister(left), Imm32(ToInt32(right)));
    else
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

    if (deoptLabel_) {
        // All non-table-based bailouts will go here.
        masm.bind(deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        masm.ma_mov(Imm32(frameSize()), lr);

        IonCompartment *ion = gen->cx->compartment->ionCompartment();
        IonCode *handler = ion->getGenericBailoutHandler(gen->cx);
        if (!handler)
            return false;

        masm.ma_b(handler->raw(), Relocation::IONCODE);
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
CodeGeneratorARM::visitOutOfLineBailout(OutOfLineBailout *ool)
{
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
        Assembler::Condition c = Assembler::Overflow;
        // Bailout on -0.0
        int32 constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::LessThan : Assembler::Equal;
            masm.ma_cmp(ToRegister(lhs), Imm32(0));
            if (bailoutIf(bailoutCond, ins->snapshot()))
                    return false;
        }
        // TODO: move these to ma_mul.
        switch (constant) {
          case -1:
              masm.ma_rsb(ToRegister(lhs), Imm32(0), ToRegister(dest));
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
            break;
          default:
#if 0
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
#endif
            if (mul->canOverflow()) {
                c = masm.ma_check_mul(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest), c);
            } else {
                masm.ma_mul(ToRegister(lhs), Imm32(ToInt32(rhs)), ToRegister(dest));
            }
        }

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(c, ins->snapshot()))
            return false;
    } else {
        Assembler::Condition c = Assembler::Overflow;

        //masm.imull(ToOperand(rhs), ToRegister(lhs));
        if (mul->canOverflow()) {
            c = masm.ma_check_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest), c);
        } else {
            masm.ma_mul(ToRegister(lhs), ToRegister(rhs), ToRegister(dest));
        }


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

extern "C" {
    extern int __aeabi_idiv(int,int);
}
bool
CodeGeneratorARM::visitDivI(LDivI *ins)
{
    // Extract the registers from this instruction
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    // Prevent INT_MIN / -1;
    // The integer division will give INT_MIN, but we want -(double)INT_MIN.
    masm.ma_cmp(lhs, Imm32(INT_MIN)); // sets EQ if lhs == INT_MIN
    masm.ma_cmp(rhs, Imm32(-1), Assembler::Equal); // if EQ (LHS == INT_MIN), sets EQ if rhs == -1
    if (!bailoutIf(Assembler::Equal, ins->snapshot()))
        return false;
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
    if (!bailoutIf(Assembler::Equal, ins->snapshot()))
        return false;
    masm.setupAlignedABICall(2);
    masm.setABIArg(0, lhs);
    masm.setABIArg(1, rhs);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, __aeabi_idiv));
    // HAXHAXHAX
    // this routine stores its result in r0 (expected), and it *happens* to store
    // the remainder in r3. If the remainer is non-zero, we should bail out.
    // this is completely unsupported by the ABI (specifies return registers r0 and r1)
    // and may change without notice in any one of a numebr of places.
    masm.ma_cmp(r3, Imm32(0));
    if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
        return false;
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
              if ((ToInt32(rhs) & 0x1f) != 0) {
                  masm.ma_asr(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs), ToRegister(dest));
              } else {
                  masm.ma_mov(ToRegister(lhs), ToRegister(dest));
              }
          } else {
                masm.ma_asr(ToRegister(rhs), ToRegister(lhs), ToRegister(dest));
          }
            break;
        case JSOP_URSH: {
            MUrsh *ursh = ins->mir()->toUrsh();
            if (rhs->isConstant()) {
                if ((ToInt32(rhs) & 0x1f) != 0) {
                    masm.ma_lsr(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs), ToRegister(dest));
                } else {
                    masm.ma_mov(ToRegister(lhs), ToRegister(dest));
                }
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
                masm.ma_cmp(ToRegister(lhs), Imm32(0));
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
    MTableSwitch *mir = ins->mir();
        Label *defaultcase = mir->getDefault()->lir()->label();
    const LAllocation *temp;

    if (ins->index()->isDouble()) {
        temp = ins->tempInt();

        // The input is a double, so try and convert it to an integer.
        // If it does not fit in an integer, take the default case.
        emitDoubleToInt32(ToFloatRegister(ins->index()), ToRegister(temp), defaultcase);
    } else {
        temp = ins->index();
    }

    int32 cases = mir->numCases();
    Register tempReg = ToRegister(temp);
    // Lower value with low value
    if (mir->low() != 0) {
        masm.ma_sub(tempReg, Imm32(mir->low()), tempReg, SetCond);
        masm.ma_rsb(tempReg, Imm32(cases - 1), tempReg, SetCond, Assembler::Unsigned);
    } else {
        masm.ma_rsb(tempReg, Imm32(cases - 1), tempReg, SetCond);
    }
    // TODO: there CANNOT be a pool between here and the *END* of the address
    // table.  there is presently no code in place to enforce this.
    masm.ma_ldr(DTRAddr(pc, DtrRegImmShift(tempReg, LSL, 2)), pc, Offset, Assembler::Unsigned);
    masm.ma_b(defaultcase);
    DeferredJumpTable *d = new DeferredJumpTable(ins, masm.nextOffset());
    masm.as_jumpPool(cases);

    if (!masm.addDeferredData(d, 0))
        return false;
    return true;
#if 0
    // Create a label pointing to the jumptable
    // This gets patched after linking
    if (!masm.addCodeLabel(label))
        return false;

    // Compute the pointer to the right case in the second temp. register
    LDefinition *base = ins->getTemp(1);


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

    for (size_t j=0; j<ins->mir()->numCases(); j++) {
        LBlock *caseblock = ins->mir()->getCase(j)->lir();

        masm.jmp(caseblock->label());
        masm.align(1 << TimesFour);
    }

    return true;

    JS_NOT_REACHED("what the deuce are tables");
    return false;
#endif
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
    // convert the floating point value to an integer, if it did not fit,
    //     then when we convert it *back* to  a float, it will have a
    //     different value, which we can test.
    masm.ma_vcvt_F64_I32(src, ScratchFloatReg);
    // move the value into the dest register.
    masm.ma_vxfer(ScratchFloatReg, dest);
    masm.ma_vcvt_I32_F64(ScratchFloatReg, ScratchFloatReg);
    masm.ma_vcmp(ScratchFloatReg, src);
    masm.as_vmrs(pc);
    // bail out if they aren't equal.
    masm.ma_b(fail, Assembler::VFP_NotEqualOrUnordered);
    // guard for /= 0.
    return true;
}

// there are two options for implementing emitTruncateDouble.
// 1) convert the floating point value to an integer, if it did not fit,
//        then it was clamped to INT_MIN/INT_MAX, and we can test it.
//        NOTE: if the value really was supposed to be INT_MAX / INT_MIN
//        then it will be wrong.
// 2) convert the floating point value to an integer, if it did not fit,
//        then it set one or two bits in the fpcsr.  Check those.
void
CodeGeneratorARM::emitTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail)
{
    masm.ma_vcvt_F64_I32(src, ScratchFloatReg);
    masm.ma_vxfer(ScratchFloatReg, dest);
    masm.ma_cmp(dest, Imm32(0x7fffffff));
    masm.ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    masm.ma_b(fail, Assembler::Equal);
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

bool
CodeGeneratorARM::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame   = value->getOperand(0);
    const LDefinition *type    = value->getDef(TYPE_INDEX);
    const LDefinition *payload = value->getDef(PAYLOAD_INDEX);

    const ptrdiff_t frameOffset = value->mir()->frameOffset();

    const ptrdiff_t payloadOffset = frameOffset + NUNBOX32_PAYLOAD_OFFSET;
    const ptrdiff_t typeOffset    = frameOffset + NUNBOX32_TYPE_OFFSET;

    masm.load32(Address(ToRegister(frame), payloadOffset), ToRegister(payload));
    masm.load32(Address(ToRegister(frame), typeOffset), ToRegister(type));

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

    masm.as_vxfer(ToRegister(payload), ToRegister(type),
                  VFPRegister(ToFloatRegister(in)), Assembler::FloatToCore);
    return true;
}

bool
CodeGeneratorARM::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();
    if (mir->fallible()) {
        const LAllocation *type = unbox->type();
        masm.ma_cmp(ToRegister(type), Imm32(MIRTypeToTag(mir->type())));
        if (!bailoutIf(Assembler::NotEqual, unbox->snapshot()))
            return false;
    }
    return true;
}

void
CodeGeneratorARM::linkAbsoluteLabels()
{
    // arm doesn't have deferred doubles, so this whole thing should be a NOP (right?)
    // deferred doubles are an x86 mechanism for loading doubles into registers by storing
    // them after the function body, then referring to them by their absolute address.
    // On arm, everything should just go in a pool.
# if 0
    JS_NOT_REACHED("Absolute Labels NYI");
    JSScript *script = gen->info().script();
    IonCode *method = script->ion->method();

    for (size_t i = 0; i < deferredDoubles_.length(); i++) {
        DeferredDouble *d = deferredDoubles_[i];
        const Value &v = script->ion->getConstant(d->index());
        MacroAssembler::Bind(method, d->label(), &v);
    }
#endif
}

bool
CodeGeneratorARM::visitDouble(LDouble *ins)
{

    const LDefinition *out = ins->getDef(0);
    const LConstantIndex *cindex = ins->getOperand(0)->toConstantIndex();
    const Value &v = graph.getConstant(cindex->index());

    masm.ma_vimm(v.toDouble(), ToFloatRegister(out));
    return true;
#if 0
    DeferredDouble *d = new DeferredDouble(cindex->index());
    if (!deferredDoubles_.append(d))
        return false;

    masm.movsd(d->label(), ToFloatRegister(out));
    return true;
#endif
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
    // OOPS.  It looks like we need a register to clobber here, and X86 doesn't
    // it also appears as if the type register is safe to clob here (at least in
    // the only case where this is presently called).  An actual temp register
    // should be reserved so this is not broken for other uses.
    Register tmp = value.typeReg();
    //DTRAddr lengthAndFlags = DTRAddr(string, DtrOffImm(JSString::offsetOfLengthAndFlags()));

    size_t mask = (0xFFFFFFFF << JSString::LENGTH_SHIFT);
    masm.ma_dtr(IsLoad, string, Imm32(JSString::offsetOfLengthAndFlags()), tmp);
    masm.ma_tst(Imm32(mask), tmp);
    return truthy ? Assembler::NonZero : Assembler::Zero;
}

bool
CodeGeneratorARM::visitTestDAndBranch(LTestDAndBranch *test)
{
    const LAllocation *opd = test->input();
    masm.as_vcmpz(VFPRegister(ToFloatRegister(opd)));
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

    Assembler::Condition cond = masm.compareDoubles(comp->jsop(), lhs, rhs);
    emitSet(cond, ToRegister(comp->output()));
    return false;
}

bool
CodeGeneratorARM::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::Condition cond = masm.compareDoubles(comp->jsop(), lhs, rhs);
    // TODO: we don't handle anything that has an undefined in it.
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    //    Assembler::Condition cond = masm.compareDoubles(comp->jsop(), lhs, rhs);

    return true;
}


bool
CodeGeneratorARM::visitLoadSlotV(LLoadSlotV *load)
{
    Register type = ToRegister(load->getDef(TYPE_INDEX));
    Register payload = ToRegister(load->getDef(PAYLOAD_INDEX));
    Register base = ToRegister(load->input());
    int32 offset = load->mir()->slot() * sizeof(js::Value);

    masm.ma_ldr(Operand(base, offset + NUNBOX32_TYPE_OFFSET), type);
    masm.ma_ldr(Operand(base, offset + NUNBOX32_PAYLOAD_OFFSET), payload);
    return true;
}

bool
CodeGeneratorARM::visitLoadSlotT(LLoadSlotT *load)
{
    Register base = ToRegister(load->input());
    int32 offset = load->mir()->slot() * sizeof(js::Value);

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
    int32 offset = store->mir()->slot() * sizeof(js::Value);

    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();

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
CodeGeneratorARM::visitLoadElementV(LLoadElementV *load)
{
    Register type = ToRegister(load->getDef(TYPE_INDEX));
    Register payload = ToRegister(load->getDef(PAYLOAD_INDEX));
    Register base = ToRegister(load->elements());
    if (load->index()->isConstant()) {
        masm.loadValue(Address(base, ToInt32(load->index()) * sizeof(Value)), ValueOperand(type, payload));
    } else {
        masm.loadValue(base, ToRegister(load->index()), ValueOperand(type, payload));
    }

    if (load->mir()->needsHoleCheck()) {
        masm.ma_cmp(type, ImmTag(JSVAL_TAG_MAGIC));
        return bailoutIf(Assembler::Equal, load->snapshot());
    }

    return true;
}

bool
CodeGeneratorARM::visitLoadElementT(LLoadElementT *load)
{
    Register base = ToRegister(load->elements());
    if (load->mir()->type() == MIRType_Double) {
        if (load->index()->isConstant()) {
            masm.loadInt32OrDouble(Address(base,ToInt32(load->index()) * sizeof(Value)), ToFloatRegister(load->output()));
        } else {
            masm.loadInt32OrDouble(base, ToRegister(load->index()), ToFloatRegister(load->output()));
        }
    } else {
        if (load->index()->isConstant()) {
            masm.load32(Address(base, ToInt32(load->index()) * sizeof(Value)), ToRegister(load->output()));
        } else {
            masm.ma_ldr(DTRAddr(base, DtrRegImmShift(ToRegister(load->index()), LSL, 3)),
                        ToRegister(load->output()));
        }
    }
    JS_ASSERT(!load->mir()->needsHoleCheck());
    return true;
}

bool
CodeGeneratorARM::visitStoreElementV(LStoreElementV *store)
{
    Register type = ToRegister(store->getDef(TYPE_INDEX));
    Register payload = ToRegister(store->getDef(PAYLOAD_INDEX));
    Register base = ToRegister(store->elements());
    if (store->index()->isConstant()) {
        masm.storeValue(ValueOperand(type, payload), Address(base, ToInt32(store->index()) * sizeof(Value)));
    } else {
        masm.storeValue(ValueOperand(type, payload), base, ToRegister(store->index()));
    }
    return true;
}

bool
CodeGeneratorARM::visitStoreElementT(LStoreElementT *store)
{
    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();
    Register base = ToRegister(store->elements());
    if (store->index()->isConstant()) {
        int32 index = ToInt32(store->index()) * sizeof(Value);
        Address dest = Address(base, index);
        if (valueType == MIRType_Double) {
            masm.ma_vstr(ToFloatRegister(value), Operand(dest));
            return true;
        }

        // Store the type tag if needed.
        if (valueType != store->mir()->elementType())
            masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), dest);

        // Store the payload.
        if (value->isConstant())
            masm.storePayload(*value->toConstant(), dest);
        else
            masm.storePayload(ToRegister(value), dest);
    } else {
        Register index = ToRegister(store->index());
        if (valueType == MIRType_Double) {
            masm.ma_vstr(ToFloatRegister(value), base, index);
            return true;
        }

        // Store the type tag if needed.
        if (valueType != store->mir()->elementType())
            masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), base, index);

        // Store the payload.
        if (value->isConstant())
            masm.storePayload(*value->toConstant(), base, index);
        else
            masm.storePayload(ToRegister(value), base, index);
    }
    return false;
}

bool
CodeGeneratorARM::visitBoundsCheck(LBoundsCheck *lir)
{
    if (lir->index()->isConstant())
        masm.ma_cmp(ToRegister(lir->length()), Imm32(ToInt32(lir->index())));
    else
        masm.ma_cmp(ToRegister(lir->length()), ToRegister(lir->index()));
    return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
}

bool
CodeGeneratorARM::visitGuardShape(LGuardShape *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());
    masm.ma_ldr(DTRAddr(obj, DtrOffImm(JSObject::offsetOfShape())), tmp);
    masm.ma_cmp(tmp, ImmGCPtr(guard->mir()->shape()));

    if (!bailoutIf(Assembler::NotEqual, guard->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorARM::visitGuardClass(LGuardClass *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadObjClass(obj, tmp);
    masm.ma_cmp(tmp, Imm32((uint32)guard->mir()->getClass()));
    if (!bailoutIf(Assembler::NotEqual, guard->snapshot()))
        return false;
    return true;
}


bool
CodeGeneratorARM::visitWriteBarrierT(LWriteBarrierT *barrier)
{
    // TODO: Perform C++ call to some WriteBarrier stub.
    // For now, we just breakpoint.
    masm.breakpoint();
    masm.breakpoint();
    return true;
}

bool
CodeGeneratorARM::visitImplicitThis(LImplicitThis *lir)
{
    Register callee = ToRegister(lir->callee());
    Register type = ToRegister(lir->getDef(TYPE_INDEX));
    Register payload = ToRegister(lir->getDef(PAYLOAD_INDEX));

    // The implicit |this| is always |undefined| if the function's environment
    // is the current global.
    masm.ma_ldr(DTRAddr(callee, DtrOffImm(JSFunction::offsetOfEnvironment())), type);
    masm.ma_cmp(type, ImmGCPtr(gen->info().script()->global()));

    // TODO: OOL stub path.
    if (!bailoutIf(Assembler::NotEqual, lir->snapshot()))
        return false;

    masm.moveValue(UndefinedValue(), type, payload);
    return true;
}

bool
CodeGeneratorARM::visitRecompileCheck(LRecompileCheck *lir)
{
    Register tmp = ToRegister(lir->tempInt());
    const size_t *addr = gen->info().script()->addressOfUseCount();

    // Bump the script's use count. Note that it's safe to bake in this pointer
    // since scripts are never nursery allocated and jitcode will be purged before
    // doing a compacting GC.
    masm.load32(ImmWord(addr), tmp);
    masm.ma_add(Imm32(1), tmp);
    masm.store32(tmp, ImmWord(addr));

    // Bailout if the script is hot.
    masm.ma_cmp(tmp, Imm32(js_IonOptions.usesBeforeInlining));
    if (!bailoutIf(Assembler::AboveOrEqual, lir->snapshot()))
        return false;
    return true;
}
