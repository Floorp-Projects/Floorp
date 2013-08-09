/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_RegisterAllocator_h
#define jit_RegisterAllocator_h

#include "mozilla/Attributes.h"

#include "jit/InlineList.h"
#include "jit/Ion.h"
#include "jit/LIR.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

// Generic structures and functions for use by register allocators.

namespace js {
namespace ion {

// Structure for running a liveness analysis on a finished register allocation.
// This analysis can be used for two purposes:
//
// - Check the integrity of the allocation, i.e. that the reads and writes of
//   physical values preserve the semantics of the original virtual registers.
//
// - Populate safepoints with live registers, GC thing and value data, to
//   streamline the process of prototyping new allocators.
struct AllocationIntegrityState
{
    AllocationIntegrityState(LIRGraph &graph)
      : graph(graph)
    {}

    // Record all virtual registers in the graph. This must be called before
    // register allocation, to pick up the original LUses.
    bool record();

    // Perform the liveness analysis on the graph, and assert on an invalid
    // allocation. This must be called after register allocation, to pick up
    // all assigned physical values. If populateSafepoints is specified,
    // safepoints will be filled in with liveness information.
    bool check(bool populateSafepoints);

  private:

    LIRGraph &graph;

    // For all instructions and phis in the graph, keep track of the virtual
    // registers for all inputs and outputs of the nodes. These are overwritten
    // in place during register allocation. This information is kept on the
    // side rather than in the instructions and phis themselves to avoid
    // debug-builds-only bloat in the size of the involved structures.

    struct InstructionInfo {
        Vector<LAllocation, 2, SystemAllocPolicy> inputs;
        Vector<LDefinition, 0, SystemAllocPolicy> temps;
        Vector<LDefinition, 1, SystemAllocPolicy> outputs;

        InstructionInfo()
        { }

        InstructionInfo(const InstructionInfo &o)
        {
            inputs.appendAll(o.inputs);
            temps.appendAll(o.temps);
            outputs.appendAll(o.outputs);
        }
    };
    Vector<InstructionInfo, 0, SystemAllocPolicy> instructions;

    struct BlockInfo {
        Vector<InstructionInfo, 5, SystemAllocPolicy> phis;
        BlockInfo() {}
        BlockInfo(const BlockInfo &o) {
            phis.appendAll(o.phis);
        }
    };
    Vector<BlockInfo, 0, SystemAllocPolicy> blocks;

    Vector<LDefinition*, 20, SystemAllocPolicy> virtualRegisters;

    // Describes a correspondence that should hold at the end of a block.
    // The value which was written to vreg in the original LIR should be
    // physically stored in alloc after the register allocation.
    struct IntegrityItem
    {
        LBlock *block;
        uint32_t vreg;
        LAllocation alloc;

        // Order of insertion into seen, for sorting.
        uint32_t index;

        typedef IntegrityItem Lookup;
        static HashNumber hash(const IntegrityItem &item) {
            HashNumber hash = item.alloc.hash();
            hash = JS_ROTATE_LEFT32(hash, 4) ^ item.vreg;
            hash = JS_ROTATE_LEFT32(hash, 4) ^ HashNumber(item.block->mir()->id());
            return hash;
        }
        static bool match(const IntegrityItem &one, const IntegrityItem &two) {
            return one.block == two.block
                && one.vreg == two.vreg
                && one.alloc == two.alloc;
        }
    };

    // Items still to be processed.
    Vector<IntegrityItem, 10, SystemAllocPolicy> worklist;

    // Set of all items that have already been processed.
    typedef HashSet<IntegrityItem, IntegrityItem, SystemAllocPolicy> IntegrityItemSet;
    IntegrityItemSet seen;

    bool checkIntegrity(LBlock *block, LInstruction *ins, uint32_t vreg, LAllocation alloc,
                        bool populateSafepoints);
    bool checkSafepointAllocation(LInstruction *ins, uint32_t vreg, LAllocation alloc,
                                  bool populateSafepoints);
    bool addPredecessor(LBlock *block, uint32_t vreg, LAllocation alloc);

    void dump();
};

// Represents with better-than-instruction precision a position in the
// instruction stream.
//
// An issue comes up when performing register allocation as to how to represent
// information such as "this register is only needed for the input of
// this instruction, it can be clobbered in the output". Just having ranges
// of instruction IDs is insufficiently expressive to denote all possibilities.
// This class solves this issue by associating an extra bit with the instruction
// ID which indicates whether the position is the input half or output half of
// an instruction.
class CodePosition
{
  private:
    MOZ_CONSTEXPR CodePosition(const uint32_t &bits)
      : bits_(bits)
    { }

    static const unsigned int INSTRUCTION_SHIFT = 1;
    static const unsigned int SUBPOSITION_MASK = 1;
    uint32_t bits_;

  public:
    static const CodePosition MAX;
    static const CodePosition MIN;

    // This is the half of the instruction this code position represents, as
    // described in the huge comment above.
    enum SubPosition {
        INPUT,
        OUTPUT
    };

    MOZ_CONSTEXPR CodePosition() : bits_(0)
    { }

    CodePosition(uint32_t instruction, SubPosition where) {
        JS_ASSERT(instruction < 0x80000000u);
        JS_ASSERT(((uint32_t)where & SUBPOSITION_MASK) == (uint32_t)where);
        bits_ = (instruction << INSTRUCTION_SHIFT) | (uint32_t)where;
    }

    uint32_t ins() const {
        return bits_ >> INSTRUCTION_SHIFT;
    }

    uint32_t pos() const {
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

// Structure to track moves inserted before or after an instruction.
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

// Structure to track all moves inserted next to instructions in a graph.
class InstructionDataMap
{
    InstructionData *insData_;
    uint32_t numIns_;

  public:
    InstructionDataMap()
      : insData_(NULL),
        numIns_(0)
    { }

    bool init(MIRGenerator *gen, uint32_t numInstructions) {
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
    InstructionData &operator[](uint32_t ins) {
        JS_ASSERT(ins < numIns_);
        return insData_[ins];
    }
};

// Common superclass for register allocators.
class RegisterAllocator
{
  protected:
    // Context
    MIRGenerator *mir;
    LIRGenerator *lir;
    LIRGraph &graph;

    // Pool of all registers that should be considered allocateable
    RegisterSet allRegisters_;

    // Computed data
    InstructionDataMap insData;

    RegisterAllocator(MIRGenerator *mir, LIRGenerator *lir, LIRGraph &graph)
      : mir(mir),
        lir(lir),
        graph(graph),
        allRegisters_(RegisterSet::All())
    {
        if (FramePointer != InvalidReg && mir->instrumentedProfiling())
            allRegisters_.take(AnyRegister(FramePointer));
#if defined(JS_CPU_X64)
        if (mir->compilingAsmJS())
            allRegisters_.take(AnyRegister(HeapReg));
#elif defined(JS_CPU_ARM)
        if (mir->compilingAsmJS()) {
            allRegisters_.take(AnyRegister(HeapReg));
            allRegisters_.take(AnyRegister(GlobalReg));
            allRegisters_.take(AnyRegister(NANReg));
        }
#endif
    }

    bool init();

    CodePosition outputOf(uint32_t pos) const {
        return CodePosition(pos, CodePosition::OUTPUT);
    }
    CodePosition outputOf(const LInstruction *ins) const {
        return CodePosition(ins->id(), CodePosition::OUTPUT);
    }
    CodePosition inputOf(uint32_t pos) const {
        return CodePosition(pos, CodePosition::INPUT);
    }
    CodePosition inputOf(const LInstruction *ins) const {
        return CodePosition(ins->id(), CodePosition::INPUT);
    }

    LMoveGroup *getInputMoveGroup(uint32_t ins);
    LMoveGroup *getMoveGroupAfter(uint32_t ins);

    LMoveGroup *getInputMoveGroup(CodePosition pos) {
        return getInputMoveGroup(pos.ins());
    }
    LMoveGroup *getMoveGroupAfter(CodePosition pos) {
        return getMoveGroupAfter(pos.ins());
    }

    size_t findFirstNonCallSafepoint(CodePosition from) const
    {
        size_t i = 0;
        for (; i < graph.numNonCallSafepoints(); i++) {
            const LInstruction *ins = graph.getNonCallSafepoint(i);
            if (from <= inputOf(ins))
                break;
        }
        return i;
    }
};

static inline AnyRegister
GetFixedRegister(LDefinition *def, const LUse *use)
{
    return def->type() == LDefinition::DOUBLE
           ? AnyRegister(FloatRegister::FromCode(use->registerCode()))
           : AnyRegister(Register::FromCode(use->registerCode()));
}

} // namespace ion
} // namespace js

#endif /* jit_RegisterAllocator_h */
