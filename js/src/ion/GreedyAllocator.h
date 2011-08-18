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
 *   David Anderson <danderson@mozilla.com>
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

#ifndef jsion_include_greedy_h__
#define jsion_include_greedy_h__

#include "MIR.h"
#include "MIRGraph.h"
#include "IonLIR.h"

namespace js {
namespace ion {

class GreedyAllocator
{
    struct Mover {
        LMoveGroup *moves;

        Mover() : moves(NULL)
        { }

        template <typename From, typename To>
        bool move(const From &from, const To &to) {
            if (!moves)
                moves = new LMoveGroup;
            return moves->add(LAllocation::New(from), LAllocation::New(to));
        }
    };

    struct VirtualRegister {
        LDefinition *def;
        uint32 stackSlot_;
        union {
            Registers::Code gprCode;
            FloatRegisters::Code fpuCode;
            uint32 registerCode;
        };
        bool hasRegister_;
        bool hasStackSlot_;

#ifdef DEBUG
        LInstruction *ins;
#endif

        LDefinition::Type type() const {
            return def->type();
        }
        bool isDouble() const {
            return type() == LDefinition::DOUBLE;
        }
        Register gpr() const {
            JS_ASSERT(!isDouble());
            JS_ASSERT(hasRegister());
            return Register::FromCode(gprCode);
        }
        FloatRegister fpu() const {
            JS_ASSERT(isDouble());
            JS_ASSERT(hasRegister());
            return FloatRegister::FromCode(fpuCode);
        }
        AnyRegister reg() const {
            return isDouble() ? AnyRegister(fpu()) : AnyRegister(gpr());
        }
        void setRegister(FloatRegister reg) {
            JS_ASSERT(isDouble());
            fpuCode = reg.code();
            hasRegister_ = true;
        }
        void setRegister(Register reg) {
            JS_ASSERT(!isDouble());
            gprCode = reg.code();
            hasRegister_ = true;
        }
        void setRegister(AnyRegister reg) {
            if (reg.isFloat())
                setRegister(reg.fpu());
            else
                setRegister(reg.gpr());
        }
        uint32 stackSlot() const {
            return stackSlot_;
        }
        bool hasBackingStack() const {
            return hasStackSlot() ||
                   (def->isPreset() && def->output()->isMemory());
        }
        LAllocation backingStack() const {
            if (hasStackSlot())
                return LStackSlot(stackSlot_, isDouble());
            JS_ASSERT(def->policy() == LDefinition::PRESET);
            JS_ASSERT(def->output()->isMemory());
            return *def->output();
        }
        void setStackSlot(uint32 index) {
            JS_ASSERT(!hasStackSlot());
            stackSlot_ = index;
            hasStackSlot_ = true;
        }
        bool hasRegister() const {
            return hasRegister_;
        }
        void unsetRegister() {
            hasRegister_ = false;
        }
        bool hasSameRegister(uint32 code) const {
            return hasRegister() && registerCode == code;
        }
        bool hasStackSlot() const {
            return hasStackSlot_;
        }
    };

    struct AllocationState {
        RegisterSet free;
        VirtualRegister *gprs[Registers::Total];
        VirtualRegister *fpus[FloatRegisters::Total];

        VirtualRegister *& operator[](const AnyRegister &reg) {
            if (reg.isFloat())
                return fpus[reg.fpu().code()];
            return gprs[reg.gpr().code()];
        }

        AllocationState()
          : free(RegisterSet::All()),
            gprs(),
            fpus()
        { }
    };

    struct BlockInfo {
        AllocationState in;
        AllocationState out;
        Mover restores;
        Mover phis;
    };

  private:
    MIRGenerator *gen;
    LIRGraph &graph;
    VirtualRegister *vars;
    RegisterSet disallowed;
    RegisterSet discouraged;
    AllocationState state;
    StackAssignment stackSlots;
    BlockInfo *blocks;

    // Aligns: If a register shuffle must occur to align input parameters (for
    //         example, ecx loading into fixed edx), it goes here.
    // Spills: A definition may have to spill its result register to the stack,
    //         if restore code lies downstream.
    // Restores: If a register is evicted, an instruction will load it off the
    //         stack for downstream uses.
    //
    // Moves happen in this order:
    //   Aligns
    //   <Instruction>
    //   Spills
    //   Restores
    // 
    LMoveGroup *aligns;
    LMoveGroup *spills;
    LMoveGroup *restores;

    bool restore(const LAllocation &from, const AnyRegister &to) {
        if (!restores)
            restores = new LMoveGroup;
        return restores->add(LAllocation::New(from), LAllocation::New(to));
    }

    template <typename LA, typename LB>
    bool spill(const LA &from, const LB &to) {
        if (!spills)
            spills = new LMoveGroup;
        return spills->add(LAllocation::New(from), LAllocation::New(to));
    }

    template <typename LA, typename LB>
    bool align(const LA &from, const LB &to) {
        if (!aligns)
            aligns = new LMoveGroup;
        return aligns->add(LAllocation::New(from), LAllocation::New(to));
    }

    void reset() {
        aligns = NULL;
        spills = NULL;
        restores = NULL;
        disallowed = RegisterSet();
        discouraged = RegisterSet();
    }

  private:
    void assertValidRegisterState();

    void findDefinitionsInLIR(LInstruction *ins);
    void findDefinitionsInBlock(LBlock *block);
    void findDefinitions();

    // Kills a definition, freeing its stack allocation and register.
    bool kill(VirtualRegister *vr);

    // Evicts a register, spilling it to the stack and allowing it to be
    // allocated.
    bool evict(AnyRegister reg);
    bool maybeEvict(AnyRegister reg);

    // Allocates or frees a stack slot.
    bool allocateStack(VirtualRegister *vr);
    void freeStack(VirtualRegister *vr);

    // Marks a register as being free.
    void freeReg(AnyRegister reg);

    // Takes a free register and assigns it to a virtual register.
    void assign(VirtualRegister *vr, AnyRegister reg);

    enum Policy {
        // A temporary register may be allocated again immediately. It is not
        // added to the disallow or used set.
        TEMPORARY,

        // A disallowed register can be re-allocated next instruction, but is
        // pinned for further allocations during this instruction.
        DISALLOW
    };

    // Allocate a free register of a particular type, possibly evicting in the
    // process.
    bool allocate(LDefinition::Type type, Policy policy, AnyRegister *out);

    // Allocate a physical register for a virtual register, possibly evicting
    // in the process.
    bool allocateRegisterOperand(LAllocation *a, VirtualRegister *vr);
    bool allocateAnyOperand(LAllocation *a, VirtualRegister *vr, bool preferReg = false);
    bool allocateFixedOperand(LAllocation *a, VirtualRegister *vr);
    bool allocateWritableOperand(LAllocation *a, VirtualRegister *vr);

    bool prescanDefinition(LDefinition *def);
    bool prescanDefinitions(LInstruction *ins);
    bool prescanUses(LInstruction *ins);
    bool informSnapshot(LSnapshot *snapshot);
    bool allocateSameAsInput(LDefinition *def, LAllocation *a, AnyRegister *out);
    bool allocateDefinitions(LInstruction *ins);
    bool allocateTemporaries(LInstruction *ins);
    bool allocateInputs(LInstruction *ins);

    bool allocateRegisters();
    bool allocateRegistersInBlock(LBlock *block);
    bool allocateInstruction(LBlock *block, LInstruction *ins);
    bool mergePhiState(LBlock *block);
    bool prepareBackedge(LBlock *block);
    bool mergeAllocationState(LBlock *block);
    bool mergeBackedgeState(LBlock *header, LBlock *backedge);
    bool mergeRegisterState(const AnyRegister &reg, LBlock *left, LBlock *right);

    VirtualRegister *getVirtualRegister(LDefinition *def) {
        JS_ASSERT(def->virtualRegister() < graph.numVirtualRegisters());
        return &vars[def->virtualRegister()];
    }
    VirtualRegister *getVirtualRegister(LUse *use) {
        JS_ASSERT(use->virtualRegister() < graph.numVirtualRegisters());
        JS_ASSERT(vars[use->virtualRegister()].def);
        return &vars[use->virtualRegister()];
    }
    RegisterSet allocatableRegs() const {
        return RegisterSet::Intersect(state.free, RegisterSet::Not(disallowed));
    }
    BlockInfo *blockInfo(LBlock *block) {
        JS_ASSERT(block->mir()->id() < graph.numBlockIds());
        return &blocks[block->mir()->id()];
    }

  public:
    GreedyAllocator(MIRGenerator *gen, LIRGraph &graph);

    bool allocate();
};

} // namespace ion
} // namespace js

#endif // jsion_include_greedy_h__

