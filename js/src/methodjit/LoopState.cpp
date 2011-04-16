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

using namespace js;
using namespace js::mjit;
using namespace js::analyze;

LoopState::LoopState(JSContext *cx, JSScript *script,
                     mjit::Compiler *cc, FrameState *frame,
                     Script *analysis, LifetimeScript *liveness)
    : cx(cx), script(script), cc(*cc), frame(*frame), analysis(analysis), liveness(liveness),
      lifetime(NULL), alloc(NULL), loopRegs(0), skipAnalysis(false),
      loopJoins(CompilerAllocPolicy(cx, *cc)),
      loopPatches(CompilerAllocPolicy(cx, *cc)),
      restoreInvariantCalls(CompilerAllocPolicy(cx, *cc)),
      invariantEntries(CompilerAllocPolicy(cx, *cc)),
      outer(NULL), PC(NULL),
      testLHS(UNASSIGNED), testRHS(UNASSIGNED),
      testConstant(0), testLessEqual(false), testLength(false),
      increments(CompilerAllocPolicy(cx, *cc)), unknownModset(false),
      growArrays(CompilerAllocPolicy(cx, *cc)),
      modifiedProperties(CompilerAllocPolicy(cx, *cc))
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

    if (!stack.analyze(liveness->pool, script, lifetime->head,
                       lifetime->backedge - lifetime->head + 1, analysis)) {
        return false;
    }

    analyzeLoopTest();
    analyzeLoopIncrements();
    analyzeModset();

    if (testLHS != UNASSIGNED) {
        JaegerSpew(JSpew_Analysis, "loop test at %u: %s %s%s %s + %d\n", lifetime->head,
                   frame.entryName(testLHS),
                   testLessEqual ? "<=" : ">=",
                   testLength ? " length" : "",
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
                   growArrays[i]->name());
    }

    for (unsigned i = 0; i < modifiedProperties.length(); i++) {
        JaegerSpew(JSpew_Analysis, "loop modified property at %u: %s %s\n", lifetime->head,
                   modifiedProperties[i].object->name(),
                   types::TypeIdString(modifiedProperties[i].id));
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
LoopState::addInvariantCall(Jump jump, Label label, bool ool, unsigned patchIndex, bool patchCall)
{
    RestoreInvariantCall call;
    call.jump = jump;
    call.label = label;
    call.ool = ool;
    call.patchIndex = patchIndex;
    call.patchCall = patchCall;
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

            if (call.ool) {
                call.jump.linkTo(masm.label(), &masm);
                restoreInvariants(masm, &failureJumps);
                masm.jump().linkTo(call.label, &masm);
            } else {
                stubcc.linkExitDirect(call.jump, masm.label());
                restoreInvariants(masm, &failureJumps);
                stubcc.crossJump(masm.jump(), call.label);
            }

            if (!failureJumps.empty()) {
                for (unsigned i = 0; i < failureJumps.length(); i++)
                    failureJumps[i].linkTo(masm.label(), &masm);

                /*
                 * Call InvariantFailure, setting up the return address to
                 * patch and any value for the call to return.
                 */
                InvariantCodePatch *patch = cc.getInvariantPatch(call.patchIndex, call.patchCall);
                patch->hasPatch = true;
                patch->codePatch = masm.storePtrWithPatch(ImmPtr(NULL),
                                                          FrameAddress(offsetof(VMFrame, scratch)));
                JS_STATIC_ASSERT(Registers::ReturnReg != Registers::ArgReg1);
                masm.move(Registers::ReturnReg, Registers::ArgReg1);
                jsbytecode *pc = cc.getInvariantPC(call.patchIndex, call.patchCall);
                masm.fallibleVMCall(true, JS_FUNC_TO_DATA_PTR(void *, stubs::InvariantFailure),
                                    pc, NULL, 0);
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
    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::BOUNDS_CHECK &&
            entry.u.check.arraySlot == arraySlot &&
            entry.u.check.valueSlot == valueSlot) {
            if (entry.u.check.constant < constant)
                entry.u.check.constant = constant;
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
    for (unsigned i = 0; !hasInvariantSlots && i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::INVARIANT_SLOTS &&
            entry.u.array.arraySlot == arraySlot) {
            hasInvariantSlots = true;
        }
    }
    if (!hasInvariantSlots) {
        uint32 which = frame.allocTemporary();
        if (which == uint32(-1))
            return false;
        FrameEntry *fe = frame.getTemporary(which);

        JaegerSpew(JSpew_Analysis, "Using %s for loop invariant slots of %s\n",
                   frame.entryName(fe), frame.entryName(arraySlot));

        InvariantEntry entry;
        entry.kind = InvariantEntry::INVARIANT_SLOTS;
        entry.u.array.arraySlot = arraySlot;
        entry.u.array.temporary = which;
        invariantEntries.append(entry);
    }

    InvariantEntry entry;
    entry.kind = InvariantEntry::BOUNDS_CHECK;
    entry.u.check.arraySlot = arraySlot;
    entry.u.check.valueSlot = valueSlot;
    entry.u.check.constant = constant;
    invariantEntries.append(entry);

    return true;
}

void
LoopState::addNegativeCheck(uint32 valueSlot, int32 constant)
{
    /*
     * Check to see if this check either implies or is implied by an
     * existing negative check.
     */
    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::NEGATIVE_CHECK &&
            entry.u.check.valueSlot == valueSlot) {
            if (entry.u.check.constant > constant)
                entry.u.check.constant = constant;
            return;
        }
    }

    InvariantEntry entry;
    entry.kind = InvariantEntry::NEGATIVE_CHECK;
    entry.u.check.valueSlot = valueSlot;
    entry.u.check.constant = constant;
    invariantEntries.append(entry);
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
    if (!growArrays.empty()) {
        unsigned count = objTypes->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            types::TypeObject *object = objTypes->getObject(i);
            if (object) {
                for (unsigned j = 0; j < growArrays.length(); j++) {
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

    if (frame.indexOfFe(index) == testLHS && testLessEqual) {
        /*
         * If the access is of the form x[y] where we know that y <= z + n at
         * the head of the loop, hoist the check as initlen < z + n provided
         * that y has not been modified since the head of the loop.
         */
        uint32 rhs = testRHS;

        /*
         * If the LHS can decrease in the loop, it could become negative and
         * underflow the array.
         */
        if (!liveness->nonDecreasing(testLHS, lifetime)) {
            JaegerSpew(JSpew_Analysis, "Index may decrease in future iterations\n");
            return false;
        }

        uint32 write = liveness->firstWrite(testLHS, lifetime);
        JS_ASSERT(write != UNASSIGNED);
        if (write < uint32(PC - script->code)) {
            JaegerSpew(JSpew_Analysis, "Index previously modified in loop\n");
            return false;
        }

        if (rhs != UNASSIGNED) {
            types::TypeSet *types = cc.getTypeSet(rhs);
            if (!types) {
                JaegerSpew(JSpew_Analysis, "Unknown type of branch test\n");
                return false;
            }
            if (testLength) {
                FrameEntry *rhsFE = frame.getOrTrack(rhs);
                FrameEntry *lengthEntry = invariantLength(rhsFE);
                if (!lengthEntry) {
                    JaegerSpew(JSpew_Analysis, "Could not get invariant entry for length\n");
                    return false;
                }
                rhs = frame.indexOfFe(lengthEntry);
            } else {
                /*
                 * The index will be a known int or will have been guarded as an int,
                 * but the branch test substitution is only valid if it is comparing
                 * integers.
                 */
                if (types->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
                    JaegerSpew(JSpew_Analysis, "Branch test may not be on integer\n");
                    return false;
                }
            }
        }

        /*
         * Check that the LHS is nonnegative every time we rejoin the loop.
         * This is only really necessary on initial loop entry. Note that this
         * test is not sensitive to changes to the LHS between when we make
         * the test and the start of the next iteration, as we've ensured the
         * LHS is nondecreasing within the body of the loop.
         */

        JaegerSpew(JSpew_Analysis, "Nonnegative check %s + %d >= 0\n",
                   frame.entryName(testLHS), 0);

        addNegativeCheck(testLHS, 0);

        JaegerSpew(JSpew_Analysis, "Hoisted as initlen > %s + %d\n",
                   (rhs == UNASSIGNED) ? "" : frame.entryName(rhs),
                   testConstant);

        return addHoistedCheck(frame.indexOfFe(obj), rhs, testConstant);
    }

    JaegerSpew(JSpew_Analysis, "No match found\n");
    return false;
}

FrameEntry *
LoopState::invariantSlots(const FrameEntry *obj)
{
    obj = obj->backing();
    uint32 slot = frame.indexOfFe(obj);

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::INVARIANT_SLOTS &&
            entry.u.array.arraySlot == slot) {
            return frame.getTemporary(entry.u.array.temporary);
        }
    }

    /* addHoistedCheck should have ensured there is an entry for the slots. */
    JS_NOT_REACHED("Missing invariant slots");
    return NULL;
}

FrameEntry *
LoopState::invariantLength(const FrameEntry *obj)
{
    obj = obj->backing();
    uint32 slot = frame.indexOfFe(obj);

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        InvariantEntry &entry = invariantEntries[i];
        if (entry.kind == InvariantEntry::INVARIANT_LENGTH &&
            entry.u.array.arraySlot == slot) {
            FrameEntry *fe = frame.getTemporary(entry.u.array.temporary);
            frame.learnType(fe, JSVAL_TYPE_INT32, false);
            return fe;
        }
    }

    if (!loopInvariantEntry(obj))
        return NULL;

    /* Make sure this is a dense array whose length fits in an int32. */
    types::TypeSet *types = cc.getTypeSet(slot);
    types::ObjectKind kind = types ? types->getKnownObjectKind(cx) : types::OBJECT_UNKNOWN;
    if (kind != types::OBJECT_DENSE_ARRAY && kind != types::OBJECT_PACKED_ARRAY)
        return NULL;

    /*
     * Don't make 'length' loop invariant if the loop might directly write
     * to the elements of any of the accessed arrays. This could invoke an
     * inline path which updates the length.
     */
    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        types::TypeObject *object = types->getObject(i);
        if (!object)
            continue;
        if (object->unknownProperties() || hasModifiedProperty(object, JSID_VOID))
            return NULL;
    }
    types->addFreeze(cx);

    uint32 which = frame.allocTemporary();
    if (which == uint32(-1))
        return NULL;
    FrameEntry *fe = frame.getTemporary(which);
    frame.learnType(fe, JSVAL_TYPE_INT32, false);

    JaegerSpew(JSpew_Analysis, "Using %s for loop invariant length of %s\n",
               frame.entryName(fe), frame.entryName(slot));

    InvariantEntry entry;
    entry.kind = InvariantEntry::INVARIANT_LENGTH;
    entry.u.array.arraySlot = slot;
    entry.u.array.temporary = which;
    invariantEntries.append(entry);

    return fe;
}

void
LoopState::restoreInvariants(Assembler &masm, Vector<Jump> *jumps)
{
    /*
     * Restore all invariants in memory when entering the loop or after any
     * scripted or C++ call, and check that all hoisted conditions. Care should
     * be taken not to clobber the return register or callee-saved registers,
     * which may still be live after some calls.
     */

    Registers regs(Registers::TempRegs);
    regs.takeReg(Registers::ReturnReg);

    RegisterID T0 = regs.takeAnyReg().reg();
    RegisterID T1 = regs.takeAnyReg().reg();

    for (unsigned i = 0; i < invariantEntries.length(); i++) {
        const InvariantEntry &entry = invariantEntries[i];
        switch (entry.kind) {

          case InvariantEntry::BOUNDS_CHECK: {
            /*
             * Hoisted bounds checks always have preceding invariant slots
             * in the invariant list, so don't recheck this is an object.
             */
            masm.loadPayload(frame.addressOf(entry.u.check.arraySlot), T0);
            masm.load32(Address(T0, offsetof(JSObject, initializedLength)), T0);

            if (entry.u.check.valueSlot != uint32(-1)) {
                masm.loadPayload(frame.addressOf(entry.u.check.valueSlot), T1);
                if (entry.u.check.constant != 0) {
                    Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                     Imm32(entry.u.check.constant), T1);
                    jumps->append(overflow);
                }
                Jump j = masm.branch32(Assembler::BelowOrEqual, T0, T1);
                jumps->append(j);
            } else {
                Jump j = masm.branch32(Assembler::BelowOrEqual, T0,
                                       Imm32(entry.u.check.constant));
                jumps->append(j);
            }
            break;
          }

          case InvariantEntry::NEGATIVE_CHECK: {
            masm.loadPayload(frame.addressOf(entry.u.check.valueSlot), T0);
            if (entry.u.check.constant != 0) {
                Jump overflow = masm.branchAdd32(Assembler::Overflow,
                                                 Imm32(entry.u.check.constant), T0);
                jumps->append(overflow);
            }
            Jump j = masm.branch32(Assembler::LessThan, T0, Imm32(0));
            jumps->append(j);
            break;
          }

          case InvariantEntry::INVARIANT_SLOTS:
          case InvariantEntry::INVARIANT_LENGTH: {
            /* Make sure this is an object before trying to access its slots or length. */
            uint32 array = entry.u.array.arraySlot;
            if (cc.getTypeSet(array)->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT) {
                Jump notObject = masm.testObject(Assembler::NotEqual, frame.addressOf(array));
                jumps->append(notObject);
            }
            masm.loadPayload(frame.addressOf(array), T0);

            uint32 offset = (entry.kind == InvariantEntry::INVARIANT_SLOTS)
                ? JSObject::offsetOfSlots()
                : offsetof(JSObject, privateData);

            masm.loadPtr(Address(T0, offset), T0);
            masm.storePtr(T0, frame.addressOf(frame.getTemporary(entry.u.array.temporary)));
            break;
          }

          default:
            JS_NOT_REACHED("Bad invariant kind");
        }
    }
}

/* Loop analysis methods. */

/* :XXX: factor out into more common code. */
static inline uint32 localSlot(JSScript *script, uint32 local) {
    return 2 + (script->fun ? script->fun->nargs : 0) + local;
}
static inline uint32 argSlot(uint32 arg) {
    return 2 + arg;
}
static inline uint32 thisSlot() {
    return 1;
}

/* Whether pc is a loop test operand accessing a variable modified by the loop. */
bool
LoopState::loopVariableAccess(jsbytecode *pc)
{
    switch (JSOp(*pc)) {
      case JSOP_GETLOCAL:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        if (analysis->localEscapes(GET_SLOTNO(pc)))
            return false;
        return liveness->firstWrite(localSlot(script, GET_SLOTNO(pc)), lifetime) != uint32(-1);
      case JSOP_GETARG:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
        if (analysis->argEscapes(GET_SLOTNO(pc)))
            return false;
        return liveness->firstWrite(argSlot(GET_SLOTNO(pc)), lifetime) != uint32(-1);
      default:
        return false;
    }
}

/*
 * Get any slot/constant accessed by a loop test operand, in terms of its value
 * at the start of the next loop iteration.
 */
bool
LoopState::getLoopTestAccess(jsbytecode *pc, uint32 *slotp, int32 *constantp)
{
    *slotp = UNASSIGNED;
    *constantp = 0;

    /*
     * If the pc is modifying a variable and the value tested is its earlier value
     * (e.g. 'x++ < n'), we need to account for the modification --- at the start
     * of the next iteration, the value compared will have been 'x - 1'.
     * Note that we don't need to worry about other accesses to the variable
     * in the condition like 'x++ < x', as loop tests where both operands are
     * modified by the loop are rejected.
     */

    JSOp op = JSOp(*pc);
    switch (op) {

      case JSOP_GETLOCAL:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        uint32 local = GET_SLOTNO(pc);
        if (analysis->localEscapes(local))
            return false;
        *slotp = localSlot(script, local);
        if (op == JSOP_LOCALINC)
            *constantp = -1;
        else if (op == JSOP_LOCALDEC)
            *constantp = 1;
        return true;
      }

      case JSOP_GETARG:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        uint32 arg = GET_SLOTNO(pc);
        if (analysis->argEscapes(arg))
            return false;
        *slotp = argSlot(arg);
        if (op == JSOP_ARGINC)
            *constantp = -1;
        else if (op == JSOP_ARGDEC)
            *constantp = 1;
        return true;
      }

      case JSOP_ZERO:
        *constantp = 0;
        return true;

      case JSOP_ONE:
        *constantp = 1;
        return true;

      case JSOP_UINT16:
        *constantp = (int32_t) GET_UINT16(pc);
        return true;

      case JSOP_UINT24:
        *constantp = (int32_t) GET_UINT24(pc);
        return true;

      case JSOP_INT8:
        *constantp = GET_INT8(pc);
        return true;

      case JSOP_INT32:
        /*
         * Don't allow big constants out of the range of an object's max
         * nslots, to avoid integer overflow.
         */
        *constantp = GET_INT32(pc);
        if (*constantp >= JSObject::NSLOTS_LIMIT || *constantp <= -JSObject::NSLOTS_LIMIT)
            return false;
        return true;

      default:
        return false;
    }
}

void
LoopState::analyzeLoopTest()
{
    /* Don't handle do-while loops. */
    if (lifetime->entry == lifetime->head)
        return;

    /* Don't handle loops with branching inside their condition. */
    if (lifetime->entry < lifetime->lastBlock)
        return;

    /* Get the test performed before branching. */
    jsbytecode *backedge = script->code + lifetime->backedge;
    if (JSOp(*backedge) != JSOP_IFNE)
        return;
    StackAnalysis::PoppedValue test = stack.popped(backedge, 0);
    if (test.offset == StackAnalysis::UNKNOWN_PUSHED)
        return;
    JSOp cmpop = JSOp(script->code[test.offset]);
    switch (cmpop) {
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_LT:
      case JSOP_LE:
        break;
      default:
        return;
    }

    StackAnalysis::PoppedValue poppedOne = stack.popped(test.offset, 1);
    StackAnalysis::PoppedValue poppedTwo = stack.popped(test.offset, 0);

    if (poppedOne.offset == StackAnalysis::UNKNOWN_PUSHED ||
        poppedTwo.offset == StackAnalysis::UNKNOWN_PUSHED) {
        return;
    }

    jsbytecode *one = script->code + poppedOne.offset;
    jsbytecode *two = script->code + poppedTwo.offset;

    /* Reverse the condition if the RHS is modified by the loop. */
    if (loopVariableAccess(two)) {
        jsbytecode *tmp = one;
        one = two;
        two = tmp;
        cmpop = ReverseCompareOp(cmpop);
    }

    /* Don't handle comparisons where both the LHS and RHS are modified in the loop. */
    if (loopVariableAccess(two))
        return;

    uint32 lhs;
    int32 lhsConstant;
    if (!getLoopTestAccess(one, &lhs, &lhsConstant))
        return;

    uint32 rhs = UNASSIGNED;
    int32 rhsConstant = 0;
    bool rhsLength = false;

    if (JSOp(*two) == JSOP_LENGTH) {
        /* Handle 'this.length' or 'x.length' for loop invariant 'x'. */
        StackAnalysis::PoppedValue array = stack.popped(two, 0);
        if (array.offset == StackAnalysis::UNKNOWN_PUSHED)
            return;
        jsbytecode *arraypc = script->code + array.offset;
        if (loopVariableAccess(arraypc))
            return;
        switch (JSOp(*arraypc)) {
          case JSOP_GETLOCAL: {
            uint32 local = GET_SLOTNO(arraypc);
            if (analysis->localEscapes(local))
                return;
            rhs = localSlot(script, local);
            break;
          }
          case JSOP_GETARG: {
            uint32 arg = GET_SLOTNO(arraypc);
            if (analysis->argEscapes(arg))
                return;
            rhs = argSlot(arg);
            break;
          }
          case JSOP_THIS:
            rhs = thisSlot();
            break;
          default:
            return;
        }
        rhsLength = true;
    } else {
        if (!getLoopTestAccess(two, &rhs, &rhsConstant))
            return;
    }

    if (lhs == UNASSIGNED)
        return;

    /* Passed all filters, this is a loop test we can capture. */

    this->testLHS = lhs;
    this->testRHS = rhs;
    this->testConstant = rhsConstant - lhsConstant;
    this->testLength = rhsLength;

    switch (cmpop) {
      case JSOP_GT:
        this->testConstant++;  /* x > y ==> x >= y + 1 */
        /* FALLTHROUGH */
      case JSOP_GE:
        this->testLessEqual = false;
        break;

      case JSOP_LT:
        this->testConstant--;  /* x < y ==> x <= y - 1 */
      case JSOP_LE:
        this->testLessEqual = true;
        break;

      default:
        JS_NOT_REACHED("Bad op");
        return;
    }
}

void
LoopState::analyzeLoopIncrements()
{
    /*
     * Find locals and arguments which are used in exactly one inc/dec operation in every
     * iteration of the loop (we only match against the last basic block, but could
     * also handle the first basic block).
     */

    unsigned nargs = script->fun ? script->fun->nargs : 0;
    for (unsigned i = 0; i < nargs; i++) {
        if (analysis->argEscapes(i))
            continue;

        uint32 offset = liveness->onlyWrite(argSlot(i), lifetime);
        if (offset == uint32(-1) || offset < lifetime->lastBlock)
            continue;

        JSOp op = JSOp(script->code[offset]);
        if (op == JSOP_SETARG)
            continue;

        Increment inc;
        inc.slot = argSlot(i);
        inc.offset = offset;
        increments.append(inc);
    }

    for (unsigned i = 0; i < script->nfixed; i++) {
        if (analysis->localEscapes(i))
            continue;

        uint32 offset = liveness->onlyWrite(localSlot(script, i), lifetime);
        if (offset == uint32(-1) || offset < lifetime->lastBlock)
            continue;

        JSOp op = JSOp(script->code[offset]);
        if (op == JSOP_SETLOCAL || op == JSOP_SETLOCALPOP)
            continue;

        Increment inc;
        inc.slot = localSlot(script, i);
        inc.offset = offset;
        increments.append(inc);
    }
}

void
LoopState::analyzeModset()
{
    /* :XXX: Currently only doing this for arrays modified in the loop. */

    unsigned offset = lifetime->head;
    while (offset < lifetime->backedge) {
        jsbytecode *pc = script->code + offset;
        uint32 successorOffset = offset + GetBytecodeLength(pc);

        analyze::Bytecode *opinfo = analysis->maybeCode(offset);
        if (!opinfo) {
            offset = successorOffset;
            continue;
        }

        JSOp op = JSOp(*pc);
        switch (op) {

          case JSOP_SETHOLE:
          case JSOP_SETELEM: {
            types::TypeSet *objTypes = poppedTypes(pc, 2);
            types::TypeSet *elemTypes = poppedTypes(pc, 1);

            /*
             * Mark the modset as unknown if the index might be non-integer,
             * we don't want to consider the SETELEM PIC here.
             */
            if (!objTypes || objTypes->unknown() || !elemTypes ||
                elemTypes->getKnownTypeTag(cx) != JSVAL_TYPE_INT32) {
                unknownModset = true;
                return;
            }

            objTypes->addFreeze(cx);
            for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
                types::TypeObject *object = objTypes->getObject(i);
                if (!object)
                    continue;
                if (!addModifiedProperty(object, JSID_VOID))
                    return;
                if (op == JSOP_SETHOLE && !addGrowArray(object))
                    return;
            }
            break;
          }

          default:
            break;
        }

        offset = successorOffset;
    }
}

bool
LoopState::addGrowArray(types::TypeObject *object)
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
LoopState::addModifiedProperty(types::TypeObject *object, jsid id)
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
LoopState::hasGrowArray(types::TypeObject *object)
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
LoopState::hasModifiedProperty(types::TypeObject *object, jsid id)
{
    if (unknownModset)
        return true;
    for (unsigned i = 0; i < modifiedProperties.length(); i++) {
        if (modifiedProperties[i].object == object && modifiedProperties[i].id == id)
            return true;
    }
    return false;
}

inline types::TypeSet *
LoopState::poppedTypes(jsbytecode *pc, unsigned which)
{
    StackAnalysis::PoppedValue value = stack.popped(pc, which);
    if (value.offset == StackAnalysis::UNKNOWN_PUSHED)
        return NULL;
    return script->types->pushed(value.offset, value.which);
}
