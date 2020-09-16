/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIR.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Unused.h"

#include "builtin/DataViewObject.h"
#include "builtin/MapObject.h"
#include "builtin/ModuleObject.h"
#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/CacheIRSpewer.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"  // IsIonEnabled
#include "jit/JitContext.h"
#include "js/experimental/JitInfo.h"  // JSJitInfo
#include "js/friend/DOMProxy.h"       // JS::ExpandoAndGeneration
#include "js/friend/WindowProxy.h"  // js::IsWindow, js::IsWindowProxy, js::ToWindowIfWindowProxy
#include "js/friend/XrayJitInfo.h"  // js::jit::GetXrayJitInfo, JS::XrayJitInfo
#include "js/ScalarType.h"          // js::Scalar::Type
#include "js/Wrapper.h"
#include "util/Unicode.h"
#include "vm/ArrayBufferObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/SelfHosting.h"
#include "vm/ThrowMsgKind.h"  // ThrowCondition

#include "jit/MacroAssembler-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/StringObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::Maybe;

using JS::DOMProxyShadowsResult;
using JS::ExpandoAndGeneration;

const char* const js::jit::CacheKindNames[] = {
#define DEFINE_KIND(kind) #kind,
    CACHE_IR_KINDS(DEFINE_KIND)
#undef DEFINE_KIND
};

const char* const js::jit::CacheIROpNames[] = {
#define OPNAME(op, ...) #op,
    CACHE_IR_OPS(OPNAME)
#undef OPNAME
};

const uint32_t js::jit::CacheIROpArgLengths[] = {
#define ARGLENGTH(op, len, ...) len,
    CACHE_IR_OPS(ARGLENGTH)
#undef ARGLENGTH
};

const uint32_t js::jit::CacheIROpHealth[] = {
#define OPHEALTH(op, len, health) health,
    CACHE_IR_OPS(OPHEALTH)
#undef OPHEALTH
};

#ifdef DEBUG
size_t js::jit::NumInputsForCacheKind(CacheKind kind) {
  switch (kind) {
    case CacheKind::NewObject:
    case CacheKind::GetIntrinsic:
      return 0;
    case CacheKind::GetProp:
    case CacheKind::TypeOf:
    case CacheKind::ToPropertyKey:
    case CacheKind::GetIterator:
    case CacheKind::ToBool:
    case CacheKind::UnaryArith:
    case CacheKind::GetName:
    case CacheKind::BindName:
    case CacheKind::Call:
    case CacheKind::OptimizeSpreadCall:
      return 1;
    case CacheKind::Compare:
    case CacheKind::GetElem:
    case CacheKind::GetPropSuper:
    case CacheKind::SetProp:
    case CacheKind::In:
    case CacheKind::HasOwn:
    case CacheKind::CheckPrivateField:
    case CacheKind::InstanceOf:
    case CacheKind::BinaryArith:
      return 2;
    case CacheKind::GetElemSuper:
    case CacheKind::SetElem:
      return 3;
  }
  MOZ_CRASH("Invalid kind");
}
#endif

void CacheIRWriter::assertSameCompartment(JSObject* obj) {
  cx_->debugOnlyCheck(obj);
}

StubField CacheIRWriter::readStubFieldForIon(uint32_t offset,
                                             StubField::Type type) const {
  size_t index = 0;
  size_t currentOffset = 0;

  // If we've seen an offset earlier than this before, we know we can start the
  // search there at least, otherwise, we start the search from the beginning.
  if (lastOffset_ < offset) {
    currentOffset = lastOffset_;
    index = lastIndex_;
  }

  while (currentOffset != offset) {
    currentOffset += StubField::sizeInBytes(stubFields_[index].type());
    index++;
    MOZ_ASSERT(index < stubFields_.length());
  }

  MOZ_ASSERT(stubFields_[index].type() == type);

  lastOffset_ = currentOffset;
  lastIndex_ = index;

  return stubFields_[index];
}

CacheIRCloner::CacheIRCloner(ICStub* stub)
    : stubInfo_(stub->cacheIRStubInfo()), stubData_(stub->cacheIRStubData()) {}

void CacheIRCloner::cloneOp(CacheOp op, CacheIRReader& reader,
                            CacheIRWriter& writer) {
  switch (op) {
#define DEFINE_OP(op, ...)     \
  case CacheOp::op:            \
    clone##op(reader, writer); \
    break;
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP
    default:
      MOZ_CRASH("Invalid op");
  }
}

uintptr_t CacheIRCloner::readStubWord(uint32_t offset) {
  return stubInfo_->getStubRawWord(stubData_, offset);
}
int64_t CacheIRCloner::readStubInt64(uint32_t offset) {
  return stubInfo_->getStubRawInt64(stubData_, offset);
}

Shape* CacheIRCloner::getShapeField(uint32_t stubOffset) {
  return reinterpret_cast<Shape*>(readStubWord(stubOffset));
}
ObjectGroup* CacheIRCloner::getGroupField(uint32_t stubOffset) {
  return reinterpret_cast<ObjectGroup*>(readStubWord(stubOffset));
}
JSObject* CacheIRCloner::getObjectField(uint32_t stubOffset) {
  return reinterpret_cast<JSObject*>(readStubWord(stubOffset));
}
JSString* CacheIRCloner::getStringField(uint32_t stubOffset) {
  return reinterpret_cast<JSString*>(readStubWord(stubOffset));
}
JSAtom* CacheIRCloner::getAtomField(uint32_t stubOffset) {
  return reinterpret_cast<JSAtom*>(readStubWord(stubOffset));
}
PropertyName* CacheIRCloner::getPropertyNameField(uint32_t stubOffset) {
  return reinterpret_cast<PropertyName*>(readStubWord(stubOffset));
}
JS::Symbol* CacheIRCloner::getSymbolField(uint32_t stubOffset) {
  return reinterpret_cast<JS::Symbol*>(readStubWord(stubOffset));
}
BaseScript* CacheIRCloner::getBaseScriptField(uint32_t stubOffset) {
  return reinterpret_cast<BaseScript*>(readStubWord(stubOffset));
}
uintptr_t CacheIRCloner::getRawWordField(uint32_t stubOffset) {
  return reinterpret_cast<uintptr_t>(readStubWord(stubOffset));
}
const void* CacheIRCloner::getRawPointerField(uint32_t stubOffset) {
  return reinterpret_cast<const void*>(readStubWord(stubOffset));
}
uint64_t CacheIRCloner::getDOMExpandoGenerationField(uint32_t stubOffset) {
  return static_cast<uint64_t>(readStubInt64(stubOffset));
}

jsid CacheIRCloner::getIdField(uint32_t stubOffset) {
  return jsid::fromRawBits(readStubWord(stubOffset));
}
const Value CacheIRCloner::getValueField(uint32_t stubOffset) {
  return Value::fromRawBits(uint64_t(readStubInt64(stubOffset)));
}

IRGenerator::IRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                         CacheKind cacheKind, ICState::Mode mode)
    : writer(cx),
      cx_(cx),
      script_(script),
      pc_(pc),
      cacheKind_(cacheKind),
      mode_(mode) {}

GetPropIRGenerator::GetPropIRGenerator(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, ICState::Mode mode,
                                       CacheKind cacheKind, HandleValue val,
                                       HandleValue idVal, HandleValue receiver,
                                       GetPropertyResultFlags resultFlags)
    : IRGenerator(cx, script, pc, cacheKind, mode),
      val_(val),
      idVal_(idVal),
      receiver_(receiver),
      resultFlags_(resultFlags),
      preliminaryObjectAction_(PreliminaryObjectAction::None) {}

static void EmitLoadSlotResult(CacheIRWriter& writer, ObjOperandId holderOp,
                               NativeObject* holder, Shape* shape) {
  if (holder->isFixedSlot(shape->slot())) {
    writer.loadFixedSlotResult(holderOp,
                               NativeObject::getFixedSlotOffset(shape->slot()));
  } else {
    size_t dynamicSlotOffset =
        holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
    writer.loadDynamicSlotResult(holderOp, dynamicSlotOffset);
  }
}

// DOM proxies
// -----------
//
// DOM proxies are proxies that are used to implement various DOM objects like
// HTMLDocument and NodeList. DOM proxies may have an expando object - a native
// object that stores extra properties added to the object. The following
// CacheIR instructions are only used with DOM proxies:
//
// * LoadDOMExpandoValue: returns the Value in the proxy's expando slot. This
//   returns either an UndefinedValue (no expando), ObjectValue (the expando
//   object), or PrivateValue(ExpandoAndGeneration*).
//
// * LoadDOMExpandoValueGuardGeneration: guards the Value in the proxy's expando
//   slot is the same PrivateValue(ExpandoAndGeneration*), then guards on its
//   generation, then returns expandoAndGeneration->expando. This Value is
//   either an UndefinedValue or ObjectValue.
//
// * LoadDOMExpandoValueIgnoreGeneration: assumes the Value in the proxy's
//   expando slot is a PrivateValue(ExpandoAndGeneration*), unboxes it, and
//   returns the expandoAndGeneration->expando Value.
//
// * GuardDOMExpandoMissingOrGuardShape: takes an expando Value as input, then
//   guards it's either UndefinedValue or an object with the expected shape.

enum class ProxyStubType {
  None,
  DOMExpando,
  DOMShadowed,
  DOMUnshadowed,
  Generic
};

static ProxyStubType GetProxyStubType(JSContext* cx, HandleObject obj,
                                      HandleId id) {
  if (!obj->is<ProxyObject>()) {
    return ProxyStubType::None;
  }

  if (!IsCacheableDOMProxy(obj)) {
    return ProxyStubType::Generic;
  }

  DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
  if (shadows == DOMProxyShadowsResult::ShadowCheckFailed) {
    cx->clearPendingException();
    return ProxyStubType::None;
  }

  if (DOMProxyIsShadowing(shadows)) {
    if (shadows == DOMProxyShadowsResult::ShadowsViaDirectExpando ||
        shadows == DOMProxyShadowsResult::ShadowsViaIndirectExpando) {
      return ProxyStubType::DOMExpando;
    }
    return ProxyStubType::DOMShadowed;
  }

  MOZ_ASSERT(shadows == DOMProxyShadowsResult::DoesntShadow ||
             shadows == DOMProxyShadowsResult::DoesntShadowUnique);
  return ProxyStubType::DOMUnshadowed;
}

static bool ValueToNameOrSymbolId(JSContext* cx, HandleValue idval,
                                  MutableHandleId id, bool* nameOrSymbol) {
  *nameOrSymbol = false;

  if (!idval.isString() && !idval.isSymbol()) {
    return true;
  }

  if (!PrimitiveValueToId<CanGC>(cx, idval, id)) {
    return false;
  }

  if (!JSID_IS_STRING(id) && !JSID_IS_SYMBOL(id)) {
    id.set(JSID_VOID);
    return true;
  }

  uint32_t dummy;
  if (JSID_IS_STRING(id) && JSID_TO_ATOM(id)->isIndex(&dummy)) {
    id.set(JSID_VOID);
    return true;
  }

  *nameOrSymbol = true;
  return true;
}

AttachDecision GetPropIRGenerator::tryAttachStub() {
  // Idempotent ICs should call tryAttachIdempotentStub instead.
  MOZ_ASSERT(!idempotent());

  AutoAssertNoPendingException aanpe(cx_);

  ValOperandId valId(writer.setInputOperandId(0));
  if (cacheKind_ != CacheKind::GetProp) {
    MOZ_ASSERT_IF(cacheKind_ == CacheKind::GetPropSuper,
                  getSuperReceiverValueId().id() == 1);
    MOZ_ASSERT_IF(cacheKind_ != CacheKind::GetPropSuper,
                  getElemKeyValueId().id() == 1);
    writer.setInputOperandId(1);
  }
  if (cacheKind_ == CacheKind::GetElemSuper) {
    MOZ_ASSERT(getSuperReceiverValueId().id() == 2);
    writer.setInputOperandId(2);
  }

  RootedId id(cx_);
  bool nameOrSymbol;
  if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  // |super.prop| getter calls use a |this| value that differs from lookup
  // object.
  ValOperandId receiverId = isSuper() ? getSuperReceiverValueId() : valId;

  if (val_.isObject()) {
    RootedObject obj(cx_, &val_.toObject());
    ObjOperandId objId = writer.guardToObject(valId);
    if (nameOrSymbol) {
      TRY_ATTACH(tryAttachObjectLength(obj, objId, id));
      TRY_ATTACH(tryAttachTypedArrayLength(obj, objId, id));
      TRY_ATTACH(tryAttachNative(obj, objId, id, receiverId));
      TRY_ATTACH(tryAttachTypedObject(obj, objId, id));
      TRY_ATTACH(tryAttachModuleNamespace(obj, objId, id));
      TRY_ATTACH(tryAttachWindowProxy(obj, objId, id));
      TRY_ATTACH(tryAttachCrossCompartmentWrapper(obj, objId, id));
      TRY_ATTACH(
          tryAttachXrayCrossCompartmentWrapper(obj, objId, id, receiverId));
      TRY_ATTACH(tryAttachFunction(obj, objId, id));
      TRY_ATTACH(tryAttachProxy(obj, objId, id, receiverId));

      trackAttached(IRGenerator::NotAttached);
      return AttachDecision::NoAction;
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem ||
               cacheKind_ == CacheKind::GetElemSuper);

    TRY_ATTACH(tryAttachProxyElement(obj, objId));

    uint32_t index;
    Int32OperandId indexId;
    if (maybeGuardInt32Index(idVal_, getElemKeyValueId(), &index, &indexId)) {
      TRY_ATTACH(tryAttachTypedElement(obj, objId, index, indexId));
      TRY_ATTACH(tryAttachDenseElement(obj, objId, index, indexId));
      TRY_ATTACH(tryAttachDenseElementHole(obj, objId, index, indexId));
      TRY_ATTACH(tryAttachSparseElement(obj, objId, index, indexId));
      TRY_ATTACH(tryAttachArgumentsObjectArg(obj, objId, indexId));
      TRY_ATTACH(tryAttachGenericElement(obj, objId, index, indexId));

      trackAttached(IRGenerator::NotAttached);
      return AttachDecision::NoAction;
    }

    TRY_ATTACH(tryAttachTypedArrayNonInt32Index(obj, objId));

    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  if (nameOrSymbol) {
    TRY_ATTACH(tryAttachPrimitive(valId, id));
    TRY_ATTACH(tryAttachStringLength(valId, id));
    TRY_ATTACH(tryAttachMagicArgumentsName(valId, id));

    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  if (idVal_.isInt32()) {
    ValOperandId indexId = getElemKeyValueId();
    TRY_ATTACH(tryAttachStringChar(valId, indexId));
    TRY_ATTACH(tryAttachMagicArgument(valId, indexId));

    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision GetPropIRGenerator::tryAttachIdempotentStub() {
  // For idempotent ICs, only attach stubs which we can be sure have no side
  // effects and produce a result which the MIR in the calling code is able
  // to handle, since we do not have a pc to explicitly monitor the result.

  MOZ_ASSERT(idempotent());

  RootedObject obj(cx_, &val_.toObject());
  RootedId id(cx_, NameToId(idVal_.toString()->asAtom().asPropertyName()));

  ValOperandId valId(writer.setInputOperandId(0));
  ObjOperandId objId = writer.guardToObject(valId);

  ValOperandId receiverId = valId;
  TRY_ATTACH(tryAttachNative(obj, objId, id, receiverId));

  // Object lengths are supported only if int32 results are allowed.
  TRY_ATTACH(tryAttachObjectLength(obj, objId, id));

  // Also support native data properties on DOMProxy prototypes.
  if (GetProxyStubType(cx_, obj, id) == ProxyStubType::DOMUnshadowed) {
    return tryAttachDOMProxyUnshadowed(obj, objId, id, receiverId);
  }

  return AttachDecision::NoAction;
}

static bool IsCacheableProtoChain(JSObject* obj, JSObject* holder) {
  while (obj != holder) {
    /*
     * We cannot assume that we find the holder object on the prototype
     * chain and must check for null proto. The prototype chain can be
     * altered during the lookupProperty call.
     */
    JSObject* proto = obj->staticPrototype();
    if (!proto || !proto->isNative()) {
      return false;
    }
    obj = proto;
  }
  return true;
}

static bool IsCacheableGetPropReadSlot(JSObject* obj, JSObject* holder,
                                       PropertyResult prop) {
  if (!prop || !IsCacheableProtoChain(obj, holder)) {
    return false;
  }

  Shape* shape = prop.shape();
  if (!shape->isDataProperty()) {
    return false;
  }

  return true;
}

enum NativeGetPropCacheability {
  CanAttachNone,
  CanAttachReadSlot,
  CanAttachNativeGetter,
  CanAttachScriptedGetter,
};

static NativeGetPropCacheability IsCacheableGetPropCall(JSObject* obj,
                                                        JSObject* holder,
                                                        Shape* shape) {
  if (!shape || !IsCacheableProtoChain(obj, holder)) {
    return CanAttachNone;
  }

  if (!shape->hasGetterValue() || !shape->getterValue().isObject()) {
    return CanAttachNone;
  }

  if (!shape->getterValue().toObject().is<JSFunction>()) {
    return CanAttachNone;
  }

  JSFunction& getter = shape->getterValue().toObject().as<JSFunction>();

  if (getter.isClassConstructor()) {
    return CanAttachNone;
  }

  // For getters that need the WindowProxy (instead of the Window) as this
  // object, don't cache if obj is the Window, since our cache will pass that
  // instead of the WindowProxy.
  if (IsWindow(obj)) {
    // Check for a getter that has jitinfo and whose jitinfo says it's
    // OK with both inner and outer objects.
    if (!getter.hasJitInfo() || getter.jitInfo()->needsOuterizedThisObject()) {
      return CanAttachNone;
    }
  }

  // Scripted functions and natives with JIT entry can use the scripted path.
  if (getter.hasJitEntry()) {
    return CanAttachScriptedGetter;
  }

  MOZ_ASSERT(getter.isNativeWithoutJitEntry());
  return CanAttachNativeGetter;
}

static bool CheckHasNoSuchOwnProperty(JSContext* cx, JSObject* obj, jsid id) {
  if (obj->isNative()) {
    // Don't handle proto chains with resolve hooks.
    if (ClassMayResolveId(cx->names(), obj->getClass(), id, obj)) {
      return false;
    }
    if (obj->as<NativeObject>().contains(cx, id)) {
      return false;
    }
  } else if (obj->is<TypedObject>()) {
    if (obj->as<TypedObject>().typeDescr().hasProperty(cx->names(), id)) {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

static bool CheckHasNoSuchProperty(JSContext* cx, JSObject* obj, jsid id) {
  JSObject* curObj = obj;
  do {
    if (!CheckHasNoSuchOwnProperty(cx, curObj, id)) {
      return false;
    }

    if (!curObj->isNative()) {
      // Non-native objects are only handled as the original receiver.
      if (curObj != obj) {
        return false;
      }
    }

    curObj = curObj->staticPrototype();
  } while (curObj);

  return true;
}

// Return whether obj is in some PreliminaryObjectArray and has a structure
// that might change in the future.
static bool IsPreliminaryObject(JSObject* obj) {
  if (obj->isSingleton()) {
    return false;
  }

  AutoSweepObjectGroup sweep(obj->group());
  TypeNewScript* newScript = obj->group()->newScript(sweep);
  if (newScript && !newScript->analyzed()) {
    return true;
  }

  if (obj->group()->maybePreliminaryObjects(sweep)) {
    return true;
  }

  return false;
}

static bool IsCacheableNoProperty(JSContext* cx, JSObject* obj,
                                  JSObject* holder, Shape* shape, jsid id,
                                  jsbytecode* pc,
                                  GetPropertyResultFlags resultFlags) {
  if (shape) {
    return false;
  }

  MOZ_ASSERT(!holder);

  // Idempotent ICs may only attach missing-property stubs if undefined
  // results are explicitly allowed, since no monitoring is done of the
  // cache result.
  if (!pc && !(resultFlags & GetPropertyResultFlags::AllowUndefined)) {
    return false;
  }

  // If we're doing a name lookup, we have to throw a ReferenceError.
  // Note that Ion does not generate idempotent caches for JSOp::GetBoundName.
  if (pc && JSOp(*pc) == JSOp::GetBoundName) {
    return false;
  }

  return CheckHasNoSuchProperty(cx, obj, id);
}

static NativeGetPropCacheability CanAttachNativeGetProp(
    JSContext* cx, HandleObject obj, HandleId id,
    MutableHandleNativeObject holder, MutableHandleShape shape, jsbytecode* pc,
    GetPropertyResultFlags resultFlags) {
  MOZ_ASSERT(JSID_IS_STRING(id) || JSID_IS_SYMBOL(id));

  // The lookup needs to be universally pure, otherwise we risk calling hooks
  // out of turn. We don't mind doing this even when purity isn't required,
  // because we only miss out on shape hashification, which is only a temporary
  // perf cost. The limits were arbitrarily set, anyways.
  JSObject* baseHolder = nullptr;
  PropertyResult prop;
  if (!LookupPropertyPure(cx, obj, id, &baseHolder, &prop)) {
    return CanAttachNone;
  }

  MOZ_ASSERT(!holder);
  if (baseHolder) {
    if (!baseHolder->isNative()) {
      return CanAttachNone;
    }
    holder.set(&baseHolder->as<NativeObject>());
  }
  shape.set(prop.maybeShape());

  if (IsCacheableGetPropReadSlot(obj, holder, prop)) {
    return CanAttachReadSlot;
  }

  if (IsCacheableNoProperty(cx, obj, holder, shape, id, pc, resultFlags)) {
    return CanAttachReadSlot;
  }

  // Idempotent ICs cannot call getters, see tryAttachIdempotentStub.
  if (pc && (resultFlags & GetPropertyResultFlags::Monitored)) {
    return IsCacheableGetPropCall(obj, holder, shape);
  }

  return CanAttachNone;
}

static void GuardGroupProto(CacheIRWriter& writer, JSObject* obj,
                            ObjOperandId objId) {
  // Uses the group to determine if the prototype is unchanged. If the
  // group's prototype is mutable, we must check the actual prototype,
  // otherwise checking the group is sufficient. This can be used if object
  // is not ShapedObject or if Shape has UNCACHEABLE_PROTO flag set.

  ObjectGroup* group = obj->groupRaw();

  if (group->hasUncacheableProto()) {
    if (JSObject* proto = obj->staticPrototype()) {
      writer.guardProto(objId, proto);
    } else {
      writer.guardNullProto(objId);
    }
  } else {
    writer.guardGroupForProto(objId, group);
  }
}

// Guard that a given object has same class and same OwnProperties (excluding
// dense elements and dynamic properties).
static void TestMatchingReceiver(CacheIRWriter& writer, JSObject* obj,
                                 ObjOperandId objId) {
  if (obj->is<TypedObject>()) {
    writer.guardGroupForLayout(objId, obj->group());
  } else if (obj->is<ProxyObject>()) {
    writer.guardShapeForClass(objId, obj->as<ProxyObject>().shape());
  } else {
    MOZ_ASSERT(obj->is<NativeObject>());
    writer.guardShapeForOwnProperties(objId,
                                      obj->as<NativeObject>().lastProperty());
  }
}

// Similar to |TestMatchingReceiver|, but specialized for NativeObject.
static void TestMatchingNativeReceiver(CacheIRWriter& writer, NativeObject* obj,
                                       ObjOperandId objId) {
  writer.guardShapeForOwnProperties(objId, obj->lastProperty());
}

// Similar to |TestMatchingReceiver|, but specialized for ProxyObject.
static void TestMatchingProxyReceiver(CacheIRWriter& writer, ProxyObject* obj,
                                      ObjOperandId objId) {
  writer.guardShapeForClass(objId, obj->shape());
}

// Adds additional guards if TestMatchingReceiver* does not also imply the
// prototype.
static void GeneratePrototypeGuardsForReceiver(CacheIRWriter& writer,
                                               JSObject* obj,
                                               ObjOperandId objId) {
  // If receiver was marked UNCACHEABLE_PROTO, the previous shape guard
  // doesn't ensure the prototype is unchanged. In this case we must use the
  // group to check the prototype.
  if (obj->hasUncacheableProto()) {
    MOZ_ASSERT(obj->is<NativeObject>());
    GuardGroupProto(writer, obj, objId);
  }

#ifdef DEBUG
  // The following cases already guaranteed the prototype is unchanged.
  if (obj->is<TypedObject>()) {
    MOZ_ASSERT(!obj->group()->hasUncacheableProto());
  } else if (obj->is<ProxyObject>()) {
    MOZ_ASSERT(!obj->hasUncacheableProto());
  }
#endif  // DEBUG
}

static bool ProtoChainSupportsTeleporting(JSObject* obj, JSObject* holder) {
  // Any non-delegate should already have been handled since its checks are
  // always required.
  MOZ_ASSERT(obj->isDelegate());

  // Prototype chain must have cacheable prototypes to ensure the cached
  // holder is the current holder.
  for (JSObject* tmp = obj; tmp != holder; tmp = tmp->staticPrototype()) {
    if (tmp->hasUncacheableProto()) {
      return false;
    }
  }

  // The holder itself only gets reshaped by teleportation if it is not
  // marked UNCACHEABLE_PROTO. See: ReshapeForProtoMutation.
  return !holder->hasUncacheableProto();
}

static void GeneratePrototypeGuards(CacheIRWriter& writer, JSObject* obj,
                                    JSObject* holder, ObjOperandId objId) {
  // Assuming target property is on |holder|, generate appropriate guards to
  // ensure |holder| is still on the prototype chain of |obj| and we haven't
  // introduced any shadowing definitions.
  //
  // For each item in the proto chain before holder, we must ensure that
  // [[GetPrototypeOf]] still has the expected result, and that
  // [[GetOwnProperty]] has no definition of the target property.
  //
  //
  // [SMDOC] Shape Teleporting Optimization
  // ------------------------------
  //
  // Starting with the assumption (and guideline to developers) that mutating
  // prototypes is an uncommon and fair-to-penalize operation we move cost
  // from the access side to the mutation side.
  //
  // Consider the following proto chain, with B defining a property 'x':
  //
  //      D  ->  C  ->  B{x: 3}  ->  A  -> null
  //
  // When accessing |D.x| we refer to D as the "receiver", and B as the
  // "holder". To optimize this access we need to ensure that neither D nor C
  // has since defined a shadowing property 'x'. Since C is a prototype that
  // we assume is rarely mutated we would like to avoid checking each time if
  // new properties are added. To do this we require that everytime C is
  // mutated that in addition to generating a new shape for itself, it will
  // walk the proto chain and generate new shapes for those objects on the
  // chain (B and A). As a result, checking the shape of D and B is
  // sufficient. Note that we do not care if the shape or properties of A
  // change since the lookup of 'x' will stop at B.
  //
  // The second condition we must verify is that the prototype chain was not
  // mutated. The same mechanism as above is used. When the prototype link is
  // changed, we generate a new shape for the object. If the object whose
  // link we are mutating is itself a prototype, we regenerate shapes down
  // the chain. This means the same two shape checks as above are sufficient.
  //
  // An additional wrinkle is the UNCACHEABLE_PROTO shape flag. This
  // indicates that the shape no longer implies any specific prototype. As
  // well, the shape will not be updated by the teleporting optimization.
  // If any shape from receiver to holder (inclusive) is UNCACHEABLE_PROTO,
  // we don't apply the optimization.
  //
  // See:
  //  - ReshapeForProtoMutation
  //  - ReshapeForShadowedProp

  MOZ_ASSERT(holder);
  MOZ_ASSERT(obj != holder);

  // Only DELEGATE objects participate in teleporting so peel off the first
  // object in the chain if needed and handle it directly.
  JSObject* pobj = obj;
  if (!obj->isDelegate()) {
    // TestMatchingReceiver does not always ensure the prototype is
    // unchanged, so generate extra guards as needed.
    GeneratePrototypeGuardsForReceiver(writer, obj, objId);

    pobj = obj->staticPrototype();
  }
  MOZ_ASSERT(pobj->isDelegate());

  // If teleporting is supported for this prototype chain, we are done.
  if (ProtoChainSupportsTeleporting(pobj, holder)) {
    return;
  }

  // If already at the holder, no further proto checks are needed.
  if (pobj == holder) {
    return;
  }

  // NOTE: We could be clever and look for a middle prototype to shape check
  //       and elide some (but not all) of the group checks. Unless we have
  //       real-world examples, let's avoid the complexity.

  // Synchronize pobj and protoId.
  MOZ_ASSERT(pobj == obj || pobj == obj->staticPrototype());
  ObjOperandId protoId = (pobj == obj) ? objId : writer.loadProto(objId);

  // Guard prototype links from |pobj| to |holder|.
  while (pobj != holder) {
    pobj = pobj->staticPrototype();

    // The object's proto could be nullptr so we must use GuardProto before
    // LoadProto (LoadProto asserts the proto is non-null).
    writer.guardProto(protoId, pobj);
    protoId = writer.loadProto(protoId);
  }
}

static void GeneratePrototypeHoleGuards(CacheIRWriter& writer, JSObject* obj,
                                        ObjOperandId objId,
                                        bool alwaysGuardFirstProto) {
  if (alwaysGuardFirstProto || obj->hasUncacheableProto()) {
    GuardGroupProto(writer, obj, objId);
  }

  JSObject* pobj = obj->staticPrototype();
  while (pobj) {
    ObjOperandId protoId = writer.loadObject(pobj);

    // If shape doesn't imply proto, additional guards are needed.
    if (pobj->hasUncacheableProto()) {
      GuardGroupProto(writer, pobj, protoId);
    }

    // Make sure the shape matches, to avoid non-dense elements or anything
    // else that is being checked by CanAttachDenseElementHole.
    writer.guardShape(protoId, pobj->as<NativeObject>().lastProperty());

    // Also make sure there are no dense elements.
    writer.guardNoDenseElements(protoId);

    pobj = pobj->staticPrototype();
  }
}

// Similar to |TestMatchingReceiver|, but for the holder object (when it
// differs from the receiver). The holder may also be the expando of the
// receiver if it exists.
static void TestMatchingHolder(CacheIRWriter& writer, JSObject* obj,
                               ObjOperandId objId) {
  // The GeneratePrototypeGuards + TestMatchingHolder checks only support
  // prototype chains composed of NativeObject (excluding the receiver
  // itself).
  MOZ_ASSERT(obj->is<NativeObject>());

  writer.guardShapeForOwnProperties(objId,
                                    obj->as<NativeObject>().lastProperty());
}

static bool UncacheableProtoOnChain(JSObject* obj) {
  while (true) {
    if (obj->hasUncacheableProto()) {
      return true;
    }

    obj = obj->staticPrototype();
    if (!obj) {
      return false;
    }
  }
}

static void ShapeGuardProtoChain(CacheIRWriter& writer, JSObject* obj,
                                 ObjOperandId objId) {
  while (true) {
    JSObject* proto = obj->staticPrototype();

    // Guard on the proto if the shape does not imply the proto.
    if (obj->hasUncacheableProto()) {
      if (proto) {
        writer.guardProto(objId, proto);
      } else {
        writer.guardNullProto(objId);
      }
    }

    if (!proto) {
      return;
    }

    obj = proto;
    objId = writer.loadProto(objId);

    writer.guardShape(objId, obj->as<NativeObject>().shape());
  }
}

// For cross compartment guards we shape-guard the prototype chain to avoid
// referencing the holder object.
//
// This peels off the first layer because it's guarded against obj == holder.
static void ShapeGuardProtoChainForCrossCompartmentHolder(
    CacheIRWriter& writer, JSObject* obj, ObjOperandId objId, JSObject* holder,
    Maybe<ObjOperandId>* holderId) {
  MOZ_ASSERT(obj != holder);
  MOZ_ASSERT(holder);
  while (true) {
    obj = obj->staticPrototype();
    MOZ_ASSERT(obj);

    objId = writer.loadProto(objId);
    if (obj == holder) {
      TestMatchingHolder(writer, obj, objId);
      holderId->emplace(objId);
      return;
    } else {
      writer.guardShapeForOwnProperties(objId, obj->as<NativeObject>().shape());
    }
  }
}

enum class SlotReadType { Normal, CrossCompartment };

template <SlotReadType MaybeCrossCompartment = SlotReadType::Normal>
static void EmitReadSlotGuard(CacheIRWriter& writer, JSObject* obj,
                              JSObject* holder, ObjOperandId objId,
                              Maybe<ObjOperandId>* holderId) {
  TestMatchingReceiver(writer, obj, objId);

  if (obj != holder) {
    if (holder) {
      if (MaybeCrossCompartment == SlotReadType::CrossCompartment) {
        // Guard proto chain integrity.
        // We use a variant of guards that avoid baking in any cross-compartment
        // object pointers.
        ShapeGuardProtoChainForCrossCompartmentHolder(writer, obj, objId,
                                                      holder, holderId);
      } else {
        // Guard proto chain integrity.
        GeneratePrototypeGuards(writer, obj, holder, objId);

        // Guard on the holder's shape.
        holderId->emplace(writer.loadObject(holder));
        TestMatchingHolder(writer, holder, holderId->ref());
      }
    } else {
      // The property does not exist. Guard on everything in the prototype
      // chain. This is guaranteed to see only Native objects because of
      // CanAttachNativeGetProp().
      ShapeGuardProtoChain(writer, obj, objId);
    }
  } else {
    holderId->emplace(objId);
  }
}

template <SlotReadType MaybeCrossCompartment = SlotReadType::Normal>
static void EmitReadSlotResult(CacheIRWriter& writer, JSObject* obj,
                               JSObject* holder, Shape* shape,
                               ObjOperandId objId) {
  Maybe<ObjOperandId> holderId;
  EmitReadSlotGuard<MaybeCrossCompartment>(writer, obj, holder, objId,
                                           &holderId);

  // Slot access.
  if (holder) {
    MOZ_ASSERT(holderId->valid());
    EmitLoadSlotResult(writer, *holderId, &holder->as<NativeObject>(), shape);
  } else {
    MOZ_ASSERT(holderId.isNothing());
    writer.loadUndefinedResult();
  }
}

static void EmitReadSlotReturn(CacheIRWriter& writer, JSObject*,
                               JSObject* holder, Shape* shape,
                               bool wrapResult = false) {
  // Slot access.
  if (holder) {
    MOZ_ASSERT(shape);
    if (wrapResult) {
      writer.wrapResult();
    }
    writer.typeMonitorResult();
  } else {
    // Normally for this op, the result would have to be monitored by TI.
    // However, since this stub ALWAYS returns UndefinedValue(), and we can be
    // sure that undefined is already registered with the type-set, this can be
    // avoided.
    writer.returnFromIC();
  }
}

static void EmitCallGetterResultNoGuards(JSContext* cx, CacheIRWriter& writer,
                                         JSObject* obj, JSObject* holder,
                                         Shape* shape,
                                         ValOperandId receiverId) {
  JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
  bool sameRealm = cx->realm() == target->realm();

  switch (IsCacheableGetPropCall(obj, holder, shape)) {
    case CanAttachNativeGetter: {
      writer.callNativeGetterResult(receiverId, target, sameRealm);
      writer.typeMonitorResult();
      break;
    }
    case CanAttachScriptedGetter: {
      writer.callScriptedGetterResult(receiverId, target, sameRealm);
      writer.typeMonitorResult();
      break;
    }
    default:
      // CanAttachNativeGetProp guarantees that the getter is either a native or
      // a scripted function.
      MOZ_ASSERT_UNREACHABLE("Can't attach getter");
      break;
  }
}

static void EmitCallGetterResultGuards(CacheIRWriter& writer, JSObject* obj,
                                       JSObject* holder, Shape* shape,
                                       ObjOperandId objId, ICState::Mode mode) {
  // Use the megamorphic guard if we're in megamorphic mode, except if |obj|
  // is a Window as GuardHasGetterSetter doesn't support this yet (Window may
  // require outerizing).
  if (mode == ICState::Mode::Specialized || IsWindow(obj)) {
    TestMatchingReceiver(writer, obj, objId);

    if (obj != holder) {
      GeneratePrototypeGuards(writer, obj, holder, objId);

      // Guard on the holder's shape.
      ObjOperandId holderId = writer.loadObject(holder);
      TestMatchingHolder(writer, holder, holderId);
    }
  } else {
    writer.guardHasGetterSetter(objId, shape);
  }
}

static void EmitCallGetterResult(JSContext* cx, CacheIRWriter& writer,
                                 JSObject* obj, JSObject* holder, Shape* shape,
                                 ObjOperandId objId, ValOperandId receiverId,
                                 ICState::Mode mode) {
  EmitCallGetterResultGuards(writer, obj, holder, shape, objId, mode);
  EmitCallGetterResultNoGuards(cx, writer, obj, holder, shape, receiverId);
}

void GetPropIRGenerator::attachMegamorphicNativeSlot(ObjOperandId objId,
                                                     jsid id,
                                                     bool handleMissing) {
  MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);

  // The stub handles the missing-properties case only if we're seeing one
  // now, to make sure Ion ICs correctly monitor the undefined type. Without
  // TI we don't use type monitoring so always allow |undefined|.
  if (!IsTypeInferenceEnabled()) {
    handleMissing = true;
  }

  if (cacheKind_ == CacheKind::GetProp ||
      cacheKind_ == CacheKind::GetPropSuper) {
    writer.megamorphicLoadSlotResult(objId, JSID_TO_ATOM(id)->asPropertyName(),
                                     handleMissing);
  } else {
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem ||
               cacheKind_ == CacheKind::GetElemSuper);
    writer.megamorphicLoadSlotByValueResult(objId, getElemKeyValueId(),
                                            handleMissing);
  }
  writer.typeMonitorResult();

  trackAttached(handleMissing ? "MegamorphicMissingNativeSlot"
                              : "MegamorphicNativeSlot");
}

AttachDecision GetPropIRGenerator::tryAttachNative(HandleObject obj,
                                                   ObjOperandId objId,
                                                   HandleId id,
                                                   ValOperandId receiverId) {
  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);

  NativeGetPropCacheability type =
      CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_, resultFlags_);
  switch (type) {
    case CanAttachNone:
      return AttachDecision::NoAction;
    case CanAttachReadSlot:
      if (mode_ == ICState::Mode::Megamorphic) {
        attachMegamorphicNativeSlot(objId, id, holder == nullptr);
        return AttachDecision::Attach;
      }

      maybeEmitIdGuard(id);
      if (holder) {
        EnsureTrackPropertyTypes(cx_, holder, id);
        // See the comment in StripPreliminaryObjectStubs.
        if (IsPreliminaryObject(obj)) {
          preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
        } else {
          preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
        }
      }
      EmitReadSlotResult(writer, obj, holder, shape, objId);
      EmitReadSlotReturn(writer, obj, holder, shape);

      trackAttached("NativeSlot");
      return AttachDecision::Attach;
    case CanAttachScriptedGetter:
    case CanAttachNativeGetter: {
      MOZ_ASSERT(!idempotent());
      maybeEmitIdGuard(id);
      EmitCallGetterResult(cx_, writer, obj, holder, shape, objId, receiverId,
                           mode_);

      trackAttached("NativeGetter");
      return AttachDecision::Attach;
    }
  }

  MOZ_CRASH("Bad NativeGetPropCacheability");
}

bool js::jit::IsWindowProxyForScriptGlobal(JSScript* script, JSObject* obj) {
  if (!IsWindowProxy(obj)) {
    return false;
  }

  MOZ_ASSERT(obj->getClass() ==
             script->runtimeFromMainThread()->maybeWindowProxyClass());

  JSObject* window = ToWindowIfWindowProxy(obj);

  // Ion relies on the WindowProxy's group changing (and the group getting
  // marked as having unknown properties) on navigation. If we ever stop
  // transplanting same-compartment WindowProxies, this assert will fail and we
  // need to fix that code.
  MOZ_ASSERT(window == &obj->nonCCWGlobal());

  // This must be a WindowProxy for a global in this compartment. Else it would
  // be a cross-compartment wrapper and IsWindowProxy returns false for
  // those.
  MOZ_ASSERT(script->compartment() == obj->compartment());

  // Only optimize lookups on the WindowProxy for the current global. Other
  // WindowProxies in the compartment may require security checks (based on
  // mutable document.domain). See bug 1516775.
  return window == &script->global();
}

// Guards objId is a WindowProxy for windowObj. Returns the window's operand id.
static ObjOperandId GuardAndLoadWindowProxyWindow(CacheIRWriter& writer,
                                                  ObjOperandId objId,
                                                  GlobalObject* windowObj) {
  // Note: update AddCacheIRGetPropFunction in BaselineInspector.cpp when making
  // changes here.
  writer.guardClass(objId, GuardClassKind::WindowProxy);
  ObjOperandId windowObjId = writer.loadWrapperTarget(objId);
  writer.guardSpecificObject(windowObjId, windowObj);
  return windowObjId;
}

AttachDecision GetPropIRGenerator::tryAttachWindowProxy(HandleObject obj,
                                                        ObjOperandId objId,
                                                        HandleId id) {
  // Attach a stub when the receiver is a WindowProxy and we can do the lookup
  // on the Window (the global object).

  if (!IsWindowProxyForScriptGlobal(script_, obj)) {
    return AttachDecision::NoAction;
  }

  // If we're megamorphic prefer a generic proxy stub that handles a lot more
  // cases.
  if (mode_ == ICState::Mode::Megamorphic) {
    return AttachDecision::NoAction;
  }

  // Now try to do the lookup on the Window (the current global).
  Handle<GlobalObject*> windowObj = cx_->global();
  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);
  NativeGetPropCacheability type = CanAttachNativeGetProp(
      cx_, windowObj, id, &holder, &shape, pc_, resultFlags_);
  switch (type) {
    case CanAttachNone:
      return AttachDecision::NoAction;

    case CanAttachReadSlot: {
      maybeEmitIdGuard(id);
      ObjOperandId windowObjId =
          GuardAndLoadWindowProxyWindow(writer, objId, windowObj);
      EmitReadSlotResult(writer, windowObj, holder, shape, windowObjId);
      EmitReadSlotReturn(writer, windowObj, holder, shape);

      trackAttached("WindowProxySlot");
      return AttachDecision::Attach;
    }

    case CanAttachNativeGetter: {
      // Make sure the native getter is okay with the IC passing the Window
      // instead of the WindowProxy as |this| value.
      JSFunction* callee = &shape->getterObject()->as<JSFunction>();
      MOZ_ASSERT(callee->isNativeWithoutJitEntry());
      if (!callee->hasJitInfo() ||
          callee->jitInfo()->needsOuterizedThisObject()) {
        return AttachDecision::NoAction;
      }

      // If a |super| access, it is not worth the complexity to attach an IC.
      if (isSuper()) {
        return AttachDecision::NoAction;
      }

      // Guard the incoming object is a WindowProxy and inline a getter call
      // based on the Window object.
      maybeEmitIdGuard(id);
      ObjOperandId windowObjId =
          GuardAndLoadWindowProxyWindow(writer, objId, windowObj);
      ValOperandId receiverId = writer.boxObject(windowObjId);
      EmitCallGetterResult(cx_, writer, windowObj, holder, shape, windowObjId,
                           receiverId, mode_);

      trackAttached("WindowProxyGetter");
      return AttachDecision::Attach;
    }

    case CanAttachScriptedGetter:
      MOZ_ASSERT_UNREACHABLE("Not possible for window proxies");
  }

  MOZ_CRASH("Unreachable");
}

AttachDecision GetPropIRGenerator::tryAttachCrossCompartmentWrapper(
    HandleObject obj, ObjOperandId objId, HandleId id) {
  // We can only optimize this very wrapper-handler, because others might
  // have a security policy.
  if (!IsWrapper(obj) ||
      Wrapper::wrapperHandler(obj) != &CrossCompartmentWrapper::singleton) {
    return AttachDecision::NoAction;
  }

  // If we're megamorphic prefer a generic proxy stub that handles a lot more
  // cases.
  if (mode_ == ICState::Mode::Megamorphic) {
    return AttachDecision::NoAction;
  }

  RootedObject unwrapped(cx_, Wrapper::wrappedObject(obj));
  MOZ_ASSERT(unwrapped == UnwrapOneCheckedStatic(obj));
  MOZ_ASSERT(!IsCrossCompartmentWrapper(unwrapped),
             "CCWs must not wrap other CCWs");

  // If we allowed different zones we would have to wrap strings.
  if (unwrapped->compartment()->zone() != cx_->compartment()->zone()) {
    return AttachDecision::NoAction;
  }

  // Take the unwrapped object's global, and wrap in a
  // this-compartment wrapper. This is what will be stored in the IC
  // keep the compartment alive.
  RootedObject wrappedTargetGlobal(cx_, &unwrapped->nonCCWGlobal());
  if (!cx_->compartment()->wrap(cx_, &wrappedTargetGlobal)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);

  // Enter realm of target since some checks have side-effects
  // such as de-lazifying type info.
  {
    AutoRealm ar(cx_, unwrapped);

    NativeGetPropCacheability canCache = CanAttachNativeGetProp(
        cx_, unwrapped, id, &holder, &shape, pc_, resultFlags_);
    if (canCache != CanAttachReadSlot) {
      return AttachDecision::NoAction;
    }

    if (holder) {
      // Need to be in the compartment of the holder to
      // call EnsureTrackPropertyTypes
      EnsureTrackPropertyTypes(cx_, holder, id);
      if (unwrapped == holder) {
        // See the comment in StripPreliminaryObjectStubs.
        if (IsPreliminaryObject(unwrapped)) {
          preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
        } else {
          preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
        }
      }
    } else {
      // UNCACHEABLE_PROTO may result in guards against specific
      // (cross-compartment) prototype objects, so don't try to attach IC if we
      // see the flag at all.
      if (UncacheableProtoOnChain(unwrapped)) {
        return AttachDecision::NoAction;
      }
    }
  }

  maybeEmitIdGuard(id);
  writer.guardIsProxy(objId);
  writer.guardHasProxyHandler(objId, Wrapper::wrapperHandler(obj));

  // Load the object wrapped by the CCW
  ObjOperandId wrapperTargetId = writer.loadWrapperTarget(objId);

  // If the compartment of the wrapped object is different we should fail.
  writer.guardCompartment(wrapperTargetId, wrappedTargetGlobal,
                          unwrapped->compartment());

  ObjOperandId unwrappedId = wrapperTargetId;
  EmitReadSlotResult<SlotReadType::CrossCompartment>(writer, unwrapped, holder,
                                                     shape, unwrappedId);
  EmitReadSlotReturn(writer, unwrapped, holder, shape, /* wrapResult = */ true);

  trackAttached("CCWSlot");
  return AttachDecision::Attach;
}

static bool GetXrayExpandoShapeWrapper(JSContext* cx, HandleObject xray,
                                       MutableHandleObject wrapper) {
  Value v = GetProxyReservedSlot(xray, GetXrayJitInfo()->xrayHolderSlot);
  if (v.isObject()) {
    NativeObject* holder = &v.toObject().as<NativeObject>();
    v = holder->getFixedSlot(GetXrayJitInfo()->holderExpandoSlot);
    if (v.isObject()) {
      RootedNativeObject expando(
          cx, &UncheckedUnwrap(&v.toObject())->as<NativeObject>());
      wrapper.set(NewWrapperWithObjectShape(cx, expando));
      return wrapper != nullptr;
    }
  }
  wrapper.set(nullptr);
  return true;
}

AttachDecision GetPropIRGenerator::tryAttachXrayCrossCompartmentWrapper(
    HandleObject obj, ObjOperandId objId, HandleId id,
    ValOperandId receiverId) {
  if (!IsProxy(obj)) {
    return AttachDecision::NoAction;
  }

  JS::XrayJitInfo* info = GetXrayJitInfo();
  if (!info || !info->isCrossCompartmentXray(GetProxyHandler(obj))) {
    return AttachDecision::NoAction;
  }

  if (!info->compartmentHasExclusiveExpandos(obj)) {
    return AttachDecision::NoAction;
  }

  RootedObject target(cx_, UncheckedUnwrap(obj));

  RootedObject expandoShapeWrapper(cx_);
  if (!GetXrayExpandoShapeWrapper(cx_, obj, &expandoShapeWrapper)) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Look for a getter we can call on the xray or its prototype chain.
  Rooted<PropertyDescriptor> desc(cx_);
  RootedObject holder(cx_, obj);
  RootedObjectVector prototypes(cx_);
  RootedObjectVector prototypeExpandoShapeWrappers(cx_);
  while (true) {
    if (!GetOwnPropertyDescriptor(cx_, holder, id, &desc)) {
      cx_->clearPendingException();
      return AttachDecision::NoAction;
    }
    if (desc.object()) {
      break;
    }
    if (!GetPrototype(cx_, holder, &holder)) {
      cx_->clearPendingException();
      return AttachDecision::NoAction;
    }
    if (!holder || !IsProxy(holder) ||
        !info->isCrossCompartmentXray(GetProxyHandler(holder))) {
      return AttachDecision::NoAction;
    }
    RootedObject prototypeExpandoShapeWrapper(cx_);
    if (!GetXrayExpandoShapeWrapper(cx_, holder,
                                    &prototypeExpandoShapeWrapper) ||
        !prototypes.append(holder) ||
        !prototypeExpandoShapeWrappers.append(prototypeExpandoShapeWrapper)) {
      cx_->recoverFromOutOfMemory();
      return AttachDecision::NoAction;
    }
  }
  if (!desc.isAccessorDescriptor()) {
    return AttachDecision::NoAction;
  }

  RootedObject getter(cx_, desc.getterObject());
  if (!getter || !getter->is<JSFunction>() ||
      !getter->as<JSFunction>().isNativeWithoutJitEntry()) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);
  writer.guardIsProxy(objId);
  writer.guardHasProxyHandler(objId, GetProxyHandler(obj));

  // Load the object wrapped by the CCW
  ObjOperandId wrapperTargetId = writer.loadWrapperTarget(objId);

  // Test the wrapped object's class. The properties held by xrays or their
  // prototypes will be invariant for objects of a given class, except for
  // changes due to xray expandos or xray prototype mutations.
  writer.guardAnyClass(wrapperTargetId, target->getClass());

  // Make sure the expandos on the xray and its prototype chain match up with
  // what we expect. The expando shape needs to be consistent, to ensure it
  // has not had any shadowing properties added, and the expando cannot have
  // any custom prototype (xray prototypes are stable otherwise).
  //
  // We can only do this for xrays with exclusive access to their expandos
  // (as we checked earlier), which store a pointer to their expando
  // directly. Xrays in other compartments may share their expandos with each
  // other and a VM call is needed just to find the expando.
  if (expandoShapeWrapper) {
    writer.guardXrayExpandoShapeAndDefaultProto(objId, expandoShapeWrapper);
  } else {
    writer.guardXrayNoExpando(objId);
  }
  for (size_t i = 0; i < prototypes.length(); i++) {
    JSObject* proto = prototypes[i];
    ObjOperandId protoId = writer.loadObject(proto);
    if (JSObject* protoShapeWrapper = prototypeExpandoShapeWrappers[i]) {
      writer.guardXrayExpandoShapeAndDefaultProto(protoId, protoShapeWrapper);
    } else {
      writer.guardXrayNoExpando(protoId);
    }
  }

  bool sameRealm = cx_->realm() == getter->as<JSFunction>().realm();
  writer.callNativeGetterResult(receiverId, &getter->as<JSFunction>(),
                                sameRealm);
  writer.typeMonitorResult();

  trackAttached("XrayGetter");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachGenericProxy(
    HandleObject obj, ObjOperandId objId, HandleId id, bool handleDOMProxies) {
  MOZ_ASSERT(obj->is<ProxyObject>());

  writer.guardIsProxy(objId);

  if (!handleDOMProxies) {
    // Ensure that the incoming object is not a DOM proxy, so that we can get to
    // the specialized stubs
    writer.guardIsNotDOMProxy(objId);
  }

  if (cacheKind_ == CacheKind::GetProp || mode_ == ICState::Mode::Specialized) {
    MOZ_ASSERT(!isSuper());
    maybeEmitIdGuard(id);
    writer.proxyGetResult(objId, id);
  } else {
    // Attach a stub that handles every id.
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
    MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);
    MOZ_ASSERT(!isSuper());
    writer.proxyGetByValueResult(objId, getElemKeyValueId());
  }

  writer.typeMonitorResult();

  trackAttached("GenericProxy");
  return AttachDecision::Attach;
}

ObjOperandId IRGenerator::guardDOMProxyExpandoObjectAndShape(
    JSObject* obj, ObjOperandId objId, const Value& expandoVal,
    JSObject* expandoObj) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  TestMatchingProxyReceiver(writer, &obj->as<ProxyObject>(), objId);

  // Shape determines Class, so now it must be a DOM proxy.
  ValOperandId expandoValId;
  if (expandoVal.isObject()) {
    expandoValId = writer.loadDOMExpandoValue(objId);
  } else {
    expandoValId = writer.loadDOMExpandoValueIgnoreGeneration(objId);
  }

  // Guard the expando is an object and shape guard.
  ObjOperandId expandoObjId = writer.guardToObject(expandoValId);
  TestMatchingHolder(writer, expandoObj, expandoObjId);
  return expandoObjId;
}

AttachDecision GetPropIRGenerator::tryAttachDOMProxyExpando(
    HandleObject obj, ObjOperandId objId, HandleId id,
    ValOperandId receiverId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  RootedValue expandoVal(cx_, GetProxyPrivate(obj));
  RootedObject expandoObj(cx_);
  if (expandoVal.isObject()) {
    expandoObj = &expandoVal.toObject();
  } else {
    MOZ_ASSERT(!expandoVal.isUndefined(),
               "How did a missing expando manage to shadow things?");
    auto expandoAndGeneration =
        static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
    MOZ_ASSERT(expandoAndGeneration);
    expandoObj = &expandoAndGeneration->expando.toObject();
  }

  // Try to do the lookup on the expando object.
  RootedNativeObject holder(cx_);
  RootedShape propShape(cx_);
  NativeGetPropCacheability canCache = CanAttachNativeGetProp(
      cx_, expandoObj, id, &holder, &propShape, pc_, resultFlags_);
  if (canCache == CanAttachNone) {
    return AttachDecision::NoAction;
  }
  if (!holder) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(holder == expandoObj);

  maybeEmitIdGuard(id);
  ObjOperandId expandoObjId =
      guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

  if (canCache == CanAttachReadSlot) {
    // Load from the expando's slots.
    EmitLoadSlotResult(writer, expandoObjId, &expandoObj->as<NativeObject>(),
                       propShape);
    writer.typeMonitorResult();
  } else {
    // Call the getter. Note that we pass objId, the DOM proxy, as |this|
    // and not the expando object.
    MOZ_ASSERT(canCache == CanAttachNativeGetter ||
               canCache == CanAttachScriptedGetter);
    EmitCallGetterResultNoGuards(cx_, writer, expandoObj, expandoObj, propShape,
                                 receiverId);
  }

  trackAttached("DOMProxyExpando");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachDOMProxyShadowed(HandleObject obj,
                                                             ObjOperandId objId,
                                                             HandleId id) {
  MOZ_ASSERT(!isSuper());
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  maybeEmitIdGuard(id);
  TestMatchingProxyReceiver(writer, &obj->as<ProxyObject>(), objId);
  writer.proxyGetResult(objId, id);
  writer.typeMonitorResult();

  trackAttached("DOMProxyShadowed");
  return AttachDecision::Attach;
}

// Callers are expected to have already guarded on the shape of the
// object, which guarantees the object is a DOM proxy.
static void CheckDOMProxyExpandoDoesNotShadow(CacheIRWriter& writer,
                                              JSObject* obj, jsid id,
                                              ObjOperandId objId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  Value expandoVal = GetProxyPrivate(obj);

  ValOperandId expandoId;
  if (!expandoVal.isObject() && !expandoVal.isUndefined()) {
    auto expandoAndGeneration =
        static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
    uint64_t generation = expandoAndGeneration->generation;
    expandoId = writer.loadDOMExpandoValueGuardGeneration(
        objId, expandoAndGeneration, generation);
    expandoVal = expandoAndGeneration->expando;
  } else {
    expandoId = writer.loadDOMExpandoValue(objId);
  }

  if (expandoVal.isUndefined()) {
    // Guard there's no expando object.
    writer.guardNonDoubleType(expandoId, ValueType::Undefined);
  } else if (expandoVal.isObject()) {
    // Guard the proxy either has no expando object or, if it has one, that
    // the shape matches the current expando object.
    NativeObject& expandoObj = expandoVal.toObject().as<NativeObject>();
    MOZ_ASSERT(!expandoObj.containsPure(id));
    writer.guardDOMExpandoMissingOrGuardShape(expandoId,
                                              expandoObj.lastProperty());
  } else {
    MOZ_CRASH("Invalid expando value");
  }
}

AttachDecision GetPropIRGenerator::tryAttachDOMProxyUnshadowed(
    HandleObject obj, ObjOperandId objId, HandleId id,
    ValOperandId receiverId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  RootedObject checkObj(cx_, obj->staticPrototype());
  if (!checkObj) {
    return AttachDecision::NoAction;
  }

  RootedNativeObject holder(cx_);
  RootedShape shape(cx_);
  NativeGetPropCacheability canCache = CanAttachNativeGetProp(
      cx_, checkObj, id, &holder, &shape, pc_, resultFlags_);
  if (canCache == CanAttachNone) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);

  // Guard that our expando object hasn't started shadowing this property.
  TestMatchingProxyReceiver(writer, &obj->as<ProxyObject>(), objId);
  CheckDOMProxyExpandoDoesNotShadow(writer, obj, id, objId);

  if (holder) {
    // Found the property on the prototype chain. Treat it like a native
    // getprop.
    GeneratePrototypeGuards(writer, obj, holder, objId);

    // Guard on the holder of the property.
    ObjOperandId holderId = writer.loadObject(holder);
    TestMatchingHolder(writer, holder, holderId);

    if (canCache == CanAttachReadSlot) {
      EmitLoadSlotResult(writer, holderId, holder, shape);
      writer.typeMonitorResult();
    } else {
      // EmitCallGetterResultNoGuards expects |obj| to be the object the
      // property is on to do some checks. Since we actually looked at
      // checkObj, and no extra guards will be generated, we can just
      // pass that instead.
      MOZ_ASSERT(canCache == CanAttachNativeGetter ||
                 canCache == CanAttachScriptedGetter);
      MOZ_ASSERT(!isSuper());
      EmitCallGetterResultNoGuards(cx_, writer, checkObj, holder, shape,
                                   receiverId);
    }
  } else {
    // Property was not found on the prototype chain. Deoptimize down to
    // proxy get call.
    MOZ_ASSERT(!isSuper());
    writer.proxyGetResult(objId, id);
    writer.typeMonitorResult();
  }

  trackAttached("DOMProxyUnshadowed");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachProxy(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id,
                                                  ValOperandId receiverId) {
  ProxyStubType type = GetProxyStubType(cx_, obj, id);
  if (type == ProxyStubType::None) {
    return AttachDecision::NoAction;
  }

  // The proxy stubs don't currently support |super| access.
  if (isSuper()) {
    return AttachDecision::NoAction;
  }

  if (mode_ == ICState::Mode::Megamorphic) {
    return tryAttachGenericProxy(obj, objId, id, /* handleDOMProxies = */ true);
  }

  switch (type) {
    case ProxyStubType::None:
      break;
    case ProxyStubType::DOMExpando:
      TRY_ATTACH(tryAttachDOMProxyExpando(obj, objId, id, receiverId));
      [[fallthrough]];  // Fall through to the generic shadowed case.
    case ProxyStubType::DOMShadowed:
      return tryAttachDOMProxyShadowed(obj, objId, id);
    case ProxyStubType::DOMUnshadowed:
      TRY_ATTACH(tryAttachDOMProxyUnshadowed(obj, objId, id, receiverId));
      return tryAttachGenericProxy(obj, objId, id,
                                   /* handleDOMProxies = */ true);
    case ProxyStubType::Generic:
      return tryAttachGenericProxy(obj, objId, id,
                                   /* handleDOMProxies = */ false);
  }

  MOZ_CRASH("Unexpected ProxyStubType");
}

static TypedThingLayout GetTypedThingLayout(const JSClass* clasp) {
  if (IsTypedArrayClass(clasp)) {
    return TypedThingLayout::TypedArray;
  }
  if (IsOutlineTypedObjectClass(clasp)) {
    return TypedThingLayout::OutlineTypedObject;
  }
  if (IsInlineTypedObjectClass(clasp)) {
    return TypedThingLayout::InlineTypedObject;
  }
  MOZ_CRASH("Bad object class");
}

AttachDecision GetPropIRGenerator::tryAttachTypedObject(HandleObject obj,
                                                        ObjOperandId objId,
                                                        HandleId id) {
  if (!obj->is<TypedObject>()) {
    return AttachDecision::NoAction;
  }

  TypedObject* typedObj = &obj->as<TypedObject>();
  if (!typedObj->typeDescr().is<StructTypeDescr>()) {
    return AttachDecision::NoAction;
  }

  StructTypeDescr* structDescr = &typedObj->typeDescr().as<StructTypeDescr>();
  size_t fieldIndex;
  if (!structDescr->fieldIndex(id, &fieldIndex)) {
    return AttachDecision::NoAction;
  }

  TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
  if (!fieldDescr->is<SimpleTypeDescr>()) {
    return AttachDecision::NoAction;
  }

  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
  uint32_t typeDescr = SimpleTypeDescrKey(&fieldDescr->as<SimpleTypeDescr>());

  maybeEmitIdGuard(id);
  writer.guardGroupForLayout(objId, obj->group());
  writer.loadTypedObjectResult(objId, layout, typeDescr, fieldOffset);

  // Only monitor the result if the type produced by this stub might vary.
  bool monitorLoad = false;
  if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
    Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);
    monitorLoad = type == Scalar::Uint32;
  } else {
    ReferenceType type = ReferenceTypeFromSimpleTypeDescrKey(typeDescr);
    monitorLoad = type != ReferenceType::TYPE_STRING;
  }

  if (monitorLoad) {
    writer.typeMonitorResult();
  } else {
    writer.returnFromIC();
  }

  trackAttached("TypedObject");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachObjectLength(HandleObject obj,
                                                         ObjOperandId objId,
                                                         HandleId id) {
  if (!JSID_IS_ATOM(id, cx_->names().length)) {
    return AttachDecision::NoAction;
  }

  if (!(resultFlags_ & GetPropertyResultFlags::AllowInt32)) {
    return AttachDecision::NoAction;
  }

  if (obj->is<ArrayObject>()) {
    // Make sure int32 is added to the TypeSet before we attach a stub, so
    // the stub can return int32 values without monitoring the result.
    if (obj->as<ArrayObject>().length() > INT32_MAX) {
      return AttachDecision::NoAction;
    }

    maybeEmitIdGuard(id);
    writer.guardClass(objId, GuardClassKind::Array);
    writer.loadInt32ArrayLengthResult(objId);
    writer.returnFromIC();

    trackAttached("ArrayLength");
    return AttachDecision::Attach;
  }

  if (obj->is<ArgumentsObject>() &&
      !obj->as<ArgumentsObject>().hasOverriddenLength()) {
    maybeEmitIdGuard(id);
    if (obj->is<MappedArgumentsObject>()) {
      writer.guardClass(objId, GuardClassKind::MappedArguments);
    } else {
      MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
      writer.guardClass(objId, GuardClassKind::UnmappedArguments);
    }
    writer.loadArgumentsObjectLengthResult(objId);
    writer.returnFromIC();

    trackAttached("ArgumentsObjectLength");
    return AttachDecision::Attach;
  }

  return AttachDecision::NoAction;
}

AttachDecision GetPropIRGenerator::tryAttachTypedArrayLength(HandleObject obj,
                                                             ObjOperandId objId,
                                                             HandleId id) {
  if (!JSID_IS_ATOM(id, cx_->names().length)) {
    return AttachDecision::NoAction;
  }

  if (!obj->is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }

  // Receiver should be the object.
  if (isSuper()) {
    return AttachDecision::NoAction;
  }

  if (!(resultFlags_ & GetPropertyResultFlags::AllowInt32)) {
    return AttachDecision::NoAction;
  }

  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);
  NativeGetPropCacheability type =
      CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_, resultFlags_);
  if (type != CanAttachNativeGetter) {
    return AttachDecision::NoAction;
  }

  JSFunction& fun = shape->getterValue().toObject().as<JSFunction>();
  if (!TypedArrayObject::isOriginalLengthGetter(fun.native())) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);
  // Emit all the normal guards for calling this native, but specialize
  // callNativeGetterResult. Also store the getter itself to enable
  // AddCacheIRGetPropFunction to read it from the IC stub, which is needed for
  // Ion-inlining.
  EmitCallGetterResultGuards(writer, obj, holder, shape, objId, mode_);
  writer.loadTypedArrayLengthResult(objId, &fun);
  writer.returnFromIC();

  trackAttached("TypedArrayLength");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachFunction(HandleObject obj,
                                                     ObjOperandId objId,
                                                     HandleId id) {
  // Function properties are lazily resolved so they might not be defined yet.
  // And we might end up in a situation where we always have a fresh function
  // object during the IC generation.
  if (!obj->is<JSFunction>()) {
    return AttachDecision::NoAction;
  }

  JSObject* holder = nullptr;
  PropertyResult prop;
  // This property exists already, don't attach the stub.
  if (LookupPropertyPure(cx_, obj, id, &holder, &prop)) {
    return AttachDecision::NoAction;
  }

  JSFunction* fun = &obj->as<JSFunction>();

  if (JSID_IS_ATOM(id, cx_->names().length)) {
    // length was probably deleted from the function.
    if (fun->hasResolvedLength()) {
      return AttachDecision::NoAction;
    }

    // Lazy functions don't store the length.
    if (!fun->hasBytecode()) {
      return AttachDecision::NoAction;
    }

    maybeEmitIdGuard(id);
    writer.guardClass(objId, GuardClassKind::JSFunction);
    writer.loadFunctionLengthResult(objId);
    writer.returnFromIC();

    trackAttached("FunctionLength");
    return AttachDecision::Attach;
  }

  return AttachDecision::NoAction;
}

AttachDecision GetPropIRGenerator::tryAttachModuleNamespace(HandleObject obj,
                                                            ObjOperandId objId,
                                                            HandleId id) {
  if (!obj->is<ModuleNamespaceObject>()) {
    return AttachDecision::NoAction;
  }

  Rooted<ModuleNamespaceObject*> ns(cx_, &obj->as<ModuleNamespaceObject>());
  RootedModuleEnvironmentObject env(cx_);
  RootedShape shape(cx_);
  if (!ns->bindings().lookup(id, env.address(), shape.address())) {
    return AttachDecision::NoAction;
  }

  // Don't emit a stub until the target binding has been initialized.
  if (env->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL)) {
    return AttachDecision::NoAction;
  }

  if (IsIonEnabled(cx_)) {
    EnsureTrackPropertyTypes(cx_, env, shape->propid());
  }

  // Check for the specific namespace object.
  maybeEmitIdGuard(id);
  writer.guardSpecificObject(objId, ns);

  ObjOperandId envId = writer.loadObject(env);
  EmitLoadSlotResult(writer, envId, env, shape);
  writer.typeMonitorResult();

  trackAttached("ModuleNamespace");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachPrimitive(ValOperandId valId,
                                                      HandleId id) {
  MOZ_ASSERT(!isSuper(), "SuperBase is guaranteed to be an object");

  JSProtoKey protoKey;
  switch (val_.type()) {
    case ValueType::String:
      if (JSID_IS_ATOM(id, cx_->names().length)) {
        // String length is special-cased, see js::GetProperty.
        return AttachDecision::NoAction;
      }
      protoKey = JSProto_String;
      break;
    case ValueType::Int32:
    case ValueType::Double:
      protoKey = JSProto_Number;
      break;
    case ValueType::Boolean:
      protoKey = JSProto_Boolean;
      break;
    case ValueType::Symbol:
      protoKey = JSProto_Symbol;
      break;
    case ValueType::BigInt:
      protoKey = JSProto_BigInt;
      break;
    case ValueType::Null:
    case ValueType::Undefined:
    case ValueType::Magic:
      return AttachDecision::NoAction;
    case ValueType::Object:
    case ValueType::PrivateGCThing:
      MOZ_CRASH("unexpected type");
  }

  RootedObject proto(cx_, cx_->global()->maybeGetPrototype(protoKey));
  if (!proto) {
    return AttachDecision::NoAction;
  }

  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);
  NativeGetPropCacheability type = CanAttachNativeGetProp(
      cx_, proto, id, &holder, &shape, pc_, resultFlags_);
  switch (type) {
    case CanAttachNone:
      return AttachDecision::NoAction;
    case CanAttachReadSlot: {
      if (holder) {
        // Instantiate this property, for use during Ion compilation.
        if (IsIonEnabled(cx_)) {
          EnsureTrackPropertyTypes(cx_, holder, id);
        }
      }

      if (val_.isNumber()) {
        writer.guardIsNumber(valId);
      } else {
        writer.guardNonDoubleType(valId, val_.type());
      }
      maybeEmitIdGuard(id);

      ObjOperandId protoId = writer.loadObject(proto);
      EmitReadSlotResult(writer, proto, holder, shape, protoId);
      EmitReadSlotReturn(writer, proto, holder, shape);

      trackAttached("PrimitiveSlot");
      return AttachDecision::Attach;
    }
    case CanAttachScriptedGetter:
    case CanAttachNativeGetter: {
      MOZ_ASSERT(!idempotent());

      if (val_.isNumber()) {
        writer.guardIsNumber(valId);
      } else {
        writer.guardNonDoubleType(valId, val_.type());
      }
      maybeEmitIdGuard(id);

      ObjOperandId protoId = writer.loadObject(proto);
      EmitCallGetterResult(cx_, writer, proto, holder, shape, protoId, valId,
                           mode_);

      trackAttached("PrimitiveGetter");
      return AttachDecision::Attach;
    }
  }

  MOZ_CRASH("Bad NativeGetPropCacheability");
}

AttachDecision GetPropIRGenerator::tryAttachStringLength(ValOperandId valId,
                                                         HandleId id) {
  if (!val_.isString() || !JSID_IS_ATOM(id, cx_->names().length)) {
    return AttachDecision::NoAction;
  }

  StringOperandId strId = writer.guardToString(valId);
  maybeEmitIdGuard(id);
  writer.loadStringLengthResult(strId);
  writer.returnFromIC();

  trackAttached("StringLength");
  return AttachDecision::Attach;
}

static bool CanAttachStringChar(HandleValue val, HandleValue idVal) {
  if (!val.isString() || !idVal.isInt32()) {
    return false;
  }

  int32_t index = idVal.toInt32();
  if (index < 0) {
    return false;
  }

  JSString* str = val.toString();
  if (size_t(index) >= str->length()) {
    return false;
  }

  // This follows JSString::getChar, otherwise we fail to attach getChar in a
  // lot of cases.
  if (str->isRope()) {
    JSRope* rope = &str->asRope();

    // Make sure the left side contains the index.
    if (size_t(index) >= rope->leftChild()->length()) {
      return false;
    }

    str = rope->leftChild();
  }

  if (!str->isLinear()) {
    return false;
  }

  return true;
}

AttachDecision GetPropIRGenerator::tryAttachStringChar(ValOperandId valId,
                                                       ValOperandId indexId) {
  MOZ_ASSERT(idVal_.isInt32());

  if (!CanAttachStringChar(val_, idVal_)) {
    return AttachDecision::NoAction;
  }

  StringOperandId strId = writer.guardToString(valId);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);
  writer.loadStringCharResult(strId, int32IndexId);
  writer.returnFromIC();

  trackAttached("StringChar");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachMagicArgumentsName(
    ValOperandId valId, HandleId id) {
  if (!val_.isMagic(JS_OPTIMIZED_ARGUMENTS)) {
    return AttachDecision::NoAction;
  }

  if (!JSID_IS_ATOM(id, cx_->names().length) &&
      !JSID_IS_ATOM(id, cx_->names().callee)) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);
  writer.guardMagicValue(valId, JS_OPTIMIZED_ARGUMENTS);
  writer.guardFrameHasNoArgumentsObject();

  if (JSID_IS_ATOM(id, cx_->names().length)) {
    writer.loadFrameNumActualArgsResult();
    writer.returnFromIC();
  } else {
    MOZ_ASSERT(JSID_IS_ATOM(id, cx_->names().callee));
    writer.loadFrameCalleeResult();
    writer.typeMonitorResult();
  }

  trackAttached("MagicArgumentsName");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachMagicArgument(
    ValOperandId valId, ValOperandId indexId) {
  MOZ_ASSERT(idVal_.isInt32());

  if (!val_.isMagic(JS_OPTIMIZED_ARGUMENTS)) {
    return AttachDecision::NoAction;
  }

  writer.guardMagicValue(valId, JS_OPTIMIZED_ARGUMENTS);
  writer.guardFrameHasNoArgumentsObject();

  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);
  writer.loadFrameArgumentResult(int32IndexId);
  writer.typeMonitorResult();

  trackAttached("MagicArgument");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachArgumentsObjectArg(
    HandleObject obj, ObjOperandId objId, Int32OperandId indexId) {
  if (!obj->is<ArgumentsObject>() ||
      obj->as<ArgumentsObject>().hasOverriddenElement()) {
    return AttachDecision::NoAction;
  }

  if (!(resultFlags_ & GetPropertyResultFlags::Monitored)) {
    return AttachDecision::NoAction;
  }

  if (obj->is<MappedArgumentsObject>()) {
    writer.guardClass(objId, GuardClassKind::MappedArguments);
  } else {
    MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
    writer.guardClass(objId, GuardClassKind::UnmappedArguments);
  }

  writer.loadArgumentsObjectArgResult(objId, indexId);
  writer.typeMonitorResult();

  trackAttached("ArgumentsObjectArg");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachDenseElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (!nobj->containsDenseElement(index)) {
    return AttachDecision::NoAction;
  }

  TestMatchingNativeReceiver(writer, nobj, objId);
  writer.loadDenseElementResult(objId, indexId);
  writer.typeMonitorResult();

  trackAttached("DenseElement");
  return AttachDecision::Attach;
}

static bool CanAttachDenseElementHole(NativeObject* obj, bool ownProp,
                                      bool allowIndexedReceiver = false) {
  // Make sure the objects on the prototype don't have any indexed properties
  // or that such properties can't appear without a shape change.
  // Otherwise returning undefined for holes would obviously be incorrect,
  // because we would have to lookup a property on the prototype instead.
  do {
    // The first two checks are also relevant to the receiver object.
    if (!allowIndexedReceiver && obj->isIndexed()) {
      return false;
    }
    allowIndexedReceiver = false;

    if (ClassCanHaveExtraProperties(obj->getClass())) {
      return false;
    }

    // Don't need to check prototype for OwnProperty checks
    if (ownProp) {
      return true;
    }

    JSObject* proto = obj->staticPrototype();
    if (!proto) {
      break;
    }

    if (!proto->isNative()) {
      return false;
    }

    // Make sure objects on the prototype don't have dense elements.
    if (proto->as<NativeObject>().getDenseInitializedLength() != 0) {
      return false;
    }

    obj = &proto->as<NativeObject>();
  } while (true);

  return true;
}

AttachDecision GetPropIRGenerator::tryAttachDenseElementHole(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (nobj->containsDenseElement(index)) {
    return AttachDecision::NoAction;
  }
  if (!CanAttachDenseElementHole(nobj, false)) {
    return AttachDecision::NoAction;
  }

  // Guard on the shape, to prevent non-dense elements from appearing.
  TestMatchingNativeReceiver(writer, nobj, objId);
  GeneratePrototypeHoleGuards(writer, nobj, objId,
                              /* alwaysGuardFirstProto = */ false);
  writer.loadDenseElementHoleResult(objId, indexId);
  writer.typeMonitorResult();

  trackAttached("DenseElementHole");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachSparseElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }
  NativeObject* nobj = &obj->as<NativeObject>();

  // Stub doesn't handle negative indices.
  if (index > INT_MAX) {
    return AttachDecision::NoAction;
  }

  // We also need to be past the end of the dense capacity, to ensure sparse.
  if (index < nobj->getDenseInitializedLength()) {
    return AttachDecision::NoAction;
  }

  // Only handle Array objects in this stub.
  if (!nobj->is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }

  // Here, we ensure that the prototype chain does not define any sparse
  // indexed properties on the shape lineage. This allows us to guard on
  // the shapes up the prototype chain to ensure that no indexed properties
  // exist outside of the dense elements.
  //
  // The `GeneratePrototypeHoleGuards` call below will guard on the shapes,
  // as well as ensure that no prototypes contain dense elements, allowing
  // us to perform a pure shape-search for out-of-bounds integer-indexed
  // properties on the recevier object.
  if ((nobj->staticPrototype() != nullptr) &&
      ObjectMayHaveExtraIndexedProperties(nobj->staticPrototype())) {
    return AttachDecision::NoAction;
  }

  // Ensure that obj is an Array.
  writer.guardClass(objId, GuardClassKind::Array);

  // The helper we are going to call only applies to non-dense elements.
  writer.guardIndexGreaterThanDenseInitLength(objId, indexId);

  // Ensures we are able to efficiently able to map to an integral jsid.
  writer.guardIndexIsNonNegative(indexId);

  // Shape guard the prototype chain to avoid shadowing indexes from appearing.
  // The helper function also ensures that the index does not appear within the
  // dense element set of the prototypes.
  GeneratePrototypeHoleGuards(writer, nobj, objId,
                              /* alwaysGuardFirstProto = */ true);

  // At this point, we are guaranteed that the indexed property will not
  // be found on one of the prototypes. We are assured that we only have
  // to check that the receiving object has the property.

  writer.callGetSparseElementResult(objId, indexId);
  writer.typeMonitorResult();

  trackAttached("GetSparseElement");
  return AttachDecision::Attach;
}

static bool IsPrimitiveArrayTypedObject(JSObject* obj) {
  if (!obj->is<TypedObject>()) {
    return false;
  }
  TypeDescr& descr = obj->as<TypedObject>().typeDescr();
  return descr.is<ArrayTypeDescr>() &&
         descr.as<ArrayTypeDescr>().elementType().is<ScalarTypeDescr>();
}

static Scalar::Type PrimitiveArrayTypedObjectType(JSObject* obj) {
  MOZ_ASSERT(IsPrimitiveArrayTypedObject(obj));
  TypeDescr& descr = obj->as<TypedObject>().typeDescr();
  return descr.as<ArrayTypeDescr>().elementType().as<ScalarTypeDescr>().type();
}

static Scalar::Type TypedThingElementType(JSObject* obj) {
  return obj->is<TypedArrayObject>() ? obj->as<TypedArrayObject>().type()
                                     : PrimitiveArrayTypedObjectType(obj);
}

// For Uint32Array we let the stub return a double only if the current result is
// a double, to allow better codegen in Warp.
static bool AllowDoubleForUint32Array(TypedArrayObject* tarr, uint32_t index) {
  if (TypedThingElementType(tarr) != Scalar::Type::Uint32) {
    // Return value is only relevant for Uint32Array.
    return false;
  }

  if (index >= tarr->length()) {
    return false;
  }

  Value res;
  MOZ_ALWAYS_TRUE(tarr->getElementPure(index, &res));
  MOZ_ASSERT(res.isNumber());
  return res.isDouble();
}

AttachDecision GetPropIRGenerator::tryAttachTypedElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId) {
  if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj)) {
    return AttachDecision::NoAction;
  }

  // Ensure the index is in-bounds so the element type gets monitored.
  if (obj->is<TypedArrayObject>() &&
      index >= obj->as<TypedArrayObject>().length()) {
    return AttachDecision::NoAction;
  }

  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  if (IsPrimitiveArrayTypedObject(obj)) {
    writer.guardGroupForLayout(objId, obj->group());
  } else {
    writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());
  }

  // Don't handle out-of-bounds accesses here because we have to ensure the
  // |undefined| type is monitored. See also tryAttachTypedArrayNonInt32Index.
  if (layout == TypedThingLayout::TypedArray) {
    TypedArrayObject* tarr = &obj->as<TypedArrayObject>();
    bool allowDoubleForUint32 = AllowDoubleForUint32Array(tarr, index);
    writer.loadTypedArrayElementResult(
        objId, indexId, TypedThingElementType(obj),
        /* handleOOB = */ false, allowDoubleForUint32);
  } else {
    writer.loadTypedObjectElementResult(objId, indexId, layout,
                                        TypedThingElementType(obj));
  }

  // Reading from Uint32Array may produce an int32 now but a double value
  // later, so ensure we monitor the result.
  if (TypedThingElementType(obj) == Scalar::Type::Uint32) {
    writer.typeMonitorResult();
  } else {
    writer.returnFromIC();
  }

  trackAttached("TypedElement");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachTypedArrayNonInt32Index(
    HandleObject obj, ObjOperandId objId) {
  if (!obj->is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }

  if (!idVal_.isNumber()) {
    return AttachDecision::NoAction;
  }

  TypedArrayObject* tarr = &obj->as<TypedArrayObject>();

  // Try to convert the number to a typed array index. Use NumberEqualsInt32
  // because ToPropertyKey(-0) is 0. If the number is not representable as an
  // int32 the result will be |undefined| so we leave |allowDoubleForUint32| as
  // false.
  bool allowDoubleForUint32 = false;
  int32_t indexInt32;
  if (mozilla::NumberEqualsInt32(idVal_.toNumber(), &indexInt32)) {
    uint32_t index = uint32_t(indexInt32);
    allowDoubleForUint32 = AllowDoubleForUint32Array(tarr, index);
  }

  ValOperandId keyId = getElemKeyValueId();
  Int32OperandId indexId = writer.guardToTypedArrayIndex(keyId);

  writer.guardShapeForClass(objId, tarr->shape());

  writer.loadTypedArrayElementResult(objId, indexId, TypedThingElementType(obj),
                                     /* handleOOB = */ true,
                                     allowDoubleForUint32);

  // Always monitor the result when out-of-bounds accesses are expected.
  writer.typeMonitorResult();

  trackAttached("TypedArrayNonInt32Index");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachGenericElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  // To allow other types to attach in the non-megamorphic case we test the
  // specific matching native receiver; however, once megamorphic we can attach
  // for any native
  if (mode_ == ICState::Mode::Megamorphic) {
    writer.guardIsNativeObject(objId);
  } else {
    NativeObject* nobj = &obj->as<NativeObject>();
    TestMatchingNativeReceiver(writer, nobj, objId);
  }
  writer.guardIndexGreaterThanDenseInitLength(objId, indexId);
  writer.callNativeGetElementResult(objId, indexId);
  writer.typeMonitorResult();

  trackAttached(mode_ == ICState::Mode::Megamorphic
                    ? "GenericElementMegamorphic"
                    : "GenericElement");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachProxyElement(HandleObject obj,
                                                         ObjOperandId objId) {
  if (!obj->is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

  // The proxy stubs don't currently support |super| access.
  if (isSuper()) {
    return AttachDecision::NoAction;
  }

  writer.guardIsProxy(objId);

  // We are not guarding against DOM proxies here, because there is no other
  // specialized DOM IC we could attach.
  // We could call maybeEmitIdGuard here and then emit ProxyGetResult,
  // but for GetElem we prefer to attach a stub that can handle any Value
  // so we don't attach a new stub for every id.
  MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
  MOZ_ASSERT(!isSuper());
  writer.proxyGetByValueResult(objId, getElemKeyValueId());
  writer.typeMonitorResult();

  trackAttached("ProxyElement");
  return AttachDecision::Attach;
}

void GetPropIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("base", val_);
    sp.valueProperty("property", idVal_);
  }
#endif
}

void IRGenerator::emitIdGuard(ValOperandId valId, jsid id) {
  if (JSID_IS_SYMBOL(id)) {
    SymbolOperandId symId = writer.guardToSymbol(valId);
    writer.guardSpecificSymbol(symId, JSID_TO_SYMBOL(id));
  } else {
    MOZ_ASSERT(JSID_IS_ATOM(id));
    StringOperandId strId = writer.guardToString(valId);
    writer.guardSpecificAtom(strId, JSID_TO_ATOM(id));
  }
}

void GetPropIRGenerator::maybeEmitIdGuard(jsid id) {
  if (cacheKind_ == CacheKind::GetProp ||
      cacheKind_ == CacheKind::GetPropSuper) {
    // Constant PropertyName, no guards necessary.
    MOZ_ASSERT(&idVal_.toString()->asAtom() == JSID_TO_ATOM(id));
    return;
  }

  MOZ_ASSERT(cacheKind_ == CacheKind::GetElem ||
             cacheKind_ == CacheKind::GetElemSuper);
  emitIdGuard(getElemKeyValueId(), id);
}

void SetPropIRGenerator::maybeEmitIdGuard(jsid id) {
  if (cacheKind_ == CacheKind::SetProp) {
    // Constant PropertyName, no guards necessary.
    MOZ_ASSERT(&idVal_.toString()->asAtom() == JSID_TO_ATOM(id));
    return;
  }

  MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
  emitIdGuard(setElemKeyValueId(), id);
}

GetNameIRGenerator::GetNameIRGenerator(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, ICState::Mode mode,
                                       HandleObject env,
                                       HandlePropertyName name)
    : IRGenerator(cx, script, pc, CacheKind::GetName, mode),
      env_(env),
      name_(name) {}

AttachDecision GetNameIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::GetName);

  AutoAssertNoPendingException aanpe(cx_);

  ObjOperandId envId(writer.setInputOperandId(0));
  RootedId id(cx_, NameToId(name_));

  TRY_ATTACH(tryAttachGlobalNameValue(envId, id));
  TRY_ATTACH(tryAttachGlobalNameGetter(envId, id));
  TRY_ATTACH(tryAttachEnvironmentName(envId, id));

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

bool CanAttachGlobalName(JSContext* cx,
                         Handle<LexicalEnvironmentObject*> globalLexical,
                         HandleId id, MutableHandleNativeObject holder,
                         MutableHandleShape shape) {
  // The property must be found, and it must be found as a normal data property.
  RootedNativeObject current(cx, globalLexical);
  while (true) {
    shape.set(current->lookup(cx, id));
    if (shape) {
      break;
    }

    if (current == globalLexical) {
      current = &globalLexical->global();
    } else {
      // In the browser the global prototype chain should be immutable.
      if (!current->staticPrototypeIsImmutable()) {
        return false;
      }

      JSObject* proto = current->staticPrototype();
      if (!proto || !proto->is<NativeObject>()) {
        return false;
      }

      current = &proto->as<NativeObject>();
    }
  }

  holder.set(current);
  return true;
}

AttachDecision GetNameIRGenerator::tryAttachGlobalNameValue(ObjOperandId objId,
                                                            HandleId id) {
  if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope()) {
    return AttachDecision::NoAction;
  }

  Handle<LexicalEnvironmentObject*> globalLexical =
      env_.as<LexicalEnvironmentObject>();
  MOZ_ASSERT(globalLexical->isGlobal());

  RootedNativeObject holder(cx_);
  RootedShape shape(cx_);
  if (!CanAttachGlobalName(cx_, globalLexical, id, &holder, &shape)) {
    return AttachDecision::NoAction;
  }

  // The property must be found, and it must be found as a normal data property.
  if (!shape->isDataProperty()) {
    return AttachDecision::NoAction;
  }

  // This might still be an uninitialized lexical.
  if (holder->getSlot(shape->slot()).isMagic()) {
    return AttachDecision::NoAction;
  }

  // Instantiate this global property, for use during Ion compilation.
  if (IsIonEnabled(cx_)) {
    EnsureTrackPropertyTypes(cx_, holder, id);
  }

  if (holder == globalLexical) {
    // There is no need to guard on the shape. Lexical bindings are
    // non-configurable, and this stub cannot be shared across globals.
    size_t dynamicSlotOffset =
        holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
    writer.loadDynamicSlotResult(objId, dynamicSlotOffset);
  } else {
    // Check the prototype chain from the global to the holder
    // prototype. Ignore the global lexical scope as it doesn't figure
    // into the prototype chain. We guard on the global lexical
    // scope's shape independently.
    if (!IsCacheableGetPropReadSlot(&globalLexical->global(), holder,
                                    PropertyResult(shape))) {
      return AttachDecision::NoAction;
    }

    // Shape guard for global lexical.
    writer.guardShape(objId, globalLexical->lastProperty());

    // Guard on the shape of the GlobalObject.
    ObjOperandId globalId = writer.loadEnclosingEnvironment(objId);
    writer.guardShape(globalId, globalLexical->global().lastProperty());

    ObjOperandId holderId = globalId;
    if (holder != &globalLexical->global()) {
      // Shape guard holder.
      holderId = writer.loadObject(holder);
      writer.guardShape(holderId, holder->lastProperty());
    }

    EmitLoadSlotResult(writer, holderId, holder, shape);
  }

  writer.typeMonitorResult();

  trackAttached("GlobalNameValue");
  return AttachDecision::Attach;
}

AttachDecision GetNameIRGenerator::tryAttachGlobalNameGetter(ObjOperandId objId,
                                                             HandleId id) {
  if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope()) {
    return AttachDecision::NoAction;
  }

  Handle<LexicalEnvironmentObject*> globalLexical =
      env_.as<LexicalEnvironmentObject>();
  MOZ_ASSERT(globalLexical->isGlobal());

  RootedNativeObject holder(cx_);
  RootedShape shape(cx_);
  if (!CanAttachGlobalName(cx_, globalLexical, id, &holder, &shape)) {
    return AttachDecision::NoAction;
  }

  if (holder == globalLexical) {
    return AttachDecision::NoAction;
  }

  if (IsCacheableGetPropCall(&globalLexical->global(), holder, shape) !=
      CanAttachNativeGetter) {
    return AttachDecision::NoAction;
  }

  if (IsIonEnabled(cx_)) {
    EnsureTrackPropertyTypes(cx_, holder, id);
  }

  // Shape guard for global lexical.
  writer.guardShape(objId, globalLexical->lastProperty());

  // Guard on the shape of the GlobalObject.
  ObjOperandId globalId = writer.loadEnclosingEnvironment(objId);
  writer.guardShape(globalId, globalLexical->global().lastProperty());

  if (holder != &globalLexical->global()) {
    // Shape guard holder.
    ObjOperandId holderId = writer.loadObject(holder);
    writer.guardShape(holderId, holder->lastProperty());
  }

  ValOperandId receiverId = writer.boxObject(globalId);
  EmitCallGetterResultNoGuards(cx_, writer, &globalLexical->global(), holder,
                               shape, receiverId);

  trackAttached("GlobalNameGetter");
  return AttachDecision::Attach;
}

static bool NeedEnvironmentShapeGuard(JSObject* envObj) {
  if (!envObj->is<CallObject>()) {
    return true;
  }

  // We can skip a guard on the call object if the script's bindings are
  // guaranteed to be immutable (and thus cannot introduce shadowing variables).
  // If the function is a relazified self-hosted function it has no BaseScript
  // and we pessimistically create the guard.
  CallObject* callObj = &envObj->as<CallObject>();
  JSFunction* fun = &callObj->callee();
  if (!fun->hasBaseScript() || fun->baseScript()->funHasExtensibleScope()) {
    return true;
  }

  return false;
}

AttachDecision GetNameIRGenerator::tryAttachEnvironmentName(ObjOperandId objId,
                                                            HandleId id) {
  if (IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope()) {
    return AttachDecision::NoAction;
  }

  RootedObject env(cx_, env_);
  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);

  while (env) {
    if (env->is<GlobalObject>()) {
      shape = env->as<GlobalObject>().lookup(cx_, id);
      if (shape) {
        break;
      }
      return AttachDecision::NoAction;
    }

    if (!env->is<EnvironmentObject>() || env->is<WithEnvironmentObject>()) {
      return AttachDecision::NoAction;
    }

    MOZ_ASSERT(!env->hasUncacheableProto());

    // Check for an 'own' property on the env. There is no need to
    // check the prototype as non-with scopes do not inherit properties
    // from any prototype.
    shape = env->as<NativeObject>().lookup(cx_, id);
    if (shape) {
      break;
    }

    env = env->enclosingEnvironment();
  }

  holder = &env->as<NativeObject>();
  if (!IsCacheableGetPropReadSlot(holder, holder, PropertyResult(shape))) {
    return AttachDecision::NoAction;
  }
  if (holder->getSlot(shape->slot()).isMagic()) {
    return AttachDecision::NoAction;
  }

  ObjOperandId lastObjId = objId;
  env = env_;
  while (env) {
    if (NeedEnvironmentShapeGuard(env)) {
      writer.guardShape(lastObjId, env->shape());
    }

    if (env == holder) {
      break;
    }

    lastObjId = writer.loadEnclosingEnvironment(lastObjId);
    env = env->enclosingEnvironment();
  }

  if (holder->isFixedSlot(shape->slot())) {
    writer.loadEnvironmentFixedSlotResult(
        lastObjId, NativeObject::getFixedSlotOffset(shape->slot()));
  } else {
    size_t dynamicSlotOffset =
        holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
    writer.loadEnvironmentDynamicSlotResult(lastObjId, dynamicSlotOffset);
  }
  writer.typeMonitorResult();

  trackAttached("EnvironmentName");
  return AttachDecision::Attach;
}

void GetNameIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("base", ObjectValue(*env_));
    sp.valueProperty("property", StringValue(name_));
  }
#endif
}

BindNameIRGenerator::BindNameIRGenerator(JSContext* cx, HandleScript script,
                                         jsbytecode* pc, ICState::Mode mode,
                                         HandleObject env,
                                         HandlePropertyName name)
    : IRGenerator(cx, script, pc, CacheKind::BindName, mode),
      env_(env),
      name_(name) {}

AttachDecision BindNameIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::BindName);

  AutoAssertNoPendingException aanpe(cx_);

  ObjOperandId envId(writer.setInputOperandId(0));
  RootedId id(cx_, NameToId(name_));

  TRY_ATTACH(tryAttachGlobalName(envId, id));
  TRY_ATTACH(tryAttachEnvironmentName(envId, id));

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision BindNameIRGenerator::tryAttachGlobalName(ObjOperandId objId,
                                                        HandleId id) {
  if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope()) {
    return AttachDecision::NoAction;
  }

  Handle<LexicalEnvironmentObject*> globalLexical =
      env_.as<LexicalEnvironmentObject>();
  MOZ_ASSERT(globalLexical->isGlobal());

  JSObject* result = nullptr;
  if (Shape* shape = globalLexical->lookup(cx_, id)) {
    // If this is an uninitialized lexical or a const, we need to return a
    // RuntimeLexicalErrorObject.
    if (globalLexical->getSlot(shape->slot()).isMagic() || !shape->writable()) {
      return AttachDecision::NoAction;
    }
    result = globalLexical;
  } else {
    result = &globalLexical->global();
  }

  if (result == globalLexical) {
    // Lexical bindings are non-configurable so we can just return the
    // global lexical.
    writer.loadObjectResult(objId);
  } else {
    // If the property exists on the global and is non-configurable, it cannot
    // be shadowed by the lexical scope so we can just return the global without
    // a shape guard.
    Shape* shape = result->as<GlobalObject>().lookup(cx_, id);
    if (!shape || shape->configurable()) {
      writer.guardShape(objId, globalLexical->lastProperty());
    }
    ObjOperandId globalId = writer.loadEnclosingEnvironment(objId);
    writer.loadObjectResult(globalId);
  }
  writer.returnFromIC();

  trackAttached("GlobalName");
  return AttachDecision::Attach;
}

AttachDecision BindNameIRGenerator::tryAttachEnvironmentName(ObjOperandId objId,
                                                             HandleId id) {
  if (IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope()) {
    return AttachDecision::NoAction;
  }

  RootedObject env(cx_, env_);
  RootedShape shape(cx_);
  while (true) {
    if (!env->is<GlobalObject>() && !env->is<EnvironmentObject>()) {
      return AttachDecision::NoAction;
    }
    if (env->is<WithEnvironmentObject>()) {
      return AttachDecision::NoAction;
    }

    MOZ_ASSERT(!env->hasUncacheableProto());

    // When we reach an unqualified variables object (like the global) we
    // have to stop looking and return that object.
    if (env->isUnqualifiedVarObj()) {
      break;
    }

    // Check for an 'own' property on the env. There is no need to
    // check the prototype as non-with scopes do not inherit properties
    // from any prototype.
    shape = env->as<NativeObject>().lookup(cx_, id);
    if (shape) {
      break;
    }

    env = env->enclosingEnvironment();
  }

  // If this is an uninitialized lexical or a const, we need to return a
  // RuntimeLexicalErrorObject.
  RootedNativeObject holder(cx_, &env->as<NativeObject>());
  if (shape && holder->is<EnvironmentObject>() &&
      (holder->getSlot(shape->slot()).isMagic() || !shape->writable())) {
    return AttachDecision::NoAction;
  }

  ObjOperandId lastObjId = objId;
  env = env_;
  while (env) {
    if (NeedEnvironmentShapeGuard(env) && !env->is<GlobalObject>()) {
      writer.guardShape(lastObjId, env->shape());
    }

    if (env == holder) {
      break;
    }

    lastObjId = writer.loadEnclosingEnvironment(lastObjId);
    env = env->enclosingEnvironment();
  }
  writer.loadObjectResult(lastObjId);
  writer.returnFromIC();

  trackAttached("EnvironmentName");
  return AttachDecision::Attach;
}

void BindNameIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("base", ObjectValue(*env_));
    sp.valueProperty("property", StringValue(name_));
  }
#endif
}

HasPropIRGenerator::HasPropIRGenerator(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, ICState::Mode mode,
                                       CacheKind cacheKind, HandleValue idVal,
                                       HandleValue val)
    : IRGenerator(cx, script, pc, cacheKind, mode), val_(val), idVal_(idVal) {}

AttachDecision HasPropIRGenerator::tryAttachDense(HandleObject obj,
                                                  ObjOperandId objId,
                                                  uint32_t index,
                                                  Int32OperandId indexId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (!nobj->containsDenseElement(index)) {
    return AttachDecision::NoAction;
  }

  // Guard shape to ensure object class is NativeObject.
  TestMatchingNativeReceiver(writer, nobj, objId);
  writer.loadDenseElementExistsResult(objId, indexId);
  writer.returnFromIC();

  trackAttached("DenseHasProp");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachDenseHole(HandleObject obj,
                                                      ObjOperandId objId,
                                                      uint32_t index,
                                                      Int32OperandId indexId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (nobj->containsDenseElement(index)) {
    return AttachDecision::NoAction;
  }
  if (!CanAttachDenseElementHole(nobj, hasOwn)) {
    return AttachDecision::NoAction;
  }

  // Guard shape to ensure class is NativeObject and to prevent non-dense
  // elements being added. Also ensures prototype doesn't change if dynamic
  // checks aren't emitted.
  TestMatchingNativeReceiver(writer, nobj, objId);

  // Generate prototype guards if needed. This includes monitoring that
  // properties were not added in the chain.
  if (!hasOwn) {
    GeneratePrototypeHoleGuards(writer, nobj, objId,
                                /* alwaysGuardFirstProto = */ false);
  }

  writer.loadDenseElementHoleExistsResult(objId, indexId);
  writer.returnFromIC();

  trackAttached("DenseHasPropHole");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachSparse(HandleObject obj,
                                                   ObjOperandId objId,
                                                   Int32OperandId indexId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }
  if (!obj->as<NativeObject>().isIndexed()) {
    return AttachDecision::NoAction;
  }
  if (!CanAttachDenseElementHole(&obj->as<NativeObject>(), hasOwn,
                                 /* allowIndexedReceiver = */ true)) {
    return AttachDecision::NoAction;
  }

  // Guard that this is a native object.
  writer.guardIsNativeObject(objId);

  // Generate prototype guards if needed. This includes monitoring that
  // properties were not added in the chain.
  if (!hasOwn) {
    GeneratePrototypeHoleGuards(writer, obj, objId,
                                /* alwaysGuardFirstProto = */ true);
  }

  // Because of the prototype guard we know that the prototype chain
  // does not include any dense or sparse (i.e indexed) properties.
  writer.callObjectHasSparseElementResult(objId, indexId);
  writer.returnFromIC();

  trackAttached("Sparse");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachNamedProp(HandleObject obj,
                                                      ObjOperandId objId,
                                                      HandleId key,
                                                      ValOperandId keyId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  JSObject* holder = nullptr;
  PropertyResult prop;

  if (hasOwn) {
    if (!LookupOwnPropertyPure(cx_, obj, key, &prop)) {
      return AttachDecision::NoAction;
    }

    holder = obj;
  } else {
    if (!LookupPropertyPure(cx_, obj, key, &holder, &prop)) {
      return AttachDecision::NoAction;
    }
  }
  if (!prop) {
    return AttachDecision::NoAction;
  }

  TRY_ATTACH(tryAttachMegamorphic(objId, keyId));
  TRY_ATTACH(tryAttachNative(obj, objId, key, keyId, prop, holder));
  TRY_ATTACH(tryAttachTypedObject(obj, objId, key, keyId));

  return AttachDecision::NoAction;
}

AttachDecision HasPropIRGenerator::tryAttachMegamorphic(ObjOperandId objId,
                                                        ValOperandId keyId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  if (mode_ != ICState::Mode::Megamorphic) {
    return AttachDecision::NoAction;
  }

  writer.megamorphicHasPropResult(objId, keyId, hasOwn);
  writer.returnFromIC();
  trackAttached("MegamorphicHasProp");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachNative(JSObject* obj,
                                                   ObjOperandId objId, jsid key,
                                                   ValOperandId keyId,
                                                   PropertyResult prop,
                                                   JSObject* holder) {
  if (!prop.isNativeProperty()) {
    return AttachDecision::NoAction;
  }

  if (!IsCacheableProtoChain(obj, holder)) {
    return AttachDecision::NoAction;
  }

  Maybe<ObjOperandId> tempId;
  emitIdGuard(keyId, key);
  EmitReadSlotGuard(writer, obj, holder, objId, &tempId);
  writer.loadBooleanResult(true);
  writer.returnFromIC();

  trackAttached("NativeHasProp");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachTypedArray(HandleObject obj,
                                                       ObjOperandId objId,
                                                       Int32OperandId indexId) {
  if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj)) {
    return AttachDecision::NoAction;
  }

  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  if (IsPrimitiveArrayTypedObject(obj)) {
    writer.guardGroupForLayout(objId, obj->group());
  } else {
    writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());
  }

  writer.loadTypedElementExistsResult(objId, indexId, layout);

  writer.returnFromIC();

  trackAttached("TypedArrayObject");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachTypedArrayNonInt32Index(
    HandleObject obj, ObjOperandId objId, ValOperandId keyId) {
  if (!obj->is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }

  if (!idVal_.isNumber()) {
    return AttachDecision::NoAction;
  }

  Int32OperandId indexId = writer.guardToTypedArrayIndex(keyId);

  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());

  writer.loadTypedElementExistsResult(objId, indexId, layout);

  writer.returnFromIC();

  trackAttached("TypedArrayObjectNonInt32Index");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachTypedObject(JSObject* obj,
                                                        ObjOperandId objId,
                                                        jsid key,
                                                        ValOperandId keyId) {
  if (!obj->is<TypedObject>()) {
    return AttachDecision::NoAction;
  }

  if (!obj->as<TypedObject>().typeDescr().hasProperty(cx_->names(), key)) {
    return AttachDecision::NoAction;
  }

  emitIdGuard(keyId, key);
  writer.guardGroupForLayout(objId, obj->group());
  writer.loadBooleanResult(true);
  writer.returnFromIC();

  trackAttached("TypedObjectHasProp");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachSlotDoesNotExist(
    JSObject* obj, ObjOperandId objId, jsid key, ValOperandId keyId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  emitIdGuard(keyId, key);
  if (hasOwn) {
    TestMatchingReceiver(writer, obj, objId);
  } else {
    Maybe<ObjOperandId> tempId;
    EmitReadSlotGuard(writer, obj, nullptr, objId, &tempId);
  }
  writer.loadBooleanResult(false);
  writer.returnFromIC();

  trackAttached("DoesNotExist");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachDoesNotExist(HandleObject obj,
                                                         ObjOperandId objId,
                                                         HandleId key,
                                                         ValOperandId keyId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  // Check that property doesn't exist on |obj| or it's prototype chain. These
  // checks allow Native/Typed objects with a NativeObject prototype
  // chain. They return NoAction if unknown such as resolve hooks or proxies.
  if (hasOwn) {
    if (!CheckHasNoSuchOwnProperty(cx_, obj, key)) {
      return AttachDecision::NoAction;
    }
  } else {
    if (!CheckHasNoSuchProperty(cx_, obj, key)) {
      return AttachDecision::NoAction;
    }
  }

  TRY_ATTACH(tryAttachMegamorphic(objId, keyId));
  TRY_ATTACH(tryAttachSlotDoesNotExist(obj, objId, key, keyId));

  return AttachDecision::NoAction;
}

AttachDecision HasPropIRGenerator::tryAttachProxyElement(HandleObject obj,
                                                         ObjOperandId objId,
                                                         ValOperandId keyId) {
  bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

  if (!obj->is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

  writer.guardIsProxy(objId);
  writer.proxyHasPropResult(objId, keyId, hasOwn);
  writer.returnFromIC();

  trackAttached("ProxyHasProp");
  return AttachDecision::Attach;
}

AttachDecision HasPropIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::In || cacheKind_ == CacheKind::HasOwn);

  AutoAssertNoPendingException aanpe(cx_);

  // NOTE: Argument order is PROPERTY, OBJECT
  ValOperandId keyId(writer.setInputOperandId(0));
  ValOperandId valId(writer.setInputOperandId(1));

  if (!val_.isObject()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }
  RootedObject obj(cx_, &val_.toObject());
  ObjOperandId objId = writer.guardToObject(valId);

  // Optimize Proxies
  TRY_ATTACH(tryAttachProxyElement(obj, objId, keyId));

  RootedId id(cx_);
  bool nameOrSymbol;
  if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  if (nameOrSymbol) {
    TRY_ATTACH(tryAttachNamedProp(obj, objId, id, keyId));
    TRY_ATTACH(tryAttachDoesNotExist(obj, objId, id, keyId));

    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  uint32_t index;
  Int32OperandId indexId;
  if (maybeGuardInt32Index(idVal_, keyId, &index, &indexId)) {
    TRY_ATTACH(tryAttachDense(obj, objId, index, indexId));
    TRY_ATTACH(tryAttachDenseHole(obj, objId, index, indexId));
    TRY_ATTACH(tryAttachTypedArray(obj, objId, indexId));
    TRY_ATTACH(tryAttachSparse(obj, objId, indexId));

    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  TRY_ATTACH(tryAttachTypedArrayNonInt32Index(obj, objId, keyId));

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

void HasPropIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("base", val_);
    sp.valueProperty("property", idVal_);
  }
#endif
}

CheckPrivateFieldIRGenerator::CheckPrivateFieldIRGenerator(
    JSContext* cx, HandleScript script, jsbytecode* pc, ICState::Mode mode,
    CacheKind cacheKind, HandleValue idVal, HandleValue val)
    : IRGenerator(cx, script, pc, cacheKind, mode), val_(val), idVal_(idVal) {
  MOZ_ASSERT(idVal.isSymbol() && idVal.toSymbol()->isPrivateName());
}

AttachDecision CheckPrivateFieldIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);

  ValOperandId valId(writer.setInputOperandId(0));
  ValOperandId keyId(writer.setInputOperandId(1));

  if (!val_.isObject()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }
  RootedObject obj(cx_, &val_.toObject());
  ObjOperandId objId = writer.guardToObject(valId);
  RootedId key(cx_, SYMBOL_TO_JSID(idVal_.toSymbol()));

  ThrowCondition condition;
  ThrowMsgKind msgKind;
  GetCheckPrivateFieldOperands(pc_, &condition, &msgKind);

  bool hasOwn = false;
  if (!HasOwnDataPropertyPure(cx_, obj, key, &hasOwn)) {
    // Can't determine if HasOwnProperty purely.
    return AttachDecision::NoAction;
  }

  if (CheckPrivateFieldWillThrow(condition, hasOwn)) {
    // Don't attach a stub if the operation will throw.
    return AttachDecision::NoAction;
  }

  TRY_ATTACH(tryAttachNative(obj, objId, key, keyId, hasOwn));

  return AttachDecision::NoAction;
}

AttachDecision CheckPrivateFieldIRGenerator::tryAttachNative(JSObject* obj,
                                                             ObjOperandId objId,
                                                             jsid key,
                                                             ValOperandId keyId,
                                                             bool hasOwn) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  Maybe<ObjOperandId> tempId;
  emitIdGuard(keyId, key);
  EmitReadSlotGuard(writer, obj, obj, objId, &tempId);
  writer.loadBooleanResult(hasOwn);
  writer.returnFromIC();

  trackAttached("NativeCheckPrivateField");
  return AttachDecision::Attach;
}

void CheckPrivateFieldIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("base", val_);
    sp.valueProperty("property", idVal_);
  }
#endif
}

bool IRGenerator::maybeGuardInt32Index(const Value& index, ValOperandId indexId,
                                       uint32_t* int32Index,
                                       Int32OperandId* int32IndexId) {
  if (index.isNumber()) {
    int32_t indexSigned;
    if (index.isInt32()) {
      indexSigned = index.toInt32();
    } else {
      // We allow negative zero here.
      if (!mozilla::NumberEqualsInt32(index.toDouble(), &indexSigned)) {
        return false;
      }
    }

    if (indexSigned < 0) {
      return false;
    }

    *int32Index = uint32_t(indexSigned);
    *int32IndexId = writer.guardToInt32Index(indexId);
    return true;
  }

  if (index.isString()) {
    int32_t indexSigned = GetIndexFromString(index.toString());
    if (indexSigned < 0) {
      return false;
    }

    StringOperandId strId = writer.guardToString(indexId);
    *int32Index = uint32_t(indexSigned);
    *int32IndexId = writer.guardStringToIndex(strId);
    return true;
  }

  return false;
}

SetPropIRGenerator::SetPropIRGenerator(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, CacheKind cacheKind,
                                       ICState::Mode mode, HandleValue lhsVal,
                                       HandleValue idVal, HandleValue rhsVal,
                                       bool needsTypeBarrier,
                                       bool maybeHasExtraIndexedProps)
    : IRGenerator(cx, script, pc, cacheKind, mode),
      lhsVal_(lhsVal),
      idVal_(idVal),
      rhsVal_(rhsVal),
      typeCheckInfo_(cx, needsTypeBarrier),
      preliminaryObjectAction_(PreliminaryObjectAction::None),
      attachedTypedArrayOOBStub_(false),
      maybeHasExtraIndexedProps_(maybeHasExtraIndexedProps) {}

AttachDecision SetPropIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);

  ValOperandId objValId(writer.setInputOperandId(0));
  ValOperandId rhsValId;
  if (cacheKind_ == CacheKind::SetProp) {
    rhsValId = ValOperandId(writer.setInputOperandId(1));
  } else {
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    MOZ_ASSERT(setElemKeyValueId().id() == 1);
    writer.setInputOperandId(1);
    rhsValId = ValOperandId(writer.setInputOperandId(2));
  }

  RootedId id(cx_);
  bool nameOrSymbol;
  if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  if (lhsVal_.isObject()) {
    RootedObject obj(cx_, &lhsVal_.toObject());

    ObjOperandId objId = writer.guardToObject(objValId);
    if (IsPropertySetOp(JSOp(*pc_))) {
      TRY_ATTACH(tryAttachMegamorphicSetElement(obj, objId, rhsValId));
    }
    if (nameOrSymbol) {
      TRY_ATTACH(tryAttachNativeSetSlot(obj, objId, id, rhsValId));
      TRY_ATTACH(tryAttachTypedObjectProperty(obj, objId, id, rhsValId));
      if (IsPropertySetOp(JSOp(*pc_))) {
        TRY_ATTACH(tryAttachSetArrayLength(obj, objId, id, rhsValId));
        TRY_ATTACH(tryAttachSetter(obj, objId, id, rhsValId));
        TRY_ATTACH(tryAttachWindowProxy(obj, objId, id, rhsValId));
        TRY_ATTACH(tryAttachProxy(obj, objId, id, rhsValId));
      }
      if (canAttachAddSlotStub(obj, id)) {
        deferType_ = DeferType::AddSlot;
        return AttachDecision::Deferred;
      }
      return AttachDecision::NoAction;
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);

    if (IsPropertySetOp(JSOp(*pc_))) {
      TRY_ATTACH(tryAttachProxyElement(obj, objId, rhsValId));
    }

    uint32_t index;
    Int32OperandId indexId;
    if (maybeGuardInt32Index(idVal_, setElemKeyValueId(), &index, &indexId)) {
      TRY_ATTACH(
          tryAttachSetDenseElement(obj, objId, index, indexId, rhsValId));
      TRY_ATTACH(
          tryAttachSetDenseElementHole(obj, objId, index, indexId, rhsValId));
      TRY_ATTACH(
          tryAttachSetTypedElement(obj, objId, index, indexId, rhsValId));
      TRY_ATTACH(tryAttachAddOrUpdateSparseElement(obj, objId, index, indexId,
                                                   rhsValId));
      return AttachDecision::NoAction;
    }

    TRY_ATTACH(
        tryAttachSetTypedArrayElementNonInt32Index(obj, objId, rhsValId));
  }
  return AttachDecision::NoAction;
}

static void EmitStoreSlotAndReturn(CacheIRWriter& writer, ObjOperandId objId,
                                   NativeObject* nobj, Shape* shape,
                                   ValOperandId rhsId) {
  if (nobj->isFixedSlot(shape->slot())) {
    size_t offset = NativeObject::getFixedSlotOffset(shape->slot());
    writer.storeFixedSlot(objId, offset, rhsId);
  } else {
    size_t offset = nobj->dynamicSlotIndex(shape->slot()) * sizeof(Value);
    writer.storeDynamicSlot(objId, offset, rhsId);
  }
  writer.returnFromIC();
}

static Shape* LookupShapeForSetSlot(JSOp op, NativeObject* obj, jsid id) {
  Shape* shape = obj->lookupPure(id);
  if (!shape || !shape->isDataProperty() || !shape->writable()) {
    return nullptr;
  }

  // If this is an op like JSOp::InitElem / [[DefineOwnProperty]], the
  // property's attributes may have to be changed too, so make sure it's a
  // simple data property.
  if (IsPropertyInitOp(op) &&
      (!shape->configurable() || !shape->enumerable())) {
    return nullptr;
  }

  return shape;
}

static bool CanAttachNativeSetSlot(JSContext* cx, JSOp op, HandleObject obj,
                                   HandleId id,
                                   bool* isTemporarilyUnoptimizable,
                                   MutableHandleShape propShape) {
  if (!obj->isNative()) {
    return false;
  }

  propShape.set(LookupShapeForSetSlot(op, &obj->as<NativeObject>(), id));
  if (!propShape) {
    return false;
  }

  ObjectGroup* group = JSObject::getGroup(cx, obj);
  if (!group) {
    cx->recoverFromOutOfMemory();
    return false;
  }

  // For some property writes, such as the initial overwrite of global
  // properties, TI will not mark the property as having been
  // overwritten. Don't attach a stub in this case, so that we don't
  // execute another write to the property without TI seeing that write.
  EnsureTrackPropertyTypes(cx, obj, id);
  if (!PropertyHasBeenMarkedNonConstant(obj, id)) {
    *isTemporarilyUnoptimizable = true;
    return false;
  }

  return true;
}

AttachDecision SetPropIRGenerator::tryAttachNativeSetSlot(HandleObject obj,
                                                          ObjOperandId objId,
                                                          HandleId id,
                                                          ValOperandId rhsId) {
  RootedShape propShape(cx_);
  bool isTemporarilyUnoptimizable = false;
  if (!CanAttachNativeSetSlot(cx_, JSOp(*pc_), obj, id,
                              &isTemporarilyUnoptimizable, &propShape)) {
    return isTemporarilyUnoptimizable ? AttachDecision::TemporarilyUnoptimizable
                                      : AttachDecision::NoAction;
  }

  // Don't attach a megamorphic store slot stub for ops like JSOp::InitElem.
  if (mode_ == ICState::Mode::Megamorphic && cacheKind_ == CacheKind::SetProp &&
      IsPropertySetOp(JSOp(*pc_))) {
    writer.megamorphicStoreSlot(objId, JSID_TO_ATOM(id)->asPropertyName(),
                                rhsId, typeCheckInfo_.needsTypeBarrier());
    writer.returnFromIC();
    trackAttached("MegamorphicNativeSlot");
    return AttachDecision::Attach;
  }

  maybeEmitIdGuard(id);

  // If we need a property type barrier (always in Baseline, sometimes in
  // Ion), guard on both the shape and the group. If Ion knows the property
  // types match, we don't need the group guard.
  NativeObject* nobj = &obj->as<NativeObject>();
  if (typeCheckInfo_.needsTypeBarrier()) {
    writer.guardGroupForTypeBarrier(objId, nobj->group());
  }
  TestMatchingNativeReceiver(writer, nobj, objId);

  if (IsPreliminaryObject(obj)) {
    preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
  } else {
    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
  }

  typeCheckInfo_.set(nobj->group(), id);
  EmitStoreSlotAndReturn(writer, objId, nobj, propShape, rhsId);

  trackAttached("NativeSlot");
  return AttachDecision::Attach;
}

OperandId IRGenerator::emitNumericGuard(ValOperandId valId, Scalar::Type type) {
  switch (type) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
      return writer.guardToInt32ModUint32(valId);

    case Scalar::Float32:
    case Scalar::Float64:
      return writer.guardIsNumber(valId);

    case Scalar::Uint8Clamped:
      return writer.guardToUint8Clamped(valId);

    case Scalar::BigInt64:
    case Scalar::BigUint64:
      return writer.guardToBigInt(valId);

    case Scalar::MaxTypedArrayViewType:
    case Scalar::Int64:
    case Scalar::Simd128:
      break;
  }
  MOZ_CRASH("Unsupported TypedArray type");
}

static bool ValueIsNumeric(Scalar::Type type, const Value& val) {
  if (Scalar::isBigIntType(type)) {
    return val.isBigInt();
  }
  return val.isNumber();
}

AttachDecision SetPropIRGenerator::tryAttachTypedObjectProperty(
    HandleObject obj, ObjOperandId objId, HandleId id, ValOperandId rhsId) {
  if (!obj->is<TypedObject>()) {
    return AttachDecision::NoAction;
  }

  if (!obj->as<TypedObject>().typeDescr().is<StructTypeDescr>()) {
    return AttachDecision::NoAction;
  }

  StructTypeDescr* structDescr =
      &obj->as<TypedObject>().typeDescr().as<StructTypeDescr>();
  size_t fieldIndex;
  if (!structDescr->fieldIndex(id, &fieldIndex)) {
    return AttachDecision::NoAction;
  }

  TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
  if (!fieldDescr->is<SimpleTypeDescr>()) {
    return AttachDecision::NoAction;
  }

  if (fieldDescr->is<ReferenceTypeDescr>() &&
      fieldDescr->as<ReferenceTypeDescr>().type() ==
          ReferenceType::TYPE_WASM_ANYREF) {
    // TODO/AnyRef-boxing: we can probably do better, in particular, code
    // that stores object pointers and null in an anyref slot should be able
    // to get a fast path.
    return AttachDecision::NoAction;
  }

  if (fieldDescr->is<ScalarTypeDescr>()) {
    Scalar::Type type = fieldDescr->as<ScalarTypeDescr>().type();
    if (!ValueIsNumeric(type, rhsVal_)) {
      return AttachDecision::NoAction;
    }
  }

  uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  maybeEmitIdGuard(id);
  writer.guardGroupForLayout(objId, obj->group());

  typeCheckInfo_.set(obj->group(), id);

  // Scalar types can always be stored without a type update stub.
  if (fieldDescr->is<ScalarTypeDescr>()) {
    Scalar::Type type = fieldDescr->as<ScalarTypeDescr>().type();
    OperandId rhsValId = emitNumericGuard(rhsId, type);

    writer.storeTypedObjectScalarProperty(objId, fieldOffset, layout, type,
                                          rhsValId);
    writer.returnFromIC();

    trackAttached("TypedObject");
    return AttachDecision::Attach;
  }

  // For reference types, guard on the RHS type first, so that
  // StoreTypedObjectReferenceProperty is infallible.
  ReferenceType type = fieldDescr->as<ReferenceTypeDescr>().type();
  switch (type) {
    case ReferenceType::TYPE_ANY:
      break;
    case ReferenceType::TYPE_OBJECT:
      writer.guardIsObjectOrNull(rhsId);
      break;
    case ReferenceType::TYPE_STRING:
      writer.guardNonDoubleType(rhsId, ValueType::String);
      break;
    case ReferenceType::TYPE_WASM_ANYREF:
      MOZ_CRASH();
  }

  writer.storeTypedObjectReferenceProperty(objId, fieldOffset, layout, type,
                                           rhsId);
  writer.returnFromIC();

  trackAttached("TypedObject");
  return AttachDecision::Attach;
}

void SetPropIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.opcodeProperty("op", JSOp(*pc_));
    sp.valueProperty("base", lhsVal_);
    sp.valueProperty("property", idVal_);
    sp.valueProperty("value", rhsVal_);
  }
#endif
}

static bool IsCacheableSetPropCallNative(JSObject* obj, JSObject* holder,
                                         Shape* shape) {
  if (!shape || !IsCacheableProtoChain(obj, holder)) {
    return false;
  }

  if (!shape->hasSetterValue()) {
    return false;
  }

  if (!shape->setterObject() || !shape->setterObject()->is<JSFunction>()) {
    return false;
  }

  JSFunction& setter = shape->setterObject()->as<JSFunction>();
  if (!setter.isNativeWithoutJitEntry()) {
    return false;
  }

  if (setter.isClassConstructor()) {
    return false;
  }

  if (setter.hasJitInfo() && !setter.jitInfo()->needsOuterizedThisObject()) {
    return true;
  }

  return !IsWindow(obj);
}

static bool IsCacheableSetPropCallScripted(JSObject* obj, JSObject* holder,
                                           Shape* shape) {
  if (!shape || !IsCacheableProtoChain(obj, holder)) {
    return false;
  }

  if (IsWindow(obj)) {
    return false;
  }

  if (!shape->hasSetterValue()) {
    return false;
  }

  if (!shape->setterObject() || !shape->setterObject()->is<JSFunction>()) {
    return false;
  }

  JSFunction& setter = shape->setterObject()->as<JSFunction>();
  if (setter.isClassConstructor()) {
    return false;
  }

  // Scripted functions and natives with JIT entry can use the scripted path.
  return setter.hasJitEntry();
}

static bool CanAttachSetter(JSContext* cx, jsbytecode* pc, HandleObject obj,
                            HandleId id, MutableHandleObject holder,
                            MutableHandleShape propShape) {
  // Don't attach a setter stub for ops like JSOp::InitElem.
  MOZ_ASSERT(IsPropertySetOp(JSOp(*pc)));

  PropertyResult prop;
  if (!LookupPropertyPure(cx, obj, id, holder.address(), &prop)) {
    return false;
  }

  if (prop.isNonNativeProperty()) {
    return false;
  }

  propShape.set(prop.maybeShape());
  if (!IsCacheableSetPropCallScripted(obj, holder, propShape) &&
      !IsCacheableSetPropCallNative(obj, holder, propShape)) {
    return false;
  }

  return true;
}

static void EmitCallSetterNoGuards(JSContext* cx, CacheIRWriter& writer,
                                   JSObject* obj, JSObject* holder,
                                   Shape* shape, ObjOperandId objId,
                                   ValOperandId rhsId) {
  JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
  bool sameRealm = cx->realm() == target->realm();

  if (target->isNativeWithoutJitEntry()) {
    MOZ_ASSERT(IsCacheableSetPropCallNative(obj, holder, shape));
    writer.callNativeSetter(objId, target, rhsId, sameRealm);
    writer.returnFromIC();
    return;
  }

  MOZ_ASSERT(IsCacheableSetPropCallScripted(obj, holder, shape));
  writer.callScriptedSetter(objId, target, rhsId, sameRealm);
  writer.returnFromIC();
}

AttachDecision SetPropIRGenerator::tryAttachSetter(HandleObject obj,
                                                   ObjOperandId objId,
                                                   HandleId id,
                                                   ValOperandId rhsId) {
  RootedObject holder(cx_);
  RootedShape propShape(cx_);
  if (!CanAttachSetter(cx_, pc_, obj, id, &holder, &propShape)) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);

  // Use the megamorphic guard if we're in megamorphic mode, except if |obj|
  // is a Window as GuardHasGetterSetter doesn't support this yet (Window may
  // require outerizing).
  if (mode_ == ICState::Mode::Specialized || IsWindow(obj)) {
    TestMatchingReceiver(writer, obj, objId);

    if (obj != holder) {
      GeneratePrototypeGuards(writer, obj, holder, objId);

      // Guard on the holder's shape.
      ObjOperandId holderId = writer.loadObject(holder);
      TestMatchingHolder(writer, holder, holderId);
    }
  } else {
    writer.guardHasGetterSetter(objId, propShape);
  }

  EmitCallSetterNoGuards(cx_, writer, obj, holder, propShape, objId, rhsId);

  trackAttached("Setter");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachSetArrayLength(HandleObject obj,
                                                           ObjOperandId objId,
                                                           HandleId id,
                                                           ValOperandId rhsId) {
  // Don't attach an array length stub for ops like JSOp::InitElem.
  MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

  if (!obj->is<ArrayObject>() || !JSID_IS_ATOM(id, cx_->names().length) ||
      !obj->as<ArrayObject>().lengthIsWritable()) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);
  writer.guardClass(objId, GuardClassKind::Array);
  writer.callSetArrayLength(objId, IsStrictSetPC(pc_), rhsId);
  writer.returnFromIC();

  trackAttached("SetArrayLength");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachSetDenseElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId, ValOperandId rhsId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (!nobj->containsDenseElement(index) || nobj->denseElementsAreFrozen()) {
    return AttachDecision::NoAction;
  }

  // Setting holes requires extra code for marking the elements non-packed.
  MOZ_ASSERT(!rhsVal_.isMagic(JS_ELEMENTS_HOLE));

  // Don't optimize InitElem (DefineProperty) on non-extensible objects: when
  // the elements are sealed, we have to throw an exception. Note that we have
  // to check !isExtensible instead of denseElementsAreSealed because sealing
  // a (non-extensible) object does not necessarily trigger a Shape change.
  if (IsPropertyInitOp(JSOp(*pc_)) && !nobj->isExtensible()) {
    return AttachDecision::NoAction;
  }

  if (typeCheckInfo_.needsTypeBarrier()) {
    writer.guardGroupForTypeBarrier(objId, nobj->group());
  }
  TestMatchingNativeReceiver(writer, nobj, objId);

  writer.storeDenseElement(objId, indexId, rhsId);
  writer.returnFromIC();

  // Type inference uses JSID_VOID for the element types.
  typeCheckInfo_.set(nobj->group(), JSID_VOID);

  trackAttached("SetDenseElement");
  return AttachDecision::Attach;
}

static bool CanAttachAddElement(NativeObject* obj, bool isInit) {
  // Make sure the objects on the prototype don't have any indexed properties
  // or that such properties can't appear without a shape change.
  do {
    // The first two checks are also relevant to the receiver object.
    if (obj->isIndexed()) {
      return false;
    }

    const JSClass* clasp = obj->getClass();
    if (clasp != &ArrayObject::class_ &&
        (clasp->getAddProperty() || clasp->getResolve() ||
         clasp->getOpsLookupProperty() || clasp->getOpsSetProperty())) {
      return false;
    }

    // If we're initializing a property instead of setting one, the objects
    // on the prototype are not relevant.
    if (isInit) {
      break;
    }

    JSObject* proto = obj->staticPrototype();
    if (!proto) {
      break;
    }

    if (!proto->isNative()) {
      return false;
    }

    // We have to make sure the proto has no non-writable (frozen) elements
    // because we're not allowed to shadow them.
    NativeObject* nproto = &proto->as<NativeObject>();
    if (nproto->denseElementsAreFrozen() &&
        nproto->getDenseInitializedLength() > 0) {
      return false;
    }

    obj = nproto;
  } while (true);

  return true;
}

AttachDecision SetPropIRGenerator::tryAttachSetDenseElementHole(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId, ValOperandId rhsId) {
  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  // Setting holes requires extra code for marking the elements non-packed.
  if (rhsVal_.isMagic(JS_ELEMENTS_HOLE)) {
    return AttachDecision::NoAction;
  }

  JSOp op = JSOp(*pc_);
  MOZ_ASSERT(IsPropertySetOp(op) || IsPropertyInitOp(op));

  if (op == JSOp::InitHiddenElem) {
    return AttachDecision::NoAction;
  }

  NativeObject* nobj = &obj->as<NativeObject>();
  if (!nobj->isExtensible()) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(!nobj->denseElementsAreFrozen(),
             "Extensible objects should not have frozen elements");

  uint32_t initLength = nobj->getDenseInitializedLength();

  // Optimize if we're adding an element at initLength or writing to a hole.
  //
  // In the case where index > initLength, we need noteHasDenseAdd to be called
  // to ensure Ion is aware that writes have occurred to-out-of-bound indexes
  // before.
  bool isAdd = index == initLength;
  bool isHoleInBounds =
      index < initLength && !nobj->containsDenseElement(index);
  if (!isAdd && !isHoleInBounds) {
    return AttachDecision::NoAction;
  }

  // Can't add new elements to arrays with non-writable length.
  if (isAdd && nobj->is<ArrayObject>() &&
      !nobj->as<ArrayObject>().lengthIsWritable()) {
    return AttachDecision::NoAction;
  }

  // Typed arrays don't have dense elements.
  if (nobj->is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }

  // Check for other indexed properties or class hooks.
  if (!CanAttachAddElement(nobj, IsPropertyInitOp(op))) {
    return AttachDecision::NoAction;
  }

  if (typeCheckInfo_.needsTypeBarrier()) {
    writer.guardGroupForTypeBarrier(objId, nobj->group());
  }
  TestMatchingNativeReceiver(writer, nobj, objId);

  // Also shape guard the proto chain, unless this is an InitElem or we know
  // the proto chain has no indexed props.
  if (IsPropertySetOp(op) && maybeHasExtraIndexedProps_) {
    ShapeGuardProtoChain(writer, obj, objId);
  }

  writer.storeDenseElementHole(objId, indexId, rhsId, isAdd);
  writer.returnFromIC();

  // Type inference uses JSID_VOID for the element types.
  typeCheckInfo_.set(nobj->group(), JSID_VOID);

  trackAttached(isAdd ? "AddDenseElement" : "StoreDenseElementHole");
  return AttachDecision::Attach;
}

// Add an IC for adding or updating a sparse array element.
AttachDecision SetPropIRGenerator::tryAttachAddOrUpdateSparseElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId, ValOperandId rhsId) {
  JSOp op = JSOp(*pc_);
  MOZ_ASSERT(IsPropertySetOp(op) || IsPropertyInitOp(op));

  if (op != JSOp::SetElem && op != JSOp::StrictSetElem) {
    return AttachDecision::NoAction;
  }

  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }
  NativeObject* nobj = &obj->as<NativeObject>();

  // We cannot attach a stub to a non-extensible object
  if (!nobj->isExtensible()) {
    return AttachDecision::NoAction;
  }

  // Stub doesn't handle negative indices.
  if (index > INT_MAX) {
    return AttachDecision::NoAction;
  }

  // We also need to be past the end of the dense capacity, to ensure sparse.
  if (index < nobj->getDenseInitializedLength()) {
    return AttachDecision::NoAction;
  }

  // Only handle Array objects in this stub.
  if (!nobj->is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }
  ArrayObject* aobj = &nobj->as<ArrayObject>();

  // Don't attach if we're adding to an array with non-writable length.
  bool isAdd = (index >= aobj->length());
  if (isAdd && !aobj->lengthIsWritable()) {
    return AttachDecision::NoAction;
  }

  // Indexed properties on the prototype chain aren't handled by the helper.
  if ((aobj->staticPrototype() != nullptr) &&
      ObjectMayHaveExtraIndexedProperties(aobj->staticPrototype())) {
    return AttachDecision::NoAction;
  }

  // Ensure we are still talking about an array class.
  writer.guardClass(objId, GuardClassKind::Array);

  // The helper we are going to call only applies to non-dense elements.
  writer.guardIndexGreaterThanDenseInitLength(objId, indexId);

  // Guard extensible: We may be trying to add a new element, and so we'd best
  // be able to do so safely.
  writer.guardIsExtensible(objId);

  // Ensures we are able to efficiently able to map to an integral jsid.
  writer.guardIndexIsNonNegative(indexId);

  // Shape guard the prototype chain to avoid shadowing indexes from appearing.
  // Guard the prototype of the receiver explicitly, because the receiver's
  // shape is not being guarded as a proxy for that.
  GuardGroupProto(writer, obj, objId);

  // Dense elements may appear on the prototype chain (and prototypes may
  // have a different notion of which elements are dense), but they can
  // only be data properties, so our specialized Set handler is ok to bind
  // to them.
  ShapeGuardProtoChain(writer, obj, objId);

  // Ensure that if we're adding an element to the object, the object's
  // length is writable.
  writer.guardIndexIsValidUpdateOrAdd(objId, indexId);

  writer.callAddOrUpdateSparseElementHelper(
      objId, indexId, rhsId,
      /* strict = */ op == JSOp::StrictSetElem);
  writer.returnFromIC();

  trackAttached("AddOrUpdateSparseElement");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachSetTypedElement(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId, ValOperandId rhsId) {
  if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj)) {
    return AttachDecision::NoAction;
  }

  bool handleOutOfBounds = false;
  if (obj->is<TypedArrayObject>()) {
    handleOutOfBounds = (index >= obj->as<TypedArrayObject>().length());
  } else {
    // Typed objects throw on out of bounds accesses. Don't attach
    // a stub in this case.
    if (index >= obj->as<TypedObject>().length()) {
      return AttachDecision::NoAction;
    }
  }

  Scalar::Type elementType = TypedThingElementType(obj);
  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  // Don't attach if the input type doesn't match the guard added below.
  if (!ValueIsNumeric(elementType, rhsVal_)) {
    return AttachDecision::NoAction;
  }

  if (IsPrimitiveArrayTypedObject(obj)) {
    writer.guardGroupForLayout(objId, obj->group());
  } else {
    writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());
  }

  OperandId rhsValId = emitNumericGuard(rhsId, elementType);

  if (layout == TypedThingLayout::TypedArray) {
    writer.storeTypedArrayElement(objId, elementType, indexId, rhsValId,
                                  handleOutOfBounds);
  } else {
    MOZ_ASSERT(!handleOutOfBounds);
    writer.storeTypedObjectElement(objId, layout, elementType, indexId,
                                   rhsValId);
  }
  writer.returnFromIC();

  if (handleOutOfBounds) {
    attachedTypedArrayOOBStub_ = true;
  }

  trackAttached(handleOutOfBounds ? "SetTypedElementOOB" : "SetTypedElement");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachSetTypedArrayElementNonInt32Index(
    HandleObject obj, ObjOperandId objId, ValOperandId rhsId) {
  if (!obj->is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }

  if (!idVal_.isNumber()) {
    return AttachDecision::NoAction;
  }

  Scalar::Type elementType = TypedThingElementType(obj);

  // Don't attach if the input type doesn't match the guard added below.
  if (!ValueIsNumeric(elementType, rhsVal_)) {
    return AttachDecision::NoAction;
  }

  ValOperandId keyId = setElemKeyValueId();
  Int32OperandId indexId = writer.guardToTypedArrayIndex(keyId);

  writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());

  OperandId rhsValId = emitNumericGuard(rhsId, elementType);

  // When the index isn't an int32 index, we always assume the TypedArray access
  // can be out-of-bounds.
  bool handleOutOfBounds = true;

  writer.storeTypedArrayElement(objId, elementType, indexId, rhsValId,
                                handleOutOfBounds);
  writer.returnFromIC();

  attachedTypedArrayOOBStub_ = true;

  trackAttached("SetTypedElementNonInt32Index");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachGenericProxy(
    HandleObject obj, ObjOperandId objId, HandleId id, ValOperandId rhsId,
    bool handleDOMProxies) {
  MOZ_ASSERT(obj->is<ProxyObject>());

  writer.guardIsProxy(objId);

  if (!handleDOMProxies) {
    // Ensure that the incoming object is not a DOM proxy, so that we can
    // get to the specialized stubs. If handleDOMProxies is true, we were
    // unable to attach a specialized DOM stub, so we just handle all
    // proxies here.
    writer.guardIsNotDOMProxy(objId);
  }

  if (cacheKind_ == CacheKind::SetProp || mode_ == ICState::Mode::Specialized) {
    maybeEmitIdGuard(id);
    writer.proxySet(objId, id, rhsId, IsStrictSetPC(pc_));
  } else {
    // Attach a stub that handles every id.
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);
    writer.proxySetByValue(objId, setElemKeyValueId(), rhsId,
                           IsStrictSetPC(pc_));
  }

  writer.returnFromIC();

  trackAttached("GenericProxy");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachDOMProxyShadowed(
    HandleObject obj, ObjOperandId objId, HandleId id, ValOperandId rhsId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  maybeEmitIdGuard(id);
  TestMatchingProxyReceiver(writer, &obj->as<ProxyObject>(), objId);
  writer.proxySet(objId, id, rhsId, IsStrictSetPC(pc_));
  writer.returnFromIC();

  trackAttached("DOMProxyShadowed");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachDOMProxyUnshadowed(
    HandleObject obj, ObjOperandId objId, HandleId id, ValOperandId rhsId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  RootedObject proto(cx_, obj->staticPrototype());
  if (!proto) {
    return AttachDecision::NoAction;
  }

  RootedObject holder(cx_);
  RootedShape propShape(cx_);
  if (!CanAttachSetter(cx_, pc_, proto, id, &holder, &propShape)) {
    return AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);

  // Guard that our expando object hasn't started shadowing this property.
  TestMatchingProxyReceiver(writer, &obj->as<ProxyObject>(), objId);
  CheckDOMProxyExpandoDoesNotShadow(writer, obj, id, objId);

  GeneratePrototypeGuards(writer, obj, holder, objId);

  // Guard on the holder of the property.
  ObjOperandId holderId = writer.loadObject(holder);
  TestMatchingHolder(writer, holder, holderId);

  // EmitCallSetterNoGuards expects |obj| to be the object the property is
  // on to do some checks. Since we actually looked at proto, and no extra
  // guards will be generated, we can just pass that instead.
  EmitCallSetterNoGuards(cx_, writer, proto, holder, propShape, objId, rhsId);

  trackAttached("DOMProxyUnshadowed");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachDOMProxyExpando(
    HandleObject obj, ObjOperandId objId, HandleId id, ValOperandId rhsId) {
  MOZ_ASSERT(IsCacheableDOMProxy(obj));

  RootedValue expandoVal(cx_, GetProxyPrivate(obj));
  RootedObject expandoObj(cx_);
  if (expandoVal.isObject()) {
    expandoObj = &expandoVal.toObject();
  } else {
    MOZ_ASSERT(!expandoVal.isUndefined(),
               "How did a missing expando manage to shadow things?");
    auto expandoAndGeneration =
        static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
    MOZ_ASSERT(expandoAndGeneration);
    expandoObj = &expandoAndGeneration->expando.toObject();
  }

  bool isTemporarilyUnoptimizable = false;

  RootedShape propShape(cx_);
  if (CanAttachNativeSetSlot(cx_, JSOp(*pc_), expandoObj, id,
                             &isTemporarilyUnoptimizable, &propShape)) {
    maybeEmitIdGuard(id);
    ObjOperandId expandoObjId =
        guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

    NativeObject* nativeExpandoObj = &expandoObj->as<NativeObject>();
    writer.guardGroupForTypeBarrier(expandoObjId, nativeExpandoObj->group());
    typeCheckInfo_.set(nativeExpandoObj->group(), id);

    EmitStoreSlotAndReturn(writer, expandoObjId, nativeExpandoObj, propShape,
                           rhsId);
    trackAttached("DOMProxyExpandoSlot");
    return AttachDecision::Attach;
  }

  RootedObject holder(cx_);
  if (CanAttachSetter(cx_, pc_, expandoObj, id, &holder, &propShape)) {
    // Note that we don't actually use the expandoObjId here after the
    // shape guard. The DOM proxy (objId) is passed to the setter as
    // |this|.
    maybeEmitIdGuard(id);
    guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

    MOZ_ASSERT(holder == expandoObj);
    EmitCallSetterNoGuards(cx_, writer, expandoObj, expandoObj, propShape,
                           objId, rhsId);
    trackAttached("DOMProxyExpandoSetter");
    return AttachDecision::Attach;
  }

  return isTemporarilyUnoptimizable ? AttachDecision::TemporarilyUnoptimizable
                                    : AttachDecision::NoAction;
}

AttachDecision SetPropIRGenerator::tryAttachProxy(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id,
                                                  ValOperandId rhsId) {
  // Don't attach a proxy stub for ops like JSOp::InitElem.
  MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

  ProxyStubType type = GetProxyStubType(cx_, obj, id);
  if (type == ProxyStubType::None) {
    return AttachDecision::NoAction;
  }

  if (mode_ == ICState::Mode::Megamorphic) {
    return tryAttachGenericProxy(obj, objId, id, rhsId,
                                 /* handleDOMProxies = */ true);
  }

  switch (type) {
    case ProxyStubType::None:
      break;
    case ProxyStubType::DOMExpando:
      TRY_ATTACH(tryAttachDOMProxyExpando(obj, objId, id, rhsId));
      [[fallthrough]];  // Fall through to the generic shadowed case.
    case ProxyStubType::DOMShadowed:
      return tryAttachDOMProxyShadowed(obj, objId, id, rhsId);
    case ProxyStubType::DOMUnshadowed:
      TRY_ATTACH(tryAttachDOMProxyUnshadowed(obj, objId, id, rhsId));
      return tryAttachGenericProxy(obj, objId, id, rhsId,
                                   /* handleDOMProxies = */ true);
    case ProxyStubType::Generic:
      return tryAttachGenericProxy(obj, objId, id, rhsId,
                                   /* handleDOMProxies = */ false);
  }

  MOZ_CRASH("Unexpected ProxyStubType");
}

AttachDecision SetPropIRGenerator::tryAttachProxyElement(HandleObject obj,
                                                         ObjOperandId objId,
                                                         ValOperandId rhsId) {
  // Don't attach a proxy stub for ops like JSOp::InitElem.
  MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

  if (!obj->is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

  writer.guardIsProxy(objId);

  // Like GetPropIRGenerator::tryAttachProxyElement, don't check for DOM
  // proxies here as we don't have specialized DOM stubs for this.
  MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
  writer.proxySetByValue(objId, setElemKeyValueId(), rhsId, IsStrictSetPC(pc_));
  writer.returnFromIC();

  trackAttached("ProxyElement");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachMegamorphicSetElement(
    HandleObject obj, ObjOperandId objId, ValOperandId rhsId) {
  MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

  if (mode_ != ICState::Mode::Megamorphic || cacheKind_ != CacheKind::SetElem) {
    return AttachDecision::NoAction;
  }

  // The generic proxy stubs are faster.
  if (obj->is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

  writer.megamorphicSetElement(objId, setElemKeyValueId(), rhsId,
                               IsStrictSetPC(pc_));
  writer.returnFromIC();

  trackAttached("MegamorphicSetElement");
  return AttachDecision::Attach;
}

AttachDecision SetPropIRGenerator::tryAttachWindowProxy(HandleObject obj,
                                                        ObjOperandId objId,
                                                        HandleId id,
                                                        ValOperandId rhsId) {
  // Attach a stub when the receiver is a WindowProxy and we can do the set
  // on the Window (the global object).

  if (!IsWindowProxyForScriptGlobal(script_, obj)) {
    return AttachDecision::NoAction;
  }

  // If we're megamorphic prefer a generic proxy stub that handles a lot more
  // cases.
  if (mode_ == ICState::Mode::Megamorphic) {
    return AttachDecision::NoAction;
  }

  // Now try to do the set on the Window (the current global).
  Handle<GlobalObject*> windowObj = cx_->global();

  RootedShape propShape(cx_);
  bool isTemporarilyUnoptimizable = false;
  if (!CanAttachNativeSetSlot(cx_, JSOp(*pc_), windowObj, id,
                              &isTemporarilyUnoptimizable, &propShape)) {
    return isTemporarilyUnoptimizable ? AttachDecision::TemporarilyUnoptimizable
                                      : AttachDecision::NoAction;
  }

  maybeEmitIdGuard(id);

  ObjOperandId windowObjId =
      GuardAndLoadWindowProxyWindow(writer, objId, windowObj);
  writer.guardShape(windowObjId, windowObj->lastProperty());
  writer.guardGroupForTypeBarrier(windowObjId, windowObj->group());
  typeCheckInfo_.set(windowObj->group(), id);

  EmitStoreSlotAndReturn(writer, windowObjId, windowObj, propShape, rhsId);

  trackAttached("WindowProxySlot");
  return AttachDecision::Attach;
}

bool SetPropIRGenerator::canAttachAddSlotStub(HandleObject obj, HandleId id) {
  // Special-case JSFunction resolve hook to allow redefining the 'prototype'
  // property without triggering lazy expansion of property and object
  // allocation.
  if (obj->is<JSFunction>() && JSID_IS_ATOM(id, cx_->names().prototype)) {
    MOZ_ASSERT(ClassMayResolveId(cx_->names(), obj->getClass(), id, obj));

    // We're only interested in functions that have a builtin .prototype
    // property (needsPrototypeProperty). The stub will guard on this because
    // the builtin .prototype property is non-configurable/non-enumerable and it
    // would be wrong to add a property with those attributes to a function that
    // doesn't have a builtin .prototype.
    //
    // Inlining needsPrototypeProperty in JIT code is complicated so we use
    // isNonBuiltinConstructor as a stronger condition that's easier to check
    // from JIT code.
    JSFunction* fun = &obj->as<JSFunction>();
    if (!fun->isNonBuiltinConstructor()) {
      return false;
    }
    MOZ_ASSERT(fun->needsPrototypeProperty());

    // If property exists this isn't an "add".
    if (fun->lookupPure(id)) {
      return false;
    }
  } else {
    // Normal Case: If property exists this isn't an "add"
    PropertyResult prop;
    if (!LookupOwnPropertyPure(cx_, obj, id, &prop)) {
      return false;
    }
    if (prop) {
      return false;
    }
  }

  // Object must be extensible, or we must be initializing a private
  // elem.
  bool canAddNewProperty = obj->nonProxyIsExtensible() || id.isPrivateName();
  if (!canAddNewProperty) {
    return false;
  }

  // Also watch out for addProperty hooks. Ignore the Array addProperty hook,
  // because it doesn't do anything for non-index properties.
  DebugOnly<uint32_t> index;
  MOZ_ASSERT_IF(obj->is<ArrayObject>(), !IdIsIndex(id, &index));
  if (!obj->is<ArrayObject>() && obj->getClass()->getAddProperty()) {
    return false;
  }

  // Walk up the object prototype chain and ensure that all prototypes are
  // native, and that all prototypes have no setter defined on the property.
  for (JSObject* proto = obj->staticPrototype(); proto;
       proto = proto->staticPrototype()) {
    if (!proto->isNative()) {
      return false;
    }

    // If prototype defines this property in a non-plain way, don't optimize.
    Shape* protoShape = proto->as<NativeObject>().lookup(cx_, id);
    if (protoShape && !protoShape->isDataDescriptor()) {
      return false;
    }

    // Otherwise, if there's no such property, watch out for a resolve hook
    // that would need to be invoked and thus prevent inlining of property
    // addition. Allow the JSFunction resolve hook as it only defines plain
    // data properties and we don't need to invoke it for objects on the
    // proto chain.
    if (ClassMayResolveId(cx_->names(), proto->getClass(), id, proto) &&
        !proto->is<JSFunction>()) {
      return false;
    }
  }

  return true;
}

AttachDecision SetPropIRGenerator::tryAttachAddSlotStub(
    HandleObjectGroup oldGroup, HandleShape oldShape) {
  ValOperandId objValId(writer.setInputOperandId(0));
  ValOperandId rhsValId;
  if (cacheKind_ == CacheKind::SetProp) {
    rhsValId = ValOperandId(writer.setInputOperandId(1));
  } else {
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    MOZ_ASSERT(setElemKeyValueId().id() == 1);
    writer.setInputOperandId(1);
    rhsValId = ValOperandId(writer.setInputOperandId(2));
  }

  RootedId id(cx_);
  bool nameOrSymbol;
  if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  if (!lhsVal_.isObject() || !nameOrSymbol) {
    return AttachDecision::NoAction;
  }

  RootedObject obj(cx_, &lhsVal_.toObject());

  PropertyResult prop;
  if (!LookupOwnPropertyPure(cx_, obj, id, &prop)) {
    return AttachDecision::NoAction;
  }
  if (!prop) {
    return AttachDecision::NoAction;
  }

  if (!obj->isNative()) {
    return AttachDecision::NoAction;
  }

  Shape* propShape = prop.shape();
  NativeObject* holder = &obj->as<NativeObject>();

  MOZ_ASSERT(propShape);

  // The property must be the last added property of the object.
  MOZ_RELEASE_ASSERT(holder->lastProperty() == propShape);

  // Old shape should be parent of new shape. Object flag updates may make this
  // false even for simple data properties. It may be possible to support these
  // transitions in the future, but ignore now for simplicity.
  if (propShape->previous() != oldShape) {
    return AttachDecision::NoAction;
  }

  // Basic shape checks.
  if (propShape->inDictionary() || !propShape->isDataProperty() ||
      !propShape->writable()) {
    return AttachDecision::NoAction;
  }

  ObjOperandId objId = writer.guardToObject(objValId);
  maybeEmitIdGuard(id);

  if (IsTypeInferenceEnabled()) {
    // Guard on the group for the property type barrier and group change.
    writer.guardGroup(objId, oldGroup);
  }

  // If we are adding a property to an object for which the new script
  // properties analysis hasn't been performed yet, make sure the stub fails
  // after we run the analysis as a group change may be required here. The
  // group change is not required for correctness but improves type
  // information elsewhere.
  AutoSweepObjectGroup sweep(oldGroup);
  if (oldGroup->newScript(sweep) && !oldGroup->newScript(sweep)->analyzed()) {
    writer.guardGroupHasUnanalyzedNewScript(oldGroup);
    MOZ_ASSERT(IsPreliminaryObject(obj));
    preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
  } else {
    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
  }

  // Shape guard the object.
  writer.guardShape(objId, oldShape);

  // If this is the special function.prototype case, we need to guard the
  // function is a non-builtin constructor. See canAttachAddSlotStub.
  if (obj->is<JSFunction>() && JSID_IS_ATOM(id, cx_->names().prototype)) {
    MOZ_ASSERT(obj->as<JSFunction>().isNonBuiltinConstructor());
    writer.guardFunctionIsNonBuiltinCtor(objId);
  }

  ShapeGuardProtoChain(writer, obj, objId);

  ObjectGroup* newGroup = obj->group();

  // Check if we have to change the object's group. We only have to change from
  // a partially to fully initialized group if the object is a PlainObject.
  bool changeGroup = oldGroup != newGroup;
  MOZ_ASSERT_IF(changeGroup, obj->is<PlainObject>());

  if (holder->isFixedSlot(propShape->slot())) {
    size_t offset = NativeObject::getFixedSlotOffset(propShape->slot());
    writer.addAndStoreFixedSlot(objId, offset, rhsValId, changeGroup, newGroup,
                                propShape);
    trackAttached("AddSlot");
  } else {
    size_t offset = holder->dynamicSlotIndex(propShape->slot()) * sizeof(Value);
    uint32_t numOldSlots = NativeObject::calculateDynamicSlots(oldShape);
    uint32_t numNewSlots = holder->numDynamicSlots();
    if (numOldSlots == numNewSlots) {
      writer.addAndStoreDynamicSlot(objId, offset, rhsValId, changeGroup,
                                    newGroup, propShape);
      trackAttached("AddSlot");
    } else {
      MOZ_ASSERT(numNewSlots > numOldSlots);
      writer.allocateAndStoreDynamicSlot(objId, offset, rhsValId, changeGroup,
                                         newGroup, propShape, numNewSlots);
      trackAttached("AllocateSlot");
    }
  }
  writer.returnFromIC();

  typeCheckInfo_.set(oldGroup, id);
  return AttachDecision::Attach;
}

InstanceOfIRGenerator::InstanceOfIRGenerator(JSContext* cx, HandleScript script,
                                             jsbytecode* pc, ICState::Mode mode,
                                             HandleValue lhs, HandleObject rhs)
    : IRGenerator(cx, script, pc, CacheKind::InstanceOf, mode),
      lhsVal_(lhs),
      rhsObj_(rhs) {}

AttachDecision InstanceOfIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::InstanceOf);
  AutoAssertNoPendingException aanpe(cx_);

  // Ensure RHS is a function -- could be a Proxy, which the IC isn't prepared
  // to handle.
  if (!rhsObj_->is<JSFunction>()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  HandleFunction fun = rhsObj_.as<JSFunction>();

  if (fun->isBoundFunction()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  // Look up the @@hasInstance property, and check that Function.__proto__ is
  // the property holder, and that no object further down the prototype chain
  // (including this function) has shadowed it; together with the fact that
  // Function.__proto__[@@hasInstance] is immutable, this ensures that the
  // hasInstance hook will not change without the need to guard on the actual
  // property value.
  PropertyResult hasInstanceProp;
  JSObject* hasInstanceHolder = nullptr;
  jsid hasInstanceID = SYMBOL_TO_JSID(cx_->wellKnownSymbols().hasInstance);
  if (!LookupPropertyPure(cx_, fun, hasInstanceID, &hasInstanceHolder,
                          &hasInstanceProp) ||
      !hasInstanceProp.isFound() || hasInstanceProp.isNonNativeProperty()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  Value funProto = cx_->global()->getPrototype(JSProto_Function);
  if (hasInstanceHolder != &funProto.toObject()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  // If the above succeeded, then these should be true about @@hasInstance,
  // because the property on Function.__proto__ is an immutable data property:
  MOZ_ASSERT(hasInstanceProp.shape()->isDataProperty());
  MOZ_ASSERT(!hasInstanceProp.shape()->configurable());
  MOZ_ASSERT(!hasInstanceProp.shape()->writable());

  if (!IsCacheableProtoChain(fun, hasInstanceHolder)) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  // Ensure that the function's prototype slot is the same.
  Shape* shape = fun->lookupPure(cx_->names().prototype);
  if (!shape || !shape->isDataProperty()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  uint32_t slot = shape->slot();

  MOZ_ASSERT(fun->numFixedSlots() == 0, "Stub code relies on this");
  if (!fun->getSlot(slot).isObject()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  JSObject* prototypeObject = &fun->getSlot(slot).toObject();

  // Abstract Objects
  ValOperandId lhs(writer.setInputOperandId(0));
  ValOperandId rhs(writer.setInputOperandId(1));

  ObjOperandId rhsId = writer.guardToObject(rhs);
  writer.guardShape(rhsId, fun->lastProperty());

  // Ensure that the shapes up the prototype chain for the RHS remain the same
  // so that @@hasInstance is not shadowed by some intermediate prototype
  // object.
  if (hasInstanceHolder != fun) {
    GeneratePrototypeGuards(writer, fun, hasInstanceHolder, rhsId);
    ObjOperandId holderId = writer.loadObject(hasInstanceHolder);
    TestMatchingHolder(writer, hasInstanceHolder, holderId);
  }

  // Load prototypeObject into the cache -- consumed twice in the IC
  ObjOperandId protoId = writer.loadObject(prototypeObject);
  // Ensure that rhs[slot] == prototypeObject.
  writer.guardDynamicSlotIsSpecificObject(rhsId, protoId, slot);

  // Needn't guard LHS is object, because the actual stub can handle that
  // and correctly return false.
  writer.loadInstanceOfObjectResult(lhs, protoId);
  writer.returnFromIC();
  trackAttached("InstanceOf");
  return AttachDecision::Attach;
}

void InstanceOfIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("lhs", lhsVal_);
    sp.valueProperty("rhs", ObjectValue(*rhsObj_));
  }
#else
  // Silence Clang -Wunused-private-field warning.
  mozilla::Unused << lhsVal_;
#endif
}

TypeOfIRGenerator::TypeOfIRGenerator(JSContext* cx, HandleScript script,
                                     jsbytecode* pc, ICState::Mode mode,
                                     HandleValue value)
    : IRGenerator(cx, script, pc, CacheKind::TypeOf, mode), val_(value) {}

void TypeOfIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

AttachDecision TypeOfIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::TypeOf);

  AutoAssertNoPendingException aanpe(cx_);

  ValOperandId valId(writer.setInputOperandId(0));

  TRY_ATTACH(tryAttachPrimitive(valId));
  TRY_ATTACH(tryAttachObject(valId));

  MOZ_ASSERT_UNREACHABLE("Failed to attach TypeOf");
  return AttachDecision::NoAction;
}

AttachDecision TypeOfIRGenerator::tryAttachPrimitive(ValOperandId valId) {
  if (!val_.isPrimitive()) {
    return AttachDecision::NoAction;
  }

  if (val_.isNumber()) {
    writer.guardIsNumber(valId);
  } else {
    writer.guardNonDoubleType(valId, val_.type());
  }

  writer.loadConstantStringResult(
      TypeName(js::TypeOfValue(val_), cx_->names()));
  writer.returnFromIC();
  trackAttached("Primitive");
  return AttachDecision::Attach;
}

AttachDecision TypeOfIRGenerator::tryAttachObject(ValOperandId valId) {
  if (!val_.isObject()) {
    return AttachDecision::NoAction;
  }

  ObjOperandId objId = writer.guardToObject(valId);
  writer.loadTypeOfObjectResult(objId);
  writer.returnFromIC();
  trackAttached("Object");
  return AttachDecision::Attach;
}

GetIteratorIRGenerator::GetIteratorIRGenerator(JSContext* cx,
                                               HandleScript script,
                                               jsbytecode* pc,
                                               ICState::Mode mode,
                                               HandleValue value)
    : IRGenerator(cx, script, pc, CacheKind::GetIterator, mode), val_(value) {}

AttachDecision GetIteratorIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::GetIterator);

  AutoAssertNoPendingException aanpe(cx_);

  if (mode_ == ICState::Mode::Megamorphic) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  if (!val_.isObject()) {
    return AttachDecision::NoAction;
  }

  RootedObject obj(cx_, &val_.toObject());

  ObjOperandId objId = writer.guardToObject(valId);
  TRY_ATTACH(tryAttachNativeIterator(objId, obj));

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision GetIteratorIRGenerator::tryAttachNativeIterator(
    ObjOperandId objId, HandleObject obj) {
  MOZ_ASSERT(JSOp(*pc_) == JSOp::Iter);

  PropertyIteratorObject* iterobj = LookupInIteratorCache(cx_, obj);
  if (!iterobj) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(obj->isNative());

  // Guard on the receiver's shape.
  TestMatchingNativeReceiver(writer, &obj->as<NativeObject>(), objId);

  // Ensure the receiver has no dense elements.
  writer.guardNoDenseElements(objId);

  // Do the same for the objects on the proto chain.
  GeneratePrototypeHoleGuards(writer, obj, objId,
                              /* alwaysGuardFirstProto = */ false);

  ObjOperandId iterId = writer.guardAndGetIterator(
      objId, iterobj, &ObjectRealm::get(obj).enumerators);
  writer.loadObjectResult(iterId);
  writer.returnFromIC();

  trackAttached("GetIterator");
  return AttachDecision::Attach;
}

void GetIteratorIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

OptimizeSpreadCallIRGenerator::OptimizeSpreadCallIRGenerator(
    JSContext* cx, HandleScript script, jsbytecode* pc, ICState::Mode mode,
    HandleValue value)
    : IRGenerator(cx, script, pc, CacheKind::OptimizeSpreadCall, mode),
      val_(value) {}

AttachDecision OptimizeSpreadCallIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::OptimizeSpreadCall);

  AutoAssertNoPendingException aanpe(cx_);

  TRY_ATTACH(tryAttachArray());
  TRY_ATTACH(tryAttachNotOptimizable());

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

static bool IsArrayPrototypeOptimizable(JSContext* cx, ArrayObject* arr,
                                        NativeObject** arrProto, uint32_t* slot,
                                        JSFunction** iterFun) {
  // Prototype must be Array.prototype.
  auto* proto = cx->global()->maybeGetArrayPrototype();
  if (!proto || arr->staticPrototype() != proto) {
    return false;
  }
  *arrProto = proto;

  // The object must not have an own @@iterator property.
  JS::PropertyKey iteratorKey = SYMBOL_TO_JSID(cx->wellKnownSymbols().iterator);
  if (arr->lookupPure(iteratorKey)) {
    return false;
  }

  // Ensure that Array.prototype's @@iterator slot is unchanged.
  Shape* shape = proto->lookupPure(iteratorKey);
  if (!shape || !shape->isDataProperty()) {
    return false;
  }

  *slot = shape->slot();
  MOZ_ASSERT(proto->numFixedSlots() == 0, "Stub code relies on this");

  const Value& iterVal = proto->getSlot(*slot);
  if (!iterVal.isObject() || !iterVal.toObject().is<JSFunction>()) {
    return false;
  }

  *iterFun = &iterVal.toObject().as<JSFunction>();
  return IsSelfHostedFunctionWithName(*iterFun, cx->names().ArrayValues);
}

static bool IsArrayIteratorPrototypeOptimizable(JSContext* cx,
                                                NativeObject** arrIterProto,
                                                uint32_t* slot,
                                                JSFunction** nextFun) {
  auto* proto = cx->global()->maybeGetArrayIteratorPrototype();
  if (!proto) {
    return false;
  }
  *arrIterProto = proto;

  // Ensure that %ArrayIteratorPrototype%'s "next" slot is unchanged.
  Shape* shape = proto->lookupPure(cx->names().next);
  if (!shape || !shape->isDataProperty()) {
    return false;
  }

  *slot = shape->slot();
  MOZ_ASSERT(proto->numFixedSlots() == 0, "Stub code relies on this");

  const Value& nextVal = proto->getSlot(*slot);
  if (!nextVal.isObject() || !nextVal.toObject().is<JSFunction>()) {
    return false;
  }

  *nextFun = &nextVal.toObject().as<JSFunction>();
  return IsSelfHostedFunctionWithName(*nextFun, cx->names().ArrayIteratorNext);
}

AttachDecision OptimizeSpreadCallIRGenerator::tryAttachArray() {
  // The value must be a packed array.
  if (!val_.isObject()) {
    return AttachDecision::NoAction;
  }
  JSObject* obj = &val_.toObject();
  if (!IsPackedArray(obj)) {
    return AttachDecision::NoAction;
  }

  // Prototype must be Array.prototype and Array.prototype[@@iterator] must not
  // be modified.
  NativeObject* arrProto;
  uint32_t arrProtoIterSlot;
  JSFunction* iterFun;
  if (!IsArrayPrototypeOptimizable(cx_, &obj->as<ArrayObject>(), &arrProto,
                                   &arrProtoIterSlot, &iterFun)) {
    return AttachDecision::NoAction;
  }

  // %ArrayIteratorPrototype%.next must not be modified.
  NativeObject* arrayIteratorProto;
  uint32_t iterNextSlot;
  JSFunction* nextFun;
  if (!IsArrayIteratorPrototypeOptimizable(cx_, &arrayIteratorProto,
                                           &iterNextSlot, &nextFun)) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  ObjOperandId objId = writer.guardToObject(valId);

  // Guard the object is a packed array with Array.prototype as proto.
  writer.guardShape(objId, obj->as<ArrayObject>().lastProperty());
  writer.guardArrayIsPacked(objId);
  if (obj->hasUncacheableProto()) {
    writer.guardProto(objId, arrProto);
  }

  // Guard on Array.prototype[@@iterator].
  ObjOperandId arrProtoId = writer.loadObject(arrProto);
  ObjOperandId iterId = writer.loadObject(iterFun);
  writer.guardShape(arrProtoId, arrProto->lastProperty());
  writer.guardDynamicSlotIsSpecificObject(arrProtoId, iterId, arrProtoIterSlot);

  // Guard on %ArrayIteratorPrototype%.next.
  ObjOperandId iterProtoId = writer.loadObject(arrayIteratorProto);
  ObjOperandId nextId = writer.loadObject(nextFun);
  writer.guardShape(iterProtoId, arrayIteratorProto->lastProperty());
  writer.guardDynamicSlotIsSpecificObject(iterProtoId, nextId, iterNextSlot);

  writer.loadBooleanResult(true);
  writer.returnFromIC();

  trackAttached("Array");
  return AttachDecision::Attach;
}

AttachDecision OptimizeSpreadCallIRGenerator::tryAttachNotOptimizable() {
  ValOperandId valId(writer.setInputOperandId(0));

  writer.loadBooleanResult(false);
  writer.returnFromIC();

  trackAttached("NotOptimizable");
  return AttachDecision::Attach;
}

void OptimizeSpreadCallIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

CallIRGenerator::CallIRGenerator(JSContext* cx, HandleScript script,
                                 jsbytecode* pc, JSOp op, ICState::Mode mode,
                                 bool isFirstStub, uint32_t argc,
                                 HandleValue callee, HandleValue thisval,
                                 HandleValue newTarget, HandleValueArray args)
    : IRGenerator(cx, script, pc, CacheKind::Call, mode),
      op_(op),
      isFirstStub_(isFirstStub),
      argc_(argc),
      callee_(callee),
      thisval_(thisval),
      newTarget_(newTarget),
      args_(args),
      typeCheckInfo_(cx, /* needsTypeBarrier = */ true),
      cacheIRStubKind_(BaselineCacheIRStubKind::Regular) {}

void CallIRGenerator::emitNativeCalleeGuard(HandleFunction callee) {
  // Note: we rely on GuardSpecificFunction to also guard against the same
  // native from a different realm.
  MOZ_ASSERT(callee->isNativeWithoutJitEntry());

  bool isConstructing = IsConstructPC(pc_);
  CallFlags flags(isConstructing, IsSpreadPC(pc_));

  ValOperandId calleeValId =
      writer.loadArgumentFixedSlot(ArgumentKind::Callee, argc_, flags);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificFunction(calleeObjId, callee);

  // If we're constructing we also need to guard newTarget == callee.
  if (isConstructing) {
    MOZ_ASSERT(&newTarget_.toObject() == callee);
    ValOperandId newTargetValId =
        writer.loadArgumentFixedSlot(ArgumentKind::NewTarget, argc_, flags);
    ObjOperandId newTargetObjId = writer.guardToObject(newTargetValId);
    writer.guardSpecificFunction(newTargetObjId, callee);
  }
}

void CallIRGenerator::emitCalleeGuard(ObjOperandId calleeId,
                                      HandleFunction callee) {
  // Guarding on the callee JSFunction* is most efficient, but doesn't work well
  // for lambda clones (multiple functions with the same BaseScript). We guard
  // on the function's BaseScript if the callee is scripted and this isn't the
  // first IC stub.
  if (!JitOptions.warpBuilder || isFirstStub_ || !callee->hasBaseScript() ||
      callee->isSelfHostedBuiltin()) {
    writer.guardSpecificFunction(calleeId, callee);
  } else {
    writer.guardClass(calleeId, GuardClassKind::JSFunction);
    writer.guardFunctionScript(calleeId, callee->baseScript());
  }
}

AttachDecision CallIRGenerator::tryAttachArrayPush(HandleFunction callee) {
  // Only optimize on obj.push(val);
  if (argc_ != 1 || !thisval_.isObject()) {
    return AttachDecision::NoAction;
  }

  // Where |obj| is a native array.
  JSObject* thisobj = &thisval_.toObject();
  if (!thisobj->is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }

  if (thisobj->hasLazyGroup()) {
    return AttachDecision::NoAction;
  }

  auto* thisarray = &thisobj->as<ArrayObject>();

  // Check for other indexed properties or class hooks.
  if (!CanAttachAddElement(thisarray, /* isInit = */ false)) {
    return AttachDecision::NoAction;
  }

  // Can't add new elements to arrays with non-writable length.
  if (!thisarray->lengthIsWritable()) {
    return AttachDecision::NoAction;
  }

  // Check that array is extensible.
  if (!thisarray->isExtensible()) {
    return AttachDecision::NoAction;
  }

  // Check that the array is completely initialized (no holes).
  if (thisarray->getDenseInitializedLength() != thisarray->length()) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(!thisarray->denseElementsAreFrozen(),
             "Extensible arrays should not have frozen elements");

  // After this point, we can generate code fine.

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'push' native function.
  emitNativeCalleeGuard(callee);

  // Guard this is an array object.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);

  // This is a soft assert, documenting the fact that we pass 'true'
  // for needsTypeBarrier when constructing typeCheckInfo_ for CallIRGenerator.
  // Can be removed safely if the assumption becomes false.
  MOZ_ASSERT_IF(IsTypeInferenceEnabled(), typeCheckInfo_.needsTypeBarrier());

  // Guard that the group and shape matches.
  if (typeCheckInfo_.needsTypeBarrier()) {
    writer.guardGroupForTypeBarrier(thisObjId, thisobj->group());
  }
  TestMatchingNativeReceiver(writer, thisarray, thisObjId);

  // Guard proto chain shapes.
  ShapeGuardProtoChain(writer, thisobj, thisObjId);

  // arr.push(x) is equivalent to arr[arr.length] = x for regular arrays.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  writer.arrayPush(thisObjId, argId);

  writer.returnFromIC();

  // Set the type-check info, and the stub kind to Updated
  typeCheckInfo_.set(thisobj->group(), JSID_VOID);

  cacheIRStubKind_ = BaselineCacheIRStubKind::Updated;

  trackAttached("ArrayPush");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayPopShift(HandleFunction callee,
                                                       InlinableNative native) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Only optimize if |this| is a packed array.
  if (!thisval_.isObject() || !IsPackedArray(&thisval_.toObject())) {
    return AttachDecision::NoAction;
  }

  // Other conditions:
  //
  // * The array length needs to be writable because we're changing it.
  // * The elements must not be copy-on-write because we're deleting an element.
  // * The array must be extensible. Non-extensible arrays require preserving
  //   the |initializedLength == capacity| invariant on ObjectElements.
  //   See NativeObject::shrinkCapacityToInitializedLength.
  //   This also ensures the elements aren't sealed/frozen.
  // * There must not be a for-in iterator for the elements because the IC stub
  //   does not suppress deleted properties.
  ArrayObject* arr = &thisval_.toObject().as<ArrayObject>();
  if (!arr->lengthIsWritable() || arr->denseElementsAreCopyOnWrite() ||
      !arr->isExtensible() || arr->denseElementsHaveMaybeInIterationFlag()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'pop' or 'shift' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId objId = writer.guardToObject(thisValId);
  writer.guardClass(objId, GuardClassKind::Array);

  if (native == InlinableNative::ArrayPop) {
    writer.packedArrayPopResult(objId);
  } else {
    MOZ_ASSERT(native == InlinableNative::ArrayShift);
    writer.packedArrayShiftResult(objId);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("ArrayPopShift");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayJoin(HandleFunction callee) {
  // Only handle argc <= 1.
  if (argc_ > 1) {
    return AttachDecision::NoAction;
  }

  // Only optimize if |this| is an array.
  if (!thisval_.isObject() || !thisval_.toObject().is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }

  // The separator argument must be a string, if present.
  if (argc_ > 0 && !args_[0].isString()) {
    return AttachDecision::NoAction;
  }

  // IC stub code can handle non-packed array.

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'join' native function.
  emitNativeCalleeGuard(callee);

  // Guard this is an array object.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);
  writer.guardClass(thisObjId, GuardClassKind::Array);

  StringOperandId sepId;
  if (argc_ == 1) {
    // If argcount is 1, guard that the argument is a string.
    ValOperandId argValId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
    sepId = writer.guardToString(argValId);
  } else {
    sepId = writer.loadConstantString(cx_->names().comma);
  }

  // Do the join.
  writer.arrayJoinResult(thisObjId, sepId);

  writer.returnFromIC();

  // The result of this stub does not need to be monitored because it will
  // always return a string.  We will add String to the stack typeset when
  // attaching this stub.

  // Set the stub kind to Regular
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ArrayJoin");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArraySlice(HandleFunction callee) {
  // Only handle argc <= 2.
  if (argc_ > 2) {
    return AttachDecision::NoAction;
  }

  // Only optimize if |this| is a packed array.
  if (!thisval_.isObject() || !IsPackedArray(&thisval_.toObject())) {
    return AttachDecision::NoAction;
  }

  // Arguments for the sliced region must be integers.
  if (argc_ > 0 && !args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }
  if (argc_ > 1 && !args_[1].isInt32()) {
    return AttachDecision::NoAction;
  }

  RootedArrayObject arr(cx_, &thisval_.toObject().as<ArrayObject>());

  // The group of the result will be dynamically fixed up to match the input
  // object, allowing us to handle 'this' objects that might have more than one
  // group. Make sure that no singletons can be sliced here.
  if (arr->isSingleton()) {
    return AttachDecision::NoAction;
  }

  JSObject* templateObj =
      NewFullyAllocatedArrayTryReuseGroup(cx_, arr, 0, TenuredObject);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'slice' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId objId = writer.guardToObject(thisValId);
  writer.guardClass(objId, GuardClassKind::Array);

  Int32OperandId int32BeginId;
  if (argc_ > 0) {
    ValOperandId beginId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
    int32BeginId = writer.guardToInt32(beginId);
  } else {
    int32BeginId = writer.loadInt32Constant(0);
  }

  Int32OperandId int32EndId;
  if (argc_ > 1) {
    ValOperandId endId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
    int32EndId = writer.guardToInt32(endId);
  } else {
    int32EndId = writer.loadInt32ArrayLength(objId);
  }

  writer.packedArraySliceResult(templateObj, objId, int32BeginId, int32EndId);

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("ArraySlice");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayIsArray(HandleFunction callee) {
  // Need a single argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'isArray' native function.
  emitNativeCalleeGuard(callee);

  // Check if the argument is an Array and return result.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  writer.isArrayResult(argId);

  // This stub does not need to be monitored, because it always
  // returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ArrayIsArray");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachDataViewGet(HandleFunction callee,
                                                     Scalar::Type type) {
  // Ensure |this| is a DataViewObject.
  if (!thisval_.isObject() || !thisval_.toObject().is<DataViewObject>()) {
    return AttachDecision::NoAction;
  }

  // Expected arguments: offset (number), optional littleEndian (boolean).
  if (argc_ < 1 || argc_ > 2) {
    return AttachDecision::NoAction;
  }
  if (!args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }
  if (argc_ > 1 && !args_[1].isBoolean()) {
    return AttachDecision::NoAction;
  }

  DataViewObject* dv = &thisval_.toObject().as<DataViewObject>();

  // Bounds check the offset.
  int32_t offsetInt32;
  if (!mozilla::NumberEqualsInt32(args_[0].toNumber(), &offsetInt32)) {
    return AttachDecision::NoAction;
  }
  if (offsetInt32 < 0 ||
      !dv->offsetIsInBounds(Scalar::byteSize(type), offsetInt32)) {
    return AttachDecision::NoAction;
  }

  // For getUint32 we let the stub return a double only if the current result is
  // a double, to allow better codegen in Warp.
  bool allowDoubleForUint32 = false;
  if (type == Scalar::Uint32) {
    bool isLittleEndian = argc_ > 1 && args_[1].toBoolean();
    uint32_t res = dv->read<uint32_t>(offsetInt32, isLittleEndian);
    allowDoubleForUint32 = res >= INT32_MAX;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is this DataView native function.
  emitNativeCalleeGuard(callee);

  // Guard |this| is a DataViewObject.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId objId = writer.guardToObject(thisValId);
  writer.guardClass(objId, GuardClassKind::DataView);

  // Convert offset to int32.
  ValOperandId offsetId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32OffsetId = writer.guardToInt32Index(offsetId);

  BooleanOperandId boolLittleEndianId;
  if (argc_ > 1) {
    ValOperandId littleEndianId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
    boolLittleEndianId = writer.guardToBoolean(littleEndianId);
  } else {
    boolLittleEndianId = writer.loadBooleanConstant(false);
  }

  writer.loadDataViewValueResult(objId, int32OffsetId, boolLittleEndianId, type,
                                 allowDoubleForUint32);

  // This stub does not need to be monitored, because it returns the same type
  // as this call.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("DataViewGet");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachDataViewSet(HandleFunction callee,
                                                     Scalar::Type type) {
  // Ensure |this| is a DataViewObject.
  if (!thisval_.isObject() || !thisval_.toObject().is<DataViewObject>()) {
    return AttachDecision::NoAction;
  }

  // Expected arguments: offset (number), value, optional littleEndian (boolean)
  if (argc_ < 2 || argc_ > 3) {
    return AttachDecision::NoAction;
  }
  if (!args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }
  if (!ValueIsNumeric(type, args_[1])) {
    return AttachDecision::NoAction;
  }
  if (argc_ > 2 && !args_[2].isBoolean()) {
    return AttachDecision::NoAction;
  }

  DataViewObject* dv = &thisval_.toObject().as<DataViewObject>();

  // Bounds check the offset.
  int32_t offsetInt32;
  if (!mozilla::NumberEqualsInt32(args_[0].toNumber(), &offsetInt32)) {
    return AttachDecision::NoAction;
  }
  if (offsetInt32 < 0 ||
      !dv->offsetIsInBounds(Scalar::byteSize(type), offsetInt32)) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is this DataView native function.
  emitNativeCalleeGuard(callee);

  // Guard |this| is a DataViewObject.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId objId = writer.guardToObject(thisValId);
  writer.guardClass(objId, GuardClassKind::DataView);

  // Convert offset to int32.
  ValOperandId offsetId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32OffsetId = writer.guardToInt32Index(offsetId);

  // Convert value to number or BigInt.
  ValOperandId valueId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  OperandId numericValueId = emitNumericGuard(valueId, type);

  BooleanOperandId boolLittleEndianId;
  if (argc_ > 2) {
    ValOperandId littleEndianId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
    boolLittleEndianId = writer.guardToBoolean(littleEndianId);
  } else {
    boolLittleEndianId = writer.loadBooleanConstant(false);
  }

  writer.storeDataViewValueResult(objId, int32OffsetId, numericValueId,
                                  boolLittleEndianId, type);

  // This stub doesn't need type monitoring, it always returns |undefined|.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("DataViewSet");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachUnsafeGetReservedSlot(
    HandleFunction callee, InlinableNative native) {
  // Self-hosted code calls this with (object, int32) arguments.
  MOZ_ASSERT(argc_ == 2);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[1].isInt32());
  MOZ_ASSERT(args_[1].toInt32() >= 0);

  uint32_t slot = uint32_t(args_[1].toInt32());
  if (slot >= NativeObject::MAX_FIXED_SLOTS) {
    return AttachDecision::NoAction;
  }
  size_t offset = NativeObject::getFixedSlotOffset(slot);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard that the first argument is an object.
  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);

  // BytecodeEmitter::checkSelfHostedUnsafeGetReservedSlot ensures that the
  // slot argument is constant. (At least for direct calls)

  switch (native) {
    case InlinableNative::IntrinsicUnsafeGetReservedSlot:
      writer.loadFixedSlotResult(objId, offset);
      break;
    case InlinableNative::IntrinsicUnsafeGetObjectFromReservedSlot:
      writer.loadFixedSlotTypedResult(objId, offset, ValueType::Object);
      break;
    case InlinableNative::IntrinsicUnsafeGetInt32FromReservedSlot:
      writer.loadFixedSlotTypedResult(objId, offset, ValueType::Int32);
      break;
    case InlinableNative::IntrinsicUnsafeGetStringFromReservedSlot:
      writer.loadFixedSlotTypedResult(objId, offset, ValueType::String);
      break;
    case InlinableNative::IntrinsicUnsafeGetBooleanFromReservedSlot:
      writer.loadFixedSlotTypedResult(objId, offset, ValueType::Boolean);
      break;
    default:
      MOZ_CRASH("unexpected native");
  }

  // For simplicity just monitor all stubs.
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("UnsafeGetReservedSlot");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachUnsafeSetReservedSlot(
    HandleFunction callee) {
  // Self-hosted code calls this with (object, int32, value) arguments.
  MOZ_ASSERT(argc_ == 3);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[1].isInt32());
  MOZ_ASSERT(args_[1].toInt32() >= 0);

  uint32_t slot = uint32_t(args_[1].toInt32());
  if (slot >= NativeObject::MAX_FIXED_SLOTS) {
    return AttachDecision::NoAction;
  }
  size_t offset = NativeObject::getFixedSlotOffset(slot);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard that the first argument is an object.
  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);

  // BytecodeEmitter::checkSelfHostedUnsafeSetReservedSlot ensures that the
  // slot argument is constant. (At least for direct calls)

  // Get the value to set.
  ValOperandId valId = writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);

  // Set the fixed slot and return undefined.
  writer.storeFixedSlotUndefinedResult(objId, offset, valId);

  // This stub always returns undefined.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("UnsafeSetReservedSlot");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsSuspendedGenerator(
    HandleFunction callee) {
  // The IsSuspendedGenerator intrinsic is only called in
  // self-hosted code, so it's safe to assume we have a single
  // argument and the callee is our intrinsic.

  MOZ_ASSERT(argc_ == 1);

  Int32OperandId argcId(writer.setInputOperandId(0));

  // Stack layout here is (bottom to top):
  //  2: Callee
  //  1: ThisValue
  //  0: Arg <-- Top of stack.
  // We only care about the argument.
  ValOperandId valId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  // Check whether the argument is a suspended generator.
  // We don't need guards, because IsSuspendedGenerator returns
  // false for values that are not generator objects.
  writer.callIsSuspendedGeneratorResult(valId);
  writer.returnFromIC();

  // This stub does not need to be monitored, because it always
  // returns Boolean. The stack typeset will be updated when we
  // attach the stub.
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsSuspendedGenerator");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachToObject(HandleFunction callee,
                                                  InlinableNative native) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT_IF(native == InlinableNative::IntrinsicToObject, argc_ == 1);

  // Need a single object argument.
  // TODO(Warp): Support all or more conversions to object.
  // Note: ToObject and Object differ in their behavior for undefined/null.
  if (argc_ != 1 || !args_[0].isObject()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'Object' function.
  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.
  if (native == InlinableNative::Object) {
    emitNativeCalleeGuard(callee);
  }

  // Guard that the argument is an object.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argId);

  // Return the object.
  writer.loadObjectResult(objId);

  // Monitor the returned object.
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  if (native == InlinableNative::IntrinsicToObject) {
    trackAttached("ToObject");
  } else {
    MOZ_ASSERT(native == InlinableNative::Object);
    trackAttached("Object");
  }
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachToInteger(HandleFunction callee) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT(argc_ == 1);

  // Need a single int32 argument.
  // TODO(Warp): Support all or more conversions to integer.
  // Make sure to update this code correctly if we ever start
  // returning non-int32 integers.
  if (!args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard that the argument is an int32.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32Id = writer.guardToInt32(argId);

  // Return the int32.
  writer.loadInt32Result(int32Id);

  // This stub does not need to be monitored, because it always
  // returns an int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ToInteger");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachToLength(HandleFunction callee) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT(argc_ == 1);

  // Need a single int32 argument.
  if (!args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // ToLength(int32) is equivalent to max(int32, 0).
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32ArgId = writer.guardToInt32(argId);
  Int32OperandId zeroId = writer.loadInt32Constant(0);
  bool isMax = true;
  Int32OperandId maxId = writer.int32MinMax(isMax, int32ArgId, zeroId);
  writer.loadInt32Result(maxId);

  // This stub does not need to be monitored, because it always returns an
  // int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ToLength");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsObject(HandleFunction callee) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT(argc_ == 1);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Type check the argument and return result.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  writer.isObjectResult(argId);

  // This stub does not need to be monitored, because it always
  // returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsObject");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsPackedArray(HandleFunction callee) {
  // Self-hosted code calls this with a single object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Check if the argument is packed and return result.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);
  writer.isPackedArrayResult(objArgId);

  // This stub does not need to be monitored, because it always
  // returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsPackedArray");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsCallable(HandleFunction callee) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT(argc_ == 1);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Check if the argument is callable and return result.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  writer.isCallableResult(argId);

  // This stub does not need to be monitored, because it always
  // returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsCallable");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsConstructor(HandleFunction callee) {
  // Self-hosted code calls this with a single argument.
  MOZ_ASSERT(argc_ == 1);

  // Need a single object argument.
  if (!args_[0].isObject()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard that the argument is an object.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argId);

  // Check if the argument is a constructor and return result.
  writer.isConstructorResult(objId);

  // This stub does not need to be monitored, because it always
  // returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsCrossRealmArrayConstructor(
    HandleFunction callee) {
  // Self-hosted code calls this with an object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  if (args_[0].toObject().is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argId);
  writer.guardIsNotProxy(objId);
  writer.isCrossRealmArrayConstructorResult(objId);

  // This stub does not need to be monitored, it always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsCrossRealmArrayConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachGuardToClass(HandleFunction callee,
                                                      InlinableNative native) {
  // Self-hosted code calls this with an object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Class must match.
  const JSClass* clasp = InlinableNativeGuardToClass(native);
  if (args_[0].toObject().getClass() != clasp) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard that the argument is an object.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argId);

  // Guard that the object has the correct class.
  writer.guardAnyClass(objId, clasp);

  // Return the object.
  writer.loadObjectResult(objId);

  // Monitor the returned object.
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("GuardToClass");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachHasClass(HandleFunction callee,
                                                  const JSClass* clasp,
                                                  bool isPossiblyWrapped) {
  // Self-hosted code calls this with an object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Only optimize when the object isn't a proxy.
  if (isPossiblyWrapped && IsProxy(&args_[0].toObject())) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Perform the Class check.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argId);

  if (isPossiblyWrapped) {
    writer.guardIsNotProxy(objId);
  }

  writer.hasClassResult(objId, clasp);

  // Return without type monitoring, because this always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("HasClass");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachRegExpMatcherSearcherTester(
    HandleFunction callee, InlinableNative native) {
  // Self-hosted code calls this with (object, string, number) arguments.
  MOZ_ASSERT(argc_ == 3);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[1].isString());
  MOZ_ASSERT(args_[2].isNumber());

  // It's not guaranteed that the JITs have typed |lastIndex| as an Int32.
  if (!args_[2].isInt32()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  // Guard argument types.
  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId reId = writer.guardToObject(arg0Id);

  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  StringOperandId inputId = writer.guardToString(arg1Id);

  ValOperandId arg2Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId lastIndexId = writer.guardToInt32(arg2Id);

  switch (native) {
    case InlinableNative::RegExpMatcher:
      writer.callRegExpMatcherResult(reId, inputId, lastIndexId);
      writer.typeMonitorResult();
      cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;
      trackAttached("RegExpMatcher");
      break;

    case InlinableNative::RegExpSearcher:
      // No type monitoring because this always returns an int32.
      writer.callRegExpSearcherResult(reId, inputId, lastIndexId);
      writer.returnFromIC();
      cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;
      trackAttached("RegExpSearcher");
      break;

    case InlinableNative::RegExpTester:
      // No type monitoring because this always returns an int32.
      writer.callRegExpTesterResult(reId, inputId, lastIndexId);
      writer.returnFromIC();
      cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;
      trackAttached("RegExpTester");
      break;

    default:
      MOZ_CRASH("Unexpected native");
  }

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachRegExpPrototypeOptimizable(
    HandleFunction callee) {
  // Self-hosted code calls this with a single object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId protoId = writer.guardToObject(arg0Id);

  writer.regExpPrototypeOptimizableResult(protoId);

  // No type monitoring because this always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("RegExpPrototypeOptimizable");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachRegExpInstanceOptimizable(
    HandleFunction callee) {
  // Self-hosted code calls this with two object arguments.
  MOZ_ASSERT(argc_ == 2);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[1].isObject());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId regexpId = writer.guardToObject(arg0Id);

  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  ObjOperandId protoId = writer.guardToObject(arg1Id);

  writer.regExpInstanceOptimizableResult(regexpId, protoId);

  // No type monitoring because this always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("RegExpInstanceOptimizable");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachGetFirstDollarIndex(
    HandleFunction callee) {
  // Self-hosted code calls this with a single string argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isString());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  StringOperandId strId = writer.guardToString(arg0Id);

  writer.getFirstDollarIndexResult(strId);

  // No type monitoring because this always returns an int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("GetFirstDollarIndex");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachSubstringKernel(
    HandleFunction callee) {
  // Self-hosted code calls this with (string, int32, int32) arguments.
  MOZ_ASSERT(argc_ == 3);
  MOZ_ASSERT(args_[0].isString());
  MOZ_ASSERT(args_[1].isInt32());
  MOZ_ASSERT(args_[2].isInt32());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  StringOperandId strId = writer.guardToString(arg0Id);

  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  Int32OperandId beginId = writer.guardToInt32(arg1Id);

  ValOperandId arg2Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId lengthId = writer.guardToInt32(arg2Id);

  writer.callSubstringKernelResult(strId, beginId, lengthId);

  // No type monitoring because this always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("SubstringKernel");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachObjectHasPrototype(
    HandleFunction callee) {
  // Self-hosted code calls this with (object, object) arguments.
  MOZ_ASSERT(argc_ == 2);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[1].isObject());

  auto* obj = &args_[0].toObject().as<NativeObject>();
  auto* proto = &args_[1].toObject().as<NativeObject>();

  // Only attach when obj.__proto__ is proto.
  if (obj->staticPrototype() != proto) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);

  writer.guardProto(objId, proto);
  writer.loadBooleanResult(true);

  // No type monitoring because this always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ObjectHasPrototype");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachString(HandleFunction callee) {
  // Need a single string argument.
  // TODO(Warp): Support all or more conversions to strings.
  if (argc_ != 1 || !args_[0].isString()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'String' function.
  emitNativeCalleeGuard(callee);

  // Guard that the argument is a string.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  StringOperandId strId = writer.guardToString(argId);

  // Return the string.
  writer.loadStringResult(strId);

  // This stub does not need to be monitored, because it always
  // returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("String");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringConstructor(
    HandleFunction callee) {
  // Need a single string argument.
  // TODO(Warp): Support all or more conversions to strings.
  if (argc_ != 1 || !args_[0].isString()) {
    return AttachDecision::NoAction;
  }

  RootedString emptyString(cx_, cx_->runtime()->emptyString);
  JSObject* templateObj = StringObject::create(
      cx_, emptyString, /* proto = */ nullptr, TenuredObject);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'String' function.
  emitNativeCalleeGuard(callee);

  CallFlags flags(IsConstructPC(pc_), IsSpreadPC(pc_));

  // Guard that the argument is a string.
  ValOperandId argId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_, flags);
  StringOperandId strId = writer.guardToString(argId);

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }

  writer.newStringObjectResult(templateObj, strId);
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("StringConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringToStringValueOf(
    HandleFunction callee) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Ensure |this| is a primitive string value.
  if (!thisval_.isString()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'toString' OR 'valueOf' native function.
  emitNativeCalleeGuard(callee);

  // Guard |this| is a string.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  StringOperandId strId = writer.guardToString(thisValId);

  // Return the string
  writer.loadStringResult(strId);

  // This stub doesn't need to be monitored, because it always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringToStringValueOf");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringReplaceString(
    HandleFunction callee) {
  // Self-hosted code calls this with (string, string, string) arguments.
  MOZ_ASSERT(argc_ == 3);
  MOZ_ASSERT(args_[0].isString());
  MOZ_ASSERT(args_[1].isString());
  MOZ_ASSERT(args_[2].isString());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  StringOperandId strId = writer.guardToString(arg0Id);

  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  StringOperandId patternId = writer.guardToString(arg1Id);

  ValOperandId arg2Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  StringOperandId replacementId = writer.guardToString(arg2Id);

  writer.stringReplaceStringResult(strId, patternId, replacementId);

  // No type monitoring because this always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringReplaceString");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringSplitString(
    HandleFunction callee) {
  // Self-hosted code calls this with (string, string) arguments.
  MOZ_ASSERT(argc_ == 2);
  MOZ_ASSERT(args_[0].isString());
  MOZ_ASSERT(args_[1].isString());

  ObjectGroup* group = ObjectGroupRealm::getStringSplitStringGroup(cx_);
  if (!group) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  StringOperandId strId = writer.guardToString(arg0Id);

  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  StringOperandId separatorId = writer.guardToString(arg1Id);

  writer.stringSplitStringResult(strId, separatorId, group);

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("StringSplitString");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringChar(HandleFunction callee,
                                                    StringChar kind) {
  // Need one argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  if (!CanAttachStringChar(thisval_, args_[0])) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'charCodeAt' or 'charAt' native function.
  emitNativeCalleeGuard(callee);

  // Guard this is a string.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  StringOperandId strId = writer.guardToString(thisValId);

  // Guard int32 index.
  ValOperandId indexId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);

  // Load string char or code.
  if (kind == StringChar::CodeAt) {
    writer.loadStringCharCodeResult(strId, int32IndexId);
  } else {
    writer.loadStringCharResult(strId, int32IndexId);
  }

  writer.returnFromIC();

  // This stub does not need to be monitored, because it always
  // returns a string for charAt or int32_t for charCodeAt.
  // The stack typeset will be updated when we attach the stub.
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  if (kind == StringChar::CodeAt) {
    trackAttached("StringCharCodeAt");
  } else {
    trackAttached("StringCharAt");
  }

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringCharCodeAt(
    HandleFunction callee) {
  return tryAttachStringChar(callee, StringChar::CodeAt);
}

AttachDecision CallIRGenerator::tryAttachStringCharAt(HandleFunction callee) {
  return tryAttachStringChar(callee, StringChar::At);
}

AttachDecision CallIRGenerator::tryAttachStringFromCharCode(
    HandleFunction callee) {
  // Need one int32 argument.
  if (argc_ != 1 || !args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'fromCharCode' native function.
  emitNativeCalleeGuard(callee);

  // Guard int32 argument.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId codeId = writer.guardToInt32(argId);

  // Return string created from code.
  writer.stringFromCharCodeResult(codeId);

  // This stub does not need to be monitored, because it always
  // returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringFromCharCode");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringFromCodePoint(
    HandleFunction callee) {
  // Need one int32 argument.
  if (argc_ != 1 || !args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  // String.fromCodePoint throws for invalid code points.
  int32_t codePoint = args_[0].toInt32();
  if (codePoint < 0 || codePoint > int32_t(unicode::NonBMPMax)) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'fromCodePoint' native function.
  emitNativeCalleeGuard(callee);

  // Guard int32 argument.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId codeId = writer.guardToInt32(argId);

  // Return string created from code point.
  writer.stringFromCodePointResult(codeId);

  // This stub doesn't need to be monitored, because it always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringFromCodePoint");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringToLowerCase(
    HandleFunction callee) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Ensure |this| is a primitive string value.
  if (!thisval_.isString()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'toLowerCase' native function.
  emitNativeCalleeGuard(callee);

  // Guard this is a string.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  StringOperandId strId = writer.guardToString(thisValId);

  // Return string converted to lower-case.
  writer.stringToLowerCaseResult(strId);

  // This stub doesn't need to be monitored, because it always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringToLowerCase");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStringToUpperCase(
    HandleFunction callee) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Ensure |this| is a primitive string value.
  if (!thisval_.isString()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'toUpperCase' native function.
  emitNativeCalleeGuard(callee);

  // Guard this is a string.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  StringOperandId strId = writer.guardToString(thisValId);

  // Return string converted to upper-case.
  writer.stringToUpperCaseResult(strId);

  // This stub doesn't need to be monitored, because it always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("StringToUpperCase");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathRandom(HandleFunction callee) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(cx_->realm() == callee->realm(),
             "Shouldn't inline cross-realm Math.random because per-realm RNG");

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'random' native function.
  emitNativeCalleeGuard(callee);

  mozilla::non_crypto::XorShift128PlusRNG* rng =
      &cx_->realm()->getOrCreateRandomNumberGenerator();
  writer.mathRandomResult(rng);

  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathRandom");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathAbs(HandleFunction callee) {
  // Need one argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  if (!args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'abs' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  // abs(INT_MIN) is a double.
  if (args_[0].isInt32() && args_[0].toInt32() != INT_MIN) {
    Int32OperandId int32Id = writer.guardToInt32(argumentId);
    writer.mathAbsInt32Result(int32Id);
  } else {
    NumberOperandId numberId = writer.guardIsNumber(argumentId);
    writer.mathAbsNumberResult(numberId);
  }

  // The INT_MIN case and implementation details of the C++ abs
  // make this a bit tricky. We probably can just remove monitoring
  // in the future completely, so do the easy thing right now.
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathAbs");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathClz32(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'clz32' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  Int32OperandId int32Id;
  if (args_[0].isInt32()) {
    int32Id = writer.guardToInt32(argId);
  } else {
    MOZ_ASSERT(args_[0].isDouble());
    NumberOperandId numId = writer.guardIsNumber(argId);
    int32Id = writer.truncateDoubleToUInt32(numId);
  }
  writer.mathClz32Result(int32Id);

  // Math.clz32 always returns an int32 so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathClz32");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathSign(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'sign' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  if (args_[0].isInt32()) {
    Int32OperandId int32Id = writer.guardToInt32(argId);
    writer.mathSignInt32Result(int32Id);
  } else {
    // Math.sign returns a double only if the input is -0 or NaN so try to
    // optimize the common Number => Int32 case.
    double d = math_sign_impl(args_[0].toDouble());
    int32_t unused;
    bool resultIsInt32 = mozilla::NumberIsInt32(d, &unused);

    NumberOperandId numId = writer.guardIsNumber(argId);
    if (resultIsInt32) {
      writer.mathSignNumberToInt32Result(numId);
    } else {
      writer.mathSignNumberResult(numId);
    }
  }

  // The stub's return type matches the current return value so we don't need
  // type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathSign");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathImul(HandleFunction callee) {
  // Need two (number) arguments.
  if (argc_ != 2 || !args_[0].isNumber() || !args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'imul' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ValOperandId arg1Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);

  Int32OperandId int32Arg0Id, int32Arg1Id;
  if (args_[0].isInt32() && args_[1].isInt32()) {
    int32Arg0Id = writer.guardToInt32(arg0Id);
    int32Arg1Id = writer.guardToInt32(arg1Id);
  } else {
    // Treat both arguments as numbers if at least one of them is non-int32.
    NumberOperandId numArg0Id = writer.guardIsNumber(arg0Id);
    NumberOperandId numArg1Id = writer.guardIsNumber(arg1Id);
    int32Arg0Id = writer.truncateDoubleToUInt32(numArg0Id);
    int32Arg1Id = writer.truncateDoubleToUInt32(numArg1Id);
  }
  writer.mathImulResult(int32Arg0Id, int32Arg1Id);

  // Math.imul always returns int32 so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathImul");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathFloor(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Check if the result fits in int32.
  double res = math_floor_impl(args_[0].toNumber());
  int32_t unused;
  bool resultIsInt32 = mozilla::NumberIsInt32(res, &unused);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'floor' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);

  if (resultIsInt32) {
    writer.mathFloorToInt32Result(numberId);
  } else {
    writer.mathFunctionNumberResult(numberId, UnaryMathFunction::Floor);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathFloor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathCeil(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Check if the result fits in int32.
  double res = math_ceil_impl(args_[0].toNumber());
  int32_t unused;
  bool resultIsInt32 = mozilla::NumberIsInt32(res, &unused);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'ceil' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);

  if (resultIsInt32) {
    writer.mathCeilToInt32Result(numberId);
  } else {
    writer.mathFunctionNumberResult(numberId, UnaryMathFunction::Ceil);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathCeil");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathTrunc(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Check if the result fits in int32.
  double res = math_trunc_impl(args_[0].toNumber());
  int32_t unused;
  bool resultIsInt32 = mozilla::NumberIsInt32(res, &unused);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'trunc' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);

  if (resultIsInt32) {
    writer.mathTruncToInt32Result(numberId);
  } else {
    writer.mathFunctionNumberResult(numberId, UnaryMathFunction::Trunc);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathTrunc");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathRound(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Check if the result fits in int32.
  double res = math_round_impl(args_[0].toNumber());
  int32_t unused;
  bool resultIsInt32 = mozilla::NumberIsInt32(res, &unused);

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'round' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);

  if (resultIsInt32) {
    writer.mathRoundToInt32Result(numberId);
  } else {
    writer.mathFunctionNumberResult(numberId, UnaryMathFunction::Round);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathRound");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathSqrt(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'sqrt' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);
  writer.mathSqrtNumberResult(numberId);

  // Math.sqrt always returns a double so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathSqrt");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathFRound(HandleFunction callee) {
  // Need one (number) argument.
  if (argc_ != 1 || !args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'fround' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);
  writer.mathFRoundNumberResult(numberId);

  // Math.fround always returns a double so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathFRound");
  return AttachDecision::Attach;
}

static bool CanAttachInt32Pow(const Value& baseVal, const Value& powerVal) {
  MOZ_ASSERT(baseVal.isInt32() || baseVal.isBoolean());
  MOZ_ASSERT(powerVal.isInt32() || powerVal.isBoolean());

  int32_t base = baseVal.isInt32() ? baseVal.toInt32() : baseVal.toBoolean();
  int32_t power =
      powerVal.isInt32() ? powerVal.toInt32() : powerVal.toBoolean();

  // x^y where y < 0 is most of the time not an int32, except when x is 1 or y
  // gets large enough. It's hard to determine when exactly y is "large enough",
  // so we don't use Int32PowResult when x != 1 and y < 0.
  // Note: it's important for this condition to match the code generated by
  // MacroAssembler::pow32 to prevent failure loops.
  if (power < 0) {
    return base == 1;
  }

  double res = powi(base, power);
  int32_t unused;
  return mozilla::NumberIsInt32(res, &unused);
}

AttachDecision CallIRGenerator::tryAttachMathPow(HandleFunction callee) {
  // Need two number arguments.
  if (argc_ != 2 || !args_[0].isNumber() || !args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'pow' function.
  emitNativeCalleeGuard(callee);

  ValOperandId baseId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ValOperandId exponentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);

  if (args_[0].isInt32() && args_[1].isInt32() &&
      CanAttachInt32Pow(args_[0], args_[1])) {
    Int32OperandId baseInt32Id = writer.guardToInt32(baseId);
    Int32OperandId exponentInt32Id = writer.guardToInt32(exponentId);
    writer.int32PowResult(baseInt32Id, exponentInt32Id);
  } else {
    NumberOperandId baseNumberId = writer.guardIsNumber(baseId);
    NumberOperandId exponentNumberId = writer.guardIsNumber(exponentId);
    writer.doublePowResult(baseNumberId, exponentNumberId);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("MathPow");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathHypot(HandleFunction callee) {
  // Only optimize if there are 2-4 arguments.
  if (argc_ < 2 || argc_ > 4) {
    return AttachDecision::NoAction;
  }

  for (size_t i = 0; i < argc_; i++) {
    if (!args_[i].isNumber()) {
      return AttachDecision::NoAction;
    }
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'hypot' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId firstId = writer.loadStandardCallArgument(0, argc_);
  ValOperandId secondId = writer.loadStandardCallArgument(1, argc_);

  NumberOperandId firstNumId = writer.guardIsNumber(firstId);
  NumberOperandId secondNumId = writer.guardIsNumber(secondId);

  ValOperandId thirdId;
  ValOperandId fourthId;
  NumberOperandId thirdNumId;
  NumberOperandId fourthNumId;

  switch (argc_) {
    case 2:
      writer.mathHypot2NumberResult(firstNumId, secondNumId);
      break;
    case 3:
      thirdId = writer.loadStandardCallArgument(2, argc_);
      thirdNumId = writer.guardIsNumber(thirdId);
      writer.mathHypot3NumberResult(firstNumId, secondNumId, thirdNumId);
      break;
    case 4:
      thirdId = writer.loadStandardCallArgument(2, argc_);
      fourthId = writer.loadStandardCallArgument(3, argc_);
      thirdNumId = writer.guardIsNumber(thirdId);
      fourthNumId = writer.guardIsNumber(fourthId);
      writer.mathHypot4NumberResult(firstNumId, secondNumId, thirdNumId,
                                    fourthNumId);
      break;
    default:
      MOZ_CRASH("Unexpected number of arguments to hypot function.");
  }

  // Math.hypot always returns a double so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathHypot");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathATan2(HandleFunction callee) {
  // Requires two numbers as arguments.
  if (argc_ != 2 || !args_[0].isNumber() || !args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'atan2' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId yId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ValOperandId xId = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);

  NumberOperandId yNumberId = writer.guardIsNumber(yId);
  NumberOperandId xNumberId = writer.guardIsNumber(xId);

  writer.mathAtan2NumberResult(yNumberId, xNumberId);

  // Math.atan2 always returns a double so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathAtan2");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathMinMax(HandleFunction callee,
                                                    bool isMax) {
  // For now only optimize if there are 1-4 arguments.
  if (argc_ < 1 || argc_ > 4) {
    return AttachDecision::NoAction;
  }

  // Ensure all arguments are numbers.
  bool allInt32 = true;
  for (size_t i = 0; i < argc_; i++) {
    if (!args_[i].isNumber()) {
      return AttachDecision::NoAction;
    }
    if (!args_[i].isInt32()) {
      allInt32 = false;
    }
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is this Math function.
  emitNativeCalleeGuard(callee);

  if (allInt32) {
    ValOperandId valId = writer.loadStandardCallArgument(0, argc_);
    Int32OperandId resId = writer.guardToInt32(valId);
    for (size_t i = 1; i < argc_; i++) {
      ValOperandId argId = writer.loadStandardCallArgument(i, argc_);
      Int32OperandId argInt32Id = writer.guardToInt32(argId);
      resId = writer.int32MinMax(isMax, resId, argInt32Id);
    }
    writer.loadInt32Result(resId);
  } else {
    ValOperandId valId = writer.loadStandardCallArgument(0, argc_);
    NumberOperandId resId = writer.guardIsNumber(valId);
    for (size_t i = 1; i < argc_; i++) {
      ValOperandId argId = writer.loadStandardCallArgument(i, argc_);
      NumberOperandId argNumId = writer.guardIsNumber(argId);
      resId = writer.numberMinMax(isMax, resId, argNumId);
    }
    writer.loadDoubleResult(resId);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached(isMax ? "MathMax" : "MathMin");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachMathFunction(HandleFunction callee,
                                                      UnaryMathFunction fun) {
  // Need one argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  if (!args_[0].isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is this Math function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  NumberOperandId numberId = writer.guardIsNumber(argumentId);
  writer.mathFunctionNumberResult(numberId, fun);

  // These functions always return a double so we don't need type monitoring.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("MathFunction");
  return AttachDecision::Attach;
}

StringOperandId EmitToStringGuard(CacheIRWriter& writer, ValOperandId id,
                                  HandleValue v) {
  if (v.isString()) {
    return writer.guardToString(id);
  }
  if (v.isInt32()) {
    Int32OperandId intId = writer.guardToInt32(id);
    return writer.callInt32ToString(intId);
  }
  // At this point we are creating an IC that will handle
  // both Int32 and Double cases.
  MOZ_ASSERT(v.isNumber());
  NumberOperandId numId = writer.guardIsNumber(id);
  return writer.callNumberToString(numId);
}

AttachDecision CallIRGenerator::tryAttachNumberToString(HandleFunction callee) {
  // Expecting no arguments, which means base 10.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Ensure |this| is a primitive number value.
  if (!thisval_.isNumber()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'toString' native function.
  emitNativeCalleeGuard(callee);

  // Initialize the |this| operand.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);

  // Guard on number and convert to string.
  StringOperandId strId = EmitToStringGuard(writer, thisValId, thisval_);

  // Return the string.
  writer.loadStringResult(strId);

  // This stub doesn't need to be monitored, because it always returns a string.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("NumberToString");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachReflectGetPrototypeOf(
    HandleFunction callee) {
  // Need one argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  if (!args_[0].isObject()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'getPrototypeOf' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId argumentId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(argumentId);

  writer.reflectGetPrototypeOfResult(objId);

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("ReflectGetPrototypeOf");
  return AttachDecision::Attach;
}

static bool AtomicsMeetsPreconditions(TypedArrayObject* typedArray,
                                      double index) {
  switch (typedArray->type()) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
      break;

    case Scalar::BigInt64:
    case Scalar::BigUint64:
      // Bug 1638295: Not yet implemented.
      return false;

    case Scalar::Float32:
    case Scalar::Float64:
    case Scalar::Uint8Clamped:
      // Exclude floating types and Uint8Clamped.
      return false;

    case Scalar::MaxTypedArrayViewType:
    case Scalar::Int64:
    case Scalar::Simd128:
      MOZ_CRASH("Unsupported TypedArray type");
  }

  // Bounds check the index argument.
  int32_t indexInt32;
  if (!mozilla::NumberEqualsInt32(index, &indexInt32)) {
    return false;
  }
  if (indexInt32 < 0 || uint32_t(indexInt32) >= typedArray->length()) {
    return false;
  }

  return true;
}

AttachDecision CallIRGenerator::tryAttachAtomicsCompareExchange(
    HandleFunction callee) {
  if (!JitSupportsAtomics()) {
    return AttachDecision::NoAction;
  }

  // Need four arguments.
  if (argc_ != 4) {
    return AttachDecision::NoAction;
  }

  // Arguments: typedArray, index (number), expected, replacement.
  if (!args_[0].isObject() || !args_[0].toObject().is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }
  if (!args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }
  if (!args_[2].isNumber()) {
    return AttachDecision::NoAction;
  }
  if (!args_[3].isNumber()) {
    return AttachDecision::NoAction;
  }

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();
  if (!AtomicsMeetsPreconditions(typedArray, args_[1].toNumber())) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `compareExchange` native function.
  emitNativeCalleeGuard(callee);

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);
  writer.guardShapeForClass(objId, typedArray->shape());

  // Convert index to int32.
  ValOperandId indexId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);

  // Convert expected value to int32.
  ValOperandId expectedId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId int32ExpectedId = writer.guardToInt32ModUint32(expectedId);

  // Convert replacement value to int32.
  ValOperandId replacementId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg3, argc_);
  Int32OperandId int32ReplacementId =
      writer.guardToInt32ModUint32(replacementId);

  writer.atomicsCompareExchangeResult(objId, int32IndexId, int32ExpectedId,
                                      int32ReplacementId, typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsCompareExchange");
  return AttachDecision::Attach;
}

bool CallIRGenerator::canAttachAtomicsReadWriteModify() {
  if (!JitSupportsAtomics()) {
    return false;
  }

  // Need three arguments.
  if (argc_ != 3) {
    return false;
  }

  // Arguments: typedArray, index (number), value.
  if (!args_[0].isObject() || !args_[0].toObject().is<TypedArrayObject>()) {
    return false;
  }
  if (!args_[1].isNumber()) {
    return false;
  }
  if (!args_[2].isNumber()) {
    return false;
  }

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();
  if (!AtomicsMeetsPreconditions(typedArray, args_[1].toNumber())) {
    return false;
  }

  return true;
}

CallIRGenerator::AtomicsReadWriteModifyOperands
CallIRGenerator::emitAtomicsReadWriteModifyOperands(HandleFunction callee) {
  MOZ_ASSERT(canAttachAtomicsReadWriteModify());

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is this Atomics function.
  emitNativeCalleeGuard(callee);

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);
  writer.guardShapeForClass(objId, typedArray->shape());

  // Convert index to int32.
  ValOperandId indexId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);

  // Convert value to int32.
  ValOperandId valueId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId int32ValueId = writer.guardToInt32ModUint32(valueId);

  return {objId, int32IndexId, int32ValueId};
}

AttachDecision CallIRGenerator::tryAttachAtomicsExchange(
    HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsExchangeResult(objId, int32IndexId, int32ValueId,
                               typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsExchange");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsAdd(HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsAddResult(objId, int32IndexId, int32ValueId,
                          typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsAdd");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsSub(HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsSubResult(objId, int32IndexId, int32ValueId,
                          typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsSub");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsAnd(HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsAndResult(objId, int32IndexId, int32ValueId,
                          typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsAnd");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsOr(HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsOrResult(objId, int32IndexId, int32ValueId, typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsOr");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsXor(HandleFunction callee) {
  if (!canAttachAtomicsReadWriteModify()) {
    return AttachDecision::NoAction;
  }

  auto [objId, int32IndexId, int32ValueId] =
      emitAtomicsReadWriteModifyOperands(callee);

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();

  writer.atomicsXorResult(objId, int32IndexId, int32ValueId,
                          typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsXor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsLoad(HandleFunction callee) {
  if (!JitSupportsAtomics()) {
    return AttachDecision::NoAction;
  }

  // Need two arguments.
  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  // Arguments: typedArray, index (number).
  if (!args_[0].isObject() || !args_[0].toObject().is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }
  if (!args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();
  if (!AtomicsMeetsPreconditions(typedArray, args_[1].toNumber())) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `load` native function.
  emitNativeCalleeGuard(callee);

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);
  writer.guardShapeForClass(objId, typedArray->shape());

  // Convert index to int32.
  ValOperandId indexId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);

  writer.atomicsLoadResult(objId, int32IndexId, typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or, for uint32, a double.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsLoad");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsStore(HandleFunction callee) {
  if (!JitSupportsAtomics()) {
    return AttachDecision::NoAction;
  }

  // Need three arguments.
  if (argc_ != 3) {
    return AttachDecision::NoAction;
  }

  // Atomics.store() is annoying because it returns the result of converting the
  // value by ToInteger(), not the input value, nor the result of converting the
  // value by ToInt32(). It is especially annoying because almost nobody uses
  // the result value.
  //
  // As an expedient compromise, therefore, we inline only if the result is
  // obviously unused or if the argument is already Int32 and thus requires no
  // conversion.

  // Arguments: typedArray, index (number), value.
  if (!args_[0].isObject() || !args_[0].toObject().is<TypedArrayObject>()) {
    return AttachDecision::NoAction;
  }
  if (!args_[1].isNumber()) {
    return AttachDecision::NoAction;
  }
  if (op_ == JSOp::CallIgnoresRv ? !args_[2].isNumber() : !args_[2].isInt32()) {
    return AttachDecision::NoAction;
  }

  auto* typedArray = &args_[0].toObject().as<TypedArrayObject>();
  if (!AtomicsMeetsPreconditions(typedArray, args_[1].toNumber())) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `store` native function.
  emitNativeCalleeGuard(callee);

  ValOperandId arg0Id = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objId = writer.guardToObject(arg0Id);
  writer.guardShapeForClass(objId, typedArray->shape());

  // Convert index to int32.
  ValOperandId indexId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  Int32OperandId int32IndexId = writer.guardToInt32Index(indexId);

  // Ensure value is int32.
  ValOperandId valueId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId int32ValueId;
  if (op_ == JSOp::CallIgnoresRv) {
    int32ValueId = writer.guardToInt32ModUint32(valueId);
  } else {
    int32ValueId = writer.guardToInt32(valueId);
  }

  writer.atomicsStoreResult(objId, int32IndexId, int32ValueId,
                            typedArray->type());

  // This stub doesn't need to be monitored, because it always returns an int32
  // or the result is not observed.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsStore");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAtomicsIsLockFree(
    HandleFunction callee) {
  // Need one argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  if (!args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `isLockFree` native function.
  emitNativeCalleeGuard(callee);

  // Ensure value is int32.
  ValOperandId valueId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  Int32OperandId int32ValueId = writer.guardToInt32(valueId);

  writer.atomicsIsLockFreeResult(int32ValueId);

  // This stub doesn't need to be monitored, because it always returns a bool.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AtomicsIsLockFree");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachBoolean(HandleFunction callee) {
  // Need zero or one argument.
  if (argc_ > 1) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'Boolean' native function.
  emitNativeCalleeGuard(callee);

  if (argc_ == 0) {
    writer.loadBooleanResult(false);
  } else {
    ValOperandId valId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

    writer.loadValueTruthyResult(valId);
  }

  // This stub doesn't need to be monitored, because it always returns a bool.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("Boolean");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachBailout(HandleFunction callee) {
  // Expecting no arguments.
  if (argc_ != 0) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'bailout' native function.
  emitNativeCalleeGuard(callee);

  writer.bailout();
  writer.loadUndefinedResult();

  // This stub doesn't need to be monitored, it always returns undefined.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("Bailout");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAssertFloat32(HandleFunction callee) {
  // Expecting two arguments.
  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'assertFloat32' native function.
  emitNativeCalleeGuard(callee);

  // TODO: Warp doesn't yet optimize Float32 (bug 1655773).

  // NOP when not in IonMonkey.
  writer.loadUndefinedResult();

  // This stub doesn't need to be monitored, it always returns undefined.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AssertFloat32");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachAssertRecoveredOnBailout(
    HandleFunction callee) {
  // Expecting two arguments.
  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  // (Fuzzing unsafe) testing function which must be called with a constant
  // boolean as its second argument.
  bool mustBeRecovered = args_[1].toBoolean();

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'assertRecoveredOnBailout' native function.
  emitNativeCalleeGuard(callee);

  ValOperandId valId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  writer.assertRecoveredOnBailoutResult(valId, mustBeRecovered);

  // This stub doesn't need to be monitored, it always returns undefined.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("AssertRecoveredOnBailout");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachObjectIs(HandleFunction callee) {
  // Need two arguments.
  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  // TODO(Warp): attach this stub just once to prevent slowdowns for polymorphic
  // calls.

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `is` native function.
  emitNativeCalleeGuard(callee);

  ValOperandId lhsId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ValOperandId rhsId = writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);

  HandleValue lhs = args_[0];
  HandleValue rhs = args_[1];

  if (lhs.isNumber() && rhs.isNumber() && !(lhs.isInt32() && rhs.isInt32())) {
    NumberOperandId lhsNumId = writer.guardIsNumber(lhsId);
    NumberOperandId rhsNumId = writer.guardIsNumber(rhsId);
    writer.compareDoubleSameValueResult(lhsNumId, rhsNumId);
  } else if (!SameType(lhs, rhs)) {
    // Compare tags for strictly different types.
    ValueTagOperandId lhsTypeId = writer.loadValueTag(lhsId);
    ValueTagOperandId rhsTypeId = writer.loadValueTag(rhsId);
    writer.guardTagNotEqual(lhsTypeId, rhsTypeId);
    writer.loadBooleanResult(false);
  } else {
    MOZ_ASSERT(lhs.type() == rhs.type());
    MOZ_ASSERT(lhs.type() != JS::ValueType::Double);

    switch (lhs.type()) {
      case JS::ValueType::Int32: {
        Int32OperandId lhsIntId = writer.guardToInt32(lhsId);
        Int32OperandId rhsIntId = writer.guardToInt32(rhsId);
        writer.compareInt32Result(JSOp::StrictEq, lhsIntId, rhsIntId);
        break;
      }
      case JS::ValueType::Boolean: {
        Int32OperandId lhsIntId = writer.guardBooleanToInt32(lhsId);
        Int32OperandId rhsIntId = writer.guardBooleanToInt32(rhsId);
        writer.compareInt32Result(JSOp::StrictEq, lhsIntId, rhsIntId);
        break;
      }
      case JS::ValueType::Undefined: {
        writer.guardIsUndefined(lhsId);
        writer.guardIsUndefined(rhsId);
        writer.loadBooleanResult(true);
        break;
      }
      case JS::ValueType::Null: {
        writer.guardIsNull(lhsId);
        writer.guardIsNull(rhsId);
        writer.loadBooleanResult(true);
        break;
      }
      case JS::ValueType::String: {
        StringOperandId lhsStrId = writer.guardToString(lhsId);
        StringOperandId rhsStrId = writer.guardToString(rhsId);
        writer.compareStringResult(JSOp::StrictEq, lhsStrId, rhsStrId);
        break;
      }
      case JS::ValueType::Symbol: {
        SymbolOperandId lhsSymId = writer.guardToSymbol(lhsId);
        SymbolOperandId rhsSymId = writer.guardToSymbol(rhsId);
        writer.compareSymbolResult(JSOp::StrictEq, lhsSymId, rhsSymId);
        break;
      }
      case JS::ValueType::BigInt: {
        BigIntOperandId lhsBigIntId = writer.guardToBigInt(lhsId);
        BigIntOperandId rhsBigIntId = writer.guardToBigInt(rhsId);
        writer.compareBigIntResult(JSOp::StrictEq, lhsBigIntId, rhsBigIntId);
        break;
      }
      case JS::ValueType::Object: {
        ObjOperandId lhsObjId = writer.guardToObject(lhsId);
        ObjOperandId rhsObjId = writer.guardToObject(rhsId);
        writer.compareObjectResult(JSOp::StrictEq, lhsObjId, rhsObjId);
        break;
      }

      case JS::ValueType::Double:
      case JS::ValueType::Magic:
      case JS::ValueType::PrivateGCThing:
        MOZ_CRASH("Unexpected type");
    }
  }

  // This stub does not need to be monitored, because it always returns a
  // boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ObjectIs");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachObjectIsPrototypeOf(
    HandleFunction callee) {
  // Ensure |this| is an object.
  if (!thisval_.isObject()) {
    return AttachDecision::NoAction;
  }

  // Need a single argument.
  if (argc_ != 1) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the `isPrototypeOf` native function.
  emitNativeCalleeGuard(callee);

  // Guard that |this| is an object.
  ValOperandId thisValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::This, argcId);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);

  writer.loadInstanceOfObjectResult(argId, thisObjId);

  // This stub does not need to be monitored, because it always returns a
  // boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ObjectIsPrototypeOf");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachFunCall(HandleFunction callee) {
  if (!callee->isNativeWithoutJitEntry() || callee->native() != fun_call) {
    return AttachDecision::NoAction;
  }

  if (!thisval_.isObject() || !thisval_.toObject().is<JSFunction>()) {
    return AttachDecision::NoAction;
  }
  RootedFunction target(cx_, &thisval_.toObject().as<JSFunction>());

  bool isScripted = target->hasJitEntry();
  MOZ_ASSERT_IF(!isScripted, target->isNativeWithoutJitEntry());

  if (target->isClassConstructor()) {
    return AttachDecision::NoAction;
  }
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard that callee is the |fun_call| native function.
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificFunction(calleeObjId, callee);

  // Guard that |this| is an object.
  ValOperandId thisValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::This, argcId);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);

  if (mode_ == ICState::Mode::Specialized) {
    // Ensure that |this| is the expected target function.
    emitCalleeGuard(thisObjId, target);

    CallFlags targetFlags(CallFlags::FunCall);
    if (isScripted) {
      writer.callScriptedFunction(thisObjId, argcId, targetFlags);
    } else {
      writer.callNativeFunction(thisObjId, argcId, op_, target, targetFlags);
    }
  } else {
    // Guard that |this| is a function.
    writer.guardClass(thisObjId, GuardClassKind::JSFunction);

    // Guard that function is not a class constructor.
    writer.guardNotClassConstructor(thisObjId);

    CallFlags targetFlags(CallFlags::FunCall);
    if (isScripted) {
      writer.guardFunctionHasJitEntry(thisObjId, /*isConstructing =*/false);
      writer.callScriptedFunction(thisObjId, argcId, targetFlags);
    } else {
      writer.guardFunctionHasNoJitEntry(thisObjId);
      writer.callAnyNativeFunction(thisObjId, argcId, targetFlags);
    }
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  if (isScripted) {
    trackAttached("Scripted fun_call");
  } else {
    trackAttached("Native fun_call");
  }

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsTypedArray(HandleFunction callee,
                                                      bool isPossiblyWrapped) {
  // Self-hosted code calls this with a single object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);
  writer.isTypedArrayResult(objArgId, isPossiblyWrapped);

  // This stub does not need to be monitored because it always returns a bool.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached(isPossiblyWrapped ? "IsPossiblyWrappedTypedArray"
                                  : "IsTypedArray");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsTypedArrayConstructor(
    HandleFunction callee) {
  // Self-hosted code calls this with a single object argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);
  writer.isTypedArrayConstructorResult(objArgId);

  // This stub does not need to be monitored because it always returns a bool.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsTypedArrayConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachTypedArrayByteOffset(
    HandleFunction callee) {
  // Self-hosted code calls this with a single TypedArrayObject argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[0].toObject().is<TypedArrayObject>());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);
  writer.typedArrayByteOffsetResult(objArgId);

  // This stub does not need to be monitored because it always returns int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("TypedArrayByteOffset");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachTypedArrayElementShift(
    HandleFunction callee) {
  // Self-hosted code calls this with a single TypedArrayObject argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());
  MOZ_ASSERT(args_[0].toObject().is<TypedArrayObject>());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);
  writer.typedArrayElementShiftResult(objArgId);

  // This stub does not need to be monitored because it always returns int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("TypedArrayElementShift");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachTypedArrayLength(
    HandleFunction callee, bool isPossiblyWrapped) {
  // Self-hosted code calls this with a single, possibly wrapped,
  // TypedArrayObject argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Only optimize when the object isn't a wrapper.
  if (isPossiblyWrapped && IsWrapper(&args_[0].toObject())) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(args_[0].toObject().is<TypedArrayObject>());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);

  if (isPossiblyWrapped) {
    writer.guardIsNotProxy(objArgId);
  }

  // Note: the "getter" argument is a hint for IonBuilder. Just pass |callee|,
  // the field isn't used for this intrinsic call.
  writer.loadTypedArrayLengthResult(objArgId, callee);

  // This stub does not need to be monitored because it always returns int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("TypedArrayLength");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayBufferByteLength(
    HandleFunction callee, bool isPossiblyWrapped) {
  // Self-hosted code calls this with a single, possibly wrapped,
  // ArrayBufferObject argument.
  MOZ_ASSERT(argc_ == 1);
  MOZ_ASSERT(args_[0].isObject());

  // Only optimize when the object isn't a wrapper.
  if (isPossiblyWrapped && IsWrapper(&args_[0].toObject())) {
    return AttachDecision::NoAction;
  }

  MOZ_ASSERT(args_[0].toObject().is<ArrayBufferObject>());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objArgId = writer.guardToObject(argId);

  if (isPossiblyWrapped) {
    writer.guardIsNotProxy(objArgId);
  }

  size_t offset =
      NativeObject::getFixedSlotOffset(ArrayBufferObject::BYTE_LENGTH_SLOT);

  writer.loadFixedSlotTypedResult(objArgId, offset, ValueType::Int32);

  // This stub does not need to be monitored because it always returns int32.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ArrayBufferByteLength");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsConstructing(HandleFunction callee) {
  // Self-hosted code calls this with no arguments in function scripts.
  MOZ_ASSERT(argc_ == 0);
  MOZ_ASSERT(script_->isFunction());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  writer.frameIsConstructingResult();

  // This stub does not need to be monitored, it always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("IsConstructing");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachGetNextMapSetEntryForIterator(
    HandleFunction callee, bool isMap) {
  // Self-hosted code calls this with two objects.
  MOZ_ASSERT(argc_ == 2);
  if (isMap) {
    MOZ_ASSERT(args_[0].toObject().is<MapIteratorObject>());
  } else {
    MOZ_ASSERT(args_[0].toObject().is<SetIteratorObject>());
  }
  MOZ_ASSERT(args_[1].toObject().is<ArrayObject>());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId iterId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objIterId = writer.guardToObject(iterId);

  ValOperandId resultArrId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  ObjOperandId objResultArrId = writer.guardToObject(resultArrId);

  writer.getNextMapSetEntryForIteratorResult(objIterId, objResultArrId, isMap);

  // This stub does not need to be monitored, it always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("GetNextMapSetEntryForIterator");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachFinishBoundFunctionInit(
    HandleFunction callee) {
  // Self-hosted code calls this with (boundFunction, targetObj, argCount)
  // arguments.
  MOZ_ASSERT(argc_ == 3);
  MOZ_ASSERT(args_[0].toObject().is<JSFunction>());
  MOZ_ASSERT(args_[1].isObject());
  MOZ_ASSERT(args_[2].isInt32());

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ValOperandId boundId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  ObjOperandId objBoundId = writer.guardToObject(boundId);

  ValOperandId targetId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  ObjOperandId objTargetId = writer.guardToObject(targetId);

  ValOperandId argCountId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_);
  Int32OperandId int32ArgCountId = writer.guardToInt32(argCountId);

  writer.finishBoundFunctionInitResult(objBoundId, objTargetId,
                                       int32ArgCountId);

  // This stub does not need to be monitored, it always returns |undefined|.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("FinishBoundFunctionInit");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachNewArrayIterator(
    HandleFunction callee) {
  // Self-hosted code calls this without any arguments
  MOZ_ASSERT(argc_ == 0);

  JSObject* templateObj = NewArrayIteratorTemplate(cx_);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }
  writer.newArrayIteratorResult(templateObj);
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("NewArrayIterator");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachNewStringIterator(
    HandleFunction callee) {
  // Self-hosted code calls this without any arguments
  MOZ_ASSERT(argc_ == 0);

  JSObject* templateObj = NewStringIteratorTemplate(cx_);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }
  writer.newStringIteratorResult(templateObj);
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("NewStringIterator");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachNewRegExpStringIterator(
    HandleFunction callee) {
  // Self-hosted code calls this without any arguments
  MOZ_ASSERT(argc_ == 0);

  JSObject* templateObj = NewRegExpStringIteratorTemplate(cx_);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }
  writer.newRegExpStringIteratorResult(templateObj);
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("NewRegExpStringIterator");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayIteratorPrototypeOptimizable(
    HandleFunction callee) {
  // Self-hosted code calls this without any arguments
  MOZ_ASSERT(argc_ == 0);

  // TODO(Warp): attach this stub just once to prevent slowdowns for polymorphic
  // calls.

  NativeObject* arrayIteratorProto;
  uint32_t slot;
  JSFunction* nextFun;
  if (!IsArrayIteratorPrototypeOptimizable(cx_, &arrayIteratorProto, &slot,
                                           &nextFun)) {
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Note: we don't need to call emitNativeCalleeGuard for intrinsics.

  ObjOperandId protoId = writer.loadObject(arrayIteratorProto);
  ObjOperandId nextId = writer.loadObject(nextFun);

  writer.guardShape(protoId, arrayIteratorProto->lastProperty());

  // Ensure that proto[slot] == nextFun.
  writer.guardDynamicSlotIsSpecificObject(protoId, nextId, slot);
  writer.loadBooleanResult(true);

  // This stub does not need to be monitored, it always returns a boolean.
  writer.returnFromIC();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ArrayIteratorPrototypeOptimizable");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachObjectCreate(HandleFunction callee) {
  // Need a single object-or-null argument.
  if (argc_ != 1 || !args_[0].isObjectOrNull()) {
    return AttachDecision::NoAction;
  }

  // TODO(Warp): attach this stub just once to prevent slowdowns for
  // polymorphic calls.

  RootedObject proto(cx_, args_[0].toObjectOrNull());
  JSObject* templateObj = ObjectCreateImpl(cx_, proto, TenuredObject);
  if (!templateObj) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the 'create' native function.
  emitNativeCalleeGuard(callee);

  // Guard on the proto argument.
  ValOperandId argId = writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
  if (proto) {
    ObjOperandId protoId = writer.guardToObject(argId);
    writer.guardSpecificObject(protoId, proto);
  } else {
    writer.guardIsNull(argId);
  }

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }

  writer.objectCreateResult(templateObj);
  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("ObjectCreate");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachArrayConstructor(
    HandleFunction callee) {
  // Only optimize the |Array()| and |Array(n)| cases (with or without |new|)
  // for now. Note that self-hosted code calls this without |new| via std_Array.
  if (argc_ > 1) {
    return AttachDecision::NoAction;
  }
  if (argc_ == 1 && !args_[0].isInt32()) {
    return AttachDecision::NoAction;
  }

  int32_t length = (argc_ == 1) ? args_[0].toInt32() : 0;
  if (length < 0 || uint32_t(length) > ArrayObject::EagerAllocationMaxLength) {
    return AttachDecision::NoAction;
  }

  // We allow inlining this function across realms so make sure the template
  // object is allocated in that realm. See CanInlineNativeCrossRealm.
  JSObject* templateObj;
  {
    AutoRealm ar(cx_, callee);
    templateObj = NewFullyAllocatedArrayForCallingAllocationSite(cx_, length,
                                                                 TenuredObject);
    if (!templateObj) {
      cx_->recoverFromOutOfMemory();
      return AttachDecision::NoAction;
    }
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee and newTarget (if constructing) are this Array constructor
  // function.
  emitNativeCalleeGuard(callee);

  CallFlags flags(IsConstructPC(pc_), IsSpreadPC(pc_));

  Int32OperandId lengthId;
  if (argc_ == 1) {
    ValOperandId arg0Id =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_, flags);
    lengthId = writer.guardToInt32(arg0Id);
  } else {
    MOZ_ASSERT(argc_ == 0);
    lengthId = writer.loadInt32Constant(0);
  }

  writer.newArrayFromLengthResult(templateObj, lengthId);

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("ArrayConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachTypedArrayConstructor(
    HandleFunction callee) {
  MOZ_ASSERT(IsConstructPC(pc_));

  if (argc_ == 0 || argc_ > 3) {
    return AttachDecision::NoAction;
  }

  // TODO(Warp); attach this stub just once to prevent slowdowns for
  // polymorphic calls.

  // The first argument must be int32 or a non-proxy object.
  if (!args_[0].isInt32() && !args_[0].isObject()) {
    return AttachDecision::NoAction;
  }
  if (args_[0].isObject() && args_[0].toObject().is<ProxyObject>()) {
    return AttachDecision::NoAction;
  }

#ifdef JS_CODEGEN_X86
  // Unfortunately NewTypedArrayFromArrayBufferResult needs more registers than
  // we can easily support on 32-bit x86 for now.
  if (args_[0].isObject() &&
      args_[0].toObject().is<ArrayBufferObjectMaybeShared>()) {
    return AttachDecision::NoAction;
  }
#endif

  RootedObject templateObj(cx_);
  if (!TypedArrayObject::GetTemplateObjectForNative(cx_, callee->native(),
                                                    args_, &templateObj)) {
    cx_->recoverFromOutOfMemory();
    return AttachDecision::NoAction;
  }

  if (!templateObj) {
    // This can happen for large length values.
    MOZ_ASSERT(args_[0].isInt32());
    return AttachDecision::NoAction;
  }

  // Initialize the input operand.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee and newTarget are this TypedArray constructor function.
  emitNativeCalleeGuard(callee);

  CallFlags flags(IsConstructPC(pc_), IsSpreadPC(pc_));
  ValOperandId arg0Id =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_, flags);

  if (args_[0].isInt32()) {
    // From length.
    Int32OperandId lengthId = writer.guardToInt32(arg0Id);
    writer.newTypedArrayFromLengthResult(templateObj, lengthId);
  } else {
    JSObject* obj = &args_[0].toObject();
    ObjOperandId objId = writer.guardToObject(arg0Id);

    if (obj->is<ArrayBufferObjectMaybeShared>()) {
      // From ArrayBuffer.
      if (obj->is<ArrayBufferObject>()) {
        writer.guardClass(objId, GuardClassKind::ArrayBuffer);
      } else {
        MOZ_ASSERT(obj->is<SharedArrayBufferObject>());
        writer.guardClass(objId, GuardClassKind::SharedArrayBuffer);
      }
      ValOperandId byteOffsetId;
      if (argc_ > 1) {
        byteOffsetId =
            writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_, flags);
      } else {
        byteOffsetId = writer.loadUndefined();
      }
      ValOperandId lengthId;
      if (argc_ > 2) {
        lengthId =
            writer.loadArgumentFixedSlot(ArgumentKind::Arg2, argc_, flags);
      } else {
        lengthId = writer.loadUndefined();
      }
      writer.newTypedArrayFromArrayBufferResult(templateObj, objId,
                                                byteOffsetId, lengthId);
    } else {
      // From Array-like.
      writer.guardIsNotArrayBufferMaybeShared(objId);
      writer.guardIsNotProxy(objId);
      writer.newTypedArrayFromArrayResult(templateObj, objId);
    }
  }

  if (!JitOptions.warpBuilder) {
    // Store the template object for BaselineInspector.
    writer.metaNativeTemplateObject(callee, templateObj);
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  trackAttached("TypedArrayConstructor");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachFunApply(HandleFunction calleeFunc) {
  if (!calleeFunc->isNativeWithoutJitEntry() ||
      calleeFunc->native() != fun_apply) {
    return AttachDecision::NoAction;
  }

  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  if (!thisval_.isObject() || !thisval_.toObject().is<JSFunction>()) {
    return AttachDecision::NoAction;
  }
  RootedFunction target(cx_, &thisval_.toObject().as<JSFunction>());

  bool isScripted = target->hasJitEntry();
  MOZ_ASSERT_IF(!isScripted, target->isNativeWithoutJitEntry());

  if (target->isClassConstructor()) {
    return AttachDecision::NoAction;
  }

  CallFlags::ArgFormat format = CallFlags::Standard;
  if (args_[1].isMagic(JS_OPTIMIZED_ARGUMENTS) && !script_->needsArgsObj()) {
    format = CallFlags::FunApplyArgs;
  } else if (args_[1].isObject() && args_[1].toObject().is<ArrayObject>() &&
             args_[1].toObject().as<ArrayObject>().length() <=
                 JIT_ARGS_LENGTH_MAX) {
    format = CallFlags::FunApplyArray;
  } else {
    return AttachDecision::NoAction;
  }

  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard that callee is the |fun_apply| native function.
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificFunction(calleeObjId, calleeFunc);

  // Guard that |this| is an object.
  ValOperandId thisValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::This, argcId);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);

  ValOperandId argValId =
      writer.loadArgumentFixedSlot(ArgumentKind::Arg1, argc_);
  if (format == CallFlags::FunApplyArgs) {
    writer.guardMagicValue(argValId, JS_OPTIMIZED_ARGUMENTS);
    writer.guardFrameHasNoArgumentsObject();
  } else {
    MOZ_ASSERT(format == CallFlags::FunApplyArray);
    ObjOperandId argObjId = writer.guardToObject(argValId);
    writer.guardClass(argObjId, GuardClassKind::Array);
    writer.guardArrayIsPacked(argObjId);
  }

  CallFlags targetFlags(format);
  if (mode_ == ICState::Mode::Specialized) {
    // Ensure that |this| is the expected target function.
    emitCalleeGuard(thisObjId, target);

    if (isScripted) {
      writer.callScriptedFunction(thisObjId, argcId, targetFlags);
    } else {
      writer.callNativeFunction(thisObjId, argcId, op_, target, targetFlags);
    }
  } else {
    // Guard that |this| is a function.
    writer.guardClass(thisObjId, GuardClassKind::JSFunction);

    // Guard that function is not a class constructor.
    writer.guardNotClassConstructor(thisObjId);

    if (isScripted) {
      // Guard that function is scripted.
      writer.guardFunctionHasJitEntry(thisObjId, /*constructing =*/false);
      writer.callScriptedFunction(thisObjId, argcId, targetFlags);
    } else {
      // Guard that function is native.
      writer.guardFunctionHasNoJitEntry(thisObjId);
      writer.callAnyNativeFunction(thisObjId, argcId, targetFlags);
    }
  }

  writer.typeMonitorResult();
  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;

  if (isScripted) {
    trackAttached("Scripted fun_apply");
  } else {
    trackAttached("Native fun_apply");
  }

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachInlinableNative(
    HandleFunction callee) {
  MOZ_ASSERT(mode_ == ICState::Mode::Specialized);
  MOZ_ASSERT(callee->isNativeWithoutJitEntry());

  // Special case functions are only optimized for normal calls.
  if (op_ != JSOp::Call && op_ != JSOp::New && op_ != JSOp::CallIgnoresRv) {
    return AttachDecision::NoAction;
  }

  if (!callee->hasJitInfo() ||
      callee->jitInfo()->type() != JSJitInfo::InlinableNative) {
    return AttachDecision::NoAction;
  }

  InlinableNative native = callee->jitInfo()->inlinableNative;

  // Not all natives can be inlined cross-realm.
  if (cx_->realm() != callee->realm() && !CanInlineNativeCrossRealm(native)) {
    return AttachDecision::NoAction;
  }

  // Check for special-cased native constructors.
  if (op_ == JSOp::New) {
    // newTarget must match the callee. CacheIR for this is emitted in
    // emitNativeCalleeGuard.
    if (callee_ != newTarget_) {
      return AttachDecision::NoAction;
    }
    switch (native) {
      case InlinableNative::Array:
        return tryAttachArrayConstructor(callee);
      case InlinableNative::TypedArrayConstructor:
        return tryAttachTypedArrayConstructor(callee);
      case InlinableNative::String:
        return tryAttachStringConstructor(callee);
      default:
        break;
    }
    return AttachDecision::NoAction;
  }

  // Check for special-cased native functions.
  switch (native) {
    // Array natives.
    case InlinableNative::Array:
      return tryAttachArrayConstructor(callee);
    case InlinableNative::ArrayPush:
      return tryAttachArrayPush(callee);
    case InlinableNative::ArrayPop:
    case InlinableNative::ArrayShift:
      return tryAttachArrayPopShift(callee, native);
    case InlinableNative::ArrayJoin:
      return tryAttachArrayJoin(callee);
    case InlinableNative::ArraySlice:
      return tryAttachArraySlice(callee);
    case InlinableNative::ArrayIsArray:
      return tryAttachArrayIsArray(callee);

    // DataView natives.
    case InlinableNative::DataViewGetInt8:
      return tryAttachDataViewGet(callee, Scalar::Int8);
    case InlinableNative::DataViewGetUint8:
      return tryAttachDataViewGet(callee, Scalar::Uint8);
    case InlinableNative::DataViewGetInt16:
      return tryAttachDataViewGet(callee, Scalar::Int16);
    case InlinableNative::DataViewGetUint16:
      return tryAttachDataViewGet(callee, Scalar::Uint16);
    case InlinableNative::DataViewGetInt32:
      return tryAttachDataViewGet(callee, Scalar::Int32);
    case InlinableNative::DataViewGetUint32:
      return tryAttachDataViewGet(callee, Scalar::Uint32);
    case InlinableNative::DataViewGetFloat32:
      return tryAttachDataViewGet(callee, Scalar::Float32);
    case InlinableNative::DataViewGetFloat64:
      return tryAttachDataViewGet(callee, Scalar::Float64);
    case InlinableNative::DataViewGetBigInt64:
      return tryAttachDataViewGet(callee, Scalar::BigInt64);
    case InlinableNative::DataViewGetBigUint64:
      return tryAttachDataViewGet(callee, Scalar::BigUint64);
    case InlinableNative::DataViewSetInt8:
      return tryAttachDataViewSet(callee, Scalar::Int8);
    case InlinableNative::DataViewSetUint8:
      return tryAttachDataViewSet(callee, Scalar::Uint8);
    case InlinableNative::DataViewSetInt16:
      return tryAttachDataViewSet(callee, Scalar::Int16);
    case InlinableNative::DataViewSetUint16:
      return tryAttachDataViewSet(callee, Scalar::Uint16);
    case InlinableNative::DataViewSetInt32:
      return tryAttachDataViewSet(callee, Scalar::Int32);
    case InlinableNative::DataViewSetUint32:
      return tryAttachDataViewSet(callee, Scalar::Uint32);
    case InlinableNative::DataViewSetFloat32:
      return tryAttachDataViewSet(callee, Scalar::Float32);
    case InlinableNative::DataViewSetFloat64:
      return tryAttachDataViewSet(callee, Scalar::Float64);
    case InlinableNative::DataViewSetBigInt64:
      return tryAttachDataViewSet(callee, Scalar::BigInt64);
    case InlinableNative::DataViewSetBigUint64:
      return tryAttachDataViewSet(callee, Scalar::BigUint64);

    // Intl natives.
    case InlinableNative::IntlGuardToCollator:
    case InlinableNative::IntlGuardToDateTimeFormat:
    case InlinableNative::IntlGuardToDisplayNames:
    case InlinableNative::IntlGuardToListFormat:
    case InlinableNative::IntlGuardToNumberFormat:
    case InlinableNative::IntlGuardToPluralRules:
    case InlinableNative::IntlGuardToRelativeTimeFormat:
      return tryAttachGuardToClass(callee, native);

    // Slot intrinsics.
    case InlinableNative::IntrinsicUnsafeGetReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetObjectFromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetInt32FromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetStringFromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetBooleanFromReservedSlot:
      return tryAttachUnsafeGetReservedSlot(callee, native);
    case InlinableNative::IntrinsicUnsafeSetReservedSlot:
      return tryAttachUnsafeSetReservedSlot(callee);

    // Intrinsics.
    case InlinableNative::IntrinsicIsSuspendedGenerator:
      return tryAttachIsSuspendedGenerator(callee);
    case InlinableNative::IntrinsicToObject:
      return tryAttachToObject(callee, native);
    case InlinableNative::IntrinsicToInteger:
      return tryAttachToInteger(callee);
    case InlinableNative::IntrinsicToLength:
      return tryAttachToLength(callee);
    case InlinableNative::IntrinsicIsObject:
      return tryAttachIsObject(callee);
    case InlinableNative::IntrinsicIsPackedArray:
      return tryAttachIsPackedArray(callee);
    case InlinableNative::IntrinsicIsCallable:
      return tryAttachIsCallable(callee);
    case InlinableNative::IntrinsicIsConstructor:
      return tryAttachIsConstructor(callee);
    case InlinableNative::IntrinsicIsCrossRealmArrayConstructor:
      return tryAttachIsCrossRealmArrayConstructor(callee);
    case InlinableNative::IntrinsicGuardToArrayIterator:
    case InlinableNative::IntrinsicGuardToMapIterator:
    case InlinableNative::IntrinsicGuardToSetIterator:
    case InlinableNative::IntrinsicGuardToStringIterator:
    case InlinableNative::IntrinsicGuardToRegExpStringIterator:
    case InlinableNative::IntrinsicGuardToWrapForValidIterator:
    case InlinableNative::IntrinsicGuardToIteratorHelper:
    case InlinableNative::IntrinsicGuardToAsyncIteratorHelper:
      return tryAttachGuardToClass(callee, native);
    case InlinableNative::IntrinsicSubstringKernel:
      return tryAttachSubstringKernel(callee);
    case InlinableNative::IntrinsicIsConstructing:
      return tryAttachIsConstructing(callee);
    case InlinableNative::IntrinsicFinishBoundFunctionInit:
      return tryAttachFinishBoundFunctionInit(callee);
    case InlinableNative::IntrinsicNewArrayIterator:
      return tryAttachNewArrayIterator(callee);
    case InlinableNative::IntrinsicNewStringIterator:
      return tryAttachNewStringIterator(callee);
    case InlinableNative::IntrinsicNewRegExpStringIterator:
      return tryAttachNewRegExpStringIterator(callee);
    case InlinableNative::IntrinsicArrayIteratorPrototypeOptimizable:
      return tryAttachArrayIteratorPrototypeOptimizable(callee);
    case InlinableNative::IntrinsicObjectHasPrototype:
      return tryAttachObjectHasPrototype(callee);

    // RegExp natives.
    case InlinableNative::IsRegExpObject:
      return tryAttachHasClass(callee, &RegExpObject::class_,
                               /* isPossiblyWrapped = */ false);
    case InlinableNative::IsPossiblyWrappedRegExpObject:
      return tryAttachHasClass(callee, &RegExpObject::class_,
                               /* isPossiblyWrapped = */ true);
    case InlinableNative::RegExpMatcher:
    case InlinableNative::RegExpSearcher:
    case InlinableNative::RegExpTester:
      return tryAttachRegExpMatcherSearcherTester(callee, native);
    case InlinableNative::RegExpPrototypeOptimizable:
      return tryAttachRegExpPrototypeOptimizable(callee);
    case InlinableNative::RegExpInstanceOptimizable:
      return tryAttachRegExpInstanceOptimizable(callee);
    case InlinableNative::GetFirstDollarIndex:
      return tryAttachGetFirstDollarIndex(callee);

    // String natives.
    case InlinableNative::String:
      return tryAttachString(callee);
    case InlinableNative::StringToString:
    case InlinableNative::StringValueOf:
      return tryAttachStringToStringValueOf(callee);
    case InlinableNative::StringCharCodeAt:
      return tryAttachStringCharCodeAt(callee);
    case InlinableNative::StringCharAt:
      return tryAttachStringCharAt(callee);
    case InlinableNative::StringFromCharCode:
      return tryAttachStringFromCharCode(callee);
    case InlinableNative::StringFromCodePoint:
      return tryAttachStringFromCodePoint(callee);
    case InlinableNative::StringToLowerCase:
      return tryAttachStringToLowerCase(callee);
    case InlinableNative::StringToUpperCase:
      return tryAttachStringToUpperCase(callee);
    case InlinableNative::IntrinsicStringReplaceString:
      return tryAttachStringReplaceString(callee);
    case InlinableNative::IntrinsicStringSplitString:
      return tryAttachStringSplitString(callee);

    // Math natives.
    case InlinableNative::MathRandom:
      return tryAttachMathRandom(callee);
    case InlinableNative::MathAbs:
      return tryAttachMathAbs(callee);
    case InlinableNative::MathClz32:
      return tryAttachMathClz32(callee);
    case InlinableNative::MathSign:
      return tryAttachMathSign(callee);
    case InlinableNative::MathImul:
      return tryAttachMathImul(callee);
    case InlinableNative::MathFloor:
      return tryAttachMathFloor(callee);
    case InlinableNative::MathCeil:
      return tryAttachMathCeil(callee);
    case InlinableNative::MathTrunc:
      return tryAttachMathTrunc(callee);
    case InlinableNative::MathRound:
      return tryAttachMathRound(callee);
    case InlinableNative::MathSqrt:
      return tryAttachMathSqrt(callee);
    case InlinableNative::MathFRound:
      return tryAttachMathFRound(callee);
    case InlinableNative::MathHypot:
      return tryAttachMathHypot(callee);
    case InlinableNative::MathATan2:
      return tryAttachMathATan2(callee);
    case InlinableNative::MathSin:
      return tryAttachMathFunction(callee, UnaryMathFunction::Sin);
    case InlinableNative::MathTan:
      return tryAttachMathFunction(callee, UnaryMathFunction::Tan);
    case InlinableNative::MathCos:
      return tryAttachMathFunction(callee, UnaryMathFunction::Cos);
    case InlinableNative::MathExp:
      return tryAttachMathFunction(callee, UnaryMathFunction::Exp);
    case InlinableNative::MathLog:
      return tryAttachMathFunction(callee, UnaryMathFunction::Log);
    case InlinableNative::MathASin:
      return tryAttachMathFunction(callee, UnaryMathFunction::ASin);
    case InlinableNative::MathATan:
      return tryAttachMathFunction(callee, UnaryMathFunction::ATan);
    case InlinableNative::MathACos:
      return tryAttachMathFunction(callee, UnaryMathFunction::ACos);
    case InlinableNative::MathLog10:
      return tryAttachMathFunction(callee, UnaryMathFunction::Log10);
    case InlinableNative::MathLog2:
      return tryAttachMathFunction(callee, UnaryMathFunction::Log2);
    case InlinableNative::MathLog1P:
      return tryAttachMathFunction(callee, UnaryMathFunction::Log1P);
    case InlinableNative::MathExpM1:
      return tryAttachMathFunction(callee, UnaryMathFunction::ExpM1);
    case InlinableNative::MathCosH:
      return tryAttachMathFunction(callee, UnaryMathFunction::CosH);
    case InlinableNative::MathSinH:
      return tryAttachMathFunction(callee, UnaryMathFunction::SinH);
    case InlinableNative::MathTanH:
      return tryAttachMathFunction(callee, UnaryMathFunction::TanH);
    case InlinableNative::MathACosH:
      return tryAttachMathFunction(callee, UnaryMathFunction::ACosH);
    case InlinableNative::MathASinH:
      return tryAttachMathFunction(callee, UnaryMathFunction::ASinH);
    case InlinableNative::MathATanH:
      return tryAttachMathFunction(callee, UnaryMathFunction::ATanH);
    case InlinableNative::MathCbrt:
      return tryAttachMathFunction(callee, UnaryMathFunction::Cbrt);
    case InlinableNative::MathPow:
      return tryAttachMathPow(callee);
    case InlinableNative::MathMin:
      return tryAttachMathMinMax(callee, /* isMax = */ false);
    case InlinableNative::MathMax:
      return tryAttachMathMinMax(callee, /* isMax = */ true);

    // Map intrinsics.
    case InlinableNative::IntrinsicGuardToMapObject:
      return tryAttachGuardToClass(callee, native);
    case InlinableNative::IntrinsicGetNextMapEntryForIterator:
      return tryAttachGetNextMapSetEntryForIterator(callee, /* isMap = */ true);

    // Number natives.
    case InlinableNative::NumberToString:
      return tryAttachNumberToString(callee);

    // Object natives.
    case InlinableNative::Object:
      return tryAttachToObject(callee, native);
    case InlinableNative::ObjectCreate:
      return tryAttachObjectCreate(callee);
    case InlinableNative::ObjectIs:
      return tryAttachObjectIs(callee);
    case InlinableNative::ObjectIsPrototypeOf:
      return tryAttachObjectIsPrototypeOf(callee);
    case InlinableNative::ObjectToString:
      return AttachDecision::NoAction;  // Not yet supported.

    // Set intrinsics.
    case InlinableNative::IntrinsicGuardToSetObject:
      return tryAttachGuardToClass(callee, native);
    case InlinableNative::IntrinsicGetNextSetEntryForIterator:
      return tryAttachGetNextMapSetEntryForIterator(callee,
                                                    /* isMap = */ false);

    // ArrayBuffer intrinsics.
    case InlinableNative::IntrinsicGuardToArrayBuffer:
      return tryAttachGuardToClass(callee, native);
    case InlinableNative::IntrinsicArrayBufferByteLength:
      return tryAttachArrayBufferByteLength(callee,
                                            /* isPossiblyWrapped = */ false);
    case InlinableNative::IntrinsicPossiblyWrappedArrayBufferByteLength:
      return tryAttachArrayBufferByteLength(callee,
                                            /* isPossiblyWrapped = */ true);

    // SharedArrayBuffer intrinsics.
    case InlinableNative::IntrinsicGuardToSharedArrayBuffer:
      return tryAttachGuardToClass(callee, native);

    // TypedArray intrinsics.
    case InlinableNative::TypedArrayConstructor:
      return AttachDecision::NoAction;  // Not callable.
    case InlinableNative::IntrinsicIsTypedArray:
      return tryAttachIsTypedArray(callee, /* isPossiblyWrapped = */ false);
    case InlinableNative::IntrinsicIsPossiblyWrappedTypedArray:
      return tryAttachIsTypedArray(callee, /* isPossiblyWrapped = */ true);
    case InlinableNative::IntrinsicIsTypedArrayConstructor:
      return tryAttachIsTypedArrayConstructor(callee);
    case InlinableNative::IntrinsicTypedArrayByteOffset:
      return tryAttachTypedArrayByteOffset(callee);
    case InlinableNative::IntrinsicTypedArrayElementShift:
      return tryAttachTypedArrayElementShift(callee);
    case InlinableNative::IntrinsicTypedArrayLength:
      return tryAttachTypedArrayLength(callee, /* isPossiblyWrapped = */ false);
    case InlinableNative::IntrinsicPossiblyWrappedTypedArrayLength:
      return tryAttachTypedArrayLength(callee, /* isPossiblyWrapped = */ true);

    // Reflect natives.
    case InlinableNative::ReflectGetPrototypeOf:
      return tryAttachReflectGetPrototypeOf(callee);

    // Atomics intrinsics:
    case InlinableNative::AtomicsCompareExchange:
      return tryAttachAtomicsCompareExchange(callee);
    case InlinableNative::AtomicsExchange:
      return tryAttachAtomicsExchange(callee);
    case InlinableNative::AtomicsAdd:
      return tryAttachAtomicsAdd(callee);
    case InlinableNative::AtomicsSub:
      return tryAttachAtomicsSub(callee);
    case InlinableNative::AtomicsAnd:
      return tryAttachAtomicsAnd(callee);
    case InlinableNative::AtomicsOr:
      return tryAttachAtomicsOr(callee);
    case InlinableNative::AtomicsXor:
      return tryAttachAtomicsXor(callee);
    case InlinableNative::AtomicsLoad:
      return tryAttachAtomicsLoad(callee);
    case InlinableNative::AtomicsStore:
      return tryAttachAtomicsStore(callee);
    case InlinableNative::AtomicsIsLockFree:
      return tryAttachAtomicsIsLockFree(callee);

    // Boolean natives.
    case InlinableNative::Boolean:
      return tryAttachBoolean(callee);

    // Testing functions.
    case InlinableNative::TestBailout:
      return tryAttachBailout(callee);
    case InlinableNative::TestAssertFloat32:
      return tryAttachAssertFloat32(callee);
    case InlinableNative::TestAssertRecoveredOnBailout:
      return tryAttachAssertRecoveredOnBailout(callee);

    case InlinableNative::Limit:
      break;
  }

  MOZ_CRASH("Shouldn't get here");
}

// Remember the template object associated with any script being called
// as a constructor, for later use during Ion compilation.
bool CallIRGenerator::getTemplateObjectForScripted(HandleFunction calleeFunc,
                                                   MutableHandleObject result,
                                                   bool* skipAttach) {
  MOZ_ASSERT(!*skipAttach);

  // Some constructors allocate their own |this| object.
  if (calleeFunc->constructorNeedsUninitializedThis()) {
    return true;
  }

  // Don't allocate a template object for super() calls as Ion doesn't support
  // super() yet.
  bool isSuper = op_ == JSOp::SuperCall || op_ == JSOp::SpreadSuperCall;
  if (isSuper) {
    return true;
  }

  // Only attach a stub if the newTarget is a function that already has a
  // prototype and we can look it up without causing side effects.
  RootedValue protov(cx_);
  RootedObject newTarget(cx_, &newTarget_.toObject());
  if (!newTarget->is<JSFunction>() ||
      !newTarget->as<JSFunction>().hasNonConfigurablePrototypeDataProperty()) {
    trackAttached(IRGenerator::NotAttached);
    *skipAttach = true;
    return true;
  }
  if (!GetPropertyPure(cx_, newTarget, NameToId(cx_->names().prototype),
                       protov.address())) {
    // Can't purely lookup function prototype
    trackAttached(IRGenerator::NotAttached);
    *skipAttach = true;
    return true;
  }

  if (!protov.isObject()) {
    *skipAttach = true;
    return true;
  }

  {
    AutoRealm ar(cx_, calleeFunc);
    TaggedProto proto(&protov.toObject());
    ObjectGroup* group = ObjectGroup::defaultNewGroup(cx_, &PlainObject::class_,
                                                      proto, newTarget);
    if (!group) {
      return false;
    }

    AutoSweepObjectGroup sweep(group);
    if (group->newScript(sweep) && !group->newScript(sweep)->analyzed()) {
      // Function newScript has not been analyzed
      trackAttached(IRGenerator::NotAttached);
      *skipAttach = true;
      return true;
    }
  }

  PlainObject* thisObject =
      CreateThisForFunction(cx_, calleeFunc, newTarget, TenuredObject);
  if (!thisObject) {
    return false;
  }

  MOZ_ASSERT(thisObject->nonCCWRealm() == calleeFunc->realm());
  result.set(thisObject);
  return true;
}

AttachDecision CallIRGenerator::tryAttachCallScripted(
    HandleFunction calleeFunc) {
  MOZ_ASSERT(calleeFunc->hasJitEntry());

  // Never attach optimized scripted call stubs for JSOp::FunApply.
  // MagicArguments may escape the frame through them.
  if (op_ == JSOp::FunApply) {
    return AttachDecision::NoAction;
  }

  bool isSpecialized = mode_ == ICState::Mode::Specialized;

  bool isConstructing = IsConstructPC(pc_);
  bool isSpread = IsSpreadPC(pc_);
  bool isSameRealm = isSpecialized && cx_->realm() == calleeFunc->realm();
  CallFlags flags(isConstructing, isSpread, isSameRealm);

  // If callee is not an interpreted constructor, we have to throw.
  if (isConstructing && !calleeFunc->isConstructor()) {
    return AttachDecision::NoAction;
  }

  // Likewise, if the callee is a class constructor, we have to throw.
  if (!isConstructing && calleeFunc->isClassConstructor()) {
    return AttachDecision::NoAction;
  }

  if (isConstructing && !calleeFunc->hasJitScript()) {
    // If we're constructing, require the callee to have a JitScript. This isn't
    // required for correctness but avoids allocating a template object below
    // for constructors that aren't hot. See bug 1419758.
    return AttachDecision::TemporarilyUnoptimizable;
  }

  // Verify that spread calls have a reasonable number of arguments.
  if (isSpread && args_.length() > JIT_ARGS_LENGTH_MAX) {
    return AttachDecision::NoAction;
  }

  // Keep track of the function's |prototype| property in type
  // information, for use during Ion compilation.
  if (IsIonEnabled(cx_)) {
    EnsureTrackPropertyTypes(cx_, calleeFunc, NameToId(cx_->names().prototype));
  }

  RootedObject templateObj(cx_);
  bool skipAttach = false;
  if (isConstructing && isSpecialized &&
      !getTemplateObjectForScripted(calleeFunc, &templateObj, &skipAttach)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }
  if (skipAttach) {
    return AttachDecision::TemporarilyUnoptimizable;
  }

  if (isConstructing && isSpecialized &&
      calleeFunc->constructorNeedsUninitializedThis()) {
    flags.setNeedsUninitializedThis();
  }

  // Load argc.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Load the callee and ensure it is an object
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId, flags);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);

  if (isSpecialized) {
    // Ensure callee matches this stub's callee
    emitCalleeGuard(calleeObjId, calleeFunc);
    if (templateObj) {
      // Call metaScriptedTemplateObject before emitting the call, so that Warp
      // can use this template object before transpiling the call.
      MOZ_ASSERT(!flags.needsUninitializedThis());
      if (JitOptions.warpBuilder) {
        // Emit guards to ensure the newTarget's .prototype property is what we
        // expect. Note that getTemplateObjectForScripted checked newTarget is a
        // function with a non-configurable .prototype data property.
        JSFunction* newTarget = &newTarget_.toObject().as<JSFunction>();
        Shape* shape = newTarget->lookupPure(cx_->names().prototype);
        MOZ_ASSERT(shape);
        MOZ_ASSERT(newTarget->numFixedSlots() == 0, "Stub code relies on this");
        uint32_t slot = shape->slot();
        JSObject* prototypeObject = &newTarget->getSlot(slot).toObject();

        ValOperandId newTargetValId = writer.loadArgumentDynamicSlot(
            ArgumentKind::NewTarget, argcId, flags);
        ObjOperandId newTargetObjId = writer.guardToObject(newTargetValId);
        writer.guardShape(newTargetObjId, newTarget->lastProperty());
        ObjOperandId protoId = writer.loadObject(prototypeObject);
        writer.guardDynamicSlotIsSpecificObject(newTargetObjId, protoId, slot);
      }
      writer.metaScriptedTemplateObject(calleeFunc, templateObj);
    }
  } else {
    // Guard that object is a scripted function
    writer.guardClass(calleeObjId, GuardClassKind::JSFunction);
    writer.guardFunctionHasJitEntry(calleeObjId, isConstructing);

    if (isConstructing) {
      // If callee is not a constructor, we have to throw.
      writer.guardFunctionIsConstructor(calleeObjId);
    } else {
      // If callee is a class constructor, we have to throw.
      writer.guardNotClassConstructor(calleeObjId);
    }
  }

  writer.callScriptedFunction(calleeObjId, argcId, flags);
  writer.typeMonitorResult();

  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;
  if (isSpecialized) {
    trackAttached("Call scripted func");
  } else {
    trackAttached("Call any scripted func");
  }

  return AttachDecision::Attach;
}

bool CallIRGenerator::getTemplateObjectForNative(HandleFunction calleeFunc,
                                                 MutableHandleObject res) {
  AutoRealm ar(cx_, calleeFunc);

  // Don't allocate a template object for super() calls as Ion doesn't support
  // super() yet.
  bool isSuper = op_ == JSOp::SuperCall || op_ == JSOp::SpreadSuperCall;
  if (isSuper) {
    return true;
  }

  if (!calleeFunc->hasJitInfo() ||
      calleeFunc->jitInfo()->type() != JSJitInfo::InlinableNative) {
    return true;
  }

  bool isConstructing = IsConstructOp(op_);

  // Check for natives to which template objects can be attached. This is
  // done to provide templates to Ion for inlining these natives later on.
  switch (calleeFunc->jitInfo()->inlinableNative) {
    case InlinableNative::Array: {
      // Note: the template array won't be used if its length is inaccurately
      // computed here.  (We allocate here because compilation may occur on a
      // separate thread where allocation is impossible.)

      if (args_.length() <= 1) {
        // This case is handled by tryAttachArrayConstructor.
        return true;
      }

      size_t count = args_.length();
      if (count > ArrayObject::EagerAllocationMaxLength) {
        return true;
      }

      // With this and other array templates, analyze the group so that
      // we don't end up with a template whose structure might change later.
      res.set(NewFullyAllocatedArrayForCallingAllocationSite(cx_, count,
                                                             TenuredObject));
      return !!res;
    }

    case InlinableNative::ArraySlice: {
      if (!thisval_.isObject()) {
        return true;
      }

      RootedObject obj(cx_, &thisval_.toObject());
      if (obj->isSingleton()) {
        return true;
      }

      if (IsPackedArray(obj)) {
        // This case is handled by tryAttachArraySlice.
        return true;
      }

      // TODO(Warp): Support non-packed arrays in tryAttachArraySlice if they're
      // common in user code.
      if (JitOptions.warpBuilder) {
        return true;
      }

      res.set(NewFullyAllocatedArrayTryReuseGroup(cx_, obj, 0, TenuredObject));
      return !!res;
    }

    case InlinableNative::String: {
      if (!isConstructing) {
        return true;
      }

      if (args_.length() == 1 && args_[0].isString()) {
        // This case is handled by tryAttachStringConstructor.
        return true;
      }

      RootedString emptyString(cx_, cx_->runtime()->emptyString);
      res.set(StringObject::create(cx_, emptyString, /* proto = */ nullptr,
                                   TenuredObject));
      return !!res;
    }

    default:
      return true;
  }
}

AttachDecision CallIRGenerator::tryAttachCallNative(HandleFunction calleeFunc) {
  MOZ_ASSERT(calleeFunc->isNativeWithoutJitEntry());

  bool isSpecialized = mode_ == ICState::Mode::Specialized;

  bool isSpread = IsSpreadPC(pc_);
  bool isSameRealm = isSpecialized && cx_->realm() == calleeFunc->realm();
  bool isConstructing = IsConstructPC(pc_);
  CallFlags flags(isConstructing, isSpread, isSameRealm);

  if (isConstructing && !calleeFunc->isConstructor()) {
    return AttachDecision::NoAction;
  }

  // Verify that spread calls have a reasonable number of arguments.
  if (isSpread && args_.length() > JIT_ARGS_LENGTH_MAX) {
    return AttachDecision::NoAction;
  }

  // Check for specific native-function optimizations.
  if (isSpecialized) {
    TRY_ATTACH(tryAttachInlinableNative(calleeFunc));
  }

  RootedObject templateObj(cx_);
  if (isSpecialized && !getTemplateObjectForNative(calleeFunc, &templateObj)) {
    cx_->clearPendingException();
    return AttachDecision::NoAction;
  }

  // Load argc.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Load the callee and ensure it is an object
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId, flags);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);

  if (isSpecialized) {
    // Ensure callee matches this stub's callee
    writer.guardSpecificFunction(calleeObjId, calleeFunc);
    writer.callNativeFunction(calleeObjId, argcId, op_, calleeFunc, flags);
  } else {
    // Guard that object is a native function
    writer.guardClass(calleeObjId, GuardClassKind::JSFunction);
    writer.guardFunctionHasNoJitEntry(calleeObjId);

    if (isConstructing) {
      // If callee is not a constructor, we have to throw.
      writer.guardFunctionIsConstructor(calleeObjId);
    } else {
      // If callee is a class constructor, we have to throw.
      writer.guardNotClassConstructor(calleeObjId);
    }
    writer.callAnyNativeFunction(calleeObjId, argcId, flags);
  }

  writer.typeMonitorResult();

  if (templateObj) {
    MOZ_ASSERT(isSpecialized);
    writer.metaNativeTemplateObject(calleeFunc, templateObj);
  }

  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;
  if (isSpecialized) {
    trackAttached("Call native func");
  } else {
    trackAttached("Call any native func");
  }

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachCallHook(HandleObject calleeObj) {
  if (op_ == JSOp::FunCall || op_ == JSOp::FunApply) {
    return AttachDecision::NoAction;
  }

  if (mode_ != ICState::Mode::Specialized) {
    // We do not have megamorphic call hook stubs.
    // TODO: Should we attach specialized call hook stubs in
    // megamorphic mode to avoid going generic?
    return AttachDecision::NoAction;
  }

  bool isSpread = IsSpreadPC(pc_);
  bool isConstructing = IsConstructPC(pc_);
  CallFlags flags(isConstructing, isSpread);
  JSNative hook =
      isConstructing ? calleeObj->constructHook() : calleeObj->callHook();
  if (!hook) {
    return AttachDecision::NoAction;
  }

  // Verify that spread calls have a reasonable number of arguments.
  if (isSpread && args_.length() > JIT_ARGS_LENGTH_MAX) {
    return AttachDecision::NoAction;
  }

  // Load argc.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Load the callee and ensure it is an object
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId, flags);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);

  // Ensure the callee's class matches the one in this stub.
  writer.guardAnyClass(calleeObjId, calleeObj->getClass());

  writer.callClassHook(calleeObjId, argcId, hook, flags);
  writer.typeMonitorResult();

  cacheIRStubKind_ = BaselineCacheIRStubKind::Monitored;
  trackAttached("Call native func");

  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);

  // Some opcodes are not yet supported.
  switch (op_) {
    case JSOp::Call:
    case JSOp::CallIgnoresRv:
    case JSOp::CallIter:
    case JSOp::SpreadCall:
    case JSOp::New:
    case JSOp::SpreadNew:
    case JSOp::SuperCall:
    case JSOp::SpreadSuperCall:
    case JSOp::FunCall:
    case JSOp::FunApply:
      break;
    default:
      return AttachDecision::NoAction;
  }

  MOZ_ASSERT(mode_ != ICState::Mode::Generic);

  // Ensure callee is a function.
  if (!callee_.isObject()) {
    return AttachDecision::NoAction;
  }

  RootedObject calleeObj(cx_, &callee_.toObject());
  if (!calleeObj->is<JSFunction>()) {
    return tryAttachCallHook(calleeObj);
  }

  HandleFunction calleeFunc = calleeObj.as<JSFunction>();

  if (op_ == JSOp::FunCall) {
    return tryAttachFunCall(calleeFunc);
  }
  if (op_ == JSOp::FunApply) {
    return tryAttachFunApply(calleeFunc);
  }

  // Check for scripted optimizations.
  if (calleeFunc->hasJitEntry()) {
    return tryAttachCallScripted(calleeFunc);
  }

  // Check for native-function optimizations.
  MOZ_ASSERT(calleeFunc->isNativeWithoutJitEntry());

  return tryAttachCallNative(calleeFunc);
}

AttachDecision CallIRGenerator::tryAttachDeferredStub(HandleValue result) {
  AutoAssertNoPendingException aanpe(cx_);

  // Ensure that the opcode makes sense.
  MOZ_ASSERT(op_ == JSOp::Call || op_ == JSOp::CallIgnoresRv);

  // Ensure that the mode makes sense.
  MOZ_ASSERT(mode_ == ICState::Mode::Specialized);

  MOZ_ASSERT_UNREACHABLE("No deferred functions currently exist");
  return AttachDecision::NoAction;
}

void CallIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("callee", callee_);
    sp.valueProperty("thisval", thisval_);
    sp.valueProperty("argc", Int32Value(argc_));

    // Try to log the first two arguments.
    if (args_.length() >= 1) {
      sp.valueProperty("arg0", args_[0]);
    }
    if (args_.length() >= 2) {
      sp.valueProperty("arg1", args_[1]);
    }
  }
#endif
}

// Class which holds a shape pointer for use when caches might reference data in
// other zones.
static const JSClass shapeContainerClass = {"ShapeContainer",
                                            JSCLASS_HAS_RESERVED_SLOTS(1)};

static const size_t SHAPE_CONTAINER_SLOT = 0;

JSObject* jit::NewWrapperWithObjectShape(JSContext* cx,
                                         HandleNativeObject obj) {
  MOZ_ASSERT(cx->compartment() != obj->compartment());

  RootedObject wrapper(cx);
  {
    AutoRealm ar(cx, obj);
    wrapper = NewBuiltinClassInstance(cx, &shapeContainerClass);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->as<NativeObject>().setSlot(
        SHAPE_CONTAINER_SLOT, PrivateGCThingValue(obj->lastProperty()));
  }
  if (!JS_WrapObject(cx, &wrapper)) {
    return nullptr;
  }
  MOZ_ASSERT(IsWrapper(wrapper));
  return wrapper;
}

void jit::LoadShapeWrapperContents(MacroAssembler& masm, Register obj,
                                   Register dst, Label* failure) {
  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), dst);
  Address privateAddr(dst,
                      js::detail::ProxyReservedSlots::offsetOfPrivateSlot());
  masm.fallibleUnboxObject(privateAddr, dst, failure);
  masm.unboxNonDouble(
      Address(dst, NativeObject::getFixedSlotOffset(SHAPE_CONTAINER_SLOT)), dst,
      JSVAL_TYPE_PRIVATE_GCTHING);
}

CompareIRGenerator::CompareIRGenerator(JSContext* cx, HandleScript script,
                                       jsbytecode* pc, ICState::Mode mode,
                                       JSOp op, HandleValue lhsVal,
                                       HandleValue rhsVal)
    : IRGenerator(cx, script, pc, CacheKind::Compare, mode),
      op_(op),
      lhsVal_(lhsVal),
      rhsVal_(rhsVal) {}

AttachDecision CompareIRGenerator::tryAttachString(ValOperandId lhsId,
                                                   ValOperandId rhsId) {
  if (!lhsVal_.isString() || !rhsVal_.isString()) {
    return AttachDecision::NoAction;
  }

  StringOperandId lhsStrId = writer.guardToString(lhsId);
  StringOperandId rhsStrId = writer.guardToString(rhsId);
  writer.compareStringResult(op_, lhsStrId, rhsStrId);
  writer.returnFromIC();

  trackAttached("String");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachObject(ValOperandId lhsId,
                                                   ValOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op_));

  if (!lhsVal_.isObject() || !rhsVal_.isObject()) {
    return AttachDecision::NoAction;
  }

  ObjOperandId lhsObjId = writer.guardToObject(lhsId);
  ObjOperandId rhsObjId = writer.guardToObject(rhsId);
  writer.compareObjectResult(op_, lhsObjId, rhsObjId);
  writer.returnFromIC();

  trackAttached("Object");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachSymbol(ValOperandId lhsId,
                                                   ValOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op_));

  if (!lhsVal_.isSymbol() || !rhsVal_.isSymbol()) {
    return AttachDecision::NoAction;
  }

  SymbolOperandId lhsSymId = writer.guardToSymbol(lhsId);
  SymbolOperandId rhsSymId = writer.guardToSymbol(rhsId);
  writer.compareSymbolResult(op_, lhsSymId, rhsSymId);
  writer.returnFromIC();

  trackAttached("Symbol");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachStrictDifferentTypes(
    ValOperandId lhsId, ValOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op_));

  if (op_ != JSOp::StrictEq && op_ != JSOp::StrictNe) {
    return AttachDecision::NoAction;
  }

  // Probably can't hit some of these.
  if (SameType(lhsVal_, rhsVal_) ||
      (lhsVal_.isNumber() && rhsVal_.isNumber())) {
    return AttachDecision::NoAction;
  }

  // Compare tags
  ValueTagOperandId lhsTypeId = writer.loadValueTag(lhsId);
  ValueTagOperandId rhsTypeId = writer.loadValueTag(rhsId);
  writer.guardTagNotEqual(lhsTypeId, rhsTypeId);

  // Now that we've passed the guard, we know differing types, so return the
  // bool result.
  writer.loadBooleanResult(op_ == JSOp::StrictNe ? true : false);
  writer.returnFromIC();

  trackAttached("StrictDifferentTypes");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachInt32(ValOperandId lhsId,
                                                  ValOperandId rhsId) {
  if ((!lhsVal_.isInt32() && !lhsVal_.isBoolean()) ||
      (!rhsVal_.isInt32() && !rhsVal_.isBoolean())) {
    return AttachDecision::NoAction;
  }

  Int32OperandId lhsIntId = lhsVal_.isBoolean()
                                ? writer.guardBooleanToInt32(lhsId)
                                : writer.guardToInt32(lhsId);
  Int32OperandId rhsIntId = rhsVal_.isBoolean()
                                ? writer.guardBooleanToInt32(rhsId)
                                : writer.guardToInt32(rhsId);

  // Strictly different types should have been handed by
  // tryAttachStrictDifferentTypes
  MOZ_ASSERT_IF(op_ == JSOp::StrictEq || op_ == JSOp::StrictNe,
                lhsVal_.isInt32() == rhsVal_.isInt32());

  writer.compareInt32Result(op_, lhsIntId, rhsIntId);
  writer.returnFromIC();

  trackAttached(lhsVal_.isBoolean() ? "Boolean" : "Int32");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachNumber(ValOperandId lhsId,
                                                   ValOperandId rhsId) {
  if (!lhsVal_.isNumber() || !rhsVal_.isNumber()) {
    return AttachDecision::NoAction;
  }

  NumberOperandId lhs = writer.guardIsNumber(lhsId);
  NumberOperandId rhs = writer.guardIsNumber(rhsId);
  writer.compareDoubleResult(op_, lhs, rhs);
  writer.returnFromIC();

  trackAttached("Number");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachBigInt(ValOperandId lhsId,
                                                   ValOperandId rhsId) {
  if (!lhsVal_.isBigInt() || !rhsVal_.isBigInt()) {
    return AttachDecision::NoAction;
  }

  BigIntOperandId lhs = writer.guardToBigInt(lhsId);
  BigIntOperandId rhs = writer.guardToBigInt(rhsId);

  writer.compareBigIntResult(op_, lhs, rhs);
  writer.returnFromIC();

  trackAttached("BigInt");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachObjectUndefined(
    ValOperandId lhsId, ValOperandId rhsId) {
  if (!(lhsVal_.isNullOrUndefined() && rhsVal_.isObject()) &&
      !(rhsVal_.isNullOrUndefined() && lhsVal_.isObject()))
    return AttachDecision::NoAction;

  if (op_ != JSOp::Eq && op_ != JSOp::Ne) {
    return AttachDecision::NoAction;
  }

  ValOperandId obj = rhsVal_.isObject() ? rhsId : lhsId;
  ValOperandId undefOrNull = rhsVal_.isObject() ? lhsId : rhsId;

  writer.guardIsNullOrUndefined(undefOrNull);
  ObjOperandId objOperand = writer.guardToObject(obj);
  writer.compareObjectUndefinedNullResult(op_, objOperand);
  writer.returnFromIC();

  trackAttached("ObjectUndefined");
  return AttachDecision::Attach;
}

// Handle NumberUndefined comparisons
AttachDecision CompareIRGenerator::tryAttachNumberUndefined(
    ValOperandId lhsId, ValOperandId rhsId) {
  if (!(lhsVal_.isUndefined() && rhsVal_.isNumber()) &&
      !(rhsVal_.isUndefined() && lhsVal_.isNumber())) {
    return AttachDecision::NoAction;
  }

  if (lhsVal_.isNumber()) {
    writer.guardIsNumber(lhsId);
  } else {
    writer.guardIsUndefined(lhsId);
  }

  if (rhsVal_.isNumber()) {
    writer.guardIsNumber(rhsId);
  } else {
    writer.guardIsUndefined(rhsId);
  }

  // Comparing a number with undefined will always be true for Ne/StrictNe,
  // and always be false for other compare ops.
  writer.loadBooleanResult(op_ == JSOp::Ne || op_ == JSOp::StrictNe);
  writer.returnFromIC();

  trackAttached("NumberUndefined");
  return AttachDecision::Attach;
}

// Handle Primitive x {undefined,null} equality comparisons
AttachDecision CompareIRGenerator::tryAttachPrimitiveUndefined(
    ValOperandId lhsId, ValOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op_));

  // The set of primitive cases we want to handle here (excluding null,
  // undefined)
  auto isPrimitive = [](HandleValue x) {
    return x.isString() || x.isSymbol() || x.isBoolean() || x.isNumber() ||
           x.isBigInt();
  };

  if (!(lhsVal_.isNullOrUndefined() && isPrimitive(rhsVal_)) &&
      !(rhsVal_.isNullOrUndefined() && isPrimitive(lhsVal_))) {
    return AttachDecision::NoAction;
  }

  auto guardPrimitive = [&](HandleValue v, ValOperandId id) {
    if (v.isNumber()) {
      writer.guardIsNumber(id);
      return;
    }
    switch (v.extractNonDoubleType()) {
      case JSVAL_TYPE_BOOLEAN:
        writer.guardToBoolean(id);
        return;
      case JSVAL_TYPE_SYMBOL:
        writer.guardToSymbol(id);
        return;
      case JSVAL_TYPE_BIGINT:
        writer.guardToBigInt(id);
        return;
      case JSVAL_TYPE_STRING:
        writer.guardToString(id);
        return;
      default:
        MOZ_CRASH("unexpected type");
        return;
    }
  };

  isPrimitive(lhsVal_) ? guardPrimitive(lhsVal_, lhsId)
                       : writer.guardIsNullOrUndefined(lhsId);
  isPrimitive(rhsVal_) ? guardPrimitive(rhsVal_, rhsId)
                       : writer.guardIsNullOrUndefined(rhsId);

  // Comparing a primitive with undefined/null will always be true for
  // Ne/StrictNe, and always be false for other compare ops.
  writer.loadBooleanResult(op_ == JSOp::Ne || op_ == JSOp::StrictNe);
  writer.returnFromIC();

  trackAttached("PrimitiveUndefined");
  return AttachDecision::Attach;
}

// Handle {null/undefined} x {null,undefined} equality comparisons
AttachDecision CompareIRGenerator::tryAttachNullUndefined(ValOperandId lhsId,
                                                          ValOperandId rhsId) {
  if (!lhsVal_.isNullOrUndefined() || !rhsVal_.isNullOrUndefined()) {
    return AttachDecision::NoAction;
  }

  if (op_ == JSOp::Eq || op_ == JSOp::Ne) {
    writer.guardIsNullOrUndefined(lhsId);
    writer.guardIsNullOrUndefined(rhsId);
    // Sloppy equality means we actually only care about the op:
    writer.loadBooleanResult(op_ == JSOp::Eq);
    trackAttached("SloppyNullUndefined");
  } else {
    // Strict equality only hits this branch, and only in the
    // undef {!,=}==  undef and null {!,=}== null cases.
    // The other cases should have hit compareStrictlyDifferentTypes.
    MOZ_ASSERT(lhsVal_.isNull() == rhsVal_.isNull());
    lhsVal_.isNull() ? writer.guardIsNull(lhsId)
                     : writer.guardIsUndefined(lhsId);
    rhsVal_.isNull() ? writer.guardIsNull(rhsId)
                     : writer.guardIsUndefined(rhsId);
    writer.loadBooleanResult(op_ == JSOp::StrictEq);
    trackAttached("StrictNullUndefinedEquality");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachStringNumber(ValOperandId lhsId,
                                                         ValOperandId rhsId) {
  // Ensure String x Number
  if (!(lhsVal_.isString() && rhsVal_.isNumber()) &&
      !(rhsVal_.isString() && lhsVal_.isNumber())) {
    return AttachDecision::NoAction;
  }

  // Case should have been handled by tryAttachStrictlDifferentTypes
  MOZ_ASSERT(op_ != JSOp::StrictEq && op_ != JSOp::StrictNe);

  auto createGuards = [&](HandleValue v, ValOperandId vId) {
    if (v.isString()) {
      StringOperandId strId = writer.guardToString(vId);
      return writer.guardStringToNumber(strId);
    }
    MOZ_ASSERT(v.isNumber());
    NumberOperandId numId = writer.guardIsNumber(vId);
    return numId;
  };

  NumberOperandId lhsGuardedId = createGuards(lhsVal_, lhsId);
  NumberOperandId rhsGuardedId = createGuards(rhsVal_, rhsId);
  writer.compareDoubleResult(op_, lhsGuardedId, rhsGuardedId);
  writer.returnFromIC();

  trackAttached("StringNumber");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachPrimitiveSymbol(
    ValOperandId lhsId, ValOperandId rhsId) {
  MOZ_ASSERT(IsEqualityOp(op_));

  // The set of primitive cases we want to handle here (excluding null,
  // undefined, and symbol)
  auto isPrimitive = [](HandleValue x) {
    return x.isString() || x.isBoolean() || x.isNumber() || x.isBigInt();
  };

  // Ensure Symbol x {String, Bool, Number, BigInt}.
  if (!(lhsVal_.isSymbol() && isPrimitive(rhsVal_)) &&
      !(rhsVal_.isSymbol() && isPrimitive(lhsVal_))) {
    return AttachDecision::NoAction;
  }

  auto guardPrimitive = [&](HandleValue v, ValOperandId id) {
    MOZ_ASSERT(isPrimitive(v));
    if (v.isNumber()) {
      writer.guardIsNumber(id);
      return;
    }
    switch (v.extractNonDoubleType()) {
      case JSVAL_TYPE_STRING:
        writer.guardToString(id);
        return;
      case JSVAL_TYPE_BOOLEAN:
        writer.guardToBoolean(id);
        return;
      case JSVAL_TYPE_BIGINT:
        writer.guardToBigInt(id);
        return;
      default:
        MOZ_CRASH("unexpected type");
        return;
    }
  };

  if (lhsVal_.isSymbol()) {
    writer.guardToSymbol(lhsId);
    guardPrimitive(rhsVal_, rhsId);
  } else {
    guardPrimitive(lhsVal_, lhsId);
    writer.guardToSymbol(rhsId);
  }

  // Comparing a primitive with symbol will always be true for Ne/StrictNe, and
  // always be false for other compare ops.
  writer.loadBooleanResult(op_ == JSOp::Ne || op_ == JSOp::StrictNe);
  writer.returnFromIC();

  trackAttached("PrimitiveSymbol");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachBoolStringOrNumber(
    ValOperandId lhsId, ValOperandId rhsId) {
  // Ensure Boolean x {String, Number}.
  if (!(lhsVal_.isBoolean() && (rhsVal_.isString() || rhsVal_.isNumber())) &&
      !(rhsVal_.isBoolean() && (lhsVal_.isString() || lhsVal_.isNumber()))) {
    return AttachDecision::NoAction;
  }

  // Case should have been handled by tryAttachStrictlDifferentTypes
  MOZ_ASSERT(op_ != JSOp::StrictEq && op_ != JSOp::StrictNe);

  // Case should have been handled by tryAttachInt32
  MOZ_ASSERT(!lhsVal_.isInt32() && !rhsVal_.isInt32());

  auto createGuards = [&](HandleValue v, ValOperandId vId) {
    if (v.isBoolean()) {
      BooleanOperandId boolId = writer.guardToBoolean(vId);
      return writer.booleanToNumber(boolId);
    }
    if (v.isString()) {
      StringOperandId strId = writer.guardToString(vId);
      return writer.guardStringToNumber(strId);
    }
    MOZ_ASSERT(v.isNumber());
    return writer.guardIsNumber(vId);
  };

  NumberOperandId lhsGuardedId = createGuards(lhsVal_, lhsId);
  NumberOperandId rhsGuardedId = createGuards(rhsVal_, rhsId);
  writer.compareDoubleResult(op_, lhsGuardedId, rhsGuardedId);
  writer.returnFromIC();

  trackAttached("BoolStringOrNumber");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachBigIntInt32(ValOperandId lhsId,
                                                        ValOperandId rhsId) {
  // Ensure BigInt x {Int32, Boolean}.
  if (!(lhsVal_.isBigInt() && (rhsVal_.isInt32() || rhsVal_.isBoolean())) &&
      !(rhsVal_.isBigInt() && (lhsVal_.isInt32() || lhsVal_.isBoolean()))) {
    return AttachDecision::NoAction;
  }

  // Case should have been handled by tryAttachStrictlDifferentTypes
  MOZ_ASSERT(op_ != JSOp::StrictEq && op_ != JSOp::StrictNe);

  auto createGuards = [&](HandleValue v, ValOperandId vId) {
    if (v.isBoolean()) {
      return writer.guardBooleanToInt32(vId);
    }
    MOZ_ASSERT(v.isInt32());
    return writer.guardToInt32(vId);
  };

  if (lhsVal_.isBigInt()) {
    BigIntOperandId bigIntId = writer.guardToBigInt(lhsId);
    Int32OperandId intId = createGuards(rhsVal_, rhsId);

    writer.compareBigIntInt32Result(op_, bigIntId, intId);
  } else {
    Int32OperandId intId = createGuards(lhsVal_, lhsId);
    BigIntOperandId bigIntId = writer.guardToBigInt(rhsId);

    writer.compareInt32BigIntResult(op_, intId, bigIntId);
  }
  writer.returnFromIC();

  trackAttached("BigIntInt32");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachBigIntNumber(ValOperandId lhsId,
                                                         ValOperandId rhsId) {
  // Ensure BigInt x Number.
  if (!(lhsVal_.isBigInt() && rhsVal_.isNumber()) &&
      !(rhsVal_.isBigInt() && lhsVal_.isNumber())) {
    return AttachDecision::NoAction;
  }

  // Case should have been handled by tryAttachStrictlDifferentTypes
  MOZ_ASSERT(op_ != JSOp::StrictEq && op_ != JSOp::StrictNe);

  if (lhsVal_.isBigInt()) {
    BigIntOperandId bigIntId = writer.guardToBigInt(lhsId);
    NumberOperandId numId = writer.guardIsNumber(rhsId);

    writer.compareBigIntNumberResult(op_, bigIntId, numId);
  } else {
    NumberOperandId numId = writer.guardIsNumber(lhsId);
    BigIntOperandId bigIntId = writer.guardToBigInt(rhsId);

    writer.compareNumberBigIntResult(op_, numId, bigIntId);
  }
  writer.returnFromIC();

  trackAttached("BigIntNumber");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachBigIntString(ValOperandId lhsId,
                                                         ValOperandId rhsId) {
  // Ensure BigInt x String.
  if (!(lhsVal_.isBigInt() && rhsVal_.isString()) &&
      !(rhsVal_.isBigInt() && lhsVal_.isString())) {
    return AttachDecision::NoAction;
  }

  // Case should have been handled by tryAttachStrictlDifferentTypes
  MOZ_ASSERT(op_ != JSOp::StrictEq && op_ != JSOp::StrictNe);

  if (lhsVal_.isBigInt()) {
    BigIntOperandId bigIntId = writer.guardToBigInt(lhsId);
    StringOperandId strId = writer.guardToString(rhsId);

    writer.compareBigIntStringResult(op_, bigIntId, strId);
  } else {
    StringOperandId strId = writer.guardToString(lhsId);
    BigIntOperandId bigIntId = writer.guardToBigInt(rhsId);

    writer.compareStringBigIntResult(op_, strId, bigIntId);
  }
  writer.returnFromIC();

  trackAttached("BigIntString");
  return AttachDecision::Attach;
}

AttachDecision CompareIRGenerator::tryAttachStub() {
  MOZ_ASSERT(cacheKind_ == CacheKind::Compare);
  MOZ_ASSERT(IsEqualityOp(op_) || IsRelationalOp(op_));

  AutoAssertNoPendingException aanpe(cx_);

  constexpr uint8_t lhsIndex = 0;
  constexpr uint8_t rhsIndex = 1;

  static_assert(lhsIndex == 0 && rhsIndex == 1,
                "Indexes relied upon by baseline inspector");

  ValOperandId lhsId(writer.setInputOperandId(lhsIndex));
  ValOperandId rhsId(writer.setInputOperandId(rhsIndex));

  // For sloppy equality ops, there are cases this IC does not handle:
  // - {Object} x {String, Symbol, Bool, Number, BigInt}.
  //
  // (The above lists omits the equivalent case {B} x {A} when {A} x {B} is
  // already present.)

  if (IsEqualityOp(op_)) {
    TRY_ATTACH(tryAttachObject(lhsId, rhsId));
    TRY_ATTACH(tryAttachSymbol(lhsId, rhsId));

    // Handle the special case of Object compared to null/undefined.
    // This is special due to the IsHTMLDDA internal slot semantics.
    TRY_ATTACH(tryAttachObjectUndefined(lhsId, rhsId));

    // This covers -strict- equality/inequality using a type tag check, so
    // catches all different type pairs outside of Numbers, which cannot be
    // checked on tags alone.
    TRY_ATTACH(tryAttachStrictDifferentTypes(lhsId, rhsId));

    // These checks should come after tryAttachStrictDifferentTypes since it
    // handles strict inequality with a more generic IC.
    TRY_ATTACH(tryAttachPrimitiveUndefined(lhsId, rhsId));

    TRY_ATTACH(tryAttachNullUndefined(lhsId, rhsId));

    TRY_ATTACH(tryAttachPrimitiveSymbol(lhsId, rhsId));
  }

  // This should preceed the Int32/Number cases to allow
  // them to not concern themselves with handling undefined
  // or null.
  TRY_ATTACH(tryAttachNumberUndefined(lhsId, rhsId));

  // We want these to be last, to allow us to bypass the
  // strictly-different-types cases in the below attachment code
  TRY_ATTACH(tryAttachInt32(lhsId, rhsId));
  TRY_ATTACH(tryAttachNumber(lhsId, rhsId));
  TRY_ATTACH(tryAttachBigInt(lhsId, rhsId));
  TRY_ATTACH(tryAttachString(lhsId, rhsId));

  TRY_ATTACH(tryAttachStringNumber(lhsId, rhsId));
  TRY_ATTACH(tryAttachBoolStringOrNumber(lhsId, rhsId));

  TRY_ATTACH(tryAttachBigIntInt32(lhsId, rhsId));
  TRY_ATTACH(tryAttachBigIntNumber(lhsId, rhsId));
  TRY_ATTACH(tryAttachBigIntString(lhsId, rhsId));

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

void CompareIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("lhs", lhsVal_);
    sp.valueProperty("rhs", rhsVal_);
    sp.opcodeProperty("op", op_);
  }
#endif
}

ToBoolIRGenerator::ToBoolIRGenerator(JSContext* cx, HandleScript script,
                                     jsbytecode* pc, ICState::Mode mode,
                                     HandleValue val)
    : IRGenerator(cx, script, pc, CacheKind::ToBool, mode), val_(val) {}

void ToBoolIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

AttachDecision ToBoolIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);

  TRY_ATTACH(tryAttachInt32());
  TRY_ATTACH(tryAttachNumber());
  TRY_ATTACH(tryAttachString());
  TRY_ATTACH(tryAttachNullOrUndefined());
  TRY_ATTACH(tryAttachObject());
  TRY_ATTACH(tryAttachSymbol());
  TRY_ATTACH(tryAttachBigInt());

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision ToBoolIRGenerator::tryAttachInt32() {
  if (!val_.isInt32()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  writer.guardNonDoubleType(valId, ValueType::Int32);
  writer.loadInt32TruthyResult(valId);
  writer.returnFromIC();
  trackAttached("ToBoolInt32");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachNumber() {
  if (!val_.isNumber()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  NumberOperandId numId = writer.guardIsNumber(valId);
  writer.loadDoubleTruthyResult(numId);
  writer.returnFromIC();
  trackAttached("ToBoolNumber");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachSymbol() {
  if (!val_.isSymbol()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  writer.guardNonDoubleType(valId, ValueType::Symbol);
  writer.loadBooleanResult(true);
  writer.returnFromIC();
  trackAttached("ToBoolSymbol");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachString() {
  if (!val_.isString()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  StringOperandId strId = writer.guardToString(valId);
  writer.loadStringTruthyResult(strId);
  writer.returnFromIC();
  trackAttached("ToBoolString");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachNullOrUndefined() {
  if (!val_.isNullOrUndefined()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  writer.guardIsNullOrUndefined(valId);
  writer.loadBooleanResult(false);
  writer.returnFromIC();
  trackAttached("ToBoolNullOrUndefined");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachObject() {
  if (!val_.isObject()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  ObjOperandId objId = writer.guardToObject(valId);
  writer.loadObjectTruthyResult(objId);
  writer.returnFromIC();
  trackAttached("ToBoolObject");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachBigInt() {
  if (!val_.isBigInt()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  BigIntOperandId bigIntId = writer.guardToBigInt(valId);
  writer.loadBigIntTruthyResult(bigIntId);
  writer.returnFromIC();
  trackAttached("ToBoolBigInt");
  return AttachDecision::Attach;
}

GetIntrinsicIRGenerator::GetIntrinsicIRGenerator(JSContext* cx,
                                                 HandleScript script,
                                                 jsbytecode* pc,
                                                 ICState::Mode mode,
                                                 HandleValue val)
    : IRGenerator(cx, script, pc, CacheKind::GetIntrinsic, mode), val_(val) {}

void GetIntrinsicIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

AttachDecision GetIntrinsicIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);
  writer.loadValueResult(val_);
  writer.returnFromIC();
  trackAttached("GetIntrinsic");
  return AttachDecision::Attach;
}

UnaryArithIRGenerator::UnaryArithIRGenerator(JSContext* cx, HandleScript script,
                                             jsbytecode* pc, ICState::Mode mode,
                                             JSOp op, HandleValue val,
                                             HandleValue res)
    : IRGenerator(cx, script, pc, CacheKind::UnaryArith, mode),
      op_(op),
      val_(val),
      res_(res) {}

void UnaryArithIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
    sp.valueProperty("res", res_);
  }
#endif
}

AttachDecision UnaryArithIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);
  TRY_ATTACH(tryAttachInt32());
  TRY_ATTACH(tryAttachNumber());
  TRY_ATTACH(tryAttachBigInt());
  TRY_ATTACH(tryAttachStringInt32());
  TRY_ATTACH(tryAttachStringNumber());

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision UnaryArithIRGenerator::tryAttachInt32() {
  if (!val_.isInt32() || !res_.isInt32()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));

  Int32OperandId intId = writer.guardToInt32(valId);
  switch (op_) {
    case JSOp::BitNot:
      writer.int32NotResult(intId);
      trackAttached("UnaryArith.Int32Not");
      break;
    case JSOp::Pos:
      writer.loadInt32Result(intId);
      trackAttached("UnaryArith.Int32Pos");
      break;
    case JSOp::Neg:
      writer.int32NegationResult(intId);
      trackAttached("UnaryArith.Int32Neg");
      break;
    case JSOp::Inc:
      writer.int32IncResult(intId);
      trackAttached("UnaryArith.Int32Inc");
      break;
    case JSOp::Dec:
      writer.int32DecResult(intId);
      trackAttached("UnaryArith.Int32Dec");
      break;
    case JSOp::ToNumeric:
      writer.loadInt32Result(intId);
      trackAttached("UnaryArith.Int32ToNumeric");
      break;
    default:
      MOZ_CRASH("unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision UnaryArithIRGenerator::tryAttachNumber() {
  if (!val_.isNumber()) {
    return AttachDecision::NoAction;
  }
  MOZ_ASSERT(res_.isNumber());

  ValOperandId valId(writer.setInputOperandId(0));
  NumberOperandId numId = writer.guardIsNumber(valId);
  Int32OperandId truncatedId;
  switch (op_) {
    case JSOp::BitNot:
      truncatedId = writer.truncateDoubleToUInt32(numId);
      writer.int32NotResult(truncatedId);
      trackAttached("UnaryArith.DoubleNot");
      break;
    case JSOp::Pos:
      writer.loadDoubleResult(numId);
      trackAttached("UnaryArith.DoublePos");
      break;
    case JSOp::Neg:
      writer.doubleNegationResult(numId);
      trackAttached("UnaryArith.DoubleNeg");
      break;
    case JSOp::Inc:
      writer.doubleIncResult(numId);
      trackAttached("UnaryArith.DoubleInc");
      break;
    case JSOp::Dec:
      writer.doubleDecResult(numId);
      trackAttached("UnaryArith.DoubleDec");
      break;
    case JSOp::ToNumeric:
      writer.loadDoubleResult(numId);
      trackAttached("UnaryArith.DoubleToNumeric");
      break;
    default:
      MOZ_CRASH("Unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision UnaryArithIRGenerator::tryAttachBigInt() {
  if (!val_.isBigInt()) {
    return AttachDecision::NoAction;
  }
  MOZ_ASSERT(res_.isBigInt());

  MOZ_ASSERT(op_ != JSOp::Pos,
             "Applying the unary + operator on BigInt values throws an error");

  ValOperandId valId(writer.setInputOperandId(0));
  BigIntOperandId bigIntId = writer.guardToBigInt(valId);
  switch (op_) {
    case JSOp::BitNot:
      writer.bigIntNotResult(bigIntId);
      trackAttached("UnaryArith.BigIntNot");
      break;
    case JSOp::Neg:
      writer.bigIntNegationResult(bigIntId);
      trackAttached("UnaryArith.BigIntNeg");
      break;
    case JSOp::Inc:
      writer.bigIntIncResult(bigIntId);
      trackAttached("UnaryArith.BigIntInc");
      break;
    case JSOp::Dec:
      writer.bigIntDecResult(bigIntId);
      trackAttached("UnaryArith.BigIntDec");
      break;
    case JSOp::ToNumeric:
      writer.loadBigIntResult(bigIntId);
      trackAttached("UnaryArith.BigIntToNumeric");
      break;
    default:
      MOZ_CRASH("Unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision UnaryArithIRGenerator::tryAttachStringInt32() {
  if (!val_.isString()) {
    return AttachDecision::NoAction;
  }
  MOZ_ASSERT(res_.isNumber());

  if (!res_.isInt32()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  StringOperandId stringId = writer.guardToString(valId);
  Int32OperandId intId = writer.guardStringToInt32(stringId);

  switch (op_) {
    case JSOp::BitNot:
      writer.int32NotResult(intId);
      trackAttached("UnaryArith.StringInt32Not");
      break;
    case JSOp::Pos:
      writer.loadInt32Result(intId);
      trackAttached("UnaryArith.StringInt32Pos");
      break;
    case JSOp::Neg:
      writer.int32NegationResult(intId);
      trackAttached("UnaryArith.StringInt32Neg");
      break;
    case JSOp::Inc:
      writer.int32IncResult(intId);
      trackAttached("UnaryArith.StringInt32Inc");
      break;
    case JSOp::Dec:
      writer.int32DecResult(intId);
      trackAttached("UnaryArith.StringInt32Dec");
      break;
    case JSOp::ToNumeric:
      writer.loadInt32Result(intId);
      trackAttached("UnaryArith.StringInt32ToNumeric");
      break;
    default:
      MOZ_CRASH("Unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision UnaryArithIRGenerator::tryAttachStringNumber() {
  if (!val_.isString()) {
    return AttachDecision::NoAction;
  }
  MOZ_ASSERT(res_.isNumber());

  ValOperandId valId(writer.setInputOperandId(0));
  StringOperandId stringId = writer.guardToString(valId);
  NumberOperandId numId = writer.guardStringToNumber(stringId);

  Int32OperandId truncatedId;
  switch (op_) {
    case JSOp::BitNot:
      truncatedId = writer.truncateDoubleToUInt32(numId);
      writer.int32NotResult(truncatedId);
      trackAttached("UnaryArith.StringNumberNot");
      break;
    case JSOp::Pos:
      writer.loadDoubleResult(numId);
      trackAttached("UnaryArith.StringNumberPos");
      break;
    case JSOp::Neg:
      writer.doubleNegationResult(numId);
      trackAttached("UnaryArith.StringNumberNeg");
      break;
    case JSOp::Inc:
      writer.doubleIncResult(numId);
      trackAttached("UnaryArith.StringNumberInc");
      break;
    case JSOp::Dec:
      writer.doubleDecResult(numId);
      trackAttached("UnaryArith.StringNumberDec");
      break;
    case JSOp::ToNumeric:
      writer.loadDoubleResult(numId);
      trackAttached("UnaryArith.StringNumberToNumeric");
      break;
    default:
      MOZ_CRASH("Unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

ToPropertyKeyIRGenerator::ToPropertyKeyIRGenerator(JSContext* cx,
                                                   HandleScript script,
                                                   jsbytecode* pc,
                                                   ICState::Mode mode,
                                                   HandleValue val)
    : IRGenerator(cx, script, pc, CacheKind::ToPropertyKey, mode), val_(val) {}

void ToPropertyKeyIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.valueProperty("val", val_);
  }
#endif
}

AttachDecision ToPropertyKeyIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);
  TRY_ATTACH(tryAttachInt32());
  TRY_ATTACH(tryAttachNumber());
  TRY_ATTACH(tryAttachString());
  TRY_ATTACH(tryAttachSymbol());

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision ToPropertyKeyIRGenerator::tryAttachInt32() {
  if (!val_.isInt32()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));

  Int32OperandId intId = writer.guardToInt32(valId);
  writer.loadInt32Result(intId);
  writer.returnFromIC();

  trackAttached("ToPropertyKey.Int32");
  return AttachDecision::Attach;
}

AttachDecision ToPropertyKeyIRGenerator::tryAttachNumber() {
  if (!val_.isNumber()) {
    return AttachDecision::NoAction;
  }

  // We allow negative zero here because ToPropertyKey(-0.0) is 0.
  int32_t unused;
  if (!mozilla::NumberEqualsInt32(val_.toNumber(), &unused)) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));

  Int32OperandId intId = writer.guardToInt32Index(valId);
  writer.loadInt32Result(intId);
  writer.returnFromIC();

  trackAttached("ToPropertyKey.Number");
  return AttachDecision::Attach;
}

AttachDecision ToPropertyKeyIRGenerator::tryAttachString() {
  if (!val_.isString()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));

  StringOperandId strId = writer.guardToString(valId);
  writer.loadStringResult(strId);
  writer.returnFromIC();

  trackAttached("ToPropertyKey.String");
  return AttachDecision::Attach;
}

AttachDecision ToPropertyKeyIRGenerator::tryAttachSymbol() {
  if (!val_.isSymbol()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));

  SymbolOperandId strId = writer.guardToSymbol(valId);
  writer.loadSymbolResult(strId);
  writer.returnFromIC();

  trackAttached("ToPropertyKey.Symbol");
  return AttachDecision::Attach;
}

BinaryArithIRGenerator::BinaryArithIRGenerator(
    JSContext* cx, HandleScript script, jsbytecode* pc, ICState::Mode mode,
    JSOp op, HandleValue lhs, HandleValue rhs, HandleValue res)
    : IRGenerator(cx, script, pc, CacheKind::BinaryArith, mode),
      op_(op),
      lhs_(lhs),
      rhs_(rhs),
      res_(res) {}

void BinaryArithIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.opcodeProperty("op", op_);
    sp.valueProperty("rhs", rhs_);
    sp.valueProperty("lhs", lhs_);
  }
#endif
}

AttachDecision BinaryArithIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);
  // Arithmetic operations with Int32 operands
  TRY_ATTACH(tryAttachInt32());

  // Bitwise operations with Int32/Double/Boolean operands.
  TRY_ATTACH(tryAttachBitwise());

  // Arithmetic operations with Double operands. This needs to come after
  // tryAttachInt32, as the guards overlap, and we'd prefer to attach the
  // more specialized Int32 IC if it is possible.
  TRY_ATTACH(tryAttachDouble());

  // String x String
  TRY_ATTACH(tryAttachStringConcat());

  // String x Object
  TRY_ATTACH(tryAttachStringObjectConcat());

  TRY_ATTACH(tryAttachStringNumberConcat());

  // String + Boolean
  TRY_ATTACH(tryAttachStringBooleanConcat());

  // Arithmetic operations or bitwise operations with BigInt operands
  TRY_ATTACH(tryAttachBigInt());

  // Arithmetic operations (without addition) with String x Int32.
  TRY_ATTACH(tryAttachStringInt32Arith());

  trackAttached(IRGenerator::NotAttached);
  return AttachDecision::NoAction;
}

AttachDecision BinaryArithIRGenerator::tryAttachBitwise() {
  // Only bit-wise and shifts.
  if (op_ != JSOp::BitOr && op_ != JSOp::BitXor && op_ != JSOp::BitAnd &&
      op_ != JSOp::Lsh && op_ != JSOp::Rsh && op_ != JSOp::Ursh) {
    return AttachDecision::NoAction;
  }

  // Check guard conditions
  if (!(lhs_.isNumber() || lhs_.isBoolean()) ||
      !(rhs_.isNumber() || rhs_.isBoolean())) {
    return AttachDecision::NoAction;
  }

  // All ops, with the exception of Ursh, produce Int32 values.
  MOZ_ASSERT_IF(op_ != JSOp::Ursh, res_.isInt32());

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  // Convert type into int32 for the bitwise/shift operands.
  auto guardToInt32 = [&](ValOperandId id, HandleValue val) {
    if (val.isInt32()) {
      return writer.guardToInt32(id);
    }
    if (val.isBoolean()) {
      return writer.guardBooleanToInt32(id);
    }
    MOZ_ASSERT(val.isDouble());
    NumberOperandId numId = writer.guardIsNumber(id);
    return writer.truncateDoubleToUInt32(numId);
  };

  Int32OperandId lhsIntId = guardToInt32(lhsId, lhs_);
  Int32OperandId rhsIntId = guardToInt32(rhsId, rhs_);

  switch (op_) {
    case JSOp::BitOr:
      writer.int32BitOrResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.BitOr");
      break;
    case JSOp::BitXor:
      writer.int32BitXorResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.BitXor");
      break;
    case JSOp::BitAnd:
      writer.int32BitAndResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.BitAnd");
      break;
    case JSOp::Lsh:
      writer.int32LeftShiftResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.LeftShift");
      break;
    case JSOp::Rsh:
      writer.int32RightShiftResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.RightShift");
      break;
    case JSOp::Ursh:
      writer.int32URightShiftResult(lhsIntId, rhsIntId, res_.isDouble());
      trackAttached("BinaryArith.Bitwise.UnsignedRightShift");
      break;
    default:
      MOZ_CRASH("Unhandled op in tryAttachBitwise");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachDouble() {
  // Check valid opcodes
  if (op_ != JSOp::Add && op_ != JSOp::Sub && op_ != JSOp::Mul &&
      op_ != JSOp::Div && op_ != JSOp::Mod && op_ != JSOp::Pow) {
    return AttachDecision::NoAction;
  }

  // Check guard conditions
  if (!lhs_.isNumber() || !rhs_.isNumber()) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  NumberOperandId lhs = writer.guardIsNumber(lhsId);
  NumberOperandId rhs = writer.guardIsNumber(rhsId);

  switch (op_) {
    case JSOp::Add:
      writer.doubleAddResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Add");
      break;
    case JSOp::Sub:
      writer.doubleSubResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Sub");
      break;
    case JSOp::Mul:
      writer.doubleMulResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Mul");
      break;
    case JSOp::Div:
      writer.doubleDivResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Div");
      break;
    case JSOp::Mod:
      writer.doubleModResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Mod");
      break;
    case JSOp::Pow:
      writer.doublePowResult(lhs, rhs);
      trackAttached("BinaryArith.Double.Pow");
      break;
    default:
      MOZ_CRASH("Unhandled Op");
  }
  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachInt32() {
  // Check guard conditions
  if (!(lhs_.isInt32() || lhs_.isBoolean()) ||
      !(rhs_.isInt32() || rhs_.isBoolean())) {
    return AttachDecision::NoAction;
  }

  // These ICs will failure() if result can't be encoded in an Int32:
  // If sample result is not Int32, we should avoid IC.
  if (!res_.isInt32()) {
    return AttachDecision::NoAction;
  }

  if (op_ != JSOp::Add && op_ != JSOp::Sub && op_ != JSOp::Mul &&
      op_ != JSOp::Div && op_ != JSOp::Mod && op_ != JSOp::Pow) {
    return AttachDecision::NoAction;
  }

  if (op_ == JSOp::Pow && !CanAttachInt32Pow(lhs_, rhs_)) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  auto guardToInt32 = [&](ValOperandId id, HandleValue v) {
    if (v.isInt32()) {
      return writer.guardToInt32(id);
    }
    MOZ_ASSERT(v.isBoolean());
    return writer.guardBooleanToInt32(id);
  };

  Int32OperandId lhsIntId = guardToInt32(lhsId, lhs_);
  Int32OperandId rhsIntId = guardToInt32(rhsId, rhs_);

  switch (op_) {
    case JSOp::Add:
      writer.int32AddResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Add");
      break;
    case JSOp::Sub:
      writer.int32SubResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Sub");
      break;
    case JSOp::Mul:
      writer.int32MulResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Mul");
      break;
    case JSOp::Div:
      writer.int32DivResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Div");
      break;
    case JSOp::Mod:
      writer.int32ModResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Mod");
      break;
    case JSOp::Pow:
      writer.int32PowResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Int32.Pow");
      break;
    default:
      MOZ_CRASH("Unhandled op in tryAttachInt32");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachStringNumberConcat() {
  // Only Addition
  if (op_ != JSOp::Add) {
    return AttachDecision::NoAction;
  }

  if (!(lhs_.isString() && rhs_.isNumber()) &&
      !(lhs_.isNumber() && rhs_.isString())) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  StringOperandId lhsStrId = EmitToStringGuard(writer, lhsId, lhs_);
  StringOperandId rhsStrId = EmitToStringGuard(writer, rhsId, rhs_);

  writer.callStringConcatResult(lhsStrId, rhsStrId);

  writer.returnFromIC();
  trackAttached("BinaryArith.StringNumberConcat");
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachStringBooleanConcat() {
  // Only Addition
  if (op_ != JSOp::Add) {
    return AttachDecision::NoAction;
  }

  if ((!lhs_.isString() || !rhs_.isBoolean()) &&
      (!lhs_.isBoolean() || !rhs_.isString())) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  auto guardToString = [&](ValOperandId id, HandleValue v) {
    if (v.isString()) {
      return writer.guardToString(id);
    }
    MOZ_ASSERT(v.isBoolean());
    BooleanOperandId boolId = writer.guardToBoolean(id);
    return writer.booleanToString(boolId);
  };

  StringOperandId lhsStrId = guardToString(lhsId, lhs_);
  StringOperandId rhsStrId = guardToString(rhsId, rhs_);

  writer.callStringConcatResult(lhsStrId, rhsStrId);

  writer.returnFromIC();
  trackAttached("BinaryArith.StringBooleanConcat");
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachStringConcat() {
  // Only Addition
  if (op_ != JSOp::Add) {
    return AttachDecision::NoAction;
  }

  // Check guards
  if (!lhs_.isString() || !rhs_.isString()) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  StringOperandId lhsStrId = writer.guardToString(lhsId);
  StringOperandId rhsStrId = writer.guardToString(rhsId);

  writer.callStringConcatResult(lhsStrId, rhsStrId);

  writer.returnFromIC();
  trackAttached("BinaryArith.StringConcat");
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachStringObjectConcat() {
  // Only Addition
  if (op_ != JSOp::Add) {
    return AttachDecision::NoAction;
  }

  // Check Guards
  if (!(lhs_.isObject() && rhs_.isString()) &&
      !(lhs_.isString() && rhs_.isObject()))
    return AttachDecision::NoAction;

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  // This guard is actually overly tight, as the runtime
  // helper can handle lhs or rhs being a string, so long
  // as the other is an object.
  if (lhs_.isString()) {
    writer.guardToString(lhsId);
    writer.guardToObject(rhsId);
  } else {
    writer.guardToObject(lhsId);
    writer.guardToString(rhsId);
  }

  writer.callStringObjectConcatResult(lhsId, rhsId);

  writer.returnFromIC();
  trackAttached("BinaryArith.StringObjectConcat");
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachBigInt() {
  // Check Guards
  if (!lhs_.isBigInt() || !rhs_.isBigInt()) {
    return AttachDecision::NoAction;
  }

  switch (op_) {
    case JSOp::Add:
    case JSOp::Sub:
    case JSOp::Mul:
    case JSOp::Div:
    case JSOp::Mod:
    case JSOp::Pow:
      // Arithmetic operations.
      break;

    case JSOp::BitOr:
    case JSOp::BitXor:
    case JSOp::BitAnd:
    case JSOp::Lsh:
    case JSOp::Rsh:
      // Bitwise operations.
      break;

    default:
      return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  BigIntOperandId lhsBigIntId = writer.guardToBigInt(lhsId);
  BigIntOperandId rhsBigIntId = writer.guardToBigInt(rhsId);

  switch (op_) {
    case JSOp::Add:
      writer.bigIntAddResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Add");
      break;
    case JSOp::Sub:
      writer.bigIntSubResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Sub");
      break;
    case JSOp::Mul:
      writer.bigIntMulResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Mul");
      break;
    case JSOp::Div:
      writer.bigIntDivResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Div");
      break;
    case JSOp::Mod:
      writer.bigIntModResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Mod");
      break;
    case JSOp::Pow:
      writer.bigIntPowResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.Pow");
      break;
    case JSOp::BitOr:
      writer.bigIntBitOrResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.BitOr");
      break;
    case JSOp::BitXor:
      writer.bigIntBitXorResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.BitXor");
      break;
    case JSOp::BitAnd:
      writer.bigIntBitAndResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.BitAnd");
      break;
    case JSOp::Lsh:
      writer.bigIntLeftShiftResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.LeftShift");
      break;
    case JSOp::Rsh:
      writer.bigIntRightShiftResult(lhsBigIntId, rhsBigIntId);
      trackAttached("BinaryArith.BigInt.RightShift");
      break;
    default:
      MOZ_CRASH("Unhandled op in tryAttachBigInt");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision BinaryArithIRGenerator::tryAttachStringInt32Arith() {
  // Check for either int32 x string or string x int32.
  if (!(lhs_.isInt32() && rhs_.isString()) &&
      !(lhs_.isString() && rhs_.isInt32())) {
    return AttachDecision::NoAction;
  }

  // The created ICs will fail if the result can't be encoded as as int32.
  // Thus skip this IC, if the sample result is not an int32.
  if (!res_.isInt32()) {
    return AttachDecision::NoAction;
  }

  // Must _not_ support Add, because it would be string concatenation instead.
  // For Pow we can't easily determine the CanAttachInt32Pow conditions so we
  // reject that as well.
  if (op_ != JSOp::Sub && op_ != JSOp::Mul && op_ != JSOp::Div &&
      op_ != JSOp::Mod) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  auto guardToInt32 = [&](ValOperandId id, HandleValue v) {
    if (v.isInt32()) {
      return writer.guardToInt32(id);
    }

    MOZ_ASSERT(v.isString());
    StringOperandId strId = writer.guardToString(id);
    return writer.guardStringToInt32(strId);
  };

  Int32OperandId lhsIntId = guardToInt32(lhsId, lhs_);
  Int32OperandId rhsIntId = guardToInt32(rhsId, rhs_);

  switch (op_) {
    case JSOp::Sub:
      writer.int32SubResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.StringInt32.Sub");
      break;
    case JSOp::Mul:
      writer.int32MulResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.StringInt32.Mul");
      break;
    case JSOp::Div:
      writer.int32DivResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.StringInt32.Div");
      break;
    case JSOp::Mod:
      writer.int32ModResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.StringInt32.Mod");
      break;
    default:
      MOZ_CRASH("Unhandled op in tryAttachStringInt32Arith");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

NewObjectIRGenerator::NewObjectIRGenerator(JSContext* cx, HandleScript script,
                                           jsbytecode* pc, ICState::Mode mode,
                                           JSOp op, HandleObject templateObj)
    : IRGenerator(cx, script, pc, CacheKind::NewObject, mode),
#ifdef JS_CACHEIR_SPEW
      op_(op),
#endif
      templateObject_(templateObj) {
  MOZ_ASSERT(templateObject_);
}

void NewObjectIRGenerator::trackAttached(const char* name) {
#ifdef JS_CACHEIR_SPEW
  if (const CacheIRSpewer::Guard& sp = CacheIRSpewer::Guard(*this, name)) {
    sp.opcodeProperty("op", op_);
  }
#endif
}

AttachDecision NewObjectIRGenerator::tryAttachStub() {
  AutoAssertNoPendingException aanpe(cx_);
  if (templateObject_->as<NativeObject>().hasDynamicSlots()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  // Don't attach stub if group is pretenured, as the stub
  // won't succeed.
  AutoSweepObjectGroup sweep(templateObject_->group());
  if (templateObject_->group()->shouldPreTenure(sweep)) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }
  // Stub doesn't support metadata builder
  if (cx_->realm()->hasAllocationMetadataBuilder()) {
    trackAttached(IRGenerator::NotAttached);
    return AttachDecision::NoAction;
  }

  writer.guardNoAllocationMetadataBuilder();
  writer.guardObjectGroupNotPretenured(templateObject_->group());

  // Bake in a monotonically increasing number to ensure we differentiate
  // between different baseline stubs that otherwise might share stub code.
  uint64_t id = cx_->runtime()->jitRuntime()->nextDisambiguationId();
  uint32_t idHi = id >> 32;
  uint32_t idLo = id & UINT32_MAX;
  writer.loadNewObjectFromTemplateResult(templateObject_, idHi, idLo);

  writer.returnFromIC();

  trackAttached("NewObjectWithTemplate");
  return AttachDecision::Attach;
}

#ifdef JS_SIMULATOR
bool js::jit::CallAnyNative(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* calleeObj = &args.callee();

  MOZ_ASSERT(calleeObj->is<JSFunction>());
  auto* calleeFunc = &calleeObj->as<JSFunction>();
  MOZ_ASSERT(calleeFunc->isNativeWithoutJitEntry());

  JSNative native = calleeFunc->native();
  return native(cx, args.length(), args.base());
}
#endif
