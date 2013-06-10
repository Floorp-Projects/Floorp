/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineCompiler-shared.h"
#include "ion/BaselineIC.h"
#include "ion/VMFunctions.h"

using namespace js;
using namespace js::ion;

BaselineCompilerShared::BaselineCompilerShared(JSContext *cx, HandleScript script)
  : cx(cx),
    script(cx, script),
    pc(script->code),
    ionCompileable_(ion::IsEnabled(cx) && CanIonCompileScript(cx, script, false)),
    ionOSRCompileable_(ion::IsEnabled(cx) && CanIonCompileScript(cx, script, true)),
    debugMode_(cx->compartment()->debugMode()),
    analysis_(script),
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
BaselineCompilerShared::callVM(const VMFunction &fun)
{
    IonCompartment *ion = cx->compartment()->ionCompartment();
    IonCode *code = ion->getVMWrapper(fun);
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

    uint32_t frameSize = BaselineFrame::FramePointerOffset + BaselineFrame::Size() +
        (frame.nlocals() + frame.stackDepth()) * sizeof(Value);

    masm.store32(Imm32(frameSize), Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFrameSize()));

    uint32_t descriptor = MakeFrameDescriptor(frameSize + argSize, IonFrame_BaselineJS);
    masm.push(Imm32(descriptor));

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
