/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "assembler/assembler/MacroAssembler.h"
#include "ion/IonCompartment.h"
#include "ion/IonLinker.h"
#include "ion/IonFrames.h"
#include "ion/Bailouts.h"
#include "ion/VMFunctions.h"
#include "ion/IonSpewer.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

/* This method generates a trampoline on x64 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard x64 fastcall calling convention
 */
IonCode *
IonRuntime::generateEnterJIT(JSContext *cx)
{
    MacroAssembler masm(cx);

    const Register reg_code  = IntArgReg0;
    const Register reg_argc  = IntArgReg1;
    const Register reg_argv  = IntArgReg2;
    JS_ASSERT(OsrFrameReg == IntArgReg3);

#if defined(_WIN64)
    const Operand token  = Operand(rbp, 16 + ShadowStackSpace);
    const Operand result = Operand(rbp, 24 + ShadowStackSpace);
#else
    const Register token  = IntArgReg4;
    const Register result = IntArgReg5;
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

    // Call function.
    masm.call(reg_code);

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
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.movq(Register::FromCode(i), Operand(rsp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(rsp, i * sizeof(double)));

    masm.movq(rsp, rbx); // Argument to ion::InvalidationBailout.

    // Make space for InvalidationBailout's frameSize outparam.
    masm.reserveStack(sizeof(size_t));
    masm.movq(rsp, rcx);

    masm.setupUnalignedABICall(2, rdx);
    masm.passABIArg(rbx);
    masm.passABIArg(rcx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.pop(rbx); // Get the frameSize outparam.

    // Pop the machine state and the dead frame.
    masm.lea(Operand(rsp, rbx, TimesOne, sizeof(InvalidationBailoutStack)), rsp);

    masm.generateBailoutTail(rdx);

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

IonCode *
IonRuntime::generateArgumentsRectifier(JSContext *cx)
{
    // Do not erase the frame pointer in this function.

    MacroAssembler masm(cx);

    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == r8);

    // Load the number of |undefined|s to push into %rcx.
    masm.movq(Operand(rsp, IonRectifierFrameLayout::offsetOfCalleeToken()), rax);
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
    masm.push(rax); // calleeToken
    masm.push(r9); // descriptor

    // Call the target function.
    // Note that this code assumes the function is JITted.
    masm.movq(Operand(rax, offsetof(JSFunction, u.i.script_)), rax);
    masm.movq(Operand(rax, offsetof(JSScript, ion)), rax);
    masm.movq(Operand(rax, IonScript::offsetOfMethod()), rax);
    masm.movq(Operand(rax, IonCode::offsetOfCode()), rax);
    masm.call(rax);

    // Remove the rectifier frame.
    masm.pop(r9);             // r9 <- descriptor with FrameType.
    masm.shrq(Imm32(FRAMESIZE_SHIFT), r9);
    masm.pop(r11);            // Discard calleeToken.
    masm.pop(r11);            // Discard numActualArgs.
    masm.addq(r9, rsp);       // Discard pushed arguments.

    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

static void
GenerateBailoutThunk(JSContext *cx, MacroAssembler &masm, uint32_t frameClass)
{
    // Push registers such that we can access them from [base + code].
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.movq(Register::FromCode(i), Operand(rsp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(rsp, i * sizeof(double)));

    // Get the stack pointer into a register, pre-alignment.
    masm.movq(rsp, r8);

    // Call the bailout function.
    masm.setupUnalignedABICall(1, rax);
    masm.passABIArg(r8);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

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

    masm.generateBailoutTail(rdx);
}

IonCode *
IonRuntime::generateBailoutTable(JSContext *cx, uint32_t frameClass)
{
    JS_NOT_REACHED("x64 does not use bailout tables");
    return NULL;
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

    // Stack is:
    //    ... frame ...
    //  +12 [args]
    //  +8  descriptor
    //  +0  returnAddress
    //
    // We're aligned to an exit frame, so link it up.
    masm.enterExitFrame(&f);

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
        masm.Push(UndefinedValue());
        masm.movq(esp, outReg);
        break;

      case Type_Int32:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(int32_t));
        masm.movq(esp, outReg);
        break;

      default:
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }

    Register temp = regs.getAny();
    masm.setupUnalignedABICall(f.argc(), temp);

    // Initialize the context parameter.
    Register cxreg = IntArgReg0;
    masm.loadJSContext(cxreg);
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
              case VMFunction::WordByRef:
                masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByValue:
              case VMFunction::DoubleByRef:
                JS_NOT_REACHED("NYI: x64 callVM should not be used with 128bits values.");
                break;
            }
        }
    }

    // Copy the implicit outparam, if any.
    if (outReg != InvalidReg)
        masm.passABIArg(outReg);

    masm.callWithABI(f.wrapped);

    // Test for failure.
    Label exception;
    switch (f.failType()) {
      case Type_Object:
        masm.testq(rax, rax);
        masm.j(Assembler::Zero, &exception);
        break;
      case Type_Bool:
        masm.testb(rax, rax);
        masm.j(Assembler::Zero, &exception);
        break;
      default:
        JS_NOT_REACHED("unknown failure kind");
        break;
    }

    // Load the outparam and free any allocated stack.
    switch (f.outParam) {
      case Type_Handle:
      case Type_Value:
        masm.loadValue(Address(esp, 0), JSReturnOperand);
        masm.freeStack(sizeof(Value));
        break;

      case Type_Int32:
        masm.load32(Address(esp, 0), ReturnReg);
        masm.freeStack(sizeof(int32_t));
        break;

      default:
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }
    masm.leaveExitFrame();
    masm.retn(Imm32(sizeof(IonExitFrameLayout) + f.explicitStackSlots() * sizeof(void *)));

    masm.bind(&exception);
    masm.handleException();

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
    masm.movq(ImmWord(cx->runtime), rcx);

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

