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

#include "jsatominlines.h"
#include "jscompartmentinlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"

#include "gc/ForkJoinNursery-inl.h"
#include "vm/ObjectImpl-inl.h"

/* static */ inline bool
JSObject::setGenericAttributes(JSContext *cx, js::HandleObject obj,
                               js::HandleId id, unsigned *attrsp)
{
    js::types::MarkTypePropertyNonData(cx, obj, id);
    js::GenericAttributesOp op = obj->getOps()->setGenericAttributes;
    return (op ? op : js::baseops::SetAttributes)(cx, obj, id, attrsp);
}

/* static */ inline bool
JSObject::changePropertyAttributes(JSContext *cx, js::HandleObject obj,
                                   js::HandleShape shape, unsigned attrs)
{
    return !!changeProperty<js::SequentialExecution>(cx, obj, shape, attrs, 0,
                                                     shape->getter(), shape->setter());
}

/* static */ inline bool
JSObject::deleteGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                         bool *succeeded)
{
    js::types::MarkTypePropertyNonData(cx, obj, id);
    js::DeleteGenericOp op = obj->getOps()->deleteGeneric;
    return (op ? op : js::baseops::DeleteGeneric)(cx, obj, id, succeeded);
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
    JS_ASSERT(isTenured());
    if (!IsBackgroundFinalized(tenuredGetAllocKind())) {
        /* Assert we're on the main thread. */
        JS_ASSERT(CurrentThreadCanAccessRuntime(fop->runtime()));
    }
#endif
    const js::Class *clasp = getClass();
    if (clasp->finalize)
        clasp->finalize(fop, this);

    finish(fop);
}

inline void
JSObject::setLastPropertyInfallible(js::Shape *shape)
{
    JS_ASSERT(!shape->inDictionary());
    JS_ASSERT(shape->compartment() == compartment());
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(slotSpan() == shape->slotSpan());
    JS_ASSERT(numFixedSlots() == shape->numFixedSlots());

    shape_ = shape;
}

inline void
JSObject::removeLastProperty(js::ExclusiveContext *cx)
{
    JS_ASSERT(canRemoveLastProperty());
    JS::RootedObject self(cx, this);
    js::RootedShape prev(cx, lastProperty()->previous());
    JS_ALWAYS_TRUE(setLastProperty(cx, self, prev));
}

inline bool
JSObject::canRemoveLastProperty()
{
    /*
     * Check that the information about the object stored in the last
     * property's base shape is consistent with that stored in the previous
     * shape. If not consistent, then the last property cannot be removed as it
     * will induce a change in the object itself, and the object must be
     * converted to dictionary mode instead. See BaseShape comment in jsscope.h
     */
    JS_ASSERT(!inDictionaryMode());
    js::Shape *previous = lastProperty()->previous().get();
    return previous->getObjectParent() == lastProperty()->getObjectParent()
        && previous->getObjectMetadata() == lastProperty()->getObjectMetadata()
        && previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline void
JSObject::setShouldConvertDoubleElements()
{
    JS_ASSERT(is<js::ArrayObject>() && !hasEmptyElements());
    getElementsHeader()->setShouldConvertDoubleElements();
}

inline void
JSObject::clearShouldConvertDoubleElements()
{
    JS_ASSERT(is<js::ArrayObject>() && !hasEmptyElements());
    getElementsHeader()->clearShouldConvertDoubleElements();
}

inline bool
JSObject::setDenseElementIfHasType(uint32_t index, const js::Value &val)
{
    if (!js::types::HasTypePropertyId(this, JSID_VOID, val))
        return false;
    setDenseElementMaybeConvertDouble(index, val);
    return true;
}

inline void
JSObject::setDenseElementWithType(js::ExclusiveContext *cx, uint32_t index,
                                  const js::Value &val)
{
    // Avoid a slow AddTypePropertyId call if the type is the same as the type
    // of the previous element.
    js::types::Type thisType = js::types::GetValueType(val);
    if (index == 0 || js::types::GetValueType(elements[index - 1]) != thisType)
        js::types::AddTypePropertyId(cx, this, JSID_VOID, thisType);
    setDenseElementMaybeConvertDouble(index, val);
}

inline void
JSObject::initDenseElementWithType(js::ExclusiveContext *cx, uint32_t index,
                                   const js::Value &val)
{
    JS_ASSERT(!shouldConvertDoubleElements());
    js::types::AddTypePropertyId(cx, this, JSID_VOID, val);
    initDenseElement(index, val);
}

inline void
JSObject::setDenseElementHole(js::ExclusiveContext *cx, uint32_t index)
{
    js::types::MarkTypeObjectFlags(cx, this, js::types::OBJECT_FLAG_NON_PACKED);
    setDenseElement(index, js::MagicValue(JS_ELEMENTS_HOLE));
}

/* static */ inline void
JSObject::removeDenseElementForSparseIndex(js::ExclusiveContext *cx,
                                           js::HandleObject obj, uint32_t index)
{
    js::types::MarkTypeObjectFlags(cx, obj,
                                   js::types::OBJECT_FLAG_NON_PACKED |
                                   js::types::OBJECT_FLAG_SPARSE_INDEXES);
    if (obj->containsDenseElement(index))
        obj->setDenseElement(index, js::MagicValue(JS_ELEMENTS_HOLE));
}

inline bool
JSObject::writeToIndexWouldMarkNotPacked(uint32_t index)
{
    return getElementsHeader()->initializedLength < index;
}

inline void
JSObject::markDenseElementsNotPacked(js::ExclusiveContext *cx)
{
    JS_ASSERT(isNative());
    MarkTypeObjectFlags(cx, this, js::types::OBJECT_FLAG_NON_PACKED);
}

inline void
JSObject::ensureDenseInitializedLengthNoPackedCheck(js::ThreadSafeContext *cx, uint32_t index,
                                                    uint32_t extra)
{
    JS_ASSERT(cx->isThreadLocal(this));
    JS_ASSERT(!denseElementsAreCopyOnWrite());

    /*
     * Ensure that the array's contents have been initialized up to index, and
     * mark the elements through 'index + extra' as initialized in preparation
     * for a write.
     */
    JS_ASSERT(index + extra <= getDenseCapacity());
    uint32_t &initlen = getElementsHeader()->initializedLength;

    if (initlen < index + extra) {
        size_t offset = initlen;
        for (js::HeapSlot *sp = elements + initlen;
             sp != elements + (index + extra);
             sp++, offset++)
        {
            sp->init(this, js::HeapSlot::Element, offset, js::MagicValue(JS_ELEMENTS_HOLE));
        }
        initlen = index + extra;
    }
}

inline void
JSObject::ensureDenseInitializedLength(js::ExclusiveContext *cx, uint32_t index, uint32_t extra)
{
    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);
    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
}

inline void
JSObject::ensureDenseInitializedLengthPreservePackedFlag(js::ThreadSafeContext *cx,
                                                         uint32_t index, uint32_t extra)
{
    JS_ASSERT(!writeToIndexWouldMarkNotPacked(index));
    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
}

JSObject::EnsureDenseResult
JSObject::extendDenseElements(js::ThreadSafeContext *cx,
                              uint32_t requiredCapacity, uint32_t extra)
{
    JS_ASSERT(cx->isThreadLocal(this));
    JS_ASSERT(!denseElementsAreCopyOnWrite());

    /*
     * Don't grow elements for non-extensible objects or watched objects. Dense
     * elements can be added/written with no extensible or watchpoint checks as
     * long as there is capacity for them.
     */
    if (!nonProxyIsExtensible() || watched()) {
        JS_ASSERT(getDenseCapacity() == 0);
        return ED_SPARSE;
    }

    /*
     * Don't grow elements for objects which already have sparse indexes.
     * This avoids needing to count non-hole elements in willBeSparseElements
     * every time a new index is added.
     */
    if (isIndexed())
        return ED_SPARSE;

    /*
     * We use the extra argument also as a hint about number of non-hole
     * elements to be inserted.
     */
    if (requiredCapacity > MIN_SPARSE_INDEX &&
        willBeSparseElements(requiredCapacity, extra)) {
        return ED_SPARSE;
    }

    if (!growElements(cx, requiredCapacity))
        return ED_FAILED;

    return ED_OK;
}

inline JSObject::EnsureDenseResult
JSObject::ensureDenseElementsNoPackedCheck(js::ThreadSafeContext *cx, uint32_t index, uint32_t extra)
{
    JS_ASSERT(isNative());

    if (!maybeCopyElementsForWrite(cx))
        return ED_FAILED;

    uint32_t currentCapacity = getDenseCapacity();

    uint32_t requiredCapacity;
    if (extra == 1) {
        /* Optimize for the common case. */
        if (index < currentCapacity) {
            ensureDenseInitializedLengthNoPackedCheck(cx, index, 1);
            return ED_OK;
        }
        requiredCapacity = index + 1;
        if (requiredCapacity == 0) {
            /* Overflow. */
            return ED_SPARSE;
        }
    } else {
        requiredCapacity = index + extra;
        if (requiredCapacity < index) {
            /* Overflow. */
            return ED_SPARSE;
        }
        if (requiredCapacity <= currentCapacity) {
            ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
            return ED_OK;
        }
    }

    EnsureDenseResult edr = extendDenseElements(cx, requiredCapacity, extra);
    if (edr != ED_OK)
        return edr;

    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
    return ED_OK;
}

inline JSObject::EnsureDenseResult
JSObject::ensureDenseElements(js::ExclusiveContext *cx, uint32_t index, uint32_t extra)
{
    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);
    return ensureDenseElementsNoPackedCheck(cx, index, extra);
}

inline JSObject::EnsureDenseResult
JSObject::ensureDenseElementsPreservePackedFlag(js::ThreadSafeContext *cx, uint32_t index,
                                                uint32_t extra)
{
    JS_ASSERT(!writeToIndexWouldMarkNotPacked(index));
    return ensureDenseElementsNoPackedCheck(cx, index, extra);
}

inline js::Value
JSObject::getDenseOrTypedArrayElement(uint32_t idx)
{
    if (is<js::TypedArrayObject>())
        return as<js::TypedArrayObject>().getElement(idx);
    return getDenseElement(idx);
}

inline void
JSObject::initDenseElementsUnbarriered(uint32_t dstStart, const js::Value *src, uint32_t count) {
    /*
     * For use by parallel threads, which since they cannot see nursery
     * things do not require a barrier.
     */
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JS_ASSERT(!denseElementsAreCopyOnWrite());
#if defined(DEBUG) && defined(JSGC_GENERATIONAL)
    /*
     * This asserts a global invariant: parallel code does not
     * observe objects inside the generational GC's nursery.
     */
    JS_ASSERT(!js::gc::IsInsideGGCNursery(this));
    for (uint32_t index = 0; index < count; ++index) {
        const JS::Value& value = src[index];
        if (value.isMarkable())
            JS_ASSERT(!js::gc::IsInsideGGCNursery(static_cast<js::gc::Cell *>(value.toGCThing())));
    }
#endif
    memcpy(&elements[dstStart], src, count * sizeof(js::HeapSlot));
}

/* static */ inline bool
JSObject::setSingletonType(js::ExclusiveContext *cx, js::HandleObject obj)
{
    JS_ASSERT_IF(cx->isJSContext(), !IsInsideNursery(obj));

    js::types::TypeObject *type = cx->getSingletonType(obj->getClass(), obj->getTaggedProto());
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline js::types::TypeObject*
JSObject::getType(JSContext *cx)
{
    JS_ASSERT(cx->compartment() == compartment());
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
    JS_ASSERT(!obj->hasSingletonType());
    JS_ASSERT(cx->compartment() == obj->compartment());

    js::types::TypeObject *type = cx->getNewType(obj->getClass(), js::TaggedProto(nullptr));
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
    JS_ASSERT(newType);
    JS_ASSERT(!hasSingletonType());
    type_ = newType;
}

/* static */ inline bool
JSObject::getProto(JSContext *cx, js::HandleObject obj, js::MutableHandleObject protop)
{
    if (obj->getTaggedProto().isLazy()) {
        JS_ASSERT(obj->is<js::ProxyObject>());
        return js::Proxy::getPrototypeOf(cx, obj, protop);
    } else {
        protop.set(obj->getTaggedProto().toObjectOrNull());
        return true;
    }
}

/* static */ inline bool
JSObject::setProto(JSContext *cx, JS::HandleObject obj, JS::HandleObject proto, bool *succeeded)
{
    /* Proxies live in their own little world. */
    if (obj->getTaggedProto().isLazy()) {
        JS_ASSERT(obj->is<js::ProxyObject>());
        return js::Proxy::setPrototypeOf(cx, obj, proto, succeeded);
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
    return SetClassAndProto(cx, obj, obj->getClass(), taggedProto, succeeded);
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

/* static */ inline JSObject *
JSObject::create(js::ExclusiveContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                 js::HandleShape shape, js::HandleTypeObject type)
{
    JS_ASSERT(shape && type);
    JS_ASSERT(type->clasp() == shape->getObjectClass());
    JS_ASSERT(type->clasp() != &js::ArrayObject::class_);
    JS_ASSERT_IF(!ClassCanHaveFixedData(type->clasp()),
                 js::gc::GetGCKindSlots(kind, type->clasp()) == shape->numFixedSlots());
    JS_ASSERT_IF(type->clasp()->flags & JSCLASS_BACKGROUND_FINALIZE, IsBackgroundFinalized(kind));
    JS_ASSERT_IF(type->clasp()->finalize, heap == js::gc::TenuredHeap);

    const js::Class *clasp = type->clasp();
    size_t nDynamicSlots = dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(), clasp);

    JSObject *obj = js::NewGCObject<js::CanGC>(cx, kind, nDynamicSlots, heap);
    if (!obj)
        return nullptr;

    obj->shape_.init(shape);
    obj->type_.init(type);
    // Note: slots are created and assigned internally by NewGCObject.
    obj->elements = js::emptyObjectElements;

    if (clasp->hasPrivate())
        obj->privateRef(shape->numFixedSlots()) = nullptr;

    size_t span = shape->slotSpan();
    if (span)
        obj->initializeSlotRange(0, span);

    // JSFunction's fixed slots expect POD-style initialization.
    if (type->clasp()->isJSFunction())
        memset(obj->fixedSlots(), 0, sizeof(js::HeapSlot) * GetGCKindSlots(kind));

    js::gc::TraceCreateObject(obj);

    return obj;
}

/* static */ inline JSObject *
JSObject::createArrayInternal(js::ExclusiveContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                              js::HandleShape shape, js::HandleTypeObject type)
{
    // Create a new array and initialize everything except for its elements.
    JS_ASSERT(shape && type);
    JS_ASSERT(type->clasp() == shape->getObjectClass());
    JS_ASSERT(type->clasp() == &js::ArrayObject::class_);
    JS_ASSERT_IF(type->clasp()->finalize, heap == js::gc::TenuredHeap);

    // Arrays can use their fixed slots to store elements, so can't have shapes
    // which allow named properties to be stored in the fixed slots.
    JS_ASSERT(shape->numFixedSlots() == 0);

    size_t nDynamicSlots = dynamicSlotsCount(0, shape->slotSpan(), type->clasp());
    JSObject *obj = js::NewGCObject<js::CanGC>(cx, kind, nDynamicSlots, heap);
    if (!obj)
        return nullptr;

    obj->shape_.init(shape);
    obj->type_.init(type);

    return obj;
}

/* static */ inline js::ArrayObject *
JSObject::createArray(js::ExclusiveContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                      js::HandleShape shape, js::HandleTypeObject type,
                      uint32_t length)
{
    JSObject *obj = createArrayInternal(cx, kind, heap, shape, type);
    if (!obj)
        return nullptr;

    uint32_t capacity = js::gc::GetGCKindSlots(kind) - js::ObjectElements::VALUES_PER_HEADER;

    obj->setFixedElements();
    new (obj->getElementsHeader()) js::ObjectElements(capacity, length);

    size_t span = shape->slotSpan();
    if (span)
        obj->initializeSlotRange(0, span);

    js::gc::TraceCreateObject(obj);

    return &obj->as<js::ArrayObject>();
}

/* static */ inline js::ArrayObject *
JSObject::createArray(js::ExclusiveContext *cx, js::gc::InitialHeap heap,
                      js::HandleShape shape, js::HandleTypeObject type,
                      js::HeapSlot *elements)
{
    // Use the smallest allocation kind for the array, as it can't have any
    // fixed slots (see assert in the above function) and will not be using its
    // fixed elements.
    js::gc::AllocKind kind = js::gc::FINALIZE_OBJECT0_BACKGROUND;

    JSObject *obj = createArrayInternal(cx, kind, heap, shape, type);
    if (!obj)
        return nullptr;

    obj->elements = elements;

    size_t span = shape->slotSpan();
    if (span)
        obj->initializeSlotRange(0, span);

    js::gc::TraceCreateObject(obj);

    return &obj->as<js::ArrayObject>();
}

inline void
JSObject::finish(js::FreeOp *fop)
{
    if (hasDynamicSlots())
        fop->free_(slots);

    if (hasDynamicElements()) {
        js::ObjectElements *elements = getElementsHeader();
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

inline bool
JSObject::nativeSetSlotIfHasType(js::Shape *shape, const js::Value &value, bool overwriting)
{
    if (!js::types::HasTypePropertyId(this, shape->propid(), value))
        return false;
    nativeSetSlot(shape->slot(), value);

    if (overwriting)
        shape->setOverwritten();

    return true;
}

inline void
JSObject::nativeSetSlotWithType(js::ExclusiveContext *cx, js::Shape *shape,
                                const js::Value &value, bool overwriting)
{
    nativeSetSlot(shape->slot(), value);

    if (overwriting)
        shape->setOverwritten();

    js::types::AddTypePropertyId(cx, this, shape->propid(), value);
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
ClassMethodIsNative(JSContext *cx, JSObject *obj, const Class *clasp, jsid methodid, JSNative native)
{
    JS_ASSERT(!obj->is<ProxyObject>());
    JS_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        JSObject *proto = obj->getProto();
        if (!proto || proto->getClass() != clasp || !HasDataProperty(cx, proto, methodid, &v))
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
    while (!HasDataProperty(cx, obj, valueOf, &v)) {
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
        if (ClassMethodIsNative(cx, obj, &StringObject::class_, id, js_str_toString)) {
            vp.setString(obj->as<StringObject>().unbox());
            return true;
        }
    }

    /* Optimize new Number(...).valueOf(). */
    if (obj->is<NumberObject>()) {
        jsid id = NameToId(cx->names().valueOf);
        if (ClassMethodIsNative(cx, obj, &NumberObject::class_, id, js_num_valueOf)) {
            vp.setNumber(obj->as<NumberObject>().unbox());
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
    JS_ASSERT(preferredType != JSTYPE_VOID); /* Use the other ToPrimitive! */
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
        JS_ASSERT(JSCLASS_CACHED_PROTO_KEY(clasp) == JSProto_Null);
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

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, HandleObject baseobj, NewObjectKind newKind = GenericObject)
{
    JS_ASSERT(baseobj->getClass() == &JSObject::class_);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind allocKind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    allocKind = gc::GetBackgroundAllocKind(allocKind);
    JS_ASSERT_IF(baseobj->isTenured(), allocKind == baseobj->tenuredGetAllocKind());
    RootedObject obj(cx);
    obj = NewBuiltinClassInstance(cx, &JSObject::class_, allocKind, newKind);
    if (!obj)
        return nullptr;

    RootedObject metadata(cx, obj->getMetadata());
    RootedShape lastProp(cx, baseobj->lastProperty());
    if (!JSObject::setLastProperty(cx, obj, lastProp))
        return nullptr;
    if (metadata && !JSObject::setMetadata(cx, obj, metadata))
        return nullptr;

    return obj;
}

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
      case ESClass_ArrayBuffer:
        return obj->is<ArrayBufferObject>() || obj->is<SharedArrayBufferObject>();
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
    else
        vp.setUndefined();

    return true;
}

static MOZ_ALWAYS_INLINE bool
NewObjectMetadata(ExclusiveContext *cxArg, JSObject **pmetadata)
{
    // The metadata callback is invoked before each created object, except when
    // analysis/compilation is active, to avoid recursion.  It is also skipped
    // when we allocate objects during a bailout, to prevent stack iterations.
    JS_ASSERT(!*pmetadata);
    if (JSContext *cx = cxArg->maybeJSContext()) {
        if (MOZ_UNLIKELY((size_t)cx->compartment()->hasObjectMetadataCallback()) &&
            !cx->compartment()->activeAnalysis)
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

inline bool
DefineNativeProperty(ExclusiveContext *cx, HandleObject obj, PropertyName *name, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return DefineNativeProperty(cx, obj, id, value, getter, setter, attrs);
}

namespace baseops {

inline bool
LookupProperty(ExclusiveContext *cx, HandleObject obj, PropertyName *name,
               MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return LookupProperty<CanGC>(cx, obj, id, objp, propp);
}

inline bool
DefineProperty(ExclusiveContext *cx, HandleObject obj, PropertyName *name, HandleValue value,
               JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return DefineGeneric(cx, obj, id, value, getter, setter, attrs);
}

} /* namespace baseops */

} /* namespace js */

extern JSObject *
js_InitClass(JSContext *cx, js::HandleObject obj, JSObject *parent_proto,
             const js::Class *clasp, JSNative constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
             JSObject **ctorp = nullptr,
             js::gc::AllocKind ctorKind = JSFunction::FinalizeKind);

#endif /* jsobjinlines_h */
