/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"

#include "assembler/assembler/MacroAssembler.h"
#include "jit/arm/BaselineHelpers-arm.h"
#include "jit/Bailouts.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonFrames.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/JitCompartment.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"

using namespace js;
using namespace js::jit;

static const FloatRegisterSet NonVolatileFloatRegs =
    FloatRegisterSet((1 << FloatRegisters::d8) |
                     (1 << FloatRegisters::d9) |
                     (1 << FloatRegisters::d10) |
                     (1 << FloatRegisters::d11) |
                     (1 << FloatRegisters::d12) |
                     (1 << FloatRegisters::d13) |
                     (1 << FloatRegisters::d14) |
                     (1 << FloatRegisters::d15));

static void
GenerateReturn(MacroAssembler &masm, int returnCode)
{
    // Restore non-volatile floating point registers
    masm.transferMultipleByRuns(NonVolatileFloatRegs, IsLoad, StackPointer, IA);

    // Get rid of the bogus r0 push.
    masm.as_add(sp, sp, Imm8(4));

    // Set up return value
    masm.ma_mov(Imm32(returnCode), r0);

    // Pop and return
    masm.startDataTransferM(IsLoad, sp, IA, WriteBack);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    // r12 isn't saved, so it shouldn't be restored.
    masm.transferReg(pc);
    masm.finishDataTransfer();
    masm.dumpPool();
}

struct EnterJITStack
{
    double d8;
    double d9;
    double d10;
    double d11;
    double d12;
    double d13;
    double d14;
    double d15;

    void *r0; // alignment.

    // non-volatile registers.
    void *r4;
    void *r5;
    void *r6;
    void *r7;
    void *r8;
    void *r9;
    void *r10;
    void *r11;
    // The abi does not expect r12 (ip) to be preserved
    void *lr;

    // Arguments.
    // code == r0
    // argc == r1
    // argv == r2
    // frame == r3
    CalleeToken token;
    JSObject *scopeChain;
    size_t numStackValues;
    Value *vp;
};

/*
 * This method generates a trampoline for a c++ function with the following
 * signature:
 *   void enter(void *code, int argc, Value *argv, StackFrame *fp, CalleeToken
 *              calleeToken, JSObject *scopeChain, Value *vp)
 *   ...using standard EABI calling convention
 */
IonCode *
JitRuntime::generateEnterJIT(JSContext *cx, EnterJitType type)
{
    const Address slot_token(sp, offsetof(EnterJITStack, token));
    const Address slot_vp(sp, offsetof(EnterJITStack, vp));

    JS_ASSERT(OsrFrameReg == r3);

    MacroAssembler masm(cx);
    AutoFlushCache afc("GenerateEnterJIT", cx->runtime()->jitRuntime());
    Assembler *aasm = &masm;

    // Save non-volatile registers. These must be saved by the trampoline,
    // rather than the JIT'd code, because they are scanned by the conservative
    // scanner.
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    masm.transferReg(r0); // [sp,0]
    masm.transferReg(r4); // [sp,4]
    masm.transferReg(r5); // [sp,8]
    masm.transferReg(r6); // [sp,12]
    masm.transferReg(r7); // [sp,16]
    masm.transferReg(r8); // [sp,20]
    masm.transferReg(r9); // [sp,24]
    masm.transferReg(r10); // [sp,28]
    masm.transferReg(r11); // [sp,32]
    // The abi does not expect r12 (ip) to be preserved
    masm.transferReg(lr);  // [sp,36]
    // The 5th argument is located at [sp, 40]
    masm.finishDataTransfer();
    masm.transferMultipleByRuns(NonVolatileFloatRegs, IsStore, sp, DB);

    // Save stack pointer into r8
    masm.movePtr(sp, r8);

    // Load calleeToken into r9.
    masm.loadPtr(slot_token, r9);

    // Save stack pointer.
    if (type == EnterJitBaseline)
        masm.movePtr(sp, r11);

    // Load the number of actual arguments into r10.
    masm.loadPtr(slot_vp, r10);
    masm.unboxInt32(Address(r10, 0), r10);

#if 0
    // This is in case we want to go back to using frames that
    // aren't 8 byte alinged
    // there are r1 2-word arguments to the js code
    // we want 2 word alignment, so this shouldn't matter.
    // After the arguments have been pushed, we want to push an additional 3 words of
    // data, so in all, we want to decrease sp by 4 if it is currently aligned to
    // 8, and not touch it otherwise
    aasm->as_sub(sp, sp, Imm8(4));
    aasm->as_orr(sp, sp, Imm8(4));
#endif

    // Subtract off the size of the arguments from the stack pointer, store elsewhere
    aasm->as_sub(r4, sp, O2RegImmShift(r1, LSL, 3)); //r4 = sp - argc*8
    // Get the final position of the stack pointer into the stack pointer
    aasm->as_sub(sp, r4, Imm8(16)); // sp' = sp - argc*8 - 16
    // Get a copy of the number of args to use as a decrement counter, also
    // Set the zero condition code
    aasm->as_mov(r5, O2Reg(r1), SetCond);

    // Loop over arguments, copying them from an unknown buffer onto the Ion
    // stack so they can be accessed from JIT'ed code.
    {
        Label header, footer;
        // If there aren't any arguments, don't do anything
        aasm->as_b(&footer, Assembler::Zero);
        // Get the top of the loop
        masm.bind(&header);
        aasm->as_sub(r5, r5, Imm8(1), SetCond);
        // We could be more awesome, and unroll this, using a loadm
        // (particularly since the offset is effectively 0)
        // but that seems more error prone, and complex.
        // BIG FAT WARNING: this loads both r6 and r7.
        aasm->as_extdtr(IsLoad,  64, true, PostIndex, r6, EDtrAddr(r2, EDtrOffImm(8)));
        aasm->as_extdtr(IsStore, 64, true, PostIndex, r6, EDtrAddr(r4, EDtrOffImm(8)));
        aasm->as_b(&header, Assembler::NonZero);
        masm.bind(&footer);
    }

    masm.ma_sub(r8, sp, r8);
    masm.makeFrameDescriptor(r8, IonFrame_Entry);

    masm.startDataTransferM(IsStore, sp, IB, NoWriteBack);
                           // [sp]    = return address (written later)
    masm.transferReg(r8);  // [sp',4] = descriptor, argc*8+20
    masm.transferReg(r9);  // [sp',8]  = callee token
    masm.transferReg(r10); // [sp',12]  = actual arguments
    masm.finishDataTransfer();

    Label returnLabel;
    if (type == EnterJitBaseline) {
        // Handle OSR.
        GeneralRegisterSet regs(GeneralRegisterSet::All());
        regs.take(JSReturnOperand);
        regs.takeUnchecked(OsrFrameReg);
        regs.take(r11);
        regs.take(ReturnReg);

        const Address slot_numStackValues(r11, offsetof(EnterJITStack, numStackValues));

        Label notOsr;
        masm.branchTestPtr(Assembler::Zero, OsrFrameReg, OsrFrameReg, &notOsr);

        Register scratch = regs.takeAny();

        Register numStackValues = regs.takeAny();
        masm.load32(slot_numStackValues, numStackValues);

        // Write return address. On ARM, CodeLabel is only used for tableswitch,
        // so we can't use it here to get the return address. Instead, we use
        // pc + a fixed offset to a jump to returnLabel. The pc register holds
        // pc + 8, so we add the size of 2 instructions to skip the instructions
        // emitted by storePtr and jump(&skipJump).
        {
            AutoForbidPools afp(&masm);
            Label skipJump;
            masm.mov(pc, scratch);
            masm.addPtr(Imm32(2 * sizeof(uint32_t)), scratch);
            masm.storePtr(scratch, Address(sp, 0));
            masm.jump(&skipJump);
            masm.jump(&returnLabel);
            masm.bind(&skipJump);
        }

        // Push previous frame pointer.
        masm.push(r11);

        // Reserve frame.
        Register framePtr = r11;
        masm.subPtr(Imm32(BaselineFrame::Size()), sp);
        masm.mov(sp, framePtr);

#ifdef XP_WIN
        // Can't push large frames blindly on windows.  Touch frame memory incrementally.
        masm.ma_lsl(Imm32(3), numStackValues, scratch);
        masm.subPtr(scratch, framePtr);
        {
            masm.ma_sub(sp, Imm32(WINDOWS_BIG_FRAME_TOUCH_INCREMENT), scratch);

            Label touchFrameLoop;
            Label touchFrameLoopEnd;
            masm.bind(&touchFrameLoop);
            masm.branchPtr(Assembler::Below, scratch, framePtr, &touchFrameLoopEnd);
            masm.store32(Imm32(0), Address(scratch, 0));
            masm.subPtr(Imm32(WINDOWS_BIG_FRAME_TOUCH_INCREMENT), scratch);
            masm.jump(&touchFrameLoop);
            masm.bind(&touchFrameLoopEnd);
        }
        masm.mov(sp, framePtr);
#endif

        // Reserve space for locals and stack values.
        masm.ma_lsl(Imm32(3), numStackValues, scratch);
        masm.ma_sub(sp, scratch, sp);

        // Enter exit frame.
        masm.addPtr(Imm32(BaselineFrame::Size() + BaselineFrame::FramePointerOffset), scratch);
        masm.makeFrameDescriptor(scratch, IonFrame_BaselineJS);
        masm.push(scratch);
        masm.push(Imm32(0)); // Fake return address.
        masm.enterFakeExitFrame();

        masm.push(framePtr); // BaselineFrame
        masm.push(r0); // jitcode

        masm.setupUnalignedABICall(3, scratch);
        masm.passABIArg(r11); // BaselineFrame
        masm.passABIArg(OsrFrameReg); // StackFrame
        masm.passABIArg(numStackValues);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, jit::InitBaselineFrameForOsr));

        Register jitcode = regs.takeAny();
        masm.pop(jitcode);
        masm.pop(framePtr);

        JS_ASSERT(jitcode != ReturnReg);

        Label error;
        masm.addPtr(Imm32(IonExitFrameLayout::SizeWithFooter()), sp);
        masm.addPtr(Imm32(BaselineFrame::Size()), framePtr);
        masm.branchIfFalseBool(ReturnReg, &error);

        masm.jump(jitcode);

        // OOM: load error value, discard return address and previous frame
        // pointer and return.
        masm.bind(&error);
        masm.mov(framePtr, sp);
        masm.addPtr(Imm32(2 * sizeof(uintptr_t)), sp);
        masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
        masm.jump(&returnLabel);

        masm.bind(&notOsr);
        // Load the scope chain in R1.
        JS_ASSERT(R1.scratchReg() != r0);
        masm.loadPtr(Address(r11, offsetof(EnterJITStack, scopeChain)), R1.scratchReg());
    }

    // Call the function.
    masm.ma_callIonNoPush(r0);

    if (type == EnterJitBaseline) {
        // Baseline OSR will return here.
        masm.bind(&returnLabel);
    }

    // The top of the stack now points to the address of the field following
    // the return address because the return address is popped for the
    // return, so we need to remove the size of the return address field.
    aasm->as_sub(sp, sp, Imm8(4));

    // Load off of the stack the size of our local stack
    masm.loadPtr(Address(sp, IonJSFrameLayout::offsetOfDescriptor()), r5);
    aasm->as_add(sp, sp, lsr(r5, FRAMESIZE_SHIFT));

    // Store the returned value into the slot_vp
    masm.loadPtr(slot_vp, r5);
    masm.storeValue(JSReturnOperand, Address(r5, 0));

    // :TODO: Optimize storeValue with:
    // We're using a load-double here.  In order for that to work,
    // the data needs to be stored in two consecutive registers,
    // make sure this is the case
    //   ASSERT(JSReturnReg_Type.code() == JSReturnReg_Data.code()+1);
    //   aasm->as_extdtr(IsStore, 64, true, Offset,
    //                   JSReturnReg_Data, EDtrAddr(r5, EDtrOffImm(0)));

    // Restore non-volatile registers and return.
    GenerateReturn(masm, true);

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "EnterJIT");
#endif

    return code;
}

IonCode *
JitRuntime::generateInvalidator(JSContext *cx)
{
    // See large comment in x86's JitRuntime::generateInvalidator.
    MacroAssembler masm(cx);
    //masm.as_bkpt();
    // At this point, one of two things has happened.
    // 1) Execution has just returned from C code, which left the stack aligned
    // 2) Execution has just returned from Ion code, which left the stack unaligned.
    // The old return address should not matter, but we still want the
    // stack to be aligned, and there is no good reason to automatically align it with
    // a call to setupUnalignedABICall.
    masm.ma_and(Imm32(~7), sp, sp);
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    // We don't have to push everything, but this is likely easier.
    // setting regs_
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.transferReg(Register::FromCode(i));
    masm.finishDataTransfer();

    masm.startFloatTransferM(IsStore, sp, DB, WriteBack);
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.transferFloatReg(FloatRegister::FromCode(i));
    masm.finishFloatTransfer();

    masm.ma_mov(sp, r0);
    const int sizeOfRetval = sizeof(size_t)*2;
    masm.reserveStack(sizeOfRetval);
    masm.mov(sp, r1);
    const int sizeOfBailoutInfo = sizeof(void *)*2;
    masm.reserveStack(sizeOfBailoutInfo);
    masm.mov(sp, r2);
    masm.setupAlignedABICall(3);
    masm.passABIArg(r0);
    masm.passABIArg(r1);
    masm.passABIArg(r2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InvalidationBailout));

    masm.ma_ldr(Address(sp, 0), r2);
    masm.ma_ldr(Address(sp, sizeOfBailoutInfo), r1);
    // Remove the return address, the IonScript, the register state
    // (InvaliationBailoutStack) and the space that was allocated for the return value
    masm.ma_add(sp, Imm32(sizeof(InvalidationBailoutStack) + sizeOfRetval + sizeOfBailoutInfo), sp);
    // remove the space that this frame was using before the bailout
    // (computed by InvalidationBailout)
    masm.ma_add(sp, r1, sp);

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in r2.
    IonCode *bailoutTail = cx->runtime()->jitRuntime()->getBailoutTail();
    masm.branch(bailoutTail);

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);
    IonSpew(IonSpew_Invalidate, "   invalidation thunk created at %p", (void *) code->raw());

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "Invalidator");
#endif

    return code;
}

IonCode *
JitRuntime::generateArgumentsRectifier(JSContext *cx, ExecutionMode mode, void **returnAddrOut)
{
    MacroAssembler masm(cx);
    // ArgumentsRectifierReg contains the |nargs| pushed onto the current frame.
    // Including |this|, there are (|nargs| + 1) arguments to copy.
    JS_ASSERT(ArgumentsRectifierReg == r8);

    // Copy number of actual arguments into r0
    masm.ma_ldr(DTRAddr(sp, DtrOffImm(IonRectifierFrameLayout::offsetOfNumActualArgs())), r0);

    // Load the number of |undefined|s to push into r6.
    masm.ma_ldr(DTRAddr(sp, DtrOffImm(IonRectifierFrameLayout::offsetOfCalleeToken())), r1);
    masm.ma_ldrh(EDtrAddr(r1, EDtrOffImm(offsetof(JSFunction, nargs))), r6);

    masm.ma_sub(r6, r8, r2);

    masm.moveValue(UndefinedValue(), r5, r4);

    masm.ma_mov(sp, r3); // Save %sp.
    masm.ma_mov(sp, r7); // Save %sp again.

    // Push undefined.
    {
        Label undefLoopTop;
        masm.bind(&undefLoopTop);
        masm.ma_dataTransferN(IsStore, 64, true, sp, Imm32(-8), r4, PreIndex);
        masm.ma_sub(r2, Imm32(1), r2, SetCond);

        masm.ma_b(&undefLoopTop, Assembler::NonZero);
    }

    // Get the topmost argument.

    masm.ma_alu(r3, lsl(r8, 3), r3, op_add); // r3 <- r3 + nargs * 8
    masm.ma_add(r3, Imm32(sizeof(IonRectifierFrameLayout)), r3);

    // Push arguments, |nargs| + 1 times (to include |this|).
    {
        Label copyLoopTop;
        masm.bind(&copyLoopTop);
        masm.ma_dataTransferN(IsLoad, 64, true, r3, Imm32(-8), r4, PostIndex);
        masm.ma_dataTransferN(IsStore, 64, true, sp, Imm32(-8), r4, PreIndex);

        masm.ma_sub(r8, Imm32(1), r8, SetCond);
        masm.ma_b(&copyLoopTop, Assembler::Unsigned);
    }

    // translate the framesize from values into bytes
    masm.ma_add(r6, Imm32(1), r6);
    masm.ma_lsl(Imm32(3), r6, r6);

    // Construct sizeDescriptor.
    masm.makeFrameDescriptor(r6, IonFrame_Rectifier);

    // Construct IonJSFrameLayout.
    masm.ma_push(r0); // actual arguments.
    masm.ma_push(r1); // callee token
    masm.ma_push(r6); // frame descriptor.

    // Call the target function.
    // Note that this code assumes the function is JITted.
    masm.ma_ldr(DTRAddr(r1, DtrOffImm(JSFunction::offsetOfNativeOrScript())), r3);
    masm.loadBaselineOrIonRaw(r3, r3, mode, nullptr);
    masm.ma_callIonHalfPush(r3);

    uint32_t returnOffset = masm.currentOffset();

    // arg1
    //  ...
    // argN
    // num actual args
    // callee token
    // sizeDescriptor     <- sp now
    // return address

    // Remove the rectifier frame.
    masm.ma_dtr(IsLoad, sp, Imm32(12), r4, PostIndex);

    // arg1
    //  ...
    // argN               <- sp now; r4 <- frame descriptor
    // num actual args
    // callee token
    // sizeDescriptor
    // return address

    // Discard pushed arguments.
    masm.ma_alu(sp, lsr(r4, FRAMESIZE_SHIFT), sp, op_add);

    masm.ret();
    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

    CodeOffsetLabel returnLabel(returnOffset);
    returnLabel.fixup(&masm);
    if (returnAddrOut)
        *returnAddrOut = (void *) (code->raw() + returnLabel.offset());

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "ArgumentsRectifier");
#endif

    return code;
}

static void
GenerateBailoutThunk(JSContext *cx, MacroAssembler &masm, uint32_t frameClass)
{
    // the stack should look like:
    // [IonFrame]
    // bailoutFrame.registersnapshot
    // bailoutFrame.fpsnapshot
    // bailoutFrame.snapshotOffset
    // bailoutFrame.frameSize

    // STEP 1a: save our register sets to the stack so Bailout() can
    // read everything.
    // sp % 8 == 0
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    // We don't have to push everything, but this is likely easier.
    // setting regs_
    for (uint32_t i = 0; i < Registers::Total; i++)
        masm.transferReg(Register::FromCode(i));
    masm.finishDataTransfer();

    masm.startFloatTransferM(IsStore, sp, DB, WriteBack);
    for (uint32_t i = 0; i < FloatRegisters::Total; i++)
        masm.transferFloatReg(FloatRegister::FromCode(i));
    masm.finishFloatTransfer();

    // STEP 1b: Push both the "return address" of the function call (the
    //          address of the instruction after the call that we used to get
    //          here) as well as the callee token onto the stack.  The return
    //          address is currently in r14.  We will proceed by loading the
    //          callee token into a sacrificial register <= r14, then pushing
    //          both onto the stack

    // now place the frameClass onto the stack, via a register
    masm.ma_mov(Imm32(frameClass), r4);
    // And onto the stack.  Since the stack is full, we need to put this
    // one past the end of the current stack. Sadly, the ABI says that we need
    // to always point to the lowest place that has been written.  the OS is
    // free to do whatever it wants below sp.
    masm.startDataTransferM(IsStore, sp, DB, WriteBack);
    // set frameClassId_
    masm.transferReg(r4);
    // Set tableOffset_; higher registers are stored at higher locations on
    // the stack.
    masm.transferReg(lr);
    masm.finishDataTransfer();

    // SP % 8 == 4
    // STEP 1c: Call the bailout function, giving a pointer to the
    //          structure we just blitted onto the stack
    masm.ma_mov(sp, r0);
    const int sizeOfBailoutInfo = sizeof(void *)*2;
    masm.reserveStack(sizeOfBailoutInfo);
    masm.mov(sp, r1);
    masm.setupAlignedABICall(2);

    // Decrement sp by another 4, so we keep alignment
    // Not Anymore!  pushing both the snapshotoffset as well as the
    // masm.as_sub(sp, sp, Imm8(4));

    // Set the old (4-byte aligned) value of the sp as the first argument
    masm.passABIArg(r0);
    masm.passABIArg(r1);

    // Sp % 8 == 0
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, Bailout));
    masm.ma_ldr(Address(sp, 0), r2);
    masm.ma_add(sp, Imm32(sizeOfBailoutInfo), sp);
    // Common size of a bailout frame.
    uint32_t bailoutFrameSize = sizeof(void *) + // frameClass
                              sizeof(double) * FloatRegisters::Total +
                              sizeof(void *) * Registers::Total;

    if (frameClass == NO_FRAME_SIZE_CLASS_ID) {
        // Make sure the bailout frame size fits into the offset for a load
        masm.as_dtr(IsLoad, 32, Offset,
                    r4, DTRAddr(sp, DtrOffImm(4)));
        // used to be: offsetof(BailoutStack, frameSize_)
        // this structure is no longer available to us :(
        // We add 12 to the bailoutFrameSize because:
        // sizeof(uint32_t) for the tableOffset that was pushed onto the stack
        // sizeof(uintptr_t) for the snapshotOffset;
        // alignment to round the uintptr_t up to a multiple of 8 bytes.
        masm.ma_add(sp, Imm32(bailoutFrameSize+12), sp);
        masm.as_add(sp, sp, O2Reg(r4));
    } else {
        uint32_t frameSize = FrameSizeClass::FromClass(frameClass).frameSize();
        masm.ma_add(Imm32(frameSize // the frame that was added when we entered the most recent function
                          + sizeof(void*) // the size of the "return address" that was dumped on the stack
                          + bailoutFrameSize) // everything else that was pushed on the stack
                    , sp);
    }

    // Jump to shared bailout tail. The BailoutInfo pointer has to be in r2.
    IonCode *bailoutTail = cx->runtime()->jitRuntime()->getBailoutTail();
    masm.branch(bailoutTail);
}

IonCode *
JitRuntime::generateBailoutTable(JSContext *cx, uint32_t frameClass)
{
    MacroAssembler masm(cx);

    Label bailout;
    for (size_t i = 0; i < BAILOUT_TABLE_SIZE; i++)
        masm.ma_bl(&bailout);
    masm.bind(&bailout);

    GenerateBailoutThunk(cx, masm, frameClass);

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutTable");
#endif

    return code;
}

IonCode *
JitRuntime::generateBailoutHandler(JSContext *cx)
{
    MacroAssembler masm(cx);
    GenerateBailoutThunk(cx, masm, NO_FRAME_SIZE_CLASS_ID);

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutHandler");
#endif

    return code;
}

IonCode *
JitRuntime::generateVMWrapper(JSContext *cx, const VMFunction &f)
{
    typedef MoveResolver::MoveOperand MoveOperand;

    JS_ASSERT(functionWrappers_);
    JS_ASSERT(functionWrappers_->initialized());
    VMWrapperMap::AddPtr p = functionWrappers_->lookupForAdd(&f);
    if (p)
        return p->value;

    // Generate a separated code for the wrapper.
    MacroAssembler masm(cx);
    GeneralRegisterSet regs = GeneralRegisterSet(Register::Codes::WrapperMask);

    // Wrapper register set is a superset of Volatile register set.
    JS_STATIC_ASSERT((Register::Codes::VolatileMask & ~Register::Codes::WrapperMask) == 0);

    // The context is the first argument; r0 is the first argument register.
    Register cxreg = r0;
    regs.take(cxreg);

    // Stack is:
    //    ... frame ...
    //  +8  [args] + argPadding
    //  +0  ExitFrame
    //
    // We're aligned to an exit frame, so link it up.
    masm.enterExitFrameAndLoadContext(&f, cxreg, regs.getAny(), f.executionMode);

    // Save the base of the argument set stored on the stack.
    Register argsBase = InvalidReg;
    if (f.explicitArgs) {
        argsBase = r5;
        regs.take(argsBase);
        masm.ma_add(sp, Imm32(IonExitFrameLayout::SizeWithFooter()), argsBase);
    }

    // Reserve space for the outparameter.
    Register outReg = InvalidReg;
    switch (f.outParam) {
      case Type_Value:
        outReg = r4;
        regs.take(outReg);
        masm.reserveStack(sizeof(Value));
        masm.ma_mov(sp, outReg);
        break;

      case Type_Handle:
        outReg = r4;
        regs.take(outReg);
        masm.PushEmptyRooted(f.outParamRootType);
        masm.ma_mov(sp, outReg);
        break;

      case Type_Int32:
      case Type_Pointer:
      case Type_Bool:
        outReg = r4;
        regs.take(outReg);
        masm.reserveStack(sizeof(int32_t));
        masm.ma_mov(sp, outReg);
        break;

      case Type_Double:
        outReg = r4;
        regs.take(outReg);
        masm.reserveStack(sizeof(double));
        masm.ma_mov(sp, outReg);
        break;

      default:
        JS_ASSERT(f.outParam == Type_Void);
        break;
    }

    masm.setupUnalignedABICall(f.argc(), regs.getAny());
    masm.passABIArg(cxreg);

    size_t argDisp = 0;

    // Copy any arguments.
    for (uint32_t explicitArg = 0; explicitArg < f.explicitArgs; explicitArg++) {
        MoveOperand from;
        switch (f.argProperties(explicitArg)) {
          case VMFunction::WordByValue:
            masm.passABIArg(MoveOperand(argsBase, argDisp));
            argDisp += sizeof(void *);
            break;
          case VMFunction::DoubleByValue:
            // Values should be passed by reference, not by value, so we
            // assert that the argument is a double-precision float.
            JS_ASSERT(f.argPassedInFloatReg(explicitArg));
            masm.passABIArg(MoveOperand(argsBase, argDisp, MoveOperand::FLOAT));
            argDisp += sizeof(double);
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

    // Copy the implicit outparam, if any.
    if (outReg != InvalidReg)
        masm.passABIArg(outReg);

    masm.callWithABI(f.wrapped);

    // Test for failure.
    switch (f.failType()) {
      case Type_Object:
      case Type_Bool:
        // Called functions return bools, which are 0/false and non-zero/true
        masm.branch32(Assembler::Equal, r0, Imm32(0), masm.failureLabel(f.executionMode));
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
        masm.loadValue(Address(sp, 0), JSReturnOperand);
        masm.freeStack(sizeof(Value));
        break;

      case Type_Int32:
      case Type_Pointer:
        masm.load32(Address(sp, 0), ReturnReg);
        masm.freeStack(sizeof(int32_t));
        break;

      case Type_Bool:
        masm.load8ZeroExtend(Address(sp, 0), ReturnReg);
        masm.freeStack(sizeof(int32_t));
        break;

      case Type_Double:
        if (cx->runtime()->jitSupportsFloatingPoint)
            masm.loadDouble(Address(sp, 0), ReturnFloatReg);
        else
            masm.breakpoint();
        masm.freeStack(sizeof(double));
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
    IonCode *wrapper = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);
    if (!wrapper)
        return nullptr;

    // linker.newCode may trigger a GC and sweep functionWrappers_ so we have to
    // use relookupOrAdd instead of add.
    if (!functionWrappers_->relookupOrAdd(p, &f, wrapper))
        return nullptr;

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(wrapper, "VMWrapper");
#endif

    return wrapper;
}

IonCode *
JitRuntime::generatePreBarrier(JSContext *cx, MIRType type)
{
    MacroAssembler masm(cx);

    RegisterSet save;
    if (cx->runtime()->jitSupportsFloatingPoint) {
        save = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                           FloatRegisterSet(FloatRegisters::VolatileMask));
    } else {
        save = RegisterSet(GeneralRegisterSet(Registers::VolatileMask),
                           FloatRegisterSet());
    }
    masm.PushRegsInMask(save);

    JS_ASSERT(PreBarrierReg == r1);
    masm.movePtr(ImmPtr(cx->runtime()), r0);

    masm.setupUnalignedABICall(2, r2);
    masm.passABIArg(r0);
    masm.passABIArg(r1);
    if (type == MIRType_Value) {
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkValueFromIon));
    } else {
        JS_ASSERT(type == MIRType_Shape);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, MarkShapeFromIon));
    }

    masm.PopRegsInMask(save);
    masm.ret();

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "PreBarrier");
#endif

    return code;
}

typedef bool (*HandleDebugTrapFn)(JSContext *, BaselineFrame *, uint8_t *, bool *);
static const VMFunction HandleDebugTrapInfo = FunctionInfo<HandleDebugTrapFn>(HandleDebugTrap);

IonCode *
JitRuntime::generateDebugTrapHandler(JSContext *cx)
{
    MacroAssembler masm;

    Register scratch1 = r0;
    Register scratch2 = r1;

    // Load BaselineFrame pointer in scratch1.
    masm.mov(r11, scratch1);
    masm.subPtr(Imm32(BaselineFrame::Size()), scratch1);

    // Enter a stub frame and call the HandleDebugTrap VM function. Ensure
    // the stub frame has a nullptr ICStub pointer, since this pointer is
    // marked during GC.
    masm.movePtr(ImmPtr(nullptr), BaselineStubReg);
    EmitEnterStubFrame(masm, scratch2);

    IonCode *code = cx->runtime()->jitRuntime()->getVMWrapper(HandleDebugTrapInfo);
    if (!code)
        return nullptr;

    masm.push(lr);
    masm.push(scratch1);
    EmitCallVM(code, masm);

    EmitLeaveStubFrame(masm);

    // If the stub returns |true|, we have to perform a forced return
    // (return from the JS frame). If the stub returns |false|, just return
    // from the trap stub so that execution continues at the current pc.
    Label forcedReturn;
    masm.branchTest32(Assembler::NonZero, ReturnReg, ReturnReg, &forcedReturn);
    masm.mov(lr, pc);

    masm.bind(&forcedReturn);
    masm.loadValue(Address(r11, BaselineFrame::reverseOffsetOfReturnValue()),
                   JSReturnOperand);
    masm.mov(r11, sp);
    masm.pop(r11);
    masm.ret();

    Linker linker(masm);
    IonCode *codeDbg = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(codeDbg, "DebugTrapHandler");
#endif

    return codeDbg;
}

IonCode *
JitRuntime::generateExceptionTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.handleFailureWithHandlerTail();

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "ExceptionTailStub");
#endif

    return code;
}

IonCode *
JitRuntime::generateBailoutTailStub(JSContext *cx)
{
    MacroAssembler masm;

    masm.generateBailoutTail(r1, r2);

    Linker linker(masm);
    IonCode *code = linker.newCode<NoGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerIonCodeProfile(code, "BailoutTailStub");
#endif

    return code;
}
