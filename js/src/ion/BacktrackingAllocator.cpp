/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BacktrackingAllocator.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;

bool
BacktrackingAllocator::init()
{
    RegisterSet remainingRegisters(allRegisters_);
    while (!remainingRegisters.empty(/* float = */ false)) {
        AnyRegister reg = AnyRegister(remainingRegisters.takeGeneral());
        registers[reg.code()].allocatable = true;
    }
    while (!remainingRegisters.empty(/* float = */ true)) {
        AnyRegister reg = AnyRegister(remainingRegisters.takeFloat());
        registers[reg.code()].allocatable = true;
    }

    LifoAlloc *alloc = mir->temp().lifoAlloc();
    for (size_t i = 0; i < AnyRegister::Total; i++) {
        registers[i].reg = AnyRegister::FromCode(i);
        registers[i].allocations.setAllocator(alloc);

        LiveInterval *fixed = fixedIntervals[i];
        for (size_t j = 0; j < fixed->numRanges(); j++) {
            AllocatedRange range(fixed, fixed->getRange(j));
            if (!registers[i].allocations.insert(range))
                return false;
        }
    }

    hotcode.setAllocator(alloc);

    // Partition the graph into hot and cold sections, for helping to make
    // splitting decisions. Since we don't have any profiling data this is a
    // crapshoot, so just mark the bodies of inner loops as hot and everything
    // else as cold.

    LiveInterval *hotcodeInterval = new LiveInterval(0);

    LBlock *backedge = NULL;
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *block = graph.getBlock(i);

        // If we see a loop header, mark the backedge so we know when we have
        // hit the end of the loop. Don't process the loop immediately, so that
        // if there is an inner loop we will ignore the outer backedge.
        if (block->mir()->isLoopHeader())
            backedge = block->mir()->backedge()->lir();

        if (block == backedge) {
            LBlock *header = block->mir()->loopHeaderOfBackedge()->lir();
            CodePosition from = inputOf(header->firstId());
            CodePosition to = outputOf(block->lastId()).next();
            if (!hotcodeInterval->addRange(from, to))
                return false;
        }
    }

    for (size_t i = 0; i < hotcodeInterval->numRanges(); i++) {
        AllocatedRange range(hotcodeInterval, hotcodeInterval->getRange(i));
        if (!hotcode.insert(range))
            return false;
    }

    return true;
}

static inline const char *
IntervalString(const LiveInterval *interval)
{
#ifdef DEBUG
    if (!interval->numRanges())
        return " empty";

    // Not reentrant!
    static char buf[1000];

    char *cursor = buf;
    char *end = cursor + sizeof(buf);

    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);
        int n = JS_snprintf(cursor, end - cursor, " [%u,%u>", range->from.pos(), range->to.pos());
        if (n < 0)
            return " ???";
        cursor += n;
    }

    return buf;
#else
    return " ???";
#endif
}

bool
BacktrackingAllocator::go()
{
    IonSpew(IonSpew_RegAlloc, "Beginning register allocation");

    IonSpew(IonSpew_RegAlloc, "Beginning liveness analysis");
    if (!buildLivenessInfo())
        return false;
    IonSpew(IonSpew_RegAlloc, "Liveness analysis complete");

    if (IonSpewEnabled(IonSpew_RegAlloc))
        dumpLiveness();

    if (!init())
        return false;

    if (!allocationQueue.reserve(graph.numVirtualRegisters() * 3 / 2))
        return false;

    if (!groupAndQueueRegisters())
        return false;

    if (IonSpewEnabled(IonSpew_RegAlloc))
        dumpRegisterGroups();

    // Allocate, spill and split register intervals until finished.
    while (!allocationQueue.empty()) {
        if (mir->shouldCancel("Backtracking Allocation"))
            return false;

        QueueItem item = allocationQueue.removeHighest();
        if (item.interval ? !processInterval(item.interval) : !processGroup(item.group))
            return false;
    }

    if (IonSpewEnabled(IonSpew_RegAlloc))
        dumpAllocations();

    return resolveControlFlow() && reifyAllocations() && populateSafepoints();
}

static bool
LifetimesOverlap(BacktrackingVirtualRegister *reg0, BacktrackingVirtualRegister *reg1)
{
    // Registers may have been eagerly split in two, see tryGroupReusedRegister.
    // In such cases, only consider the first interval.
    JS_ASSERT(reg0->numIntervals() <= 2 && reg1->numIntervals() <= 2);

    LiveInterval *interval0 = reg0->getInterval(0), *interval1 = reg1->getInterval(0);

    // Interval ranges are sorted in reverse order. The lifetimes overlap if
    // any of their ranges overlap.
    size_t index0 = 0, index1 = 0;
    while (index0 < interval0->numRanges() && index1 < interval1->numRanges()) {
        const LiveInterval::Range
            *range0 = interval0->getRange(index0),
            *range1 = interval1->getRange(index1);
        if (range0->from >= range1->to)
            index0++;
        else if (range1->from >= range0->to)
            index1++;
        else
            return true;
    }

    return false;
}

bool
BacktrackingAllocator::canAddToGroup(VirtualRegisterGroup *group, BacktrackingVirtualRegister *reg)
{
    for (size_t i = 0; i < group->registers.length(); i++) {
        if (LifetimesOverlap(reg, &vregs[group->registers[i]]))
            return false;
    }
    return true;
}

bool
BacktrackingAllocator::tryGroupRegisters(uint32_t vreg0, uint32_t vreg1)
{
    // See if reg0 and reg1 can be placed in the same group, following the
    // restrictions imposed by VirtualRegisterGroup and any other registers
    // already grouped with reg0 or reg1.
    BacktrackingVirtualRegister *reg0 = &vregs[vreg0], *reg1 = &vregs[vreg1];

    if (reg0->isDouble() != reg1->isDouble())
        return true;

    VirtualRegisterGroup *group0 = reg0->group(), *group1 = reg1->group();

    if (!group0 && group1)
        return tryGroupRegisters(vreg1, vreg0);

    if (group0) {
        if (group1) {
            if (group0 == group1) {
                // The registers are already grouped together.
                return true;
            }
            // Try to unify the two distinct groups.
            for (size_t i = 0; i < group1->registers.length(); i++) {
                if (!canAddToGroup(group0, &vregs[group1->registers[i]]))
                    return true;
            }
            for (size_t i = 0; i < group1->registers.length(); i++) {
                uint32_t vreg = group1->registers[i];
                if (!group0->registers.append(vreg))
                    return false;
                vregs[vreg].setGroup(group0);
            }
            return true;
        }
        if (!canAddToGroup(group0, reg1))
            return true;
        if (!group0->registers.append(vreg1))
            return false;
        reg1->setGroup(group0);
        return true;
    }

    if (LifetimesOverlap(reg0, reg1))
        return true;

    VirtualRegisterGroup *group = new VirtualRegisterGroup();
    if (!group->registers.append(vreg0) || !group->registers.append(vreg1))
        return false;

    reg0->setGroup(group);
    reg1->setGroup(group);
    return true;
}

bool
BacktrackingAllocator::tryGroupReusedRegister(uint32_t def, uint32_t use)
{
    BacktrackingVirtualRegister &reg = vregs[def], &usedReg = vregs[use];

    // reg is a vreg which reuses its input usedReg for its output physical
    // register. Try to group reg with usedReg if at all possible, as avoiding
    // copies before reg's instruction is crucial for the quality of the
    // generated code (MUST_REUSE_INPUT is used by all arithmetic instructions
    // on x86/x64).

    if (reg.intervalFor(inputOf(reg.ins()))) {
        JS_ASSERT(reg.isTemp());
        reg.setMustCopyInput();
        return true;
    }

    if (!usedReg.intervalFor(outputOf(reg.ins()))) {
        // The input is not live after the instruction, either in a safepoint
        // for the instruction or in subsequent code. The input and output
        // can thus be in the same group.
        return tryGroupRegisters(use, def);
    }

    // The input is live afterwards, either in future instructions or in a
    // safepoint for the reusing instruction. This is impossible to satisfy
    // without copying the input.
    //
    // It may or may not be better to split the interval at the point of the
    // definition, which may permit grouping. One case where it is definitely
    // better to split is if the input never has any register uses after the
    // instruction. Handle this splitting eagerly.

    if (usedReg.numIntervals() != 1 ||
        (usedReg.def()->isPreset() && !usedReg.def()->output()->isRegister())) {
        reg.setMustCopyInput();
        return true;
    }
    LiveInterval *interval = usedReg.getInterval(0);
    LBlock *block = insData[reg.ins()].block();

    // The input's lifetime must end within the same block as the definition,
    // otherwise it could live on in phis elsewhere.
    if (interval->end() > outputOf(block->lastId())) {
        reg.setMustCopyInput();
        return true;
    }

    for (UsePositionIterator iter = interval->usesBegin(); iter != interval->usesEnd(); iter++) {
        if (iter->pos <= inputOf(reg.ins()))
            continue;

        LUse *use = iter->use;
        if (FindReusingDefinition(insData[iter->pos].ins(), use)) {
            reg.setMustCopyInput();
            return true;
        }
        if (use->policy() != LUse::ANY && use->policy() != LUse::KEEPALIVE) {
            reg.setMustCopyInput();
            return true;
        }
    }

    LiveInterval *preInterval = new LiveInterval(interval->vreg(), 0);
    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);
        JS_ASSERT(range->from <= inputOf(reg.ins()));

        CodePosition to = (range->to <= outputOf(reg.ins())) ? range->to : outputOf(reg.ins());
        if (!preInterval->addRange(range->from, to))
            return false;
    }

    LiveInterval *postInterval = new LiveInterval(interval->vreg(), 0);
    if (!postInterval->addRange(inputOf(reg.ins()), interval->end()))
        return false;

    LiveIntervalVector newIntervals;
    if (!newIntervals.append(preInterval) || !newIntervals.append(postInterval))
        return false;

    if (!distributeUses(interval, newIntervals) || !split(interval, newIntervals))
        return false;

    JS_ASSERT(usedReg.numIntervals() == 2);

    usedReg.setCanonicalSpillExclude(inputOf(reg.ins()));

    return tryGroupRegisters(use, def);
}

bool
BacktrackingAllocator::groupAndQueueRegisters()
{
    // Try to group registers with their reused inputs.
    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        BacktrackingVirtualRegister &reg = vregs[i];
        if (!reg.numIntervals())
            continue;

        if (reg.def()->policy() == LDefinition::MUST_REUSE_INPUT) {
            LUse *use = reg.ins()->getOperand(reg.def()->getReusedInput())->toUse();
            if (!tryGroupReusedRegister(i, use->virtualRegister()))
                return false;
        }
    }

    // Try to group phis with their inputs.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        LBlock *block = graph.getBlock(i);
        for (size_t j = 0; j < block->numPhis(); j++) {
            LPhi *phi = block->getPhi(j);
            uint32_t output = phi->getDef(0)->virtualRegister();
            for (size_t k = 0; k < phi->numOperands(); k++) {
                uint32_t input = phi->getOperand(k)->toUse()->virtualRegister();
                if (!tryGroupRegisters(input, output))
                    return false;
            }
        }
    }

    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        if (mir->shouldCancel("Backtracking Enqueue Registers"))
            return false;

        BacktrackingVirtualRegister &reg = vregs[i];
        JS_ASSERT(reg.numIntervals() <= 2);
        JS_ASSERT(!reg.canonicalSpill());

        if (!reg.numIntervals())
            continue;

        // Eagerly set the canonical spill slot for registers which are preset
        // for that slot, and reuse it for other registers in the group.
        LDefinition *def = reg.def();
        if (def->policy() == LDefinition::PRESET && !def->output()->isRegister()) {
            reg.setCanonicalSpill(*def->output());
            if (reg.group() && reg.group()->spill.isUse())
                reg.group()->spill = *def->output();
        }

        // Place all intervals for this register on the allocation queue.
        // During initial queueing use single queue items for groups of
        // registers, so that they will be allocated together and reduce the
        // risk of unnecessary conflicts. This is in keeping with the idea that
        // register groups are effectively single registers whose value changes
        // during execution. If any intervals in the group are evicted later
        // then they will be reallocated individually.
        size_t start = 0;
        if (VirtualRegisterGroup *group = reg.group()) {
            if (i == group->canonicalReg()) {
                size_t priority = computePriority(group);
                if (!allocationQueue.insert(QueueItem(group, priority)))
                    return false;
            }
            start++;
        }
        for (; start < reg.numIntervals(); start++) {
            LiveInterval *interval = reg.getInterval(start);
            if (interval->numRanges() > 0) {
                size_t priority = computePriority(interval);
                if (!allocationQueue.insert(QueueItem(interval, priority)))
                    return false;
            }
        }
    }

    return true;
}

static const size_t MAX_ATTEMPTS = 2;

bool
BacktrackingAllocator::processInterval(LiveInterval *interval)
{
    if (IonSpewEnabled(IonSpew_RegAlloc)) {
        IonSpew(IonSpew_RegAlloc, "Allocating v%u [priority %lu] [weight %lu]: %s",
                interval->vreg(), computePriority(interval), computeSpillWeight(interval),
                IntervalString(interval));
    }

    // An interval can be processed by doing any of the following:
    //
    // - Assigning the interval a register. The interval cannot overlap any
    //   other interval allocated for that physical register.
    //
    // - Spilling the interval, provided it has no register uses.
    //
    // - Splitting the interval into two or more intervals which cover the
    //   original one. The new intervals are placed back onto the priority
    //   queue for later processing.
    //
    // - Evicting one or more existing allocated intervals, and then doing one
    //   of the above operations. Evicted intervals are placed back on the
    //   priority queue. Any evicted intervals must have a lower spill weight
    //   than the interval being processed.
    //
    // As long as this structure is followed, termination is guaranteed.
    // In general, we want to minimize the amount of interval splitting
    // (which generally necessitates spills), so allocate longer lived, lower
    // weight intervals first and evict and split them later if they prevent
    // allocation for higher weight intervals.

    bool canAllocate = setIntervalRequirement(interval);

    bool fixed;
    LiveInterval *conflict;
    for (size_t attempt = 0;; attempt++) {
        if (canAllocate) {
            // Spill intervals which are required to be in a certain stack slot.
            if (interval->requirement()->kind() == Requirement::FIXED &&
                !interval->requirement()->allocation().isRegister())
            {
                IonSpew(IonSpew_RegAlloc, "stack allocation requirement");
                interval->setAllocation(interval->requirement()->allocation());
                return true;
            }

            fixed = false;
            conflict = NULL;

            // If we want, but do not require an interval to be in a specific
            // register, only look at that register for allocating and evict
            // or spill if it is not available. Picking a separate register may
            // be even worse than spilling, as it will still necessitate moves
            // and will tie up more registers than if we spilled.
            if (interval->hint()->kind() == Requirement::FIXED) {
                AnyRegister reg = interval->hint()->allocation().toRegister();
                bool success;
                if (!tryAllocateRegister(registers[reg.code()], interval, &success, &fixed, &conflict))
                    return false;
                if (success)
                    return true;
            }

            // Spill intervals which have no hint or register requirement.
            if (interval->requirement()->kind() == Requirement::NONE) {
                spill(interval);
                return true;
            }

            if (!conflict || minimalInterval(interval)) {
                // Search for any available register which the interval can be
                // allocated to.
                for (size_t i = 0; i < AnyRegister::Total; i++) {
                    bool success;
                    if (!tryAllocateRegister(registers[i], interval, &success, &fixed, &conflict))
                        return false;
                    if (success)
                        return true;
                }
            }
        }

        // Failed to allocate a register for this interval.

        if (attempt < MAX_ATTEMPTS &&
            canAllocate &&
            !fixed &&
            conflict &&
            computeSpillWeight(conflict) < computeSpillWeight(interval))
        {
            if (!evictInterval(conflict))
                return false;
            continue;
        }

        // A minimal interval cannot be split any further. If we try to split
        // it at this point we will just end up with the same interval and will
        // enter an infinite loop. Weights and the initial live intervals must
        // be constructed so that any minimal interval is allocatable.
        JS_ASSERT(!minimalInterval(interval));

        if (canAllocate && fixed)
            return splitAcrossCalls(interval);
        return chooseIntervalSplit(interval);
    }
}

bool
BacktrackingAllocator::processGroup(VirtualRegisterGroup *group)
{
    if (IonSpewEnabled(IonSpew_RegAlloc)) {
        IonSpew(IonSpew_RegAlloc, "Allocating group v%u [priority %lu] [weight %lu]",
                group->registers[0], computePriority(group), computeSpillWeight(group));
    }

    bool fixed;
    LiveInterval *conflict;
    for (size_t attempt = 0;; attempt++) {
        // Search for any available register which the group can be allocated to.
        fixed = false;
        conflict = NULL;
        for (size_t i = 0; i < AnyRegister::Total; i++) {
            bool success;
            if (!tryAllocateGroupRegister(registers[i], group, &success, &fixed, &conflict))
                return false;
            if (success) {
                conflict = NULL;
                break;
            }
        }

        if (attempt < MAX_ATTEMPTS &&
            !fixed &&
            conflict &&
            conflict->hasVreg() &&
            computeSpillWeight(conflict) < computeSpillWeight(group))
        {
            if (!evictInterval(conflict))
                return false;
            continue;
        }

        for (size_t i = 0; i < group->registers.length(); i++) {
            VirtualRegister &reg = vregs[group->registers[i]];
            JS_ASSERT(reg.numIntervals() <= 2);
            if (!processInterval(reg.getInterval(0)))
                return false;
        }

        return true;
    }
}

bool
BacktrackingAllocator::setIntervalRequirement(LiveInterval *interval)
{
    // Set any requirement or hint on interval according to its definition and
    // uses. Return false if there are conflicting requirements which will
    // require the interval to be split.
    interval->setHint(Requirement());
    interval->setRequirement(Requirement());

    BacktrackingVirtualRegister *reg = &vregs[interval->vreg()];

    // Set a hint if another interval in the same group is in a register.
    if (VirtualRegisterGroup *group = reg->group()) {
        if (group->allocation.isRegister())
            interval->setHint(Requirement(group->allocation));
    }

    if (interval->index() == 0) {
        // The first interval is the definition, so deal with any definition
        // constraints/hints.

        LDefinition::Policy policy = reg->def()->policy();
        if (policy == LDefinition::PRESET) {
            // Preset policies get a FIXED requirement.
            interval->setRequirement(Requirement(*reg->def()->output()));
        } else if (reg->ins()->isPhi()) {
            // Phis don't have any requirements, but they should prefer their
            // input allocations. This is captured by the group hints above.
        } else {
            // Non-phis get a REGISTER requirement.
            interval->setRequirement(Requirement(Requirement::REGISTER));
        }
    }

    // Search uses for requirements.
    for (UsePositionIterator iter = interval->usesBegin();
         iter != interval->usesEnd();
         iter++)
    {
        LUse::Policy policy = iter->use->policy();
        if (policy == LUse::FIXED) {
            AnyRegister required = GetFixedRegister(reg->def(), iter->use);

            // If there are multiple fixed registers which the interval is
            // required to use, fail. The interval will need to be split before
            // it can be allocated.
            if (!interval->addRequirement(Requirement(LAllocation(required))))
                return false;
        } else if (policy == LUse::REGISTER) {
            if (!interval->addRequirement(Requirement(Requirement::REGISTER)))
                return false;
        }
    }

    return true;
}

bool
BacktrackingAllocator::tryAllocateGroupRegister(PhysicalRegister &r, VirtualRegisterGroup *group,
                                                bool *psuccess, bool *pfixed, LiveInterval **pconflicting)
{
    *psuccess = false;

    if (!r.allocatable)
        return true;

    if (r.reg.isFloat() != vregs[group->registers[0]].isDouble())
        return true;

    bool allocatable = true;
    LiveInterval *conflicting = NULL;

    for (size_t i = 0; i < group->registers.length(); i++) {
        VirtualRegister &reg = vregs[group->registers[i]];
        JS_ASSERT(reg.numIntervals() <= 2);
        LiveInterval *interval = reg.getInterval(0);

        for (size_t j = 0; j < interval->numRanges(); j++) {
            AllocatedRange range(interval, interval->getRange(j)), existing;
            if (r.allocations.contains(range, &existing)) {
                if (conflicting) {
                    if (conflicting != existing.interval)
                        return true;
                } else {
                    conflicting = existing.interval;
                }
                allocatable = false;
            }
        }
    }

    if (!allocatable) {
        JS_ASSERT(conflicting);
        if (!*pconflicting || computeSpillWeight(conflicting) < computeSpillWeight(*pconflicting))
            *pconflicting = conflicting;
        if (!conflicting->hasVreg())
            *pfixed = true;
        return true;
    }

    *psuccess = true;

    group->allocation = LAllocation(r.reg);
    return true;
}

bool
BacktrackingAllocator::tryAllocateRegister(PhysicalRegister &r, LiveInterval *interval,
                                           bool *success, bool *pfixed, LiveInterval **pconflicting)
{
    *success = false;

    if (!r.allocatable)
        return true;

    BacktrackingVirtualRegister *reg = &vregs[interval->vreg()];
    if (reg->isDouble() != r.reg.isFloat())
        return true;

    if (interval->requirement()->kind() == Requirement::FIXED) {
        if (interval->requirement()->allocation() != LAllocation(r.reg)) {
            IonSpew(IonSpew_RegAlloc, "%s does not match fixed requirement", r.reg.name());
            return true;
        }
    }

    for (size_t i = 0; i < interval->numRanges(); i++) {
        AllocatedRange range(interval, interval->getRange(i)), existing;
        if (r.allocations.contains(range, &existing)) {
            if (existing.interval->hasVreg()) {
                if (IonSpewEnabled(IonSpew_RegAlloc)) {
                    IonSpew(IonSpew_RegAlloc, "%s collides with v%u [%u,%u> [weight %lu]",
                            r.reg.name(), existing.interval->vreg(),
                            existing.range->from.pos(), existing.range->to.pos(),
                            computeSpillWeight(existing.interval));
                }
                if (!*pconflicting || computeSpillWeight(existing.interval) < computeSpillWeight(*pconflicting))
                    *pconflicting = existing.interval;
            } else {
                if (IonSpewEnabled(IonSpew_RegAlloc)) {
                    IonSpew(IonSpew_RegAlloc, "%s collides with fixed use [%u,%u>",
                            r.reg.name(), existing.range->from.pos(), existing.range->to.pos());
                }
                *pfixed = true;
            }
            return true;
        }
    }

    IonSpew(IonSpew_RegAlloc, "allocated to %s", r.reg.name());

    for (size_t i = 0; i < interval->numRanges(); i++) {
        AllocatedRange range(interval, interval->getRange(i));
        if (!r.allocations.insert(range))
            return false;
    }

    // Set any register hint for allocating other intervals in the same group.
    if (VirtualRegisterGroup *group = reg->group()) {
        if (!group->allocation.isRegister())
            group->allocation = LAllocation(r.reg);
    }

    interval->setAllocation(LAllocation(r.reg));
    *success = true;
    return true;
}

bool
BacktrackingAllocator::evictInterval(LiveInterval *interval)
{
    if (IonSpewEnabled(IonSpew_RegAlloc)) {
        IonSpew(IonSpew_RegAlloc, "Evicting interval v%u: %s",
                interval->vreg(), IntervalString(interval));
    }

    JS_ASSERT(interval->getAllocation()->isRegister());

    AnyRegister reg(interval->getAllocation()->toRegister());
    PhysicalRegister &physical = registers[reg.code()];
    JS_ASSERT(physical.reg == reg && physical.allocatable);

    for (size_t i = 0; i < interval->numRanges(); i++) {
        AllocatedRange range(interval, interval->getRange(i));
        physical.allocations.remove(range);
    }

    interval->setAllocation(LAllocation());

    size_t priority = computePriority(interval);
    return allocationQueue.insert(QueueItem(interval, priority));
}

bool
BacktrackingAllocator::distributeUses(LiveInterval *interval,
                                      const LiveIntervalVector &newIntervals)
{
    JS_ASSERT(newIntervals.length() >= 2);

    // Simple redistribution of uses from an old interval to a set of new
    // intervals. Intervals are permitted to overlap, in which case this will
    // assign uses in the overlapping section to the interval with the latest
    // start position.
    for (UsePositionIterator iter(interval->usesBegin());
         iter != interval->usesEnd();
         iter++)
    {
        CodePosition pos = iter->pos;
        LiveInterval *addInterval = NULL;
        for (size_t i = 0; i < newIntervals.length(); i++) {
            LiveInterval *newInterval = newIntervals[i];
            if (newInterval->covers(pos)) {
                if (!addInterval || newInterval->start() < addInterval->start())
                    addInterval = newInterval;
            }
        }
        addInterval->addUse(new UsePosition(iter->use, iter->pos));
    }

    return true;
}

bool
BacktrackingAllocator::split(LiveInterval *interval,
                             const LiveIntervalVector &newIntervals)
{
    JS_ASSERT(newIntervals.length() >= 2);

    if (IonSpewEnabled(IonSpew_RegAlloc)) {
        IonSpew(IonSpew_RegAlloc, "splitting interval v%u %s:",
                interval->vreg(), IntervalString(interval));
        for (size_t i = 0; i < newIntervals.length(); i++)
            IonSpew(IonSpew_RegAlloc, "    %s", IntervalString(newIntervals[i]));
    }

    // Find the earliest interval in the new list.
    LiveInterval *first = newIntervals[0];
    for (size_t i = 1; i < newIntervals.length(); i++) {
        if (newIntervals[i]->start() < first->start())
            first = newIntervals[i];
    }

    // Replace the old interval in the virtual register's state with the new intervals.
    VirtualRegister *reg = &vregs[interval->vreg()];
    reg->replaceInterval(interval, first);
    for (size_t i = 0; i < newIntervals.length(); i++) {
        if (newIntervals[i] != first && !reg->addInterval(newIntervals[i]))
            return false;
    }

    return true;
}

bool BacktrackingAllocator::requeueIntervals(const LiveIntervalVector &newIntervals)
{
    // Queue the new intervals for register assignment.
    for (size_t i = 0; i < newIntervals.length(); i++) {
        LiveInterval *newInterval = newIntervals[i];
        size_t priority = computePriority(newInterval);
        if (!allocationQueue.insert(QueueItem(newInterval, priority)))
            return false;
    }
    return true;
}

void
BacktrackingAllocator::spill(LiveInterval *interval)
{
    IonSpew(IonSpew_RegAlloc, "Spilling interval");

    JS_ASSERT(interval->requirement()->kind() == Requirement::NONE);

    // We can't spill bogus intervals.
    JS_ASSERT(interval->hasVreg());

    BacktrackingVirtualRegister *reg = &vregs[interval->vreg()];

    bool useCanonical = !reg->hasCanonicalSpillExclude()
        || interval->start() < reg->canonicalSpillExclude();

    if (useCanonical) {
        if (reg->canonicalSpill()) {
            IonSpew(IonSpew_RegAlloc, "  Picked canonical spill location %s",
                    reg->canonicalSpill()->toString());
            interval->setAllocation(*reg->canonicalSpill());
            return;
        }

        if (reg->group() && !reg->group()->spill.isUse()) {
            IonSpew(IonSpew_RegAlloc, "  Reusing group spill location %s",
                    reg->group()->spill.toString());
            interval->setAllocation(reg->group()->spill);
            reg->setCanonicalSpill(reg->group()->spill);
            return;
        }
    }

    uint32_t stackSlot;
    if (reg->isDouble())
        stackSlot = stackSlotAllocator.allocateDoubleSlot();
    else
        stackSlot = stackSlotAllocator.allocateSlot();
    JS_ASSERT(stackSlot <= stackSlotAllocator.stackHeight());

    LStackSlot alloc(stackSlot, reg->isDouble());
    interval->setAllocation(alloc);

    IonSpew(IonSpew_RegAlloc, "  Allocating spill location %s", alloc.toString());

    if (useCanonical) {
        reg->setCanonicalSpill(alloc);
        if (reg->group())
            reg->group()->spill = alloc;
    }
}

// Add moves to resolve conflicting assignments between a block and its
// predecessors. XXX try to common this with LinearScanAllocator.
bool
BacktrackingAllocator::resolveControlFlow()
{
    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        BacktrackingVirtualRegister *reg = &vregs[i];

        if (mir->shouldCancel("Backtracking Resolve Control Flow (vreg loop)"))
            return false;

        for (size_t j = 1; j < reg->numIntervals(); j++) {
            LiveInterval *interval = reg->getInterval(j);
            JS_ASSERT(interval->index() == j);

            bool skip = false;
            for (int k = j - 1; k >= 0; k--) {
                LiveInterval *prevInterval = reg->getInterval(k);
                if (prevInterval->start() != interval->start())
                    break;
                if (*prevInterval->getAllocation() == *interval->getAllocation()) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;

            CodePosition start = interval->start();
            InstructionData *data = &insData[start];
            if (interval->start() > inputOf(data->block()->firstId())) {
                JS_ASSERT(start == inputOf(data->ins()) || start == outputOf(data->ins()));

                LiveInterval *prevInterval = reg->intervalFor(start.previous());
                if (start.subpos() == CodePosition::INPUT) {
                    if (!moveInput(inputOf(data->ins()), prevInterval, interval))
                        return false;
                } else {
                    if (!moveAfter(outputOf(data->ins()), prevInterval, interval))
                        return false;
                }
            }
        }
    }

    for (size_t i = 0; i < graph.numBlocks(); i++) {
        if (mir->shouldCancel("Backtracking Resolve Control Flow (block loop)"))
            return false;

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
        }

        // Resolve split intervals with moves
        BitSet *live = liveIn[mSuccessor->id()];

        for (BitSet::Iterator liveRegId(*live); liveRegId; liveRegId++) {
            VirtualRegister &reg = vregs[*liveRegId];

            for (size_t j = 0; j < mSuccessor->numPredecessors(); j++) {
                LBlock *predecessor = mSuccessor->getPredecessor(j)->lir();

                for (size_t k = 0; k < reg.numIntervals(); k++) {
                    LiveInterval *to = reg.getInterval(k);
                    if (!to->covers(inputOf(successor->firstId())))
                        continue;
                    if (to->covers(outputOf(predecessor->lastId())))
                        continue;

                    LiveInterval *from = reg.intervalFor(outputOf(predecessor->lastId()));

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
    }

    return true;
}

bool
BacktrackingAllocator::isReusedInput(LUse *use, LInstruction *ins, bool considerCopy)
{
    if (LDefinition *def = FindReusingDefinition(ins, use))
        return considerCopy || !vregs[def->virtualRegister()].mustCopyInput();
    return false;
}

bool
BacktrackingAllocator::isRegisterUse(LUse *use, LInstruction *ins)
{
    switch (use->policy()) {
      case LUse::ANY:
        return isReusedInput(use, ins);

      case LUse::REGISTER:
      case LUse::FIXED:
        return true;

      default:
        return false;
    }
}

bool
BacktrackingAllocator::isRegisterDefinition(LiveInterval *interval)
{
    if (interval->index() != 0)
        return false;

    VirtualRegister &reg = vregs[interval->vreg()];
    if (reg.ins()->isPhi())
        return false;

    if (reg.def()->policy() == LDefinition::PRESET && !reg.def()->output()->isRegister())
        return false;

    return true;
}

bool
BacktrackingAllocator::reifyAllocations()
{
    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        VirtualRegister *reg = &vregs[i];

        if (mir->shouldCancel("Backtracking Reify Allocations (main loop)"))
            return false;

        for (size_t j = 0; j < reg->numIntervals(); j++) {
            LiveInterval *interval = reg->getInterval(j);
            JS_ASSERT(interval->index() == j);

            if (interval->index() == 0) {
                reg->def()->setOutput(*interval->getAllocation());
                if (reg->ins()->recoversInput()) {
                    LSnapshot *snapshot = reg->ins()->snapshot();
                    for (size_t i = 0; i < snapshot->numEntries(); i++) {
                        LAllocation *entry = snapshot->getEntry(i);
                        if (entry->isUse() && entry->toUse()->policy() == LUse::RECOVERED_INPUT)
                            *entry = *reg->def()->output();
                    }
                }
            }

            for (UsePositionIterator iter(interval->usesBegin());
                 iter != interval->usesEnd();
                 iter++)
            {
                LAllocation *alloc = iter->use;
                *alloc = *interval->getAllocation();

                // For any uses which feed into MUST_REUSE_INPUT definitions,
                // add copies if the use and def have different allocations.
                LInstruction *ins = insData[iter->pos].ins();
                if (LDefinition *def = FindReusingDefinition(ins, alloc)) {
                    LiveInterval *outputInterval =
                        vregs[def->virtualRegister()].intervalFor(outputOf(ins));
                    LAllocation *res = outputInterval->getAllocation();
                    LAllocation *sourceAlloc = interval->getAllocation();

                    if (*res != *alloc) {
                        LMoveGroup *group = getInputMoveGroup(inputOf(ins));
                        if (!group->addAfter(sourceAlloc, res))
                            return false;
                        *alloc = *res;
                    }
                }
            }

            addLiveRegistersForInterval(reg, interval);
        }
    }

    graph.setLocalSlotCount(stackSlotAllocator.stackHeight());
    return true;
}

bool
BacktrackingAllocator::populateSafepoints()
{
    size_t firstSafepoint = 0;

    for (uint32_t i = 0; i < vregs.numVirtualRegisters(); i++) {
        BacktrackingVirtualRegister *reg = &vregs[i];

        if (!reg->def() || (!IsTraceable(reg) && !IsNunbox(reg)))
            continue;

        firstSafepoint = findFirstSafepoint(reg->getInterval(0), firstSafepoint);
        if (firstSafepoint >= graph.numSafepoints())
            break;

        // Find the furthest endpoint.
        CodePosition end = reg->getInterval(0)->end();
        for (size_t j = 1; j < reg->numIntervals(); j++)
            end = Max(end, reg->getInterval(j)->end());

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

            for (size_t k = 0; k < reg->numIntervals(); k++) {
                LiveInterval *interval = reg->getInterval(k);
                if (!interval->covers(inputOf(ins)))
                    continue;

                LAllocation *a = interval->getAllocation();
                if (a->isGeneralReg() && ins->isCall())
                    continue;

                switch (reg->type()) {
                  case LDefinition::OBJECT:
                    safepoint->addGcPointer(*a);
                    break;
#ifdef JS_NUNBOX32
                  case LDefinition::TYPE:
                    safepoint->addNunboxType(i, *a);
                    break;
                  case LDefinition::PAYLOAD:
                    safepoint->addNunboxPayload(i, *a);
                    break;
#else
                  case LDefinition::BOX:
                    safepoint->addBoxedValue(*a);
                    break;
#endif
                  default:
                    JS_NOT_REACHED("Bad register type");
                }
            }
        }
    }

    return true;
}

void
BacktrackingAllocator::dumpRegisterGroups()
{
    printf("Register groups:\n");
    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        VirtualRegisterGroup *group = vregs[i].group();
        if (group && i == group->canonicalReg()) {
            for (size_t j = 0; j < group->registers.length(); j++)
                printf(" v%u", group->registers[j]);
            printf("\n");
        }
    }
}

void
BacktrackingAllocator::dumpLiveness()
{
#ifdef DEBUG
    printf("Virtual Registers:\n");

    for (size_t blockIndex = 0; blockIndex < graph.numBlocks(); blockIndex++) {
        LBlock *block = graph.getBlock(blockIndex);
        MBasicBlock *mir = block->mir();

        printf("\nBlock %lu", blockIndex);
        for (size_t i = 0; i < mir->numSuccessors(); i++)
            printf(" [successor %u]", mir->getSuccessor(i)->id());
        printf("\n");

        for (size_t i = 0; i < block->numPhis(); i++) {
            LPhi *phi = block->getPhi(i);

            printf("Phi v%u <-", phi->getDef(0)->virtualRegister());
            for (size_t j = 0; j < phi->numOperands(); j++)
                printf(" v%u", phi->getOperand(j)->toUse()->virtualRegister());
            printf("\n");
        }

        for (LInstructionIterator iter = block->begin(); iter != block->end(); iter++) {
            LInstruction *ins = *iter;

            printf("[%u,%u %s]", inputOf(ins).pos(), outputOf(ins).pos(), ins->opName());

            for (size_t i = 0; i < ins->numTemps(); i++) {
                LDefinition *temp = ins->getTemp(i);
                if (!temp->isBogusTemp())
                    printf(" [temp v%u]", temp->virtualRegister());
            }

            for (size_t i = 0; i < ins->numDefs(); i++) {
                LDefinition *def = ins->getDef(i);
                printf(" [def v%u]", def->virtualRegister());
            }

            for (LInstruction::InputIterator alloc(*ins); alloc.more(); alloc.next()) {
                if (alloc->isUse())
                    printf(" [use v%u]", alloc->toUse()->virtualRegister());
            }

            printf("\n");
        }
    }

    printf("\nLive Ranges:\n\n");

    for (size_t i = 0; i < AnyRegister::Total; i++)
        printf("reg %s: %s\n", AnyRegister::FromCode(i).name(), IntervalString(fixedIntervals[i]));

    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        printf("v%lu:", i);
        VirtualRegister &vreg = vregs[i];
        for (size_t j = 0; j < vreg.numIntervals(); j++) {
            if (j)
                printf(" *");
            printf("%s", IntervalString(vreg.getInterval(j)));
        }
        printf("\n");
    }

    printf("\n");
#endif // DEBUG
}

#ifdef DEBUG
struct BacktrackingAllocator::PrintLiveIntervalRange
{
    void operator()(const AllocatedRange &item)
    {
        if (item.range == item.interval->getRange(0)) {
            printf("  v%u: %s\n",
                   item.interval->hasVreg() ? item.interval->vreg() : 0,
                   IntervalString(item.interval));
        }
    }
};
#endif

void
BacktrackingAllocator::dumpAllocations()
{
#ifdef DEBUG
    printf("Allocations:\n");

    for (size_t i = 0; i < graph.numVirtualRegisters(); i++) {
        printf("v%lu:", i);
        VirtualRegister &vreg = vregs[i];
        for (size_t j = 0; j < vreg.numIntervals(); j++) {
            if (j)
                printf(" *");
            LiveInterval *interval = vreg.getInterval(j);
            printf("%s :: %s", IntervalString(interval), interval->getAllocation()->toString());
        }
        printf("\n");
    }

    printf("\n");

    for (size_t i = 0; i < AnyRegister::Total; i++) {
        printf("reg %s:\n", AnyRegister::FromCode(i).name());
        registers[i].allocations.forEach(PrintLiveIntervalRange());
    }

    printf("\n");
#endif // DEBUG
}

bool
BacktrackingAllocator::addLiveInterval(LiveIntervalVector &intervals, uint32_t vreg,
                                       CodePosition from, CodePosition to)
{
    LiveInterval *interval = new LiveInterval(vreg, 0);
    return interval->addRange(from, to) && intervals.append(interval);
}

///////////////////////////////////////////////////////////////////////////////
// Heuristic Methods
///////////////////////////////////////////////////////////////////////////////

size_t
BacktrackingAllocator::computePriority(const LiveInterval *interval)
{
    // The priority of an interval is its total length, so that longer lived
    // intervals will be processed before shorter ones (even if the longer ones
    // have a low spill weight). See processInterval().
    size_t lifetimeTotal = 0;

    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);
        lifetimeTotal += range->to.pos() - range->from.pos();
    }

    return lifetimeTotal;
}

size_t
BacktrackingAllocator::computePriority(const VirtualRegisterGroup *group)
{
    size_t priority = 0;
    for (size_t j = 0; j < group->registers.length(); j++) {
        uint32_t vreg = group->registers[j];
        priority += computePriority(vregs[vreg].getInterval(0));
    }
    return priority;
}

CodePosition
BacktrackingAllocator::minimalDefEnd(LInstruction *ins)
{
    // Compute the shortest interval that captures vregs defined by ins.
    // Watch for instructions that are followed by an OSI point and/or Nop.
    // If moves are introduced between the instruction and the OSI point then
    // safepoint information for the instruction may be incorrect. This is
    // pretty disgusting and should be fixed somewhere else in the compiler.
    while (true) {
        LInstruction *next = insData[outputOf(ins).next()].ins();
        if (!next->isNop() && !next->isOsiPoint())
            break;
        ins = next;
    }
    return outputOf(ins);
}

bool
BacktrackingAllocator::minimalDef(const LiveInterval *interval, LInstruction *ins)
{
    // Whether interval is a minimal interval capturing a definition at ins.
    return (interval->end() <= minimalDefEnd(ins).next()) &&
        (interval->start() == inputOf(ins) || interval->start() == outputOf(ins));
}

bool
BacktrackingAllocator::minimalUse(const LiveInterval *interval, LInstruction *ins)
{
    // Whether interval is a minimal interval capturing a use at ins.
    return (interval->start() == inputOf(ins)) &&
        (interval->end() == outputOf(ins) || interval->end() == outputOf(ins).next());
}

bool
BacktrackingAllocator::minimalInterval(const LiveInterval *interval, bool *pfixed)
{
    if (!interval->hasVreg()) {
        *pfixed = true;
        return true;
    }

    if (interval->index() == 0) {
        VirtualRegister &reg = vregs[interval->vreg()];
        if (pfixed)
            *pfixed = reg.def()->policy() == LDefinition::PRESET && reg.def()->output()->isRegister();
        return minimalDef(interval, reg.ins());
    }

    for (UsePositionIterator iter = interval->usesBegin(); iter != interval->usesEnd(); iter++) {
        LUse *use = iter->use;

        switch (use->policy()) {
          case LUse::FIXED:
            if (pfixed)
                *pfixed = true;
            return minimalUse(interval, insData[iter->pos].ins());

          case LUse::REGISTER:
            if (pfixed)
                *pfixed = false;
            return minimalUse(interval, insData[iter->pos].ins());

          default:
            break;
        }
    }

    return false;
}

size_t
BacktrackingAllocator::computeSpillWeight(const LiveInterval *interval)
{
    // Minimal intervals have an extremely high spill weight, to ensure they
    // can evict any other intervals and be allocated to a register.
    bool fixed;
    if (minimalInterval(interval, &fixed))
        return fixed ? 2000000 : 1000000;

    size_t usesTotal = 0;

    if (interval->index() == 0) {
        VirtualRegister *reg = &vregs[interval->vreg()];
        if (reg->def()->policy() == LDefinition::PRESET && reg->def()->output()->isRegister())
            usesTotal += 2000;
        else if (!reg->ins()->isPhi())
            usesTotal += 2000;
    }

    for (UsePositionIterator iter = interval->usesBegin(); iter != interval->usesEnd(); iter++) {
        LUse *use = iter->use;

        switch (use->policy()) {
          case LUse::ANY:
            usesTotal += 1000;
            break;

          case LUse::REGISTER:
          case LUse::FIXED:
            usesTotal += 2000;
            break;

          case LUse::KEEPALIVE:
            break;

          default:
            // Note: RECOVERED_INPUT will not appear in UsePositionIterator.
            JS_NOT_REACHED("Bad use");
        }
    }

    // Intervals for registers in groups get higher weights.
    if (interval->hint()->kind() != Requirement::NONE)
        usesTotal += 2000;

    // Compute spill weight as a use density, lowering the weight for long
    // lived intervals with relatively few uses.
    size_t lifetimeTotal = computePriority(interval);
    return lifetimeTotal ? usesTotal / lifetimeTotal : 0;
}

size_t
BacktrackingAllocator::computeSpillWeight(const VirtualRegisterGroup *group)
{
    size_t maxWeight = 0;
    for (size_t j = 0; j < group->registers.length(); j++) {
        uint32_t vreg = group->registers[j];
        maxWeight = Max(maxWeight, computeSpillWeight(vregs[vreg].getInterval(0)));
    }
    return maxWeight;
}

bool
BacktrackingAllocator::trySplitAcrossHotcode(LiveInterval *interval, bool *success)
{
    // If this interval has portions that are hot and portions that are cold,
    // split it at the boundaries between hot and cold code.

    const LiveInterval::Range *hotRange = NULL;

    for (size_t i = 0; i < interval->numRanges(); i++) {
        AllocatedRange range(interval, interval->getRange(i)), existing;
        if (hotcode.contains(range, &existing)) {
            hotRange = existing.range;
            break;
        }
    }

    // Don't split if there is no hot code in the interval.
    if (!hotRange)
        return true;

    // Don't split if there is no cold code in the interval.
    bool coldCode = false;
    for (size_t i = 0; i < interval->numRanges(); i++) {
        if (!hotRange->contains(interval->getRange(i))) {
            coldCode = true;
            break;
        }
    }
    if (!coldCode)
        return true;

    LiveInterval *hotInterval = new LiveInterval(interval->vreg(), 0);
    LiveInterval *preInterval = NULL, *postInterval = NULL;

    // Accumulate the ranges of hot and cold code in the interval. Note that
    // we are only comparing with the single hot range found, so the cold code
    // may contain separate hot ranges.
    Vector<LiveInterval::Range, 1, SystemAllocPolicy> hotList, coldList;
    for (size_t i = 0; i < interval->numRanges(); i++) {
        LiveInterval::Range hot, coldPre, coldPost;
        interval->getRange(i)->intersect(hotRange, &coldPre, &hot, &coldPost);

        if (!hot.empty() && !hotInterval->addRange(hot.from, hot.to))
            return false;

        if (!coldPre.empty()) {
            if (!preInterval)
                preInterval = new LiveInterval(interval->vreg(), 0);
            if (!preInterval->addRange(coldPre.from, coldPre.to))
                return false;
        }

        if (!coldPost.empty()) {
            if (!postInterval)
                postInterval = new LiveInterval(interval->vreg(), 0);
            if (!postInterval->addRange(coldPost.from, coldPost.to))
                return false;
        }
    }

    JS_ASSERT(preInterval || postInterval);
    JS_ASSERT(hotInterval->numRanges());

    LiveIntervalVector newIntervals;
    if (!newIntervals.append(hotInterval))
        return false;
    if (preInterval && !newIntervals.append(preInterval))
        return false;
    if (postInterval && !newIntervals.append(postInterval))
        return false;

    *success = true;
    return distributeUses(interval, newIntervals) &&
        split(interval, newIntervals) &&
        requeueIntervals(newIntervals);
}

bool
BacktrackingAllocator::trySplitAfterLastRegisterUse(LiveInterval *interval, bool *success)
{
    // If this interval's later uses do not require it to be in a register,
    // split it after the last use which does require a register.

    CodePosition lastRegisterFrom, lastRegisterTo, lastUse;

    for (UsePositionIterator iter(interval->usesBegin());
         iter != interval->usesEnd();
         iter++)
    {
        LUse *use = iter->use;
        LInstruction *ins = insData[iter->pos].ins();

        // Uses in the interval should be sorted.
        JS_ASSERT(iter->pos >= lastUse);
        lastUse = inputOf(ins);

        switch (use->policy()) {
          case LUse::ANY:
            if (isReusedInput(iter->use, ins, /* considerCopy = */ true)) {
                lastRegisterFrom = inputOf(ins);
                lastRegisterTo = iter->pos.next();
            }
            break;

          case LUse::REGISTER:
          case LUse::FIXED:
            lastRegisterFrom = inputOf(ins);
            lastRegisterTo = iter->pos.next();
            break;

          default:
            break;
        }
    }

    if (!lastRegisterFrom.pos() || lastRegisterFrom == lastUse) {
        // Can't trim non-register uses off the end by splitting.
        return true;
    }

    LiveInterval *preInterval = new LiveInterval(interval->vreg(), 0);
    LiveInterval *postInterval = new LiveInterval(interval->vreg(), 0);

    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);

        if (range->from < lastRegisterTo) {
            CodePosition to = (range->to <= lastRegisterTo) ? range->to : lastRegisterTo;
            if (!preInterval->addRange(range->from, to))
                return false;
        }

        if (lastRegisterFrom < range->to) {
            CodePosition from = (lastRegisterFrom <= range->from) ? range->from : lastRegisterFrom;
            if (!postInterval->addRange(from, range->to))
                return false;
        }
    }

    LiveIntervalVector newIntervals;
    if (!newIntervals.append(preInterval) || !newIntervals.append(postInterval))
        return false;

    *success = true;
    return distributeUses(interval, newIntervals) &&
        split(interval, newIntervals) &&
        requeueIntervals(newIntervals);
}

bool
BacktrackingAllocator::splitAtAllRegisterUses(LiveInterval *interval)
{
    // Split this interval so that all its register uses become minimal
    // intervals and allow the vreg to be spilled throughout its range.

    LiveIntervalVector newIntervals;
    uint32_t vreg = interval->vreg();

    CodePosition spillStart = interval->start();
    if (isRegisterDefinition(interval)) {
        // Treat the definition of the interval as a register use so that it
        // can be split and spilled ASAP.
        CodePosition from = interval->start();
        CodePosition to = minimalDefEnd(insData[from].ins()).next();
        if (!addLiveInterval(newIntervals, vreg, from, to))
            return false;
        spillStart = to;
    }

    LiveInterval *spillInterval = new LiveInterval(vreg, 0);
    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);
        CodePosition from = range->from < spillStart ? spillStart : range->from;
        if (!spillInterval->addRange(from, range->to))
            return false;
    }

    for (UsePositionIterator iter(interval->usesBegin());
         iter != interval->usesEnd();
         iter++)
    {
        LInstruction *ins = insData[iter->pos].ins();
        if (iter->pos < spillStart) {
            newIntervals.back()->addUse(new UsePosition(iter->use, iter->pos));
        } else if (isRegisterUse(iter->use, ins)) {
            // For register uses which are not useRegisterAtStart, pick an
            // interval that covers both the instruction's input and output, so
            // that the register is not reused for an output.
            CodePosition from = inputOf(ins);
            CodePosition to = iter->pos.next();

            // Watch for duplicate register use positions.
            if (newIntervals.empty() || newIntervals.back()->end() != to) {
                if (!addLiveInterval(newIntervals, vreg, from, to))
                    return false;
            }

            newIntervals.back()->addUse(new UsePosition(iter->use, iter->pos));
        } else {
            spillInterval->addUse(new UsePosition(iter->use, iter->pos));
        }
    }

    if (!newIntervals.append(spillInterval))
        return false;

    return split(interval, newIntervals) && requeueIntervals(newIntervals);
}

bool
BacktrackingAllocator::splitAcrossCalls(LiveInterval *interval)
{
    // Split the interval to separate register uses and non-register uses and
    // allow the vreg to be spilled across its range. Unlike splitAtAllRegisterUses,
    // consolidate any register uses which have no intervening calls into the
    // same resulting interval.

    // Don't spill the interval until after the end of its definition.
    CodePosition spillStart = interval->start();
    if (isRegisterDefinition(interval))
        spillStart = minimalDefEnd(insData[interval->start()].ins()).next();

    // Find the locations of all calls in the interval's range. Fixed intervals
    // are introduced by buildLivenessInfo only for calls when allocating for
    // the backtracking allocator.
    Vector<CodePosition, 4, SystemAllocPolicy> callPositions;
    for (size_t i = 0; i < fixedIntervalsUnion->numRanges(); i++) {
        const LiveInterval::Range *range = fixedIntervalsUnion->getRange(i);
        if (interval->covers(range->from) && spillStart < range->from) {
            if (!callPositions.append(range->from))
                return false;
        }
    }
    JS_ASSERT(callPositions.length());

    uint32_t vreg = interval->vreg();

    LiveInterval *spillInterval = new LiveInterval(vreg, 0);
    for (size_t i = 0; i < interval->numRanges(); i++) {
        const LiveInterval::Range *range = interval->getRange(i);
        CodePosition from = range->from < spillStart ? spillStart : range->from;
        if (!spillInterval->addRange(from, range->to))
            return false;
    }

    LiveIntervalVector newIntervals;

    CodePosition lastRegisterUse;
    if (spillStart != interval->start()) {
        LiveInterval *newInterval = new LiveInterval(vreg, 0);
        if (!newIntervals.append(newInterval))
            return false;
        lastRegisterUse = interval->start();
    }

    int activeCallPosition = callPositions.length() - 1;
    for (UsePositionIterator iter(interval->usesBegin()); iter != interval->usesEnd(); iter++) {
        LInstruction *ins = insData[iter->pos].ins();
        if (iter->pos < spillStart) {
            newIntervals.back()->addUse(new UsePosition(iter->use, iter->pos));
        } else if (isRegisterUse(iter->use, ins)) {
            bool useNewInterval = false;
            if (lastRegisterUse.pos() == 0) {
                useNewInterval = true;
            } else {
                // Place this register use into a different interval from the
                // last one if there are any calls between the two uses or if
                // the register uses are in different subranges of the original
                // interval.
                for (; activeCallPosition >= 0; activeCallPosition--) {
                    CodePosition pos = callPositions[activeCallPosition];
                    if (iter->pos < pos)
                        break;
                    if (lastRegisterUse < pos) {
                        useNewInterval = true;
                        break;
                    }
                }
                if (!useNewInterval) {
                    for (size_t i = 0; i < interval->numRanges(); i++) {
                        const LiveInterval::Range *range = interval->getRange(i);
                        if (range->from <= lastRegisterUse && range->to <= iter->pos) {
                            useNewInterval = true;
                            break;
                        }
                    }
                }
            }
            if (useNewInterval) {
                LiveInterval *newInterval = new LiveInterval(vreg, 0);
                if (!newIntervals.append(newInterval))
                    return false;
            }
            newIntervals.back()->addUse(new UsePosition(iter->use, iter->pos));
            lastRegisterUse = iter->pos;
        } else {
            spillInterval->addUse(new UsePosition(iter->use, iter->pos));
        }
    }

    // Compute ranges for each new interval that cover all its uses.
    for (size_t i = 0; i < newIntervals.length(); i++) {
        LiveInterval *newInterval = newIntervals[i];
        CodePosition start, end;
        if (i == 0 && spillStart != interval->start()) {
            start = interval->start();
            end = spillStart;
        } else {
            start = inputOf(insData[newInterval->usesBegin()->pos].ins());
        }
        for (UsePositionIterator iter(newInterval->usesBegin());
             iter != newInterval->usesEnd();
             iter++) {
            end = iter->pos.next();
        }
        if (!newInterval->addRange(start, end))
            return false;
    }

    if (!newIntervals.append(spillInterval))
        return false;

    return split(interval, newIntervals) && requeueIntervals(newIntervals);
}

bool
BacktrackingAllocator::chooseIntervalSplit(LiveInterval *interval)
{
    bool success = false;

    if (!trySplitAcrossHotcode(interval, &success))
        return false;
    if (success)
        return true;

    if (!trySplitAfterLastRegisterUse(interval, &success))
        return false;
    if (success)
        return true;

    return splitAtAllRegisterUses(interval);
}
