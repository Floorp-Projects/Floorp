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
#ifndef js_ion_registerallocator_h__
#define js_ion_registerallocator_h__

#include "Ion.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "InlineList.h"
#include "IonLIR.h"
#include "Lowering.h"
#include "BitSet.h"

#include "jsvector.h"

namespace js {
namespace ion {

struct LOperand
{
  public:
    LUse *use;
    LInstruction *ins;
    bool snapshot;

    LOperand(LUse *use, LInstruction *ins, bool snapshot) :
        use(use),
        ins(ins),
        snapshot(snapshot)
    { }

};

class VirtualRegister;

/*
 * Represents with better-than-instruction precision a position in the
 * instruction stream.
 *
 * An issue comes up when dealing live intervals as to how to represent
 * information such as "this register is only needed for the input of
 * this instruction, it can be clobbered in the output". Just having ranges
 * of instruction IDs is insufficiently expressive to denote all possibilities.
 * This class solves this issue by associating an extra bit with the instruction
 * ID which indicates whether the position is the input half or output half of
 * an instruction. This way, an interval that ends in the input halg of an
 * instruction will not intersect with the interval that starts in its output
 * half, so they could be assigned the same register without requiring a special
 * case in the register allocator.
 *
 * Doing this also allows live intervals to be inclusive, which makes much of
 * the legwork cleaner. Temporaries and clobbers are represented as intervals
 * consisting only of the output half of an instruction, and thus require no
 * special consideration.
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
};

/*
 * A live interval is a set of disjoint ranges of code positions where a
 * virtual register is live. Linear scan register allocation operates on
 * these intervals, splitting them as necessary and assigning allocations
 * to them as it runs.
 */
class LiveInterval : public InlineListNode<LiveInterval>
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
        { }
        CodePosition from;
        CodePosition to;
    };

  private:
    Vector<Range, 1, IonAllocPolicy> ranges_;
    LAllocation alloc_;
    VirtualRegister *reg_;
    uint32 index_;
    int priority_;

  public:

    LiveInterval(VirtualRegister *reg, uint32 index)
      : reg_(reg),
        index_(index),
        priority_(0)
    { }

    bool addRange(CodePosition from, CodePosition to);

    void setFrom(CodePosition from);

    CodePosition start();

    CodePosition end();

    CodePosition intersect(LiveInterval *other);

    bool covers(CodePosition pos);

    CodePosition nextCoveredAfter(CodePosition pos);

    size_t numRanges();

    Range *getRange(size_t i);

    LAllocation *getAllocation() {
        return &alloc_;
    }

    void setAllocation(LAllocation alloc) {
        alloc_ = alloc;
    }

    VirtualRegister *reg() {
        return reg_;
    }

    uint32 index() const {
        return index_;
    }

    void setIndex(uint32 index) {
        index_ = index;
    }

    int priority() const {
        return priority_;
    }

    void setPriority(int priority) {
        priority_ = priority;
    }

    bool splitFrom(CodePosition pos, LiveInterval *after);
};

/*
 * Represents all of the register allocation state associated with a virtual
 * register, including all associated intervals and pointers to relevant LIR
 * structures.
 */
class VirtualRegister : public TempObject
{
  private:
    uint32 reg_;
    LBlock *block_;
    LInstruction *ins_;
    LDefinition *def_;
    Vector<LiveInterval *, 1, IonAllocPolicy> intervals_;
    Vector<LOperand, 0, IonAllocPolicy> uses_;
    LMoveGroup *inputMoves_;
    LMoveGroup *outputMoves_;
    LAllocation *canonicalSpill_;
    bool isTemporary_;

  public:
    VirtualRegister()
      : reg_(0),
        block_(NULL),
        ins_(NULL),
        intervals_(),
        inputMoves_(NULL),
        outputMoves_(NULL),
        canonicalSpill_(NULL)
    { }

    bool init(uint32 reg, LBlock *block, LInstruction *ins, LDefinition *def,
              bool temporary_ = false)
    {
        reg_ = reg;
        block_ = block;
        ins_ = ins;
        def_ = def;
        isTemporary_ = temporary_;
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
    LDefinition *def() {
        return def_;
    }
    size_t numIntervals() {
        return intervals_.length();
    }
    LiveInterval *getInterval(size_t i) {
        return intervals_[i];
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
    size_t numUses() {
        return uses_.length();
    }
    LOperand *getUse(size_t i) {
        return &uses_[i];
    }
    bool addUse(LOperand operand) {
        return uses_.append(operand);
    }
    void setInputMoves(LMoveGroup *moves) {
        inputMoves_ = moves;
    }
    LMoveGroup *inputMoves() {
        return inputMoves_;
    }
    void setOutputMoves(LMoveGroup *moves) {
        outputMoves_ = moves;
    }
    LMoveGroup *outputMoves() {
        return outputMoves_;
    }
    void setCanonicalSpill(LAllocation *alloc) {
        canonicalSpill_ = alloc;
    }
    LAllocation *canonicalSpill() {
        return canonicalSpill_;
    }
    bool isTemporary() {
        return isTemporary_;
    }

    LiveInterval *intervalFor(CodePosition pos);
    LOperand *nextUseAfter(CodePosition pos);
    CodePosition nextUsePosAfter(CodePosition pos);
    CodePosition nextIncompatibleUseAfter(CodePosition after, LAllocation alloc);
    LiveInterval *getFirstInterval();
};

class VirtualRegisterMap
{
  private:
    VirtualRegister *vregs_;
#ifdef DEBUG
    uint32 numVregs_;
#endif

  public:
    VirtualRegisterMap()
      : vregs_(NULL)
#ifdef DEBUG
      , numVregs_(0)
#endif
    { }

    bool init(MIRGenerator *gen, uint32 numVregs) {
        vregs_ = gen->allocate<VirtualRegister>(numVregs);
#ifdef DEBUG
        numVregs_ = numVregs;
#endif
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
    VirtualRegister &operator[](const CodePosition &pos) {
        JS_ASSERT(pos.ins() < numVregs_);
        return vregs_[pos.ins()];
    }
    VirtualRegister &operator[](const LInstruction *ins) {
        JS_ASSERT(ins->id() < numVregs_);
        return vregs_[ins->id()];
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

class LinearScanAllocator
{
  private:
    friend class C1Spewer;
    friend class JSONSpewer;

    // FIXME (668302): This could be implemented as a more efficient structure.
    class UnhandledQueue : private InlineList<LiveInterval>
    {
      public:
        void enqueue(LiveInterval *interval);
        void enqueue(InlineList<LiveInterval> &list) {
            for (IntervalIterator i(list.begin()); i != list.end(); ) {
                LiveInterval *save = *i;
                i = list.removeAt(i);
                enqueue(save);
            }
        }

        LiveInterval *dequeue();
    };

    // Context
    LIRGenerator *lir;
    LIRGraph &graph;

    // Computed inforamtion
    BitSet **liveIn;
    VirtualRegisterMap vregs;

    // Allocation state
    StackAssignment stackAssignment;

    // Run-time state
    UnhandledQueue unhandled;
    InlineList<LiveInterval> active;
    InlineList<LiveInterval> inactive;
    InlineList<LiveInterval> handled;
    LiveInterval *current;
    LOperand *firstUse;
    CodePosition firstUsePos;

    bool createDataStructures();
    bool buildLivenessInfo();
    bool allocateRegisters();
    bool resolveControlFlow();
    bool reifyAllocations();

    bool splitInterval(LiveInterval *interval, CodePosition pos);
    bool assign(LAllocation allocation);
    bool spill();
    void finishInterval(LiveInterval *interval);
    AnyRegister::Code findBestFreeRegister(CodePosition *freeUntil);
    AnyRegister::Code findBestBlockedRegister(CodePosition *nextUsed);
    bool canCoexist(LiveInterval *a, LiveInterval *b);
    LMoveGroup *getMoveGroupBefore(CodePosition pos);
    bool moveBefore(CodePosition pos, LiveInterval *from, LiveInterval *to);
    void setIntervalPriority(LiveInterval *interval);

#ifdef DEBUG
    void validateIntervals();
    void validateAllocations();
#else
    inline void validateIntervals() { };
    inline void validateAllocations() { };
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

  public:
    LinearScanAllocator(LIRGenerator *lir, LIRGraph &graph)
      : lir(lir),
        graph(graph),
        firstUsePos(CodePosition::MAX)
    { }

    bool go();

};

}
}

#endif
