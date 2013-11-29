/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/BaselineCompiler-shared.h"

#include "jit/BaselineIC.h"
#include "jit/VMFunctions.h"

using namespace js;
using namespace js::jit;

BaselineCompilerShared::BaselineCompilerShared(JSContext *cx, TempAllocator &alloc, HandleScript script)
  : cx(cx),
    script(cx, script),
    pc(script->code),
    ionCompileable_(jit::IsIonEnabled(cx) && CanIonCompileScript(cx, script, false)),
    ionOSRCompileable_(jit::IsIonEnabled(cx) && CanIonCompileScript(cx, script, true)),
    debugMode_(cx->compartment()->debugMode()),
    alloc_(alloc),
    analysis_(alloc, script),
    frame(cx, script, masm),
    stubSpace_(),
    icEntries_(),
    pcMappingEntries_(),
    icLoadLabels_(),
    pushedBeforeCall_(0),
    inCall_(false),
    spsPushToggleOffset_()
{ }

bool
BaselineCompilerShared::callVM(const VMFunction &fun, CallVMPhase phase)
{
    IonCode *code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

#ifdef DEBUG
    // Assert prepareVMCall() has been called.
    JS_ASSERT(inCall_);
    inCall_ = false;
#endif

    // Compute argument size. Note that this include the size of the frame pointer
    // pushed by prepareVMCall.
    uint32_t argSize = fun.explicitStackSlots() * sizeof(void *) + sizeof(void *);

    // Assert all arguments were pushed.
    JS_ASSERT(masm.framePushed() - pushedBeforeCall_ == argSize);

    Address frameSizeAddress(BaselineFrameReg, BaselineFrame::reverseOffsetOfFrameSize());
    uint32_t frameVals = frame.nlocals() + frame.stackDepth();
    uint32_t frameBaseSize = BaselineFrame::FramePointerOffset + BaselineFrame::Size();
    uint32_t frameFullSize = frameBaseSize + (frameVals * sizeof(Value));
    if (phase == POST_INITIALIZE) {
        masm.store32(Imm32(frameFullSize), frameSizeAddress);
        uint32_t descriptor = MakeFrameDescriptor(frameFullSize + argSize, IonFrame_BaselineJS);
        masm.push(Imm32(descriptor));

    } else if (phase == PRE_INITIALIZE) {
        masm.store32(Imm32(frameBaseSize), frameSizeAddress);
        uint32_t descriptor = MakeFrameDescriptor(frameBaseSize + argSize, IonFrame_BaselineJS);
        masm.push(Imm32(descriptor));

    } else {
        JS_ASSERT(phase == CHECK_OVER_RECURSED);
        Label afterWrite;
        Label writePostInitialize;

        // If OVER_RECURSED is set, then frame locals haven't been pushed yet.
        masm.branchTest32(Assembler::Zero,
                          frame.addressOfFlags(),
                          Imm32(BaselineFrame::OVER_RECURSED),
                          &writePostInitialize);

        masm.move32(Imm32(frameBaseSize), BaselineTailCallReg);
        masm.jump(&afterWrite);

        masm.bind(&writePostInitialize);
        masm.move32(Imm32(frameFullSize), BaselineTailCallReg);

        masm.bind(&afterWrite);
        masm.store32(BaselineTailCallReg, frameSizeAddress);
        masm.add32(Imm32(argSize), BaselineTailCallReg);
        masm.makeFrameDescriptor(BaselineTailCallReg, IonFrame_BaselineJS);
        masm.push(BaselineTailCallReg);
    }

    // Perform the call.
    masm.call(code);
    uint32_t callOffset = masm.currentOffset();
    masm.pop(BaselineFrameReg);

    // Add a fake ICEntry (without stubs), so that the return offset to
    // pc mapping works.
    ICEntry entry(pc - script->code, false);
    entry.setReturnOffset(callOffset);

    return icEntries_.append(entry);
}
