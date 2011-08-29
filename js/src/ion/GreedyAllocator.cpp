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

#include "GreedyAllocator.h"

using namespace js;
using namespace js::ion;

GreedyAllocator::GreedyAllocator(MIRGenerator *gen, LIRGraph &graph)
  : gen(gen),
    graph(graph)
{
}

void
GreedyAllocator::findDefinitionsInLIR(LInstruction *ins)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        LDefinition *def = ins->getDef(i);
        JS_ASSERT(def->virtualRegister() < graph.numVirtualRegisters());

        if (def->policy() == LDefinition::REDEFINED)
            continue;

        vars[def->virtualRegister()].def = def;
#ifdef DEBUG
        vars[def->virtualRegister()].ins = ins;
#endif
    }
}

void
GreedyAllocator::findDefinitionsInBlock(LBlock *block)
{
    for (size_t i = 0; i < block->numPhis(); i++)
        findDefinitionsInLIR(block->getPhi(i));
    for (LInstructionIterator i = block->begin(); i != block->end(); i++)
        findDefinitionsInLIR(*i);
}

void
GreedyAllocator::findDefinitions()
{
    for (size_t i = 0; i < graph.numBlocks(); i++)
        findDefinitionsInBlock(graph.getBlock(i));
}

bool
GreedyAllocator::maybeEvict(AnyRegister reg)
{
    if (!state.free.has(reg))
        return evict(reg);
    return true;
}

static inline AnyRegister
GetFixedRegister(LDefinition *def, LUse *use)
{
    return def->type() == LDefinition::DOUBLE
           ? AnyRegister(FloatRegister::FromCode(use->registerCode()))
           : AnyRegister(Register::FromCode(use->registerCode()));
}

static inline AnyRegister
GetAllocatedRegister(const LAllocation *a)
{
    JS_ASSERT(a->isRegister());
    return a->isFloatReg()
           ? AnyRegister(a->toFloatReg()->reg())
           : AnyRegister(a->toGeneralReg()->reg());
}

static inline AnyRegister
GetPresetRegister(const LDefinition *def)
{
    JS_ASSERT(def->policy() == LDefinition::PRESET);
    return GetAllocatedRegister(def->output());
}

bool
GreedyAllocator::prescanDefinition(LDefinition *def)
{
    // If the definition is fakeo, a redefinition, ignore it entirely. It's not
    // valid to kill it, and it doesn't matter if an input uses the same
    // register (thus it does not go into the disallow set).
    if (def->policy() == LDefinition::REDEFINED)
        return true;

    VirtualRegister *vr = getVirtualRegister(def);

    // Add its stack slot and register to the free pool.
    if (!kill(vr))
        return false;

    // If it has a register, prevent it from being allocated this round.
    if (vr->hasRegister())
        disallowed.add(vr->reg());

    if (def->policy() == LDefinition::PRESET) {
        const LAllocation *a = def->output();
        if (a->isRegister()) {
            // Evict fixed registers. Use the unchecked version of set-add
            // because the register does not reflect any allocation state, so
            // it may have already been added.
            AnyRegister reg = GetPresetRegister(def);
            disallowed.addUnchecked(reg);
            if (!maybeEvict(reg))
                return false;
        }
    }
    return true;
}

bool
GreedyAllocator::prescanDefinitions(LInstruction *ins)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        if (!prescanDefinition(ins->getDef(i)))
            return false;
    }
    for (size_t i = 0; i < ins->numTemps(); i++) {
        LDefinition *temp = ins->getTemp(i);
        if (temp->isBogusTemp())
            continue;
        if (!prescanDefinition(temp))
            return false;
    }
    return true;
}

bool
GreedyAllocator::prescanUses(LInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        LAllocation *a = ins->getOperand(i);
        if (!a->isUse()) {
            JS_ASSERT(a->isConstant());
            continue;
        }

        LUse *use = a->toUse();
        VirtualRegister *vr = getVirtualRegister(use);

        if (use->policy() == LUse::FIXED)
            disallowed.add(GetFixedRegister(vr->def, use));
        else if (vr->hasRegister())
            discouraged.addUnchecked(vr->reg());
    }
    return true;
}

bool
GreedyAllocator::allocateStack(VirtualRegister *vr)
{
    if (vr->hasBackingStack())
        return true;

    uint32 index;
    if (vr->isDouble()) {
        if (!stackSlots.allocateDoubleSlot(&index))
            return false;
    } else {
        if (!stackSlots.allocateSlot(&index))
            return false;
    }

    vr->setStackSlot(index);
    return true;
}

bool
GreedyAllocator::allocate(LDefinition::Type type, Policy policy, AnyRegister *out)
{
    RegisterSet allowed = RegisterSet::Not(disallowed);
    RegisterSet free = allocatableRegs();
    RegisterSet tryme = RegisterSet::Intersect(free, RegisterSet::Not(discouraged));

    if (tryme.empty(type == LDefinition::DOUBLE)) {
        if (free.empty(type == LDefinition::DOUBLE)) {
            *out = allowed.takeAny(type == LDefinition::DOUBLE);
            if (!evict(*out))
                return false;
        } else {
            *out = free.takeAny(type == LDefinition::DOUBLE);
        }
    } else {
        *out = tryme.takeAny(type == LDefinition::DOUBLE);
    }

    if (policy != TEMPORARY)
        disallowed.add(*out);

    return true;
}

void
GreedyAllocator::freeStack(VirtualRegister *vr)
{
    if (vr->isDouble())
        stackSlots.freeDoubleSlot(vr->stackSlot());
    else
        stackSlots.freeSlot(vr->stackSlot());
}

void
GreedyAllocator::freeReg(AnyRegister reg)
{
    state[reg] = NULL;
    state.free.add(reg);
}

bool
GreedyAllocator::kill(VirtualRegister *vr)
{
    if (vr->hasRegister()) {
        AnyRegister reg = vr->reg();
        JS_ASSERT(state[reg] == vr);

        freeReg(reg);
    }
    if (vr->hasStackSlot())
        freeStack(vr);
    return true;
}

bool
GreedyAllocator::evict(AnyRegister reg)
{
    VirtualRegister *vr = state[reg];
    JS_ASSERT(vr->reg() == reg);

    // If the virtual register does not have a stack slot, allocate one now.
    if (!allocateStack(vr))
        return false;

    // We're allocating bottom-up, so eviction *restores* a register, otherwise
    // it could not be used downstream.
    if (!restore(vr->backingStack(), reg))
        return false;

    freeReg(reg);
    vr->unsetRegister();
    return true;
}

void
GreedyAllocator::assign(VirtualRegister *vr, AnyRegister reg)
{
    JS_ASSERT(!state[reg]);
    state[reg] = vr;
    vr->setRegister(reg);
    state.free.take(reg);
}

bool
GreedyAllocator::allocateRegisterOperand(LAllocation *a, VirtualRegister *vr)
{
    AnyRegister reg;

    // Note that the disallow policy is required to prevent other allocations
    // in later uses clobbering the register.
    if (vr->hasRegister()) {
        reg = vr->reg();
        disallowed.add(reg);
    } else {
        // If it does not have a register, allocate one now.
        if (!allocate(vr->type(), DISALLOW, &reg))
            return false;
        assign(vr, reg);
    }

    *a = LAllocation(reg);
    return true;
}

bool
GreedyAllocator::allocateWritableOperand(LAllocation *a, VirtualRegister *vr)
{
    AnyRegister reg;
    if (!vr->hasRegister()) {
        // If the vr has no register assigned, then we can assign a register and
        // steal it knowing that it will be that register's last use.
        if (!allocate(vr->type(), DISALLOW, &reg))
            return false;
        assign(vr, reg);
    } else {
        if (allocatableRegs().empty(vr->isDouble())) {
            // If there are registers free, get one.
            if (!allocate(vr->type(), DISALLOW, &reg))
                return false;
            align(vr->reg(), reg);
        } else {
            // Otherwise, just steal the register.
            if (!evict(vr->reg()))
                return false;
            reg = vr->reg();
        }
    }

    *a = LAllocation(reg);
    return true;
}

bool
GreedyAllocator::allocateAnyOperand(LAllocation *a, VirtualRegister *vr, bool preferReg)
{
    if (vr->hasRegister()) {
        *a = LAllocation(vr->reg());
        return true;
    }

    // Are any registers free? Don't bother if the requestee is a type tag.
    if ((preferReg || vr->type() != LDefinition::TYPE) && !allocatableRegs().empty(vr->isDouble()))
        return allocateRegisterOperand(a, vr);

    // Otherwise, use a memory operand.
    if (!allocateStack(vr))
        return false;
    *a = vr->backingStack();
    return true;
}

bool
GreedyAllocator::allocateFixedOperand(LAllocation *a, VirtualRegister *vr)
{
    // Note that this register is already in the disallow set.
    AnyRegister needed = GetFixedRegister(vr->def, a->toUse());

    *a = LAllocation(needed);

    if (!vr->hasRegister()) {
        if (!maybeEvict(needed))
            return false;
        assign(vr, needed);
        return true;
    }

    if (vr->reg() == needed)
        return true;

    // Otherwise, we need to align the input.
    return align(vr->reg(), needed);
}

bool
GreedyAllocator::allocateSameAsInput(LDefinition *def, LAllocation *a, AnyRegister *out)
{
    LUse *use = a->toUse();
    VirtualRegister *vdef = getVirtualRegister(def);
    VirtualRegister *vuse = getVirtualRegister(use);

    JS_ASSERT(vdef->isDouble() == vuse->isDouble());

    AnyRegister reg;

    // Find a suitable output register. For simplicity, we do not consider the
    // current allocation of the input virtual register, which means it could
    // be evicted.
    if (use->isFixedRegister()) {
        reg = GetFixedRegister(def, use);
    } else if (vdef->hasRegister()) {
        reg = vdef->reg();
    } else {
        if (!allocate(vdef->type(), DISALLOW, &reg))
            return false;
    }
    JS_ASSERT(disallowed.has(reg));

    if (vuse->hasRegister()) {
        JS_ASSERT(vuse->reg() != reg);
        if (!align(vuse->reg(), reg))
            return false;
    } else {
        // If the input has no register, we can just re-use the output register
        // directly, because nothing downstream could be clobbered by consuming
        // the register.
        assign(vuse, reg);
    }

    // Overwrite the input allocation now.
    *a = LAllocation(reg);

    *out = reg;
    return true;
}

bool
GreedyAllocator::allocateDefinitions(LInstruction *ins)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        LDefinition *def = ins->getDef(i);
        VirtualRegister *vr = getVirtualRegister(def);

        LAllocation output;
        switch (def->policy()) {
          case LDefinition::REDEFINED:
            // This is purely passthru, so ignore it.
            continue;

          case LDefinition::DEFAULT:
          {
            // Either take the register requested, or allocate a new one.
            if (vr->hasRegister()) {
                output = LAllocation(vr->reg());
            } else {
                AnyRegister reg;
                if (!allocate(vr->type(), DISALLOW, &reg))
                    return false;
                output = LAllocation(reg);
            }
            break;
          }

          case LDefinition::PRESET:
          {
            // Eviction and disallowing occurred during the definition
            // pre-scan pass.
            output = *def->output();
            break;
          }

          case LDefinition::MUST_REUSE_INPUT:
          {
            AnyRegister out_reg;
            if (!allocateSameAsInput(def, ins->getOperand(0), &out_reg))
                return false;
            output = LAllocation(out_reg);
            break;
          }
        }

        if (output.isRegister()) {
            JS_ASSERT_IF(output.isFloatReg(), disallowed.has(output.toFloatReg()->reg()));
            JS_ASSERT_IF(output.isGeneralReg(), disallowed.has(output.toGeneralReg()->reg()));
        }

        // |output| is now the allocation state leaving the instruction.
        // However, this is not necessarily the allocation state expected
        // downstream, so emit moves where necessary.
        if (output.isRegister()) {
            if (vr->hasRegister()) {
                // If the returned register is different from the output
                // register, a move is required.
                AnyRegister out = GetAllocatedRegister(&output);
                if (out != vr->reg()) {
                    if (!spill(output, vr->reg()))
                        return false;
                }
            }

            // Spill to the stack if needed.
            if (vr->hasStackSlot() && !spill(output, vr->backingStack()))
                return false;
        } else if (vr->hasRegister()) {
            // This definition has a canonical spill location, so make sure to
            // load it to the resulting register, if any.
            JS_ASSERT(!vr->hasStackSlot());
            JS_ASSERT(vr->hasBackingStack());
            if (!spill(output, vr->reg()))
                return false;
        }

        // Finally, set the output.
        *def = LDefinition(def->type(), output);
    }

    return true;
}

bool
GreedyAllocator::allocateTemporaries(LInstruction *ins)
{
    for (size_t i = 0; i < ins->numTemps(); i++) {
        LDefinition *def = ins->getTemp(i);
        if (def->policy() == LDefinition::PRESET)
            continue;

        JS_ASSERT(def->policy() == LDefinition::DEFAULT);
        AnyRegister reg;
        if (!allocate(def->type(), DISALLOW, &reg))
            return false;
        *def = LDefinition(def->type(), LAllocation(reg));
    }
    return true;
}

bool
GreedyAllocator::allocateInputs(LInstruction *ins)
{
    // First deal with fixed-register policies and policies that require
    // registers.
    for (size_t i = 0; i < ins->numOperands(); i++) {
        LAllocation *a = ins->getOperand(i);
        if (!a->isUse())
            continue;
        LUse *use = a->toUse();
        VirtualRegister *vr = getVirtualRegister(use);
        if (use->policy() == LUse::FIXED) {
            if (!allocateFixedOperand(a, vr))
                return false;
        } else if (use->policy() == LUse::REGISTER) {
            if (!allocateRegisterOperand(a, vr))
                return false;
        } else if (use->policy() == LUse::COPY) {
            if (!allocateWritableOperand(a, vr))
                return false;
        }
    }

    // Allocate temporaries before uses that accept memory operands, because
    // temporaries require registers.
    if (!allocateTemporaries(ins))
        return false;

    // Finally, deal with things that take either registers or memory.
    for (size_t i = 0; i < ins->numOperands(); i++) {
        LAllocation *a = ins->getOperand(i);
        if (!a->isUse())
            continue;

        LUse *use = a->toUse();
        JS_ASSERT(use->policy() == LUse::ANY);

        VirtualRegister *vr = getVirtualRegister(use);
        if (!allocateAnyOperand(a, vr))
            return false;
    }

    return true;
}

bool
GreedyAllocator::informSnapshot(LSnapshot *snapshot)
{
    for (size_t i = 0; i < snapshot->numEntries(); i++) {
        LAllocation *a = snapshot->getEntry(i);
        if (!a->isUse())
            continue;

        LUse *use = a->toUse();
        VirtualRegister *vr = getVirtualRegister(use);
        if (vr->hasRegister()) {
            *a = LAllocation(vr->reg());
        } else {
            if (!allocateStack(vr))
                return false;
            *a = vr->backingStack();
        }
    }
    return true;
}

void
GreedyAllocator::assertValidRegisterState()
{
#ifdef DEBUG
    // Assert that for each taken register in state.free, that it maps to a vr
    // and that that vr has that register.
    for (AnyRegisterIterator iter; iter.more(); iter++) {
        AnyRegister reg = *iter;
        VirtualRegister *vr = state[reg];
        if (!reg.allocatable()) {
            JS_ASSERT(!vr);
            continue;
        }
        JS_ASSERT(!vr == state.free.has(reg));
        JS_ASSERT_IF(vr, vr->reg() == reg);
    }
#endif
}

bool
GreedyAllocator::allocateInstruction(LBlock *block, LInstruction *ins)
{
    if (!gen->ensureBallast())
        return false;

    // Reset internal state used for evicting.
    reset();
    assertValidRegisterState();

    // Step 1. Find all fixed writable registers, adding them to the
    // disallow set.
    if (!prescanDefinitions(ins))
        return false;

    // Step 2. For each use, add fixed policies to the disallow set and
    // already allocated registers to the discouraged set.
    if (!prescanUses(ins))
        return false;

    // Step 3. Allocate registers for each definition.
    if (!allocateDefinitions(ins))
        return false;

    // Step 4. Allocate inputs and temporaries.
    if (!allocateInputs(ins))
        return false;

    // Step 5. Assign fields of a snapshot.
    if (ins->snapshot() && !informSnapshot(ins->snapshot()))
        return false;

    if (aligns)
        block->insertBefore(ins, aligns);

    return true;
}

bool
GreedyAllocator::allocateRegistersInBlock(LBlock *block)
{
    LInstructionReverseIterator ri = block->instructions().rbegin();

    // Control instructions need to be handled specially. Since they have no
    // outputs, we are guaranteed they do not spill. But restores may occur,
    // and may need to be duplicated on each outgoing edge.
    if (!allocateInstruction(block, *ri))
        return false;
    ri++;

    JS_ASSERT(!spills);

    if (restores) {
        // For each successor that has already been allocated, duplicate the
        // move group into the start of its block. We don't yet have the
        // ability to detect whether the receiving blocks actually need this
        // move.
        for (size_t i = 0; i < block->mir()->numSuccessors(); i++) {
            MBasicBlock *msuccessor = block->mir()->getSuccessor(i);
            if (msuccessor->id() <= block->mir()->id())
                continue;

            Mover moves;
            LBlock *successor = msuccessor->lir();
            for (size_t i = 0; i < restores->numMoves(); i++) {
                const LMove &move = restores->getMove(i);
                if (!moves.move(*move.from(), *move.to()))
                    return false;
            }

            successor->insertBefore(*successor->begin(), moves.moves);
        }
    }

    if (block->mir()->isLoopBackedge()) {
        // If this is a loop backedge, save its exit allocation state at the
        // loop header. Note this occurs after allocating the initial jump
        // instruction, to avoid placing useless moves at the loop edge.
        if (!prepareBackedge(block))
            return false;
    }
    blockInfo(block)->out = state;

    for (; ri != block->instructions().rend(); ri++) {
        LInstruction *ins = *ri;

        if (!allocateInstruction(block, ins))
            return false;

        // Step 6. Insert move instructions.
        if (restores)
            block->insertAfter(ins, restores);
        if (spills) {
            JS_ASSERT(ri != block->rbegin());
            block->insertAfter(ins, spills);
        }

        assertValidRegisterState();
    }
    return true;
}

bool
GreedyAllocator::mergeRegisterState(const AnyRegister &reg, LBlock *left, LBlock *right)
{
    VirtualRegister *vleft = state[reg];
    VirtualRegister *vright = blockInfo(right)->in[reg];

    // If the input register is unused or occupied by the same vr, we're done.
    if (vleft == vright)
        return true;

    // If the right-hand side has no allocation, then do nothing because the
    // left-hand side has already propagated its value up.
    if (!vright)
        return true;

    BlockInfo *rinfo = blockInfo(right);

    if (!vleft && !vright->hasRegister()) {
        // The left-hand side never assigned a register to |vright|, and has
        // not assigned this register, so just inherit the right-hand side's
        // allocation.
        assign(vright, reg);
        return true;
    }

    // Otherwise, we have reached one of two situations:
    //  (1) The left-hand and right-hand sides have two different definitions
    //      in the same register.
    //  (2) The right-hand side expects a definition in a different register
    //      than the left-hand side has assigned.
    //
    // In both cases, we emit a load or move on the right-hand side to ensure
    // that the definition is in the expected register.
    if (!vright->hasRegister() && !allocatableRegs().empty(vright->isDouble())) {
        AnyRegister reg;
        if (!allocate(vright->type(), DISALLOW, &reg))
            return false;
        assign(vright, reg);
    }

    if (vright->hasRegister()) {
        JS_ASSERT(vright->reg() != reg);
        if (!rinfo->restores.move(vright->reg(), reg))
            return false;
    } else {
        if (!allocateStack(vright))
            return false;
        if (!rinfo->restores.move(vright->backingStack(), reg))
            return false;
    }

    return true;
}

bool
GreedyAllocator::prepareBackedge(LBlock *block)
{
    MBasicBlock *msuccessor = block->mir()->successorWithPhis();
    if (!msuccessor)
        return true;

    LBlock *successor = msuccessor->lir();

    uint32 pos = block->mir()->positionInPhiSuccessor();
    for (size_t i = 0; i < successor->numPhis(); i++) {
        LPhi *phi = successor->getPhi(i);
        LAllocation *a = phi->getOperand(pos);
        if (!a->isUse())
            continue;
        VirtualRegister *vr = getVirtualRegister(a->toUse());

        // We ensure a phi always has an allocation, because it's too early to
        // tell whether something in the loop uses it.
        LAllocation result;
        if (!allocateAnyOperand(&result, vr, true))
            return false;

        // Store the def's exit allocation in the phi's output, as a cheap
        // trick. At the loop header we'll see this and emit moves from the def
        // to the phi's final storage.
        phi->getDef(0)->setOutput(result);
    }

    return true;
}

bool
GreedyAllocator::mergeBackedgeState(LBlock *header, LBlock *backedge)
{
    BlockInfo *info = blockInfo(backedge);

    // Handle loop-carried carried registers, making sure anything live at the
    // backedge is also properly held live at the top of the loop.
    Mover carried;
    for (AnyRegisterIterator iter; iter.more(); iter++) {
        AnyRegister reg = *iter;
        VirtualRegister *inVr = state[reg];
        if (!inVr)
            continue;

        VirtualRegister *outVr = info->out[reg];
        if (inVr == outVr)
            continue;

        // A register is live coming into the loop, but has a different exit
        // assignment. For this to work, we either need to insert a spill or a
        // move. This may insert unnecessary moves, since it cannot tell if a
        // register was clobbered in the loop. It only knows if the allocation
        // states at the loop edges are different. Note that for the same
        // reasons, we cannot assume a register allocated here will be
        // preserved across the loop.
        if (!allocateStack(inVr))
            return false;
        if (!carried.move(inVr->backingStack(), reg))
            return false;
    }
    if (carried.moves) {
        LInstruction *ins = *header->instructions().begin();
        header->insertBefore(ins, carried.moves);
    }

    Mover phis;

    // Handle loop phis.
    for (size_t i = 0; i < header->numPhis(); i++) {
        LPhi *phi = header->getPhi(i);
        LDefinition *def = phi->getDef(0);
        VirtualRegister *vr = getVirtualRegister(def);

        JS_ASSERT(def->policy() == LDefinition::PRESET);
        const LAllocation *a = def->output();

        if (vr->hasStackSlot() && !phis.move(*a, vr->backingStack()))
            return false;

        if (vr->hasRegister() && (!a->isRegister() || vr->reg() != a->toRegister())) {
            if (!phis.move(*a, vr->reg()))
                return false;
        }
    }

    if (phis.moves) {
        LInstruction *ins = *backedge->instructions().rbegin();
        backedge->insertBefore(ins, phis.moves);
    }

    return true;
}

bool
GreedyAllocator::mergePhiState(LBlock *block)
{
    MBasicBlock *mblock = block->mir();
    if (!mblock->successorWithPhis())
        return true;

    // Reset state so evictions will work.
    reset();

    Mover phis;

    uint32 pos = mblock->positionInPhiSuccessor();
    LBlock *successor = mblock->successorWithPhis()->lir();
    for (size_t i = 0; i < successor->numPhis(); i++) {
        LPhi *phi = successor->getPhi(i);
        VirtualRegister *def = getVirtualRegister(phi->getDef(0));

        // Ignore non-loop phis with no uses.
        if (!def->hasRegister() && !def->hasStackSlot())
            continue;

        LAllocation *a = phi->getOperand(pos);
        VirtualRegister *use = getVirtualRegister(a->toUse());

        // Try to give the use a register.
        if (!use->hasRegister()) {
            if (def->hasRegister() && !state[def->reg()]) {
                assign(use, def->reg());
            } else {
                LAllocation unused;
                if (!allocateAnyOperand(&unused, use, true))
                    return false;
            }
        }

        // Emit a move from the use to a def register.
        if (def->hasRegister()) {
            if (use->hasRegister()) {
                if (use->reg() != def->reg() && !phis.move(use->reg(), def->reg()))
                    return false;
            } else {
                if (!phis.move(use->backingStack(), def->reg()))
                    return false;
            }
        }

        // Emit a move from the use to a def stack slot.
        if (def->hasStackSlot()) {
            if (use->hasRegister()) {
                if (!phis.move(use->reg(), def->backingStack()))
                    return false;
            } else if (use->backingStack() != def->backingStack()) {
                if (!phis.move(use->backingStack(), def->backingStack()))
                    return false;
            }
        }
    }

    // Now insert restores (if any) and phi moves.
    JS_ASSERT(!aligns);
    JS_ASSERT(!spills);
    LInstruction *before = *block->instructions().rbegin();
    if (restores)
        block->insertBefore(before, restores);
    if (phis.moves)
        block->insertBefore(before, phis.moves);

    return true;
}

bool
GreedyAllocator::mergeAllocationState(LBlock *block)
{
    MBasicBlock *mblock = block->mir();

    if (!mblock->numSuccessors()) {
        state = AllocationState();
        return true;
    }

    // Prefer the successor with phis as the baseline state
    LBlock *leftblock = mblock->getSuccessor(0)->lir();
    state = blockInfo(leftblock)->in;

    // To complete inheriting our successor's state, make sure each taken
    // register is applied to the def for which it was intended.
    for (AnyRegisterIterator iter; iter.more(); iter++) {
        AnyRegister reg = *iter;
        if (VirtualRegister *vr = state[reg])
            vr->setRegister(reg);
    }

    // Merge state from each additional successor.
    for (size_t i = 1; i < mblock->numSuccessors(); i++) {
        LBlock *rightblock = mblock->getSuccessor(i)->lir();

        for (AnyRegisterIterator iter; iter.more(); iter++) {
            AnyRegister reg = *iter;
            if (!mergeRegisterState(reg, leftblock, rightblock))
                return false;
        }

        // If there were parallel moves, append them now.
        BlockInfo *info = blockInfo(rightblock);
        if (info->restores.moves)
            rightblock->insertBefore(*rightblock->begin(), info->restores.moves);
    }

    if (!mergePhiState(block))
        return false;

    return true;
}

bool
GreedyAllocator::allocateRegisters()
{
    // Allocate registers bottom-up, such that we see all uses before their
    // definitions.
    for (size_t i = graph.numBlocks() - 1; i < graph.numBlocks(); i--) {
        LBlock *block = graph.getBlock(i);

        // Merge allocation state from our successors.
        if (!mergeAllocationState(block))
            return false;

        // Allocate registers.
        if (!allocateRegistersInBlock(block))
            return false;

        // If this is a loop header, insert moves at the backedge from phi
        // inputs to phi outputs.
        if (block->mir()->isLoopHeader()) {
            if (!mergeBackedgeState(block, block->mir()->backedge()->lir()))
                return false;
        }

        // Kill phis.
        for (size_t i = 0; i < block->numPhis(); i++) {
            LPhi *phi = block->getPhi(i);
            JS_ASSERT(phi->numDefs() == 1);

            VirtualRegister *vr = getVirtualRegister(phi->getDef(0));
            kill(vr);
        }

        // We've reached the top of the block. Save the mapping of registers to
        // definitions, as our predecessors will need this to merge state.
        // Then, clear the register assignments to all defs. This is necessary
        // otherwise block A's state could be left around for completely
        // independent block B, which never actually allocated for that def.
        blockInfo(block)->in = state;
        for (AnyRegisterIterator iter; iter.more(); iter++) {
            AnyRegister reg = *iter;
            VirtualRegister *vr = state[reg];
            if (vr) {
                JS_ASSERT(vr->reg() == reg);
                vr->unsetRegister();
            }
        }
    }
    return true;
}

bool
GreedyAllocator::allocate()
{
    vars = gen->allocate<VirtualRegister>(graph.numVirtualRegisters());
    if (!vars)
        return false;
    memset(vars, 0, sizeof(VirtualRegister) * graph.numVirtualRegisters());

    blocks = gen->allocate<BlockInfo>(graph.numBlockIds());
    for (size_t i = 0; i < graph.numBlockIds(); i++)
        new (&blocks[i]) BlockInfo();

    findDefinitions();
    if (!allocateRegisters())
        return false;
    graph.setLocalSlotCount(stackSlots.stackHeight());

    return true;
}

