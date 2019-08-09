/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/VMFunctions.h"

#include "builtin/Promise.h"
#include "builtin/String.h"
#include "builtin/TypedObject.h"
#include "frontend/BytecodeCompiler.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/BaselineIC.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "jit/mips32/Simulator-mips32.h"
#include "jit/mips64/Simulator-mips64.h"
#include "vm/ArrayObject.h"
#include "vm/EqualityOperations.h"  // js::StrictlyEqual
#include "vm/Interpreter.h"
#include "vm/SelfHosting.h"
#include "vm/TraceLogging.h"

#include "debugger/DebugAPI-inl.h"
#include "jit/BaselineFrame-inl.h"
#include "jit/JitFrames-inl.h"
#include "jit/VMFunctionList-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/StringObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

// Helper template to build the VMFunctionData for a function.
template <typename... Args>
struct VMFunctionDataHelper;

template <class R, typename... Args>
struct VMFunctionDataHelper<R (*)(JSContext*, Args...)>
    : public VMFunctionData {
  using Fun = R (*)(JSContext*, Args...);

  static constexpr DataType returnType() { return TypeToDataType<R>::result; }
  static constexpr DataType outParam() {
    return OutParamToDataType<typename LastArg<Args...>::Type>::result;
  }
  static constexpr RootType outParamRootType() {
    return OutParamToRootType<typename LastArg<Args...>::Type>::result;
  }
  static constexpr size_t NbArgs() { return LastArg<Args...>::nbArgs; }
  static constexpr size_t explicitArgs() {
    return NbArgs() - (outParam() != Type_Void ? 1 : 0);
  }
  static constexpr uint32_t argumentProperties() {
    return BitMask<TypeToArgProperties, uint32_t, 2, Args...>::result;
  }
  static constexpr uint32_t argumentPassedInFloatRegs() {
    return BitMask<TypeToPassInFloatReg, uint32_t, 2, Args...>::result;
  }
  static constexpr uint64_t argumentRootTypes() {
    return BitMask<TypeToRootType, uint64_t, 3, Args...>::result;
  }
  constexpr explicit VMFunctionDataHelper(const char* name)
      : VMFunctionData(name, explicitArgs(), argumentProperties(),
                       argumentPassedInFloatRegs(), argumentRootTypes(),
                       outParam(), outParamRootType(), returnType(),
                       /* extraValuesToPop = */ 0, NonTailCall) {}
  constexpr explicit VMFunctionDataHelper(const char* name,
                                          MaybeTailCall expectTailCall,
                                          PopValues extraValuesToPop)
      : VMFunctionData(name, explicitArgs(), argumentProperties(),
                       argumentPassedInFloatRegs(), argumentRootTypes(),
                       outParam(), outParamRootType(), returnType(),
                       extraValuesToPop.numValues, expectTailCall) {}
};

// GCC warns when the signature does not have matching attributes (for example
// MOZ_MUST_USE). Squelch this warning to avoid a GCC-only footgun.
#if MOZ_IS_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

// Generate VMFunctionData array.
static constexpr VMFunctionData vmFunctions[] = {
#define DEF_VMFUNCTION(name, fp) VMFunctionDataHelper<decltype(&(::fp))>(#name),
    VMFUNCTION_LIST(DEF_VMFUNCTION)
#undef DEF_VMFUNCTION
};
static constexpr VMFunctionData tailCallVMFunctions[] = {
#define DEF_VMFUNCTION(name, fp, valuesToPop)              \
  VMFunctionDataHelper<decltype(&(::fp))>(#name, TailCall, \
                                          PopValues(valuesToPop)),
    TAIL_CALL_VMFUNCTION_LIST(DEF_VMFUNCTION)
#undef DEF_VMFUNCTION
};

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

// Generate arrays storing C++ function pointers. These pointers are not stored
// in VMFunctionData because there's no good way to cast them to void* in
// constexpr code. Compilers are smart enough to treat the const array below as
// constexpr.
#define DEF_VMFUNCTION(name, fp, ...) (void*)(::fp),
static void* const vmFunctionTargets[] = {VMFUNCTION_LIST(DEF_VMFUNCTION)};
static void* const tailCallVMFunctionTargets[] = {
    TAIL_CALL_VMFUNCTION_LIST(DEF_VMFUNCTION)};
#undef DEF_VMFUNCTION

const VMFunctionData& GetVMFunction(VMFunctionId id) {
  return vmFunctions[size_t(id)];
}
const VMFunctionData& GetVMFunction(TailCallVMFunctionId id) {
  return tailCallVMFunctions[size_t(id)];
}

static void* GetVMFunctionTarget(VMFunctionId id) {
  return vmFunctionTargets[size_t(id)];
}

static void* GetVMFunctionTarget(TailCallVMFunctionId id) {
  return tailCallVMFunctionTargets[size_t(id)];
}

template <typename IdT>
bool JitRuntime::generateVMWrappers(JSContext* cx, MacroAssembler& masm,
                                    VMWrapperOffsets& offsets) {
  // Generate all VM function wrappers.

  static constexpr size_t NumVMFunctions = size_t(IdT::Count);

  if (!offsets.reserve(NumVMFunctions)) {
    return false;
  }

#ifdef DEBUG
  const char* lastName = nullptr;
#endif

  for (size_t i = 0; i < NumVMFunctions; i++) {
    IdT id = IdT(i);
    const VMFunctionData& fun = GetVMFunction(id);

#ifdef DEBUG
    // Assert the list is sorted by name.
    if (lastName) {
      MOZ_ASSERT(strcmp(lastName, fun.name()) < 0,
                 "VM function list must be sorted by name");
    }
    lastName = fun.name();
#endif

    JitSpew(JitSpew_Codegen, "# VM function wrapper (%s)", fun.name());

    uint32_t offset;
    if (!generateVMWrapper(cx, masm, fun, GetVMFunctionTarget(id), &offset)) {
      return false;
    }

    MOZ_ASSERT(offsets.length() == size_t(id));
    offsets.infallibleAppend(offset);
  }

  return true;
};

bool JitRuntime::generateVMWrappers(JSContext* cx, MacroAssembler& masm) {
  if (!generateVMWrappers<VMFunctionId>(cx, masm, functionWrapperOffsets_)) {
    return false;
  }

  if (!generateVMWrappers<TailCallVMFunctionId>(
          cx, masm, tailCallFunctionWrapperOffsets_)) {
    return false;
  }

  return true;
}

AutoDetectInvalidation::AutoDetectInvalidation(JSContext* cx,
                                               MutableHandleValue rval)
    : cx_(cx),
      ionScript_(GetTopJitJSScript(cx)->ionScript()),
      rval_(rval),
      disabled_(false) {}

bool InvokeFunction(JSContext* cx, HandleObject obj, bool constructing,
                    bool ignoresReturnValue, uint32_t argc, Value* argv,
                    MutableHandleValue rval) {
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  TraceLogStartEvent(logger, TraceLogger_Call);

  AutoArrayRooter argvRoot(cx, argc + 1 + constructing, argv);

  // Data in the argument vector is arranged for a JIT -> JIT call.
  RootedValue thisv(cx, argv[0]);
  Value* argvWithoutThis = argv + 1;

  RootedValue fval(cx, ObjectValue(*obj));
  if (constructing) {
    if (!IsConstructor(fval)) {
      ReportValueError(cx, JSMSG_NOT_CONSTRUCTOR, JSDVG_IGNORE_STACK, fval,
                       nullptr);
      return false;
    }

    ConstructArgs cargs(cx);
    if (!cargs.init(cx, argc)) {
      return false;
    }

    for (uint32_t i = 0; i < argc; i++) {
      cargs[i].set(argvWithoutThis[i]);
    }

    RootedValue newTarget(cx, argvWithoutThis[argc]);

    // If |this| hasn't been created, or is JS_UNINITIALIZED_LEXICAL,
    // we can use normal construction code without creating an extraneous
    // object.
    if (thisv.isMagic()) {
      MOZ_ASSERT(thisv.whyMagic() == JS_IS_CONSTRUCTING ||
                 thisv.whyMagic() == JS_UNINITIALIZED_LEXICAL);

      RootedObject obj(cx);
      if (!Construct(cx, fval, cargs, newTarget, &obj)) {
        return false;
      }

      rval.setObject(*obj);
      return true;
    }

    // Otherwise the default |this| has already been created.  We could
    // almost perform a *call* at this point, but we'd break |new.target|
    // in the function.  So in this one weird case we call a one-off
    // construction path that *won't* set |this| to JS_IS_CONSTRUCTING.
    return InternalConstructWithProvidedThis(cx, fval, thisv, cargs, newTarget,
                                             rval);
  }

  InvokeArgsMaybeIgnoresReturnValue args(cx, ignoresReturnValue);
  if (!args.init(cx, argc)) {
    return false;
  }

  for (size_t i = 0; i < argc; i++) {
    args[i].set(argvWithoutThis[i]);
  }

  return Call(cx, fval, thisv, args, rval);
}

bool InvokeFunctionShuffleNewTarget(JSContext* cx, HandleObject obj,
                                    uint32_t numActualArgs,
                                    uint32_t numFormalArgs, Value* argv,
                                    MutableHandleValue rval) {
  MOZ_ASSERT(numFormalArgs > numActualArgs);
  argv[1 + numActualArgs] = argv[1 + numFormalArgs];
  return InvokeFunction(cx, obj, true, false, numActualArgs, argv, rval);
}

bool InvokeFromInterpreterStub(JSContext* cx,
                               InterpreterStubExitFrameLayout* frame) {
  JitFrameLayout* jsFrame = frame->jsFrame();
  CalleeToken token = jsFrame->calleeToken();

  Value* argv = jsFrame->argv();
  uint32_t numActualArgs = jsFrame->numActualArgs();
  bool constructing = CalleeTokenIsConstructing(token);
  RootedFunction fun(cx, CalleeTokenToFunction(token));

  // Ensure new.target immediately follows the actual arguments (the arguments
  // rectifier added padding). See also InvokeFunctionShuffleNewTarget.
  if (constructing && numActualArgs < fun->nargs()) {
    argv[1 + numActualArgs] = argv[1 + fun->nargs()];
  }

  RootedValue rval(cx);
  if (!InvokeFunction(cx, fun, constructing,
                      /* ignoresReturnValue = */ false, numActualArgs, argv,
                      &rval)) {
    return false;
  }

  // Overwrite |this| with the return value.
  argv[0] = rval;
  return true;
}

static bool CheckOverRecursedImpl(JSContext* cx, size_t extra) {
  // We just failed the jitStackLimit check. There are two possible reasons:
  //  1) jitStackLimit was the real stack limit and we're over-recursed
  //  2) jitStackLimit was set to UINTPTR_MAX by JSContext::requestInterrupt
  //     and we need to call JSContext::handleInterrupt.

  // This handles 1).
#ifdef JS_SIMULATOR
  if (cx->simulator()->overRecursedWithExtra(extra)) {
    ReportOverRecursed(cx);
    return false;
  }
#else
  if (!CheckRecursionLimitWithExtra(cx, extra)) {
    return false;
  }
#endif

  // This handles 2).
  gc::MaybeVerifyBarriers(cx);
  return cx->handleInterrupt();
}

bool CheckOverRecursed(JSContext* cx) { return CheckOverRecursedImpl(cx, 0); }

bool CheckOverRecursedBaseline(JSContext* cx, BaselineFrame* frame) {
  // The stack check in Baseline happens before pushing locals so we have to
  // account for that by including script->nslots() in the C++ recursion check.
  size_t extra = frame->script()->nslots() * sizeof(Value);
  return CheckOverRecursedImpl(cx, extra);
}

bool MutatePrototype(JSContext* cx, HandlePlainObject obj, HandleValue value) {
  if (!value.isObjectOrNull()) {
    return true;
  }

  RootedObject newProto(cx, value.toObjectOrNull());
  return SetPrototype(cx, obj, newProto);
}

template <EqualityKind Kind>
bool LooselyEqual(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
                  bool* res) {
  if (!js::LooselyEqual(cx, lhs, rhs, res)) {
    return false;
  }
  if (Kind != EqualityKind::Equal) {
    *res = !*res;
  }
  return true;
}

template bool LooselyEqual<EqualityKind::Equal>(JSContext* cx,
                                                MutableHandleValue lhs,
                                                MutableHandleValue rhs,
                                                bool* res);
template bool LooselyEqual<EqualityKind::NotEqual>(JSContext* cx,
                                                   MutableHandleValue lhs,
                                                   MutableHandleValue rhs,
                                                   bool* res);

template <EqualityKind Kind>
bool StrictlyEqual(JSContext* cx, MutableHandleValue lhs,
                   MutableHandleValue rhs, bool* res) {
  if (!js::StrictlyEqual(cx, lhs, rhs, res)) {
    return false;
  }
  if (Kind != EqualityKind::Equal) {
    *res = !*res;
  }
  return true;
}

template bool StrictlyEqual<EqualityKind::Equal>(JSContext* cx,
                                                 MutableHandleValue lhs,
                                                 MutableHandleValue rhs,
                                                 bool* res);
template bool StrictlyEqual<EqualityKind::NotEqual>(JSContext* cx,
                                                    MutableHandleValue lhs,
                                                    MutableHandleValue rhs,
                                                    bool* res);

bool LessThan(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
              bool* res) {
  return LessThanOperation(cx, lhs, rhs, res);
}

bool LessThanOrEqual(JSContext* cx, MutableHandleValue lhs,
                     MutableHandleValue rhs, bool* res) {
  return LessThanOrEqualOperation(cx, lhs, rhs, res);
}

bool GreaterThan(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
                 bool* res) {
  return GreaterThanOperation(cx, lhs, rhs, res);
}

bool GreaterThanOrEqual(JSContext* cx, MutableHandleValue lhs,
                        MutableHandleValue rhs, bool* res) {
  return GreaterThanOrEqualOperation(cx, lhs, rhs, res);
}

template <EqualityKind Kind>
bool StringsEqual(JSContext* cx, HandleString lhs, HandleString rhs,
                  bool* res) {
  if (!js::EqualStrings(cx, lhs, rhs, res)) {
    return false;
  }
  if (Kind != EqualityKind::Equal) {
    *res = !*res;
  }
  return true;
}

template bool StringsEqual<EqualityKind::Equal>(JSContext* cx, HandleString lhs,
                                                HandleString rhs, bool* res);
template bool StringsEqual<EqualityKind::NotEqual>(JSContext* cx,
                                                   HandleString lhs,
                                                   HandleString rhs, bool* res);

template <ComparisonKind Kind>
bool StringsCompare(JSContext* cx, HandleString lhs, HandleString rhs,
                    bool* res) {
  int32_t result;
  if (!js::CompareStrings(cx, lhs, rhs, &result)) {
    return false;
  }
  if (Kind == ComparisonKind::LessThan) {
    *res = result < 0;
  } else {
    *res = result >= 0;
  }
  return true;
}

template bool StringsCompare<ComparisonKind::LessThan>(JSContext* cx,
                                                       HandleString lhs,
                                                       HandleString rhs,
                                                       bool* res);
template bool StringsCompare<ComparisonKind::GreaterThanOrEqual>(
    JSContext* cx, HandleString lhs, HandleString rhs, bool* res);

bool ArrayPopDense(JSContext* cx, HandleObject obj, MutableHandleValue rval) {
  MOZ_ASSERT(obj->is<ArrayObject>());

  AutoDetectInvalidation adi(cx, rval);

  JS::AutoValueArray<2> argv(cx);
  argv[0].setUndefined();
  argv[1].setObject(*obj);
  if (!js::array_pop(cx, 0, argv.begin())) {
    return false;
  }

  // If the result is |undefined|, the array was probably empty and we
  // have to monitor the return value.
  rval.set(argv[0]);
  if (rval.isUndefined()) {
    jsbytecode* pc;
    JSScript* script = cx->currentScript(&pc);
    JitScript::MonitorBytecodeType(cx, script, pc, rval);
  }
  return true;
}

bool ArrayPushDense(JSContext* cx, HandleArrayObject arr, HandleValue v,
                    uint32_t* length) {
  *length = arr->length();
  DenseElementResult result = arr->setOrExtendDenseElements(
      cx, *length, v.address(), 1, ShouldUpdateTypes::DontUpdate);
  if (result != DenseElementResult::Incomplete) {
    (*length)++;
    return result == DenseElementResult::Success;
  }

  // AutoDetectInvalidation uses GetTopJitJSScript(cx)->ionScript(), but it's
  // possible the setOrExtendDenseElements call already invalidated the
  // IonScript. JSJitFrameIter::ionScript works when the script is invalidated
  // so we use that instead.
  JSJitFrameIter frame(cx->activation()->asJit());
  MOZ_ASSERT(frame.type() == FrameType::Exit);
  ++frame;
  IonScript* ionScript = frame.ionScript();

  JS::AutoValueArray<3> argv(cx);
  AutoDetectInvalidation adi(cx, argv[0], ionScript);
  argv[0].setUndefined();
  argv[1].setObject(*arr);
  argv[2].set(v);
  if (!js::array_push(cx, 1, argv.begin())) {
    return false;
  }

  if (argv[0].isInt32()) {
    *length = argv[0].toInt32();
    return true;
  }

  // array_push changed the length to be larger than INT32_MAX. In this case
  // OBJECT_FLAG_LENGTH_OVERFLOW was set, TI invalidated the script, and the
  // AutoDetectInvalidation instance on the stack will replace *length with
  // the actual return value during bailout.
  MOZ_ASSERT(adi.shouldSetReturnOverride());
  MOZ_ASSERT(argv[0].toDouble() == double(INT32_MAX) + 1);
  *length = 0;
  return true;
}

bool ArrayShiftDense(JSContext* cx, HandleObject obj, MutableHandleValue rval) {
  MOZ_ASSERT(obj->is<ArrayObject>());

  AutoDetectInvalidation adi(cx, rval);

  JS::AutoValueArray<2> argv(cx);
  argv[0].setUndefined();
  argv[1].setObject(*obj);
  if (!js::array_shift(cx, 0, argv.begin())) {
    return false;
  }

  // If the result is |undefined|, the array was probably empty and we
  // have to monitor the return value.
  rval.set(argv[0]);
  if (rval.isUndefined()) {
    jsbytecode* pc;
    JSScript* script = cx->currentScript(&pc);
    JitScript::MonitorBytecodeType(cx, script, pc, rval);
  }
  return true;
}

JSString* ArrayJoin(JSContext* cx, HandleObject array, HandleString sep) {
  JS::AutoValueArray<3> argv(cx);
  argv[0].setUndefined();
  argv[1].setObject(*array);
  argv[2].setString(sep);
  if (!js::array_join(cx, 1, argv.begin())) {
    return nullptr;
  }
  return argv[0].toString();
}

bool SetArrayLength(JSContext* cx, HandleObject obj, HandleValue value,
                    bool strict) {
  Handle<ArrayObject*> array = obj.as<ArrayObject>();

  RootedId id(cx, NameToId(cx->names().length));
  ObjectOpResult result;

  // SetArrayLength is called by IC stubs for SetProp and SetElem on arrays'
  // "length" property.
  //
  // ArraySetLength below coerces |value| before checking for length being
  // writable, and in the case of illegal values, will throw RangeError even
  // when "length" is not writable. This is incorrect observable behavior,
  // as a regular [[Set]] operation will check for "length" being
  // writable before attempting any assignment.
  //
  // So, perform ArraySetLength if and only if "length" is writable.
  if (array->lengthIsWritable()) {
    if (!ArraySetLength(cx, array, id, JSPROP_PERMANENT, value, result)) {
      return false;
    }
  } else {
    MOZ_ALWAYS_TRUE(result.fail(JSMSG_READ_ONLY));
  }

  return result.checkStrictErrorOrWarning(cx, obj, id, strict);
}

bool CharCodeAt(JSContext* cx, HandleString str, int32_t index,
                uint32_t* code) {
  char16_t c;
  if (!str->getChar(cx, index, &c)) {
    return false;
  }
  *code = c;
  return true;
}

JSFlatString* StringFromCharCode(JSContext* cx, int32_t code) {
  char16_t c = char16_t(code);

  if (StaticStrings::hasUnit(c)) {
    return cx->staticStrings().getUnit(c);
  }

  return NewStringCopyNDontDeflate<CanGC>(cx, &c, 1);
}

JSString* StringFromCodePoint(JSContext* cx, int32_t codePoint) {
  RootedValue rval(cx, Int32Value(codePoint));
  if (!str_fromCodePoint_one_arg(cx, rval, &rval)) {
    return nullptr;
  }

  return rval.toString();
}

bool SetProperty(JSContext* cx, HandleObject obj, HandlePropertyName name,
                 HandleValue value, bool strict, jsbytecode* pc) {
  RootedId id(cx, NameToId(name));

  JSOp op = JSOp(*pc);

  if (op == JSOP_SETALIASEDVAR || op == JSOP_INITALIASEDLEXICAL) {
    // Aliased var assigns ignore readonly attributes on the property, as
    // required for initializing 'const' closure variables.
    Shape* shape = obj->as<NativeObject>().lookup(cx, name);
    MOZ_ASSERT(shape && shape->isDataProperty());
    obj->as<NativeObject>().setSlotWithType(cx, shape, value);
    return true;
  }

  RootedValue receiver(cx, ObjectValue(*obj));
  ObjectOpResult result;
  if (MOZ_LIKELY(!obj->getOpsSetProperty())) {
    if (op == JSOP_SETNAME || op == JSOP_STRICTSETNAME || op == JSOP_SETGNAME ||
        op == JSOP_STRICTSETGNAME) {
      if (!NativeSetProperty<Unqualified>(cx, obj.as<NativeObject>(), id, value,
                                          receiver, result)) {
        return false;
      }
    } else {
      if (!NativeSetProperty<Qualified>(cx, obj.as<NativeObject>(), id, value,
                                        receiver, result)) {
        return false;
      }
    }
  } else {
    if (!SetProperty(cx, obj, id, value, receiver, result)) {
      return false;
    }
  }
  return result.checkStrictErrorOrWarning(cx, obj, id, strict);
}

bool InterruptCheck(JSContext* cx) {
  gc::MaybeVerifyBarriers(cx);

  return CheckForInterrupt(cx);
}

JSObject* NewCallObject(JSContext* cx, HandleShape shape,
                        HandleObjectGroup group) {
  JSObject* obj = CallObject::create(cx, shape, group);
  if (!obj) {
    return nullptr;
  }

  // The JIT creates call objects in the nursery, so elides barriers for
  // the initializing writes. The interpreter, however, may have allocated
  // the call object tenured, so barrier as needed before re-entering.
  if (!IsInsideNursery(obj)) {
    cx->runtime()->gc.storeBuffer().putWholeCell(obj);
  }

  return obj;
}

JSObject* NewStringObject(JSContext* cx, HandleString str) {
  return StringObject::create(cx, str);
}

bool OperatorIn(JSContext* cx, HandleValue key, HandleObject obj, bool* out) {
  RootedId id(cx);
  return ToPropertyKey(cx, key, &id) && HasProperty(cx, obj, id, out);
}

bool OperatorInI(JSContext* cx, uint32_t index, HandleObject obj, bool* out) {
  RootedValue key(cx, Int32Value(index));
  return OperatorIn(cx, key, obj, out);
}

bool GetIntrinsicValue(JSContext* cx, HandlePropertyName name,
                       MutableHandleValue rval) {
  if (!GlobalObject::getIntrinsicValue(cx, cx->global(), name, rval)) {
    return false;
  }

  // This function is called when we try to compile a cold getintrinsic
  // op. MCallGetIntrinsicValue has an AliasSet of None for optimization
  // purposes, as its side effect is not observable from JS. We are
  // guaranteed to bail out after this function, but because of its AliasSet,
  // type info will not be reflowed. Manually monitor here.
  jsbytecode* pc;
  JSScript* script = cx->currentScript(&pc);
  JitScript::MonitorBytecodeType(cx, script, pc, rval);

  return true;
}

bool CreateThis(JSContext* cx, HandleObject callee, HandleObject newTarget,
                MutableHandleValue rval) {
  rval.set(MagicValue(JS_IS_CONSTRUCTING));

  if (callee->is<JSFunction>()) {
    RootedFunction fun(cx, &callee->as<JSFunction>());
    if (fun->isInterpreted() && fun->isConstructor()) {
      JSScript* script = JSFunction::getOrCreateScript(cx, fun);
      if (!script) {
        return false;
      }
      if (!js::CreateThis(cx, fun, script, newTarget, GenericObject, rval)) {
        return false;
      }
      MOZ_ASSERT_IF(rval.isObject(),
                    fun->realm() == rval.toObject().nonCCWRealm());
    }
  }

  return true;
}

bool GetDynamicNamePure(JSContext* cx, JSObject* envChain, JSString* str,
                        Value* vp) {
  // Lookup a string on the env chain, returning the value found through rval.
  // This function is infallible, and cannot GC or invalidate.
  // Returns false if the lookup could not be completed without GC.

  AutoUnsafeCallWithABI unsafe;

  JSAtom* atom;
  if (str->isAtom()) {
    atom = &str->asAtom();
  } else {
    atom = AtomizeString(cx, str);
    if (!atom) {
      cx->recoverFromOutOfMemory();
      return false;
    }
  }

  if (!frontend::IsIdentifier(atom) || frontend::IsKeyword(atom)) {
    return false;
  }

  PropertyResult prop;
  JSObject* scope = nullptr;
  JSObject* pobj = nullptr;
  if (LookupNameNoGC(cx, atom->asPropertyName(), envChain, &scope, &pobj,
                     &prop)) {
    return FetchNameNoGC(pobj, prop, vp);
  }
  return false;
}

void PostWriteBarrier(JSRuntime* rt, js::gc::Cell* cell) {
  AutoUnsafeCallWithABI unsafe;
  MOZ_ASSERT(!IsInsideNursery(cell));
  rt->gc.storeBuffer().putWholeCell(cell);
}

static const size_t MAX_WHOLE_CELL_BUFFER_SIZE = 4096;

template <IndexInBounds InBounds>
void PostWriteElementBarrier(JSRuntime* rt, JSObject* obj, int32_t index) {
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(!IsInsideNursery(obj));

  if (InBounds == IndexInBounds::Yes) {
    MOZ_ASSERT(uint32_t(index) <
               obj->as<NativeObject>().getDenseInitializedLength());
  } else {
    if (MOZ_UNLIKELY(!obj->is<NativeObject>() || index < 0 ||
                     uint32_t(index) >=
                         NativeObject::MAX_DENSE_ELEMENTS_COUNT)) {
      rt->gc.storeBuffer().putWholeCell(obj);
      return;
    }
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (nobj->isInWholeCellBuffer()) {
    return;
  }

  if (nobj->getDenseInitializedLength() > MAX_WHOLE_CELL_BUFFER_SIZE
#ifdef JS_GC_ZEAL
      || rt->hasZealMode(gc::ZealMode::ElementsBarrier)
#endif
  ) {
    rt->gc.storeBuffer().putSlot(nobj, HeapSlot::Element,
                                 nobj->unshiftedIndex(index), 1);
    return;
  }

  rt->gc.storeBuffer().putWholeCell(obj);
}

template void PostWriteElementBarrier<IndexInBounds::Yes>(JSRuntime* rt,
                                                          JSObject* obj,
                                                          int32_t index);

template void PostWriteElementBarrier<IndexInBounds::Maybe>(JSRuntime* rt,
                                                            JSObject* obj,
                                                            int32_t index);

void PostGlobalWriteBarrier(JSRuntime* rt, GlobalObject* obj) {
  MOZ_ASSERT(obj->JSObject::is<GlobalObject>());

  if (!obj->realm()->globalWriteBarriered) {
    PostWriteBarrier(rt, obj);
    obj->realm()->globalWriteBarriered = 1;
  }
}

int32_t GetIndexFromString(JSString* str) {
  // We shouldn't GC here as this is called directly from IC code.
  AutoUnsafeCallWithABI unsafe;

  if (!str->isFlat()) {
    return -1;
  }

  uint32_t index = UINT32_MAX;
  if (!str->asFlat().isIndex(&index) || index > INT32_MAX) {
    return -1;
  }

  return int32_t(index);
}

JSObject* WrapObjectPure(JSContext* cx, JSObject* obj) {
  // IC code calls this directly so we shouldn't GC.
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(obj);
  MOZ_ASSERT(cx->compartment() != obj->compartment());

  // From: Compartment::getNonWrapperObjectForCurrentCompartment
  // Note that if the object is same-compartment, but has been wrapped into a
  // different compartment, we need to unwrap it and return the bare same-
  // compartment object. Note again that windows are always wrapped by a
  // WindowProxy even when same-compartment so take care not to strip this
  // particular wrapper.
  obj = UncheckedUnwrap(obj, /* stopAtWindowProxy = */ true);
  if (cx->compartment() == obj->compartment()) {
    MOZ_ASSERT(!IsWindow(obj));
    JS::ExposeObjectToActiveJS(obj);
    return obj;
  }

  // Try to Lookup an existing wrapper for this object. We assume that
  // if we can find such a wrapper, not calling preWrap is correct.
  if (ObjectWrapperMap::Ptr p = cx->compartment()->lookupWrapper(obj)) {
    JSObject* wrapped = p->value().get();

    // Ensure the wrapper is still exposed.
    JS::ExposeObjectToActiveJS(wrapped);
    return wrapped;
  }

  return nullptr;
}

static bool HandlePrologueResumeMode(JSContext* cx, BaselineFrame* frame,
                                     jsbytecode* pc, bool* mustReturn,
                                     ResumeMode resumeMode) {
  *mustReturn = false;
  switch (resumeMode) {
    case ResumeMode::Continue:
      return true;

    case ResumeMode::Return:
      // The script is going to return immediately, so we have to call the
      // debug epilogue handler as well.
      MOZ_ASSERT(frame->hasReturnValue());
      *mustReturn = true;
      return jit::DebugEpilogue(cx, frame, pc, true);

    case ResumeMode::Throw:
    case ResumeMode::Terminate:
      return false;

    default:
      MOZ_CRASH("bad DebugAPI::onEnterFrame resume mode");
  }
}

bool DebugPrologue(JSContext* cx, BaselineFrame* frame, jsbytecode* pc,
                   bool* mustReturn) {
  ResumeMode resumeMode = DebugAPI::onEnterFrame(cx, frame);
  return HandlePrologueResumeMode(cx, frame, pc, mustReturn, resumeMode);
}

bool DebugEpilogueOnBaselineReturn(JSContext* cx, BaselineFrame* frame,
                                   jsbytecode* pc) {
  if (!DebugEpilogue(cx, frame, pc, true)) {
    // DebugEpilogue popped the frame by updating packedExitFP, so run the
    // stop event here before we enter the exception handler.
    TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
    TraceLogStopEvent(logger, TraceLogger_Baseline);
    TraceLogStopEvent(logger, TraceLogger_Scripts);
    return false;
  }

  return true;
}

bool DebugEpilogue(JSContext* cx, BaselineFrame* frame, jsbytecode* pc,
                   bool ok) {
  // If DebugAPI::onLeaveFrame returns |true| we have to return the frame's
  // return value. If it returns |false|, the debugger threw an exception.
  // In both cases we have to pop debug scopes.
  ok = DebugAPI::onLeaveFrame(cx, frame, pc, ok);

  // Unwind to the outermost environment.
  EnvironmentIter ei(cx, frame, pc);
  UnwindAllEnvironmentsInFrame(cx, ei);

  if (!ok) {
    // Pop this frame by updating packedExitFP, so that the exception
    // handling code will start at the previous frame.
    JitFrameLayout* prefix = frame->framePrefix();
    EnsureBareExitFrame(cx->activation()->asJit(), prefix);
    return false;
  }

  return true;
}

void FrameIsDebuggeeCheck(BaselineFrame* frame) {
  AutoUnsafeCallWithABI unsafe;
  if (frame->script()->isDebuggee()) {
    frame->setIsDebuggee();
  }
}

JSObject* CreateGenerator(JSContext* cx, BaselineFrame* frame) {
  return AbstractGeneratorObject::create(cx, frame);
}

bool NormalSuspend(JSContext* cx, HandleObject obj, BaselineFrame* frame,
                   jsbytecode* pc) {
  MOZ_ASSERT(*pc == JSOP_YIELD || *pc == JSOP_AWAIT);

  MOZ_ASSERT(frame->numValueSlots() > frame->script()->nfixed());
  uint32_t stackDepth = frame->numValueSlots() - frame->script()->nfixed();

  // Return value is still on the stack.
  MOZ_ASSERT(stackDepth >= 1);

  // The expression stack slots are stored on the stack in reverse order, so
  // we copy them to a Vector and pass a pointer to that instead. We use
  // stackDepth - 1 because we don't want to include the return value.
  RootedValueVector exprStack(cx);
  if (!exprStack.reserve(stackDepth - 1)) {
    return false;
  }

  size_t firstSlot = frame->numValueSlots() - stackDepth;
  for (size_t i = 0; i < stackDepth - 1; i++) {
    exprStack.infallibleAppend(*frame->valueSlot(firstSlot + i));
  }

  MOZ_ASSERT(exprStack.length() == stackDepth - 1);

  return AbstractGeneratorObject::normalSuspend(
      cx, obj, frame, pc, exprStack.begin(), stackDepth - 1);
}

bool FinalSuspend(JSContext* cx, HandleObject obj, jsbytecode* pc) {
  MOZ_ASSERT(*pc == JSOP_FINALYIELDRVAL);
  AbstractGeneratorObject::finalSuspend(obj);
  return true;
}

bool InterpretResume(JSContext* cx, HandleObject obj, HandleValue val,
                     HandlePropertyName kind, MutableHandleValue rval) {
  MOZ_ASSERT(obj->is<AbstractGeneratorObject>());

  FixedInvokeArgs<3> args(cx);

  args[0].setObject(*obj);
  args[1].set(val);
  args[2].setString(kind);

  return CallSelfHostedFunction(cx, cx->names().InterpretGeneratorResume,
                                UndefinedHandleValue, args, rval);
}

bool DebugAfterYield(JSContext* cx, BaselineFrame* frame, jsbytecode* pc,
                     bool* mustReturn) {
  // The BaselineFrame has just been constructed by JSOP_RESUME in the
  // caller. We need to set its debuggee flag as necessary.
  //
  // If a breakpoint is set on JSOP_AFTERYIELD, or stepping is enabled,
  // we may already have done this work. Don't fire onEnterFrame again.
  if (frame->script()->isDebuggee() && !frame->isDebuggee()) {
    frame->setIsDebuggee();
    ResumeMode resumeMode = DebugAPI::onResumeFrame(cx, frame);
    return HandlePrologueResumeMode(cx, frame, pc, mustReturn, resumeMode);
  }

  *mustReturn = false;
  return true;
}

bool GeneratorThrowOrReturn(JSContext* cx, BaselineFrame* frame,
                            Handle<AbstractGeneratorObject*> genObj,
                            HandleValue arg, uint32_t resumeKindArg) {
  JSScript* script = frame->script();
  uint32_t offset = script->resumeOffsets()[genObj->resumeIndex()];
  jsbytecode* pc = script->offsetToPC(offset);

  // Always use an interpreter frame so frame iteration can easily recover the
  // generator's bytecode pc (we don't have a matching RetAddrEntry in the
  // BaselineScript).
  MOZ_ASSERT(!frame->runningInInterpreter());
  frame->initInterpFieldsForGeneratorThrowOrReturn(script, pc);

  // In the interpreter, AbstractGeneratorObject::resume marks the generator as
  // running, so we do the same.
  genObj->setRunning();

  bool mustReturn = false;
  if (!DebugAfterYield(cx, frame, pc, &mustReturn)) {
    return false;
  }

  GeneratorResumeKind resumeKind = GeneratorResumeKind(resumeKindArg);
  if (mustReturn) {
    resumeKind = GeneratorResumeKind::Return;
  }

  MOZ_ALWAYS_FALSE(
      js::GeneratorThrowOrReturn(cx, frame, genObj, arg, resumeKind));
  return false;
}

bool GlobalNameConflictsCheckFromIon(JSContext* cx, HandleScript script) {
  Rooted<LexicalEnvironmentObject*> globalLexical(
      cx, &cx->global()->lexicalEnvironment());
  return CheckGlobalDeclarationConflicts(cx, script, globalLexical,
                                         cx->global());
}

bool InitFunctionEnvironmentObjects(JSContext* cx, BaselineFrame* frame) {
  return frame->initFunctionEnvironmentObjects(cx);
}

bool NewArgumentsObject(JSContext* cx, BaselineFrame* frame,
                        MutableHandleValue res) {
  ArgumentsObject* obj = ArgumentsObject::createExpected(cx, frame);
  if (!obj) {
    return false;
  }
  res.setObject(*obj);
  return true;
}

JSObject* CopyLexicalEnvironmentObject(JSContext* cx, HandleObject env,
                                       bool copySlots) {
  Handle<LexicalEnvironmentObject*> lexicalEnv =
      env.as<LexicalEnvironmentObject>();

  if (copySlots) {
    return LexicalEnvironmentObject::clone(cx, lexicalEnv);
  }

  return LexicalEnvironmentObject::recreate(cx, lexicalEnv);
}

JSObject* InitRestParameter(JSContext* cx, uint32_t length, Value* rest,
                            HandleObject templateObj, HandleObject objRes) {
  if (objRes) {
    Rooted<ArrayObject*> arrRes(cx, &objRes->as<ArrayObject>());

    MOZ_ASSERT(!arrRes->getDenseInitializedLength());
    MOZ_ASSERT(arrRes->group() == templateObj->group());

    // Fast path: we managed to allocate the array inline; initialize the
    // slots.
    if (length > 0) {
      if (!arrRes->ensureElements(cx, length)) {
        return nullptr;
      }
      arrRes->initDenseElements(rest, length);
      arrRes->setLengthInt32(length);
    }
    return arrRes;
  }

  NewObjectKind newKind;
  {
    AutoSweepObjectGroup sweep(templateObj->group());
    newKind = templateObj->group()->shouldPreTenure(sweep) ? TenuredObject
                                                           : GenericObject;
  }
  ArrayObject* arrRes = NewDenseCopiedArray(cx, length, rest, nullptr, newKind);
  if (arrRes) {
    arrRes->setGroup(templateObj->group());
  }
  return arrRes;
}

bool HandleDebugTrap(JSContext* cx, BaselineFrame* frame, uint8_t* retAddr,
                     bool* mustReturn) {
  *mustReturn = false;

  RootedScript script(cx, frame->script());
  jsbytecode* pc;
  if (frame->runningInInterpreter()) {
    pc = frame->interpreterPC();
  } else {
    BaselineScript* blScript = script->baselineScript();
    pc = blScript->retAddrEntryFromReturnAddress(retAddr).pc(script);
  }

  // The Baseline Interpreter calls HandleDebugTrap for every op when the script
  // is in step mode or has breakpoints. The Baseline Compiler can toggle
  // breakpoints more granularly for specific bytecode PCs.
  if (frame->runningInInterpreter()) {
    MOZ_ASSERT(DebugAPI::hasAnyBreakpointsOrStepMode(script));
  } else {
    MOZ_ASSERT(DebugAPI::stepModeEnabled(script) ||
               DebugAPI::hasBreakpointsAt(script, pc));
  }

  if (*pc == JSOP_AFTERYIELD) {
    // JSOP_AFTERYIELD will set the frame's debuggee flag and call the
    // onEnterFrame handler, but if we set a breakpoint there we have to do
    // it now.
    MOZ_ASSERT(!frame->isDebuggee());

    if (!DebugAfterYield(cx, frame, pc, mustReturn)) {
      return false;
    }
    if (*mustReturn) {
      return true;
    }

    // If the frame is not a debuggee we're done. This can happen, for instance,
    // if the onEnterFrame hook called removeDebuggee.
    if (!frame->isDebuggee()) {
      return true;
    }
  }

  MOZ_ASSERT(frame->isDebuggee());

  RootedValue rval(cx);
  ResumeMode resumeMode = ResumeMode::Continue;

  if (DebugAPI::stepModeEnabled(script)) {
    resumeMode = DebugAPI::onSingleStep(cx, &rval);
  }

  if (resumeMode == ResumeMode::Continue &&
      DebugAPI::hasBreakpointsAt(script, pc)) {
    resumeMode = DebugAPI::onTrap(cx, &rval);
  }

  switch (resumeMode) {
    case ResumeMode::Continue:
      break;

    case ResumeMode::Terminate:
      return false;

    case ResumeMode::Return:
      *mustReturn = true;
      frame->setReturnValue(rval);
      return jit::DebugEpilogue(cx, frame, pc, true);

    case ResumeMode::Throw:
      cx->setPendingExceptionAndCaptureStack(rval);
      return false;

    default:
      MOZ_CRASH("Invalid step/breakpoint resume mode");
  }

  return true;
}

bool OnDebuggerStatement(JSContext* cx, BaselineFrame* frame, jsbytecode* pc,
                         bool* mustReturn) {
  *mustReturn = false;

  switch (DebugAPI::onDebuggerStatement(cx, frame)) {
    case ResumeMode::Continue:
      return true;

    case ResumeMode::Return:
      *mustReturn = true;
      return jit::DebugEpilogue(cx, frame, pc, true);

    case ResumeMode::Throw:
    case ResumeMode::Terminate:
      return false;

    default:
      MOZ_CRASH("Invalid OnDebuggerStatement resume mode");
  }
}

bool GlobalHasLiveOnDebuggerStatement(JSContext* cx) {
  AutoUnsafeCallWithABI unsafe;
  return cx->realm()->isDebuggee() &&
         DebugAPI::hasDebuggerStatementHook(cx->global());
}

bool PushLexicalEnv(JSContext* cx, BaselineFrame* frame,
                    Handle<LexicalScope*> scope) {
  return frame->pushLexicalEnvironment(cx, scope);
}

bool PopLexicalEnv(JSContext* cx, BaselineFrame* frame) {
  frame->popOffEnvironmentChain<LexicalEnvironmentObject>();
  return true;
}

bool DebugLeaveThenPopLexicalEnv(JSContext* cx, BaselineFrame* frame,
                                 jsbytecode* pc) {
  MOZ_ALWAYS_TRUE(DebugLeaveLexicalEnv(cx, frame, pc));
  frame->popOffEnvironmentChain<LexicalEnvironmentObject>();
  return true;
}

bool FreshenLexicalEnv(JSContext* cx, BaselineFrame* frame) {
  return frame->freshenLexicalEnvironment(cx);
}

bool DebugLeaveThenFreshenLexicalEnv(JSContext* cx, BaselineFrame* frame,
                                     jsbytecode* pc) {
  MOZ_ALWAYS_TRUE(DebugLeaveLexicalEnv(cx, frame, pc));
  return frame->freshenLexicalEnvironment(cx);
}

bool RecreateLexicalEnv(JSContext* cx, BaselineFrame* frame) {
  return frame->recreateLexicalEnvironment(cx);
}

bool DebugLeaveThenRecreateLexicalEnv(JSContext* cx, BaselineFrame* frame,
                                      jsbytecode* pc) {
  MOZ_ALWAYS_TRUE(DebugLeaveLexicalEnv(cx, frame, pc));
  return frame->recreateLexicalEnvironment(cx);
}

bool DebugLeaveLexicalEnv(JSContext* cx, BaselineFrame* frame, jsbytecode* pc) {
  MOZ_ASSERT_IF(!frame->runningInInterpreter(),
                frame->script()->baselineScript()->hasDebugInstrumentation());
  if (cx->realm()->isDebuggee()) {
    DebugEnvironments::onPopLexical(cx, frame, pc);
  }
  return true;
}

bool PushVarEnv(JSContext* cx, BaselineFrame* frame, HandleScope scope) {
  return frame->pushVarEnvironment(cx, scope);
}

bool PopVarEnv(JSContext* cx, BaselineFrame* frame) {
  frame->popOffEnvironmentChain<VarEnvironmentObject>();
  return true;
}

bool EnterWith(JSContext* cx, BaselineFrame* frame, HandleValue val,
               Handle<WithScope*> templ) {
  return EnterWithOperation(cx, frame, val, templ);
}

bool LeaveWith(JSContext* cx, BaselineFrame* frame) {
  if (MOZ_UNLIKELY(frame->isDebuggee())) {
    DebugEnvironments::onPopWith(frame);
  }
  frame->popOffEnvironmentChain<WithEnvironmentObject>();
  return true;
}

bool InitBaselineFrameForOsr(BaselineFrame* frame,
                             InterpreterFrame* interpFrame,
                             uint32_t numStackValues) {
  return frame->initForOsr(interpFrame, numStackValues);
}

JSObject* CreateDerivedTypedObj(JSContext* cx, HandleObject descr,
                                HandleObject owner, int32_t offset) {
  MOZ_ASSERT(descr->is<TypeDescr>());
  MOZ_ASSERT(owner->is<TypedObject>());
  Rooted<TypeDescr*> descr1(cx, &descr->as<TypeDescr>());
  Rooted<TypedObject*> owner1(cx, &owner->as<TypedObject>());
  return OutlineTypedObject::createDerived(cx, descr1, owner1, offset);
}

JSString* StringReplace(JSContext* cx, HandleString string,
                        HandleString pattern, HandleString repl) {
  MOZ_ASSERT(string);
  MOZ_ASSERT(pattern);
  MOZ_ASSERT(repl);

  return str_replace_string_raw(cx, string, pattern, repl);
}

bool RecompileImpl(JSContext* cx, bool force) {
  MOZ_ASSERT(cx->currentlyRunningInJit());
  JitActivationIterator activations(cx);
  JSJitFrameIter frame(activations->asJit());

  MOZ_ASSERT(frame.type() == FrameType::Exit);
  ++frame;

  RootedScript script(cx, frame.script());
  MOZ_ASSERT(script->hasIonScript());

  if (!IsIonEnabled()) {
    return true;
  }

  MethodStatus status = Recompile(cx, script, nullptr, nullptr, force);
  if (status == Method_Error) {
    return false;
  }

  return true;
}

bool IonForcedRecompile(JSContext* cx) {
  return RecompileImpl(cx, /* force = */ true);
}

bool IonRecompile(JSContext* cx) {
  return RecompileImpl(cx, /* force = */ false);
}

bool IonForcedInvalidation(JSContext* cx) {
  MOZ_ASSERT(cx->currentlyRunningInJit());
  JitActivationIterator activations(cx);
  JSJitFrameIter frame(activations->asJit());

  MOZ_ASSERT(frame.type() == FrameType::Exit);
  ++frame;

  RootedScript script(cx, frame.script());
  MOZ_ASSERT(script->hasIonScript());

  if (script->baselineScript()->hasPendingIonBuilder()) {
    LinkIonScript(cx, script);
    return true;
  }

  Invalidate(cx, script, /* resetUses = */ false,
             /* cancelOffThread = */ false);
  return true;
}

bool SetDenseElement(JSContext* cx, HandleNativeObject obj, int32_t index,
                     HandleValue value, bool strict) {
  // This function is called from Ion code for StoreElementHole's OOL path.
  // In this case we know the object is native and that no type changes are
  // needed.

  DenseElementResult result = obj->setOrExtendDenseElements(
      cx, index, value.address(), 1, ShouldUpdateTypes::DontUpdate);
  if (result != DenseElementResult::Incomplete) {
    return result == DenseElementResult::Success;
  }

  RootedValue indexVal(cx, Int32Value(index));
  return SetObjectElement(cx, obj, indexVal, value, strict);
}

void AutoDetectInvalidation::setReturnOverride() {
  cx_->setIonReturnOverride(rval_.get());
}

void AssertValidObjectPtr(JSContext* cx, JSObject* obj) {
  AutoUnsafeCallWithABI unsafe;
#ifdef DEBUG
  // Check what we can, so that we'll hopefully assert/crash if we get a
  // bogus object (pointer).
  MOZ_ASSERT(obj->compartment() == cx->compartment());
  MOZ_ASSERT(obj->zoneFromAnyThread() == cx->zone());
  MOZ_ASSERT(obj->runtimeFromMainThread() == cx->runtime());

  MOZ_ASSERT_IF(!obj->hasLazyGroup(),
                obj->group()->clasp() == obj->shape()->getObjectClass());

  if (obj->isTenured()) {
    MOZ_ASSERT(obj->isAligned());
    gc::AllocKind kind = obj->asTenured().getAllocKind();
    MOZ_ASSERT(gc::IsObjectAllocKind(kind));
  }
#endif
}

void AssertValidObjectOrNullPtr(JSContext* cx, JSObject* obj) {
  AutoUnsafeCallWithABI unsafe;
  if (obj) {
    AssertValidObjectPtr(cx, obj);
  }
}

void AssertValidStringPtr(JSContext* cx, JSString* str) {
  AutoUnsafeCallWithABI unsafe;
#ifdef DEBUG
  // We can't closely inspect strings from another runtime.
  if (str->runtimeFromAnyThread() != cx->runtime()) {
    MOZ_ASSERT(str->isPermanentAtom());
    return;
  }

  if (str->isAtom()) {
    MOZ_ASSERT(str->zone()->isAtomsZone());
  } else {
    MOZ_ASSERT(str->zone() == cx->zone());
  }

  MOZ_ASSERT(str->isAligned());
  MOZ_ASSERT(str->length() <= JSString::MAX_LENGTH);

  gc::AllocKind kind = str->getAllocKind();
  if (str->isFatInline()) {
    MOZ_ASSERT(kind == gc::AllocKind::FAT_INLINE_STRING ||
               kind == gc::AllocKind::FAT_INLINE_ATOM);
  } else if (str->isExternal()) {
    MOZ_ASSERT(kind == gc::AllocKind::EXTERNAL_STRING);
  } else if (str->isAtom()) {
    MOZ_ASSERT(kind == gc::AllocKind::ATOM);
  } else if (str->isFlat()) {
    MOZ_ASSERT(kind == gc::AllocKind::STRING ||
               kind == gc::AllocKind::FAT_INLINE_STRING ||
               kind == gc::AllocKind::EXTERNAL_STRING);
  } else {
    MOZ_ASSERT(kind == gc::AllocKind::STRING);
  }
#endif
}

void AssertValidSymbolPtr(JSContext* cx, JS::Symbol* sym) {
  AutoUnsafeCallWithABI unsafe;

  // We can't closely inspect symbols from another runtime.
  if (sym->runtimeFromAnyThread() != cx->runtime()) {
    MOZ_ASSERT(sym->isWellKnownSymbol());
    return;
  }

  MOZ_ASSERT(sym->zone()->isAtomsZone());
  MOZ_ASSERT(sym->isAligned());
  if (JSString* desc = sym->description()) {
    MOZ_ASSERT(desc->isAtom());
    AssertValidStringPtr(cx, desc);
  }

  MOZ_ASSERT(sym->getAllocKind() == gc::AllocKind::SYMBOL);
}

void AssertValidBigIntPtr(JSContext* cx, JS::BigInt* bi) {
  AutoUnsafeCallWithABI unsafe;
  // FIXME: check runtime?
  MOZ_ASSERT(cx->zone() == bi->zone());
  MOZ_ASSERT(bi->isAligned());
  MOZ_ASSERT(bi->isTenured());
  MOZ_ASSERT(bi->getAllocKind() == gc::AllocKind::BIGINT);
}

void AssertValidValue(JSContext* cx, Value* v) {
  AutoUnsafeCallWithABI unsafe;
  if (v->isObject()) {
    AssertValidObjectPtr(cx, &v->toObject());
  } else if (v->isString()) {
    AssertValidStringPtr(cx, v->toString());
  } else if (v->isSymbol()) {
    AssertValidSymbolPtr(cx, v->toSymbol());
  } else if (v->isBigInt()) {
    AssertValidBigIntPtr(cx, v->toBigInt());
  }
}

bool ObjectIsCallable(JSObject* obj) {
  AutoUnsafeCallWithABI unsafe;
  return obj->isCallable();
}

bool ObjectIsConstructor(JSObject* obj) {
  AutoUnsafeCallWithABI unsafe;
  return obj->isConstructor();
}

void MarkValueFromJit(JSRuntime* rt, Value* vp) {
  AutoUnsafeCallWithABI unsafe;
  TraceManuallyBarrieredEdge(&rt->gc.marker, vp, "write barrier");
}

void MarkStringFromJit(JSRuntime* rt, JSString** stringp) {
  AutoUnsafeCallWithABI unsafe;
  MOZ_ASSERT(*stringp);
  TraceManuallyBarrieredEdge(&rt->gc.marker, stringp, "write barrier");
}

void MarkObjectFromJit(JSRuntime* rt, JSObject** objp) {
  AutoUnsafeCallWithABI unsafe;
  MOZ_ASSERT(*objp);
  TraceManuallyBarrieredEdge(&rt->gc.marker, objp, "write barrier");
}

void MarkShapeFromJit(JSRuntime* rt, Shape** shapep) {
  AutoUnsafeCallWithABI unsafe;
  TraceManuallyBarrieredEdge(&rt->gc.marker, shapep, "write barrier");
}

void MarkObjectGroupFromJit(JSRuntime* rt, ObjectGroup** groupp) {
  AutoUnsafeCallWithABI unsafe;
  TraceManuallyBarrieredEdge(&rt->gc.marker, groupp, "write barrier");
}

bool ThrowRuntimeLexicalError(JSContext* cx, unsigned errorNumber) {
  ScriptFrameIter iter(cx);
  RootedScript script(cx, iter.script());
  ReportRuntimeLexicalError(cx, errorNumber, script, iter.pc());
  return false;
}

bool ThrowBadDerivedReturn(JSContext* cx, HandleValue v) {
  ReportValueError(cx, JSMSG_BAD_DERIVED_RETURN, JSDVG_IGNORE_STACK, v,
                   nullptr);
  return false;
}

bool BaselineThrowUninitializedThis(JSContext* cx, BaselineFrame* frame) {
  return ThrowUninitializedThis(cx, frame);
}

bool BaselineThrowInitializedThis(JSContext* cx) {
  return ThrowInitializedThis(cx);
}

bool ThrowObjectCoercible(JSContext* cx, HandleValue v) {
  MOZ_ASSERT(v.isUndefined() || v.isNull());
  MOZ_ALWAYS_FALSE(ToObjectSlow(cx, v, true));
  return false;
}

bool BaselineGetFunctionThis(JSContext* cx, BaselineFrame* frame,
                             MutableHandleValue res) {
  return GetFunctionThis(cx, frame, res);
}

bool CallNativeGetter(JSContext* cx, HandleFunction callee, HandleObject obj,
                      MutableHandleValue result) {
  AutoRealm ar(cx, callee);

  MOZ_ASSERT(callee->isNative());
  JSNative natfun = callee->native();

  JS::AutoValueArray<2> vp(cx);
  vp[0].setObject(*callee.get());
  vp[1].setObject(*obj.get());

  if (!natfun(cx, 0, vp.begin())) {
    return false;
  }

  result.set(vp[0]);
  return true;
}

bool CallNativeGetterByValue(JSContext* cx, HandleFunction callee,
                             HandleValue receiver, MutableHandleValue result) {
  AutoRealm ar(cx, callee);

  MOZ_ASSERT(callee->isNative());
  JSNative natfun = callee->native();

  JS::AutoValueArray<2> vp(cx);
  vp[0].setObject(*callee.get());
  vp[1].set(receiver);

  if (!natfun(cx, 0, vp.begin())) {
    return false;
  }

  result.set(vp[0]);
  return true;
}

bool CallNativeSetter(JSContext* cx, HandleFunction callee, HandleObject obj,
                      HandleValue rhs) {
  AutoRealm ar(cx, callee);

  MOZ_ASSERT(callee->isNative());
  JSNative natfun = callee->native();

  JS::AutoValueArray<3> vp(cx);
  vp[0].setObject(*callee.get());
  vp[1].setObject(*obj.get());
  vp[2].set(rhs);

  return natfun(cx, 1, vp.begin());
}

bool EqualStringsHelperPure(JSString* str1, JSString* str2) {
  // IC code calls this directly so we shouldn't GC.
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(str1->isAtom());
  MOZ_ASSERT(!str2->isAtom());
  MOZ_ASSERT(str1->length() == str2->length());

  // ensureLinear is intentionally called with a nullptr to avoid OOM
  // reporting; if it fails, we will continue to the next stub.
  JSLinearString* str2Linear = str2->ensureLinear(nullptr);
  if (!str2Linear) {
    return false;
  }

  return EqualChars(&str1->asLinear(), str2Linear);
}

bool CheckIsCallable(JSContext* cx, HandleValue v, CheckIsCallableKind kind) {
  if (!IsCallable(v)) {
    return ThrowCheckIsCallable(cx, kind);
  }

  return true;
}

template <bool HandleMissing>
static MOZ_ALWAYS_INLINE bool GetNativeDataPropertyPure(JSContext* cx,
                                                        NativeObject* obj,
                                                        jsid id, Value* vp) {
  // Fast path used by megamorphic IC stubs. Unlike our other property
  // lookup paths, this is optimized to be as fast as possible for simple
  // data property lookups.

  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(JSID_IS_ATOM(id) || JSID_IS_SYMBOL(id));

  while (true) {
    if (Shape* shape = obj->lastProperty()->search(cx, id)) {
      if (!shape->isDataProperty()) {
        return false;
      }

      *vp = obj->getSlot(shape->slot());
      return true;
    }

    // Property not found. Watch out for Class hooks.
    if (MOZ_UNLIKELY(!obj->is<PlainObject>())) {
      if (ClassMayResolveId(cx->names(), obj->getClass(), id, obj)) {
        return false;
      }
    }

    JSObject* proto = obj->staticPrototype();
    if (!proto) {
      if (HandleMissing) {
        vp->setUndefined();
        return true;
      }
      return false;
    }

    if (!proto->isNative()) {
      return false;
    }
    obj = &proto->as<NativeObject>();
  }
}

template <bool HandleMissing>
bool GetNativeDataPropertyPure(JSContext* cx, JSObject* obj, PropertyName* name,
                               Value* vp) {
  // Condition checked by caller.
  MOZ_ASSERT(obj->isNative());
  return GetNativeDataPropertyPure<HandleMissing>(cx, &obj->as<NativeObject>(),
                                                  NameToId(name), vp);
}

template bool GetNativeDataPropertyPure<true>(JSContext* cx, JSObject* obj,
                                              PropertyName* name, Value* vp);

template bool GetNativeDataPropertyPure<false>(JSContext* cx, JSObject* obj,
                                               PropertyName* name, Value* vp);

static MOZ_ALWAYS_INLINE bool ValueToAtomOrSymbolPure(JSContext* cx,
                                                      Value& idVal, jsid* id) {
  if (MOZ_LIKELY(idVal.isString())) {
    JSString* s = idVal.toString();
    JSAtom* atom;
    if (s->isAtom()) {
      atom = &s->asAtom();
    } else {
      atom = AtomizeString(cx, s);
      if (!atom) {
        cx->recoverFromOutOfMemory();
        return false;
      }
    }
    *id = AtomToId(atom);
  } else if (idVal.isSymbol()) {
    *id = SYMBOL_TO_JSID(idVal.toSymbol());
  } else {
    if (!ValueToIdPure(idVal, id)) {
      return false;
    }
  }

  // Watch out for ids that may be stored in dense elements.
  static_assert(NativeObject::MAX_DENSE_ELEMENTS_COUNT < JSID_INT_MAX,
                "All dense elements must have integer jsids");
  if (MOZ_UNLIKELY(JSID_IS_INT(*id))) {
    return false;
  }

  return true;
}

template <bool HandleMissing>
bool GetNativeDataPropertyByValuePure(JSContext* cx, JSObject* obj, Value* vp) {
  AutoUnsafeCallWithABI unsafe;

  // Condition checked by caller.
  MOZ_ASSERT(obj->isNative());

  // vp[0] contains the id, result will be stored in vp[1].
  Value idVal = vp[0];
  jsid id;
  if (!ValueToAtomOrSymbolPure(cx, idVal, &id)) {
    return false;
  }

  Value* res = vp + 1;
  return GetNativeDataPropertyPure<HandleMissing>(cx, &obj->as<NativeObject>(),
                                                  id, res);
}

template bool GetNativeDataPropertyByValuePure<true>(JSContext* cx,
                                                     JSObject* obj, Value* vp);

template bool GetNativeDataPropertyByValuePure<false>(JSContext* cx,
                                                      JSObject* obj, Value* vp);

template <bool NeedsTypeBarrier>
bool SetNativeDataPropertyPure(JSContext* cx, JSObject* obj, PropertyName* name,
                               Value* val) {
  AutoUnsafeCallWithABI unsafe;

  if (MOZ_UNLIKELY(!obj->isNative())) {
    return false;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  Shape* shape = nobj->lastProperty()->search(cx, NameToId(name));
  if (!shape || !shape->isDataProperty() || !shape->writable()) {
    return false;
  }

  if (NeedsTypeBarrier && !HasTypePropertyId(nobj, NameToId(name), *val)) {
    return false;
  }

  nobj->setSlot(shape->slot(), *val);
  return true;
}

template bool SetNativeDataPropertyPure<true>(JSContext* cx, JSObject* obj,
                                              PropertyName* name, Value* val);

template bool SetNativeDataPropertyPure<false>(JSContext* cx, JSObject* obj,
                                               PropertyName* name, Value* val);

bool ObjectHasGetterSetterPure(JSContext* cx, JSObject* objArg,
                               Shape* propShape) {
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(propShape->hasGetterObject() || propShape->hasSetterObject());

  // Window objects may require outerizing (passing the WindowProxy to the
  // getter/setter), so we don't support them here.
  if (MOZ_UNLIKELY(!objArg->isNative() || IsWindow(objArg))) {
    return false;
  }

  NativeObject* nobj = &objArg->as<NativeObject>();
  jsid id = propShape->propid();

  while (true) {
    if (Shape* shape = nobj->lastProperty()->search(cx, id)) {
      if (shape == propShape) {
        return true;
      }
      if (shape->getterOrUndefined() == propShape->getterOrUndefined() &&
          shape->setterOrUndefined() == propShape->setterOrUndefined()) {
        return true;
      }
      return false;
    }

    // Property not found. Watch out for Class hooks.
    if (!nobj->is<PlainObject>()) {
      if (ClassMayResolveId(cx->names(), nobj->getClass(), id, nobj)) {
        return false;
      }
    }

    JSObject* proto = nobj->staticPrototype();
    if (!proto) {
      return false;
    }

    if (!proto->isNative()) {
      return false;
    }
    nobj = &proto->as<NativeObject>();
  }
}

template <bool HasOwn>
bool HasNativeDataPropertyPure(JSContext* cx, JSObject* obj, Value* vp) {
  AutoUnsafeCallWithABI unsafe;

  // vp[0] contains the id, result will be stored in vp[1].
  Value idVal = vp[0];
  jsid id;
  if (!ValueToAtomOrSymbolPure(cx, idVal, &id)) {
    return false;
  }

  do {
    if (obj->isNative()) {
      if (obj->as<NativeObject>().lastProperty()->search(cx, id)) {
        vp[1].setBoolean(true);
        return true;
      }

      // Fail if there's a resolve hook, unless the mayResolve hook tells
      // us the resolve hook won't define a property with this id.
      if (MOZ_UNLIKELY(
              ClassMayResolveId(cx->names(), obj->getClass(), id, obj))) {
        return false;
      }
    } else if (obj->is<TypedObject>()) {
      if (obj->as<TypedObject>().typeDescr().hasProperty(cx->names(), id)) {
        vp[1].setBoolean(true);
        return true;
      }
    } else {
      return false;
    }

    // If implementing Object.hasOwnProperty, don't follow protochain.
    if (HasOwn) {
      break;
    }

    // Get prototype. Objects that may allow dynamic prototypes are already
    // filtered out above.
    obj = obj->staticPrototype();
  } while (obj);

  // Missing property.
  vp[1].setBoolean(false);
  return true;
}

template bool HasNativeDataPropertyPure<true>(JSContext* cx, JSObject* obj,
                                              Value* vp);

template bool HasNativeDataPropertyPure<false>(JSContext* cx, JSObject* obj,
                                               Value* vp);

bool HasNativeElementPure(JSContext* cx, NativeObject* obj, int32_t index,
                          Value* vp) {
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(obj->getClass()->isNative());
  MOZ_ASSERT(!obj->getOpsHasProperty());
  MOZ_ASSERT(!obj->getOpsLookupProperty());
  MOZ_ASSERT(!obj->getOpsGetOwnPropertyDescriptor());

  if (MOZ_UNLIKELY(index < 0)) {
    return false;
  }

  if (obj->containsDenseElement(index)) {
    vp[0].setBoolean(true);
    return true;
  }

  jsid id = INT_TO_JSID(index);
  if (obj->lastProperty()->search(cx, id)) {
    vp[0].setBoolean(true);
    return true;
  }

  // Fail if there's a resolve hook, unless the mayResolve hook tells
  // us the resolve hook won't define a property with this id.
  if (MOZ_UNLIKELY(ClassMayResolveId(cx->names(), obj->getClass(), id, obj))) {
    return false;
  }
  // TypedArrayObject are also native and contain indexed properties.
  if (MOZ_UNLIKELY(obj->is<TypedArrayObject>())) {
    vp[0].setBoolean(uint32_t(index) < obj->as<TypedArrayObject>().length());
    return true;
  }

  vp[0].setBoolean(false);
  return true;
}

void HandleCodeCoverageAtPC(BaselineFrame* frame, jsbytecode* pc) {
  AutoUnsafeCallWithABI unsafe(UnsafeABIStrictness::AllowPendingExceptions);

  MOZ_ASSERT(frame->runningInInterpreter());

  JSScript* script = frame->script();
  MOZ_ASSERT(pc == script->main() || BytecodeIsJumpTarget(JSOp(*pc)));

  if (!script->hasScriptCounts()) {
    if (!script->realm()->collectCoverageForDebug()) {
      return;
    }
    JSContext* cx = script->runtimeFromMainThread()->mainContextFromOwnThread();
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!script->initScriptCounts(cx)) {
      oomUnsafe.crash("initScriptCounts");
    }
  }

  PCCounts* counts = script->maybeGetPCCounts(pc);
  MOZ_ASSERT(counts);
  counts->numExec()++;
}

void HandleCodeCoverageAtPrologue(BaselineFrame* frame) {
  AutoUnsafeCallWithABI unsafe;

  MOZ_ASSERT(frame->runningInInterpreter());

  JSScript* script = frame->script();
  jsbytecode* main = script->main();
  if (!BytecodeIsJumpTarget(JSOp(*main))) {
    HandleCodeCoverageAtPC(frame, main);
  }
}

JSString* TypeOfObject(JSObject* obj, JSRuntime* rt) {
  AutoUnsafeCallWithABI unsafe;
  JSType type = js::TypeOfObject(obj);
  return TypeName(type, *rt->commonNames);
}

bool GetPrototypeOf(JSContext* cx, HandleObject target,
                    MutableHandleValue rval) {
  MOZ_ASSERT(target->hasDynamicPrototype());

  RootedObject proto(cx);
  if (!GetPrototype(cx, target, &proto)) {
    return false;
  }
  rval.setObjectOrNull(proto);
  return true;
}

static JSString* ConvertObjectToStringForConcat(JSContext* cx,
                                                HandleValue obj) {
  MOZ_ASSERT(obj.isObject());
  RootedValue rootedObj(cx, obj);
  if (!ToPrimitive(cx, &rootedObj)) {
    return nullptr;
  }
  return ToString<CanGC>(cx, rootedObj);
}

bool DoConcatStringObject(JSContext* cx, HandleValue lhs, HandleValue rhs,
                          MutableHandleValue res) {
  JSString* lstr = nullptr;
  JSString* rstr = nullptr;

  if (lhs.isString()) {
    // Convert rhs first.
    MOZ_ASSERT(lhs.isString() && rhs.isObject());
    rstr = ConvertObjectToStringForConcat(cx, rhs);
    if (!rstr) {
      return false;
    }

    // lhs is already string.
    lstr = lhs.toString();
  } else {
    MOZ_ASSERT(rhs.isString() && lhs.isObject());
    // Convert lhs first.
    lstr = ConvertObjectToStringForConcat(cx, lhs);
    if (!lstr) {
      return false;
    }

    // rhs is already string.
    rstr = rhs.toString();
  }

  JSString* str = ConcatStrings<NoGC>(cx, lstr, rstr);
  if (!str) {
    RootedString nlstr(cx, lstr), nrstr(cx, rstr);
    str = ConcatStrings<CanGC>(cx, nlstr, nrstr);
    if (!str) {
      return false;
    }
  }

  // Note: we don't have to call JitScript::MonitorBytecodeType because we
  // monitored the string-type when attaching the IC stub.

  res.setString(str);
  return true;
}

MOZ_MUST_USE bool TrySkipAwait(JSContext* cx, HandleValue val,
                               MutableHandleValue resolved) {
  bool canSkip;
  if (!TrySkipAwait(cx, val, &canSkip, resolved)) {
    return false;
  }

  if (!canSkip) {
    resolved.setMagic(JS_CANNOT_SKIP_AWAIT);
  }

  return true;
}

bool IsPossiblyWrappedTypedArray(JSContext* cx, JSObject* obj, bool* result) {
  JSObject* unwrapped = CheckedUnwrapDynamic(obj, cx);
  if (!unwrapped) {
    ReportAccessDenied(cx);
    return false;
  }

  *result = unwrapped->is<TypedArrayObject>();
  return true;
}

bool DoToNumber(JSContext* cx, HandleValue arg, MutableHandleValue ret) {
  ret.set(arg);
  return ToNumber(cx, ret);
}

bool DoToNumeric(JSContext* cx, HandleValue arg, MutableHandleValue ret) {
  ret.set(arg);
  return ToNumeric(cx, ret);
}

}  // namespace jit
}  // namespace js
