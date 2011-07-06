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
 *   Andrew Drake <adrake@adrake.org>
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

#include <limits.h>
#include "BitSet.h"
#include "LinearScan.h"
#include "IonLIR-inl.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

LiveInterval *
LiveInterval::New(VirtualRegister *reg)
{
    LiveInterval *result = new LiveInterval(reg);
    return result;
}

bool
LiveInterval::addRange(CodePosition from, CodePosition to)
{
    JS_ASSERT(from <= to);

    Range newRange(from, to);

    Range *i;
    // Find the location to insert the new range
    for (i = ranges_.end() - 1; i >= ranges_.begin(); i--) {
        if (newRange.from <= i->from)
            break;
    }
    // Perform coalescing on overlapping ranges
    for (; i >= ranges_.begin(); i--) {
        if (newRange.to < i->from)
            break;
        if (newRange.to < i->to)
            newRange.to = i->to;
        ranges_.erase(i);
    }

    return ranges_.insert(i + 1, newRange);
}

void
LiveInterval::setFrom(CodePosition from)
{
    while (!ranges_.empty()) {
        if (ranges_.back().to < from) {
            ranges_.erase(&ranges_.back());
        } else {
            ranges_.back().from = from;
            break;
        }
    }
}

CodePosition
LiveInterval::start()
{
    JS_ASSERT(!ranges_.empty());
    return ranges_.back().from;
}

CodePosition
LiveInterval::end()
{
    JS_ASSERT(!ranges_.empty());
    return ranges_.begin()->to;
}

bool
LiveInterval::covers(CodePosition pos)
{
    for (size_t i = 0; i < ranges_.length(); i++) {
        if (ranges_[i].to < pos)
            return false;
        if (ranges_[i].from <= pos)
            return true;
    }
    return false;
}

CodePosition
LiveInterval::nextCoveredAfter(CodePosition pos)
{
    for (size_t i = 0; i < ranges_.length(); i++) {
        if (ranges_[i].to < pos) {
            if (i)
                return ranges_[i-1].from;
            break;
        }
        if (ranges_[i].from <= pos)
            return pos;
    }
    return CodePosition::MIN;
}

CodePosition
LiveInterval::intersect(LiveInterval *other)
{
    size_t i = 0;
    size_t j = 0;

    while (i < ranges_.length() && j < other->ranges_.length()) {
        if (ranges_[i].from <= other->ranges_[j].from) {
            if (other->ranges_[j].from < ranges_[i].to)
                return other->ranges_[j].from;
            i++;
        } else if (other->ranges_[j].from <= ranges_[i].from) {
            if (ranges_[i].from < other->ranges_[j].to)
                return ranges_[i].from;
            j++;
        }
    }

    return CodePosition::MIN;
}

size_t
LiveInterval::numRanges()
{
    return ranges_.length();
}

LiveInterval::Range *
LiveInterval::getRange(size_t i)
{
    return &ranges_[i];
}

bool
LiveInterval::splitFrom(CodePosition pos, LiveInterval *after)
{
    JS_ASSERT(pos >= start() && pos <= end());

    for (Range *i = ranges_.begin(); i != ranges_.end(); ranges_.erase(i)) {
        if (pos > i->from) {
            if (pos <= i->to) {
                // Split the range
                Range split(pos, i->to);
                i->to = pos.previous();
                if (!after->ranges_.append(split))
                    return false;
            }
            return true;
        } else if (!after->ranges_.append(*i)) {
            return false;
        }
    }

    JS_NOT_REACHED("Emptied an interval");
    return false;
}

LiveInterval *
VirtualRegister::intervalFor(CodePosition pos)
{
    for (LiveInterval **i = intervals_.begin(); i != intervals_.end(); i++) {
        if ((*i)->covers(pos))
            return *i;
    }
    return NULL;
}

CodePosition
VirtualRegister::nextUseAfter(CodePosition after)
{
    CodePosition min = CodePosition::MAX;
    for (LOperand *i = uses_.begin(); i != uses_.end(); i++) {
        CodePosition ip(i->ins->id(), CodePosition::INPUT);
        if (i->use->policy() != LUse::KEEPALIVE && ip >= after && ip < min)
            min = ip;
    }
    return min;
}

CodePosition
VirtualRegister::nextIncompatibleUseAfter(CodePosition after, LAllocation alloc)
{
    CodePosition min = CodePosition::MAX;
    for (LOperand *i = uses_.begin(); i != uses_.end(); i++) {
        CodePosition ip(i->ins->id(), CodePosition::INPUT);
        if (i->use->policy() == LUse::ANY && (alloc.isStackSlot() ||
                                              alloc.isGeneralReg() ||
                                              alloc.isArgument()))
        {
            continue;
        }
        if (i->use->policy() == LUse::REGISTER && alloc.isGeneralReg())
            continue;
        if (i->use->isFixedRegister() && alloc.isGeneralReg() &&
            alloc.toGeneralReg()->reg().code() == i->use->registerCode())
        {
            continue;
        }
        if (i->use->policy() == LUse::KEEPALIVE)
            continue;
        if (ip >= after && ip < min)
            min = ip;
    }
    return min;
}

LiveInterval *
VirtualRegister::splitIntervalAfter(VirtualRegister *ins, CodePosition pos)
{
    // Insert a move instruction or grab the last one
    LMove *move;
    LInstructionIterator iter(ins->ins());
    if (iter.prev() != ins->block()->end() && iter.prev()->isMove()) {
        move = iter.prev()->toMove();
    } else {
        move = new LMove;
        ins->block()->insertBefore(ins->ins(), move);
    }

    // Find the interval we need to split
    LiveInterval **oldInterval;
    for (oldInterval = intervals_.begin(); oldInterval != intervals_.end(); oldInterval++) {
        if ((*oldInterval)->start() <= pos && pos <= (*oldInterval)->end())
            break;
    }
    JS_ASSERT(oldInterval != intervals_.end());

    // Set up the new interval
    LiveInterval *newInterval = LiveInterval::New(this);
    if (!(*oldInterval)->splitFrom(pos, newInterval))
        return NULL;

    JS_ASSERT((*oldInterval)->numRanges() > 0);
    JS_ASSERT(newInterval->numRanges() > 0);

    // Insert the new interval
    if (!intervals_.insert(oldInterval + 1, newInterval))
        return NULL;

    // Add the move entry to the move
    move->add((*oldInterval)->getAllocation(), newInterval->getAllocation());

    return newInterval;
}

const CodePosition CodePosition::MAX(UINT_MAX);
const CodePosition CodePosition::MIN(0);

bool
RegisterAllocator::createDataStructures()
{
    allowedRegs = RegisterSet::All();

    // Allocate free/next use data structures
    freeUntilPos = lir->mir()->allocate<CodePosition>(RegisterCodes::Total);
    nextUsePos = lir->mir()->allocate<CodePosition>(RegisterCodes::Total);

    // Build virtual register objects
    vregs = new VirtualRegister[graph.numVirtualRegisters()];

    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *b = graph.getBlock(i);
        for (LInstructionIterator ins = b->begin(); ins != b->end(); ins++) {
            if (ins->numDefs()) {
                for (size_t j = 0; j < ins->numDefs(); j++) {
                    uint32 reg = ins->getDef(j)->virtualRegister();
                    if (!vregs[reg].init(reg, b, *ins, ins->getDef(j)))
                        return false;
                }
            } else {
                if (!vregs[ins->id()].init(ins->id(), b, *ins, NULL))
                    return false;
            }
            if (ins->numTemps()) {
                for (size_t j = 0; j < ins->numTemps(); j++) {
                    uint32 reg = ins->getTemp(j)->virtualRegister();
                    if (!vregs[reg].init(reg, b, *ins, ins->getTemp(j)))
                        return false;
                }
            }
        }
        for (size_t j = 0; j < b->numPhis(); j++) {
            LPhi *phi = b->getPhi(j);
            uint32 reg = phi->getDef(0)->virtualRegister();
            if (!vregs[reg].init(phi->id(), b, phi, phi->getDef(0)))
                return false;
        }
    }

    return true;
}

bool
RegisterAllocator::buildLivenessInfo()
{
    BitSet **liveIn = new BitSet*[graph.numBlocks()];
    if (!liveIn)
        return false;

    for (size_t i = graph.numBlocks(); i > 0; i--) {
        LBlock *b = graph.getBlock(i - 1);
        MBasicBlock *mb = b->mir();

        BitSet *live = BitSet::New(graph.numVirtualRegisters());
        if (!live)
            return false;

        // Propagate liveIn from our successors to us
        for (size_t i = 0; i < mb->lastIns()->numSuccessors(); i++) {
            MBasicBlock *successor = mb->lastIns()->getSuccessor(i);
            // Skip backedges, as we fix them up at the loop header.
            if (mb->id() < successor->id())
                live->insertAll(liveIn[successor->id()]);
        }

        // Add successor phis
        if (mb->successorWithPhis()) {
            LBlock *phiSuccessor = mb->successorWithPhis()->lir();
            for (unsigned int j = 0; j < phiSuccessor->numPhis(); j++) {
                LPhi *phi = phiSuccessor->getPhi(j);
                LAllocation *use = phi->getOperand(mb->positionInPhiSuccessor());
                uint32 reg = use->toUse()->virtualRegister();
                live->insert(reg);
            }
        }

        // Variables are assumed alive for the entire block, a define shortens
        // the interval to the point of definition.
        for (BitSet::Iterator i(live->begin()); i != live->end(); i++)
            vregs[*i].getInterval(0)->addRange(outputOf(b->firstId()), inputOf(b->lastId() + 1));

        // Shorten the front end of live intervals for live variables to their
        // point of definition, if found.
        for (LInstructionReverseIterator ins = b->rbegin();
             ins != b->rend();
             ins++)
        {
            for (size_t i = 0; i < ins->numDefs(); i++) {
                uint32 reg = ins->getDef(i)->virtualRegister();
                vregs[reg].getInterval(0)->setFrom(outputOf(*ins));
                live->remove(reg);
            }
            for (size_t i = 0; i < ins->numTemps(); i++) {
                uint32 reg = ins->getTemp(i)->virtualRegister();
                vregs[reg].getInterval(0)->addRange(outputOf(*ins), outputOf(*ins));
            }
            for (LInstruction::InputIterator alloc(**ins);
                 alloc.more();
                 alloc.next())
            {
                if (alloc->isUse()) {
                    LUse *use = alloc->toUse();
                    uint32 reg = use->virtualRegister();

                    if (use->policy() != LUse::KEEPALIVE)
                        vregs[reg].addUse(LOperand(use, *ins));

                    if (ins->id() == b->firstId())
                        vregs[reg].getInterval(0)->addRange(inputOf(*ins), inputOf(*ins));
                    else
                        vregs[reg].getInterval(0)->addRange(outputOf(b->firstId()), inputOf(*ins));
                    live->insert(reg);
                }
            }
        }

        // Phis have simultaneous assignment semantics at block begin, so at
        // the beginning of the block we can be sure that liveIn does not
        // contain any phi outputs.
        for (unsigned int i = 0; i < b->numPhis(); i++)
            live->remove(b->getPhi(i)->getDef(0)->virtualRegister());

        // While not necessarily true, we make the simplifying assumption that
        // variables live at the loop header must be live for the entire loop.
        if (mb->isLoopHeader()) {
            MBasicBlock *backedge = mb->backedge();
            for (BitSet::Iterator i(live->begin()); i != live->end(); i++)
                vregs[*i].getInterval(0)->addRange(outputOf(b->firstId()),
                                                   outputOf(backedge->lir()->lastId()));
        }

        liveIn[mb->id()] = live;
    }

    return true;
}

bool
RegisterAllocator::allocateRegisters()
{
    // Enqueue intervals for allocation
    for (size_t i = 1; i < graph.numVirtualRegisters(); i++) {
        LiveInterval *live = vregs[i].getInterval(0);
        if (live->numRanges() > 0)
            unhandled.enqueue(live);
    }

    // Iterate through all intervals in ascending start order
    while ((current = unhandled.dequeue()) != NULL) {
        JS_ASSERT(current->getAllocation()->isUse());
        JS_ASSERT(current->numRanges() > 0);

        CodePosition position = current->start();

        IonSpew(IonSpew_LSRA, "Processing %d = [%u, %u]",
                current->reg()->reg(), current->start().pos(),
                current->end().pos());

        // Shift active intervals to the inactive or handled sets as appropriate
        for (IntervalIterator i(active.begin()); i != active.end(); ) {
            LiveInterval *it = *i;
            JS_ASSERT(it->getAllocation()->isGeneralReg());
            JS_ASSERT(it->numRanges() > 0);

            if (it->end() < position) {
                i = active.removeAt(i);
                finishInterval(it);
            } else if (!it->covers(position)) {
                i = active.removeAt(i);
                inactive.insert(it);
            } else {
                i++;
            }
        }

        // Shift inactive intervals to the active or handled sets as appropriate
        for (IntervalIterator i(inactive.begin()); i != inactive.end(); ) {
            LiveInterval *it = *i;
            JS_ASSERT(it->getAllocation()->isGeneralReg());
            JS_ASSERT(it->numRanges() > 0);

            if (it->end() < position) {
                i = inactive.removeAt(i);
                finishInterval(it);
            } else if (it->covers(position)) {
                i = inactive.removeAt(i);
                active.insert(it);
            } else {
                i++;
            }
        }

#ifdef DEBUG
        // Sanity check all intervals in all sets
        for (IntervalIterator i(active.begin()); i != active.end(); i++) {
            JS_ASSERT(i->getAllocation()->isGeneralReg());
            JS_ASSERT(i->numRanges() > 0);
            JS_ASSERT(i->covers(position));

            for (IntervalIterator j(active.begin()); j != i; j++)
                JS_ASSERT(canCoexist(*i, *j));
        }

        for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
            JS_ASSERT(i->getAllocation()->isGeneralReg());
            JS_ASSERT(i->numRanges() > 0);
            JS_ASSERT(i->end() >= position);
            JS_ASSERT(!i->covers(position));

            for (IntervalIterator j(active.begin()); j != active.end(); j++) {
                JS_ASSERT(*i != *j);
                JS_ASSERT(canCoexist(*i, *j));
            }
            for (IntervalIterator j(inactive.begin()); j != i; j++)
                JS_ASSERT(canCoexist(*i, *j));
        }

        for (IntervalIterator i(handled.begin()); i != handled.end(); i++) {
            JS_ASSERT(!i->getAllocation()->isUse());
            JS_ASSERT(i->numRanges() > 0);
            if (i->getAllocation()->isGeneralReg()) {
                JS_ASSERT(i->end() < position);
                JS_ASSERT(!i->covers(position));
            }

            for (IntervalIterator j(active.begin()); j != active.end(); j++)
                JS_ASSERT(*i != *j);
            for (IntervalIterator j(inactive.begin()); j != inactive.end(); j++)
                JS_ASSERT(*i != *j);
        }
#endif

        // Check the allocation policy if this is a definition or a use
        bool mustHaveRegister = false;
        if (position.ins() == current->reg()->ins()->id()) {
            JS_ASSERT(position.subpos() == CodePosition::OUTPUT);

            LDefinition *def = current->reg()->def();
            LDefinition::Policy pol = current->reg()->def()->policy();
            if (pol == LDefinition::PRESET) {
                IonSpew(IonSpew_LSRA, " Definition has preset policy.");
                current->setFlag(LiveInterval::FIXED);
                if (!assign(*def->output()))
                    return false;
                continue;
            }
            if (pol == LDefinition::MUST_REUSE_INPUT) {
                IonSpew(IonSpew_LSRA, " Definition has 'must reuse input' policy.");
                LInstruction *ins = current->reg()->ins();
                JS_ASSERT(ins->numOperands() > 0);
                JS_ASSERT(ins->getOperand(0)->isUse());
                uint32 inputReg = ins->getOperand(0)->toUse()->virtualRegister();
                LiveInterval *inputInterval = vregs[inputReg].intervalFor(inputOf(ins));
                JS_ASSERT(inputInterval);
                JS_ASSERT(inputInterval->getAllocation()->isGeneralReg());
                if (!assign(*inputInterval->getAllocation()))
                    return false;
                continue;
            }

            JS_ASSERT(pol == LDefinition::DEFAULT);
            mustHaveRegister = true;
        } else {
            // Scan uses for any at the current instruction
            LOperand *fixedOp = NULL;
            for (size_t i = 0; i < current->reg()->numUses(); i++) {
                LOperand *op = current->reg()->getUse(i);
                if (op->ins->id() == position.ins()) {
                    LUse::Policy pol = op->use->policy();
                    if (pol == LUse::FIXED) {
                        IonSpew(IonSpew_LSRA, " Use has fixed policy.");
                        JS_ASSERT(!fixedOp);
                        fixedOp = op;
                    } else if (pol == LUse::REGISTER) {
                        IonSpew(IonSpew_LSRA, " Use has 'must have register' policy.");
                        mustHaveRegister = true;
                    } else {
                        JS_ASSERT(pol == LUse::ANY || pol == LUse::KEEPALIVE);
                    }
                }
            }

            // Handle the fixed constraint if present
            if (fixedOp) {
                current->setFlag(LiveInterval::FIXED);
                if (!assign(LGeneralReg(Register::FromCode(fixedOp->use->registerCode()))))
                    return false;
                continue;
            }
        }

        // Try to allocate a free register
        IonSpew(IonSpew_LSRA, " Attempting free register allocation");

        // Find the best register
        Register best = findBestFreeRegister();
        IonSpew(IonSpew_LSRA, "  Decided best register was %u, free until %u", best.code(),
                freeUntilPos[best.code()].pos());

        // Attempt to allocate
        if (freeUntilPos[best.code()] != CodePosition::MIN) {
            // Split when the register is next needed if necessary
            if (freeUntilPos[best.code()] <= current->end()) {
                if (!splitInterval(current, freeUntilPos[best.code()]))
                    return false;
            }
            if (!assign(LGeneralReg(best)))
                return false;
            continue;
        }

        // We need to allocate a blocked register
        IonSpew(IonSpew_LSRA, "  Unable to allocate free register");
        IonSpew(IonSpew_LSRA, " Attempting blocked register allocation");

        // Find the best register
        best = findBestBlockedRegister();

        // Figure out whether to spill the blocker or ourselves
        CodePosition firstUse = current->reg()->nextUseAfter(position);
        IonSpew(IonSpew_LSRA, "  Current interval next used at %u", firstUse.pos());

        // Deal with constraints
        if (mustHaveRegister) {
            IonSpew(IonSpew_LSRA, "   But this interval needs a register.");
            firstUse = position;
        }

        // We only spill the blocking interval if it is a strict improvement
        // to do so, otherwise we will continuously re-spill intervals.
        if (firstUse >= nextUsePos[best.code()]) {
            if (!spill())
                return false;
        } else {
            if (!assign(LGeneralReg(best)))
                return false;
        }
    }

#ifdef DEBUG
    // Verify final intervals are valid
    for (IntervalIterator i(handled.begin()); i != handled.end(); i++) {
        for (IntervalIterator j(handled.begin()); j != i; j++) {
            JS_ASSERT(*i != *j);
            JS_ASSERT(canCoexist(*i, *j));
        }
    }

    // Verify all moves are filled in
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *b = graph.getBlock(i);
        for (LInstructionIterator ins = b->begin(); ins != b->end(); ins++) {
            if (ins->isMove()) {
                LMove *move = ins->toMove();
                for (size_t j = 0; j < move->numEntries(); j++) {
                    LMove::Entry *entry = move->getEntry(j);
                    JS_ASSERT(!entry->from->isUse());
                    JS_ASSERT(!entry->to->isUse());
                }
            }
        }
    }
#endif

    return true;
}

bool
RegisterAllocator::resolveControlFlow()
{
    // FIXME (668292): Implement
    return true;
}

bool
RegisterAllocator::reifyAllocations()
{
    // FIXME (668295): Implement
    return true;
}

bool
RegisterAllocator::splitInterval(LiveInterval *interval, CodePosition pos)
{
    VirtualRegister *splitIns = &vregs[pos.ins()];

    LiveInterval *after = interval->reg()->splitIntervalAfter(splitIns, pos);
    if (!after)
        return false;

    IonSpew(IonSpew_LSRA, "  Split interval to %u = [%u, %u]",
            interval->reg()->reg(), interval->start().pos(),
            interval->end().pos());
    IonSpew(IonSpew_LSRA, "  Created new interval %u = [%u, %u]",
            after->reg()->reg(), after->start().pos(),
            after->end().pos());

    unhandled.enqueue(after);
    return true;
}

bool
RegisterAllocator::assign(LAllocation allocation)
{
    if (allocation.isGeneralReg())
        IonSpew(IonSpew_LSRA, "Assigning register %s", allocation.toGeneralReg()->reg().name());
    current->setAllocation(allocation);

    // Split this interval at the next incompatible one
    CodePosition toSplit = current->reg()->nextIncompatibleUseAfter(current->start(), allocation);
    if (toSplit <= current->end()) {
        JS_ASSERT(toSplit != current->start());
        if (!splitInterval(current, toSplit))
            return false;
    }

    if (allocation.isGeneralReg()) {
        // Split the blocking interval if it exists
        for (IntervalIterator i(active.begin()); i != active.end(); i++) {
            if (i->getAllocation()->isGeneralReg() &&
                i->getAllocation()->toGeneralReg()->reg() == allocation.toGeneralReg()->reg())
            {
                IonSpew(IonSpew_LSRA, " Splitting active interval %u = [%u, %u]",
                        i->reg()->ins()->id(), i->start().pos(), i->end().pos());

                LiveInterval *it = *i;
                if (it->start() == current->start()) {
                    // We assigned a needed fixed register to another
                    // interval, so we break a very important rule
                    // and "de-assign" the other one. We can do this
                    // safely (i.e. be guaranteed to terminate) iff
                    // the register we are reallocating was not itself
                    // assigned to fulfill a FIXED constraint.
                    JS_ASSERT(!it->hasFlag(LiveInterval::FIXED));

                    it->setAllocation(LUse(it->reg()->reg(), LUse::ANY));
                    active.removeAt(i);
                    unhandled.enqueue(it);
                    break;
                } else {
                    JS_ASSERT(it->covers(current->start()));
                    JS_ASSERT(it->start() != current->start());

                    if (!splitInterval(it, current->start()))
                        return false;

                    active.removeAt(i);
                    handled.insert(it);
                    break;
                }
            }
        }

        // Split any inactive intervals at the next live point
        for (IntervalIterator i(inactive.begin()); i != inactive.end(); ) {
            if (i->getAllocation()->isGeneralReg() &&
                i->getAllocation()->toGeneralReg()->reg() == allocation.toGeneralReg()->reg())
            {
                IonSpew(IonSpew_LSRA, " Splitting inactive interval %u = [%u, %u]",
                        i->reg()->ins()->id(), i->start().pos(), i->end().pos());

                LiveInterval *it = *i;
                CodePosition nextActive = it->nextCoveredAfter(current->start());
                JS_ASSERT(nextActive != CodePosition::MIN);

                if (!splitInterval(it, nextActive))
                    return false;

                i = inactive.removeAt(i);
                finishInterval(it);
            } else {
                i++;
            }
        }
    }

    if (allocation.isGeneralReg())
        active.insert(current);
    else
        finishInterval(current);

    return true;
}

bool
RegisterAllocator::spill()
{
    IonSpew(IonSpew_LSRA, "  Decided to spill current interval");

    uint32 stackSlot;
    if (!stackAssignment.allocateSlot(&stackSlot))
        return false;

    IonSpew(IonSpew_LSRA, "  Allocated spill slot %u", stackSlot);

    return assign(LStackSlot(stackSlot));
}

void
RegisterAllocator::finishInterval(LiveInterval *interval)
{
    LAllocation *alloc = interval->getAllocation();
    JS_ASSERT(!alloc->isUse());

    if (alloc->isStackSlot()) {
        if (alloc->toStackSlot()->isDouble())
            stackAssignment.freeDoubleSlot(alloc->toStackSlot()->slot());
        else
            stackAssignment.freeSlot(alloc->toStackSlot()->slot());
    }

    handled.insert(interval);
}

Register
RegisterAllocator::findBestFreeRegister()
{
    // Update freeUntilPos for search
    IonSpew(IonSpew_LSRA, "  Computing freeUntilPos");
    for (size_t i = 0; i < RegisterCodes::Total; i++) {
        if (allowedRegs.has(Register::FromCode(i)))
            freeUntilPos[i] = CodePosition::MAX;
        else
            freeUntilPos[i] = CodePosition::MIN;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        Register reg = i->getAllocation()->toGeneralReg()->reg();
        freeUntilPos[reg.code()] = CodePosition::MIN;
        IonSpew(IonSpew_LSRA, "   Register %u not free", reg.code());
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        Register reg = i->getAllocation()->toGeneralReg()->reg();
        CodePosition pos = current->intersect(*i);
        if (pos != CodePosition::MIN && pos < freeUntilPos[reg.code()]) {
            freeUntilPos[reg.code()] = pos;
            IonSpew(IonSpew_LSRA, "   Register %u free until %u", reg.code(), pos.pos());
        }
    }

    // Search freeUntilPos for largest value
    Register best = Register::FromCode(0);
    for (uint32 i = 0; i < RegisterCodes::Total; i++) {
        if (freeUntilPos[i] > freeUntilPos[best.code()])
            best = Register::FromCode(i);
    }

    return best;
}

Register
RegisterAllocator::findBestBlockedRegister()
{
    // Update nextUsePos for search
    IonSpew(IonSpew_LSRA, "  Computing nextUsePos");
    for (size_t i = 0; i < RegisterCodes::Total; i++) {
        if (allowedRegs.has(Register::FromCode(i)))
            nextUsePos[i] = CodePosition::MAX;
        else
            nextUsePos[i] = CodePosition::MIN;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        Register reg = i->getAllocation()->toGeneralReg()->reg();
        CodePosition nextUse = i->reg()->nextUseAfter(current->start());
        IonSpew(IonSpew_LSRA, "   Register %u next used %u", reg.code(), nextUse.pos());

        if (i->start() == current->start()) {
            IonSpew(IonSpew_LSRA, "    Disqualifying due to recency.");
            nextUsePos[reg.code()] = current->start();
        } else {
            nextUsePos[reg.code()] = nextUse;
        }
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        Register reg = i->getAllocation()->toGeneralReg()->reg();
        CodePosition pos = i->reg()->nextUseAfter(current->start());
        JS_ASSERT(i->covers(pos) || i->end() == pos);
        if (pos < nextUsePos[reg.code()]) {
            nextUsePos[reg.code()] = pos;
            IonSpew(IonSpew_LSRA, "   Register %u next used %u", reg.code(), pos.pos());
        }
    }

    // Search nextUsePos for largest value
    Register best = Register::FromCode(0);
    for (size_t i = 0; i < RegisterCodes::Total; i++) {
        if (nextUsePos[i] > nextUsePos[best.code()])
            best = Register::FromCode(i);
    }
    IonSpew(IonSpew_LSRA, "  Decided best register was %u", best.code());
    return best;
}


bool
RegisterAllocator::canCoexist(LiveInterval *a, LiveInterval *b)
{
    LAllocation *aa = a->getAllocation();
    LAllocation *ba = b->getAllocation();
    if (aa->isGeneralReg() && ba->isGeneralReg() &&
        aa->toGeneralReg()->reg() == ba->toGeneralReg()->reg())
    {
        return a->intersect(b) == CodePosition::MIN;
    }
    return true;
}

bool
RegisterAllocator::go()
{
    IonSpew(IonSpew_LSRA, "Beginning register allocation");

    IonSpew(IonSpew_LSRA, "Beginning creation of initial data structures");
    if (!createDataStructures())
        return false;
    IonSpew(IonSpew_LSRA, "Creation of initial data structures completed");

    IonSpew(IonSpew_LSRA, "Beginning liveness analysis");
    if (!buildLivenessInfo())
        return false;
    IonSpew(IonSpew_LSRA, "Liveness analysis complete");

    IonSpew(IonSpew_LSRA, "Beginning preliminary register allocation");
    if (!allocateRegisters())
        return false;
    IonSpew(IonSpew_LSRA, "Preliminary register allocation complete");

    IonSpew(IonSpew_LSRA, "Beginning control flow resolution");
    if (!resolveControlFlow())
        return false;
    IonSpew(IonSpew_LSRA, "Control flow resolution complete");

    IonSpew(IonSpew_LSRA, "Beginning register allocation reification");
    if (!reifyAllocations())
        return false;
    IonSpew(IonSpew_LSRA, "Register allocation reification complete");

    IonSpew(IonSpew_LSRA, "Register allocation complete");

    return true;
}

void
RegisterAllocator::UnhandledQueue::enqueue(LiveInterval *interval)
{
    IntervalIterator i(begin());
    for (; i != end(); i++) {
        if (i->start() < interval->start())
            break;
    }
    insertBefore(*i, interval);
}

LiveInterval *
RegisterAllocator::UnhandledQueue::dequeue()
{
    if (rbegin() == rend())
        return NULL;

    LiveInterval *result = *rbegin();
    remove(result);
    return result;
}
