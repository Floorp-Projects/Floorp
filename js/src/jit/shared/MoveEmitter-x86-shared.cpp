/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/MoveEmitter-x86-shared.h"

using namespace js;
using namespace js::jit;

MoveEmitterX86::MoveEmitterX86(MacroAssemblerSpecific &masm)
  : inCycle_(false),
    masm(masm),
    pushedAtCycle_(-1)
{
    pushedAtStart_ = masm.framePushed();
}

// Examine the cycle in moves starting at position i. Determine if it's a
// simple cycle consisting of all register-to-register moves in a single class,
// and whether it can be implemented entirely by swaps.
size_t
MoveEmitterX86::characterizeCycle(const MoveResolver &moves, size_t i,
                                  bool *allGeneralRegs, bool *allFloatRegs)
{
    size_t swapCount = 0;

    for (size_t j = i; ; j++) {
        const Move &move = moves.getMove(j);

        // If it isn't a cycle of registers of the same kind, we won't be able
        // to optimize it.
        if (!move.to().isGeneralReg())
            *allGeneralRegs = false;
        if (!move.to().isFloatReg())
            *allFloatRegs = false;
        if (!*allGeneralRegs && !*allFloatRegs)
            return -1;

        // The first and last move of the cycle are marked with inCycle(). Stop
        // iterating when we see the last one.
        if (j != i && move.inCycle())
            break;

        // Check that this move is actually part of the cycle. This is
        // over-conservative when there are multiple reads from the same source,
        // but that's expected to be rare.
        if (move.from() != moves.getMove(j + 1).to()) {
            *allGeneralRegs = false;
            *allFloatRegs = false;
            return -1;
        }

        swapCount++;
    }

    // Check that the last move cycles back to the first move.
    const Move &move = moves.getMove(i + swapCount);
    if (move.from() != moves.getMove(i).to()) {
        *allGeneralRegs = false;
        *allFloatRegs = false;
        return -1;
    }

    return swapCount;
}

// If we can emit optimized code for the cycle in moves starting at position i,
// do so, and return true.
bool
MoveEmitterX86::maybeEmitOptimizedCycle(const MoveResolver &moves, size_t i,
                                        bool allGeneralRegs, bool allFloatRegs, size_t swapCount)
{
    if (allGeneralRegs && swapCount <= 2) {
        // Use x86's swap-integer-registers instruction if we only have a few
        // swaps. (x86 also has a swap between registers and memory but it's
        // slow.)
        for (size_t k = 0; k < swapCount; k++)
            masm.xchg(moves.getMove(i + k).to().reg(), moves.getMove(i + k + 1).to().reg());
        return true;
    }

    if (allFloatRegs && swapCount == 1) {
        // There's no xchg for xmm registers, but if we only need a single swap,
        // it's cheap to do an XOR swap.
        FloatRegister a = moves.getMove(i).to().floatReg();
        FloatRegister b = moves.getMove(i + 1).to().floatReg();
        masm.xorpd(a, b);
        masm.xorpd(b, a);
        masm.xorpd(a, b);
        return true;
    }

    return false;
}

void
MoveEmitterX86::emit(const MoveResolver &moves)
{
    for (size_t i = 0; i < moves.numMoves(); i++) {
        const Move &move = moves.getMove(i);
        const MoveOperand &from = move.from();
        const MoveOperand &to = move.to();

        if (move.inCycle()) {
            // If this is the end of a cycle for which we're using the stack,
            // handle the end.
            if (inCycle_) {
                completeCycle(to, move.kind());
                inCycle_ = false;
                continue;
            }

            // Characterize the cycle.
            bool allGeneralRegs = true, allFloatRegs = true;
            size_t swapCount = characterizeCycle(moves, i, &allGeneralRegs, &allFloatRegs);

            // Attempt to optimize it to avoid using the stack.
            if (maybeEmitOptimizedCycle(moves, i, allGeneralRegs, allFloatRegs, swapCount)) {
                i += swapCount;
                continue;
            }

            // Otherwise use the stack.
            breakCycle(to, move.kind());
            inCycle_ = true;
        }

        // A normal move which is not part of a cycle.
        if (move.kind() == Move::DOUBLE)
            emitDoubleMove(from, to);
        else
            emitGeneralMove(from, to);
    }
}

MoveEmitterX86::~MoveEmitterX86()
{
    assertDone();
}

Address
MoveEmitterX86::cycleSlot()
{
    if (pushedAtCycle_ == -1) {
        // Reserve stack for cycle resolution
        masm.reserveStack(sizeof(double));
        pushedAtCycle_ = masm.framePushed();
    }

    return Address(StackPointer, masm.framePushed() - pushedAtCycle_);
}

Address
MoveEmitterX86::toAddress(const MoveOperand &operand) const
{
    if (operand.base() != StackPointer)
        return Address(operand.base(), operand.disp());

    JS_ASSERT(operand.disp() >= 0);

    // Otherwise, the stack offset may need to be adjusted.
    return Address(StackPointer, operand.disp() + (masm.framePushed() - pushedAtStart_));
}

// Warning, do not use the resulting operand with pop instructions, since they
// compute the effective destination address after altering the stack pointer.
// Use toPopOperand if an Operand is needed for a pop.
Operand
MoveEmitterX86::toOperand(const MoveOperand &operand) const
{
    if (operand.isMemory() || operand.isEffectiveAddress() || operand.isFloatAddress())
        return Operand(toAddress(operand));
    if (operand.isGeneralReg())
        return Operand(operand.reg());

    JS_ASSERT(operand.isFloatReg());
    return Operand(operand.floatReg());
}

// This is the same as toOperand except that it computes an Operand suitable for
// use in a pop.
Operand
MoveEmitterX86::toPopOperand(const MoveOperand &operand) const
{
    if (operand.isMemory()) {
        if (operand.base() != StackPointer)
            return Operand(operand.base(), operand.disp());

        JS_ASSERT(operand.disp() >= 0);

        // Otherwise, the stack offset may need to be adjusted.
        // Note the adjustment by the stack slot here, to offset for the fact that pop
        // computes its effective address after incrementing the stack pointer.
        return Operand(StackPointer,
                       operand.disp() + (masm.framePushed() - sizeof(void *) - pushedAtStart_));
    }
    if (operand.isGeneralReg())
        return Operand(operand.reg());

    JS_ASSERT(operand.isFloatReg());
    return Operand(operand.floatReg());
}

void
MoveEmitterX86::breakCycle(const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (A -> B), which we reach first. We save B, then allow
    // the original move to continue.
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            masm.loadDouble(toAddress(to), ScratchFloatReg);
            masm.storeDouble(ScratchFloatReg, cycleSlot());
        } else {
            masm.storeDouble(to.floatReg(), cycleSlot());
        }
    } else {
        masm.Push(toOperand(to));
    }
}

void
MoveEmitterX86::completeCycle(const MoveOperand &to, Move::Kind kind)
{
    // There is some pattern:
    //   (A -> B)
    //   (B -> A)
    //
    // This case handles (B -> A), which we reach last. We emit a move from the
    // saved value of B, to A.
    if (kind == Move::DOUBLE) {
        if (to.isMemory()) {
            masm.loadDouble(cycleSlot(), ScratchFloatReg);
            masm.storeDouble(ScratchFloatReg, toAddress(to));
        } else {
            masm.loadDouble(cycleSlot(), to.floatReg());
        }
    } else {
        if (to.isMemory()) {
            masm.Pop(toPopOperand(to));
        } else {
            masm.Pop(to.reg());
        }
    }
}

void
MoveEmitterX86::emitGeneralMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isGeneralReg()) {
        masm.mov(from.reg(), toOperand(to));
    } else if (to.isGeneralReg()) {
        JS_ASSERT(from.isMemory() || from.isEffectiveAddress());
        if (from.isMemory())
            masm.loadPtr(toAddress(from), to.reg());
        else
            masm.lea(toOperand(from), to.reg());
    } else if (from.isMemory()) {
        // Memory to memory gpr move.
#ifdef JS_CPU_X64
        // x64 has a ScratchReg. Use it.
        masm.loadPtr(toAddress(from), ScratchReg);
        masm.mov(ScratchReg, toOperand(to));
#else
        // No ScratchReg; bounce it off the stack.
        masm.Push(toOperand(from));
        masm.Pop(toPopOperand(to));
#endif
    } else {
        // Effective address to memory move.
        JS_ASSERT(from.isEffectiveAddress());
#ifdef JS_CPU_X64
        // x64 has a ScratchReg. Use it.
        masm.lea(toOperand(from), ScratchReg);
        masm.mov(ScratchReg, toOperand(to));
#else
        // This is tricky without a ScratchReg. We can't do an lea. Bounce the
        // base register off the stack, then add the offset in place.
        masm.Push(from.base());
        masm.Pop(toPopOperand(to));
        masm.addPtr(Imm32(from.disp()), toOperand(to));
#endif
    }
}

void
MoveEmitterX86::emitDoubleMove(const MoveOperand &from, const MoveOperand &to)
{
    if (from.isFloatReg()) {
        if (to.isFloatReg())
            masm.moveDouble(from.floatReg(), to.floatReg());
        else
            masm.storeDouble(from.floatReg(), toAddress(to));
    } else if (to.isFloatReg()) {
        masm.loadDouble(toAddress(from), to.floatReg());
    } else {
        // Memory to memory float move.
        JS_ASSERT(from.isMemory());
        masm.loadDouble(toAddress(from), ScratchFloatReg);
        masm.storeDouble(ScratchFloatReg, toAddress(to));
    }
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

    masm.freeStack(masm.framePushed() - pushedAtStart_);
}

