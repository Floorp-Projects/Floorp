/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineJIT.h"
#include "jit/CalleeToken.h"
#include "jit/JitFrames.h"
#include "jit/JitRuntime.h"
#include "jit/JitSpewer.h"
#ifdef JS_ION_PERF
#  include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"
#include "jit/x86/SharedICHelpers-x86.h"
#include "vm/JitActivation.h"  // js::jit::JitActivation
#include "vm/JSContext.h"
#include "vm/Realm.h"
#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif

#include "jit/MacroAssembler-inl.h"
#include "vm/JSScript-inl.h"

using mozilla::IsPowerOfTwo;

using namespace js;
using namespace js::jit;

// All registers to save and restore. This includes the stack pointer, since we
// use the ability to reference register values on the stack by index.
static const LiveRegisterSet AllRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::AllMask),
                    FloatRegisterSet(FloatRegisters::AllMask));

enum EnterJitEbpArgumentOffset {
  ARG_JITCODE = 2 * sizeof(void*),
  ARG_ARGC = 3 * sizeof(void*),
  ARG_ARGV = 4 * sizeof(void*),
  ARG_STACKFRAME = 5 * sizeof(void*),
  ARG_CALLEETOKEN = 6 * sizeof(void*),
  ARG_SCOPECHAIN = 7 * sizeof(void*),
  ARG_STACKVALUES = 8 * sizeof(void*),
  ARG_RESULT = 9 * sizeof(void*)
};

// Generates a trampoline for calling Jit compiled code from a C++ function.
// The trampoline use the EnterJitCode signature, with the standard cdecl
// calling convention.
void JitRuntime::generateEnterJIT(JSContext* cx, MacroAssembler& masm) {
  AutoCreatedBy acb(masm, "JitRuntime::generateEnterJIT");

  enterJITOffset_ = startTrampolineCode(masm);

  masm.assertStackAlignment(ABIStackAlignment,
                            -int32_t(sizeof(uintptr_t)) /* return address */);

  // Save old stack frame pointer, set new stack frame pointer.
  masm.push(ebp);
  masm.movl(esp, ebp);

  // Save non-volatile registers. These must be saved by the trampoline,
  // rather than the JIT'd code, because they are scanned by the conservative
  // scanner.
  masm.push(ebx);
  masm.push(esi);
  masm.push(edi);

  // Keep track of the stack which has to be unwound after returning from the
  // compiled function.
  masm.movl(esp, esi);

  // Load the number of values to be copied (argc) into eax
  masm.loadPtr(Address(ebp, ARG_ARGC), eax);

  // If we are constructing, that also needs to include newTarget
  {
    Label noNewTarget;
    masm.loadPtr(Address(ebp, ARG_CALLEETOKEN), edx);
    masm.branchTest32(Assembler::Zero, edx,
                      Imm32(CalleeToken_FunctionConstructing), &noNewTarget);

    masm.addl(Imm32(1), eax);

    masm.bind(&noNewTarget);
  }

  // eax <- 8*numValues, eax is now the offset betwen argv and the last value.
  masm.shll(Imm32(3), eax);

  // Guarantee stack alignment of Jit frames.
  //
  // This code compensates for the offset created by the copy of the vector of
  // arguments, such that the jit frame will be aligned once the return
  // address is pushed on the stack.
  //
  // In the computation of the offset, we omit the size of the JitFrameLayout
  // which is pushed on the stack, as the JitFrameLayout size is a multiple of
  // the JitStackAlignment.
  masm.movl(esp, ecx);
  masm.subl(eax, ecx);
  static_assert(
      sizeof(JitFrameLayout) % JitStackAlignment == 0,
      "No need to consider the JitFrameLayout for aligning the stack");

  // ecx = ecx & 15, holds alignment.
  masm.andl(Imm32(JitStackAlignment - 1), ecx);
  masm.subl(ecx, esp);

  /***************************************************************
  Loop over argv vector, push arguments onto stack in reverse order
  ***************************************************************/

  // ebx = argv   --argv pointer is in ebp + 16
  masm.loadPtr(Address(ebp, ARG_ARGV), ebx);

  // eax = argv[8(argc)]  --eax now points one value past the last argument
  masm.addl(ebx, eax);

  // while (eax > ebx)  --while still looping through arguments
  {
    Label header, footer;
    masm.bind(&header);

    masm.cmp32(eax, ebx);
    masm.j(Assembler::BelowOrEqual, &footer);

    // eax -= 8  --move to previous argument
    masm.subl(Imm32(8), eax);

    // Push what eax points to on stack, a Value is 2 words
    masm.push(Operand(eax, 4));
    masm.push(Operand(eax, 0));

    masm.jmp(&header);
    masm.bind(&footer);
  }

  // Create the frame descriptor.
  masm.subl(esp, esi);
  masm.makeFrameDescriptor(esi, FrameType::CppToJSJit, JitFrameLayout::Size());

  // Push the number of actual arguments.  |result| is used to store the
  // actual number of arguments without adding an extra argument to the enter
  // JIT.
  masm.mov(Operand(ebp, ARG_RESULT), eax);
  masm.unboxInt32(Address(eax, 0x0), eax);
  masm.push(eax);

  // Push the callee token.
  masm.push(Operand(ebp, ARG_CALLEETOKEN));

  // Load the InterpreterFrame address into the OsrFrameReg.
  // This address is also used for setting the constructing bit on all paths.
  masm.loadPtr(Address(ebp, ARG_STACKFRAME), OsrFrameReg);

  // Push the descriptor.
  masm.push(esi);

  CodeLabel returnLabel;
  CodeLabel oomReturnLabel;
  {
    // Handle Interpreter -> Baseline OSR.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(JSReturnOperand);
    regs.takeUnchecked(OsrFrameReg);
    regs.take(ebp);
    regs.take(ReturnReg);

    Register scratch = regs.takeAny();

    Label notOsr;
    masm.branchTestPtr(Assembler::Zero, OsrFrameReg, OsrFrameReg, &notOsr);

    Register numStackValues = regs.takeAny();
    masm.loadPtr(Address(ebp, ARG_STACKVALUES), numStackValues);

    Register jitcode = regs.takeAny();
    masm.loadPtr(Address(ebp, ARG_JITCODE), jitcode);

    // Push return address.
    masm.mov(&returnLabel, scratch);
    masm.push(scratch);

    // Push previous frame pointer.
    masm.push(ebp);

    // Reserve frame.
    Register framePtr = ebp;
    masm.subPtr(Imm32(BaselineFrame::Size()), esp);

    masm.touchFrameValues(numStackValues, scratch, framePtr);
    masm.mov(esp, framePtr);

    // Reserve space for locals and stack values.
    masm.mov(numStackValues, scratch);
    masm.shll(Imm32(3), scratch);
    masm.subPtr(scratch, esp);

    // Enter exit frame.
    masm.addPtr(
        Imm32(BaselineFrame::Size() + BaselineFrame::FramePointerOffset),
        scratch);
    masm.makeFrameDescriptor(scratch, FrameType::BaselineJS,
                             ExitFrameLayout::Size());
    masm.push(scratch);  // Fake return address.
    masm.push(Imm32(0));
    // No GC things to mark on the stack, push a bare token.
    masm.loadJSContext(scratch);
    masm.enterFakeExitFrame(scratch, scratch, ExitFrameType::Bare);

    masm.push(framePtr);
    masm.push(jitcode);

    using Fn = bool (*)(BaselineFrame * frame, InterpreterFrame * interpFrame,
                        uint32_t numStackValues);
    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(framePtr);     // BaselineFrame
    masm.passABIArg(OsrFrameReg);  // InterpreterFrame
    masm.passABIArg(numStackValues);
    masm.callWithABI<Fn, jit::InitBaselineFrameForOsr>(
        MoveOp::GENERAL, CheckUnsafeCallWithABI::DontCheckHasExitFrame);

    masm.pop(jitcode);
    masm.pop(framePtr);

    MOZ_ASSERT(jitcode != ReturnReg);

    Label error;
    masm.addPtr(Imm32(ExitFrameLayout::SizeWithFooter()), esp);
    masm.addPtr(Imm32(BaselineFrame::Size()), framePtr);
    masm.branchIfFalseBool(ReturnReg, &error);

    // If OSR-ing, then emit instrumentation for setting lastProfilerFrame
    // if profiler instrumentation is enabled.
    {
      Label skipProfilingInstrumentation;
      Register realFramePtr = numStackValues;
      AbsoluteAddress addressOfEnabled(
          cx->runtime()->geckoProfiler().addressOfEnabled());
      masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0),
                    &skipProfilingInstrumentation);
      masm.lea(Operand(framePtr, sizeof(void*)), realFramePtr);
      masm.profilerEnterFrame(realFramePtr, scratch);
      masm.bind(&skipProfilingInstrumentation);
    }

    masm.jump(jitcode);

    // OOM: load error value, discard return address and previous frame
    // pointer and return.
    masm.bind(&error);
    masm.mov(framePtr, esp);
    masm.addPtr(Imm32(2 * sizeof(uintptr_t)), esp);
    masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    masm.mov(&oomReturnLabel, scratch);
    masm.jump(scratch);

    masm.bind(&notOsr);
    masm.loadPtr(Address(ebp, ARG_SCOPECHAIN), R1.scratchReg());
  }

  // The call will push the return address on the stack, thus we check that
  // the stack would be aligned once the call is complete.
  masm.assertStackAlignment(JitStackAlignment, sizeof(uintptr_t));

  /***************************************************************
      Call passed-in code, get return value and fill in the
      passed in return value pointer
  ***************************************************************/
  masm.call(Address(ebp, ARG_JITCODE));

  {
    // Interpreter -> Baseline OSR will return here.
    masm.bind(&returnLabel);
    masm.addCodeLabel(returnLabel);
    masm.bind(&oomReturnLabel);
    masm.addCodeLabel(oomReturnLabel);
  }

  // Pop arguments off the stack.
  // eax <- 8*argc (size of all arguments we pushed on the stack)
  masm.pop(eax);
  masm.shrl(Imm32(FRAMESIZE_SHIFT), eax);  // Unmark EntryFrame.
  masm.pop(ebx);                           // Discard calleeToken.
  masm.pop(ebx);                           // Discard numActualArgs.
  masm.addl(eax, esp);

  // |ebp| could have been clobbered by the inner function.
  // Grab the address for the Value result from the argument stack.
  //  +20 ... arguments ...
  //  +16 <return>
  //  +12 ebp <- original %ebp pointing here.
  //  +8  ebx
  //  +4  esi
  //  +0  edi
  masm.loadPtr(Address(esp, ARG_RESULT + 3 * sizeof(void*)), eax);
  masm.storeValue(JSReturnOperand, Operand(eax, 0));

  /**************************************************************
      Return stack and registers to correct state
  **************************************************************/

  // Restore non-volatile registers
  masm.pop(edi);
  masm.pop(esi);
  masm.pop(ebx);

  // Restore old stack frame pointer
  masm.pop(ebp);
  masm.ret();
}

// static
mozilla::Maybe<::JS::ProfilingFrameIterator::RegisterState>
JitRuntime::getCppEntryRegisters(JitFrameLayout* frameStackAddress) {
  // Not supported, or not implemented yet.
  // TODO: Implement along with the corresponding stack-walker changes, in
  // coordination with the Gecko Profiler, see bug 1635987 and follow-ups.
  return mozilla::Nothing{};
}

// Push AllRegs in a way that is compatible with RegisterDump, regardless of
// what PushRegsInMask might do to reduce the set size.
static void DumpAllRegs(MacroAssembler& masm) {
#ifdef ENABLE_WASM_SIMD
  masm.PushRegsInMask(AllRegs);
#else
  // When SIMD isn't supported, PushRegsInMask reduces the set of float
  // registers to be double-sized, while the RegisterDump expects each of
  // the float registers to have the maximal possible size
  // (Simd128DataSize). To work around this, we just spill the double
  // registers by hand here, using the register dump offset directly.
  for (GeneralRegisterBackwardIterator iter(AllRegs.gprs()); iter.more();
       ++iter) {
    masm.Push(*iter);
  }

  masm.reserveStack(sizeof(RegisterDump::FPUArray));
  for (FloatRegisterBackwardIterator iter(AllRegs.fpus()); iter.more();
       ++iter) {
    FloatRegister reg = *iter;
    Address spillAddress(StackPointer, reg.getRegisterDumpOffsetInBytes());
    masm.storeDouble(reg, spillAddress);
  }
#endif
}

void JitRuntime::generateInvalidator(MacroAssembler& masm, Label* bailoutTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateInvalidator");

  invalidatorOffset_ = startTrampolineCode(masm);

  // We do the minimum amount of work in assembly and shunt the rest
  // off to InvalidationBailout. Assembly does:
  //
  // - Push the machine state onto the stack.
  // - Call the InvalidationBailout routine with the stack pointer.
  // - Now that the frame has been bailed out, convert the invalidated
  //   frame into an exit frame.
  // - Do the normal check-return-code-and-thunk-to-the-interpreter dance.

  // Push registers such that we can access them from [base + code].
  DumpAllRegs(masm);

  masm.movl(esp, eax);  // Argument to jit::InvalidationBailout.

  // Make space for InvalidationBailout's frameSize outparam.
  masm.reserveStack(sizeof(size_t));
  masm.movl(esp, ebx);

  // Make space for InvalidationBailout's bailoutInfo outparam.
  masm.reserveStack(sizeof(void*));
  masm.movl(esp, ecx);

  using Fn = bool (*)(InvalidationBailoutStack * sp, size_t * frameSizeOut,
                      BaselineBailoutInfo * *info);
  masm.setupUnalignedABICall(edx);
  masm.passABIArg(eax);
  masm.passABIArg(ebx);
  masm.passABIArg(ecx);
  masm.callWithABI<Fn, InvalidationBailout>(
      MoveOp::GENERAL, CheckUnsafeCallWithABI::DontCheckOther);

  masm.pop(ecx);  // Get bailoutInfo outparam.
  masm.pop(ebx);  // Get the frameSize outparam.

  // Pop the machine state and the dead frame.
  masm.lea(Operand(esp, ebx, TimesOne, sizeof(InvalidationBailoutStack)), esp);

  // Jump to shared bailout tail. The BailoutInfo pointer has to be in ecx.
  masm.jmp(bailoutTail);
}

void JitRuntime::generateArgumentsRectifier(MacroAssembler& masm,
                                            ArgumentsRectifierKind kind) {
  AutoCreatedBy acb(masm, "JitRuntime::generateArgumentsRectifier");

  switch (kind) {
    case ArgumentsRectifierKind::Normal:
      argumentsRectifierOffset_ = startTrampolineCode(masm);
      break;
    case ArgumentsRectifierKind::TrialInlining:
      trialInliningArgumentsRectifierOffset_ = startTrampolineCode(masm);
      break;
  }

  // Caller:
  // [arg2] [arg1] [this] [ [argc] [callee] [descr] [raddr] ] <- esp

  // Load argc.
  masm.loadPtr(Address(esp, RectifierFrameLayout::offsetOfNumActualArgs()),
               esi);

  // Load the number of |undefined|s to push into %ecx.
  masm.loadPtr(Address(esp, RectifierFrameLayout::offsetOfCalleeToken()), eax);
  masm.mov(eax, ecx);
  masm.andl(Imm32(CalleeTokenMask), ecx);
  masm.mov(Operand(ecx, JSFunction::offsetOfFlagsAndArgCount()), ecx);
  masm.rshift32(Imm32(JSFunction::ArgCountShift), ecx);

  // The frame pointer and its padding are pushed on the stack.
  // Including |this|, there are (|nformals| + 1) arguments to push to the
  // stack.  Then we push a JitFrameLayout.  We compute the padding expressed
  // in the number of extra |undefined| values to push on the stack.
  static_assert(
      sizeof(JitFrameLayout) % JitStackAlignment == 0,
      "No need to consider the JitFrameLayout for aligning the stack");
  static_assert((sizeof(Value) + 2 * sizeof(void*)) % JitStackAlignment == 0,
                "No need to consider |this| and the frame pointer and its "
                "padding for aligning the stack");
  static_assert(
      JitStackAlignment % sizeof(Value) == 0,
      "Ensure that we can pad the stack by pushing extra UndefinedValue");
  static_assert(IsPowerOfTwo(JitStackValueAlignment),
                "must have power of two for masm.andl to do its job");

  masm.addl(Imm32(JitStackValueAlignment - 1 /* for padding */), ecx);

  // Account for newTarget, if necessary.
  static_assert(
      CalleeToken_FunctionConstructing == 1,
      "Ensure that we can use the constructing bit to count an extra push");
  masm.mov(eax, edx);
  masm.andl(Imm32(CalleeToken_FunctionConstructing), edx);
  masm.addl(edx, ecx);

  masm.andl(Imm32(~(JitStackValueAlignment - 1)), ecx);
  masm.subl(esi, ecx);

  // Copy the number of actual arguments into edx.
  masm.mov(esi, edx);

  masm.moveValue(UndefinedValue(), ValueOperand(ebx, edi));

  // NOTE: The fact that x86 ArgumentsRectifier saves the FramePointer
  // is relied upon by the baseline bailout code.  If this changes,
  // fix that code!  See the |#if defined(JS_CODEGEN_X86) portions of
  // BaselineStackBuilder::calculatePrevFramePtr and
  // BaselineStackBuilder::buildRectifierFrame (in BaselineBailouts.cpp).
  masm.push(FramePointer);
  masm.movl(esp, FramePointer);  // Save %esp.
  masm.push(FramePointer /* padding */);

  // Caller:
  // [arg2] [arg1] [this] [ [argc] [callee] [descr] [raddr] ]
  // '-- #esi ---'
  //
  // Rectifier frame:
  // [ebp'] <- ebp [padding] <- esp [undef] [undef] [arg2] [arg1] [this]
  //                                '--- #ecx ----' '-- #esi ---'
  //
  // [ [argc] [callee] [descr] [raddr] ]

  // Push undefined.
  {
    Label undefLoopTop;
    masm.bind(&undefLoopTop);

    masm.push(ebx);  // type(undefined);
    masm.push(edi);  // payload(undefined);
    masm.subl(Imm32(1), ecx);
    masm.j(Assembler::NonZero, &undefLoopTop);
  }

  // Get the topmost argument. We did a push of %ebp earlier, so be sure to
  // account for this in the offset
  BaseIndex b(FramePointer, esi, TimesEight,
              sizeof(RectifierFrameLayout) + sizeof(void*));
  masm.lea(Operand(b), ecx);

  // Push arguments, |nargs| + 1 times (to include |this|).
  masm.addl(Imm32(1), esi);
  {
    Label copyLoopTop;

    masm.bind(&copyLoopTop);
    masm.push(Operand(ecx, sizeof(Value) / 2));
    masm.push(Operand(ecx, 0x0));
    masm.subl(Imm32(sizeof(Value)), ecx);
    masm.subl(Imm32(1), esi);
    masm.j(Assembler::NonZero, &copyLoopTop);
  }

  {
    Label notConstructing;

    masm.mov(eax, ebx);
    masm.branchTest32(Assembler::Zero, ebx,
                      Imm32(CalleeToken_FunctionConstructing),
                      &notConstructing);

    BaseValueIndex src(
        FramePointer, edx,
        sizeof(RectifierFrameLayout) + sizeof(Value) + sizeof(void*));

    masm.andl(Imm32(CalleeTokenMask), ebx);
    masm.movl(Operand(ebx, JSFunction::offsetOfFlagsAndArgCount()), ebx);
    masm.rshift32(Imm32(JSFunction::ArgCountShift), ebx);

    BaseValueIndex dst(esp, ebx, sizeof(Value));

    ValueOperand newTarget(ecx, edi);

    masm.loadValue(src, newTarget);
    masm.storeValue(newTarget, dst);

    masm.bind(&notConstructing);
  }

  // Construct descriptor, accounting for pushed frame pointer above
  masm.lea(Operand(FramePointer, sizeof(void*)), ebx);
  masm.subl(esp, ebx);
  masm.makeFrameDescriptor(ebx, FrameType::Rectifier, JitFrameLayout::Size());

  // Construct JitFrameLayout.
  masm.push(edx);  // number of actual arguments
  masm.push(eax);  // callee token
  masm.push(ebx);  // descriptor

  // Call the target function.
  masm.andl(Imm32(CalleeTokenMask), eax);
  switch (kind) {
    case ArgumentsRectifierKind::Normal:
      masm.loadJitCodeRaw(eax, eax);
      argumentsRectifierReturnOffset_ = masm.callJitNoProfiler(eax);
      break;
    case ArgumentsRectifierKind::TrialInlining:
      Label noBaselineScript, done;
      masm.loadBaselineJitCodeRaw(eax, ebx, &noBaselineScript);
      masm.callJitNoProfiler(ebx);
      masm.jump(&done);

      // See BaselineCacheIRCompiler::emitCallInlinedFunction.
      masm.bind(&noBaselineScript);
      masm.loadJitCodeRaw(eax, eax);
      masm.callJitNoProfiler(eax);
      masm.bind(&done);
      break;
  }

  // Remove the rectifier frame.
  masm.pop(ebx);                           // ebx <- descriptor with FrameType.
  masm.shrl(Imm32(FRAMESIZE_SHIFT), ebx);  // ebx <- descriptor.
  masm.pop(edi);                           // Discard calleeToken.
  masm.pop(edi);  // Discard number of actual arguments.

  // Discard pushed arguments, but not the pushed frame pointer.
  BaseIndex unwind(esp, ebx, TimesOne, -int32_t(sizeof(void*)));
  masm.lea(Operand(unwind), esp);

  masm.pop(FramePointer);
  masm.ret();
}

static void PushBailoutFrame(MacroAssembler& masm, uint32_t frameClass,
                             Register spArg) {
  // Push registers such that we can access them from [base + code].
  DumpAllRegs(masm);

  // Push the bailout table number.
  masm.push(Imm32(frameClass));

  // The current stack pointer is the first argument to jit::Bailout.
  masm.movl(esp, spArg);
}

static void GenerateBailoutThunk(MacroAssembler& masm, uint32_t frameClass,
                                 Label* bailoutTail) {
  PushBailoutFrame(masm, frameClass, eax);

  // Make space for Bailout's baioutInfo outparam.
  masm.reserveStack(sizeof(void*));
  masm.movl(esp, ebx);

  // Call the bailout function. This will correct the size of the bailout.
  using Fn = bool (*)(BailoutStack * sp, BaselineBailoutInfo * *info);
  masm.setupUnalignedABICall(ecx);
  masm.passABIArg(eax);
  masm.passABIArg(ebx);
  masm.callWithABI<Fn, Bailout>(MoveOp::GENERAL,
                                CheckUnsafeCallWithABI::DontCheckOther);

  masm.pop(ecx);  // Get bailoutInfo outparam.

  // Common size of stuff we've pushed.
  static const uint32_t BailoutDataSize = 0 + sizeof(void*)  // frameClass
                                          + sizeof(RegisterDump);

  // Remove both the bailout frame and the topmost Ion frame's stack.
  if (frameClass == NO_FRAME_SIZE_CLASS_ID) {
    // We want the frameSize. Stack is:
    //    ... frame ...
    //    snapshotOffset
    //    frameSize
    //    ... bailoutFrame ...
    masm.addl(Imm32(BailoutDataSize), esp);
    masm.pop(ebx);
    masm.addl(Imm32(sizeof(uint32_t)), esp);
    masm.addl(ebx, esp);
  } else {
    // Stack is:
    //    ... frame ...
    //    bailoutId
    //    ... bailoutFrame ...
    uint32_t frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
    masm.addl(Imm32(BailoutDataSize + sizeof(void*) + frameSize), esp);
  }

  // Jump to shared bailout tail. The BailoutInfo pointer has to be in ecx.
  masm.jmp(bailoutTail);
}

JitRuntime::BailoutTable JitRuntime::generateBailoutTable(MacroAssembler& masm,
                                                          Label* bailoutTail,
                                                          uint32_t frameClass) {
  AutoCreatedBy acb(masm, "JitRuntime::generateBailoutTable");

  uint32_t offset = startTrampolineCode(masm);

  Label bailout;
  for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++) {
    masm.call(&bailout);
  }
  masm.bind(&bailout);

  GenerateBailoutThunk(masm, frameClass, bailoutTail);

  return BailoutTable(offset, masm.currentOffset() - offset);
}

void JitRuntime::generateBailoutHandler(MacroAssembler& masm,
                                        Label* bailoutTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateBailoutHandler");

  bailoutHandlerOffset_ = startTrampolineCode(masm);

  GenerateBailoutThunk(masm, NO_FRAME_SIZE_CLASS_ID, bailoutTail);
}

bool JitRuntime::generateVMWrapper(JSContext* cx, MacroAssembler& masm,
                                   const VMFunctionData& f, DynFn nativeFun,
                                   uint32_t* wrapperOffset) {
  AutoCreatedBy acb(masm, "JitRuntime::generateVMWrapper");

  *wrapperOffset = startTrampolineCode(masm);

  // Avoid conflicts with argument registers while discarding the result after
  // the function call.
  AllocatableGeneralRegisterSet regs(Register::Codes::WrapperMask);

  static_assert(
      (Register::Codes::VolatileMask & ~Register::Codes::WrapperMask) == 0,
      "Wrapper register set must be a superset of Volatile register set.");

  // The context is the first argument.
  Register cxreg = regs.takeAny();

  // Stack is:
  //    ... frame ...
  //  +8  [args]
  //  +4  descriptor
  //  +0  returnAddress
  //
  // We're aligned to an exit frame, so link it up.
  masm.loadJSContext(cxreg);
  masm.enterExitFrame(cxreg, regs.getAny(), &f);

  // Save the current stack pointer as the base for copying arguments.
  Register argsBase = InvalidReg;
  if (f.explicitArgs) {
    argsBase = regs.takeAny();
    masm.lea(Operand(esp, ExitFrameLayout::SizeWithFooter()), argsBase);
  }

  // Reserve space for the outparameter.
  Register outReg = InvalidReg;
  switch (f.outParam) {
    case Type_Value:
      outReg = regs.takeAny();
      masm.Push(UndefinedValue());
      masm.movl(esp, outReg);
      break;

    case Type_Handle:
      outReg = regs.takeAny();
      masm.PushEmptyRooted(f.outParamRootType);
      masm.movl(esp, outReg);
      break;

    case Type_Int32:
    case Type_Pointer:
    case Type_Bool:
      outReg = regs.takeAny();
      masm.reserveStack(sizeof(int32_t));
      masm.movl(esp, outReg);
      break;

    case Type_Double:
      outReg = regs.takeAny();
      masm.reserveStack(sizeof(double));
      masm.movl(esp, outReg);
      break;

    default:
      MOZ_ASSERT(f.outParam == Type_Void);
      break;
  }

  if (!generateTLEnterVM(masm, f)) {
    return false;
  }

  masm.setupUnalignedABICall(regs.getAny());
  masm.passABIArg(cxreg);

  size_t argDisp = 0;

  // Copy arguments.
  for (uint32_t explicitArg = 0; explicitArg < f.explicitArgs; explicitArg++) {
    switch (f.argProperties(explicitArg)) {
      case VMFunctionData::WordByValue:
        masm.passABIArg(MoveOperand(argsBase, argDisp), MoveOp::GENERAL);
        argDisp += sizeof(void*);
        break;
      case VMFunctionData::DoubleByValue:
        // We don't pass doubles in float registers on x86, so no need
        // to check for argPassedInFloatReg.
        masm.passABIArg(MoveOperand(argsBase, argDisp), MoveOp::GENERAL);
        argDisp += sizeof(void*);
        masm.passABIArg(MoveOperand(argsBase, argDisp), MoveOp::GENERAL);
        argDisp += sizeof(void*);
        break;
      case VMFunctionData::WordByRef:
        masm.passABIArg(
            MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE_ADDRESS),
            MoveOp::GENERAL);
        argDisp += sizeof(void*);
        break;
      case VMFunctionData::DoubleByRef:
        masm.passABIArg(
            MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE_ADDRESS),
            MoveOp::GENERAL);
        argDisp += 2 * sizeof(void*);
        break;
    }
  }

  // Copy the implicit outparam, if any.
  if (outReg != InvalidReg) {
    masm.passABIArg(outReg);
  }

  masm.callWithABI(nativeFun, MoveOp::GENERAL,
                   CheckUnsafeCallWithABI::DontCheckHasExitFrame);

  if (!generateTLExitVM(masm, f)) {
    return false;
  }

  // Test for failure.
  switch (f.failType()) {
    case Type_Cell:
      masm.branchTestPtr(Assembler::Zero, eax, eax, masm.failureLabel());
      break;
    case Type_Bool:
      masm.testb(eax, eax);
      masm.j(Assembler::Zero, masm.failureLabel());
      break;
    case Type_Void:
      break;
    default:
      MOZ_CRASH("unknown failure kind");
  }

  // Load the outparam and free any allocated stack.
  switch (f.outParam) {
    case Type_Handle:
      masm.popRooted(f.outParamRootType, ReturnReg, JSReturnOperand);
      break;

    case Type_Value:
      masm.Pop(JSReturnOperand);
      break;

    case Type_Int32:
    case Type_Pointer:
      masm.Pop(ReturnReg);
      break;

    case Type_Bool:
      masm.Pop(ReturnReg);
      masm.movzbl(ReturnReg, ReturnReg);
      break;

    case Type_Double:
      if (JitOptions.supportsFloatingPoint) {
        masm.Pop(ReturnDoubleReg);
      } else {
        masm.assumeUnreachable(
            "Unable to pop to float reg, with no FP support.");
      }
      break;

    default:
      MOZ_ASSERT(f.outParam == Type_Void);
      break;
  }

  // Until C++ code is instrumented against Spectre, prevent speculative
  // execution from returning any private data.
  if (f.returnsData() && JitOptions.spectreJitToCxxCalls) {
    masm.speculationBarrier();
  }

  masm.leaveExitFrame();
  masm.retn(Imm32(sizeof(ExitFrameLayout) +
                  f.explicitStackSlots() * sizeof(void*) +
                  f.extraValuesToPop * sizeof(Value)));

  return true;
}

uint32_t JitRuntime::generatePreBarrier(JSContext* cx, MacroAssembler& masm,
                                        MIRType type) {
  AutoCreatedBy acb(masm, "JitRuntime::generatePreBarrier");

  uint32_t offset = startTrampolineCode(masm);

  static_assert(PreBarrierReg == edx);
  Register temp1 = eax;
  Register temp2 = ebx;
  Register temp3 = ecx;
  masm.push(temp1);
  masm.push(temp2);
  masm.push(temp3);

  Label noBarrier;
  masm.emitPreBarrierFastPath(cx->runtime(), type, temp1, temp2, temp3,
                              &noBarrier);

  // Call into C++ to mark this GC thing.
  masm.pop(temp3);
  masm.pop(temp2);
  masm.pop(temp1);

  LiveRegisterSet save;
  if (JitOptions.supportsFloatingPoint) {
    save.set() = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                             FloatRegisterSet(FloatRegisters::VolatileMask));
  } else {
    save.set() = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                             FloatRegisterSet());
  }
  masm.PushRegsInMask(save);

  masm.movl(ImmPtr(cx->runtime()), ecx);

  masm.setupUnalignedABICall(eax);
  masm.passABIArg(ecx);
  masm.passABIArg(edx);
  masm.callWithABI(JitPreWriteBarrier(type));

  masm.PopRegsInMask(save);
  masm.ret();

  masm.bind(&noBarrier);
  masm.pop(temp3);
  masm.pop(temp2);
  masm.pop(temp1);
  masm.ret();

  return offset;
}

void JitRuntime::generateExceptionTailStub(MacroAssembler& masm,
                                           Label* profilerExitTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateExceptionTailStub");

  exceptionTailOffset_ = startTrampolineCode(masm);

  masm.bind(masm.failureLabel());
  masm.handleFailureWithHandlerTail(profilerExitTail);
}

void JitRuntime::generateBailoutTailStub(MacroAssembler& masm,
                                         Label* bailoutTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateBailoutTailStub");

  bailoutTailOffset_ = startTrampolineCode(masm);
  masm.bind(bailoutTail);

  masm.generateBailoutTail(edx, ecx);
}

void JitRuntime::generateProfilerExitFrameTailStub(MacroAssembler& masm,
                                                   Label* profilerExitTail) {
  AutoCreatedBy acb(masm, "JitRuntime::generateProfilerExitFrameTailStub");

  profilerExitFrameTailOffset_ = startTrampolineCode(masm);
  masm.bind(profilerExitTail);

  Register scratch1 = eax;
  Register scratch2 = ebx;
  Register scratch3 = esi;
  Register scratch4 = edi;

  //
  // The code generated below expects that the current stack pointer points
  // to an Ion or Baseline frame, at the state it would be immediately
  // before a ret().  Thus, after this stub's business is done, it executes
  // a ret() and returns directly to the caller script, on behalf of the
  // callee script that jumped to this code.
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
  // the return address into that frame.  The frame could either be an
  // immediate "caller" frame, or it could be a frame in a previous
  // JitActivation (if the current frame was entered from C++, and the C++
  // was entered by some caller jit-frame further down the stack).
  //
  // So this jitcode is responsible for "walking up" the jit stack, finding
  // the previous Ion or Baseline JS frame, and storing its address and the
  // return address into the appropriate fields on the current jitActivation.
  //
  // There are a fixed number of different path types that can lead to the
  // current frame, which is either a baseline or ion frame:
  //
  // <Baseline-Or-Ion>
  // ^
  // |
  // ^--- Ion
  // |
  // ^--- Baseline Stub <---- Baseline
  // |
  // ^--- Argument Rectifier
  // |    ^
  // |    |
  // |    ^--- Ion
  // |    |
  // |    ^--- Baseline Stub <---- Baseline
  // |
  // ^--- Entry Frame (From C++)
  //
  Register actReg = scratch4;
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
    masm.loadPtr(lastProfilingFrame, scratch1);
    Label checkOk;
    masm.branchPtr(Assembler::Equal, scratch1, ImmWord(0), &checkOk);
    masm.branchPtr(Assembler::Equal, StackPointer, scratch1, &checkOk);
    masm.assumeUnreachable(
        "Mismatch between stored lastProfilingFrame and current stack "
        "pointer.");
    masm.bind(&checkOk);
  }
#endif

  // Load the frame descriptor into |scratch1|, figure out what to do
  // depending on its type.
  masm.loadPtr(Address(StackPointer, JitFrameLayout::offsetOfDescriptor()),
               scratch1);

  // Going into the conditionals, we will have:
  //      FrameDescriptor.size in scratch1
  //      FrameDescriptor.type in scratch2
  masm.movePtr(scratch1, scratch2);
  masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), scratch1);
  masm.and32(Imm32((1 << FRAMETYPE_BITS) - 1), scratch2);

  // Handling of each case is dependent on FrameDescriptor.type
  Label handle_IonJS;
  Label handle_BaselineStub;
  Label handle_Rectifier;
  Label handle_IonICCall;
  Label handle_Entry;
  Label end;

  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::IonJS),
                &handle_IonJS);
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::BaselineJS),
                &handle_IonJS);
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::BaselineStub),
                &handle_BaselineStub);
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::Rectifier),
                &handle_Rectifier);
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::IonICCall),
                &handle_IonICCall);
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::CppToJSJit),
                &handle_Entry);

  // The WasmToJSJit is just another kind of entry.
  masm.branch32(Assembler::Equal, scratch2, Imm32(FrameType::WasmToJSJit),
                &handle_Entry);

  masm.assumeUnreachable(
      "Invalid caller frame type when exiting from Ion frame.");

  //
  // FrameType::IonJS
  //
  // Stack layout:
  //                  ...
  //                  Ion-Descriptor
  //     Prev-FP ---> Ion-ReturnAddr
  //                  ... previous frame data ... |- Descriptor.Size
  //                  ... arguments ...           |
  //                  ActualArgc          |
  //                  CalleeToken         |- JitFrameLayout::Size()
  //                  Descriptor          |
  //        FP -----> ReturnAddr          |
  //
  masm.bind(&handle_IonJS);
  {
    // |scratch1| contains Descriptor.size

    // returning directly to an IonJS frame.  Store return addr to frame
    // in lastProfilingCallSite.
    masm.loadPtr(Address(StackPointer, JitFrameLayout::offsetOfReturnAddress()),
                 scratch2);
    masm.storePtr(scratch2, lastProfilingCallSite);

    // Store return frame in lastProfilingFrame.
    // scratch2 := StackPointer + Descriptor.size*1 + JitFrameLayout::Size();
    masm.lea(Operand(StackPointer, scratch1, TimesOne, JitFrameLayout::Size()),
             scratch2);
    masm.storePtr(scratch2, lastProfilingFrame);
    masm.ret();
  }

  //
  // FrameType::BaselineStub
  //
  // Look past the stub and store the frame pointer to
  // the baselineJS frame prior to it.
  //
  // Stack layout:
  //              ...
  //              BL-Descriptor
  // Prev-FP ---> BL-ReturnAddr
  //      +-----> BL-PrevFramePointer
  //      |       ... BL-FrameData ...
  //      |       BLStub-Descriptor
  //      |       BLStub-ReturnAddr
  //      |       BLStub-StubPointer          |
  //      +------ BLStub-SavedFramePointer    |- Descriptor.Size
  //              ... arguments ...           |
  //              ActualArgc          |
  //              CalleeToken         |- JitFrameLayout::Size()
  //              Descriptor          |
  //    FP -----> ReturnAddr          |
  //
  // We take advantage of the fact that the stub frame saves the frame
  // pointer pointing to the baseline frame, so a bunch of calculation can
  // be avoided.
  //
  masm.bind(&handle_BaselineStub);
  {
    BaseIndex stubFrameReturnAddr(
        StackPointer, scratch1, TimesOne,
        JitFrameLayout::Size() +
            BaselineStubFrameLayout::offsetOfReturnAddress());
    masm.loadPtr(stubFrameReturnAddr, scratch2);
    masm.storePtr(scratch2, lastProfilingCallSite);

    BaseIndex stubFrameSavedFramePtr(
        StackPointer, scratch1, TimesOne,
        JitFrameLayout::Size() - (2 * sizeof(void*)));
    masm.loadPtr(stubFrameSavedFramePtr, scratch2);
    masm.addPtr(Imm32(sizeof(void*)), scratch2);  // Skip past BL-PrevFramePtr
    masm.storePtr(scratch2, lastProfilingFrame);
    masm.ret();
  }

  //
  // FrameType::Rectifier
  //
  // The rectifier frame can be preceded by either an IonJS, a BaselineStub,
  // or a CppToJSJit/WasmToJSJit frame.
  //
  // Stack layout if caller of rectifier was Ion or CppToJSJit/WasmToJSJit:
  //
  //              Ion-Descriptor
  //              Ion-ReturnAddr
  //              ... ion frame data ... |- Rect-Descriptor.Size
  //              < COMMON LAYOUT >
  //
  // Stack layout if caller of rectifier was Baseline:
  //
  //              BL-Descriptor
  // Prev-FP ---> BL-ReturnAddr
  //      +-----> BL-SavedFramePointer
  //      |       ... baseline frame data ...
  //      |       BLStub-Descriptor
  //      |       BLStub-ReturnAddr
  //      |       BLStub-StubPointer          |
  //      +------ BLStub-SavedFramePointer    |- Rect-Descriptor.Size
  //              ... args to rectifier ...   |
  //              < COMMON LAYOUT >
  //
  // Common stack layout:
  //
  //              ActualArgc          |
  //              CalleeToken         |- IonRectitiferFrameLayout::Size()
  //              Rect-Descriptor     |
  //              Rect-ReturnAddr     |
  //              ... rectifier data & args ... |- Descriptor.Size
  //              ActualArgc      |
  //              CalleeToken     |- JitFrameLayout::Size()
  //              Descriptor      |
  //    FP -----> ReturnAddr      |
  //
  masm.bind(&handle_Rectifier);
  {
    // scratch2 := StackPointer + Descriptor.size + JitFrameLayout::Size()
    masm.lea(Operand(StackPointer, scratch1, TimesOne, JitFrameLayout::Size()),
             scratch2);
    masm.loadPtr(Address(scratch2, RectifierFrameLayout::offsetOfDescriptor()),
                 scratch3);
    masm.movePtr(scratch3, scratch1);
    masm.and32(Imm32((1 << FRAMETYPE_BITS) - 1), scratch3);
    masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), scratch1);

    // Now |scratch1| contains Rect-Descriptor.Size
    // and |scratch2| points to Rectifier frame
    // and |scratch3| contains Rect-Descriptor.Type

    masm.assertRectifierFrameParentType(scratch3);

    // Check for either Ion or BaselineStub frame.
    Label notIonFrame;
    masm.branch32(Assembler::NotEqual, scratch3, Imm32(FrameType::IonJS),
                  &notIonFrame);

    // Handle Rectifier <- IonJS
    // scratch3 := RectFrame[ReturnAddr]
    masm.loadPtr(
        Address(scratch2, RectifierFrameLayout::offsetOfReturnAddress()),
        scratch3);
    masm.storePtr(scratch3, lastProfilingCallSite);

    // scratch3 := RectFrame + Rect-Descriptor.Size +
    //             RectifierFrameLayout::Size()
    masm.lea(
        Operand(scratch2, scratch1, TimesOne, RectifierFrameLayout::Size()),
        scratch3);
    masm.storePtr(scratch3, lastProfilingFrame);
    masm.ret();

    masm.bind(&notIonFrame);

    // Check for either BaselineStub or a CppToJSJit/WasmToJSJit entry
    // frame.
    masm.branch32(Assembler::NotEqual, scratch3, Imm32(FrameType::BaselineStub),
                  &handle_Entry);

    // Handle Rectifier <- BaselineStub <- BaselineJS
    BaseIndex stubFrameReturnAddr(
        scratch2, scratch1, TimesOne,
        RectifierFrameLayout::Size() +
            BaselineStubFrameLayout::offsetOfReturnAddress());
    masm.loadPtr(stubFrameReturnAddr, scratch3);
    masm.storePtr(scratch3, lastProfilingCallSite);

    BaseIndex stubFrameSavedFramePtr(
        scratch2, scratch1, TimesOne,
        RectifierFrameLayout::Size() - (2 * sizeof(void*)));
    masm.loadPtr(stubFrameSavedFramePtr, scratch3);
    masm.addPtr(Imm32(sizeof(void*)), scratch3);
    masm.storePtr(scratch3, lastProfilingFrame);
    masm.ret();
  }

  // FrameType::IonICCall
  //
  // The caller is always an IonJS frame.
  //
  //              Ion-Descriptor
  //              Ion-ReturnAddr
  //              ... ion frame data ... |- CallFrame-Descriptor.Size
  //              StubCode               |
  //              ICCallFrame-Descriptor |- IonICCallFrameLayout::Size()
  //              ICCallFrame-ReturnAddr |
  //              ... call frame data & args ... |- Descriptor.Size
  //              ActualArgc      |
  //              CalleeToken     |- JitFrameLayout::Size()
  //              Descriptor      |
  //    FP -----> ReturnAddr      |
  masm.bind(&handle_IonICCall);
  {
    // scratch2 := StackPointer + Descriptor.size + JitFrameLayout::Size()
    masm.lea(Operand(StackPointer, scratch1, TimesOne, JitFrameLayout::Size()),
             scratch2);

    // scratch3 := ICCallFrame-Descriptor.Size
    masm.loadPtr(Address(scratch2, IonICCallFrameLayout::offsetOfDescriptor()),
                 scratch3);
#ifdef DEBUG
    // Assert previous frame is an IonJS frame.
    masm.movePtr(scratch3, scratch1);
    masm.and32(Imm32((1 << FRAMETYPE_BITS) - 1), scratch1);
    {
      Label checkOk;
      masm.branch32(Assembler::Equal, scratch1, Imm32(FrameType::IonJS),
                    &checkOk);
      masm.assumeUnreachable("IonICCall frame must be preceded by IonJS frame");
      masm.bind(&checkOk);
    }
#endif
    masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), scratch3);

    // lastProfilingCallSite := ICCallFrame-ReturnAddr
    masm.loadPtr(
        Address(scratch2, IonICCallFrameLayout::offsetOfReturnAddress()),
        scratch1);
    masm.storePtr(scratch1, lastProfilingCallSite);

    // lastProfilingFrame := ICCallFrame + ICCallFrame-Descriptor.Size +
    //                       IonICCallFrameLayout::Size()
    masm.lea(
        Operand(scratch2, scratch3, TimesOne, IonICCallFrameLayout::Size()),
        scratch1);
    masm.storePtr(scratch1, lastProfilingFrame);
    masm.ret();
  }

  //
  // FrameType::CppToJSJit / FrameType::WasmToJSJit
  //
  // If at an entry frame, store null into both fields.
  // A fast-path wasm->jit transition frame is an entry frame from the point
  // of view of the JIT.
  //
  masm.bind(&handle_Entry);
  {
    masm.movePtr(ImmPtr(nullptr), scratch1);
    masm.storePtr(scratch1, lastProfilingCallSite);
    masm.storePtr(scratch1, lastProfilingFrame);
    masm.ret();
  }
}
