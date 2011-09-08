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
#include "jscntxt.h"
#include "jscompartment.h"
#include "CodeGenerator-x86-shared.h"
#include "CodeGenerator-shared-inl.h"
#include "ion/IonFrames.h"
#include "ion/MoveEmitter.h"
#include "ion/IonCompartment.h"

using namespace js;
using namespace js::ion;

class DeferredJumpTable : public DeferredData
{
    LTableSwitch *lswitch;

  public:
    DeferredJumpTable(LTableSwitch *lswitch)
      : lswitch(lswitch)
    { }
    
    void copy(IonCode *code, uint8 *buffer) const {
        void **jumpData = (void **)buffer;

        // For every case write the pointer to the start in the table
        for (uint j = 0; j < lswitch->mir()->numCases(); j++) { 
            LBlock *caseblock = lswitch->mir()->getCase(j)->lir();
            Label *caseheader = caseblock->label();

            uint32 offset = caseheader->offset();
            *jumpData = (void *)(code->raw() + offset);
            jumpData++;
        }
    }
};

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorShared(gen, graph),
    deoptLabel_(NULL)
{
}

bool
CodeGeneratorX86Shared::generatePrologue()
{
    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameSize());

    // Allocate returnLabel_ on the heap, so we don't run it's destructor and
    // assert-not-bound in debug mode on compilation failure.
    returnLabel_ = new HeapLabel();

    return true;
}

bool
CodeGeneratorX86Shared::generateEpilogue()
{
    masm.bind(returnLabel_);

    // Pop the stack we allocated at the start of the function.
    masm.freeStack(frameSize());
    JS_ASSERT(masm.framePushed() == 0);

    masm.ret();
    return true;
}

bool
OutOfLineBailout::accept(CodeGeneratorX86Shared *codegen)
{
    return codegen->visitOutOfLineBailout(this);
}

bool
CodeGeneratorX86Shared::visitGoto(LGoto *jump)
{
    LBlock *target = jump->target()->lir();

    // Don't bother emitting a jump if we'll flow through to the next block.
    if (isNextBlock(target))
        return true;

    masm.jmp(target->label());
    return true;
}

void
CodeGeneratorX86Shared::emitBranch(Assembler::Condition cond, MBasicBlock *mirTrue, MBasicBlock *mirFalse)
{
    LBlock *ifTrue = mirTrue->lir();
    LBlock *ifFalse = mirFalse->lir();
    if (isNextBlock(ifFalse)) {
        masm.j(cond, ifTrue->label());
    } else {
        masm.j(Assembler::InvertCondition(cond), ifFalse->label());
        if (!isNextBlock(ifTrue))
            masm.jmp(ifTrue->label());
    }
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
    masm.xorpd(ScratchFloatReg, ScratchFloatReg);
    masm.ucomisd(ToFloatRegister(opd), ScratchFloatReg);
    emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
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

void
CodeGeneratorX86Shared::emitSet(Assembler::Condition cond, const Register &dest)
{
    if (GeneralRegisterSet(Registers::SingleByteRegs).has(dest)) {
        // If the register we're defining is a single byte register,
        // take advantage of the setCC instruction
        masm.setCC(cond, dest);
        masm.movzxbl(dest, dest);
    } else {
        Label ifTrue;
        masm.movl(Imm32(1), dest);
        masm.j(cond, &ifTrue);
        masm.xorl(dest, dest);
        masm.bind(&ifTrue);
    }
}

bool
CodeGeneratorX86Shared::visitCompareI(LCompareI *comp)
{
    masm.cmpl(ToRegister(comp->left()), ToOperand(comp->right()));
    emitSet(JSOpToCondition(comp->jsop()), ToRegister(comp->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareIAndBranch(LCompareIAndBranch *comp)
{
    Assembler::Condition cond = JSOpToCondition(comp->jsop());
    masm.cmpl(ToRegister(comp->left()), ToOperand(comp->right()));
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareD(LCompareD *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::Condition cond = masm.compareDoubles(comp->jsop(), lhs, rhs);
    emitSet(cond, ToRegister(comp->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::Condition cond = masm.compareDoubles(comp->jsop(), lhs, rhs);
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_) {
        // All non-table-based bailouts will go here.
        masm.bind(deoptLabel_);
        
        // Push the frame size, so the handler can recover the IonScript.
        masm.push(Imm32(frameSize()));

        IonCompartment *ion = gen->cx->compartment->ionCompartment();
        IonCode *handler = ion->getGenericBailoutHandler(gen->cx);
        if (!handler)
            return false;

        masm.jmp(handler->raw(), Relocation::CODE);
    }

    return true;
}

class BailoutJump {
    Assembler::Condition cond_;

  public:
    BailoutJump(Assembler::Condition cond) : cond_(cond)
    { }
#ifdef JS_CPU_X86
    void operator()(MacroAssembler &masm, uint8 *code) const {
        masm.j(cond_, code, Relocation::EXTERNAL);
    }
#endif
    void operator()(MacroAssembler &masm, Label *label) const {
        masm.j(cond_, label);
    }
};

class BailoutLabel {
    Label *label_;

  public:
    BailoutLabel(Label *label) : label_(label)
    { }
#ifdef JS_CPU_X86
    void operator()(MacroAssembler &masm, uint8 *code) const {
        masm.retarget(label_, code, Relocation::EXTERNAL);
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
    JS_ASSERT_IF(frameClass_ != FrameSizeClass::None(),
                 frameClass_.frameSize() == masm.framePushed());

#ifdef JS_CPU_X86
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
    OutOfLineBailout *ool = new OutOfLineBailout(snapshot, masm.framePushed());
    if (!addOutOfLineCode(ool))
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
CodeGeneratorX86Shared::bailoutFrom(Label *label, LSnapshot *snapshot)
{
    JS_ASSERT(label->used() && !label->bound());
    return bailout(BailoutLabel(label), snapshot);
}

bool
CodeGeneratorX86Shared::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    masm.bind(ool->entry());

    if (!deoptLabel_)
        deoptLabel_ = new HeapLabel();

    masm.push(Imm32(ool->snapshot()->snapshotOffset()));
    masm.jmp(deoptLabel_);
    return true;
}

bool
CodeGeneratorX86Shared::visitAddI(LAddI *ins)
{
    if (ins->rhs()->isConstant())
        masm.addl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.addl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorX86Shared::visitSubI(LSubI *ins)
{
    if (ins->rhs()->isConstant())
        masm.subl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.subl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorX86Shared::visitMulI(LMulI *ins)
{
    const LAllocation *lhs = ins->lhs();
    const LAllocation *rhs = ins->rhs();
    MMul *mul = ins->mir();

    if (rhs->isConstant()) {
        // Bailout on -0.0
        int32 constant = ToInt32(rhs);
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
                int32 shift = JS_FloorLog2(constant);
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

        // Bailout on 0 (could be -0.0)
        if (mul->canBeNegativeZero()) {
            masm.testl(ToRegister(lhs), ToRegister(lhs));
            if (!bailoutIf(Assembler::Equal, ins->snapshot()))
                return false;
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

    JS_ASSERT(remainder == edx);
    JS_ASSERT(lhs == eax);

    // Prevent divide by zero.
    masm.testl(rhs, rhs);
    if (!bailoutIf(Assembler::Zero, ins->snapshot()))
        return false;

    // Prevent an integer overflow exception from -2147483648 / -1.
    Label notmin;
    masm.cmpl(lhs, Imm32(INT_MIN));
    masm.j(Assembler::NotEqual, &notmin);
    masm.cmpl(rhs, Imm32(-1));
    if (!bailoutIf(Assembler::Equal, ins->snapshot()))
        return false;
    masm.bind(&notmin);

    // Prevent negative 0.
    Label nonzero;
    masm.testl(lhs, lhs);
    masm.j(Assembler::NonZero, &nonzero);
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

    return true;
}

bool
CodeGeneratorX86Shared::visitBitNot(LBitNot *ins)
{
    const LAllocation *input = ins->getOperand(0);
    JS_ASSERT(!input->isConstant());

    masm.notl(ToOperand(input));
    return true;
}

bool
CodeGeneratorX86Shared::visitBitOp(LBitOp *ins)
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
            JS_NOT_REACHED("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitShiftOp(LShiftOp *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    switch (ins->bitop()) {
        case JSOP_LSH:
            if (rhs->isConstant())
                masm.shll(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs));
            else
                masm.shll_cl(ToRegister(lhs));
            break;
        case JSOP_RSH:
            if (rhs->isConstant())
                masm.sarl(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs));
            else
                masm.sarl_cl(ToRegister(lhs));
            break;
        case JSOP_URSH: {
            MUrsh *ursh = ins->mir()->toUrsh(); 
            if (rhs->isConstant())
                masm.shrl(Imm32(ToInt32(rhs) & 0x1F), ToRegister(lhs));
            else
                masm.shrl_cl(ToRegister(lhs));
 
            // Note: this is an unsigned operation.
            // We don't have a UINT32 type, so we will emulate this with INT32
            // The bit representation of an integer from ToInt32 and ToUint32 are the same.
            // So the inputs are ok.
            // But we need to bring the output back again from UINT32 to INT32.
            // Both representation overlap each other in the positive numbers. (in INT32)
            // So there is only a problem when solution (in INT32) is negative.
            if (ursh->canOverflow()) {
                masm.cmpl(ToOperand(lhs), Imm32(0));
                if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
                    return false;
            }
            break;
        }
        default:
            JS_NOT_REACHED("unexpected shift opcode");
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitInteger(LInteger *ins)
{
    const LDefinition *def = ins->getDef(0);
    masm.movl(Imm32(ins->getValue()), ToRegister(def));
    return true;
}

typedef MoveResolver::MoveOperand MoveOperand;

MoveOperand
CodeGeneratorX86Shared::toMoveOperand(const LAllocation *a) const
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    return MoveOperand(StackPointer, ToStackOffset(a));
}

bool
CodeGeneratorX86Shared::visitMoveGroup(LMoveGroup *group)
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

        if (!resolver.addMove(toMoveOperand(from), toMoveOperand(to), kind))
            return false;
    }

    if (!resolver.resolve())
        return false;

    MoveEmitter emitter(masm);
    emitter.emit(resolver);
    emitter.finish();

    return true;
}

bool
CodeGeneratorX86Shared::visitTableSwitch(LTableSwitch *ins)
{
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

    // Lower value with low value
    if (mir->low() != 0)
        masm.subl(Imm32(mir->low()), ToRegister(temp));

    // Jump to default case if input is out of range
    int32 cases = mir->numCases();
    masm.cmpl(ToRegister(temp), Imm32(cases));
    masm.j(AssemblerX86Shared::AboveOrEqual, defaultcase);

    // Create a JumpTable that during linking will get written.
    DeferredJumpTable *d = new DeferredJumpTable(ins);
    if (!masm.addDeferredData(d, (1 << ScalePointer) * cases))
        return false;
   
    // Compute the position where a pointer to the right case stands.
    const LAllocation *base = ins->tempPointer();
    masm.mov(d->label(), ToRegister(base));
    Operand pointer = Operand(ToRegister(base), ToRegister(temp), ScalePointer);

    // Jump to the right case
    masm.jmp(pointer);

    return true;
}

bool
CodeGeneratorX86Shared::visitCallGeneric(LCallGeneric *call)
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
    masm.movePtr(Operand(objreg, JSObject::offsetOfClassPointer()), tokreg);
    masm.cmpPtr(tokreg, ImmWord(&js::FunctionClass));
    if (!bailoutIf(Assembler::NotEqual, call->snapshot()))
        return false;

    // Guard that objreg is a non-native function:
    // Non-native iff (obj->flags & JSFUN_KINDMASK >= JSFUN_INTERPRETED).
    masm.movl(Operand(objreg, offsetof(JSFunction, flags)), tokreg);
    masm.andl(Imm32(JSFUN_KINDMASK), tokreg);
    masm.cmpl(tokreg, Imm32(JSFUN_INTERPRETED));
    if (!bailoutIf(Assembler::Below, call->snapshot()))
        return false;

    // Save the calleeToken, which equals the function object.
    masm.mov(objreg, tokreg);

    // Knowing that objreg is a non-native function, load the JSScript.
    masm.movePtr(Operand(objreg, offsetof(JSFunction, u.i.script)), objreg);
    masm.movePtr(Operand(objreg, offsetof(JSScript, ion)), objreg);

    // Bail if the callee has not yet been JITted.
    masm.testPtr(objreg, objreg);
    if (!bailoutIf(Assembler::Zero, call->snapshot()))
        return false;

    // Remember the size of the frame above this point, in case of bailout.
    uint32 stack_size = masm.framePushed() - unused_stack;
    // Mark !IonFramePrefix::isEntryFrame().
    uint32 size_descriptor = stack_size << 1;

    // Nestle %esp up to the argument vector.
    if (unused_stack)
        masm.addPtr(Imm32(unused_stack), StackPointer);

    // Construct the IonFramePrefix.
    masm.push(tokreg);
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
        masm.load16(Operand(tokreg, offsetof(JSFunction, nargs)), nargsreg);
        masm.cmpl(nargsreg, Imm32(call->nargs()));
        masm.j(Assembler::Above, &thunk);

        // No argument fixup needed. Call the function normally.
        masm.movePtr(Operand(objreg, offsetof(IonScript, method_)), objreg);
        masm.movePtr(Operand(objreg, IonCode::OffsetOfCode()), objreg);
        masm.call(objreg);
        masm.jump(&rejoin);

        // Argument fixup needed. Create a frame with correct |nargs| and then call.
        masm.bind(&thunk);
        masm.mov(Imm32(call->nargs()), ArgumentsRectifierReg);
        masm.movePtr(ImmWord(argumentsRectifier->raw()), ecx); // safe to take: return reg.
        masm.call(ecx);

        masm.bind(&rejoin);
    }

    // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
    int prefix_garbage = 2 * sizeof(void *);
    int restore_diff = prefix_garbage - unused_stack;
    
    if (restore_diff > 0)
        masm.addPtr(Imm32(restore_diff), StackPointer);
    else if (restore_diff < 0)
        masm.subPtr(Imm32(-restore_diff), StackPointer);

    return true;
}

bool
CodeGeneratorX86Shared::visitMathD(LMathD *math)
{
    const LAllocation *input = math->getOperand(1);
    const LDefinition *output = math->getDef(0);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.addsd(ToFloatRegister(input), ToFloatRegister(output));
        break;
      case JSOP_SUB:
        masm.subsd(ToFloatRegister(input), ToFloatRegister(output));
        break;
      case JSOP_MUL:
        masm.mulsd(ToFloatRegister(input), ToFloatRegister(output));
        break;
      case JSOP_DIV:
        masm.divsd(ToFloatRegister(input), ToFloatRegister(output));
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
void
CodeGeneratorX86Shared::emitDoubleToInt32(const FloatRegister &src, const Register &dest, Label *fail)
{
    // Note that we don't specify the destination width for the truncated
    // conversion to integer. x64 will use the native width (quadword) which
    // sign-extends the top bits, preserving a little sanity.
    masm.cvttsd2s(src, dest);
    masm.cvtsi2sd(dest, ScratchFloatReg);
    masm.ucomisd(src, ScratchFloatReg);
    masm.j(Assembler::Parity, fail);
    masm.j(Assembler::NotEqual, fail);

    // Check for -0
    Label notZero;
    masm.testl(dest, dest);
    masm.j(Assembler::NonZero, &notZero);

    if (Assembler::HasSSE41()) {
        masm.ptest(src, src);
        masm.j(Assembler::NonZero, fail);
    } else {
        // bit 0 = sign of low double
        // bit 1 = sign of high double
        masm.movmskpd(src, dest);
        masm.andl(Imm32(1), dest);
        masm.j(Assembler::NonZero, fail);
    }
    
    masm.bind(&notZero);
}

JS_STATIC_ASSERT(INT_MIN == int(0x80000000));

void
CodeGeneratorX86Shared::emitTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail)
{
    masm.cvttsd2si(src, dest);
    masm.cmpl(dest, Imm32(INT_MIN));
    masm.j(Assembler::Equal, fail);
}

