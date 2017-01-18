/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIRCompiler.h"
#include "jit/IonCaches.h"
#include "jit/IonIC.h"

#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"
#include "proxy/Proxy.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

// IonCacheIRCompiler compiles CacheIR to IonIC native code.
class MOZ_RAII IonCacheIRCompiler : public CacheIRCompiler
{
  public:
    friend class AutoSaveLiveRegisters;

    IonCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer, IonIC* ic, IonScript* ionScript)
      : CacheIRCompiler(cx, writer, Mode::Ion),
        writer_(writer),
        ic_(ic),
        ionScript_(ionScript),
        nextStubField_(0),
#ifdef DEBUG
        calledPrepareVMCall_(false),
#endif
        savedLiveRegs_(false)
    {
        MOZ_ASSERT(ic_);
        MOZ_ASSERT(ionScript_);
    }

    MOZ_MUST_USE bool init();
    JitCode* compile(IonICStub* stub);

  private:
    const CacheIRWriter& writer_;
    IonIC* ic_;
    IonScript* ionScript_;

    CodeOffsetJump rejoinOffset_;
    Vector<CodeOffset, 4, SystemAllocPolicy> nextCodeOffsets_;
    Maybe<LiveRegisterSet> liveRegs_;
    Maybe<CodeOffset> stubJitCodeOffset_;
    uint32_t nextStubField_;

#ifdef DEBUG
    bool calledPrepareVMCall_;
#endif
    bool savedLiveRegs_;

    uintptr_t readStubWord(uint32_t offset, StubField::Type type) {
        MOZ_ASSERT((offset % sizeof(uintptr_t)) == 0);
        return writer_.readStubFieldForIon(nextStubField_++, type).asWord();
    }
    uint64_t readStubInt64(uint32_t offset, StubField::Type type) {
        MOZ_ASSERT((offset % sizeof(uintptr_t)) == 0);
        return writer_.readStubFieldForIon(nextStubField_++, type).asInt64();
    }
    int32_t int32StubField(uint32_t offset) {
        return readStubWord(offset, StubField::Type::RawWord);
    }
    Shape* shapeStubField(uint32_t offset) {
        return (Shape*)readStubWord(offset, StubField::Type::Shape);
    }
    JSObject* objectStubField(uint32_t offset) {
        return (JSObject*)readStubWord(offset, StubField::Type::JSObject);
    }
    JSString* stringStubField(uint32_t offset) {
        return (JSString*)readStubWord(offset, StubField::Type::String);
    }
    JS::Symbol* symbolStubField(uint32_t offset) {
        return (JS::Symbol*)readStubWord(offset, StubField::Type::Symbol);
    }
    ObjectGroup* groupStubField(uint32_t offset) {
        return (ObjectGroup*)readStubWord(offset, StubField::Type::ObjectGroup);
    }
    jsid idStubField(uint32_t offset) {
        return mozilla::BitwiseCast<jsid>(readStubWord(offset, StubField::Type::Id));
    }
    template <typename T>
    T rawWordStubField(uint32_t offset) {
        static_assert(sizeof(T) == sizeof(uintptr_t), "T must have word size");
        return (T)readStubWord(offset, StubField::Type::RawWord);
    }
    template <typename T>
    T rawInt64StubField(uint32_t offset) {
        static_assert(sizeof(T) == sizeof(int64_t), "T musthave int64 size");
        return (T)readStubInt64(offset, StubField::Type::RawInt64);
    }

    void prepareVMCall(MacroAssembler& masm);
    MOZ_MUST_USE bool callVM(MacroAssembler& masm, const VMFunction& fun);

    void pushStubCodePointer() {
        stubJitCodeOffset_.emplace(masm.PushWithPatch(ImmPtr((void*)-1)));
    }

#define DEFINE_OP(op) MOZ_MUST_USE bool emit##op();
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP
};

// AutoSaveLiveRegisters must be used when we make a call that can GC. The
// constructor ensures all live registers are stored on the stack (where the GC
// expects them) and the destructor restores these registers.
class MOZ_RAII AutoSaveLiveRegisters
{
    IonCacheIRCompiler& compiler_;

    AutoSaveLiveRegisters(const AutoSaveLiveRegisters&) = delete;
    void operator=(const AutoSaveLiveRegisters&) = delete;

  public:
    explicit AutoSaveLiveRegisters(IonCacheIRCompiler& compiler)
      : compiler_(compiler)
    {
        MOZ_ASSERT(compiler_.liveRegs_.isSome());
        compiler_.allocator.saveIonLiveRegisters(compiler_.masm,
                                                 compiler_.liveRegs_.ref(),
                                                 compiler_.ic_->scratchRegisterForEntryJump(),
                                                 compiler_.ionScript_);
        compiler_.savedLiveRegs_ = true;
    }
    ~AutoSaveLiveRegisters() {
        MOZ_ASSERT(compiler_.stubJitCodeOffset_.isSome(), "Must have pushed JitCode* pointer");
        compiler_.allocator.restoreIonLiveRegisters(compiler_.masm, compiler_.liveRegs_.ref());
        MOZ_ASSERT(compiler_.masm.framePushed() == compiler_.ionScript_->frameSize());
    }
};

} // namespace jit
} // namespace js

#define DEFINE_SHARED_OP(op) \
    bool IonCacheIRCompiler::emit##op() { return CacheIRCompiler::emit##op(); }
    CACHE_IR_SHARED_OPS(DEFINE_SHARED_OP)
#undef DEFINE_SHARED_OP

void
CacheRegisterAllocator::saveIonLiveRegisters(MacroAssembler& masm, LiveRegisterSet liveRegs,
                                             Register scratch, IonScript* ionScript)
{
    MOZ_ASSERT(!liveRegs.has(scratch));

    // We have to push all registers in liveRegs on the stack. It's possible we
    // stored other values in our live registers and stored operands on the
    // stack (where our live registers should go), so this requires some careful
    // work. Try to keep it simple by taking one small step at a time.

    // Step 1. Discard any dead operands so we can reuse their registers.
    freeDeadOperandRegisters();

    // Step 2. Figure out the size of our live regs.
    size_t sizeOfLiveRegsInBytes =
        liveRegs.gprs().size() * sizeof(intptr_t) +
        liveRegs.fpus().getPushSizeInBytes();

    MOZ_ASSERT(sizeOfLiveRegsInBytes > 0);

    // Step 3. Ensure all non-input operands are on the stack.
    size_t numInputs = writer_.numInputOperands();
    for (size_t i = numInputs; i < operandLocations_.length(); i++) {
        OperandLocation& loc = operandLocations_[i];
        if (loc.isInRegister())
            spillOperandToStack(masm, &loc);
    }

    // Step 4. Restore the register state, but don't discard the stack as
    // non-input operands are stored there.
    restoreInputState(masm, /* shouldDiscardStack = */ false);

    // We just restored the input state, so no input operands should be stored
    // on the stack.
#ifdef DEBUG
    for (size_t i = 0; i < numInputs; i++) {
        const OperandLocation& loc = operandLocations_[i];
        MOZ_ASSERT(!loc.isOnStack());
    }
#endif

    // Step 5. At this point our register state is correct. Stack values,
    // however, may cover the space where we have to store the live registers.
    // Move them out of the way.

    bool hasOperandOnStack = false;
    for (size_t i = numInputs; i < operandLocations_.length(); i++) {
        OperandLocation& loc = operandLocations_[i];
        if (!loc.isOnStack())
            continue;

        hasOperandOnStack = true;

        size_t operandSize = loc.stackSizeInBytes();
        size_t operandStackPushed = loc.stackPushed();
        MOZ_ASSERT(operandSize > 0);
        MOZ_ASSERT(stackPushed_ >= operandStackPushed);
        MOZ_ASSERT(operandStackPushed >= operandSize);

        // If this operand doesn't cover the live register space, there's
        // nothing to do.
        if (operandStackPushed - operandSize >= sizeOfLiveRegsInBytes) {
            MOZ_ASSERT(stackPushed_ > sizeOfLiveRegsInBytes);
            continue;
        }

        // Reserve stack space for the live registers if needed.
        if (sizeOfLiveRegsInBytes > stackPushed_) {
            size_t extraBytes = sizeOfLiveRegsInBytes - stackPushed_;
            MOZ_ASSERT((extraBytes % sizeof(uintptr_t)) == 0);
            masm.subFromStackPtr(Imm32(extraBytes));
            stackPushed_ += extraBytes;
        }

        // Push the operand below the live register space.
        if (loc.kind() == OperandLocation::PayloadStack) {
            masm.push(Address(masm.getStackPointer(), stackPushed_ - operandStackPushed));
            stackPushed_ += operandSize;
            loc.setPayloadStack(stackPushed_, loc.payloadType());
            continue;
        }
        MOZ_ASSERT(loc.kind() == OperandLocation::ValueStack);
        masm.pushValue(Address(masm.getStackPointer(), stackPushed_ - operandStackPushed));
        stackPushed_ += operandSize;
        loc.setValueStack(stackPushed_);
    }

    // Step 6. If we have any operands on the stack, adjust their stackPushed
    // values to not include sizeOfLiveRegsInBytes (this simplifies code down
    // the line). Then push/store the live registers.
    if (hasOperandOnStack) {
        MOZ_ASSERT(stackPushed_ > sizeOfLiveRegsInBytes);
        stackPushed_ -= sizeOfLiveRegsInBytes;

        for (size_t i = numInputs; i < operandLocations_.length(); i++) {
            OperandLocation& loc = operandLocations_[i];
            if (loc.isOnStack())
                loc.adjustStackPushed(-int32_t(sizeOfLiveRegsInBytes));
        }

        size_t stackBottom = stackPushed_ + sizeOfLiveRegsInBytes;
        masm.storeRegsInMask(liveRegs, Address(masm.getStackPointer(), stackBottom), scratch);
        masm.setFramePushed(masm.framePushed() + sizeOfLiveRegsInBytes);
    } else {
        // If no operands are on the stack, discard the unused stack space.
        if (stackPushed_ > 0) {
            masm.addToStackPtr(Imm32(stackPushed_));
            stackPushed_ = 0;
        }
        masm.PushRegsInMask(liveRegs);
    }

    MOZ_ASSERT(masm.framePushed() == ionScript->frameSize() + sizeOfLiveRegsInBytes);

    // Step 7. All live registers and non-input operands are stored on the stack
    // now, so at this point all registers except for the input registers are
    // available.
    availableRegs_.set() = GeneralRegisterSet::Not(inputRegisterSet());
    availableRegsAfterSpill_.set() = GeneralRegisterSet();
}

void
CacheRegisterAllocator::restoreIonLiveRegisters(MacroAssembler& masm, LiveRegisterSet liveRegs)
{
    masm.PopRegsInMask(liveRegs);

    availableRegs_.set() = GeneralRegisterSet();
    availableRegsAfterSpill_.set() = GeneralRegisterSet::All();
}

void
IonCacheIRCompiler::prepareVMCall(MacroAssembler& masm)
{
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS,
                                              IonICCallFrameLayout::Size());
    pushStubCodePointer();
    masm.Push(Imm32(descriptor));
    masm.Push(ImmPtr(GetReturnAddressToIonCode(cx_)));

#ifdef DEBUG
    calledPrepareVMCall_ = true;
#endif
}

bool
IonCacheIRCompiler::callVM(MacroAssembler& masm, const VMFunction& fun)
{
    MOZ_ASSERT(calledPrepareVMCall_);

    JitCode* code = cx_->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    uint32_t frameSize = fun.explicitStackSlots() * sizeof(void*);
    uint32_t descriptor = MakeFrameDescriptor(frameSize, JitFrame_IonICCall,
                                              ExitFrameLayout::Size());
    masm.Push(Imm32(descriptor));
    masm.callJit(code);

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly poped when returning.
    int framePop = sizeof(ExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(frameSize + framePop);
    masm.freeStack(IonICCallFrameLayout::Size());
    return true;
}

bool
IonCacheIRCompiler::init()
{
    if (!allocator.init())
        return false;

    size_t numInputs = writer_.numInputOperands();

    AllocatableGeneralRegisterSet available;

    if (ic_->kind() == CacheKind::GetProp || ic_->kind() == CacheKind::GetElem) {
        IonGetPropertyIC* ic = ic_->asGetPropertyIC();
        TypedOrValueRegister output = ic->output();

        if (output.hasValue())
            available.add(output.valueReg());
        else if (!output.typedReg().isFloat())
            available.add(output.typedReg().gpr());

        if (ic->maybeTemp() != InvalidReg)
            available.add(ic->maybeTemp());

        liveRegs_.emplace(ic->liveRegs());
        outputUnchecked_.emplace(output);

        allowDoubleResult_.emplace(ic->allowDoubleResult());

        MOZ_ASSERT(numInputs == 1 || numInputs == 2);

        allocator.initInputLocation(0, ic->object(), JSVAL_TYPE_OBJECT);
        if (numInputs > 1)
            allocator.initInputLocation(1, ic->id());
    } else {
        MOZ_CRASH("Invalid cache");
    }

    allocator.initAvailableRegs(available);
    allocator.initAvailableRegsAfterSpill();
    return true;
}

JitCode*
IonCacheIRCompiler::compile(IonICStub* stub)
{
    masm.setFramePushed(ionScript_->frameSize());
    if (cx_->spsProfiler.enabled())
        masm.enableProfilingInstrumentation();

    do {
        switch (reader.readOp()) {
#define DEFINE_OP(op)                   \
          case CacheOp::op:             \
            if (!emit##op())            \
                return nullptr;         \
            break;
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

          default:
            MOZ_CRASH("Invalid op");
        }

        allocator.nextOp();
    } while (reader.more());

    MOZ_ASSERT(nextStubField_ == writer_.numStubFields());

    masm.assumeUnreachable("Should have returned from IC");

    // Done emitting the main IC code. Now emit the failure paths.
    for (size_t i = 0; i < failurePaths.length(); i++) {
        if (!emitFailurePath(i))
            return nullptr;
        Register scratch = ic_->scratchRegisterForEntryJump();
        CodeOffset offset = masm.movWithPatch(ImmWord(-1), scratch);
        masm.jump(Address(scratch, 0));
        if (!nextCodeOffsets_.append(offset))
            return nullptr;
    }

    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx_, linker.newCode<NoGC>(cx_, ION_CODE));
    if (!newStubCode) {
        cx_->recoverFromOutOfMemory();
        return nullptr;
    }

    rejoinOffset_.fixup(&masm);
    CodeLocationJump rejoinJump(newStubCode, rejoinOffset_);
    PatchJump(rejoinJump, ic_->rejoinLabel());

    for (CodeOffset offset : nextCodeOffsets_) {
        Assembler::PatchDataWithValueCheck(CodeLocationLabel(newStubCode, offset),
                                           ImmPtr(stub->nextCodeRawPtr()),
                                           ImmPtr((void*)-1));
    }
    if (stubJitCodeOffset_) {
        Assembler::PatchDataWithValueCheck(CodeLocationLabel(newStubCode, *stubJitCodeOffset_),
                                           ImmPtr(newStubCode.get()),
                                           ImmPtr((void*)-1));
    }

    // All barriers are emitted off-by-default, enable them if needed.
    if (cx_->zone()->needsIncrementalBarrier())
        newStubCode->togglePreBarriers(true, DontReprotect);

    return newStubCode;
}

bool
IonCacheIRCompiler::emitGuardShape()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Shape* shape = shapeStubField(reader.stubOffset());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestObjShape(Assembler::NotEqual, obj, shape, failure->label());
    return true;
}

bool
IonCacheIRCompiler::emitGuardGroup()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ObjectGroup* group = groupStubField(reader.stubOffset());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestObjGroup(Assembler::NotEqual, obj, group, failure->label());
    return true;
}

bool
IonCacheIRCompiler::emitGuardProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    JSObject* proto = objectStubField(reader.stubOffset());

    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadObjProto(obj, scratch);
    masm.branchPtr(Assembler::NotEqual, scratch, ImmGCPtr(proto), failure->label());
    return true;
}

bool
IonCacheIRCompiler::emitGuardSpecificObject()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    JSObject* expected = objectStubField(reader.stubOffset());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchPtr(Assembler::NotEqual, obj, ImmGCPtr(expected), failure->label());
    return true;
}

bool
IonCacheIRCompiler::emitGuardSpecificAtom()
{
    Register str = allocator.useRegister(masm, reader.stringOperandId());
    AutoScratchRegister scratch(allocator, masm);

    JSAtom* atom = &stringStubField(reader.stubOffset())->asAtom();

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Label done;
    masm.branchPtr(Assembler::Equal, str, ImmGCPtr(atom), &done);

    // The pointers are not equal, so if the input string is also an atom it
    // must be a different string.
    masm.branchTest32(Assembler::NonZero, Address(str, JSString::offsetOfFlags()),
                      Imm32(JSString::ATOM_BIT), failure->label());

    // Check the length.
    masm.branch32(Assembler::NotEqual, Address(str, JSString::offsetOfLength()),
                  Imm32(atom->length()), failure->label());

    // We have a non-atomized string with the same length. Call a helper
    // function to do the comparison.
    LiveRegisterSet volatileRegs(RegisterSet::Volatile());
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch);
    masm.movePtr(ImmGCPtr(atom), scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(str);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, EqualStringsHelper));
    masm.mov(ReturnReg, scratch);

    LiveRegisterSet ignore;
    ignore.add(scratch);
    masm.PopRegsInMaskIgnore(volatileRegs, ignore);
    masm.branchIfFalseBool(scratch, failure->label());

    masm.bind(&done);
    return true;
}

bool
IonCacheIRCompiler::emitGuardSpecificSymbol()
{
    Register sym = allocator.useRegister(masm, reader.symbolOperandId());
    JS::Symbol* expected = symbolStubField(reader.stubOffset());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchPtr(Assembler::NotEqual, sym, ImmGCPtr(expected), failure->label());
    return true;
}

bool
IonCacheIRCompiler::emitLoadFixedSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    int32_t offset = int32StubField(reader.stubOffset());
    masm.loadTypedOrValue(Address(obj, offset), output);
    return true;
}

bool
IonCacheIRCompiler::emitLoadDynamicSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    int32_t offset = int32StubField(reader.stubOffset());

    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch);
    masm.loadTypedOrValue(Address(scratch, offset), output);
    return true;
}

bool
IonCacheIRCompiler::emitCallScriptedGetterResult()
{
    AutoSaveLiveRegisters save(*this);
    AutoOutputRegister output(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    JSFunction* target = &objectStubField(reader.stubOffset())->as<JSFunction>();
    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    uint32_t framePushedBefore = masm.framePushed();

    // Construct IonICCallFrameLayout.
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS,
                                              IonICCallFrameLayout::Size());
    pushStubCodePointer();
    masm.Push(Imm32(descriptor));
    masm.Push(ImmPtr(GetReturnAddressToIonCode(cx_)));

    // The JitFrameLayout pushed below will be aligned to JitStackAlignment,
    // so we just have to make sure the stack is aligned after we push the
    // |this| + argument Values.
    uint32_t argSize = (target->nargs() + 1) * sizeof(Value);
    uint32_t padding = ComputeByteAlignment(masm.framePushed() + argSize, JitStackAlignment);
    MOZ_ASSERT(padding % sizeof(uintptr_t) == 0);
    MOZ_ASSERT(padding < JitStackAlignment);
    masm.reserveStack(padding);

    for (size_t i = 0; i < target->nargs(); i++)
        masm.Push(UndefinedValue());
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));

    masm.movePtr(ImmGCPtr(target), scratch);

    descriptor = MakeFrameDescriptor(argSize + padding, JitFrame_IonICCall,
                                     JitFrameLayout::Size());
    masm.Push(Imm32(0)); // argc
    masm.Push(scratch);
    masm.Push(Imm32(descriptor));

    // Check stack alignment. Add sizeof(uintptr_t) for the return address.
    MOZ_ASSERT(((masm.framePushed() + sizeof(uintptr_t)) % JitStackAlignment) == 0);

    // The getter has JIT code now and we will only discard the getter's JIT
    // code when discarding all JIT code in the Zone, so we can assume it'll
    // still have JIT code.
    MOZ_ASSERT(target->hasJITCode());
    masm.loadPtr(Address(scratch, JSFunction::offsetOfNativeOrScript()), scratch);
    masm.loadBaselineOrIonRaw(scratch, scratch, nullptr);
    masm.callJit(scratch);
    masm.storeCallResultValue(output);

    masm.freeStack(masm.framePushed() - framePushedBefore);
    return true;
}

bool
IonCacheIRCompiler::emitCallNativeGetterResult()
{
    AutoSaveLiveRegisters save(*this);
    AutoOutputRegister output(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    JSFunction* target = &objectStubField(reader.stubOffset())->as<JSFunction>();
    MOZ_ASSERT(target->isNative());

    AutoScratchRegister argJSContext(allocator, masm);
    AutoScratchRegister argUintN(allocator, masm);
    AutoScratchRegister argVp(allocator, masm);
    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    // Native functions have the signature:
    //  bool (*)(JSContext*, unsigned, Value* vp)
    // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Construct vp array:
    // Push object value for |this|
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));
    // Push callee/outparam.
    masm.Push(ObjectValue(*target));

    // Preload arguments into registers.
    masm.loadJSContext(argJSContext);
    masm.move32(Imm32(0), argUintN);
    masm.moveStackPtrTo(argVp.get());

    // Push marking data for later use.
    masm.Push(argUintN);
    pushStubCodePointer();

    if (!masm.icBuildOOLFakeExitFrame(GetReturnAddressToIonCode(cx_), save))
        return false;
    masm.enterFakeExitFrame(IonOOLNativeExitFrameLayoutToken);

    // Construct and execute call.
    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(argJSContext);
    masm.passABIArg(argUintN);
    masm.passABIArg(argVp);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, target->native()));

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the outparam vp[0] into output register(s).
    Address outparam(masm.getStackPointer(), IonOOLNativeExitFrameLayout::offsetOfResult());
    masm.loadValue(outparam, output.valueReg());

    masm.adjustStack(IonOOLNativeExitFrameLayout::Size(0));
    return true;
}

bool
IonCacheIRCompiler::emitCallProxyGetResult()
{
    AutoSaveLiveRegisters save(*this);
    AutoOutputRegister output(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    jsid id = idStubField(reader.stubOffset());

    // ProxyGetProperty(JSContext* cx, HandleObject proxy, HandleId id,
    //                  MutableHandleValue vp)
    AutoScratchRegisterMaybeOutput argJSContext(allocator, masm, output);
    AutoScratchRegister argProxy(allocator, masm);
    AutoScratchRegister argId(allocator, masm);
    AutoScratchRegister argVp(allocator, masm);
    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    // Push stubCode for marking.
    pushStubCodePointer();

    // Push args on stack first so we can take pointers to make handles.
    masm.Push(UndefinedValue());
    masm.moveStackPtrTo(argVp.get());

    masm.Push(id, scratch);
    masm.moveStackPtrTo(argId.get());

    // Push the proxy. Also used as receiver.
    masm.Push(obj);
    masm.moveStackPtrTo(argProxy.get());

    masm.loadJSContext(argJSContext);

    if (!masm.icBuildOOLFakeExitFrame(GetReturnAddressToIonCode(cx_), save))
        return false;
    masm.enterFakeExitFrame(IonOOLProxyExitFrameLayoutToken);

    // Make the call.
    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(argJSContext);
    masm.passABIArg(argProxy);
    masm.passABIArg(argId);
    masm.passABIArg(argVp);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ProxyGetProperty));

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    // Load the outparam vp[0] into output register(s).
    Address outparam(masm.getStackPointer(), IonOOLProxyExitFrameLayout::offsetOfResult());
    masm.loadValue(outparam, output.valueReg());

    // masm.leaveExitFrame & pop locals
    masm.adjustStack(IonOOLProxyExitFrameLayout::Size());
    return true;
}

typedef bool (*ProxyGetPropertyByValueFn)(JSContext*, HandleObject, HandleValue, MutableHandleValue);
static const VMFunction ProxyGetPropertyByValueInfo =
    FunctionInfo<ProxyGetPropertyByValueFn>(ProxyGetPropertyByValue, "ProxyGetPropertyByValue");

bool
IonCacheIRCompiler::emitCallProxyGetByValueResult()
{
    AutoSaveLiveRegisters save(*this);
    AutoOutputRegister output(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());

    allocator.discardStack(masm);

    prepareVMCall(masm);

    masm.Push(idVal);
    masm.Push(obj);

    if (!callVM(masm, ProxyGetPropertyByValueInfo))
        return false;

    masm.storeCallResultValue(output);
    return true;
}

bool
IonCacheIRCompiler::emitLoadUnboxedPropertyResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());

    JSValueType fieldType = reader.valueType();
    int32_t fieldOffset = int32StubField(reader.stubOffset());
    masm.loadUnboxedProperty(Address(obj, fieldOffset), fieldType, output);
    return true;
}

bool
IonCacheIRCompiler::emitGuardFrameHasNoArgumentsObject()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadFrameCalleeResult()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadFrameNumActualArgsResult()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadFrameArgumentResult()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadEnvironmentFixedSlotResult()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadEnvironmentDynamicSlotResult()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitStoreFixedSlot()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitStoreDynamicSlot()
{
    MOZ_CRASH("Baseline-specific op");
}

bool
IonCacheIRCompiler::emitLoadTypedObjectResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    TypedThingLayout layout = reader.typedThingLayout();
    uint32_t typeDescr = reader.typeDescrKey();
    uint32_t fieldOffset = int32StubField(reader.stubOffset());

    // Get the object's data pointer.
    LoadTypedThingData(masm, layout, obj, scratch1);

    Address fieldAddr(scratch1, fieldOffset);
    emitLoadTypedObjectResultShared(fieldAddr, scratch2, layout, typeDescr, output);
    return true;
}

bool
IonCacheIRCompiler::emitTypeMonitorResult()
{
    return emitReturnFromIC();
}

bool
IonCacheIRCompiler::emitReturnFromIC()
{
    if (!savedLiveRegs_)
        allocator.restoreInputState(masm);

    RepatchLabel rejoin;
    rejoinOffset_ = masm.jumpWithPatch(&rejoin);
    masm.bind(&rejoin);
    return true;
}

bool
IonCacheIRCompiler::emitLoadObject()
{
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    JSObject* obj = objectStubField(reader.stubOffset());
    masm.movePtr(ImmGCPtr(obj), reg);
    return true;
}

bool
IonCacheIRCompiler::emitGuardDOMExpandoMissingOrGuardShape()
{
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    Shape* shape = shapeStubField(reader.stubOffset());

    AutoScratchRegister objScratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Label done;
    masm.branchTestUndefined(Assembler::Equal, val, &done);

    masm.debugAssertIsObject(val);
    masm.unboxObject(val, objScratch);
    masm.branchTestObjShape(Assembler::NotEqual, objScratch, shape, failure->label());

    masm.bind(&done);
    return true;
}

bool
IonCacheIRCompiler::emitLoadDOMExpandoValueGuardGeneration()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ExpandoAndGeneration* expandoAndGeneration =
        rawWordStubField<ExpandoAndGeneration*>(reader.stubOffset());
    uint64_t generation = rawInt64StubField<uint64_t>(reader.stubOffset());

    AutoScratchRegister scratch(allocator, masm);
    ValueOperand output = allocator.defineValueRegister(masm, reader.valOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), scratch);
    Address expandoAddr(scratch, ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot()));

    // Guard the ExpandoAndGeneration* matches the proxy's ExpandoAndGeneration.
    masm.loadValue(expandoAddr, output);
    masm.branchTestValue(Assembler::NotEqual, output, PrivateValue(expandoAndGeneration),
                         failure->label());

    // Guard expandoAndGeneration->generation matches the expected generation.
    masm.movePtr(ImmPtr(expandoAndGeneration), output.scratchReg());
    masm.branch64(Assembler::NotEqual,
                  Address(output.scratchReg(), ExpandoAndGeneration::offsetOfGeneration()),
                  Imm64(generation),
                  failure->label());

    // Load expandoAndGeneration->expando into the output Value register.
    masm.loadValue(Address(output.scratchReg(), ExpandoAndGeneration::offsetOfExpando()), output);
    return true;
}

bool
IonIC::attachCacheIRStub(JSContext* cx, const CacheIRWriter& writer, CacheKind kind,
                         HandleScript outerScript)
{
    // We shouldn't GC or report OOM (or any other exception) here.
    AutoAssertNoPendingException aanpe(cx);
    JS::AutoCheckCannotGC nogc;

    // Do nothing if the IR generator failed or triggered a GC that invalidated
    // the script.
    if (writer.failed() || !outerScript->hasIonScript())
        return false;

    JitContext jctx(cx, nullptr);
    IonCacheIRCompiler compiler(cx, writer, this, outerScript->ionScript());
    if (!compiler.init())
        return false;

    JitZone* jitZone = cx->zone()->jitZone();
    uint32_t stubDataOffset = sizeof(IonICStub);

    // Try to reuse a previously-allocated CacheIRStubInfo.
    CacheIRStubKey::Lookup lookup(kind, ICStubEngine::IonIC,
                                  writer.codeStart(), writer.codeLength());
    CacheIRStubInfo* stubInfo = jitZone->getIonCacheIRStubInfo(lookup);
    if (!stubInfo) {
        // Allocate the shared CacheIRStubInfo. Note that the
        // putIonCacheIRStubInfo call below will transfer ownership to
        // the stub info HashSet, so we don't have to worry about freeing
        // it below.

        // For Ion ICs, we don't track/use the makesGCCalls flag, so just pass true.
        bool makesGCCalls = true;
        stubInfo = CacheIRStubInfo::New(kind, ICStubEngine::IonIC, makesGCCalls,
                                        stubDataOffset, writer);
        if (!stubInfo)
            return false;

        CacheIRStubKey key(stubInfo);
        if (!jitZone->putIonCacheIRStubInfo(lookup, key))
            return false;
    }

    MOZ_ASSERT(stubInfo);

    // Ensure we don't attach duplicate stubs. This can happen if a stub failed
    // for some reason and the IR generator doesn't check for exactly the same
    // conditions.
    for (IonICStub* stub = firstStub_; stub; stub = stub->next()) {
        if (stub->stubInfo() != stubInfo)
            continue;
        if (!writer.stubDataEquals(stub->stubDataStart()))
            continue;
        return true;
    }

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();

    // Allocate the IonICStub in the optimized stub space. Ion stubs and
    // CacheIRStubInfo instances for Ion stubs can be purged on GC. That's okay
    // because the stub code is rooted separately when we make a VM call, and
    // stub code should never access the IonICStub after making a VM call. The
    // IonICStub::poison method poisons the stub to catch bugs in this area.
    ICStubSpace* stubSpace = cx->zone()->jitZone()->optimizedStubSpace();
    void* newStubMem = stubSpace->alloc(bytesNeeded);
    if (!newStubMem)
        return false;

    IonICStub* newStub = new(newStubMem) IonICStub(fallbackLabel_.raw(), stubInfo);

    JitCode* code = compiler.compile(newStub);
    if (!code)
        return false;

    writer.copyStubData(newStub->stubDataStart());

    attachStub(newStub, code);
    return true;
}
