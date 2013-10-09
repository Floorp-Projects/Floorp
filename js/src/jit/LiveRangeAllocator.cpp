/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/LiveRangeAllocator.h"
#include "mozilla/DebugOnly.h"
#include "jit/BacktrackingAllocator.h"
#include "jit/BitSet.h"
#include "jit/LinearScan.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

int
Requirement::priority() const
{
    switch (kind_) {
      case Requirement::FIXED:
        return 0;

      case Requirement::REGISTER:
        return 1;

      case Requirement::NONE:
        return 2;

      default:
        MOZ_ASSUME_UNREACHABLE("Unknown requirement kind.");
    }
}

bool
LiveInterval::Range::contains(const Range *other) const
{
    Range pre, inside, post;
    intersect(other, &pre, &inside, &post);
    return inside.from == other->from && inside.to == other->to;
}

void
LiveInterval::Range::intersect(const Range *other, Range *pre, Range *inside, Range *post) const
{
    JS_ASSERT(pre->empty() && inside->empty() && post->empty());

    CodePosition innerFrom = from;
    if (from < other->from) {
        if (to < other->from) {
            *pre = Range(from, to);
            return;
        }
        *pre = Range(from, other->from);
        innerFrom = other->from;
    }

    CodePosition innerTo = to;
    if (to > other->to) {
        if (from >= other->to) {
            *post = Range(from, to);
            return;
        }
        *post = Range(other->to, to);
        innerTo = other->to;
    }

    if (innerFrom != innerTo)
        *inside = Range(innerFrom, innerTo);
}

bool
LiveInterval::addRangeAtHead(CodePosition from, CodePosition to)
{
    JS_ASSERT(from < to);

    Range newRange(from, to);

    if (ranges_.empty())
        return ranges_.append(newRange);

    Range &first = ranges_.back();
    if (to < first.from)
        return ranges_.append(newRange);

    if (to == first.from) {
        first.from = from;
        return true;
    }

    JS_ASSERT(from < first.to);
    JS_ASSERT(to > first.from);
    if (from < first.from)
        first.from = from;
    if (to > first.to)
        first.to = to;

    return true;
}

bool
LiveInterval::addRange(CodePosition from, CodePosition to)
{
    JS_ASSERT(from < to);

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
            if (from == ranges_.back().to)
                ranges_.erase(&ranges_.back());
            else
                ranges_.back().from = from;
            break;
        }
    }
}

bool
LiveInterval::covers(CodePosition pos)
{
    if (pos < start() || pos >= end())
        return false;

    // Loop over the ranges in ascending order.
    size_t i = lastProcessedRangeIfValid(pos);
    for (; i < ranges_.length(); i--) {
        if (pos < ranges_[i].from)
            return false;
        setLastProcessedRange(i, pos);
        if (pos < ranges_[i].to)
            return true;
    }
    return false;
}

CodePosition
LiveInterval::nextCoveredAfter(CodePosition pos)
{
    for (size_t i = 0; i < ranges_.length(); i++) {
        if (ranges_[i].to <= pos) {
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
    if (start() > other->start())
        return other->intersect(this);

    // Loop over the ranges in ascending order. As an optimization,
    // try to start at the last processed range.
    size_t i = lastProcessedRangeIfValid(other->start());
    size_t j = other->ranges_.length() - 1;
    if (i >= ranges_.length() || j >= other->ranges_.length())
        return CodePosition::MIN;

    while (true) {
        const Range &r1 = ranges_[i];
        const Range &r2 = other->ranges_[j];

        if (r1.from <= r2.from) {
            if (r1.from <= other->start())
                setLastProcessedRange(i, other->start());
            if (r2.from < r1.to)
                return r2.from;
            if (i == 0 || ranges_[i-1].from > other->end())
                break;
            i--;
        } else {
            if (r1.from < r2.to)
                return r1.from;
            if (j == 0 || other->ranges_[j-1].from > end())
                break;
            j--;
        }
    }

    return CodePosition::MIN;
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
    JS_ASSERT(pos >= start() && pos < end());
    JS_ASSERT(after->ranges_.empty());

    // Move all intervals over to the target
    size_t bufferLength = ranges_.length();
    Range *buffer = ranges_.extractRawBuffer();
    if (!buffer)
        return false;
    after->ranges_.replaceRawBuffer(buffer, bufferLength);

    // Move intervals back as required
    for (Range *i = &after->ranges_.back(); i >= after->ranges_.begin(); i--) {
        if (pos >= i->to)
            continue;

        if (pos > i->from) {
            // Split the range
            Range split(i->from, pos);
            i->from = pos;
            if (!ranges_.append(split))
                return false;
        }
        if (!ranges_.append(i + 1, after->ranges_.end()))
            return false;
        after->ranges_.shrinkBy(after->ranges_.end() - i - 1);
        break;
    }

    // Split the linked list of use positions
    UsePosition *prev = nullptr;
    for (UsePositionIterator usePos(usesBegin()); usePos != usesEnd(); usePos++) {
        if (usePos->pos > pos)
            break;
        prev = *usePos;
    }

    uses_.splitAfter(prev, &after->uses_);
    return true;
}

void
LiveInterval::addUse(UsePosition *use)
{
    // Insert use positions in ascending order. Note that instructions
    // are visited in reverse order, so in most cases the loop terminates
    // at the first iteration and the use position will be added to the
    // front of the list.
    UsePosition *prev = nullptr;
    for (UsePositionIterator current(usesBegin()); current != usesEnd(); current++) {
        if (current->pos >= use->pos)
            break;
        prev = *current;
    }

    if (prev)
        uses_.insertAfter(prev, use);
    else
        uses_.pushFront(use);
}

UsePosition *
LiveInterval::nextUseAfter(CodePosition after)
{
    for (UsePositionIterator usePos(usesBegin()); usePos != usesEnd(); usePos++) {
        if (usePos->pos >= after) {
            LUse::Policy policy = usePos->use->policy();
            JS_ASSERT(policy != LUse::RECOVERED_INPUT);
            if (policy != LUse::KEEPALIVE)
                return *usePos;
        }
    }
    return nullptr;
}

/*
 * This function locates the first "real" use of this interval that follows
 * the given code position. Non-"real" uses are currently just snapshots,
 * which keep virtual registers alive but do not result in the
 * generation of code that use them.
 */
CodePosition
LiveInterval::nextUsePosAfter(CodePosition after)
{
    UsePosition *min = nextUseAfter(after);
    return min ? min->pos : CodePosition::MAX;
}

/*
 * This function finds the position of the first use of this interval
 * that is incompatible with the provideded allocation. For example,
 * a use with a REGISTER policy would be incompatible with a stack slot
 * allocation.
 */
CodePosition
LiveInterval::firstIncompatibleUse(LAllocation alloc)
{
    for (UsePositionIterator usePos(usesBegin()); usePos != usesEnd(); usePos++) {
        if (!UseCompatibleWith(usePos->use, alloc))
            return usePos->pos;
    }
    return CodePosition::MAX;
}

LiveInterval *
VirtualRegister::intervalFor(CodePosition pos)
{
    for (LiveInterval **i = intervals_.begin(); i != intervals_.end(); i++) {
        if ((*i)->covers(pos))
            return *i;
        if (pos < (*i)->end())
            break;
    }
    return nullptr;
}

LiveInterval *
VirtualRegister::getFirstInterval()
{
    JS_ASSERT(!intervals_.empty());
    return intervals_[0];
}

// Instantiate LiveRangeAllocator for each template instance.
template bool LiveRangeAllocator<LinearScanVirtualRegister>::buildLivenessInfo();
template bool LiveRangeAllocator<BacktrackingVirtualRegister>::buildLivenessInfo();

#ifdef DEBUG
static inline bool
NextInstructionHasFixedUses(LBlock *block, LInstruction *ins)
{
    LInstructionIterator iter(block->begin(ins));
    iter++;
    for (LInstruction::InputIterator alloc(**iter); alloc.more(); alloc.next()) {
        if (alloc->isUse() && alloc->toUse()->isFixedRegister())
            return true;
    }
    return false;
}

// Returns true iff ins has a def/temp reusing the input allocation.
static bool
IsInputReused(LInstruction *ins, LUse *use)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        if (ins->getDef(i)->policy() == LDefinition::MUST_REUSE_INPUT &&
            ins->getOperand(ins->getDef(i)->getReusedInput())->toUse() == use)
        {
            return true;
        }
    }

    for (size_t i = 0; i < ins->numTemps(); i++) {
        if (ins->getTemp(i)->policy() == LDefinition::MUST_REUSE_INPUT &&
            ins->getOperand(ins->getTemp(i)->getReusedInput())->toUse() == use)
        {
            return true;
        }
    }

    return false;
}
#endif

/*
 * This function pre-allocates and initializes as much global state as possible
 * to avoid littering the algorithms with memory management cruft.
 */
template <typename VREG>
bool
LiveRangeAllocator<VREG>::init()
{
    if (!RegisterAllocator::init())
        return false;

    liveIn = mir->allocate<BitSet*>(graph.numBlockIds());
    if (!liveIn)
        return false;

    // Initialize fixed intervals.
    for (size_t i = 0; i < AnyRegister::Total; i++) {
        AnyRegister reg = AnyRegister::FromCode(i);
        LiveInterval *interval = new LiveInterval(0);
        interval->setAllocation(LAllocation(reg));
        fixedIntervals[i] = interval;
    }

    fixedIntervalsUnion = new LiveInterval(0);

    if (!vregs.init(mir, graph.numVirtualRegisters()))
        return false;

    // Build virtual register objects
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        if (mir->shouldCancel("LSRA create data structures (main loop)"))
            return false;

        LBlock *block = graph.getBlock(i);
        for (LInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            for (size_t j = 0; j < ins->numDefs(); j++) {
                LDefinition *def = ins->getDef(j);
                if (def->policy() != LDefinition::PASSTHROUGH) {
                    if (!vregs[def].init(block, *ins, def, /* isTemp */ false))
                        return false;
                }
            }

            for (size_t j = 0; j < ins->numTemps(); j++) {
                LDefinition *def = ins->getTemp(j);
                if (def->isBogusTemp())
                    continue;
                if (!vregs[def].init(block, *ins, def, /* isTemp */ true))
                    return false;
            }
        }
        for (size_t j = 0; j < block->numPhis(); j++) {
            LPhi *phi = block->getPhi(j);
            LDefinition *def = phi->getDef(0);
            if (!vregs[def].init(block, phi, def, /* isTemp */ false))
                return false;
        }
    }

    return true;
}

static void
AddRegisterToSafepoint(LSafepoint *safepoint, AnyRegister reg, const LDefinition &def)
{
    safepoint->addLiveRegister(reg);

    JS_ASSERT(def.type() == LDefinition::GENERAL ||
              def.type() == LDefinition::DOUBLE ||
              def.type() == LDefinition::OBJECT);

    if (def.type() == LDefinition::OBJECT)
        safepoint->addGcRegister(reg.gpr());
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
template <typename VREG>
bool
LiveRangeAllocator<VREG>::buildLivenessInfo()
{
    if (!init())
        return false;

    Vector<MBasicBlock *, 1, SystemAllocPolicy> loopWorkList;
    BitSet *loopDone = BitSet::New(graph.numBlockIds());
    if (!loopDone)
        return false;

    for (size_t i = graph.numBlocks(); i > 0; i--) {
        if (mir->shouldCancel("LSRA Build Liveness Info (main loop)"))
            return false;

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
                uint32_t reg = use->toUse()->virtualRegister();
                live->insert(reg);
            }
        }

        // Variables are assumed alive for the entire block, a define shortens
        // the interval to the point of definition.
        for (BitSet::Iterator liveRegId(*live); liveRegId; liveRegId++) {
            if (!vregs[*liveRegId].getInterval(0)->addRangeAtHead(inputOf(block->firstId()),
                                                                  outputOf(block->lastId()).next()))
            {
                return false;
            }
        }

        // Shorten the front end of live intervals for live variables to their
        // point of definition, if found.
        for (LInstructionReverseIterator ins = block->rbegin(); ins != block->rend(); ins++) {
            // Calls may clobber registers, so force a spill and reload around the callsite.
            if (ins->isCall()) {
                for (AnyRegisterIterator iter(allRegisters_); iter.more(); iter++) {
                    if (forLSRA) {
                        if (!addFixedRangeAtHead(*iter, inputOf(*ins), outputOf(*ins)))
                            return false;
                    } else {
                        bool found = false;
                        for (size_t i = 0; i < ins->numDefs(); i++) {
                            if (ins->getDef(i)->isPreset() &&
                                *ins->getDef(i)->output() == LAllocation(*iter)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found && !addFixedRangeAtHead(*iter, outputOf(*ins), outputOf(*ins).next()))
                            return false;
                    }
                }
            }

            for (size_t i = 0; i < ins->numDefs(); i++) {
                if (ins->getDef(i)->policy() != LDefinition::PASSTHROUGH) {
                    LDefinition *def = ins->getDef(i);

                    CodePosition from;
                    if (def->policy() == LDefinition::PRESET && def->output()->isRegister() && forLSRA) {
                        // The fixed range covers the current instruction so the
                        // interval for the virtual register starts at the next
                        // instruction. If the next instruction has a fixed use,
                        // this can lead to unnecessary register moves. To avoid
                        // special handling for this, assert the next instruction
                        // has no fixed uses. defineFixed guarantees this by inserting
                        // an LNop.
                        JS_ASSERT(!NextInstructionHasFixedUses(block, *ins));
                        AnyRegister reg = def->output()->toRegister();
                        if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins).next()))
                            return false;
                        from = outputOf(*ins).next();
                    } else {
                        from = forLSRA ? inputOf(*ins) : outputOf(*ins);
                    }

                    if (def->policy() == LDefinition::MUST_REUSE_INPUT) {
                        // MUST_REUSE_INPUT is implemented by allocating an output
                        // register and moving the input to it. Register hints are
                        // used to avoid unnecessary moves. We give the input an
                        // LUse::ANY policy to avoid allocating a register for the
                        // input.
                        LUse *inputUse = ins->getOperand(def->getReusedInput())->toUse();
                        JS_ASSERT(inputUse->policy() == LUse::REGISTER);
                        JS_ASSERT(inputUse->usedAtStart());
                        *inputUse = LUse(inputUse->virtualRegister(), LUse::ANY, /* usedAtStart = */ true);
                    }

                    LiveInterval *interval = vregs[def].getInterval(0);
                    interval->setFrom(from);

                    // Ensure that if there aren't any uses, there's at least
                    // some interval for the output to go into.
                    if (interval->numRanges() == 0) {
                        if (!interval->addRangeAtHead(from, from.next()))
                            return false;
                    }
                    live->remove(def->virtualRegister());
                }
            }

            for (size_t i = 0; i < ins->numTemps(); i++) {
                LDefinition *temp = ins->getTemp(i);
                if (temp->isBogusTemp())
                    continue;

                if (forLSRA) {
                    if (temp->policy() == LDefinition::PRESET) {
                        if (ins->isCall())
                            continue;
                        AnyRegister reg = temp->output()->toRegister();
                        if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins)))
                            return false;

                        // Fixed intervals are not added to safepoints, so do it
                        // here.
                        if (LSafepoint *safepoint = ins->safepoint())
                            AddRegisterToSafepoint(safepoint, reg, *temp);
                    } else {
                        JS_ASSERT(!ins->isCall());
                        if (!vregs[temp].getInterval(0)->addRangeAtHead(inputOf(*ins), outputOf(*ins)))
                            return false;
                    }
                } else {
                    CodePosition to =
                        ins->isCall() ? outputOf(*ins) : outputOf(*ins).next();
                    if (!vregs[temp].getInterval(0)->addRangeAtHead(inputOf(*ins), to))
                        return false;
                }
            }

            DebugOnly<bool> hasUseRegister = false;
            DebugOnly<bool> hasUseRegisterAtStart = false;

            for (LInstruction::InputIterator alloc(**ins); alloc.more(); alloc.next()) {
                if (alloc->isUse()) {
                    LUse *use = alloc->toUse();

                    // The first instruction, LLabel, has no uses.
                    JS_ASSERT(inputOf(*ins) > outputOf(block->firstId()));

                    // Call uses should always be at-start or fixed, since the fixed intervals
                    // use all registers.
                    JS_ASSERT_IF(ins->isCall() && !alloc.isSnapshotInput(),
                                 use->isFixedRegister() || use->usedAtStart());

#ifdef DEBUG
                    // Don't allow at-start call uses if there are temps of the same kind,
                    // so that we don't assign the same register.
                    if (ins->isCall() && use->usedAtStart()) {
                        for (size_t i = 0; i < ins->numTemps(); i++)
                            JS_ASSERT(vregs[ins->getTemp(i)].isDouble() != vregs[use].isDouble());
                    }

                    // If there are both useRegisterAtStart(x) and useRegister(y)
                    // uses, we may assign the same register to both operands due to
                    // interval splitting (bug 772830). Don't allow this for now.
                    if (use->policy() == LUse::REGISTER) {
                        if (use->usedAtStart()) {
                            if (!IsInputReused(*ins, use))
                                hasUseRegisterAtStart = true;
                        } else {
                            hasUseRegister = true;
                        }
                    }

                    JS_ASSERT(!(hasUseRegister && hasUseRegisterAtStart));
#endif

                    // Don't treat RECOVERED_INPUT uses as keeping the vreg alive.
                    if (use->policy() == LUse::RECOVERED_INPUT)
                        continue;

                    CodePosition to;
                    if (forLSRA) {
                        if (use->isFixedRegister()) {
                            JS_ASSERT(!use->usedAtStart());
                            AnyRegister reg = GetFixedRegister(vregs[use].def(), use);
                            if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins)))
                                return false;
                            to = inputOf(*ins);

                            // Fixed intervals are not added to safepoints, so do it
                            // here.
                            LSafepoint *safepoint = ins->safepoint();
                            if (!ins->isCall() && safepoint)
                                AddRegisterToSafepoint(safepoint, reg, *vregs[use].def());
                        } else {
                            to = use->usedAtStart() ? inputOf(*ins) : outputOf(*ins);
                        }
                    } else {
                        to = (use->usedAtStart() || ins->isCall())
                           ? inputOf(*ins) : outputOf(*ins);
                        if (use->isFixedRegister()) {
                            LAllocation reg(AnyRegister::FromCode(use->registerCode()));
                            for (size_t i = 0; i < ins->numDefs(); i++) {
                                LDefinition *def = ins->getDef(i);
                                if (def->policy() == LDefinition::PRESET && *def->output() == reg)
                                    to = inputOf(*ins);
                            }
                        }
                    }

                    LiveInterval *interval = vregs[use].getInterval(0);
                    if (!interval->addRangeAtHead(inputOf(block->firstId()), forLSRA ? to : to.next()))
                        return false;
                    interval->addUse(new UsePosition(use, to));

                    live->insert(use->virtualRegister());
                }
            }
        }

        // Phis have simultaneous assignment semantics at block begin, so at
        // the beginning of the block we can be sure that liveIn does not
        // contain any phi outputs.
        for (unsigned int i = 0; i < block->numPhis(); i++) {
            LDefinition *def = block->getPhi(i)->getDef(0);
            if (live->contains(def->virtualRegister())) {
                live->remove(def->virtualRegister());
            } else {
                // This is a dead phi, so add a dummy range over all phis. This
                // can go away if we have an earlier dead code elimination pass.
                if (!vregs[def].getInterval(0)->addRangeAtHead(inputOf(block->firstId()),
                                                               outputOf(block->firstId())))
                {
                    return false;
                }
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
                // Blocks must already have been visited to have a liveIn set.
                JS_ASSERT(loopBlock->id() >= mblock->id());

                // Add an interval for this entire loop block
                CodePosition from = inputOf(loopBlock->lir()->firstId());
                CodePosition to = outputOf(loopBlock->lir()->lastId()).next();

                for (BitSet::Iterator liveRegId(*live); liveRegId; liveRegId++) {
                    if (!vregs[*liveRegId].getInterval(0)->addRange(from, to))
                        return false;
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

                // Terminate loop if out of work.
                if (loopWorkList.empty())
                    break;

                // Grab the next block off the work list, skipping any OSR block.
                while (!loopWorkList.empty()) {
                    loopBlock = loopWorkList.popCopy();
                    if (loopBlock->lir() != graph.osrBlock())
                        break;
                }

                // If end is reached without finding a non-OSR block, then no more work items were found.
                if (loopBlock->lir() == graph.osrBlock()) {
                    JS_ASSERT(loopWorkList.empty());
                    break;
                }
            }

            // Clear the done set for other loops
            loopDone->clear();
        }

        JS_ASSERT_IF(!mblock->numPredecessors(), live->empty());
    }

    validateVirtualRegisters();

    // If the script has an infinite loop, there may be no MReturn and therefore
    // no fixed intervals. Add a small range to fixedIntervalsUnion so that the
    // rest of the allocator can assume it has at least one range.
    if (fixedIntervalsUnion->numRanges() == 0) {
        if (!fixedIntervalsUnion->addRangeAtHead(CodePosition(0, CodePosition::INPUT),
                                                 CodePosition(0, CodePosition::OUTPUT)))
        {
            return false;
        }
    }

    return true;
}

#ifdef DEBUG

void
LiveInterval::validateRanges()
{
    Range *prev = nullptr;

    for (size_t i = ranges_.length() - 1; i < ranges_.length(); i--) {
        Range *range = &ranges_[i];

        JS_ASSERT(range->from < range->to);
        JS_ASSERT_IF(prev, prev->to <= range->from);
        prev = range;
    }
}

#endif // DEBUG
