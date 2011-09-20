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
#include "methodjit/Compiler.h"

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
 * check it when initially entering the loop (from JIT code or the
 * interpreter) and after every stub or C++ call.
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

struct TemporaryCopy;

enum InvariantArrayKind { DENSE_ARRAY, TYPED_ARRAY };

class LoopState : public MacroAssemblerTypedefs
{
    JSContext *cx;
    analyze::CrossScriptSSA *ssa;
    JSScript *outerScript;
    analyze::ScriptAnalysis *outerAnalysis;

    Compiler &cc;
    FrameState &frame;

    /* Basic information about this loop. */
    analyze::LoopAnalysis *lifetime;

    /* Allocation at the head of the loop, has all loop carried variables. */
    RegisterAllocation *alloc;

    /*
     * Set if this is not a do-while loop and the compiler has advanced past
     * the loop's entry point.
     */
    bool reachedEntryPoint;

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
        bool entry;
        unsigned patchIndex;  /* Index into Compiler's callSites. */

        /* Any copies of temporaries on the stack */
        Vector<TemporaryCopy> *temporaryCopies;
    };
    Vector<RestoreInvariantCall> restoreInvariantCalls;

    /*
     * Aggregate structure for all loop invariant code and hoisted checks we
     * can perform. These are all stored in the same vector as they may depend
     * on each other and we need to emit code restoring them in order.
     */
    struct InvariantEntry {
        enum EntryKind {
            /*
             * initializedLength(array) > value1 + value2 + constant.
             * Unsigned comparison, so will fail if value + constant < 0
             */
            DENSE_ARRAY_BOUNDS_CHECK,
            TYPED_ARRAY_BOUNDS_CHECK,

            /* value1 + constant >= 0 */
            NEGATIVE_CHECK,

            /* constant >= value1 + value2 */
            RANGE_CHECK,

            /* For dense arrays */
            DENSE_ARRAY_SLOTS,
            DENSE_ARRAY_LENGTH,

            /* For typed arrays */
            TYPED_ARRAY_SLOTS,
            TYPED_ARRAY_LENGTH,

            /* For lazy arguments */
            INVARIANT_ARGS_BASE,
            INVARIANT_ARGS_LENGTH,

            /* For definite properties */
            INVARIANT_PROPERTY
        } kind;
        union {
            struct {
                uint32 arraySlot;
                uint32 valueSlot1;
                uint32 valueSlot2;
                int32 constant;
            } check;
            struct {
                uint32 arraySlot;
                uint32 temporary;
            } array;
            struct {
                uint32 objectSlot;
                uint32 propertySlot;
                uint32 temporary;
                jsid id;
            } property;
        } u;
        InvariantEntry() { PodZero(this); }
        bool isBoundsCheck() const {
            return kind == DENSE_ARRAY_BOUNDS_CHECK || kind == TYPED_ARRAY_BOUNDS_CHECK;
        }
        bool isCheck() const {
            return isBoundsCheck() || kind == NEGATIVE_CHECK || kind == RANGE_CHECK;
        }
    };
    Vector<InvariantEntry, 4, CompilerAllocPolicy> invariantEntries;

    static inline bool entryRedundant(const InvariantEntry &e0, const InvariantEntry &e1);
    bool checkRedundantEntry(const InvariantEntry &entry);

    bool loopInvariantEntry(uint32 slot);
    bool addHoistedCheck(InvariantArrayKind arrayKind, uint32 arraySlot,
                         uint32 valueSlot1, uint32 valueSlot2, int32 constant);
    void addNegativeCheck(uint32 valueSlot, int32 constant);
    void addRangeCheck(uint32 valueSlot1, uint32 valueSlot2, int32 constant);
    bool hasTestLinearRelationship(uint32 slot);

    bool hasInvariants() { return !invariantEntries.empty(); }
    void restoreInvariants(jsbytecode *pc, Assembler &masm,
                           Vector<TemporaryCopy> *temporaryCopies, Vector<Jump> *jumps);

  public:

    /* Outer loop to this one, in case of loop nesting. */
    LoopState *outer;

    /* Offset from the outermost frame at which temporaries should be allocated. */
    uint32 temporariesStart;

    LoopState(JSContext *cx, analyze::CrossScriptSSA *ssa,
              Compiler *cc, FrameState *frame);
    bool init(jsbytecode *head, Jump entry, jsbytecode *entryTarget);

    void setOuterPC(jsbytecode *pc)
    {
        if (uint32(pc - outerScript->code) == lifetime->entry && lifetime->entry != lifetime->head)
            reachedEntryPoint = true;
    }

    bool generatingInvariants() { return !skipAnalysis; }

    /* Add a call with trailing jump/label, after which invariants need to be restored. */
    void addInvariantCall(Jump jump, Label label, bool ool, bool entry, unsigned patchIndex, Uses uses);

    uint32 headOffset() { return lifetime->head; }
    uint32 getLoopRegs() { return loopRegs.freeMask; }

    Jump entryJump() { return entry; }
    uint32 entryOffset() { return lifetime->entry; }
    uint32 backedgeOffset() { return lifetime->backedge; }

    /* Whether the payload of slot is carried around the loop in a register. */
    bool carriesLoopReg(FrameEntry *fe) { return alloc->hasAnyReg(frame.entrySlot(fe)); }

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

    /*
     * These should only be used for entries which are known to be dense arrays
     * (if they are objects at all).
     */
    bool hoistArrayLengthCheck(InvariantArrayKind arrayKind,
                               const analyze::CrossSSAValue &obj,
                               const analyze::CrossSSAValue &index);
    FrameEntry *invariantArraySlots(const analyze::CrossSSAValue &obj);

    /* Methods for accesses on lazy arguments. */
    bool hoistArgsLengthCheck(const analyze::CrossSSAValue &index);
    FrameEntry *invariantArguments();

    FrameEntry *invariantLength(const analyze::CrossSSAValue &obj);
    FrameEntry *invariantProperty(const analyze::CrossSSAValue &obj, jsid id);

    /* Whether a binary or inc/dec op's result cannot overflow. */
    bool cannotIntegerOverflow(const analyze::CrossSSAValue &pushed);

    /*
     * Whether integer overflow in addition or negative zeros in multiplication
     * at a binary op can be safely ignored.
     */
    bool ignoreIntegerOverflow(const analyze::CrossSSAValue &pushed);

  private:
    /* Analysis information for the loop. */

    /*
     * Any inequality known to hold at the head of the loop. This has the
     * form 'lhs <= rhs + constant' or 'lhs >= rhs + constant', depending on
     * lessEqual. The lhs may be modified within the loop body (the test is
     * invalid afterwards), and the rhs is invariant. This information is only
     * valid if the LHS/RHS are known integers.
     */
    enum { UNASSIGNED = uint32(-1) };
    uint32 testLHS;
    uint32 testRHS;
    int32 testConstant;
    bool testLessEqual;

    /*
     * A variable which will be incremented or decremented exactly once in each
     * iteration of the loop. The offset of the operation is indicated, which
     * may or may not run after the initial entry into the loop.
     */
    struct Increment {
        uint32 slot;
        uint32 offset;
    };
    Vector<Increment, 4, CompilerAllocPolicy> increments;

    /* It is unknown which arrays grow or which objects are modified in this loop. */
    bool unknownModset;

    /*
     * Arrays which might grow during this loop. This is a guess, and may
     * underapproximate the actual set of such arrays.
     */
    Vector<types::TypeObject *, 4, CompilerAllocPolicy> growArrays;

    /* Properties which might be modified during this loop. */
    struct ModifiedProperty {
        types::TypeObject *object;
        jsid id;
    };
    Vector<ModifiedProperty, 4, CompilerAllocPolicy> modifiedProperties;

    /*
     * Whether this loop only performs integer and double arithmetic and dense
     * array accesses. Integer overflows in this loop which only flow to bitops
     * can be ignored.
     */
    bool constrainedLoop;

    void analyzeLoopTest();
    void analyzeLoopIncrements();
    void analyzeLoopBody(unsigned frame);

    bool definiteArrayAccess(const analyze::SSAValue &obj, const analyze::SSAValue &index);
    bool getLoopTestAccess(const analyze::SSAValue &v, uint32 *pslot, int32 *pconstant);

    bool addGrowArray(types::TypeObject *object);
    bool addModifiedProperty(types::TypeObject *object, jsid id);

    bool hasGrowArray(types::TypeObject *object);
    bool hasModifiedProperty(types::TypeObject *object, jsid id);

    uint32 getIncrement(uint32 slot);
    int32 adjustConstantForIncrement(jsbytecode *pc, uint32 slot);

    bool getEntryValue(const analyze::CrossSSAValue &v, uint32 *pslot, int32 *pconstant);
    bool computeInterval(const analyze::CrossSSAValue &v, int32 *pmin, int32 *pmax);
    bool valueFlowsToBitops(const analyze::SSAValue &v);
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_loopstate_h__ */
