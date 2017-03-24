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

#include "wasm/WasmFrameIterator.h"

#include "wasm/WasmInstance.h"

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

static DebugFrame*
FrameToDebugFrame(void* fp)
{
    return reinterpret_cast<DebugFrame*>((uint8_t*)fp - DebugFrame::offsetOfFrame());
}

FrameIterator::FrameIterator()
  : activation_(nullptr),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(nullptr),
    unwind_(Unwind::False),
    missingFrameMessage_(false)
{
    MOZ_ASSERT(done());
}

FrameIterator::FrameIterator(WasmActivation* activation, Unwind unwind)
  : activation_(activation),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(nullptr),
    unwind_(unwind),
    missingFrameMessage_(false)
{
    // When execution is interrupted, the embedding may capture a stack trace.
    // Since we've lost all the register state, we can't unwind the full stack
    // like ProfilingFrameIterator does. However, we can recover the interrupted
    // function via the resumePC and at least print that frame.
    if (void* resumePC = activation->resumePC()) {
        code_ = activation->compartment()->wasm.lookupCode(resumePC);
        codeRange_ = code_->lookupRange(resumePC);
        MOZ_ASSERT(codeRange_->kind() == CodeRange::Function);
        MOZ_ASSERT(!done());
        return;
    }

    fp_ = activation->exitFP();

    if (!fp_) {
        MOZ_ASSERT(done());
        return;
    }

    settle();
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
        settle();
    } else if (codeRange_) {
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
    if (unwind_ == Unwind::True)
        activation_->unwindExitFP(fp_);

    void* returnAddress = ReturnAddressFromFP(fp_);

    fp_ = CallerFPFromFP(fp_);

    if (!fp_) {
        code_ = nullptr;
        codeRange_ = nullptr;
        callsite_ = nullptr;

        if (unwind_ == Unwind::True)
            activation_->unwindExitFP(nullptr);

        MOZ_ASSERT(done());
        return;
    }

    code_ = activation_->compartment()->wasm.lookupCode(returnAddress);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(returnAddress);
    MOZ_ASSERT(codeRange_->kind() == CodeRange::Function);

    callsite_ = code_->lookupCallSite(returnAddress);
    MOZ_ASSERT(callsite_);

    MOZ_ASSERT(!done());
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
        const char* msg = "asm.js/wasm frames may be missing below this one";
        JSAtom* atom = Atomize(cx, msg, strlen(msg));
        if (!atom) {
            cx->clearPendingException();
            return cx->names().empty;
        }

        return atom;
    }

    MOZ_ASSERT(codeRange_);

    JSAtom* atom = code_->getFuncAtom(cx, codeRange_->funcIndex());
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

bool
FrameIterator::hasInstance() const
{
    MOZ_ASSERT(!done());
    return !!fp_;
}

Instance*
FrameIterator::instance() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(hasInstance());
    return FrameToDebugFrame(fp_)->instance();
}

bool
FrameIterator::debugEnabled() const
{
    MOZ_ASSERT(!done() && code_);
    MOZ_ASSERT_IF(!missingFrameMessage_, codeRange_->kind() == CodeRange::Function);
    MOZ_ASSERT_IF(missingFrameMessage_, !codeRange_ && !fp_);
    // Only non-imported functions can have debug frames.
    return code_->metadata().debugEnabled &&
           fp_ &&
           !missingFrameMessage_ &&
           codeRange_->funcIndex() >= code_->metadata().funcImports.length();
}

DebugFrame*
FrameIterator::debugFrame() const
{
    MOZ_ASSERT(!done() && debugEnabled());
    MOZ_ASSERT(fp_);
    return FrameToDebugFrame(fp_);
}

const CallSite*
FrameIterator::debugTrapCallsite() const
{
    MOZ_ASSERT(!done() && debugEnabled());
    MOZ_ASSERT(callsite_->kind() == CallSite::EnterFrame || callsite_->kind() == CallSite::LeaveFrame ||
               callsite_->kind() == CallSite::Breakpoint);
    return callsite_;
}

/*****************************************************************************/
// Prologue/epilogue code generation

// These constants reflect statically-determined offsets in the
// prologue/epilogue. The offsets are dynamically asserted during code
// generation.
#if defined(JS_CODEGEN_X64)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 1;
static const unsigned PushedTLS = 3;
static const unsigned PoppedTLS = 1;
#elif defined(JS_CODEGEN_X86)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 1;
static const unsigned PushedTLS = 2;
static const unsigned PoppedTLS = 1;
#elif defined(JS_CODEGEN_ARM)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 4;
static const unsigned PushedFP = 8;
static const unsigned PushedTLS = 12;
static const unsigned PoppedTLS = 4;
#elif defined(JS_CODEGEN_ARM64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 0;
static const unsigned PushedTLS = 0;
static const unsigned PoppedTLS = 0;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 4;
static const unsigned PushedFP = 8;
static const unsigned PushedTLS = 12;
static const unsigned PoppedTLS = 4;
#elif defined(JS_CODEGEN_NONE)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 0;
static const unsigned PushedTLS = 0;
static const unsigned PoppedTLS = 0;
#else
# error "Unknown architecture!"
#endif

static void
PushRetAddr(MacroAssembler& masm, unsigned entry)
{
#if defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(masm.currentOffset() - entry == BeforePushRetAddr);
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(masm.currentOffset() - entry == BeforePushRetAddr);
    masm.push(ra);
#else
    // The x86/x64 call instruction pushes the return address.
#endif
}

static void
GenerateCallablePrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                         uint32_t* entry)
{
    // ProfilingFrameIterator needs to know the offsets of several key
    // instructions from entry. To save space, we make these offsets static
    // constants and assert that they match the actual codegen below. On ARM,
    // this requires AutoForbidPools to prevent a constant pool from being
    // randomly inserted between two instructions.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 8);
#endif

        *entry = masm.currentOffset();

        PushRetAddr(masm, *entry);
        MOZ_ASSERT_IF(!masm.oom(), PushedRetAddr == masm.currentOffset() - *entry);
        masm.push(FramePointer);
        MOZ_ASSERT_IF(!masm.oom(), PushedFP == masm.currentOffset() - *entry);
        masm.push(WasmTlsReg);
        MOZ_ASSERT_IF(!masm.oom(), PushedTLS == masm.currentOffset() - *entry);
        masm.moveStackPtrTo(FramePointer);
    }

    if (reason != ExitReason::None) {
        Register scratch = ABINonArgReg0;
        masm.loadWasmActivationFromTls(scratch);
        masm.wasmAssertNonExitInvariants(scratch);
        Address exitReason(scratch, WasmActivation::offsetOfExitReason());
        masm.store32(Imm32(int32_t(reason)), exitReason);
        Address exitFP(scratch, WasmActivation::offsetOfExitFP());
        masm.storePtr(FramePointer, exitFP);
    }

    if (framePushed)
        masm.subFromStackPtr(Imm32(framePushed));
}

static void
GenerateCallableEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                         uint32_t* ret)
{
    if (framePushed)
        masm.addToStackPtr(Imm32(framePushed));


    if (reason != ExitReason::None) {
        Register scratch = ABINonArgReturnReg0;
        masm.loadWasmActivationFromTls(scratch);
        Address exitFP(scratch, WasmActivation::offsetOfExitFP());
        masm.storePtr(ImmWord(0), exitFP);
        Address exitReason(scratch, WasmActivation::offsetOfExitReason());
        masm.store32(Imm32(int32_t(ExitReason::None)), exitReason);
    }

    // Forbid pools for the same reason as described in GenerateCallablePrologue.
#if defined(JS_CODEGEN_ARM)
    AutoForbidPools afp(&masm, /* number of instructions in scope = */ 3);
#endif

    masm.pop(WasmTlsReg);
    DebugOnly<uint32_t> poppedTLS = masm.currentOffset();
    masm.pop(FramePointer);
    *ret = masm.currentOffset();
    masm.ret();
    MOZ_ASSERT_IF(!masm.oom(), PoppedTLS == *ret - poppedTLS);
}

void
wasm::GenerateFunctionPrologue(MacroAssembler& masm, unsigned framePushed, const SigIdDesc& sigId,
                               FuncOffsets* offsets)
{
#if defined(JS_CODEGEN_ARM)
    // Flush pending pools so they do not get dumped between the 'begin' and
    // 'normalEntry' offsets since the difference must be less than UINT8_MAX
    // to be stored in CodeRange::funcBeginToNormalEntry_.
    masm.flushBuffer();
#endif
    masm.haltingAlign(CodeAlignment);

    // Generate table entry:
    offsets->begin = masm.currentOffset();
    TrapOffset trapOffset(0);  // ignored by masm.wasmEmitTrapOutOfLineCode
    TrapDesc trap(trapOffset, Trap::IndirectCallBadSig, masm.framePushed());
    switch (sigId.kind()) {
      case SigIdDesc::Kind::Global: {
        Register scratch = WasmTableCallScratchReg;
        masm.loadWasmGlobalPtr(sigId.globalDataOffset(), scratch);
        masm.branchPtr(Assembler::Condition::NotEqual, WasmTableCallSigReg, scratch, trap);
        break;
      }
      case SigIdDesc::Kind::Immediate:
        masm.branch32(Assembler::Condition::NotEqual, WasmTableCallSigReg, Imm32(sigId.immediate()), trap);
        break;
      case SigIdDesc::Kind::None:
        break;
    }

    // Generate normal entry:
    masm.nopAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, ExitReason::None, &offsets->normalEntry);

    masm.setFramePushed(framePushed);
}

void
wasm::GenerateFunctionEpilogue(MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets)
{
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateCallableEpilogue(masm, framePushed, ExitReason::None, &offsets->ret);
    masm.setFramePushed(0);
}

void
wasm::GenerateExitPrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           CallableOffsets* offsets)
{
    masm.haltingAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, reason, &offsets->begin);
    masm.setFramePushed(framePushed);
}

void
wasm::GenerateExitEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           CallableOffsets* offsets)
{
    // Inverse of GenerateExitPrologue:
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateCallableEpilogue(masm, framePushed, reason, &offsets->ret);
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
    initFromExitFP();
}

static inline void
AssertMatchesCallSite(const WasmActivation& activation, void* callerPC, void* callerFP)
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
#endif
}

void
ProfilingFrameIterator::initFromExitFP()
{
    uint8_t* fp = activation_->exitFP();
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
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
      case CodeRange::Throw:
      case CodeRange::FarJumpIsland:
        MOZ_CRASH("Unexpected CodeRange kind");
    }

    // The iterator inserts a pretend innermost frame for non-None ExitReasons.
    // This allows the variety of exit reasons to show up in the callstack.
    exitReason_ = activation_->exitReason();

    MOZ_ASSERT(!done());
}

typedef JS::ProfilingFrameIterator::RegisterState RegisterState;

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
    // In the case of ImportJitExit, the fp register may be temporarily
    // clobbered on return from Ion so always use activation.fp when it is set.
    if (activation.exitFP()) {
        initFromExitFP();
        return;
    }

    // If pc isn't in the instance's code, we must have exited the code via an
    // exit trampoline or signal handler.
    code_ = activation_->compartment()->wasm.lookupCode(state.pc);
    if (!code_) {
        MOZ_ASSERT(done());
        return;
    }

    // When the pc is inside the prologue/epilogue, the innermost call's Frame
    // is not complete and thus fp points to the second-to-innermost call's
    // Frame. Since fp can only tell you about its caller, naively unwinding
    // while pc is in the prologue/epilogue would skip the second-to-innermost
    // call. To avoid this problem, we use the static structure of the code in
    // the prologue and epilogue to do the Right Thing.
    uint8_t* fp = (uint8_t*)state.fp;
    uint8_t* pc = (uint8_t*)state.pc;
    void** sp = (void**)state.sp;

    const CodeRange* codeRange = code_->lookupRange(pc);
    uint32_t offsetInModule = pc - code_->segment().base();
    MOZ_ASSERT(offsetInModule >= codeRange->begin());
    MOZ_ASSERT(offsetInModule < codeRange->end());

    // Compute the offset of the pc from the (normal) entry of the code range.
    // The stack state of the pc for the entire table-entry is equivalent to
    // that of the first pc of the normal-entry. Thus, we can simplify the below
    // case analysis by redirecting all pc-in-table-entry cases to the
    // pc-at-normal-entry case.
    uint32_t offsetFromEntry;
    if (codeRange->isFunction()) {
        if (offsetInModule < codeRange->funcNormalEntry())
            offsetFromEntry = 0;
        else
            offsetFromEntry = offsetInModule - codeRange->funcNormalEntry();
    } else {
        offsetFromEntry = offsetInModule - codeRange->begin();
    }

    switch (codeRange->kind()) {
      case CodeRange::Function:
      case CodeRange::FarJumpIsland:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        if (offsetFromEntry == BeforePushRetAddr || codeRange->isThunk()) {
            // The return address is still in lr and fp holds the caller's fp.
            callerPC_ = state.lr;
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else
#endif
        if (offsetFromEntry == PushedRetAddr || codeRange->isThunk()) {
            // The return address has been pushed on the stack but fp still
            // points to the caller's fp.
            callerPC_ = sp[0];
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else if (offsetFromEntry == PushedFP) {
            // The return address and caller's fp have been pushed on the stack; fp
            // is still the caller's fp.
            callerPC_ = sp[1];
            callerFP_ = sp[0];
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else if (offsetFromEntry == PushedTLS) {
            // The full Frame has been pushed; fp is still the caller's fp.
            MOZ_ASSERT(fp == CallerFPFromFP(sp));
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else if (offsetInModule == codeRange->ret() - PoppedTLS) {
            // The TLS field of the Frame has been popped.
            callerPC_ = sp[1];
            callerFP_ = sp[0];
        } else if (offsetInModule == codeRange->ret()) {
            // Both the TLS and callerFP fields have been popped and fp now
            // points to the caller's frame.
            callerPC_ = sp[0];
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else {
            // Not in the prologue/epilogue.
            callerPC_ = ReturnAddressFromFP(fp);
            callerFP_ = CallerFPFromFP(fp);
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        }
        break;
      case CodeRange::Entry:
        // The entry trampoline is the final frame in an WasmActivation. The entry
        // trampoline also doesn't GeneratePrologue/Epilogue so we can't use
        // the general unwinding logic above.
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
        // Most inline code stubs execute after the prologue/epilogue have
        // completed so we can simply unwind based on fp. The only exception is
        // the async interrupt stub, since it can be executed at any time.
        // However, the async interrupt is super rare, so we can tolerate
        // skipped frames. Thus, we use simply unwind based on fp.
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        break;
      case CodeRange::Throw:
        // The throw stub executes a small number of instructions before popping
        // the entire activation. To simplify testing, we simply pretend throw
        // stubs have already popped the entire stack.
        MOZ_ASSERT(done());
        return;
    }

    codeRange_ = codeRange;
    stackAddress_ = sp;
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
      case CodeRange::Throw:
        MOZ_ASSERT(callerFP_ == nullptr);
        callerPC_ = nullptr;
        break;
      case CodeRange::Function:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
      case CodeRange::FarJumpIsland:
        stackAddress_ = callerFP_;
        callerPC_ = ReturnAddressFromFP(callerFP_);
        AssertMatchesCallSite(*activation_, callerPC_, CallerFPFromFP(callerFP_));
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
    const char* trapDescription = "trap handling (in asm.js)";
    const char* debugTrapDescription = "debug trap handling (in asm.js)";

    switch (exitReason_) {
      case ExitReason::None:
        break;
      case ExitReason::ImportJit:
        return importJitDescription;
      case ExitReason::ImportInterp:
        return importInterpDescription;
      case ExitReason::Trap:
        return trapDescription;
      case ExitReason::DebugTrap:
        return debugTrapDescription;
    }

    switch (codeRange_->kind()) {
      case CodeRange::Function:         return code_->profilingLabel(codeRange_->funcIndex());
      case CodeRange::Entry:            return "entry trampoline (in asm.js)";
      case CodeRange::ImportJitExit:    return importJitDescription;
      case CodeRange::ImportInterpExit: return importInterpDescription;
      case CodeRange::TrapExit:         return trapDescription;
      case CodeRange::DebugTrap:        return debugTrapDescription;
      case CodeRange::Inline:           return "inline stub (in asm.js)";
      case CodeRange::FarJumpIsland:    return "interstitial (in asm.js)";
      case CodeRange::Throw:            MOZ_CRASH("no frame for throw stubs");
    }

    MOZ_CRASH("bad code range kind");
}

void
wasm::TraceActivations(JSContext* cx, const CooperatingContext& target, JSTracer* trc)
{
    for (ActivationIterator iter(cx, target); !iter.done(); ++iter) {
        if (iter.activation()->isWasm()) {
            for (FrameIterator fi(iter.activation()->asWasm()); !fi.done(); ++fi) {
                if (fi.hasInstance())
                    fi.instance()->trace(trc);
            }
        }
    }
}
