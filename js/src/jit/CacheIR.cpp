/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIR.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Unused.h"

#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/CacheIRSpewer.h"
#include "jit/InlinableNatives.h"
#include "vm/SelfHosting.h"

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

const char* const js::jit::CacheKindNames[] = {
#define DEFINE_KIND(kind) #kind,
    CACHE_IR_KINDS(DEFINE_KIND)
#undef DEFINE_KIND
};

// We need to enter the namespace here so that the definition of
// CacheIROpFormat::ArgLengths can see CacheIROpFormat::ArgType
// (without defining None/Id/Field/etc everywhere else in this file.)
namespace js {
namespace jit {
namespace CacheIROpFormat {

static constexpr uint32_t CacheIRArgLength(ArgType arg) {
  switch (arg) {
    case None:
      return 0;
    case Id:
      return sizeof(uint8_t);
    case Field:
      return sizeof(uint8_t);
    case Byte:
      return sizeof(uint8_t);
    case Int32:
    case UInt32:
      return sizeof(uint32_t);
    case Word:
      return sizeof(uintptr_t);
  }
}
template <typename... Args>
static constexpr uint32_t CacheIRArgLength(ArgType arg, Args... args) {
  return CacheIRArgLength(arg) + CacheIRArgLength(args...);
}

const uint32_t ArgLengths[] = {
#define ARGLENGTH(op, ...) CacheIRArgLength(__VA_ARGS__),
    CACHE_IR_OPS(ARGLENGTH)
#undef ARGLENGTH
};

}  // namespace CacheIROpFormat
}  // namespace jit
}  // namespace js

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
  if (shadows == ShadowCheckFailed) {
    cx->clearPendingException();
    return ProxyStubType::None;
  }

  if (DOMProxyIsShadowing(shadows)) {
    if (shadows == ShadowsViaDirectExpando ||
        shadows == ShadowsViaIndirectExpando) {
      return ProxyStubType::DOMExpando;
    }
    return ProxyStubType::DOMShadowed;
  }

  MOZ_ASSERT(shadows == DoesntShadow || shadows == DoesntShadowUnique);
  return ProxyStubType::DOMUnshadowed;
}

static bool ValueToNameOrSymbolId(JSContext* cx, HandleValue idval,
                                  MutableHandleId id, bool* nameOrSymbol) {
  *nameOrSymbol = false;

  if (!idval.isString() && !idval.isSymbol()) {
    return true;
  }

  if (!ValueToId<CanGC>(cx, idval, id)) {
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

  // Non-object receivers are a degenerate case, so don't try to attach
  // stubs. The stubs we do emit will still perform runtime checks and
  // fallback as needed.
  if (isSuper() && !receiver_.isObject()) {
    return AttachDecision::NoAction;
  }

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

  if (val_.isObject()) {
    RootedObject obj(cx_, &val_.toObject());
    ObjOperandId objId = writer.guardToObject(valId);
    if (nameOrSymbol) {
      TRY_ATTACH(tryAttachObjectLength(obj, objId, id));
      TRY_ATTACH(tryAttachNative(obj, objId, id));
      TRY_ATTACH(tryAttachTypedObject(obj, objId, id));
      TRY_ATTACH(tryAttachModuleNamespace(obj, objId, id));
      TRY_ATTACH(tryAttachWindowProxy(obj, objId, id));
      TRY_ATTACH(tryAttachCrossCompartmentWrapper(obj, objId, id));
      TRY_ATTACH(tryAttachXrayCrossCompartmentWrapper(obj, objId, id));
      TRY_ATTACH(tryAttachFunction(obj, objId, id));
      TRY_ATTACH(tryAttachProxy(obj, objId, id));

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

  TRY_ATTACH(tryAttachNative(obj, objId, id));

  // Object lengths are supported only if int32 results are allowed.
  TRY_ATTACH(tryAttachObjectLength(obj, objId, id));

  // Also support native data properties on DOMProxy prototypes.
  if (GetProxyStubType(cx_, obj, id) == ProxyStubType::DOMUnshadowed) {
    return tryAttachDOMProxyUnshadowed(obj, objId, id);
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
  CanAttachTemporarilyUnoptimizable
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

  if (getter.isBuiltinNative()) {
    return CanAttachNativeGetter;
  }

  if (getter.hasBytecode() || getter.isNativeWithJitEntry()) {
    return CanAttachScriptedGetter;
  }

  if (getter.isInterpreted()) {
    return CanAttachTemporarilyUnoptimizable;
  }

  return CanAttachNone;
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
    writer.guardProto(objId, obj->staticPrototype());
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
    protoId = writer.loadProto(protoId);

    writer.guardSpecificObject(protoId, pobj);
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
    // Guard on the proto if the shape does not imply the proto.
    bool guardProto = obj->hasUncacheableProto();

    obj = obj->staticPrototype();
    if (!obj && !guardProto) {
      return;
    }

    objId = writer.loadProto(objId);

    if (guardProto) {
      writer.guardSpecificObject(objId, obj);
    }

    if (!obj) {
      return;
    }

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

static void EmitCallGetterResultNoGuards(CacheIRWriter& writer, JSObject* obj,
                                         JSObject* holder, Shape* shape,
                                         ObjOperandId receiverId) {
  switch (IsCacheableGetPropCall(obj, holder, shape)) {
    case CanAttachNativeGetter: {
      JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
      MOZ_ASSERT(target->isBuiltinNative());
      writer.callNativeGetterResult(receiverId, target);
      writer.typeMonitorResult();
      break;
    }
    case CanAttachScriptedGetter: {
      JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
      MOZ_ASSERT(target->hasJitEntry());
      writer.callScriptedGetterResult(receiverId, target);
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

static void EmitCallGetterResult(CacheIRWriter& writer, JSObject* obj,
                                 JSObject* holder, Shape* shape,
                                 ObjOperandId objId, ObjOperandId receiverId,
                                 ICState::Mode mode) {
  EmitCallGetterResultGuards(writer, obj, holder, shape, objId, mode);
  EmitCallGetterResultNoGuards(writer, obj, holder, shape, receiverId);
}

static void EmitCallGetterResult(CacheIRWriter& writer, JSObject* obj,
                                 JSObject* holder, Shape* shape,
                                 ObjOperandId objId, ICState::Mode mode) {
  EmitCallGetterResult(writer, obj, holder, shape, objId, objId, mode);
}

static void EmitCallGetterByValueResult(CacheIRWriter& writer, JSObject* obj,
                                        JSObject* holder, Shape* shape,
                                        ObjOperandId objId,
                                        ValOperandId receiverId,
                                        ICState::Mode mode) {
  EmitCallGetterResultGuards(writer, obj, holder, shape, objId, mode);

  switch (IsCacheableGetPropCall(obj, holder, shape)) {
    case CanAttachNativeGetter: {
      JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
      MOZ_ASSERT(target->isBuiltinNative());
      writer.callNativeGetterByValueResult(receiverId, target);
      writer.typeMonitorResult();
      break;
    }
    case CanAttachScriptedGetter: {
      JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
      MOZ_ASSERT(target->hasJitEntry());
      writer.callScriptedGetterByValueResult(receiverId, target);
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

void GetPropIRGenerator::attachMegamorphicNativeSlot(ObjOperandId objId,
                                                     jsid id,
                                                     bool handleMissing) {
  MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);

  // The stub handles the missing-properties case only if we're seeing one
  // now, to make sure Ion ICs correctly monitor the undefined type.

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
                                                   HandleId id) {
  RootedShape shape(cx_);
  RootedNativeObject holder(cx_);

  NativeGetPropCacheability type =
      CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_, resultFlags_);
  switch (type) {
    case CanAttachNone:
      return AttachDecision::NoAction;
    case CanAttachTemporarilyUnoptimizable:
      return AttachDecision::TemporarilyUnoptimizable;
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
      // |super.prop| accesses use a |this| value that differs from lookup
      // object
      MOZ_ASSERT(!idempotent());
      ObjOperandId receiverId =
          isSuper() ? writer.guardToObject(getSuperReceiverValueId()) : objId;
      maybeEmitIdGuard(id);
      EmitCallGetterResult(writer, obj, holder, shape, objId, receiverId,
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
      MOZ_ASSERT(callee->isNative());
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
      EmitCallGetterResult(writer, windowObj, holder, shape, windowObjId,
                           mode_);

      trackAttached("WindowProxyGetter");
      return AttachDecision::Attach;
    }

    case CanAttachScriptedGetter:
    case CanAttachTemporarilyUnoptimizable:
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
    if (canCache == CanAttachTemporarilyUnoptimizable) {
      return AttachDecision::TemporarilyUnoptimizable;
    }
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
    HandleObject obj, ObjOperandId objId, HandleId id) {
  if (!IsProxy(obj)) {
    return AttachDecision::NoAction;
  }

  XrayJitInfo* info = GetXrayJitInfo();
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
      !getter->as<JSFunction>().isNative()) {
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
  writer.guardXrayExpandoShapeAndDefaultProto(objId, expandoShapeWrapper);
  for (size_t i = 0; i < prototypes.length(); i++) {
    JSObject* proto = prototypes[i];
    ObjOperandId protoId = writer.loadObject(proto);
    writer.guardXrayExpandoShapeAndDefaultProto(
        protoId, prototypeExpandoShapeWrappers[i]);
  }

  writer.callNativeGetterResult(objId, &getter->as<JSFunction>());
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
    writer.guardNotDOMProxy(objId);
  }

  if (cacheKind_ == CacheKind::GetProp || mode_ == ICState::Mode::Specialized) {
    MOZ_ASSERT(!isSuper());
    maybeEmitIdGuard(id);
    writer.callProxyGetResult(objId, id);
  } else {
    // Attach a stub that handles every id.
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
    MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);
    MOZ_ASSERT(!isSuper());
    writer.callProxyGetByValueResult(objId, getElemKeyValueId());
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

AttachDecision GetPropIRGenerator::tryAttachDOMProxyExpando(HandleObject obj,
                                                            ObjOperandId objId,
                                                            HandleId id) {
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
  if (canCache == CanAttachTemporarilyUnoptimizable) {
    return AttachDecision::TemporarilyUnoptimizable;
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
    EmitCallGetterResultNoGuards(writer, expandoObj, expandoObj, propShape,
                                 objId);
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
  writer.callProxyGetResult(objId, id);
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
    expandoId =
        writer.loadDOMExpandoValueGuardGeneration(objId, expandoAndGeneration);
    expandoVal = expandoAndGeneration->expando;
  } else {
    expandoId = writer.loadDOMExpandoValue(objId);
  }

  if (expandoVal.isUndefined()) {
    // Guard there's no expando object.
    writer.guardType(expandoId, ValueType::Undefined);
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
    HandleObject obj, ObjOperandId objId, HandleId id) {
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
  if (canCache == CanAttachTemporarilyUnoptimizable) {
    return AttachDecision::TemporarilyUnoptimizable;
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
      EmitCallGetterResultNoGuards(writer, checkObj, holder, shape, objId);
    }
  } else {
    // Property was not found on the prototype chain. Deoptimize down to
    // proxy get call.
    MOZ_ASSERT(!isSuper());
    writer.callProxyGetResult(objId, id);
    writer.typeMonitorResult();
  }

  trackAttached("DOMProxyUnshadowed");
  return AttachDecision::Attach;
}

AttachDecision GetPropIRGenerator::tryAttachProxy(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id) {
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
      TRY_ATTACH(tryAttachDOMProxyExpando(obj, objId, id));
      [[fallthrough]];  // Fall through to the generic shadowed case.
    case ProxyStubType::DOMShadowed:
      return tryAttachDOMProxyShadowed(obj, objId, id);
    case ProxyStubType::DOMUnshadowed:
      TRY_ATTACH(tryAttachDOMProxyUnshadowed(obj, objId, id));
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
    return Layout_TypedArray;
  }
  if (IsOutlineTypedObjectClass(clasp)) {
    return Layout_OutlineTypedObject;
  }
  if (IsInlineTypedObjectClass(clasp)) {
    return Layout_InlineTypedObject;
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
  writer.loadTypedObjectResult(objId, fieldOffset, layout, typeDescr);

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
    case CanAttachTemporarilyUnoptimizable:
      return AttachDecision::TemporarilyUnoptimizable;
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
        writer.guardType(valId, val_.type());
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
        writer.guardType(valId, val_.type());
      }
      maybeEmitIdGuard(id);

      ObjOperandId protoId = writer.loadObject(proto);
      EmitCallGetterByValueResult(writer, proto, holder, shape, protoId, valId,
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

AttachDecision GetPropIRGenerator::tryAttachStringChar(ValOperandId valId,
                                                       ValOperandId indexId) {
  MOZ_ASSERT(idVal_.isInt32());

  if (!val_.isString()) {
    return AttachDecision::NoAction;
  }

  int32_t index = idVal_.toInt32();
  if (index < 0) {
    return AttachDecision::NoAction;
  }

  JSString* str = val_.toString();
  if (size_t(index) >= str->length()) {
    return AttachDecision::NoAction;
  }

  // This follows JSString::getChar, otherwise we fail to attach getChar in a
  // lot of cases.
  if (str->isRope()) {
    JSRope* rope = &str->asRope();

    // Make sure the left side contains the index.
    if (size_t(index) >= rope->leftChild()->length()) {
      return AttachDecision::NoAction;
    }

    str = rope->leftChild();
  }

  if (!str->isLinear() || str->asLinear().latin1OrTwoByteChar(index) >=
                              StaticStrings::UNIT_STATIC_LIMIT) {
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
  writer.loadTypedElementResult(objId, indexId, layout,
                                TypedThingElementType(obj),
                                /* handleOOB = */ false);

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

  ValOperandId keyId = getElemKeyValueId();
  Int32OperandId indexId = writer.guardToTypedArrayIndex(keyId);

  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());

  writer.loadTypedElementResult(objId, indexId, layout,
                                TypedThingElementType(obj),
                                /* handleOOB = */ true);

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
  // We could call maybeEmitIdGuard here and then emit CallProxyGetResult,
  // but for GetElem we prefer to attach a stub that can handle any Value
  // so we don't attach a new stub for every id.
  MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
  MOZ_ASSERT(!isSuper());
  writer.callProxyGetByValueResult(objId, getElemKeyValueId());
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

  EmitCallGetterResultNoGuards(writer, &globalLexical->global(), holder, shape,
                               globalId);

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
  writer.callProxyHasPropResult(objId, keyId, hasOwn);
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
    *int32IndexId = writer.guardAndGetIndexFromString(strId);
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

OperandId SetPropIRGenerator::emitNumericGuard(ValOperandId valId,
                                               Scalar::Type type) {
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
      break;
  }
  MOZ_CRASH("Unsupported TypedArray type");
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
      writer.guardType(rhsId, ValueType::String);
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
  if (!setter.isBuiltinNative()) {
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

static bool IsCacheableSetPropCallScripted(
    JSObject* obj, JSObject* holder, Shape* shape,
    bool* isTemporarilyUnoptimizable = nullptr) {
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
  if (setter.isBuiltinNative()) {
    return false;
  }

  // Natives with jit entry can use the scripted path.
  if (setter.isNativeWithJitEntry()) {
    return true;
  }

  if (!setter.hasBytecode()) {
    if (isTemporarilyUnoptimizable) {
      *isTemporarilyUnoptimizable = true;
    }
    return false;
  }

  if (setter.isClassConstructor()) {
    return false;
  }

  return true;
}

static bool CanAttachSetter(JSContext* cx, jsbytecode* pc, HandleObject obj,
                            HandleId id, MutableHandleObject holder,
                            MutableHandleShape propShape,
                            bool* isTemporarilyUnoptimizable) {
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
  if (!IsCacheableSetPropCallScripted(obj, holder, propShape,
                                      isTemporarilyUnoptimizable) &&
      !IsCacheableSetPropCallNative(obj, holder, propShape)) {
    return false;
  }

  return true;
}

static void EmitCallSetterNoGuards(CacheIRWriter& writer, JSObject* obj,
                                   JSObject* holder, Shape* shape,
                                   ObjOperandId objId, ValOperandId rhsId) {
  if (IsCacheableSetPropCallNative(obj, holder, shape)) {
    JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
    MOZ_ASSERT(target->isBuiltinNative());
    writer.callNativeSetter(objId, target, rhsId);
    writer.returnFromIC();
    return;
  }

  MOZ_ASSERT(IsCacheableSetPropCallScripted(obj, holder, shape));

  JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
  MOZ_ASSERT(target->hasJitEntry());
  writer.callScriptedSetter(objId, target, rhsId);
  writer.returnFromIC();
}

AttachDecision SetPropIRGenerator::tryAttachSetter(HandleObject obj,
                                                   ObjOperandId objId,
                                                   HandleId id,
                                                   ValOperandId rhsId) {
  RootedObject holder(cx_);
  RootedShape propShape(cx_);
  bool isTemporarilyUnoptimizable = false;
  if (!CanAttachSetter(cx_, pc_, obj, id, &holder, &propShape,
                       &isTemporarilyUnoptimizable)) {
    return isTemporarilyUnoptimizable ? AttachDecision::TemporarilyUnoptimizable
                                      : AttachDecision::NoAction;
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

  EmitCallSetterNoGuards(writer, obj, holder, propShape, objId, rhsId);

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
  if (!nobj->containsDenseElement(index) ||
      nobj->getElementsHeader()->isFrozen()) {
    return AttachDecision::NoAction;
  }

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
    // because we're not allowed to shadow them. There are a few cases to
    // consider:
    //
    // * If the proto is extensible, its Shape will change when it's made
    //   non-extensible.
    //
    // * If the proto is already non-extensible, no new elements will be
    //   added, so if there are no elements now it doesn't matter if the
    //   object is frozen later on.
    NativeObject* nproto = &proto->as<NativeObject>();
    if (!nproto->isExtensible() && nproto->getDenseInitializedLength() > 0) {
      return false;
    }

    obj = nproto;
  } while (true);

  return true;
}

AttachDecision SetPropIRGenerator::tryAttachSetDenseElementHole(
    HandleObject obj, ObjOperandId objId, uint32_t index,
    Int32OperandId indexId, ValOperandId rhsId) {
  if (!obj->isNative() || rhsVal_.isMagic(JS_ELEMENTS_HOLE)) {
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

  MOZ_ASSERT(!nobj->getElementsHeader()->isFrozen(),
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
  if (Scalar::isBigIntType(elementType)) {
    if (!rhsVal_.isBigInt()) {
      return AttachDecision::NoAction;
    }
  } else {
    if (!rhsVal_.isNumber()) {
      return AttachDecision::NoAction;
    }
  }

  if (IsPrimitiveArrayTypedObject(obj)) {
    writer.guardGroupForLayout(objId, obj->group());
  } else {
    writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());
  }

  OperandId rhsValId = emitNumericGuard(rhsId, elementType);

  writer.storeTypedElement(objId, layout, elementType, indexId, rhsValId,
                           handleOutOfBounds);
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
  TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

  // Don't attach if the input type doesn't match the guard added below.
  if (Scalar::isBigIntType(elementType)) {
    if (!rhsVal_.isBigInt()) {
      return AttachDecision::NoAction;
    }
  } else {
    if (!rhsVal_.isNumber()) {
      return AttachDecision::NoAction;
    }
  }

  ValOperandId keyId = setElemKeyValueId();
  Int32OperandId indexId = writer.guardToTypedArrayIndex(keyId);

  writer.guardShapeForClass(objId, obj->as<TypedArrayObject>().shape());

  OperandId rhsValId = emitNumericGuard(rhsId, elementType);

  // When the index isn't an int32 index, we always assume the TypedArray access
  // can be out-of-bounds.
  bool handleOutOfBounds = true;

  writer.storeTypedElement(objId, layout, elementType, indexId, rhsValId,
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
    writer.guardNotDOMProxy(objId);
  }

  if (cacheKind_ == CacheKind::SetProp || mode_ == ICState::Mode::Specialized) {
    maybeEmitIdGuard(id);
    writer.callProxySet(objId, id, rhsId, IsStrictSetPC(pc_));
  } else {
    // Attach a stub that handles every id.
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);
    writer.callProxySetByValue(objId, setElemKeyValueId(), rhsId,
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
  writer.callProxySet(objId, id, rhsId, IsStrictSetPC(pc_));
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
  bool isTemporarilyUnoptimizable = false;
  if (!CanAttachSetter(cx_, pc_, proto, id, &holder, &propShape,
                       &isTemporarilyUnoptimizable)) {
    return isTemporarilyUnoptimizable ? AttachDecision::TemporarilyUnoptimizable
                                      : AttachDecision::NoAction;
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
  EmitCallSetterNoGuards(writer, proto, holder, propShape, objId, rhsId);

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
  if (CanAttachSetter(cx_, pc_, expandoObj, id, &holder, &propShape,
                      &isTemporarilyUnoptimizable)) {
    // Note that we don't actually use the expandoObjId here after the
    // shape guard. The DOM proxy (objId) is passed to the setter as
    // |this|.
    maybeEmitIdGuard(id);
    guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

    MOZ_ASSERT(holder == expandoObj);
    EmitCallSetterNoGuards(writer, expandoObj, expandoObj, propShape, objId,
                           rhsId);
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
  writer.callProxySetByValue(objId, setElemKeyValueId(), rhsId,
                             IsStrictSetPC(pc_));
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

    // We check group->maybeInterpretedFunction() here and guard on the
    // group. The group is unique for a particular function so this ensures
    // we don't add the default prototype property to functions that don't
    // have it.
    JSFunction* fun = &obj->as<JSFunction>();
    if (!obj->group()->maybeInterpretedFunction() ||
        !fun->needsPrototypeProperty()) {
      return false;
    }

    // If property exists this isn't an "add"
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

  // Object must be extensible.
  if (!obj->nonProxyIsExtensible()) {
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

  // In addition to guarding for type barrier, we need this group guard (or
  // shape guard below) to ensure class is unchanged. This group guard may also
  // imply maybeInterpretedFunction() for the special-case of function
  // prototype property set.
  writer.guardGroup(objId, oldGroup);

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

  // Shape guard the holder.
  ObjOperandId holderId = objId;
  writer.guardShape(holderId, oldShape);

  ShapeGuardProtoChain(writer, obj, objId);

  ObjectGroup* newGroup = obj->group();

  // Check if we have to change the object's group. We only have to change from
  // a partially to fully initialized group if the object is a PlainObject.
  bool changeGroup = oldGroup != newGroup;
  MOZ_ASSERT_IF(changeGroup, obj->is<PlainObject>());

  if (holder->isFixedSlot(propShape->slot())) {
    size_t offset = NativeObject::getFixedSlotOffset(propShape->slot());
    writer.addAndStoreFixedSlot(holderId, offset, rhsValId, propShape,
                                changeGroup, newGroup);
    trackAttached("AddSlot");
  } else {
    size_t offset = holder->dynamicSlotIndex(propShape->slot()) * sizeof(Value);
    uint32_t numOldSlots = NativeObject::dynamicSlotsCount(oldShape);
    uint32_t numNewSlots = NativeObject::dynamicSlotsCount(propShape);
    if (numOldSlots == numNewSlots) {
      writer.addAndStoreDynamicSlot(holderId, offset, rhsValId, propShape,
                                    changeGroup, newGroup);
      trackAttached("AddSlot");
    } else {
      MOZ_ASSERT(numNewSlots > numOldSlots);
      writer.allocateAndStoreDynamicSlot(holderId, offset, rhsValId, propShape,
                                         changeGroup, newGroup, numNewSlots);
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
  writer.guardFunctionPrototype(rhsId, slot, protoId);

  // Needn't guard LHS is object, because the actual stub can handle that
  // and correctly return false.
  writer.loadInstanceOfObjectResult(lhs, protoId, slot);
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
    writer.guardType(valId, val_.type());
  }

  writer.loadStringResult(TypeName(js::TypeOfValue(val_), cx_->names()));
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

CallIRGenerator::CallIRGenerator(JSContext* cx, HandleScript script,
                                 jsbytecode* pc, JSOp op, ICState::Mode mode,
                                 uint32_t argc, HandleValue callee,
                                 HandleValue thisval, HandleValue newTarget,
                                 HandleValueArray args)
    : IRGenerator(cx, script, pc, CacheKind::Call, mode),
      op_(op),
      argc_(argc),
      callee_(callee),
      thisval_(thisval),
      newTarget_(newTarget),
      args_(args),
      typeCheckInfo_(cx, /* needsTypeBarrier = */ true),
      cacheIRStubKind_(BaselineCacheIRStubKind::Regular) {}

AttachDecision CallIRGenerator::tryAttachArrayPush() {
  // Only optimize on obj.push(val);
  if (argc_ != 1 || !thisval_.isObject()) {
    return AttachDecision::NoAction;
  }

  // Where |obj| is a native array.
  RootedObject thisobj(cx_, &thisval_.toObject());
  if (!thisobj->is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }

  if (thisobj->hasLazyGroup()) {
    return AttachDecision::NoAction;
  }

  RootedArrayObject thisarray(cx_, &thisobj->as<ArrayObject>());

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

  MOZ_ASSERT(!thisarray->getElementsHeader()->isFrozen(),
             "Extensible arrays should not have frozen elements");
  MOZ_ASSERT(thisarray->lengthIsWritable());

  // After this point, we can generate code fine.

  // Generate code.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Ensure argc == 1.
  writer.guardSpecificInt32Immediate(argcId, 1);

  // Guard callee is the |js::array_push| native function.
  ValOperandId calleeValId =
      writer.loadArgumentFixedSlot(ArgumentKind::Callee, argc_);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificNativeFunction(calleeObjId, js::array_push);

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

AttachDecision CallIRGenerator::tryAttachArrayJoin() {
  // Only handle argc <= 1.
  if (argc_ > 1) {
    return AttachDecision::NoAction;
  }

  // Only optimize on obj.join(...);
  if (!thisval_.isObject()) {
    return AttachDecision::NoAction;
  }

  // Where |obj| is a native array.
  RootedObject thisobj(cx_, &thisval_.toObject());
  if (!thisobj->is<ArrayObject>()) {
    return AttachDecision::NoAction;
  }

  RootedArrayObject thisarray(cx_, &thisobj->as<ArrayObject>());

  // And the array is of length 0 or 1.
  if (thisarray->length() > 1) {
    return AttachDecision::NoAction;
  }

  // And the array is packed.
  if (thisarray->getDenseInitializedLength() != thisarray->length()) {
    return AttachDecision::NoAction;
  }

  // And the only element (if it exists) is a string
  if (thisarray->length() == 1 && !thisarray->getDenseElement(0).isString()) {
    return AttachDecision::NoAction;
  }

  // We don't need to worry about indexed properties because we can perform
  // hole check manually.

  // Generate code.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard callee is the |js::array_join| native function.
  ValOperandId calleeValId =
      writer.loadArgumentFixedSlot(ArgumentKind::Callee, argc_);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificNativeFunction(calleeObjId, js::array_join);

  if (argc_ == 1) {
    // If argcount is 1, guard that the argument is a string.
    ValOperandId argValId =
        writer.loadArgumentFixedSlot(ArgumentKind::Arg0, argc_);
    writer.guardToString(argValId);
  }

  // Guard this is an array object.
  ValOperandId thisValId =
      writer.loadArgumentFixedSlot(ArgumentKind::This, argc_);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);
  writer.guardClass(thisObjId, GuardClassKind::Array);

  // Do the join.
  writer.arrayJoinResult(thisObjId);

  writer.returnFromIC();

  // The result of this stub does not need to be monitored because it will
  // always return a string.  We will add String to the stack typeset when
  // attaching this stub.

  // Set the stub kind to Regular
  cacheIRStubKind_ = BaselineCacheIRStubKind::Regular;

  trackAttached("ArrayJoin");
  return AttachDecision::Attach;
}

AttachDecision CallIRGenerator::tryAttachIsSuspendedGenerator() {
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

AttachDecision CallIRGenerator::tryAttachFunCall(HandleFunction calleeFunc) {
  MOZ_ASSERT(calleeFunc->isNative());
  if (calleeFunc->native() != fun_call) {
    return AttachDecision::NoAction;
  }

  if (!thisval_.isObject() || !thisval_.toObject().is<JSFunction>()) {
    return AttachDecision::NoAction;
  }
  RootedFunction target(cx_, &thisval_.toObject().as<JSFunction>());

  bool isScripted = target->isInterpreted() || target->isNativeWithJitEntry();
  MOZ_ASSERT_IF(!isScripted, target->isNative());

  if (target->isClassConstructor()) {
    return AttachDecision::NoAction;
  }
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard that callee is the |fun_call| native function.
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificNativeFunction(calleeObjId, fun_call);

  // Guard that |this| is a function.
  ValOperandId thisValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::This, argcId);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);
  writer.guardClass(thisObjId, GuardClassKind::JSFunction);

  // Guard that function is not a class constructor.
  writer.guardNotClassConstructor(thisObjId);

  CallFlags targetFlags(CallFlags::FunCall);
  if (isScripted) {
    writer.guardFunctionHasJitEntry(thisObjId, /*isConstructing =*/false);
    writer.callScriptedFunction(thisObjId, argcId, targetFlags);
  } else {
    writer.guardFunctionIsNative(thisObjId);
    writer.callAnyNativeFunction(thisObjId, argcId, targetFlags);
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

AttachDecision CallIRGenerator::tryAttachFunApply(HandleFunction calleeFunc) {
  MOZ_ASSERT(calleeFunc->isNative());
  if (calleeFunc->native() != fun_apply) {
    return AttachDecision::NoAction;
  }

  if (argc_ != 2) {
    return AttachDecision::NoAction;
  }

  if (!thisval_.isObject() || !thisval_.toObject().is<JSFunction>()) {
    return AttachDecision::NoAction;
  }
  RootedFunction target(cx_, &thisval_.toObject().as<JSFunction>());

  bool isScripted = target->isInterpreted() || target->isNativeWithJitEntry();
  MOZ_ASSERT_IF(!isScripted, target->isNative());

  if (target->isClassConstructor()) {
    return AttachDecision::NoAction;
  }

  CallFlags::ArgFormat format = CallFlags::Standard;
  if (args_[1].isMagic(JS_OPTIMIZED_ARGUMENTS) && !script_->needsArgsObj()) {
    format = CallFlags::FunApplyArgs;
  } else if (args_[1].isObject() && args_[1].toObject().is<ArrayObject>() &&
             args_[1].toObject().as<ArrayObject>().length() <=
                 CacheIRCompiler::MAX_ARGS_ARRAY_LENGTH) {
    format = CallFlags::FunApplyArray;
  } else {
    return AttachDecision::NoAction;
  }

  Int32OperandId argcId(writer.setInputOperandId(0));

  // Guard that callee is the |fun_apply| native function.
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);
  writer.guardSpecificNativeFunction(calleeObjId, fun_apply);

  // Guard that |this| is a function.
  ValOperandId thisValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::This, argcId);
  ObjOperandId thisObjId = writer.guardToObject(thisValId);
  writer.guardClass(thisObjId, GuardClassKind::JSFunction);

  // Guard that function is not a class constructor.
  writer.guardNotClassConstructor(thisObjId);

  CallFlags targetFlags(format);
  writer.guardFunApply(argcId, targetFlags);

  if (isScripted) {
    // Guard that function is scripted.
    writer.guardFunctionHasJitEntry(thisObjId, /*isConstructing =*/false);
    writer.callScriptedFunction(thisObjId, argcId, targetFlags);
  } else {
    // Guard that function is native.
    writer.guardFunctionIsNative(thisObjId);
    writer.callAnyNativeFunction(thisObjId, argcId, targetFlags);
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

AttachDecision CallIRGenerator::tryAttachSpecialCaseCallNative(
    HandleFunction callee) {
  MOZ_ASSERT(mode_ == ICState::Mode::Specialized);
  MOZ_ASSERT(callee->isNative());

  // Special case functions are only optimized for normal calls.
  if (op_ != JSOp::Call && op_ != JSOp::CallIgnoresRv) {
    return AttachDecision::NoAction;
  }

  // Check for special-cased native functions.
  if (callee->native() == js::array_push) {
    return tryAttachArrayPush();
  }
  if (callee->native() == js::array_join) {
    return tryAttachArrayJoin();
  }
  if (callee->native() == intrinsic_IsSuspendedGenerator) {
    return tryAttachIsSuspendedGenerator();
  }

  return AttachDecision::NoAction;
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

  // Only attach a stub if the function already has a prototype and
  // we can look it up without causing side effects.
  RootedValue protov(cx_);
  RootedObject newTarget(cx_, &newTarget_.toObject());
  if (!GetPropertyPure(cx_, newTarget, NameToId(cx_->names().prototype),
                       protov.address())) {
    // Can't purely lookup function prototype
    trackAttached(IRGenerator::NotAttached);
    *skipAttach = true;
    return true;
  }

  if (protov.isObject()) {
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

  JSObject* thisObject =
      CreateThisForFunction(cx_, calleeFunc, newTarget, TenuredObject);
  if (!thisObject) {
    return false;
  }

  MOZ_ASSERT(thisObject->nonCCWRealm() == calleeFunc->realm());

  if (thisObject->is<PlainObject>()) {
    result.set(thisObject);
  }

  return true;
}

AttachDecision CallIRGenerator::tryAttachCallScripted(
    HandleFunction calleeFunc) {
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

  if (!calleeFunc->hasJitEntry()) {
    // Don't treat this as an unoptimizable case, as we'll add a
    // stub when the callee is delazified.
    return AttachDecision::TemporarilyUnoptimizable;
  }

  if (isConstructing && !calleeFunc->hasJitScript()) {
    // If we're constructing, require the callee to have a JitScript. This isn't
    // required for correctness but avoids allocating a template object below
    // for constructors that aren't hot. See bug 1419758.
    return AttachDecision::TemporarilyUnoptimizable;
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

  // Load argc.
  Int32OperandId argcId(writer.setInputOperandId(0));

  // Load the callee and ensure it is an object
  ValOperandId calleeValId =
      writer.loadArgumentDynamicSlot(ArgumentKind::Callee, argcId, flags);
  ObjOperandId calleeObjId = writer.guardToObject(calleeValId);

  FieldOffset calleeOffset = 0;
  if (isSpecialized) {
    // Ensure callee matches this stub's callee
    calleeOffset = writer.guardSpecificFunction(calleeObjId, calleeFunc);
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

  if (templateObj) {
    MOZ_ASSERT(isSpecialized);
    writer.metaScriptedTemplateObject(templateObj, calleeOffset);
  }

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

  // Check for natives to which template objects can be attached. This is
  // done to provide templates to Ion for inlining these natives later on.
  switch (calleeFunc->jitInfo()->inlinableNative) {
    case InlinableNative::Array: {
      // Note: the template array won't be used if its length is inaccurately
      // computed here.  (We allocate here because compilation may occur on a
      // separate thread where allocation is impossible.)
      size_t count = 0;
      if (args_.length() != 1) {
        count = args_.length();
      } else if (args_.length() == 1 && args_[0].isInt32() &&
                 args_[0].toInt32() >= 0) {
        count = args_[0].toInt32();
      }

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

      res.set(NewFullyAllocatedArrayTryReuseGroup(cx_, obj, 0, TenuredObject));
      return !!res;
    }

    case InlinableNative::String: {
      RootedString emptyString(cx_, cx_->runtime()->emptyString);
      res.set(StringObject::create(cx_, emptyString, /* proto = */ nullptr,
                                   TenuredObject));
      return !!res;
    }

    case InlinableNative::ObjectCreate: {
      if (args_.length() != 1 || !args_[0].isObjectOrNull()) {
        return true;
      }
      RootedObject proto(cx_, args_[0].toObjectOrNull());
      res.set(ObjectCreateImpl(cx_, proto, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewArrayIterator: {
      res.set(NewArrayIteratorObject(cx_, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewStringIterator: {
      res.set(NewStringIteratorObject(cx_, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewRegExpStringIterator: {
      res.set(NewRegExpStringIteratorObject(cx_, TenuredObject));
      return !!res;
    }

    case InlinableNative::TypedArrayConstructor: {
      return TypedArrayObject::GetTemplateObjectForNative(
          cx_, calleeFunc->native(), args_, res);
    }

    default:
      return true;
  }
}

AttachDecision CallIRGenerator::tryAttachCallNative(HandleFunction calleeFunc) {
  MOZ_ASSERT(calleeFunc->isNative());

  bool isSpecialized = mode_ == ICState::Mode::Specialized;

  bool isSpread = IsSpreadPC(pc_);
  bool isSameRealm = isSpecialized && cx_->realm() == calleeFunc->realm();
  bool isConstructing = IsConstructPC(pc_);
  CallFlags flags(isConstructing, isSpread, isSameRealm);

  if (isConstructing && !calleeFunc->isConstructor()) {
    return AttachDecision::NoAction;
  }

  // Check for specific native-function optimizations.
  if (isSpecialized) {
    TRY_ATTACH(tryAttachSpecialCaseCallNative(calleeFunc));
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

  FieldOffset calleeOffset = 0;
  if (isSpecialized) {
    // Ensure callee matches this stub's callee
    calleeOffset = writer.guardSpecificFunction(calleeObjId, calleeFunc);
    writer.callNativeFunction(calleeObjId, argcId, op_, calleeFunc, flags);
  } else {
    // Guard that object is a native function
    writer.guardClass(calleeObjId, GuardClassKind::JSFunction);
    writer.guardFunctionIsNative(calleeObjId);

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
    writer.metaNativeTemplateObject(templateObj, calleeOffset);
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
  if (op_ == JSOp::FunApply) {
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

  RootedFunction calleeFunc(cx_, &calleeObj->as<JSFunction>());

  // Check for scripted optimizations.
  if (calleeFunc->isInterpreted() || calleeFunc->isNativeWithJitEntry()) {
    return tryAttachCallScripted(calleeFunc);
  }

  // Check for native-function optimizations.
  if (calleeFunc->isNative()) {
    if (op_ == JSOp::FunCall) {
      return tryAttachFunCall(calleeFunc);
    }
    if (op_ == JSOp::FunApply) {
      return tryAttachFunApply(calleeFunc);
    }
    return tryAttachCallNative(calleeFunc);
  }

  return AttachDecision::NoAction;
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
    if (!obj) {
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
  masm.branchTestObject(Assembler::NotEqual, privateAddr, failure);
  masm.unboxObject(privateAddr, dst);
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

  Int32OperandId lhsIntId = lhsVal_.isBoolean() ? writer.guardToBoolean(lhsId)
                                                : writer.guardToInt32(lhsId);
  Int32OperandId rhsIntId = rhsVal_.isBoolean() ? writer.guardToBoolean(rhsId)
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
      return writer.guardAndGetNumberFromString(strId);
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
      Int32OperandId boolId = writer.guardToBoolean(vId);
      return writer.guardAndGetNumberFromBoolean(boolId);
    }
    if (v.isString()) {
      StringOperandId strId = writer.guardToString(vId);
      return writer.guardAndGetNumberFromString(strId);
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
      return writer.guardToBoolean(vId);
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
  TRY_ATTACH(tryAttachDouble());
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
  writer.guardType(valId, ValueType::Int32);
  writer.loadInt32TruthyResult(valId);
  writer.returnFromIC();
  trackAttached("ToBoolInt32");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachDouble() {
  if (!val_.isDouble()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  writer.guardType(valId, ValueType::Double);
  writer.loadDoubleTruthyResult(valId);
  writer.returnFromIC();
  trackAttached("ToBoolDouble");
  return AttachDecision::Attach;
}

AttachDecision ToBoolIRGenerator::tryAttachSymbol() {
  if (!val_.isSymbol()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  writer.guardType(valId, ValueType::Symbol);
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
    default:
      MOZ_CRASH("unexpected OP");
  }

  writer.returnFromIC();
  return AttachDecision::Attach;
}

AttachDecision UnaryArithIRGenerator::tryAttachNumber() {
  if (!val_.isNumber() || !res_.isNumber()) {
    return AttachDecision::NoAction;
  }

  ValOperandId valId(writer.setInputOperandId(0));
  NumberOperandId numId = writer.guardIsNumber(valId);
  Int32OperandId truncatedId;
  switch (op_) {
    case JSOp::BitNot:
      truncatedId = writer.truncateDoubleToUInt32(numId);
      writer.int32NotResult(truncatedId);
      trackAttached("UnaryArith.DoubleNot");
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
    default:
      MOZ_CRASH("Unexpected OP");
  }

  writer.returnFromIC();
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

  // Bitwise operations with Int32 operands
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

  // All ops, with the exception of Ursh produce Int32 values.
  MOZ_ASSERT_IF(op_ != JSOp::Ursh, res_.isInt32());

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  // Convert type into int32 for the bitwise/shift operands.
  auto guardToInt32 = [&](ValOperandId id, HandleValue val) {
    if (val.isInt32()) {
      return writer.guardToInt32(id);
    }
    if (val.isBoolean()) {
      return writer.guardToBoolean(id);
    }
    MOZ_ASSERT(val.isDouble());
    writer.guardType(id, ValueType::Double);
    return writer.truncateDoubleToUInt32(id);
  };

  Int32OperandId lhsIntId = guardToInt32(lhsId, lhs_);
  Int32OperandId rhsIntId = guardToInt32(rhsId, rhs_);

  switch (op_) {
    case JSOp::BitOr:
      writer.int32BitOrResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.BitOr");
      break;
    case JSOp::BitXor:
      writer.int32BitXOrResult(lhsIntId, rhsIntId);
      trackAttached("BinaryArith.Bitwise.BitXOr");
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
      op_ != JSOp::Div && op_ != JSOp::Mod) {
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
      op_ != JSOp::Div && op_ != JSOp::Mod) {
    return AttachDecision::NoAction;
  }

  ValOperandId lhsId(writer.setInputOperandId(0));
  ValOperandId rhsId(writer.setInputOperandId(1));

  auto guardToInt32 = [&](ValOperandId id, HandleValue v) {
    if (v.isInt32()) {
      return writer.guardToInt32(id);
    }
    MOZ_ASSERT(v.isBoolean());
    return writer.guardToBoolean(id);
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

  auto guardToString = [&](ValOperandId id, HandleValue v) {
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
  };

  StringOperandId lhsStrId = guardToString(lhsId, lhs_);
  StringOperandId rhsStrId = guardToString(rhsId, rhs_);

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
    Int32OperandId intId = writer.guardToBoolean(id);
    return writer.booleanToString(intId);
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
    NumberOperandId numId = writer.guardAndGetNumberFromString(strId);
    return writer.guardAndGetInt32FromNumber(numId);
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
  if (templateObject_->as<PlainObject>().hasDynamicSlots()) {
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
  writer.loadNewObjectFromTemplateResult(templateObject_);
  writer.returnFromIC();

  trackAttached("NewObjectWithTemplate");
  return AttachDecision::Attach;
}

#ifdef JS_SIMULATOR
bool js::jit::CallAnyNative(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject calleeObj(cx, &args.callee());

  MOZ_ASSERT(calleeObj->is<JSFunction>());
  RootedFunction calleeFunc(cx, &calleeObj->as<JSFunction>());
  MOZ_ASSERT(calleeFunc->isNative());

  JSNative native = calleeFunc->native();
  return native(cx, args.length(), args.base());
}
#endif
