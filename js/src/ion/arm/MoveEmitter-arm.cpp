/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoveEmitter-arm.h"

using namespace js;
using namespace js::ion;

MoveEmitterARM::MoveEmitterARM(MacroAssemblerARMCompat &masm)
  : inCycle_(false),
    masm(masm),
    pushedAtCycle_(-1),
    pushedAtSpill_(-1),
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
    int offset =  masm.framePushed() - pushedAtCycle_;
    JS_ASSERT(offset < 4096 && offset > -4096);
    return Operand(StackPointer, offset);
}

// THIS IS ALWAYS AN LDRAddr.  It should not be wrapped in an operand, methinks
Operand
MoveEmitterARM::spillSlot() const
{
    int offset =  masm.framePushed() - pushedAtSpill_;
    JS_ASSERT(offset < 4096 && offset > -4096);
    return Operand(StackPointer, offset);
}

Operand
MoveEmitterARM::toOperand(const MoveOperand &operand, bool isFloat) const
{
    if (operand.isMemory() || operand.isEffectiveAddress()) {
        if (operand.base() != StackPointer) {
            JS_ASSERT(operand.disp() < 1024 && operand.disp() > -1024);
            return Operand(operand.base(), operand.disp());
        }

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

    // For now, just pick r12/ip as the eviction point. This is totally
    // random, and if it ends up being bad, we can use actual heuristics later.
    // r12 is actually a bad choice.  it is the scratch register, which is frequently
    // used for address computations, such as those found when we attempt to access
    // values more than 4096 off of the stack pointer.
    // instead, use lr, the LinkRegister.
    spilledReg_ = r14;
    if (pushedAtSpill_ == -1) {
        masm.Push(spilledReg_);
        pushedAtSpill_ = masm.framePushed();
    } else {
        masm.ma_str(spilledReg_, spillSlot());
    }
    return spilledReg_;
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
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            FloatRegister temp = ScratchFloatReg;
            masm.ma_vldr(toOperand(to, true), temp);
            masm.ma_vstr(temp, cycleSlot());
        } else {
            masm.ma_vstr(to.floatReg(), cycleSlot());
        }
    } else {
        // an non-vfp value
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.ma_ldr(toOperand(to, false), temp);
            masm.ma_str(temp, cycleSlot());
        } else {
            if (to.reg() == spilledReg_) {
                // If the destination was spilled, restore it first.
                masm.ma_ldr(spillSlot(), spilledReg_);
                spilledReg_ = InvalidReg;
            }
            masm.ma_str(to.reg(), cycleSlot());
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
            FloatRegister temp = ScratchFloatReg;
            masm.ma_vldr(cycleSlot(), temp);
            masm.ma_vstr(temp, toOperand(to, true));
        } else {
            masm.ma_vldr(cycleSlot(), to.floatReg());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.ma_ldr(cycleSlot(), temp);
            masm.ma_str(temp, toOperand(to, false));
        } else {
            if (to.reg() == spilledReg_) {
                // Make sure we don't re-clobber the spilled register later.
                spilledReg_ = InvalidReg;
            }
            masm.ma_ldr(cycleSlot(), to.reg());
        }
    }
}

void
MoveEmitterARM::emitMove(const MoveOperand &from, const MoveOperand &to)
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
            masm.ma_ldr(spillSlot(), spilledReg_);
            spilledReg_ = InvalidReg;
        }
        switch (toOperand(to, false).getTag()) {
          case Operand::OP2:
            // secretly must be a register
            masm.ma_mov(from.reg(), to.reg());
            break;
          case Operand::MEM:
            masm.ma_str(from.reg(), toOperand(to, false));
            break;
          default:
            JS_NOT_REACHED("strange move!");
        }
    } else if (to.isGeneralReg()) {
        JS_ASSERT(from.isMemory() || from.isEffectiveAddress());
        if (from.isMemory())
            masm.ma_ldr(toOperand(from, false), to.reg());
        else
            masm.ma_add(from.base(), Imm32(from.disp()), to.reg());
    } else {
        // Memory to memory gpr move.
        Register reg = tempReg();

        JS_ASSERT(from.isMemory() || from.isEffectiveAddress());
        if (from.isMemory())
            masm.ma_ldr(toOperand(from, false), reg);
        else
            masm.ma_add(from.base(), Imm32(from.disp()), reg);
        JS_ASSERT(to.base() != reg);
        masm.ma_str(reg, toOperand(to, false));
    }
}

void
MoveEmitterARM::emitDoubleMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isFloatReg()) {
        if (to.isFloatReg())
            masm.ma_vmov(from.floatReg(), to.floatReg());
        else
            masm.ma_vstr(from.floatReg(), toOperand(to, true));
    } else if (to.isFloatReg()) {
        masm.ma_vldr(toOperand(from, true), to.floatReg());
    } else {
        // Memory to memory float move.
        JS_ASSERT(from.isMemory());
        FloatRegister reg = ScratchFloatReg;
        masm.ma_vldr(toOperand(from, true), reg);
        masm.ma_vstr(reg, toOperand(to, true));
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

    if (pushedAtSpill_ != -1 && spilledReg_ != InvalidReg)
        masm.ma_ldr(spillSlot(), spilledReg_);
    masm.freeStack(masm.framePushed() - pushedAtStart_);
}
