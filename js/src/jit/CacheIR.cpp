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
                                       bool* isTemporarilyUnoptimizable,
                                       HandleValue val, HandlePropertyName name,
                                       MutableHandleValue res)
  : cx_(cx),
    pc_(pc),
    val_(val),
    name_(name),
    res_(res),
    engine_(engine),
    isTemporarilyUnoptimizable_(isTemporarilyUnoptimizable),
    emitted_(false),
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
GetPropIRGenerator::tryAttachStub(Maybe<CacheIRWriter>& writer)
{
    AutoAssertNoPendingException aanpe(cx_);
    JS::AutoCheckCannotGC nogc;

    MOZ_ASSERT(!emitted_);

    writer.emplace();
    ValOperandId valId(writer->setInputOperandId(0));

    if (val_.isObject()) {
        RootedObject obj(cx_, &val_.toObject());
        ObjOperandId objId = writer->guardIsObject(valId);

        if (!emitted_ && !tryAttachObjectLength(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachNative(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachUnboxed(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachUnboxedExpando(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachTypedObject(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachModuleNamespace(*writer, obj, objId))
            return false;
        if (!emitted_ && !tryAttachWindowProxy(*writer, obj, objId))
            return false;
        return true;
    }

    if (!emitted_ && !tryAttachPrimitive(*writer, valId))
        return false;

    return true;
}

static bool
IsCacheableNoProperty(JSContext* cx, JSObject* obj, JSObject* holder, Shape* shape, jsid id,
                      jsbytecode* pc)
{
    if (shape)
        return false;

    MOZ_ASSERT(!holder);

    // If we're doing a name lookup, we have to throw a ReferenceError.
    if (*pc == JSOP_GETXPROP)
        return false;

    return CheckHasNoSuchProperty(cx, obj, JSID_TO_ATOM(id)->asPropertyName());
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

    if (IsCacheableGetPropCallNative(obj, holder, shape)) {
        JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
        MOZ_ASSERT(target->isNative());
        writer.callNativeGetterResult(objId, target);
        return;
    }

    MOZ_ASSERT(IsCacheableGetPropCallScripted(obj, holder, shape));

    JSFunction* target = &shape->getterValue().toObject().as<JSFunction>();
    MOZ_ASSERT(target->hasJITCode());
    writer.callScriptedGetterResult(objId, target);
}

bool
GetPropIRGenerator::tryAttachNative(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    RootedShape shape(cx_);
    RootedNativeObject holder(cx_);

    RootedId id(cx_, NameToId(name_));
    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, obj, id, &holder, &shape, pc_,
                                                            engine_, isTemporarilyUnoptimizable_);
    if (type == CanAttachNone)
        return true;

    emitted_ = true;

    switch (type) {
      case CanAttachReadSlot:
        if (holder) {
            EnsureTrackPropertyTypes(cx_, holder, NameToId(name_));
            if (obj == holder) {
                // See the comment in StripPreliminaryObjectStubs.
                if (IsPreliminaryObject(obj))
                    preliminaryObjectAction_ = PreliminaryObjectAction::NotePreliminary;
                else
                    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
            }
        }
        EmitReadSlotResult(writer, obj, holder, shape, objId);
        break;
      case CanAttachCallGetter:
        EmitCallGetterResult(writer, obj, holder, shape, objId);
        break;
      default:
        MOZ_CRASH("Bad NativeGetPropCacheability");
    }

    return true;
}

bool
GetPropIRGenerator::tryAttachWindowProxy(CacheIRWriter& writer, HandleObject obj,
                                         ObjOperandId objId)
{
    // Attach a stub when the receiver is a WindowProxy and we are calling some
    // kinds of JSNative getters on the Window object (the global object).

    MOZ_ASSERT(!emitted_);

    if (!IsWindowProxy(obj))
        return true;

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
    RootedId id(cx_, NameToId(name_));
    NativeGetPropCacheability type = CanAttachNativeGetProp(cx_, windowObj, id, &holder, &shape, pc_,
                                                            engine_, isTemporarilyUnoptimizable_);
    if (type != CanAttachCallGetter ||
        !IsCacheableGetPropCallNative(windowObj, holder, shape))
    {
        return true;
    }

    // Make sure the native getter is okay with the IC passing the Window
    // instead of the WindowProxy as |this| value.
    JSFunction* callee = &shape->getterObject()->as<JSFunction>();
    MOZ_ASSERT(callee->isNative());
    if (!callee->jitInfo() || callee->jitInfo()->needsOuterizedThisObject())
        return true;

    emitted_ = true;

    // Guard the incoming object is a WindowProxy and inline a getter call based
    // on the Window object.
    writer.guardClass(objId, GuardClassKind::WindowProxy);
    ObjOperandId windowObjId = writer.loadObject(windowObj);
    EmitCallGetterResult(writer, windowObj, holder, shape, windowObjId);
    return true;
}

bool
GetPropIRGenerator::tryAttachUnboxed(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    if (!obj->is<UnboxedPlainObject>())
        return true;

    const UnboxedLayout::Property* property = obj->as<UnboxedPlainObject>().layout().lookup(name_);
    if (!property)
        return true;

    if (!cx_->runtime()->jitSupportsFloatingPoint)
        return true;

    writer.guardGroup(objId, obj->group());
    writer.loadUnboxedPropertyResult(objId, property->type,
                                     UnboxedPlainObject::offsetOfData() + property->offset);
    emitted_ = true;
    preliminaryObjectAction_ = PreliminaryObjectAction::Unlink;
    return true;
}

bool
GetPropIRGenerator::tryAttachUnboxedExpando(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    if (!obj->is<UnboxedPlainObject>())
        return true;

    UnboxedExpandoObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
    if (!expando)
        return true;

    Shape* shape = expando->lookup(cx_, NameToId(name_));
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot())
        return true;

    emitted_ = true;

    EmitReadSlotResult(writer, obj, obj, shape, objId);
    return true;
}

bool
GetPropIRGenerator::tryAttachTypedObject(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    if (!obj->is<TypedObject>() ||
        !cx_->runtime()->jitSupportsFloatingPoint ||
        cx_->compartment()->detachedTypedObjects)
    {
        return true;
    }

    TypedObject* typedObj = &obj->as<TypedObject>();
    if (!typedObj->typeDescr().is<StructTypeDescr>())
        return true;

    StructTypeDescr* structDescr = &typedObj->typeDescr().as<StructTypeDescr>();
    size_t fieldIndex;
    if (!structDescr->fieldIndex(NameToId(name_), &fieldIndex))
        return true;

    TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
    if (!fieldDescr->is<SimpleTypeDescr>())
        return true;

    Shape* shape = typedObj->maybeShape();
    TypedThingLayout layout = GetTypedThingLayout(shape->getObjectClass());

    uint32_t fieldOffset = structDescr->fieldOffset(fieldIndex);
    uint32_t typeDescr = SimpleTypeDescrKey(&fieldDescr->as<SimpleTypeDescr>());

    writer.guardNoDetachedTypedObjects();
    writer.guardShape(objId, shape);
    writer.loadTypedObjectResult(objId, fieldOffset, layout, typeDescr);
    emitted_ = true;
    return true;
}

bool
GetPropIRGenerator::tryAttachObjectLength(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    if (name_ != cx_->names().length)
        return true;

    if (obj->is<ArrayObject>()) {
        // Make sure int32 is added to the TypeSet before we attach a stub, so
        // the stub can return int32 values without monitoring the result.
        if (obj->as<ArrayObject>().length() > INT32_MAX)
            return true;

        writer.guardClass(objId, GuardClassKind::Array);
        writer.loadInt32ArrayLengthResult(objId);
        emitted_ = true;
        return true;
    }

    if (obj->is<UnboxedArrayObject>()) {
        writer.guardClass(objId, GuardClassKind::UnboxedArray);
        writer.loadUnboxedArrayLengthResult(objId);
        emitted_ = true;
        return true;
    }

    if (obj->is<ArgumentsObject>() && !obj->as<ArgumentsObject>().hasOverriddenLength()) {
        if (obj->is<MappedArgumentsObject>()) {
            writer.guardClass(objId, GuardClassKind::MappedArguments);
        } else {
            MOZ_ASSERT(obj->is<UnmappedArgumentsObject>());
            writer.guardClass(objId, GuardClassKind::UnmappedArguments);
        }
        writer.loadArgumentsObjectLengthResult(objId);
        emitted_ = true;
        return true;
    }

    return true;
}

bool
GetPropIRGenerator::tryAttachModuleNamespace(CacheIRWriter& writer, HandleObject obj,
                                             ObjOperandId objId)
{
    MOZ_ASSERT(!emitted_);

    if (!obj->is<ModuleNamespaceObject>())
        return true;

    Rooted<ModuleNamespaceObject*> ns(cx_, &obj->as<ModuleNamespaceObject>());
    RootedModuleEnvironmentObject env(cx_);
    RootedShape shape(cx_);
    if (!ns->bindings().lookup(NameToId(name_), env.address(), shape.address()))
        return true;

    // Don't emit a stub until the target binding has been initialized.
    if (env->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL))
        return true;

    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, env, shape->propid());

    emitted_ = true;

    // Check for the specific namespace object.
    writer.guardSpecificObject(objId, ns);

    ObjOperandId envId = writer.loadObject(env);
    EmitLoadSlotResult(writer, envId, env, shape);
    return true;
}

bool
GetPropIRGenerator::tryAttachPrimitive(CacheIRWriter& writer, ValOperandId valId)
{
    MOZ_ASSERT(!emitted_);

    JSValueType primitiveType;
    RootedNativeObject proto(cx_);
    if (val_.isString()) {
        if (name_ == cx_->names().length) {
            // String length is special-cased, see js::GetProperty.
            return true;
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
        return true;
    }
    if (!proto)
        return true;

    // Instantiate this property, for use during Ion compilation.
    RootedId id(cx_, NameToId(name_));
    if (IsIonEnabled(cx_))
        EnsureTrackPropertyTypes(cx_, proto, id);

    // For now, only look for properties directly set on the prototype.
    Shape* shape = proto->lookup(cx_, id);
    if (!shape || !shape->hasSlot() || !shape->hasDefaultGetter())
        return true;

    writer.guardType(valId, primitiveType);

    ObjOperandId protoId = writer.loadObject(proto);
    writer.guardShape(protoId, proto->lastProperty());
    EmitLoadSlotResult(writer, protoId, proto, shape);

    emitted_ = true;
    return true;
}
