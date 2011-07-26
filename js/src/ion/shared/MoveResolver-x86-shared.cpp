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
 *   David Anderson <dvander@alliedmods.net>
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
#if defined(JS_CPU_X86)
# include "ion/x86/CodeGenerator-x86.h"
#else
# include "ion/x64/CodeGenerator-x64.h"
#endif
#include "CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::ion;

MoveResolverX86::MoveResolverX86(CodeGenerator *codegen)
  : inCycle_(false),
    codegen(codegen),
    masm(&codegen->masm),
    pushedAtCycle_(-1),
    pushedAtSpill_(-1),
    pushedAtDoubleSpill_(-1),
    spilledReg_(InvalidReg),
    spilledFloatReg_(InvalidFloatReg)
{
}

MoveResolverX86::~MoveResolverX86()
{
}

void
MoveResolverX86::setup(LMoveGroup *group)
{
    pushedAtCycle_ = -1;
    freeRegs_ = group->freeRegs();

    if (codegen->moveGroupResolver.hasCycles()) {
        cycleReg_ = freeRegs_.empty(false) ? InvalidReg : freeRegs_.takeGeneral();
        cycleFloatReg_ = freeRegs_.empty(true) ? InvalidFloatReg : freeRegs_.takeFloat();

        // No free registers to resolve a potential cycle, so reserve stack.
        if (cycleReg_ == InvalidReg || cycleFloatReg_ == InvalidFloatReg) {
            masm->reserveStack(sizeof(double));
            pushedAtCycle_ = masm->framePushed();
        }
    }

    spilledReg_ = InvalidReg;
    spilledFloatReg_ = InvalidFloatReg;
    pushedAtSpill_ = -1;
    pushedAtDoubleSpill_ = -1;
}

Operand
MoveResolverX86::cycleSlot() const
{
    return Operand(StackPointer, masm->framePushed() - pushedAtCycle_);
}

Operand
MoveResolverX86::spillSlot() const
{
    return Operand(StackPointer, masm->framePushed() - pushedAtSpill_);
}

Operand
MoveResolverX86::doubleSpillSlot() const
{
    return Operand(StackPointer, masm->framePushed() - pushedAtDoubleSpill_);
}

Register
MoveResolverX86::tempReg()
{
    if (spilledReg_ != InvalidReg)
        return spilledReg_;

    if (!freeRegs_.empty(false)) {
        spilledReg_ = freeRegs_.takeGeneral();
        return spilledReg_;
    }

    // No registers are free. For now, just pick ecx/rcx as the eviction
    // point. This is totally random, and if it ends up being bad, we can
    // use actual heuristics later.
    spilledReg_ = Register::FromCode(2);
    if (pushedAtSpill_ == -1) {
        masm->Push(spilledReg_);
        pushedAtSpill_ = masm->framePushed();
    } else {
        codegen->masm.mov(spilledReg_, spillSlot());
    }
    return spilledReg_;
}

FloatRegister
MoveResolverX86::tempFloatReg()
{
    if (spilledFloatReg_ != InvalidFloatReg)
        return spilledFloatReg_;

    if (!freeRegs_.empty(true)) {
        spilledFloatReg_ = freeRegs_.takeFloat();
        return spilledFloatReg_;
    }

    // No registers are free. For now, just pick xmm7 as the eviction
    // point. This is totally random, and if it ends up being bad, we can
    // use actual heuristics later.
    spilledFloatReg_ = FloatRegister::FromCode(7);
    if (pushedAtDoubleSpill_ == -1) {
        masm->reserveStack(sizeof(double));
        pushedAtDoubleSpill_ = masm->framePushed();
    }
    codegen->masm.movsd(spilledFloatReg_, doubleSpillSlot());
    return spilledFloatReg_;
}

void
MoveResolverX86::breakCycle(const LAllocation *from, const LAllocation *to)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (A -> B), which we reach first. We save B, then allow
    // the original move to continue.
    if (to->isDouble()) {
        if (cycleFloatReg_ != InvalidFloatReg) {
            codegen->masm.movsd(codegen->ToOperand(to), cycleFloatReg_);
        } else if (to->isMemory()) {
            FloatRegister temp = tempFloatReg();
            codegen->masm.movsd(codegen->ToOperand(to), temp);
            codegen->masm.movsd(temp, cycleSlot());
        } else {
            codegen->masm.movsd(ToFloatRegister(to), cycleSlot());
        }
    } else {
        if (cycleReg_ != InvalidReg) {
            codegen->masm.mov(codegen->ToOperand(to), cycleReg_);
        } else if (to->isMemory()) {
            Register temp = tempReg();
            codegen->masm.mov(codegen->ToOperand(to), temp);
            codegen->masm.mov(temp, cycleSlot());
        } else {
            codegen->masm.mov(ToRegister(to), cycleSlot());
        }
    }
}

void
MoveResolverX86::completeCycle(const LAllocation *from, const LAllocation *to)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (B -> A), which we reach last. We emit a move from the
    // saved value of B, to A.
    if (from->isDouble()) {
        if (cycleFloatReg_ != InvalidFloatReg) {
            codegen->masm.movsd(cycleFloatReg_, codegen->ToOperand(to));
        } else if (to->isMemory()) {
            FloatRegister temp = tempFloatReg();
            codegen->masm.movsd(cycleSlot(), temp);
            codegen->masm.movsd(temp, codegen->ToOperand(to));
        } else {
            codegen->masm.movsd(cycleSlot(), ToFloatRegister(to));
        }
    } else {
        if (cycleReg_ != InvalidReg) {
            codegen->masm.mov(cycleReg_, codegen->ToOperand(to));
        } else if (to->isMemory()) {
            Register temp = tempReg();
            codegen->masm.mov(cycleSlot(), temp);
            codegen->masm.mov(temp, codegen->ToOperand(to));
        } else {
            codegen->masm.mov(cycleSlot(), ToRegister(to));
        }
    }
}

void
MoveResolverX86::emitMove(const LAllocation *from, const LAllocation *to)
{
    if (from->isGeneralReg()) {
        if (ToRegister(from) == spilledReg_) {
            // If the source is a register that has been spilled, make sure
            // to load the source back into that register.
            codegen->masm.mov(spillSlot(), spilledReg_);
            spilledReg_ = InvalidReg;
        }
        codegen->masm.mov(ToRegister(from), codegen->ToOperand(to));
    } else if (to->isGeneralReg()) {
        if (ToRegister(to) == spilledReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledReg_ = InvalidReg;
        }
        codegen->masm.mov(codegen->ToOperand(from), ToRegister(to));
    } else {
        // Memory to memory gpr move.
        Register reg = tempReg();
        codegen->masm.mov(codegen->ToOperand(from), reg);
        codegen->masm.mov(reg, codegen->ToOperand(to));
    }
}

void
MoveResolverX86::emitDoubleMove(const LAllocation *from, const LAllocation *to)
{
    if (from->isFloatReg()) {
        if (ToFloatRegister(from) == spilledFloatReg_) {
            // If the source is a register that has been spilled, make
            // sure to load the source back into that register.
            codegen->masm.movsd(doubleSpillSlot(), spilledFloatReg_);
            spilledFloatReg_ = InvalidFloatReg;
        }
        codegen->masm.movsd(ToFloatRegister(from), codegen->ToOperand(to));
    } else if (to->isFloatReg()) {
        if (ToFloatRegister(to) == spilledFloatReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledFloatReg_ = InvalidFloatReg;
        }
        codegen->masm.movsd(codegen->ToOperand(from), ToFloatRegister(to));
    } else {
        // Memory to memory float move.
        FloatRegister reg = tempFloatReg();
        codegen->masm.movsd(codegen->ToOperand(from), reg);
        codegen->masm.movsd(reg, codegen->ToOperand(to));
    }
}

void
MoveResolverX86::assertValidMove(const LAllocation *from, const LAllocation *to)
{
    JS_ASSERT(from->isDouble() == to->isDouble());
    JS_ASSERT_IF(from->isGeneralReg(), !freeRegs_.has(ToRegister(from)));
    JS_ASSERT_IF(to->isGeneralReg(), !freeRegs_.has(ToRegister(to)));
    JS_ASSERT_IF(from->isFloatReg(), !freeRegs_.has(ToFloatRegister(from)));
    JS_ASSERT_IF(to->isFloatReg(), !freeRegs_.has(ToFloatRegister(to)));
}

void
MoveResolverX86::emit(const MoveGroupResolver::Move &move)
{
    const LAllocation *from = move.from();
    const LAllocation *to = move.to();
    
    if (move.inCycle()) {
        if (inCycle_) {
            completeCycle(from, to);
            inCycle_ = false;
            return;
        }

        inCycle_ = true;
        completeCycle(from, to);
    }
    
    if (!from->isDouble())
        emitMove(from, to);
    else
        emitDoubleMove(from, to);
}

void
MoveResolverX86::assertDone()
{
    JS_ASSERT(!inCycle_);
}

void
MoveResolverX86::finish()
{
    assertDone();

    int32 decrement = 0;

    if (pushedAtDoubleSpill_ != -1) {
        if (spilledFloatReg_ != InvalidFloatReg)
            codegen->masm.movsd(doubleSpillSlot(), spilledFloatReg_);
        decrement += sizeof(double);
    }
    if (pushedAtSpill_ != -1) {
        if (spilledReg_ != InvalidReg)
            codegen->masm.mov(spillSlot(), spilledReg_);
        decrement += STACK_SLOT_SIZE;
    }
    if (pushedAtCycle_ != -1)
        decrement += sizeof(double);

    masm->freeStack(decrement);
}

