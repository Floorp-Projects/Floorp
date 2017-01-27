/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIR.h"

#include "mozilla/FloatingPoint.h"

#include "jit/BaselineIC.h"
#include "jit/IonCaches.h"

#include "jsobjinlines.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/UnboxedObject-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

IRGenerator::IRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind)
  : writer(cx),
    cx_(cx),
    pc_(pc),
    cacheKind_(cacheKind)
{}

GetPropIRGenerator::GetPropIRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind,
                                       ICStubEngine engine,
                                       bool* isTemporarilyUnoptimizable,
                                       HandleValue val, HandleValue idVal,
                                       CanAttachGetter canAttachGetter)
  : IRGenerator(cx, pc, cacheKind),
    val_(val),
    idVal_(idVal),
    engine_(engine),
    isTemporarilyUnoptimizable_(isTemporarilyUnoptimizable),
    canAttachGetter_(canAttachGetter),
    preliminaryObjectAction_(PreliminaryObjectAction::None)
{}

static void
EmitLoadSlotResult(CacheIRWriter& writer, ObjOperandId holderOp, NativeObject* holder,
                   Shape* shape)
{
    if (holder->isFixedSlot(shape->slot())) {
        writer.loadFixedSlotResult(holderOp, NativeObject::getFixedSlotOffset(shape->slot()));
    } else {
        size_t dynamicSlotOffset = holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
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

static ProxyStubType
GetProxyStubType(JSContext* cx, HandleObject obj, HandleId id)
{
    if (!obj->is<ProxyObject>())
        return ProxyStubType::None;

    if (!IsCacheableDOMProxy(obj))
        return ProxyStubType::Generic;

    DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx, obj, id);
    if (shadows == ShadowCheckFailed) {
        cx->clearPendingException();
        return ProxyStubType::None;
    }

    if (DOMProxyIsShadowing(shadows)) {
        if (shadows == ShadowsViaDirectExpando || shadows == ShadowsViaIndirectExpando)
            return ProxyStubType::DOMExpando;
        return ProxyStubType::DOMShadowed;
    }

    MOZ_ASSERT(shadows == DoesntShadow || shadows == DoesntShadowUnique);
    return ProxyStubType::DOMUnshadowed;
}

bool
GetPropIRGenerator::tryAttachStub()
{
    // Idempotent ICs should call tryAttachIdempotentStub instead.
    MOZ_ASSERT(!idempotent());

    AutoAssertNoPendingException aanpe(cx_);

    ValOperandId valId(writer.setInputOperandId(0));
    if (cacheKind_ == CacheKind::GetElem) {
        MOZ_ASSERT(getElemKeyValueId().id() == 1);
        writer.setInputOperandId(1);
    }

    RootedId id(cx_);
    bool nameOrSymbol;
    if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
        cx_->clearPendingException();
        return false;
    }

    if (val_.isObject()) {
        RootedObject obj(cx_, &val_.toObject());
        ObjOperandId objId = writer.guardIsObject(valId);
        if (nameOrSymbol) {
            if (tryAttachObjectLength(obj, objId, id))
                return true;
            if (tryAttachNative(obj, objId, id))
                return true;
            if (tryAttachUnboxed(obj, objId, id))
                return true;
            if (tryAttachUnboxedExpando(obj, objId, id))
                return true;
            if (tryAttachTypedObject(obj, objId, id))
                return true;
            if (tryAttachModuleNamespace(obj, objId, id))
                return true;
            if (tryAttachWindowProxy(obj, objId, id))
                return true;
            if (tryAttachFunction(obj, objId, id))
                return true;
            if (tryAttachProxy(obj, objId, id))
                return true;
            return false;
        }

        MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);

        if (tryAttachProxyElement(obj, objId))
            return true;

        uint32_t index;
        Int32OperandId indexId;
        if (maybeGuardInt32Index(idVal_, getElemKeyValueId(), &index, &indexId)) {
            if (tryAttachTypedElement(obj, objId, index, indexId))
                return true;
            if (tryAttachDenseElement(obj, objId, index, indexId))
                return true;
            if (tryAttachDenseElementHole(obj, objId, index, indexId))
                return true;
            if (tryAttachUnboxedArrayElement(obj, objId, index, indexId))
                return true;
            if (tryAttachArgumentsObjectArg(obj, objId, index, indexId))
                return true;
            return false;
        }

        return false;
    }

    if (nameOrSymbol) {
        if (tryAttachPrimitive(valId, id))
            return true;
        if (tryAttachStringLength(valId, id))
            return true;
        if (tryAttachMagicArgumentsName(valId, id))
            return true;
        return false;
    }

    if (idVal_.isInt32()) {
        ValOperandId indexId = getElemKeyValueId();
        if (tryAttachStringChar(valId, indexId))
            return true;
        if (tryAttachMagicArgument(valId, indexId))
            return true;
        return false;
    }

    return false;
}

bool
GetPropIRGenerator::tryAttachIdempotentStub()
{
    // For idempotent ICs, only attach stubs for plain data properties.
    // This ensures (1) the lookup has no side-effects and (2) Ion has complete
    // static type information and we don't have to monitor the result. Because
    // of (2), we don't support for instance missing properties or array
    // lengths, as TI does not account for these cases.

    MOZ_ASSERT(idempotent());

    RootedObject obj(cx_, &val_.toObject());
    RootedId id(cx_, NameToId(idVal_.toString()->asAtom().asPropertyName()));

    ValOperandId valId(writer.setInputOperandId(0));
    ObjOperandId objId = writer.guardIsObject(valId);
    if (tryAttachNative(obj, objId, id))
        return true;

    // Also support native data properties on DOMProxy prototypes.
    if (GetProxyStubType(cx_, obj, id) == ProxyStubType::DOMUnshadowed)
        return tryAttachDOMProxyUnshadowed(obj, objId, id);

    return false;
}

static bool
IsCacheableNoProperty(JSContext* cx, JSObject* obj, JSObject* holder, Shape* shape, jsid id,
                      jsbytecode* pc)
{
    if (shape)
        return false;

    MOZ_ASSERT(!holder);

    if (!pc) {
        // This is an idempotent IC, don't attach a missing-property stub.
        // See tryAttachStub.
        return false;
    }

    // If we're doing a name lookup, we have to throw a ReferenceError. If
    // extra warnings are enabled, we may have to report a warning.
    if (*pc == JSOP_GETXPROP || cx->compartment()->behaviors().extraWarnings(cx))
        return false;

    return CheckHasNoSuchProperty(cx, obj, id);
}

enum NativeGetPropCacheability {
    CanAttachNone,
    CanAttachReadSlot,
    CanAttachCallGetter,
};

static NativeGetPropCacheability
CanAttachNativeGetProp(JSContext* cx, HandleObject obj, HandleId id,
                       MutableHandleNativeObject holder, MutableHandleShape shape,
                       jsbytecode* pc, ICStubEngine engine, CanAttachGetter canAttachGetter,
                       bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(JSID_IS_STRING(id) || JSID_IS_SYMBOL(id));

    // The lookup needs to be universally pure, otherwise we risk calling hooks out
    // of turn. We don't mind doing this even when purity isn't required, because we
    // only miss out on shape hashification, which is only a temporary perf cost.
    // The limits were arbitrarily set, anyways.
    JSObject* baseHolder = nullptr;
    PropertyResult prop;
    if (!LookupPropertyPure(cx, obj, id, &baseHolder, &prop))
        return CanAttachNone;

    MOZ_ASSERT(!holder);
    if (baseHolder) {
        if (!baseHolder->isNative())
            return CanAttachNone;
        holder.set(&baseHolder->as<NativeObject>());
    }
    shape.set(prop.maybeShape());

    if (IsCacheableGetPropReadSlotForIonOrCacheIR(obj, holder, prop))
        return CanAttachReadSlot;

    // Idempotent ICs only support plain data properties, see
    // tryAttachIdempotentStub.
    if (!pc)
        return CanAttachNone;

    if (IsCacheableNoProperty(cx, obj, holder, shape, id, pc))
        return CanAttachReadSlot;

    if (canAttachGetter == CanAttachGetter::No)
        return CanAttachNone;

    if (IsCacheableGetPropCallScripted(obj, holder, shape, isTemporarilyUnoptimizable)) {
        // See bug 1226816.
        if (engine != ICStubEngine::IonSharedIC)
            return CanAttachCallGetter;
    }

    if (IsCacheableGetPropCallNative(obj, holder, shape))
        return CanAttachCallGetter;

    return CanAttachNone;
}

static void
GeneratePrototypeGuards(CacheIRWriter& writer, JSObject* obj, JSObject* holder, ObjOperandId objId)
{
    // The guards here protect against the effects of JSObject::swap(). If the
    // prototype chain is directly altered, then TI will toss the jitcode, so we
    // don't have to worry about it, and any other change to the holder, or
    // adding a shadowing property will result in reshaping the holder, and thus
    // the failure of the shape guard.
    MOZ_ASSERT(obj != holder);

    if (obj->hasUncacheableProto()) {
        // If the shape does not imply the proto, emit an explicit proto guard.
        writer.guardProto(objId, obj->staticPrototype());
    }

    JSObject* pobj = obj->staticPrototype();
    if (!pobj)
        return;

    while (pobj != holder) {
        if (pobj->hasUncacheableProto()) {
            ObjOperandId protoId = writer.loadObject(pobj);
            if (pobj->isSingleton()) {
                // Singletons can have their group's |proto| mutated directly.
                writer.guardProto(protoId, pobj->staticPrototype());
            } else {
                writer.guardGroup(protoId, pobj->group());
            }
        }
        pobj = pobj->staticPrototype();
    }
}

static void
TestMatchingReceiver(CacheIRWriter& writer, JSObject* obj, Shape* shape, ObjOperandId objId,
                     Maybe<ObjOperandId>* expandoId)
{
    if (obj->is<UnboxedPlainObject>()) {
        writer.guardGroup(objId, obj->group());

        if (UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando()) {
            expandoId->emplace(writer.guardAndLoadUnboxedExpando(objId));
            writer.guardShape(expandoId->ref(), expando->lastProperty());
        } else {
            writer.guardNoUnboxedExpando(objId);
        }
    } else if (obj->is<UnboxedArrayObject>() || obj->is<TypedObject>()) {
        writer.guardGroup(objId, obj->group());
    } else {
        Shape* shape = obj->maybeShape();
        MOZ_ASSERT(shape);
        writer.guardShape(objId, shape);
    }
}

static void
EmitReadSlotResult(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                   Shape* shape, ObjOperandId objId)
{
    Maybe<ObjOperandId> expandoId;
    TestMatchingReceiver(writer, obj, shape, objId, &expandoId);

    ObjOperandId holderId;
    if (obj != holder) {
        GeneratePrototypeGuards(writer, obj, holder, objId);

        if (holder) {
            // Guard on the holder's shape.
            holderId = writer.loadObject(holder);
            writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());
        } else {
            // The property does not exist. Guard on everything in the prototype
            // chain. This is guaranteed to see only Native objects because of
            // CanAttachNativeGetProp().
            JSObject* proto = obj->taggedProto().toObjectOrNull();
            ObjOperandId lastObjId = objId;
            while (proto) {
                ObjOperandId protoId = writer.loadProto(lastObjId);
                writer.guardShape(protoId, proto->as<NativeObject>().lastProperty());
                proto = proto->staticPrototype();
                lastObjId = protoId;
            }
        }
    } else if (obj->is<UnboxedPlainObject>()) {
        holder = obj->as<UnboxedPlainObject>().maybeExpando();
        holderId = *expandoId;
    } else {
        holderId = objId;
    }

    // Slot access.
    if (holder) {
        MOZ_ASSERT(holderId.valid());
        EmitLoadSlotResult(writer, holderId, &holder->as<NativeObject>(), shape);
    } else {
        MOZ_ASSERT(!holderId.valid());
        writer.loadUndefinedResult();
    }
}

static void
EmitReadSlotReturn(CacheIRWriter& writer, JSObject*, JSObject* holder, Shape* shape)
{
    // Slot access.
    if (holder) {
        MOZ_ASSERT(shape);
        writer.typeMonitorResult();
    } else {
        // Normally for this op, the result would have to be monitored by TI.
        // However, since this stub ALWAYS returns UndefinedValue(), and we can be sure
        // that undefined is already registered with the type-set, this can be avoided.
        writer.returnFromIC();
    }
}

static void
EmitCallGetterResultNoGuards(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                             Shape* shape, ObjOperandId objId)
{
    if (IsCacheableGetPropCallNative(obj, holder, shape)) {
        JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
        MOZ_ASSERT(target->isNative());
        writer.callNativeGetterResult(objId, target);
        writer.typeMonitorResult();
        return;
    }

    MOZ_ASSERT(IsCacheableGetPropCallScripted(obj, holder, shape));

    JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
    MOZ_ASSERT(target->hasJITCode());
    writer.callScriptedGetterResult(objId, target);
    writer.typeMonitorResult();
}

static void
EmitCallGetterResult(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                     Shape* shape, ObjOperandId objId)
{
    Maybe<ObjOperandId> expandoId;
    TestMatchingReceiver(writer, obj, shape, objId, &expandoId);

    if (obj != holder) {
        GeneratePrototypeGuards(writer, obj, holder, objId);

        // Guard on the holder's shape.
        ObjOperandId holderId = writer.loadObject(holder);
        writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());
    }

    EmitCallGetterResultNoGuards(writer, obj, holder, shape, objId);
}

bool
GetPropIRGenerator::tryAttachNative(HandleObject obj, ObjOperandId objId, HandleId id)
{
    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);

    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_,
                                                            engine_, canAttachGetter_,
                                                            isTemporarilyUnoptimizable_);
    MOZ_ASSERT_IF(idempotent(),
                  type == CanAttachNone || (type == CanAttachReadSlot && holder));
    switch (type) {
      case CanAttachNone:
        return false;
      case CanAttachReadSlot:
        maybeEmitIdGuard(id);
        if (holder) {
            EnsureTrackPropertyTypes(cx_, holder, id);
            if (obj == holder) {
                // See the comment in StripPreliminaryObjectStubs.
                if (IsPreliminaryObject(obj))
                    preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
                else
                    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
            }
        }
        EmitReadSlotResult(writer, obj, holder, shape, objId);
        EmitReadSlotReturn(writer, obj, holder, shape);
        return true;
      case CanAttachCallGetter:
        maybeEmitIdGuard(id);
        EmitCallGetterResult(writer, obj, holder, shape, objId);
        return true;
    }

    MOZ_CRASH("Bad NativeGetPropCacheability");
}

bool
GetPropIRGenerator::tryAttachWindowProxy(HandleObject obj, ObjOperandId objId, HandleId id)
{
    // Attach a stub when the receiver is a WindowProxy and we can do the lookup
    // on the Window (the global object).

    if (!IsWindowProxy(obj))
        return false;

    // This must be a WindowProxy for the current Window/global. Else it would
    // be a cross-compartment wrapper and IsWindowProxy returns false for
    // those.
    MOZ_ASSERT(obj->getClass() == cx_->runtime()->maybeWindowProxyClass());
    MOZ_ASSERT(ToWindowIfWindowProxy(obj) == cx_->global());

    // Now try to do the lookup on the Window (the current global).
    HandleObject windowObj = cx_->global();
    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);
    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, windowObj, id, &holder, &shape, pc_,
                                                            engine_, canAttachGetter_,
                                                            isTemporarilyUnoptimizable_);
    switch (type) {
      case CanAttachNone:
        return false;

      case CanAttachReadSlot: {
        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::WindowProxy);

        ObjOperandId windowObjId = writer.loadObject(windowObj);
        EmitReadSlotResult(writer, windowObj, holder, shape, windowObjId);
        EmitReadSlotReturn(writer, windowObj, holder, shape);
        return true;
      }

      case CanAttachCallGetter: {
        if (!IsCacheableGetPropCallNative(windowObj, holder, shape))
            return false;

        // Make sure the native getter is okay with the IC passing the Window
        // instead of the WindowProxy as |this| value.
        JSFunction* callee = &shape->getterObject()->as<JSFunction>();
        MOZ_ASSERT(callee->isNative());
        if (!callee->jitInfo() || callee->jitInfo()->needsOuterizedThisObject())
            return false;

        // Guard the incoming object is a WindowProxy and inline a getter call based
        // on the Window object.
        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::WindowProxy);
        ObjOperandId windowObjId = writer.loadObject(windowObj);
        EmitCallGetterResult(writer, windowObj, holder, shape, windowObjId);
        return true;
      }
    }

    MOZ_CRASH("Unreachable");
}

bool
GetPropIRGenerator::tryAttachGenericProxy(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(obj->is<ProxyObject>());

    writer.guardIsProxy(objId);

    // Ensure that the incoming object is not a DOM proxy, so that we can get to
    // the specialized stubs
    writer.guardNotDOMProxy(objId);

    if (cacheKind_ == CacheKind::GetProp) {
        writer.callProxyGetResult(objId, id);
    } else {
        // We could call maybeEmitIdGuard here and then emit CallProxyGetResult,
        // but for GetElem we prefer to attach a stub that can handle any Value
        // so we don't attach a new stub for every id.
        MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
        writer.callProxyGetByValueResult(objId, getElemKeyValueId());
    }

    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedValue expandoVal(cx_, GetProxyExtra(obj, GetDOMProxyExpandoSlot()));
    RootedObject expandoObj(cx_);
    ExpandoAndGeneration* expandoAndGeneration = nullptr;
    if (expandoVal.isObject()) {
        expandoObj = &expandoVal.toObject();
    } else {
        MOZ_ASSERT(!expandoVal.isUndefined(),
                   "How did a missing expando manage to shadow things?");
        expandoAndGeneration = static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
        MOZ_ASSERT(expandoAndGeneration);
        expandoObj = &expandoAndGeneration->expando.toObject();
    }

    // Try to do the lookup on the expando object.
    RootedNativeObject holder(cx_);
    RootedShape propShape(cx_);
    NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx_, expandoObj, id, &holder, &propShape, pc_, engine_,
                               canAttachGetter_, isTemporarilyUnoptimizable_);
    if (canCache != CanAttachReadSlot && canCache != CanAttachCallGetter)
        return false;
    if (!holder)
        return false;

    MOZ_ASSERT(holder == expandoObj);

    maybeEmitIdGuard(id);
    writer.guardShape(objId, obj->maybeShape());

    // Shape determines Class, so now it must be a DOM proxy.
    ValOperandId expandoValId;
    if (expandoVal.isObject()) {
        expandoValId = writer.loadDOMExpandoValue(objId);
    } else {
        MOZ_ASSERT(expandoAndGeneration);
        expandoValId = writer.loadDOMExpandoValueIgnoreGeneration(objId);
    }

    // Guard the expando is an object and shape guard.
    ObjOperandId expandoObjId = writer.guardIsObject(expandoValId);
    writer.guardShape(expandoObjId, expandoObj->as<NativeObject>().shape());

    if (canCache == CanAttachReadSlot) {
        // Load from the expando's slots.
        EmitLoadSlotResult(writer, expandoObjId, &expandoObj->as<NativeObject>(), propShape);
        writer.typeMonitorResult();
    } else {
        // Call the getter. Note that we pass objId, the DOM proxy, as |this|
        // and not the expando object.
        MOZ_ASSERT(canCache == CanAttachCallGetter);
        EmitCallGetterResultNoGuards(writer, expandoObj, expandoObj, propShape, objId);
    }
    return true;
}

bool
GetPropIRGenerator::tryAttachDOMProxyShadowed(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    maybeEmitIdGuard(id);
    writer.guardShape(objId, obj->maybeShape());

    // No need for more guards: we know this is a DOM proxy, since the shape
    // guard enforces a given JSClass, so just go ahead and emit the call to
    // ProxyGet.
    writer.callProxyGetResult(objId, id);
    writer.typeMonitorResult();
    return true;
}

// Callers are expected to have already guarded on the shape of the
// object, which guarantees the object is a DOM proxy.
static void
CheckDOMProxyExpandoDoesNotShadow(CacheIRWriter& writer, JSObject* obj, jsid id,
                                  ObjOperandId objId)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    Value expandoVal = GetProxyExtra(obj, GetDOMProxyExpandoSlot());

    ValOperandId expandoId;
    if (!expandoVal.isObject() && !expandoVal.isUndefined()) {
        auto expandoAndGeneration = static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
        expandoId = writer.loadDOMExpandoValueGuardGeneration(objId, expandoAndGeneration);
        expandoVal = expandoAndGeneration->expando;
    } else {
        expandoId = writer.loadDOMExpandoValue(objId);
    }

    if (expandoVal.isUndefined()) {
        // Guard there's no expando object.
        writer.guardType(expandoId, JSVAL_TYPE_UNDEFINED);
    } else if (expandoVal.isObject()) {
        // Guard the proxy either has no expando object or, if it has one, that
        // the shape matches the current expando object.
        NativeObject& expandoObj = expandoVal.toObject().as<NativeObject>();
        MOZ_ASSERT(!expandoObj.containsPure(id));
        writer.guardDOMExpandoMissingOrGuardShape(expandoId, expandoObj.lastProperty());
    } else {
        MOZ_CRASH("Invalid expando value");
    }
}

bool
GetPropIRGenerator::tryAttachDOMProxyUnshadowed(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedObject checkObj(cx_, obj->staticPrototype());
    if (!checkObj)
        return false;

    RootedNativeObject holder(cx_);
    RootedShape shape(cx_);
    NativeGetPropCacheability canCache = CanAttachNativeGetProp(cx_, checkObj, id, &holder, &shape,
                                                                pc_, engine_, canAttachGetter_,
                                                                isTemporarilyUnoptimizable_);
    MOZ_ASSERT_IF(idempotent(),
                  canCache == CanAttachNone || (canCache == CanAttachReadSlot && holder));
    if (canCache == CanAttachNone)
        return false;

    maybeEmitIdGuard(id);
    writer.guardShape(objId, obj->maybeShape());

    // Guard that our expando object hasn't started shadowing this property.
    CheckDOMProxyExpandoDoesNotShadow(writer, obj, id, objId);

    if (holder) {
        // Found the property on the prototype chain. Treat it like a native
        // getprop.
        GeneratePrototypeGuards(writer, obj, holder, objId);

        // Guard on the holder of the property.
        ObjOperandId holderId = writer.loadObject(holder);
        writer.guardShape(holderId, holder->lastProperty());

        if (canCache == CanAttachReadSlot) {
            EmitLoadSlotResult(writer, holderId, holder, shape);
            writer.typeMonitorResult();
        } else {
            // EmitCallGetterResultNoGuards expects |obj| to be the object the
            // property is on to do some checks. Since we actually looked at
            // checkObj, and no extra guards will be generated, we can just
            // pass that instead.
            MOZ_ASSERT(canCache == CanAttachCallGetter);
            EmitCallGetterResultNoGuards(writer, checkObj, holder, shape, objId);
        }
    } else {
        // Property was not found on the prototype chain. Deoptimize down to
        // proxy get call.
        writer.callProxyGetResult(objId, id);
        writer.typeMonitorResult();
    }

    return true;
}

bool
GetPropIRGenerator::tryAttachProxy(HandleObject obj, ObjOperandId objId, HandleId id)
{
    switch (GetProxyStubType(cx_, obj, id)) {
      case ProxyStubType::None:
        return false;
      case ProxyStubType::DOMExpando:
        if (tryAttachDOMProxyExpando(obj, objId, id))
            return true;
        if (*isTemporarilyUnoptimizable_) {
            // Scripted getter without JIT code. Just wait.
            return false;
        }
        MOZ_FALLTHROUGH; // Fall through to the generic shadowed case.
      case ProxyStubType::DOMShadowed:
        return tryAttachDOMProxyShadowed(obj, objId, id);
      case ProxyStubType::DOMUnshadowed:
        return tryAttachDOMProxyUnshadowed(obj, objId, id);
      case ProxyStubType::Generic:
        return tryAttachGenericProxy(obj, objId, id);
    }

    MOZ_CRASH("Unexpected ProxyStubType");
}

bool
GetPropIRGenerator::tryAttachUnboxed(HandleObject obj, ObjOperandId objId, HandleId id)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(id);
    if (!property)
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint)
        return false;

    maybeEmitIdGuard(id);
    writer.guardGroup(objId, obj->group());
    writer.loadUnboxedPropertyResult(objId, property->type,
                                     UnboxedPlainObject::offsetOfData() + property->offset);
    if (property->type == JSVAL_TYPE_OBJECT)
        writer.typeMonitorResult();
    else
        writer.returnFromIC();

    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
    return true;
}

bool
GetPropIRGenerator::tryAttachUnboxedExpando(HandleObject obj, ObjOperandId objId, HandleId id)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
    if (!expando)
        return false;

    Shape* shape = expando->lookup(cx_, id);
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot())
        return false;

    maybeEmitIdGuard(id);
    EmitReadSlotResult(writer, obj, obj, shape, objId);
    EmitReadSlotReturn(writer, obj, obj, shape);
    return true;
}

bool
GetPropIRGenerator::tryAttachTypedObject(HandleObject obj, ObjOperandId objId, HandleId id)
{
    if (!obj->is<TypedObject>() ||
        !cx_->runtime()->jitSupportsFloatingPoint ||
        cx_->compartment()->detachedTypedObjects)
    {
        return false;
    }

    TypedObject* typedObj = &obj->as<TypedObject>();
    if (!typedObj->typeDescr().is<StructTypeDescr>())
        return false;

    StructTypeDescr* structDescr = &typedObj->typeDescr().as<StructTypeDescr>();
    size_t fieldIndex;
    if (!structDescr->fieldIndex(id, &fieldIndex))
        return false;

    TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
    if (!fieldDescr->is<SimpleTypeDescr>())
        return false;

    Shape* shape = typedObj->maybeShape();
    TypedThingLayout layout = GetTypedThingLayout(shape->getObjectClass());

    uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
    uint32_t typeDescr = SimpleTypeDescrKey(&fieldDescr->as<SimpleTypeDescr>());

    maybeEmitIdGuard(id);
    writer.guardNoDetachedTypedObjects();
    writer.guardShape(objId, shape);
    writer.loadTypedObjectResult(objId, fieldOffset, layout, typeDescr);

    // Only monitor the result if the type produced by this stub might vary.
    bool monitorLoad = false;
    if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
        Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);
        monitorLoad = type == Scalar::Uint32;
    } else {
        ReferenceTypeDescr::Type type = ReferenceTypeFromSimpleTypeDescrKey(typeDescr);
        monitorLoad = type != ReferenceTypeDescr::TYPE_STRING;
    }

    if (monitorLoad)
        writer.typeMonitorResult();
    else
        writer.returnFromIC();

    return true;
}

bool
GetPropIRGenerator::tryAttachObjectLength(HandleObject obj, ObjOperandId objId, HandleId id)
{
    if (!JSID_IS_ATOM(id, cx_->names().length))
        return false;

    if (obj->is<ArrayObject>()) {
        // Make sure int32 is added to the TypeSet before we attach a stub, so
        // the stub can return int32 values without monitoring the result.
        if (obj->as<ArrayObject>().length() > INT32_MAX)
            return false;

        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::Array);
        writer.loadInt32ArrayLengthResult(objId);
        writer.returnFromIC();
        return true;
    }

    if (obj->is<UnboxedArrayObject>()) {
        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::UnboxedArray);
        writer.loadUnboxedArrayLengthResult(objId);
        writer.returnFromIC();
        return true;
    }

    if (obj->is<ArgumentsObject>() && !obj->as<ArgumentsObject>().hasOverriddenLength()) {
        maybeEmitIdGuard(id);
        if (obj->is<MappedArgumentsObject>()) {
            writer.guardClass(objId, GuardClassKind::MappedArguments);
        } else {
            MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
            writer.guardClass(objId, GuardClassKind::UnmappedArguments);
        }
        writer.loadArgumentsObjectLengthResult(objId);
        writer.returnFromIC();
        return true;
    }

    return false;
}

bool
GetPropIRGenerator::tryAttachFunction(HandleObject obj, ObjOperandId objId, HandleId id)
{
    // Function properties are lazily resolved so they might not be defined yet.
    // And we might end up in a situation where we always have a fresh function
    // object during the IC generation.
    if (!obj->is<JSFunction>())
        return false;

    JSObject* holder = nullptr;
    PropertyResult prop;
    // This property exists already, don't attach the stub.
    if (LookupPropertyPure(cx_, obj, id, &holder, &prop))
        return false;

    JSFunction* fun = &obj->as<JSFunction>();

    if (JSID_IS_ATOM(id, cx_->names().length)) {
        // length was probably deleted from the function.
        if (fun->hasResolvedLength())
            return false;

        // Lazy functions don't store the length.
        if (fun->isInterpretedLazy())
            return false;

        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::JSFunction);
        writer.loadFunctionLengthResult(objId);
        writer.returnFromIC();
        return true;
    }

    return false;
}

bool
GetPropIRGenerator::tryAttachModuleNamespace(HandleObject obj, ObjOperandId objId, HandleId id)
{
    if (!obj->is<ModuleNamespaceObject>())
        return false;

    Rooted<ModuleNamespaceObject*> ns(cx_, &obj->as<ModuleNamespaceObject>());
    RootedModuleEnvironmentObject env(cx_);
    RootedShape shape(cx_);
    if (!ns->bindings().lookup(id, env.address(), shape.address()))
        return false;

    // Don't emit a stub until the target binding has been initialized.
    if (env->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL))
        return false;

    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, env, shape->propid());

    // Check for the specific namespace object.
    maybeEmitIdGuard(id);
    writer.guardSpecificObject(objId, ns);

    ObjOperandId envId = writer.loadObject(env);
    EmitLoadSlotResult(writer, envId, env, shape);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachPrimitive(ValOperandId valId, HandleId id)
{
    JSValueType primitiveType;
    RootedNativeObject proto(cx_);
    if (val_.isString()) {
        if (JSID_IS_ATOM(id, cx_->names().length)) {
            // String length is special-cased, see js::GetProperty.
            return false;
        }
        primitiveType = JSVAL_TYPE_STRING;
        proto = MaybeNativeObject(GetBuiltinPrototypePure(cx_->global(), JSProto_String));
    } else if (val_.isNumber()) {
        primitiveType = JSVAL_TYPE_DOUBLE;
        proto = MaybeNativeObject(GetBuiltinPrototypePure(cx_->global(), JSProto_Number));
    } else if (val_.isBoolean()) {
        primitiveType = JSVAL_TYPE_BOOLEAN;
        proto = MaybeNativeObject(GetBuiltinPrototypePure(cx_->global(), JSProto_Boolean));
    } else if (val_.isSymbol()) {
        primitiveType = JSVAL_TYPE_SYMBOL;
        proto = MaybeNativeObject(GetBuiltinPrototypePure(cx_->global(), JSProto_Symbol));
    } else {
        MOZ_ASSERT(val_.isNullOrUndefined() || val_.isMagic());
        return false;
    }
    if (!proto)
        return false;

    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);
    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, proto, id, &holder, &shape, pc_,
                                                            engine_, canAttachGetter_,
                                                            isTemporarilyUnoptimizable_);
    if (type != CanAttachReadSlot)
        return false;

    if (holder) {
        // Instantiate this property, for use during Ion compilation.
        if (IsIonEnabled(cx_))
            EnsureTrackPropertyTypes(cx_, holder, id);
    }

    writer.guardType(valId, primitiveType);
    maybeEmitIdGuard(id);

    ObjOperandId protoId = writer.loadObject(proto);
    EmitReadSlotResult(writer, proto, holder, shape, protoId);
    EmitReadSlotReturn(writer, proto, holder, shape);
    return true;
}

bool
GetPropIRGenerator::tryAttachStringLength(ValOperandId valId, HandleId id)
{
    if (!val_.isString() || !JSID_IS_ATOM(id, cx_->names().length))
        return false;

    StringOperandId strId = writer.guardIsString(valId);
    maybeEmitIdGuard(id);
    writer.loadStringLengthResult(strId);
    writer.returnFromIC();
    return true;
}

bool
GetPropIRGenerator::tryAttachStringChar(ValOperandId valId, ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!val_.isString())
        return false;

    JSString* str = val_.toString();
    int32_t index = idVal_.toInt32();
    if (size_t(index) >= str->length() ||
        !str->isLinear() ||
        str->asLinear().latin1OrTwoByteChar(index) >= StaticStrings::UNIT_STATIC_LIMIT)
    {
        return false;
    }

    StringOperandId strId = writer.guardIsString(valId);
    Int32OperandId int32IndexId = writer.guardIsInt32Index(indexId);
    writer.loadStringCharResult(strId, int32IndexId);
    writer.returnFromIC();
    return true;
}

bool
GetPropIRGenerator::tryAttachMagicArgumentsName(ValOperandId valId, HandleId id)
{
    if (!val_.isMagic(JS_OPTIMIZED_ARGUMENTS))
        return false;

    if (!JSID_IS_ATOM(id, cx_->names().length) && !JSID_IS_ATOM(id, cx_->names().callee))
        return false;

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

    return true;
}

bool
GetPropIRGenerator::tryAttachMagicArgument(ValOperandId valId, ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!val_.isMagic(JS_OPTIMIZED_ARGUMENTS))
        return false;

    writer.guardMagicValue(valId, JS_OPTIMIZED_ARGUMENTS);
    writer.guardFrameHasNoArgumentsObject();

    Int32OperandId int32IndexId = writer.guardIsInt32Index(indexId);
    writer.loadFrameArgumentResult(int32IndexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachArgumentsObjectArg(HandleObject obj, ObjOperandId objId,
                                                uint32_t index, Int32OperandId indexId)
{
    if (!obj->is<ArgumentsObject>() || obj->as<ArgumentsObject>().hasOverriddenElement())
        return false;

    if (obj->is<MappedArgumentsObject>()) {
        writer.guardClass(objId, GuardClassKind::MappedArguments);
    } else {
        MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
        writer.guardClass(objId, GuardClassKind::UnmappedArguments);
    }

    writer.loadArgumentsObjectArgResult(objId, indexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachDenseElement(HandleObject obj, ObjOperandId objId,
                                          uint32_t index, Int32OperandId indexId)
{
    if (!obj->isNative())
        return false;

    if (!obj->as<NativeObject>().containsDenseElement(index))
        return false;

    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());
    writer.loadDenseElementResult(objId, indexId);
    writer.typeMonitorResult();
    return true;
}

static bool
CanAttachDenseElementHole(JSObject* obj)
{
    // Make sure the objects on the prototype don't have any indexed properties
    // or that such properties can't appear without a shape change.
    // Otherwise returning undefined for holes would obviously be incorrect,
    // because we would have to lookup a property on the prototype instead.
    do {
        // The first two checks are also relevant to the receiver object.
        if (obj->isIndexed())
            return false;

        if (ClassCanHaveExtraProperties(obj->getClass()))
            return false;

        JSObject* proto = obj->staticPrototype();
        if (!proto)
            break;

        if (!proto->isNative())
            return false;

        // Make sure objects on the prototype don't have dense elements.
        if (proto->as<NativeObject>().getDenseInitializedLength() != 0)
            return false;

        obj = proto;
    } while (true);

    return true;
}

bool
GetPropIRGenerator::tryAttachDenseElementHole(HandleObject obj, ObjOperandId objId,
                                              uint32_t index, Int32OperandId indexId)
{
    if (!obj->isNative())
        return false;

    if (obj->as<NativeObject>().containsDenseElement(index))
        return false;

    if (!CanAttachDenseElementHole(obj))
        return false;

    // Guard on the shape, to prevent non-dense elements from appearing.
    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());

    if (obj->hasUncacheableProto()) {
        // If the shape does not imply the proto, emit an explicit proto guard.
        writer.guardProto(objId, obj->staticPrototype());
    }

    JSObject* pobj = obj->staticPrototype();
    while (pobj) {
        ObjOperandId protoId = writer.loadObject(pobj);

        // Non-singletons with uncacheable protos can change their proto
        // without a shape change, so also guard on the group (which determines
        // the proto) in this case.
        if (pobj->hasUncacheableProto() && !pobj->isSingleton())
            writer.guardGroup(protoId, pobj->group());

        // Make sure the shape matches, to avoid non-dense elements or anything
        // else that is being checked by CanAttachDenseElementHole.
        writer.guardShape(protoId, pobj->as<NativeObject>().lastProperty());

        // Also make sure there are no dense elements.
        writer.guardNoDenseElements(protoId);

        pobj = pobj->staticPrototype();
    }

    writer.loadDenseElementHoleResult(objId, indexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachUnboxedArrayElement(HandleObject obj, ObjOperandId objId,
                                                 uint32_t index, Int32OperandId indexId)
{
    if (!obj->is<UnboxedArrayObject>())
        return false;

    if (index >= obj->as<UnboxedArrayObject>().initializedLength())
        return false;

    writer.guardGroup(objId, obj->group());

    JSValueType elementType = obj->group()->unboxedLayoutDontCheckGeneration().elementType();
    writer.loadUnboxedArrayElementResult(objId, indexId, elementType);

    // Only monitor the result if its type might change.
    if (elementType == JSVAL_TYPE_OBJECT)
        writer.typeMonitorResult();
    else
        writer.returnFromIC();

    return true;
}

bool
GetPropIRGenerator::tryAttachTypedElement(HandleObject obj, ObjOperandId objId,
                                          uint32_t index, Int32OperandId indexId)
{
    if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj))
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint && TypedThingRequiresFloatingPoint(obj))
        return false;

    // Ensure the index is in-bounds so the element type gets monitored.
    if (obj->is<TypedArrayObject>() && index >= obj->as<TypedArrayObject>().length())
        return false;

    // Don't attach typed object stubs if the underlying storage could be
    // detached, as the stub will always bail out.
    if (IsPrimitiveArrayTypedObject(obj) && cx_->compartment()->detachedTypedObjects)
        return false;

    TypedThingLayout layout = GetTypedThingLayout(obj->getClass());
    if (layout != Layout_TypedArray)
        writer.guardNoDetachedTypedObjects();

    writer.guardShape(objId, obj->as<ShapedObject>().shape());

    writer.loadTypedElementResult(objId, indexId, layout, TypedThingElementType(obj));

    // Reading from Uint32Array may produce an int32 now but a double value
    // later, so ensure we monitor the result.
    if (TypedThingElementType(obj) == Scalar::Type::Uint32)
        writer.typeMonitorResult();
    else
        writer.returnFromIC();
    return true;
}

bool
GetPropIRGenerator::tryAttachProxyElement(HandleObject obj, ObjOperandId objId)
{
    if (!obj->is<ProxyObject>())
        return false;

    writer.guardIsProxy(objId);

    // We are not guarding against DOM proxies here, because there is no other
    // specialized DOM IC we could attach.
    // We could call maybeEmitIdGuard here and then emit CallProxyGetResult,
    // but for GetElem we prefer to attach a stub that can handle any Value
    // so we don't attach a new stub for every id.
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
    writer.callProxyGetByValueResult(objId, getElemKeyValueId());
    writer.typeMonitorResult();
    return true;
}

void
IRGenerator::emitIdGuard(ValOperandId valId, jsid id)
{
    if (JSID_IS_SYMBOL(id)) {
        SymbolOperandId symId = writer.guardIsSymbol(valId);
        writer.guardSpecificSymbol(symId, JSID_TO_SYMBOL(id));
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id));
        StringOperandId strId = writer.guardIsString(valId);
        writer.guardSpecificAtom(strId, JSID_TO_ATOM(id));
    }
}

void
GetPropIRGenerator::maybeEmitIdGuard(jsid id)
{
    if (cacheKind_ == CacheKind::GetProp) {
        // Constant PropertyName, no guards necessary.
        MOZ_ASSERT(&idVal_.toString()->asAtom() == JSID_TO_ATOM(id));
        return;
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
    emitIdGuard(getElemKeyValueId(), id);
}

void
SetPropIRGenerator::maybeEmitIdGuard(jsid id)
{
    if (cacheKind_ == CacheKind::SetProp) {
        // Constant PropertyName, no guards necessary.
        MOZ_ASSERT(&idVal_.toString()->asAtom() == JSID_TO_ATOM(id));
        return;
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    emitIdGuard(setElemKeyValueId(), id);
}

GetNameIRGenerator::GetNameIRGenerator(JSContext* cx, jsbytecode* pc, HandleScript script,
                                       HandleObject env, HandlePropertyName name)
  : IRGenerator(cx, pc, CacheKind::GetName),
    script_(script),
    env_(env),
    name_(name)
{}

bool
GetNameIRGenerator::tryAttachStub()
{
    MOZ_ASSERT(cacheKind_ == CacheKind::GetName);

    AutoAssertNoPendingException aanpe(cx_);

    ObjOperandId envId(writer.setInputOperandId(0));
    RootedId id(cx_, NameToId(name_));

    if (tryAttachGlobalNameValue(envId, id))
        return true;
    if (tryAttachGlobalNameGetter(envId, id))
        return true;
    if (tryAttachEnvironmentName(envId, id))
        return true;

    return false;
}

bool
CanAttachGlobalName(JSContext* cx, Handle<LexicalEnvironmentObject*> globalLexical, HandleId id,
                    MutableHandleNativeObject holder, MutableHandleShape shape)
{
    // The property must be found, and it must be found as a normal data property.
    RootedNativeObject current(cx, globalLexical);
    while (true) {
        shape.set(current->lookup(cx, id));
        if (shape)
            break;

        if (current == globalLexical) {
            current = &globalLexical->global();
        } else {
            // In the browser the global prototype chain should be immutable.
            if (!current->staticPrototypeIsImmutable())
                return false;

            JSObject* proto = current->staticPrototype();
            if (!proto || !proto->is<NativeObject>())
                return false;

            current = &proto->as<NativeObject>();
        }
    }

    holder.set(current);
    return true;
}

bool
GetNameIRGenerator::tryAttachGlobalNameValue(ObjOperandId objId, HandleId id)
{
    if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope())
        return false;

    Handle<LexicalEnvironmentObject*> globalLexical = env_.as<LexicalEnvironmentObject>();
    MOZ_ASSERT(globalLexical->isGlobal());

    RootedNativeObject holder(cx_);
    RootedShape shape(cx_);
    if (!CanAttachGlobalName(cx_, globalLexical, id, &holder, &shape))
        return false;

    // The property must be found, and it must be found as a normal data property.
    if (!shape->hasDefaultGetter() || !shape->hasSlot())
        return false;

    // This might still be an uninitialized lexical.
    if (holder->getSlot(shape->slot()).isMagic())
        return false;

    // Instantiate this global property, for use during Ion compilation.
    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, holder, id);

    if (holder == globalLexical) {
        // There is no need to guard on the shape. Lexical bindings are
        // non-configurable, and this stub cannot be shared across globals.
        size_t dynamicSlotOffset = holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
        writer.loadDynamicSlotResult(objId, dynamicSlotOffset);
    } else {
        // Check the prototype chain from the global to the holder
        // prototype. Ignore the global lexical scope as it doesn't figure
        // into the prototype chain. We guard on the global lexical
        // scope's shape independently.
        if (!IsCacheableGetPropReadSlotForIonOrCacheIR(&globalLexical->global(), holder,
                                                       PropertyResult(shape)))
            return false;

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
    return true;
}

bool
GetNameIRGenerator::tryAttachGlobalNameGetter(ObjOperandId objId, HandleId id)
{
    if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope())
        return false;

    Handle<LexicalEnvironmentObject*> globalLexical = env_.as<LexicalEnvironmentObject>();
    MOZ_ASSERT(globalLexical->isGlobal());

    RootedNativeObject holder(cx_);
    RootedShape shape(cx_);
    if (!CanAttachGlobalName(cx_, globalLexical, id, &holder, &shape))
        return false;

    if (holder == globalLexical)
        return false;

    if (!IsCacheableGetPropCallNative(&globalLexical->global(), holder, shape))
        return false;

    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, holder, id);

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

    EmitCallGetterResultNoGuards(writer, &globalLexical->global(), holder, shape, globalId);
    return true;
}

bool
GetNameIRGenerator::tryAttachEnvironmentName(ObjOperandId objId, HandleId id)
{
    if (IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope())
        return false;

    RootedObject env(cx_, env_);
    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);

    while (env) {
        if (env->is<GlobalObject>()) {
            shape = env->as<GlobalObject>().lookup(cx_, id);
            if (shape)
                break;
            return false;
        }

        if (!env->is<EnvironmentObject>() || env->is<WithEnvironmentObject>())
            return false;

        MOZ_ASSERT(!env->hasUncacheableProto());

        // Check for an 'own' property on the env. There is no need to
        // check the prototype as non-with scopes do not inherit properties
        // from any prototype.
        shape = env->as<NativeObject>().lookup(cx_, id);
        if (shape)
            break;

        env = env->enclosingEnvironment();
    }

    holder = &env->as<NativeObject>();
    if (!IsCacheableGetPropReadSlotForIonOrCacheIR(holder, holder, PropertyResult(shape)))
        return false;
    if (holder->getSlot(shape->slot()).isMagic())
        return false;

    ObjOperandId lastObjId = objId;
    env = env_;
    while (env) {
        if (env == holder) {
            writer.guardShape(lastObjId, holder->maybeShape());
            break;
        }

        writer.guardShape(lastObjId, env->maybeShape());
        lastObjId = writer.loadEnclosingEnvironment(lastObjId);
        env = env->enclosingEnvironment();
    }

    if (holder->isFixedSlot(shape->slot())) {
        writer.loadEnvironmentFixedSlotResult(lastObjId, NativeObject::getFixedSlotOffset(shape->slot()));
    } else {
        size_t dynamicSlotOffset = holder->dynamicSlotIndex(shape->slot()) * sizeof(Value);
        writer.loadEnvironmentDynamicSlotResult(lastObjId, dynamicSlotOffset);
    }

    writer.typeMonitorResult();
    return true;
}

InIRGenerator::InIRGenerator(JSContext* cx, jsbytecode* pc, HandleValue key, HandleObject obj)
  : IRGenerator(cx, pc, CacheKind::In),
    key_(key), obj_(obj)
{ }

bool
InIRGenerator::tryAttachDenseIn(uint32_t index, Int32OperandId indexId,
                                HandleObject obj, ObjOperandId objId)
{
    if (!obj->isNative())
        return false;
    if (!obj->as<NativeObject>().containsDenseElement(index))
        return false;

    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());
    writer.loadDenseElementExistsResult(objId, indexId);
    writer.returnFromIC();
    return true;
}

bool
InIRGenerator::tryAttachStub()
{
    MOZ_ASSERT(cacheKind_ == CacheKind::In);

    AutoAssertNoPendingException aanpe(cx_);

    ValOperandId keyId(writer.setInputOperandId(0));
    ValOperandId valId(writer.setInputOperandId(1));
    ObjOperandId objId = writer.guardIsObject(valId);

    uint32_t index;
    Int32OperandId indexId;
    if (maybeGuardInt32Index(key_, keyId, &index, &indexId)) {
        if (tryAttachDenseIn(index, indexId, obj_, objId))
            return true;
        return false;
    }

    return false;
}

bool
IRGenerator::maybeGuardInt32Index(const Value& index, ValOperandId indexId,
                                  uint32_t* int32Index, Int32OperandId* int32IndexId)
{
    if (index.isNumber()) {
        int32_t indexSigned;
        if (index.isInt32()) {
            indexSigned = index.toInt32();
        } else {
            // We allow negative zero here.
            if (!mozilla::NumberEqualsInt32(index.toDouble(), &indexSigned))
                return false;
            if (!cx_->runtime()->jitSupportsFloatingPoint)
                return false;
        }

        if (indexSigned < 0)
            return false;

        *int32Index = uint32_t(indexSigned);
        *int32IndexId = writer.guardIsInt32Index(indexId);
        return true;
    }

    if (index.isString()) {
        int32_t indexSigned = GetIndexFromString(index.toString());
        if (indexSigned < 0)
            return false;

        StringOperandId strId = writer.guardIsString(indexId);
        *int32Index = uint32_t(indexSigned);
        *int32IndexId = writer.guardAndGetIndexFromString(strId);
        return true;
    }

    return false;
}

SetPropIRGenerator::SetPropIRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind,
                                       bool* isTemporarilyUnoptimizable, HandleValue lhsVal,
                                       HandleValue idVal, HandleValue rhsVal)
  : IRGenerator(cx, pc, cacheKind),
    lhsVal_(lhsVal),
    idVal_(idVal),
    rhsVal_(rhsVal),
    isTemporarilyUnoptimizable_(isTemporarilyUnoptimizable),
    preliminaryObjectAction_(PreliminaryObjectAction::None),
    updateStubGroup_(cx),
    updateStubId_(cx, JSID_EMPTY),
    needUpdateStub_(false)
{}

bool
SetPropIRGenerator::tryAttachStub()
{
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
        return false;
    }

    if (lhsVal_.isObject()) {
        RootedObject obj(cx_, &lhsVal_.toObject());
        if (obj->watched())
            return false;

        ObjOperandId objId = writer.guardIsObject(objValId);
        if (nameOrSymbol) {
            if (tryAttachNativeSetSlot(obj, objId, id, rhsValId))
                return true;
            if (tryAttachUnboxedExpandoSetSlot(obj, objId, id, rhsValId))
                return true;
            if (tryAttachUnboxedProperty(obj, objId, id, rhsValId))
                return true;
            if (tryAttachSetter(obj, objId, id, rhsValId))
                return true;
            if (tryAttachTypedObjectProperty(obj, objId, id, rhsValId))
                return true;
            if (tryAttachSetArrayLength(obj, objId, id, rhsValId))
                return true;
        }
        return false;
    }

    return false;
}

static void
EmitStoreSlotAndReturn(CacheIRWriter& writer, ObjOperandId objId, NativeObject* nobj, Shape* shape,
                       ValOperandId rhsId)
{
    if (nobj->isFixedSlot(shape->slot())) {
        size_t offset = NativeObject::getFixedSlotOffset(shape->slot());
        writer.storeFixedSlot(objId, offset, rhsId);
    } else {
        size_t offset = nobj->dynamicSlotIndex(shape->slot()) * sizeof(Value);
        writer.storeDynamicSlot(objId, offset, rhsId);
    }
    writer.returnFromIC();
}

static Shape*
LookupShapeForSetSlot(NativeObject* obj, jsid id)
{
    Shape* shape = obj->lookupPure(id);
    if (shape && shape->hasSlot() && shape->hasDefaultSetter() && shape->writable())
        return shape;
    return nullptr;
}

bool
SetPropIRGenerator::tryAttachNativeSetSlot(HandleObject obj, ObjOperandId objId, HandleId id,
                                           ValOperandId rhsId)
{
    if (!obj->isNative())
        return false;

    RootedShape propShape(cx_, LookupShapeForSetSlot(&obj->as<NativeObject>(), id));
    if (!propShape)
        return false;

    RootedObjectGroup group(cx_, JSObject::getGroup(cx_, obj));
    if (!group) {
        cx_->recoverFromOutOfMemory();
        return false;
    }

    // For some property writes, such as the initial overwrite of global
    // properties, TI will not mark the property as having been
    // overwritten. Don't attach a stub in this case, so that we don't
    // execute another write to the property without TI seeing that write.
    EnsureTrackPropertyTypes(cx_, obj, id);
    if (!PropertyHasBeenMarkedNonConstant(obj, id)) {
        *isTemporarilyUnoptimizable_ = true;
        return false;
    }

    maybeEmitIdGuard(id);

    // For Baseline, we have to guard on both the shape and group, because the
    // type update IC applies to a single group. When we port the Ion IC, we can
    // do a bit better and avoid the group guard if we don't have to guard on
    // the property types.
    NativeObject* nobj = &obj->as<NativeObject>();
    writer.guardGroup(objId, nobj->group());
    writer.guardShape(objId, nobj->lastProperty());

    if (IsPreliminaryObject(obj))
        preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
    else
        preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;

    setUpdateStubInfo(nobj->group(), id);
    EmitStoreSlotAndReturn(writer, objId, nobj, propShape, rhsId);
    return true;
}

bool
SetPropIRGenerator::tryAttachUnboxedExpandoSetSlot(HandleObject obj, ObjOperandId objId,
                                                   HandleId id, ValOperandId rhsId)
{
    if (!obj->is<UnboxedPlainObject>())
        return false;

    UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
    if (!expando)
        return false;

    Shape* propShape = LookupShapeForSetSlot(expando, id);
    if (!propShape)
        return false;

    maybeEmitIdGuard(id);
    writer.guardGroup(objId, obj->group());
    ObjOperandId expandoId = writer.guardAndLoadUnboxedExpando(objId);
    writer.guardShape(expandoId, expando->lastProperty());

    // Property types must be added to the unboxed object's group, not the
    // expando's group (it has unknown properties).
    setUpdateStubInfo(obj->group(), id);
    EmitStoreSlotAndReturn(writer, expandoId, expando, propShape, rhsId);
    return true;
}

static void
EmitGuardUnboxedPropertyType(CacheIRWriter& writer, JSValueType propType, ValOperandId valId)
{
    if (propType == JSVAL_TYPE_OBJECT) {
        // Unboxed objects store NullValue as nullptr object.
        writer.guardIsObjectOrNull(valId);
    } else {
        writer.guardType(valId, propType);
    }
}

bool
SetPropIRGenerator::tryAttachUnboxedProperty(HandleObject obj, ObjOperandId objId, HandleId id,
                                             ValOperandId rhsId)
{
    if (!obj->is<UnboxedPlainObject>() || !cx_->runtime()->jitSupportsFloatingPoint)
        return false;

    const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(id);
    if (!property)
        return false;

    maybeEmitIdGuard(id);
    writer.guardGroup(objId, obj->group());
    EmitGuardUnboxedPropertyType(writer, property->type, rhsId);
    writer.storeUnboxedProperty(objId, property->type,
                                UnboxedPlainObject::offsetOfData() + property->offset,
                                rhsId);
    writer.returnFromIC();

    setUpdateStubInfo(obj->group(), id);
    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
    return true;
}

bool
SetPropIRGenerator::tryAttachTypedObjectProperty(HandleObject obj, ObjOperandId objId, HandleId id,
                                                 ValOperandId rhsId)
{
    if (!obj->is<TypedObject>() || !cx_->runtime()->jitSupportsFloatingPoint)
        return false;

    if (cx_->compartment()->detachedTypedObjects)
        return false;

    if (!obj->as<TypedObject>().typeDescr().is<StructTypeDescr>())
        return false;

    StructTypeDescr* structDescr = &obj->as<TypedObject>().typeDescr().as<StructTypeDescr>();
    size_t fieldIndex;
    if (!structDescr->fieldIndex(id, &fieldIndex))
        return false;

    TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
    if (!fieldDescr->is<SimpleTypeDescr>())
        return false;

    uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
    TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

    maybeEmitIdGuard(id);
    writer.guardNoDetachedTypedObjects();
    writer.guardShape(objId, obj->as<TypedObject>().shape());
    writer.guardGroup(objId, obj->group());

    setUpdateStubInfo(obj->group(), id);

    // Scalar types can always be stored without a type update stub.
    if (fieldDescr->is<ScalarTypeDescr>()) {
        Scalar::Type type = fieldDescr->as<ScalarTypeDescr>().type();
        writer.storeTypedObjectScalarProperty(objId, fieldOffset, layout, type, rhsId);
        writer.returnFromIC();
        return true;
    }

    // For reference types, guard on the RHS type first, so that
    // StoreTypedObjectReferenceProperty is infallible.
    ReferenceTypeDescr::Type type = fieldDescr->as<ReferenceTypeDescr>().type();
    switch (type) {
      case ReferenceTypeDescr::TYPE_ANY:
        break;
      case ReferenceTypeDescr::TYPE_OBJECT:
        writer.guardIsObjectOrNull(rhsId);
        break;
      case ReferenceTypeDescr::TYPE_STRING:
        writer.guardType(rhsId, JSVAL_TYPE_STRING);
        break;
    }

    writer.storeTypedObjectReferenceProperty(objId, fieldOffset, layout, type, rhsId);
    writer.returnFromIC();
    return true;
}

static void
EmitCallSetterResultNoGuards(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                             Shape* shape, ObjOperandId objId, ValOperandId rhsId)
{
    if (IsCacheableSetPropCallNative(obj, holder, shape)) {
        JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
        MOZ_ASSERT(target->isNative());
        writer.callNativeSetter(objId, target, rhsId);
        writer.returnFromIC();
        return;
    }

    MOZ_ASSERT(IsCacheableSetPropCallScripted(obj, holder, shape));

    JSFunction* target = &shape->setterValue().toObject().as<JSFunction>();
    MOZ_ASSERT(target->hasJITCode());
    writer.callScriptedSetter(objId, target, rhsId);
    writer.returnFromIC();
}

bool
SetPropIRGenerator::tryAttachSetter(HandleObject obj, ObjOperandId objId, HandleId id,
                                    ValOperandId rhsId)
{
    // Don't attach a setter stub for ops like JSOP_INITELEM.
    if (IsPropertyInitOp(JSOp(*pc_)))
        return false;
    MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

    PropertyResult prop;
    JSObject* holder;
    if (!LookupPropertyPure(cx_, obj, id, &holder, &prop))
        return false;

    if (prop.isNonNativeProperty())
        return false;

    Shape* shape = prop.maybeShape();
    if (!IsCacheableSetPropCallScripted(obj, holder, shape, isTemporarilyUnoptimizable_) &&
        !IsCacheableSetPropCallNative(obj, holder, shape))
    {
        return false;
    }

    maybeEmitIdGuard(id);

    Maybe<ObjOperandId> expandoId;
    TestMatchingReceiver(writer, obj, shape, objId, &expandoId);

    if (obj != holder) {
        GeneratePrototypeGuards(writer, obj, holder, objId);

        // Guard on the holder's shape.
        ObjOperandId holderId = writer.loadObject(holder);
        writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());
    }

    EmitCallSetterResultNoGuards(writer, obj, holder, shape, objId, rhsId);
    return true;
}

bool
SetPropIRGenerator::tryAttachSetArrayLength(HandleObject obj, ObjOperandId objId, HandleId id,
                                            ValOperandId rhsId)
{
    if (!obj->is<ArrayObject>() || !JSID_IS_ATOM(id, cx_->names().length))
        return false;

    maybeEmitIdGuard(id);
    writer.guardClass(objId, GuardClassKind::Array);
    writer.callSetArrayLength(objId, IsStrictSetPC(pc_), rhsId);
    writer.returnFromIC();
    return true;
}

bool
SetPropIRGenerator::tryAttachAddSlotStub(HandleObjectGroup oldGroup, HandleShape oldShape)
{
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
        return false;
    }

    if (!lhsVal_.isObject() || !nameOrSymbol)
        return false;

    RootedObject obj(cx_, &lhsVal_.toObject());
    if (obj->watched())
        return false;

    PropertyResult prop;
    JSObject* holder;
    if (!LookupPropertyPure(cx_, obj, id, &holder, &prop))
        return false;
    if (obj != holder)
        return false;

    Shape* propShape = nullptr;
    NativeObject* holderOrExpando = nullptr;

    if (obj->isNative()) {
        propShape = prop.shape();
        holderOrExpando = &obj->as<NativeObject>();
    } else {
        if (!obj->is<UnboxedPlainObject>())
            return false;
        UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
        if (!expando)
            return false;
        propShape = expando->lookupPure(id);
        if (!propShape)
            return false;
        holderOrExpando = expando;
    }

    MOZ_ASSERT(propShape);

    // The property must be the last added property of the object.
    if (holderOrExpando->lastProperty() != propShape)
        return false;

    // Object must be extensible, oldShape must be immediate parent of
    // current shape.
    if (!obj->nonProxyIsExtensible() || propShape->previous() != oldShape)
        return false;

    // Basic shape checks.
    if (propShape->inDictionary() ||
        !propShape->hasSlot() ||
        !propShape->hasDefaultSetter() ||
        !propShape->writable())
    {
        return false;
    }

    // Watch out for resolve or addProperty hooks.
    if (ClassMayResolveId(cx_->names(), obj->getClass(), id, obj) ||
        obj->getClass()->getAddProperty())
    {
        return false;
    }

    // Walk up the object prototype chain and ensure that all prototypes are
    // native, and that all prototypes have no setter defined on the property.
    for (JSObject* proto = obj->staticPrototype(); proto; proto = proto->staticPrototype()) {
        if (!proto->isNative())
            return false;

        // If prototype defines this property in a non-plain way, don't optimize.
        Shape* protoShape = proto->as<NativeObject>().lookup(cx_, id);
        if (protoShape && !protoShape->hasDefaultSetter())
            return false;

        // Otherwise, if there's no such property, watch out for a resolve hook
        // that would need to be invoked and thus prevent inlining of property
        // addition.
        if (ClassMayResolveId(cx_->names(), proto->getClass(), id, proto))
            return false;
    }

    // Don't attach if we are adding a property to an object which the new
    // script properties analysis hasn't been performed for yet, as there
    // may be a shape change required here afterwards.
    if (oldGroup->newScript() && !oldGroup->newScript()->analyzed()) {
        *isTemporarilyUnoptimizable_ = true;
        return false;
    }

    ObjOperandId objId = writer.guardIsObject(objValId);
    maybeEmitIdGuard(id);

    writer.guardGroup(objId, oldGroup);

    // Shape guard the holder.
    ObjOperandId holderId = objId;
    if (!obj->isNative()) {
        MOZ_ASSERT(obj->as<UnboxedPlainObject>().maybeExpando());
        holderId = writer.guardAndLoadUnboxedExpando(objId);
    }
    writer.guardShape(holderId, oldShape);

    // Shape guard the objects on the proto chain.
    JSObject* lastObj = obj;
    ObjOperandId lastObjId = objId;
    while (true) {
        // Guard on the proto if the shape does not imply the proto. Singleton
        // objects always trigger a shape change when the proto changes, so we
        // don't need a guard in that case.
        bool guardProto = lastObj->hasUncacheableProto() && !lastObj->isSingleton();

        lastObj = lastObj->staticPrototype();
        if (!lastObj)
            break;

        lastObjId = writer.loadProto(lastObjId);
        if (guardProto)
            writer.guardSpecificObject(lastObjId, lastObj);
        writer.guardShape(lastObjId, lastObj->as<NativeObject>().shape());
    }

    ObjectGroup* newGroup = obj->group();

    // Check if we have to change the object's group. If we're adding an
    // unboxed expando property, we pass the expando object to AddAndStore*Slot.
    // That's okay because we only have to do a group change if the object is a
    // PlainObject.
    bool changeGroup = oldGroup != newGroup;
    MOZ_ASSERT_IF(changeGroup, obj->is<PlainObject>());

    if (holderOrExpando->isFixedSlot(propShape->slot())) {
        size_t offset = NativeObject::getFixedSlotOffset(propShape->slot());
        writer.addAndStoreFixedSlot(holderId, offset, rhsValId, propShape,
                                    changeGroup, newGroup);
    } else {
        size_t offset = holderOrExpando->dynamicSlotIndex(propShape->slot()) * sizeof(Value);
        uint32_t numOldSlots = NativeObject::dynamicSlotsCount(oldShape);
        uint32_t numNewSlots = NativeObject::dynamicSlotsCount(propShape);
        if (numOldSlots == numNewSlots) {
            writer.addAndStoreDynamicSlot(holderId, offset, rhsValId, propShape,
                                          changeGroup, newGroup);
        } else {
            MOZ_ASSERT(numNewSlots > numOldSlots);
            writer.allocateAndStoreDynamicSlot(holderId, offset, rhsValId, propShape,
                                               changeGroup, newGroup, numNewSlots);
        }
    }
    writer.returnFromIC();

    setUpdateStubInfo(oldGroup, id);
    return true;
}
