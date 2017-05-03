/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCacheIRCompiler.h"

#include "jit/CacheIR.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"
#include "proxy/Proxy.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

class AutoStubFrame;

Address
CacheRegisterAllocator::addressOf(MacroAssembler& masm, BaselineFrameSlot slot) const
{
    uint32_t offset = stackPushed_ + ICStackValueOffset + slot.slot() * sizeof(JS::Value);
    return Address(masm.getStackPointer(), offset);
}

// BaselineCacheIRCompiler compiles CacheIR to BaselineIC native code.
class MOZ_RAII BaselineCacheIRCompiler : public CacheIRCompiler
{
#ifdef DEBUG
    // Some Baseline IC stubs can be used in IonMonkey through SharedStubs.
    // Those stubs have different machine code, so we need to track whether
    // we're compiling for Baseline or Ion.
    ICStubEngine engine_;
#endif

    uint32_t stubDataOffset_;
    bool inStubFrame_;
    bool makesGCCalls_;

    MOZ_MUST_USE bool callVM(MacroAssembler& masm, const VMFunction& fun);

    MOZ_MUST_USE bool callTypeUpdateIC(Register obj, ValueOperand val, Register scratch,
                                       LiveGeneralRegisterSet saveRegs);

    MOZ_MUST_USE bool emitStoreSlotShared(bool isFixed);
    MOZ_MUST_USE bool emitAddAndStoreSlotShared(CacheOp op);

  public:
    friend class AutoStubFrame;

    BaselineCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer, ICStubEngine engine,
                            uint32_t stubDataOffset)
      : CacheIRCompiler(cx, writer, Mode::Baseline),
#ifdef DEBUG
        engine_(engine),
#endif
        stubDataOffset_(stubDataOffset),
        inStubFrame_(false),
        makesGCCalls_(false)
    {}

    MOZ_MUST_USE bool init(CacheKind kind);

    JitCode* compile();

    bool makesGCCalls() const { return makesGCCalls_; }

  private:
#define DEFINE_OP(op) MOZ_MUST_USE bool emit##op();
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

    Address stubAddress(uint32_t offset) const {
        return Address(ICStubReg, stubDataOffset_ + offset);
    }
};

#define DEFINE_SHARED_OP(op) \
    bool BaselineCacheIRCompiler::emit##op() { return CacheIRCompiler::emit##op(); }
    CACHE_IR_SHARED_OPS(DEFINE_SHARED_OP)
#undef DEFINE_SHARED_OP

enum class CallCanGC { CanGC, CanNotGC };

// Instructions that have to perform a callVM require a stub frame. Call its
// enter() and leave() methods to enter/leave the stub frame.
class MOZ_RAII AutoStubFrame
{
    BaselineCacheIRCompiler& compiler;
#ifdef DEBUG
    uint32_t framePushedAtEnterStubFrame_;
#endif

    AutoStubFrame(const AutoStubFrame&) = delete;
    void operator=(const AutoStubFrame&) = delete;

  public:
    explicit AutoStubFrame(BaselineCacheIRCompiler& compiler)
      : compiler(compiler)
#ifdef DEBUG
        , framePushedAtEnterStubFrame_(0)
#endif
    { }

    void enter(MacroAssembler& masm, Register scratch, CallCanGC canGC = CallCanGC::CanGC) {
        MOZ_ASSERT(compiler.allocator.stackPushed() == 0);
        MOZ_ASSERT(compiler.engine_ == ICStubEngine::Baseline);

        EmitBaselineEnterStubFrame(masm, scratch);

#ifdef DEBUG
        framePushedAtEnterStubFrame_ = masm.framePushed();
#endif

        MOZ_ASSERT(!compiler.inStubFrame_);
        compiler.inStubFrame_ = true;
        if (canGC == CallCanGC::CanGC)
            compiler.makesGCCalls_ = true;
    }
    void leave(MacroAssembler& masm, bool calledIntoIon = false) {
        MOZ_ASSERT(compiler.inStubFrame_);
        compiler.inStubFrame_ = false;

#ifdef DEBUG
        masm.setFramePushed(framePushedAtEnterStubFrame_);
        if (calledIntoIon)
            masm.adjustFrame(sizeof(intptr_t)); // Calls into ion have this extra.
#endif

        EmitBaselineLeaveStubFrame(masm, calledIntoIon);
    }

#ifdef DEBUG
    ~AutoStubFrame() {
        MOZ_ASSERT(!compiler.inStubFrame_);
    }
#endif
};

bool
BaselineCacheIRCompiler::callVM(MacroAssembler& masm, const VMFunction& fun)
{
    MOZ_ASSERT(inStubFrame_);

    JitCode* code = cx_->runtime()->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    MOZ_ASSERT(fun.expectTailCall == NonTailCall);
    MOZ_ASSERT(engine_ == ICStubEngine::Baseline);

    EmitBaselineCallVM(code, masm);
    return true;
}

JitCode*
BaselineCacheIRCompiler::compile()
{
#ifndef JS_USE_LINK_REGISTER
    // The first value contains the return addres,
    // which we pull into ICTailCallReg for tail calls.
    masm.adjustFrame(sizeof(intptr_t));
#endif
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

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

    MOZ_ASSERT(!inStubFrame_);
    masm.assumeUnreachable("Should have returned from IC");

    // Done emitting the main IC code. Now emit the failure paths.
    for (size_t i = 0; i < failurePaths.length(); i++) {
        if (!emitFailurePath(i))
            return nullptr;
        EmitStubGuardFailure(masm);
    }

    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx_, linker.newCode<NoGC>(cx_, BASELINE_CODE));
    if (!newStubCode) {
        cx_->recoverFromOutOfMemory();
        return nullptr;
    }

    return newStubCode;
}

bool
BaselineCacheIRCompiler::emitGuardShape()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadPtr(addr, scratch);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardGroup()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadPtr(addr, scratch);
    masm.branchTestObjGroup(Assembler::NotEqual, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardGroupHasUnanalyzedNewScript()
{
    Address addr(stubAddress(reader.stubOffset()));
    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(addr, scratch1);
    masm.guardGroupHasUnanalyzedNewScript(scratch1, scratch2, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadObjProto(obj, scratch);
    masm.branchPtr(Assembler::NotEqual, addr, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardCompartment()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    reader.stubOffset(); // Read global wrapper.
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadPtr(Address(obj, JSObject::offsetOfGroup()), scratch);
    masm.loadPtr(Address(scratch, ObjectGroup::offsetOfCompartment()), scratch);
    masm.branchPtr(Assembler::NotEqual, addr, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardSpecificObject()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.branchPtr(Assembler::NotEqual, addr, obj, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardSpecificAtom()
{
    Register str = allocator.useRegister(masm, reader.stringOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address atomAddr(stubAddress(reader.stubOffset()));

    Label done;
    masm.branchPtr(Assembler::Equal, atomAddr, str, &done);

    // The pointers are not equal, so if the input string is also an atom it
    // must be a different string.
    masm.branchTest32(Assembler::NonZero, Address(str, JSString::offsetOfFlags()),
                      Imm32(JSString::ATOM_BIT), failure->label());

    // Check the length.
    masm.loadPtr(atomAddr, scratch);
    masm.loadStringLength(scratch, scratch);
    masm.branch32(Assembler::NotEqual, Address(str, JSString::offsetOfLength()),
                  scratch, failure->label());

    // We have a non-atomized string with the same length. Call a helper
    // function to do the comparison.
    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch);
    masm.loadPtr(atomAddr, scratch);
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
BaselineCacheIRCompiler::emitGuardSpecificSymbol()
{
    Register sym = allocator.useRegister(masm, reader.symbolOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.branchPtr(Assembler::NotEqual, addr, sym, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFixedSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadValue(BaseIndex(obj, scratch, TimesOne), output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDynamicSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
    AutoScratchRegister scratch2(allocator, masm);

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch2);
    masm.loadValue(BaseIndex(scratch2, scratch, TimesOne), output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitMegamorphicLoadSlotResult()
{
    AutoOutputRegister output(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address nameAddr = stubAddress(reader.stubOffset());
    bool handleMissing = reader.readBool();

    AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
    AutoScratchRegister scratch2(allocator, masm);
    AutoScratchRegister scratch3(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.Push(UndefinedValue());
    masm.moveStackPtrTo(scratch3.get());

    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
    volatileRegs.takeUnchecked(scratch1);
    volatileRegs.takeUnchecked(scratch2);
    volatileRegs.takeUnchecked(scratch3);
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch1);
    masm.loadJSContext(scratch1);
    masm.passABIArg(scratch1);
    masm.passABIArg(obj);
    masm.loadPtr(nameAddr, scratch2);
    masm.passABIArg(scratch2);
    masm.passABIArg(scratch3);
    if (handleMissing)
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataProperty<true>)));
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataProperty<false>)));
    masm.mov(ReturnReg, scratch2);
    masm.PopRegsInMask(volatileRegs);

    masm.loadTypedOrValue(Address(masm.getStackPointer(), 0), output);
    masm.adjustStack(sizeof(Value));

    masm.branchIfFalseBool(scratch2, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitMegamorphicStoreSlot()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address nameAddr = stubAddress(reader.stubOffset());
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    bool needsTypeBarrier = reader.readBool();

    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.Push(val);
    masm.moveStackPtrTo(val.scratchReg());

    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
    volatileRegs.takeUnchecked(scratch1);
    volatileRegs.takeUnchecked(scratch2);
    volatileRegs.takeUnchecked(val);
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch1);
    masm.loadJSContext(scratch1);
    masm.passABIArg(scratch1);
    masm.passABIArg(obj);
    masm.loadPtr(nameAddr, scratch2);
    masm.passABIArg(scratch2);
    masm.passABIArg(val.scratchReg());
    if (needsTypeBarrier)
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (SetNativeDataProperty<true>)));
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (SetNativeDataProperty<false>)));
    masm.mov(ReturnReg, scratch1);
    masm.PopRegsInMask(volatileRegs);

    masm.loadValue(Address(masm.getStackPointer(), 0), val);
    masm.adjustStack(sizeof(Value));

    masm.branchIfFalseBool(scratch1, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardHasGetterSetter()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address shapeAddr = stubAddress(reader.stubOffset());

    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
    volatileRegs.takeUnchecked(scratch1);
    volatileRegs.takeUnchecked(scratch2);
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch1);
    masm.loadJSContext(scratch1);
    masm.passABIArg(scratch1);
    masm.passABIArg(obj);
    masm.loadPtr(shapeAddr, scratch2);
    masm.passABIArg(scratch2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, ObjectHasGetterSetter));
    masm.mov(ReturnReg, scratch1);
    masm.PopRegsInMask(volatileRegs);

    masm.branchIfFalseBool(scratch1, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitCallScriptedGetterResult()
{
    MOZ_ASSERT(engine_ == ICStubEngine::Baseline);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address getterAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegisterExcluding code(allocator, masm, ArgumentsRectifierReg);
    AutoScratchRegister callee(allocator, masm);
    AutoScratchRegister scratch(allocator, masm);

    // First, ensure our getter is non-lazy and has JIT code.
    {
        FailurePath* failure;
        if (!addFailurePath(&failure))
            return false;

        masm.loadPtr(getterAddr, callee);
        masm.branchIfFunctionHasNoScript(callee, failure->label());
        masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), code);
        masm.loadBaselineOrIonRaw(code, code, failure->label());
    }

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(0);

    // Getter is called with 0 arguments, just |obj| as thisv.
    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));

    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());
    masm.Push(Imm32(0));  // ActualArgc is 0
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
    masm.branch32(Assembler::Equal, callee, Imm32(0), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);

        JitCode* argumentsRectifier = cx_->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(ImmWord(0), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    stubFrame.leave(masm, true);
    return true;
}

typedef bool (*CallNativeGetterFn)(JSContext*, HandleFunction, HandleObject, MutableHandleValue);
static const VMFunction CallNativeGetterInfo =
    FunctionInfo<CallNativeGetterFn>(CallNativeGetter, "CallNativeGetter");

bool
BaselineCacheIRCompiler::emitCallNativeGetterResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address getterAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    // Load the callee in the scratch register.
    masm.loadPtr(getterAddr, scratch);

    masm.Push(obj);
    masm.Push(scratch);

    if (!callVM(masm, CallNativeGetterInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxyGetPropertyFn)(JSContext*, HandleObject, HandleId, MutableHandleValue);
static const VMFunction ProxyGetPropertyInfo =
    FunctionInfo<ProxyGetPropertyFn>(ProxyGetProperty, "ProxyGetProperty");

bool
BaselineCacheIRCompiler::emitCallProxyGetResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address idAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    // Load the jsid in the scratch register.
    masm.loadPtr(idAddr, scratch);

    masm.Push(scratch);
    masm.Push(obj);

    if (!callVM(masm, ProxyGetPropertyInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxyGetPropertyByValueFn)(JSContext*, HandleObject, HandleValue, MutableHandleValue);
static const VMFunction ProxyGetPropertyByValueInfo =
    FunctionInfo<ProxyGetPropertyByValueFn>(ProxyGetPropertyByValue, "ProxyGetPropertyByValue");

bool
BaselineCacheIRCompiler::emitCallProxyGetByValueResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    masm.Push(idVal);
    masm.Push(obj);

    if (!callVM(masm, ProxyGetPropertyByValueInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxyHasOwnFn)(JSContext*, HandleObject, HandleValue, MutableHandleValue);
static const VMFunction ProxyHasOwnInfo = FunctionInfo<ProxyHasOwnFn>(ProxyHasOwn, "ProxyHasOwn");

bool
BaselineCacheIRCompiler::emitCallProxyHasOwnResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    masm.Push(idVal);
    masm.Push(obj);

    if (!callVM(masm, ProxyHasOwnInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedPropertyResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    JSValueType fieldType = reader.valueType();
    Address fieldOffset(stubAddress(reader.stubOffset()));
    masm.load32(fieldOffset, scratch);
    masm.loadUnboxedProperty(BaseIndex(obj, scratch, TimesOne), fieldType, output);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardFrameHasNoArgumentsObject()
{
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTest32(Assembler::NonZero,
                      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                      Imm32(BaselineFrame::HAS_ARGS_OBJ),
                      failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameCalleeResult()
{
    AutoOutputRegister output(*this);
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    Address callee(BaselineFrameReg, BaselineFrame::offsetOfCalleeToken());
    masm.loadFunctionFromCalleeToken(callee, scratch);
    masm.tagValue(JSVAL_TYPE_OBJECT, scratch, output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameNumActualArgsResult()
{
    AutoOutputRegister output(*this);
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    Address actualArgs(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs());
    masm.loadPtr(actualArgs, scratch);
    masm.tagValue(JSVAL_TYPE_INT32, scratch, output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadTypedObjectResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    TypedThingLayout layout = reader.typedThingLayout();
    uint32_t typeDescr = reader.typeDescrKey();
    Address fieldOffset(stubAddress(reader.stubOffset()));

    // Get the object's data pointer.
    LoadTypedThingData(masm, layout, obj, scratch1);

    // Get the address being written to.
    masm.load32(fieldOffset, scratch2);
    masm.addPtr(scratch2, scratch1);

    Address fieldAddr(scratch1, 0);
    emitLoadTypedObjectResultShared(fieldAddr, scratch2, layout, typeDescr, output);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameArgumentResult()
{
    AutoOutputRegister output(*this);
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    masm.loadPtr(Address(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs()), scratch);
    masm.branch32(Assembler::AboveOrEqual, index, scratch, failure->label());

    // Load the argument.
    masm.loadValue(BaseValueIndex(BaselineFrameReg, index, BaselineFrame::offsetOfArg(0)),
                   output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadEnvironmentFixedSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    BaseIndex slot(obj, scratch, TimesOne);

    // Check for uninitialized lexicals.
    masm.branchTestMagic(Assembler::Equal, slot, failure->label());

    // Load the value.
    masm.loadValue(slot, output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadEnvironmentDynamicSlotResult()
{
    AutoOutputRegister output(*this);
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);
    AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch2);

    // Check for uninitialized lexicals.
    BaseIndex slot(scratch2, scratch, TimesOne);
    masm.branchTestMagic(Assembler::Equal, slot, failure->label());

    // Load the value.
    masm.loadValue(slot, output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadStringResult()
{
    AutoOutputRegister output(*this);
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

    masm.loadPtr(stubAddress(reader.stubOffset()), scratch);
    masm.tagValue(JSVAL_TYPE_STRING, scratch, output.valueReg());
    return true;
}

bool
BaselineCacheIRCompiler::callTypeUpdateIC(Register obj, ValueOperand val, Register scratch,
                                          LiveGeneralRegisterSet saveRegs)
{
    // Ensure the stack is empty for the VM call below.
    allocator.discardStack(masm);

    // R0 contains the value that needs to be typechecked.
    MOZ_ASSERT(val == R0);
    MOZ_ASSERT(scratch == R1.scratchReg());

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    static const bool CallClobbersTailReg = false;
#else
    static const bool CallClobbersTailReg = true;
#endif

    // Call the first type update stub.
    if (CallClobbersTailReg)
        masm.push(ICTailCallReg);
    masm.push(ICStubReg);
    masm.loadPtr(Address(ICStubReg, ICUpdatedStub::offsetOfFirstUpdateStub()),
                 ICStubReg);
    masm.call(Address(ICStubReg, ICStub::offsetOfStubCode()));
    masm.pop(ICStubReg);
    if (CallClobbersTailReg)
        masm.pop(ICTailCallReg);

    // The update IC will store 0 or 1 in |scratch|, R1.scratchReg(), reflecting
    // if the value in R0 type-checked properly or not.
    Label done;
    masm.branch32(Assembler::Equal, scratch, Imm32(1), &done);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch, CallCanGC::CanNotGC);

    masm.PushRegsInMask(saveRegs);

    masm.Push(val);
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));
    masm.Push(ICStubReg);

    // Load previous frame pointer, push BaselineFrame*.
    masm.loadPtr(Address(BaselineFrameReg, 0), scratch);
    masm.pushBaselineFramePtr(scratch, scratch);

    if (!callVM(masm, DoTypeUpdateFallbackInfo))
        return false;

    masm.PopRegsInMask(saveRegs);

    stubFrame.leave(masm);

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreSlotShared(bool isFixed)
{
    ObjOperandId objId = reader.objOperandId();
    Address offsetAddr = stubAddress(reader.stubOffset());

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch1(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);
    Maybe<AutoScratchRegister> scratch2;
    if (!isFixed)
        scratch2.emplace(allocator, masm);

    LiveGeneralRegisterSet saveRegs;
    saveRegs.add(obj);
    saveRegs.add(val);
    if (!callTypeUpdateIC(obj, val, scratch1, saveRegs))
        return false;

    masm.load32(offsetAddr, scratch1);

    if (isFixed) {
        BaseIndex slot(obj, scratch1, TimesOne);
        EmitPreBarrier(masm, slot, MIRType::Value);
        masm.storeValue(val, slot);
    } else {
        masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch2.ref());
        BaseIndex slot(scratch2.ref(), scratch1, TimesOne);
        EmitPreBarrier(masm, slot, MIRType::Value);
        masm.storeValue(val, slot);
    }

    emitPostBarrierSlot(obj, val, scratch1);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreFixedSlot()
{
    return emitStoreSlotShared(true);
}

bool
BaselineCacheIRCompiler::emitStoreDynamicSlot()
{
    return emitStoreSlotShared(false);
}

bool
BaselineCacheIRCompiler::emitAddAndStoreSlotShared(CacheOp op)
{
    ObjOperandId objId = reader.objOperandId();
    Address offsetAddr = stubAddress(reader.stubOffset());

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch1(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);
    AutoScratchRegister scratch2(allocator, masm);

    bool changeGroup = reader.readBool();
    Address newGroupAddr = stubAddress(reader.stubOffset());
    Address newShapeAddr = stubAddress(reader.stubOffset());

    if (op == CacheOp::AllocateAndStoreDynamicSlot) {
        // We have to (re)allocate dynamic slots. Do this first, as it's the
        // only fallible operation here. This simplifies the callTypeUpdateIC
        // call below: it does not have to worry about saving registers used by
        // failure paths. Note that growSlotsDontReportOOM is fallible but does
        // not GC.
        Address numNewSlotsAddr = stubAddress(reader.stubOffset());

        FailurePath* failure;
        if (!addFailurePath(&failure))
            return false;

        LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
        masm.PushRegsInMask(save);

        masm.setupUnalignedABICall(scratch1);
        masm.loadJSContext(scratch1);
        masm.passABIArg(scratch1);
        masm.passABIArg(obj);
        masm.load32(numNewSlotsAddr, scratch2);
        masm.passABIArg(scratch2);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, NativeObject::growSlotsDontReportOOM));
        masm.mov(ReturnReg, scratch1);

        LiveRegisterSet ignore;
        ignore.add(scratch1);
        masm.PopRegsInMaskIgnore(save, ignore);

        masm.branchIfFalseBool(scratch1, failure->label());
    }

    LiveGeneralRegisterSet saveRegs;
    saveRegs.add(obj);
    saveRegs.add(val);
    if (!callTypeUpdateIC(obj, val, scratch1, saveRegs))
        return false;

    if (changeGroup) {
        // Changing object's group from a partially to fully initialized group,
        // per the acquired properties analysis. Only change the group if the
        // old group still has a newScript. This only applies to PlainObjects.
        Label noGroupChange;
        masm.loadPtr(Address(obj, JSObject::offsetOfGroup()), scratch1);
        masm.branchPtr(Assembler::Equal,
                       Address(scratch1, ObjectGroup::offsetOfAddendum()),
                       ImmWord(0),
                       &noGroupChange);

        // Reload the new group from the cache.
        masm.loadPtr(newGroupAddr, scratch1);

        Address groupAddr(obj, JSObject::offsetOfGroup());
        EmitPreBarrier(masm, groupAddr, MIRType::ObjectGroup);
        masm.storePtr(scratch1, groupAddr);

        masm.bind(&noGroupChange);
    }

    // Update the object's shape.
    Address shapeAddr(obj, ShapedObject::offsetOfShape());
    masm.loadPtr(newShapeAddr, scratch1);
    EmitPreBarrier(masm, shapeAddr, MIRType::Shape);
    masm.storePtr(scratch1, shapeAddr);

    // Perform the store. No pre-barrier required since this is a new
    // initialization.
    masm.load32(offsetAddr, scratch1);
    if (op == CacheOp::AddAndStoreFixedSlot) {
        BaseIndex slot(obj, scratch1, TimesOne);
        masm.storeValue(val, slot);
    } else {
        MOZ_ASSERT(op == CacheOp::AddAndStoreDynamicSlot ||
                   op == CacheOp::AllocateAndStoreDynamicSlot);
        masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch2);
        BaseIndex slot(scratch2, scratch1, TimesOne);
        masm.storeValue(val, slot);
    }

    emitPostBarrierSlot(obj, val, scratch1);
    return true;
}

bool
BaselineCacheIRCompiler::emitAddAndStoreFixedSlot()
{
    return emitAddAndStoreSlotShared(CacheOp::AddAndStoreFixedSlot);
}

bool
BaselineCacheIRCompiler::emitAddAndStoreDynamicSlot()
{
    return emitAddAndStoreSlotShared(CacheOp::AddAndStoreDynamicSlot);
}

bool
BaselineCacheIRCompiler::emitAllocateAndStoreDynamicSlot()
{
    return emitAddAndStoreSlotShared(CacheOp::AllocateAndStoreDynamicSlot);
}

bool
BaselineCacheIRCompiler::emitStoreUnboxedProperty()
{
    ObjOperandId objId = reader.objOperandId();
    JSValueType fieldType = reader.valueType();
    Address offsetAddr = stubAddress(reader.stubOffset());

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);

    // We only need the type update IC if we are storing an object.
    if (fieldType == JSVAL_TYPE_OBJECT) {
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(obj);
        saveRegs.add(val);
        if (!callTypeUpdateIC(obj, val, scratch, saveRegs))
            return false;
    }

    masm.load32(offsetAddr, scratch);
    BaseIndex fieldAddr(obj, scratch, TimesOne);

    // Note that the storeUnboxedProperty call here is infallible, as the
    // IR emitter is responsible for guarding on |val|'s type.
    EmitICUnboxedPreBarrier(masm, fieldAddr, fieldType);
    masm.storeUnboxedProperty(fieldAddr, fieldType,
                              ConstantOrRegister(TypedOrValueRegister(val)),
                              /* failure = */ nullptr);

    if (UnboxedTypeNeedsPostBarrier(fieldType))
        emitPostBarrierSlot(obj, val, scratch);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreTypedObjectReferenceProperty()
{
    ObjOperandId objId = reader.objOperandId();
    Address offsetAddr = stubAddress(reader.stubOffset());
    TypedThingLayout layout = reader.typedThingLayout();
    ReferenceTypeDescr::Type type = reader.referenceTypeDescrType();

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch1(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);
    AutoScratchRegister scratch2(allocator, masm);

    // We don't need a type update IC if the property is always a string.
    if (type != ReferenceTypeDescr::TYPE_STRING) {
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(obj);
        saveRegs.add(val);
        if (!callTypeUpdateIC(obj, val, scratch1, saveRegs))
            return false;
    }

    // Compute the address being written to.
    LoadTypedThingData(masm, layout, obj, scratch1);
    masm.addPtr(offsetAddr, scratch1);
    Address dest(scratch1, 0);

    emitStoreTypedObjectReferenceProp(val, type, dest, scratch2);

    if (type != ReferenceTypeDescr::TYPE_STRING)
        emitPostBarrierSlot(obj, val, scratch1);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreTypedObjectScalarProperty()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address offsetAddr = stubAddress(reader.stubOffset());
    TypedThingLayout layout = reader.typedThingLayout();
    Scalar::Type type = reader.scalarType();
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Compute the address being written to.
    LoadTypedThingData(masm, layout, obj, scratch1);
    masm.addPtr(offsetAddr, scratch1);
    Address dest(scratch1, 0);

    StoreToTypedArray(cx_, masm, type, val, dest, scratch2, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreDenseElement()
{
    ObjOperandId objId = reader.objOperandId();
    Int32OperandId indexId = reader.int32OperandId();

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);
    Register index = allocator.useRegister(masm, indexId);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Load obj->elements in scratch.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Bounds check.
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, index, failure->label());

    // Hole check.
    BaseObjectElementIndex element(scratch, index);
    masm.branchTestMagic(Assembler::Equal, element, failure->label());

    // Perform a single test to see if we either need to convert double
    // elements, clone the copy on write elements in the object or fail
    // due to a frozen element.
    Label noSpecialHandling;
    Address elementsFlags(scratch, ObjectElements::offsetOfFlags());
    masm.branchTest32(Assembler::Zero, elementsFlags,
                      Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS |
                            ObjectElements::COPY_ON_WRITE |
                            ObjectElements::FROZEN),
                      &noSpecialHandling);

    // Fail if we need to clone copy on write elements or to throw due
    // to a frozen element.
    masm.branchTest32(Assembler::NonZero, elementsFlags,
                      Imm32(ObjectElements::COPY_ON_WRITE |
                            ObjectElements::FROZEN),
                      failure->label());

    // We need to convert int32 values being stored into doubles. Note that
    // double arrays are only created by IonMonkey, so if we have no FP support
    // Ion is disabled and there should be no double arrays.
    if (cx_->runtime()->jitSupportsFloatingPoint) {
        // It's fine to convert the value in place in Baseline. We can't do
        // this in Ion.
        masm.convertInt32ValueToDouble(val);
    } else {
        masm.assumeUnreachable("There shouldn't be double arrays when there is no FP support.");
    }

    masm.bind(&noSpecialHandling);

    // Call the type update IC. After this everything must be infallible as we
    // don't save all registers here.
    LiveGeneralRegisterSet saveRegs;
    saveRegs.add(obj);
    saveRegs.add(index);
    saveRegs.add(val);
    if (!callTypeUpdateIC(obj, val, scratch, saveRegs))
        return false;

    // Perform the store. Reload obj->elements because callTypeUpdateIC
    // used the scratch register.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
    EmitPreBarrier(masm, element, MIRType::Value);
    masm.storeValue(val, element);

    emitPostBarrierElement(obj, val, scratch, index);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreDenseElementHole()
{
    ObjOperandId objId = reader.objOperandId();
    Int32OperandId indexId = reader.int32OperandId();

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    Register obj = allocator.useRegister(masm, objId);
    Register index = allocator.useRegister(masm, indexId);

    bool handleAdd = reader.readBool();

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Load obj->elements in scratch.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    BaseObjectElementIndex element(scratch, index);
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    Address elementsFlags(scratch, ObjectElements::offsetOfFlags());

    // Check for copy-on-write or frozen elements.
    masm.branchTest32(Assembler::NonZero, elementsFlags,
                      Imm32(ObjectElements::COPY_ON_WRITE |
                            ObjectElements::FROZEN),
                      failure->label());

    if (handleAdd) {
        // Fail if index > initLength.
        masm.branch32(Assembler::Below, initLength, index, failure->label());

        // If index < capacity, we can add a dense element inline. If not we
        // need to allocate more elements.
        Label capacityOk;
        Address capacity(scratch, ObjectElements::offsetOfCapacity());
        masm.branch32(Assembler::Above, capacity, index, &capacityOk);

        // Check for non-writable array length. We only have to do this if
        // index >= capacity.
        masm.branchTest32(Assembler::NonZero, elementsFlags,
                          Imm32(ObjectElements::NONWRITABLE_ARRAY_LENGTH),
                          failure->label());

        LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
        save.takeUnchecked(scratch);
        masm.PushRegsInMask(save);

        masm.setupUnalignedABICall(scratch);
        masm.loadJSContext(scratch);
        masm.passABIArg(scratch);
        masm.passABIArg(obj);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, NativeObject::addDenseElementDontReportOOM));
        masm.mov(ReturnReg, scratch);

        masm.PopRegsInMask(save);
        masm.branchIfFalseBool(scratch, failure->label());

        // Load the reallocated elements pointer.
        masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

        masm.bind(&capacityOk);

        // We increment initLength after the callTypeUpdateIC call, to ensure
        // the type update code doesn't read uninitialized memory.
    } else {
        // Fail if index >= initLength.
        masm.branch32(Assembler::BelowOrEqual, initLength, index, failure->label());
    }

    // Check if we have to convert a double element.
    Label noConversion;
    masm.branchTest32(Assembler::Zero, elementsFlags,
                      Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                      &noConversion);

    // We need to convert int32 values being stored into doubles. Note that
    // double arrays are only created by IonMonkey, so if we have no FP support
    // Ion is disabled and there should be no double arrays.
    if (cx_->runtime()->jitSupportsFloatingPoint) {
        // It's fine to convert the value in place in Baseline. We can't do
        // this in Ion.
        masm.convertInt32ValueToDouble(val);
    } else {
        masm.assumeUnreachable("There shouldn't be double arrays when there is no FP support.");
    }

    masm.bind(&noConversion);

    // Call the type update IC. After this everything must be infallible as we
    // don't save all registers here.
    LiveGeneralRegisterSet saveRegs;
    saveRegs.add(obj);
    saveRegs.add(index);
    saveRegs.add(val);
    if (!callTypeUpdateIC(obj, val, scratch, saveRegs))
        return false;

    // Reload obj->elements as callTypeUpdateIC used the scratch register.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    Label doStore;
    if (handleAdd) {
        // If index == initLength, increment initLength.
        Label inBounds;
        masm.branch32(Assembler::NotEqual, initLength, index, &inBounds);

        // Increment initLength.
        masm.add32(Imm32(1), initLength);

        // If length is now <= index, increment length too.
        Label skipIncrementLength;
        Address length(scratch, ObjectElements::offsetOfLength());
        masm.branch32(Assembler::Above, length, index, &skipIncrementLength);
        masm.add32(Imm32(1), length);
        masm.bind(&skipIncrementLength);

        // Skip EmitPreBarrier as the memory is uninitialized.
        masm.jump(&doStore);

        masm.bind(&inBounds);
    }

    EmitPreBarrier(masm, element, MIRType::Value);

    masm.bind(&doStore);
    masm.storeValue(val, element);

    emitPostBarrierElement(obj, val, scratch, index);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreTypedElement()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

    TypedThingLayout layout = reader.typedThingLayout();
    Scalar::Type type = reader.scalarType();
    bool handleOOB = reader.readBool();

    AutoScratchRegister scratch1(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    Label done;
    LoadTypedThingLength(masm, layout, obj, scratch1);
    masm.branch32(Assembler::BelowOrEqual, scratch1, index, handleOOB ? &done : failure->label());

    // Load the elements vector.
    LoadTypedThingData(masm, layout, obj, scratch1);

    BaseIndex dest(scratch1, index, ScaleFromElemWidth(Scalar::byteSize(type)));

    // Use ICStubReg as second scratch register. TODO: consider doing the RHS
    // type check/conversion as a separate IR instruction so we can simplify
    // this.
    Register scratch2 = ICStubReg;
    masm.push(scratch2);

    Label fail;
    StoreToTypedArray(cx_, masm, type, val, dest, scratch2, &fail);
    masm.pop(scratch2);
    masm.jump(&done);

    masm.bind(&fail);
    masm.pop(scratch2);
    masm.jump(failure->label());

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreUnboxedArrayElement()
{
    ObjOperandId objId = reader.objOperandId();
    Int32OperandId indexId = reader.int32OperandId();

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    JSValueType elementType = reader.valueType();
    Register obj = allocator.useRegister(masm, objId);
    Register index = allocator.useRegister(masm, indexId);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    Address initLength(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength());
    masm.load32(initLength, scratch);
    masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratch);
    masm.branch32(Assembler::BelowOrEqual, scratch, index, failure->label());

    // Call the type update IC. After this everything must be infallible as we
    // don't save all registers here.
    if (elementType == JSVAL_TYPE_OBJECT) {
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(obj);
        saveRegs.add(index);
        saveRegs.add(val);
        if (!callTypeUpdateIC(obj, val, scratch, saveRegs))
            return false;
    }

    // Load obj->elements.
    masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratch);

    // Note that the storeUnboxedProperty call here is infallible, as the
    // IR emitter is responsible for guarding on |val|'s type.
    BaseIndex element(scratch, index, ScaleFromElemWidth(UnboxedTypeSize(elementType)));
    EmitICUnboxedPreBarrier(masm, element, elementType);
    masm.storeUnboxedProperty(element, elementType,
                              ConstantOrRegister(TypedOrValueRegister(val)),
                              /* failure = */ nullptr);

    if (UnboxedTypeNeedsPostBarrier(elementType))
        emitPostBarrierSlot(obj, val, scratch);
    return true;
}

bool
BaselineCacheIRCompiler::emitStoreUnboxedArrayElementHole()
{
    ObjOperandId objId = reader.objOperandId();
    Int32OperandId indexId = reader.int32OperandId();

    // Allocate the fixed registers first. These need to be fixed for
    // callTypeUpdateIC.
    AutoScratchRegister scratch(allocator, masm, R1.scratchReg());
    ValueOperand val = allocator.useFixedValueRegister(masm, reader.valOperandId(), R0);

    JSValueType elementType = reader.valueType();
    Register obj = allocator.useRegister(masm, objId);
    Register index = allocator.useRegister(masm, indexId);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Check index <= initLength.
    Address initLength(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength());
    masm.load32(initLength, scratch);
    masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratch);
    masm.branch32(Assembler::Below, scratch, index, failure->label());

    // Check capacity.
    masm.checkUnboxedArrayCapacity(obj, RegisterOrInt32Constant(index), scratch, failure->label());

    // Call the type update IC. After this everything must be infallible as we
    // don't save all registers here.
    if (elementType == JSVAL_TYPE_OBJECT) {
        LiveGeneralRegisterSet saveRegs;
        saveRegs.add(obj);
        saveRegs.add(index);
        saveRegs.add(val);
        if (!callTypeUpdateIC(obj, val, scratch, saveRegs))
            return false;
    }

    // Load obj->elements.
    masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratch);

    // If index == initLength, increment initialized length.
    Label inBounds, doStore;
    masm.load32(initLength, scratch);
    masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratch);
    masm.branch32(Assembler::NotEqual, scratch, index, &inBounds);

    masm.add32(Imm32(1), initLength);

    // If length is now <= index, increment length.
    Address length(obj, UnboxedArrayObject::offsetOfLength());
    Label skipIncrementLength;
    masm.branch32(Assembler::Above, length, index, &skipIncrementLength);
    masm.add32(Imm32(1), length);
    masm.bind(&skipIncrementLength);

    // Skip EmitICUnboxedPreBarrier as the memory is uninitialized.
    masm.jump(&doStore);

    masm.bind(&inBounds);

    BaseIndex element(scratch, index, ScaleFromElemWidth(UnboxedTypeSize(elementType)));
    EmitICUnboxedPreBarrier(masm, element, elementType);

    // Note that the storeUnboxedProperty call here is infallible, as the
    // IR emitter is responsible for guarding on |val|'s type.
    masm.bind(&doStore);
    masm.storeUnboxedProperty(element, elementType,
                              ConstantOrRegister(TypedOrValueRegister(val)),
                              /* failure = */ nullptr);

    if (UnboxedTypeNeedsPostBarrier(elementType))
        emitPostBarrierSlot(obj, val, scratch);
    return true;
}

typedef bool (*CallNativeSetterFn)(JSContext*, HandleFunction, HandleObject, HandleValue);
static const VMFunction CallNativeSetterInfo =
    FunctionInfo<CallNativeSetterFn>(CallNativeSetter, "CallNativeSetter");

bool
BaselineCacheIRCompiler::emitCallNativeSetter()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address setterAddr(stubAddress(reader.stubOffset()));
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    // Load the callee in the scratch register.
    masm.loadPtr(setterAddr, scratch);

    masm.Push(val);
    masm.Push(obj);
    masm.Push(scratch);

    if (!callVM(masm, CallNativeSetterInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitCallScriptedSetter()
{
    AutoScratchRegisterExcluding scratch1(allocator, masm, ArgumentsRectifierReg);
    AutoScratchRegister scratch2(allocator, masm);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address setterAddr(stubAddress(reader.stubOffset()));
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

    // First, ensure our setter is non-lazy and has JIT code. This also loads
    // the callee in scratch1.
    {
        FailurePath* failure;
        if (!addFailurePath(&failure))
            return false;

        masm.loadPtr(setterAddr, scratch1);
        masm.branchIfFunctionHasNoScript(scratch1, failure->label());
        masm.loadPtr(Address(scratch1, JSFunction::offsetOfNativeOrScript()), scratch2);
        masm.loadBaselineOrIonRaw(scratch2, scratch2, failure->label());
    }

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch2);

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(1);

    // Setter is called with 1 argument, and |obj| as thisv. Note that we use
    // Push, not push, so that callJit will align the stack properly on ARM.
    masm.Push(val);
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));

    // Now that the object register is no longer needed, use it as second
    // scratch.
    EmitBaselineCreateStubFrameDescriptor(masm, scratch2, JitFrameLayout::Size());
    masm.Push(Imm32(1));  // ActualArgc

    // Push callee.
    masm.Push(scratch1);

    // Push frame descriptor.
    masm.Push(scratch2);

    // Load callee->nargs in scratch2 and the JIT code in scratch.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(scratch1, JSFunction::offsetOfNargs()), scratch2);
    masm.loadPtr(Address(scratch1, JSFunction::offsetOfNativeOrScript()), scratch1);
    masm.loadBaselineOrIonRaw(scratch1, scratch1, nullptr);

    // Handle arguments underflow.
    masm.branch32(Assembler::BelowOrEqual, scratch2, Imm32(1), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != scratch1);

        JitCode* argumentsRectifier = cx_->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(ImmGCPtr(argumentsRectifier), scratch1);
        masm.loadPtr(Address(scratch1, JitCode::offsetOfCode()), scratch1);
        masm.movePtr(ImmWord(1), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(scratch1);

    stubFrame.leave(masm, true);
    return true;
}

typedef bool (*SetArrayLengthFn)(JSContext*, HandleObject, HandleValue, bool);
static const VMFunction SetArrayLengthInfo =
    FunctionInfo<SetArrayLengthFn>(SetArrayLength, "SetArrayLength");

bool
BaselineCacheIRCompiler::emitCallSetArrayLength()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    bool strict = reader.readBool();
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    masm.Push(Imm32(strict));
    masm.Push(val);
    masm.Push(obj);

    if (!callVM(masm, SetArrayLengthInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxySetPropertyFn)(JSContext*, HandleObject, HandleId, HandleValue, bool);
static const VMFunction ProxySetPropertyInfo =
    FunctionInfo<ProxySetPropertyFn>(ProxySetProperty, "ProxySetProperty");

bool
BaselineCacheIRCompiler::emitCallProxySet()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    Address idAddr(stubAddress(reader.stubOffset()));
    bool strict = reader.readBool();

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, scratch);

    // Load the jsid in the scratch register.
    masm.loadPtr(idAddr, scratch);

    masm.Push(Imm32(strict));
    masm.Push(val);
    masm.Push(scratch);
    masm.Push(obj);

    if (!callVM(masm, ProxySetPropertyInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxySetPropertyByValueFn)(JSContext*, HandleObject, HandleValue, HandleValue, bool);
static const VMFunction ProxySetPropertyByValueInfo =
    FunctionInfo<ProxySetPropertyByValueFn>(ProxySetPropertyByValue, "ProxySetPropertyByValue");

bool
BaselineCacheIRCompiler::emitCallProxySetByValue()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    bool strict = reader.readBool();

    allocator.discardStack(masm);

    // We need a scratch register but we don't have any registers available on
    // x86, so temporarily store |obj| in the frame's scratch slot.
    int scratchOffset = BaselineFrame::reverseOffsetOfScratchValue();
    masm.storePtr(obj, Address(BaselineFrameReg, scratchOffset));

    AutoStubFrame stubFrame(*this);
    stubFrame.enter(masm, obj);

    // Restore |obj|. Because we entered a stub frame we first have to load
    // the original frame pointer.
    masm.loadPtr(Address(BaselineFrameReg, 0), obj);
    masm.loadPtr(Address(obj, scratchOffset), obj);

    masm.Push(Imm32(strict));
    masm.Push(val);
    masm.Push(idVal);
    masm.Push(obj);

    if (!callVM(masm, ProxySetPropertyByValueInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitTypeMonitorResult()
{
    allocator.discardStack(masm);
    EmitEnterTypeMonitorIC(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitReturnFromIC()
{
    allocator.discardStack(masm);
    EmitReturnFromIC(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadObject()
{
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    masm.loadPtr(stubAddress(reader.stubOffset()), reg);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardDOMExpandoMissingOrGuardShape()
{
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    AutoScratchRegister shapeScratch(allocator, masm);
    AutoScratchRegister objScratch(allocator, masm);
    Address shapeAddr(stubAddress(reader.stubOffset()));

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Label done;
    masm.branchTestUndefined(Assembler::Equal, val, &done);

    masm.debugAssertIsObject(val);
    masm.loadPtr(shapeAddr, shapeScratch);
    masm.unboxObject(val, objScratch);
    masm.branchTestObjShape(Assembler::NotEqual, objScratch, shapeScratch, failure->label());

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDOMExpandoValueGuardGeneration()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address expandoAndGenerationAddr(stubAddress(reader.stubOffset()));
    Address generationAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);
    ValueOperand output = allocator.defineValueRegister(masm, reader.valOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), scratch);
    Address expandoAddr(scratch, detail::ProxyReservedSlots::offsetOfPrivateSlot());

    // Load the ExpandoAndGeneration* in the output scratch register and guard
    // it matches the proxy's ExpandoAndGeneration.
    masm.loadPtr(expandoAndGenerationAddr, output.scratchReg());
    masm.branchPrivatePtr(Assembler::NotEqual, expandoAddr, output.scratchReg(), failure->label());

    // Guard expandoAndGeneration->generation matches the expected generation.
    masm.branch64(Assembler::NotEqual,
                  Address(output.scratchReg(), ExpandoAndGeneration::offsetOfGeneration()),
                  generationAddr,
                  scratch, failure->label());

    // Load expandoAndGeneration->expando into the output Value register.
    masm.loadValue(Address(output.scratchReg(), ExpandoAndGeneration::offsetOfExpando()), output);
    return true;
}

bool
BaselineCacheIRCompiler::init(CacheKind kind)
{
    if (!allocator.init())
        return false;

    // Baseline ICs monitor values when needed, so returning doubles is fine.
    allowDoubleResult_.emplace(true);

    size_t numInputs = writer_.numInputOperands();

    // Baseline passes the first 2 inputs in R0/R1, other Values are stored on
    // the stack.
    size_t numInputsInRegs = std::min(numInputs, size_t(2));
    AllocatableGeneralRegisterSet available(ICStubCompiler::availableGeneralRegs(numInputsInRegs));

    switch (kind) {
      case CacheKind::GetProp:
      case CacheKind::TypeOf:
        MOZ_ASSERT(numInputs == 1);
        allocator.initInputLocation(0, R0);
        break;
      case CacheKind::GetElem:
      case CacheKind::SetProp:
      case CacheKind::In:
      case CacheKind::HasOwn:
        MOZ_ASSERT(numInputs == 2);
        allocator.initInputLocation(0, R0);
        allocator.initInputLocation(1, R1);
        break;
      case CacheKind::SetElem:
        MOZ_ASSERT(numInputs == 3);
        allocator.initInputLocation(0, R0);
        allocator.initInputLocation(1, R1);
        allocator.initInputLocation(2, BaselineFrameSlot(0));
        break;
      case CacheKind::GetName:
      case CacheKind::BindName:
        MOZ_ASSERT(numInputs == 1);
        allocator.initInputLocation(0, R0.scratchReg(), JSVAL_TYPE_OBJECT);
#if defined(JS_NUNBOX32)
        // availableGeneralRegs can't know that GetName/BindName is only using
        // the payloadReg and not typeReg on x86.
        available.add(R0.typeReg());
#endif
        break;
    }

    // Baseline doesn't allocate float registers so none of them are live.
    liveFloatRegs_ = LiveFloatRegisterSet(FloatRegisterSet());

    allocator.initAvailableRegs(available);
    outputUnchecked_.emplace(R0);
    return true;
}

static const size_t MaxOptimizedCacheIRStubs = 16;

ICStub*
jit::AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                               CacheKind kind, ICStubEngine engine, JSScript* outerScript,
                               ICFallbackStub* stub, bool* attached)
{
    // We shouldn't GC or report OOM (or any other exception) here.
    AutoAssertNoPendingException aanpe(cx);
    JS::AutoCheckCannotGC nogc;

    MOZ_ASSERT(!*attached);

    if (writer.failed())
        return nullptr;

    // Just a sanity check: the caller should ensure we don't attach an
    // unlimited number of stubs.
    MOZ_ASSERT(stub->numOptimizedStubs() < MaxOptimizedCacheIRStubs);

    enum class CacheIRStubKind { Regular, Monitored, Updated };

    uint32_t stubDataOffset;
    CacheIRStubKind stubKind;
    switch (kind) {
      case CacheKind::In:
      case CacheKind::HasOwn:
      case CacheKind::BindName:
      case CacheKind::TypeOf:
        stubDataOffset = sizeof(ICCacheIR_Regular);
        stubKind = CacheIRStubKind::Regular;
        break;
      case CacheKind::GetProp:
      case CacheKind::GetElem:
      case CacheKind::GetName:
        stubDataOffset = sizeof(ICCacheIR_Monitored);
        stubKind = CacheIRStubKind::Monitored;
        break;
      case CacheKind::SetProp:
      case CacheKind::SetElem:
        stubDataOffset = sizeof(ICCacheIR_Updated);
        stubKind = CacheIRStubKind::Updated;
        break;
    }

    JitZone* jitZone = cx->zone()->jitZone();

    // Check if we already have JitCode for this stub.
    CacheIRStubInfo* stubInfo;
    CacheIRStubKey::Lookup lookup(kind, engine, writer.codeStart(), writer.codeLength());
    JitCode* code = jitZone->getBaselineCacheIRStubCode(lookup, &stubInfo);
    if (!code) {
        // We have to generate stub code.
        JitContext jctx(cx, nullptr);
        BaselineCacheIRCompiler comp(cx, writer, engine, stubDataOffset);
        if (!comp.init(kind))
            return nullptr;

        code = comp.compile();
        if (!code)
            return nullptr;

        // Allocate the shared CacheIRStubInfo. Note that the
        // putBaselineCacheIRStubCode call below will transfer ownership
        // to the stub code HashMap, so we don't have to worry about freeing
        // it below.
        MOZ_ASSERT(!stubInfo);
        stubInfo = CacheIRStubInfo::New(kind, engine, comp.makesGCCalls(), stubDataOffset, writer);
        if (!stubInfo)
            return nullptr;

        CacheIRStubKey key(stubInfo);
        if (!jitZone->putBaselineCacheIRStubCode(lookup, key, code))
            return nullptr;
    }

    MOZ_ASSERT(code);
    MOZ_ASSERT(stubInfo);
    MOZ_ASSERT(stubInfo->stubDataSize() == writer.stubDataSize());

    // Ensure we don't attach duplicate stubs. This can happen if a stub failed
    // for some reason and the IR generator doesn't check for exactly the same
    // conditions.
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        bool updated = false;
        switch (stubKind) {
          case CacheIRStubKind::Regular: {
            if (!iter->isCacheIR_Regular())
                continue;
            auto otherStub = iter->toCacheIR_Regular();
            if (otherStub->stubInfo() != stubInfo)
                continue;
            if (!writer.stubDataEqualsMaybeUpdate(otherStub->stubDataStart(), &updated))
                continue;
            break;
          }
          case CacheIRStubKind::Monitored: {
            if (!iter->isCacheIR_Monitored())
                continue;
            auto otherStub = iter->toCacheIR_Monitored();
            if (otherStub->stubInfo() != stubInfo)
                continue;
            if (!writer.stubDataEqualsMaybeUpdate(otherStub->stubDataStart(), &updated))
                continue;
            break;
          }
          case CacheIRStubKind::Updated: {
            if (!iter->isCacheIR_Updated())
                continue;
            auto otherStub = iter->toCacheIR_Updated();
            if (otherStub->stubInfo() != stubInfo)
                continue;
            if (!writer.stubDataEqualsMaybeUpdate(otherStub->stubDataStart(), &updated))
                continue;
            break;
          }
        }

        // We found a stub that's exactly the same as the stub we're about to
        // attach. Just return nullptr, the caller should do nothing in this
        // case.
        if (updated)
            *attached = true;
        return nullptr;
    }

    // Time to allocate and attach a new stub.

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();

    ICStubSpace* stubSpace = ICStubCompiler::StubSpaceForStub(stubInfo->makesGCCalls(),
                                                              outerScript, engine);
    void* newStubMem = stubSpace->alloc(bytesNeeded);
    if (!newStubMem)
        return nullptr;

    switch (stubKind) {
      case CacheIRStubKind::Regular: {
        auto newStub = new(newStubMem) ICCacheIR_Regular(code, stubInfo);
        writer.copyStubData(newStub->stubDataStart());
        stub->addNewStub(newStub);
        *attached = true;
        return newStub;
      }
      case CacheIRStubKind::Monitored: {
        ICStub* monitorStub =
            stub->toMonitoredFallbackStub()->fallbackMonitorStub()->firstMonitorStub();
        auto newStub = new(newStubMem) ICCacheIR_Monitored(code, monitorStub, stubInfo);
        writer.copyStubData(newStub->stubDataStart());
        stub->addNewStub(newStub);
        *attached = true;
        return newStub;
      }
      case CacheIRStubKind::Updated: {
        auto newStub = new(newStubMem) ICCacheIR_Updated(code, stubInfo);
        if (!newStub->initUpdatingChain(cx, stubSpace)) {
            cx->recoverFromOutOfMemory();
            return nullptr;
        }
        writer.copyStubData(newStub->stubDataStart());
        stub->addNewStub(newStub);
        *attached = true;
        return newStub;
      }
    }

    MOZ_CRASH("Invalid kind");
}

uint8_t*
ICCacheIR_Regular::stubDataStart()
{
    return reinterpret_cast<uint8_t*>(this) + stubInfo_->stubDataOffset();
}

uint8_t*
ICCacheIR_Monitored::stubDataStart()
{
    return reinterpret_cast<uint8_t*>(this) + stubInfo_->stubDataOffset();
}

uint8_t*
ICCacheIR_Updated::stubDataStart()
{
    return reinterpret_cast<uint8_t*>(this) + stubInfo_->stubDataOffset();
}

/* static */ ICCacheIR_Monitored*
ICCacheIR_Monitored::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                           ICCacheIR_Monitored& other)
{
    const CacheIRStubInfo* stubInfo = other.stubInfo();
    MOZ_ASSERT(stubInfo->makesGCCalls());

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();
    void* newStub = space->alloc(bytesNeeded);
    if (!newStub)
        return nullptr;

    ICCacheIR_Monitored* res = new(newStub) ICCacheIR_Monitored(other.jitCode(), firstMonitorStub,
                                                                stubInfo);
    stubInfo->copyStubData(&other, res);
    return res;
}

/* static */ ICCacheIR_Updated*
ICCacheIR_Updated::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                         ICCacheIR_Updated& other)
{
    const CacheIRStubInfo* stubInfo = other.stubInfo();
    MOZ_ASSERT(stubInfo->makesGCCalls());

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();
    void* newStub = space->alloc(bytesNeeded);
    if (!newStub)
        return nullptr;

    ICCacheIR_Updated* res = new(newStub) ICCacheIR_Updated(other.jitCode(), stubInfo);
    res->updateStubGroup() = other.updateStubGroup();
    res->updateStubId() = other.updateStubId();

    stubInfo->copyStubData(&other, res);
    return res;
}
