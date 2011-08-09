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

bool
LiveInterval::addRange(CodePosition from, CodePosition to)
{
    JS_ASSERT(from <= to);

    Range newRange(from, to);

    Range *i;
    // Find the location to insert the new range
    for (i = ranges_.end() - 1; i >= ranges_.begin(); i--) {
        if (newRange.from <= i->to) {
            if (i->from < newRange.from)
                newRange.from = i->from;
            break;
        }
    }
    // Perform coalescing on overlapping ranges
    for (; i >= ranges_.begin(); i--) {
        if (newRange.to < i->from.previous())
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

/*
 * This function takes the callee interval and moves all ranges following or
 * including provided position to the target interval. Additionally, if a
 * range in the callee interval spans the given position, it is split and the
 * latter half is placed in the target interval.
 *
 * This function should only be called if it is known that the interval should
 * actually be split (and, presumably, a move inserted). As such, it is an
 * error for the caller to request a split that moves all intervals into the
 * target. Doing so will trip the assertion at the bottom of the function.
 */
bool
LiveInterval::splitFrom(CodePosition pos, LiveInterval *after)
{
    JS_ASSERT(pos >= start() && pos <= end());
    JS_ASSERT(after->ranges_.empty());

    // Move all intervals over to the target
    size_t bufferLength = ranges_.length();
    Range *buffer = ranges_.extractRawBuffer();
    if (!buffer)
        return false;
    after->ranges_.replaceRawBuffer(buffer, bufferLength);

    // Move intervals back as required
    for (Range *i = &after->ranges_.back(); i >= after->ranges_.begin(); i--) {
        if (pos > i->to)
            continue;

        if (pos > i->from) {
            // Split the range
            Range split(i->from, pos.previous());
            i->from = pos;
            if (!ranges_.append(split))
                return false;
        }
        if (!ranges_.append(i + 1, after->ranges_.end()))
            return false;
        after->ranges_.shrinkBy(after->ranges_.end() - i - 1);
        return true;
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
        if (pos <= (*i)->end())
            break;
    }
    return NULL;
}

LiveInterval *
VirtualRegister::getFirstInterval()
{
    JS_ASSERT(!intervals_.empty());
    return intervals_[0];
}

LOperand *
VirtualRegister::nextUseAfter(CodePosition after)
{
    LOperand *min = NULL;
    CodePosition minPos = CodePosition::MAX;
    for (LOperand *i = uses_.begin(); i != uses_.end(); i++) {
        CodePosition ip(i->ins->id(), CodePosition::INPUT);
        if (i->use->policy() != LUse::KEEPALIVE && ip >= after && ip < minPos) {
            min = i;
            minPos = ip;
        }
    }
    return min;
}

/*
 * This function locates the first "real" use of the callee virtual register
 * that follows the given code position. Non-"real" uses are currently just
 * snapshots, which keep virtual registers alive but do not result in the
 * generation of code that use them.
 */
CodePosition
VirtualRegister::nextUsePosAfter(CodePosition after)
{
    LOperand *min = nextUseAfter(after);
    if (min)
        return CodePosition(min->ins->id(), CodePosition::INPUT);
    return CodePosition::MAX;
}

/*
 * This function finds the position of the next use of the callee virtual
 * register that is incompatible with the provideded allocation. For example,
 * a use with a REGISTER policy would be incompatible with a stack slot
 * allocation.
 */
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

const CodePosition CodePosition::MAX(UINT_MAX);
const CodePosition CodePosition::MIN(0);

/*
 * This function pre-allocates and initializes as much global state as possible
 * to avoid littering the algorithms with memory management cruft.
 */
bool
LinearScanAllocator::createDataStructures()
{
    allowedRegs = RegisterSet::All();

    liveIn = lir->mir()->allocate<BitSet*>(graph.numBlockIds());
    freeUntilPos = lir->mir()->allocate<CodePosition>(Registers::Total);
    nextUsePos = lir->mir()->allocate<CodePosition>(Registers::Total);
    if (!liveIn || !freeUntilPos || !nextUsePos)
        return false;

    if (!vregs.init(lir->mir(), graph.numVirtualRegisters()))
        return false;

    // Build virtual register objects
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *block = graph.getBlock(i);
        for (LInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            bool foundRealDef = false;
            for (size_t j = 0; j < ins->numDefs(); j++) {
                if (ins->getDef(j)->policy() != LDefinition::REDEFINED) {
                    foundRealDef = true;
                    uint32 reg = ins->getDef(j)->virtualRegister();
                    if (!vregs[reg].init(reg, block, *ins, ins->getDef(j)))
                        return false;
                }
            }
            if (!foundRealDef) {
                if (!vregs[*ins].init(ins->id(), block, *ins, NULL))
                    return false;
            }
            if (ins->numTemps()) {
                for (size_t j = 0; j < ins->numTemps(); j++) {
                    LDefinition *def = ins->getTemp(j);
                    if (!vregs[def].init(def->virtualRegister(), block, *ins, def, true))
                        return false;
                }
            }
        }
        for (size_t j = 0; j < block->numPhis(); j++) {
            LPhi *phi = block->getPhi(j);
            LDefinition *def = phi->getDef(0);
            if (!vregs[def].init(phi->id(), block, phi, def))
                return false;
        }
    }

    return true;
}

/*
 * This function builds up liveness intervals for all virtual registers
 * defined in the function. Additionally, it populates the liveIn array with
 * information about which registers are live at the beginning of a block, to
 * aid resolution and reification in a later phase.
 *
 * The algorithm is based on the one published in:
 *
 * Wimmer, Christian, and Michael Franz. "Linear Scan Register Allocation on
 *     SSA Form." Proceedings of the International Symposium on Code Generation
 *     and Optimization. Toronto, Ontario, Canada, ACM. 2010. 170-79. PDF.
 *
 * The algorithm operates on blocks ordered such that dominators of a block
 * are before the block itself, and such that all blocks of a loop are
 * contiguous. It proceeds backwards over the instructions in this order,
 * marking registers live at their uses, ending their live intervals at
 * definitions, and recording which registers are live at the top of every
 * block. To deal with loop backedges, variables live at the beginning of
 * a loop gain an interval covering the entire loop.
 */
bool
LinearScanAllocator::buildLivenessInfo()
{
    Vector<MBasicBlock *, 1, SystemAllocPolicy> loopWorkList;
    BitSet *loopDone = BitSet::New(graph.numBlockIds());
    if (!loopDone)
        return false;

    for (size_t i = graph.numBlocks(); i > 0; i--) {
        LBlock *block = graph.getBlock(i - 1);
        MBasicBlock *mblock = block->mir();

        BitSet *live = BitSet::New(graph.numVirtualRegisters());
        if (!live)
            return false;
        liveIn[mblock->id()] = live;

        // Propagate liveIn from our successors to us
        for (size_t i = 0; i < mblock->lastIns()->numSuccessors(); i++) {
            MBasicBlock *successor = mblock->lastIns()->getSuccessor(i);
            // Skip backedges, as we fix them up at the loop header.
            if (mblock->id() < successor->id())
                live->insertAll(liveIn[successor->id()]);
        }

        // Add successor phis
        if (mblock->successorWithPhis()) {
            LBlock *phiSuccessor = mblock->successorWithPhis()->lir();
            for (unsigned int j = 0; j < phiSuccessor->numPhis(); j++) {
                LPhi *phi = phiSuccessor->getPhi(j);
                LAllocation *use = phi->getOperand(mblock->positionInPhiSuccessor());
                uint32 reg = use->toUse()->virtualRegister();
                live->insert(reg);
            }
        }

        // Variables are assumed alive for the entire block, a define shortens
        // the interval to the point of definition.
        for (BitSet::Iterator i(live->begin()); i != live->end(); i++) {
            vregs[*i].getInterval(0)->addRange(inputOf(block->firstId()),
                                               outputOf(block->lastId()));
        }

        // Shorten the front end of live intervals for live variables to their
        // point of definition, if found.
        for (LInstructionReverseIterator ins = block->rbegin(); ins != block->rend(); ins++) {
            for (size_t i = 0; i < ins->numDefs(); i++) {
                if (ins->getDef(i)->policy() != LDefinition::REDEFINED) {
                    LDefinition *def = ins->getDef(i);

                    // The output register may be clobbered before inputs are
                    // read unless the policy says otherwise, so we have to
                    // make the intervals overlap.
                    CodePosition from(inputOf(*ins));
                    if (def->policy() == LDefinition::MUST_REUSE_INPUT)
                        from = outputOf(*ins);

                    // Ensure that if there aren't any uses, there's at least
                    // some interval for the output to go into.
                    if (vregs[def].getInterval(0)->numRanges() == 0)
                        vregs[def].getInterval(0)->addRange(from, outputOf(*ins));
                    vregs[def].getInterval(0)->setFrom(from);
                    live->remove(def->virtualRegister());
                }
            }

            for (size_t i = 0; i < ins->numTemps(); i++)
                vregs[ins->getTemp(i)].getInterval(0)->addRange(inputOf(*ins), outputOf(*ins));

            for (LInstruction::InputIterator alloc(**ins); alloc.more(); alloc.next())
            {
                if (alloc->isUse()) {
                    LUse *use = alloc->toUse();
                    vregs[use].addUse(LOperand(use, *ins, alloc.isSnapshotInput()));

                    if (ins->id() == block->firstId()) {
                        vregs[use].getInterval(0)->addRange(inputOf(*ins), outputOf(*ins));
                    } else {
                        vregs[use].getInterval(0)->addRange(inputOf(block->firstId()),
                                                            outputOf(*ins));
                    }
                    live->insert(use->virtualRegister());
                }
            }
        }

        // Phis have simultaneous assignment semantics at block begin, so at
        // the beginning of the block we can be sure that liveIn does not
        // contain any phi outputs.
        for (unsigned int i = 0; i < block->numPhis();) {
            if (live->contains(block->getPhi(i)->getDef(0)->virtualRegister())) {
                live->remove(block->getPhi(i)->getDef(0)->virtualRegister());
                i++;
            } else {
                // This is a dead phi, so we can be shamelessly opportunistic
                // and remove it here.
                block->removePhi(i);
            }
        }

        if (mblock->isLoopHeader()) {
            // A divergence from the published algorithm is required here, as
            // our block order does not guarantee that blocks of a loop are
            // contiguous. As a result, a single live interval spanning the
            // loop is not possible. Additionally, we require liveIn in a later
            // pass for resolution, so that must also be fixed up here.
            MBasicBlock *loopBlock = mblock->backedge();
            while (true) {
                // Add an interval for this entire loop block
                for (BitSet::Iterator i(live->begin()); i != live->end(); i++) {
                    vregs[*i].getInterval(0)->addRange(inputOf(loopBlock->lir()->firstId()),
                                                       outputOf(loopBlock->lir()->lastId()));
                }

                // Fix up the liveIn set to account for the new interval
                liveIn[loopBlock->id()]->insertAll(live);

                // Make sure we don't visit this node again
                loopDone->insert(loopBlock->id());

                // If this is the loop header, any predecessors are either the
                // backedge or out of the loop, so skip any predecessors of
                // this block
                if (loopBlock != mblock) {
                    for (size_t i = 0; i < loopBlock->numPredecessors(); i++) {
                        MBasicBlock *pred = loopBlock->getPredecessor(i);
                        if (loopDone->contains(pred->id()))
                            continue;
                        if (!loopWorkList.append(pred))
                            return false;
                    }
                }

                // Grab the next block off the work list
                if (loopWorkList.empty())
                    break;
                loopBlock = loopWorkList.popCopy();
            }

            // Clear the done set for other loops
            loopDone->clear();
        }

        JS_ASSERT_IF(!mblock->numPredecessors(), live->empty());
    }

    return true;
}

/*
 * This function performs a preliminary allocation on the already-computed
 * live intervals, storing the result in the allocation field of the intervals
 * themselves.
 *
 * The algorithm is based on the one published in:
 *
 * Wimmer, Christian, and Hanspeter Mössenböck. "Optimized Interval Splitting
 *     in a Linear Scan Register Allocator." Proceedings of the First
 *     ACM/USENIX Conference on Virtual Execution Environments. Chicago, IL,
 *     USA, ACM. 2005. PDF.
 *
 * The algorithm proceeds over each interval in order of their start position.
 * If a free register is available, the register that is free for the largest
 * portion of the current interval is allocated. Otherwise, the interval
 * with the farthest-away next use is spilled to make room for this one. In all
 * cases, intervals which conflict either with other intervals or with
 * use or definition constraints are split at the point of conflict to be
 * assigned a compatible allocation later.
 */
bool
LinearScanAllocator::allocateRegisters()
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
            JS_ASSERT(it->numRanges() > 0);

            if (it->end() < position) {
                i = active.removeAt(i);
                finishInterval(it);
            } else if (!it->covers(position)) {
                i = active.removeAt(i);
                inactive.pushBack(it);
            } else {
                i++;
            }
        }

        // Shift inactive intervals to the active or handled sets as appropriate
        for (IntervalIterator i(inactive.begin()); i != inactive.end(); ) {
            LiveInterval *it = *i;
            JS_ASSERT(it->numRanges() > 0);

            if (it->end() < position) {
                i = inactive.removeAt(i);
                finishInterval(it);
            } else if (it->covers(position)) {
                i = inactive.removeAt(i);
                active.pushBack(it);
            } else {
                i++;
            }
        }

        // Sanity check all intervals in all sets
        validateIntervals();

        // Check the allocation policy if this is a definition or a use
        bool mustHaveRegister = false;
        bool canSpillOthers = true;
        if (position == current->reg()->getFirstInterval()->start()) {
            LDefinition *def = current->reg()->def();
            LDefinition::Policy policy = def->policy();
            if (policy == LDefinition::PRESET) {
                IonSpew(IonSpew_LSRA, " Definition has preset policy.");
                current->setFlag(LiveInterval::FIXED);
                if (!assign(*def->output()))
                    return false;
                continue;
            }
            if (policy == LDefinition::MUST_REUSE_INPUT) {
                IonSpew(IonSpew_LSRA, " Definition has 'must reuse input' policy.");
                LInstruction *ins = current->reg()->ins();
                JS_ASSERT(ins->numOperands() > 0);
                JS_ASSERT(ins->getOperand(0)->isUse());
                LiveInterval *inputInterval = vregs[ins->getOperand(0)].intervalFor(inputOf(ins));
                JS_ASSERT(inputInterval);
                JS_ASSERT(inputInterval->getAllocation()->isGeneralReg());
                if (!assign(*inputInterval->getAllocation()))
                    return false;
                continue;
            }
            if (policy == LDefinition::DEFAULT) {
                if (current->reg()->ins()->isPhi()) {
                    mustHaveRegister = false;
                    canSpillOthers = false;
                } else {
                    mustHaveRegister = true;
                }
            } else {
                JS_ASSERT(policy == LDefinition::REDEFINED);
            }
        } else if (position.subpos() == CodePosition::INPUT) {
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

        // Find the first use of this register
        firstUse = current->reg()->nextUseAfter(position);
        if (firstUse)
            firstUsePos = inputOf(firstUse->ins);
        else
            firstUsePos = CodePosition::MAX;

        // If we really don't need this in a register, don't allocate one
        if (!mustHaveRegister && !firstUse && current->reg()->canonicalSpill()) {
            if (!spill())
                return false;
            continue;
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

        IonSpew(IonSpew_LSRA, "  Unable to allocate free register");

        // We may need to allocate a blocked register
        if (!canSpillOthers) {
            IonSpew(IonSpew_LSRA, " Can't spill any other intervals, spilling this one");
            if (!spill())
                return false;
            continue;
        }

        IonSpew(IonSpew_LSRA, " Attempting blocked register allocation");

        // Find the best register
        best = findBestBlockedRegister();

        // Figure out whether to spill the blocker or ourselves
        IonSpew(IonSpew_LSRA, "  Current interval next used at %u", firstUsePos.pos());

        // Deal with constraints
        if (mustHaveRegister) {
            IonSpew(IonSpew_LSRA, "   But this interval needs a register.");
            firstUsePos = position;
        }

        // We only spill the blocking interval if it is a strict improvement
        // to do so, otherwise we will continuously re-spill intervals.
        if (firstUsePos >= nextUsePos[best.code()]) {
            if (!spill())
                return false;
        } else {
            if (!assign(LGeneralReg(best)))
                return false;
        }
    }

    validateAllocations();

    return true;
}

/*
 * This function iterates over control flow edges in the function and resolves
 * conflicts wherein two predecessors of a block have different allocations
 * for a virtual register than the block itself. In such cases, moves are
 * inserted at the end of the predecessor blocks.
 *
 * The algorithm is based on the one published in "Linear Scan Register
 * Allocation on SSA Form" by C. Wimmer et al., for which the full citation
 * appears above.
 */
bool
LinearScanAllocator::resolveControlFlow()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *successor = graph.getBlock(i);
        MBasicBlock *mSuccessor = successor->mir();
        if (mSuccessor->numPredecessors() < 1)
            continue;

        IonSpew(IonSpew_LSRA, " Resolving control flow into block %d", successor->mir()->id());

        // We want to recover the state of liveIn prior to the removal of phi-
        // defined instructions. So, we (destructively) add all phis back in.
        BitSet *live = liveIn[successor->mir()->id()];
        for (size_t j = 0; j < successor->numPhis(); j++)
            live->insert(successor->getPhi(j)->getDef(0)->virtualRegister());

        for (BitSet::Iterator liveRegId(live->begin()); liveRegId != live->end(); liveRegId++) {
            IonSpew(IonSpew_LSRA, "  Inspecting live register %d", *liveRegId);
            VirtualRegister *reg = &vregs[*liveRegId];

            // Locate the interval in the successor block
            LPhi *phi = NULL;
            LiveInterval *to;
            if (reg->ins()->isPhi() &&
                reg->ins()->id() >= successor->firstId() &&
                reg->ins()->id() <= successor->lastId())
            {
                IonSpew(IonSpew_LSRA, "   Defined by phi");
                phi = reg->ins()->toPhi();
                to = reg->intervalFor(outputOf(successor->firstId()));
            } else {
                IonSpew(IonSpew_LSRA, "   Present at input");
                to = reg->intervalFor(inputOf(successor->firstId()));
            }
            JS_ASSERT(to);

            // Locate and resolve the interval in each predecessor block
            for (size_t j = 0; j < mSuccessor->numPredecessors(); j++) {
                MBasicBlock *mPredecessor = mSuccessor->getPredecessor(j);
                LBlock *predecessor = mPredecessor->lir();
                CodePosition predecessorEnd = outputOf(predecessor->lastId());
                LiveInterval *from = NULL;

                // If this does assertion does not hold, then the edge
                // currently under consideration is "critical", and can
                // not be resolved directly without the insertion of an
                // additional piece of control flow. These critical edges
                // should have been split in an earlier pass so that this
                // pass does not have to deal with them.
                JS_ASSERT_IF(phi, mPredecessor->numSuccessors() == 1);

                // Find the interval at the "from" half of the edge
                if (phi) {
                    JS_ASSERT(mPredecessor->successorWithPhis() == successor->mir());

                    LAllocation *phiInput = phi->getOperand(mPredecessor->
                                                            positionInPhiSuccessor());
                    JS_ASSERT(phiInput->isUse());

                    IonSpew(IonSpew_LSRA, "   Known as register %u at phi input",
                            phiInput->toUse()->virtualRegister());

                    from = vregs[phiInput].intervalFor(predecessorEnd);
                } else {
                    from = reg->intervalFor(predecessorEnd);
                }

                // Resolve the edge with a move if necessary
                JS_ASSERT(from);
                if (*from->getAllocation() != *to->getAllocation()) {
                    IonSpew(IonSpew_LSRA, "    Inserting move");
                    if (!moveBefore(predecessorEnd, from, to))
                        return false;
                }
            }
        }
    }

    return true;
}

/*
 * This function takes the allocations assigned to the live intervals and
 * erases all virtual registers in the function with the allocations
 * corresponding to the appropriate interval. It also removes the now
 * useless phi nodes, as they have been resolved with moves in the previous
 * pass.
 */
bool
LinearScanAllocator::reifyAllocations()
{
    // Enqueue all handled intervals for reification
    unhandled.enqueue(inactive);
    unhandled.enqueue(active);
    unhandled.enqueue(handled);

    // Handle each interval in sequence
    LiveInterval *interval;
    while ((interval = unhandled.dequeue()) != NULL) {
        VirtualRegister *reg = interval->reg();

        IonSpew(IonSpew_LSRA, " Reifying interval %u = [%u,%u]", reg->reg(),
                interval->start().pos(), interval->end().pos());

        // Erase all uses of this interval
        for (size_t i = 0; i < reg->numUses(); i++) {
            LOperand *use = reg->getUse(i);
            LAllocation *alloc = use->use;
            CodePosition pos = inputOf(use->ins);
            if (use->snapshot)
                pos = outputOf(use->ins);
            if (interval->covers(pos))
                *alloc = *interval->getAllocation();
        }

        if (interval->index() == 0)
        {
            // Erase the def of this interval if it's the first one
            reg->def()->setOutput(*interval->getAllocation());
        }
        else if (!interval->reg()->canonicalSpill() ||
                 interval->reg()->canonicalSpill() == interval->getAllocation() ||
                 *interval->reg()->canonicalSpill() != *interval->getAllocation())
        {
            // If this virtual register has no canonical spill location, this
            // is the first spill to that location, or this is a move to somewhere
            // completely different, we have to emit a move for this interval.
            LiveInterval *from = reg->getInterval(interval->index() - 1);
            if (!moveBefore(interval->start(), from, interval))
                return false;
        }
    }

    // Strip phis out
    for (size_t i = 0; i < graph.numBlocks(); i++)
        graph.getBlock(i)->clearPhis();

    // Set the graph overall stack height
    graph.setLocalSlotCount(stackAssignment.stackHeight());

    return true;
}

/*
 * Split the given interval at the given position, and add the created
 * interval to the unhandled queue.
 */
bool
LinearScanAllocator::splitInterval(LiveInterval *interval, CodePosition pos)
{
    // Make sure we're actually splitting this interval, not some other
    // interval in the same virtual register.
    JS_ASSERT(interval->start() <= pos && pos <= interval->end());

    VirtualRegister *reg = interval->reg();

    // Do the split
    LiveInterval *newInterval = new LiveInterval(reg, interval->index() + 1);
    if (!interval->splitFrom(pos, newInterval))
        return false;

    JS_ASSERT(interval->numRanges() > 0);
    JS_ASSERT(newInterval->numRanges() > 0);

    if (!reg->addInterval(newInterval))
        return false;

    // We'll need a move group later, insert it now.
    if (!getMoveGroupBefore(pos))
        return false;

    IonSpew(IonSpew_LSRA, "  Split interval to %u = [%u, %u]",
            interval->reg()->reg(), interval->start().pos(),
            interval->end().pos());
    IonSpew(IonSpew_LSRA, "  Created new interval %u = [%u, %u]",
            newInterval->reg()->reg(), newInterval->start().pos(),
            newInterval->end().pos());

    // We always want to enqueue the resulting split. We always split
    // forward, and we never want to handle something forward of our
    // current position.
    unhandled.enqueue(newInterval);

    return true;
}

/*
 * Assign the current interval the given allocation, splitting conflicting
 * intervals as necessary to make the allocation stick.
 */
bool
LinearScanAllocator::assign(LAllocation allocation)
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
                    finishInterval(it);
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
    } else if (allocation.isMemory()) {
        if (current->reg()->canonicalSpill())
            JS_ASSERT(allocation == *current->reg()->canonicalSpill());
        else
            current->reg()->setCanonicalSpill(current->getAllocation());
    }

    active.pushBack(current);

    return true;
}

bool
LinearScanAllocator::spill()
{
    IonSpew(IonSpew_LSRA, "  Decided to spill current interval");

    if (current->reg()->canonicalSpill()) {
        IonSpew(IonSpew_LSRA, "  Allocating canonical spill location");

        return assign(*current->reg()->canonicalSpill());
    }

    uint32 stackSlot;
    if (!stackAssignment.allocateSlot(&stackSlot))
        return false;

    IonSpew(IonSpew_LSRA, "  Allocated spill slot %u", stackSlot);

    return assign(LStackSlot(stackSlot));
}

void
LinearScanAllocator::finishInterval(LiveInterval *interval)
{
    LAllocation *alloc = interval->getAllocation();
    JS_ASSERT(!alloc->isUse());

    bool lastInterval = interval->index() == (interval->reg()->numIntervals() - 1);
    if (alloc->isStackSlot() && (alloc != interval->reg()->canonicalSpill() || lastInterval)) {
        if (alloc->toStackSlot()->isDouble())
            stackAssignment.freeDoubleSlot(alloc->toStackSlot()->slot());
        else
            stackAssignment.freeSlot(alloc->toStackSlot()->slot());
    }

    handled.pushBack(interval);
}

/*
 * This function locates a register which may be assigned by the register
 * and is not assigned to any active interval. The register which is free
 * for the longest period of time is then returned. As a side effect,
 * the freeUntilPos array is updated for use elsewhere in the algorithm.
 */
Register
LinearScanAllocator::findBestFreeRegister()
{
    // Update freeUntilPos for search
    IonSpew(IonSpew_LSRA, "  Computing freeUntilPos");
    for (size_t i = 0; i < Registers::Total; i++) {
        if (allowedRegs.has(Register::FromCode(i)))
            freeUntilPos[i] = CodePosition::MAX;
        else
            freeUntilPos[i] = CodePosition::MIN;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        if (i->getAllocation()->isGeneralReg()) {
            Register reg = i->getAllocation()->toGeneralReg()->reg();
            freeUntilPos[reg.code()] = CodePosition::MIN;
            IonSpew(IonSpew_LSRA, "   Register %u not free", reg.code());
        }
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        if (i->getAllocation()->isGeneralReg()) {
            Register reg = i->getAllocation()->toGeneralReg()->reg();
            CodePosition pos = current->intersect(*i);
            if (pos != CodePosition::MIN && pos < freeUntilPos[reg.code()]) {
                freeUntilPos[reg.code()] = pos;
                IonSpew(IonSpew_LSRA, "   Register %u free until %u", reg.code(), pos.pos());
            }
        }
    }

    // Search freeUntilPos for largest value
    Register best = Register::FromCode(0);
    for (uint32 i = 0; i < Registers::Total; i++) {
        Register reg = Register::FromCode(i);
        if (freeUntilPos[i] > freeUntilPos[best.code()])
            best = reg;
    }

    // As an optimization, use the allocation from the previous interval if it
    // is available.
    if (current->index()) {
        LiveInterval *previous = current->reg()->getInterval(current->index() - 1);
        if (previous->getAllocation()->isGeneralReg()) {
            Register prevReg = previous->getAllocation()->toGeneralReg()->reg();
            if (freeUntilPos[prevReg.code()] != CodePosition::MIN)
                best = prevReg;
        }
    }

    // Alternately, use the upcoming fixed register if it is available.
    if (firstUse && firstUse->use->policy() == LUse::FIXED) {
        uint32 fixedReg = firstUse->use->registerCode();
        if (freeUntilPos[fixedReg] != CodePosition::MIN)
            best = Register::FromCode(fixedReg);
    }

    return best;
}

/*
 * This function locates a register which is assigned to an active interval,
 * and returns the one with the furthest away next use. As a side effect,
 * the nextUsePos array is updated with the next use position of all active
 * intervals for use elsewhere in the algorithm.
 */
Register
LinearScanAllocator::findBestBlockedRegister()
{
    // Update nextUsePos for search
    IonSpew(IonSpew_LSRA, "  Computing nextUsePos");
    for (size_t i = 0; i < Registers::Total; i++) {
        if (allowedRegs.has(Register::FromCode(i)))
            nextUsePos[i] = CodePosition::MAX;
        else
            nextUsePos[i] = CodePosition::MIN;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        if (i->getAllocation()->isGeneralReg()) {
            Register reg = i->getAllocation()->toGeneralReg()->reg();
            CodePosition nextUse = i->reg()->nextUsePosAfter(current->start());
            IonSpew(IonSpew_LSRA, "   Register %u next used %u", reg.code(), nextUse.pos());

            if (i->start() == current->start()) {
                IonSpew(IonSpew_LSRA, "    Disqualifying due to recency.");
                nextUsePos[reg.code()] = current->start();
            } else {
                nextUsePos[reg.code()] = nextUse;
            }
        }
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        if (i->getAllocation()->isGeneralReg()) {
            Register reg = i->getAllocation()->toGeneralReg()->reg();
            CodePosition pos = i->reg()->nextUsePosAfter(current->start());
            JS_ASSERT(i->covers(pos) || i->end() == pos);
            if (pos < nextUsePos[reg.code()]) {
                nextUsePos[reg.code()] = pos;
                IonSpew(IonSpew_LSRA, "   Register %u next used %u", reg.code(), pos.pos());
            }
        }
    }

    // Search nextUsePos for largest value
    Register best = Register::FromCode(0);
    for (size_t i = 0; i < Registers::Total; i++) {
        if (nextUsePos[i] > nextUsePos[best.code()])
            best = Register::FromCode(i);
    }
    IonSpew(IonSpew_LSRA, "  Decided best register was %u", best.code());
    return best;
}

/*
 * Two intervals can coexist if any of the following conditions is met:
 *
 *   - The intervals do not intersect.
 *   - The intervals have different allocations.
 *   - The intervals have the same allocation, but the allocation may be
 *     shared.
 *
 * Intuitively, it is a bug if any allocated intervals exist which can not
 * coexist.
 */
bool
LinearScanAllocator::canCoexist(LiveInterval *a, LiveInterval *b)
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

LMoveGroup *
LinearScanAllocator::getMoveGroupBefore(CodePosition pos)
{
    VirtualRegister *vreg = &vregs[pos];
    JS_ASSERT(vreg->ins());
    JS_ASSERT(!vreg->ins()->isPhi());

    LMoveGroup *moves;
    switch (pos.subpos()) {
      case CodePosition::INPUT:
        if (vreg->inputMoves())
            return vreg->inputMoves();

        moves = new LMoveGroup;
        vreg->setInputMoves(moves);
        if (vreg->outputMoves())
            vreg->block()->insertBefore(vreg->outputMoves(), moves);
        else
            vreg->block()->insertBefore(vreg->ins(), moves);
        return moves;

      case CodePosition::OUTPUT:
        if (vreg->outputMoves())
            return vreg->outputMoves();

        moves = new LMoveGroup;
        vreg->setOutputMoves(moves);
        vreg->block()->insertBefore(vreg->ins(), moves);
        return moves;

      default:
        JS_NOT_REACHED("Unknown subposition");
        return NULL;
    }
}

bool
LinearScanAllocator::moveBefore(CodePosition pos, LiveInterval *from, LiveInterval *to)
{
    LMoveGroup *moves = getMoveGroupBefore(pos);
    if (*from->getAllocation() == *to->getAllocation())
        return true;
    return moves->add(from->getAllocation(), to->getAllocation());
}

#ifdef DEBUG
/*
 * Ensure intervals appear in exactly the appropriate one of {active,inactive,
 * handled}, and that active and inactive intervals do not conflict. Handled
 * intervals are checked for conflicts in validateAllocations for performance
 * reasons.
 */
void
LinearScanAllocator::validateIntervals()
{
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        JS_ASSERT(i->numRanges() > 0);
        JS_ASSERT(i->covers(current->start()));

        for (IntervalIterator j(active.begin()); j != i; j++)
            JS_ASSERT(canCoexist(*i, *j));
    }

    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        JS_ASSERT(i->numRanges() > 0);
        JS_ASSERT(i->end() >= current->start());
        JS_ASSERT(!i->covers(current->start()));

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
            JS_ASSERT(i->end() < current->start());
            JS_ASSERT(!i->covers(current->start()));
        }

        for (IntervalIterator j(active.begin()); j != active.end(); j++)
            JS_ASSERT(*i != *j);
        for (IntervalIterator j(inactive.begin()); j != inactive.end(); j++)
            JS_ASSERT(*i != *j);
    }
}

/*
 * This function performs a nice, expensive check that all intervals
 * in the function can coext with every other interval.
 */
void
LinearScanAllocator::validateAllocations()
{
    for (IntervalIterator i(handled.begin()); i != handled.end(); i++) {
        for (IntervalIterator j(handled.begin()); j != i; j++) {
            JS_ASSERT(*i != *j);
            JS_ASSERT(canCoexist(*i, *j));
        }
        bool found = false;
        for (size_t j = 0; j < i->reg()->numIntervals(); j++) {
            if (i->reg()->getInterval(j) == *i) {
                JS_ASSERT(j == i->index());
                found = true;
                break;
            }
        }
        JS_ASSERT(found);
    }
}
#endif

bool
LinearScanAllocator::go()
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
LinearScanAllocator::UnhandledQueue::enqueue(LiveInterval *interval)
{
    IntervalIterator i(begin());
    for (; i != end(); i++) {
        if (i->start() < interval->start())
            break;
    }
    insertBefore(*i, interval);
}

LiveInterval *
LinearScanAllocator::UnhandledQueue::dequeue()
{
    if (rbegin() == rend())
        return NULL;

    LiveInterval *result = *rbegin();
    remove(result);
    return result;
}
