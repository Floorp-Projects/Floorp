/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_registerallocator_h__
#define js_ion_registerallocator_h__

#include "Ion.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "InlineList.h"
#include "LIR.h"
#include "Lowering.h"
#include "BitSet.h"
#include "StackSlotAllocator.h"

#include "js/Vector.h"

namespace js {
namespace ion {

class VirtualRegister;

/*
 * Represents with better-than-instruction precision a position in the
 * instruction stream.
 *
 * An issue comes up when dealing with live intervals as to how to represent
 * information such as "this register is only needed for the input of
 * this instruction, it can be clobbered in the output". Just having ranges
 * of instruction IDs is insufficiently expressive to denote all possibilities.
 * This class solves this issue by associating an extra bit with the instruction
 * ID which indicates whether the position is the input half or output half of
 * an instruction.
 */
class CodePosition
{
  private:
    CodePosition(const uint32 &bits)
      : bits_(bits)
    { }

    static const unsigned int INSTRUCTION_SHIFT = 1;
    static const unsigned int SUBPOSITION_MASK = 1;
    uint32 bits_;

  public:
    static const CodePosition MAX;
    static const CodePosition MIN;

    /*
     * This is the half of the instruction this code position represents, as
     * described in the huge comment above.
     */
    enum SubPosition {
        INPUT,
        OUTPUT
    };

    CodePosition() : bits_(0)
    { }

    CodePosition(uint32 instruction, SubPosition where) {
        JS_ASSERT(instruction < 0x80000000u);
        JS_ASSERT(((uint32)where & SUBPOSITION_MASK) == (uint32)where);
        bits_ = (instruction << INSTRUCTION_SHIFT) | (uint32)where;
    }

    uint32 ins() const {
        return bits_ >> INSTRUCTION_SHIFT;
    }

    uint32 pos() const {
        return bits_;
    }

    SubPosition subpos() const {
        return (SubPosition)(bits_ & SUBPOSITION_MASK);
    }

    bool operator <(const CodePosition &other) const {
        return bits_ < other.bits_;
    }

    bool operator <=(const CodePosition &other) const {
        return bits_ <= other.bits_;
    }

    bool operator !=(const CodePosition &other) const {
        return bits_ != other.bits_;
    }

    bool operator ==(const CodePosition &other) const {
        return bits_ == other.bits_;
    }

    bool operator >(const CodePosition &other) const {
        return bits_ > other.bits_;
    }

    bool operator >=(const CodePosition &other) const {
        return bits_ >= other.bits_;
    }

    CodePosition previous() const {
        JS_ASSERT(*this != MIN);
        return CodePosition(bits_ - 1);
    }
    CodePosition next() const {
        JS_ASSERT(*this != MAX);
        return CodePosition(bits_ + 1);
    }
};

struct UsePosition : public TempObject,
                     public InlineForwardListNode<UsePosition>
{
    LUse *use;
    CodePosition pos;

    UsePosition(LUse *use, CodePosition pos) :
        use(use),
        pos(pos)
    { }
};

class Requirement
{
  public:
    enum Kind {
        NONE,
        REGISTER,
        FIXED,
        SAME_AS_OTHER
    };

    Requirement()
      : kind_(NONE)
    { }

    Requirement(Kind kind)
      : kind_(kind)
    {
        // These have dedicated constructors;
        JS_ASSERT(kind != FIXED && kind != SAME_AS_OTHER);
    }

    Requirement(Kind kind, CodePosition at)
      : kind_(kind),
        position_(at)
    { }

    Requirement(LAllocation fixed)
      : kind_(FIXED),
        allocation_(fixed)
    { }

    // Only useful as a hint, encodes where the fixed requirement is used to
    // avoid allocating a fixed register too early.
    Requirement(LAllocation fixed, CodePosition at)
      : kind_(FIXED),
        allocation_(fixed),
        position_(at)
    { }

    Requirement(uint32 vreg, CodePosition at)
      : kind_(SAME_AS_OTHER),
        allocation_(LUse(vreg, LUse::ANY)),
        position_(at)
    { }

    Kind kind() const {
        return kind_;
    }

    LAllocation allocation() const {
        JS_ASSERT(!allocation_.isUse());
        return allocation_;
    }

    uint32 virtualRegister() const {
        JS_ASSERT(allocation_.isUse());
        return allocation_.toUse()->virtualRegister();
    }

    CodePosition pos() const {
        return position_;
    }

    int priority() const;

  private:
    Kind kind_;
    LAllocation allocation_;
    CodePosition position_;
};

typedef InlineForwardListIterator<UsePosition> UsePositionIterator;

/*
 * A live interval is a set of disjoint ranges of code positions where a
 * virtual register is live. Linear scan register allocation operates on
 * these intervals, splitting them as necessary and assigning allocations
 * to them as it runs.
 */
class LiveInterval
  : public InlineListNode<LiveInterval>,
    public TempObject
{
  public:
    /*
     * A range is a contiguous sequence of CodePositions where the virtual
     * register associated with this interval is live.
     */
    struct Range {
        Range(CodePosition f, CodePosition t)
          : from(f),
            to(t)
        {
            JS_ASSERT(from < to);
        }
        CodePosition from;

        // The end of this range, exclusive.
        CodePosition to;
    };

  private:
    Vector<Range, 1, IonAllocPolicy> ranges_;
    LAllocation alloc_;
    VirtualRegister *reg_;
    uint32 index_;
    Requirement requirement_;
    Requirement hint_;
    InlineForwardList<UsePosition> uses_;
    size_t lastProcessedRange_;

  public:

    LiveInterval(VirtualRegister *reg, uint32 index)
      : reg_(reg),
        index_(index),
        lastProcessedRange_(size_t(-1))
    { }

    bool addRange(CodePosition from, CodePosition to);
    bool addRangeAtHead(CodePosition from, CodePosition to);
    void setFrom(CodePosition from);
    CodePosition intersect(LiveInterval *other);
    bool covers(CodePosition pos);
    CodePosition nextCoveredAfter(CodePosition pos);

    CodePosition start() const {
        JS_ASSERT(!ranges_.empty());
        return ranges_.back().from;
    }

    CodePosition end() const {
        JS_ASSERT(!ranges_.empty());
        return ranges_.begin()->to;
    }

    size_t numRanges() const {
        return ranges_.length();
    }
    const Range *getRange(size_t i) const {
        return &ranges_[i];
    }
    void setLastProcessedRange(size_t range, DebugOnly<CodePosition> pos) {
        // If the range starts after pos, we may not be able to use
        // it in the next lastProcessedRangeIfValid call.
        JS_ASSERT(ranges_[range].from <= pos);
        lastProcessedRange_ = range;
    }
    size_t lastProcessedRangeIfValid(CodePosition pos) const {
        if (lastProcessedRange_ < ranges_.length() && ranges_[lastProcessedRange_].from <= pos)
            return lastProcessedRange_;
        return ranges_.length() - 1;
    }

    LAllocation *getAllocation() {
        return &alloc_;
    }
    void setAllocation(LAllocation alloc) {
        alloc_ = alloc;
    }
    VirtualRegister *reg() const {
        return reg_;
    }
    uint32 index() const {
        return index_;
    }
    void setIndex(uint32 index) {
        index_ = index;
    }
    Requirement *requirement() {
        return &requirement_;
    }
    void setRequirement(const Requirement &requirement) {
        // A SAME_AS_OTHER requirement complicates regalloc too much; it
        // should only be used as hint.
        JS_ASSERT(requirement.kind() != Requirement::SAME_AS_OTHER);

        // Fixed registers are handled with fixed intervals, so fixed requirements
        // are only valid for non-register allocations.f
        JS_ASSERT_IF(requirement.kind() == Requirement::FIXED,
                     !requirement.allocation().isRegister());

        requirement_ = requirement;
    }
    Requirement *hint() {
        return &hint_;
    }
    void setHint(const Requirement &hint) {
        hint_ = hint;
    }
    bool isSpill() const {
        return alloc_.isStackSlot();
    }
    bool splitFrom(CodePosition pos, LiveInterval *after);

    void addUse(UsePosition *use);
    UsePosition *nextUseAfter(CodePosition pos);
    CodePosition nextUsePosAfter(CodePosition pos);
    CodePosition firstIncompatibleUse(LAllocation alloc);

    UsePositionIterator usesBegin() const {
        return uses_.begin();
    }

    UsePositionIterator usesEnd() const {
        return uses_.end();
    }

#ifdef DEBUG
    void validateRanges();
#endif
};

/*
 * Represents all of the register allocation state associated with a virtual
 * register, including all associated intervals and pointers to relevant LIR
 * structures.
 */
class VirtualRegister
{
    uint32 reg_;
    LBlock *block_;
    LInstruction *ins_;
    LDefinition *def_;
    Vector<LiveInterval *, 1, IonAllocPolicy> intervals_;
    LAllocation *canonicalSpill_;
    CodePosition spillPosition_ ;

    bool spillAtDefinition_ : 1;

    // This bit is used to determine whether both halves of a nunbox have been
    // processed by freeAllocation().
    bool finished_ : 1;

    // Whether def_ is a temp or an output.
    bool isTemp_ : 1;

  public:
    bool init(uint32 reg, LBlock *block, LInstruction *ins, LDefinition *def, bool isTemp) {
        reg_ = reg;
        block_ = block;
        ins_ = ins;
        def_ = def;
        isTemp_ = isTemp;
        LiveInterval *initial = new LiveInterval(this, 0);
        if (!initial)
            return false;
        return intervals_.append(initial);
    }
    uint32 reg() {
        return reg_;
    }
    LBlock *block() {
        return block_;
    }
    LInstruction *ins() {
        return ins_;
    }
    LDefinition *def() const {
        return def_;
    }
    LDefinition::Type type() const {
        return def()->type();
    }
    bool isTemp() const {
        return isTemp_;
    }
    size_t numIntervals() const {
        return intervals_.length();
    }
    LiveInterval *getInterval(size_t i) const {
        return intervals_[i];
    }
    LiveInterval *lastInterval() const {
        JS_ASSERT(numIntervals() > 0);
        return getInterval(numIntervals() - 1);
    }
    bool addInterval(LiveInterval *interval) {
        JS_ASSERT(interval->numRanges());

        // Preserve ascending order for faster lookups.
        LiveInterval **found = NULL;
        LiveInterval **i;
        for (i = intervals_.begin(); i != intervals_.end(); i++) {
            if (!found && interval->start() < (*i)->start())
                found = i;
            if (found)
                (*i)->setIndex((*i)->index() + 1);
        }
        if (!found)
            found = intervals_.end();
        return intervals_.insert(found, interval);
    }
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
    bool isDouble() const {
        return def_->type() == LDefinition::DOUBLE;
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

    LiveInterval *intervalFor(CodePosition pos);
    LiveInterval *getFirstInterval();
};

class VirtualRegisterMap
{
  private:
    VirtualRegister *vregs_;
    uint32 numVregs_;

  public:
    VirtualRegisterMap()
      : vregs_(NULL),
        numVregs_(0)
    { }

    bool init(MIRGenerator *gen, uint32 numVregs) {
        vregs_ = gen->allocate<VirtualRegister>(numVregs);
        numVregs_ = numVregs;
        if (!vregs_)
            return false;
        memset(vregs_, 0, sizeof(VirtualRegister) * numVregs);
        return true;
    }
    VirtualRegister &operator[](unsigned int index) {
        JS_ASSERT(index < numVregs_);
        return vregs_[index];
    }
    VirtualRegister &operator[](const LAllocation *alloc) {
        JS_ASSERT(alloc->isUse());
        JS_ASSERT(alloc->toUse()->virtualRegister() < numVregs_);
        return vregs_[alloc->toUse()->virtualRegister()];
    }
    VirtualRegister &operator[](const LDefinition *def) {
        JS_ASSERT(def->virtualRegister() < numVregs_);
        return vregs_[def->virtualRegister()];
    }
    uint32 numVirtualRegisters() const {
        return numVregs_;
    }
};

class InstructionData
{
    LInstruction *ins_;
    LBlock *block_;
    LMoveGroup *inputMoves_;
    LMoveGroup *movesAfter_;

  public:
    void init(LInstruction *ins, LBlock *block) {
        JS_ASSERT(!ins_);
        JS_ASSERT(!block_);
        ins_ = ins;
        block_ = block;
    }
    LInstruction *ins() const {
        return ins_;
    }
    LBlock *block() const {
        return block_;
    }
    void setInputMoves(LMoveGroup *moves) {
        inputMoves_ = moves;
    }
    LMoveGroup *inputMoves() const {
        return inputMoves_;
    }
    void setMovesAfter(LMoveGroup *moves) {
        movesAfter_ = moves;
    }
    LMoveGroup *movesAfter() const {
        return movesAfter_;
    }
};

class InstructionDataMap
{
    InstructionData *insData_;
    uint32 numIns_;

  public:
    InstructionDataMap()
      : insData_(NULL),
        numIns_(0)
    { }

    bool init(MIRGenerator *gen, uint32 numInstructions) {
        insData_ = gen->allocate<InstructionData>(numInstructions);
        numIns_ = numInstructions;
        if (!insData_)
            return false;
        memset(insData_, 0, sizeof(InstructionData) * numInstructions);
        return true;
    }

    InstructionData &operator[](const CodePosition &pos) {
        JS_ASSERT(pos.ins() < numIns_);
        return insData_[pos.ins()];
    }
    InstructionData &operator[](LInstruction *ins) {
        JS_ASSERT(ins->id() < numIns_);
        return insData_[ins->id()];
    }
};

typedef HashMap<uint32,
                LInstruction *,
                DefaultHasher<uint32>,
                IonAllocPolicy> InstructionMap;

typedef HashMap<uint32,
                LiveInterval *,
                DefaultHasher<uint32>,
                IonAllocPolicy> LiveMap;

typedef InlineList<LiveInterval>::iterator IntervalIterator;
typedef InlineList<LiveInterval>::reverse_iterator IntervalReverseIterator;

class LinearScanAllocator
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

    // Context
    LIRGenerator *lir;
    LIRGraph &graph;

    // Computed inforamtion
    BitSet **liveIn;
    VirtualRegisterMap vregs;
    InstructionDataMap insData;
    FixedArityList<LiveInterval *, AnyRegister::Total> fixedIntervals;

    // Union of all ranges in fixedIntervals, used to quickly determine
    // whether an interval intersects with a fixed register.
    LiveInterval *fixedIntervalsUnion;

    // Allocation state
    StackSlotAllocator stackSlotAllocator;

    typedef Vector<LiveInterval *, 0, SystemAllocPolicy> SlotList;
    SlotList finishedSlots_;
    SlotList finishedDoubleSlots_;

    // Pool of all registers that should be considered allocateable
    RegisterSet allRegisters_;

    // Run-time state
    UnhandledQueue unhandled;
    InlineList<LiveInterval> active;
    InlineList<LiveInterval> inactive;
    InlineList<LiveInterval> fixed;
    InlineList<LiveInterval> handled;
    LiveInterval *current;

    bool createDataStructures();
    bool buildLivenessInfo();
    bool allocateRegisters();
    bool resolveControlFlow();
    bool reifyAllocations();
    bool populateSafepoints();

    // Optimization for the UnsortedQueue.
    void enqueueVirtualRegisterIntervals();

    uint32 allocateSlotFor(const LiveInterval *interval);
    bool splitInterval(LiveInterval *interval, CodePosition pos);
    bool splitBlockingIntervals(LAllocation allocation);
    bool assign(LAllocation allocation);
    bool spill();
    void freeAllocation(LiveInterval *interval, LAllocation *alloc);
    void finishInterval(LiveInterval *interval);
    AnyRegister::Code findBestFreeRegister(CodePosition *freeUntil);
    AnyRegister::Code findBestBlockedRegister(CodePosition *nextUsed);
    bool canCoexist(LiveInterval *a, LiveInterval *b);
    LMoveGroup *getInputMoveGroup(CodePosition pos);
    LMoveGroup *getMoveGroupAfter(CodePosition pos);
    bool addMove(LMoveGroup *moves, LiveInterval *from, LiveInterval *to);
    bool moveInput(CodePosition pos, LiveInterval *from, LiveInterval *to);
    bool moveInputAlloc(CodePosition pos, LAllocation *from, LAllocation *to);
    bool moveAfter(CodePosition pos, LiveInterval *from, LiveInterval *to);
    void setIntervalRequirement(LiveInterval *interval);
    size_t findFirstSafepoint(LiveInterval *interval, size_t firstSafepoint);
    size_t findFirstNonCallSafepoint(CodePosition from);

    bool addFixedRangeAtHead(AnyRegister reg, CodePosition from, CodePosition to) {
        if (!fixedIntervals[reg.code()]->addRangeAtHead(from, to))
            return false;
        return fixedIntervalsUnion->addRangeAtHead(from, to);
    }

#ifdef DEBUG
    void validateIntervals();
    void validateAllocations();
    void validateVirtualRegisters();
#else
    inline void validateIntervals() { };
    inline void validateAllocations() { };
    inline void validateVirtualRegisters() { };
#endif

    CodePosition outputOf(uint32 pos) {
        return CodePosition(pos, CodePosition::OUTPUT);
    }
    CodePosition outputOf(LInstruction *ins) {
        return CodePosition(ins->id(), CodePosition::OUTPUT);
    }
    CodePosition inputOf(uint32 pos) {
        return CodePosition(pos, CodePosition::INPUT);
    }
    CodePosition inputOf(LInstruction *ins) {
        return CodePosition(ins->id(), CodePosition::INPUT);
    }
#ifdef JS_NUNBOX32
    VirtualRegister *otherHalfOfNunbox(VirtualRegister *vreg);
#endif

  public:
    LinearScanAllocator(LIRGenerator *lir, LIRGraph &graph)
      : lir(lir),
        graph(graph),
        allRegisters_(RegisterSet::All())
    {
        if (FramePointer != InvalidReg && lir->mir()->instrumentedProfiling())
            allRegisters_.take(AnyRegister(FramePointer));
    }

    bool go();
};

} // namespace ion
} // namespace js

#endif
