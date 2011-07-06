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
#include "IonLowering.h"
#include "BitSet.h"

#include "jsvector.h"

namespace js {
namespace ion {

struct LOperand
{
public:
    LUse *use;
    LInstruction *ins;

    LOperand(LUse *use, LInstruction *ins) :
        use(use),
        ins(ins)
    { }

};

class VirtualRegister;

class CodePosition
{
private:
    CodePosition(const uint32 &bits) :
      bits_(bits)
    { }

    static const unsigned int INSTRUCTION_SHIFT = 1;
    static const unsigned int SUBPOSITION_MASK = 1;
    uint32 bits_;

public:
    static const CodePosition MAX;
    static const CodePosition MIN;

    enum SubPosition {
        INPUT,
        OUTPUT
    };

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

class LiveInterval : public InlineListNode<LiveInterval>
{
public:
    struct Range {
        Range(CodePosition f, CodePosition t) :
          from(f),
          to(t)
        { }
        CodePosition from;
        CodePosition to;
    };

    enum Flag {
        FIXED
    };

private:
    Vector<Range, 1, IonAllocPolicy> ranges_;
    LAllocation alloc_;
    VirtualRegister *reg_;
    uint32 flags_;

public:

    LiveInterval(VirtualRegister *reg)
      : reg_(reg),
        flags_(0)
    { }

    static LiveInterval *New(VirtualRegister *reg);

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

    bool hasFlag(Flag flag) const {
        return !!(flags_ & (1 << (uint32)flag));
    }

    void setFlag(Flag flag) {
        flags_ |= (1 << (uint32)flag);
    }

    bool splitFrom(CodePosition pos, LiveInterval *after);
};

class VirtualRegister : public TempObject
{
private:
    uint32 reg_;
    LBlock *block_;
    LInstruction *ins_;
    LDefinition *def_;
    Vector<LiveInterval *, 1, IonAllocPolicy> intervals_;
    Vector<LOperand, 0, IonAllocPolicy> uses_;

public:
    VirtualRegister() :
      reg_(0),
      block_(NULL),
      ins_(NULL),
      intervals_()
    { }

    bool init(uint32 reg, LBlock *block, LInstruction *ins, LDefinition *def) {
        reg_ = reg;
        block_ = block;
        ins_ = ins;
        def_ = def;
        LiveInterval *initial = LiveInterval::New(this);
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
        LiveInterval **i;
        for (i = intervals_.begin(); i != intervals_.end(); i++) {
            if (interval->start() < (*i)->start())
                break;
        }
        return intervals_.insert(i, interval);
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

    LiveInterval *intervalFor(CodePosition pos);
    CodePosition nextUseAfter(CodePosition pos);
    CodePosition nextIncompatibleUseAfter(CodePosition after, LAllocation alloc);
    LiveInterval *getFirstInterval();

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

class RegisterAllocator
{
private:
    friend class C1Spewer;
    friend class JSONSpewer;

    // FIXME (668302): This could be implemented as a more efficient structure.
    class UnhandledQueue : private InlineList<LiveInterval>
    {
      public:
        void enqueue(LiveInterval *interval);

        LiveInterval *dequeue();
    };

    // Context
    LIRGenerator *lir;
    LIRGraph &graph;

    // Allocation state
    StackAssignment stackAssignment;

    RegisterSet allowedRegs;
    VirtualRegister *vregs;
    UnhandledQueue unhandled;
    InlineList<LiveInterval> active;
    InlineList<LiveInterval> inactive;
    InlineList<LiveInterval> handled;
    BitSet **liveIn;
    CodePosition *freeUntilPos;
    CodePosition *nextUsePos;
    LiveInterval *current;

    bool createDataStructures();
    bool buildLivenessInfo();
    bool allocateRegisters();
    bool resolveControlFlow();
    bool reifyAllocations();

    bool splitInterval(LiveInterval *interval, CodePosition pos);
    bool assign(LAllocation allocation);
    bool spill();
    void finishInterval(LiveInterval *interval);
    Register findBestFreeRegister();
    Register findBestBlockedRegister();
    bool canCoexist(LiveInterval *a, LiveInterval *b);
    bool moveBefore(CodePosition pos, LiveInterval *from, LiveInterval *to);

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
    RegisterAllocator(LIRGenerator *lir, LIRGraph &graph)
      : lir(lir),
        graph(graph)
    { }

    bool go();

};

}
}

#endif
