/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
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

#if !defined jsjaeger_loopstate_h__ && defined JS_METHODJIT
#define jsjaeger_loopstate_h__

#include "jsanalyze.h"
#include "methodjit/BaseCompiler.h"

namespace js {
namespace mjit {

/*
 * The LoopState keeps track of register and analysis state within the loop
 * currently being processed by the Compiler.
 *
 * There are several analyses we do that are specific to loops: loop carried
 * registers, bounds check hoisting, and loop invariant code motion. Brief
 * descriptions of these analyses:
 *
 * Loop carried registers. We allocate registers as we emit code, in a single
 * forward pass over the script. Normally this would mean we need to pick the
 * register allocation at the head of the loop before any of the body has been
 * processed. Instead, while processing the loop body we retroactively mark
 * registers as holding the payload of certain entries at the head (being
 * carried around the loop), so that the head's allocation ends up holding
 * registers that are likely to be used shortly. This can be done provided that
 * (a) the register has not been touched since the loop head, (b) the slot
 * has not been modified or separately assigned a different register, and (c)
 * all prior slow path rejoins in the loop are patched with reloads of the
 * register. The register allocation at the loop head must have all entries
 * synced, so that prior slow path syncs do not also need patching.
 *
 * Bounds check hoisting. If we can determine a loop invariant test which
 * implies the bounds check at one or more array accesses, we hoist that and
 * only check it when initially entering the loop (from JIT code or the
 * interpreter). This condition never needs to be checked again within the
 * loop, but can be invalidated if the script's arguments are indirectly
 * written via the 'arguments' property/local (which loop analysis assumes
 * does not happen) or if the involved arrays shrink dynamically through
 * assignments to the length property.
 *
 * Loop invariant code motion. If we can determine a computation (arithmetic,
 * array slot pointer or property access) is loop invariant, we give it a slot
 * on the stack and preserve its value throughout the loop. We can allocate
 * and carry registers for loop invariant slots as for normal slots. These
 * slots sit above the frame's normal slots, and are transient --- they are
 * clobbered whenever a new frame is pushed. We thus regenerate the loop
 * invariant slots after every C++ and scripted call, and avoid doing LICM on
 * loops which have such calls. This has a nice property that the slots only
 * need to be loop invariant wrt the side effects that happen directly in the
 * loop; if C++ calls a getter which scribbles on the object properties
 * involved in an 'invariant' then we will reload the invariant's new value
 * after the call finishes.
 */

class LoopState : public MacroAssemblerTypedefs
{
    JSContext *cx;
    JSScript *script;
    Compiler &cc;
    FrameState &frame;
    analyze::Script *analysis;
    analyze::LifetimeScript *liveness;

    /* Basic information about this loop. */
    analyze::LifetimeLoop *lifetime;

    /* Allocation at the head of the loop, has all loop carried variables. */
    RegisterAllocation *alloc;

    /*
     * Jump which initially enters the loop. The state is synced when this jump
     * occurs, and needs a trampoline generated to load the right registers
     * before going to entryTarget.
     */
    Jump entry;

    /* Registers available for loop variables. */
    Registers loopRegs;

    /* Whether to skip all bounds check hoisting and loop invariant code analysis. */
    bool skipAnalysis;

    /* Prior stub rejoins to patch when new loop registers are allocated. */
    struct StubJoin {
        unsigned index;
        bool script;
    };
    Vector<StubJoin,16,CompilerAllocPolicy> loopJoins;

    /* Pending loads to patch for stub rejoins. */
    struct StubJoinPatch {
        StubJoin join;
        Address address;
        AnyRegisterID reg;
    };
    Vector<StubJoinPatch,16,CompilerAllocPolicy> loopPatches;

    /*
     * Pair of a jump/label immediately after each call in the loop, to patch
     * with restores of the loop invariant stack values.
     */
    struct RestoreInvariantCall {
        Jump jump;
        Label label;
        bool ool;
    };
    Vector<RestoreInvariantCall> restoreInvariantCalls;

    /*
     * Array bounds check hoisted out of the loop. This is a check that needs
     * to be performed, expressed in terms of the state at the loop head.
     */
    struct HoistedBoundsCheck
    {
        /* initializedLength(array) > value + constant */
        uint32 arraySlot;
        uint32 valueSlot;
        int32 constant;
    };
    Vector<HoistedBoundsCheck, 4, CompilerAllocPolicy> hoistedBoundsChecks;

    bool loopInvariantEntry(const FrameEntry *fe);
    bool addHoistedCheck(uint32 arraySlot, uint32 valueSlot, int32 constant);

    /*
     * Track analysis temporaries in the frame state which hold slots pointers
     * for arrays throughout the loop.
     */
    struct InvariantArraySlots
    {
        uint32 arraySlot;
        uint32 temporary;
    };
    Vector<InvariantArraySlots, 4, CompilerAllocPolicy> invariantArraySlots;

    bool hasInvariants() { return !invariantArraySlots.empty(); }
    void restoreInvariants(Assembler &masm);

  public:

    /* Outer loop to this one, in case of loop nesting. */
    LoopState *outer;

    /* Current bytecode for compilation. */
    jsbytecode *PC;

    LoopState(JSContext *cx, JSScript *script,
              Compiler *cc, FrameState *frame,
              analyze::Script *analysis, analyze::LifetimeScript *liveness);
    bool init(jsbytecode *head, Jump entry, jsbytecode *entryTarget);

    bool generatingInvariants() { return !skipAnalysis; }

    /* Add a call with trailing jump/label, after which invariants need to be restored. */
    void addInvariantCall(Jump jump, Label label, bool ool);

    uint32 headOffset() { return lifetime->head; }
    uint32 getLoopRegs() { return loopRegs.freeMask; }

    Jump entryJump() { return entry; }
    uint32 entryOffset() { return lifetime->entry; }
    uint32 backedgeOffset() { return lifetime->backedge; }

    /* Whether the payload of slot is carried around the loop in a register. */
    bool carriesLoopReg(FrameEntry *fe) { return alloc->hasAnyReg(frame.indexOfFe(fe)); }

    void setLoopReg(AnyRegisterID reg, FrameEntry *fe);

    void clearLoopReg(AnyRegisterID reg)
    {
        /*
         * Mark reg as having been modified since the start of the loop; it
         * cannot subsequently be marked to carry a register around the loop.
         */
        JS_ASSERT(loopRegs.hasReg(reg) == alloc->loop(reg));
        if (loopRegs.hasReg(reg)) {
            loopRegs.takeReg(reg);
            alloc->setUnassigned(reg);
            JaegerSpew(JSpew_Regalloc, "clearing loop register %s\n", reg.name());
        }
    }

    void addJoin(unsigned index, bool script);
    void clearLoopRegisters();

    void flushLoop(StubCompiler &stubcc);

    bool hoistArrayLengthCheck(const FrameEntry *obj, const FrameEntry *id);
    FrameEntry *invariantSlots(const FrameEntry *obj);

    bool checkHoistedBounds(jsbytecode *PC, Assembler &masm, Vector<Jump> *jumps);
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_loopstate_h__ */
