/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CacheIRGenerator_h
#define jit_CacheIRGenerator_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stdint.h>

#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/Rooting.h"
#include "jit/CacheIR.h"
#include "jit/CacheIRWriter.h"
#include "jit/ICState.h"
#include "js/Id.h"
#include "js/RootingAPI.h"
#include "js/ScalarType.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/ValueArray.h"
#include "vm/Opcodes.h"

class JSFunction;

namespace JS {
struct XrayJitInfo;
}

namespace js {

class NativeObject;
class PropertyResult;
class ProxyObject;
enum class UnaryMathFunction : uint8_t;

namespace jit {

class BaselineFrame;
class Label;
class MacroAssembler;
struct Register;
enum class InlinableNative : uint16_t;

// Some ops refer to shapes that might be in other zones. Instead of putting
// cross-zone pointers in the caches themselves (which would complicate tracing
// enormously), these ops instead contain wrappers for objects in the target
// zone, which refer to the actual shape via a reserved slot.
void LoadShapeWrapperContents(MacroAssembler& masm, Register obj, Register dst,
                              Label* failure);

class MOZ_RAII IRGenerator {
 protected:
  CacheIRWriter writer;
  JSContext* cx_;
  HandleScript script_;
  jsbytecode* pc_;
  CacheKind cacheKind_;
  ICState::Mode mode_;
  bool isFirstStub_;

  IRGenerator(const IRGenerator&) = delete;
  IRGenerator& operator=(const IRGenerator&) = delete;

  bool maybeGuardInt32Index(const Value& index, ValOperandId indexId,
                            uint32_t* int32Index, Int32OperandId* int32IndexId);

  IntPtrOperandId guardToIntPtrIndex(const Value& index, ValOperandId indexId,
                                     bool supportOOB);

  ObjOperandId guardDOMProxyExpandoObjectAndShape(ProxyObject* obj,
                                                  ObjOperandId objId,
                                                  const Value& expandoVal,
                                                  NativeObject* expandoObj);

  void emitIdGuard(ValOperandId valId, const Value& idVal, jsid id);

  OperandId emitNumericGuard(ValOperandId valId, Scalar::Type type);

  StringOperandId emitToStringGuard(ValOperandId id, const Value& v);

  friend class CacheIRSpewer;

 public:
  explicit IRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                       CacheKind cacheKind, ICState state);

  const CacheIRWriter& writerRef() const { return writer; }
  CacheKind cacheKind() const { return cacheKind_; }

  static constexpr char* NotAttached = nullptr;
};

// GetPropIRGenerator generates CacheIR for a GetProp IC.
class MOZ_RAII GetPropIRGenerator : public IRGenerator {
  HandleValue val_;
  HandleValue idVal_;

  AttachDecision tryAttachNative(HandleObject obj, ObjOperandId objId,
                                 HandleId id, ValOperandId receiverId);
  AttachDecision tryAttachObjectLength(HandleObject obj, ObjOperandId objId,
                                       HandleId id);
  AttachDecision tryAttachTypedArray(HandleObject obj, ObjOperandId objId,
                                     HandleId id);
  AttachDecision tryAttachDataView(HandleObject obj, ObjOperandId objId,
                                   HandleId id);
  AttachDecision tryAttachArrayBufferMaybeShared(HandleObject obj,
                                                 ObjOperandId objId,
                                                 HandleId id);
  AttachDecision tryAttachRegExp(HandleObject obj, ObjOperandId objId,
                                 HandleId id);
  AttachDecision tryAttachModuleNamespace(HandleObject obj, ObjOperandId objId,
                                          HandleId id);
  AttachDecision tryAttachWindowProxy(HandleObject obj, ObjOperandId objId,
                                      HandleId id);
  AttachDecision tryAttachCrossCompartmentWrapper(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id);
  AttachDecision tryAttachXrayCrossCompartmentWrapper(HandleObject obj,
                                                      ObjOperandId objId,
                                                      HandleId id,
                                                      ValOperandId receiverId);
  AttachDecision tryAttachFunction(HandleObject obj, ObjOperandId objId,
                                   HandleId id);
  AttachDecision tryAttachArgumentsObjectIterator(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id);

  AttachDecision tryAttachGenericProxy(Handle<ProxyObject*> obj,
                                       ObjOperandId objId, HandleId id,
                                       bool handleDOMProxies);
  AttachDecision tryAttachDOMProxyExpando(Handle<ProxyObject*> obj,
                                          ObjOperandId objId, HandleId id,
                                          ValOperandId receiverId);
  AttachDecision tryAttachDOMProxyShadowed(Handle<ProxyObject*> obj,
                                           ObjOperandId objId, HandleId id);
  AttachDecision tryAttachDOMProxyUnshadowed(Handle<ProxyObject*> obj,
                                             ObjOperandId objId, HandleId id,
                                             ValOperandId receiverId);
  AttachDecision tryAttachProxy(HandleObject obj, ObjOperandId objId,
                                HandleId id, ValOperandId receiverId);

  AttachDecision tryAttachPrimitive(ValOperandId valId, HandleId id);
  AttachDecision tryAttachStringChar(ValOperandId valId, ValOperandId indexId);
  AttachDecision tryAttachStringLength(ValOperandId valId, HandleId id);

  AttachDecision tryAttachArgumentsObjectArg(HandleObject obj,
                                             ObjOperandId objId, uint32_t index,
                                             Int32OperandId indexId);
  AttachDecision tryAttachArgumentsObjectArgHole(HandleObject obj,
                                                 ObjOperandId objId,
                                                 uint32_t index,
                                                 Int32OperandId indexId);
  AttachDecision tryAttachArgumentsObjectCallee(HandleObject obj,
                                                ObjOperandId objId,
                                                HandleId id);

  AttachDecision tryAttachDenseElement(HandleObject obj, ObjOperandId objId,
                                       uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachDenseElementHole(HandleObject obj, ObjOperandId objId,
                                           uint32_t index,
                                           Int32OperandId indexId);
  AttachDecision tryAttachSparseElement(HandleObject obj, ObjOperandId objId,
                                        uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachTypedArrayElement(HandleObject obj,
                                            ObjOperandId objId);

  AttachDecision tryAttachGenericElement(HandleObject obj, ObjOperandId objId,
                                         uint32_t index,
                                         Int32OperandId indexId);

  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId);

  void attachMegamorphicNativeSlot(ObjOperandId objId, jsid id);

  ValOperandId getElemKeyValueId() const {
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem ||
               cacheKind_ == CacheKind::GetElemSuper);
    return ValOperandId(1);
  }

  ValOperandId getSuperReceiverValueId() const {
    if (cacheKind_ == CacheKind::GetPropSuper) {
      return ValOperandId(1);
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::GetElemSuper);
    return ValOperandId(2);
  }

  bool isSuper() const {
    return (cacheKind_ == CacheKind::GetPropSuper ||
            cacheKind_ == CacheKind::GetElemSuper);
  }

  // If this is a GetElem cache, emit instructions to guard the incoming Value
  // matches |id|.
  void maybeEmitIdGuard(jsid id);

  void trackAttached(const char* name);

 public:
  GetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState state, CacheKind cacheKind, HandleValue val,
                     HandleValue idVal);

  AttachDecision tryAttachStub();
};

// GetNameIRGenerator generates CacheIR for a GetName IC.
class MOZ_RAII GetNameIRGenerator : public IRGenerator {
  HandleObject env_;
  HandlePropertyName name_;

  AttachDecision tryAttachGlobalNameValue(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachGlobalNameGetter(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachEnvironmentName(ObjOperandId objId, HandleId id);

  void trackAttached(const char* name);

 public:
  GetNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState state, HandleObject env, HandlePropertyName name);

  AttachDecision tryAttachStub();
};

// BindNameIRGenerator generates CacheIR for a BindName IC.
class MOZ_RAII BindNameIRGenerator : public IRGenerator {
  HandleObject env_;
  HandlePropertyName name_;

  AttachDecision tryAttachGlobalName(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachEnvironmentName(ObjOperandId objId, HandleId id);

  void trackAttached(const char* name);

 public:
  BindNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                      ICState state, HandleObject env, HandlePropertyName name);

  AttachDecision tryAttachStub();
};

// SetPropIRGenerator generates CacheIR for a SetProp IC.
class MOZ_RAII SetPropIRGenerator : public IRGenerator {
  HandleValue lhsVal_;
  HandleValue idVal_;
  HandleValue rhsVal_;

 public:
  enum class DeferType { None, AddSlot };

 private:
  DeferType deferType_ = DeferType::None;

  ValOperandId setElemKeyValueId() const {
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    return ValOperandId(1);
  }

  ValOperandId rhsValueId() const {
    if (cacheKind_ == CacheKind::SetProp) {
      return ValOperandId(1);
    }
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    return ValOperandId(2);
  }

  // If this is a SetElem cache, emit instructions to guard the incoming Value
  // matches |id|.
  void maybeEmitIdGuard(jsid id);

  AttachDecision tryAttachNativeSetSlot(HandleObject obj, ObjOperandId objId,
                                        HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachSetter(HandleObject obj, ObjOperandId objId,
                                 HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachSetArrayLength(HandleObject obj, ObjOperandId objId,
                                         HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachWindowProxy(HandleObject obj, ObjOperandId objId,
                                      HandleId id, ValOperandId rhsId);

  AttachDecision tryAttachSetDenseElement(HandleObject obj, ObjOperandId objId,
                                          uint32_t index,
                                          Int32OperandId indexId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachSetTypedArrayElement(HandleObject obj,
                                               ObjOperandId objId,
                                               ValOperandId rhsId);

  AttachDecision tryAttachSetDenseElementHole(HandleObject obj,
                                              ObjOperandId objId,
                                              uint32_t index,
                                              Int32OperandId indexId,
                                              ValOperandId rhsId);

  AttachDecision tryAttachAddOrUpdateSparseElement(HandleObject obj,
                                                   ObjOperandId objId,
                                                   uint32_t index,
                                                   Int32OperandId indexId,
                                                   ValOperandId rhsId);

  AttachDecision tryAttachGenericProxy(Handle<ProxyObject*> obj,
                                       ObjOperandId objId, HandleId id,
                                       ValOperandId rhsId,
                                       bool handleDOMProxies);
  AttachDecision tryAttachDOMProxyShadowed(Handle<ProxyObject*> obj,
                                           ObjOperandId objId, HandleId id,
                                           ValOperandId rhsId);
  AttachDecision tryAttachDOMProxyUnshadowed(Handle<ProxyObject*> obj,
                                             ObjOperandId objId, HandleId id,
                                             ValOperandId rhsId);
  AttachDecision tryAttachDOMProxyExpando(Handle<ProxyObject*> obj,
                                          ObjOperandId objId, HandleId id,
                                          ValOperandId rhsId);
  AttachDecision tryAttachProxy(HandleObject obj, ObjOperandId objId,
                                HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId,
                                       ValOperandId rhsId);
  AttachDecision tryAttachMegamorphicSetElement(HandleObject obj,
                                                ObjOperandId objId,
                                                ValOperandId rhsId);

  bool canAttachAddSlotStub(HandleObject obj, HandleId id);

 public:
  SetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     CacheKind cacheKind, ICState state, HandleValue lhsVal,
                     HandleValue idVal, HandleValue rhsVal);

  AttachDecision tryAttachStub();
  AttachDecision tryAttachAddSlotStub(HandleShape oldShape);
  void trackAttached(const char* name);

  DeferType deferType() const { return deferType_; }
};

// HasPropIRGenerator generates CacheIR for a HasProp IC. Used for
// CacheKind::In / CacheKind::HasOwn.
class MOZ_RAII HasPropIRGenerator : public IRGenerator {
  HandleValue val_;
  HandleValue idVal_;

  AttachDecision tryAttachDense(HandleObject obj, ObjOperandId objId,
                                uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachDenseHole(HandleObject obj, ObjOperandId objId,
                                    uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachTypedArray(HandleObject obj, ObjOperandId objId,
                                     ValOperandId keyId);
  AttachDecision tryAttachSparse(HandleObject obj, ObjOperandId objId,
                                 Int32OperandId indexId);
  AttachDecision tryAttachArgumentsObjectArg(HandleObject obj,
                                             ObjOperandId objId,
                                             Int32OperandId indexId);
  AttachDecision tryAttachNamedProp(HandleObject obj, ObjOperandId objId,
                                    HandleId key, ValOperandId keyId);
  AttachDecision tryAttachMegamorphic(ObjOperandId objId, ValOperandId keyId);
  AttachDecision tryAttachNative(NativeObject* obj, ObjOperandId objId,
                                 jsid key, ValOperandId keyId,
                                 PropertyResult prop, NativeObject* holder);
  AttachDecision tryAttachSlotDoesNotExist(NativeObject* obj,
                                           ObjOperandId objId, jsid key,
                                           ValOperandId keyId);
  AttachDecision tryAttachDoesNotExist(HandleObject obj, ObjOperandId objId,
                                       HandleId key, ValOperandId keyId);
  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId,
                                       ValOperandId keyId);

  void trackAttached(const char* name);

 public:
  // NOTE: Argument order is PROPERTY, OBJECT
  HasPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState state, CacheKind cacheKind, HandleValue idVal,
                     HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII CheckPrivateFieldIRGenerator : public IRGenerator {
  HandleValue val_;
  HandleValue idVal_;

  AttachDecision tryAttachNative(JSObject* obj, ObjOperandId objId, jsid key,
                                 ValOperandId keyId, bool hasOwn);

  void trackAttached(const char* name);

 public:
  CheckPrivateFieldIRGenerator(JSContext* cx, HandleScript script,
                               jsbytecode* pc, ICState state,
                               CacheKind cacheKind, HandleValue idVal,
                               HandleValue val);
  AttachDecision tryAttachStub();
};

class MOZ_RAII InstanceOfIRGenerator : public IRGenerator {
  HandleValue lhsVal_;
  HandleObject rhsObj_;

  void trackAttached(const char* name);

 public:
  InstanceOfIRGenerator(JSContext*, HandleScript, jsbytecode*, ICState,
                        HandleValue, HandleObject);

  AttachDecision tryAttachStub();
};

class MOZ_RAII TypeOfIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachPrimitive(ValOperandId valId);
  AttachDecision tryAttachObject(ValOperandId valId);
  void trackAttached(const char* name);

 public:
  TypeOfIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc, ICState state,
                    HandleValue value);

  AttachDecision tryAttachStub();
};

class MOZ_RAII GetIteratorIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachNativeIterator(ValOperandId valId);
  AttachDecision tryAttachNullOrUndefined(ValOperandId valId);
  AttachDecision tryAttachMegamorphic(ValOperandId valId);

 public:
  GetIteratorIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                         ICState state, HandleValue value);

  AttachDecision tryAttachStub();

  void trackAttached(const char* name);
};

class MOZ_RAII OptimizeSpreadCallIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachArray();
  AttachDecision tryAttachArguments();
  AttachDecision tryAttachNotOptimizable();

 public:
  OptimizeSpreadCallIRGenerator(JSContext* cx, HandleScript script,
                                jsbytecode* pc, ICState state,
                                HandleValue value);

  AttachDecision tryAttachStub();

  void trackAttached(const char* name);
};

enum class StringChar { CodeAt, At };
enum class ScriptedThisResult { NoAction, UninitializedThis, PlainObjectShape };

class MOZ_RAII CallIRGenerator : public IRGenerator {
 private:
  JSOp op_;
  uint32_t argc_;
  HandleValue callee_;
  HandleValue thisval_;
  HandleValue newTarget_;
  HandleValueArray args_;

  ScriptedThisResult getThisShapeForScripted(HandleFunction calleeFunc,
                                             MutableHandleShape result);

  void emitNativeCalleeGuard(JSFunction* callee);
  void emitCalleeGuard(ObjOperandId calleeId, JSFunction* callee);

  bool canAttachAtomicsReadWriteModify();

  struct AtomicsReadWriteModifyOperands {
    ObjOperandId objId;
    IntPtrOperandId intPtrIndexId;
    OperandId numericValueId;
  };

  AtomicsReadWriteModifyOperands emitAtomicsReadWriteModifyOperands(
      HandleFunction callee);

  AttachDecision tryAttachArrayPush(HandleFunction callee);
  AttachDecision tryAttachArrayPopShift(HandleFunction callee,
                                        InlinableNative native);
  AttachDecision tryAttachArrayJoin(HandleFunction callee);
  AttachDecision tryAttachArraySlice(HandleFunction callee);
  AttachDecision tryAttachArrayIsArray(HandleFunction callee);
  AttachDecision tryAttachDataViewGet(HandleFunction callee, Scalar::Type type);
  AttachDecision tryAttachDataViewSet(HandleFunction callee, Scalar::Type type);
  AttachDecision tryAttachUnsafeGetReservedSlot(HandleFunction callee,
                                                InlinableNative native);
  AttachDecision tryAttachUnsafeSetReservedSlot(HandleFunction callee);
  AttachDecision tryAttachIsSuspendedGenerator(HandleFunction callee);
  AttachDecision tryAttachToObject(HandleFunction callee,
                                   InlinableNative native);
  AttachDecision tryAttachToInteger(HandleFunction callee);
  AttachDecision tryAttachToLength(HandleFunction callee);
  AttachDecision tryAttachIsObject(HandleFunction callee);
  AttachDecision tryAttachIsPackedArray(HandleFunction callee);
  AttachDecision tryAttachIsCallable(HandleFunction callee);
  AttachDecision tryAttachIsConstructor(HandleFunction callee);
  AttachDecision tryAttachIsCrossRealmArrayConstructor(HandleFunction callee);
  AttachDecision tryAttachGuardToClass(HandleFunction callee,
                                       InlinableNative native);
  AttachDecision tryAttachHasClass(HandleFunction callee, const JSClass* clasp,
                                   bool isPossiblyWrapped);
  AttachDecision tryAttachRegExpMatcherSearcherTester(HandleFunction callee,
                                                      InlinableNative native);
  AttachDecision tryAttachRegExpPrototypeOptimizable(HandleFunction callee);
  AttachDecision tryAttachRegExpInstanceOptimizable(HandleFunction callee);
  AttachDecision tryAttachGetFirstDollarIndex(HandleFunction callee);
  AttachDecision tryAttachSubstringKernel(HandleFunction callee);
  AttachDecision tryAttachObjectHasPrototype(HandleFunction callee);
  AttachDecision tryAttachString(HandleFunction callee);
  AttachDecision tryAttachStringConstructor(HandleFunction callee);
  AttachDecision tryAttachStringToStringValueOf(HandleFunction callee);
  AttachDecision tryAttachStringChar(HandleFunction callee, StringChar kind);
  AttachDecision tryAttachStringCharCodeAt(HandleFunction callee);
  AttachDecision tryAttachStringCharAt(HandleFunction callee);
  AttachDecision tryAttachStringFromCharCode(HandleFunction callee);
  AttachDecision tryAttachStringFromCodePoint(HandleFunction callee);
  AttachDecision tryAttachStringToLowerCase(HandleFunction callee);
  AttachDecision tryAttachStringToUpperCase(HandleFunction callee);
  AttachDecision tryAttachStringReplaceString(HandleFunction callee);
  AttachDecision tryAttachStringSplitString(HandleFunction callee);
  AttachDecision tryAttachMathRandom(HandleFunction callee);
  AttachDecision tryAttachMathAbs(HandleFunction callee);
  AttachDecision tryAttachMathClz32(HandleFunction callee);
  AttachDecision tryAttachMathSign(HandleFunction callee);
  AttachDecision tryAttachMathImul(HandleFunction callee);
  AttachDecision tryAttachMathFloor(HandleFunction callee);
  AttachDecision tryAttachMathCeil(HandleFunction callee);
  AttachDecision tryAttachMathTrunc(HandleFunction callee);
  AttachDecision tryAttachMathRound(HandleFunction callee);
  AttachDecision tryAttachMathSqrt(HandleFunction callee);
  AttachDecision tryAttachMathFRound(HandleFunction callee);
  AttachDecision tryAttachMathHypot(HandleFunction callee);
  AttachDecision tryAttachMathATan2(HandleFunction callee);
  AttachDecision tryAttachMathFunction(HandleFunction callee,
                                       UnaryMathFunction fun);
  AttachDecision tryAttachMathPow(HandleFunction callee);
  AttachDecision tryAttachMathMinMax(HandleFunction callee, bool isMax);
  AttachDecision tryAttachSpreadMathMinMax(HandleFunction callee, bool isMax);
  AttachDecision tryAttachIsTypedArray(HandleFunction callee,
                                       bool isPossiblyWrapped);
  AttachDecision tryAttachIsTypedArrayConstructor(HandleFunction callee);
  AttachDecision tryAttachTypedArrayByteOffset(HandleFunction callee);
  AttachDecision tryAttachTypedArrayElementSize(HandleFunction callee);
  AttachDecision tryAttachTypedArrayLength(HandleFunction callee,
                                           bool isPossiblyWrapped);
  AttachDecision tryAttachArrayBufferByteLength(HandleFunction callee,
                                                bool isPossiblyWrapped);
  AttachDecision tryAttachIsConstructing(HandleFunction callee);
  AttachDecision tryAttachGetNextMapSetEntryForIterator(HandleFunction callee,
                                                        bool isMap);
  AttachDecision tryAttachFinishBoundFunctionInit(HandleFunction callee);
  AttachDecision tryAttachNewArrayIterator(HandleFunction callee);
  AttachDecision tryAttachNewStringIterator(HandleFunction callee);
  AttachDecision tryAttachNewRegExpStringIterator(HandleFunction callee);
  AttachDecision tryAttachArrayIteratorPrototypeOptimizable(
      HandleFunction callee);
  AttachDecision tryAttachObjectCreate(HandleFunction callee);
  AttachDecision tryAttachArrayConstructor(HandleFunction callee);
  AttachDecision tryAttachTypedArrayConstructor(HandleFunction callee);
  AttachDecision tryAttachNumberToString(HandleFunction callee);
  AttachDecision tryAttachReflectGetPrototypeOf(HandleFunction callee);
  AttachDecision tryAttachAtomicsCompareExchange(HandleFunction callee);
  AttachDecision tryAttachAtomicsExchange(HandleFunction callee);
  AttachDecision tryAttachAtomicsAdd(HandleFunction callee);
  AttachDecision tryAttachAtomicsSub(HandleFunction callee);
  AttachDecision tryAttachAtomicsAnd(HandleFunction callee);
  AttachDecision tryAttachAtomicsOr(HandleFunction callee);
  AttachDecision tryAttachAtomicsXor(HandleFunction callee);
  AttachDecision tryAttachAtomicsLoad(HandleFunction callee);
  AttachDecision tryAttachAtomicsStore(HandleFunction callee);
  AttachDecision tryAttachAtomicsIsLockFree(HandleFunction callee);
  AttachDecision tryAttachBoolean(HandleFunction callee);
  AttachDecision tryAttachBailout(HandleFunction callee);
  AttachDecision tryAttachAssertFloat32(HandleFunction callee);
  AttachDecision tryAttachAssertRecoveredOnBailout(HandleFunction callee);
  AttachDecision tryAttachObjectIs(HandleFunction callee);
  AttachDecision tryAttachObjectIsPrototypeOf(HandleFunction callee);
  AttachDecision tryAttachObjectToString(HandleFunction callee);
  AttachDecision tryAttachBigIntAsIntN(HandleFunction callee);
  AttachDecision tryAttachBigIntAsUintN(HandleFunction callee);
  AttachDecision tryAttachSetHas(HandleFunction callee);
  AttachDecision tryAttachMapHas(HandleFunction callee);
  AttachDecision tryAttachMapGet(HandleFunction callee);

  AttachDecision tryAttachFunCall(HandleFunction calleeFunc);
  AttachDecision tryAttachFunApply(HandleFunction calleeFunc);
  AttachDecision tryAttachCallScripted(HandleFunction calleeFunc);
  AttachDecision tryAttachInlinableNative(HandleFunction calleeFunc);
  AttachDecision tryAttachWasmCall(HandleFunction calleeFunc);
  AttachDecision tryAttachCallNative(HandleFunction calleeFunc);
  AttachDecision tryAttachCallHook(HandleObject calleeObj);

  void trackAttached(const char* name);

 public:
  CallIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc, JSOp op,
                  ICState state, uint32_t argc, HandleValue callee,
                  HandleValue thisval, HandleValue newTarget,
                  HandleValueArray args);

  AttachDecision tryAttachStub();
};

class MOZ_RAII CompareIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue lhsVal_;
  HandleValue rhsVal_;

  AttachDecision tryAttachString(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachObject(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachSymbol(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachStrictDifferentTypes(ValOperandId lhsId,
                                               ValOperandId rhsId);
  AttachDecision tryAttachInt32(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigInt(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachNumberUndefined(ValOperandId lhsId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachAnyNullUndefined(ValOperandId lhsId,
                                           ValOperandId rhsId);
  AttachDecision tryAttachNullUndefined(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachStringNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachPrimitiveSymbol(ValOperandId lhsId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachBoolStringOrNumber(ValOperandId lhsId,
                                             ValOperandId rhsId);
  AttachDecision tryAttachBigIntInt32(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigIntNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigIntString(ValOperandId lhsId, ValOperandId rhsId);

  void trackAttached(const char* name);

 public:
  CompareIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc, ICState state,
                     JSOp op, HandleValue lhsVal, HandleValue rhsVal);

  AttachDecision tryAttachStub();
};

class MOZ_RAII ToBoolIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachBool();
  AttachDecision tryAttachInt32();
  AttachDecision tryAttachNumber();
  AttachDecision tryAttachString();
  AttachDecision tryAttachSymbol();
  AttachDecision tryAttachNullOrUndefined();
  AttachDecision tryAttachObject();
  AttachDecision tryAttachBigInt();

  void trackAttached(const char* name);

 public:
  ToBoolIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc, ICState state,
                    HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII GetIntrinsicIRGenerator : public IRGenerator {
  HandleValue val_;

  void trackAttached(const char* name);

 public:
  GetIntrinsicIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                          ICState state, HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII UnaryArithIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue val_;
  HandleValue res_;

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachNumber();
  AttachDecision tryAttachBitwise();
  AttachDecision tryAttachBigInt();
  AttachDecision tryAttachStringInt32();
  AttachDecision tryAttachStringNumber();

  void trackAttached(const char* name);

 public:
  UnaryArithIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                        ICState state, JSOp op, HandleValue val,
                        HandleValue res);

  AttachDecision tryAttachStub();
};

class MOZ_RAII ToPropertyKeyIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachNumber();
  AttachDecision tryAttachString();
  AttachDecision tryAttachSymbol();

  void trackAttached(const char* name);

 public:
  ToPropertyKeyIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                           ICState state, HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII BinaryArithIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue lhs_;
  HandleValue rhs_;
  HandleValue res_;

  void trackAttached(const char* name);

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachDouble();
  AttachDecision tryAttachBitwise();
  AttachDecision tryAttachStringConcat();
  AttachDecision tryAttachStringObjectConcat();
  AttachDecision tryAttachStringNumberConcat();
  AttachDecision tryAttachStringBooleanConcat();
  AttachDecision tryAttachBigInt();
  AttachDecision tryAttachStringInt32Arith();

 public:
  BinaryArithIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                         ICState state, JSOp op, HandleValue lhs,
                         HandleValue rhs, HandleValue res);

  AttachDecision tryAttachStub();
};

class MOZ_RAII NewArrayIRGenerator : public IRGenerator {
#ifdef JS_CACHEIR_SPEW
  JSOp op_;
#endif
  HandleObject templateObject_;
  BaselineFrame* frame_;

  void trackAttached(const char* name);

 public:
  NewArrayIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                      ICState state, JSOp op, HandleObject templateObj,
                      BaselineFrame* frame);

  AttachDecision tryAttachStub();
  AttachDecision tryAttachArrayObject();
};

class MOZ_RAII NewObjectIRGenerator : public IRGenerator {
#ifdef JS_CACHEIR_SPEW
  JSOp op_;
#endif
  HandleObject templateObject_;
  BaselineFrame* frame_;

  void trackAttached(const char* name);

 public:
  NewObjectIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                       ICState state, JSOp op, HandleObject templateObj,
                       BaselineFrame* frame);

  AttachDecision tryAttachStub();
  AttachDecision tryAttachPlainObject();
};

inline bool BytecodeOpCanHaveAllocSite(JSOp op) {
  return op == JSOp::NewArray || op == JSOp::NewObject || op == JSOp::NewInit;
}

// Retrieve Xray JIT info set by the embedder.
extern JS::XrayJitInfo* GetXrayJitInfo();

}  // namespace jit
}  // namespace js

#endif /* jit_CacheIRGenerator_h */
