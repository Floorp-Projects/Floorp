/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_backtrackingallocator_h__
#define js_ion_backtrackingallocator_h__

#include "LiveRangeAllocator.h"

#include "ds/PriorityQueue.h"
#include "ds/SplayTree.h"

// Backtracking priority queue based register allocator based on that described
// in the following blog post:
//
// http://blog.llvm.org/2011/09/greedy-register-allocation-in-llvm-30.html

namespace js {
namespace ion {

// Information about a group of registers. Registers may be grouped together
// when (a) all of their lifetimes are disjoint, (b) they are of the same type
// (double / non-double) and (c) it is desirable that they have the same
// allocation.
struct VirtualRegisterGroup : public TempObject
{
    // All virtual registers in the group.
    Vector<uint32_t, 2, IonAllocPolicy> registers;

    // Desired physical register to use for registers in the group.
    LAllocation allocation;

    // Spill location to be shared by registers in the group.
    LAllocation spill;

    VirtualRegisterGroup()
      : allocation(LUse(0, LUse::ANY)), spill(LUse(0, LUse::ANY))
    {}
};

class BacktrackingVirtualRegister : public VirtualRegister
{
    // If this register's definition is MUST_REUSE_INPUT, whether a copy must
    // be introduced before the definition that relaxes the policy.
    bool mustCopyInput_;

    // Spill location to use for this register.
    LAllocation canonicalSpill_;

    // If this register is associated with a group of other registers,
    // information about the group. This structure is shared between all
    // registers in the group.
    VirtualRegisterGroup *group_;

  public:
    void setMustCopyInput() {
        mustCopyInput_ = true;
    }
    bool mustCopyInput() {
        return mustCopyInput_;
    }

    void setCanonicalSpill(LAllocation alloc) {
        canonicalSpill_ = alloc;
    }
    const LAllocation *canonicalSpill() const {
        return canonicalSpill_.isStackSlot() ? &canonicalSpill_ : NULL;
    }
    unsigned canonicalSpillSlot() const {
        return canonicalSpill_.toStackSlot()->slot();
    }

    void setGroup(VirtualRegisterGroup *group) {
        group_ = group;
    }
    VirtualRegisterGroup *group() {
        return group_;
    }
};

class BacktrackingAllocator : public LiveRangeAllocator<BacktrackingVirtualRegister>
{
    // Priority queue element: an interval and its immutable priority.
    struct QueuedInterval
    {
        LiveInterval *interval;

        QueuedInterval(LiveInterval *interval, size_t priority)
          : interval(interval), priority_(priority)
        {}

        static size_t priority(const QueuedInterval &v) {
            return v.priority_;
        }

      private:
        size_t priority_;
    };

    PriorityQueue<QueuedInterval, QueuedInterval, 0, SystemAllocPolicy> queuedIntervals;

    // A subrange over which a physical register is allocated.
    struct AllocatedRange {
        LiveInterval *interval;
        const LiveInterval::Range *range;

        AllocatedRange()
          : interval(NULL), range(NULL)
        {}

        AllocatedRange(LiveInterval *interval, const LiveInterval::Range *range)
          : interval(interval), range(range)
        {}

        static int compare(const AllocatedRange &v0, const AllocatedRange &v1) {
            // LiveInterval::Range includes 'from' but excludes 'to'.
            if (v0.range->to.pos() <= v1.range->from.pos())
                return -1;
            if (v0.range->from.pos() >= v1.range->to.pos())
                return 1;
            return 0;
        }
    };

    typedef SplayTree<AllocatedRange, AllocatedRange> AllocatedRangeSet;

    // Each physical register is associated with the set of ranges over which
    // that register is currently allocated.
    struct PhysicalRegister {
        bool allocatable;
        AnyRegister reg;
        AllocatedRangeSet allocations;

        PhysicalRegister() : allocatable(false) {}
    };
    FixedArityList<PhysicalRegister, AnyRegister::Total> registers;

    // Ranges of code which are considered to be hot, for which good allocation
    // should be prioritized.
    AllocatedRangeSet hotcode;

  public:
    BacktrackingAllocator(MIRGenerator *mir, LIRGenerator *lir, LIRGraph &graph)
      : LiveRangeAllocator<BacktrackingVirtualRegister>(mir, lir, graph, /* forLSRA = */ false)
    { }

    bool go();

  private:

    typedef Vector<LiveInterval *, 4, SystemAllocPolicy> LiveIntervalVector;

    bool init();
    bool canAddToGroup(VirtualRegisterGroup *group, BacktrackingVirtualRegister *reg);
    bool tryGroupRegisters(uint32_t vreg0, uint32_t vreg1);
    bool groupAndQueueRegisters();
    bool processInterval(LiveInterval *interval);
    bool setIntervalRequirement(LiveInterval *interval);
    bool tryAllocateRegister(PhysicalRegister &r, LiveInterval *interval,
                             bool *success, LiveInterval **pconflicting);
    bool evictInterval(LiveInterval *interval);
    bool splitAndRequeueInterval(LiveInterval *interval,
                                 const LiveIntervalVector &newIntervals);
    void spill(LiveInterval *interval);

    bool isReusedInput(LUse *use, LInstruction *ins, bool considerCopy = false);
    bool addLiveInterval(LiveIntervalVector &intervals, uint32_t vreg,
                         CodePosition from, CodePosition to);

    bool resolveControlFlow();
    bool reifyAllocations();

    void dumpRegisterGroups();
    void dumpLiveness();
    void dumpAllocations();

    CodePosition minimalDefEnd(LInstruction *ins);
    bool minimalDef(const LiveInterval *interval, LInstruction *ins);
    bool minimalUse(const LiveInterval *interval, LInstruction *ins);
    bool minimalInterval(const LiveInterval *interval, bool *pfixed = NULL);

    // Heuristic methods.

    size_t computePriority(const LiveInterval *interval);
    size_t computeSpillWeight(const LiveInterval *interval);

    bool chooseIntervalSplit(LiveInterval *interval);
    bool trySplitAcrossHotcode(LiveInterval *interval, bool *success);
    bool trySplitAfterLastRegisterUse(LiveInterval *interval, bool *success);
    bool splitAtAllRegisterUses(LiveInterval *interval);
};

} // namespace ion
} // namespace js

#endif
