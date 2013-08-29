/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_LiveRangeAllocator_h
#define jit_LiveRangeAllocator_h

#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"

#include "jit/RegisterAllocator.h"
#include "jit/StackSlotAllocator.h"

// Common structures and functions used by register allocators that operate on
// virtual register live ranges.

namespace js {
namespace jit {

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

    Requirement(uint32_t vreg, CodePosition at)
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

    uint32_t virtualRegister() const {
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

typedef InlineForwardListIterator<UsePosition> UsePositionIterator;

static inline bool
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
        MOZ_ASSUME_UNREACHABLE("Unknown use policy");
    }
}

#ifdef DEBUG

static inline bool
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
        MOZ_ASSUME_UNREACHABLE("Unknown definition policy");
    }
}

#endif // DEBUG

static inline LDefinition *
FindReusingDefinition(LInstruction *ins, LAllocation *alloc)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        LDefinition *def = ins->getDef(i);
        if (def->policy() == LDefinition::MUST_REUSE_INPUT &&
            ins->getOperand(def->getReusedInput()) == alloc)
            return def;
    }
    for (size_t i = 0; i < ins->numTemps(); i++) {
        LDefinition *def = ins->getTemp(i);
        if (def->policy() == LDefinition::MUST_REUSE_INPUT &&
            ins->getOperand(def->getReusedInput()) == alloc)
            return def;
    }
    return NULL;
}

/*
 * A live interval is a set of disjoint ranges of code positions where a
 * virtual register is live. Register allocation operates on these intervals,
 * splitting them as necessary and assigning allocations to them as it runs.
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
        Range()
          : from(),
            to()
        { }
        Range(CodePosition f, CodePosition t)
          : from(f),
            to(t)
        {
            JS_ASSERT(from < to);
        }
        CodePosition from;

        // The end of this range, exclusive.
        CodePosition to;

        bool empty() const {
            return from >= to;
        }

        // Whether this range wholly contains other.
        bool contains(const Range *other) const;

        // Intersect this range with other, returning the subranges of this
        // that are before, inside, or after other.
        void intersect(const Range *other, Range *pre, Range *inside, Range *post) const;
    };

  private:
    Vector<Range, 1, IonAllocPolicy> ranges_;
    LAllocation alloc_;
    uint32_t vreg_;
    uint32_t index_;
    Requirement requirement_;
    Requirement hint_;
    InlineForwardList<UsePosition> uses_;
    size_t lastProcessedRange_;

  public:

    LiveInterval(uint32_t vreg, uint32_t index)
      : vreg_(vreg),
        index_(index),
        lastProcessedRange_(size_t(-1))
    { }

    LiveInterval(uint32_t index)
      : vreg_(UINT32_MAX),
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
    void setLastProcessedRange(size_t range, mozilla::DebugOnly<CodePosition> pos) {
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
    bool hasVreg() const {
        return vreg_ != UINT32_MAX;
    }
    uint32_t vreg() const {
        JS_ASSERT(hasVreg());
        return vreg_;
    }
    uint32_t index() const {
        return index_;
    }
    void setIndex(uint32_t index) {
        index_ = index;
    }
    const Requirement *requirement() const {
        return &requirement_;
    }
    void setRequirement(const Requirement &requirement) {
        // A SAME_AS_OTHER requirement complicates regalloc too much; it
        // should only be used as hint.
        JS_ASSERT(requirement.kind() != Requirement::SAME_AS_OTHER);
        requirement_ = requirement;
    }
    bool addRequirement(const Requirement &newRequirement) {
        // Merge newRequirement with any existing requirement, returning false
        // if the new and old requirements conflict.
        JS_ASSERT(newRequirement.kind() != Requirement::SAME_AS_OTHER);

        if (newRequirement.kind() == Requirement::FIXED) {
            if (requirement_.kind() == Requirement::FIXED)
                return newRequirement.allocation() == requirement_.allocation();
            requirement_ = newRequirement;
            return true;
        }

        JS_ASSERT(newRequirement.kind() == Requirement::REGISTER);
        if (requirement_.kind() == Requirement::FIXED)
            return requirement_.allocation().isRegister();

        requirement_ = newRequirement;
        return true;
    }
    const Requirement *hint() const {
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
    uint32_t id_;
    LBlock *block_;
    LInstruction *ins_;
    LDefinition *def_;
    Vector<LiveInterval *, 1, IonAllocPolicy> intervals_;

    // Whether def_ is a temp or an output.
    bool isTemp_ : 1;

  public:
    bool init(uint32_t id, LBlock *block, LInstruction *ins, LDefinition *def, bool isTemp) {
        JS_ASSERT(block && !block_);
        id_ = id;
        block_ = block;
        ins_ = ins;
        def_ = def;
        isTemp_ = isTemp;
        LiveInterval *initial = new LiveInterval(def->virtualRegister(), 0);
        if (!initial)
            return false;
        return intervals_.append(initial);
    }
    uint32_t id() const {
        return id_;
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
    void replaceInterval(LiveInterval *old, LiveInterval *interval) {
        JS_ASSERT(intervals_[old->index()] == old);
        interval->setIndex(old->index());
        intervals_[old->index()] = interval;
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
        interval->setIndex(found - intervals_.begin());
        return intervals_.insert(found, interval);
    }
    bool isDouble() const {
        return def_->type() == LDefinition::DOUBLE;
    }

    LiveInterval *intervalFor(CodePosition pos);
    LiveInterval *getFirstInterval();
};

// Index of the virtual registers in a graph. VREG is a subclass of
// VirtualRegister extended with any allocator specific state for the vreg.
template <typename VREG>
class VirtualRegisterMap
{
  private:
    VREG *vregs_;
    uint32_t numVregs_;

  public:
    VirtualRegisterMap()
      : vregs_(NULL),
        numVregs_(0)
    { }

    bool init(MIRGenerator *gen, uint32_t numVregs) {
        vregs_ = gen->allocate<VREG>(numVregs);
        numVregs_ = numVregs;
        if (!vregs_)
            return false;
        memset(vregs_, 0, sizeof(VREG) * numVregs);
        return true;
    }
    VREG &operator[](unsigned int index) {
        JS_ASSERT(index < numVregs_);
        return vregs_[index];
    }
    VREG &operator[](const LAllocation *alloc) {
        JS_ASSERT(alloc->isUse());
        JS_ASSERT(alloc->toUse()->virtualRegister() < numVregs_);
        return vregs_[alloc->toUse()->virtualRegister()];
    }
    VREG &operator[](const LDefinition *def) {
        JS_ASSERT(def->virtualRegister() < numVregs_);
        return vregs_[def->virtualRegister()];
    }
    uint32_t numVirtualRegisters() const {
        return numVregs_;
    }
};

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
IsSlotsOrElements(VirtualRegister *vreg)
{
    return vreg->type() == LDefinition::SLOTS;
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

typedef InlineList<LiveInterval>::iterator IntervalIterator;
typedef InlineList<LiveInterval>::reverse_iterator IntervalReverseIterator;

template <typename VREG>
class LiveRangeAllocator : public RegisterAllocator
{
  protected:
    // Computed inforamtion
    BitSet **liveIn;
    VirtualRegisterMap<VREG> vregs;
    mozilla::Array<LiveInterval *, AnyRegister::Total> fixedIntervals;

    // Union of all ranges in fixedIntervals, used to quickly determine
    // whether an interval intersects with a fixed register.
    LiveInterval *fixedIntervalsUnion;

    // Whether the underlying allocator is LSRA. This changes the generated
    // live ranges in various ways: inserting additional fixed uses of
    // registers, and shifting the boundaries of live ranges by small amounts.
    // This exists because different allocators handle live ranges differently;
    // ideally, they would all treat live ranges in the same way.
    bool forLSRA;

    // Allocation state
    StackSlotAllocator stackSlotAllocator;

    LiveRangeAllocator(MIRGenerator *mir, LIRGenerator *lir, LIRGraph &graph, bool forLSRA)
      : RegisterAllocator(mir, lir, graph),
        liveIn(NULL),
        fixedIntervalsUnion(NULL),
        forLSRA(forLSRA)
    {
    }

    bool buildLivenessInfo();

    bool init();

    bool addFixedRangeAtHead(AnyRegister reg, CodePosition from, CodePosition to) {
        if (!fixedIntervals[reg.code()]->addRangeAtHead(from, to))
            return false;
        return fixedIntervalsUnion->addRangeAtHead(from, to);
    }

    void validateVirtualRegisters()
    {
#ifdef DEBUG
        for (size_t i = 1; i < graph.numVirtualRegisters(); i++) {
            VirtualRegister *reg = &vregs[i];

            LiveInterval *prev = NULL;
            for (size_t j = 0; j < reg->numIntervals(); j++) {
                LiveInterval *interval = reg->getInterval(j);
                JS_ASSERT(interval->vreg() == i);
                JS_ASSERT(interval->index() == j);

                if (interval->numRanges() == 0)
                    continue;

                JS_ASSERT_IF(prev, prev->end() <= interval->start());
                interval->validateRanges();

                prev = interval;
            }
        }
#endif
    }

#ifdef JS_NUNBOX32
    VREG *otherHalfOfNunbox(VirtualRegister *vreg) {
        signed offset = OffsetToOtherHalfOfNunbox(vreg->type());
        VREG *other = &vregs[vreg->def()->virtualRegister() + offset];
        AssertTypesFormANunbox(vreg->type(), other->type());
        return other;
    }
#endif

    bool addMove(LMoveGroup *moves, LiveInterval *from, LiveInterval *to) {
        if (*from->getAllocation() == *to->getAllocation())
            return true;
        return moves->add(from->getAllocation(), to->getAllocation());
    }

    bool moveInput(CodePosition pos, LiveInterval *from, LiveInterval *to) {
        LMoveGroup *moves = getInputMoveGroup(pos);
        return addMove(moves, from, to);
    }

    bool moveAfter(CodePosition pos, LiveInterval *from, LiveInterval *to) {
        LMoveGroup *moves = getMoveGroupAfter(pos);
        return addMove(moves, from, to);
    }

    void addLiveRegistersForInterval(VirtualRegister *reg, LiveInterval *interval)
    {
        // Fill in the live register sets for all non-call safepoints.
        LAllocation *a = interval->getAllocation();
        if (!a->isRegister())
            return;

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

    // Finds the first safepoint that is within range of an interval.
    size_t findFirstSafepoint(const LiveInterval *interval, size_t startFrom) const
    {
        size_t i = startFrom;
        for (; i < graph.numSafepoints(); i++) {
            LInstruction *ins = graph.getSafepoint(i);
            if (interval->start() <= inputOf(ins))
                break;
        }
        return i;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_LiveRangeAllocator_h */
