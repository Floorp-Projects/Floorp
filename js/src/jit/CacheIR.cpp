/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIR.h"

#include "jit/BaselineIC.h"
#include "jit/IonCaches.h"

#include "jsobjinlines.h"

#include "vm/UnboxedObject-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

GetPropIRGenerator::GetPropIRGenerator(JSContext* cx, jsbytecode* pc, ICStubEngine engine,
                                       CacheKind cacheKind,
                                       bool* isTemporarilyUnoptimizable,
                                       HandleValue val, HandleValue idVal)
  : writer(cx),
    cx_(cx),
    pc_(pc),
    val_(val),
    idVal_(idVal),
    engine_(engine),
    cacheKind_(cacheKind),
    isTemporarilyUnoptimizable_(isTemporarilyUnoptimizable),
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

bool
GetPropIRGenerator::tryAttachStub()
{
    // pc_ should only be null for idempotent ICs, and idempotent ICs should
    // call tryAttachIdempotentStub instead.
    MOZ_ASSERT(pc_);

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
            if (tryAttachProxy(obj, objId, id))
                return true;
            return false;
        }
        if (idVal_.isNumber()) {
            ValOperandId indexId = getElemKeyValueId();
            if (tryAttachTypedElement(obj, objId, indexId))
                return true;
        }
        if (idVal_.isInt32()) {
            ValOperandId indexId = getElemKeyValueId();
            if (tryAttachDenseElement(obj, objId, indexId))
                return true;
            if (tryAttachDenseElementHole(obj, objId, indexId))
                return true;
            if (tryAttachUnboxedArrayElement(obj, objId, indexId))
                return true;
            if (tryAttachArgumentsObjectArg(obj, objId, indexId))
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

    // No pc if idempotent, as there can be multiple bytecode locations
    // due to GVN.
    MOZ_ASSERT(!pc_);

    RootedObject obj(cx_, &val_.toObject());
    RootedId id(cx_, NameToId(idVal_.toString()->asAtom().asPropertyName()));

    ValOperandId valId(writer.setInputOperandId(0));
    ObjOperandId objId = writer.guardIsObject(valId);
    return tryAttachNative(obj, objId, id);
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

    // If we're doing a name lookup, we have to throw a ReferenceError.
    if (*pc == JSOP_GETXPROP)
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
                       jsbytecode* pc, ICStubEngine engine, bool* isTemporarilyUnoptimizable)
{
    MOZ_ASSERT(JSID_IS_STRING(id) || JSID_IS_SYMBOL(id));

    // The lookup needs to be universally pure, otherwise we risk calling hooks out
    // of turn. We don't mind doing this even when purity isn't required, because we
    // only miss out on shape hashification, which is only a temporary perf cost.
    // The limits were arbitrarily set, anyways.
    JSObject* baseHolder = nullptr;
    if (!LookupPropertyPure(cx, obj, id, &baseHolder, shape.address()))
        return CanAttachNone;

    MOZ_ASSERT(!holder);
    if (baseHolder) {
        if (!baseHolder->isNative())
            return CanAttachNone;
        holder.set(&baseHolder->as<NativeObject>());
    }

    if (IsCacheableGetPropReadSlotForIonOrCacheIR(obj, holder, shape) ||
        IsCacheableNoProperty(cx, obj, holder, shape, id, pc))
    {
        return CanAttachReadSlot;
    }

    if (IsCacheableGetPropCallScripted(obj, holder, shape, isTemporarilyUnoptimizable)) {
        // See bug 1226816.
        if (engine == ICStubEngine::Baseline)
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
                                                            engine_, isTemporarilyUnoptimizable_);
    if (!pc_) {
        // Idempotent ICs only support plain data properties.
        if (type != CanAttachReadSlot)
            return false;
        MOZ_ASSERT(holder);
    }

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
    // Attach a stub when the receiver is a WindowProxy and we are calling some
    // kinds of JSNative getters on the Window object (the global object).

    if (!IsWindowProxy(obj))
        return false;

    // This must be a WindowProxy for the current Window/global. Else it would
    // be a cross-compartment wrapper and IsWindowProxy returns false for
    // those.
    MOZ_ASSERT(obj->getClass() == cx_->maybeWindowProxyClass());
    MOZ_ASSERT(ToWindowIfWindowProxy(obj) == cx_->global());

    // Now try to do the lookup on the Window (the current global) and see if
    // it's a native getter.
    HandleObject windowObj = cx_->global();
    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);
    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, windowObj, id, &holder, &shape, pc_,
                                                            engine_, isTemporarilyUnoptimizable_);
    if (type != CanAttachCallGetter ||
        !IsCacheableGetPropCallNative(windowObj, holder, shape))
    {
        return false;
    }

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
        ExpandoAndGeneration* expandoAndGeneration = (ExpandoAndGeneration*)expandoVal.toPrivate();
        expandoId = writer.guardDOMExpandoGeneration(objId, expandoAndGeneration,
                                                     expandoAndGeneration->generation);
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
        writer.guardDOMExpandoObject(expandoId, expandoObj.lastProperty());
    } else {
        MOZ_CRASH("Invalid expando value");
    }
}

bool
GetPropIRGenerator::tryAttachDOMProxyUnshadowed(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedObject checkObj(cx_, obj->staticPrototype());
    RootedNativeObject holder(cx_);
    RootedShape shape(cx_);

    NativeGetPropCacheability canCache = CanAttachNativeGetProp(cx_, checkObj, id, &holder, &shape,
                                                                pc_, engine_,
                                                                isTemporarilyUnoptimizable_);
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
    if (!obj->is<ProxyObject>())
        return false;

    // Skim off DOM proxies.
    if (IsCacheableDOMProxy(obj)) {
        DOMProxyShadowsResult shadows = GetDOMProxyShadowsCheck()(cx_, obj, id);
        if (shadows == ShadowCheckFailed) {
            cx_->clearPendingException();
            return false;
        }
        if (DOMProxyIsShadowing(shadows))
            return tryAttachDOMProxyShadowed(obj, objId, id);

        MOZ_ASSERT(shadows == DoesntShadow || shadows == DoesntShadowUnique);
        return tryAttachDOMProxyUnshadowed(obj, objId, id);
    }

    return tryAttachGenericProxy(obj, objId, id);
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

    // Instantiate this property, for use during Ion compilation.
    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, proto, id);

    // For now, only look for properties directly set on the prototype.
    Shape* shape = proto->lookup(cx_, id);
    if (!shape || !shape->hasSlot() || !shape->hasDefaultGetter())
        return false;

    writer.guardType(valId, primitiveType);
    maybeEmitIdGuard(id);

    ObjOperandId protoId = writer.loadObject(proto);
    writer.guardShape(protoId, proto->lastProperty());
    EmitLoadSlotResult(writer, protoId, proto, shape);
    writer.typeMonitorResult();
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
    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
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

    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadFrameArgumentResult(int32IndexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachArgumentsObjectArg(HandleObject obj, ObjOperandId objId,
                                                ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!obj->is<ArgumentsObject>() || obj->as<ArgumentsObject>().hasOverriddenElement())
        return false;

    if (obj->is<MappedArgumentsObject>()) {
        writer.guardClass(objId, GuardClassKind::MappedArguments);
    } else {
        MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
        writer.guardClass(objId, GuardClassKind::UnmappedArguments);
    }

    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadArgumentsObjectArgResult(objId, int32IndexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachDenseElement(HandleObject obj, ObjOperandId objId,
                                          ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!obj->isNative())
        return false;

    if (!obj->as<NativeObject>().containsDenseElement(uint32_t(idVal_.toInt32())))
        return false;

    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());

    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadDenseElementResult(objId, int32IndexId);
    writer.typeMonitorResult();
    return true;
}

static bool
CanAttachDenseElementHole(JSObject* obj)
{
    // Make sure this object already has dense elements.
    if (obj->as<NativeObject>().getDenseInitializedLength() == 0)
        return false;

    // Now we have to make sure the objects on the prototype don't
    // have any int32 properties or that such properties can't appear
    // without a shape change.
    // Otherwise returning undefined for holes would obviously be incorrect,
    // because we would have to lookup a property on the prototype instead.
    do {
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
                                              ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (idVal_.toInt32() < 0)
        return false;

    if (!obj->isNative() || !CanAttachDenseElementHole(obj))
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

    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadDenseElementHoleResult(objId, int32IndexId);
    writer.typeMonitorResult();
    return true;
}

bool
GetPropIRGenerator::tryAttachUnboxedArrayElement(HandleObject obj, ObjOperandId objId,
                                                 ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!obj->is<UnboxedArrayObject>())
        return false;

    if (uint32_t(idVal_.toInt32()) >= obj->as<UnboxedArrayObject>().initializedLength())
        return false;

    writer.guardGroup(objId, obj->group());

    JSValueType elementType = obj->group()->unboxedLayoutDontCheckGeneration().elementType();
    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadUnboxedArrayElementResult(objId, int32IndexId, elementType);

    // Only monitor the result if its type might change.
    if (elementType == JSVAL_TYPE_OBJECT)
        writer.typeMonitorResult();
    else
        writer.returnFromIC();

    return true;
}

bool
GetPropIRGenerator::tryAttachTypedElement(HandleObject obj, ObjOperandId objId,
                                          ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isNumber());

    if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj))
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint &&
        (TypedThingRequiresFloatingPoint(obj) || idVal_.isDouble()))
    {
        return false;
    }

    // Don't attach typed object stubs if the underlying storage could be
    // detached, as the stub will always bail out.
    if (IsPrimitiveArrayTypedObject(obj) && cx_->compartment()->detachedTypedObjects)
        return false;

    TypedThingLayout layout = GetTypedThingLayout(obj->getClass());
    if (layout != Layout_TypedArray)
        writer.guardNoDetachedTypedObjects();

    writer.guardShape(objId, obj->as<ShapedObject>().shape());

    Int32OperandId int32IndexId = writer.guardIsInt32(indexId);
    writer.loadTypedElementResult(objId, int32IndexId, layout, TypedThingElementType(obj));
    writer.returnFromIC();
    return true;
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

    ValOperandId valId = getElemKeyValueId();
    if (JSID_IS_SYMBOL(id)) {
        SymbolOperandId symId = writer.guardIsSymbol(valId);
        writer.guardSpecificSymbol(symId, JSID_TO_SYMBOL(id));
    } else {
        MOZ_ASSERT(JSID_IS_ATOM(id));
        StringOperandId strId = writer.guardIsString(valId);
        writer.guardSpecificAtom(strId, JSID_TO_ATOM(id));
    }
}
