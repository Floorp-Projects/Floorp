/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>

#include "mozilla/DebugOnly.h"

#include "BitSet.h"
#include "LinearScan.h"
#include "IonBuilder.h"
#include "IonSpewer.h"
#include "LIR-inl.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;

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

        if (mir->shouldCancel("LSRA Allocate Registers (main loop)"))
            return false;

        CodePosition position = current->start();
        const Requirement *req = current->requirement();
        const Requirement *hint = current->hint();

        IonSpew(IonSpew_RegAlloc, "Processing %d = [%u, %u] (pri=%d)",
                current->hasVreg() ? current->vreg() : 0, current->start().pos(),
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
                    current->hasVreg() ? current->vreg() : 0);
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
        if (mir->shouldCancel("LSRA Resolve Control Flow (main loop)"))
            return false;

        LBlock *successor = graph.getBlock(i);
        MBasicBlock *mSuccessor = successor->mir();
        if (mSuccessor->numPredecessors() < 1)
            continue;

        // Resolve phis to moves
        for (size_t j = 0; j < successor->numPhis(); j++) {
            LPhi *phi = successor->getPhi(j);
            JS_ASSERT(phi->numDefs() == 1);
            LinearScanVirtualRegister *vreg = &vregs[phi->getDef(0)];
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
                if (!moves->add(to->getAllocation(), vregs[to->vreg()].canonicalSpill()))
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
        LinearScanVirtualRegister *reg = &vregs[j];
        if (mir->shouldCancel("LSRA Reification (main loop)"))
            return false;

    for (size_t k = 0; k < reg->numIntervals(); k++) {
        LiveInterval *interval = reg->getInterval(k);
        JS_ASSERT(reg == &vregs[interval->vreg()]);
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

                    JS_ASSERT(!alloc->isUse());

                    *alloc = *interval->getAllocation();
                    if (!moveInputAlloc(inputOf(reg->ins()), origAlloc, alloc))
                        return false;
                }

                JS_ASSERT(DefinitionCompatibleWith(reg->ins(), def, *interval->getAllocation()));
                def->setOutput(*interval->getAllocation());

                spillFrom = interval->getAllocation();
            }

            if (reg->ins()->recoversInput()) {
                LSnapshot *snapshot = reg->ins()->snapshot();
                for (size_t i = 0; i < snapshot->numEntries(); i++) {
                    LAllocation *entry = snapshot->getEntry(i);
                    if (entry->isUse() && entry->toUse()->policy() == LUse::RECOVERED_INPUT)
                        *entry = *def->output();
                }
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

        addLiveRegistersForInterval(reg, interval);
    }} // Iteration over virtual register intervals.

    // Set the graph overall stack height
    graph.setLocalSlotCount(stackSlotAllocator.stackHeight());

    return true;
}

inline bool
LinearScanAllocator::isSpilledAt(LiveInterval *interval, CodePosition pos)
{
    LinearScanVirtualRegister *reg = &vregs[interval->vreg()];
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

    for (uint32_t i = 0; i < vregs.numVirtualRegisters(); i++) {
        LinearScanVirtualRegister *reg = &vregs[i];

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

                if (isSpilledAt(interval, inputOf(ins))) {
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
                LinearScanVirtualRegister *other = otherHalfOfNunbox(reg);
                LinearScanVirtualRegister *type = (reg->type() == LDefinition::TYPE) ? reg : other;
                LinearScanVirtualRegister *payload = (reg->type() == LDefinition::PAYLOAD) ? reg : other;
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

                if (isSpilledAt(typeInterval, inputOf(ins)) &&
                    isSpilledAt(payloadInterval, inputOf(ins)))
                {
                    // These two components of the Value are spilled
                    // contiguously, so simply keep track of the base slot.
                    uint32_t payloadSlot = payload->canonicalSpillSlot();
                    uint32_t slot = BaseOfNunboxSlot(LDefinition::PAYLOAD, payloadSlot);
                    if (!safepoint->addValueSlot(slot))
                        return false;
                }

                if (!ins->isCall() &&
                    (!isSpilledAt(typeInterval, inputOf(ins)) || payloadAlloc->isGeneralReg()))
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

    LinearScanVirtualRegister *reg = &vregs[interval->vreg()];

    // "Bogus" intervals cannot be split.
    JS_ASSERT(reg);

    // Do the split.
    LiveInterval *newInterval = new LiveInterval(interval->vreg(), interval->index() + 1);
    if (!interval->splitFrom(pos, newInterval))
        return false;

    JS_ASSERT(interval->numRanges() > 0);
    JS_ASSERT(newInterval->numRanges() > 0);

    if (!reg->addInterval(newInterval))
        return false;

    IonSpew(IonSpew_RegAlloc, "  Split interval to %u = [%u, %u]/[%u, %u]",
            interval->vreg(), interval->start().pos(),
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
                    vregs[i->vreg()].ins()->id(), i->start().pos(), i->end().pos());

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
                    vregs[i->vreg()].ins()->id(), i->start().pos(), i->end().pos());

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
    LinearScanVirtualRegister *reg = &vregs[current->vreg()];
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
            JS_ASSERT(allocation == *reg->canonicalSpill());

            // This interval is spilled more than once, so just always spill
            // it at its definition.
            reg->setSpillAtDefinition(outputOf(reg->ins()));
        } else {
            reg->setCanonicalSpill(current->getAllocation());

            // If this spill is inside a loop, and the definition is outside
            // the loop, instead move the spill to outside the loop.
            InstructionData *other = &insData[current->start()];
            uint32_t loopDepthAtDef = reg->block()->mir()->loopDepth();
            uint32_t loopDepthAtSpill = other->block()->mir()->loopDepth();
            if (loopDepthAtSpill > loopDepthAtDef)
                reg->setSpillAtDefinition(outputOf(reg->ins()));
        }
    }

    active.pushBack(current);

    return true;
}

uint32_t
LinearScanAllocator::allocateSlotFor(const LiveInterval *interval)
{
    LinearScanVirtualRegister *reg = &vregs[interval->vreg()];

    SlotList *freed;
    if (reg->type() == LDefinition::DOUBLE || IsNunbox(reg))
        freed = &finishedDoubleSlots_;
    else
        freed = &finishedSlots_;

    if (!freed->empty()) {
        LiveInterval *maybeDead = freed->back();
        if (maybeDead->end() < reg->getInterval(0)->start()) {
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
            LinearScanVirtualRegister *dead = &vregs[maybeDead->vreg()];
#ifdef JS_NUNBOX32
            if (IsNunbox(dead))
                return BaseOfNunboxSlot(dead->type(), dead->canonicalSpillSlot());
#endif
            return dead->canonicalSpillSlot();
        }
    }

    if (IsNunbox(reg))
        return stackSlotAllocator.allocateValueSlot();
    if (reg->isDouble())
        return stackSlotAllocator.allocateDoubleSlot();
    return stackSlotAllocator.allocateSlot();
}

bool
LinearScanAllocator::spill()
{
    IonSpew(IonSpew_RegAlloc, "  Decided to spill current interval");

    // We can't spill bogus intervals
    JS_ASSERT(current->hasVreg());

    LinearScanVirtualRegister *reg = &vregs[current->vreg()];

    if (reg->canonicalSpill()) {
        IonSpew(IonSpew_RegAlloc, "  Allocating canonical spill location");

        return assign(*reg->canonicalSpill());
    }

    uint32_t stackSlot;
#if defined JS_NUNBOX32
    if (IsNunbox(reg)) {
        LinearScanVirtualRegister *other = otherHalfOfNunbox(reg);

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
        stackSlot -= OffsetOfNunboxSlot(reg->type());
    } else
#endif
    {
        stackSlot = allocateSlotFor(current);
    }
    JS_ASSERT(stackSlot <= stackSlotAllocator.stackHeight());

    return assign(LStackSlot(stackSlot, reg->isDouble()));
}

void
LinearScanAllocator::freeAllocation(LiveInterval *interval, LAllocation *alloc)
{
    LinearScanVirtualRegister *mine = &vregs[interval->vreg()];
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
    LinearScanVirtualRegister *other = otherHalfOfNunbox(mine);
    if (other->finished()) {
        if (!mine->canonicalSpill() && !other->canonicalSpill())
            return;

        JS_ASSERT_IF(mine->canonicalSpill() && other->canonicalSpill(),
                     mine->canonicalSpill()->isStackSlot() == other->canonicalSpill()->isStackSlot());

        LinearScanVirtualRegister *candidate = mine->canonicalSpill() ? mine : other;
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
    if (!interval->hasVreg())
        return;

    LinearScanVirtualRegister *reg = &vregs[interval->vreg()];

    // All spills should be equal to the canonical spill location.
    JS_ASSERT_IF(alloc->isStackSlot(), *alloc == *reg->canonicalSpill());

    bool lastInterval = interval->index() == (reg->numIntervals() - 1);
    if (lastInterval) {
        freeAllocation(interval, alloc);
        reg->setFinished();
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
    bool needFloat = vregs[current->vreg()].isDouble();
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
        LiveInterval *previous = vregs[current->vreg()].getInterval(current->index() - 1);
        if (previous->getAllocation()->isRegister()) {
            AnyRegister prevReg = previous->getAllocation()->toRegister();
            if (freeUntilPos[prevReg.code()] != CodePosition::MIN)
                bestCode = prevReg.code();
        }
    }

    // Assign the register suggested by the hint if it's free.
    const Requirement *hint = current->hint();
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
        for (uint32_t i = 0; i < AnyRegister::Total; i++) {
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
    bool needFloat = vregs[current->vreg()].isDouble();
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
        LinearScanVirtualRegister *reg = &vregs[i->vreg()];
        bool found = false;
        for (size_t j = 0; j < reg->numIntervals(); j++) {
            if (reg->getInterval(j) == *i) {
                JS_ASSERT(j == i->index());
                found = true;
                break;
            }
        }
        JS_ASSERT(found);
    }
}

#endif // DEBUG

bool
LinearScanAllocator::go()
{
    IonSpew(IonSpew_RegAlloc, "Beginning register allocation");

    IonSpew(IonSpew_RegAlloc, "Beginning liveness analysis");
    if (!buildLivenessInfo())
        return false;
    IonSpew(IonSpew_RegAlloc, "Liveness analysis complete");

    if (mir->shouldCancel("LSRA Liveness"))
        return false;

    IonSpew(IonSpew_RegAlloc, "Beginning preliminary register allocation");
    if (!allocateRegisters())
        return false;
    IonSpew(IonSpew_RegAlloc, "Preliminary register allocation complete");

    if (mir->shouldCancel("LSRA Preliminary Regalloc"))
        return false;

    IonSpew(IonSpew_RegAlloc, "Beginning control flow resolution");
    if (!resolveControlFlow())
        return false;
    IonSpew(IonSpew_RegAlloc, "Control flow resolution complete");

    if (mir->shouldCancel("LSRA Control Flow"))
        return false;

    IonSpew(IonSpew_RegAlloc, "Beginning register allocation reification");
    if (!reifyAllocations())
        return false;
    IonSpew(IonSpew_RegAlloc, "Register allocation reification complete");

    if (mir->shouldCancel("LSRA Reification"))
        return false;

    IonSpew(IonSpew_RegAlloc, "Beginning safepoint population.");
    if (!populateSafepoints())
        return false;
    IonSpew(IonSpew_RegAlloc, "Safepoint population complete.");

    if (mir->shouldCancel("LSRA Safepoints"))
        return false;

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
    LinearScanVirtualRegister *reg = &vregs[interval->vreg()];

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
    if (!fixedOp && !vregs[interval->vreg()].canonicalSpill()) {
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
