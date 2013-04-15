/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StubCalls.h"
#include "StubCompiler.h"
#include "Compiler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "FrameState-inl.h"

using namespace js;
using namespace mjit;

StubCompiler::StubCompiler(JSContext *cx, mjit::Compiler &cc, FrameState &frame)
: cx(cx),
  cc(cc),
  frame(frame),
  masm(&cc.sps, &cc.PC),
  generation(1),
  lastGeneration(0),
  exits(CompilerAllocPolicy(cx, cc)),
  joins(CompilerAllocPolicy(cx, cc)),
  scriptJoins(CompilerAllocPolicy(cx, cc)),
  jumpList(SystemAllocPolicy())
{
#ifdef DEBUG
    masm.setSpewPath(true);
#endif
}

void
StubCompiler::linkExitDirect(Jump j, Label L)
{
    exits.append(CrossPatch(j, L));
}

JSC::MacroAssembler::Label
StubCompiler::syncExit(Uses uses)
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW MERGE CODE ---- \n");

    if (lastGeneration == generation) {
        Jump j2 = masm.jump();
        jumpList.append(j2);
    }

    Label l = masm.label();
    frame.sync(masm, uses);
    lastGeneration = generation;

    JaegerSpew(JSpew_Insns, " ---- END SLOW MERGE CODE ---- \n");

    return l;
}

JSC::MacroAssembler::Label
StubCompiler::syncExitAndJump(Uses uses)
{
    Label l = syncExit(uses);
    Jump j2 = masm.jump();
    jumpList.append(j2);
    /* Suppress jumping on next sync/link. */
    generation++;
    return l;
}

// Link an exit from the fast path to a slow path. This does two main things:
// (a) links the given jump to the slow path, and (b) generates a prolog for
// the slow path that syncs frame state for a slow call that uses |uses|
// values from the top of the stack.
//
// The return value is the label for the start of the merge code. This is
// the correct place to jump to in order to execute the slow path being
// generated here.
//
// Note 1: Slow path generation is interleaved with fast path generation, but
// the slow path goes into a separate buffer. The slow path code is appended
// to the fast path code to keep it nearby in code memory.
//
// Note 2: A jump from the fast path to the slow path is called an "exit".
//         A jump from the slow path to the fast path is called a "rejoin".
JSC::MacroAssembler::Label
StubCompiler::linkExit(Jump j, Uses uses)
{
    Label l = syncExit(uses);
    linkExitDirect(j, l);
    return l;
}

// Special version of linkExit that is used when there is a JavaScript
// control-flow branch after the slow path. Our compilation strategy
// requires the JS frame to be fully materialized in memory across branches.
// This function does a linkExit and also fully materializes the frame.
void
StubCompiler::linkExitForBranch(Jump j)
{
    Label l = syncExit(Uses(frame.frameSlots()));
    linkExitDirect(j, l);
}

void
StubCompiler::leave()
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW LEAVE CODE ---- \n");
    for (size_t i = 0; i < jumpList.length(); i++)
        jumpList[i].linkTo(masm.label(), &masm);
    jumpList.clear();
    generation++;
    JaegerSpew(JSpew_Insns, " ---- END SLOW LEAVE CODE ---- \n");
}

void
StubCompiler::rejoin(Changes changes)
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");

    frame.merge(masm, changes);

    unsigned index = crossJump(masm.jump(), cc.getLabel());
    if (cc.loop)
        cc.loop->addJoin(index, false);

    JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");
}

void
StubCompiler::linkRejoin(Jump j)
{
    crossJump(j, cc.getLabel());
}

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::DataLabelPtr DataLabelPtr;

JSC::MacroAssembler::Call
StubCompiler::emitStubCall(void *ptr, RejoinState rejoin, Uses uses)
{
    return emitStubCall(ptr, rejoin, uses, frame.totalDepth());
}

JSC::MacroAssembler::Call
StubCompiler::emitStubCall(void *ptr, RejoinState rejoin, Uses uses, int32_t slots)
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW CALL CODE ---- \n");
    masm.bumpStubCount(cc.script_, cc.PC, Registers::tempCallReg());
    DataLabelPtr inlinePatch;
    Call cl = masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                  ptr, cc.outerPC(), &inlinePatch, slots);
    JaegerSpew(JSpew_Insns, " ---- END SLOW CALL CODE ---- \n");

    /* Add the call site for debugging and recompilation. */
    Compiler::InternalCallSite site(masm.callReturnOffset(cl),
                                    cc.inlineIndex(), cc.inlinePC(),
                                    rejoin, true);
    site.inlinePatch = inlinePatch;

    /* Add a hook for restoring loop invariants if necessary. */
    if (cc.loop && cc.loop->generatingInvariants()) {
        site.loopJumpLabel = masm.label();
        Jump j = masm.jump();
        Label l = masm.label();
        /* MissedBoundsCheck* are not actually called, so f.regs need to be written before InvariantFailure. */
        bool entry = (ptr == JS_FUNC_TO_DATA_PTR(void *, stubs::MissedBoundsCheckEntry))
                  || (ptr == JS_FUNC_TO_DATA_PTR(void *, stubs::MissedBoundsCheckHead));
        cc.loop->addInvariantCall(j, l, true, entry, cc.callSites.length(), uses);
    }

    cc.addCallSite(site);
    return cl;
}

void
StubCompiler::fixCrossJumps(uint8_t *ncode, size_t offset, size_t total)
{
    JSC::LinkBuffer fast(ncode, total, JSC::JAEGER_CODE);
    JSC::LinkBuffer slow(ncode + offset, total - offset, JSC::JAEGER_CODE);

    for (size_t i = 0; i < exits.length(); i++)
        fast.link(exits[i].from, slow.locationOf(exits[i].to));

    for (size_t i = 0; i < scriptJoins.length(); i++) {
        const CrossJumpInScript &cj = scriptJoins[i];
        slow.link(cj.from, fast.locationOf(cc.labelOf(cj.pc, cj.inlineIndex)));
    }

    for (size_t i = 0; i < joins.length(); i++)
        slow.link(joins[i].from, fast.locationOf(joins[i].to));
}

unsigned
StubCompiler::crossJump(Jump j, Label L)
{
    joins.append(CrossPatch(j, L));

    /* This won't underflow, as joins has space preallocated for some entries. */
    return joins.length() - 1;
}

bool
StubCompiler::jumpInScript(Jump j, jsbytecode *target)
{
    if (cc.knownJump(target)) {
        unsigned index = crossJump(j, cc.labelOf(target, cc.inlineIndex()));
        if (cc.loop)
            cc.loop->addJoin(index, false);
    } else {
        if (!scriptJoins.append(CrossJumpInScript(j, target, cc.inlineIndex())))
            return false;
        if (cc.loop)
            cc.loop->addJoin(scriptJoins.length() - 1, true);
    }
    return true;
}

void
StubCompiler::patchJoin(unsigned i, bool script, Assembler::Address address, AnyRegisterID reg)
{
    Jump &j = script ? scriptJoins[i].from : joins[i].from;
    j.linkTo(masm.label(), &masm);

    if (reg.isReg())
        masm.loadPayload(address, reg.reg());
    else
        masm.loadDouble(address, reg.fpreg());

    j = masm.jump();
}
