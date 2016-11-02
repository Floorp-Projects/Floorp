/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "asmjs/WasmFrameIterator.h"

#include "asmjs/WasmInstance.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::DebugOnly;
using mozilla::Swap;

/*****************************************************************************/
// FrameIterator implementation

static void*
ReturnAddressFromFP(void* fp)
{
    return reinterpret_cast<Frame*>(fp)->returnAddress;
}

static uint8_t*
CallerFPFromFP(void* fp)
{
    return reinterpret_cast<Frame*>(fp)->callerFP;
}

FrameIterator::FrameIterator()
  : activation_(nullptr),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(nullptr),
    pc_(nullptr),
    missingFrameMessage_(false)
{
    MOZ_ASSERT(done());
}

FrameIterator::FrameIterator(const WasmActivation& activation)
  : activation_(&activation),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(activation.fp()),
    pc_(nullptr),
    missingFrameMessage_(false)
{
    if (fp_) {
        settle();
        return;
    }

    void* pc = activation.resumePC();
    if (!pc) {
        MOZ_ASSERT(done());
        return;
    }
    pc_ = (uint8_t*)pc;

    code_ = activation_->compartment()->wasm.lookupCode(pc);
    MOZ_ASSERT(code_);

    const CodeRange* codeRange = code_->lookupRange(pc);
    MOZ_ASSERT(codeRange);

    if (codeRange->kind() == CodeRange::Function)
        codeRange_ = codeRange;
    else
        missingFrameMessage_ = true;

    MOZ_ASSERT(!done());
}

bool
FrameIterator::done() const
{
    return !codeRange_ && !missingFrameMessage_;
}

void
FrameIterator::operator++()
{
    MOZ_ASSERT(!done());
    if (fp_) {
        DebugOnly<uint8_t*> oldfp = fp_;
        fp_ += callsite_->stackDepth();
        MOZ_ASSERT_IF(code_->profilingEnabled(), fp_ == CallerFPFromFP(oldfp));
        settle();
    } else if (codeRange_) {
        MOZ_ASSERT(codeRange_);
        codeRange_ = nullptr;
        missingFrameMessage_ = true;
    } else {
        MOZ_ASSERT(missingFrameMessage_);
        missingFrameMessage_ = false;
    }
}

void
FrameIterator::settle()
{
    void* returnAddress = ReturnAddressFromFP(fp_);

    code_ = activation_->compartment()->wasm.lookupCode(returnAddress);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(returnAddress);
    MOZ_ASSERT(codeRange_);

    switch (codeRange_->kind()) {
      case CodeRange::Function:
        pc_ = (uint8_t*)returnAddress;
        callsite_ = code_->lookupCallSite(returnAddress);
        MOZ_ASSERT(callsite_);
        break;
      case CodeRange::Entry:
        fp_ = nullptr;
        pc_ = nullptr;
        code_ = nullptr;
        codeRange_ = nullptr;
        MOZ_ASSERT(done());
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
      case CodeRange::Inline:
      case CodeRange::FarJumpIsland:
        MOZ_CRASH("Should not encounter an exit during iteration");
    }
}

const char*
FrameIterator::filename() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().filename.get();
}

const char16_t*
FrameIterator::displayURL() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().displayURL();
}

bool
FrameIterator::mutedErrors() const
{
    MOZ_ASSERT(!done());
    return code_->metadata().mutedErrors();
}

JSAtom*
FrameIterator::functionDisplayAtom() const
{
    MOZ_ASSERT(!done());

    JSContext* cx = activation_->cx();

    if (missingFrameMessage_) {
        const char* msg = "asm.js/wasm frames may be missing; enable the profiler before running "
                          "to see all frames";
        JSAtom* atom = Atomize(cx, msg, strlen(msg));
        if (!atom) {
            cx->clearPendingException();
            return cx->names().empty;
        }

        return atom;
    }

    MOZ_ASSERT(codeRange_);

    JSAtom* atom = code_->getFuncDefAtom(cx, codeRange_->funcDefIndex());
    if (!atom) {
        cx->clearPendingException();
        return cx->names().empty;
    }

    return atom;
}

unsigned
FrameIterator::lineOrBytecode() const
{
    MOZ_ASSERT(!done());
    return callsite_ ? callsite_->lineOrBytecode()
                     : (codeRange_ ? codeRange_->funcLineOrBytecode() : 0);
}

/*****************************************************************************/
// Prologue/epilogue code generation

// These constants reflect statically-determined offsets in the profiling
// prologue/epilogue. The offsets are dynamically asserted during code
// generation.
#if defined(JS_CODEGEN_X64)
# if defined(DEBUG)
static const unsigned PushedRetAddr = 0;
static const unsigned PostStorePrePopFP = 0;
# endif
static const unsigned PushedFP = 23;
static const unsigned StoredFP = 30;
#elif defined(JS_CODEGEN_X86)
# if defined(DEBUG)
static const unsigned PushedRetAddr = 0;
static const unsigned PostStorePrePopFP = 0;
# endif
static const unsigned PushedFP = 14;
static const unsigned StoredFP = 17;
#elif defined(JS_CODEGEN_ARM)
static const unsigned PushedRetAddr = 4;
static const unsigned PushedFP = 24;
static const unsigned StoredFP = 28;
static const unsigned PostStorePrePopFP = 4;
#elif defined(JS_CODEGEN_ARM64)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 0;
static const unsigned StoredFP = 0;
static const unsigned PostStorePrePopFP = 0;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned PushedRetAddr = 8;
static const unsigned PushedFP = 32;
static const unsigned StoredFP = 36;
static const unsigned PostStorePrePopFP = 4;
#elif defined(JS_CODEGEN_NONE)
# if defined(DEBUG)
static const unsigned PushedRetAddr = 0;
static const unsigned PostStorePrePopFP = 0;
# endif
static const unsigned PushedFP = 1;
static const unsigned StoredFP = 1;
#else
# error "Unknown architecture!"
#endif

static void
PushRetAddr(MacroAssembler& masm)
{
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    masm.push(ra);
#else
    // The x86/x64 call instruction pushes the return address.
#endif
}

// Generate a prologue that maintains WasmActivation::fp as the virtual frame
// pointer so that ProfilingFrameIterator can walk the stack at any pc in
// generated code.
static void
GenerateProfilingPrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                          ProfilingOffsets* offsets)
{
    Register scratch = ABINonArgReg0;

    // ProfilingFrameIterator needs to know the offsets of several key
    // instructions from entry. To save space, we make these offsets static
    // constants and assert that they match the actual codegen below. On ARM,
    // this requires AutoForbidPools to prevent a constant pool from being
    // randomly inserted between two instructions.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 7);
#endif

        offsets->begin = masm.currentOffset();

        PushRetAddr(masm);
        MOZ_ASSERT_IF(!masm.oom(), PushedRetAddr == masm.currentOffset() - offsets->begin);

        masm.loadWasmActivationFromSymbolicAddress(scratch);
        masm.push(Address(scratch, WasmActivation::offsetOfFP()));
        MOZ_ASSERT_IF(!masm.oom(), PushedFP == masm.currentOffset() - offsets->begin);

        masm.storePtr(masm.getStackPointer(), Address(scratch, WasmActivation::offsetOfFP()));
        MOZ_ASSERT_IF(!masm.oom(), StoredFP == masm.currentOffset() - offsets->begin);
    }

    if (reason != ExitReason::None)
        masm.store32(Imm32(int32_t(reason)), Address(scratch, WasmActivation::offsetOfExitReason()));

    if (framePushed)
        masm.subFromStackPtr(Imm32(framePushed));
}

// Generate the inverse of GenerateProfilingPrologue.
static void
GenerateProfilingEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                          ProfilingOffsets* offsets)
{
    Register scratch = ABINonArgReturnReg0;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    Register scratch2 = ABINonArgReturnReg1;
#endif

    if (framePushed)
        masm.addToStackPtr(Imm32(framePushed));

    masm.loadWasmActivationFromSymbolicAddress(scratch);

    if (reason != ExitReason::None) {
        masm.store32(Imm32(int32_t(ExitReason::None)),
                     Address(scratch, WasmActivation::offsetOfExitReason()));
    }

    // ProfilingFrameIterator assumes fixed offsets of the last few
    // instructions from profilingReturn, so AutoForbidPools to ensure that
    // unintended instructions are not automatically inserted.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 4);
#endif

        // sp protects the stack from clobber via asynchronous signal handlers
        // and the async interrupt exit. Since activation.fp can be read at any
        // time and still points to the current frame, be careful to only update
        // sp after activation.fp has been repointed to the caller's frame.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        masm.loadPtr(Address(masm.getStackPointer(), 0), scratch2);
        masm.storePtr(scratch2, Address(scratch, WasmActivation::offsetOfFP()));
        DebugOnly<uint32_t> prePop = masm.currentOffset();
        masm.addToStackPtr(Imm32(sizeof(void *)));
        MOZ_ASSERT_IF(!masm.oom(), PostStorePrePopFP == masm.currentOffset() - prePop);
#else
        masm.pop(Address(scratch, WasmActivation::offsetOfFP()));
        MOZ_ASSERT(PostStorePrePopFP == 0);
#endif

        offsets->profilingReturn = masm.currentOffset();
        masm.ret();
    }
}

// In profiling mode, we need to maintain fp so that we can unwind the stack at
// any pc. In non-profiling mode, the only way to observe WasmActivation::fp is
// to call out to C++ so, as an optimization, we don't update fp. To avoid
// recompilation when the profiling mode is toggled, we generate both prologues
// a priori and switch between prologues when the profiling mode is toggled.
// Specifically, ToggleProfiling patches all callsites to either call the
// profiling or non-profiling entry point.
void
wasm::GenerateFunctionPrologue(MacroAssembler& masm, unsigned framePushed, const SigIdDesc& sigId,
                               const TrapOffset& trapOffset, FuncOffsets* offsets)
{
#if defined(JS_CODEGEN_ARM)
    // Flush pending pools so they do not get dumped between the 'begin' and
    // 'entry' offsets since the difference must be less than UINT8_MAX.
    masm.flushBuffer();
#endif

    masm.haltingAlign(CodeAlignment);

    GenerateProfilingPrologue(masm, framePushed, ExitReason::None, offsets);
    Label body;
    masm.jump(&body);

    // Generate table entry thunk:
    masm.haltingAlign(CodeAlignment);
    offsets->tableEntry = masm.currentOffset();
    TrapDesc trap(trapOffset, Trap::IndirectCallBadSig, masm.framePushed());
    switch (sigId.kind()) {
      case SigIdDesc::Kind::Global: {
        Register scratch = WasmTableCallScratchReg;
        masm.loadWasmGlobalPtr(sigId.globalDataOffset(), scratch);
        masm.branch32(Assembler::Condition::NotEqual, WasmTableCallSigReg, scratch, trap);
        break;
      }
      case SigIdDesc::Kind::Immediate:
        masm.branch32(Assembler::Condition::NotEqual, WasmTableCallSigReg, Imm32(sigId.immediate()), trap);
        break;
      case SigIdDesc::Kind::None:
        break;
    }
    offsets->tableProfilingJump = masm.nopPatchableToNearJump().offset();

    // Generate normal prologue:
    masm.nopAlign(CodeAlignment);
    offsets->nonProfilingEntry = masm.currentOffset();
    PushRetAddr(masm);
    masm.subFromStackPtr(Imm32(framePushed + FrameBytesAfterReturnAddress));

    // Prologue join point, body begin:
    masm.bind(&body);
    masm.setFramePushed(framePushed);
}

// Similar to GenerateFunctionPrologue (see comment), we generate both a
// profiling and non-profiling epilogue a priori. When the profiling mode is
// toggled, ToggleProfiling patches the 'profiling jump' to either be a nop
// (falling through to the normal prologue) or a jump (jumping to the profiling
// epilogue).
void
wasm::GenerateFunctionEpilogue(MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets)
{
    MOZ_ASSERT(masm.framePushed() == framePushed);

#if defined(JS_CODEGEN_ARM)
    // Flush pending pools so they do not get dumped between the profilingReturn
    // and profilingJump/profilingEpilogue offsets since the difference must be
    // less than UINT8_MAX.
    masm.flushBuffer();
#endif

    // Generate a nop that is overwritten by a jump to the profiling epilogue
    // when profiling is enabled.
    offsets->profilingJump = masm.nopPatchableToNearJump().offset();

    // Normal epilogue:
    masm.addToStackPtr(Imm32(framePushed + FrameBytesAfterReturnAddress));
    masm.ret();
    masm.setFramePushed(0);

    // Profiling epilogue:
    offsets->profilingEpilogue = masm.currentOffset();
    GenerateProfilingEpilogue(masm, framePushed, ExitReason::None, offsets);
}

void
wasm::GenerateExitPrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           ProfilingOffsets* offsets)
{
    masm.haltingAlign(CodeAlignment);
    GenerateProfilingPrologue(masm, framePushed, reason, offsets);
    masm.setFramePushed(framePushed);
}

void
wasm::GenerateExitEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           ProfilingOffsets* offsets)
{
    // Inverse of GenerateExitPrologue:
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateProfilingEpilogue(masm, framePushed, reason, offsets);
    masm.setFramePushed(0);
}

/*****************************************************************************/
// ProfilingFrameIterator

ProfilingFrameIterator::ProfilingFrameIterator()
  : activation_(nullptr),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::None)
{
    MOZ_ASSERT(done());
}

ProfilingFrameIterator::ProfilingFrameIterator(const WasmActivation& activation)
  : activation_(&activation),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::None)
{
    // If profiling hasn't been enabled for this instance, then CallerFPFromFP
    // will be trash, so ignore the entire activation. In practice, this only
    // happens if profiling is enabled while the instance is on the stack (in
    // which case profiling will be enabled when the instance becomes inactive
    // and gets called again).
    if (!activation_->compartment()->wasm.profilingEnabled()) {
        MOZ_ASSERT(done());
        return;
    }

    initFromFP();
}

static inline void
AssertMatchesCallSite(const WasmActivation& activation, void* callerPC, void* callerFP, void* fp)
{
#ifdef DEBUG
    Code* code = activation.compartment()->wasm.lookupCode(callerPC);
    MOZ_ASSERT(code);

    const CodeRange* callerCodeRange = code->lookupRange(callerPC);
    MOZ_ASSERT(callerCodeRange);

    if (callerCodeRange->kind() == CodeRange::Entry) {
        MOZ_ASSERT(callerFP == nullptr);
        return;
    }

    const CallSite* callsite = code->lookupCallSite(callerPC);
    MOZ_ASSERT(callsite);

    MOZ_ASSERT(callerFP == (uint8_t*)fp + callsite->stackDepth());
#endif
}

void
ProfilingFrameIterator::initFromFP()
{
    uint8_t* fp = activation_->fp();
    stackAddress_ = fp;

    // If a signal was handled while entering an activation, the frame will
    // still be null.
    if (!fp) {
        MOZ_ASSERT(done());
        return;
    }

    void* pc = ReturnAddressFromFP(fp);

    code_ = activation_->compartment()->wasm.lookupCode(pc);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(pc);
    MOZ_ASSERT(codeRange_);

    // Since we don't have the pc for fp, start unwinding at the caller of fp
    // (ReturnAddressFromFP(fp)). This means that the innermost frame is
    // skipped. This is fine because:
    //  - for import exit calls, the innermost frame is a thunk, so the first
    //    frame that shows up is the function calling the import;
    //  - for Math and other builtin calls as well as interrupts, we note the absence
    //    of an exit reason and inject a fake "builtin" frame; and
    //  - for async interrupts, we just accept that we'll lose the innermost frame.
    switch (codeRange_->kind()) {
      case CodeRange::Entry:
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case CodeRange::Function:
        fp = CallerFPFromFP(fp);
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_, fp);
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
      case CodeRange::Inline:
      case CodeRange::FarJumpIsland:
        MOZ_CRASH("Unexpected CodeRange kind");
    }

    // The iterator inserts a pretend innermost frame for non-None ExitReasons.
    // This allows the variety of exit reasons to show up in the callstack.
    exitReason_ = activation_->exitReason();

    // In the case of calls to builtins or asynchronous interrupts, no exit path
    // is taken so the exitReason is None. Coerce these to the Native exit
    // reason so that self-time is accounted for.
    if (exitReason_ == ExitReason::None)
        exitReason_ = ExitReason::Native;

    MOZ_ASSERT(!done());
}

typedef JS::ProfilingFrameIterator::RegisterState RegisterState;

static bool
InThunk(const CodeRange& codeRange, uint32_t offsetInModule)
{
    if (codeRange.kind() == CodeRange::FarJumpIsland)
        return true;

    return codeRange.isFunction() &&
           offsetInModule >= codeRange.funcTableEntry() &&
           offsetInModule < codeRange.funcNonProfilingEntry();
}

ProfilingFrameIterator::ProfilingFrameIterator(const WasmActivation& activation,
                                               const RegisterState& state)
  : activation_(&activation),
    code_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::None)
{
    // If profiling hasn't been enabled for this instance, then CallerFPFromFP
    // will be trash, so ignore the entire activation. In practice, this only
    // happens if profiling is enabled while the instance is on the stack (in
    // which case profiling will be enabled when the instance becomes inactive
    // and gets called again).
    if (!activation_->compartment()->wasm.profilingEnabled()) {
        MOZ_ASSERT(done());
        return;
    }

    // If pc isn't in the instance's code, we must have exited the code via an
    // exit trampoline or signal handler.
    code_ = activation_->compartment()->wasm.lookupCode(state.pc);
    if (!code_) {
        initFromFP();
        return;
    }

    // Note: fp may be null while entering and leaving the activation.
    uint8_t* fp = activation.fp();

    const CodeRange* codeRange = code_->lookupRange(state.pc);
    switch (codeRange->kind()) {
      case CodeRange::Function:
      case CodeRange::FarJumpIsland:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit: {
        // When the pc is inside the prologue/epilogue, the innermost call's
        // Frame is not complete and thus fp points to the second-to-innermost
        // call's Frame. Since fp can only tell you about its caller (via
        // ReturnAddressFromFP(fp)), naively unwinding while pc is in the
        // prologue/epilogue would skip the second-to- innermost call. To avoid
        // this problem, we use the static structure of the code in the prologue
        // and epilogue to do the Right Thing.
        uint32_t offsetInModule = (uint8_t*)state.pc - code_->segment().base();
        MOZ_ASSERT(offsetInModule >= codeRange->begin());
        MOZ_ASSERT(offsetInModule < codeRange->end());
        uint32_t offsetInCodeRange = offsetInModule - codeRange->begin();
        void** sp = (void**)state.sp;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        if (offsetInCodeRange < PushedRetAddr || InThunk(*codeRange, offsetInModule)) {
            // First instruction of the ARM/MIPS function; the return address is
            // still in lr and fp still holds the caller's fp.
            callerPC_ = state.lr;
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_, sp - 2);
        } else if (offsetInModule == codeRange->profilingReturn() - PostStorePrePopFP) {
            // Second-to-last instruction of the ARM/MIPS function; fp points to
            // the caller's fp; have not yet popped Frame.
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = CallerFPFromFP(sp);
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_, sp);
        } else
#endif
        if (offsetInCodeRange < PushedFP || offsetInModule == codeRange->profilingReturn() ||
            InThunk(*codeRange, offsetInModule))
        {
            // The return address has been pushed on the stack but not fp; fp
            // still points to the caller's fp.
            callerPC_ = *sp;
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_, sp - 1);
        } else if (offsetInCodeRange < StoredFP) {
            // The full Frame has been pushed; fp still points to the caller's
            // frame.
            MOZ_ASSERT(fp == CallerFPFromFP(sp));
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = CallerFPFromFP(sp);
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_, sp);
        } else {
            // Not in the prologue/epilogue.
            callerPC_ = ReturnAddressFromFP(fp);
            callerFP_ = CallerFPFromFP(fp);
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_, fp);
        }
        break;
      }
      case CodeRange::Entry: {
        // The entry trampoline is the final frame in an WasmActivation. The entry
        // trampoline also doesn't GeneratePrologue/Epilogue so we can't use
        // the general unwinding logic above.
        MOZ_ASSERT(!fp);
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      }
      case CodeRange::Inline: {
        // The throw stub clears WasmActivation::fp on it's way out.
        if (!fp) {
            MOZ_ASSERT(done());
            return;
        }

        // Most inline code stubs execute after the prologue/epilogue have
        // completed so we can simply unwind based on fp. The only exception is
        // the async interrupt stub, since it can be executed at any time.
        // However, the async interrupt is super rare, so we can tolerate
        // skipped frames. Thus, we use simply unwind based on fp.
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_, fp);
        break;
      }
    }

    codeRange_ = codeRange;
    stackAddress_ = state.sp;
    MOZ_ASSERT(!done());
}

void
ProfilingFrameIterator::operator++()
{
    if (exitReason_ != ExitReason::None) {
        MOZ_ASSERT(codeRange_);
        exitReason_ = ExitReason::None;
        MOZ_ASSERT(!done());
        return;
    }

    if (!callerPC_) {
        MOZ_ASSERT(!callerFP_);
        codeRange_ = nullptr;
        MOZ_ASSERT(done());
        return;
    }

    code_ = activation_->compartment()->wasm.lookupCode(callerPC_);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(callerPC_);
    MOZ_ASSERT(codeRange_);

    switch (codeRange_->kind()) {
      case CodeRange::Entry:
        MOZ_ASSERT(callerFP_ == nullptr);
        callerPC_ = nullptr;
        break;
      case CodeRange::Function:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
      case CodeRange::Inline:
      case CodeRange::FarJumpIsland:
        stackAddress_ = callerFP_;
        callerPC_ = ReturnAddressFromFP(callerFP_);
        AssertMatchesCallSite(*activation_, callerPC_, CallerFPFromFP(callerFP_), callerFP_);
        callerFP_ = CallerFPFromFP(callerFP_);
        break;
    }

    MOZ_ASSERT(!done());
}

const char*
ProfilingFrameIterator::label() const
{
    MOZ_ASSERT(!done());

    // Use the same string for both time inside and under so that the two
    // entries will be coalesced by the profiler.
    //
    // NB: these labels are parsed for location by
    //     devtools/client/performance/modules/logic/frame-utils.js
    const char* importJitDescription = "fast FFI trampoline (in asm.js)";
    const char* importInterpDescription = "slow FFI trampoline (in asm.js)";
    const char* nativeDescription = "native call (in asm.js)";
    const char* trapDescription = "trap handling (in asm.js)";

    switch (exitReason_) {
      case ExitReason::None:
        break;
      case ExitReason::ImportJit:
        return importJitDescription;
      case ExitReason::ImportInterp:
        return importInterpDescription;
      case ExitReason::Native:
        return nativeDescription;
      case ExitReason::Trap:
        return trapDescription;
    }

    switch (codeRange_->kind()) {
      case CodeRange::Function:         return code_->profilingLabel(codeRange_->funcDefIndex());
      case CodeRange::Entry:            return "entry trampoline (in asm.js)";
      case CodeRange::ImportJitExit:    return importJitDescription;
      case CodeRange::ImportInterpExit: return importInterpDescription;
      case CodeRange::TrapExit:         return trapDescription;
      case CodeRange::Inline:           return "inline stub (in asm.js)";
      case CodeRange::FarJumpIsland:    return "interstitial (in asm.js)";
    }

    MOZ_CRASH("bad code range kind");
}

/*****************************************************************************/
// Runtime patching to enable/disable profiling

void
wasm::ToggleProfiling(const Code& code, const CallSite& callSite, bool enabled)
{
    if (callSite.kind() != CallSite::FuncDef)
        return;

    uint8_t* callerRetAddr = code.segment().base() + callSite.returnAddressOffset();

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    void* callee = X86Encoding::GetRel32Target(callerRetAddr);
#elif defined(JS_CODEGEN_ARM)
    uint8_t* caller = callerRetAddr - 4;
    Instruction* callerInsn = reinterpret_cast<Instruction*>(caller);
    BOffImm calleeOffset;
    callerInsn->as<InstBLImm>()->extractImm(&calleeOffset);
    void* callee = calleeOffset.getDest(callerInsn);
#elif defined(JS_CODEGEN_ARM64)
    MOZ_CRASH();
    void* callee = nullptr;
    (void)callerRetAddr;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    uint8_t* caller = callerRetAddr - 2 * sizeof(uint32_t);
    InstImm* callerInsn = reinterpret_cast<InstImm*>(caller);
    BOffImm16 calleeOffset;
    callerInsn->extractImm16(&calleeOffset);
    void* callee = calleeOffset.getDest(reinterpret_cast<Instruction*>(caller));
#elif defined(JS_CODEGEN_NONE)
    MOZ_CRASH();
    void* callee = nullptr;
#else
# error "Missing architecture"
#endif

    const CodeRange* codeRange = code.lookupRange(callee);
    if (!codeRange->isFunction())
        return;

    uint8_t* from = code.segment().base() + codeRange->funcNonProfilingEntry();
    uint8_t* to = code.segment().base() + codeRange->funcProfilingEntry();
    if (!enabled)
        Swap(from, to);

    MOZ_ASSERT(callee == from);

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    X86Encoding::SetRel32(callerRetAddr, to);
#elif defined(JS_CODEGEN_ARM)
    new (caller) InstBLImm(BOffImm(to - caller), Assembler::Always);
#elif defined(JS_CODEGEN_ARM64)
    (void)to;
    MOZ_CRASH();
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    new (caller) InstImm(op_regimm, zero, rt_bgezal, BOffImm16(to - caller));
#elif defined(JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Missing architecture"
#endif
}

void
wasm::ToggleProfiling(const Code& code, const CallThunk& callThunk, bool enabled)
{
    const CodeRange& cr = code.metadata().codeRanges[callThunk.u.codeRangeIndex];
    uint32_t calleeOffset = enabled ? cr.funcProfilingEntry() : cr.funcNonProfilingEntry();
    MacroAssembler::repatchFarJump(code.segment().base(), callThunk.offset, calleeOffset);
}

void
wasm::ToggleProfiling(const Code& code, const CodeRange& codeRange, bool enabled)
{
    if (!codeRange.isFunction())
        return;

    uint8_t* codeBase = code.segment().base();
    uint8_t* profilingEntry     = codeBase + codeRange.funcProfilingEntry();
    uint8_t* tableProfilingJump = codeBase + codeRange.funcTableProfilingJump();
    uint8_t* profilingJump      = codeBase + codeRange.funcProfilingJump();
    uint8_t* profilingEpilogue  = codeBase + codeRange.funcProfilingEpilogue();

    if (enabled) {
        MacroAssembler::patchNopToNearJump(tableProfilingJump, profilingEntry);
        MacroAssembler::patchNopToNearJump(profilingJump, profilingEpilogue);
    } else {
        MacroAssembler::patchNearJumpToNop(tableProfilingJump);
        MacroAssembler::patchNearJumpToNop(profilingJump);
    }
}
