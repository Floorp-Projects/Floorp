/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Scheff <ascheff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscompartment.h"
#include "assembler/assembler/MacroAssembler.h"
#include "ion/IonCompartment.h"
#include "ion/IonLinker.h"
#include "ion/IonFrames.h"
#include "ion/IonSpewer.h"
#include "ion/Bailouts.h"
#include "ion/VMFunctions.h"

using namespace js;
using namespace js::ion;

/*
 * Loads regs.fp into OsrFrameReg.
 * Exists as a prologue to generateEnterJIT().
 */
IonCode *
IonCompartment::generateOsrPrologue(JSContext *cx)
{
    MacroAssembler masm(cx);

    // Load fifth argument, skipping pushed return address.
    masm.movl(Operand(esp, 6 * sizeof(void *)), OsrFrameReg);

    // Caller always invokes generateEnterJIT() first.
    // Jump to default entry, threading OsrFrameReg through it.
    JS_ASSERT(enterJIT_);
    masm.jmp(enterJIT_);

    Linker linker(masm);
    return linker.newCode(cx);
}

/*
 * This method generates a trampoline on x86 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard cdecl calling convention
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    MacroAssembler masm(cx);
    // OsrFrameReg (edx) may not be used below.

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(ebp);
    masm.movl(esp, ebp);

    // Save non-volatile registers. These must be saved by the trampoline,
    // rather than the JIT'd code, because they are scanned by the conservative
    // scanner.
    masm.push(ebx);
    masm.push(esi);
    masm.push(edi);

    // eax <- 8*argc, eax is now the offset betwen argv and the last
    // parameter    --argc is in ebp + 12
    masm.movl(Operand(ebp, 12), eax);
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
    masm.movl(Operand(ebp, 16), ebx);

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

    // Push the callee token.
    masm.push(Operand(ebp, 24));

    // Save the stack size so we can remove arguments and alignment after the
    // call.
    masm.movl(Operand(ebp, 12), eax);
    masm.shll(Imm32(3), eax);
    masm.addl(eax, ecx);
    masm.addl(Imm32(4), ecx);

    // Create a frame descriptor.
    masm.makeFrameDescriptor(ecx, IonFrame_Entry);
    masm.push(ecx);

    /***************************************************************
        Call passed-in code, get return value and fill in the
        passed in return value pointer
    ***************************************************************/
    // Call code  --code pointer is in ebp + 8
    masm.call(Operand(ebp, 8));

    // Pop arguments off the stack.
    // eax <- 8*argc (size of all arugments we pushed on the stack)
    masm.pop(eax);
    masm.shrl(Imm32(FRAMETYPE_BITS), eax); // Unmark EntryFrame.
    masm.addl(eax, esp);

    // |ebp| could have been clobbered by the inner function. For now, re-grab
    // |vp| directly off the stack:
    //
    //  +32 vp
    //  +28 argv
    //  +24 argc
    //  +20 code
    //  +16 <return>
    //  +12 ebp
    //  +8  ebx
    //  +4  esi
    //  +0  edi
    masm.movl(Operand(esp, 32), eax);
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
    return linker.newCode(cx);
}

static void
GenerateBailoutTail(MacroAssembler &masm)
{
    masm.linkExitFrame();

    Label reflow;
    Label interpret;
    Label exception;

    // The return value from Bailout is tagged as:
    // - 0x0: done (thunk to interpreter)
    // - 0x1: error (handle exception)
    // - 0x2: reflow args
    // - 0x3: reflow barrier
    // - 0x4: recompile to inline calls

    masm.cmpl(eax, Imm32(BAILOUT_RETURN_FATAL_ERROR));
    masm.j(Assembler::LessThan, &interpret);
    masm.j(Assembler::Equal, &exception);

    masm.cmpl(eax, Imm32(BAILOUT_RETURN_RECOMPILE_CHECK));
    masm.j(Assembler::LessThan, &reflow);

    // Recompile to inline calls.
    masm.setupUnalignedABICall(0, edx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, RecompileForInlining));

    masm.testl(eax, eax);
    masm.j(Assembler::Zero, &exception);

    masm.jmp(&interpret);

    // Otherwise, we're in the "reflow" case.
    masm.bind(&reflow);
    masm.setupUnalignedABICall(1, edx);
    masm.setABIArg(0, eax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ReflowTypeInfo));

    masm.testl(eax, eax);
    masm.j(Assembler::Zero, &exception);

    masm.bind(&interpret);
    // Reserve space for Interpret() to store a Value.
    masm.subl(Imm32(sizeof(Value)), esp);
    masm.movl(esp, ecx);

    // Call out to the interpreter.
    masm.setupUnalignedABICall(1, edx);
    masm.setABIArg(0, ecx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ThunkToInterpreter));

    // Load the value the interpreter returned.
    masm.popValue(JSReturnOperand);

    // Check for an exception.
    masm.testl(eax, eax);
    masm.j(Assembler::Zero, &exception);

    // Return to the caller.
    masm.ret();

    masm.bind(&exception);
    masm.handleException();
}

IonCode *
IonCompartment::generateInvalidator(JSContext *cx)
{
    AutoIonContextAlloc aica(cx);
    MacroAssembler masm(cx);

    // When a frame gets invalidated (as the result of some call), its
    // *callee* is made to return to this invalidator.
    //
    // This code is reponsible for bailing out the invalidated frame and
    // thunking to the interpreter.
    //
    // We start in the following stack state:
    //
    //
    //        |          ...             |
    //        >--------------------------< <- invalid JS frame fp
    //        |  invalid callee token    |
    //        +--------------------------+
    //        | caller frame descriptor  |
    //        +--------------------------+
    //        |        retaddr           |
    //        +--------------------------+
    //        |         invalid          |
    //        |      locals, args        |
    //        |          ...             |
    //        +--------------------------+
    //        | invalid frame descriptor |
    // esp -> >--------------------------<
    //        |       old retaddr        |
    //        \--------------------------/
    //
    // Within the invalidated frame, the invalidation process has
    // replaced what is typically the callee token with an
    // InvalidationRecord.
    //
    // We do the minimum amount of work in assembly and shunt the rest
    // off to InvalidationBailout. Assembly does:
    //
    // - Push the machine state onto the stack.
    // - Call the InvalidationBailout routine with the stack pointer.
    // - Now that the frame has been bailed out, convert the invalidated
    //   frame into an exit frame.
    // - Do the normal check-return-code-and-thunk-to-the-interpreter dance.

    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32 i = 0; i < Registers::Total; i++)
        masm.movl(Register::FromCode(i), Operand(esp, i * sizeof(void *)));

    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32 i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(esp, i * sizeof(double)));

    masm.movl(esp, ebx); // Argument to ion::InvalidationBailout.

    // Make space for InvalidationBailout's frameSize outparam.
    masm.reserveStack(sizeof(size_t));
    masm.movl(esp, ecx);

    masm.setupUnalignedABICall(2, edx);
    masm.setABIArg(0, ebx);
    masm.setABIArg(1, ecx);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.pop(ebx); // Get the frameSize outparam.

    // Pop the machine state and the dead frame.
    const uint32 BailoutDataSize = sizeof(double) * FloatRegisters::Total +
                                   sizeof(void *) * Registers::Total;
    masm.lea(Operand(esp, ebx, TimesOne, BailoutDataSize), esp);

    GenerateBailoutTail(masm);

    Linker linker(masm);
    IonCode *code = linker.newCode(cx);
    IonSpew(IonSpew_Invalidate, "   invalidation thunk created at %p", (void *) code->raw());
    return code;
}

IonCode *
IonCompartment::generateArgumentsRectifier(JSContext *cx)
{
    MacroAssembler masm(cx);

    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == esi);

    // Load the number of |undefined|s to push into %ecx.
    masm.movl(Operand(esp, IonJSFrameLayout::offsetOfCalleeToken()), eax);
    masm.load16(Operand(eax, offsetof(JSFunction, nargs)), ecx);
    masm.subl(esi, ecx);

    masm.moveValue(UndefinedValue(), ebx, edi);

    masm.movl(esp, ebp); // Save %esp.

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

    // Get the topmost argument.
    masm.movl(esi, edi);
    masm.shll(Imm32(3), edi); // edi <- nargs * sizeof(Value);

    masm.movl(ebp, ecx);
    masm.addl(Imm32(sizeof(IonRectifierFrameLayout)), ecx);
    masm.addl(edi, ecx);

    // Push arguments, |nargs| + 1 times (to include |this|).
    {
        Label copyLoopTop, initialSkip;

        masm.jump(&initialSkip);

        masm.bind(&copyLoopTop);
        masm.subl(Imm32(sizeof(Value)), ecx);
        masm.subl(Imm32(1), esi);
        masm.bind(&initialSkip);

        masm.mov(Operand(ecx, sizeof(Value)/2), edx);
        masm.push(edx);
        masm.mov(Operand(ecx, 0x0), edx);
        masm.push(edx);

        masm.testl(esi, esi);
        masm.j(Assembler::NonZero, &copyLoopTop);
    }

    // Construct descriptor.
    masm.subl(esp, ebp);
    masm.makeFrameDescriptor(ebp, IonFrame_Rectifier);

    // Construct IonJSFrameLayout.
    masm.push(eax); // calleeToken
    masm.push(ebp); // descriptor

    // Call the target function.
    // Note that this assumes the function is JITted.
    masm.movl(Operand(eax, offsetof(JSFunction, u.i.script_)), eax);
    masm.movl(Operand(eax, offsetof(JSScript, ion)), eax);
    masm.movl(Operand(eax, offsetof(IonScript, method_)), eax);
    masm.movl(Operand(eax, IonCode::OffsetOfCode()), eax);
    masm.call(eax);

    // Remove the rectifier frame.
    masm.pop(ebp);            // ebp <- descriptor with FrameType.
    masm.shrl(Imm32(FRAMETYPE_BITS), ebp); // ebp <- descriptor.
    masm.pop(edi);            // Discard calleeToken.
    masm.addl(ebp, esp);      // Discard pushed arguments.

    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx);
}

static void
GenerateBailoutThunk(JSContext *cx, MacroAssembler &masm, uint32 frameClass)
{
    // Push registers such that we can access them from [base + code].
    masm.reserveStack(Registers::Total * sizeof(void *));
    for (uint32 i = 0; i < Registers::Total; i++)
        masm.movl(Register::FromCode(i), Operand(esp, i * sizeof(void *)));

    // Push xmm registers, such that we can access them from [base + code].
    masm.reserveStack(FloatRegisters::Total * sizeof(double));
    for (uint32 i = 0; i < FloatRegisters::Total; i++)
        masm.movsd(FloatRegister::FromCode(i), Operand(esp, i * sizeof(double)));

    // Push the bailout table number.
    masm.push(Imm32(frameClass));

    // The current stack pointer is the first argument to ion::Bailout.
    masm.movl(esp, eax);

    // Call the bailout function. This will correct the size of the bailout.
    masm.setupUnalignedABICall(1, ecx);
    masm.setABIArg(0, eax);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));

    // Common size of stuff we've pushed.
    const uint32 BailoutDataSize = sizeof(void *) + // frameClass
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
        masm.addl(Imm32(sizeof(uint32)), esp);
        masm.addl(ecx, esp);
    } else {
        // Stack is:
        //    ... frame ...
        //    bailoutId
        //    ... bailoutFrame ...
        uint32 frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
        masm.addl(Imm32(BailoutDataSize + sizeof(void *) + frameSize), esp);
    }

    GenerateBailoutTail(masm);
}

IonCode *
IonCompartment::generateBailoutTable(JSContext *cx, uint32 frameClass)
{
    MacroAssembler masm;

    Label bailout;
    for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++)
        masm.call(&bailout);
    masm.bind(&bailout);

    GenerateBailoutThunk(cx, masm, frameClass);

    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm;

    GenerateBailoutThunk(cx, masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    return linker.newCode(cx);
}

IonCode *
IonCompartment::generateVMWrapper(JSContext *cx, const VMFunction &f)
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
    GeneralRegisterSet regs =
        GeneralRegisterSet::Not(GeneralRegisterSet(Register::Codes::VolatileMask));

    // Stack is:
    //    ... frame ...
    //  +8  [args]
    //  +4  descriptor
    //  +0  returnAddress
    //  -4  oldReturnAddress (will be pushed after linkExitFrame)
    //
    // We're aligned to an exit frame, so link it up.
    masm.linkExitFrame();

    // Push the return address, which we'll use to check for OSI.
    masm.push(Operand(esp, 0));

    // Save the current stack pointer as the base for copying arguments.
    Register argsBase = InvalidReg;
    if (f.explicitArgs) {
        argsBase = regs.takeAny();
        masm.lea(Operand(esp, sizeof(IonExitFrameLayout) + sizeof(uintptr_t)), argsBase);
    }

    // Reserve space for the outparameter.
    Register outReg = InvalidReg;
    if (f.outParam == Type_Value) {
        outReg = regs.takeAny();
        masm.reserveStack(sizeof(Value));
        masm.movl(esp, outReg);
    }

    Register temp = regs.getAny();
    masm.setupUnalignedABICall(f.argc(), temp);

    // Initialize and set the context parameter.
    Register cxreg = regs.takeAny();
    masm.movePtr(ImmWord(JS_THREAD_DATA(cx)), cxreg);
    masm.loadPtr(Address(cxreg, offsetof(ThreadData, ionJSContext)), cxreg);
    masm.setABIArg(0, cxreg);

    size_t argDisp = 0;
    size_t argc = 1;

    // Copy arguments.
    if (f.explicitArgs) {
        for (uint32 explicitArg = 0; explicitArg < f.explicitArgs; explicitArg++) {
            MoveOperand from;
            switch (f.argProperties(explicitArg)) {
              case VMFunction::WordByValue:
                masm.setABIArg(argc++, MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByValue:
                masm.setABIArg(argc++, MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                masm.setABIArg(argc++, MoveOperand(argsBase, argDisp));
                argDisp += sizeof(void *);
                break;
              case VMFunction::WordByRef:
                masm.setABIArg(argc++, MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += sizeof(void *);
                break;
              case VMFunction::DoubleByRef:
                masm.setABIArg(argc++, MoveOperand(argsBase, argDisp, MoveOperand::EFFECTIVE));
                argDisp += 2 * sizeof(void *);
                break;
            }
        }
    }

    // Copy the implicit outparam, if any.
    if (outReg != InvalidReg)
        masm.setABIArg(argc++, outReg);
    JS_ASSERT(f.argc() == argc);

    masm.callWithABI(f.wrapped);

    // Test for failure.
    Label exception;
    masm.testl(eax, eax);
    masm.j(Assembler::Zero, &exception);

    // Load the outparam and free any allocated stack.
    if (f.outParam == Type_Value) {
        masm.loadValue(Address(esp, 0), JSReturnOperand);
        masm.freeStack(sizeof(Value));
    }

    // Check if the calling frame has been invalidated, in which case we can't
    // disturb the frame descriptor.
    Register scratch = (f.outParam == Type_Value) ? ReturnReg : JSReturnReg_Type;
    Label invalidated;
    masm.pop(scratch);
    masm.cmpl(scratch, Operand(esp, 0));
    masm.j(Assembler::NotEqual, &invalidated);

    masm.retn(Imm32(sizeof(IonExitFrameLayout) + f.explicitStackSlots() * sizeof(void *)));

    masm.bind(&exception);
    masm.handleException();

    masm.bind(&invalidated);
    masm.cmpl(Operand(esp, 0), Imm32(0));
    masm.j(Assembler::Equal, &exception);
    masm.ret();

    Linker linker(masm);
    IonCode *wrapper = linker.newCode(cx);
    if (!wrapper || !functionWrappers_->add(p, &f, wrapper))
        return NULL;

    return wrapper;
}
