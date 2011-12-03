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

#include "methodjit/Compiler.h"
#include "methodjit/LoopState.h"
#include "methodjit/FrameState-inl.h"
#include "methodjit/StubCalls.h"

#include "jstypedarrayinlines.h"

using namespace js;
using namespace js::mjit;
using namespace js::analyze;
using namespace js::types;

inline bool
SafeAdd(int32 one, int32 two, int32 *res)
{
    *res = one + two;
    int64 ores = (int64)one + (int64)two;
    if (ores == (int64)*res)
        return true;
    JaegerSpew(JSpew_Analysis, "Overflow computing %d + %d\n", one, two);
    return false;
}

inline bool
SafeSub(int32 one, int32 two, int32 *res)
{
    *res = one - two;
    int64 ores = (int64)one - (int64)two;
    if (ores == (int64)*res)
        return true;
    JaegerSpew(JSpew_Analysis, "Overflow computing %d - %d\n", one, two);
    return false;
}

inline bool
SafeMul(int32 one, int32 two, int32 *res)
{
    *res = one * two;
    int64 ores = (int64)one * (int64)two;
    if (ores == (int64)*res)
        return true;
    JaegerSpew(JSpew_Analysis, "Overflow computing %d * %d\n", one, two);
    return false;
}

LoopState::LoopState(JSContext *cx, analyze::CrossScriptSSA *ssa,
                     mjit::Compiler *cc, FrameState *frame)
    : cx(cx), ssa(ssa),
      outerScript(ssa->outerScript()), outerAnalysis(outerScript->analysis()),
      cc(*cc), frame(*frame),
      lifetime(NULL), alloc(NULL), reachedEntryPoint(false), loopRegs(0), skipAnalysis(false),
      loopJoins(CompilerAllocPolicy(cx, *cc)),
      loopPatches(CompilerAllocPolicy(cx, *cc)),
      restoreInvariantCalls(CompilerAllocPolicy(cx, *cc)),
      invariantEntries(CompilerAllocPolicy(cx, *cc)),
      outer(NULL), temporariesStart(0),
      testLHS(UNASSIGNED), testRHS(UNASSIGNED),
      testConstant(0), testLessEqual(false),
      increments(CompilerAllocPolicy(cx, *cc)), unknownModset(false),
      growArrays(CompilerAllocPolicy(cx, *cc)),
      modifiedProperties(CompilerAllocPolicy(cx, *cc)),
      constrainedLoop(true)
{
    JS_ASSERT(cx->typeInferenceEnabled());
}

bool
LoopState::init(jsbytecode *head, Jump entry, jsbytecode *entryTarget)
{
    this->lifetime = outerAnalysis->getLoop(head);
    JS_ASSERT(lifetime &&
              lifetime->head == uint32(head - outerScript->code) &&
              lifetime->entry == uint32(entryTarget - outerScript->code));

    this->entry = entry;

    analyzeLoopTest();
    analyzeLoopIncrements();
    for (unsigned i = 0; i < ssa->numFrames(); i++) {
        /* Only analyze this frame if it is nested within the loop itself. */
        uint32 index = ssa->iterFrame(i).index;
        if (index != CrossScriptSSA::OUTER_FRAME) {
            unsigned pframe = index;
            while (ssa->getFrame(pframe).parent != CrossScriptSSA::OUTER_FRAME)
                pframe = ssa->getFrame(pframe).parent;
            uint32 offset = ssa->getFrame(pframe).parentpc - outerScript->code;
            JS_ASSERT(offset < outerScript->length);
            if (offset < lifetime->head || offset > lifetime->backedge)
                continue;
        }
        analyzeLoopBody(index);
    }

    if (testLHS != UNASSIGNED) {
        JaegerSpew(JSpew_Analysis, "loop test at %u: %s %s %s + %d\n", lifetime->head,
                   frame.entryName(testLHS),
                   testLessEqual ? "<=" : ">=",
                   (testRHS == UNASSIGNED) ? "" : frame.entryName(testRHS),
                   testConstant);
    }

    for (unsigned i = 0; i < increments.length(); i++) {
        JaegerSpew(JSpew_Analysis, "loop increment at %u for %s: %u\n", lifetime->head,
                   frame.entryName(increments[i].slot),
                   increments[i].offset);
    }

    for (unsigned i = 0; i < growArrays.length(); i++) {
        JaegerSpew(JSpew_Analysis, "loop grow array at %u: %s\n", lifetime->head,
                   types::TypeString(types::Type::ObjectType(growArrays[i])));
    }

    for (unsigned i = 0; i < modifiedProperties.length(); i++) {
        JaegerSpew(JSpew_Analysis, "loop modified property at %u: %s %s\n", lifetime->head,
                   types::TypeString(types::Type::ObjectType(modifiedProperties[i].object)),
                   TypeIdString(modifiedProperties[i].id));
    }

    RegisterAllocation *&alloc = outerAnalysis->getAllocation(head);
    JS_ASSERT(!alloc);

    alloc = cx->typeLifoAlloc().new_<RegisterAllocation>(true);
    if (!alloc)
        return false;

    this->alloc = alloc;
    this->loopRegs = Registers::AvailAnyRegs;

    /*
     * Don't hoist bounds checks or loop invariant code in scripts that have
     * had indirect modification of their arguments.
     */
    if (outerScript->hasFunction) {
        if (TypeSet::HasObjectFlags(cx, outerScript->function()->getType(cx), OBJECT_FLAG_UNINLINEABLE))
            this->skipAnalysis = true;
    }

    /*
     * Don't hoist bounds checks or loop invariant code in loops with safe
     * points in the middle, which the interpreter can join at directly without
     * performing hoisted bounds checks or doing initial computation of loop
     * invariant terms.
     */
    if (lifetime->hasSafePoints)
        this->skipAnalysis = true;

    return true;
}

void
LoopState::addJoin(unsigned index, bool script)
{
    StubJoin r;
    r.index = index;
    r.script = script;
    loopJoins.append(r);
}

void
LoopState::addInvariantCall(Jump jump, Label label, bool ool, bool entry, unsigned patchIndex, Uses uses)
{
    RestoreInvariantCall call;
    call.jump = jump;
    call.label = label;
    call.ool = ool;
    call.entry = entry;
    call.patchIndex = patchIndex;
    call.temporaryCopies = frame.getTemporaryCopies(uses);

    restoreInvariantCalls.append(call);
}

void
LoopState::flushLoop(StubCompiler &stubcc)
{
    clearLoopRegisters();

    /*
     * Patch stub compiler rejoins with loads of loop carried registers
     * discovered after the fact.
     */
    for (unsigned i = 0; i < loopPatches.length(); i++) {
        const StubJoinPatch &p = loopPatches[i];
        stubcc.patchJoin(p.join.index, p.join.script, p.address, p.reg);
    }
    loopJoins.clear();
    loopPatches.clear();

    if (hasInvariants()) {
        for (unsigned i = 0; i < restoreInvariantCalls.length(); i++) {
            RestoreInvariantCall &call = restoreInvariantCalls[i];
            Assembler &masm = cc.getAssembler(true);
            Vector<Jump> failureJumps(cx);

            jsbytecode *pc = cc.getInvariantPC(call.patchIndex);

            if (call.ool) {
                call.jump.linkTo(masm.label(), &masm);
                restoreInvariants(pc, masm, call.temporaryCopies, &failureJumps);
                masm.jump().linkTo(call.label, &masm);
            } else {
                stubcc.linkExitDirect(call.jump, masm.label());
                restoreInvariants(pc, masm, call.temporaryCopies, &failureJumps);
                stubcc.crossJump(masm.jump(), call.label);
            }

            if (!failureJumps.empty()) {
                for (unsigned i = 0; i < failureJumps.length(); i++)
                    failureJumps[i].linkTo(masm.label(), &masm);

                /*
                 * Call InvariantFailure, setting up the return address to
                 * patch and any value for the call to return.
                 */
                InvariantCodePatch *patch = cc.getInvariantPatch(call.patchIndex);
                patch->hasPatch = true;
                patch->codePatch = masm.storePtrWithPatch(ImmPtr(NULL),
                                                          FrameAddress(offsetof(VMFrame, scratch)));
                JS_STATIC_ASSERT(Registers::ReturnReg != Registers::ArgReg1);
                masm.move(Registers::ReturnReg, Registers::ArgReg1);

                if (call.entry) {
                    masm.fallibleVMCall(true, JS_FUNC_TO_DATA_PTR(void *, stubs::InvariantFailure),
                                        pc, NULL, 0);
                } else {
                    /* f.regs are already coherent, don't write new values to them. */
                    masm.infallibleVMCall(JS_FUNC_TO_DATA_PTR(void *, stubs::InvariantFailure), -1);
                }
            }
        }
    } else {
        for (unsigned i = 0; i < restoreInvariantCalls.length(); i++) {
            RestoreInvariantCall &call = restoreInvariantCalls[i];
            Assembler &masm = cc.getAssembler(call.ool);
            call.jump.linkTo(call.label, &masm);
        }
    }
    restoreInvariantCalls.clear();
}

void
LoopState::clearLoopRegisters()
{
    alloc->clearLoops();
    loopRegs = 0;
}

bool
LoopState::loopInvariantEntry(uint32 slot)
{
    if (slot == UNASSIGNED)
        return true;

    /* Watch for loop temporaries. :XXX: this is really gross. */
    if (slot >= analyze::LocalSlot(outerScript, outerScript->nslots))
        return true;

    if (slot == analyze::CalleeSlot() || outerAnalysis->slotEscapes(slot))
        return false;
    return outerAnalysis->liveness(slot).firstWrite(lifetime) == uint32(-1);
}

inline bool
LoopState::entryRedundant(const InvariantEntry &e0, const InvariantEntry &e1)
{
    JS_ASSERT(e0.isCheck() && e1.isCheck());

    uint32 array0 = e0.u.check.arraySlot;
    uint32 array1 = e1.u.check.arraySlot;

    uint32 value01 = e0.u.check.valueSlot1;
    uint32 value02 = e0.u.check.valueSlot2;

    uint32 value11 = e1.u.check.valueSlot1;
    uint32 value12 = e1.u.check.valueSlot2;

    int32 c0 = e0.u.check.constant;
    int32 c1 = e1.u.check.constant;

    /*
     * initialized lengths are always <= JSObject::NSLOTS_LIMIT, check for
     * integer overflow checks redundant given initialized length checks.
     * If Y <= c0 and Y + c1 < initlen(array):
     *
     * Y <= c0
     * initlen(array) - c1 <= c0
     * NSLOTS_LIMIT <= c0 + c1
     */
    if (e0.kind == InvariantEntry::RANGE_CHECK && e1.isBoundsCheck() &&
        value01 == value11 && value02 == value12) {
        int32 constant;
        if (c1 >= 0)
            constant = c0;
        else if (!SafeAdd(c0, c1, &constant))
            return false;
        return constant >= JSObject::NSLOTS_LIMIT;
    }

    /* Look for matching tests that differ only in their constants. */
    if (e0.kind == e1.kind && array0 == array1 && value01 == value11 && value02 == value12) {
        if (e0.isBoundsCheck()) {
            /* If e0 is X >= Y + c0 and e1 is X >= Y + c1, e0 is redundant if c0 <= c1 */
            return (c0 <= c1);
        } else {
            /* If e0 is c0 >= Y and e1 is c1 >= Y, e0 is redundant if c0 >= c1 */
            return (c0 >= c1);
        }
    }

    return false;
}

bool
LoopState::checkRedundantEntry(const InvariantEntry &entry)
{
    /*
     * Return true if entry is implied by an existing entry, otherwise filter
     * out any existing entries which entry implies.
     */
    JS_ASSERT(entry.isCheck());

    /* Maintain this separately, GCC miscompiles if the loop test is invariantEntries.length(). */
    unsigned length = invariantEntries.length();

    for (unsigned i = 0; i < length; i++) {
        InvariantEntry &baseEntry = invariantEntries[i];
        if (!baseEntry.isCheck())
            continue;
        if (entryRedundant(entry, baseEntry))
            return true;
        if (entryRedundant(baseEntry, entry)) {
            /*
             * Make sure to maintain the existing ordering on how invariant
             * entries are generated, this is required for e.g. entries which
             * use temporaries or slot computations which appear before any
             * bounds checks on the arrays.
             */
            for (unsigned j = i; j < length - 1; j++)
                invariantEntries[j] = invariantEntries[j + 1];
            invariantEntries.popBack();
            i--;
            length--;
        }
    }

    return false;
}

bool
LoopState::addHoistedCheck(InvariantArrayKind arrayKind, uint32 arraySlot,
                           uint32 valueSlot1, uint32 valueSlot2, int32 constant)
{
#ifdef DEBUG
    JS_ASSERT_IF(valueSlot1 == UNASSIGNED, valueSlot2 == UNASSIGNED);
    const char *field = (arrayKind == DENSE_ARRAY) ? "initlen" : "length";
    if (valueSlot1 == UNASSIGNED) {
        JaegerSpew(JSpew_Analysis, "Hoist %s > %d\n", field, constant);
    } else if (valueSlot2 == UNASSIGNED) {
        JaegerSpew(JSpew_Analysis, "Hoisted as %s > %s + %d\n", field,
                   frame.entryName(valueSlot1), constant);
    } else {
        JaegerSpew(JSpew_Analysis, "Hoisted as %s > %s + %s + %d\n", field,
                   frame.entryName(valueSlot1), frame.entryName(valueSlot2), constant);
    }
#endif

    InvariantEntry entry;
    entry.kind = (arrayKind == DENSE_ARRAY)
                 ? InvariantEntry::DENSE_ARRAY_BOUNDS_CHECK
                 : InvariantEntry::TYPED_ARRAY_BOUNDS_CHECK;
    entry.u.check.arraySlot = arraySlot;
    entry.u.check.valueSlot1 = valueSlot1;
    entry.u.check.valueSlot2 = valueSlot2;
    entry.u.check.constant = constant;

    if (checkRedundantEntry(entry))
        return true;

    /*
     * Maintain an invariant that for any array with a hoisted bounds check,
     * we also have a loop invariant slot to hold the array's slots pointer.
     * The compiler gets invariant array slots only for accesses with a hoisted
     * bounds check, so this makes invariantSlots infallible.
     */
    bool hasInvariantSlots = false;
    InvariantEntry::EntryKind slotsKind = (arrayKind == DENSE_ARRAY)
                                          ? InvariantEntry::DENSE_ARRAY_SLOTS
                                          : InvariantEntry::TYPED_ARRAY_SLOTS;
    for (unsigned i = 0; !hasInvariantSlots && i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == slotsKind && entry.u.array.arraySlot == arraySlot)
            hasInvariantSlots = true;
    }
    if (!hasInvariantSlots) {
        uint32 which = frame.allocTemporary();
        if (which == uint32(-1))
            return false;
        FrameEntry *fe = frame.getTemporary(which);

        JaegerSpew(JSpew_Analysis, "Using %s for loop invariant slots of %s\n",
                   frame.entryName(fe), frame.entryName(arraySlot));

        InvariantEntry slotsEntry;
        slotsEntry.kind = slotsKind;
        slotsEntry.u.array.arraySlot = arraySlot;
        slotsEntry.u.array.temporary = which;
        invariantEntries.append(slotsEntry);
    }

    invariantEntries.append(entry);
    return true;
}

void
LoopState::addNegativeCheck(uint32 valueSlot, int32 constant)
{
    JaegerSpew(JSpew_Analysis, "Nonnegative check %s + %d >= 0\n",
               frame.entryName(valueSlot), constant);

    InvariantEntry entry;
    entry.kind = InvariantEntry::NEGATIVE_CHECK;
    entry.u.check.valueSlot1 = valueSlot;
    entry.u.check.constant = constant;

    if (!checkRedundantEntry(entry))
        invariantEntries.append(entry);
}

void
LoopState::addRangeCheck(uint32 valueSlot1, uint32 valueSlot2, int32 constant)
{
    JaegerSpew(JSpew_Analysis, "Range check %d >= %s + %s\n",
               constant, frame.entryName(valueSlot1),
               valueSlot2 == uint32(-1) ? "" : frame.entryName(valueSlot2));

    InvariantEntry entry;
    entry.kind = InvariantEntry::RANGE_CHECK;
    entry.u.check.valueSlot1 = valueSlot1;
    entry.u.check.valueSlot2 = valueSlot2;
    entry.u.check.constant = constant;

    if (!checkRedundantEntry(entry))
        invariantEntries.append(entry);
}

void
LoopState::setLoopReg(AnyRegisterID reg, FrameEntry *fe)
{
    JS_ASSERT(alloc->loop(reg));
    loopRegs.takeReg(reg);

    uint32 slot = frame.outerSlot(fe);
    JaegerSpew(JSpew_Regalloc, "allocating loop register %s for %s\n",
               reg.name(), frame.entryName(fe));

    alloc->set(reg, slot, true);

    /*
     * Mark pending rejoins to patch up with the load. We don't do this now as that would
     * cause us to emit into the slow path, which may be in progress.
     */
    for (unsigned i = 0; i < loopJoins.length(); i++) {
        StubJoinPatch p;
        p.join = loopJoins[i];
        p.address = frame.addressOf(fe);
        p.reg = reg;
        loopPatches.append(p);
    }

    if (reachedEntryPoint) {
        /*
         * We've advanced past the entry point of the loop (we're analyzing the condition),
         * so need to update the register state at that entry point so that the right
         * things get loaded when we enter the loop.
         */
        RegisterAllocation *alloc = outerAnalysis->getAllocation(lifetime->entry);
        JS_ASSERT(alloc && !alloc->assigned(reg));
        alloc->set(reg, slot, true);
    }
}

bool
LoopState::hoistArrayLengthCheck(InvariantArrayKind arrayKind, const CrossSSAValue &obj,
                                 const CrossSSAValue &index)
{
    /*
     * Note: this method requires that the index is definitely an integer, and
     * that obj is either a dense array, a typed array or not an object.
     */
    if (skipAnalysis)
        return false;

    uint32 objSlot;
    int32 objConstant;
    if (!getEntryValue(obj, &objSlot, &objConstant) || objSlot == UNASSIGNED || objConstant != 0)
        return false;

    JaegerSpew(JSpew_Analysis, "Trying to hoist bounds check on %s\n",
               frame.entryName(objSlot));

    if (!loopInvariantEntry(objSlot)) {
        JaegerSpew(JSpew_Analysis, "Object is not loop invariant\n");
        return false;
    }

    /*
     * Check for an overlap with the arrays we think might grow in this loop.
     * This information is only a guess; if we don't think the array can grow
     * but it actually can, we will probably recompile after the hoisted
     * bounds check fails.
     */
    TypeSet *objTypes = ssa->getValueTypes(obj);
    if (arrayKind == DENSE_ARRAY && !growArrays.empty()) {
        unsigned count = objTypes->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            if (objTypes->getSingleObject(i) != NULL) {
                JaegerSpew(JSpew_Analysis, "Object might be a singleton");
                return false;
            }
            TypeObject *object = objTypes->getTypeObject(i);
            if (object && hasGrowArray(object)) {
                JaegerSpew(JSpew_Analysis, "Object might grow inside loop\n");
                return false;
            }
        }
    }

    /*
     * Get an expression for the index 'index + indexConstant', where index
     * is the value of a slot at loop entry.
     */
    uint32 indexSlot;
    int32 indexConstant;
    if (!getEntryValue(index, &indexSlot, &indexConstant)) {
        JaegerSpew(JSpew_Analysis, "Could not compute index in terms of loop entry state\n");
        return false;
    }

    if (indexSlot == UNASSIGNED) {
        /* Hoist checks on x[n] accesses for constant n. */
        if (indexConstant < 0) {
            JaegerSpew(JSpew_Analysis, "Constant index is negative\n");
            return false;
        }
        return addHoistedCheck(arrayKind, objSlot, UNASSIGNED, UNASSIGNED, indexConstant);
    }

    if (loopInvariantEntry(indexSlot)) {
        /* Hoist checks on x[y] accesses when y is loop invariant. */
        addNegativeCheck(indexSlot, indexConstant);
        return addHoistedCheck(arrayKind, objSlot, indexSlot, UNASSIGNED, indexConstant);
    }

    /*
     * If the LHS can decrease in the loop, it could become negative and
     * underflow the array. We currently only hoist bounds checks for loops
     * which walk arrays going forward.
     */
    if (!outerAnalysis->liveness(indexSlot).nonDecreasing(outerScript, lifetime)) {
        JaegerSpew(JSpew_Analysis, "Index may decrease in future iterations\n");
        return false;
    }

    /*
     * If the access is of the form x[y + a] where we know that y <= z + b
     * (both in terms of state at the head of the loop), hoist as follows:
     *
     * y + a < initlen(x)
     * y < initlen(x) - a
     * z + b < initlen(x) - a
     * z + b + a < initlen(x)
     */
    if (indexSlot == testLHS && testLessEqual) {
        int32 constant;
        if (!SafeAdd(testConstant, indexConstant, &constant))
            return false;

        /*
         * Check that the LHS is nonnegative every time we rejoin the loop.
         * This is only really necessary on initial loop entry. Note that this
         * test is not sensitive to changes to the LHS between when we make
         * the test and the start of the next iteration, as we've ensured the
         * LHS is nondecreasing within the body of the loop.
         */
        addNegativeCheck(indexSlot, indexConstant);

        return addHoistedCheck(arrayKind, objSlot, testRHS, UNASSIGNED, constant);
    }

    /*
     * If the access is of the form x[y + a] where we know that z >= b at the
     * head of the loop and y has a linear relationship with z such that
     * (y + z) always has the same value at the head of the loop, hoist as
     * follows:
     *
     * y + a < initlen(x)
     * y + z < initlen(x) + z - a
     * y + z < initlen(x) + b - a
     * y + z + a - b < initlen(x)
     */
    if (hasTestLinearRelationship(indexSlot)) {
        int32 constant;
        if (!SafeSub(indexConstant, testConstant, &constant))
            return false;

        addNegativeCheck(indexSlot, indexConstant);
        return addHoistedCheck(arrayKind, objSlot, indexSlot, testLHS, constant);
    }

    JaegerSpew(JSpew_Analysis, "No match found\n");
    return false;
}

bool
LoopState::hoistArgsLengthCheck(const CrossSSAValue &index)
{
    if (skipAnalysis)
        return false;

    JaegerSpew(JSpew_Analysis, "Trying to hoist argument range check\n");

    uint32 indexSlot;
    int32 indexConstant;
    if (!getEntryValue(index, &indexSlot, &indexConstant)) {
        JaegerSpew(JSpew_Analysis, "Could not compute index in terms of loop entry state\n");
        return false;
    }

    /*
     * We only hoist arguments checks which can be completely eliminated, for
     * now just tests with 'i < arguments.length' or similar in the condition.
     */

    if (indexSlot == UNASSIGNED || loopInvariantEntry(indexSlot)) {
        JaegerSpew(JSpew_Analysis, "Index is constant or loop invariant\n");
        return false;
    }

    if (!outerAnalysis->liveness(indexSlot).nonDecreasing(outerScript, lifetime)) {
        JaegerSpew(JSpew_Analysis, "Index may decrease in future iterations\n");
        return false;
    }

    if (indexSlot == testLHS && indexConstant == 0 && testConstant == -1 && testLessEqual) {
        bool found = false;
        for (unsigned i = 0; i < invariantEntries.length(); i++) {
            const InvariantEntry &entry = invariantEntries[i];
            if (entry.kind == InvariantEntry::INVARIANT_ARGS_LENGTH) {
                uint32 slot = frame.outerSlot(frame.getTemporary(entry.u.array.temporary));
                if (slot == testRHS)
                    found = true;
                break;
            }
        }
        if (found) {
            addNegativeCheck(indexSlot, indexConstant);
            JaegerSpew(JSpew_Analysis, "Access implied by loop test\n");
            return true;
        }
    }

    JaegerSpew(JSpew_Analysis, "No match found\n");
    return false;
}

bool
LoopState::hasTestLinearRelationship(uint32 slot)
{
    /*
     * Determine whether slot has a linear relationship with the loop test
     * variable 'test', such that (slot + test) always has the same value at
     * the head of the loop.
     */

    if (testLHS == UNASSIGNED || testRHS != UNASSIGNED || testLessEqual)
        return false;

    uint32 incrementOffset = getIncrement(slot);
    if (incrementOffset == uint32(-1)) {
        /*
         * Variable is not always incremented in the loop, or is incremented
         * multiple times. Note that the nonDecreasing test done earlier
         * ensures that if there is a single write, it is an increment.
         */
        return false;
    }

    uint32 decrementOffset = getIncrement(testLHS);
    if (decrementOffset == uint32(-1))
        return false;

    JSOp op = JSOp(outerScript->code[decrementOffset]);
    switch (op) {
      case JSOP_DECLOCAL:
      case JSOP_LOCALDEC:
      case JSOP_DECARG:
      case JSOP_ARGDEC:
        return true;
      default:
        return false;
    }
}

FrameEntry *
LoopState::invariantArraySlots(const CrossSSAValue &obj)
{
    JS_ASSERT(!skipAnalysis);

    uint32 objSlot;
    int32 objConstant;
    if (!getEntryValue(obj, &objSlot, &objConstant) || objSlot == UNASSIGNED || objConstant != 0) {
        JS_NOT_REACHED("Bad value");
        return NULL;
    }

    /*
     * Note: we don't have to check arrayKind (dense array or typed array) here,
     * because an array cannot have entries for both dense array slots and typed
     * array slots.
     */
    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if ((entry.kind == InvariantEntry::DENSE_ARRAY_SLOTS ||
             entry.kind == InvariantEntry::TYPED_ARRAY_SLOTS) &&
            entry.u.array.arraySlot == objSlot) {
            return frame.getTemporary(entry.u.array.temporary);
        }
    }

    /* addHoistedCheck should have ensured there is an entry for the slots. */
    JS_NOT_REACHED("Missing invariant slots");
    return NULL;
}

FrameEntry *
LoopState::invariantArguments()
{
    if (skipAnalysis)
        return NULL;

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::INVARIANT_ARGS_BASE)
            return frame.getTemporary(entry.u.array.temporary);
    }

    uint32 which = frame.allocTemporary();
    if (which == uint32(-1))
        return NULL;
    FrameEntry *fe = frame.getTemporary(which);

    InvariantEntry entry;
    entry.kind = InvariantEntry::INVARIANT_ARGS_BASE;
    entry.u.array.temporary = which;
    invariantEntries.append(entry);

    JaegerSpew(JSpew_Analysis, "Using %s for loop invariant args base\n",
               frame.entryName(fe));
    return fe;
}

FrameEntry *
LoopState::invariantLength(const CrossSSAValue &obj)
{
    if (skipAnalysis)
        return NULL;

    uint32 objSlot;
    int32 objConstant;
    if (!getEntryValue(obj, &objSlot, &objConstant) || objSlot == UNASSIGNED || objConstant != 0)
        return NULL;
    TypeSet *objTypes = ssa->getValueTypes(obj);

    /* Check for 'length' on the lazy arguments for the current frame. */
    if (objTypes->isLazyArguments(cx)) {
        JS_ASSERT(obj.frame == CrossScriptSSA::OUTER_FRAME);

        for (unsigned i = 0; i < invariantEntries.length(); i++) {
            InvariantEntry &entry = invariantEntries[i];
            if (entry.kind == InvariantEntry::INVARIANT_ARGS_LENGTH)
                return frame.getTemporary(entry.u.array.temporary);
        }

        uint32 which = frame.allocTemporary();
        if (which == uint32(-1))
            return NULL;
        FrameEntry *fe = frame.getTemporary(which);

        InvariantEntry entry;
        entry.kind = InvariantEntry::INVARIANT_ARGS_LENGTH;
        entry.u.array.temporary = which;
        invariantEntries.append(entry);

        JaegerSpew(JSpew_Analysis, "Using %s for loop invariant args length\n",
                   frame.entryName(fe));
        return fe;
    }

    /*
     * Note: we don't have to check arrayKind (dense array or typed array) here,
     * because an array cannot have entries for both dense array length and typed
     * array length.
     */
    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if ((entry.kind == InvariantEntry::DENSE_ARRAY_LENGTH ||
             entry.kind == InvariantEntry::TYPED_ARRAY_LENGTH) &&
            entry.u.array.arraySlot == objSlot) {
            return frame.getTemporary(entry.u.array.temporary);
        }
    }

    if (!loopInvariantEntry(objSlot))
        return NULL;

    /* Hoist 'length' access on typed arrays. */
    if (!objTypes->hasObjectFlags(cx, OBJECT_FLAG_NON_TYPED_ARRAY)) {
        /* Recompile if object type changes. */
        objTypes->addFreeze(cx);

        uint32 which = frame.allocTemporary();
        if (which == uint32(-1))
            return NULL;
        FrameEntry *fe = frame.getTemporary(which);

        JaegerSpew(JSpew_Analysis, "Using %s for loop invariant typed array length of %s\n",
                   frame.entryName(fe), frame.entryName(objSlot));

        InvariantEntry entry;
        entry.kind = InvariantEntry::TYPED_ARRAY_LENGTH;
        entry.u.array.arraySlot = objSlot;
        entry.u.array.temporary = which;
        invariantEntries.append(entry);

        return fe;
    }

    if (objTypes->hasObjectFlags(cx, OBJECT_FLAG_NON_DENSE_ARRAY))
        return NULL;

    /*
     * Don't make 'length' loop invariant if the loop might directly write
     * to the elements of any of the accessed arrays. This could invoke an
     * inline path which updates the length. There is no need to check the
     * modset for direct 'length' writes, as we don't generate inline paths
     * updating array lengths.
     */
    for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
        if (objTypes->getSingleObject(i) != NULL)
            return NULL;
        TypeObject *object = objTypes->getTypeObject(i);
        if (object && hasModifiedProperty(object, JSID_VOID))
            return NULL;
    }
    objTypes->addFreeze(cx);

    uint32 which = frame.allocTemporary();
    if (which == uint32(-1))
        return NULL;
    FrameEntry *fe = frame.getTemporary(which);

    JaegerSpew(JSpew_Analysis, "Using %s for loop invariant dense array length of %s\n",
               frame.entryName(fe), frame.entryName(objSlot));

    InvariantEntry entry;
    entry.kind = InvariantEntry::DENSE_ARRAY_LENGTH;
    entry.u.array.arraySlot = objSlot;
    entry.u.array.temporary = which;
    invariantEntries.append(entry);

    return fe;
}

FrameEntry *
LoopState::invariantProperty(const CrossSSAValue &obj, jsid id)
{
    if (skipAnalysis)
        return NULL;

    if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom))
        return NULL;

    uint32 objSlot;
    int32 objConstant;
    if (!getEntryValue(obj, &objSlot, &objConstant) || objSlot == UNASSIGNED || objConstant != 0)
        return NULL;

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::INVARIANT_PROPERTY &&
            entry.u.property.objectSlot == objSlot &&
            entry.u.property.id == id) {
            return frame.getTemporary(entry.u.property.temporary);
        }
    }

    if (!loopInvariantEntry(objSlot))
        return NULL;

    /* Check that the property is definite and not written anywhere in the loop. */
    TypeSet *objTypes = ssa->getValueTypes(obj);
    if (objTypes->unknownObject() || objTypes->getObjectCount() != 1)
        return NULL;
    TypeObject *object = objTypes->getTypeObject(0);
    if (!object || object->unknownProperties() || hasModifiedProperty(object, id) || id != MakeTypeId(cx, id))
        return NULL;
    TypeSet *propertyTypes = object->getProperty(cx, id, false);
    if (!propertyTypes)
        return NULL;
    if (!propertyTypes->isDefiniteProperty() || propertyTypes->isOwnProperty(cx, object, true))
        return NULL;
    objTypes->addFreeze(cx);

    uint32 which = frame.allocTemporary();
    if (which == uint32(-1))
        return NULL;
    FrameEntry *fe = frame.getTemporary(which);

    JaegerSpew(JSpew_Analysis, "Using %s for loop invariant property of %s\n",
               frame.entryName(fe), frame.entryName(objSlot));

    InvariantEntry entry;
    entry.kind = InvariantEntry::INVARIANT_PROPERTY;
    entry.u.property.objectSlot = objSlot;
    entry.u.property.propertySlot = propertyTypes->definiteSlot();
    entry.u.property.temporary = which;
    entry.u.property.id = id;
    invariantEntries.append(entry);

    return fe;
}

bool
LoopState::cannotIntegerOverflow(const CrossSSAValue &pushed)
{
    if (skipAnalysis)
        return false;

    int32 min, max;
    if (computeInterval(pushed, &min, &max)) {
        JaegerSpew(JSpew_Analysis, "Integer operation fits in range [%d, %d]\n", min, max);
        return true;
    }

    /*
     * Compute a slot and constant such that the result of the binary op is
     * 'slot + constant', where slot is expressed in terms of its value at
     * the head of the loop.
     */
    JS_ASSERT(pushed.v.kind() == SSAValue::PUSHED);
    jsbytecode *PC = ssa->getFrame(pushed.frame).script->code + pushed.v.pushedOffset();
    ScriptAnalysis *analysis = ssa->getFrame(pushed.frame).script->analysis();

    if (!analysis->integerOperation(cx, PC))
        return false;

    uint32 baseSlot = UNASSIGNED;
    int32 baseConstant = 0;
    JSOp op = JSOp(*PC);
    switch (op) {

      case JSOP_INCLOCAL:
      case JSOP_LOCALINC:
      case JSOP_INCARG:
      case JSOP_ARGINC: {
        CrossSSAValue cv(pushed.frame, analysis->poppedValue(PC, 0));
        if (!getEntryValue(cv, &baseSlot, &baseConstant))
            return false;
        if (!SafeAdd(baseConstant, 1, &baseConstant))
            return false;
        break;
      }

      case JSOP_DECLOCAL:
      case JSOP_LOCALDEC:
      case JSOP_DECARG:
      case JSOP_ARGDEC: {
        CrossSSAValue cv(pushed.frame, analysis->poppedValue(PC, 0));
        if (!getEntryValue(cv, &baseSlot, &baseConstant))
            return false;
        if (!SafeSub(baseConstant, 1, &baseConstant))
            return false;
        break;
      }

      case JSOP_ADD:
      case JSOP_SUB: {
        uint32 lhs = UNASSIGNED, rhs = UNASSIGNED;
        int32 lhsconstant = 0, rhsconstant = 0;
        CrossSSAValue lcv(pushed.frame, analysis->poppedValue(PC, 1));
        CrossSSAValue rcv(pushed.frame, analysis->poppedValue(PC, 0));
        if (!getEntryValue(lcv, &lhs, &lhsconstant))
            return false;
        if (!getEntryValue(rcv, &rhs, &rhsconstant))
            return false;
        if (op == JSOP_ADD) {
            if (!SafeAdd(lhsconstant, rhsconstant, &baseConstant))
                return false;
            if (lhs != UNASSIGNED && rhs != UNASSIGNED)
                return false;
            baseSlot = (lhs == UNASSIGNED) ? rhs : lhs;
        } else {
            if (!SafeSub(lhsconstant, rhsconstant, &baseConstant))
                return false;
            if (rhs != UNASSIGNED)
                return false;
            baseSlot = lhs;
        }
        break;
      }

      default:
        return false;
    }

    if (baseSlot == UNASSIGNED)
        return false;

    JaegerSpew(JSpew_Analysis, "Trying to hoist integer overflow check on %s + %d\n",
               frame.entryName(baseSlot), baseConstant);

    if (baseConstant == 0) {
        JaegerSpew(JSpew_Analysis, "Vacuously holds\n");
        return true;
    }

    if (baseConstant < 0) {
        /*
         * If the access is of the form 'y + a' where a is negative and we know
         * that y >= b at the head of the loop, we can eliminate as follows:
         *
         * y + a >= INT_MIN
         * b + a >= INT_MIN
         */
        if (baseSlot == testLHS && !testLessEqual && testRHS == UNASSIGNED) {
            int32 constant;
            if (!SafeAdd(testConstant, baseConstant, &constant))
                return false;

            JaegerSpew(JSpew_Analysis, "Loop test comparison must hold\n");
            return true;
        }

        JaegerSpew(JSpew_Analysis, "No match found\n");
        return false;
    }

    /*
     * If the access is of the form 'y + a' where we know that y <= z + b
     * (both in terms of state at the head of the loop), hoist as follows:
     *
     * y + a <= INT_MAX
     * y <= INT_MAX - a
     * z + b <= INT_MAX - a
     * z <= INT_MAX - (a + b)
     */
    if (baseSlot == testLHS && testLessEqual) {
        int32 constant;
        if (!SafeAdd(testConstant, baseConstant, &constant))
            return false;

        if (testRHS == UNASSIGNED || constant <= 0) {
            /*
             * Reduces to '(a + b) <= INT_MAX', which SafeAdd ensures,
             * or 'z <= INT_MAX', which integer checks on z ensure.
             */
            JaegerSpew(JSpew_Analysis, "Loop test comparison must hold\n");
            return true;
        }

        constant = JSVAL_INT_MAX - constant;

        addRangeCheck(testRHS, UNASSIGNED, constant);
        return true;
    }

    /*
     * If the access is of the form 'y + a' where we know that z >= b at the
     * head of the loop and y has a linear relationship with z such that
     * (y + z) always has the same value at the head of the loop, hoist as
     * follows:
     *
     * y + a <= INT_MAX
     * y + z <= INT_MAX + z - a
     * y + z <= INT_MAX + b - a
     */
    if (hasTestLinearRelationship(baseSlot)) {
        int32 constant;
        if (!SafeSub(testConstant, baseConstant, &constant))
            return false;

        if (constant >= 0)
            constant = 0;
        constant = JSVAL_INT_MAX + constant;

        addRangeCheck(baseSlot, testLHS, constant);
        return true;
    }

    JaegerSpew(JSpew_Analysis, "No match found\n");
    return false;
}

bool
LoopState::ignoreIntegerOverflow(const CrossSSAValue &pushed)
{
    if (skipAnalysis || unknownModset || !constrainedLoop)
        return false;

    /*
     * Under certain circumstances, we can ignore arithmetic overflow in adds
     * and multiplies. As long as the result of the add/mul is either only used
     * in bitwise arithmetic or is only used in additions whose result is only
     * used in bitwise arithmetic, then the conversion to integer performed by
     * the bitop will undo the effect of the earlier overflow. There are two
     * additional things to watch for before performing this transformation:
     *
     * 1. If the overflowing double is sufficiently large that it loses
     * precision in its lower bits (with a 48 bit mantissa, this may happen for
     * values of N >= 2^48), the resulting rounding could change the result.
     * We don't ignore overflow on multiplications without range information,
     * though assume that no amount of integer additions we perform in a single
     * loop iteration will overflow 2^48.
     *
     * 2. If used in an addition with a string, the overflowing and truncated
     * results may produce different values (e.g. '(x + "e3") & y'). We must
     * restrict the loop body in such a way that no string operand is possible
     * or becomes possible due to dynamic type changes for such additions.
     * constrainedLoop indicates whether the only operations which can happen
     * in the loop body are int/double arithmetic and bitops, and reads/writes
     * from known dense arrays which can only produce ints and doubles.
     */

    /* This value must be in the outer loop: loops with inline calls are not constrained. */
    JS_ASSERT(pushed.frame == CrossScriptSSA::OUTER_FRAME);

    JS_ASSERT(pushed.v.kind() == SSAValue::PUSHED);
    jsbytecode *PC = outerScript->code + pushed.v.pushedOffset();

    JSOp op = JSOp(*PC);
    if (op != JSOP_MUL && op != JSOP_ADD)
        return false;

    if (valueFlowsToBitops(pushed.v)) {
        JaegerSpew(JSpew_Analysis, "Integer result flows to bitops\n");
        return true;
    }

    if (op == JSOP_MUL) {
        /*
         * If the multiply will only be used in an addition, negative zero can
         * be ignored as long as the other operand in the addition cannot be
         * negative zero.
         */
        if (!outerAnalysis->trackUseChain(pushed.v))
            return false;

        SSAUseChain *use = outerAnalysis->useChain(pushed.v);
        if (!use || use->next || !use->popped || outerScript->code[use->offset] != JSOP_ADD)
            return false;

        if (use->u.which == 1) {
            /*
             * Only ignore negative zero if this is the RHS of an addition.
             * Otherwise the result of the other side could change to a double
             * after the first LHS has been computed, and be affected by a
             * negative zero LHS.
             */
            return false;
        }

        TypeSet *lhsTypes = outerAnalysis->poppedTypes(use->offset, 1);
        if (lhsTypes->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
            return false;

        JaegerSpew(JSpew_Analysis, "Integer result is RHS in integer addition\n");
        return true;
    }

    return false;
}

bool
LoopState::valueFlowsToBitops(const analyze::SSAValue &v)
{
    /*
     * Determine whether v can only be used in a bitop later in the same
     * iteration of this loop, or in additions whose result is also only
     * used in such a bitop.
     */
    if (!outerAnalysis->trackUseChain(v))
        return false;

    SSAUseChain *use = outerAnalysis->useChain(v);
    while (use) {
        if (!use->popped) {
            /*
             * Ignore variables used in phi nodes, so long as the variable is
             * dead at the phi. We don't track live variables across back edges
             * or complex control flow.
             */
            if (v.kind() == SSAValue::VAR) {
                analyze::Lifetime *lifetime = outerAnalysis->liveness(v.varSlot()).live(use->offset);
                if (!lifetime) {
                    use = use->next;
                    continue;
                }
            }
            return false;
        }

        if (use->offset > lifetime->backedge)
            return false;

        jsbytecode *pc = outerScript->code + use->offset;
        JSOp op = JSOp(*pc);
        switch (op) {
          case JSOP_ADD:
          case JSOP_GETLOCAL: {
            SSAValue pushv;
            pushv.initPushed(use->offset, 0);
            if (!valueFlowsToBitops(pushv))
                return false;
            break;
          }

          case JSOP_SETLOCAL: {
            uint32 slot = GetBytecodeSlot(outerScript, pc);
            if (!outerAnalysis->trackSlot(slot))
                return false;
            SSAValue writev;
            writev.initWritten(slot, use->offset);
            if (!valueFlowsToBitops(writev))
                return false;
            break;
          }

          case JSOP_BITAND:
          case JSOP_BITOR:
          case JSOP_BITXOR:
          case JSOP_RSH:
          case JSOP_LSH:
          case JSOP_URSH:
          case JSOP_BITNOT:
            break;

          default:
            return false;
        }

        use = use->next;
    }

    return true;
}

void
LoopState::restoreInvariants(jsbytecode *pc, Assembler &masm,
                             Vector<TemporaryCopy> *temporaryCopies, Vector<Jump> *jumps)
{
    /*
     * Restore all invariants in memory when entering the loop or after any
     * scripted or C++ call, and check that all hoisted conditions still hold.
     * Care should be taken not to clobber the return register or callee-saved
     * registers, which may still be live after some calls.
     */

    Registers regs(Registers::TempRegs);
    regs.takeReg(Registers::ReturnReg);
    if (regs.hasReg(JSReturnReg_Data))
        regs.takeReg(JSReturnReg_Data);
    if (regs.hasReg(JSReturnReg_Type))
        regs.takeReg(JSReturnReg_Type);

    RegisterID T0 = regs.takeAnyReg().reg();
    RegisterID T1 = regs.takeAnyReg().reg();

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        const InvariantEntry &entry = invariantEntries[i];
        switch (entry.kind) {

          case InvariantEntry::DENSE_ARRAY_BOUNDS_CHECK:
          case InvariantEntry::TYPED_ARRAY_BOUNDS_CHECK: {
            /*
             * Hoisted bounds checks always have preceding invariant slots
             * in the invariant list, so don't recheck this is an object.
             */
            masm.loadPayload(frame.addressOf(entry.u.check.arraySlot), T0);
            if (entry.kind == InvariantEntry::DENSE_ARRAY_BOUNDS_CHECK)
                masm.load32(Address(T0, JSObject::offsetOfInitializedLength()), T0);
            else
                masm.loadPayload(Address(T0, TypedArray::lengthOffset()), T0);

            int32 constant = entry.u.check.constant;

            if (entry.u.check.valueSlot1 != uint32(-1)) {
                constant += adjustConstantForIncrement(pc, entry.u.check.valueSlot1);
                masm.loadPayload(frame.addressOf(entry.u.check.valueSlot1), T1);
                if (entry.u.check.valueSlot2 != uint32(-1)) {
                    constant += adjustConstantForIncrement(pc, entry.u.check.valueSlot2);
                    Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                     frame.addressOf(entry.u.check.valueSlot2), T1);
                    jumps->append(overflow);
                }
                if (constant != 0) {
                    Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                     Imm32(constant), T1);
                    jumps->append(overflow);
                }
                Jump j = masm.branch32(Assembler::LessThanOrEqual, T0, T1);
                jumps->append(j);
            } else {
                Jump j = masm.branch32(Assembler::LessThanOrEqual, T0,
                                       Imm32(constant));
                jumps->append(j);
            }
            break;
          }

          case InvariantEntry::RANGE_CHECK: {
            int32 constant = 0;

            constant += adjustConstantForIncrement(pc, entry.u.check.valueSlot1);
            masm.loadPayload(frame.addressOf(entry.u.check.valueSlot1), T0);
            if (entry.u.check.valueSlot2 != uint32(-1)) {
                constant += adjustConstantForIncrement(pc, entry.u.check.valueSlot2);
                Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                 frame.addressOf(entry.u.check.valueSlot2), T0);
                jumps->append(overflow);
            }
            if (constant != 0) {
                Jump overflow = masm.branchAdd32(Assembler::Overflow, Imm32(constant), T0);
                jumps->append(overflow);
            }
            Jump j = masm.branch32(Assembler::GreaterThan, T0, Imm32(entry.u.check.constant));
            jumps->append(j);
            break;
          }

          case InvariantEntry::NEGATIVE_CHECK: {
            masm.loadPayload(frame.addressOf(entry.u.check.valueSlot1), T0);
            if (entry.u.check.constant != 0) {
                Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                 Imm32(entry.u.check.constant), T0);
                jumps->append(overflow);
            }
            Jump j = masm.branch32(Assembler::LessThan, T0, Imm32(0));
            jumps->append(j);
            break;
          }

          case InvariantEntry::DENSE_ARRAY_SLOTS:
          case InvariantEntry::DENSE_ARRAY_LENGTH: {
            uint32 array = entry.u.array.arraySlot;
            Jump notObject = masm.testObject(Assembler::NotEqual, frame.addressOf(array));
            jumps->append(notObject);
            masm.loadPayload(frame.addressOf(array), T0);

            uint32 offset = (entry.kind == InvariantEntry::DENSE_ARRAY_SLOTS)
                ? JSObject::offsetOfSlots()
                : offsetof(JSObject, privateData);

            Address address = frame.addressOf(frame.getTemporary(entry.u.array.temporary));

            masm.loadPtr(Address(T0, offset), T0);
            if (entry.kind == InvariantEntry::DENSE_ARRAY_LENGTH)
                masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), T0, address);
            else
                masm.storePayload(T0, address);
            break;
          }

          case InvariantEntry::TYPED_ARRAY_SLOTS:
          case InvariantEntry::TYPED_ARRAY_LENGTH: {
            uint32 array = entry.u.array.arraySlot;
            Jump notObject = masm.testObject(Assembler::NotEqual, frame.addressOf(array));
            jumps->append(notObject);
            masm.loadPayload(frame.addressOf(array), T0);

            Address address = frame.addressOf(frame.getTemporary(entry.u.array.temporary));

            if (entry.kind == InvariantEntry::TYPED_ARRAY_LENGTH) {
                masm.loadPayload(Address(T0, TypedArray::lengthOffset()), T0);
                masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), T0, address);
            } else {
                masm.loadPtr(Address(T0, js::TypedArray::dataOffset()), T0);
                masm.storePayload(T0, address);
            }
            break;
          }

          case InvariantEntry::INVARIANT_ARGS_BASE: {
            Address address = frame.addressOf(frame.getTemporary(entry.u.array.temporary));
            masm.loadFrameActuals(outerScript->function(), T0);
            masm.storePayload(T0, address);
            break;
          }

          case InvariantEntry::INVARIANT_ARGS_LENGTH: {
            Address address = frame.addressOf(frame.getTemporary(entry.u.array.temporary));
            masm.load32(Address(JSFrameReg, StackFrame::offsetOfArgs()), T0);
            masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), T0, address);
            break;
          }

          case InvariantEntry::INVARIANT_PROPERTY: {
            uint32 object = entry.u.property.objectSlot;
            Jump notObject = masm.testObject(Assembler::NotEqual, frame.addressOf(object));
            jumps->append(notObject);
            masm.loadPayload(frame.addressOf(object), T0);

            masm.loadInlineSlot(T0, entry.u.property.propertySlot, T1, T0);
            masm.storeValueFromComponents(T1, T0,
                frame.addressOf(frame.getTemporary(entry.u.property.temporary)));
            break;
          }

          default:
            JS_NOT_REACHED("Bad invariant kind");
        }
    }

    /*
     * If there were any copies of temporaries on the stack, make sure the
     * value we just reconstructed matches the stored value of that temporary.
     * We sync the entire stack before calls, so the copy's slot holds the old
     * value, but in future code we will assume the copy is valid and use the
     * changed value of the invariant.
     */

    for (unsigned i = 0; temporaryCopies && i < temporaryCopies->length(); i++) {
        const TemporaryCopy &copy = (*temporaryCopies)[i];
        masm.compareValue(copy.copy, copy.temporary, T0, T1, jumps);
    }

    if (temporaryCopies)
        cx->delete_(temporaryCopies);
}

/* Loop analysis methods. */

/*
 * Get any slot/constant accessed by a loop test operand, in terms of its value
 * at the start of the next loop iteration.
 */
bool
LoopState::getLoopTestAccess(const SSAValue &v, uint32 *pslot, int32 *pconstant)
{
    *pslot = UNASSIGNED;
    *pconstant = 0;

    if (v.kind() == SSAValue::PHI || v.kind() == SSAValue::VAR) {
        /*
         * Getting the value of a variable at a previous offset. Check that it
         * is not updated before the start of the next loop iteration.
         */
        uint32 slot;
        uint32 offset;
        if (v.kind() == SSAValue::PHI) {
            slot = v.phiSlot();
            offset = v.phiOffset();
        } else {
            slot = v.varSlot();
            offset = v.varInitial() ? 0 : v.varOffset();
        }
        if (outerAnalysis->slotEscapes(slot))
            return false;
        if (outerAnalysis->liveness(slot).firstWrite(offset + 1, lifetime->backedge) != uint32(-1))
            return false;
        *pslot = slot;
        *pconstant = 0;
        return true;
    }

    jsbytecode *pc = outerScript->code + v.pushedOffset();

    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];

    /*
     * If the pc is modifying a variable and the value tested is its earlier value
     * (e.g. 'x++ < n'), we need to account for the modification --- at the start
     * of the next iteration, the value compared will have been 'x - 1'.
     * Note that we don't need to worry about other accesses to the variable
     * in the condition like 'x++ < x', as loop tests where both operands are
     * modified by the loop are rejected.
     */

    switch (op) {

      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        if (!outerAnalysis->integerOperation(cx, pc))
            return false;
        uint32 slot = GetBytecodeSlot(outerScript, pc);
        if (outerAnalysis->slotEscapes(slot))
            return false;

        *pslot = slot;
        if (cs->format & JOF_POST) {
            if (cs->format & JOF_INC)
                *pconstant = -1;
            else
                *pconstant = 1;
        }
        return true;
      }

      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_INT8:
      case JSOP_INT32:
        *pconstant = GetBytecodeInteger(pc);
        return true;

      default:
        return false;
    }
}

void
LoopState::analyzeLoopTest()
{
    if (cc.debugMode())
        return;

    /* Don't handle do-while loops. */
    if (lifetime->entry == lifetime->head)
        return;

    /* Don't handle loops with branching inside their condition. */
    if (lifetime->entry < lifetime->lastBlock)
        return;

    /* Get the test performed before branching. */
    jsbytecode *backedge = outerScript->code + lifetime->backedge;
    if (JSOp(*backedge) != JSOP_IFNE)
        return;
    const SSAValue &test = outerAnalysis->poppedValue(backedge, 0);
    if (test.kind() != SSAValue::PUSHED)
        return;
    JSOp cmpop = JSOp(outerScript->code[test.pushedOffset()]);
    switch (cmpop) {
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_LT:
      case JSOP_LE:
        break;
      default:
        return;
    }

    SSAValue one = outerAnalysis->poppedValue(test.pushedOffset(), 1);
    SSAValue two = outerAnalysis->poppedValue(test.pushedOffset(), 0);

    /* The test must be comparing known integers. */
    if (outerAnalysis->getValueTypes(one)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32 ||
        outerAnalysis->getValueTypes(two)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
        return;
    }

    /* Reverse the condition if the RHS is modified by the loop. */
    uint32 swapRHS;
    int32 swapConstant;
    if (getLoopTestAccess(two, &swapRHS, &swapConstant)) {
        if (swapRHS != UNASSIGNED && outerAnalysis->liveness(swapRHS).firstWrite(lifetime) != uint32(-1)) {
            SSAValue tmp = one;
            one = two;
            two = tmp;
            cmpop = ReverseCompareOp(cmpop);
        }
    }

    uint32 lhs;
    int32 lhsConstant;
    if (!getLoopTestAccess(one, &lhs, &lhsConstant))
        return;

    uint32 rhs = UNASSIGNED;
    int32 rhsConstant = 0;
    CrossSSAValue rhsv(CrossScriptSSA::OUTER_FRAME, two);
    if (!getEntryValue(rhsv, &rhs, &rhsConstant))
        return;
    if (!loopInvariantEntry(rhs))
        return;

    if (lhs == UNASSIGNED)
        return;

    int32 constant;
    if (!SafeSub(rhsConstant, lhsConstant, &constant))
        return;

    /* x > y ==> x >= y + 1 */
    if (cmpop == JSOP_GT && !SafeAdd(constant, 1, &constant))
        return;

    /* x < y ==> x <= y - 1 */
    if (cmpop == JSOP_LT && !SafeSub(constant, 1, &constant))
        return;

    /* Passed all filters, this is a loop test we can capture. */

    this->testLHS = lhs;
    this->testRHS = rhs;
    this->testConstant = constant;
    this->testLessEqual = (cmpop == JSOP_LT || cmpop == JSOP_LE);
}

void
LoopState::analyzeLoopIncrements()
{
    if (cc.debugMode())
        return;

    /*
     * Find locals and arguments which are used in exactly one inc/dec operation in every
     * iteration of the loop (we only match against the last basic block, but could
     * also handle the first basic block).
     */

    for (uint32 slot = ArgSlot(0); slot < LocalSlot(outerScript, outerScript->nfixed); slot++) {
        if (outerAnalysis->slotEscapes(slot))
            continue;

        uint32 offset = outerAnalysis->liveness(slot).onlyWrite(lifetime);
        if (offset == uint32(-1) || offset < lifetime->lastBlock)
            continue;

        jsbytecode *pc = outerScript->code + offset;
        JSOp op = JSOp(*pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        if (cs->format & (JOF_INC | JOF_DEC)) {
            if (!outerAnalysis->integerOperation(cx, pc))
                continue;

            Increment inc;
            inc.slot = slot;
            inc.offset = offset;
            increments.append(inc);
        }
    }
}

bool
LoopState::definiteArrayAccess(const SSAValue &obj, const SSAValue &index)
{
    /*
     * Check that an index on obj is definitely accessing a dense array, giving
     * either a value modelled by the pushed types or a hole. This needs to be
     * robust against recompilations that could be triggered inside the loop:
     * the array must be loop invariant, and the index must definitely be an
     * integer.
     *
     * This is used to determine if we can ignore possible integer overflow in
     * an operation; if this site could read a non-integer element out of the
     * array or invoke a scripted getter/setter, it could produce a string or
     * other value by which the overflow could be observed.
     */

    TypeSet *objTypes = outerAnalysis->getValueTypes(obj);
    TypeSet *elemTypes = outerAnalysis->getValueTypes(index);

    if (objTypes->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT ||
        elemTypes->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
        return false;
    }

    if (objTypes->hasObjectFlags(cx, OBJECT_FLAG_NON_DENSE_ARRAY))
        return false;

    if (cc.arrayPrototypeHasIndexedProperty())
        return false;

    uint32 objSlot;
    int32 objConstant;
    CrossSSAValue objv(CrossScriptSSA::OUTER_FRAME, obj);
    if (!getEntryValue(objv, &objSlot, &objConstant) || objSlot == UNASSIGNED || objConstant != 0)
        return false;
    if (!loopInvariantEntry(objSlot))
        return false;

    /* Bitops must produce integers. */
    if (index.kind() == SSAValue::PUSHED) {
        JSOp op = JSOp(outerScript->code[index.pushedOffset()]);
        switch (op) {
          case JSOP_BITAND:
          case JSOP_BITOR:
          case JSOP_BITXOR:
          case JSOP_BITNOT:
          case JSOP_RSH:
          case JSOP_LSH:
          case JSOP_URSH:
            return true;
          default:;
        }
    }

    uint32 indexSlot;
    int32 indexConstant;
    CrossSSAValue indexv(CrossScriptSSA::OUTER_FRAME, index);
    if (!getEntryValue(indexv, &indexSlot, &indexConstant))
        return false;

    /*
     * The index is determined from a variable's value at loop entry. We don't
     * carry values with ignored overflows around loop back edges, so will know
     * the index is a non-integer before such overflows are ignored.
     */
    return true;
}

void
LoopState::analyzeLoopBody(unsigned frame)
{
    if (cc.debugMode()) {
        skipAnalysis = true;
        return;
    }

    JSScript *script = ssa->getFrame(frame).script;
    analyze::ScriptAnalysis *analysis = script->analysis();
    JS_ASSERT(analysis && !analysis->failed() && analysis->ranInference());

    /*
     * The temporaries need to be positioned after all values in the deepest
     * inlined frame plus another stack frame pushed by, e.g. ic::Call.
     * This new frame will have been partially initialized by the call, and
     * we don't want to scribble on that frame when restoring invariants.
     */
    temporariesStart =
        Max<uint32>(temporariesStart,
                    ssa->getFrame(frame).depth + VALUES_PER_STACK_FRAME * 2 + script->nslots);

    if (script->failedBoundsCheck || analysis->localsAliasStack())
        skipAnalysis = true;

    /* Analyze the entire script for frames inlined in the loop body. */
    unsigned start = (frame == CrossScriptSSA::OUTER_FRAME) ? lifetime->head + JSOP_LOOPHEAD_LENGTH : 0;
    unsigned end = (frame == CrossScriptSSA::OUTER_FRAME) ? lifetime->backedge : script->length;

    unsigned offset = start;
    while (offset < end) {
        jsbytecode *pc = script->code + offset;
        uint32 successorOffset = offset + analyze::GetBytecodeLength(pc);

        analyze::Bytecode *opinfo = analysis->maybeCode(offset);
        if (!opinfo) {
            offset = successorOffset;
            continue;
        }

        JSOp op = JSOp(*pc);

        /* Don't do any hoisting for outer loops in case of nesting. */
        if (op == JSOP_LOOPHEAD)
            skipAnalysis = true;

        switch (op) {

          case JSOP_CALL: {
            /*
             * Don't hoist within this loop unless calls at this site are inlined.
             * :XXX: also recognize native calls which will be inlined.
             */
            bool foundInline = false;
            for (unsigned i = 0; !foundInline && i < ssa->numFrames(); i++) {
                if (ssa->iterFrame(i).parent == frame && ssa->iterFrame(i).parentpc == pc)
                    foundInline = true;
            }
            if (!foundInline)
                skipAnalysis = true;
            break;
          }

          case JSOP_EVAL:
          case JSOP_FUNCALL:
          case JSOP_FUNAPPLY:
          case JSOP_NEW:
            skipAnalysis = true;
            break;

          case JSOP_SETELEM: {
            SSAValue objValue = analysis->poppedValue(pc, 2);
            SSAValue elemValue = analysis->poppedValue(pc, 1);

            TypeSet *objTypes = analysis->getValueTypes(objValue);
            TypeSet *elemTypes = analysis->getValueTypes(elemValue);

            /*
             * Mark the modset as unknown if the index might be non-integer,
             * we don't want to consider the SETELEM PIC here.
             */
            if (objTypes->unknownObject() || elemTypes->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
                unknownModset = true;
                break;
            }

            objTypes->addFreeze(cx);
            for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
                TypeObject *object = objTypes->getTypeObject(i);
                if (!object)
                    continue;
                if (!addModifiedProperty(object, JSID_VOID))
                    return;
                if (analysis->getCode(pc).arrayWriteHole && !addGrowArray(object))
                    return;
            }

            if (constrainedLoop && !definiteArrayAccess(objValue, elemValue))
                constrainedLoop = false;
            break;
          }

          case JSOP_GETELEM: {
            SSAValue objValue = analysis->poppedValue(pc, 1);
            SSAValue elemValue = analysis->poppedValue(pc, 0);

            if (constrainedLoop && !definiteArrayAccess(objValue, elemValue))
                constrainedLoop = false;
            break;
          }

          case JSOP_SETPROP:
          case JSOP_SETMETHOD: {
            JSAtom *atom = script->getAtom(js_GetIndexFromBytecode(cx, script, pc, 0));
            jsid id = MakeTypeId(cx, ATOM_TO_JSID(atom));

            TypeSet *objTypes = analysis->poppedTypes(pc, 1);
            if (objTypes->unknownObject()) {
                unknownModset = true;
                break;
            }

            objTypes->addFreeze(cx);
            for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
                TypeObject *object = objTypes->getTypeObject(i);
                if (!object)
                    continue;
                if (!addModifiedProperty(object, id))
                    continue;
            }

            constrainedLoop = false;
            break;
          }

          case JSOP_ENUMELEM:
          case JSOP_ENUMCONSTELEM:
            unknownModset = true;
            break;

          case JSOP_LOOPHEAD:
          case JSOP_POP:
          case JSOP_ZERO:
          case JSOP_ONE:
          case JSOP_INT8:
          case JSOP_INT32:
          case JSOP_UINT16:
          case JSOP_UINT24:
          case JSOP_FALSE:
          case JSOP_TRUE:
          case JSOP_GETARG:
          case JSOP_SETARG:
          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC:
          case JSOP_THIS:
          case JSOP_GETLOCAL:
          case JSOP_SETLOCAL:
          case JSOP_SETLOCALPOP:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC:
          case JSOP_IFEQ:
          case JSOP_IFEQX:
          case JSOP_IFNE:
          case JSOP_IFNEX:
          case JSOP_AND:
          case JSOP_ANDX:
          case JSOP_OR:
          case JSOP_ORX:
          case JSOP_GOTO:
          case JSOP_GOTOX:
            break;

          case JSOP_ADD:
          case JSOP_SUB:
          case JSOP_MUL:
          case JSOP_MOD:
          case JSOP_DIV:
          case JSOP_BITAND:
          case JSOP_BITOR:
          case JSOP_BITXOR:
          case JSOP_RSH:
          case JSOP_LSH:
          case JSOP_URSH:
          case JSOP_EQ:
          case JSOP_NE:
          case JSOP_LT:
          case JSOP_LE:
          case JSOP_GT:
          case JSOP_GE:
          case JSOP_STRICTEQ:
          case JSOP_STRICTNE: {
            JSValueType type = analysis->poppedTypes(pc, 1)->getKnownTypeTag(cx);
            if (type != JSVAL_TYPE_INT32 && type != JSVAL_TYPE_DOUBLE)
                constrainedLoop = false;
          }
          /* FALLTHROUGH */

          case JSOP_POS:
          case JSOP_NEG:
          case JSOP_BITNOT: {
            JSValueType type = analysis->poppedTypes(pc, 0)->getKnownTypeTag(cx);
            if (type != JSVAL_TYPE_INT32 && type != JSVAL_TYPE_DOUBLE)
                constrainedLoop = false;
            break;
          }

          default:
            constrainedLoop = false;
            break;
        }

        offset = successorOffset;
    }
}

bool
LoopState::addGrowArray(TypeObject *object)
{
    static const uint32 MAX_SIZE = 10;
    for (unsigned i = 0; i < growArrays.length(); i++) {
        if (growArrays[i] == object)
            return true;
    }
    if (growArrays.length() >= MAX_SIZE) {
        unknownModset = true;
        return false;
    }
    growArrays.append(object);

    return true;
}

bool
LoopState::addModifiedProperty(TypeObject *object, jsid id)
{
    static const uint32 MAX_SIZE = 20;
    for (unsigned i = 0; i < modifiedProperties.length(); i++) {
        if (modifiedProperties[i].object == object && modifiedProperties[i].id == id)
            return true;
    }
    if (modifiedProperties.length() >= MAX_SIZE) {
        unknownModset = true;
        return false;
    }

    ModifiedProperty property;
    property.object = object;
    property.id = id;
    modifiedProperties.append(property);

    return true;
}

bool
LoopState::hasGrowArray(TypeObject *object)
{
    if (unknownModset)
        return true;
    for (unsigned i = 0; i < growArrays.length(); i++) {
        if (growArrays[i] == object)
            return true;
    }
    return false;
}

bool
LoopState::hasModifiedProperty(TypeObject *object, jsid id)
{
    if (unknownModset)
        return true;
    id = MakeTypeId(cx, id);
    for (unsigned i = 0; i < modifiedProperties.length(); i++) {
        if (modifiedProperties[i].object == object && modifiedProperties[i].id == id)
            return true;
    }
    return false;
}

uint32
LoopState::getIncrement(uint32 slot)
{
    for (unsigned i = 0; i < increments.length(); i++) {
        if (increments[i].slot == slot)
            return increments[i].offset;
    }
    return uint32(-1);
}

int32
LoopState::adjustConstantForIncrement(jsbytecode *pc, uint32 slot)
{
    /*
     * The only terms that can appear in a hoisted bounds check are either
     * loop invariant or are incremented or decremented exactly once in each
     * iteration of the loop. Depending on the current pc in the body of the
     * loop, return a constant adjustment if an increment/decrement for slot
     * has not yet happened, such that 'slot + n' at this point is the value
     * of slot at the start of the next iteration.
     */
    uint32 offset = getIncrement(slot);

    /*
     * Note the '<' here. If this PC is at one of the increment opcodes, then
     * behave as if the increment has not happened yet. This is needed for loop
     * entry points, which can be directly at an increment. We won't rejoin
     * after the increment, as we only take stub calls in such situations on
     * integer overflow, which will disable hoisted conditions involving the
     * variable anyways.
     */
    if (offset == uint32(-1) || offset < uint32(pc - outerScript->code))
        return 0;

    switch (JSOp(outerScript->code[offset])) {
      case JSOP_INCLOCAL:
      case JSOP_LOCALINC:
      case JSOP_INCARG:
      case JSOP_ARGINC:
        return 1;
      case JSOP_DECLOCAL:
      case JSOP_LOCALDEC:
      case JSOP_DECARG:
      case JSOP_ARGDEC:
        return -1;
      default:
        JS_NOT_REACHED("Bad op");
        return 0;
    }
}

bool
LoopState::getEntryValue(const CrossSSAValue &iv, uint32 *pslot, int32 *pconstant)
{
    CrossSSAValue cv = ssa->foldValue(iv);

    JSScript *script = ssa->getFrame(cv.frame).script;
    ScriptAnalysis *analysis = script->analysis();
    const SSAValue &v = cv.v;

    /*
     * For a stack value popped by the bytecode at offset, try to get an
     * expression 'slot + constant' with the same value as the stack value
     * and expressed in terms of the state at loop entry.
     */

    if (v.kind() == SSAValue::PHI) {
        if (cv.frame != CrossScriptSSA::OUTER_FRAME)
            return false;
        if (v.phiSlot() >= TotalSlots(script))
            return false;
        if (v.phiOffset() > lifetime->head &&
            outerAnalysis->liveness(v.phiSlot()).firstWrite(lifetime) < v.phiOffset()) {
            return false;
        }
        *pslot = v.phiSlot();
        *pconstant = 0;
        return true;
    }

    if (v.kind() == SSAValue::VAR) {
        if (cv.frame != CrossScriptSSA::OUTER_FRAME)
            return false;
        if (v.varInitial() || v.varOffset() < lifetime->head) {
            *pslot = v.varSlot();
            *pconstant = 0;
            return true;
        }
    }

    if (v.kind() != SSAValue::PUSHED)
        return false;

    jsbytecode *pc = script->code + v.pushedOffset();
    JSOp op = (JSOp)*pc;

    switch (op) {

      case JSOP_GETLOCAL:
      case JSOP_LOCALINC:
      case JSOP_INCLOCAL:
      case JSOP_GETARG:
      case JSOP_ARGINC:
      case JSOP_INCARG: {
        if (cv.frame != CrossScriptSSA::OUTER_FRAME || !analysis->integerOperation(cx, pc))
            return false;
        uint32 slot = GetBytecodeSlot(outerScript, pc);
        if (outerAnalysis->slotEscapes(slot))
            return false;
        uint32 write = outerAnalysis->liveness(slot).firstWrite(lifetime);
        if (write != uint32(-1) && write < v.pushedOffset()) {
            /* Variable has been modified since the start of the loop. */
            return false;
        }
        *pslot = slot;
        *pconstant = (op == JSOP_INCLOCAL || op == JSOP_INCARG) ? 1 : 0;
        return true;
      }

      case JSOP_THIS:
        if (cv.frame != CrossScriptSSA::OUTER_FRAME)
            return false;
        *pslot = ThisSlot();
        *pconstant = 0;
        return true;

      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_INT8:
      case JSOP_INT32:
        *pslot = UNASSIGNED;
        *pconstant = GetBytecodeInteger(pc);
        return true;

      case JSOP_LENGTH: {
        CrossSSAValue lengthcv(cv.frame, analysis->poppedValue(v.pushedOffset(), 0));
        FrameEntry *tmp = invariantLength(lengthcv);
        if (!tmp)
            return false;
        *pslot = frame.outerSlot(tmp);
        *pconstant = 0;
        return true;
      }

      case JSOP_GETPROP: {
        JSAtom *atom = script->getAtom(js_GetIndexFromBytecode(cx, script, pc, 0));
        jsid id = ATOM_TO_JSID(atom);
        CrossSSAValue objcv(cv.frame, analysis->poppedValue(v.pushedOffset(), 0));
        FrameEntry *tmp = invariantProperty(objcv, id);
        if (!tmp)
            return false;
        *pslot = frame.outerSlot(tmp);
        *pconstant = 0;
        return true;
      }

      default:
        return false;
    }
}

bool
LoopState::computeInterval(const CrossSSAValue &cv, int32 *pmin, int32 *pmax)
{
    JSScript *script = ssa->getFrame(cv.frame).script;
    ScriptAnalysis *analysis = script->analysis();
    const SSAValue &v = cv.v;

    if (v.kind() == SSAValue::VAR && !v.varInitial()) {
        jsbytecode *pc = script->code + v.varOffset();
        switch (JSOp(*pc)) {
          case JSOP_SETLOCAL:
          case JSOP_SETARG: {
            CrossSSAValue ncv(cv.frame, analysis->poppedValue(pc, 0));
            return computeInterval(ncv, pmin, pmax);
          }

          default:
            return false;
        }
    }

    if (v.kind() != SSAValue::PUSHED)
        return false;

    jsbytecode *pc = script->code + v.pushedOffset();
    JSOp op = (JSOp)*pc;

    /* Note: this was adapted from similar code in nanojit/LIR.cpp */
    switch (op) {

      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_INT8:
      case JSOP_INT32: {
        int32 constant = GetBytecodeInteger(pc);
        *pmin = constant;
        *pmax = constant;
        return true;
      }

      case JSOP_BITAND: {
        int32 lhsmin, lhsmax, rhsmin, rhsmax;
        CrossSSAValue lhsv(cv.frame, analysis->poppedValue(pc, 1));
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        bool haslhs = computeInterval(lhsv, &lhsmin, &lhsmax);
        bool hasrhs = computeInterval(rhsv, &rhsmin, &rhsmax);

        /* Only handle bitand with a constant operand. */
        haslhs = haslhs && lhsmin == lhsmax && lhsmin >= 0;
        hasrhs = hasrhs && rhsmin == rhsmax && rhsmin >= 0;

        if (haslhs && hasrhs) {
            *pmin = 0;
            *pmax = Min(lhsmax, rhsmax);
        } else if (haslhs) {
            *pmin = 0;
            *pmax = lhsmax;
        } else if (hasrhs) {
            *pmin = 0;
            *pmax = rhsmax;
        } else {
            return false;
        }
        return true;
      }

      case JSOP_RSH: {
        int32 rhsmin, rhsmax;
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(rhsv, &rhsmin, &rhsmax) || rhsmin != rhsmax)
            return false;

        /* Only use the bottom 5 bits. */
        int32 shift = rhsmin & 0x1f;
        *pmin = -(1 << (31 - shift));
        *pmax = (1 << (31 - shift)) - 1;
        return true;
      }

      case JSOP_URSH: {
        int32 rhsmin, rhsmax;
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(rhsv, &rhsmin, &rhsmax) || rhsmin != rhsmax)
            return false;

        /* Only use the bottom 5 bits. */
        int32 shift = rhsmin & 0x1f;
        if (shift == 0)
            return false;
        *pmin = 0;
        *pmax = (1 << (31 - shift)) - 1;
        return true;
      }

      case JSOP_MOD: {
        int32 rhsmin, rhsmax;
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(rhsv, &rhsmin, &rhsmax) || rhsmin != rhsmax)
            return false;

        int32 rhs = abs(rhsmax);
        *pmin = -(rhs - 1);
        *pmax = rhs - 1;
        return true;
      }

      case JSOP_ADD: {
        int32 lhsmin, lhsmax, rhsmin, rhsmax;
        CrossSSAValue lhsv(cv.frame, analysis->poppedValue(pc, 1));
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(lhsv, &lhsmin, &lhsmax) || !computeInterval(rhsv, &rhsmin, &rhsmax))
            return false;
        return SafeAdd(lhsmin, rhsmin, pmin) && SafeAdd(lhsmax, rhsmax, pmax);
      }

      case JSOP_SUB: {
        int32 lhsmin, lhsmax, rhsmin, rhsmax;
        CrossSSAValue lhsv(cv.frame, analysis->poppedValue(pc, 1));
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(lhsv, &lhsmin, &lhsmax) || !computeInterval(rhsv, &rhsmin, &rhsmax))
            return false;
        return SafeSub(lhsmin, rhsmax, pmin) && SafeSub(lhsmax, rhsmin, pmax);
      }

      case JSOP_MUL: {
        int32 lhsmin, lhsmax, rhsmin, rhsmax;
        CrossSSAValue lhsv(cv.frame, analysis->poppedValue(pc, 1));
        CrossSSAValue rhsv(cv.frame, analysis->poppedValue(pc, 0));
        if (!computeInterval(lhsv, &lhsmin, &lhsmax) || !computeInterval(rhsv, &rhsmin, &rhsmax))
            return false;
        int32 nlhs = Max(abs(lhsmin), abs(lhsmax));
        int32 nrhs = Max(abs(rhsmin), abs(rhsmax));

        if (!SafeMul(nlhs, nrhs, pmax))
            return false;

        if (lhsmin < 0 || rhsmin < 0) {
            /* pmax is nonnegative, so can be negated without overflow. */
            *pmin = -*pmax;
        } else {
            *pmin = 0;
        }

        return true;
      }

      default:
        return false;
    }
}
