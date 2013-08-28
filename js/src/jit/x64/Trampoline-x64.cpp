/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "assembler/assembler/MacroAssembler.h"
#include "jit/Bailouts.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonCompartment.h"
#include "jit/IonFrames.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/VMFunctions.h"
#include "jit/x64/BaselineHelpers-x64.h"

using namespace js;
using namespace js::jit;

// All registers to save and restore. This includes the stack pointer, since we
// use the ability to reference register values on the stack by index.
static const RegisterSet AllRegs =
  RegisterSet(GeneralRegisterSet(Registers::AllMask),
              FloatRegisterSet(FloatRegisters::AllMask));

/* This method generates a trampoline on x64 for a c++ function with
 * the following signature:
 *   bool blah(void *code, int argc, Value *argv, JSObject *scopeChain,
 *               Value *vp)
 *   ...using standard x64 fastcall calling convention
 */
IonCode *
IonRuntime::generateEnterJIT(JSContext *cx, EnterJitType type)
{
    MacroAssembler masm(cx);

    const Register reg_code  = IntArgReg0;
    const Register reg_argc  = IntArgReg1;
    const Register reg_argv  = IntArgReg2;
    JS_ASSERT(OsrFrameReg == IntArgReg3);

#if defined(_WIN64)
    const Operand token  = Operand(rbp, 16 + ShadowStackSpace);
    const Operand scopeChain = Operand(rbp, 24 + ShadowStackSpace);
    const Operand numStackValuesAddr = Operand(rbp, 32 + ShadowStackSpace);
    const Operand result = Operand(rbp, 40 + ShadowStackSpace);
#else
    const Register token = IntArgReg4;
    const Register scopeChain = IntArgReg5;
    const Operand numStackValuesAddr = Operand(rbp, 16 + ShadowStackSpace);
    const Operand result = Operand(rbp, 24 + ShadowStackSpace);
#endif

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(rbp);
    masm.mov(rsp, rbp);

    // Save non-volatile registers. These must be saved by the trampoline, rather
    // than by the JIT'd code, because they are scanned by the conservative scanner.
    masm.push(rbx);
    masm.push(r12);
    masm.push(r13);
    masm.push(r14);
    masm.push(r15);
#if defined(_WIN64)
    masm.push(rdi);
    masm.push(rsi);

    // 16-byte aligment for movdqa
    masm.subq(Imm32(16 * 10 + 8), rsp);

    masm.movdqa(xmm6, Operand(rsp, 16 * 0));
    masm.movdqa(xmm7, Operand(rsp, 16 * 1));
    masm.movdqa(xmm8, Operand(rsp, 16 * 2));
    masm.movdqa(xmm9, Operand(rsp, 16 * 3));
    masm.movdqa(xmm10, Operand(rsp, 16 * 4));
    masm.movdqa(xmm11, Operand(rsp, 16 * 5));
    masm.movdqa(xmm12, Operand(rsp, 16 * 6));
    masm.movdqa(xmm13, Operand(rsp, 16 * 7));
    masm.movdqa(xmm14, Operand(rsp, 16 * 8));
    masm.movdqa(xmm15, Operand(rsp, 16 * 9));
#endif

    // Save arguments passed in registers needed after function call.
    masm.push(result);

    // Remember stack depth without padding and arguments.
    masm.mov(rsp, r14);

    // Remember number of bytes occupied by argument vector
    masm.mov(reg_argc, r13);
    masm.shll(Imm32(3), r13);

    // Guarantee 16-byte alignment.
    // We push argc, callee token, frame size, and return address.
    // The latter two are 16 bytes together, so we only consider argc and the
    // token.
    masm.mov(rsp, r12);
    masm.subq(r13, r12);
    masm.subq(Imm32(8), r12);
    masm.andl(Imm32(0xf), r12);
    masm.subq(r12, rsp);

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/

    // r13 still stores the number of bytes in the argument vector.
    masm.addq(reg_argv, r13); // r13 points above last argument.

    // while r13 > rdx, push arguments.
    {
        Label header, footer;
        masm.bind(&header);

        masm.cmpq(r13, reg_argv);
        masm.j(AssemblerX86Shared::BelowOrEqual, &footer);

        masm.subq(Imm32(8), r13);
        masm.push(Operand(r13, 0));
        masm.jmp(&header);

        masm.bind(&footer);
    }

    // Push the number of actual arguments.  |result| is used to store the
    // actual number of arguments without adding an extra argument to the enter
    // JIT.
    masm.movq(result, reg_argc);
    masm.unboxInt32(Operand(reg_argc, 0), reg_argc);
    masm.push(reg_argc);

    // Push the callee token.
    masm.push(token);

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    masm.subq(rsp, r14);

    // Create a frame descriptor.
    masm.makeFrameDescriptor(r14, IonFrame_Entry);
    masm.push(r14);

    CodeLabel returnLabel;
    if (type == EnterJitBaseline) {
        // Handle OSR.
        GeneralRegisterSet regs(GeneralRegisterSet::All());
        regs.takeUnchecked(OsrFrameReg);
        regs.take(rbp);
        regs.take(reg_code);

        // Ensure that |scratch| does not end up being JSReturnOperand.
        // Do takeUnchecked because on Win64/x64, reg_code (IntArgReg0) and JSReturnOperand are
        // the same (rcx).  See bug 849398.
        regs.takeUnchecked(JSReturnOperand);
        Register scratch = regs.takeAny();

        Label notOsr;
        masm.branchTestPtr(Assembler::Zero, OsrFrameReg, OsrFrameReg, &notOsr);

        Register numStackValues = regs.takeAny();
        masm.movq(numStackValuesAddr, numStackValues);

        // Push return address, previous frame pointer.
        masm.mov(returnLabel.dest(), scratch);
        masm.push(scratch);
        masm.push(rbp);

        // Reserve frame.
        Register framePtr = rbp;
        masm.subPtr(Imm32(BaselineFrame::Size()), rsp);
        masm.mov(rsp, framePtr);

        // Reserve space for locals and stack values.
        Register valuesSize = regs.takeAny();
        masm.mov(numStackValues, valuesSize);
        masm.shll(Imm32(3), valuesSize);
        masm.subPtr(valuesSize, rsp);

        // Enter exit frame.
        masm.addPtr(Imm32(BaselineFrame::Size() + BaselineFrame::FramePointerOffset), valuesSize);
        masm.makeFrameDescriptor(valuesSize, IonFrame_BaselineJS);
        masm.push(valuesSize);
        masm.push(Imm32(0)); // Fake return address.
        masm.enterFakeExitFrame();

        regs.add(valuesSize);

        masm.push(framePtr);
        masm.push(reg_code);

        masm.setupUnalignedABICall(3, scratch);
        masm.passABIArg(framePtr); // BaselineFrame
        masm.passABIArg(OsrFrameReg); // StackFrame
        masm.passABIArg(numStackValues);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, jit::InitBaselineFrameForOsr));

        masm.pop(reg_code);
        masm.pop(framePtr);

        JS_ASSERT(reg_code != ReturnReg);

        Label error;
        masm.addPtr(Imm32(IonExitFrameLayout::SizeWithFooter()), rsp);
        masm.addPtr(Imm32(BaselineFrame::Size()), framePtr);
        masm.branchIfFalseBool(ReturnReg, &error);

        masm.jump(reg_code);

        // OOM: load error value, discard return address and previous frame
        // pointer and return.
        masm.bind(&error);
        masm.mov(framePtr, rsp);
        masm.addPtr(Imm32(2 * sizeof(uintptr_t)), rsp);
        masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
        masm.mov(returnLabel.dest(), scratch);
        masm.jump(scratch);

        masm.bind(&notOsr);
        masm.movq(scopeChain, R1.scratchReg());
    }

    // Call function.
    masm.call(reg_code);

    if (type == EnterJitBaseline) {
        // Baseline OSR will return here.
        masm.bind(returnLabel.src());
        if (!masm.addCodeLabel(returnLabel))
            return NULL;
    }

    // Pop arguments and padding from stack.
    masm.pop(r14);              // Pop and decode descriptor.
    masm.shrq(Imm32(FRAMESIZE_SHIFT), r14);
    masm.addq(r14, rsp);        // Remove arguments.

    /*****************************************************************
    Place return value where it belongs, pop all saved registers
    *****************************************************************/
    masm.pop(r12); // vp
    masm.storeValue(JSReturnOperand, Operand(r12, 0));

    // Restore non-volatile registers.
#if defined(_WIN64)
    masm.movdqa(Operand(rsp, 16 * 0), xmm6);
    masm.movdqa(Operand(rsp, 16 * 1), xmm7);
    masm.movdqa(Operand(rsp, 16 * 2), xmm8);
    masm.movdqa(Operand(rsp, 16 * 3), xmm9);
    masm.movdqa(Operand(rsp, 16 * 4), xmm10);
    masm.movdqa(Operand(rsp, 16 * 5), xmm11);
    masm.movdqa(Operand(rsp, 16 * 6), xmm12);
    masm.movdqa(Operand(rsp, 16 * 7), xmm13);
    masm.movdqa(Operand(rsp, 16 * 8), xmm14);
    masm.movdqa(Operand(rsp, 16 * 9), xmm15);

    masm.addq(Imm32(16 * 10 + 8), rsp);

    masm.pop(rsi);
    masm.pop(rdi);
#endif
    masm.pop(r15);
    masm.pop(r14);
    masm.pop(r13);
    masm.pop(r12);
    masm.pop(rbx);

    // Restore frame pointer and return.
    masm.pop(rbp);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

IonCode *
IonRuntime::generateInvalidator(JSContext *cx)
{
    AutoIonContextAlloc aica(cx);
    MacroAssembler masm(cx);

    // See explanatory comment in x86's IonRuntime::generateInvalidator.

    masm.addq(Imm32(sizeof(uintptr_t)), rsp);

    // Push registers such that we can access them from [base + code].
    masm.PushRegsInMask(AllRegs);

    masm.movq(rsp, rax); // Argument to jit::InvalidationBailout.

    // Make space for InvalidationBailout's frameSize outparam.
    masm.reserveStack(sizeof(size_t));
    masm.movq(rsp, rbx);

    // Make space for InvalidationBailout's bailoutInfo outparam.
    masm.reserveStack(sizeof(void *));
    masm.movq(rsp, r9);

    masm.setupUnalignedABICall(3, rdx);
    masm.passABIArg(rax);
    masm.passABIArg(rbx);
    masm.passABIArg(r9);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.pop(r9); // Get the bailoutInfo outparam.
    masm.pop(rbx); // Get the frameSize outparam.

    // Pop the machine state and the dead frame.
    masm.lea(Operand(rsp, rbx, TimesOne, sizeof(InvalidationBailoutStack)), rsp);

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in r9.
    IonCode *bailoutTail = cx->runtime()->ionRuntime()->getBailoutTail();
    masm.jmp(bailoutTail);

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

IonCode *
IonRuntime::generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut)
{
    // Do not erase the frame pointer in this function.

    MacroAssembler masm(cx);

    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == r8);

    // Load the number of |undefined|s to push into %rcx.
    masm.movq(Operand(rsp, IonRectifierFrameLayout::offsetOfCalleeToken()), rax);
    masm.clearCalleeTag(rax, mode);
    masm.movzwl(Operand(rax, offsetof(JSFunction, nargs)), rcx);
    masm.subq(r8, rcx);

    // Copy the number of actual arguments
    masm.movq(Operand(rsp, IonRectifierFrameLayout::offsetOfNumActualArgs()), rdx);

    masm.moveValue(UndefinedValue(), r10);

    masm.movq(rsp, r9); // Save %rsp.

    // Push undefined.
    {
        Label undefLoopTop;
        masm.bind(&undefLoopTop);

        masm.push(r10);
        masm.subl(Imm32(1), rcx);

        masm.testl(rcx, rcx);
        masm.j(Assembler::NonZero, &undefLoopTop);
    }

    // Get the topmost argument.
    BaseIndex b = BaseIndex(r9, r8, TimesEight, sizeof(IonRectifierFrameLayout));
    masm.lea(Operand(b), rcx);

    // Push arguments, |nargs| + 1 times (to include |this|).
    {
        Label copyLoopTop, initialSkip;

        masm.jump(&initialSkip);

        masm.bind(&copyLoopTop);
        masm.subq(Imm32(sizeof(Value)), rcx);
        masm.subl(Imm32(1), r8);
        masm.bind(&initialSkip);

        masm.push(Operand(rcx, 0x0));

        masm.testl(r8, r8);
        masm.j(Assembler::NonZero, &copyLoopTop);
    }

    // Construct descriptor.
    masm.subq(rsp, r9);
    masm.makeFrameDescriptor(r9, IonFrame_Rectifier);

    // Construct IonJSFrameLayout.
    masm.push(rdx); // numActualArgs
    masm.pushCalleeToken(rax, mode);
    masm.push(r9); // descriptor

    // Call the target function.
    // Note that this code assumes the function is JITted.
    masm.movq(Operand(rax, JSFunction::offsetOfNativeOrScript()), rax);
    masm.loadBaselineOrIonRaw(rax, rax, mode, NULL);
    masm.call(rax);
    uint32_t returnOffset = masm.currentOffset();

    // Remove the rectifier frame.
    masm.pop(r9);             // r9 <- descriptor with FrameType.
    masm.shrq(Imm32(FRAMESIZE_SHIFT), r9);
    masm.pop(r11);            // Discard calleeToken.
    masm.pop(r11);            // Discard numActualArgs.
    masm.addq(r9, rsp);       // Discard pushed arguments.

    masm.ret();

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);

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

    // Get the stack pointer into a register, pre-alignment.
    masm.movq(rsp, r8);

    // Make space for Bailout's bailoutInfo outparam.
    masm.reserveStack(sizeof(void *));
    masm.movq(rsp, r9);

    // Call the bailout function.
    masm.setupUnalignedABICall(2, rax);
    masm.passABIArg(r8);
    masm.passABIArg(r9);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

    masm.pop(r9); // Get the bailoutInfo outparam.

    // Stack is:
    //     [frame]
    //     snapshotOffset
    //     frameSize
    //     [bailoutFrame]
    //
    // Remove both the bailout frame and the topmost Ion frame's stack.
    static const uint32_t BailoutDataSize = sizeof(void *) * Registers::Total +
                                          sizeof(double) * FloatRegisters::Total;
    masm.addq(Imm32(BailoutDataSize), rsp);
    masm.pop(rcx);
    masm.lea(Operand(rsp, rcx, TimesOne, sizeof(void *)), rsp);

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in r9.
    IonCode *bailoutTail = cx->runtime()->ionRuntime()->getBailoutTail();
    masm.jmp(bailoutTail);
}

IonCode *
IonRuntime::generateBailoutTable(JSContext *cx, uint32_t frameClass)
{
    MOZ_ASSUME_UNREACHABLE("x64 does not use bailout tables");
}

IonCode *
IonRuntime::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm;

    GenerateBailoutThunk(cx, masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
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
    Register cxreg = IntArgReg0;

    // Stack is:
    //    ... frame ...
    //  +12 [args]
    //  +8  descriptor
    //  +0  returnAddress
    //
    // We're aligned to an exit frame, so link it up.
    masm.enterExitFrameAndLoadContext(&f, cxreg, regs.getAny(), f.executionMode);

    // Save the current stack pointer as the base for copying arguments.
    Register argsBase = InvalidReg;
    if (f.explicitArgs) {
        argsBase = r10;
        regs.take(argsBase);
        masm.lea(Operand(rsp,IonExitFrameLayout::SizeWithFooter()), argsBase);
    }

    // Reserve space for the outparameter.
    Register outReg = InvalidReg;
    switch (f.outParam) {
      case Type_Value:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(Value));
        masm.movq(esp, outReg);
        break;

      case Type_Handle:
        outReg = regs.takeAny();
        masm.PushEmptyRooted(f.outParamRootType);
        masm.movq(esp, outReg);
        break;

      case Type_Int32:
      case Type_Bool:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(int32_t));
        masm.movq(esp, outReg);
        break;

      case Type_Double:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(double));
        masm.movq(esp, outReg);
        break;

      case Type_Pointer:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(uintptr_t));
        masm.movq(esp, outReg);
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
                if (f.argPassedInFloatReg(explicitArg))
                    masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::FLOAT));
                else
                    masm.passABIArg(MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                break;
              case VMFunction::WordByRef:
                masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByValue:
              case VMFunction::DoubleByRef:
                MOZ_ASSUME_UNREACHABLE("NYI: x64 callVM should not be used with 128bits values.");
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
        masm.branchTestPtr(Assembler::Zero, rax, rax, masm.failureLabel(f.executionMode));
        break;
      case Type_Bool:
        masm.testb(rax, rax);
        masm.j(Assembler::Zero, masm.failureLabel(f.executionMode));
        break;
      case Type_ParallelResult:
        masm.branchPtr(Assembler::NotEqual, rax, Imm32(TP_SUCCESS),
                       masm.failureLabel(f.executionMode));
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
        masm.loadValue(Address(esp, 0), JSReturnOperand);
        masm.freeStack(sizeof(Value));
        break;

      case Type_Int32:
        masm.load32(Address(esp, 0), ReturnReg);
        masm.freeStack(sizeof(int32_t));
        break;

      case Type_Bool:
        masm.load8ZeroExtend(Address(esp, 0), ReturnReg);
        masm.freeStack(sizeof(int32_t));
        break;

      case Type_Double:
        JS_ASSERT(cx->runtime()->jitSupportsFloatingPoint);
        masm.loadDouble(Address(esp, 0), ReturnFloatReg);
        masm.freeStack(sizeof(double));
        break;

      case Type_Pointer:
        masm.loadPtr(Address(esp, 0), ReturnReg);
        masm.freeStack(sizeof(uintptr_t));
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
        return NULL;

    // linker.newCode may trigger a GC and sweep functionWrappers_ so we have to
    // use relookupOrAdd instead of add.
    if (!functionWrappers_->relookupOrAdd(p, &f, wrapper))
        return NULL;

    return wrapper;
}

IonCode *
IonRuntime::generatePreBarrier(JSContext *cx, MIRType type)
{
    MacroAssembler masm;

    RegisterSet regs = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                                   FloatRegisterSet(FloatRegisters::VolatileMask));
    masm.PushRegsInMask(regs);

    JS_ASSERT(PreBarrierReg == rdx);
    masm.mov(ImmWord(cx->runtime()), rcx);

    masm.setupUnalignedABICall(2, rax);
    masm.passABIArg(rcx);
    masm.passABIArg(rdx);
    if (type == MIRType_Value) {
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkValueFromIon));
    } else {
        JS_ASSERT(type == MIRType_Shape);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkShapeFromIon));
    }

    masm.PopRegsInMask(regs);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

typedef bool (*HandleDebugTrapFn)(JSContext *, BaselineFrame *, uint8_t *, bool *);
static const VMFunction HandleDebugTrapInfo = FunctionInfo<HandleDebugTrapFn>(HandleDebugTrap);

IonCode *
IonRuntime::generateDebugTrapHandler(JSContext *cx)
{
    MacroAssembler masm;

    Register scratch1 = rax;
    Register scratch2 = rcx;
    Register scratch3 = rdx;

    // Load the return address in scratch1.
    masm.loadPtr(Address(rsp, 0), scratch1);

    // Load BaselineFrame pointer in scratch2.
    masm.mov(rbp, scratch2);
    masm.subPtr(Imm32(BaselineFrame::Size()), scratch2);

    // Enter a stub frame and call the HandleDebugTrap VM function. Ensure
    // the stub frame has a NULL ICStub pointer, since this pointer is marked
    // during GC.
    masm.movePtr(ImmWord((void *)NULL), BaselineStubReg);
    EmitEnterStubFrame(masm, scratch3);

    IonCode *code = cx->runtime()->ionRuntime()->getVMWrapper(HandleDebugTrapInfo);
    if (!code)
        return NULL;

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
    masm.mov(rbp, rsp);
    masm.pop(rbp);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

IonCode *
IonRuntime::generateExceptionTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.handleFailureWithHandlerTail();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

IonCode *
IonRuntime::generateBailoutTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.generateBailoutTail(rdx, r9);

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}
