/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoveEmitter-x86-shared.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

MoveEmitterX86::MoveEmitterX86(MacroAssemblerSpecific &masm)
  : inCycle_(false),
    masm(masm),
    pushedAtCycle_(-1),
    pushedAtSpill_(-1),
    spilledReg_(InvalidReg)
{
    pushedAtStart_ = masm.framePushed();
}

void
MoveEmitterX86::emit(const MoveResolver &moves)
{
    if (moves.hasCycles()) {
        // Reserve stack for cycle resolution
        masm.reserveStack(sizeof(double));
        pushedAtCycle_ = masm.framePushed();
    }

    for (size_t i = 0; i < moves.numMoves(); i++)
        emit(moves.getMove(i));
}

MoveEmitterX86::~MoveEmitterX86()
{
    assertDone();
}

Operand
MoveEmitterX86::cycleSlot() const
{
    return Operand(StackPointer, masm.framePushed() - pushedAtCycle_);
}

Operand
MoveEmitterX86::spillSlot() const
{
    return Operand(StackPointer, masm.framePushed() - pushedAtSpill_);
}

Operand
MoveEmitterX86::toOperand(const MoveOperand &operand) const
{
    if (operand.isMemory() || operand.isEffectiveAddress()) {
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
MoveEmitterX86::tempReg()
{
    if (spilledReg_ != InvalidReg)
        return spilledReg_;

    // For now, just pick edx/rdx as the eviction point. This is totally
    // random, and if it ends up being bad, we can use actual heuristics later.
    spilledReg_ = edx;

#ifdef JS_CPU_X64
    JS_ASSERT(edx == rdx);
#endif

    if (pushedAtSpill_ == -1) {
        masm.Push(spilledReg_);
        pushedAtSpill_ = masm.framePushed();
    } else {
        masm.mov(spilledReg_, spillSlot());
    }
    return spilledReg_;
}

void
MoveEmitterX86::breakCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (A -> B), which we reach first. We save B, then allow
    // the original move to continue.
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            masm.movsd(toOperand(to), ScratchFloatReg);
            masm.movsd(ScratchFloatReg, cycleSlot());
        } else {
            masm.movsd(to.floatReg(), cycleSlot());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.mov(toOperand(to), temp);
            masm.mov(temp, cycleSlot());
        } else {
            if (to.reg() == spilledReg_) {
                // If the destination was spilled, restore it first.
                masm.mov(spillSlot(), spilledReg_);
                spilledReg_ = InvalidReg;
            }
            masm.mov(to.reg(), cycleSlot());
        }
    }
}

void
MoveEmitterX86::completeCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (B -> A), which we reach last. We emit a move from the
    // saved value of B, to A.
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            masm.movsd(cycleSlot(), ScratchFloatReg);
            masm.movsd(ScratchFloatReg, toOperand(to));
        } else {
            masm.movsd(cycleSlot(), to.floatReg());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.mov(cycleSlot(), temp);
            masm.mov(temp, toOperand(to));
        } else {
            if (to.reg() == spilledReg_) {
                // Make sure we don't re-clobber the spilled register later.
                spilledReg_ = InvalidReg;
            }
            masm.mov(cycleSlot(), to.reg());
        }
    }
}

void
MoveEmitterX86::emitMove(const MoveOperand &from, const MoveOperand &to)
{
    if (to.isGeneralReg() && to.reg() == spilledReg_) {
        // If the destination is the spilled register, make sure we
        // don't re-clobber its value.
        spilledReg_ = InvalidReg;
    }

    if (from.isGeneralReg()) {
        if (from.reg() == spilledReg_) {
            // If the source is a register that has been spilled, make sure
            // to load the source back into that register.
            masm.mov(spillSlot(), spilledReg_);
            spilledReg_ = InvalidReg;
        }
        masm.mov(from.reg(), toOperand(to));
    } else if (to.isGeneralReg()) {
        JS_ASSERT(from.isMemory() || from.isEffectiveAddress());
        if (from.isMemory())
            masm.mov(toOperand(from), to.reg());
        else
            masm.lea(toOperand(from), to.reg());
    } else {
        // Memory to memory gpr move.
        Register reg = tempReg();
        // Reload its previous value from the stack.
        if (reg == from.base())
            masm.mov(spillSlot(), from.base());

        JS_ASSERT(from.isMemory() || from.isEffectiveAddress());
        if (from.isMemory())
            masm.mov(toOperand(from), reg);
        else
            masm.lea(toOperand(from), reg);
        JS_ASSERT(to.base() != reg);
        masm.mov(reg, toOperand(to));
    }
}

void
MoveEmitterX86::emitDoubleMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isFloatReg()) {
        masm.movsd(from.floatReg(), toOperand(to));
    } else if (to.isFloatReg()) {
        masm.movsd(toOperand(from), to.floatReg());
    } else {
        // Memory to memory float move.
        JS_ASSERT(from.isMemory());
        masm.movsd(toOperand(from), ScratchFloatReg);
        masm.movsd(ScratchFloatReg, toOperand(to));
    }
}

void
MoveEmitterX86::emit(const Move &move)
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
MoveEmitterX86::assertDone()
{
    JS_ASSERT(!inCycle_);
}

void
MoveEmitterX86::finish()
{
    assertDone();

    if (pushedAtSpill_ != -1 && spilledReg_ != InvalidReg)
        masm.mov(spillSlot(), spilledReg_);

    masm.freeStack(masm.framePushed() - pushedAtStart_);
}

