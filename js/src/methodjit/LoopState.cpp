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

using namespace js;
using namespace js::mjit;
using namespace js::analyze;

LoopState::LoopState(JSContext *cx, JSScript *script,
                     Compiler *cc, FrameState *frame,
                     Script *analysis, LifetimeScript *liveness)
    : cx(cx), script(script), cc(*cc), frame(*frame), analysis(analysis), liveness(liveness),
      lifetime(NULL), alloc(NULL), loopRegs(0), skipAnalysis(false),
      loopJoins(CompilerAllocPolicy(cx, *cc)),
      loopPatches(CompilerAllocPolicy(cx, *cc)),
      restoreInvariantCalls(CompilerAllocPolicy(cx, *cc)),
      hoistedBoundsChecks(CompilerAllocPolicy(cx, *cc)),
      invariantArraySlots(CompilerAllocPolicy(cx, *cc)),
      outer(NULL), PC(NULL)
{
    JS_ASSERT(cx->typeInferenceEnabled());
}

bool
LoopState::init(jsbytecode *head, Jump entry, jsbytecode *entryTarget)
{
    this->lifetime = liveness->getCode(head).loop;
    JS_ASSERT(lifetime &&
              lifetime->head == uint32(head - script->code) &&
              lifetime->entry == uint32(entryTarget - script->code));

    this->entry = entry;

    liveness->analyzeLoopTest(lifetime);
    if (!liveness->analyzeLoopIncrements(cx, lifetime))
        return false;
    if (!liveness->analyzeLoopModset(cx, lifetime))
        return false;

    if (lifetime->testLHS != LifetimeLoop::UNASSIGNED) {
        JaegerSpew(JSpew_Analysis, "loop test at %u: %s %s %s + %d\n", lifetime->head,
                   frame.entryName(lifetime->testLHS),
                   lifetime->testLessEqual ? "<=" : ">=",
                   (lifetime->testRHS == LifetimeLoop::UNASSIGNED)
                       ? ""
                       : frame.entryName(lifetime->testRHS),
                   lifetime->testConstant);
    }

    for (unsigned i = 0; i < lifetime->nIncrements; i++) {
        JaegerSpew(JSpew_Analysis, "loop increment at %u for %s: %u\n", lifetime->head,
                   frame.entryName(lifetime->increments[i].slot),
                   lifetime->increments[i].offset);
    }

    for (unsigned i = 0; i < lifetime->nGrowArrays; i++) {
        JaegerSpew(JSpew_Analysis, "loop grow array at %u: %s\n", lifetime->head,
                   lifetime->growArrays[i]->name());
    }

    RegisterAllocation *&alloc = liveness->getCode(head).allocation;
    JS_ASSERT(!alloc);

    alloc = ArenaNew<RegisterAllocation>(liveness->pool, true);
    if (!alloc)
        return false;

    this->alloc = alloc;
    this->loopRegs = Registers::AvailAnyRegs;
    this->PC = head;

    /*
     * Don't hoist bounds checks or loop invariant code in scripts that have
     * had indirect modification of their arguments.
     */
    if (script->fun) {
        types::ObjectKind kind = types::TypeSet::GetObjectKind(cx, script->fun->getType());
        if (kind != types::OBJECT_INLINEABLE_FUNCTION && kind != types::OBJECT_SCRIPTED_FUNCTION)
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

    /*
     * Don't do hoisting in loops with inner loops or calls. This is way too
     * pessimistic and needs to get fixed.
     */
    if (lifetime->hasCallsLoops)
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
LoopState::addInvariantCall(Jump jump, Label label, bool ool)
{
    RestoreInvariantCall call;
    call.jump = jump;
    call.label = label;
    call.ool = ool;
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
            if (call.ool) {
                call.jump.linkTo(masm.label(), &masm);
                restoreInvariants(masm);
                masm.jump().linkTo(call.label, &masm);
            } else {
                stubcc.linkExitDirect(call.jump, masm.label());
                restoreInvariants(masm);
                stubcc.crossJump(masm.jump(), call.label);
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
LoopState::loopInvariantEntry(const FrameEntry *fe)
{
    uint32 slot = frame.indexOfFe(fe);
    unsigned nargs = script->fun ? script->fun->nargs : 0;

    if (slot >= 2 + nargs + script->nfixed)
        return false;

    if (liveness->firstWrite(slot, lifetime) != uint32(-1))
        return false;

    if (slot == 0) /* callee */
        return false;
    if (slot == 1) /* this */
        return true;
    slot -= 2;

    if (slot < nargs && !analysis->argEscapes(slot))
        return true;
    if (script->fun)
        slot -= script->fun->nargs;

    return !analysis->localEscapes(slot);
}

bool
LoopState::addHoistedCheck(uint32 arraySlot, uint32 valueSlot, int32 constant)
{
    /*
     * Check to see if this bounds check either implies or is implied by an
     * existing hoisted check.
     */
    for (unsigned i = 0; i < hoistedBoundsChecks.length(); i++) {
        HoistedBoundsCheck &check = hoistedBoundsChecks[i];
        if (check.arraySlot == arraySlot && check.valueSlot == valueSlot) {
            if (check.constant < constant)
                check.constant = constant;
            return true;
        }
    }

    /*
     * Maintain an invariant that for any array with a hoisted bounds check,
     * we also have a loop invariant slot to hold the array's slots pointer.
     * The compiler gets invariant array slots only for accesses with a hoisted
     * bounds check, so this makes invariantSlots infallible.
     */
    bool hasInvariantSlots = false;
    for (unsigned i = 0; !hasInvariantSlots && i < invariantArraySlots.length(); i++) {
        if (invariantArraySlots[i].arraySlot == arraySlot)
            hasInvariantSlots = true;
    }
    if (!hasInvariantSlots) {
        uint32 which = frame.allocTemporary();
        if (which == uint32(-1))
            return false;
        FrameEntry *fe = frame.getTemporary(which);

        JaegerSpew(JSpew_Analysis, "Using %s for loop invariant slots of %s\n",
                   frame.entryName(fe), frame.entryName(arraySlot));

        InvariantArraySlots slots;
        slots.arraySlot = arraySlot;
        slots.temporary = which;
        invariantArraySlots.append(slots);
    }

    HoistedBoundsCheck check;
    check.arraySlot = arraySlot;
    check.valueSlot = valueSlot;
    check.constant = constant;
    hoistedBoundsChecks.append(check);

    return true;
}

void
LoopState::setLoopReg(AnyRegisterID reg, FrameEntry *fe)
{
    JS_ASSERT(alloc->loop(reg));
    loopRegs.takeReg(reg);

    uint32 slot = frame.indexOfFe(fe);
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

    if (lifetime->entry != lifetime->head && PC >= script->code + lifetime->entry) {
        /*
         * We've advanced past the entry point of the loop (we're analyzing the condition),
         * so need to update the register state at that entry point so that the right
         * things get loaded when we enter the loop.
         */
        RegisterAllocation *entry = liveness->getCode(lifetime->entry).allocation;
        JS_ASSERT(entry && !entry->assigned(reg));
        entry->set(reg, slot, true);
    }
}

bool
LoopState::hoistArrayLengthCheck(const FrameEntry *obj, const FrameEntry *index)
{
    if (skipAnalysis || script->failedBoundsCheck)
        return false;

    /*
     * Note: this should only be used when the object is known to be a dense
     * array (if it is an object at all) whose length has never shrunk.
     * (determined by checking types->getKnownObjectKind for the object).
     */

    obj = obj->backing();
    index = index->backing();

    JaegerSpew(JSpew_Analysis, "Trying to hoist bounds check array %s index %s\n",
               frame.entryName(obj), frame.entryName(index));

    if (!loopInvariantEntry(obj)) {
        JaegerSpew(JSpew_Analysis, "Object is not loop invariant\n");
        return false;
    }

    types::TypeSet *objTypes = cc.getTypeSet(obj);
    JS_ASSERT(objTypes && !objTypes->unknown());

    /* Currently, we only do hoisting/LICM on values which are definitely objects. */
    if (objTypes->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT) {
        JaegerSpew(JSpew_Analysis, "Object might be a primitive\n");
        return false;
    }

    /*
     * Check for an overlap with the arrays we think might grow in this loop.
     * This information is only a guess; if we don't think the array can grow
     * but it actually can, we will probably recompile after the hoisted
     * bounds check fails.
     */
    if (lifetime->nGrowArrays) {
        types::TypeObject **growArrays = lifetime->growArrays;
        unsigned count = objTypes->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            types::TypeObject *object = objTypes->getObject(i);
            if (object) {
                for (unsigned j = 0; j < lifetime->nGrowArrays; j++) {
                    if (object == growArrays[j]) {
                        JaegerSpew(JSpew_Analysis, "Object might grow inside loop\n");
                        return false;
                    }
                }
            }
        }
    }

    if (index->isConstant()) {
        /* Hoist checks on x[n] accesses for constant n. */
        int32 value = index->getValue().toInt32();
        JaegerSpew(JSpew_Analysis, "Hoisted as initlen > %d\n", value);

        return addHoistedCheck(frame.indexOfFe(obj), uint32(-1), value);
    }

    if (loopInvariantEntry(index)) {
        /* Hoist checks on x[y] accesses when y is loop invariant. */
        JaegerSpew(JSpew_Analysis, "Hoisted as initlen > %s\n", frame.entryName(index));

        return addHoistedCheck(frame.indexOfFe(obj), frame.indexOfFe(index), 0);
    }

    if (frame.indexOfFe(index) == lifetime->testLHS && lifetime->testLessEqual) {
        /*
         * If the access is of the form x[y] where we know that y <= z + n at
         * the head of the loop, hoist the check as initlen < z + n provided
         * that y has not been modified since the head of the loop.
         */
        uint32 rhs = lifetime->testRHS;
        int32 constant = lifetime->testConstant;

        uint32 write = liveness->firstWrite(lifetime->testLHS, lifetime);
        JS_ASSERT(write != LifetimeLoop::UNASSIGNED);
        if (write < uint32(PC - script->code)) {
            JaegerSpew(JSpew_Analysis, "Index previously modified in loop\n");
            return false;
        }

        if (rhs != LifetimeLoop::UNASSIGNED) {
            /*
             * The index will be a known int or will have been guarded as an int,
             * but the branch test substitution is only valid if it is comparing
             * integers.
             */
            types::TypeSet *types = cc.getTypeSet(rhs);
            if (types->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
                JaegerSpew(JSpew_Analysis, "Branch test may not be on integer\n");
                return false;
            }
        }

        JaegerSpew(JSpew_Analysis, "Hoisted as initlen > %s + %d\n",
                   (rhs == LifetimeLoop::UNASSIGNED) ? "" : frame.entryName(rhs),
                   constant);

        return addHoistedCheck(frame.indexOfFe(obj), rhs, constant);
    }

    JaegerSpew(JSpew_Analysis, "No match found\n");
    return false;
}

bool
LoopState::checkHoistedBounds(jsbytecode *PC, Assembler &masm, Vector<Jump> *jumps)
{
    restoreInvariants(masm);

    /*
     * Emit code to validate all hoisted bounds checks, filling jumps with all
     * failure paths. This is done from a fully synced state, and all registers
     * can be used as temporaries. Note: we assume that no modifications to the
     * terms in the hoisted checks occur between PC and the head of the loop.
     */

    for (unsigned i = 0; i < hoistedBoundsChecks.length(); i++) {
        /* Testing: initializedLength(array) > value + constant; */
        const HoistedBoundsCheck &check = hoistedBoundsChecks[i];

        RegisterID initlen = Registers::ArgReg0;
        masm.loadPayload(frame.addressOf(check.arraySlot), initlen);
        masm.load32(Address(initlen, offsetof(JSObject, initializedLength)), initlen);

        if (check.valueSlot != uint32(-1)) {
            RegisterID value = Registers::ArgReg1;
            masm.loadPayload(frame.addressOf(check.valueSlot), value);
            if (check.constant != 0) {
                Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                 Imm32(check.constant), value);
                if (!jumps->append(overflow))
                    return false;
            }
            Jump j = masm.branch32(Assembler::BelowOrEqual, initlen, value);
            if (!jumps->append(j))
                return false;
        } else {
            Jump j = masm.branch32(Assembler::BelowOrEqual, initlen, Imm32(check.constant));
            if (!jumps->append(j))
                return false;
        }
    }

    return true;
}

FrameEntry *
LoopState::invariantSlots(const FrameEntry *obj)
{
    obj = obj->backing();
    uint32 slot = frame.indexOfFe(obj);

    for (unsigned i = 0; i < invariantArraySlots.length(); i++) {
        if (invariantArraySlots[i].arraySlot == slot)
            return frame.getTemporary(invariantArraySlots[i].temporary);
    }

    /* addHoistedCheck should have ensured there is an entry for the slots. */
    JS_NOT_REACHED("Missing invariant slots");
    return NULL;
}

void
LoopState::restoreInvariants(Assembler &masm)
{
    /*
     * Restore all invariants in memory when entering the loop or after any
     * scripted or C++ call. Care should be taken not to clobber the return
     * register, which may still be live after some calls.
     */

    Registers regs(Registers::AvailRegs);
    regs.takeReg(Registers::ReturnReg);

    for (unsigned i = 0; i < invariantArraySlots.length(); i++) {
        const InvariantArraySlots &entry = invariantArraySlots[i];
        FrameEntry *fe = frame.getTemporary(entry.temporary);

        Address array = frame.addressOf(entry.arraySlot);
        Address address = frame.addressOf(fe);

        RegisterID reg = regs.takeAnyReg().reg();
        masm.loadPayload(array, reg);
        masm.loadPtr(Address(reg, JSObject::offsetOfSlots()), reg);
        masm.storePtr(reg, address);
        regs.putReg(reg);
    }
}
