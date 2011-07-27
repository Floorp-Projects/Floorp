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
#include "CodeGenerator-x86-shared.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "CodeGenerator-shared-inl.h"
#include "ion/IonFrames.h"
#include "ion/MoveEmitter.h"

using namespace js;
using namespace js::ion;

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorShared(gen, graph)
{
}

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
    if (class_ < JS_ARRAY_LENGTH(FrameSizes))
        return FrameSizes[class_];

    uint32 step = class_ - JS_ARRAY_LENGTH(FrameSizes);
    return LAST_FRAME_SIZE + step * LAST_FRAME_INCREMENT;
}

bool
CodeGeneratorX86Shared::generatePrologue()
{
    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameStaticSize_);

    // Allocate returnLabel_ on the heap, so we don't run it's destructor and
    // assert-not-bound in debug mode on compilation failure.
    returnLabel_ = gen->allocate<Label>();
    new (returnLabel_) Label();

    return true;
}

bool
CodeGeneratorX86Shared::generateEpilogue()
{
    masm.bind(returnLabel_);
    masm.freeStack(frameStaticSize_);
    masm.ret();
    return true;
}

bool
CodeGeneratorX86Shared::visitLabel(LLabel *label)
{
    masm.bind(label->label());
    return true;
}

bool
CodeGeneratorX86Shared::visitGoto(LGoto *jump)
{
    LBlock *target = jump->target()->lir();
    LLabel *header = target->begin()->toLabel();

    // Don't bother emitting a jump if we'll flow through to the next block.
    if (current->mir()->id() + 1 == target->mir()->id())
        return true;

    masm.jmp(header->label());
    return true;
}

bool
CodeGeneratorX86Shared::visitAddI(LAddI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    if (rhs->isConstant())
        masm.addl(Imm32(ToInt32(rhs)), ToOperand(lhs));
    else
        masm.addl(ToOperand(rhs), ToRegister(lhs));

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
CodeGeneratorX86Shared::visitInteger(LInteger *ins)
{
    const LDefinition *def = ins->getDef(0);
    masm.movl(Imm32(ins->getValue()), ToRegister(def));
    return true;
}

typedef MoveResolver::MoveOperand MoveOperand;

MoveOperand
CodeGeneratorX86Shared::toMoveOperand(const LAllocation *a)
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    int32 disp = a->isStackSlot()
                 ? SlotToStackOffset(a->toStackSlot()->slot())
                 : ArgToStackOffset(a->toArgument()->index());
    return MoveOperand(StackPointer, disp);
}

bool
CodeGeneratorX86Shared::visitMoveGroup(LMoveGroup *group)
{
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
    emitter.emit(resolver, group->freeRegs());
    emitter.finish();

    return true;
}

