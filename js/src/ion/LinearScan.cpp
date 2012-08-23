/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>
#include "BitSet.h"
#include "LinearScan.h"
#include "IonSpewer.h"
#include "LIR-inl.h"

using namespace js;
using namespace js::ion;

static bool
UseCompatibleWith(const LUse *use, LAllocation alloc)
{
    switch (use->policy()) {
      case LUse::ANY:
      case LUse::KEEPALIVE:
        return alloc.isRegister() || alloc.isMemory();
      case LUse::REGISTER:
        return alloc.isRegister();
      case LUse::FIXED:
          // Fixed uses are handled using fixed intervals. The
          // UsePosition is only used as hint.
        return alloc.isRegister();
      default:
        JS_NOT_REACHED("Unknown use policy");
    }
    return false;
}

#ifdef DEBUG
static bool
DefinitionCompatibleWith(LInstruction *ins, const LDefinition *def, LAllocation alloc)
{
    if (ins->isPhi()) {
        if (def->type() == LDefinition::DOUBLE)
            return alloc.isFloatReg() || alloc.kind() == LAllocation::DOUBLE_SLOT;
        return alloc.isGeneralReg() || alloc.kind() == LAllocation::STACK_SLOT;
    }

    switch (def->policy()) {
      case LDefinition::DEFAULT:
        if (!alloc.isRegister())
            return false;
        return alloc.isFloatReg() == (def->type() == LDefinition::DOUBLE);
      case LDefinition::PRESET:
        return alloc == *def->output();
      case LDefinition::MUST_REUSE_INPUT:
        if (!alloc.isRegister() || !ins->numOperands())
            return false;
        return alloc == *ins->getOperand(def->getReusedInput());
      case LDefinition::PASSTHROUGH:
        return true;
      default:
        JS_NOT_REACHED("Unknown definition policy");
    }
    return false;
}
#endif

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
        JS_NOT_REACHED("Unknown requirement kind.");
        return -1;
    }
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
    UsePosition *prev = NULL;
    for (UsePositionIterator usePos(usesBegin()); usePos != usesEnd(); usePos++) {
        if (usePos->pos > pos)
            break;
        prev = *usePos;
    }

    uses_.splitAfter(prev, &after->uses_);
    return true;
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
    return NULL;
}

LiveInterval *
VirtualRegister::getFirstInterval()
{
    JS_ASSERT(!intervals_.empty());
    return intervals_[0];
}

void
LiveInterval::addUse(UsePosition *use)
{
    // Insert use positions in ascending order. Note that instructions
    // are visited in reverse order, so in most cases the loop terminates
    // at the first iteration and the use position will be added to the
    // front of the list.
    UsePosition *prev = NULL;
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
        if (usePos->pos >= after && usePos->use->policy() != LUse::KEEPALIVE)
            return *usePos;
    }
    return NULL;
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

const CodePosition CodePosition::MAX(UINT_MAX);
const CodePosition CodePosition::MIN(0);

/*
 * This function pre-allocates and initializes as much global state as possible
 * to avoid littering the algorithms with memory management cruft.
 */
bool
LinearScanAllocator::createDataStructures()
{
    liveIn = lir->mir()->allocate<BitSet*>(graph.numBlockIds());
    if (!liveIn)
        return false;

    // Initialize fixed intervals.
    for (size_t i = 0; i < AnyRegister::Total; i++) {
        AnyRegister reg = AnyRegister::FromCode(i);
        LiveInterval *interval = new LiveInterval(NULL, 0);
        interval->setAllocation(LAllocation(reg));
        fixedIntervals[i] = interval;
    }

    fixedIntervalsUnion = new LiveInterval(NULL, 0);

    if (!vregs.init(lir->mir(), graph.numVirtualRegisters()))
        return false;

    if (!insData.init(lir->mir(), graph.numInstructions()))
        return false;

    // Build virtual register objects
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *block = graph.getBlock(i);
        for (LInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            for (size_t j = 0; j < ins->numDefs(); j++) {
                LDefinition *def = ins->getDef(j);
                if (def->policy() != LDefinition::PASSTHROUGH) {
                    uint32 reg = def->virtualRegister();
                    if (!vregs[reg].init(reg, block, *ins, def, /* isTemp */ false))
                        return false;
                }
            }
            insData[*ins].init(*ins, block);

            for (size_t j = 0; j < ins->numTemps(); j++) {
                LDefinition *def = ins->getTemp(j);
                if (def->isBogusTemp())
                    continue;
                if (!vregs[def].init(def->virtualRegister(), block, *ins, def, /* isTemp */ true))
                    return false;
            }
        }
        for (size_t j = 0; j < block->numPhis(); j++) {
            LPhi *phi = block->getPhi(j);
            LDefinition *def = phi->getDef(0);
            if (!vregs[def].init(phi->id(), block, phi, def, /* isTemp */ false))
                return false;
            insData[phi].init(phi, block);
        }
    }

    return true;
}

static inline AnyRegister
GetFixedRegister(LDefinition *def, LUse *use)
{
    return def->type() == LDefinition::DOUBLE
           ? AnyRegister(FloatRegister::FromCode(use->registerCode()))
           : AnyRegister(Register::FromCode(use->registerCode()));
}

static inline bool
IsNunbox(VirtualRegister *vreg)
{
#ifdef JS_NUNBOX32
    return (vreg->type() == LDefinition::TYPE ||
            vreg->type() == LDefinition::PAYLOAD);
#else
    return false;
#endif
}

static inline bool
IsTraceable(VirtualRegister *reg)
{
    if (reg->type() == LDefinition::OBJECT)
        return true;
#ifdef JS_PUNBOX64
    if (reg->type() == LDefinition::BOX)
        return true;
#endif
    return false;
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
                    if (!addFixedRangeAtHead(*iter, inputOf(*ins), outputOf(*ins)))
                        return false;
                }
            }

            for (size_t i = 0; i < ins->numDefs(); i++) {
                if (ins->getDef(i)->policy() != LDefinition::PASSTHROUGH) {
                    LDefinition *def = ins->getDef(i);

                    CodePosition from;
                    if (def->policy() == LDefinition::PRESET && def->output()->isRegister()) {
                        // The fixed range covers the current instruction so the interval
                        // for the virtual register starts at the next instruction. The
                        // next instruction is an LNop so this is fine.
                        AnyRegister reg = def->output()->toRegister();
                        if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins).next()))
                            return false;
                        from = outputOf(*ins).next();
                    } else {
                        from = inputOf(*ins);
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
                if (ins->isCall()) {
                    JS_ASSERT(temp->isPreset());
                    continue;
                }

                if (temp->policy() == LDefinition::PRESET) {
                    AnyRegister reg = temp->output()->toRegister();
                    if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins)))
                        return false;
                } else {
                    if (!vregs[temp].getInterval(0)->addRangeAtHead(inputOf(*ins), outputOf(*ins)))
                        return false;
                }
            }

            for (LInstruction::InputIterator alloc(**ins); alloc.more(); alloc.next()) {
                if (alloc->isUse()) {
                    LUse *use = alloc->toUse();

                    // The first instruction, LLabel, has no uses.
                    JS_ASSERT(inputOf(*ins) > outputOf(block->firstId()));

                    CodePosition to;
                    if (use->isFixedRegister()) {
                        JS_ASSERT(!use->usedAtStart());
                        AnyRegister reg = GetFixedRegister(vregs[use].def(), use);
                        if (!addFixedRangeAtHead(reg, inputOf(*ins), outputOf(*ins)))
                            return false;
                        to = inputOf(*ins);
                    } else {
                        // Call instruction operands default to at-start, since the
                        // fixed intervals use all registers.
                        if (use->usedAtStart() || (ins->isCall() && !alloc.isSnapshotInput()))
                            to = inputOf(*ins);
                        else
                            to = outputOf(*ins);
                    }

                    LiveInterval *interval = vregs[use].getInterval(0);
                    if (!interval->addRangeAtHead(inputOf(block->firstId()), to))
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
                do {
                    loopBlock = loopWorkList.popCopy();
                } while (loopBlock->lir() == graph.osrBlock());
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

/*
 * Merge virtual register intervals into the UnhandledQueue, taking advantage
 * of their nearly-sorted ordering.
 */
void
LinearScanAllocator::enqueueVirtualRegisterIntervals()
{
    // Cursor into the unhandled queue, iterating through start positions.
    IntervalReverseIterator curr = unhandled.rbegin();

    // Start position is non-monotonically increasing by virtual register number.
    for (size_t i = 1; i < graph.numVirtualRegisters(); i++) {
        LiveInterval *live = vregs[i].getInterval(0);
        if (live->numRanges() > 0) {
            setIntervalRequirement(live);

            // Iterate backward until the next highest class of start position.
            for (; curr != unhandled.rend(); curr++) {
                if (curr->start() > live->start())
                    break;
            }

            // Insert forward from the current position, thereby
            // sorting by priority within the start class.
            unhandled.enqueueForward(*curr, live);
        }
    }
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
    // The unhandled queue currently contains only spill intervals, in sorted
    // order. Intervals for virtual registers exist in sorted order based on
    // start position by vreg ID, but may have priorities that require them to
    // be reordered when adding to the unhandled queue.
    enqueueVirtualRegisterIntervals();
    unhandled.assertSorted();

    // Add fixed intervals with ranges to fixed.
    for (size_t i = 0; i < AnyRegister::Total; i++) {
        if (fixedIntervals[i]->numRanges() > 0)
            fixed.pushBack(fixedIntervals[i]);
    }

    // Iterate through all intervals in ascending start order.
    CodePosition prevPosition = CodePosition::MIN;
    while ((current = unhandled.dequeue()) != NULL) {
        JS_ASSERT(current->getAllocation()->isUse());
        JS_ASSERT(current->numRanges() > 0);

        CodePosition position = current->start();
        Requirement *req = current->requirement();
        Requirement *hint = current->hint();

        IonSpew(IonSpew_RegAlloc, "Processing %d = [%u, %u] (pri=%d)",
                current->reg() ? current->reg()->reg() : 0, current->start().pos(),
                current->end().pos(), current->requirement()->priority());

        // Shift active intervals to the inactive or handled sets as appropriate
        if (position != prevPosition) {
            JS_ASSERT(position > prevPosition);
            prevPosition = position;

            for (IntervalIterator i(active.begin()); i != active.end(); ) {
                LiveInterval *it = *i;
                JS_ASSERT(it->numRanges() > 0);

                if (it->end() <= position) {
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

                if (it->end() <= position) {
                    i = inactive.removeAt(i);
                    finishInterval(it);
                } else if (it->covers(position)) {
                    i = inactive.removeAt(i);
                    active.pushBack(it);
                } else {
                    i++;
                }
            }
        }

        // Sanity check all intervals in all sets
        validateIntervals();

        // If the interval has a hard requirement, grant it.
        if (req->kind() == Requirement::FIXED) {
            JS_ASSERT(!req->allocation().isRegister());
            if (!assign(req->allocation()))
                return false;
            continue;
        }

        // If we don't really need this in a register, don't allocate one
        if (req->kind() != Requirement::REGISTER && hint->kind() == Requirement::NONE) {
            IonSpew(IonSpew_RegAlloc, "  Eagerly spilling virtual register %d",
                    current->reg() ? current->reg()->reg() : 0);
            if (!spill())
                return false;
            continue;
        }

        // Try to allocate a free register
        IonSpew(IonSpew_RegAlloc, " Attempting free register allocation");
        CodePosition bestFreeUntil;
        AnyRegister::Code bestCode = findBestFreeRegister(&bestFreeUntil);
        if (bestCode != AnyRegister::Invalid) {
            AnyRegister best = AnyRegister::FromCode(bestCode);
            IonSpew(IonSpew_RegAlloc, "  Decided best register was %s", best.name());

            // Split when the register is next needed if necessary
            if (bestFreeUntil < current->end()) {
                if (!splitInterval(current, bestFreeUntil))
                    return false;
            }
            if (!assign(LAllocation(best)))
                return false;

            continue;
        }

        IonSpew(IonSpew_RegAlloc, " Attempting blocked register allocation");

        // If we absolutely need a register or our next use is closer than the
        // selected blocking register then we spill the blocker. Otherwise, we
        // spill the current interval.
        CodePosition bestNextUsed;
        bestCode = findBestBlockedRegister(&bestNextUsed);
        if (bestCode != AnyRegister::Invalid &&
            (req->kind() == Requirement::REGISTER || hint->pos() < bestNextUsed))
        {
            AnyRegister best = AnyRegister::FromCode(bestCode);
            IonSpew(IonSpew_RegAlloc, "  Decided best register was %s", best.name());

            if (!splitBlockingIntervals(LAllocation(best)))
                return false;
            if (!assign(LAllocation(best)))
                return false;

            continue;
        }

        IonSpew(IonSpew_RegAlloc, "  No registers available to spill");
        JS_ASSERT(req->kind() == Requirement::NONE);

        if (!spill())
            return false;
    }

    validateAllocations();
    validateVirtualRegisters();

    return true;
}

/*
 * This function iterates over control flow edges in the function and resolves
 * conflicts wherein two predecessors of a block have different allocations
 * for a virtual register than the block itself. It also turns phis into moves.
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

        // Resolve phis to moves
        for (size_t j = 0; j < successor->numPhis(); j++) {
            LPhi *phi = successor->getPhi(j);
            JS_ASSERT(phi->numDefs() == 1);
            VirtualRegister *vreg = &vregs[phi->getDef(0)];
            LiveInterval *to = vreg->intervalFor(inputOf(successor->firstId()));
            JS_ASSERT(to);

            for (size_t k = 0; k < mSuccessor->numPredecessors(); k++) {
                LBlock *predecessor = mSuccessor->getPredecessor(k)->lir();
                JS_ASSERT(predecessor->mir()->numSuccessors() == 1);

                LAllocation *input = phi->getOperand(predecessor->mir()->positionInPhiSuccessor());
                LiveInterval *from = vregs[input].intervalFor(outputOf(predecessor->lastId()));
                JS_ASSERT(from);

                LMoveGroup *moves = predecessor->getExitMoveGroup();
                if (!addMove(moves, from, to))
                    return false;
            }

            if (vreg->mustSpillAtDefinition() && !to->isSpill()) {
                // Make sure this phi is spilled at the loop header.
                LMoveGroup *moves = successor->getEntryMoveGroup();
                if (!moves->add(to->getAllocation(), to->reg()->canonicalSpill()))
                    return false;
            }
        }

        // Resolve split intervals with moves
        BitSet *live = liveIn[mSuccessor->id()];

        for (BitSet::Iterator liveRegId(*live); liveRegId; liveRegId++) {
            LiveInterval *to = vregs[*liveRegId].intervalFor(inputOf(successor->firstId()));
            JS_ASSERT(to);

            for (size_t j = 0; j < mSuccessor->numPredecessors(); j++) {
                LBlock *predecessor = mSuccessor->getPredecessor(j)->lir();
                LiveInterval *from = vregs[*liveRegId].intervalFor(outputOf(predecessor->lastId()));
                JS_ASSERT(from);

                if (mSuccessor->numPredecessors() > 1) {
                    JS_ASSERT(predecessor->mir()->numSuccessors() == 1);
                    LMoveGroup *moves = predecessor->getExitMoveGroup();
                    if (!addMove(moves, from, to))
                        return false;
                } else {
                    LMoveGroup *moves = successor->getEntryMoveGroup();
                    if (!addMove(moves, from, to))
                        return false;
                }
            }
        }
    }

    return true;
}

bool
LinearScanAllocator::moveInputAlloc(CodePosition pos, LAllocation *from, LAllocation *to)
{
    if (*from == *to)
        return true;
    LMoveGroup *moves = getInputMoveGroup(pos);
    return moves->add(from, to);
}

/*
 * This function takes the allocations assigned to the live intervals and
 * erases all virtual registers in the function with the allocations
 * corresponding to the appropriate interval.
 */
bool
LinearScanAllocator::reifyAllocations()
{
    // Iterate over each interval, ensuring that definitions are visited before uses.
    for (size_t j = 1; j < graph.numVirtualRegisters(); j++) {
        VirtualRegister *reg = &vregs[j];
    for (size_t k = 0; k < reg->numIntervals(); k++) {
        LiveInterval *interval = reg->getInterval(k);
        JS_ASSERT(reg == interval->reg());
        if (!interval->numRanges())
            continue;

        UsePositionIterator usePos(interval->usesBegin());
        for (; usePos != interval->usesEnd(); usePos++) {
            if (usePos->use->isFixedRegister()) {
                LiveInterval *to = fixedIntervals[GetFixedRegister(reg->def(), usePos->use).code()];

                *static_cast<LAllocation *>(usePos->use) = *to->getAllocation();
                if (!moveInput(usePos->pos, interval, to))
                    return false;
            } else {
                JS_ASSERT(UseCompatibleWith(usePos->use, *interval->getAllocation()));
                *static_cast<LAllocation *>(usePos->use) = *interval->getAllocation();
            }
        }

        // Erase the def of this interval if it's the first one
        if (interval->index() == 0)
        {
            LDefinition *def = reg->def();
            LAllocation *spillFrom;

            if (def->policy() == LDefinition::PRESET && def->output()->isRegister()) {
                AnyRegister fixedReg = def->output()->toRegister();
                LiveInterval *from = fixedIntervals[fixedReg.code()];
                if (!moveAfter(outputOf(reg->ins()), from, interval))
                    return false;
                spillFrom = from->getAllocation();
            } else {
                if (def->policy() == LDefinition::MUST_REUSE_INPUT) {
                    LAllocation *alloc = reg->ins()->getOperand(def->getReusedInput());
                    LAllocation *origAlloc = LAllocation::New(*alloc);

                    *alloc = *interval->getAllocation();
                    if (!moveInputAlloc(inputOf(reg->ins()), origAlloc, alloc))
                        return false;
                }

                JS_ASSERT(DefinitionCompatibleWith(reg->ins(), def, *interval->getAllocation()));
                def->setOutput(*interval->getAllocation());

                spillFrom = interval->getAllocation();
            }

            if (reg->mustSpillAtDefinition() && !reg->ins()->isPhi() &&
                (*reg->canonicalSpill() != *spillFrom))
            {
                // Insert a spill at the input of the next instruction. Control
                // instructions never have outputs, so the next instruction is
                // always valid. Note that we explicitly ignore phis, which
                // should have been handled in resolveControlFlow().
                LMoveGroup *moves = getMoveGroupAfter(outputOf(reg->ins()));
                if (!moves->add(spillFrom, reg->canonicalSpill()))
                    return false;
            }
        }
        else if (interval->start() > inputOf(insData[interval->start()].block()->firstId()) &&
                 (!reg->canonicalSpill() ||
                  (reg->canonicalSpill() == interval->getAllocation() &&
                   !reg->mustSpillAtDefinition()) ||
                  *reg->canonicalSpill() != *interval->getAllocation()))
        {
            // If this virtual register has no canonical spill location, this
            // is the first spill to that location, or this is a move to somewhere
            // completely different, we have to emit a move for this interval.
            // Don't do this if the interval starts at the first instruction of the
            // block; this case should have been handled by resolveControlFlow().
            //
            // If the interval starts at the output half of an instruction, we have to
            // emit the move *after* this instruction, to prevent clobbering an input
            // register.
            LiveInterval *prevInterval = reg->getInterval(interval->index() - 1);
            CodePosition start = interval->start();
            InstructionData *data = &insData[start];

            JS_ASSERT(start == inputOf(data->ins()) || start == outputOf(data->ins()));

            if (start.subpos() == CodePosition::INPUT) {
                if (!moveInput(inputOf(data->ins()), prevInterval, interval))
                    return false;
            } else {
                if (!moveAfter(outputOf(data->ins()), prevInterval, interval))
                    return false;
            }

            // Mark this interval's spill position, if needed.
            if (reg->canonicalSpill() == interval->getAllocation() &&
                !reg->mustSpillAtDefinition())
            {
                reg->setSpillPosition(interval->start());
            }
        }

        // Fill in the live register sets for all non-call safepoints.
        LAllocation *a = interval->getAllocation();
        if (a->isRegister()) {
            // Don't add output registers to the safepoint.
            CodePosition start = interval->start();
            if (interval->index() == 0 && !reg->isTemp())
                start = start.next();

            size_t i = findFirstNonCallSafepoint(start);
            for (; i < graph.numNonCallSafepoints(); i++) {
                LInstruction *ins = graph.getNonCallSafepoint(i);
                CodePosition pos = inputOf(ins);

                // Safepoints are sorted, so we can shortcut out of this loop
                // if we go out of range.
                if (interval->end() < pos)
                    break;

                if (!interval->covers(pos))
                    continue;

                LSafepoint *safepoint = ins->safepoint();
                safepoint->addLiveRegister(a->toRegister());
            }
        }
    }} // Iteration over virtual register intervals.

    // Set the graph overall stack height
    graph.setLocalSlotCount(stackSlotAllocator.stackHeight());

    return true;
}

// Finds the first safepoint that is within range of an interval.
size_t
LinearScanAllocator::findFirstSafepoint(LiveInterval *interval, size_t startFrom)
{
    size_t i = startFrom;
    for (; i < graph.numSafepoints(); i++) {
        LInstruction *ins = graph.getSafepoint(i);
        if (interval->start() <= inputOf(ins))
            break;
    }
    return i;
}

size_t
LinearScanAllocator::findFirstNonCallSafepoint(CodePosition from)
{
    size_t i = 0;
    for (; i < graph.numNonCallSafepoints(); i++) {
        LInstruction *ins = graph.getNonCallSafepoint(i);
        if (from <= inputOf(ins))
            break;
    }
    return i;
}

static inline bool
IsSpilledAt(LiveInterval *interval, CodePosition pos)
{
    VirtualRegister *reg = interval->reg();
    if (!reg->canonicalSpill() || !reg->canonicalSpill()->isStackSlot())
        return false;

    if (reg->mustSpillAtDefinition()) {
        JS_ASSERT(reg->spillPosition() <= pos);
        return true;
    }

    return interval->getAllocation() == reg->canonicalSpill();
}

bool
LinearScanAllocator::populateSafepoints()
{
    size_t firstSafepoint = 0;

    for (uint32 i = 0; i < vregs.numVirtualRegisters(); i++) {
        VirtualRegister *reg = &vregs[i];

        if (!reg->def() || (!IsTraceable(reg) && !IsNunbox(reg)))
            continue;

        firstSafepoint = findFirstSafepoint(reg->getInterval(0), firstSafepoint);
        if (firstSafepoint >= graph.numSafepoints())
            break;

        // Find the furthest endpoint.
        size_t lastInterval = reg->numIntervals() - 1;
        CodePosition end = reg->getInterval(lastInterval)->end();

        for (size_t j = firstSafepoint; j < graph.numSafepoints(); j++) {
            LInstruction *ins = graph.getSafepoint(j);

            // Stop processing safepoints if we know we're out of this virtual
            // register's range.
            if (end < inputOf(ins))
                break;

            // Include temps but not instruction outputs. Also make sure MUST_REUSE_INPUT
            // is not used with gcthings or nunboxes, or we would have to add the input reg
            // to this safepoint.
            if (ins == reg->ins() && !reg->isTemp()) {
                DebugOnly<LDefinition*> def = reg->def();
                JS_ASSERT_IF(def->policy() == LDefinition::MUST_REUSE_INPUT,
                             def->type() == LDefinition::GENERAL || def->type() == LDefinition::DOUBLE);
                continue;
            }

            LSafepoint *safepoint = ins->safepoint();

            if (!IsNunbox(reg)) {
                JS_ASSERT(IsTraceable(reg));

                LiveInterval *interval = reg->intervalFor(inputOf(ins));
                if (!interval)
                    continue;

                LAllocation *a = interval->getAllocation();
                if (a->isGeneralReg() && !ins->isCall()) {
#ifdef JS_PUNBOX64
                    if (reg->type() == LDefinition::BOX) {
                        safepoint->addValueRegister(a->toGeneralReg()->reg());
                    } else
#endif
                    {
                        safepoint->addGcRegister(a->toGeneralReg()->reg());
                    }
                }

                if (IsSpilledAt(interval, inputOf(ins))) {
#ifdef JS_PUNBOX64
                    if (reg->type() == LDefinition::BOX) {
                        if (!safepoint->addValueSlot(reg->canonicalSpillSlot()))
                            return false;
                    } else
#endif
                    {
                        if (!safepoint->addGcSlot(reg->canonicalSpillSlot()))
                            return false;
                    }
                }
#ifdef JS_NUNBOX32
            } else {
                VirtualRegister *other = otherHalfOfNunbox(reg);
                VirtualRegister *type = (reg->type() == LDefinition::TYPE) ? reg : other;
                VirtualRegister *payload = (reg->type() == LDefinition::PAYLOAD) ? reg : other;
                LiveInterval *typeInterval = type->intervalFor(inputOf(ins));
                LiveInterval *payloadInterval = payload->intervalFor(inputOf(ins));

                if (!typeInterval && !payloadInterval)
                    continue;

                LAllocation *typeAlloc = typeInterval->getAllocation();
                LAllocation *payloadAlloc = payloadInterval->getAllocation();

                // If the payload is an argument, we'll scan that explicitly as
                // part of the frame. It is therefore safe to not add any
                // safepoint entry.
                if (payloadAlloc->isArgument())
                    continue;

                if (IsSpilledAt(typeInterval, inputOf(ins)) &&
                    IsSpilledAt(payloadInterval, inputOf(ins)))
                {
                    // These two components of the Value are spilled
                    // contiguously, so simply keep track of the base slot.
                    uint32 payloadSlot = payload->canonicalSpillSlot();
                    uint32 slot = BaseOfNunboxSlot(LDefinition::PAYLOAD, payloadSlot);
                    if (!safepoint->addValueSlot(slot))
                        return false;
                }

                if (!ins->isCall() &&
                    (!IsSpilledAt(typeInterval, inputOf(ins)) || payloadAlloc->isGeneralReg()))
                {
                    // Either the payload is on the stack but the type is
                    // in a register, or the payload is in a register. In
                    // both cases, we don't have a contiguous spill so we
                    // add a torn entry.
                    if (!safepoint->addNunboxParts(*typeAlloc, *payloadAlloc))
                        return false;
                }
#endif
            }
        }

#ifdef JS_NUNBOX32
        if (IsNunbox(reg)) {
            // Skip past the next half of this nunbox so we don't track the
            // same slot twice.
            JS_ASSERT(&vregs[reg->def()->virtualRegister() + 1] == otherHalfOfNunbox(reg));
            i++;
        }
#endif
    }

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
    JS_ASSERT(interval->start() < pos && pos < interval->end());

    VirtualRegister *reg = interval->reg();

    // "Bogus" intervals cannot be split.
    JS_ASSERT(reg);

    // Do the split.
    LiveInterval *newInterval = new LiveInterval(reg, interval->index() + 1);
    if (!interval->splitFrom(pos, newInterval))
        return false;

    JS_ASSERT(interval->numRanges() > 0);
    JS_ASSERT(newInterval->numRanges() > 0);

    if (!reg->addInterval(newInterval))
        return false;

    IonSpew(IonSpew_RegAlloc, "  Split interval to %u = [%u, %u]/[%u, %u]",
            interval->reg()->reg(), interval->start().pos(),
            interval->end().pos(), newInterval->start().pos(),
            newInterval->end().pos());

    // We always want to enqueue the resulting split. We always split
    // forward, and we never want to handle something forward of our
    // current position.
    setIntervalRequirement(newInterval);

    // splitInterval() is usually called to split the node that has just been
    // popped from the unhandled queue. Therefore the split will likely be
    // closer to the lower start positions in the queue.
    unhandled.enqueueBackward(newInterval);

    return true;
}

bool
LinearScanAllocator::splitBlockingIntervals(LAllocation allocation)
{
    JS_ASSERT(allocation.isRegister());

    // Split current before the next fixed use.
    LiveInterval *fixed = fixedIntervals[allocation.toRegister().code()];
    if (fixed->numRanges() > 0) {
        CodePosition fixedPos = current->intersect(fixed);
        if (fixedPos != CodePosition::MIN) {
            JS_ASSERT(fixedPos < current->end());
            if (!splitInterval(current, fixedPos))
                return false;
        }
    }

    // Split the blocking interval if it exists.
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        if (i->getAllocation()->isRegister() && *i->getAllocation() == allocation) {
            IonSpew(IonSpew_RegAlloc, " Splitting active interval %u = [%u, %u]",
                    i->reg()->ins()->id(), i->start().pos(), i->end().pos());

            JS_ASSERT(i->start() != current->start());
            JS_ASSERT(i->covers(current->start()));
            JS_ASSERT(i->start() != current->start());

            if (!splitInterval(*i, current->start()))
                return false;

            LiveInterval *it = *i;
            active.removeAt(i);
            finishInterval(it);
            break;
        }
    }

    // Split any inactive intervals at the next live point.
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); ) {
        if (i->getAllocation()->isRegister() && *i->getAllocation() == allocation) {
            IonSpew(IonSpew_RegAlloc, " Splitting inactive interval %u = [%u, %u]",
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

    return true;
}

/*
 * Assign the current interval the given allocation, splitting conflicting
 * intervals as necessary to make the allocation stick.
 */
bool
LinearScanAllocator::assign(LAllocation allocation)
{
    if (allocation.isRegister())
        IonSpew(IonSpew_RegAlloc, "Assigning register %s", allocation.toRegister().name());
    current->setAllocation(allocation);

    // Split this interval at the next incompatible one
    VirtualRegister *reg = current->reg();
    if (reg) {
        CodePosition splitPos = current->firstIncompatibleUse(allocation);
        if (splitPos != CodePosition::MAX) {
            // Split before the incompatible use. This ensures the use position is
            // part of the second half of the interval and guarantees we never split
            // at the end (zero-length intervals are invalid).
            splitPos = splitPos.previous();
            JS_ASSERT (splitPos < current->end());
            if (!splitInterval(current, splitPos))
                return false;
        }
    }

    if (reg && allocation.isMemory()) {
        if (reg->canonicalSpill()) {
            JS_ASSERT(allocation == *current->reg()->canonicalSpill());

            // This interval is spilled more than once, so just always spill
            // it at its definition.
            reg->setSpillAtDefinition(outputOf(reg->ins()));
        } else {
            reg->setCanonicalSpill(current->getAllocation());

            // If this spill is inside a loop, and the definition is outside
            // the loop, instead move the spill to outside the loop.
            InstructionData *other = &insData[current->start()];
            uint32 loopDepthAtDef = reg->block()->mir()->loopDepth();
            uint32 loopDepthAtSpill = other->block()->mir()->loopDepth();
            if (loopDepthAtSpill > loopDepthAtDef)
                reg->setSpillAtDefinition(outputOf(reg->ins()));
        }
    }

    active.pushBack(current);

    return true;
}

#ifdef JS_NUNBOX32
VirtualRegister *
LinearScanAllocator::otherHalfOfNunbox(VirtualRegister *vreg)
{
    signed offset = OffsetToOtherHalfOfNunbox(vreg->type());
    VirtualRegister *other = &vregs[vreg->def()->virtualRegister() + offset];
    AssertTypesFormANunbox(vreg->type(), other->type());
    return other;
}
#endif


uint32
LinearScanAllocator::allocateSlotFor(const LiveInterval *interval)
{
    SlotList *freed;
    if (interval->reg()->type() == LDefinition::DOUBLE || IsNunbox(interval->reg()))
        freed = &finishedDoubleSlots_;
    else
        freed = &finishedSlots_;

    if (!freed->empty()) {
        LiveInterval *maybeDead = freed->back();
        if (maybeDead->end() < interval->reg()->getInterval(0)->start()) {
            // This spill slot is dead before the start of the interval trying
            // to reuse the slot, so reuse is safe. Otherwise, we could
            // encounter a situation where a stack slot is allocated and freed
            // inside a loop, but the same allocation is then used to hold a
            // loop-carried value.
            //
            // Note that we don't reuse the dead slot if its interval ends right
            // before the current interval, to avoid conflicting slot -> reg and
            // reg -> slot moves in the same movegroup.
            freed->popBack();
            VirtualRegister *dead = maybeDead->reg();
#ifdef JS_NUNBOX32
            if (IsNunbox(dead))
                return BaseOfNunboxSlot(dead->type(), dead->canonicalSpillSlot());
#endif
            return dead->canonicalSpillSlot();
        }
    }

    if (IsNunbox(interval->reg()))
        return stackSlotAllocator.allocateValueSlot();
    if (interval->reg()->isDouble())
        return stackSlotAllocator.allocateDoubleSlot();
    return stackSlotAllocator.allocateSlot();
}

bool
LinearScanAllocator::spill()
{
    IonSpew(IonSpew_RegAlloc, "  Decided to spill current interval");

    // We can't spill bogus intervals
    JS_ASSERT(current->reg());

    if (current->reg()->canonicalSpill()) {
        IonSpew(IonSpew_RegAlloc, "  Allocating canonical spill location");

        return assign(*current->reg()->canonicalSpill());
    }

    uint32 stackSlot;
#if defined JS_NUNBOX32
    if (IsNunbox(current->reg())) {
        VirtualRegister *other = otherHalfOfNunbox(current->reg());

        if (other->canonicalSpill()) {
            // The other half of this nunbox already has a spill slot. To
            // ensure the Value is spilled contiguously, use the other half (it
            // was allocated double-wide).
            JS_ASSERT(other->canonicalSpill()->isStackSlot());
            stackSlot = BaseOfNunboxSlot(other->type(), other->canonicalSpillSlot());
        } else {
            // No canonical spill location exists for this nunbox yet. Allocate
            // one.
            stackSlot = allocateSlotFor(current);
        }
        stackSlot -= OffsetOfNunboxSlot(current->reg()->type());
    } else
#endif
    {
        stackSlot = allocateSlotFor(current);
    }
    JS_ASSERT(stackSlot <= stackSlotAllocator.stackHeight());

    return assign(LStackSlot(stackSlot, current->reg()->isDouble()));
}

void
LinearScanAllocator::freeAllocation(LiveInterval *interval, LAllocation *alloc)
{
    VirtualRegister *mine = interval->reg();
    if (!IsNunbox(mine)) {
        if (alloc->isStackSlot()) {
            if (alloc->toStackSlot()->isDouble())
                finishedDoubleSlots_.append(interval);
            else
                finishedSlots_.append(interval);
        }
        return;
    }

#ifdef JS_NUNBOX32
    // Special handling for nunboxes. We can only free the stack slot once we
    // know both intervals have been finished.
    VirtualRegister *other = otherHalfOfNunbox(mine);
    if (other->finished()) {
        if (!mine->canonicalSpill() && !other->canonicalSpill())
            return;

        JS_ASSERT_IF(mine->canonicalSpill() && other->canonicalSpill(),
                     mine->canonicalSpill()->isStackSlot() == other->canonicalSpill()->isStackSlot());

        VirtualRegister *candidate = mine->canonicalSpill() ? mine : other;
        if (!candidate->canonicalSpill()->isStackSlot())
            return;

        finishedDoubleSlots_.append(candidate->lastInterval());
    }
#endif
}

void
LinearScanAllocator::finishInterval(LiveInterval *interval)
{
    LAllocation *alloc = interval->getAllocation();
    JS_ASSERT(!alloc->isUse());

    // Toss out the bogus interval now that it's run its course
    if (!interval->reg())
        return;

    // All spills should be equal to the canonical spill location.
    JS_ASSERT_IF(alloc->isStackSlot(), *alloc == *interval->reg()->canonicalSpill());

    bool lastInterval = interval->index() == (interval->reg()->numIntervals() - 1);
    if (lastInterval) {
        freeAllocation(interval, alloc);
        interval->reg()->setFinished();
    }

    handled.pushBack(interval);
}

/*
 * This function locates a register which may be assigned by the register
 * and is not assigned to any active interval. The register which is free
 * for the longest period of time is then returned.
 */
AnyRegister::Code
LinearScanAllocator::findBestFreeRegister(CodePosition *freeUntil)
{
    IonSpew(IonSpew_RegAlloc, "  Computing freeUntilPos");

    // Compute free-until positions for all registers
    CodePosition freeUntilPos[AnyRegister::Total];
    bool needFloat = current->reg()->isDouble();
    for (AnyRegisterIterator regs(allRegisters_); regs.more(); regs++) {
        AnyRegister reg = *regs;
        if (reg.isFloat() == needFloat)
            freeUntilPos[reg.code()] = CodePosition::MAX;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        if (i->getAllocation()->isRegister()) {
            AnyRegister reg = i->getAllocation()->toRegister();
            IonSpew(IonSpew_RegAlloc, "   Register %s not free", reg.name());
            freeUntilPos[reg.code()] = CodePosition::MIN;
        }
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        if (i->getAllocation()->isRegister()) {
            AnyRegister reg = i->getAllocation()->toRegister();
            CodePosition pos = current->intersect(*i);
            if (pos != CodePosition::MIN && pos < freeUntilPos[reg.code()]) {
                freeUntilPos[reg.code()] = pos;
                IonSpew(IonSpew_RegAlloc, "   Register %s free until %u", reg.name(), pos.pos());
            }
        }
    }

    CodePosition fixedPos = fixedIntervalsUnion->intersect(current);
    if (fixedPos != CodePosition::MIN) {
        for (IntervalIterator i(fixed.begin()); i != fixed.end(); i++) {
            AnyRegister reg = i->getAllocation()->toRegister();
            if (freeUntilPos[reg.code()] != CodePosition::MIN) {
                CodePosition pos = current->intersect(*i);
                if (pos != CodePosition::MIN && pos < freeUntilPos[reg.code()]) {
                    freeUntilPos[reg.code()] = (pos == current->start()) ? CodePosition::MIN : pos;
                    IonSpew(IonSpew_RegAlloc, "   Register %s free until %u", reg.name(), pos.pos());
                }
            }
        }
    }

    AnyRegister::Code bestCode = AnyRegister::Invalid;
    if (current->index()) {
        // As an optimization, use the allocation from the previous interval if
        // it is available.
        LiveInterval *previous = current->reg()->getInterval(current->index() - 1);
        if (previous->getAllocation()->isRegister()) {
            AnyRegister prevReg = previous->getAllocation()->toRegister();
            if (freeUntilPos[prevReg.code()] != CodePosition::MIN)
                bestCode = prevReg.code();
        }
    }

    // Assign the register suggested by the hint if it's free.
    Requirement *hint = current->hint();
    if (hint->kind() == Requirement::FIXED && hint->allocation().isRegister()) {
        AnyRegister hintReg = hint->allocation().toRegister();
        if (freeUntilPos[hintReg.code()] > hint->pos())
            bestCode = hintReg.code();
    } else if (hint->kind() == Requirement::SAME_AS_OTHER) {
        LiveInterval *other = vregs[hint->virtualRegister()].intervalFor(hint->pos());
        if (other && other->getAllocation()->isRegister()) {
            AnyRegister hintReg = other->getAllocation()->toRegister();
            if (freeUntilPos[hintReg.code()] > hint->pos())
                bestCode = hintReg.code();
        }
    }

    if (bestCode == AnyRegister::Invalid) {
        // If all else fails, search freeUntilPos for largest value
        for (uint32 i = 0; i < AnyRegister::Total; i++) {
            if (freeUntilPos[i] == CodePosition::MIN)
                continue;
            if (bestCode == AnyRegister::Invalid || freeUntilPos[i] > freeUntilPos[bestCode])
                bestCode = AnyRegister::Code(i);
        }
    }

    if (bestCode != AnyRegister::Invalid)
        *freeUntil = freeUntilPos[bestCode];
    return bestCode;
}

/*
 * This function locates a register which is assigned to an active interval,
 * and returns the one with the furthest away next use. As a side effect,
 * the nextUsePos array is updated with the next use position of all active
 * intervals for use elsewhere in the algorithm.
 */
AnyRegister::Code
LinearScanAllocator::findBestBlockedRegister(CodePosition *nextUsed)
{
    IonSpew(IonSpew_RegAlloc, "  Computing nextUsePos");

    // Compute next-used positions for all registers
    CodePosition nextUsePos[AnyRegister::Total];
    bool needFloat = current->reg()->isDouble();
    for (AnyRegisterIterator regs(allRegisters_); regs.more(); regs++) {
        AnyRegister reg = *regs;
        if (reg.isFloat() == needFloat)
            nextUsePos[reg.code()] = CodePosition::MAX;
    }
    for (IntervalIterator i(active.begin()); i != active.end(); i++) {
        if (i->getAllocation()->isRegister()) {
            AnyRegister reg = i->getAllocation()->toRegister();
            if (i->start().ins() == current->start().ins()) {
                nextUsePos[reg.code()] = CodePosition::MIN;
                IonSpew(IonSpew_RegAlloc, "   Disqualifying %s due to recency", reg.name());
            } else if (nextUsePos[reg.code()] != CodePosition::MIN) {
                nextUsePos[reg.code()] = i->nextUsePosAfter(current->start());
                IonSpew(IonSpew_RegAlloc, "   Register %s next used %u", reg.name(),
                        nextUsePos[reg.code()].pos());
            }
        }
    }
    for (IntervalIterator i(inactive.begin()); i != inactive.end(); i++) {
        if (i->getAllocation()->isRegister()) {
            AnyRegister reg = i->getAllocation()->toRegister();
            CodePosition pos = i->nextUsePosAfter(current->start());
            if (pos < nextUsePos[reg.code()]) {
                nextUsePos[reg.code()] = pos;
                IonSpew(IonSpew_RegAlloc, "   Register %s next used %u", reg.name(), pos.pos());
            }
        }
    }

    CodePosition fixedPos = fixedIntervalsUnion->intersect(current);
    if (fixedPos != CodePosition::MIN) {
        for (IntervalIterator i(fixed.begin()); i != fixed.end(); i++) {
            AnyRegister reg = i->getAllocation()->toRegister();
            if (nextUsePos[reg.code()] != CodePosition::MIN) {
                CodePosition pos = i->intersect(current);
                if (pos != CodePosition::MIN && pos < nextUsePos[reg.code()]) {
                    nextUsePos[reg.code()] = pos;
                    IonSpew(IonSpew_RegAlloc, "   Register %s next used %u (fixed)", reg.name(), pos.pos());
                }
            }
        }
    }

    // Search nextUsePos for largest value
    AnyRegister::Code bestCode = AnyRegister::Invalid;
    for (size_t i = 0; i < AnyRegister::Total; i++) {
        if (nextUsePos[i] == CodePosition::MIN)
            continue;
        if (bestCode == AnyRegister::Invalid || nextUsePos[i] > nextUsePos[bestCode])
            bestCode = AnyRegister::Code(i);
    }

    if (bestCode != AnyRegister::Invalid)
        *nextUsed = nextUsePos[bestCode];
    return bestCode;
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
    if (aa->isRegister() && ba->isRegister() && aa->toRegister() == ba->toRegister())
        return a->intersect(b) == CodePosition::MIN;
    return true;
}

LMoveGroup *
LinearScanAllocator::getInputMoveGroup(CodePosition pos)
{
    InstructionData *data = &insData[pos];
    JS_ASSERT(!data->ins()->isPhi());
    JS_ASSERT(!data->ins()->isLabel());;
    JS_ASSERT(inputOf(data->ins()) == pos);

    if (data->inputMoves())
        return data->inputMoves();

    LMoveGroup *moves = new LMoveGroup;
    data->setInputMoves(moves);
    data->block()->insertBefore(data->ins(), moves);

    return moves;
}

LMoveGroup *
LinearScanAllocator::getMoveGroupAfter(CodePosition pos)
{
    InstructionData *data = &insData[pos];
    JS_ASSERT(!data->ins()->isPhi());
    JS_ASSERT(outputOf(data->ins()) == pos);

    if (data->movesAfter())
        return data->movesAfter();

    LMoveGroup *moves = new LMoveGroup;
    data->setMovesAfter(moves);

    if (data->ins()->isLabel())
        data->block()->insertAfter(data->block()->getEntryMoveGroup(), moves);
    else
        data->block()->insertAfter(data->ins(), moves);
    return moves;
}

bool
LinearScanAllocator::addMove(LMoveGroup *moves, LiveInterval *from, LiveInterval *to)
{
    if (*from->getAllocation() == *to->getAllocation())
        return true;
    return moves->add(from->getAllocation(), to->getAllocation());
}

bool
LinearScanAllocator::moveInput(CodePosition pos, LiveInterval *from, LiveInterval *to)
{
    LMoveGroup *moves = getInputMoveGroup(pos);
    return addMove(moves, from, to);
}

bool
LinearScanAllocator::moveAfter(CodePosition pos, LiveInterval *from, LiveInterval *to)
{
    LMoveGroup *moves = getMoveGroupAfter(pos);
    return addMove(moves, from, to);
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
        if (i->getAllocation()->isRegister()) {
            JS_ASSERT(i->end() <= current->start());
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
 * in the function can coexist with every other interval.
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

void
LiveInterval::validateRanges()
{
    Range *prev = NULL;

    for (size_t i = ranges_.length() - 1; i < ranges_.length(); i--) {
        Range *range = &ranges_[i];

        JS_ASSERT(range->from < range->to);
        JS_ASSERT_IF(prev, prev->to <= range->from);
        prev = range;
    }
}

void
LinearScanAllocator::validateVirtualRegisters()
{
    for (size_t i = 1; i < graph.numVirtualRegisters(); i++) {
        VirtualRegister *reg = &vregs[i];

        LiveInterval *prev = NULL;
        for (size_t j = 0; j < reg->numIntervals(); j++) {
            LiveInterval *interval = reg->getInterval(j);
            JS_ASSERT(reg == interval->reg());
            JS_ASSERT(interval->index() == j);

            if (interval->numRanges() == 0)
                continue;

            JS_ASSERT_IF(prev, prev->end() <= interval->start());
            interval->validateRanges();

            prev = interval;
        }
    }
}

#endif // DEBUG

bool
LinearScanAllocator::go()
{
    IonSpew(IonSpew_RegAlloc, "Beginning register allocation");

    IonSpew(IonSpew_RegAlloc, "Beginning creation of initial data structures");
    if (!createDataStructures())
        return false;
    IonSpew(IonSpew_RegAlloc, "Creation of initial data structures completed");

    IonSpew(IonSpew_RegAlloc, "Beginning liveness analysis");
    if (!buildLivenessInfo())
        return false;
    IonSpew(IonSpew_RegAlloc, "Liveness analysis complete");

    IonSpew(IonSpew_RegAlloc, "Beginning preliminary register allocation");
    if (!allocateRegisters())
        return false;
    IonSpew(IonSpew_RegAlloc, "Preliminary register allocation complete");

    IonSpew(IonSpew_RegAlloc, "Beginning control flow resolution");
    if (!resolveControlFlow())
        return false;
    IonSpew(IonSpew_RegAlloc, "Control flow resolution complete");

    IonSpew(IonSpew_RegAlloc, "Beginning register allocation reification");
    if (!reifyAllocations())
        return false;
    IonSpew(IonSpew_RegAlloc, "Register allocation reification complete");

    IonSpew(IonSpew_RegAlloc, "Beginning safepoint population.");
    if (!populateSafepoints())
        return false;
    IonSpew(IonSpew_RegAlloc, "Safepoint population complete.");

    IonSpew(IonSpew_RegAlloc, "Register allocation complete");

    return true;
}

void
LinearScanAllocator::setIntervalRequirement(LiveInterval *interval)
{
    JS_ASSERT(interval->requirement()->kind() == Requirement::NONE);
    JS_ASSERT(interval->hint()->kind() == Requirement::NONE);

    // This function computes requirement by virtual register, other types of
    // interval should have requirements set manually
    VirtualRegister *reg = interval->reg();
    JS_ASSERT(reg);

    if (interval->index() == 0) {
        // The first interval is the definition, so deal with any definition
        // constraints/hints

        if (reg->def()->policy() == LDefinition::PRESET) {
            // Preset policies get a FIXED requirement or hint.
            if (reg->def()->output()->isRegister())
                interval->setHint(Requirement(*reg->def()->output()));
            else
                interval->setRequirement(Requirement(*reg->def()->output()));
        } else if (reg->def()->policy() == LDefinition::MUST_REUSE_INPUT) {
            // Reuse policies get either a FIXED requirement or a SAME_AS hint.
            LUse *use = reg->ins()->getOperand(reg->def()->getReusedInput())->toUse();
            interval->setRequirement(Requirement(Requirement::REGISTER));
            interval->setHint(Requirement(use->virtualRegister(), interval->start().previous()));
        } else if (reg->ins()->isPhi()) {
            // Phis don't have any requirements, but they should prefer
            // their input allocations, so they get a SAME_AS hint of the
            // first input
            LUse *use = reg->ins()->getOperand(0)->toUse();
            LBlock *predecessor = reg->block()->mir()->getPredecessor(0)->lir();
            CodePosition predEnd = outputOf(predecessor->lastId());
            interval->setHint(Requirement(use->virtualRegister(), predEnd));
        } else {
            // Non-phis get a REGISTER requirement
            interval->setRequirement(Requirement(Requirement::REGISTER));
        }
    }

    UsePosition *fixedOp = NULL;
    UsePosition *registerOp = NULL;

    // Search uses at the start of the interval for requirements.
    UsePositionIterator usePos(interval->usesBegin());
    for (; usePos != interval->usesEnd(); usePos++) {
        if (interval->start().next() < usePos->pos)
            break;

        LUse::Policy policy = usePos->use->policy();
        if (policy == LUse::FIXED) {
            fixedOp = *usePos;
            interval->setRequirement(Requirement(Requirement::REGISTER));
            break;
        } else if (policy == LUse::REGISTER) {
            // Register uses get a REGISTER requirement
            interval->setRequirement(Requirement(Requirement::REGISTER));
        }
    }

    // Search other uses for hints. If the virtual register already has a
    // canonical spill location, we will eagerly spill this interval, so we
    // don't have to search for hints.
    if (!fixedOp && !interval->reg()->canonicalSpill()) {
        for (; usePos != interval->usesEnd(); usePos++) {
            LUse::Policy policy = usePos->use->policy();
            if (policy == LUse::FIXED) {
                fixedOp = *usePos;
                break;
            } else if (policy == LUse::REGISTER) {
                if (!registerOp)
                    registerOp = *usePos;
            }
        }
    }

    if (fixedOp) {
        // Intervals with a fixed use now get a FIXED hint.
        AnyRegister required = GetFixedRegister(reg->def(), fixedOp->use);
        interval->setHint(Requirement(LAllocation(required), fixedOp->pos));
    } else if (registerOp) {
        // Intervals with register uses get a REGISTER hint. We may have already
        // assigned a SAME_AS hint, make sure we don't overwrite it with a weaker
        // hint.
        if (interval->hint()->kind() == Requirement::NONE)
            interval->setHint(Requirement(Requirement::REGISTER, registerOp->pos));
    }
}

/*
 * Enqueue by iteration starting from the node with the lowest start position.
 *
 * If we assign registers to intervals in order of their start positions
 * without regard to their requirements, we can end up in situations where
 * intervals that do not require registers block intervals that must have
 * registers from getting one. To avoid this, we ensure that intervals are
 * ordered by position and priority so intervals with more stringent
 * requirements are handled first.
 */
void
LinearScanAllocator::UnhandledQueue::enqueueBackward(LiveInterval *interval)
{
    IntervalReverseIterator i(rbegin()); 

    for (; i != rend(); i++) {
        if (i->start() > interval->start())
            break;
        if (i->start() == interval->start() &&
            i->requirement()->priority() >= interval->requirement()->priority())
        {
            break;
        }
    }
    insertAfter(*i, interval);
}

/*
 * Enqueue by iteration from high start position to low start position,
 * after a provided node.
 */
void
LinearScanAllocator::UnhandledQueue::enqueueForward(LiveInterval *after, LiveInterval *interval)
{
    IntervalIterator i(begin(after));
    i++; // Skip the initial node.

    for (; i != end(); i++) {
        if (i->start() < interval->start())
            break;
        if (i->start() == interval->start() &&
            i->requirement()->priority() < interval->requirement()->priority())
        {
            break;
        }
    }
    insertBefore(*i, interval);
}

/*
 * Append to the queue head in O(1).
 */
void
LinearScanAllocator::UnhandledQueue::enqueueAtHead(LiveInterval *interval)
{
#ifdef DEBUG
    // Assuming that the queue is in sorted order, assert that order is
    // maintained by inserting at the back.
    if (!empty()) {
        LiveInterval *back = peekBack();
        JS_ASSERT(back->start() >= interval->start());
        JS_ASSERT_IF(back->start() == interval->start(),
                     back->requirement()->priority() >= interval->requirement()->priority());
    }
#endif

    pushBack(interval);
}

void
LinearScanAllocator::UnhandledQueue::assertSorted()
{
#ifdef DEBUG
    LiveInterval *prev = NULL;
    for (IntervalIterator i(begin()); i != end(); i++) {
        if (prev) {
            JS_ASSERT(prev->start() >= i->start());
            JS_ASSERT_IF(prev->start() == i->start(),
                         prev->requirement()->priority() >= i->requirement()->priority());
        }
        prev = *i;
    }
#endif
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
