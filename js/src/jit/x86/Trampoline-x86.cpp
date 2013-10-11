/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"

#include "assembler/assembler/MacroAssembler.h"
#include "jit/Bailouts.h"
#include "jit/BaselineJIT.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonCompartment.h"
#include "jit/IonFrames.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"
#include "jit/x86/BaselineHelpers-x86.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::jit;

// All registers to save and restore. This includes the stack pointer, since we
// use the ability to reference register values on the stack by index.
static const RegisterSet AllRegs =
  RegisterSet(GeneralRegisterSet(Registers::AllMask),
              FloatRegisterSet(FloatRegisters::AllMask));

enum EnterJitEbpArgumentOffset {
    ARG_JITCODE         = 2 * sizeof(void *),
    ARG_ARGC            = 3 * sizeof(void *),
    ARG_ARGV            = 4 * sizeof(void *),
    ARG_STACKFRAME      = 5 * sizeof(void *),
    ARG_CALLEETOKEN     = 6 * sizeof(void *),
    ARG_SCOPECHAIN      = 7 * sizeof(void *),
    ARG_STACKVALUES     = 8 * sizeof(void *),
    ARG_RESULT          = 9 * sizeof(void *)
};

/*
 * Generates a trampoline for a C++ function with the EnterIonCode signature,
 * using the standard cdecl calling convention.
 */
IonCode *
IonRuntime::generateEnterJIT(JSContext *cx, EnterJitType type)
{
    MacroAssembler masm(cx);

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(ebp);
    masm.movl(esp, ebp);

    // Save non-volatile registers. These must be saved by the trampoline,
    // rather than the JIT'd code, because they are scanned by the conservative
    // scanner.
    masm.push(ebx);
    masm.push(esi);
    masm.push(edi);
    masm.movl(esp, esi);

    // eax <- 8*argc, eax is now the offset betwen argv and the last
    masm.loadPtr(Address(ebp, ARG_ARGC), eax);
    masm.shll(Imm32(3), eax);

    // We need to ensure that the stack is aligned on a 12-byte boundary, so
    // inside the JIT function the stack is 16-byte aligned. Our stack right
    // now might not be aligned on some platforms (win32, gcc) so we factor
    // this possibility in, and simulate what the new stack address would be.
    //   +argc * 8 for arguments
    //   +4 for pushing alignment
    //   +4 for pushing the callee token
    //   +4 for pushing the return address
    masm.movl(esp, ecx);
    masm.subl(eax, ecx);
    masm.subl(Imm32(12), ecx);

    // ecx = ecx & 15, holds alignment.
    masm.andl(Imm32(15), ecx);
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

        masm.cmpl(eax, ebx);
        masm.j(Assembler::BelowOrEqual, &footer);

        // eax -= 8  --move to previous argument
        masm.subl(Imm32(8), eax);

        // Push what eax points to on stack, a Value is 2 words
        masm.push(Operand(eax, 4));
        masm.push(Operand(eax, 0));

        masm.jmp(&header);
        masm.bind(&footer);
    }


    // Push the number of actual arguments.  |result| is used to store the
    // actual number of arguments without adding an extra argument to the enter
    // JIT.
    masm.mov(Operand(ebp, ARG_RESULT), eax);
    masm.unboxInt32(Address(eax, 0x0), eax);
    masm.push(eax);

    // Push the callee token.
    masm.push(Operand(ebp, ARG_CALLEETOKEN));

    // Load the StackFrame address into the OsrFrameReg.
    // This address is also used for setting the constructing bit on all paths.
    masm.loadPtr(Address(ebp, ARG_STACKFRAME), OsrFrameReg);

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    // Create a frame descriptor.
    masm.subl(esp, esi);
    masm.makeFrameDescriptor(esi, IonFrame_Entry);
    masm.push(esi);

    CodeLabel returnLabel;
    if (type == EnterJitBaseline) {
        // Handle OSR.
        GeneralRegisterSet regs(GeneralRegisterSet::All());
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

        // Push return address, previous frame pointer.
        masm.mov(returnLabel.dest(), scratch);
        masm.push(scratch);
        masm.push(ebp);

        // Reserve frame.
        Register framePtr = ebp;
        masm.subPtr(Imm32(BaselineFrame::Size()), esp);
        masm.mov(esp, framePtr);

#ifdef XP_WIN
        // Can't push large frames blindly on windows.  Touch frame memory incrementally.
        masm.mov(numStackValues, scratch);
        masm.shll(Imm32(3), scratch);
        masm.subPtr(scratch, framePtr);
        {
            masm.movePtr(esp, scratch);
            masm.subPtr(Imm32(WINDOWS_BIG_FRAME_TOUCH_INCREMENT), scratch);

            Label touchFrameLoop;
            Label touchFrameLoopEnd;
            masm.bind(&touchFrameLoop);
            masm.branchPtr(Assembler::Below, scratch, framePtr, &touchFrameLoopEnd);
            masm.store32(Imm32(0), Address(scratch, 0));
            masm.subPtr(Imm32(WINDOWS_BIG_FRAME_TOUCH_INCREMENT), scratch);
            masm.jump(&touchFrameLoop);
            masm.bind(&touchFrameLoopEnd);
        }
        masm.mov(esp, framePtr);
#endif

        // Reserve space for locals and stack values.
        masm.mov(numStackValues, scratch);
        masm.shll(Imm32(3), scratch);
        masm.subPtr(scratch, esp);

        // Enter exit frame.
        masm.addPtr(Imm32(BaselineFrame::Size() + BaselineFrame::FramePointerOffset), scratch);
        masm.makeFrameDescriptor(scratch, IonFrame_BaselineJS);
        masm.push(scratch);
        masm.push(Imm32(0)); // Fake return address.
        masm.enterFakeExitFrame();

        masm.push(framePtr);
        masm.push(jitcode);

        masm.setupUnalignedABICall(3, scratch);
        masm.passABIArg(framePtr); // BaselineFrame
        masm.passABIArg(OsrFrameReg); // StackFrame
        masm.passABIArg(numStackValues);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, jit::InitBaselineFrameForOsr));

        masm.pop(jitcode);
        masm.pop(framePtr);

        JS_ASSERT(jitcode != ReturnReg);

        Label error;
        masm.addPtr(Imm32(IonExitFrameLayout::SizeWithFooter()), esp);
        masm.addPtr(Imm32(BaselineFrame::Size()), framePtr);
        masm.branchIfFalseBool(ReturnReg, &error);

        masm.jump(jitcode);

        // OOM: load error value, discard return address and previous frame
        // pointer and return.
        masm.bind(&error);
        masm.mov(framePtr, esp);
        masm.addPtr(Imm32(2 * sizeof(uintptr_t)), esp);
        masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
        masm.mov(returnLabel.dest(), scratch);
        masm.jump(scratch);

        masm.bind(&notOsr);
        masm.loadPtr(Address(ebp, ARG_SCOPECHAIN), R1.scratchReg());
    }

    /***************************************************************
        Call passed-in code, get return value and fill in the
        passed in return value pointer
    ***************************************************************/
    masm.call(Operand(ebp, ARG_JITCODE));

    if (type == EnterJitBaseline) {
        // Baseline OSR will return here.
        masm.bind(returnLabel.src());
        if (!masm.addCodeLabel(returnLabel))
            return nullptr;
    }

    // Pop arguments off the stack.
    // eax <- 8*argc (size of all arguments we pushed on the stack)
    masm.pop(eax);
    masm.shrl(Imm32(FRAMESIZE_SHIFT), eax); // Unmark EntryFrame.
    masm.addl(eax, esp);

    // |ebp| could have been clobbered by the inner function.
    // Grab the address for the Value result from the argument stack.
    //  +18 ... arguments ...
    //  +14 <return>
    //  +10 ebp <- original %ebp pointing here.
    //  +8  ebx
    //  +4  esi
    //  +0  edi
    masm.loadPtr(Address(esp, ARG_RESULT + 3 * sizeof(void *)), eax);
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

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "EnterJIT");
#endif

    return code;
}

IonCode *
IonRuntime::generateInvalidator(JSContext *cx)
{
    AutoIonContextAlloc aica(cx);
    MacroAssembler masm(cx);

    // We do the minimum amount of work in assembly and shunt the rest
    // off to InvalidationBailout. Assembly does:
    //
    // - Pop the return address from the invalidation epilogue call.
    // - Push the machine state onto the stack.
    // - Call the InvalidationBailout routine with the stack pointer.
    // - Now that the frame has been bailed out, convert the invalidated
    //   frame into an exit frame.
    // - Do the normal check-return-code-and-thunk-to-the-interpreter dance.

    masm.addl(Imm32(sizeof(uintptr_t)), esp);

    // Push registers such that we can access them from [base + code].
    masm.PushRegsInMask(AllRegs);

    masm.movl(esp, eax); // Argument to jit::InvalidationBailout.

    // Make space for InvalidationBailout's frameSize outparam.
    masm.reserveStack(sizeof(size_t));
    masm.movl(esp, ebx);

    // Make space for InvalidationBailout's bailoutInfo outparam.
    masm.reserveStack(sizeof(void *));
    masm.movl(esp, ecx);

    masm.setupUnalignedABICall(3, edx);
    masm.passABIArg(eax);
    masm.passABIArg(ebx);
    masm.passABIArg(ecx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.pop(ecx); // Get bailoutInfo outparam.
    masm.pop(ebx); // Get the frameSize outparam.

    // Pop the machine state and the dead frame.
    masm.lea(Operand(esp, ebx, TimesOne, sizeof(InvalidationBailoutStack)), esp);

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in ecx.
    IonCode *bailoutTail = cx->runtime()->ionRuntime()->getBailoutTail();
    masm.jmp(bailoutTail);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);
    IonSpew(IonSpew_Invalidate, "   invalidation thunk created at %p", (void *) code->raw());

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "Invalidator");
#endif

    return code;
}

IonCode *
IonRuntime::generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut)
{
    MacroAssembler masm(cx);

    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == esi);

    // Load the number of |undefined|s to push into %ecx.
    masm.loadPtr(Address(esp, IonRectifierFrameLayout::offsetOfCalleeToken()), eax);
    masm.movzwl(Operand(eax, offsetof(JSFunction, nargs)), ecx);
    masm.subl(esi, ecx);

    // Copy the number of actual arguments.
    masm.loadPtr(Address(esp, IonRectifierFrameLayout::offsetOfNumActualArgs()), edx);

    masm.moveValue(UndefinedValue(), ebx, edi);

    // NOTE: The fact that x86 ArgumentsRectifier saves the FramePointer is relied upon
    // by the baseline bailout code.  If this changes, fix that code!  See
    // BaselineJIT.cpp/BaselineStackBuilder::calculatePrevFramePtr, and
    // BaselineJIT.cpp/InitFromBailout.  Check for the |#if defined(JS_CPU_X86)| portions.
    masm.push(FramePointer);
    masm.movl(esp, FramePointer); // Save %esp.

    // Push undefined.
    {
        Label undefLoopTop;
        masm.bind(&undefLoopTop);

        masm.push(ebx); // type(undefined);
        masm.push(edi); // payload(undefined);
        masm.subl(Imm32(1), ecx);
        masm.j(Assembler::NonZero, &undefLoopTop);
    }

    // Get the topmost argument. We did a push of %ebp earlier, so be sure to
    // account for this in the offset
    BaseIndex b = BaseIndex(FramePointer, esi, TimesEight,
                            sizeof(IonRectifierFrameLayout) + sizeof(void*));
    masm.lea(Operand(b), ecx);

    // Push arguments, |nargs| + 1 times (to include |this|).
    masm.addl(Imm32(1), esi);
    {
        Label copyLoopTop;

        masm.bind(&copyLoopTop);
        masm.push(Operand(ecx, sizeof(Value)/2));
        masm.push(Operand(ecx, 0x0));
        masm.subl(Imm32(sizeof(Value)), ecx);
        masm.subl(Imm32(1), esi);
        masm.j(Assembler::NonZero, &copyLoopTop);
    }

    // Construct descriptor, accounting for pushed frame pointer above
    masm.lea(Operand(FramePointer, sizeof(void*)), ebx);
    masm.subl(esp, ebx);
    masm.makeFrameDescriptor(ebx, IonFrame_Rectifier);

    // Construct IonJSFrameLayout.
    masm.push(edx); // number of actual arguments
    masm.push(eax); // callee token
    masm.push(ebx); // descriptor

    // Call the target function.
    // Note that this assumes the function is JITted.
    masm.loadPtr(Address(eax, JSFunction::offsetOfNativeOrScript()), eax);
    masm.loadBaselineOrIonRaw(eax, eax, mode, nullptr);
    masm.call(eax);
    uint32_t returnOffset = masm.currentOffset();

    // Remove the rectifier frame.
    masm.pop(ebx);            // ebx <- descriptor with FrameType.
    masm.shrl(Imm32(FRAMESIZE_SHIFT), ebx); // ebx <- descriptor.
    masm.pop(edi);            // Discard calleeToken.
    masm.pop(edi);            // Discard number of actual arguments.

    // Discard pushed arguments, but not the pushed frame pointer.
    BaseIndex unwind = BaseIndex(esp, ebx, TimesOne, -int32_t(sizeof(void*)));
    masm.lea(Operand(unwind), esp);

    masm.pop(FramePointer);
    masm.ret();

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "ArgumentsRectifier");
#endif

    CodeOffsetLabel returnLabel(returnOffset);
    returnLabel.fixup(&masm);
    if (returnAddrOut)
        *returnAddrOut = (void *) (code->raw() + returnLabel.offset());
    return code;
}

static void
GenerateBailoutThunk(JSContext *cx, MacroAssembler &masm, uint32_t frameClass)
{
    // Push registers such that we can access them from [base + code].
    masm.PushRegsInMask(AllRegs);

    // Push the bailout table number.
    masm.push(Imm32(frameClass));

    // The current stack pointer is the first argument to jit::Bailout.
    masm.movl(esp, eax);

    // Make space for Bailout's baioutInfo outparam.
    masm.reserveStack(sizeof(void *));
    masm.movl(esp, ebx);

    // Call the bailout function. This will correct the size of the bailout.
    masm.setupUnalignedABICall(2, ecx);
    masm.passABIArg(eax);
    masm.passABIArg(ebx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

    masm.pop(ecx); // Get bailoutInfo outparam.

    // Common size of stuff we've pushed.
    const uint32_t BailoutDataSize = sizeof(void *) + // frameClass
                                   sizeof(double) * FloatRegisters::Total +
                                   sizeof(void *) * Registers::Total;

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
        masm.addl(Imm32(BailoutDataSize + sizeof(void *) + frameSize), esp);
    }

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in ecx.
    IonCode *bailoutTail = cx->runtime()->ionRuntime()->getBailoutTail();
    masm.jmp(bailoutTail);
}

IonCode *
IonRuntime::generateBailoutTable(JSContext *cx, uint32_t frameClass)
{
    MacroAssembler masm;

    Label bailout;
    for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++)
        masm.call(&bailout);
    masm.bind(&bailout);

    GenerateBailoutThunk(cx, masm, frameClass);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutHandler");
#endif

    return code;
}

IonCode *
IonRuntime::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm;

    GenerateBailoutThunk(cx, masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutHandler");
#endif

    return code;
}

IonCode *
IonRuntime::generateVMWrapper(JSContext *cx, const VMFunction &f)
{
    typedef MoveResolver::MoveOperand MoveOperand;

    JS_ASSERT(!StackKeptAligned);
    JS_ASSERT(functionWrappers_);
    JS_ASSERT(functionWrappers_->initialized());
    VMWrapperMap::AddPtr p = functionWrappers_->lookupForAdd(&f);
    if (p)
        return p->value;

    // Generate a separated code for the wrapper.
    MacroAssembler masm;

    // Avoid conflicts with argument registers while discarding the result after
    // the function call.
    GeneralRegisterSet regs = GeneralRegisterSet(Register::Codes::WrapperMask);

    // Wrapper register set is a superset of Volatile register set.
    JS_STATIC_ASSERT((Register::Codes::VolatileMask & ~Register::Codes::WrapperMask) == 0);

    // The context is the first argument.
    Register cxreg = regs.takeAny();

    // Stack is:
    //    ... frame ...
    //  +8  [args]
    //  +4  descriptor
    //  +0  returnAddress
    //
    // We're aligned to an exit frame, so link it up.
    masm.enterExitFrameAndLoadContext(&f, cxreg, regs.getAny(), f.executionMode);

    // Save the current stack pointer as the base for copying arguments.
    Register argsBase = InvalidReg;
    if (f.explicitArgs) {
        argsBase = regs.takeAny();
        masm.lea(Operand(esp, IonExitFrameLayout::SizeWithFooter()), argsBase);
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
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }

    masm.setupUnalignedABICall(f.argc(), regs.getAny());
    masm.passABIArg(cxreg);

    size_t argDisp = 0;

    // Copy arguments.
    if (f.explicitArgs) {
        for (uint32_t explicitArg = 0; explicitArg < f.explicitArgs; explicitArg++) {
            MoveOperand from;
            switch (f.argProperties(explicitArg)) {
              case VMFunction::WordByValue:
                masm.passABIArg(MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByValue:
                // We don't pass doubles in float registers on x86, so no need
                // to check for argPassedInFloatReg.
                masm.passABIArg(MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                masm.passABIArg(MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                break;
              case VMFunction::WordByRef:
                masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByRef:
                masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += 2 * sizeof(void *);
                break;
            }
        }
    }

    // Copy the implicit outparam, if any.
    if (outReg != InvalidReg)
        masm.passABIArg(outReg);

    masm.callWithABI(f.wrapped);

    // Test for failure.
    switch (f.failType()) {
      case Type_Object:
        masm.branchTestPtr(Assembler::Zero, eax, eax, masm.failureLabel(f.executionMode));
        break;
      case Type_Bool:
        masm.testb(eax, eax);
        masm.j(Assembler::Zero, masm.failureLabel(f.executionMode));
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unknown failure kind");
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
        if (cx->runtime()->jitSupportsFloatingPoint)
            masm.Pop(ReturnFloatReg);
        else
            masm.breakpoint();
        break;

      default:
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }
    masm.leaveExitFrame();
    masm.retn(Imm32(sizeof(IonExitFrameLayout) +
                    f.explicitStackSlots() * sizeof(void *) +
                    f.extraValuesToPop * sizeof(Value)));

    Linker linker(masm);
    IonCode *wrapper = linker.newCode(cx, JSC::OTHER_CODE);
    if (!wrapper)
        return nullptr;

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(wrapper, "VMWrapper");
#endif

    // linker.newCode may trigger a GC and sweep functionWrappers_ so we have to
    // use relookupOrAdd instead of add.
    if (!functionWrappers_->relookupOrAdd(p, &f, wrapper))
        return nullptr;

    return wrapper;
}

IonCode *
IonRuntime::generatePreBarrier(JSContext *cx, MIRType type)
{
    MacroAssembler masm;

    RegisterSet save;
    if (cx->runtime()->jitSupportsFloatingPoint) {
        save = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                           FloatRegisterSet(FloatRegisters::VolatileMask));
    } else {
        save = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                           FloatRegisterSet());
    }
    masm.PushRegsInMask(save);

    JS_ASSERT(PreBarrierReg == edx);
    masm.movl(ImmPtr(cx->runtime()), ecx);

    masm.setupUnalignedABICall(2, eax);
    masm.passABIArg(ecx);
    masm.passABIArg(edx);

    if (type == MIRType_Value) {
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkValueFromIon));
    } else {
        JS_ASSERT(type == MIRType_Shape);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkShapeFromIon));
    }

    masm.PopRegsInMask(save);
    masm.ret();

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "PreBarrier");
#endif

    return code;
}

typedef bool (*HandleDebugTrapFn)(JSContext *, BaselineFrame *, uint8_t *, bool *);
static const VMFunction HandleDebugTrapInfo = FunctionInfo<HandleDebugTrapFn>(HandleDebugTrap);

IonCode *
IonRuntime::generateDebugTrapHandler(JSContext *cx)
{
    MacroAssembler masm;

    Register scratch1 = eax;
    Register scratch2 = ecx;
    Register scratch3 = edx;

    // Load the return address in scratch1.
    masm.loadPtr(Address(esp, 0), scratch1);

    // Load BaselineFrame pointer in scratch2.
    masm.mov(ebp, scratch2);
    masm.subPtr(Imm32(BaselineFrame::Size()), scratch2);

    // Enter a stub frame and call the HandleDebugTrap VM function. Ensure
    // the stub frame has a nullptr ICStub pointer, since this pointer is
    // marked during GC.
    masm.movePtr(ImmPtr(nullptr), BaselineStubReg);
    EmitEnterStubFrame(masm, scratch3);

    IonCode *code = cx->runtime()->ionRuntime()->getVMWrapper(HandleDebugTrapInfo);
    if (!code)
        return nullptr;

    masm.push(scratch1);
    masm.push(scratch2);
    EmitCallVM(code, masm);

    EmitLeaveStubFrame(masm);

    // If the stub returns |true|, we have to perform a forced return
    // (return from the JS frame). If the stub returns |false|, just return
    // from the trap stub so that execution continues at the current pc.
    Label forcedReturn;
    masm.branchTest32(Assembler::NonZero, ReturnReg, ReturnReg, &forcedReturn);
    masm.ret();

    masm.bind(&forcedReturn);
    masm.loadValue(Address(ebp, BaselineFrame::reverseOffsetOfReturnValue()),
                   JSReturnOperand);
    masm.mov(ebp, esp);
    masm.pop(ebp);
    masm.ret();

    Linker linker(masm);
    IonCode *codeDbg = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(codeDbg, "DebugTrapHandler");
#endif

    return codeDbg;
}

IonCode *
IonRuntime::generateExceptionTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.handleFailureWithHandlerTail();

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "ExceptionTailStub");
#endif

    return code;
}

IonCode *
IonRuntime::generateBailoutTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.generateBailoutTail(edx, ecx);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutTailStub");
#endif

    return code;
}
