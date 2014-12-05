/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobjinlines_h
#define jsobjinlines_h

#include "jsobj.h"

#include "builtin/MapObject.h"
#include "builtin/TypedObject.h"
#include "vm/ArrayObject.h"
#include "vm/DateObject.h"
#include "vm/NumberObject.h"
#include "vm/Probes.h"
#include "vm/ScopeObject.h"
#include "vm/StringObject.h"
#include "vm/TypedArrayCommon.h"

#include "jsatominlines.h"
#include "jscompartmentinlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"

#include "gc/ForkJoinNursery-inl.h"

/* static */ inline bool
JSObject::setGenericAttributes(JSContext *cx, js::HandleObject obj,
                               js::HandleId id, unsigned *attrsp)
{
    js::types::MarkTypePropertyNonData(cx, obj, id);
    js::GenericAttributesOp op = obj->getOps()->setGenericAttributes;
    if (op)
        return op(cx, obj, id, attrsp);
    return js::baseops::SetAttributes(cx, obj.as<js::NativeObject>(), id, attrsp);
}

/* static */ inline bool
JSObject::deleteGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                         bool *succeeded)
{
    js::types::MarkTypePropertyNonData(cx, obj, id);
    js::DeleteGenericOp op = obj->getOps()->deleteGeneric;
    if (op)
        return op(cx, obj, id, succeeded);
    return js::baseops::DeleteGeneric(cx, obj.as<js::NativeObject>(), id, succeeded);
}

/* static */ inline bool
JSObject::deleteElement(JSContext *cx, js::HandleObject obj, uint32_t index, bool *succeeded)
{
    JS::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    return deleteGeneric(cx, obj, id, succeeded);
}

/* static */ inline bool
JSObject::watch(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                JS::HandleObject callable)
{
    js::WatchOp op = obj->getOps()->watch;
    return (op ? op : js::baseops::Watch)(cx, obj, id, callable);
}

/* static */ inline bool
JSObject::unwatch(JSContext *cx, JS::HandleObject obj, JS::HandleId id)
{
    js::UnwatchOp op = obj->getOps()->unwatch;
    return (op ? op : js::baseops::Unwatch)(cx, obj, id);
}

inline void
JSObject::finalize(js::FreeOp *fop)
{
    js::probes::FinalizeObject(this);

#ifdef DEBUG
    MOZ_ASSERT(isTenured());
    if (!IsBackgroundFinalized(asTenured().getAllocKind())) {
        /* Assert we're on the main thread. */
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(fop->runtime()));
    }
#endif
    const js::Class *clasp = getClass();
    if (clasp->finalize)
        clasp->finalize(fop, this);

    if (!clasp->isNative())
        return;

    js::NativeObject *nobj = &as<js::NativeObject>();

    if (nobj->hasDynamicSlots())
        fop->free_(nobj->slots_);

    if (nobj->hasDynamicElements()) {
        js::ObjectElements *elements = nobj->getElementsHeader();
        if (elements->isCopyOnWrite()) {
            if (elements->ownerObject() == this) {
                // Don't free the elements until object finalization finishes,
                // so that other objects can access these elements while they
                // are themselves finalized.
                fop->freeLater(elements);
            }
        } else {
            fop->free_(elements);
        }
    }

    // For dictionary objects (which must be native), it's possible that
    // unreachable shapes may be marked whose listp points into this object.
    // In case this happens, null out the shape's pointer here so that a moving
    // GC will not try to access the dead object.
    if (shape_->listp == &shape_)
        shape_->listp = nullptr;
}

/* static */ inline bool
JSObject::setSingletonType(js::ExclusiveContext *cx, js::HandleObject obj)
{
    MOZ_ASSERT_IF(cx->isJSContext(), !IsInsideNursery(obj));

    js::types::TypeObject *type = cx->getSingletonType(obj->getClass(), obj->getTaggedProto());
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline js::types::TypeObject*
JSObject::getType(JSContext *cx)
{
    MOZ_ASSERT(cx->compartment() == compartment());
    if (hasLazyType()) {
        JS::RootedObject self(cx, this);
        if (cx->compartment() != compartment())
            MOZ_CRASH();
        return makeLazyType(cx, self);
    }
    return static_cast<js::types::TypeObject*>(type_);
}

/* static */ inline bool
JSObject::clearType(JSContext *cx, js::HandleObject obj)
{
    MOZ_ASSERT(!obj->hasSingletonType());
    MOZ_ASSERT(cx->compartment() == obj->compartment());

    js::types::TypeObject *type = cx->getNewType(obj->getClass(), js::TaggedProto(nullptr));
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
    MOZ_ASSERT(newType);
    MOZ_ASSERT(!hasSingletonType());
    type_ = newType;
}

/* static */ inline bool
JSObject::getProto(JSContext *cx, js::HandleObject obj, js::MutableHandleObject protop)
{
    if (obj->getTaggedProto().isLazy()) {
        MOZ_ASSERT(obj->is<js::ProxyObject>());
        return js::Proxy::getPrototypeOf(cx, obj, protop);
    } else {
        protop.set(obj->getTaggedProto().toObjectOrNull());
        return true;
    }
}

/* static */ inline bool
JSObject::setProto(JSContext *cx, JS::HandleObject obj, JS::HandleObject proto, bool *succeeded)
{
    /*
     * If |obj| has a "lazy" [[Prototype]], it is 1) a proxy 2) whose handler's
     * {get,set}PrototypeOf and setImmutablePrototype methods mediate access to
     * |obj.[[Prototype]]|.  The Proxy subsystem is responsible for responding
     * to such attempts.
     */
    if (obj->hasLazyPrototype()) {
        MOZ_ASSERT(obj->is<js::ProxyObject>());
        return js::Proxy::setPrototypeOf(cx, obj, proto, succeeded);
    }

    /* Disallow mutation of immutable [[Prototype]]s. */
    if (obj->nonLazyPrototypeIsImmutable()) {
        *succeeded = false;
        return true;
    }

    /*
     * Disallow mutating the [[Prototype]] on ArrayBuffer objects, which
     * due to their complicated delegate-object shenanigans can't easily
     * have a mutable [[Prototype]].
     */
    if (obj->is<js::ArrayBufferObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SETPROTOTYPEOF_FAIL,
                             "incompatible ArrayBuffer");
        return false;
    }

    /*
     * Disallow mutating the [[Prototype]] on Typed Objects, per the spec.
     */
    if (obj->is<js::TypedObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SETPROTOTYPEOF_FAIL,
                             "incompatible TypedObject");
        return false;
    }

    /*
     * Explicitly disallow mutating the [[Prototype]] of Location objects
     * for flash-related security reasons.
     */
    if (!strcmp(obj->getClass()->name, "Location")) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SETPROTOTYPEOF_FAIL,
                             "incompatible Location object");
        return false;
    }

    /* ES6 9.1.2 step 5 forbids changing [[Prototype]] if not [[Extensible]]. */
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (!extensible) {
        *succeeded = false;
        return true;
    }

    /* ES6 9.1.2 step 6 forbids generating cyclical prototype chains. */
    js::RootedObject obj2(cx);
    for (obj2 = proto; obj2; ) {
        if (obj2 == obj) {
            *succeeded = false;
            return true;
        }

        if (!JSObject::getProto(cx, obj2, &obj2))
            return false;
    }

    JS::Rooted<js::TaggedProto> taggedProto(cx, js::TaggedProto(proto));
    *succeeded = SetClassAndProto(cx, obj, obj->getClass(), taggedProto);
    return *succeeded;
}

/* static */ inline bool
JSObject::isExtensible(js::ExclusiveContext *cx, js::HandleObject obj, bool *extensible)
{
    if (obj->is<js::ProxyObject>()) {
        if (!cx->shouldBeJSContext())
            return false;
        return js::Proxy::isExtensible(cx->asJSContext(), obj, extensible);
    }

    *extensible = obj->nonProxyIsExtensible();
    return true;
}

inline bool
JSObject::isQualifiedVarObj()
{
    if (is<js::DebugScopeObject>())
        return as<js::DebugScopeObject>().scope().isQualifiedVarObj();
    return lastProperty()->hasObjectFlag(js::BaseShape::QUALIFIED_VAROBJ);
}

inline bool
JSObject::isUnqualifiedVarObj()
{
    if (is<js::DebugScopeObject>())
        return as<js::DebugScopeObject>().scope().isUnqualifiedVarObj();
    return lastProperty()->hasObjectFlag(js::BaseShape::UNQUALIFIED_VAROBJ);
}

namespace js {

inline bool
ClassCanHaveFixedData(const Class *clasp)
{
    // Normally, the number of fixed slots given an object is the maximum
    // permitted for its size class. For array buffers and non-shared typed
    // arrays we only use enough to cover the class reserved slots, so that
    // the remaining space in the object's allocation is available for the
    // buffer's data.
    return !clasp->isNative()
        || clasp == &js::ArrayBufferObject::class_
        || js::IsTypedArrayClass(clasp);
}

} // namespace js

/* static */ inline JSObject *
JSObject::create(js::ExclusiveContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                 js::HandleShape shape, js::HandleTypeObject type)
{
    MOZ_ASSERT(shape && type);
    MOZ_ASSERT(type->clasp() == shape->getObjectClass());
    MOZ_ASSERT(type->clasp() != &js::ArrayObject::class_);
    MOZ_ASSERT_IF(!js::ClassCanHaveFixedData(type->clasp()),
                  js::gc::GetGCKindSlots(kind, type->clasp()) == shape->numFixedSlots());
    MOZ_ASSERT_IF(type->clasp()->flags & JSCLASS_BACKGROUND_FINALIZE, IsBackgroundFinalized(kind));
    MOZ_ASSERT_IF(type->clasp()->finalize, heap == js::gc::TenuredHeap);

    // Non-native classes cannot have reserved slots or private data, and the
    // objects can't have any fixed slots, for compatibility with
    // GetReservedOrProxyPrivateSlot.
    MOZ_ASSERT_IF(!type->clasp()->isNative(), JSCLASS_RESERVED_SLOTS(type->clasp()) == 0);
    MOZ_ASSERT_IF(!type->clasp()->isNative(), !type->clasp()->hasPrivate());
    MOZ_ASSERT_IF(!type->clasp()->isNative(), shape->numFixedSlots() == 0);
    MOZ_ASSERT_IF(!type->clasp()->isNative(), shape->slotSpan() == 0);

    const js::Class *clasp = type->clasp();
    size_t nDynamicSlots =
        js::NativeObject::dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(), clasp);

    JSObject *obj = js::NewGCObject<js::CanGC>(cx, kind, nDynamicSlots, heap);
    if (!obj)
        return nullptr;

    obj->shape_.init(shape);
    obj->type_.init(type);

    // Note: slots are created and assigned internally by NewGCObject.
    obj->setInitialElementsMaybeNonNative(js::emptyObjectElements);

    if (clasp->hasPrivate())
        obj->as<js::NativeObject>().privateRef(shape->numFixedSlots()) = nullptr;

    if (size_t span = shape->slotSpan())
        obj->as<js::NativeObject>().initializeSlotRange(0, span);

    // JSFunction's fixed slots expect POD-style initialization.
    if (type->clasp()->isJSFunction())
        memset(obj->as<JSFunction>().fixedSlots(), 0, sizeof(js::HeapSlot) * GetGCKindSlots(kind));

    js::gc::TraceCreateObject(obj);

    return obj;
}

inline void
JSObject::setInitialSlotsMaybeNonNative(js::HeapSlot *slots)
{
    static_cast<js::NativeObject *>(this)->slots_ = slots;
}

inline void
JSObject::setInitialElementsMaybeNonNative(js::HeapSlot *elements)
{
    static_cast<js::NativeObject *>(this)->elements_ = elements;
}

/* static */ inline bool
JSObject::hasProperty(JSContext *cx, js::HandleObject obj,
                      js::HandleId id, bool *foundp)
{
    JS::RootedObject pobj(cx);
    js::RootedShape prop(cx);
    if (!lookupGeneric(cx, obj, id, &pobj, &prop)) {
        *foundp = false;  /* initialize to shut GCC up */
        return false;
    }
    *foundp = !!prop;
    return true;
}

/* static */ inline bool
JSObject::getElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     uint32_t index, js::MutableHandleValue vp)
{
    js::ElementIdOp op = obj->getOps()->getElement;
    if (op)
        return op(cx, obj, receiver, index, vp);

    JS::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGeneric(cx, obj, receiver, id, vp);
}

/* static */ inline bool
JSObject::getElementNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                         uint32_t index, js::Value *vp)
{
    js::ElementIdOp op = obj->getOps()->getElement;
    if (op)
        return false;

    if (index > JSID_INT_MAX)
        return false;
    return getGenericNoGC(cx, obj, receiver, INT_TO_JSID(index), vp);
}

inline js::GlobalObject &
JSObject::global() const
{
#ifdef DEBUG
    JSObject *obj = const_cast<JSObject *>(this);
    while (JSObject *parent = obj->getParent())
        obj = parent;
#endif
    /*
     * The global is read-barriered so that it is kept live by access through
     * the JSCompartment. When accessed through a JSObject, however, the global
     * will be already be kept live by the black JSObject's parent pointer, so
     * does not need to be read-barriered.
     */
    return *compartment()->unsafeUnbarrieredMaybeGlobal();
}

inline bool
JSObject::isOwnGlobal() const
{
    return &global() == this;
}

namespace js {

PropDesc::PropDesc(const Value &getter, const Value &setter,
                   Enumerability enumerable, Configurability configurable)
  : value_(UndefinedValue()),
    get_(getter), set_(setter),
    attrs(JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED |
          (enumerable ? JSPROP_ENUMERATE : 0) |
          (configurable ? 0 : JSPROP_PERMANENT)),
    hasGet_(true), hasSet_(true),
    hasValue_(false), hasWritable_(false), hasEnumerable_(true), hasConfigurable_(true),
    isUndefined_(false)
{
    MOZ_ASSERT(getter.isUndefined() || IsCallable(getter));
    MOZ_ASSERT(setter.isUndefined() || IsCallable(setter));
}

static MOZ_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v)
{
    return v.isObject() && v.toObject().is<JSFunction>();
}

static MOZ_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSFunction **fun)
{
    if (v.isObject() && v.toObject().is<JSFunction>()) {
        *fun = &v.toObject().as<JSFunction>();
        return true;
    }
    return false;
}

static MOZ_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->isNative();
}

static MOZ_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSFunction **fun)
{
    return IsFunctionObject(v, fun) && (*fun)->isNative();
}

static MOZ_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSNative native)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->maybeNative() == native;
}

/*
 * When we have an object of a builtin class, we don't quite know what its
 * valueOf/toString methods are, since these methods may have been overwritten
 * or shadowed. However, we can still do better than the general case by
 * hard-coding the necessary properties for us to find the native we expect.
 *
 * TODO: a per-thread shape-based cache would be faster and simpler.
 */
static MOZ_ALWAYS_INLINE bool
ClassMethodIsNative(JSContext *cx, NativeObject *obj, const Class *clasp, jsid methodid, JSNative native)
{
    MOZ_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        JSObject *proto = obj->getProto();
        if (!proto || proto->getClass() != clasp || !HasDataProperty(cx, &proto->as<NativeObject>(), methodid, &v))
            return false;
    }

    return IsNativeFunction(v, native);
}

// Return whether looking up 'valueOf' on 'obj' definitely resolves to the
// original Object.prototype.valueOf. The method may conservatively return
// 'false' in the case of proxies or other non-native objects.
static MOZ_ALWAYS_INLINE bool
HasObjectValueOf(JSObject *obj, JSContext *cx)
{
    if (obj->is<ProxyObject>() || !obj->isNative())
        return false;

    jsid valueOf = NameToId(cx->names().valueOf);

    Value v;
    while (!HasDataProperty(cx, &obj->as<NativeObject>(), valueOf, &v)) {
        obj = obj->getProto();
        if (!obj || obj->is<ProxyObject>() || !obj->isNative())
            return false;
    }

    return IsNativeFunction(v, obj_valueOf);
}

/* ES5 9.1 ToPrimitive(input). */
static MOZ_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, MutableHandleValue vp)
{
    if (vp.isPrimitive())
        return true;

    JSObject *obj = &vp.toObject();

    /* Optimize new String(...).valueOf(). */
    if (obj->is<StringObject>()) {
        jsid id = NameToId(cx->names().valueOf);
        StringObject *nobj = &obj->as<StringObject>();
        if (ClassMethodIsNative(cx, nobj, &StringObject::class_, id, js_str_toString)) {
            vp.setString(nobj->unbox());
            return true;
        }
    }

    /* Optimize new Number(...).valueOf(). */
    if (obj->is<NumberObject>()) {
        jsid id = NameToId(cx->names().valueOf);
        NumberObject *nobj = &obj->as<NumberObject>();
        if (ClassMethodIsNative(cx, nobj, &NumberObject::class_, id, js_num_valueOf)) {
            vp.setNumber(nobj->unbox());
            return true;
        }
    }

    RootedObject objRoot(cx, obj);
    return JSObject::defaultValue(cx, objRoot, JSTYPE_VOID, vp);
}

/* ES5 9.1 ToPrimitive(input, PreferredType). */
static MOZ_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, JSType preferredType, MutableHandleValue vp)
{
    MOZ_ASSERT(preferredType != JSTYPE_VOID); /* Use the other ToPrimitive! */
    if (vp.isPrimitive())
        return true;
    RootedObject obj(cx, &vp.toObject());
    return JSObject::defaultValue(cx, obj, preferredType, vp);
}

/*
 * Return true if this is a compiler-created internal function accessed by
 * its own object. Such a function object must not be accessible to script
 * or embedding code.
 */
inline bool
IsInternalFunctionObject(JSObject *funobj)
{
    JSFunction *fun = &funobj->as<JSFunction>();
    return fun->isLambda() && !funobj->getParent();
}

class AutoPropDescVector : public AutoVectorRooter<PropDesc>
{
  public:
    explicit AutoPropDescVector(JSContext *cx
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<PropDesc>(cx, DESCVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Make an object with the specified prototype. If parent is null, it will
 * default to the prototype's global if the prototype is non-null.
 */
JSObject *
NewObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp, TaggedProto proto, JSObject *parent,
                        gc::AllocKind allocKind, NewObjectKind newKind);

inline JSObject *
NewObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp, TaggedProto proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewObjectWithGivenProto(cx, clasp, proto, parent, allocKind, newKind);
}

inline JSObject *
NewObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    return NewObjectWithGivenProto(cx, clasp, TaggedProto(proto), parent, newKind);
}

inline bool
FindProto(ExclusiveContext *cx, const js::Class *clasp, MutableHandleObject proto)
{
    if (!FindClassPrototype(cx, proto, clasp))
        return false;

    if (!proto) {
        // We're looking for the prototype of a class that is currently being
        // resolved; the global object's resolve hook is on the
        // stack. js::FindClassPrototype detects this goofy case and returns
        // true with proto null. Fall back on Object.prototype.
        MOZ_ASSERT(JSCLASS_CACHED_PROTO_KEY(clasp) == JSProto_Null);
        return GetBuiltinPrototype(cx, JSProto_Object, proto);
    }
    return true;
}

/*
 * Make an object with the prototype set according to the specified prototype or class:
 *
 * if proto is non-null:
 *   use the specified proto
 * for a built-in class:
 *   use the memoized original value of the class constructor .prototype
 *   property object
 * else if available
 *   the current value of .prototype
 * else
 *   Object.prototype.
 *
 * The class prototype will be fetched from the parent's global. If global is
 * null, the context's active global will be used, and the resulting object's
 * parent will be that global.
 */
JSObject *
NewObjectWithClassProtoCommon(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                              gc::AllocKind allocKind, NewObjectKind newKind);

inline JSObject *
NewObjectWithClassProto(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                        gc::AllocKind allocKind, NewObjectKind newKind = GenericObject)
{
    return NewObjectWithClassProtoCommon(cx, clasp, proto, parent, allocKind, newKind);
}

inline JSObject *
NewObjectWithClassProto(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewObjectWithClassProto(cx, clasp, proto, parent, allocKind, newKind);
}

template<typename T>
inline T *
NewObjectWithProto(ExclusiveContext *cx, JSObject *proto, JSObject *parent,
                   NewObjectKind newKind = GenericObject)
{
    JSObject *obj = NewObjectWithClassProto(cx, &T::class_, proto, parent, newKind);
    if (!obj)
        return nullptr;

    return &obj->as<T>();
}

/*
 * Create a native instance of the given class with parent and proto set
 * according to the context's active global.
 */
inline JSObject *
NewBuiltinClassInstance(ExclusiveContext *cx, const Class *clasp, gc::AllocKind allocKind,
                        NewObjectKind newKind = GenericObject)
{
    return NewObjectWithClassProto(cx, clasp, nullptr, nullptr, allocKind, newKind);
}

inline JSObject *
NewBuiltinClassInstance(ExclusiveContext *cx, const Class *clasp, NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewBuiltinClassInstance(cx, clasp, allocKind, newKind);
}

template<typename T>
inline T *
NewBuiltinClassInstance(ExclusiveContext *cx, NewObjectKind newKind = GenericObject)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &T::class_, newKind);
    if (!obj)
        return nullptr;

    return &obj->as<T>();
}

template<typename T>
inline T *
NewBuiltinClassInstance(ExclusiveContext *cx, gc::AllocKind allocKind, NewObjectKind newKind = GenericObject)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &T::class_, allocKind, newKind);
    if (!obj)
        return nullptr;

    return &obj->as<T>();
}

// Used to optimize calls to (new Object())
bool
NewObjectScriptedCall(JSContext *cx, MutableHandleObject obj);

JSObject *
NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind allocKind,
                  NewObjectKind newKind = GenericObject);

inline JSObject *
NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent,
                  NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(type->clasp());
    return NewObjectWithType(cx, type, parent, allocKind, newKind);
}

JSObject *
NewReshapedObject(JSContext *cx, HandleTypeObject type, JSObject *parent,
                  gc::AllocKind allocKind, HandleShape shape,
                  NewObjectKind newKind = GenericObject);

/*
 * As for gc::GetGCObjectKind, where numSlots is a guess at the final size of
 * the object, zero if the final size is unknown. This should only be used for
 * objects that do not require any fixed slots.
 */
static inline gc::AllocKind
GuessObjectGCKind(size_t numSlots)
{
    if (numSlots)
        return gc::GetGCObjectKind(numSlots);
    return gc::FINALIZE_OBJECT4;
}

static inline gc::AllocKind
GuessArrayGCKind(size_t numSlots)
{
    if (numSlots)
        return gc::GetGCArrayKind(numSlots);
    return gc::FINALIZE_OBJECT8;
}

inline bool
ObjectClassIs(HandleObject obj, ESClassValue classValue, JSContext *cx)
{
    if (MOZ_UNLIKELY(obj->is<ProxyObject>()))
        return Proxy::objectClassIs(obj, classValue, cx);

    switch (classValue) {
      case ESClass_Object: return obj->is<JSObject>();
      case ESClass_Array: return obj->is<ArrayObject>();
      case ESClass_Number: return obj->is<NumberObject>();
      case ESClass_String: return obj->is<StringObject>();
      case ESClass_Boolean: return obj->is<BooleanObject>();
      case ESClass_RegExp: return obj->is<RegExpObject>();
      case ESClass_ArrayBuffer: return obj->is<ArrayBufferObject>();
      case ESClass_SharedArrayBuffer: return obj->is<SharedArrayBufferObject>();
      case ESClass_Date: return obj->is<DateObject>();
      case ESClass_Set: return obj->is<SetObject>();
      case ESClass_Map: return obj->is<MapObject>();
    }
    MOZ_CRASH("bad classValue");
}

inline bool
IsObjectWithClass(const Value &v, ESClassValue classValue, JSContext *cx)
{
    if (!v.isObject())
        return false;
    RootedObject obj(cx, &v.toObject());
    return ObjectClassIs(obj, classValue, cx);
}

inline bool
Unbox(JSContext *cx, HandleObject obj, MutableHandleValue vp)
{
    if (MOZ_UNLIKELY(obj->is<ProxyObject>()))
        return Proxy::boxedValue_unbox(cx, obj, vp);

    if (obj->is<BooleanObject>())
        vp.setBoolean(obj->as<BooleanObject>().unbox());
    else if (obj->is<NumberObject>())
        vp.setNumber(obj->as<NumberObject>().unbox());
    else if (obj->is<StringObject>())
        vp.setString(obj->as<StringObject>().unbox());
    else if (obj->is<DateObject>())
        vp.set(obj->as<DateObject>().UTCTime());
    else
        vp.setUndefined();

    return true;
}

static MOZ_ALWAYS_INLINE bool
NewObjectMetadata(ExclusiveContext *cxArg, JSObject **pmetadata)
{
    // The metadata callback is invoked before each created object, except when
    // analysis/compilation is active, to avoid recursion.
    MOZ_ASSERT(!*pmetadata);
    if (JSContext *cx = cxArg->maybeJSContext()) {
        if (MOZ_UNLIKELY((size_t)cx->compartment()->hasObjectMetadataCallback()) &&
            !cx->zone()->types.activeAnalysis)
        {
            // Use AutoEnterAnalysis to prohibit both any GC activity under the
            // callback, and any reentering of JS via Invoke() etc.
            types::AutoEnterAnalysis enter(cx);

            if (!cx->compartment()->callObjectMetadataCallback(cx, pmetadata))
                return false;
        }
    }
    return true;
}

namespace baseops {

inline bool
LookupProperty(ExclusiveContext *cx, HandleNativeObject obj, PropertyName *name,
               MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return LookupProperty<CanGC>(cx, obj, id, objp, propp);
}

inline bool
DefineProperty(ExclusiveContext *cx, HandleNativeObject obj, PropertyName *name, HandleValue value,
               JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

} /* namespace baseops */

static inline unsigned
ApplyAttributes(unsigned attrs, bool enumerable, bool writable, bool configurable)
{
    /*
     * Respect the fact that some callers may want to preserve existing attributes as much as
     * possible, or install defaults otherwise.
     */
    if (attrs & JSPROP_IGNORE_ENUMERATE) {
        attrs &= ~JSPROP_IGNORE_ENUMERATE;
        if (enumerable)
            attrs |= JSPROP_ENUMERATE;
        else
            attrs &= ~JSPROP_ENUMERATE;
    }
    if (attrs & JSPROP_IGNORE_READONLY) {
        attrs &= ~JSPROP_IGNORE_READONLY;
        // Only update the writability if it's relevant
        if (!(attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
            if (!writable)
                attrs |= JSPROP_READONLY;
            else
                attrs &= ~JSPROP_READONLY;
        }
    }
    if (attrs & JSPROP_IGNORE_PERMANENT) {
        attrs &= ~JSPROP_IGNORE_PERMANENT;
        if (!configurable)
            attrs |= JSPROP_PERMANENT;
        else
            attrs &= ~JSPROP_PERMANENT;
    }
    return attrs;
}

} /* namespace js */

extern js::NativeObject *
js_InitClass(JSContext *cx, js::HandleObject obj, JSObject *parent_proto,
             const js::Class *clasp, JSNative constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
             js::NativeObject **ctorp = nullptr,
             js::gc::AllocKind ctorKind = JSFunction::FinalizeKind);

#endif /* jsobjinlines_h */
