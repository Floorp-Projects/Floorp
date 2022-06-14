/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initializer_list>

#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#include "jit/MacroAssembler.h"
#include "vm/JitActivation.h"
#include "vm/JSContext.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

void JitRuntime::generateProfilerExitFrameTailStub(MacroAssembler& masm,
                                                   Label* profilerExitTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateProfilerExitFrameTailStub");

  profilerExitFrameTailOffset_ = startTrampolineCode(masm);
  masm.bind(profilerExitTail);

  // Offset from frame pointer to CommonFrameLayout.
  static constexpr size_t FPOffset = CommonFrameLayout::FramePointerOffset;

  // Assert the caller frame's type is one of the types we expect.
  auto emitAssertPrevFrameType = [&masm](
                                     Register framePtr, Register scratch,
                                     std::initializer_list<FrameType> types) {
#ifdef DEBUG
    masm.loadPtr(
        Address(framePtr, FPOffset + CommonFrameLayout::offsetOfDescriptor()),
        scratch);
    masm.and32(Imm32(FRAMETYPE_MASK), scratch);

    Label checkOk;
    for (FrameType type : types) {
      masm.branch32(Assembler::Equal, scratch, Imm32(type), &checkOk);
    }
    masm.assumeUnreachable("Unexpected previous frame");
    masm.bind(&checkOk);
#else
    (void)masm;
#endif
  };

  AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
  regs.take(JSReturnOperand);
  Register scratch = regs.takeAny();

  // The code generated below expects that the current stack pointer points
  // to an Ion or Baseline frame, at the state it would be immediately before
  // a ret(). Thus, after this stub's business is done, it executes a ret() and
  // returns directly to the caller frame, on behalf of the callee script that
  // jumped to this code. Note that the frame pointer has already been popped
  // before entering this stub, so it now points to the caller frame.
  //
  //
  // Thus the expected stack is:
  //
  //                                   StackPointer ----+
  //                                                    v
  // ..., ActualArgc, CalleeToken, Descriptor, ReturnAddr
  // MEM-HI                                       MEM-LOW
  //
  //
  // The generated jitcode is responsible for overwriting the
  // jitActivation->lastProfilingFrame field with a pointer to the previous
  // Ion or Baseline jit-frame that was pushed before this one. It is also
  // responsible for overwriting jitActivation->lastProfilingCallSite with
  // the return address into that frame.
  //
  // So this jitcode is responsible for "walking up" the jit stack, finding
  // the previous Ion or Baseline JS frame, and storing its address and the
  // return address into the appropriate fields on the current jitActivation.
  //
  // There are a fixed number of different path types that can lead to the
  // current frame, which is either a Baseline or Ion frame:
  //
  // <Baseline-Or-Ion>
  // ^
  // |
  // ^--- Ion (or Baseline JSOp::Resume)
  // |
  // ^--- Baseline Stub <---- Baseline
  // |
  // ^--- IonICCall <---- Ion
  // |
  // ^--- Arguments Rectifier
  // |    ^
  // |    |
  // |    ^--- Ion
  // |    |
  // |    ^--- Baseline Stub <---- Baseline
  // |    |
  // |    ^--- Entry Frame (CppToJSJit or WasmToJSJit)
  // |
  // ^--- Entry Frame (CppToJSJit or WasmToJSJit)

  Register actReg = regs.takeAny();
  masm.loadJSContext(actReg);
  masm.loadPtr(Address(actReg, offsetof(JSContext, profilingActivation_)),
               actReg);

  Address lastProfilingFrame(actReg,
                             JitActivation::offsetOfLastProfilingFrame());
  Address lastProfilingCallSite(actReg,
                                JitActivation::offsetOfLastProfilingCallSite());

#ifdef DEBUG
  // Ensure that frame we are exiting is current lastProfilingFrame
  {
    masm.loadPtr(lastProfilingFrame, scratch);
    Label checkOk;
    masm.branchPtr(Assembler::Equal, scratch, ImmWord(0), &checkOk);
    masm.branchStackPtr(Assembler::Equal, scratch, &checkOk);
    masm.assumeUnreachable(
        "Mismatch between stored lastProfilingFrame and current stack "
        "pointer.");
    masm.bind(&checkOk);
  }
#endif

  // Move SP/FP into scratch registers and use those scratch registers below, to
  // allow unwrapping rectifier frames without clobbering FP/SP.
  Register spScratch = regs.takeAny();
  Register fpScratch = regs.takeAny();
  masm.moveStackPtrTo(spScratch);
  masm.mov(FramePointer, fpScratch);

  Label again;
  masm.bind(&again);

  // Load the frame descriptor into |scratch|, figure out what to do depending
  // on its type.
  masm.loadPtr(Address(spScratch, JitFrameLayout::offsetOfDescriptor()),
               scratch);
  masm.and32(Imm32(FRAMETYPE_MASK), scratch);

  // Handling of each case is dependent on FrameDescriptor.type
  Label handle_BaselineOrIonJS;
  Label handle_BaselineStub;
  Label handle_Rectifier;
  Label handle_IonICCall;
  Label handle_Entry;

  // We check for IonJS and BaselineStub first because these are the most common
  // types. Calls from Baseline are usually from a BaselineStub frame.
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::IonJS),
                &handle_BaselineOrIonJS);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::BaselineStub),
                &handle_BaselineStub);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::Rectifier),
                &handle_Rectifier);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::CppToJSJit),
                &handle_Entry);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::BaselineJS),
                &handle_BaselineOrIonJS);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::IonICCall),
                &handle_IonICCall);
  masm.branch32(Assembler::Equal, scratch, Imm32(FrameType::WasmToJSJit),
                &handle_Entry);

  masm.assumeUnreachable(
      "Invalid caller frame type when returning from a JIT frame.");

  masm.bind(&handle_BaselineOrIonJS);
  {
    // Returning directly to a Baseline or Ion frame.

    // lastProfilingCallSite := ReturnAddress
    masm.loadPtr(Address(spScratch, JitFrameLayout::offsetOfReturnAddress()),
                 scratch);
    masm.storePtr(scratch, lastProfilingCallSite);

    // lastProfilingFrame := CallerFrame
    masm.computeEffectiveAddress(Address(fpScratch, FPOffset), scratch);
    masm.storePtr(scratch, lastProfilingFrame);
    masm.ret();
  }

  // Shared implementation for BaselineStub and IonICCall frames.
  auto emitHandleStubFrame = [&]() {
    // lastProfilingCallSite := StubFrame.ReturnAddress
    masm.loadPtr(Address(fpScratch,
                         FPOffset + CommonFrameLayout::offsetOfReturnAddress()),
                 scratch);
    masm.storePtr(scratch, lastProfilingCallSite);

    // lastProfilingFrame := StubFrame.CallerFrame
    masm.loadPtr(Address(fpScratch, 0), scratch);
    masm.addPtr(Imm32(FPOffset), scratch);
    masm.storePtr(scratch, lastProfilingFrame);
    masm.ret();
  };

  masm.bind(&handle_BaselineStub);
  {
    // BaselineJS => BaselineStub frame.
    emitAssertPrevFrameType(fpScratch, scratch, {FrameType::BaselineJS});
    emitHandleStubFrame();
  }

  masm.bind(&handle_IonICCall);
  {
    // IonJS => IonICCall frame.
    emitAssertPrevFrameType(fpScratch, scratch, {FrameType::IonJS});
    emitHandleStubFrame();
  }

  masm.bind(&handle_Rectifier);
  {
    // There can be multiple previous frame types so just "unwrap" the arguments
    // rectifier frame and try again.
    emitAssertPrevFrameType(fpScratch, scratch,
                            {FrameType::IonJS, FrameType::BaselineStub,
                             FrameType::CppToJSJit, FrameType::WasmToJSJit});
    masm.computeEffectiveAddress(Address(fpScratch, FPOffset), spScratch);
    masm.loadPtr(Address(fpScratch, 0), fpScratch);
    masm.jump(&again);
  }

  masm.bind(&handle_Entry);
  {
    // FrameType::CppToJSJit / FrameType::WasmToJSJit
    //
    // A fast-path wasm->jit transition frame is an entry frame from the point
    // of view of the JIT.
    // Store null into both fields.
    masm.movePtr(ImmPtr(nullptr), scratch);
    masm.storePtr(scratch, lastProfilingCallSite);
    masm.storePtr(scratch, lastProfilingFrame);
    masm.ret();
  }
}
