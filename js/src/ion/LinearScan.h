/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_linearscan_h__
#define js_ion_linearscan_h__

#include "LiveRangeAllocator.h"
#include "BitSet.h"
#include "StackSlotAllocator.h"

#include "js/Vector.h"

namespace js {
namespace ion {

class LinearScanVirtualRegister : public VirtualRegister
{
  private:
    LAllocation *canonicalSpill_;
    CodePosition spillPosition_ ;

    bool spillAtDefinition_ : 1;

    // This bit is used to determine whether both halves of a nunbox have been
    // processed by freeAllocation().
    bool finished_ : 1;

  public:
    void setCanonicalSpill(LAllocation *alloc) {
        canonicalSpill_ = alloc;
    }
    LAllocation *canonicalSpill() const {
        return canonicalSpill_;
    }
    unsigned canonicalSpillSlot() const {
        return canonicalSpill_->toStackSlot()->slot();
    }
    void setFinished() {
        finished_ = true;
    }
    bool finished() const {
        return finished_;
    }
    void setSpillAtDefinition(CodePosition pos) {
        spillAtDefinition_ = true;
        setSpillPosition(pos);
    }
    bool mustSpillAtDefinition() const {
        return spillAtDefinition_;
    }
    CodePosition spillPosition() const {
        return spillPosition_;
    }
    void setSpillPosition(CodePosition pos) {
        spillPosition_ = pos;
    }
};

class LinearScanAllocator : public LiveRangeAllocator<LinearScanVirtualRegister>
{
    friend class C1Spewer;
    friend class JSONSpewer;

    // Work set of LiveIntervals, sorted by start() and then by priority,
    // non-monotonically descending from tail to head.
    class UnhandledQueue : public InlineList<LiveInterval>
    {
      public:
        void enqueueForward(LiveInterval *after, LiveInterval *interval);
        void enqueueBackward(LiveInterval *interval);
        void enqueueAtHead(LiveInterval *interval);

        void assertSorted();

        LiveInterval *dequeue();
    };

    // Allocation state
    StackSlotAllocator stackSlotAllocator;

    typedef Vector<LiveInterval *, 0, SystemAllocPolicy> SlotList;
    SlotList finishedSlots_;
    SlotList finishedDoubleSlots_;

    // Run-time state
    UnhandledQueue unhandled;
    InlineList<LiveInterval> active;
    InlineList<LiveInterval> inactive;
    InlineList<LiveInterval> fixed;
    InlineList<LiveInterval> handled;
    LiveInterval *current;

    bool allocateRegisters();
    bool resolveControlFlow();
    bool reifyAllocations();
    bool populateSafepoints();

    // Optimization for the UnsortedQueue.
    void enqueueVirtualRegisterIntervals();

    uint32_t allocateSlotFor(const LiveInterval *interval);
    bool splitInterval(LiveInterval *interval, CodePosition pos);
    bool splitBlockingIntervals(LAllocation allocation);
    bool assign(LAllocation allocation);
    bool spill();
    void freeAllocation(LiveInterval *interval, LAllocation *alloc);
    void finishInterval(LiveInterval *interval);
    AnyRegister::Code findBestFreeRegister(CodePosition *freeUntil);
    AnyRegister::Code findBestBlockedRegister(CodePosition *nextUsed);
    bool canCoexist(LiveInterval *a, LiveInterval *b);
    bool addMove(LMoveGroup *moves, LiveInterval *from, LiveInterval *to);
    bool moveInput(CodePosition pos, LiveInterval *from, LiveInterval *to);
    bool moveInputAlloc(CodePosition pos, LAllocation *from, LAllocation *to);
    bool moveAfter(CodePosition pos, LiveInterval *from, LiveInterval *to);
    void setIntervalRequirement(LiveInterval *interval);
    size_t findFirstSafepoint(LiveInterval *interval, size_t firstSafepoint);
    size_t findFirstNonCallSafepoint(CodePosition from);
    bool isSpilledAt(LiveInterval *interval, CodePosition pos);

#ifdef DEBUG
    void validateIntervals();
    void validateAllocations();
#else
    inline void validateIntervals() { }
    inline void validateAllocations() { }
#endif

#ifdef JS_NUNBOX32
    LinearScanVirtualRegister *otherHalfOfNunbox(VirtualRegister *vreg);
#endif

  public:
    LinearScanAllocator(MIRGenerator *mir, LIRGenerator *lir, LIRGraph &graph)
      : LiveRangeAllocator<LinearScanVirtualRegister>(mir, lir, graph)
    {
    }

    bool go();
};

} // namespace ion
} // namespace js

#endif
