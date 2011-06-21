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
        if (!prescanDefinition(ins->getTemp(i)))
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
            discouraged.add(vr->reg());
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
GreedyAllocator::allocateAnyOperand(LAllocation *a, VirtualRegister *vr)
{
    if (vr->hasRegister()) {
        *a = LAllocation(vr->reg());
        return true;
    }

    // Are any registers free? Don't bother if the requestee is a type tag.
    if (vr->type() != LDefinition::TYPE && !allocatableRegs().empty(vr->isDouble()))
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
        // Load the actual register into the desired input operand.
        LAllocation from;
        if (vuse->hasRegister())
            from = LAllocation(vuse->reg());
        else
            from = vuse->backingStack();
        if (!align(from, reg))
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
        LDefinition *def = ins->getTemp(0);
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

bool
GreedyAllocator::allocateRegistersInBlock(LBlock *block)
{
    for (LInstructionReverseIterator ri = block->instructions().rbegin();
         ri != block->instructions().rend();
         ri++)
    {
        if (!gen->ensureBallast())
            return false;

        LInstruction *ins = *ri;

        // Reset internal state used for evicting.
        reset();

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

        // Step 6. Insert move instructions.
        if (restores)
            block->insertAfter(ins, restores);
        if (spills)
            block->insertAfter(ins, spills);
        if (aligns) {
            block->insertBefore(ins, aligns);
            ri++;
        }
    }
    return true;
}

bool
GreedyAllocator::mergeRegisterState(const AnyRegister &reg, LBlock *left, LBlock *right)
{
    VirtualRegister *vleft = state[reg];
    VirtualRegister *vright = blockInfo(right)->in[reg];

    // Make sure virtual registers have sensible register state.
    if (vleft)
        vleft->setRegister(vleft->reg());

    // If the input register is unused or occupied by the same vr, we're done.
    if (vleft == vright)
        return true;

    // If the right-hand side has no allocation, then do nothing because the
    // left-hand side has already propagated its value up.
    if (!vright)
        return true;

    // If the left-hand side has no allocation, merge the right-hand side in.
    if (!vleft) {
        assign(vright, reg);
        return true;
    }

    BlockInfo *info = blockInfo(right);

    // Otherwise, the same register is occupied by two different allocations:
    // the left side expects R1=A, and the right side expects R1=B.
    if (allocatableRegs().empty(vright->isDouble())) {
        // There are no free registers, so put a move on the right-hand block
        // that loads the correct register out of vright's stack.
        if (!allocateStack(vright))
            return false;
        if (!info->restores.move(vright->backingStack(), reg))
            return false;

        vright->unsetRegister();
    } else {
        // There is a free register, so grab it and assign it, and emit a move
        // on the right-hand block.
        AnyRegister newreg;
        if (!allocate(vright->type(), TEMPORARY, &newreg))
            return false;
        if (!info->restores.move(newreg, reg))
            return false;

        assign(vright, newreg);
    }

    return true;
}

bool
GreedyAllocator::mergePhiState(LBlock *block)
{
    MBasicBlock *mblock = block->mir();
    if (!mblock->successorWithPhis())
        return true;

    bool isLoopExit = mblock->successorWithPhis()->isLoopHeader() &&
                      mblock->id() >= mblock->successorWithPhis()->id();

    BlockInfo *info = blockInfo(block);

    // Reset state so evictions will work.
    reset();

    uint32 pos = mblock->positionInPhiSuccessor();
    LBlock *successor = mblock->successorWithPhis()->lir();
    for (size_t i = 0; i < successor->numPhis(); i++) {
        LPhi *phi = successor->getPhi(i);
        VirtualRegister *def = getVirtualRegister(phi->getDef(0));

        if (!def->hasRegister() && !def->hasStackSlot()) {
            // The phi has no uses, but this might be because it's a loop exit.
            // If it's a loop exit, we haven't seen uses yet and must be
            // pessimistic. Note that this generally results in suboptimal
            // code, for example, at the loop edge:
            //
            //    move (def -> [stack])
            //    move ([stack] -> phi)
            //
            // It should not be difficult to either boil this away or come up
            // with a trick to pin phis to registers.
            if (!isLoopExit)
                continue;

            if (state.free.empty(def->isDouble())) {
                if (!allocateStack(def))
                    return false;
            } else {
                AnyRegister reg;
                if (!allocate(def->type(), TEMPORARY, &reg))
                    return false;
                assign(def, reg);
            }
        }

        LAllocation *a = phi->getOperand(pos);

        // Handle constant inputs.
        if (a->isConstant()) {
            LAllocation dest;
            if (def->hasRegister())
                dest = LAllocation(def->reg());
            else
                dest = def->backingStack();
            if (!info->phis.move(*a, dest))
                return false;
            continue;
        }

        VirtualRegister *use = getVirtualRegister(a->toUse());

        // The definition contains the storage desired by the successor, and
        // the use contains the storage currently allocated in this block.
        if (use->hasRegister() && def->hasRegister()) {
            // If both are in different registers, perform a parallel move.
            if (use->reg() != def->reg()) {
                if (!info->phis.move(use->reg(), def->reg()))
                    return false;
            }
        } else if (use->hasRegister() && !def->hasRegister()) {
            // Emit a store to the stack slot.
            if (!info->phis.move(use->reg(), def->backingStack()))
                return false;
        } else {
            if (def->hasRegister()) {
                // Is its register free?
                if (!state[def->reg()]) {
                    assign(use, def->reg());
                } else {
                    // Emit a load from the stack, since eviction is
                    // inevitable.
                    if (!allocateStack(use))
                        return false;
                    if (!info->phis.move(use->backingStack(), def->reg()))
                        return false;
                }
            } else {
                if (!allocateStack(use))
                    return false;

                AnyRegister reg;
                if (!allocate(use->type(), TEMPORARY, &reg))
                    return false;

                // Memory to memory moves are not parallel.
                LMove *move = new LMove;
                if (!move->add(use->backingStack(), LAllocation(reg)))
                    return false;
                if (!move->add(LAllocation(reg), def->backingStack()))
                    return false;
                block->insertBefore(*block->instructions().rbegin(), move);
            }
        }
    }

    // Now insert restores (if any) and phi moves.
    JS_ASSERT(!aligns);
    JS_ASSERT(!spills);
    if (restores)
        block->insertBefore(*block->instructions().rbegin(), restores);
    if (info->phis.moves)
        block->insertBefore(*block->instructions().rbegin(), info->phis.moves);

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

    // Merge state from each additional successor.
    for (size_t i = 1; i < mblock->numSuccessors(); i++) {
        LBlock *rightblock = mblock->getSuccessor(i)->lir();

        for (size_t i = 0; i < RegisterCodes::Total; i++) {
            AnyRegister reg = AnyRegister(Register::FromCode(i));
            if (!mergeRegisterState(reg, leftblock, rightblock))
                return false;
        }
        for (size_t i = 0; i < FloatRegisterCodes::Total; i++) {
            AnyRegister reg = AnyRegister(FloatRegister::FromCode(i));
            if (!mergeRegisterState(reg, leftblock, rightblock))
                return false;
        }

        // If there were parallel moves, append them now.
        BlockInfo *info = blockInfo(rightblock);
        if (info->restores.moves)
            rightblock->insertBefore(*rightblock->begin(), info->restores.moves);
    }

    // Insert moves for phis.
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

        // Merge allocation state from our predecessors.
        if (!mergeAllocationState(block))
            return false;

        // Allocate registers.
        if (!allocateRegistersInBlock(block))
            return false;

        // Kill phis.
        for (size_t i = 0; i < block->numPhis(); i++) {
            LPhi *phi = block->getPhi(i);
            JS_ASSERT(phi->numDefs() == 1);

            VirtualRegister *vr = getVirtualRegister(phi->getDef(0));
            kill(vr);
        }

        // At the top of the block, copy our allocation state for our
        // predecessors.
        blockInfo(block)->in = state;
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

    blocks = gen->allocate<BlockInfo>(graph.numBlocks());
    for (size_t i = 0; i < graph.numBlocks(); i++)
        new (&blocks[i]) BlockInfo();

    findDefinitions();
    if (!allocateRegisters())
        return false;
    graph.setStackHeight(stackSlots.stackHeight());

    return true;
}

