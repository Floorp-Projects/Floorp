/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCacheIR.h"

#include "jit/CacheIR.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

// BaselineCacheIRCompiler compiles CacheIR to BaselineIC native code.
class MOZ_RAII BaselineCacheIRCompiler : public CacheIRCompiler
{
    // Some Baseline IC stubs can be used in IonMonkey through SharedStubs.
    // Those stubs have different machine code, so we need to track whether
    // we're compiling for Baseline or Ion.
    ICStubEngine engine_;

    uint32_t stubDataOffset_;
    bool inStubFrame_;
    bool makesGCCalls_;

    MOZ_MUST_USE bool callVM(MacroAssembler& masm, const VMFunction& fun);

  public:
    friend class AutoStubFrame;

    BaselineCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer, ICStubEngine engine,
                            uint32_t stubDataOffset)
      : CacheIRCompiler(cx, writer),
        engine_(engine),
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

// Instructions that have to perform a callVM require a stub frame. Use
// AutoStubFrame before allocating any registers, then call its enter() and
// leave() methods to enter/leave the stub frame.
class MOZ_RAII AutoStubFrame
{
    BaselineCacheIRCompiler& compiler;
#ifdef DEBUG
    uint32_t framePushedAtEnterStubFrame_;
#endif
    Maybe<AutoScratchRegister> tail;

    AutoStubFrame(const AutoStubFrame&) = delete;
    void operator=(const AutoStubFrame&) = delete;

  public:
    explicit AutoStubFrame(BaselineCacheIRCompiler& compiler)
      : compiler(compiler),
#ifdef DEBUG
        framePushedAtEnterStubFrame_(0),
#endif
        tail()
    {
        // We use ICTailCallReg when entering the stub frame, so ensure it's not
        // used for something else.
        if (compiler.allocator.isAllocatable(ICTailCallReg))
            tail.emplace(compiler.allocator, compiler.masm, ICTailCallReg);
    }

    void enter(MacroAssembler& masm, Register scratch) {
        if (compiler.engine_ == ICStubEngine::Baseline) {
            EmitBaselineEnterStubFrame(masm, scratch);
#ifdef DEBUG
            framePushedAtEnterStubFrame_ = masm.framePushed();
#endif
        } else {
            EmitIonEnterStubFrame(masm, scratch);
        }

        MOZ_ASSERT(!compiler.inStubFrame_);
        compiler.inStubFrame_ = true;
        compiler.makesGCCalls_ = true;
    }
    void leave(MacroAssembler& masm, bool calledIntoIon = false) {
        MOZ_ASSERT(compiler.inStubFrame_);
        compiler.inStubFrame_ = false;

        if (compiler.engine_ == ICStubEngine::Baseline) {
#ifdef DEBUG
            masm.setFramePushed(framePushedAtEnterStubFrame_);
            if (calledIntoIon)
                masm.adjustFrame(sizeof(intptr_t)); // Calls into ion have this extra.
#endif

            EmitBaselineLeaveStubFrame(masm, calledIntoIon);
        } else {
            EmitIonLeaveStubFrame(masm);
        }
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

    JitCode* code = cx_->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    MOZ_ASSERT(fun.expectTailCall == NonTailCall);
    if (engine_ == ICStubEngine::Baseline)
        EmitBaselineCallVM(code, masm);
    else
        EmitIonCallVM(code, fun.explicitStackSlots(), masm);
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
        emitFailurePath(i);
        EmitStubGuardFailure(masm);
    }

    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx_, linker.newCode<NoGC>(cx_, BASELINE_CODE));
    if (!newStubCode) {
        cx_->recoverFromOutOfMemory();
        return nullptr;
    }

    // All barriers are emitted off-by-default, enable them if needed.
    if (cx_->zone()->needsIncrementalBarrier())
        newStubCode->togglePreBarriers(true, DontReprotect);

    return newStubCode;
}

bool
BaselineCacheIRCompiler::emitGuardIsInt32()
{
    ValOperandId inputId = reader.valOperandId();
    if (allocator.knownType(inputId) == JSVAL_TYPE_INT32)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    if (cx_->runtime()->jitSupportsFloatingPoint) {
        Label done;
        masm.branchTestInt32(Assembler::Equal, input, &done);
        {
            // If the value is a double, try to convert it to int32 in place.
            // It's fine to modify |input| in this case, as the difference is not
            // observable.
            masm.branchTestDouble(Assembler::NotEqual, input, failure->label());
            masm.unboxDouble(input, FloatReg0);
            masm.convertDoubleToInt32(FloatReg0, scratch, failure->label());
            masm.tagValue(JSVAL_TYPE_INT32, scratch, input);
        }
        masm.bind(&done);
    } else {
        masm.branchTestInt32(Assembler::NotEqual, input, failure->label());
    }

    return true;
}

bool
BaselineCacheIRCompiler::emitGuardType()
{
    ValOperandId inputId = reader.valOperandId();
    JSValueType type = reader.valueType();

    if (allocator.knownType(inputId) == type)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    switch (type) {
      case JSVAL_TYPE_STRING:
        masm.branchTestString(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_SYMBOL:
        masm.branchTestSymbol(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_DOUBLE:
        masm.branchTestNumber(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_BOOLEAN:
        masm.branchTestBoolean(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_UNDEFINED:
        masm.branchTestUndefined(Assembler::NotEqual, input, failure->label());
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }

    return true;
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
BaselineCacheIRCompiler::emitGuardClass()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    const Class* clasp = nullptr;
    switch (reader.guardClassKind()) {
      case GuardClassKind::Array:
        clasp = &ArrayObject::class_;
        break;
      case GuardClassKind::UnboxedArray:
        clasp = &UnboxedArrayObject::class_;
        break;
      case GuardClassKind::MappedArguments:
        clasp = &MappedArgumentsObject::class_;
        break;
      case GuardClassKind::UnmappedArguments:
        clasp = &UnmappedArgumentsObject::class_;
        break;
      case GuardClassKind::WindowProxy:
        clasp = cx_->maybeWindowProxyClass();
        break;
    }

    MOZ_ASSERT(clasp);
    masm.branchTestObjClass(Assembler::NotEqual, obj, scratch, clasp, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardIsProxy()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestObjectIsProxy(false, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNotDOMProxy()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestProxyHandlerFamily(Assembler::Equal, obj, scratch,
                                      GetDOMProxyHandlerFamily(), failure->label());
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
    LiveRegisterSet volatileRegs(RegisterSet::Volatile());
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
BaselineCacheIRCompiler::emitGuardMagicValue()
{
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    JSWhyMagic magic = reader.whyMagic();

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestMagicValue(Assembler::NotEqual, val, magic, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNoUnboxedExpando()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address expandoAddr(obj, UnboxedPlainObject::offsetOfExpando());
    masm.branchPtr(Assembler::NotEqual, expandoAddr, ImmWord(0), failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardAndLoadUnboxedExpando()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register output = allocator.defineRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address expandoAddr(obj, UnboxedPlainObject::offsetOfExpando());
    masm.loadPtr(expandoAddr, output);
    masm.branchTestPtr(Assembler::Zero, output, output, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFixedSlotResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadValue(BaseIndex(obj, scratch, TimesOne), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDynamicSlotResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    // We're about to return, so it's safe to clobber obj now.
    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), obj);
    masm.loadValue(BaseIndex(obj, scratch, TimesOne), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitCallScriptedGetterResult()
{
    MOZ_ASSERT(engine_ == ICStubEngine::Baseline);

    AutoStubFrame stubFrame(*this);

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

typedef bool (*DoCallNativeGetterFn)(JSContext*, HandleFunction, HandleObject, MutableHandleValue);
static const VMFunction DoCallNativeGetterInfo =
    FunctionInfo<DoCallNativeGetterFn>(DoCallNativeGetter, "DoCallNativeGetter");

bool
BaselineCacheIRCompiler::emitCallNativeGetterResult()
{
    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address getterAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    stubFrame.enter(masm, scratch);

    // Load the callee in the scratch register.
    masm.loadPtr(getterAddr, scratch);

    masm.Push(obj);
    masm.Push(scratch);

    if (!callVM(masm, DoCallNativeGetterInfo))
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
    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address idAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

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
    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    stubFrame.enter(masm, scratch);

    masm.Push(idVal);
    masm.Push(obj);

    if (!callVM(masm, ProxyGetPropertyByValueInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedPropertyResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    JSValueType fieldType = reader.valueType();
    Address fieldOffset(stubAddress(reader.stubOffset()));
    masm.load32(fieldOffset, scratch);
    masm.loadUnboxedProperty(BaseIndex(obj, scratch, TimesOne), fieldType, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNoDetachedTypedObjects()
{
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    CheckForTypedObjectWithDetachedStorage(cx_, masm, failure->label());
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
    Address callee(BaselineFrameReg, BaselineFrame::offsetOfCalleeToken());
    masm.loadFunctionFromCalleeToken(callee, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_OBJECT, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameNumActualArgsResult()
{
    Address actualArgs(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs());
    masm.loadPtr(actualArgs, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadTypedObjectResult()
{
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

    if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
        Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);
        masm.loadFromTypedArray(type, Address(scratch1, 0), R0, /* allowDouble = */ true,
                                scratch2, nullptr);
    } else {
        ReferenceTypeDescr::Type type = ReferenceTypeFromSimpleTypeDescrKey(typeDescr);
        switch (type) {
          case ReferenceTypeDescr::TYPE_ANY:
            masm.loadValue(Address(scratch1, 0), R0);
            break;

          case ReferenceTypeDescr::TYPE_OBJECT: {
            Label notNull, done;
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.branchTestPtr(Assembler::NonZero, scratch1, scratch1, &notNull);
            masm.moveValue(NullValue(), R0);
            masm.jump(&done);
            masm.bind(&notNull);
            masm.tagValue(JSVAL_TYPE_OBJECT, scratch1, R0);
            masm.bind(&done);
            break;
          }

          case ReferenceTypeDescr::TYPE_STRING:
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.tagValue(JSVAL_TYPE_STRING, scratch1, R0);
            break;

          default:
            MOZ_CRASH("Invalid ReferenceTypeDescr");
        }
    }

    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUndefinedResult()
{
    masm.moveValue(UndefinedValue(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadInt32ArrayLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
    masm.load32(Address(scratch, ObjectElements::offsetOfLength()), scratch);

    // Guard length fits in an int32.
    masm.branchTest32(Assembler::Signed, scratch, scratch, failure->label());
    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedArrayLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    masm.load32(Address(obj, UnboxedArrayObject::offsetOfLength()), R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadArgumentsObjectLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Get initial length value.
    masm.unboxInt32(Address(obj, ArgumentsObject::getInitialLengthSlotOffset()), scratch);

    // Test if length has been overridden.
    masm.branchTest32(Assembler::NonZero,
                      scratch,
                      Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      failure->label());

    // Shift out arguments length and return it. No need to type monitor
    // because this stub always returns int32.
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratch);
    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadStringLengthResult()
{
    Register str = allocator.useRegister(masm, reader.stringOperandId());
    masm.loadStringLength(str, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadStringCharResult()
{
    Register str = allocator.useRegister(masm, reader.stringOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchIfRope(str, failure->label());

    // Bounds check, load string char.
    masm.branch32(Assembler::BelowOrEqual, Address(str, JSString::offsetOfLength()),
                  index, failure->label());
    masm.loadStringChar(str, index, scratch);

    // Load StaticString for this char.
    masm.branch32(Assembler::AboveOrEqual, scratch, Imm32(StaticStrings::UNIT_STATIC_LIMIT),
                  failure->label());
    masm.movePtr(ImmPtr(&cx_->staticStrings().unitStaticTable), R0.scratchReg());
    masm.loadPtr(BaseIndex(R0.scratchReg(), scratch, ScalePointer), R0.scratchReg());

    masm.tagValue(JSVAL_TYPE_STRING, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameArgumentResult()
{
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    masm.loadPtr(Address(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs()), scratch);
    masm.branch32(Assembler::AboveOrEqual, index, scratch, failure->label());

    // Load the argument.
    masm.loadValue(BaseValueIndex(BaselineFrameReg, index, BaselineFrame::offsetOfArg(0)), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadArgumentsObjectArgResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Get initial length value.
    masm.unboxInt32(Address(obj, ArgumentsObject::getInitialLengthSlotOffset()), scratch);

    // Ensure no overridden length/element.
    masm.branchTest32(Assembler::NonZero,
                      scratch,
                      Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT |
                            ArgumentsObject::ELEMENT_OVERRIDDEN_BIT),
                      failure->label());

    // Bounds check.
    masm.rshift32(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratch);
    masm.branch32(Assembler::AboveOrEqual, index, scratch, failure->label());

    // Load ArgumentsData.
    masm.loadPrivate(Address(obj, ArgumentsObject::getDataSlotOffset()), scratch);

    // Fail if we have a RareArgumentsData (elements were deleted).
    masm.branchPtr(Assembler::NotEqual,
                   Address(scratch, offsetof(ArgumentsData, rareData)),
                   ImmWord(0),
                   failure->label());

    // Guard the argument is not a FORWARD_TO_CALL_SLOT MagicValue. Note that
    // the order here matters: we should only clobber R0 after emitting the last
    // guard.
    BaseValueIndex argValue(scratch, index, ArgumentsData::offsetOfArgs());
    masm.branchTestMagic(Assembler::Equal, argValue, failure->label());
    masm.loadValue(argValue, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDenseElementResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Bounds check.
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, index, failure->label());

    // Hole check. After that it's safe to clobber R0.
    BaseObjectElementIndex element(scratch, index);
    masm.branchTestMagic(Assembler::Equal, element, failure->label());
    masm.loadValue(element, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNoDenseElements()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Make sure there are no dense elements.
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::NotEqual, initLength, Imm32(0), failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDenseElementHoleResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Make sure the index is nonnegative.
    masm.branch32(Assembler::LessThan, index, Imm32(0), failure->label());

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Guard on the initialized length.
    Label hole;
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, index, &hole);

    // Load the value.
    Label done;
    masm.loadValue(BaseObjectElementIndex(scratch, index), R0);
    masm.branchTestMagic(Assembler::NotEqual, R0, &done);

    // Load undefined for the hole.
    masm.bind(&hole);
    masm.moveValue(UndefinedValue(), R0);

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedArrayElementResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    JSValueType elementType = reader.valueType();
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    masm.load32(Address(obj, UnboxedArrayObject::offsetOfCapacityIndexAndInitializedLength()),
                scratch);
    masm.and32(Imm32(UnboxedArrayObject::InitializedLengthMask), scratch);
    masm.branch32(Assembler::BelowOrEqual, scratch, index, failure->label());

    // Load obj->elements.
    masm.loadPtr(Address(obj, UnboxedArrayObject::offsetOfElements()), scratch);

    // Load value.
    size_t width = UnboxedTypeSize(elementType);
    BaseIndex addr(scratch, index, ScaleFromElemWidth(width));
    masm.loadUnboxedProperty(addr, elementType, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadTypedElementResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register index = allocator.useRegister(masm, reader.int32OperandId());
    TypedThingLayout layout = reader.typedThingLayout();
    Scalar::Type type = reader.scalarType();

    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Bounds check.
    LoadTypedThingLength(masm, layout, obj, scratch);
    masm.branch32(Assembler::BelowOrEqual, scratch, index, failure->label());

    // Load the elements vector.
    LoadTypedThingData(masm, layout, obj, scratch);

    // Load the value.
    BaseIndex source(scratch, index, ScaleFromElemWidth(Scalar::byteSize(type)));
    masm.loadFromTypedArray(type, source, R0, false, scratch, failure->label());
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
BaselineCacheIRCompiler::emitLoadProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    masm.loadObjProto(obj, reg);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDOMExpandoValue()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand val = allocator.defineValueRegister(masm, reader.valOperandId());

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), val.scratchReg());
    masm.loadValue(Address(val.scratchReg(),
                           ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot())),
                   val);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardDOMExpandoObject()
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

    masm.loadPtr(shapeAddr, shapeScratch);
    masm.unboxObject(val, objScratch);
    masm.branchTestObjShape(Assembler::NotEqual, objScratch, shapeScratch, failure->label());

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardDOMExpandoGeneration()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address expandoAndGenerationAddr(stubAddress(reader.stubOffset()));
    Address generationAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);
    ValueOperand output = allocator.defineValueRegister(masm, reader.valOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), scratch);
    Address expandoAddr(scratch, ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot()));

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
    size_t numInputs = writer_.numInputOperands();
    if (!allocator.init(ICStubCompiler::availableGeneralRegs(numInputs)))
        return false;

    if (numInputs >= 1) {
        allocator.initInputLocation(0, R0);
        if (numInputs >= 2)
            allocator.initInputLocation(1, R1);
    }

    return true;
}

static const size_t MaxOptimizedCacheIRStubs = 16;

ICStub*
jit::AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                               CacheKind kind, ICStubEngine engine, JSScript* outerScript,
                               ICFallbackStub* stub)
{
    // We shouldn't GC or report OOM (or any other exception) here.
    AutoAssertNoPendingException aanpe(cx);
    JS::AutoCheckCannotGC nogc;

    if (writer.failed())
        return nullptr;

    // Just a sanity check: the caller should ensure we don't attach an
    // unlimited number of stubs.
    MOZ_ASSERT(stub->numOptimizedStubs() < MaxOptimizedCacheIRStubs);

    MOZ_ASSERT(kind == CacheKind::GetProp || kind == CacheKind::GetElem);
    uint32_t stubDataOffset = sizeof(ICCacheIR_Monitored);

    JitCompartment* jitCompartment = cx->compartment()->jitCompartment();

    // Check if we already have JitCode for this stub.
    CacheIRStubInfo* stubInfo;
    CacheIRStubKey::Lookup lookup(kind, engine, writer.codeStart(), writer.codeLength());
    JitCode* code = jitCompartment->getCacheIRStubCode(lookup, &stubInfo);
    if (!code) {
        // We have to generate stub code.
        JitContext jctx(cx, nullptr);
        BaselineCacheIRCompiler comp(cx, writer, engine, stubDataOffset);
        if (!comp.init(kind))
            return nullptr;

        code = comp.compile();
        if (!code)
            return nullptr;

        // Allocate the shared CacheIRStubInfo. Note that the putCacheIRStubCode
        // call below will transfer ownership to the stub code HashMap, so we
        // don't have to worry about freeing it below.
        MOZ_ASSERT(!stubInfo);
        stubInfo = CacheIRStubInfo::New(kind, engine, comp.makesGCCalls(), stubDataOffset, writer);
        if (!stubInfo)
            return nullptr;

        CacheIRStubKey key(stubInfo);
        if (!jitCompartment->putCacheIRStubCode(lookup, key, code))
            return nullptr;
    }

    MOZ_ASSERT(code);
    MOZ_ASSERT(stubInfo);
    MOZ_ASSERT(stub->isMonitoredFallback());
    MOZ_ASSERT(stubInfo->stubDataSize() == writer.stubDataSize());

    // Ensure we don't attach duplicate stubs. This can happen if a stub failed
    // for some reason and the IR generator doesn't check for exactly the same
    // conditions.
    for (ICStubConstIterator iter = stub->beginChainConst(); !iter.atEnd(); iter++) {
        if (!iter->isCacheIR_Monitored())
            continue;

        ICCacheIR_Monitored* otherStub = iter->toCacheIR_Monitored();
        if (otherStub->stubInfo() != stubInfo)
            continue;
        if (!writer.stubDataEquals(otherStub->stubDataStart()))
            continue;

        // We found a stub that's exactly the same as the stub we're about to
        // attach. Just return nullptr, the caller should do nothing in this
        // case.
        return nullptr;
    }

    // Time to allocate and attach a new stub.

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();

    ICStubSpace* stubSpace = ICStubCompiler::StubSpaceForStub(stubInfo->makesGCCalls(),
                                                              outerScript, engine);
    void* newStubMem = stubSpace->alloc(bytesNeeded);
    if (!newStubMem)
        return nullptr;

    ICStub* monitorStub = stub->toMonitoredFallbackStub()->fallbackMonitorStub()->firstMonitorStub();
    auto newStub = new(newStubMem) ICCacheIR_Monitored(code, monitorStub, stubInfo);

    writer.copyStubData(newStub->stubDataStart());
    stub->addNewStub(newStub);
    return newStub;
}

void
jit::TraceBaselineCacheIRStub(JSTracer* trc, ICStub* stub, const CacheIRStubInfo* stubInfo)
{
    uint32_t field = 0;
    size_t offset = 0;
    while (true) {
        StubField::Type fieldType = stubInfo->fieldType(field);
        switch (fieldType) {
          case StubField::Type::RawWord:
          case StubField::Type::RawInt64:
            break;
          case StubField::Type::Shape:
            TraceNullableEdge(trc, &stubInfo->getStubField<Shape*>(stub, offset),
                              "baseline-cacheir-shape");
            break;
          case StubField::Type::ObjectGroup:
            TraceNullableEdge(trc, &stubInfo->getStubField<ObjectGroup*>(stub, offset),
                              "baseline-cacheir-group");
            break;
          case StubField::Type::JSObject:
            TraceNullableEdge(trc, &stubInfo->getStubField<JSObject*>(stub, offset),
                              "baseline-cacheir-object");
            break;
          case StubField::Type::Symbol:
            TraceNullableEdge(trc, &stubInfo->getStubField<JS::Symbol*>(stub, offset),
                              "baseline-cacheir-symbol");
            break;
          case StubField::Type::String:
            TraceNullableEdge(trc, &stubInfo->getStubField<JSString*>(stub, offset),
                              "baseline-cacheir-string");
            break;
          case StubField::Type::Id:
            TraceEdge(trc, &stubInfo->getStubField<jsid>(stub, offset), "baseline-cacheir-id");
            break;
          case StubField::Type::Value:
            TraceEdge(trc, &stubInfo->getStubField<JS::Value>(stub, offset),
                      "baseline-cacheir-value");
            break;
          case StubField::Type::Limit:
            return; // Done.
        }
        field++;
        offset += StubField::sizeInBytes(fieldType);
    }
}

uint8_t*
ICCacheIR_Monitored::stubDataStart()
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
