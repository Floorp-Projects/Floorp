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

FrameIterator::FrameIterator()
  : activation_(nullptr),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(nullptr),
    unwind_(Unwind::False),
    unwoundAddressOfReturnAddress_(nullptr)
{
    MOZ_ASSERT(done());
}

FrameIterator::FrameIterator(WasmActivation* activation, Unwind unwind)
  : activation_(activation),
    code_(nullptr),
    callsite_(nullptr),
    codeRange_(nullptr),
    fp_(activation->exitFP()),
    unwind_(unwind)
{
    MOZ_ASSERT(fp_);

    // Normally, execution exits wasm code via an exit stub which sets exitFP to
    // the exit stub's frame. Thus, in this case, we want to start iteration at
    // the caller of the exit frame, whose Code, CodeRange and CallSite are
    // indicated by the returnAddress of the exit stub's frame.

    if (!activation->interrupted()) {
        popFrame();
        MOZ_ASSERT(!done());
        return;
    }

    // When asynchronously interrupted, exitFP is set to the interrupted frame
    // itself and so we do not want to skip it. Instead, we can recover the
    // Code and CodeRange from the WasmActivation, which are set when control
    // flow was interrupted. There is no CallSite (b/c the interrupt was async),
    // but this is fine because CallSite is only used for line number for which
    // we can use the beginning of the function from the CodeRange instead.

    code_ = activation_->compartment()->wasm.lookupCode(activation->resumePC());
    MOZ_ASSERT(code_);
    MOZ_ASSERT(&fp_->tls->instance->code() == code_);

    codeRange_ = code_->lookupRange(activation->resumePC());
    MOZ_ASSERT(codeRange_->kind() == CodeRange::Function);

    MOZ_ASSERT(!done());
}

bool
FrameIterator::done() const
{
    MOZ_ASSERT(!!fp_ == !!code_);
    MOZ_ASSERT(!!fp_ == !!codeRange_);
    return !fp_;
}

void
FrameIterator::operator++()
{
    MOZ_ASSERT(!done());

    // When the iterator is set to Unwind::True, each time the iterator pops a
    // frame, the WasmActivation is updated so that the just-popped frame
    // is no longer visible. This is necessary since Debugger::onLeaveFrame is
    // called before popping each frame and, once onLeaveFrame is called for a
    // given frame, that frame must not be visible to subsequent stack iteration
    // (or it could be added as a "new" frame just as it becomes garbage).
    // When the frame is "interrupted", then exitFP is included in the callstack
    // (otherwise, it is skipped, as explained above). So to unwind the
    // innermost frame, we just clear the interrupt state.

    if (unwind_ == Unwind::True) {
        if (activation_->interrupted())
            activation_->finishInterrupt();
        activation_->unwindExitFP(fp_);
    }

    popFrame();
}

void
FrameIterator::popFrame()
{
    Frame* prevFP = fp_;
    fp_ = prevFP->callerFP;

    if (!fp_) {
        code_ = nullptr;
        codeRange_ = nullptr;
        callsite_ = nullptr;

        if (unwind_ == Unwind::True) {
            activation_->unwindExitFP(nullptr);
            unwoundAddressOfReturnAddress_ = &prevFP->returnAddress;
        }

        MOZ_ASSERT(done());
        return;
    }

    void* returnAddress = prevFP->returnAddress;

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
    JSAtom* atom = instance()->getFuncAtom(cx, codeRange_->funcIndex());
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
    MOZ_ASSERT_IF(!callsite_, activation_->interrupted());
    return callsite_ ? callsite_->lineOrBytecode() : codeRange_->funcLineOrBytecode();
}

Instance*
FrameIterator::instance() const
{
    MOZ_ASSERT(!done());
    return fp_->tls->instance;
}

void**
FrameIterator::unwoundAddressOfReturnAddress() const
{
    MOZ_ASSERT(done());
    MOZ_ASSERT(unwind_ == Unwind::True);
    MOZ_ASSERT(unwoundAddressOfReturnAddress_);
    return unwoundAddressOfReturnAddress_;
}

bool
FrameIterator::debugEnabled() const
{
    MOZ_ASSERT(!done());

    // Only non-imported functions can have debug frames.
    //
    // Metadata::debugEnabled is only set if debugging is actually enabled (both
    // requested, and available via baseline compilation), and Tier::Debug code
    // will be available.
    return code_->metadata().debugEnabled &&
           codeRange_->funcIndex() >= code_->metadata(Tier::Debug).funcImports.length();
}

DebugFrame*
FrameIterator::debugFrame() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(debugEnabled());
    return reinterpret_cast<DebugFrame*>((uint8_t*)fp_ - DebugFrame::offsetOfFrame());
}

const CallSite*
FrameIterator::debugTrapCallsite() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(callsite_);
    MOZ_ASSERT(debugEnabled());
    MOZ_ASSERT(callsite_->kind() == CallSite::EnterFrame ||
               callsite_->kind() == CallSite::LeaveFrame ||
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
static const unsigned PushedTLS = 2;
static const unsigned PushedFP = 3;
static const unsigned SetFP = 6;
static const unsigned PoppedFP = 2;
#elif defined(JS_CODEGEN_X86)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 1;
static const unsigned PushedFP = 2;
static const unsigned SetFP = 4;
static const unsigned PoppedFP = 1;
#elif defined(JS_CODEGEN_ARM)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 4;
static const unsigned PushedTLS = 8;
static const unsigned PushedFP = 12;
static const unsigned SetFP = 16;
static const unsigned PoppedFP = 4;
#elif defined(JS_CODEGEN_ARM64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 0;
static const unsigned PushedFP = 0;
static const unsigned SetFP = 0;
static const unsigned PoppedFP = 0;
#elif defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
static const unsigned BeforePushRetAddr = 0;
static const unsigned PushedRetAddr = 4;
static const unsigned PushedTLS = 8;
static const unsigned PushedFP = 12;
static const unsigned SetFP = 16;
static const unsigned PoppedFP = 4;
#elif defined(JS_CODEGEN_NONE)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedTLS = 0;
static const unsigned PushedFP = 0;
static const unsigned SetFP = 0;
static const unsigned PoppedFP = 0;
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
LoadActivation(MacroAssembler& masm, Register dest)
{
    // WasmCall pushes a WasmActivation and an inactive JitActivation. The
    // JitActivation only becomes active when calling into JS from wasm.
    masm.loadPtr(Address(WasmTlsReg, offsetof(wasm::TlsData, addressOfContext)), dest);
    masm.loadPtr(Address(dest, 0), dest);
    masm.loadPtr(Address(dest, JSContext::offsetOfActivation()), dest);
    masm.loadPtr(Address(dest, Activation::offsetOfPrev()), dest);
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
        masm.push(WasmTlsReg);
        MOZ_ASSERT_IF(!masm.oom(), PushedTLS == masm.currentOffset() - *entry);
        masm.push(FramePointer);
        MOZ_ASSERT_IF(!masm.oom(), PushedFP == masm.currentOffset() - *entry);
        masm.moveStackPtrTo(FramePointer);
        MOZ_ASSERT_IF(!masm.oom(), SetFP == masm.currentOffset() - *entry);
    }

    if (!reason.isNone()) {
        // Native callers expect the native ABI, which assume that nonvolatile
        // registers are preserved.
        Register scratch = ABINonArgReg0;
        if (reason.isNative() && !scratch.volatile_())
            masm.Push(scratch);

        LoadActivation(masm, scratch);
        masm.wasmAssertNonExitInvariants(scratch);
        Address exitReason(scratch, WasmActivation::offsetOfExitReason());
        masm.store32(Imm32(reason.raw()), exitReason);
        Address exitFP(scratch, WasmActivation::offsetOfExitFP());
        masm.storePtr(FramePointer, exitFP);

        if (reason.isNative() && !scratch.volatile_())
            masm.Pop(scratch);
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

    if (!reason.isNone()) {
        // See comment in GenerateCallablePrologue.
        Register scratch = ABINonArgReturnReg0;
        if (reason.isNative() && !scratch.volatile_())
            masm.Push(scratch);

        LoadActivation(masm, scratch);
        Address exitFP(scratch, WasmActivation::offsetOfExitFP());
        masm.storePtr(ImmWord(0), exitFP);
        Address exitReason(scratch, WasmActivation::offsetOfExitReason());
#ifdef DEBUG
        // Check the passed exitReason is the same as the one on entry.
        masm.push(scratch);
        masm.loadPtr(exitReason, ABINonArgReturnReg0);
        Label ok;
        masm.branch32(Assembler::Condition::Equal, ABINonArgReturnReg0, Imm32(reason.raw()), &ok);
        masm.breakpoint();
        masm.bind(&ok);
        masm.pop(scratch);
#endif
        masm.store32(Imm32(ExitReason::None().raw()), exitReason);

        if (reason.isNative() && !scratch.volatile_())
            masm.Pop(scratch);
    }

    // Forbid pools for the same reason as described in GenerateCallablePrologue.
#if defined(JS_CODEGEN_ARM)
    AutoForbidPools afp(&masm, /* number of instructions in scope = */ 3);
#endif

    // There is an important ordering constraint here: fp must be repointed to
    // the caller's frame before any field of the frame currently pointed to by
    // fp is popped: asynchronous signal handlers (which use stack space
    // starting at sp) could otherwise clobber these fields while they are still
    // accessible via fp (fp fields are read during frame iteration which is
    // *also* done asynchronously).

    masm.pop(FramePointer);
    DebugOnly<uint32_t> poppedFP = masm.currentOffset();
    masm.pop(WasmTlsReg);
    *ret = masm.currentOffset();
    masm.ret();

    MOZ_ASSERT_IF(!masm.oom(), PoppedFP == *ret - poppedFP);
}

void
wasm::GenerateFunctionPrologue(MacroAssembler& masm, unsigned framePushed, const SigIdDesc& sigId,
                               FuncOffsets* offsets)
{
    // Flush pending pools so they do not get dumped between the 'begin' and
    // 'normalEntry' offsets since the difference must be less than UINT8_MAX
    // to be stored in CodeRange::funcBeginToNormalEntry_.
    masm.flushBuffer();
    masm.haltingAlign(CodeAlignment);

    // Generate table entry:
    offsets->begin = masm.currentOffset();
    BytecodeOffset trapOffset(0);  // ignored by masm.wasmEmitTrapOutOfLineCode
    TrapDesc trap(trapOffset, Trap::IndirectCallBadSig, masm.framePushed());
    switch (sigId.kind()) {
      case SigIdDesc::Kind::Global: {
        Register scratch = WasmTableCallScratchReg;
        masm.loadWasmGlobalPtr(sigId.globalDataOffset(), scratch);
        masm.branchPtr(Assembler::Condition::NotEqual, WasmTableCallSigReg, scratch, trap);
        break;
      }
      case SigIdDesc::Kind::Immediate: {
        masm.branch32(Assembler::Condition::NotEqual, WasmTableCallSigReg, Imm32(sigId.immediate()), trap);
        break;
      }
      case SigIdDesc::Kind::None:
        break;
    }

    // The table entry might have generated a small constant pool in case of
    // immediate comparison.
    masm.flushBuffer();

    // Generate normal entry:
    masm.nopAlign(CodeAlignment);
    GenerateCallablePrologue(masm, framePushed, ExitReason::None(), &offsets->normalEntry);

    masm.setFramePushed(framePushed);
}

void
wasm::GenerateFunctionEpilogue(MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets)
{
    MOZ_ASSERT(masm.framePushed() == framePushed);
    GenerateCallableEpilogue(masm, framePushed, ExitReason::None(), &offsets->ret);
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
    exitReason_(ExitReason::Fixed::None)
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
    exitReason_(ExitReason::Fixed::None)
{
    initFromExitFP();
}

static inline void
AssertMatchesCallSite(const WasmActivation& activation, void* callerPC, Frame* callerFP)
{
#ifdef DEBUG
    const Code* code = activation.compartment()->wasm.lookupCode(callerPC);
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
    Frame* fp = activation_->exitFP();
    void* pc = fp->returnAddress;

    stackAddress_ = fp;

    code_ = activation_->compartment()->wasm.lookupCode(pc);
    MOZ_ASSERT(code_);

    codeRange_ = code_->lookupRange(pc);
    MOZ_ASSERT(codeRange_);

    // Since we don't have the pc for fp, start unwinding at the caller of fp.
    // This means that the innermost frame is skipped. This is fine because:
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
        fp = fp->callerFP;
        callerPC_ = fp->returnAddress;
        callerFP_ = fp->callerFP;
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        break;
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::BuiltinThunk:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
      case CodeRange::Throw:
      case CodeRange::Interrupt:
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
    exitReason_(ExitReason::Fixed::None)
{
    // In the case of ImportJitExit, the fp register may be temporarily
    // clobbered on return from Ion so always use activation.fp when it is set.
    if (activation.exitFP()) {
        initFromExitFP();
        return;
    }

    Frame* fp = (Frame*)state.fp;
    uint8_t* pc = (uint8_t*)state.pc;
    void** sp = (void**)state.sp;

    // Get the CodeRange describing pc and the base address to which the
    // CodeRange is relative. If the pc is not in a wasm module or a builtin
    // thunk, then execution must be entering from or leaving to the C++ caller
    // that pushed the WasmActivation.
    const CodeRange* codeRange;
    uint8_t* codeBase;
    code_ = activation_->compartment()->wasm.lookupCode(pc);
    if (code_) {
        const CodeSegment* codeSegment;
        codeRange = code_->lookupRange(pc, &codeSegment);
        codeBase = codeSegment->base();
    } else if (!LookupBuiltinThunk(pc, &codeRange, &codeBase)) {
        MOZ_ASSERT(done());
        return;
    }

    // When the pc is inside the prologue/epilogue, the innermost call's Frame
    // is not complete and thus fp points to the second-to-innermost call's
    // Frame. Since fp can only tell you about its caller, naively unwinding
    // while pc is in the prologue/epilogue would skip the second-to-innermost
    // call. To avoid this problem, we use the static structure of the code in
    // the prologue and epilogue to do the Right Thing.
    uint32_t offsetInCode = pc - codeBase;
    MOZ_ASSERT(offsetInCode >= codeRange->begin());
    MOZ_ASSERT(offsetInCode < codeRange->end());

    // Compute the offset of the pc from the (normal) entry of the code range.
    // The stack state of the pc for the entire table-entry is equivalent to
    // that of the first pc of the normal-entry. Thus, we can simplify the below
    // case analysis by redirecting all pc-in-table-entry cases to the
    // pc-at-normal-entry case.
    uint32_t offsetFromEntry;
    if (codeRange->isFunction()) {
        if (offsetInCode < codeRange->funcNormalEntry())
            offsetFromEntry = 0;
        else
            offsetFromEntry = offsetInCode - codeRange->funcNormalEntry();
    } else {
        offsetFromEntry = offsetInCode - codeRange->begin();
    }

    switch (codeRange->kind()) {
      case CodeRange::Function:
      case CodeRange::FarJumpIsland:
      case CodeRange::ImportJitExit:
      case CodeRange::ImportInterpExit:
      case CodeRange::BuiltinThunk:
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
        } else if (offsetFromEntry == PushedTLS) {
            // The return address and caller's TLS have been pushed on the stack; fp
            // is still the caller's fp.
            callerPC_ = sp[1];
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else if (offsetFromEntry == PushedFP) {
            // The full Frame has been pushed; fp is still the caller's fp.
            MOZ_ASSERT(fp == reinterpret_cast<Frame*>(sp)->callerFP);
            callerPC_ = reinterpret_cast<Frame*>(sp)->returnAddress;
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else if (offsetInCode == codeRange->ret() - PoppedFP) {
            // The callerFP field of the Frame has been popped into fp.
            callerPC_ = sp[1];
            callerFP_ = fp;
        } else if (offsetInCode == codeRange->ret()) {
            // Both the TLS and callerFP fields have been popped and fp now
            // points to the caller's frame.
            callerPC_ = sp[0];
            callerFP_ = fp;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        } else {
            // Not in the prologue/epilogue.
            callerPC_ = fp->returnAddress;
            callerFP_ = fp->callerFP;
            AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        }
        break;
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
        // Inline code stubs execute after the prologue/epilogue have completed
        // so we can simply unwind based on fp.
        callerPC_ = fp->returnAddress;
        callerFP_ = fp->callerFP;
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_);
        break;
      case CodeRange::Entry:
        // The entry trampoline is the final frame in an WasmActivation. The entry
        // trampoline also doesn't GeneratePrologue/Epilogue so we can't use
        // the general unwinding logic above.
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case CodeRange::Throw:
        // The throw stub executes a small number of instructions before popping
        // the entire activation. To simplify testing, we simply pretend throw
        // stubs have already popped the entire stack.
        MOZ_ASSERT(done());
        return;
      case CodeRange::Interrupt:
        // When the PC is in the async interrupt stub, the fp may be garbage and
        // so we cannot blindly unwind it. Since the percent of time spent in
        // the interrupt stub is extremely small, just ignore the stack.
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
    if (!exitReason_.isNone()) {
        MOZ_ASSERT(codeRange_);
        exitReason_ = ExitReason::None();
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
      case CodeRange::BuiltinThunk:
      case CodeRange::TrapExit:
      case CodeRange::DebugTrap:
      case CodeRange::Inline:
      case CodeRange::FarJumpIsland:
        stackAddress_ = callerFP_;
        callerPC_ = callerFP_->returnAddress;
        AssertMatchesCallSite(*activation_, callerPC_, callerFP_->callerFP);
        callerFP_ = callerFP_->callerFP;
        break;
      case CodeRange::Interrupt:
      case CodeRange::Throw:
        MOZ_CRASH("code range doesn't have frame");
    }

    MOZ_ASSERT(!done());
}

static const char*
ThunkedNativeToDescription(SymbolicAddress func)
{
    MOZ_ASSERT(NeedsBuiltinThunk(func));
    switch (func) {
      case SymbolicAddress::HandleExecutionInterrupt:
      case SymbolicAddress::HandleDebugTrap:
      case SymbolicAddress::HandleThrow:
      case SymbolicAddress::ReportTrap:
      case SymbolicAddress::ReportOutOfBounds:
      case SymbolicAddress::ReportUnalignedAccess:
      case SymbolicAddress::CallImport_Void:
      case SymbolicAddress::CallImport_I32:
      case SymbolicAddress::CallImport_I64:
      case SymbolicAddress::CallImport_F64:
      case SymbolicAddress::CoerceInPlace_ToInt32:
      case SymbolicAddress::CoerceInPlace_ToNumber:
        MOZ_ASSERT(!NeedsBuiltinThunk(func), "not in sync with NeedsBuiltinThunk");
        break;
      case SymbolicAddress::ToInt32:
        return "call to asm.js native ToInt32 coercion (in wasm)";
      case SymbolicAddress::DivI64:
        return "call to native i64.div_s (in wasm)";
      case SymbolicAddress::UDivI64:
        return "call to native i64.div_u (in wasm)";
      case SymbolicAddress::ModI64:
        return "call to native i64.rem_s (in wasm)";
      case SymbolicAddress::UModI64:
        return "call to native i64.rem_u (in wasm)";
      case SymbolicAddress::TruncateDoubleToUint64:
        return "call to native i64.trunc_u/f64 (in wasm)";
      case SymbolicAddress::TruncateDoubleToInt64:
        return "call to native i64.trunc_s/f64 (in wasm)";
      case SymbolicAddress::Uint64ToDouble:
        return "call to native f64.convert_u/i64 (in wasm)";
      case SymbolicAddress::Uint64ToFloat32:
        return "call to native f32.convert_u/i64 (in wasm)";
      case SymbolicAddress::Int64ToDouble:
        return "call to native f64.convert_s/i64 (in wasm)";
      case SymbolicAddress::Int64ToFloat32:
        return "call to native f32.convert_s/i64 (in wasm)";
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
        return "call to native i32.div_s (in wasm)";
      case SymbolicAddress::aeabi_uidivmod:
        return "call to native i32.div_u (in wasm)";
      case SymbolicAddress::AtomicCmpXchg:
        return "call to native atomic compare exchange (in wasm)";
      case SymbolicAddress::AtomicXchg:
        return "call to native atomic exchange (in wasm)";
      case SymbolicAddress::AtomicFetchAdd:
        return "call to native atomic fetch add (in wasm)";
      case SymbolicAddress::AtomicFetchSub:
        return "call to native atomic fetch sub (in wasm)";
      case SymbolicAddress::AtomicFetchAnd:
        return "call to native atomic fetch and (in wasm)";
      case SymbolicAddress::AtomicFetchOr:
        return "call to native atomic fetch or (in wasm)";
      case SymbolicAddress::AtomicFetchXor:
        return "call to native atomic fetch xor (in wasm)";
#endif
      case SymbolicAddress::ModD:
        return "call to asm.js native f64 % (mod)";
      case SymbolicAddress::SinD:
        return "call to asm.js native f64 Math.sin";
      case SymbolicAddress::CosD:
        return "call to asm.js native f64 Math.cos";
      case SymbolicAddress::TanD:
        return "call to asm.js native f64 Math.tan";
      case SymbolicAddress::ASinD:
        return "call to asm.js native f64 Math.asin";
      case SymbolicAddress::ACosD:
        return "call to asm.js native f64 Math.acos";
      case SymbolicAddress::ATanD:
        return "call to asm.js native f64 Math.atan";
      case SymbolicAddress::CeilD:
        return "call to native f64.ceil (in wasm)";
      case SymbolicAddress::CeilF:
        return "call to native f32.ceil (in wasm)";
      case SymbolicAddress::FloorD:
        return "call to native f64.floor (in wasm)";
      case SymbolicAddress::FloorF:
        return "call to native f32.floor (in wasm)";
      case SymbolicAddress::TruncD:
        return "call to native f64.trunc (in wasm)";
      case SymbolicAddress::TruncF:
        return "call to native f32.trunc (in wasm)";
      case SymbolicAddress::NearbyIntD:
        return "call to native f64.nearest (in wasm)";
      case SymbolicAddress::NearbyIntF:
        return "call to native f32.nearest (in wasm)";
      case SymbolicAddress::ExpD:
        return "call to asm.js native f64 Math.exp";
      case SymbolicAddress::LogD:
        return "call to asm.js native f64 Math.log";
      case SymbolicAddress::PowD:
        return "call to asm.js native f64 Math.pow";
      case SymbolicAddress::ATan2D:
        return "call to asm.js native f64 Math.atan2";
      case SymbolicAddress::GrowMemory:
        return "call to native grow_memory (in wasm)";
      case SymbolicAddress::CurrentMemory:
        return "call to native current_memory (in wasm)";
      case SymbolicAddress::Limit:
        break;
    }
    return "?";
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
    static const char* importJitDescription = "fast FFI trampoline (in wasm)";
    static const char* importInterpDescription = "slow FFI trampoline (in wasm)";
    static const char* builtinNativeDescription = "fast FFI trampoline to native (in wasm)";
    static const char* trapDescription = "trap handling (in wasm)";
    static const char* debugTrapDescription = "debug trap handling (in wasm)";

    if (!exitReason_.isFixed())
        return ThunkedNativeToDescription(exitReason_.symbolic());

    switch (exitReason_.fixed()) {
      case ExitReason::Fixed::None:
        break;
      case ExitReason::Fixed::ImportJit:
        return importJitDescription;
      case ExitReason::Fixed::ImportInterp:
        return importInterpDescription;
      case ExitReason::Fixed::BuiltinNative:
        return builtinNativeDescription;
      case ExitReason::Fixed::Trap:
        return trapDescription;
      case ExitReason::Fixed::DebugTrap:
        return debugTrapDescription;
    }

    switch (codeRange_->kind()) {
      case CodeRange::Function:          return code_->profilingLabel(codeRange_->funcIndex());
      case CodeRange::Entry:             return "entry trampoline (in wasm)";
      case CodeRange::ImportJitExit:     return importJitDescription;
      case CodeRange::BuiltinThunk:      return builtinNativeDescription;
      case CodeRange::ImportInterpExit:  return importInterpDescription;
      case CodeRange::TrapExit:          return trapDescription;
      case CodeRange::DebugTrap:         return debugTrapDescription;
      case CodeRange::Inline:            return "inline stub (in wasm)";
      case CodeRange::FarJumpIsland:     return "interstitial (in wasm)";
      case CodeRange::Throw:             MOZ_FALLTHROUGH;
      case CodeRange::Interrupt:         MOZ_CRASH("does not have a frame");
    }

    MOZ_CRASH("bad code range kind");
}

void
wasm::TraceActivations(JSContext* cx, const CooperatingContext& target, JSTracer* trc)
{
    for (ActivationIterator iter(cx, target); !iter.done(); ++iter) {
        if (iter.activation()->isWasm()) {
            for (FrameIterator fi(iter.activation()->asWasm()); !fi.done(); ++fi)
                fi.instance()->trace(trc);
        }
    }
}

Instance*
wasm::LookupFaultingInstance(WasmActivation* activation, void* pc, void* fp)
{
    // Assume bug-caused faults can be raised at any PC and apply the logic of
    // ProfilingFrameIterator to reject any pc outside the (post-prologue,
    // pre-epilogue) body of a wasm function. This is exhaustively tested by the
    // simulators which call this function at every load/store before even
    // knowing whether there is a fault.

    const Code* code = activation->compartment()->wasm.lookupCode(pc);
    if (!code)
        return nullptr;

    const CodeSegment* codeSegment;
    const CodeRange* codeRange = code->lookupRange(pc, &codeSegment);
    if (!codeRange || !codeRange->isFunction())
        return nullptr;

    size_t offsetInModule = ((uint8_t*)pc) - codeSegment->base();
    if (offsetInModule < codeRange->funcNormalEntry() + SetFP)
        return nullptr;
    if (offsetInModule >= codeRange->ret() - PoppedFP)
        return nullptr;

    Instance* instance = reinterpret_cast<Frame*>(fp)->tls->instance;
    MOZ_RELEASE_ASSERT(&instance->code() == code);
    return instance;
}

WasmActivation*
wasm::ActivationIfInnermost(JSContext* cx)
{
    // WasmCall pushes both an outer WasmActivation and an inner JitActivation
    // that only becomes active when calling JIT code.
    Activation* act = cx->activation();
    while (act && act->isJit() && !act->asJit()->isActive())
        act = act->prev();
    if (!act || !act->isWasm())
        return nullptr;
    return act->asWasm();
}

bool
wasm::InCompiledCode(void* pc)
{
    JSContext* cx = TlsContext.get();
    if (!cx)
        return false;

    MOZ_RELEASE_ASSERT(!cx->handlingSegFault);

    if (cx->compartment()->wasm.lookupCode(pc))
        return true;

    const CodeRange* codeRange;
    uint8_t* codeBase;
    return LookupBuiltinThunk(pc, &codeRange, &codeBase);
}
