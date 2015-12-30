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

#include "jsatom.h"

#include "asmjs/WasmModule.h"

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
    return reinterpret_cast<AsmJSFrame*>(fp)->returnAddress;
}

static uint8_t*
CallerFPFromFP(void* fp)
{
    return reinterpret_cast<AsmJSFrame*>(fp)->callerFP;
}

FrameIterator::FrameIterator()
  : cx_(nullptr),
    module_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(nullptr)
{
    MOZ_ASSERT(done());
}

FrameIterator::FrameIterator(const WasmActivation& activation)
  : cx_(activation.cx()),
    module_(&activation.module()),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(activation.fp())
{
    if (fp_)
        settle();
}

void
FrameIterator::operator++()
{
    MOZ_ASSERT(!done());
    DebugOnly<uint8_t*> oldfp = fp_;
    fp_ += callsite_->stackDepth();
    MOZ_ASSERT_IF(module_->profilingEnabled(), fp_ == CallerFPFromFP(oldfp));
    settle();
}

void
FrameIterator::settle()
{
    void* returnAddress = ReturnAddressFromFP(fp_);

    const CodeRange* codeRange = module_->lookupCodeRange(returnAddress);
    MOZ_ASSERT(codeRange);
    codeRange_ = codeRange;

    switch (codeRange->kind()) {
        case CodeRange::Function:
        callsite_ = module_->lookupCallSite(returnAddress);
        MOZ_ASSERT(callsite_);
        break;
      case CodeRange::Entry:
        fp_ = nullptr;
        MOZ_ASSERT(done());
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::Interrupt:
      case CodeRange::Inline:
        MOZ_CRASH("Should not encounter an exit during iteration");
    }
}

JSAtom*
FrameIterator::functionDisplayAtom() const
{
    MOZ_ASSERT(!done());

    const char* chars = module_->functionName(codeRange_->funcNameIndex());
    UTF8Chars utf8(chars, strlen(chars));

    size_t twoByteLength;
    UniquePtr<char16_t> twoByte(JS::UTF8CharsToNewTwoByteCharsZ(cx_, utf8, &twoByteLength).get());
    if (!twoByte) {
        cx_->clearPendingException();
        return cx_->names().empty;
    }

    JSAtom* atom = AtomizeChars(cx_, twoByte.get(), twoByteLength);
    if (!atom) {
        cx_->clearPendingException();
        return cx_->names().empty;
    }

    return atom;
}

unsigned
FrameIterator::computeLine(uint32_t* column) const
{
    MOZ_ASSERT(!done());
    if (column)
        *column = callsite_->column();
    return callsite_->line();
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
static const unsigned PushedFP = 13;
static const unsigned StoredFP = 20;
#elif defined(JS_CODEGEN_X86)
# if defined(DEBUG)
static const unsigned PushedRetAddr = 0;
static const unsigned PostStorePrePopFP = 0;
# endif
static const unsigned PushedFP = 8;
static const unsigned StoredFP = 11;
#elif defined(JS_CODEGEN_ARM)
static const unsigned PushedRetAddr = 4;
static const unsigned PushedFP = 16;
static const unsigned StoredFP = 20;
static const unsigned PostStorePrePopFP = 4;
#elif defined(JS_CODEGEN_ARM64)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 0;
static const unsigned StoredFP = 0;
static const unsigned PostStorePrePopFP = 0;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned PushedRetAddr = 8;
static const unsigned PushedFP = 24;
static const unsigned StoredFP = 28;
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
                          ProfilingOffsets* offsets, Label* maybeEntry = nullptr)
{
#if !defined (JS_CODEGEN_ARM)
    Register scratch = ABIArgGenerator::NonArg_VolatileReg;
#else
    // Unfortunately, there are no unused non-arg volatile registers on ARM --
    // the MacroAssembler claims both lr and ip -- so we use the second scratch
    // register (lr) and be very careful not to call any methods that use it.
    Register scratch = lr;
    masm.setSecondScratchReg(InvalidReg);
#endif

    // ProfilingFrameIterator needs to know the offsets of several key
    // instructions from entry. To save space, we make these offsets static
    // constants and assert that they match the actual codegen below. On ARM,
    // this requires AutoForbidPools to prevent a constant pool from being
    // randomly inserted between two instructions.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 5);
#endif

        offsets->begin = masm.currentOffset();
        if (maybeEntry)
            masm.bind(maybeEntry);

        PushRetAddr(masm);
        MOZ_ASSERT_IF(!masm.oom(), PushedRetAddr == masm.currentOffset() - offsets->begin);

        masm.loadWasmActivation(scratch);
        masm.push(Address(scratch, WasmActivation::offsetOfFP()));
        MOZ_ASSERT_IF(!masm.oom(), PushedFP == masm.currentOffset() - offsets->begin);

        masm.storePtr(masm.getStackPointer(), Address(scratch, WasmActivation::offsetOfFP()));
        MOZ_ASSERT_IF(!masm.oom(), StoredFP == masm.currentOffset() - offsets->begin);
    }

    if (reason != ExitReason::None) {
        masm.store32_NoSecondScratch(Imm32(int32_t(reason)),
                                     Address(scratch, WasmActivation::offsetOfExitReason()));
    }

#if defined(JS_CODEGEN_ARM)
    masm.setSecondScratchReg(lr);
#endif

    if (framePushed)
        masm.subFromStackPtr(Imm32(framePushed));
}

// Generate the inverse of GenerateProfilingPrologue.
static void
GenerateProfilingEpilogue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                          ProfilingOffsets* offsets)
{
    Register scratch = ABIArgGenerator::NonReturn_VolatileReg0;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    Register scratch2 = ABIArgGenerator::NonReturn_VolatileReg1;
#endif

    if (framePushed)
        masm.addToStackPtr(Imm32(framePushed));

    masm.loadWasmActivation(scratch);

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
// Specifically, Module::setProfilingEnabled patches all callsites to
// either call the profiling or non-profiling entry point.
void
wasm::GenerateFunctionPrologue(MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets)
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

    // Generate normal prologue:
    masm.haltingAlign(CodeAlignment);
    offsets->nonProfilingEntry = masm.currentOffset();
    PushRetAddr(masm);
    masm.subFromStackPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress));

    // Prologue join point, body begin:
    masm.bind(&body);
    masm.setFramePushed(framePushed);
}

// Similar to GenerateFunctionPrologue (see comment), we generate both a
// profiling and non-profiling epilogue a priori. When the profiling mode is
// toggled, Module::setProfilingEnabled patches the 'profiling jump' to
// either be a nop (falling through to the normal prologue) or a jump (jumping
// to the profiling epilogue).
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
    {
#if defined(JS_CODEGEN_ARM)
        // Forbid pools from being inserted between the profilingJump label and
        // the nop since we need the location of the actual nop to patch it.
        AutoForbidPools afp(&masm, 1);
#endif

        // The exact form of this instruction must be kept consistent with the
        // patching in Module::setProfilingEnabled.
        offsets->profilingJump = masm.currentOffset();
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        masm.twoByteNop();
#elif defined(JS_CODEGEN_ARM)
        masm.nop();
#elif defined(JS_CODEGEN_MIPS32)
        masm.nop();
        masm.nop();
        masm.nop();
        masm.nop();
#elif defined(JS_CODEGEN_MIPS64)
        masm.nop();
        masm.nop();
        masm.nop();
        masm.nop();
        masm.nop();
        masm.nop();
#endif
    }

    // Normal epilogue:
    masm.addToStackPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress));
    masm.ret();
    masm.setFramePushed(0);

    // Profiling epilogue:
    offsets->profilingEpilogue = masm.currentOffset();
    GenerateProfilingEpilogue(masm, framePushed, ExitReason::None, offsets);
}

void
wasm::GenerateExitPrologue(MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                           ProfilingOffsets* offsets, Label* maybeEntry)
{
    masm.haltingAlign(CodeAlignment);
    GenerateProfilingPrologue(masm, framePushed, reason, offsets, maybeEntry);
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
  : module_(nullptr),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::None)
{
    MOZ_ASSERT(done());
}

ProfilingFrameIterator::ProfilingFrameIterator(const WasmActivation& activation)
  : module_(&activation.module()),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(ExitReason::None)
{
    // If profiling hasn't been enabled for this module, then CallerFPFromFP
    // will be trash, so ignore the entire activation. In practice, this only
    // happens if profiling is enabled while module->active() (in this case,
    // profiling will be enabled when the module becomes inactive and gets
    // called again).
    if (!module_->profilingEnabled()) {
        MOZ_ASSERT(done());
        return;
    }

    initFromFP(activation);
}

static inline void
AssertMatchesCallSite(const Module& module, void* callerPC, void* callerFP, void* fp)
{
#ifdef DEBUG
    const CodeRange* callerCodeRange = module.lookupCodeRange(callerPC);
    MOZ_ASSERT(callerCodeRange);
    if (callerCodeRange->kind() == CodeRange::Entry) {
        MOZ_ASSERT(callerFP == nullptr);
        return;
    }

    const CallSite* callsite = module.lookupCallSite(callerPC);
    MOZ_ASSERT(callsite);
    MOZ_ASSERT(callerFP == (uint8_t*)fp + callsite->stackDepth());
#endif
}

void
ProfilingFrameIterator::initFromFP(const WasmActivation& activation)
{
    uint8_t* fp = activation.fp();

    // If a signal was handled while entering an activation, the frame will
    // still be null.
    if (!fp) {
        MOZ_ASSERT(done());
        return;
    }

    // Since we don't have the pc for fp, start unwinding at the caller of fp
    // (ReturnAddressFromFP(fp)). This means that the innermost frame is
    // skipped. This is fine because:
    //  - for import exit calls, the innermost frame is a thunk, so the first
    //    frame that shows up is the function calling the import;
    //  - for Math and other builtin calls as well as interrupts, we note the absence
    //    of an exit reason and inject a fake "builtin" frame; and
    //  - for async interrupts, we just accept that we'll lose the innermost frame.
    void* pc = ReturnAddressFromFP(fp);
    const CodeRange* codeRange = module_->lookupCodeRange(pc);
    MOZ_ASSERT(codeRange);
    codeRange_ = codeRange;
    stackAddress_ = fp;

    switch (codeRange->kind()) {
      case CodeRange::Entry:
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case CodeRange::Function:
        fp = CallerFPFromFP(fp);
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*module_, callerPC_, callerFP_, fp);
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::Interrupt:
      case CodeRange::Inline:
        MOZ_CRASH("Unexpected CodeRange kind");
    }

    // The iterator inserts a pretend innermost frame for non-None ExitReasons.
    // This allows the variety of exit reasons to show up in the callstack.
    exitReason_ = activation.exitReason();

    // In the case of calls to builtins or asynchronous interrupts, no exit path
    // is taken so the exitReason is None. Coerce these to the Native exit
    // reason so that self-time is accounted for.
    if (exitReason_ == ExitReason::None)
        exitReason_ = ExitReason::Native;

    MOZ_ASSERT(!done());
}

typedef JS::ProfilingFrameIterator::RegisterState RegisterState;

ProfilingFrameIterator::ProfilingFrameIterator(const WasmActivation& activation,
                                               const RegisterState& state)
  : module_(&activation.module()),
    codeRange_(nullptr),
    callerFP_(nullptr),
    callerPC_(nullptr),
    exitReason_(ExitReason::None)
{
    // If profiling hasn't been enabled for this module, then CallerFPFromFP
    // will be trash, so ignore the entire activation. In practice, this only
    // happens if profiling is enabled while module->active() (in this case,
    // profiling will be enabled when the module becomes inactive and gets
    // called again).
    if (!module_->profilingEnabled()) {
        MOZ_ASSERT(done());
        return;
    }

    // If pc isn't in the module, we must have exited the asm.js module via an
    // exit trampoline or signal handler.
    if (!module_->containsCodePC(state.pc)) {
        initFromFP(activation);
        return;
    }

    // Note: fp may be null while entering and leaving the activation.
    uint8_t* fp = activation.fp();

    const CodeRange* codeRange = module_->lookupCodeRange(state.pc);
    switch (codeRange->kind()) {
      case CodeRange::Function:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::Interrupt: {
        // When the pc is inside the prologue/epilogue, the innermost
        // call's AsmJSFrame is not complete and thus fp points to the the
        // second-to-innermost call's AsmJSFrame. Since fp can only tell you
        // about its caller (via ReturnAddressFromFP(fp)), naively unwinding
        // while pc is in the prologue/epilogue would skip the second-to-
        // innermost call. To avoid this problem, we use the static structure of
        // the code in the prologue and epilogue to do the Right Thing.
        MOZ_ASSERT(module_->containsCodePC(state.pc));
        uint32_t offsetInModule = (uint8_t*)state.pc - module_->code();
        MOZ_ASSERT(offsetInModule >= codeRange->begin());
        MOZ_ASSERT(offsetInModule < codeRange->end());
        uint32_t offsetInCodeRange = offsetInModule - codeRange->begin();
        void** sp = (void**)state.sp;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
        if (offsetInCodeRange < PushedRetAddr) {
            // First instruction of the ARM/MIPS function; the return address is
            // still in lr and fp still holds the caller's fp.
            callerPC_ = state.lr;
            callerFP_ = fp;
            AssertMatchesCallSite(*module_, callerPC_, callerFP_, sp - 2);
        } else if (offsetInModule == codeRange->profilingReturn() - PostStorePrePopFP) {
            // Second-to-last instruction of the ARM/MIPS function; fp points to
            // the caller's fp; have not yet popped AsmJSFrame.
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = CallerFPFromFP(sp);
            AssertMatchesCallSite(*module_, callerPC_, callerFP_, sp);
        } else
#endif
        if (offsetInCodeRange < PushedFP || offsetInModule == codeRange->profilingReturn()) {
            // The return address has been pushed on the stack but not fp; fp
            // still points to the caller's fp.
            callerPC_ = *sp;
            callerFP_ = fp;
            AssertMatchesCallSite(*module_, callerPC_, callerFP_, sp - 1);
        } else if (offsetInCodeRange < StoredFP) {
            // The full AsmJSFrame has been pushed; fp still points to the
            // caller's frame.
            MOZ_ASSERT(fp == CallerFPFromFP(sp));
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = CallerFPFromFP(sp);
            AssertMatchesCallSite(*module_, callerPC_, callerFP_, sp);
        } else {
            // Not in the prologue/epilogue.
            callerPC_ = ReturnAddressFromFP(fp);
            callerFP_ = CallerFPFromFP(fp);
            AssertMatchesCallSite(*module_, callerPC_, callerFP_, fp);
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
        AssertMatchesCallSite(*module_, callerPC_, callerFP_, fp);
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

    const CodeRange* codeRange = module_->lookupCodeRange(callerPC_);
    MOZ_ASSERT(codeRange);
    codeRange_ = codeRange;

    switch (codeRange->kind()) {
      case CodeRange::Entry:
        MOZ_ASSERT(callerFP_ == nullptr);
        callerPC_ = nullptr;
        break;
      case CodeRange::Function:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::Interrupt:
      case CodeRange::Inline:
        stackAddress_ = callerFP_;
        callerPC_ = ReturnAddressFromFP(callerFP_);
        AssertMatchesCallSite(*module_, callerPC_, CallerFPFromFP(callerFP_), callerFP_);
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

    switch (exitReason_) {
      case ExitReason::None:
        break;
      case ExitReason::ImportJit:
        return importJitDescription;
      case ExitReason::ImportInterp:
        return importInterpDescription;
      case ExitReason::Native:
        return nativeDescription;
    }

    switch (codeRange_->kind()) {
      case CodeRange::Function:         return module_->profilingLabel(codeRange_->funcNameIndex());
      case CodeRange::Entry:            return "entry trampoline (in asm.js)";
      case CodeRange::ImportJitExit:    return importJitDescription;
      case CodeRange::ImportInterpExit: return importInterpDescription;
      case CodeRange::Interrupt:        return nativeDescription;
      case CodeRange::Inline:           return "inline stub (in asm.js)";
    }

    MOZ_CRASH("bad code range kind");
}

/*****************************************************************************/
// Runtime patching to enable/disable profiling

// Patch all internal (asm.js->asm.js) callsites to call the profiling
// prologues:
void
wasm::EnableProfilingPrologue(const Module& module, const CallSite& callSite, bool enabled)
{
    if (callSite.kind() != CallSite::Relative)
        return;

    uint8_t* callerRetAddr = module.code() + callSite.returnAddressOffset();

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
#elif defined(JS_CODEGEN_MIPS32)
    Instruction* instr = (Instruction*)(callerRetAddr - 4 * sizeof(uint32_t));
    void* callee = (void*)Assembler::ExtractLuiOriValue(instr, instr->next());
#elif defined(JS_CODEGEN_MIPS64)
    Instruction* instr = (Instruction*)(callerRetAddr - 6 * sizeof(uint32_t));
    void* callee = (void*)Assembler::ExtractLoad64Value(instr);
#elif defined(JS_CODEGEN_NONE)
    MOZ_CRASH();
    void* callee = nullptr;
#else
# error "Missing architecture"
#endif

    const CodeRange* codeRange = module.lookupCodeRange(callee);
    if (!codeRange->isFunction())
        return;

    uint8_t* from = module.code() + codeRange->funcNonProfilingEntry();
    uint8_t* to = module.code() + codeRange->funcProfilingEntry();
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
#elif defined(JS_CODEGEN_MIPS32)
    Assembler::WriteLuiOriInstructions(instr, instr->next(),
                                       ScratchRegister, (uint32_t)to);
    instr[2] = InstReg(op_special, ScratchRegister, zero, ra, ff_jalr);
#elif defined(JS_CODEGEN_MIPS64)
    Assembler::WriteLoad64Instructions(instr, ScratchRegister, (uint64_t)to);
    instr[4] = InstReg(op_special, ScratchRegister, zero, ra, ff_jalr);
#elif defined(JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Missing architecture"
#endif
}

// Replace all the nops in all the epilogues of asm.js functions with jumps
// to the profiling epilogues.
void
wasm::EnableProfilingEpilogue(const Module& module, const CodeRange& codeRange, bool enabled)
{
    if (!codeRange.isFunction())
        return;

    uint8_t* jump = module.code() + codeRange.functionProfilingJump();
    uint8_t* profilingEpilogue = module.code() + codeRange.funcProfilingEpilogue();

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // An unconditional jump with a 1 byte offset immediate has the opcode
    // 0x90. The offset is relative to the address of the instruction after
    // the jump. 0x66 0x90 is the canonical two-byte nop.
    ptrdiff_t jumpImmediate = profilingEpilogue - jump - 2;
    MOZ_ASSERT(jumpImmediate > 0 && jumpImmediate <= 127);
    if (enabled) {
        MOZ_ASSERT(jump[0] == 0x66);
        MOZ_ASSERT(jump[1] == 0x90);
        jump[0] = 0xeb;
        jump[1] = jumpImmediate;
    } else {
        MOZ_ASSERT(jump[0] == 0xeb);
        MOZ_ASSERT(jump[1] == jumpImmediate);
        jump[0] = 0x66;
        jump[1] = 0x90;
    }
#elif defined(JS_CODEGEN_ARM)
    if (enabled) {
        MOZ_ASSERT(reinterpret_cast<Instruction*>(jump)->is<InstNOP>());
        new (jump) InstBImm(BOffImm(profilingEpilogue - jump), Assembler::Always);
    } else {
        MOZ_ASSERT(reinterpret_cast<Instruction*>(jump)->is<InstBImm>());
        new (jump) InstNOP();
    }
#elif defined(JS_CODEGEN_ARM64)
    (void)jump;
    (void)profilingEpilogue;
    MOZ_CRASH();
#elif defined(JS_CODEGEN_MIPS32)
    Instruction* instr = (Instruction*)jump;
    if (enabled) {
        Assembler::WriteLuiOriInstructions(instr, instr->next(),
                                           ScratchRegister, (uint32_t)profilingEpilogue);
        instr[2] = InstReg(op_special, ScratchRegister, zero, zero, ff_jr);
    } else {
        for (unsigned i = 0; i < 3; i++)
            instr[i].makeNop();
    }
#elif defined(JS_CODEGEN_MIPS64)
    Instruction* instr = (Instruction*)jump;
    if (enabled) {
        Assembler::WriteLoad64Instructions(instr, ScratchRegister, (uint64_t)profilingEpilogue);
        instr[4] = InstReg(op_special, ScratchRegister, zero, zero, ff_jr);
    } else {
        for (unsigned i = 0; i < 5; i++)
            instr[i].makeNop();
    }
#elif defined(JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Missing architecture"
#endif
}
