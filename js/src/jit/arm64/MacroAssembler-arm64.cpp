/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/MacroAssembler-arm64.h"

#include "jit/arm64/MoveEmitter-arm64.h"
#include "jit/arm64/SharedICRegisters-arm64.h"
#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace jit {

void
MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output)
{
    ARMRegister dest(output, 32);
    Fcvtns(dest, ARMFPRegister(input, 64));

    {
        vixl::UseScratchRegisterScope temps(this);
        const ARMRegister scratch32 = temps.AcquireW();

        Mov(scratch32, Operand(0xff));
        Cmp(dest, scratch32);
        Csel(dest, dest, scratch32, LessThan);
    }

    Cmp(dest, Operand(0));
    Csel(dest, dest, wzr, GreaterThan);
}

void
MacroAssemblerCompat::buildFakeExitFrame(Register scratch, uint32_t* offset)
{
    mozilla::DebugOnly<uint32_t> initialDepth = framePushed();
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);

    asMasm().Push(Imm32(descriptor)); // descriptor_

    enterNoPool(3);
    Label fakeCallsite;
    Adr(ARMRegister(scratch, 64), &fakeCallsite);
    asMasm().Push(scratch);
    bind(&fakeCallsite);
    uint32_t pseudoReturnOffset = currentOffset();
    leaveNoPool();

    MOZ_ASSERT(framePushed() == initialDepth + ExitFrameLayout::Size());

    *offset = pseudoReturnOffset;
}

void
MacroAssemblerCompat::callWithExitFrame(Label* target)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);
    Push(Imm32(descriptor)); // descriptor
    asMasm().call(target);
}

void
MacroAssemblerCompat::callWithExitFrame(JitCode* target)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), JitFrame_IonJS);
    asMasm().Push(Imm32(descriptor));
    asMasm().call(target);
}

void
MacroAssemblerCompat::callWithExitFrame(JitCode* target, Register dynStack)
{
    add32(Imm32(framePushed()), dynStack);
    makeFrameDescriptor(dynStack, JitFrame_IonJS);
    Push(dynStack); // descriptor
    asMasm().call(target);
}

void
MacroAssembler::alignFrameForICArguments(MacroAssembler::AfterICSaveLive& aic)
{
    // Exists for MIPS compatibility.
}

void
MacroAssembler::restoreFrameAlignmentForICArguments(MacroAssembler::AfterICSaveLive& aic)
{
    // Exists for MIPS compatibility.
}

js::jit::MacroAssembler&
MacroAssemblerCompat::asMasm()
{
    return *static_cast<js::jit::MacroAssembler*>(this);
}

const js::jit::MacroAssembler&
MacroAssemblerCompat::asMasm() const
{
    return *static_cast<const js::jit::MacroAssembler*>(this);
}

vixl::MacroAssembler&
MacroAssemblerCompat::asVIXL()
{
    return *static_cast<vixl::MacroAssembler*>(this);
}

const vixl::MacroAssembler&
MacroAssemblerCompat::asVIXL() const
{
    return *static_cast<const vixl::MacroAssembler*>(this);
}

BufferOffset
MacroAssemblerCompat::movePatchablePtr(ImmPtr ptr, Register dest)
{
    const size_t numInst = 1; // Inserting one load instruction.
    const unsigned numPoolEntries = 2; // Every pool entry is 4 bytes.
    uint8_t* literalAddr = (uint8_t*)(&ptr.value); // TODO: Should be const.

    // Scratch space for generating the load instruction.
    //
    // allocEntry() will use InsertIndexIntoTag() to store a temporary
    // index to the corresponding PoolEntry in the instruction itself.
    //
    // That index will be fixed up later when finishPool()
    // walks over all marked loads and calls PatchConstantPoolLoad().
    uint32_t instructionScratch = 0;

    // Emit the instruction mask in the scratch space.
    // The offset doesn't matter: it will be fixed up later.
    vixl::Assembler::ldr((Instruction*)&instructionScratch, ARMRegister(dest, 64), 0);

    // Add the entry to the pool, fix up the LDR imm19 offset,
    // and add the completed instruction to the buffer.
    return armbuffer_.allocEntry(numInst, numPoolEntries,
                                 (uint8_t*)&instructionScratch, literalAddr);
}

BufferOffset
MacroAssemblerCompat::movePatchablePtr(ImmWord ptr, Register dest)
{
    const size_t numInst = 1; // Inserting one load instruction.
    const unsigned numPoolEntries = 2; // Every pool entry is 4 bytes.
    uint8_t* literalAddr = (uint8_t*)(&ptr.value);

    // Scratch space for generating the load instruction.
    //
    // allocEntry() will use InsertIndexIntoTag() to store a temporary
    // index to the corresponding PoolEntry in the instruction itself.
    //
    // That index will be fixed up later when finishPool()
    // walks over all marked loads and calls PatchConstantPoolLoad().
    uint32_t instructionScratch = 0;

    // Emit the instruction mask in the scratch space.
    // The offset doesn't matter: it will be fixed up later.
    vixl::Assembler::ldr((Instruction*)&instructionScratch, ARMRegister(dest, 64), 0);

    // Add the entry to the pool, fix up the LDR imm19 offset,
    // and add the completed instruction to the buffer.
    return armbuffer_.allocEntry(numInst, numPoolEntries,
                                 (uint8_t*)&instructionScratch, literalAddr);
}

void
MacroAssemblerCompat::handleFailureWithHandlerTail(void* handler)
{
    // Reserve space for exception information.
    int64_t size = (sizeof(ResumeFromException) + 7) & ~7;
    Sub(GetStackPointer64(), GetStackPointer64(), Operand(size));
    if (!GetStackPointer64().Is(sp))
        Mov(sp, GetStackPointer64());

    Mov(x0, GetStackPointer64());

    // Call the handler.
    setupUnalignedABICall(1, r1);
    passABIArg(r0);
    callWithABI(handler);

    Label entryFrame;
    Label catch_;
    Label finally;
    Label return_;
    Label bailout;

    MOZ_ASSERT(GetStackPointer64().Is(x28)); // Lets the code below be a little cleaner.

    loadPtr(Address(r28, offsetof(ResumeFromException, kind)), r0);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_ENTRY_FRAME), &entryFrame);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_CATCH), &catch_);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_FINALLY), &finally);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_FORCED_RETURN), &return_);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_BAILOUT), &bailout);

    breakpoint(); // Invalid kind.

    // No exception handler. Load the error value, load the new stack pointer,
    // and return from the entry frame.
    bind(&entryFrame);
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    loadPtr(Address(r28, offsetof(ResumeFromException, stackPointer)), r28);
    retn(Imm32(1 * sizeof(void*))); // Pop from stack and return.

    // If we found a catch handler, this must be a baseline frame. Restore state
    // and jump to the catch block.
    bind(&catch_);
    loadPtr(Address(r28, offsetof(ResumeFromException, target)), r0);
    loadPtr(Address(r28, offsetof(ResumeFromException, framePointer)), BaselineFrameReg);
    loadPtr(Address(r28, offsetof(ResumeFromException, stackPointer)), r28);
    syncStackPtr();
    Br(x0);

    // If we found a finally block, this must be a baseline frame.
    // Push two values expected by JSOP_RETSUB: BooleanValue(true)
    // and the exception.
    bind(&finally);
    ARMRegister exception = x1;
    Ldr(exception, MemOperand(GetStackPointer64(), offsetof(ResumeFromException, exception)));
    Ldr(x0, MemOperand(GetStackPointer64(), offsetof(ResumeFromException, target)));
    Ldr(ARMRegister(BaselineFrameReg, 64),
        MemOperand(GetStackPointer64(), offsetof(ResumeFromException, framePointer)));
    Ldr(GetStackPointer64(), MemOperand(GetStackPointer64(), offsetof(ResumeFromException, stackPointer)));
    syncStackPtr();
    pushValue(BooleanValue(true));
    push(exception);
    Br(x0);

    // Only used in debug mode. Return BaselineFrame->returnValue() to the caller.
    bind(&return_);
    loadPtr(Address(r28, offsetof(ResumeFromException, framePointer)), BaselineFrameReg);
    loadPtr(Address(r28, offsetof(ResumeFromException, stackPointer)), r28);
    loadValue(Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfReturnValue()),
              JSReturnOperand);
    movePtr(BaselineFrameReg, r28);
    vixl::MacroAssembler::Pop(ARMRegister(BaselineFrameReg, 64), vixl::lr);
    syncStackPtr();
    vixl::MacroAssembler::Ret(vixl::lr);

    // If we are bailing out to baseline to handle an exception,
    // jump to the bailout tail stub.
    bind(&bailout);
    Ldr(x2, MemOperand(GetStackPointer64(), offsetof(ResumeFromException, bailoutInfo)));
    Ldr(x1, MemOperand(GetStackPointer64(), offsetof(ResumeFromException, target)));
    Mov(x0, BAILOUT_RETURN_OK);
    Br(x1);
}

void
MacroAssemblerCompat::setupABICall(uint32_t args)
{
    MOZ_ASSERT(!inCall_);
    inCall_ = true;

    args_ = args;
    usedOutParam_ = false;
    passedIntArgs_ = 0;
    passedFloatArgs_ = 0;
    passedArgTypes_ = 0;
    stackForCall_ = ShadowStackSpace;
}

void
MacroAssemblerCompat::setupUnalignedABICall(uint32_t args, Register scratch)
{
    setupABICall(args);
    dynamicAlignment_ = true;

    int64_t alignment = ~(int64_t(ABIStackAlignment) - 1);
    ARMRegister scratch64(scratch, 64);

    // Always save LR -- Baseline ICs assume that LR isn't modified.
    push(lr);

    // Unhandled for sp -- needs slightly different logic.
    MOZ_ASSERT(!GetStackPointer64().Is(sp));

    // Remember the stack address on entry.
    Mov(scratch64, GetStackPointer64());

    // Make alignment, including the effective push of the previous sp.
    Sub(GetStackPointer64(), GetStackPointer64(), Operand(8));
    And(GetStackPointer64(), GetStackPointer64(), Operand(alignment));

    // If the PseudoStackPointer is used, sp must be <= psp before a write is valid.
    syncStackPtr();

    // Store previous sp to the top of the stack, aligned.
    Str(scratch64, MemOperand(GetStackPointer64(), 0));
}

void
MacroAssemblerCompat::passABIArg(const MoveOperand& from, MoveOp::Type type)
{
    if (!enoughMemory_)
        return;

    Register activeSP = Register::FromCode(GetStackPointer64().code());
    if (type == MoveOp::GENERAL) {
        Register dest;
        passedArgTypes_ = (passedArgTypes_ << ArgType_Shift) | ArgType_General;
        if (GetIntArgReg(passedIntArgs_++, passedFloatArgs_, &dest)) {
            if (!from.isGeneralReg() || from.reg() != dest)
                enoughMemory_ = moveResolver_.addMove(from, MoveOperand(dest), type);
            return;
        }

        enoughMemory_ = moveResolver_.addMove(from, MoveOperand(activeSP, stackForCall_), type);
        stackForCall_ += sizeof(int64_t);
        return;
    }

    MOZ_ASSERT(type == MoveOp::FLOAT32 || type == MoveOp::DOUBLE);
    if (type == MoveOp::FLOAT32)
        passedArgTypes_ = (passedArgTypes_ << ArgType_Shift) | ArgType_Float32;
    else
        passedArgTypes_ = (passedArgTypes_ << ArgType_Shift) | ArgType_Double;

    FloatRegister fdest;
    if (GetFloatArgReg(passedIntArgs_, passedFloatArgs_++, &fdest)) {
        if (!from.isFloatReg() || from.floatReg() != fdest)
            enoughMemory_ = moveResolver_.addMove(from, MoveOperand(fdest), type);
        return;
    }

    enoughMemory_ = moveResolver_.addMove(from, MoveOperand(activeSP, stackForCall_), type);
    switch (type) {
      case MoveOp::FLOAT32: stackForCall_ += sizeof(float);  break;
      case MoveOp::DOUBLE:  stackForCall_ += sizeof(double); break;
      default: MOZ_CRASH("Unexpected float register class argument type");
    }
}

void
MacroAssemblerCompat::passABIArg(Register reg)
{
    passABIArg(MoveOperand(reg), MoveOp::GENERAL);
}

void
MacroAssemblerCompat::passABIArg(FloatRegister reg, MoveOp::Type type)
{
    passABIArg(MoveOperand(reg), type);
}
void
MacroAssemblerCompat::passABIOutParam(Register reg)
{
    if (!enoughMemory_)
        return;
    MOZ_ASSERT(!usedOutParam_);
    usedOutParam_ = true;
    if (reg == r8)
        return;
    enoughMemory_ = moveResolver_.addMove(MoveOperand(reg), MoveOperand(r8), MoveOp::GENERAL);

}

void
MacroAssemblerCompat::callWithABIPre(uint32_t* stackAdjust)
{
    *stackAdjust = stackForCall_;
    // ARM64 /really/ wants the stack to always be aligned.  Since we're already tracking it
    // getting it aligned for an abi call is pretty easy.
    *stackAdjust += ComputeByteAlignment(*stackAdjust, StackAlignment);
    asMasm().reserveStack(*stackAdjust);
    {
        moveResolver_.resolve();
        MoveEmitter emitter(asMasm());
        emitter.emit(moveResolver_);
        emitter.finish();
    }

    // Call boundaries communicate stack via sp.
    syncStackPtr();
}

void
MacroAssemblerCompat::callWithABIPost(uint32_t stackAdjust, MoveOp::Type result)
{
    // Call boundaries communicate stack via sp.
    if (!GetStackPointer64().Is(sp))
        Mov(GetStackPointer64(), sp);

    inCall_ = false;
    asMasm().freeStack(stackAdjust);

    // Restore the stack pointer from entry.
    if (dynamicAlignment_)
        Ldr(GetStackPointer64(), MemOperand(GetStackPointer64(), 0));

    // Restore LR.
    pop(lr);

    // TODO: This one shouldn't be necessary -- check that callers
    // aren't enforcing the ABI themselves!
    syncStackPtr();

    // If the ABI's return regs are where ION is expecting them, then
    // no other work needs to be done.
}

#if defined(DEBUG) && defined(JS_SIMULATOR_ARM64)
static void
AssertValidABIFunctionType(uint32_t passedArgTypes)
{
    switch (passedArgTypes) {
      case Args_General0:
      case Args_General1:
      case Args_General2:
      case Args_General3:
      case Args_General4:
      case Args_General5:
      case Args_General6:
      case Args_General7:
      case Args_General8:
      case Args_Double_None:
      case Args_Int_Double:
      case Args_Float32_Float32:
      case Args_Double_Double:
      case Args_Double_Int:
      case Args_Double_DoubleInt:
      case Args_Double_DoubleDouble:
      case Args_Double_DoubleDoubleDouble:
      case Args_Double_DoubleDoubleDoubleDouble:
      case Args_Double_IntDouble:
      case Args_Int_IntDouble:
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }
}
#endif // DEBUG && JS_SIMULATOR_ARM64

void
MacroAssemblerCompat::callWithABI(void* fun, MoveOp::Type result)
{
#ifdef JS_SIMULATOR_ARM64
    MOZ_ASSERT(passedIntArgs_ + passedFloatArgs_ <= 15);
    passedArgTypes_ <<= ArgType_Shift;
    switch (result) {
      case MoveOp::GENERAL: passedArgTypes_ |= ArgType_General; break;
      case MoveOp::DOUBLE:  passedArgTypes_ |= ArgType_Double;  break;
      case MoveOp::FLOAT32: passedArgTypes_ |= ArgType_Float32; break;
      default: MOZ_CRASH("Invalid return type");
    }
# ifdef DEBUG
    AssertValidABIFunctionType(passedArgTypes_);
# endif
    ABIFunctionType type = ABIFunctionType(passedArgTypes_);
    fun = vixl::Simulator::RedirectNativeFunction(fun, type);
#endif // JS_SIMULATOR_ARM64

    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    asMasm().call(ImmPtr(fun));
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerCompat::callWithABI(Register fun, MoveOp::Type result)
{
    movePtr(fun, ip0);

    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    asMasm().call(ip0);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerCompat::callWithABI(AsmJSImmPtr imm, MoveOp::Type result)
{
    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    asMasm().call(imm);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerCompat::callWithABI(Address fun, MoveOp::Type result)
{
    loadPtr(fun, ip0);

    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    asMasm().call(ip0);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerCompat::branchPtrInNurseryRange(Condition cond, Register ptr, Register temp,
                                              Label* label)
{
    MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    MOZ_ASSERT(ptr != temp);
    MOZ_ASSERT(ptr != ScratchReg && ptr != ScratchReg2); // Both may be used internally.
    MOZ_ASSERT(temp != ScratchReg && temp != ScratchReg2);

    const Nursery& nursery = GetJitContext()->runtime->gcNursery();
    movePtr(ImmWord(-ptrdiff_t(nursery.start())), temp);
    addPtr(ptr, temp);
    branchPtr(cond == Assembler::Equal ? Assembler::Below : Assembler::AboveOrEqual,
              temp, ImmWord(nursery.nurserySize()), label);
}

void
MacroAssemblerCompat::branchValueIsNurseryObject(Condition cond, ValueOperand value, Register temp,
                                                 Label* label)
{
    MOZ_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    MOZ_ASSERT(temp != ScratchReg && temp != ScratchReg2); // Both may be used internally.

    // 'Value' representing the start of the nursery tagged as a JSObject
    const Nursery& nursery = GetJitContext()->runtime->gcNursery();
    Value start = ObjectValue(*reinterpret_cast<JSObject*>(nursery.start()));

    movePtr(ImmWord(-ptrdiff_t(start.asRawBits())), temp);
    addPtr(value.valueReg(), temp);
    branchPtr(cond == Assembler::Equal ? Assembler::Below : Assembler::AboveOrEqual,
              temp, ImmWord(nursery.nurserySize()), label);
}

void
MacroAssemblerCompat::callAndPushReturnAddress(Label* label)
{
    // FIXME: Jandem said he would refactor the code to avoid making
    // this instruction required, but probably forgot about it.
    // Instead of implementing this function, we should make it unnecessary.
    Label ret;
    {
        vixl::UseScratchRegisterScope temps(this);
        const ARMRegister scratch64 = temps.AcquireX();

        Adr(scratch64, &ret);
        asMasm().Push(scratch64.asUnsized());
    }

    Bl(label);
    bind(&ret);
}

void
MacroAssemblerCompat::breakpoint()
{
    static int code = 0xA77;
    Brk((code++) & 0xffff);
}

// ===============================================================
// Stack manipulation functions.

void
MacroAssembler::reserveStack(uint32_t amount)
{
    // TODO: This bumps |sp| every time we reserve using a second register.
    // It would save some instructions if we had a fixed frame size.
    vixl::MacroAssembler::Claim(Operand(amount));
    adjustFrame(amount);
}

void
MacroAssembler::PushRegsInMask(LiveRegisterSet set)
{
    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); ) {
        vixl::CPURegister src[4] = { vixl::NoCPUReg, vixl::NoCPUReg, vixl::NoCPUReg, vixl::NoCPUReg };

        for (size_t i = 0; i < 4 && iter.more(); i++) {
            src[i] = ARMRegister(*iter, 64);
            ++iter;
            adjustFrame(8);
        }
        vixl::MacroAssembler::Push(src[0], src[1], src[2], src[3]);
    }

    for (FloatRegisterBackwardIterator iter(set.fpus().reduceSetForPush()); iter.more(); ) {
        vixl::CPURegister src[4] = { vixl::NoCPUReg, vixl::NoCPUReg, vixl::NoCPUReg, vixl::NoCPUReg };

        for (size_t i = 0; i < 4 && iter.more(); i++) {
            src[i] = ARMFPRegister(*iter, 64);
            ++iter;
            adjustFrame(8);
        }
        vixl::MacroAssembler::Push(src[0], src[1], src[2], src[3]);
    }
}

void
MacroAssembler::PopRegsInMaskIgnore(LiveRegisterSet set, LiveRegisterSet ignore)
{
    // The offset of the data from the stack pointer.
    uint32_t offset = 0;

    for (FloatRegisterIterator iter(set.fpus().reduceSetForPush()); iter.more(); ) {
        vixl::CPURegister dest[2] = { vixl::NoCPUReg, vixl::NoCPUReg };
        uint32_t nextOffset = offset;

        for (size_t i = 0; i < 2 && iter.more(); i++) {
            if (!ignore.has(*iter))
                dest[i] = ARMFPRegister(*iter, 64);
            ++iter;
            nextOffset += sizeof(double);
        }

        if (!dest[0].IsNone() && !dest[1].IsNone())
            Ldp(dest[0], dest[1], MemOperand(GetStackPointer64(), offset));
        else if (!dest[0].IsNone())
            Ldr(dest[0], MemOperand(GetStackPointer64(), offset));
        else if (!dest[1].IsNone())
            Ldr(dest[1], MemOperand(GetStackPointer64(), offset + sizeof(double)));

        offset = nextOffset;
    }

    MOZ_ASSERT(offset == set.fpus().getPushSizeInBytes());

    for (GeneralRegisterIterator iter(set.gprs()); iter.more(); ) {
        vixl::CPURegister dest[2] = { vixl::NoCPUReg, vixl::NoCPUReg };
        uint32_t nextOffset = offset;

        for (size_t i = 0; i < 2 && iter.more(); i++) {
            if (!ignore.has(*iter))
                dest[i] = ARMRegister(*iter, 64);
            ++iter;
            nextOffset += sizeof(uint64_t);
        }

        if (!dest[0].IsNone() && !dest[1].IsNone())
            Ldp(dest[0], dest[1], MemOperand(GetStackPointer64(), offset));
        else if (!dest[0].IsNone())
            Ldr(dest[0], MemOperand(GetStackPointer64(), offset));
        else if (!dest[1].IsNone())
            Ldr(dest[1], MemOperand(GetStackPointer64(), offset + sizeof(uint64_t)));

        offset = nextOffset;
    }

    size_t bytesPushed = set.gprs().size() * sizeof(uint64_t) + set.fpus().getPushSizeInBytes();
    MOZ_ASSERT(offset == bytesPushed);
    freeStack(bytesPushed);
}

void
MacroAssembler::Push(Register reg)
{
    push(reg);
    adjustFrame(sizeof(intptr_t));
}

void
MacroAssembler::Push(const Imm32 imm)
{
    push(imm);
    adjustFrame(sizeof(intptr_t));
}

void
MacroAssembler::Push(const ImmWord imm)
{
    push(imm);
    adjustFrame(sizeof(intptr_t));
}

void
MacroAssembler::Push(const ImmPtr imm)
{
    push(imm);
    adjustFrame(sizeof(intptr_t));
}

void
MacroAssembler::Push(const ImmGCPtr ptr)
{
    push(ptr);
    adjustFrame(sizeof(intptr_t));
}

void
MacroAssembler::Push(FloatRegister f)
{
    push(f);
    adjustFrame(sizeof(double));
}

void
MacroAssembler::Pop(const Register reg)
{
    pop(reg);
    adjustFrame(-1 * int64_t(sizeof(int64_t)));
}

void
MacroAssembler::Pop(const ValueOperand& val)
{
    pop(val);
    adjustFrame(-1 * int64_t(sizeof(int64_t)));
}

// ===============================================================
// Simple call functions.

void
MacroAssembler::call(Register reg)
{
    syncStackPtr();
    Blr(ARMRegister(reg, 64));
}

void
MacroAssembler::call(Label* label)
{
    syncStackPtr();
    Bl(label);
}

void
MacroAssembler::call(ImmWord imm)
{
    call(ImmPtr((void*)imm.value));
}

void
MacroAssembler::call(ImmPtr imm)
{
    syncStackPtr();
    movePtr(imm, ip0);
    Blr(vixl::ip0);
}

void
MacroAssembler::call(AsmJSImmPtr imm)
{
    vixl::UseScratchRegisterScope temps(this);
    const Register scratch = temps.AcquireX().asUnsized();
    syncStackPtr();
    movePtr(imm, scratch);
    call(scratch);
}

void
MacroAssembler::call(JitCode* c)
{
    vixl::UseScratchRegisterScope temps(this);
    const ARMRegister scratch64 = temps.AcquireX();
    syncStackPtr();
    BufferOffset off = immPool64(scratch64, uint64_t(c->raw()));
    addPendingJump(off, ImmPtr(c->raw()), Relocation::JITCODE);
    blr(scratch64);
}

} // namespace jit
} // namespace js
