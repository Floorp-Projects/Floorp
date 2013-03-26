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
#include "ion/IonSpewer.h"
#include "ion/Bailouts.h"
#include "ion/VMFunctions.h"
#include "ion/ExecutionModeInlines.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

enum EnterJitEbpArgumentOffset {
    ARG_JITCODE     = 2 * sizeof(void *),
    ARG_ARGC        = 3 * sizeof(void *),
    ARG_ARGV        = 4 * sizeof(void *),
    ARG_STACKFRAME  = 5 * sizeof(void *),
    ARG_CALLEETOKEN = 6 * sizeof(void *),
    ARG_RESULT      = 7 * sizeof(void *)
};

/*
 * Generates a trampoline for a C++ function with the EnterIonCode signature,
 * using the standard cdecl calling convention.
 */
IonCode *
IonRuntime::generateEnterJIT(JSContext *cx)
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
    masm.movl(Operand(ebp, ARG_ARGC), eax);
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
    masm.movl(Operand(ebp, ARG_ARGV), ebx);

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
    masm.movl(Operand(ebp, ARG_STACKFRAME), OsrFrameReg);

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    // Create a frame descriptor.
    masm.subl(esp, esi);
    masm.makeFrameDescriptor(esi, IonFrame_Entry);
    masm.push(esi);

    /***************************************************************
        Call passed-in code, get return value and fill in the
        passed in return value pointer
    ***************************************************************/
    masm.call(Operand(ebp, ARG_JITCODE));

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
    masm.movl(Operand(esp, ARG_RESULT + 3 * sizeof(void *)), eax);
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
    return linker.newCode(cx, JSC::OTHER_CODE);
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

    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.movl(Register::FromCode(i), Operand(esp, i * sizeof(void *)));

    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(esp, i * sizeof(double)));

    masm.movl(esp, ebx); // Argument to ion::InvalidationBailout.

    // Make space for InvalidationBailout's frameSize outparam.
    masm.reserveStack(sizeof(size_t));
    masm.movl(esp, ecx);

    masm.setupUnalignedABICall(2, edx);
    masm.passABIArg(ebx);
    masm.passABIArg(ecx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.pop(ebx); // Get the frameSize outparam.

    // Pop the machine state and the dead frame.
    masm.lea(Operand(esp, ebx, TimesOne, sizeof(InvalidationBailoutStack)), esp);

    masm.generateBailoutTail(edx);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::OTHER_CODE);
    IonSpew(IonSpew_Invalidate, "   invalidation thunk created at %p", (void *) code->raw());
    return code;
}

IonCode *
IonRuntime::generateArgumentsRectifier(JSContext *cx, ExecutionMode mode)
{
    MacroAssembler masm(cx);

    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == esi);

    // Load the number of |undefined|s to push into %ecx.
    masm.movl(Operand(esp, IonRectifierFrameLayout::offsetOfCalleeToken()), eax);
    masm.movzwl(Operand(eax, offsetof(JSFunction, nargs)), ecx);
    masm.subl(esi, ecx);

    // Copy the number of actual arguments.
    masm.movl(Operand(esp, IonRectifierFrameLayout::offsetOfNumActualArgs()), edx);

    masm.moveValue(UndefinedValue(), ebx, edi);

    masm.push(FramePointer);
    masm.movl(esp, FramePointer); // Save %esp.

    // Push undefined.
    {
        Label undefLoopTop;
        masm.bind(&undefLoopTop);

        masm.push(ebx); // type(undefined);
        masm.push(edi); // payload(undefined);
        masm.subl(Imm32(1), ecx);

        masm.testl(ecx, ecx);
        masm.j(Assembler::NonZero, &undefLoopTop);
    }

    // Get the topmost argument. We did a push of %ebp earlier, so be sure to
    // account for this in the offset
    BaseIndex b = BaseIndex(FramePointer, esi, TimesEight,
                            sizeof(IonRectifierFrameLayout) + sizeof(void*));
    masm.lea(Operand(b), ecx);

    // Push arguments, |nargs| + 1 times (to include |this|).
    {
        Label copyLoopTop, initialSkip;

        masm.jump(&initialSkip);

        masm.bind(&copyLoopTop);
        masm.subl(Imm32(sizeof(Value)), ecx);
        masm.subl(Imm32(1), esi);
        masm.bind(&initialSkip);

        masm.push(Operand(ecx, sizeof(Value)/2));
        masm.push(Operand(ecx, 0x0));

        masm.testl(esi, esi);
        masm.j(Assembler::NonZero, &copyLoopTop);
    }

    // Construct descriptor, accounting for pushed frame pointer above
    masm.lea(Operand(FramePointer, sizeof(void*)), ebx);
    masm.subl(esp, ebx);
    masm.makeFrameDescriptor(ebx, IonFrame_Rectifier);

    // Construct IonJSFrameLayout.
    masm.push(edx); // number of actual arguments
    masm.push(eax); // calleeToken
    masm.push(ebx); // descriptor

    // Call the target function.
    // Note that this assumes the function is JITted.
    masm.movl(Operand(eax, offsetof(JSFunction, u.i.script_)), eax);
    masm.movl(Operand(eax, OffsetOfIonInJSScript(mode)), eax);
    masm.movl(Operand(eax, IonScript::offsetOfMethod()), eax);
    masm.movl(Operand(eax, IonCode::offsetOfCode()), eax);
    masm.call(eax);

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
    return linker.newCode(cx, JSC::OTHER_CODE);
}

static void
GenerateBailoutThunk(JSContext *cx, MacroAssembler &masm, uint32_t frameClass)
{
    // Push registers such that we can access them from [base + code].
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.movl(Register::FromCode(i), Operand(esp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(esp, i * sizeof(double)));

    // Push the bailout table number.
    masm.push(Imm32(frameClass));

    // The current stack pointer is the first argument to ion::Bailout.
    masm.movl(esp, eax);

    // Call the bailout function. This will correct the size of the bailout.
    masm.setupUnalignedABICall(1, ecx);
    masm.passABIArg(eax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

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
        masm.pop(ecx);
        masm.addl(Imm32(sizeof(uint32_t)), esp);
        masm.addl(ecx, esp);
    } else {
        // Stack is:
        //    ... frame ...
        //    bailoutId
        //    ... bailoutFrame ...
        uint32_t frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
        masm.addl(Imm32(BailoutDataSize + sizeof(void *) + frameSize), esp);
    }

    masm.generateBailoutTail(edx);
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
    return linker.newCode(cx, JSC::OTHER_CODE);
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
    //  +8  [args]
    //  +4  descriptor
    //  +0  returnAddress
    //
    // We're aligned to an exit frame, so link it up.
    masm.enterExitFrame(&f);

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
        masm.reserveStack(sizeof(Value));
        masm.movl(esp, outReg);
        break;

      case Type_Handle:
        outReg = regs.takeAny();
        masm.Push(UndefinedValue());
        masm.movl(esp, outReg);
        break;

      case Type_Int32:
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(int32_t));
        masm.movl(esp, outReg);
        break;

      default:
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }

    Register temp = regs.getAny();
    masm.setupUnalignedABICall(f.argc(), temp);

    // Initialize the context parameter.
    Register cxreg = regs.takeAny();
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
              case VMFunction::DoubleByValue:
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
    Label exception;
    switch (f.failType()) {
      case Type_Object:
        masm.testl(eax, eax);
        masm.j(Assembler::Zero, &exception);
        break;
      case Type_Bool:
        masm.testb(eax, eax);
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
        masm.freeStack(sizeof(JSBool));
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

    RegisterSet save = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                                   FloatRegisterSet(FloatRegisters::VolatileMask));
    masm.PushRegsInMask(save);

    JS_ASSERT(PreBarrierReg == edx);
    masm.movl(ImmWord(cx->runtime), ecx);

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
    return linker.newCode(cx, JSC::OTHER_CODE);
}

