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

#include "MoveEmitter-arm.h"

using namespace js;
using namespace js::ion;

MoveEmitterARM::MoveEmitterARM(MacroAssembler &masm)
  : inCycle_(false),
    masm(masm),
    pushedAtCycle_(-1),
    pushedAtSpill_(-1),
    pushedAtDoubleSpill_(-1),
    spilledReg_(InvalidReg),
    spilledFloatReg_(InvalidFloatReg)
{
    pushedAtStart_ = masm.framePushed();
}

void
MoveEmitterARM::emit(const MoveResolver &moves)
{
    if (moves.hasCycles()) {
        // Reserve stack for cycle resolution
        masm.reserveStack(sizeof(double));
        pushedAtCycle_ = masm.framePushed();
    }

    for (size_t i = 0; i < moves.numMoves(); i++)
        emit(moves.getMove(i));
}

MoveEmitterARM::~MoveEmitterARM()
{
    assertDone();
}

Operand
MoveEmitterARM::cycleSlot() const
{
    return Operand(StackPointer, masm.framePushed() - pushedAtCycle_);
}

Operand
MoveEmitterARM::spillSlot() const
{
    return Operand(StackPointer, masm.framePushed() - pushedAtSpill_);
}

Operand
MoveEmitterARM::doubleSpillSlot() const
{
    return Operand(StackPointer, masm.framePushed() - pushedAtDoubleSpill_);
}

Operand
MoveEmitterARM::toOperand(const MoveOperand &operand) const
{
    if (operand.isMemory()) {
        if (operand.base() != StackPointer)
            return Operand(operand.base(), operand.disp());

        JS_ASSERT(operand.disp() >= 0);

        // Otherwise, the stack offset may need to be adjusted.
        return Operand(StackPointer, operand.disp() + (masm.framePushed() - pushedAtStart_));
    }
    if (operand.isGeneralReg())
        return Operand(operand.reg());

    JS_ASSERT(operand.isFloatReg());
    return Operand(operand.floatReg());
}

Register
MoveEmitterARM::tempReg()
{
    if (spilledReg_ != InvalidReg)
        return spilledReg_;

    // For now, just pick ecx/rcx as the eviction point. This is totally
    // random, and if it ends up being bad, we can use actual heuristics later.
    spilledReg_ = Register::FromCode(2);
    if (pushedAtSpill_ == -1) {
        masm.Push(spilledReg_);
        pushedAtSpill_ = masm.framePushed();
    } else {
        masm.mov(spilledReg_, spillSlot());
    }
    return spilledReg_;
}

FloatRegister
MoveEmitterARM::tempFloatReg()
{
    if (spilledFloatReg_ != InvalidFloatReg)
        return spilledFloatReg_;

    // For now, just pick xmm7 as the eviction point. This is totally random,
    // and if it ends up being bad, we can use actual heuristics later.
    spilledFloatReg_ = FloatRegister::FromCode(7);
    if (pushedAtDoubleSpill_ == -1) {
        masm.reserveStack(sizeof(double));
        pushedAtDoubleSpill_ = masm.framePushed();
    }
    masm.movsd(spilledFloatReg_, doubleSpillSlot());
    return spilledFloatReg_;
}

void
MoveEmitterARM::breakCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (A -> B), which we reach first. We save B, then allow
    // the original move to continue.
    if (to.isDouble()) {
        if (to.isMemory()) {
            FloatRegister temp = tempFloatReg();
            masm.movsd(toOperand(to), temp);
            masm.movsd(temp, cycleSlot());
        } else {
            masm.movsd(to.floatReg(), cycleSlot());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.mov(toOperand(to), temp);
            masm.mov(temp, cycleSlot());
        } else {
            masm.mov(to.reg(), cycleSlot());
        }
    }
}

void
MoveEmitterARM::completeCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (B -> A), which we reach last. We emit a move from the
    // saved value of B, to A.
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            FloatRegister temp = tempFloatReg();
            masm.movsd(cycleSlot(), temp);
            masm.movsd(temp, toOperand(to));
        } else {
            masm.movsd(cycleSlot(), to.floatReg());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.mov(cycleSlot(), temp);
            masm.mov(temp, toOperand(to));
        } else {
            masm.mov(cycleSlot(), to.reg());
        }
    }
}

void
MoveEmitterARM::emitMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isGeneralReg()) {
        if (from.reg() == spilledReg_) {
            // If the source is a register that has been spilled, make sure
            // to load the source back into that register.
            masm.mov(spillSlot(), spilledReg_);
            spilledReg_ = InvalidReg;
        }
        masm.mov(from.reg(), toOperand(to));
    } else if (to.isGeneralReg()) {
        if (to.reg() == spilledReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledReg_ = InvalidReg;
        }
        masm.mov(toOperand(from), to.reg());
    } else {
        // Memory to memory gpr move.
        Register reg = tempReg();
        masm.mov(toOperand(from), reg);
        masm.mov(reg, toOperand(to));
    }
}

void
MoveEmitterARM::emitDoubleMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isFloatReg()) {
        if (from.floatReg() == spilledFloatReg_) {
            // If the source is a register that has been spilled, make
            // sure to load the source back into that register.
            masm.movsd(doubleSpillSlot(), spilledFloatReg_);
            spilledFloatReg_ = InvalidFloatReg;
        }
        masm.movsd(from.floatReg(), toOperand(to));
    } else if (to.isFloatReg()) {
        if (to.floatReg() == spilledFloatReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledFloatReg_ = InvalidFloatReg;
        }
        masm.movsd(toOperand(from), to.floatReg());
    } else {
        // Memory to memory float move.
        FloatRegister reg = tempFloatReg();
        masm.movsd(toOperand(from), reg);
        masm.movsd(reg, toOperand(to));
    }
}

void
MoveEmitterARM::emit(const Move &move)
{
    const MoveOperand &from = move.from();
    const MoveOperand &to = move.to();

    if (move.inCycle()) {
        if (inCycle_) {
            completeCycle(from, to, move.kind());
            inCycle_ = false;
            return;
        }

        breakCycle(from, to, move.kind());
        inCycle_ = true;
    }

    if (move.kind() == Move::DOUBLE)
        emitDoubleMove(from, to);
    else
        emitMove(from, to);
}

void
MoveEmitterARM::assertDone()
{
    JS_ASSERT(!inCycle_);
}

void
MoveEmitterARM::finish()
{
    assertDone();

    if (pushedAtDoubleSpill_ != -1 && spilledFloatReg_ != InvalidFloatReg)
        masm.movsd(doubleSpillSlot(), spilledFloatReg_);
    if (pushedAtSpill_ != -1 && spilledReg_ != InvalidReg)
        masm.mov(spillSlot(), spilledReg_);

    masm.freeStack(masm.framePushed() - pushedAtStart_);
}

