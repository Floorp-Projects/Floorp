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
    int offset =  masm.framePushed() - pushedAtCycle_;
    JS_ASSERT(offset < 4096 && offset > -4096);
    return Operand(DTRAddr(StackPointer, DtrOffImm(offset)));
}

// THIS IS ALWAYS AN LDRAddr.  It should not be wrapped in an operand, methinks
Operand
MoveEmitterARM::spillSlot() const
{
    int offset =  masm.framePushed() - pushedAtSpill_;
    JS_ASSERT(offset < 4096 && offset > -4096);
    return Operand(DTRAddr(StackPointer, DtrOffImm(offset)));
}

Operand
MoveEmitterARM::doubleSpillSlot() const
{
    int offset =  masm.framePushed() - pushedAtCycle_;
    JS_ASSERT(offset < 4096 && offset > -4096);
    // NOTE: this isn't correct.  Since we're spilling a double
    // we will need to use vldr.  vldr has a restricted range
    // in general, this single operand will be wrong, since
    // there are only a few offsets that can be encoded in a single
    // instruction.
    return Operand(DTRAddr(StackPointer, DtrOffImm(offset)));
}

Operand
MoveEmitterARM::toOperand(const MoveOperand &operand) const
{
    if (operand.isMemory()) {
        if (operand.base() != StackPointer) {
            JS_ASSERT(operand.disp() < 4096 && operand.disp() > -4096);
            return Operand(DTRAddr(operand.base(),DtrOffImm(operand.disp())));
        }

        JS_ASSERT(operand.disp() >= 0);

        // Otherwise, the stack offset may need to be adjusted.
        return Operand(DTRAddr(StackPointer,
                               DtrOffImm(operand.disp() + (masm.framePushed() - pushedAtStart_))));
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
    spilledReg_ = Register::FromCode(12);
    if (pushedAtSpill_ == -1) {
        masm.ma_push(spilledReg_);
        pushedAtSpill_ = masm.framePushed();
    } else {
        masm.ma_str(spilledReg_, spillSlot().toDTRAddr());
    }
    return spilledReg_;
}

FloatRegister
MoveEmitterARM::tempFloatReg()
{
    if (spilledFloatReg_ != InvalidFloatReg) {
        return spilledFloatReg_;
    }
    // For now, just pick d7 as the eviction point. This is totally random,
    // and if it ends up being bad, we can use actual heuristics later.
    spilledFloatReg_ = FloatRegister::FromCode(7);
    if (pushedAtDoubleSpill_ == -1) {
        masm.reserveStack(sizeof(double));
        pushedAtDoubleSpill_ = masm.framePushed();
    }
    //masm.ma_vstr(spilledFloatReg_, doubleSpillSlot().EDTRAddr());
    JS_NOT_REACHED("add vfp-offsets to the union type");
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
            masm.ma_vldr(toOperand(to).toVFPAddr(), temp);
            masm.ma_vstr(temp, cycleSlot().toVFPAddr());
        } else {
            masm.ma_vstr(to.floatReg(), cycleSlot().toVFPAddr());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.ma_ldr(toOperand(to).toDTRAddr(), temp);
            masm.ma_str(temp, cycleSlot().toDTRAddr());
        } else {
            masm.ma_str(to.reg(), cycleSlot().toDTRAddr());
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
            masm.ma_vldr(cycleSlot().toVFPAddr(), temp);
            masm.ma_vstr(temp, toOperand(to).toVFPAddr());
        } else {
            masm.ma_vldr(cycleSlot().toVFPAddr(), to.floatReg());
        }
    } else {
        if (to.isMemory()) {
            Register temp = tempReg();
            masm.ma_ldr(cycleSlot().toDTRAddr(), temp);
            masm.ma_str(temp, toOperand(to).toDTRAddr());
        } else {
            masm.ma_ldr(cycleSlot().toDTRAddr(), to.reg());
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
            masm.ma_ldr(spillSlot().toDTRAddr(), spilledReg_);
            spilledReg_ = InvalidReg;
        }
        switch (toOperand(to).getTag()) {
        case Operand::OP2:
            // secretly must be a register
            masm.ma_mov(from.reg(), to.reg());
            break;
        case Operand::DTR:
            masm.ma_str(from.reg(), toOperand(to).toDTRAddr());
            break;
        default:
            JS_NOT_REACHED("strange move!");
        }
    } else if (to.isGeneralReg()) {
        if (to.reg() == spilledReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledReg_ = InvalidReg;
        }
        masm.ma_ldr(toOperand(from).toDTRAddr(), to.reg());
    } else {
        // Memory to memory gpr move.
        Register reg = tempReg();
        masm.ma_ldr(toOperand(from).toDTRAddr(), reg);
        masm.ma_str(reg, toOperand(to).toDTRAddr());
    }
}

void
MoveEmitterARM::emitDoubleMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isFloatReg()) {
        if (from.floatReg() == spilledFloatReg_) {
            // If the source is a register that has been spilled, make
            // sure to load the source back into that register.
            masm.ma_vldr(doubleSpillSlot().toVFPAddr(), spilledFloatReg_);
            spilledFloatReg_ = InvalidFloatReg;
        }
        masm.ma_vstr(from.floatReg(), toOperand(to).toVFPAddr());
    } else if (to.isFloatReg()) {
        if (to.floatReg() == spilledFloatReg_) {
            // If the destination is the spilled register, make sure we
            // don't re-clobber its value.
            spilledFloatReg_ = InvalidFloatReg;
        }
        masm.ma_vldr(toOperand(from).toVFPAddr(), to.floatReg());
    } else {
        // Memory to memory float move.
        FloatRegister reg = tempFloatReg();
        masm.ma_vldr(toOperand(from).toVFPAddr(), reg);
        masm.ma_vstr(reg, toOperand(to).toVFPAddr());
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

    if (pushedAtDoubleSpill_ != -1 && spilledFloatReg_ != InvalidFloatReg) {
        masm.ma_vldr(doubleSpillSlot().toVFPAddr(), spilledFloatReg_);
    }
    if (pushedAtSpill_ != -1 && spilledReg_ != InvalidReg) {
        masm.ma_ldr(spillSlot().toDTRAddr(), spilledReg_);
    }
    masm.freeStack(masm.framePushed() - pushedAtStart_);
}
