/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSFrameIterator.h"

#include "jit/AsmJS.h"
#include "jit/AsmJSModule.h"
#include "jit/IonMacroAssembler.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

/*****************************************************************************/
// AsmJSFrameIterator implementation

static void *
ReturnAddressFromFP(void *fp)
{
    return reinterpret_cast<AsmJSFrame*>(fp)->returnAddress;
}

static uint8_t *
CallerFPFromFP(void *fp)
{
    return reinterpret_cast<AsmJSFrame*>(fp)->callerFP;
}

AsmJSFrameIterator::AsmJSFrameIterator(const AsmJSActivation &activation)
  : module_(&activation.module()),
    fp_(activation.fp())
{
    if (!fp_)
        return;
    settle();
}

void
AsmJSFrameIterator::operator++()
{
    JS_ASSERT(!done());
    DebugOnly<uint8_t*> oldfp = fp_;
    fp_ += callsite_->stackDepth();
    JS_ASSERT_IF(module_->profilingEnabled(), fp_ == CallerFPFromFP(oldfp));
    settle();
}

void
AsmJSFrameIterator::settle()
{
    void *returnAddress = ReturnAddressFromFP(fp_);

    const AsmJSModule::CodeRange *codeRange = module_->lookupCodeRange(returnAddress);
    JS_ASSERT(codeRange);
    codeRange_ = codeRange;

    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Function:
        callsite_ = module_->lookupCallSite(returnAddress);
        JS_ASSERT(callsite_);
        break;
      case AsmJSModule::CodeRange::Entry:
        fp_ = nullptr;
        JS_ASSERT(done());
        break;
      case AsmJSModule::CodeRange::FFI:
      case AsmJSModule::CodeRange::Interrupt:
      case AsmJSModule::CodeRange::Inline:
      case AsmJSModule::CodeRange::Thunk:
        MOZ_ASSUME_UNREACHABLE("Should not encounter an exit during iteration");
    }
}

JSAtom *
AsmJSFrameIterator::functionDisplayAtom() const
{
    JS_ASSERT(!done());
    return reinterpret_cast<const AsmJSModule::CodeRange*>(codeRange_)->functionName(*module_);
}

unsigned
AsmJSFrameIterator::computeLine(uint32_t *column) const
{
    JS_ASSERT(!done());
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
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 10;
static const unsigned StoredFP = 14;
#elif defined(JS_CODEGEN_X86)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 8;
static const unsigned StoredFP = 11;
#elif defined(JS_CODEGEN_ARM)
static const unsigned PushedRetAddr = 4;
static const unsigned PushedFP = 16;
static const unsigned StoredFP = 20;
#elif defined(JS_CODEGEN_NONE)
static const unsigned PushedRetAddr = 0;
static const unsigned PushedFP = 1;
static const unsigned StoredFP = 1;
#else
# error "Unknown architecture!"
#endif

static void
PushRetAddr(MacroAssembler &masm)
{
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS)
    masm.push(ra);
#else
    // The x86/x64 call instruction pushes the return address.
#endif
}

// Generate a prologue that maintains AsmJSActivation::fp as the virtual frame
// pointer so that AsmJSProfilingFrameIterator can walk the stack at any pc in
// generated code.
static void
GenerateProfilingPrologue(MacroAssembler &masm, unsigned framePushed, AsmJSExit::Reason reason,
                          Label *begin)
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

    // AsmJSProfilingFrameIterator needs to know the offsets of several key
    // instructions from 'begin'. To save space, we make these offsets static
    // constants and assert that they match the actual codegen below. On ARM,
    // this requires AutoForbidPools to prevent a constant pool from being
    // randomly inserted between two instructions.
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 5);
#endif
        DebugOnly<uint32_t> offsetAtBegin = masm.currentOffset();
        masm.bind(begin);

        PushRetAddr(masm);
        JS_ASSERT(PushedRetAddr == masm.currentOffset() - offsetAtBegin);

        masm.loadAsmJSActivation(scratch);
        masm.push(Address(scratch, AsmJSActivation::offsetOfFP()));
        JS_ASSERT(PushedFP == masm.currentOffset() - offsetAtBegin);

        masm.storePtr(StackPointer, Address(scratch, AsmJSActivation::offsetOfFP()));
        JS_ASSERT(StoredFP == masm.currentOffset() - offsetAtBegin);
    }

    if (reason != AsmJSExit::None)
        masm.store32_NoSecondScratch(Imm32(reason), Address(scratch, AsmJSActivation::offsetOfExitReason()));

#if defined(JS_CODEGEN_ARM)
    masm.setSecondScratchReg(lr);
#endif

    if (framePushed)
        masm.subPtr(Imm32(framePushed), StackPointer);
}

// Generate the inverse of GenerateProfilingPrologue.
static void
GenerateProfilingEpilogue(MacroAssembler &masm, unsigned framePushed, AsmJSExit::Reason reason,
                          Label *profilingReturn)
{
    Register scratch = ABIArgGenerator::NonReturn_VolatileReg0;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    Register scratch2 = ABIArgGenerator::NonReturn_VolatileReg1;
#endif

    if (framePushed)
        masm.addPtr(Imm32(framePushed), StackPointer);

    masm.loadAsmJSActivation(scratch);

    if (reason != AsmJSExit::None)
        masm.store32(Imm32(AsmJSExit::None), Address(scratch, AsmJSActivation::offsetOfExitReason()));

    // AsmJSProfilingFrameIterator assumes that there is only a single 'ret'
    // instruction (whose offset is recorded by profilingReturn) after the store
    // which sets AsmJSActivation::fp to the caller's fp. Use AutoForbidPools to
    // ensure that a pool is not inserted before the return (a pool inserts a
    // jump instruction).
    {
#if defined(JS_CODEGEN_ARM)
        AutoForbidPools afp(&masm, /* number of instructions in scope = */ 3);
#endif
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        masm.pop(scratch2);
        masm.storePtr(scratch2, Address(scratch, AsmJSActivation::offsetOfFP()));
#else
        masm.pop(Operand(scratch, AsmJSActivation::offsetOfFP()));
#endif
        masm.bind(profilingReturn);
        masm.ret();
    }
}

// In profiling mode, we need to maintain fp so that we can unwind the stack at
// any pc. In non-profiling mode, the only way to observe AsmJSActivation::fp is
// to call out to C++ so, as an optimization, we don't update fp. To avoid
// recompilation when the profiling mode is toggled, we generate both prologues
// a priori and switch between prologues when the profiling mode is toggled.
// Specifically, AsmJSModule::setProfilingEnabled patches all callsites to
// either call the profiling or non-profiling entry point.
void
js::GenerateAsmJSFunctionPrologue(MacroAssembler &masm, unsigned framePushed,
                                  AsmJSFunctionLabels *labels)
{
#if defined(JS_CODEGEN_ARM)
    // Flush pending pools so they do not get dumped between the 'begin' and
    // 'entry' labels since the difference must be less than UINT8_MAX.
    masm.flushBuffer();
#endif

    masm.align(CodeAlignment);

    GenerateProfilingPrologue(masm, framePushed, AsmJSExit::None, &labels->begin);
    Label body;
    masm.jump(&body);

    // Generate normal prologue:
    masm.align(CodeAlignment);
    masm.bind(&labels->entry);
    PushRetAddr(masm);
    masm.subPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress), StackPointer);

    // Prologue join point, body begin:
    masm.bind(&body);
    masm.setFramePushed(framePushed);

    // Overflow checks are omitted by CodeGenerator in some cases (leaf
    // functions with small framePushed). Perform overflow-checking after
    // pushing framePushed to catch cases with really large frames.
    if (!labels->overflowThunk.empty()) {
        // If framePushed is zero, we don't need a thunk to adjust StackPointer.
        Label *target = framePushed ? labels->overflowThunk.addr() : &labels->overflowExit;
        masm.branchPtr(Assembler::AboveOrEqual,
                       AsmJSAbsoluteAddress(AsmJSImm_StackLimit),
                       StackPointer,
                       target);
    }
}

// Similar to GenerateAsmJSFunctionPrologue (see comment), we generate both a
// profiling and non-profiling epilogue a priori. When the profiling mode is
// toggled, AsmJSModule::setProfilingEnabled patches the 'profiling jump' to
// either be a nop (falling through to the normal prologue) or a jump (jumping
// to the profiling epilogue).
void
js::GenerateAsmJSFunctionEpilogue(MacroAssembler &masm, unsigned framePushed,
                                  AsmJSFunctionLabels *labels)
{
    JS_ASSERT(masm.framePushed() == framePushed);

#if defined(JS_CODEGEN_ARM)
    // Flush pending pools so they do not get dumped between the profilingReturn
    // and profilingJump/profilingEpilogue labels since the difference must be
    // less than UINT8_MAX.
    masm.flushBuffer();
#endif

    {
#if defined(JS_CODEGEN_ARM)
        // Forbid pools from being inserted between the profilingJump label and
        // the nop since we need the location of the actual nop to patch it.
        AutoForbidPools afp(&masm, 1);
#endif

        // The exact form of this instruction must be kept consistent with the
        // patching in AsmJSModule::setProfilingEnabled.
        masm.bind(&labels->profilingJump);
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        masm.twoByteNop();
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        masm.nop();
#endif
    }

    // Normal epilogue:
    masm.addPtr(Imm32(framePushed + AsmJSFrameBytesAfterReturnAddress), StackPointer);
    masm.ret();
    masm.setFramePushed(0);

    // Profiling epilogue:
    masm.bind(&labels->profilingEpilogue);
    GenerateProfilingEpilogue(masm, framePushed, AsmJSExit::None, &labels->profilingReturn);

    if (!labels->overflowThunk.empty() && labels->overflowThunk.ref().used()) {
        // The general throw stub assumes that only sizeof(AsmJSFrame) bytes
        // have been pushed. The overflow check occurs after incrementing by
        // framePushed, so pop that before jumping to the overflow exit.
        masm.bind(labels->overflowThunk.addr());
        masm.addPtr(Imm32(framePushed), StackPointer);
        masm.jump(&labels->overflowExit);
    }
}

void
js::GenerateAsmJSStackOverflowExit(MacroAssembler &masm, Label *overflowExit, Label *throwLabel)
{
    masm.bind(overflowExit);

    // If we reach here via the non-profiling prologue, AsmJSActivation::fp has
    // not been updated. To enable stack unwinding from C++, store to it now. If
    // we reached here via the profiling prologue, we'll just store the same
    // value again. Do not update AsmJSFrame::callerFP as it is not necessary in
    // the non-profiling case (there is no return path from this point) and, in
    // the profiling case, it is already correct.
    Register activation = ABIArgGenerator::NonArgReturnReg0;
    masm.loadAsmJSActivation(activation);
    masm.storePtr(StackPointer, Address(activation, AsmJSActivation::offsetOfFP()));

    // Prepare the stack for calling C++.
    if (unsigned stackDec = StackDecrementForCall(sizeof(AsmJSFrame), ShadowStackSpace))
        masm.subPtr(Imm32(stackDec), StackPointer);

    // No need to restore the stack; the throw stub pops everything.
    masm.assertStackAlignment();
    masm.call(AsmJSImmPtr(AsmJSImm_ReportOverRecursed));
    masm.jump(throwLabel);
}

void
js::GenerateAsmJSExitPrologue(MacroAssembler &masm, unsigned framePushed, AsmJSExit::Reason reason,
                              Label *begin)
{
    masm.align(CodeAlignment);
    GenerateProfilingPrologue(masm, framePushed, reason, begin);
    masm.setFramePushed(framePushed);
}

void
js::GenerateAsmJSExitEpilogue(MacroAssembler &masm, unsigned framePushed, AsmJSExit::Reason reason,
                              Label *profilingReturn)
{
    // Inverse of GenerateAsmJSExitPrologue:
    JS_ASSERT(masm.framePushed() == framePushed);
    GenerateProfilingEpilogue(masm, framePushed, reason, profilingReturn);
    masm.setFramePushed(0);
}

/*****************************************************************************/
// AsmJSProfilingFrameIterator

AsmJSProfilingFrameIterator::AsmJSProfilingFrameIterator(const AsmJSActivation &activation)
  : module_(&activation.module()),
    callerFP_(nullptr),
    callerPC_(nullptr),
    stackAddress_(nullptr),
    exitReason_(AsmJSExit::None),
    codeRange_(nullptr)
{
    initFromFP(activation);
}

static inline void
AssertMatchesCallSite(const AsmJSModule &module, const AsmJSModule::CodeRange *calleeCodeRange,
                      void *callerPC, void *callerFP, void *fp)
{
#ifdef DEBUG
    const AsmJSModule::CodeRange *callerCodeRange = module.lookupCodeRange(callerPC);
    JS_ASSERT(callerCodeRange);
    if (callerCodeRange->isEntry()) {
        JS_ASSERT(callerFP == nullptr);
        return;
    }

    const CallSite *callsite = module.lookupCallSite(callerPC);
    if (calleeCodeRange->isThunk()) {
        JS_ASSERT(!callsite);
        JS_ASSERT(callerCodeRange->isFunction());
    } else {
        JS_ASSERT(callsite);
        JS_ASSERT(callerFP == (uint8_t*)fp + callsite->stackDepth());
    }
#endif
}

void
AsmJSProfilingFrameIterator::initFromFP(const AsmJSActivation &activation)
{
    uint8_t *fp = activation.fp();

    // If a signal was handled while entering an activation, the frame will
    // still be null.
    if (!fp) {
        JS_ASSERT(done());
        return;
    }

    // Since we don't have the pc for fp, start unwinding at the caller of fp,
    // whose pc we do have via fp->returnAddress. This means that the innermost
    // frame is skipped but this is fine because:
    //  - for FFI calls, the innermost frame is a thunk, so the first frame that
    //    shows up is the function calling the FFI;
    //  - for Math and other builtin calls, when profiling is activated, we
    //    patch all call sites to instead call through a thunk; and
    //  - for interrupts, we just accept that we'll lose the innermost frame.
    // However, we do want FFI trampolines to show up in callstacks (so that
    // they properly accumulate self-time) and for this we use the exitReason.

    exitReason_ = activation.exitReason();

    void *pc = ReturnAddressFromFP(fp);
    const AsmJSModule::CodeRange *codeRange = module_->lookupCodeRange(pc);
    JS_ASSERT(codeRange);
    codeRange_ = codeRange;
    stackAddress_ = fp;

    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Entry:
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      case AsmJSModule::CodeRange::Function:
        fp = CallerFPFromFP(fp);
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, fp);
        break;
      case AsmJSModule::CodeRange::FFI:
      case AsmJSModule::CodeRange::Interrupt:
      case AsmJSModule::CodeRange::Inline:
      case AsmJSModule::CodeRange::Thunk:
        MOZ_CRASH("Unexpected CodeRange kind");
    }

    JS_ASSERT(!done());
}

typedef JS::ProfilingFrameIterator::RegisterState RegisterState;

AsmJSProfilingFrameIterator::AsmJSProfilingFrameIterator(const AsmJSActivation &activation,
                                                         const RegisterState &state)
  : module_(&activation.module()),
    callerFP_(nullptr),
    callerPC_(nullptr),
    exitReason_(AsmJSExit::None),
    codeRange_(nullptr)
{
    // If profiling hasn't been enabled for this module, then CallerFPFromFP
    // will be trash, so ignore the entire activation. In practice, this only
    // happens if profiling is enabled while module->active() (in this case,
    // profiling will be enabled when the module becomes inactive and gets
    // called again).
    if (!module_->profilingEnabled()) {
        JS_ASSERT(done());
        return;
    }

    // If pc isn't in the module, we must have exited the asm.js module via an
    // exit trampoline or signal handler.
    if (!module_->containsCodePC(state.pc)) {
        initFromFP(activation);
        return;
    }

    // Note: fp may be null while entering and leaving the activation.
    uint8_t *fp = activation.fp();

    const AsmJSModule::CodeRange *codeRange = module_->lookupCodeRange(state.pc);
    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Function:
      case AsmJSModule::CodeRange::FFI:
      case AsmJSModule::CodeRange::Interrupt:
      case AsmJSModule::CodeRange::Thunk: {
        // While codeRange describes the *current* frame, the fp/pc state stored in
        // the iterator is the *caller's* frame. The reason for this is that the
        // activation.fp isn't always the AsmJSFrame for state.pc; during the
        // prologue/epilogue, activation.fp will point to the caller's frame.
        // Naively unwinding starting at activation.fp could thus lead to the
        // second-to-innermost function being skipped in the callstack which will
        // bork profiling stacks. Instead, we depend on the exact layout of the
        // prologue/epilogue, as generated by GenerateProfiling(Prologue|Epilogue)
        // below.
        uint32_t offsetInModule = ((uint8_t*)state.pc) - module_->codeBase();
        JS_ASSERT(offsetInModule < module_->codeBytes());
        JS_ASSERT(offsetInModule >= codeRange->begin());
        JS_ASSERT(offsetInModule < codeRange->end());
        uint32_t offsetInCodeRange = offsetInModule - codeRange->begin();
        void **sp = (void**)state.sp;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        if (offsetInCodeRange < PushedRetAddr) {
            callerPC_ = state.lr;
            callerFP_ = fp;
            AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, sp - 2);
        } else
#endif
        if (offsetInCodeRange < PushedFP || offsetInModule == codeRange->profilingReturn()) {
            callerPC_ = *sp;
            callerFP_ = fp;
            AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, sp - 1);
        } else if (offsetInCodeRange < StoredFP) {
            JS_ASSERT(fp == CallerFPFromFP(sp));
            callerPC_ = ReturnAddressFromFP(sp);
            callerFP_ = CallerFPFromFP(sp);
            AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, sp);
        } else {
            callerPC_ = ReturnAddressFromFP(fp);
            callerFP_ = CallerFPFromFP(fp);
            AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, fp);
        }
        break;
      }
      case AsmJSModule::CodeRange::Entry: {
        // The entry trampoline is the final frame in an AsmJSActivation. The entry
        // trampoline also doesn't GenerateAsmJSPrologue/Epilogue so we can't use
        // the general unwinding logic below.
        JS_ASSERT(!fp);
        callerPC_ = nullptr;
        callerFP_ = nullptr;
        break;
      }
      case AsmJSModule::CodeRange::Inline: {
        // The throw stub clears AsmJSActivation::fp on it's way out.
        if (!fp) {
            JS_ASSERT(done());
            return;
        }

        // Inline code ranges execute in the frame of the caller have no
        // prologue/epilogue and thus don't require the general unwinding logic
        // as below.
        callerPC_ = ReturnAddressFromFP(fp);
        callerFP_ = CallerFPFromFP(fp);
        AssertMatchesCallSite(*module_, codeRange, callerPC_, callerFP_, fp);
        break;
      }
    }

    codeRange_ = codeRange;
    stackAddress_ = state.sp;
    JS_ASSERT(!done());
}

void
AsmJSProfilingFrameIterator::operator++()
{
    if (exitReason_ != AsmJSExit::None) {
        JS_ASSERT(codeRange_);
        exitReason_ = AsmJSExit::None;
        JS_ASSERT(!done());
        return;
    }

    if (!callerPC_) {
        JS_ASSERT(!callerFP_);
        codeRange_ = nullptr;
        JS_ASSERT(done());
        return;
    }

    JS_ASSERT(callerPC_);
    const AsmJSModule::CodeRange *codeRange = module_->lookupCodeRange(callerPC_);
    JS_ASSERT(codeRange);
    codeRange_ = codeRange;

    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Entry:
        JS_ASSERT(callerFP_ == nullptr);
        JS_ASSERT(callerPC_ != nullptr);
        callerPC_ = nullptr;
        break;
      case AsmJSModule::CodeRange::Function:
      case AsmJSModule::CodeRange::FFI:
      case AsmJSModule::CodeRange::Interrupt:
      case AsmJSModule::CodeRange::Inline:
      case AsmJSModule::CodeRange::Thunk:
        stackAddress_ = callerFP_;
        callerPC_ = ReturnAddressFromFP(callerFP_);
        AssertMatchesCallSite(*module_, codeRange, callerPC_, CallerFPFromFP(callerFP_), callerFP_);
        callerFP_ = CallerFPFromFP(callerFP_);
        break;
    }

    JS_ASSERT(!done());
}

AsmJSProfilingFrameIterator::Kind
AsmJSProfilingFrameIterator::kind() const
{
    JS_ASSERT(!done());

    switch (AsmJSExit::ExtractReasonKind(exitReason_)) {
      case AsmJSExit::Reason_None:
        break;
      case AsmJSExit::Reason_Interrupt:
      case AsmJSExit::Reason_FFI:
        return JS::ProfilingFrameIterator::AsmJSTrampoline;
      case AsmJSExit::Reason_Builtin:
        return JS::ProfilingFrameIterator::CppFunction;
    }

    auto codeRange = reinterpret_cast<const AsmJSModule::CodeRange*>(codeRange_);
    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Function:
        return JS::ProfilingFrameIterator::Function;
      case AsmJSModule::CodeRange::Entry:
      case AsmJSModule::CodeRange::FFI:
      case AsmJSModule::CodeRange::Interrupt:
      case AsmJSModule::CodeRange::Inline:
        return JS::ProfilingFrameIterator::AsmJSTrampoline;
      case AsmJSModule::CodeRange::Thunk:
        return JS::ProfilingFrameIterator::CppFunction;
    }

    MOZ_ASSUME_UNREACHABLE("Bad kind");
}

JSAtom *
AsmJSProfilingFrameIterator::functionDisplayAtom() const
{
    JS_ASSERT(kind() == JS::ProfilingFrameIterator::Function);
    return reinterpret_cast<const AsmJSModule::CodeRange*>(codeRange_)->functionName(*module_);
}

const char *
AsmJSProfilingFrameIterator::functionFilename() const
{
    JS_ASSERT(kind() == JS::ProfilingFrameIterator::Function);
    return module_->scriptSource()->filename();
}

static const char *
BuiltinToName(AsmJSExit::BuiltinKind builtin)
{
    switch (builtin) {
      case AsmJSExit::Builtin_ToInt32:   return "ToInt32";
#if defined(JS_CODEGEN_ARM)
      case AsmJSExit::Builtin_IDivMod:   return "software idivmod";
      case AsmJSExit::Builtin_UDivMod:   return "software uidivmod";
#endif
      case AsmJSExit::Builtin_ModD:      return "fmod";
      case AsmJSExit::Builtin_SinD:      return "Math.sin";
      case AsmJSExit::Builtin_CosD:      return "Math.cos";
      case AsmJSExit::Builtin_TanD:      return "Math.tan";
      case AsmJSExit::Builtin_ASinD:     return "Math.asin";
      case AsmJSExit::Builtin_ACosD:     return "Math.acos";
      case AsmJSExit::Builtin_ATanD:     return "Math.atan";
      case AsmJSExit::Builtin_CeilD:
      case AsmJSExit::Builtin_CeilF:     return "Math.ceil";
      case AsmJSExit::Builtin_FloorD:
      case AsmJSExit::Builtin_FloorF:    return "Math.floor";
      case AsmJSExit::Builtin_ExpD:      return "Math.exp";
      case AsmJSExit::Builtin_LogD:      return "Math.log";
      case AsmJSExit::Builtin_PowD:      return "Math.pow";
      case AsmJSExit::Builtin_ATan2D:    return "Math.atan2";
      case AsmJSExit::Builtin_Limit:     break;
    }
    MOZ_ASSUME_UNREACHABLE("Bad builtin kind");
}

const char *
AsmJSProfilingFrameIterator::nonFunctionDescription() const
{
    JS_ASSERT(!done());
    JS_ASSERT(kind() != JS::ProfilingFrameIterator::Function);

    // Use the same string for both time inside and under so that the two
    // entries will be coalesced by the profiler.
    const char *ffiDescription = "asm.js FFI trampoline";
    const char *interruptDescription = "asm.js slow script interrupt";

    switch (AsmJSExit::ExtractReasonKind(exitReason_)) {
      case AsmJSExit::Reason_None:
        break;
      case AsmJSExit::Reason_FFI:
        return ffiDescription;
      case AsmJSExit::Reason_Interrupt:
        return interruptDescription;
      case AsmJSExit::Reason_Builtin:
        return BuiltinToName(AsmJSExit::ExtractBuiltinKind(exitReason_));
    }

    auto codeRange = reinterpret_cast<const AsmJSModule::CodeRange*>(codeRange_);
    switch (codeRange->kind()) {
      case AsmJSModule::CodeRange::Function:  MOZ_ASSUME_UNREACHABLE("non-functions only");
      case AsmJSModule::CodeRange::Entry:     return "asm.js entry trampoline";
      case AsmJSModule::CodeRange::FFI:       return ffiDescription;
      case AsmJSModule::CodeRange::Interrupt: return interruptDescription;
      case AsmJSModule::CodeRange::Inline:    return "asm.js inline stub";
      case AsmJSModule::CodeRange::Thunk:     return BuiltinToName(codeRange->thunkTarget());
    }

    MOZ_ASSUME_UNREACHABLE("Bad exit kind");
}

