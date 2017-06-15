/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIR.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"

#include "jit/BaselineIC.h"
#include "jit/CacheIRSpewer.h"
#include "jit/IonCaches.h"

#include "vm/SelfHosting.h"
#include "jsobjinlines.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/UnboxedObject-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::Maybe;

const char* js::jit::CacheKindNames[] = {
#define DEFINE_KIND(kind) #kind,
    CACHE_IR_KINDS(DEFINE_KIND)
#undef DEFINE_KIND
};


IRGenerator::IRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc, CacheKind cacheKind,
                         ICState::Mode mode)
  : writer(cx),
    cx_(cx),
    script_(script),
    pc_(pc),
    cacheKind_(cacheKind),
    mode_(mode)
{}

GetPropIRGenerator::GetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                       CacheKind cacheKind, ICState::Mode mode,
                                       bool* isTemporarilyUnoptimizable, HandleValue val,
                                       HandleValue idVal, CanAttachGetter canAttachGetter)
  : IRGenerator(cx, script, pc, cacheKind, mode),
    val_(val),
    idVal_(idVal),
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
            if (tryAttachCrossCompartmentWrapper(obj, objId, id))
                return true;
            if (tryAttachFunction(obj, objId, id))
                return true;
            if (tryAttachProxy(obj, objId, id))
                return true;

            trackNotAttached();
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

            trackNotAttached();
            return false;
        }

        trackNotAttached();
        return false;
    }

    if (nameOrSymbol) {
        if (tryAttachPrimitive(valId, id))
            return true;
        if (tryAttachStringLength(valId, id))
            return true;
        if (tryAttachMagicArgumentsName(valId, id))
            return true;

        trackNotAttached();
        return false;
    }

    if (idVal_.isInt32()) {
        ValOperandId indexId = getElemKeyValueId();
        if (tryAttachStringChar(valId, indexId))
            return true;
        if (tryAttachMagicArgument(valId, indexId))
            return true;

        trackNotAttached();
        return false;
    }

    trackNotAttached();
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
    if (*pc == JSOP_GETBOUNDNAME || cx->compartment()->behaviors().extraWarnings(cx))
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
                       jsbytecode* pc, CanAttachGetter canAttachGetter,
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

    if (IsCacheableGetPropCallScripted(obj, holder, shape, isTemporarilyUnoptimizable))
        return CanAttachCallGetter;

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
GeneratePrototypeHoleGuards(CacheIRWriter& writer, JSObject* obj, ObjOperandId objId)
{
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
}

static void
TestMatchingReceiver(CacheIRWriter& writer, JSObject* obj, ObjOperandId objId,
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
EmitReadSlotGuard(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                  ObjOperandId objId, Maybe<ObjOperandId>* holderId)
{
    Maybe<ObjOperandId> expandoId;
    TestMatchingReceiver(writer, obj, objId, &expandoId);

    if (obj != holder) {
        GeneratePrototypeGuards(writer, obj, holder, objId);

        if (holder) {
            // Guard on the holder's shape.
            holderId->emplace(writer.loadObject(holder));
            writer.guardShape(holderId->ref(), holder->as<NativeObject>().lastProperty());
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
    } else if (obj->is<UnboxedPlainObject>() && expandoId.isSome()) {
        holderId->emplace(*expandoId);
    } else {
        holderId->emplace(objId);
    }
}

static void
EmitReadSlotResult(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
                   Shape* shape, ObjOperandId objId)
{
    Maybe<ObjOperandId> holderId;
    EmitReadSlotGuard(writer, obj, holder, objId, &holderId);

    if (obj == holder && obj->is<UnboxedPlainObject>())
        holder = obj->as<UnboxedPlainObject>().maybeExpando();

    // Slot access.
    if (holder) {
        MOZ_ASSERT(holderId->valid());
        EmitLoadSlotResult(writer, *holderId, &holder->as<NativeObject>(), shape);
    } else {
        MOZ_ASSERT(holderId.isNothing());
        writer.loadUndefinedResult();
    }
}

static void
EmitReadSlotReturn(CacheIRWriter& writer, JSObject*, JSObject* holder, Shape* shape,
                   bool wrapResult = false)
{
    // Slot access.
    if (holder) {
        MOZ_ASSERT(shape);
        if (wrapResult)
            writer.wrapResult();
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
                     Shape* shape, ObjOperandId objId, ICState::Mode mode)
{
    // Use the megamorphic guard if we're in megamorphic mode, except if |obj|
    // is a Window as GuardHasGetterSetter doesn't support this yet (Window may
    // require outerizing).
    if (mode == ICState::Mode::Specialized || IsWindow(obj)) {
        Maybe<ObjOperandId> expandoId;
        TestMatchingReceiver(writer, obj, objId, &expandoId);

        if (obj != holder) {
            GeneratePrototypeGuards(writer, obj, holder, objId);

            // Guard on the holder's shape.
            ObjOperandId holderId = writer.loadObject(holder);
            writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());
        }
    } else {
        writer.guardHasGetterSetter(objId, shape);
    }

    EmitCallGetterResultNoGuards(writer, obj, holder, shape, objId);
}

void
GetPropIRGenerator::attachMegamorphicNativeSlot(ObjOperandId objId, jsid id, bool handleMissing)
{
    MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);

    // The stub handles the missing-properties case only if we're seeing one
    // now, to make sure Ion ICs correctly monitor the undefined type.

    if (cacheKind_ == CacheKind::GetProp) {
        writer.megamorphicLoadSlotResult(objId, JSID_TO_ATOM(id)->asPropertyName(),
                                         handleMissing);
    } else {
        MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
        writer.megamorphicLoadSlotByValueResult(objId, getElemKeyValueId(), handleMissing);
    }
    writer.typeMonitorResult();

    trackAttached(handleMissing ? "MegamorphicMissingNativeSlot" : "MegamorphicNativeSlot");
}

bool
GetPropIRGenerator::tryAttachNative(HandleObject obj, ObjOperandId objId, HandleId id)
{
    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);

    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_,
                                                            canAttachGetter_,
                                                            isTemporarilyUnoptimizable_);
    MOZ_ASSERT_IF(idempotent(),
                  type == CanAttachNone || (type == CanAttachReadSlot && holder));
    switch (type) {
      case CanAttachNone:
        return false;
      case CanAttachReadSlot:
        if (mode_ == ICState::Mode::Megamorphic) {
            attachMegamorphicNativeSlot(objId, id, holder == nullptr);
            return true;
        }

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

        trackAttached("NativeSlot");
        return true;
      case CanAttachCallGetter:
        maybeEmitIdGuard(id);
        EmitCallGetterResult(writer, obj, holder, shape, objId, mode_);

        trackAttached("NativeGetter");
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

    // If we're megamorphic prefer a generic proxy stub that handles a lot more
    // cases.
    if (mode_ == ICState::Mode::Megamorphic)
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
                                                            canAttachGetter_,
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

        trackAttached("WindowProxySlot");
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
        EmitCallGetterResult(writer, windowObj, holder, shape, windowObjId, mode_);

        trackAttached("WindowProxyGetter");
        return true;
      }
    }

    MOZ_CRASH("Unreachable");
}

bool
GetPropIRGenerator::tryAttachCrossCompartmentWrapper(HandleObject obj, ObjOperandId objId,
                                                     HandleId id)
{
    // We can only optimize this very wrapper-handler, because others might
    // have a security policy.
    if (!IsWrapper(obj) || Wrapper::wrapperHandler(obj) != &CrossCompartmentWrapper::singleton)
        return false;

    // If we're megamorphic prefer a generic proxy stub that handles a lot more
    // cases.
    if (mode_ == ICState::Mode::Megamorphic)
        return false;

    RootedObject unwrapped(cx_, Wrapper::wrappedObject(obj));
    MOZ_ASSERT(unwrapped == UnwrapOneChecked(obj));

    // If we allowed different zones we would have to wrap strings.
    if (unwrapped->compartment()->zone() != cx_->compartment()->zone())
        return false;

    RootedObject wrappedGlobal(cx_, &obj->global());
    if (!cx_->compartment()->wrap(cx_, &wrappedGlobal))
        return false;

    AutoCompartment ac(cx_, unwrapped);

    // The first CCW for iframes is almost always wrapping another WindowProxy
    // so we optimize for that case as well.
    bool isWindowProxy = IsWindowProxy(unwrapped);
    if (isWindowProxy) {
        MOZ_ASSERT(ToWindowIfWindowProxy(unwrapped) == unwrapped->compartment()->maybeGlobal());
        unwrapped = cx_->global();
        MOZ_ASSERT(unwrapped);
    }

    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);
    NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx_, unwrapped, id, &holder, &shape, pc_, canAttachGetter_,
                               isTemporarilyUnoptimizable_);
    if (canCache != CanAttachReadSlot)
        return false;

    if (holder) {
        EnsureTrackPropertyTypes(cx_, holder, id);
        if (unwrapped == holder) {
            // See the comment in StripPreliminaryObjectStubs.
            if (IsPreliminaryObject(unwrapped))
                preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
            else
                preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
        }
    }

    maybeEmitIdGuard(id);
    writer.guardIsProxy(objId);
    writer.guardIsCrossCompartmentWrapper(objId);

    // Load the object wrapped by the CCW
    ObjOperandId wrapperTargetId = writer.loadWrapperTarget(objId);

    // If the compartment of the wrapped object is different we should fail.
    writer.guardCompartment(wrapperTargetId, wrappedGlobal, unwrapped->compartment());

    ObjOperandId unwrappedId = wrapperTargetId;
    if (isWindowProxy) {
        // For the WindowProxy case also unwrap the inner window.
        // We avoid loadObject, because storing cross compartment objects in
        // stubs / JIT code is tricky.
        writer.guardClass(wrapperTargetId, GuardClassKind::WindowProxy);
        unwrappedId = writer.loadWrapperTarget(wrapperTargetId);
    }

    EmitReadSlotResult(writer, unwrapped, holder, shape, unwrappedId);
    EmitReadSlotReturn(writer, unwrapped, holder, shape, /* wrapResult = */ true);

    trackAttached("CCWSlot");
    return true;
}

bool
GetPropIRGenerator::tryAttachGenericProxy(HandleObject obj, ObjOperandId objId, HandleId id,
                                          bool handleDOMProxies)
{
    MOZ_ASSERT(obj->is<ProxyObject>());

    writer.guardIsProxy(objId);

    if (!handleDOMProxies) {
        // Ensure that the incoming object is not a DOM proxy, so that we can get to
        // the specialized stubs
        writer.guardNotDOMProxy(objId);
    }

    if (cacheKind_ == CacheKind::GetProp || mode_ == ICState::Mode::Specialized) {
        maybeEmitIdGuard(id);
        writer.callProxyGetResult(objId, id);
    } else {
        // Attach a stub that handles every id.
        MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
        MOZ_ASSERT(mode_ == ICState::Mode::Megamorphic);
        writer.callProxyGetByValueResult(objId, getElemKeyValueId());
    }

    writer.typeMonitorResult();

    trackAttached("GenericProxy");
    return true;
}

ObjOperandId
IRGenerator::guardDOMProxyExpandoObjectAndShape(JSObject* obj, ObjOperandId objId,
                                                const Value& expandoVal, JSObject* expandoObj)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    writer.guardShape(objId, obj->maybeShape());

    // Shape determines Class, so now it must be a DOM proxy.
    ValOperandId expandoValId;
    if (expandoVal.isObject())
        expandoValId = writer.loadDOMExpandoValue(objId);
    else
        expandoValId = writer.loadDOMExpandoValueIgnoreGeneration(objId);

    // Guard the expando is an object and shape guard.
    ObjOperandId expandoObjId = writer.guardIsObject(expandoValId);
    writer.guardShape(expandoObjId, expandoObj->as<NativeObject>().shape());
    return expandoObjId;
}

bool
GetPropIRGenerator::tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId, HandleId id)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedValue expandoVal(cx_, GetProxyPrivate(obj));
    RootedObject expandoObj(cx_);
    if (expandoVal.isObject()) {
        expandoObj = &expandoVal.toObject();
    } else {
        MOZ_ASSERT(!expandoVal.isUndefined(),
                   "How did a missing expando manage to shadow things?");
        auto expandoAndGeneration = static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
        MOZ_ASSERT(expandoAndGeneration);
        expandoObj = &expandoAndGeneration->expando.toObject();
    }

    // Try to do the lookup on the expando object.
    RootedNativeObject holder(cx_);
    RootedShape propShape(cx_);
    NativeGetPropCacheability canCache =
        CanAttachNativeGetProp(cx_, expandoObj, id, &holder, &propShape, pc_,
                               canAttachGetter_, isTemporarilyUnoptimizable_);
    if (canCache != CanAttachReadSlot && canCache != CanAttachCallGetter)
        return false;
    if (!holder)
        return false;

    MOZ_ASSERT(holder == expandoObj);

    maybeEmitIdGuard(id);
    ObjOperandId expandoObjId =
        guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

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

    trackAttached("DOMProxyExpando");
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

    trackAttached("DOMProxyShadowed");
    return true;
}

// Callers are expected to have already guarded on the shape of the
// object, which guarantees the object is a DOM proxy.
static void
CheckDOMProxyExpandoDoesNotShadow(CacheIRWriter& writer, JSObject* obj, jsid id,
                                  ObjOperandId objId)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    Value expandoVal = GetProxyPrivate(obj);

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
                                                                pc_, canAttachGetter_,
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

    trackAttached("DOMProxyUnshadowed");
    return true;
}

bool
GetPropIRGenerator::tryAttachProxy(HandleObject obj, ObjOperandId objId, HandleId id)
{
    ProxyStubType type = GetProxyStubType(cx_, obj, id);
    if (type == ProxyStubType::None)
        return false;

    if (mode_ == ICState::Mode::Megamorphic)
        return tryAttachGenericProxy(obj, objId, id, /* handleDOMProxies = */ true);

    switch (type) {
      case ProxyStubType::None:
        break;
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
        if (tryAttachDOMProxyUnshadowed(obj, objId, id))
            return true;
        if (*isTemporarilyUnoptimizable_) {
            // Scripted getter without JIT code. Just wait.
            return false;
        }
        return tryAttachGenericProxy(obj, objId, id, /* handleDOMProxies = */ true);
      case ProxyStubType::Generic:
        return tryAttachGenericProxy(obj, objId, id, /* handleDOMProxies = */ false);
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

    trackAttached("Unboxed");
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

    trackAttached("UnboxedExpando");
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

    trackAttached("TypedObject");
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

        trackAttached("ArrayLength");
        return true;
    }

    if (obj->is<UnboxedArrayObject>()) {
        maybeEmitIdGuard(id);
        writer.guardClass(objId, GuardClassKind::UnboxedArray);
        writer.loadUnboxedArrayLengthResult(objId);
        writer.returnFromIC();

        trackAttached("UnboxedArrayLength");
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

        trackAttached("ArgumentsObjectLength");
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

        trackAttached("FunctionLength");
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

    trackAttached("ModuleNamespace");
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
                                                            canAttachGetter_,
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

    trackAttached("Primitive");
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

    trackAttached("StringLength");
    return true;
}

bool
GetPropIRGenerator::tryAttachStringChar(ValOperandId valId, ValOperandId indexId)
{
    MOZ_ASSERT(idVal_.isInt32());

    if (!val_.isString())
        return false;

    int32_t index = idVal_.toInt32();
    if (index < 0)
        return false;

    JSString* str = val_.toString();
    if (size_t(index) >= str->length())
        return false;

    // This follows JSString::getChar, otherwise we fail to attach getChar in a lot of cases.
    if (str->isRope()) {
        JSRope* rope = &str->asRope();

        // Make sure the left side contains the index.
        if (size_t(index) >= rope->leftChild()->length())
            return false;

        str = rope->leftChild();
    }

    if (!str->isLinear() ||
        str->asLinear().latin1OrTwoByteChar(index) >= StaticStrings::UNIT_STATIC_LIMIT)
    {
        return false;
    }

    StringOperandId strId = writer.guardIsString(valId);
    Int32OperandId int32IndexId = writer.guardIsInt32Index(indexId);
    writer.loadStringCharResult(strId, int32IndexId);
    writer.returnFromIC();

    trackAttached("StringChar");
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

    trackAttached("MagicArgumentsName");
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

    trackAttached("MagicArgument");
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

    trackAttached("ArgumentsObjectArg");
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

    trackAttached("DenseElement");
    return true;
}

static bool
CanAttachDenseElementHole(JSObject* obj, bool ownProp)
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

        // Don't need to check prototype for OwnProperty checks
        if (ownProp)
            return true;

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

    if (!CanAttachDenseElementHole(obj, false))
        return false;

    // Guard on the shape, to prevent non-dense elements from appearing.
    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());

    GeneratePrototypeHoleGuards(writer, obj, objId);
    writer.loadDenseElementHoleResult(objId, indexId);
    writer.typeMonitorResult();

    trackAttached("DenseElementHole");
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

    trackAttached("UnboxedArrayElement");
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

    trackAttached("TypedElement");
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

    trackAttached("ProxyElement");
    return true;
}

void
GetPropIRGenerator::trackAttached(const char* name)
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", val_);
        sp.valueProperty(guard, "property", idVal_);
        sp.attached(guard, name);
        sp.endCache(guard);
    }
#endif
}

void
GetPropIRGenerator::trackNotAttached()
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", val_);
        sp.valueProperty(guard, "property", idVal_);
        sp.endCache(guard);
    }
#endif
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

GetNameIRGenerator::GetNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                       ICState::Mode mode, HandleObject env,
                                       HandlePropertyName name)
  : IRGenerator(cx, script, pc, CacheKind::GetName, mode),
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

    trackNotAttached();
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

    trackAttached("GlobalNameValue");
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

    trackAttached("GlobalNameGetter");
    return true;
}

static bool
NeedEnvironmentShapeGuard(JSObject* envObj)
{
    if (!envObj->is<CallObject>())
        return true;

    // We can skip a guard on the call object if the script's bindings are
    // guaranteed to be immutable (and thus cannot introduce shadowing
    // variables). The function might have been relazified under rare
    // conditions. In that case, we pessimistically create the guard.
    CallObject* callObj = &envObj->as<CallObject>();
    JSFunction* fun = &callObj->callee();
    if (!fun->hasScript() || fun->nonLazyScript()->funHasExtensibleScope())
        return true;

    return false;
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
        if (NeedEnvironmentShapeGuard(env))
            writer.guardShape(lastObjId, env->maybeShape());

        if (env == holder)
            break;

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

    trackAttached("EnvironmentName");
    return true;
}

void
GetNameIRGenerator::trackAttached(const char* name)
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", ObjectValue(*env_));
        sp.valueProperty(guard, "property", StringValue(name_));
        sp.attached(guard, name);
        sp.endCache(guard);
    }
#endif
}

void
GetNameIRGenerator::trackNotAttached()
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", ObjectValue(*env_));
        sp.valueProperty(guard, "property", StringValue(name_));
        sp.endCache(guard);
    }
#endif
}

BindNameIRGenerator::BindNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                         ICState::Mode mode, HandleObject env,
                                         HandlePropertyName name)
  : IRGenerator(cx, script, pc, CacheKind::BindName, mode),
    env_(env),
    name_(name)
{}

bool
BindNameIRGenerator::tryAttachStub()
{
    MOZ_ASSERT(cacheKind_ == CacheKind::BindName);

    AutoAssertNoPendingException aanpe(cx_);

    ObjOperandId envId(writer.setInputOperandId(0));
    RootedId id(cx_, NameToId(name_));

    if (tryAttachGlobalName(envId, id))
        return true;
    if (tryAttachEnvironmentName(envId, id))
        return true;

    trackNotAttached();
    return false;
}

bool
BindNameIRGenerator::tryAttachGlobalName(ObjOperandId objId, HandleId id)
{
    if (!IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope())
        return false;

    Handle<LexicalEnvironmentObject*> globalLexical = env_.as<LexicalEnvironmentObject>();
    MOZ_ASSERT(globalLexical->isGlobal());

    JSObject* result = nullptr;
    if (Shape* shape = globalLexical->lookup(cx_, id)) {
        // If this is an uninitialized lexical or a const, we need to return a
        // RuntimeLexicalErrorObject.
        if (globalLexical->getSlot(shape->slot()).isMagic() || !shape->writable())
            return false;
        result = globalLexical;
    } else {
        result = &globalLexical->global();
    }

    if (result == globalLexical) {
        // Lexical bindings are non-configurable so we can just return the
        // global lexical.
        writer.loadObjectResult(objId);
    } else {
        // If the property exists on the global and is non-configurable, it cannot be
        // shadowed by the lexical scope so we can just return the global without a
        // shape guard.
        Shape* shape = result->as<GlobalObject>().lookup(cx_, id);
        if (!shape || shape->configurable())
            writer.guardShape(objId, globalLexical->lastProperty());
        ObjOperandId globalId = writer.loadEnclosingEnvironment(objId);
        writer.loadObjectResult(globalId);
    }
    writer.returnFromIC();

    trackAttached("GlobalName");
    return true;
}

bool
BindNameIRGenerator::tryAttachEnvironmentName(ObjOperandId objId, HandleId id)
{
    if (IsGlobalOp(JSOp(*pc_)) || script_->hasNonSyntacticScope())
        return false;

    RootedObject env(cx_, env_);
    RootedShape shape(cx_);
    while (true) {
        if (!env->is<GlobalObject>() && !env->is<EnvironmentObject>())
            return false;
        if (env->is<WithEnvironmentObject>())
            return false;

        MOZ_ASSERT(!env->hasUncacheableProto());

        // When we reach an unqualified variables object (like the global) we
        // have to stop looking and return that object.
        if (env->isUnqualifiedVarObj())
            break;

        // Check for an 'own' property on the env. There is no need to
        // check the prototype as non-with scopes do not inherit properties
        // from any prototype.
        shape = env->as<NativeObject>().lookup(cx_, id);
        if (shape)
            break;

        env = env->enclosingEnvironment();
    }

    // If this is an uninitialized lexical or a const, we need to return a
    // RuntimeLexicalErrorObject.
    RootedNativeObject holder(cx_, &env->as<NativeObject>());
    if (shape &&
        holder->is<EnvironmentObject>() &&
        (holder->getSlot(shape->slot()).isMagic() || !shape->writable()))
    {
        return false;
    }

    ObjOperandId lastObjId = objId;
    env = env_;
    while (env) {
        if (NeedEnvironmentShapeGuard(env) && !env->is<GlobalObject>())
            writer.guardShape(lastObjId, env->maybeShape());

        if (env == holder)
            break;

        lastObjId = writer.loadEnclosingEnvironment(lastObjId);
        env = env->enclosingEnvironment();
    }
    writer.loadObjectResult(lastObjId);
    writer.returnFromIC();

    trackAttached("EnvironmentName");
    return true;
}

void
BindNameIRGenerator::trackAttached(const char* name)
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", ObjectValue(*env_));
        sp.valueProperty(guard, "property", StringValue(name_));
        sp.attached(guard, name);
        sp.endCache(guard);
    }
#endif
}

void
BindNameIRGenerator::trackNotAttached()
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", ObjectValue(*env_));
        sp.valueProperty(guard, "property", StringValue(name_));
        sp.endCache(guard);
    }
#endif
}

HasPropIRGenerator::HasPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                       CacheKind cacheKind, ICState::Mode mode,
                                       HandleValue idVal, HandleValue val)
  : IRGenerator(cx, script, pc, cacheKind, mode),
    val_(val),
    idVal_(idVal)
{ }

bool
HasPropIRGenerator::tryAttachDense(HandleObject obj, ObjOperandId objId,
                                   uint32_t index, Int32OperandId indexId)
{
    if (!obj->isNative())
        return false;
    if (!obj->as<NativeObject>().containsDenseElement(index))
        return false;

    // Guard shape to ensure object class is NativeObject.
    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());

    writer.loadDenseElementExistsResult(objId, indexId);
    writer.returnFromIC();

    trackAttached("DenseHasProp");
    return true;
}

bool
HasPropIRGenerator::tryAttachDenseHole(HandleObject obj, ObjOperandId objId,
                                       uint32_t index, Int32OperandId indexId)
{
    bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

    if (!obj->isNative())
        return false;
    if (obj->as<NativeObject>().containsDenseElement(index))
        return false;
    if (!CanAttachDenseElementHole(obj, hasOwn))
        return false;

    // Guard shape to ensure class is NativeObject and to prevent non-dense
    // elements being added. Also ensures prototype doesn't change if dynamic
    // checks aren't emitted.
    writer.guardShape(objId, obj->as<NativeObject>().lastProperty());

    // Generate prototype guards if needed. This includes monitoring that
    // properties were not added in the chain.
    if (!hasOwn)
        GeneratePrototypeHoleGuards(writer, obj, objId);

    writer.loadDenseElementHoleExistsResult(objId, indexId);
    writer.returnFromIC();

    trackAttached("DenseHasPropHole");
    return true;
}

bool
HasPropIRGenerator::tryAttachNative(HandleObject obj, ObjOperandId objId,
                                    HandleId key, ValOperandId keyId)
{
    bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

    JSObject* holder = nullptr;
    PropertyResult prop;

    if (hasOwn) {
        if (!LookupOwnPropertyPure(cx_, obj, key, &prop))
            return false;

        holder = obj;
    } else {
        if (!LookupPropertyPure(cx_, obj, key, &holder, &prop))
            return false;
    }
    if (!prop.isFound())
        return false;

    // Use MegamorphicHasOwnResult if applicable
    if (hasOwn && mode_ == ICState::Mode::Megamorphic) {
        writer.megamorphicHasOwnResult(objId, keyId);
        writer.returnFromIC();
        trackAttached("MegamorphicHasProp");
        return true;
    }

    Maybe<ObjOperandId> tempId;
    emitIdGuard(keyId, key);
    EmitReadSlotGuard(writer, obj, holder, objId, &tempId);
    writer.loadBooleanResult(true);
    writer.returnFromIC();

    trackAttached("NativeHasProp");
    return true;
}

bool
HasPropIRGenerator::tryAttachNativeDoesNotExist(HandleObject obj, ObjOperandId objId,
                                                HandleId key, ValOperandId keyId)
{
    bool hasOwn = (cacheKind_ == CacheKind::HasOwn);

    if (hasOwn) {
        if (!CheckHasNoSuchOwnProperty(cx_, obj, key))
            return false;
    } else {
        if (!CheckHasNoSuchProperty(cx_, obj, key))
            return false;
    }

    // Use MegamorphicHasOwnResult if applicable
    if (hasOwn && mode_ == ICState::Mode::Megamorphic) {
        writer.megamorphicHasOwnResult(objId, keyId);
        writer.returnFromIC();
        trackAttached("MegamorphicHasOwn");
        return true;
    }

    Maybe<ObjOperandId> tempId;
    emitIdGuard(keyId, key);
    if (hasOwn) {
        TestMatchingReceiver(writer, obj, objId, &tempId);
    } else {
        EmitReadSlotGuard(writer, obj, nullptr, objId, &tempId);
    }
    writer.loadBooleanResult(false);
    writer.returnFromIC();

    trackAttached("NativeDoesNotExist");
    return true;
}

bool
HasPropIRGenerator::tryAttachProxyElement(HandleObject obj, ObjOperandId objId,
                                          ValOperandId keyId)
{
    MOZ_ASSERT(cacheKind_ == CacheKind::HasOwn);

    if (!obj->is<ProxyObject>())
        return false;

    writer.guardIsProxy(objId);
    writer.callProxyHasOwnResult(objId, keyId);
    writer.returnFromIC();

    trackAttached("ProxyHasProp");
    return true;
}

bool
HasPropIRGenerator::tryAttachStub()
{
    MOZ_ASSERT(cacheKind_ == CacheKind::In ||
               cacheKind_ == CacheKind::HasOwn);

    AutoAssertNoPendingException aanpe(cx_);

    // NOTE: Argument order is PROPERTY, OBJECT
    ValOperandId keyId(writer.setInputOperandId(0));
    ValOperandId valId(writer.setInputOperandId(1));

    if (!val_.isObject()) {
        trackNotAttached();
        return false;
    }
    RootedObject obj(cx_, &val_.toObject());
    ObjOperandId objId = writer.guardIsObject(valId);

    // Optimize DOM Proxies for JSOP_HASOWN
    if (cacheKind_ == CacheKind::HasOwn) {
        if (tryAttachProxyElement(obj, objId, keyId))
            return true;
    }

    RootedId id(cx_);
    bool nameOrSymbol;
    if (!ValueToNameOrSymbolId(cx_, idVal_, &id, &nameOrSymbol)) {
        cx_->clearPendingException();
        return false;
    }

    if (nameOrSymbol) {
        if (tryAttachNative(obj, objId, id, keyId))
            return true;
        if (tryAttachNativeDoesNotExist(obj, objId, id, keyId))
            return true;

        trackNotAttached();
        return false;
    }

    uint32_t index;
    Int32OperandId indexId;
    if (maybeGuardInt32Index(idVal_, keyId, &index, &indexId)) {
        if (tryAttachDense(obj, objId, index, indexId))
            return true;
        if (tryAttachDenseHole(obj, objId, index, indexId))
            return true;

        trackNotAttached();
        return false;
    }

    trackNotAttached();
    return false;
}

void
HasPropIRGenerator::trackAttached(const char* name)
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", val_);
        sp.valueProperty(guard, "property", idVal_);
        sp.attached(guard, name);
        sp.endCache(guard);
    }
#endif
}

void
HasPropIRGenerator::trackNotAttached()
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", val_);
        sp.valueProperty(guard, "property", idVal_);
        sp.endCache(guard);
    }
#endif
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

SetPropIRGenerator::SetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                       CacheKind cacheKind, ICState::Mode mode,
                                       bool* isTemporarilyUnoptimizable,
                                       HandleValue lhsVal, HandleValue idVal, HandleValue rhsVal,
                                       bool needsTypeBarrier, bool maybeHasExtraIndexedProps)
  : IRGenerator(cx, script, pc, cacheKind, mode),
    lhsVal_(lhsVal),
    idVal_(idVal),
    rhsVal_(rhsVal),
    isTemporarilyUnoptimizable_(isTemporarilyUnoptimizable),
    typeCheckInfo_(cx, needsTypeBarrier),
    preliminaryObjectAction_(PreliminaryObjectAction::None),
    attachedTypedArrayOOBStub_(false),
    maybeHasExtraIndexedProps_(maybeHasExtraIndexedProps)
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
            if (tryAttachTypedObjectProperty(obj, objId, id, rhsValId))
                return true;
            if (tryAttachSetArrayLength(obj, objId, id, rhsValId))
                return true;
            if (IsPropertySetOp(JSOp(*pc_))) {
                if (tryAttachSetter(obj, objId, id, rhsValId))
                    return true;
                if (tryAttachWindowProxy(obj, objId, id, rhsValId))
                    return true;
                if (tryAttachProxy(obj, objId, id, rhsValId))
                    return true;
            }
            return false;
        }

        if (IsPropertySetOp(JSOp(*pc_))) {
            if (tryAttachProxyElement(obj, objId, rhsValId))
                return true;
        }

        uint32_t index;
        Int32OperandId indexId;
        if (maybeGuardInt32Index(idVal_, setElemKeyValueId(), &index, &indexId)) {
            if (tryAttachSetDenseElement(obj, objId, index, indexId, rhsValId))
                return true;
            if (tryAttachSetDenseElementHole(obj, objId, index, indexId, rhsValId))
                return true;
            if (tryAttachSetUnboxedArrayElement(obj, objId, index, indexId, rhsValId))
                return true;
            if (tryAttachSetUnboxedArrayElementHole(obj, objId, index, indexId, rhsValId))
                return true;
            if (tryAttachSetTypedElement(obj, objId, index, indexId, rhsValId))
                return true;
            return false;
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

static bool
CanAttachNativeSetSlot(JSContext* cx, HandleObject obj, HandleId id,
                       bool* isTemporarilyUnoptimizable, MutableHandleShape propShape)
{
    if (!obj->isNative())
        return false;

    propShape.set(LookupShapeForSetSlot(&obj->as<NativeObject>(), id));
    if (!propShape)
        return false;

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

bool
SetPropIRGenerator::tryAttachNativeSetSlot(HandleObject obj, ObjOperandId objId, HandleId id,
                                           ValOperandId rhsId)
{
    RootedShape propShape(cx_);
    if (!CanAttachNativeSetSlot(cx_, obj, id, isTemporarilyUnoptimizable_, &propShape))
        return false;

    if (mode_ == ICState::Mode::Megamorphic && cacheKind_ == CacheKind::SetProp) {
        writer.megamorphicStoreSlot(objId, JSID_TO_ATOM(id)->asPropertyName(), rhsId,
                                    typeCheckInfo_.needsTypeBarrier());
        writer.returnFromIC();
        trackAttached("MegamorphicNativeSlot");
        return true;
    }

    maybeEmitIdGuard(id);

    // If we need a property type barrier (always in Baseline, sometimes in
    // Ion), guard on both the shape and the group. If Ion knows the property
    // types match, we don't need the group guard.
    NativeObject* nobj = &obj->as<NativeObject>();
    if (typeCheckInfo_.needsTypeBarrier())
        writer.guardGroup(objId, nobj->group());
    writer.guardShape(objId, nobj->lastProperty());

    if (IsPreliminaryObject(obj))
        preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
    else
        preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;

    typeCheckInfo_.set(nobj->group(), id);
    EmitStoreSlotAndReturn(writer, objId, nobj, propShape, rhsId);

    trackAttached("NativeSlot");
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
    typeCheckInfo_.set(obj->group(), id);
    EmitStoreSlotAndReturn(writer, expandoId, expando, propShape, rhsId);

    trackAttached("UnboxedExpando");
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

    typeCheckInfo_.set(obj->group(), id);
    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;

    trackAttached("Unboxed");
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

    typeCheckInfo_.set(obj->group(), id);

    // Scalar types can always be stored without a type update stub.
    if (fieldDescr->is<ScalarTypeDescr>()) {
        Scalar::Type type = fieldDescr->as<ScalarTypeDescr>().type();
        writer.storeTypedObjectScalarProperty(objId, fieldOffset, layout, type, rhsId);
        writer.returnFromIC();

        trackAttached("TypedObject");
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

    trackAttached("TypedObject");
    return true;
}

void
SetPropIRGenerator::trackAttached(const char* name)
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", lhsVal_);
        sp.valueProperty(guard, "property", idVal_);
        sp.valueProperty(guard, "value", rhsVal_);
        sp.attached(guard, name);
        sp.endCache(guard);
    }
#endif
}

void
SetPropIRGenerator::trackNotAttached()
{
#ifdef JS_CACHEIR_SPEW
    CacheIRSpewer& sp = CacheIRSpewer::singleton();
    if (sp.enabled()) {
        LockGuard<Mutex> guard(sp.lock());
        sp.beginCache(guard, *this);
        sp.valueProperty(guard, "base", lhsVal_);
        sp.valueProperty(guard, "property", idVal_);
        sp.valueProperty(guard, "value", rhsVal_);
        sp.endCache(guard);
    }
#endif
}

static bool
CanAttachSetter(JSContext* cx, jsbytecode* pc, HandleObject obj, HandleId id,
                MutableHandleObject holder, MutableHandleShape propShape,
                bool* isTemporarilyUnoptimizable)
{
    // Don't attach a setter stub for ops like JSOP_INITELEM.
    MOZ_ASSERT(IsPropertySetOp(JSOp(*pc)));

    PropertyResult prop;
    if (!LookupPropertyPure(cx, obj, id, holder.address(), &prop))
        return false;

    if (prop.isNonNativeProperty())
        return false;

    propShape.set(prop.maybeShape());
    if (!IsCacheableSetPropCallScripted(obj, holder, propShape, isTemporarilyUnoptimizable) &&
        !IsCacheableSetPropCallNative(obj, holder, propShape))
    {
        return false;
    }

    return true;
}

static void
EmitCallSetterNoGuards(CacheIRWriter& writer, JSObject* obj, JSObject* holder,
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
    RootedObject holder(cx_);
    RootedShape propShape(cx_);
    if (!CanAttachSetter(cx_, pc_, obj, id, &holder, &propShape, isTemporarilyUnoptimizable_))
        return false;

    maybeEmitIdGuard(id);

    // Use the megamorphic guard if we're in megamorphic mode, except if |obj|
    // is a Window as GuardHasGetterSetter doesn't support this yet (Window may
    // require outerizing).
    if (mode_ == ICState::Mode::Specialized || IsWindow(obj)) {
        Maybe<ObjOperandId> expandoId;
        TestMatchingReceiver(writer, obj, objId, &expandoId);

        if (obj != holder) {
            GeneratePrototypeGuards(writer, obj, holder, objId);

            // Guard on the holder's shape.
            ObjOperandId holderId = writer.loadObject(holder);
            writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());
        }
    } else {
        writer.guardHasGetterSetter(objId, propShape);
    }

    EmitCallSetterNoGuards(writer, obj, holder, propShape, objId, rhsId);

    trackAttached("Setter");
    return true;
}

bool
SetPropIRGenerator::tryAttachSetArrayLength(HandleObject obj, ObjOperandId objId, HandleId id,
                                            ValOperandId rhsId)
{
    if (!obj->is<ArrayObject>() ||
        !JSID_IS_ATOM(id, cx_->names().length) ||
        !obj->as<ArrayObject>().lengthIsWritable())
    {
        return false;
    }

    maybeEmitIdGuard(id);
    writer.guardClass(objId, GuardClassKind::Array);
    writer.callSetArrayLength(objId, IsStrictSetPC(pc_), rhsId);
    writer.returnFromIC();

    trackAttached("SetArrayLength");
    return true;
}

bool
SetPropIRGenerator::tryAttachSetDenseElement(HandleObject obj, ObjOperandId objId, uint32_t index,
                                             Int32OperandId indexId, ValOperandId rhsId)
{
    if (!obj->isNative())
        return false;

    NativeObject* nobj = &obj->as<NativeObject>();
    if (!nobj->containsDenseElement(index) || nobj->getElementsHeader()->isFrozen())
        return false;

    if (typeCheckInfo_.needsTypeBarrier())
        writer.guardGroup(objId, nobj->group());
    writer.guardShape(objId, nobj->shape());

    writer.storeDenseElement(objId, indexId, rhsId);
    writer.returnFromIC();

    // Type inference uses JSID_VOID for the element types.
    typeCheckInfo_.set(nobj->group(), JSID_VOID);

    trackAttached("SetDenseElement");
    return true;
}

static bool
CanAttachAddElement(JSObject* obj, bool isInit)
{
    // Make sure the objects on the prototype don't have any indexed properties
    // or that such properties can't appear without a shape change.
    do {
        // The first two checks are also relevant to the receiver object.
        if (obj->isIndexed())
            return false;

        const Class* clasp = obj->getClass();
        if ((clasp != &ArrayObject::class_ && clasp != &UnboxedArrayObject::class_) &&
            (clasp->getAddProperty() ||
             clasp->getResolve() ||
             clasp->getOpsLookupProperty() ||
             clasp->getSetProperty() ||
             clasp->getOpsSetProperty()))
        {
            return false;
        }

        // If we're initializing a property instead of setting one, the objects
        // on the prototype are not relevant.
        if (isInit)
            break;

        JSObject* proto = obj->staticPrototype();
        if (!proto)
            break;

        if (!proto->isNative())
            return false;

        obj = proto;
    } while (true);

    return true;
}

static void
ShapeGuardProtoChain(CacheIRWriter& writer, JSObject* obj, ObjOperandId objId)
{
    while (true) {
        // Guard on the proto if the shape does not imply the proto. Singleton
        // objects always trigger a shape change when the proto changes, so we
        // don't need a guard in that case.
        bool guardProto = obj->hasUncacheableProto() && !obj->isSingleton();

        obj = obj->staticPrototype();
        if (!obj)
            return;

        objId = writer.loadProto(objId);
        if (guardProto)
            writer.guardSpecificObject(objId, obj);
        writer.guardShape(objId, obj->as<NativeObject>().shape());
    }
}

bool
SetPropIRGenerator::tryAttachSetDenseElementHole(HandleObject obj, ObjOperandId objId,
                                                 uint32_t index, Int32OperandId indexId,
                                                 ValOperandId rhsId)
{
    if (!obj->isNative() || rhsVal_.isMagic(JS_ELEMENTS_HOLE))
        return false;

    JSOp op = JSOp(*pc_);
    MOZ_ASSERT(IsPropertySetOp(op) || IsPropertyInitOp(op));

    if (op == JSOP_INITHIDDENELEM)
        return false;

    NativeObject* nobj = &obj->as<NativeObject>();
    if (!nobj->nonProxyIsExtensible())
        return false;

    MOZ_ASSERT(!nobj->getElementsHeader()->isFrozen(),
               "Extensible objects should not have frozen elements");

    uint32_t initLength = nobj->getDenseInitializedLength();

    // Optimize if we're adding an element at initLength or writing to a hole.
    // Don't handle the adding case if the current accesss is in bounds, to
    // ensure we always call noteArrayWriteHole.
    bool isAdd = index == initLength;
    bool isHoleInBounds = index < initLength && !nobj->containsDenseElement(index);
    if (!isAdd && !isHoleInBounds)
        return false;

    // Can't add new elements to arrays with non-writable length.
    if (isAdd && nobj->is<ArrayObject>() && !nobj->as<ArrayObject>().lengthIsWritable())
        return false;

    // Typed arrays don't have dense elements.
    if (nobj->is<TypedArrayObject>())
        return false;

    // Check for other indexed properties or class hooks.
    if (!CanAttachAddElement(nobj, IsPropertyInitOp(op)))
        return false;

    if (typeCheckInfo_.needsTypeBarrier())
        writer.guardGroup(objId, nobj->group());
    writer.guardShape(objId, nobj->shape());

    // Also shape guard the proto chain, unless this is an INITELEM or we know
    // the proto chain has no indexed props.
    if (IsPropertySetOp(op) && maybeHasExtraIndexedProps_)
        ShapeGuardProtoChain(writer, obj, objId);

    writer.storeDenseElementHole(objId, indexId, rhsId, isAdd);
    writer.returnFromIC();

    // Type inference uses JSID_VOID for the element types.
    typeCheckInfo_.set(nobj->group(), JSID_VOID);

    trackAttached(isAdd ? "AddDenseElement" : "StoreDenseElementHole");
    return true;
}

bool
SetPropIRGenerator::tryAttachSetUnboxedArrayElement(HandleObject obj, ObjOperandId objId,
                                                    uint32_t index, Int32OperandId indexId,
                                                    ValOperandId rhsId)
{
    if (!obj->is<UnboxedArrayObject>())
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint)
        return false;

    if (index >= obj->as<UnboxedArrayObject>().initializedLength())
        return false;

    writer.guardGroup(objId, obj->group());

    JSValueType elementType = obj->group()->unboxedLayoutDontCheckGeneration().elementType();
    EmitGuardUnboxedPropertyType(writer, elementType, rhsId);

    writer.storeUnboxedArrayElement(objId, indexId, rhsId, elementType);
    writer.returnFromIC();

    // Type inference uses JSID_VOID for the element types.
    typeCheckInfo_.set(obj->group(), JSID_VOID);

    trackAttached("SetUnboxedArrayElement");
    return true;
}

bool
SetPropIRGenerator::tryAttachSetTypedElement(HandleObject obj, ObjOperandId objId,
                                             uint32_t index, Int32OperandId indexId,
                                             ValOperandId rhsId)
{
    if (!obj->is<TypedArrayObject>() && !IsPrimitiveArrayTypedObject(obj))
        return false;

    if (!rhsVal_.isNumber())
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint && TypedThingRequiresFloatingPoint(obj))
        return false;

    bool handleOutOfBounds = false;
    if (obj->is<TypedArrayObject>()) {
        handleOutOfBounds = (index >= obj->as<TypedArrayObject>().length());
    } else {
        // Typed objects throw on out of bounds accesses. Don't attach
        // a stub in this case.
        if (index >= obj->as<TypedObject>().length())
            return false;

        // Don't attach stubs if the underlying storage for typed objects
        // in the compartment could be detached, as the stub will always
        // bail out.
        if (cx_->compartment()->detachedTypedObjects)
            return false;
    }

    Scalar::Type elementType = TypedThingElementType(obj);
    TypedThingLayout layout = GetTypedThingLayout(obj->getClass());

    if (!obj->is<TypedArrayObject>())
        writer.guardNoDetachedTypedObjects();

    writer.guardShape(objId, obj->as<ShapedObject>().shape());
    writer.storeTypedElement(objId, indexId, rhsId, layout, elementType, handleOutOfBounds);
    writer.returnFromIC();

    if (handleOutOfBounds)
        attachedTypedArrayOOBStub_ = true;

    trackAttached(handleOutOfBounds ? "SetTypedElementOOB" : "SetTypedElement");
    return true;
}

bool
SetPropIRGenerator::tryAttachSetUnboxedArrayElementHole(HandleObject obj, ObjOperandId objId,
                                                        uint32_t index, Int32OperandId indexId,
                                                        ValOperandId rhsId)
{
    if (!obj->is<UnboxedArrayObject>() || rhsVal_.isMagic(JS_ELEMENTS_HOLE))
        return false;

    if (!cx_->runtime()->jitSupportsFloatingPoint)
        return false;

    JSOp op = JSOp(*pc_);
    MOZ_ASSERT(IsPropertySetOp(op) || IsPropertyInitOp(op));

    if (op == JSOP_INITHIDDENELEM)
        return false;

    // Optimize if we're adding an element at initLength. Unboxed arrays don't
    // have holes at indexes < initLength.
    UnboxedArrayObject* aobj = &obj->as<UnboxedArrayObject>();
    if (index != aobj->initializedLength() || index >= aobj->capacity())
        return false;

    // Check for other indexed properties or class hooks.
    if (!CanAttachAddElement(aobj, IsPropertyInitOp(op)))
        return false;

    writer.guardGroup(objId, aobj->group());

    JSValueType elementType = aobj->group()->unboxedLayoutDontCheckGeneration().elementType();
    EmitGuardUnboxedPropertyType(writer, elementType, rhsId);

    // Also shape guard the proto chain, unless this is an INITELEM.
    if (IsPropertySetOp(op))
        ShapeGuardProtoChain(writer, aobj, objId);

    writer.storeUnboxedArrayElementHole(objId, indexId, rhsId, elementType);
    writer.returnFromIC();

    // Type inference uses JSID_VOID for the element types.
    typeCheckInfo_.set(aobj->group(), JSID_VOID);

    trackAttached("StoreUnboxedArrayElementHole");
    return true;
}

bool
SetPropIRGenerator::tryAttachGenericProxy(HandleObject obj, ObjOperandId objId, HandleId id,
                                          ValOperandId rhsId, bool handleDOMProxies)
{
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
        writer.callProxySetByValue(objId, setElemKeyValueId(), rhsId, IsStrictSetPC(pc_));
    }

    writer.returnFromIC();

    trackAttached("GenericProxy");
    return true;
}

bool
SetPropIRGenerator::tryAttachDOMProxyShadowed(HandleObject obj, ObjOperandId objId, HandleId id,
                                              ValOperandId rhsId)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    maybeEmitIdGuard(id);
    writer.guardShape(objId, obj->maybeShape());

    // No need for more guards: we know this is a DOM proxy, since the shape
    // guard enforces a given JSClass, so just go ahead and emit the call to
    // ProxySet.
    writer.callProxySet(objId, id, rhsId, IsStrictSetPC(pc_));
    writer.returnFromIC();

    trackAttached("DOMProxyShadowed");
    return true;
}

bool
SetPropIRGenerator::tryAttachDOMProxyUnshadowed(HandleObject obj, ObjOperandId objId, HandleId id,
                                                ValOperandId rhsId)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedObject proto(cx_, obj->staticPrototype());
    if (!proto)
        return false;

    RootedObject holder(cx_);
    RootedShape propShape(cx_);
    if (!CanAttachSetter(cx_, pc_, proto, id, &holder, &propShape, isTemporarilyUnoptimizable_))
        return false;

    maybeEmitIdGuard(id);
    writer.guardShape(objId, obj->maybeShape());

    // Guard that our expando object hasn't started shadowing this property.
    CheckDOMProxyExpandoDoesNotShadow(writer, obj, id, objId);

    GeneratePrototypeGuards(writer, obj, holder, objId);

    // Guard on the holder of the property.
    ObjOperandId holderId = writer.loadObject(holder);
    writer.guardShape(holderId, holder->as<NativeObject>().lastProperty());

    // EmitCallSetterNoGuards expects |obj| to be the object the property is
    // on to do some checks. Since we actually looked at proto, and no extra
    // guards will be generated, we can just pass that instead.
    EmitCallSetterNoGuards(writer, proto, holder, propShape, objId, rhsId);

    trackAttached("DOMProxyUnshadowed");
    return true;
}

bool
SetPropIRGenerator::tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId, HandleId id,
                                             ValOperandId rhsId)
{
    MOZ_ASSERT(IsCacheableDOMProxy(obj));

    RootedValue expandoVal(cx_, GetProxyPrivate(obj));
    RootedObject expandoObj(cx_);
    if (expandoVal.isObject()) {
        expandoObj = &expandoVal.toObject();
    } else {
        MOZ_ASSERT(!expandoVal.isUndefined(),
                   "How did a missing expando manage to shadow things?");
        auto expandoAndGeneration = static_cast<ExpandoAndGeneration*>(expandoVal.toPrivate());
        MOZ_ASSERT(expandoAndGeneration);
        expandoObj = &expandoAndGeneration->expando.toObject();
    }

    RootedShape propShape(cx_);
    if (CanAttachNativeSetSlot(cx_, expandoObj, id, isTemporarilyUnoptimizable_, &propShape)) {
        maybeEmitIdGuard(id);
        ObjOperandId expandoObjId =
            guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

        NativeObject* nativeExpandoObj = &expandoObj->as<NativeObject>();
        writer.guardGroup(expandoObjId, nativeExpandoObj->group());
        typeCheckInfo_.set(nativeExpandoObj->group(), id);

        EmitStoreSlotAndReturn(writer, expandoObjId, nativeExpandoObj, propShape, rhsId);
        trackAttached("DOMProxyExpandoSlot");
        return true;
    }

    RootedObject holder(cx_);
    if (CanAttachSetter(cx_, pc_, expandoObj, id, &holder, &propShape,
                        isTemporarilyUnoptimizable_))
    {
        // Note that we don't actually use the expandoObjId here after the
        // shape guard. The DOM proxy (objId) is passed to the setter as
        // |this|.
        maybeEmitIdGuard(id);
        guardDOMProxyExpandoObjectAndShape(obj, objId, expandoVal, expandoObj);

        MOZ_ASSERT(holder == expandoObj);
        EmitCallSetterNoGuards(writer, expandoObj, expandoObj, propShape, objId, rhsId);
        trackAttached("DOMProxyExpandoSetter");
        return true;
    }

    return false;
}

bool
SetPropIRGenerator::tryAttachProxy(HandleObject obj, ObjOperandId objId, HandleId id,
                                   ValOperandId rhsId)
{
    // Don't attach a proxy stub for ops like JSOP_INITELEM.
    MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

    ProxyStubType type = GetProxyStubType(cx_, obj, id);
    if (type == ProxyStubType::None)
        return false;

    if (mode_ == ICState::Mode::Megamorphic)
        return tryAttachGenericProxy(obj, objId, id, rhsId, /* handleDOMProxies = */ true);

    switch (type) {
      case ProxyStubType::None:
        break;
      case ProxyStubType::DOMExpando:
        if (tryAttachDOMProxyExpando(obj, objId, id, rhsId))
            return true;
        if (*isTemporarilyUnoptimizable_) {
            // Scripted setter without JIT code. Just wait.
            return false;
        }
        MOZ_FALLTHROUGH; // Fall through to the generic shadowed case.
      case ProxyStubType::DOMShadowed:
        return tryAttachDOMProxyShadowed(obj, objId, id, rhsId);
      case ProxyStubType::DOMUnshadowed:
        if (tryAttachDOMProxyUnshadowed(obj, objId, id, rhsId))
            return true;
        if (*isTemporarilyUnoptimizable_) {
            // Scripted setter without JIT code. Just wait.
            return false;
        }
        return tryAttachGenericProxy(obj, objId, id, rhsId, /* handleDOMProxies = */ true);
      case ProxyStubType::Generic:
        return tryAttachGenericProxy(obj, objId, id, rhsId, /* handleDOMProxies = */ false);
    }

    MOZ_CRASH("Unexpected ProxyStubType");
}

bool
SetPropIRGenerator::tryAttachProxyElement(HandleObject obj, ObjOperandId objId, ValOperandId rhsId)
{
    // Don't attach a proxy stub for ops like JSOP_INITELEM.
    MOZ_ASSERT(IsPropertySetOp(JSOp(*pc_)));

    if (!obj->is<ProxyObject>())
        return false;

    writer.guardIsProxy(objId);

    // Like GetPropIRGenerator::tryAttachProxyElement, don't check for DOM
    // proxies here as we don't have specialized DOM stubs for this.
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    writer.callProxySetByValue(objId, setElemKeyValueId(), rhsId, IsStrictSetPC(pc_));
    writer.returnFromIC();

    trackAttached("ProxyElement");
    return true;
}

bool
SetPropIRGenerator::tryAttachWindowProxy(HandleObject obj, ObjOperandId objId, HandleId id,
                                         ValOperandId rhsId)
{
    // Attach a stub when the receiver is a WindowProxy and we can do the set
    // on the Window (the global object).

    if (!IsWindowProxy(obj))
        return false;

    // If we're megamorphic prefer a generic proxy stub that handles a lot more
    // cases.
    if (mode_ == ICState::Mode::Megamorphic)
        return false;

    // This must be a WindowProxy for the current Window/global. Else it would
    // be a cross-compartment wrapper and IsWindowProxy returns false for
    // those.
    MOZ_ASSERT(obj->getClass() == cx_->runtime()->maybeWindowProxyClass());
    MOZ_ASSERT(ToWindowIfWindowProxy(obj) == cx_->global());

    // Now try to do the set on the Window (the current global).
    Handle<GlobalObject*> windowObj = cx_->global();

    RootedShape propShape(cx_);
    if (!CanAttachNativeSetSlot(cx_, windowObj, id, isTemporarilyUnoptimizable_, &propShape))
        return false;

    maybeEmitIdGuard(id);

    writer.guardClass(objId, GuardClassKind::WindowProxy);
    ObjOperandId windowObjId = writer.loadObject(windowObj);

    writer.guardShape(windowObjId, windowObj->lastProperty());
    writer.guardGroup(windowObjId, windowObj->group());
    typeCheckInfo_.set(windowObj->group(), id);

    EmitStoreSlotAndReturn(writer, windowObjId, windowObj, propShape, rhsId);

    trackAttached("WindowProxySlot");
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

    // Watch out for resolve hooks.
    if (ClassMayResolveId(cx_->names(), obj->getClass(), id, obj)) {
        // The JSFunction resolve hook defines a (non-configurable and
        // non-enumerable) |prototype| property on certain functions. Scripts
        // often assign a custom |prototype| object and we want to optimize
        // this |prototype| set and eliminate the default object allocation.
        //
        // We check group->maybeInterpretedFunction() here and guard on the
        // group. The group is unique for a particular function so this ensures
        // we don't add the default prototype property to functions that don't
        // have it.
        if (!obj->is<JSFunction>() ||
            !JSID_IS_ATOM(id, cx_->names().prototype) ||
            !oldGroup->maybeInterpretedFunction() ||
            !obj->as<JSFunction>().needsPrototypeProperty())
        {
            return false;
        }
        MOZ_ASSERT(!propShape->configurable());
        MOZ_ASSERT(!propShape->enumerable());
    }

    // Also watch out for addProperty hooks. Ignore the Array addProperty hook,
    // because it doesn't do anything for non-index properties.
    DebugOnly<uint32_t> index;
    MOZ_ASSERT_IF(obj->is<ArrayObject>(), !IdIsIndex(id, &index));
    if (!obj->is<ArrayObject>() && obj->getClass()->getAddProperty())
        return false;

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
        // addition. Allow the JSFunction resolve hook as it only defines plain
        // data properties and we don't need to invoke it for objects on the
        // proto chain.
        if (ClassMayResolveId(cx_->names(), proto->getClass(), id, proto) &&
            !proto->is<JSFunction>())
        {
            return false;
        }
    }

    ObjOperandId objId = writer.guardIsObject(objValId);
    maybeEmitIdGuard(id);

    writer.guardGroup(objId, oldGroup);

    // If we are adding a property to an object for which the new script
    // properties analysis hasn't been performed yet, make sure the stub fails
    // after we run the analysis as a group change may be required here. The
    // group change is not required for correctness but improves type
    // information elsewhere.
    if (oldGroup->newScript() && !oldGroup->newScript()->analyzed()) {
        writer.guardGroupHasUnanalyzedNewScript(oldGroup);
        MOZ_ASSERT(IsPreliminaryObject(obj));
        preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
    } else {
        preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
    }

    // Shape guard the holder.
    ObjOperandId holderId = objId;
    if (!obj->isNative()) {
        MOZ_ASSERT(obj->as<UnboxedPlainObject>().maybeExpando());
        holderId = writer.guardAndLoadUnboxedExpando(objId);
    }
    writer.guardShape(holderId, oldShape);

    ShapeGuardProtoChain(writer, obj, objId);

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
        trackAttached("AddSlot");
    } else {
        size_t offset = holderOrExpando->dynamicSlotIndex(propShape->slot()) * sizeof(Value);
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
    return true;
}

TypeOfIRGenerator::TypeOfIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                     ICState::Mode mode, HandleValue value)
  : IRGenerator(cx, script, pc, CacheKind::TypeOf, mode),
    val_(value)
{ }

bool
TypeOfIRGenerator::tryAttachStub()
{
    MOZ_ASSERT(cacheKind_ == CacheKind::TypeOf);

    AutoAssertNoPendingException aanpe(cx_);

    ValOperandId valId(writer.setInputOperandId(0));

    if (tryAttachPrimitive(valId))
        return true;

    MOZ_ALWAYS_TRUE(tryAttachObject(valId));
    return true;
}

bool
TypeOfIRGenerator::tryAttachPrimitive(ValOperandId valId)
{
    if (!val_.isPrimitive())
        return false;

    writer.guardType(valId, val_.isNumber() ? JSVAL_TYPE_DOUBLE : val_.extractNonDoubleType());
    writer.loadStringResult(TypeName(js::TypeOfValue(val_), cx_->names()));
    writer.returnFromIC();

    return true;
}

bool
TypeOfIRGenerator::tryAttachObject(ValOperandId valId)
{
    if (!val_.isObject())
        return false;

    ObjOperandId objId = writer.guardIsObject(valId);
    writer.loadTypeOfObjectResult(objId);
    writer.returnFromIC();

    return true;
}


CallIRGenerator::CallIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                                 ICState::Mode mode, uint32_t argc,
                                 HandleValue callee, HandleValue thisval, HandleValueArray args)
  : IRGenerator(cx, script, pc, CacheKind::Call, mode),
    argc_(argc),
    callee_(callee),
    thisval_(thisval),
    args_(args),
    cachedStrategy_()
{ }

CallIRGenerator::OptStrategy
CallIRGenerator::canOptimize()
{
    // Ensure callee is a function.
    if (!callee_.isObject() || !callee_.toObject().is<JSFunction>())
        return OptStrategy::None;

    RootedFunction calleeFunc(cx_, &callee_.toObject().as<JSFunction>());

    OptStrategy strategy;
    if ((strategy = canOptimizeStringSplit(calleeFunc)) != OptStrategy::None) {
        return strategy;
    }

    return OptStrategy::None;
}

CallIRGenerator::OptStrategy
CallIRGenerator::canOptimizeStringSplit(HandleFunction calleeFunc)
{
    if (argc_ != 2 || !args_[0].isString() || !args_[1].isString())
        return OptStrategy::None;

    // Just for now: if they're both atoms, then do not optimize using
    // CacheIR and allow the legacy "ConstStringSplit" BaselineIC optimization
    // to proceed.
    if (args_[0].toString()->isAtom() && args_[1].toString()->isAtom())
        return OptStrategy::None;

    if (!calleeFunc->isNative())
        return OptStrategy::None;

    if (calleeFunc->native() != js::intrinsic_StringSplitString)
        return OptStrategy::None;

    return OptStrategy::StringSplit;
}

bool
CallIRGenerator::tryAttachStringSplit()
{
    // Get the object group to use for this location.
    RootedObjectGroup group(cx_, ObjectGroupCompartment::getStringSplitStringGroup(cx_));
    if (!group) {
        return false;
    }

    AutoAssertNoPendingException aanpe(cx_);
    Int32OperandId argcId(writer.setInputOperandId(0));

    // Ensure argc == 1.
    writer.guardSpecificInt32Immediate(argcId, 2);

    // 1 argument only.  Stack-layout here is (bottom to top):
    //
    //  3: Callee
    //  2: ThisValue
    //  1: Arg0
    //  0: Arg1 <-- Top of stack

    // Ensure callee is an object and is the function that matches the callee optimized
    // against during stub generation (i.e. the String_split function object).
    ValOperandId calleeValId = writer.loadStackValue(3);
    ObjOperandId calleeObjId = writer.guardIsObject(calleeValId);
    writer.guardIsNativeFunction(calleeObjId, js::intrinsic_StringSplitString);

    // Ensure arg0 is a string.
    ValOperandId arg0ValId = writer.loadStackValue(1);
    StringOperandId arg0StrId = writer.guardIsString(arg0ValId);

    // Ensure arg1 is a string.
    ValOperandId arg1ValId = writer.loadStackValue(0);
    StringOperandId arg1StrId = writer.guardIsString(arg1ValId);

    // Call custom string splitter VM-function.
    writer.callStringSplitResult(arg0StrId, arg1StrId, group);
    writer.typeMonitorResult();

    return true;
}

CallIRGenerator::OptStrategy
CallIRGenerator::getOptStrategy(bool* optimizeAfterCall)
{
    if (!cachedStrategy_) {
        cachedStrategy_ = mozilla::Some(canOptimize());
    }
    if (optimizeAfterCall != nullptr) {
        MOZ_ASSERT(cachedStrategy_.isSome());
        switch (cachedStrategy_.value()) {
          case OptStrategy::StringSplit:
            *optimizeAfterCall = true;
            break;

          default:
            *optimizeAfterCall = false;
        }
    }
    return cachedStrategy_.value();
}

bool
CallIRGenerator::tryAttachStub()
{
    OptStrategy strategy = getOptStrategy();

    if (strategy == OptStrategy::StringSplit) {
        return tryAttachStringSplit();
    }

    MOZ_ASSERT(strategy == OptStrategy::None);
    return false;
}
