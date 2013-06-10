/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobjinlines_h___
#define jsobjinlines_h___

#include "jsobj.h"

#include "jsapi.h"
#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsstr.h"
#include "jstypedarray.h"
#include "jswrapper.h"

#include "builtin/Module.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/MemoryMetrics.h"
#include "js/RootingAPI.h"
#include "js/TemplateLib.h"
#include "vm/BooleanObject.h"
#include "vm/GlobalObject.h"
#include "vm/Shape.h"
#include "vm/NumberObject.h"
#include "vm/Probes.h"
#include "vm/RegExpStatics.h"
#include "vm/StringObject.h"

#include "jsatominlines.h"
#include "jscompartmentinlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "gc/Barrier-inl.h"
#include "vm/ObjectImpl-inl.h"
#include "vm/Shape-inl.h"
#include "vm/String-inl.h"

/* static */ inline bool
JSObject::enumerate(JSContext *cx, JS::HandleObject obj, JSIterateOp iterop,
                    JS::MutableHandleValue statep, JS::MutableHandleId idp)
{
    JSNewEnumerateOp op = obj->getOps()->enumerate;
    return (op ? op : JS_EnumerateState)(cx, obj, iterop, statep, idp);
}

/* static */ inline bool
JSObject::defaultValue(JSContext *cx, js::HandleObject obj, JSType hint, js::MutableHandleValue vp)
{
    JSConvertOp op = obj->getClass()->convert;
    bool ok;
    if (op == JS_ConvertStub)
        ok = js::DefaultValue(cx, obj, hint, vp);
    else
        ok = op(cx, obj, hint, vp);
    JS_ASSERT_IF(ok, vp.isPrimitive());
    return ok;
}

/* static */ inline JSObject *
JSObject::thisObject(JSContext *cx, js::HandleObject obj)
{
    JSObjectOp op = obj->getOps()->thisObject;
    return op ? op(cx, obj) : obj;
}

/* static */ inline JSBool
JSObject::setGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::HandleId id, js::MutableHandleValue vp, JSBool strict)
{
    if (obj->getOps()->setGeneric)
        return nonNativeSetProperty(cx, obj, id, vp, strict);
    return js::baseops::SetPropertyHelper(cx, obj, receiver, id, 0, vp, strict);
}

/* static */ inline JSBool
JSObject::setProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                      js::PropertyName *name, js::MutableHandleValue vp, JSBool strict)
{
    JS::RootedId id(cx, js::NameToId(name));
    return setGeneric(cx, obj, receiver, id, vp, strict);
}

/* static */ inline JSBool
JSObject::setElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     uint32_t index, js::MutableHandleValue vp, JSBool strict)
{
    if (obj->getOps()->setElement)
        return nonNativeSetElement(cx, obj, index, vp, strict);
    return js::baseops::SetElementHelper(cx, obj, receiver, index, 0, vp, strict);
}

/* static */ inline JSBool
JSObject::setSpecial(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::SpecialId sid, js::MutableHandleValue vp, JSBool strict)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return setGeneric(cx, obj, receiver, id, vp, strict);
}

/* static */ inline JSBool
JSObject::setGenericAttributes(JSContext *cx, js::HandleObject obj,
                               js::HandleId id, unsigned *attrsp)
{
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::GenericAttributesOp op = obj->getOps()->setGenericAttributes;
    return (op ? op : js::baseops::SetAttributes)(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::setPropertyAttributes(JSContext *cx, js::HandleObject obj,
                                js::PropertyName *name, unsigned *attrsp)
{
    JS::RootedId id(cx, js::NameToId(name));
    return setGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::setElementAttributes(JSContext *cx, js::HandleObject obj,
                               uint32_t index, unsigned *attrsp)
{
    js::ElementAttributesOp op = obj->getOps()->setElementAttributes;
    return (op ? op : js::baseops::SetElementAttributes)(cx, obj, index, attrsp);
}

/* static */ inline JSBool
JSObject::setSpecialAttributes(JSContext *cx, js::HandleObject obj,
                               js::SpecialId sid, unsigned *attrsp)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return setGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline bool
JSObject::changePropertyAttributes(JSContext *cx, js::HandleObject obj,
                                   js::HandleShape shape, unsigned attrs)
{
    return !!changeProperty(cx, obj, shape, attrs, 0, shape->getter(), shape->setter());
}

/* static */ inline JSBool
JSObject::getGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::HandleId id, js::MutableHandleValue vp)
{
    js::GenericIdOp op = obj->getOps()->getGeneric;
    if (op) {
        if (!op(cx, obj, receiver, id, vp))
            return false;
    } else {
        if (!js::baseops::GetProperty(cx, obj, receiver, id, vp))
            return false;
    }
    return true;
}

/* static */ inline JSBool
JSObject::getGenericNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                         jsid id, js::Value *vp)
{
    js::GenericIdOp op = obj->getOps()->getGeneric;
    if (op)
        return false;
    return js::baseops::GetPropertyNoGC(cx, obj, receiver, id, vp);
}

/* static */ inline JSBool
JSObject::getProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                      js::PropertyName *name, js::MutableHandleValue vp)
{
    JS::RootedId id(cx, js::NameToId(name));
    return getGeneric(cx, obj, receiver, id, vp);
}

/* static */ inline JSBool
JSObject::getPropertyNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                          js::PropertyName *name, js::Value *vp)
{
    return getGenericNoGC(cx, obj, receiver, js::NameToId(name), vp);
}

/* static */ inline bool
JSObject::deleteProperty(JSContext *cx, js::HandleObject obj, js::HandlePropertyName name,
                         JSBool *succeeded)
{
    JS::RootedId id(cx, js::NameToId(name));
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeletePropertyOp op = obj->getOps()->deleteProperty;
    return (op ? op : js::baseops::DeleteProperty)(cx, obj, name, succeeded);
}

/* static */ inline bool
JSObject::deleteElement(JSContext *cx, js::HandleObject obj, uint32_t index, JSBool *succeeded)
{
    JS::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeleteElementOp op = obj->getOps()->deleteElement;
    return (op ? op : js::baseops::DeleteElement)(cx, obj, index, succeeded);
}

/* static */ inline bool
JSObject::deleteSpecial(JSContext *cx, js::HandleObject obj, js::HandleSpecialId sid,
                        JSBool *succeeded)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeleteSpecialOp op = obj->getOps()->deleteSpecial;
    return (op ? op : js::baseops::DeleteSpecial)(cx, obj, sid, succeeded);
}

inline void
JSObject::finalize(js::FreeOp *fop)
{
    js::Probes::finalizeObject(this);

#ifdef DEBUG
    JS_ASSERT(isTenured());
    if (!IsBackgroundFinalized(tenuredGetAllocKind())) {
        /* Assert we're on the main thread. */
        fop->runtime()->assertValidThread();
    }
#endif
    js::Class *clasp = getClass();
    if (clasp->finalize)
        clasp->finalize(fop, this);

    finish(fop);
}

inline JSObject *
JSObject::getParent() const
{
    return lastProperty()->getObjectParent();
}

inline JSObject *
JSObject::getMetadata() const
{
    return lastProperty()->getObjectMetadata();
}

inline JSObject *
JSObject::enclosingScope()
{
    return isScope()
           ? &asScope().enclosingScope()
           : isDebugScope()
           ? &asDebugScope().enclosingScope()
           : getParent();
}

inline bool
JSObject::isFixedSlot(size_t slot)
{
    return slot < numFixedSlots();
}

inline size_t
JSObject::dynamicSlotIndex(size_t slot)
{
    JS_ASSERT(slot >= numFixedSlots());
    return slot - numFixedSlots();
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
JSObject::removeLastProperty(JSContext *cx)
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

inline const js::HeapSlot *
JSObject::getRawSlots()
{
    JS_ASSERT(isGlobal());
    return slots;
}

inline void
JSObject::setReservedSlot(uint32_t index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    setSlot(index, v);
}

inline void
JSObject::initReservedSlot(uint32_t index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    initSlot(index, v);
}

inline void
JSObject::prepareSlotRangeForOverwrite(size_t start, size_t end)
{
    for (size_t i = start; i < end; i++)
        getSlotAddressUnchecked(i)->js::HeapSlot::~HeapSlot();
}

inline void
JSObject::prepareElementRangeForOverwrite(size_t start, size_t end)
{
    JS_ASSERT(end <= getDenseInitializedLength());
    for (size_t i = start; i < end; i++)
        elements[i].js::HeapSlot::~HeapSlot();
}

inline uint32_t
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return getElementsHeader()->length;
}

inline bool
JSObject::arrayLengthIsWritable() const
{
    JS_ASSERT(isArray());
    return !getElementsHeader()->hasNonwritableArrayLength();
}

/* static */ inline void
JSObject::setArrayLength(JSContext *cx, js::HandleObject obj, uint32_t length)
{
    JS_ASSERT(obj->isArray());
    JS_ASSERT(obj->arrayLengthIsWritable());

    if (length > INT32_MAX) {
        /* Track objects with overflowing lengths in type information. */
        js::types::MarkTypeObjectFlags(cx, obj,
                                       js::types::OBJECT_FLAG_LENGTH_OVERFLOW);
        jsid lengthId = js::NameToId(cx->names().length);
        js::types::AddTypePropertyId(cx, obj, lengthId,
                                     js::types::Type::DoubleType());
    }

    obj->getElementsHeader()->length = length;
}

inline void
JSObject::setArrayLengthInt32(uint32_t length)
{
    /* Variant of setArrayLength for use on arrays where the length cannot overflow int32_t. */
    JS_ASSERT(isArray());
    JS_ASSERT(arrayLengthIsWritable());
    JS_ASSERT(length <= INT32_MAX);
    getElementsHeader()->length = length;
}

inline void
JSObject::setDenseInitializedLength(uint32_t length)
{
    JS_ASSERT(isNative());
    JS_ASSERT(length <= getDenseCapacity());
    prepareElementRangeForOverwrite(length, getElementsHeader()->initializedLength);
    getElementsHeader()->initializedLength = length;
}

inline uint32_t
JSObject::getDenseCapacity()
{
    JS_ASSERT(isNative());
    JS_ASSERT(getElementsHeader()->capacity >= getElementsHeader()->initializedLength);
    return getElementsHeader()->capacity;
}

inline bool
JSObject::shouldConvertDoubleElements()
{
    JS_ASSERT(isNative());
    return getElementsHeader()->shouldConvertDoubleElements();
}

inline void
JSObject::setShouldConvertDoubleElements()
{
    JS_ASSERT(isArray() && !hasEmptyElements());
    getElementsHeader()->setShouldConvertDoubleElements();
}

inline bool
JSObject::ensureElements(JSContext *cx, uint32_t capacity)
{
    if (capacity > getDenseCapacity())
        return growElements(cx, capacity);
    return true;
}

inline void
JSObject::setDynamicElements(js::ObjectElements *header)
{
    JS_ASSERT(!hasDynamicElements());
    elements = header->elements();
    JS_ASSERT(hasDynamicElements());
}

inline void
JSObject::setDenseElement(uint32_t index, const js::Value &val)
{
    JS_ASSERT(isNative() && index < getDenseInitializedLength());
    elements[index].set(this, js::HeapSlot::Element, index, val);
}

inline void
JSObject::setDenseElementMaybeConvertDouble(uint32_t index, const js::Value &val)
{
    if (val.isInt32() && shouldConvertDoubleElements())
        setDenseElement(index, js::DoubleValue(val.toInt32()));
    else
        setDenseElement(index, val);
}

inline void
JSObject::initDenseElement(uint32_t index, const js::Value &val)
{
    JS_ASSERT(isNative() && index < getDenseInitializedLength());
    elements[index].init(this, js::HeapSlot::Element, index, val);
}

/* static */ inline void
JSObject::setDenseElementWithType(JSContext *cx, js::HandleObject obj, uint32_t index,
                                  const js::Value &val)
{
    js::types::AddTypePropertyId(cx, obj, JSID_VOID, val);
    obj->setDenseElementMaybeConvertDouble(index, val);
}

/* static */ inline void
JSObject::initDenseElementWithType(JSContext *cx, js::HandleObject obj, uint32_t index,
                                   const js::Value &val)
{
    JS_ASSERT(!obj->shouldConvertDoubleElements());
    js::types::AddTypePropertyId(cx, obj, JSID_VOID, val);
    obj->initDenseElement(index, val);
}

/* static */ inline void
JSObject::setDenseElementHole(JSContext *cx, js::HandleObject obj, uint32_t index)
{
    js::types::MarkTypeObjectFlags(cx, obj, js::types::OBJECT_FLAG_NON_PACKED);
    obj->setDenseElement(index, js::MagicValue(JS_ELEMENTS_HOLE));
}

/* static */ inline void
JSObject::removeDenseElementForSparseIndex(JSContext *cx, js::HandleObject obj, uint32_t index)
{
    js::types::MarkTypeObjectFlags(cx, obj,
                                   js::types::OBJECT_FLAG_NON_PACKED |
                                   js::types::OBJECT_FLAG_SPARSE_INDEXES);
    if (obj->containsDenseElement(index))
        obj->setDenseElement(index, js::MagicValue(JS_ELEMENTS_HOLE));
}

inline void
JSObject::copyDenseElements(uint32_t dstStart, const js::Value *src, uint32_t count)
{
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JS::Zone *zone = this->zone();
    for (uint32_t i = 0; i < count; ++i)
        elements[dstStart + i].set(zone, this, js::HeapSlot::Element, dstStart + i, src[i]);
}

inline void
JSObject::initDenseElements(uint32_t dstStart, const js::Value *src, uint32_t count)
{
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JSRuntime *rt = runtime();
    for (uint32_t i = 0; i < count; ++i)
        elements[dstStart + i].init(rt, this, js::HeapSlot::Element, dstStart + i, src[i]);
}

inline void
JSObject::moveDenseElements(uint32_t dstStart, uint32_t srcStart, uint32_t count)
{
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JS_ASSERT(srcStart + count <= getDenseInitializedLength());

    /*
     * Using memmove here would skip write barriers. Also, we need to consider
     * an array containing [A, B, C], in the following situation:
     *
     * 1. Incremental GC marks slot 0 of array (i.e., A), then returns to JS code.
     * 2. JS code moves slots 1..2 into slots 0..1, so it contains [B, C, C].
     * 3. Incremental GC finishes by marking slots 1 and 2 (i.e., C).
     *
     * Since normal marking never happens on B, it is very important that the
     * write barrier is invoked here on B, despite the fact that it exists in
     * the array before and after the move.
    */
    JS::Zone *zone = this->zone();
    if (zone->needsBarrier()) {
        if (dstStart < srcStart) {
            js::HeapSlot *dst = elements + dstStart;
            js::HeapSlot *src = elements + srcStart;
            for (uint32_t i = 0; i < count; i++, dst++, src++)
                dst->set(zone, this, js::HeapSlot::Element, dst - elements, *src);
        } else {
            js::HeapSlot *dst = elements + dstStart + count - 1;
            js::HeapSlot *src = elements + srcStart + count - 1;
            for (uint32_t i = 0; i < count; i++, dst--, src--)
                dst->set(zone, this, js::HeapSlot::Element, dst - elements, *src);
        }
    } else {
        memmove(elements + dstStart, elements + srcStart, count * sizeof(js::HeapSlot));
        DenseRangeWriteBarrierPost(runtime(), this, dstStart, count);
    }
}

inline void
JSObject::moveDenseElementsUnbarriered(uint32_t dstStart, uint32_t srcStart, uint32_t count)
{
    JS_ASSERT(!zone()->needsBarrier());

    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JS_ASSERT(srcStart + count <= getDenseCapacity());

    memmove(elements + dstStart, elements + srcStart, count * sizeof(js::Value));
}

inline void
JSObject::markDenseElementsNotPacked(JSContext *cx)
{
    JS_ASSERT(isNative());
    MarkTypeObjectFlags(cx, this, js::types::OBJECT_FLAG_NON_PACKED);
}

inline void
JSObject::ensureDenseInitializedLength(JSContext *cx, uint32_t index, uint32_t extra)
{
    /*
     * Ensure that the array's contents have been initialized up to index, and
     * mark the elements through 'index + extra' as initialized in preparation
     * for a write.
     */
    JS_ASSERT(index + extra <= getDenseCapacity());
    uint32_t &initlen = getElementsHeader()->initializedLength;
    if (initlen < index)
        markDenseElementsNotPacked(cx);

    if (initlen < index + extra) {
        JSRuntime *rt = runtime();
        size_t offset = initlen;
        for (js::HeapSlot *sp = elements + initlen;
             sp != elements + (index + extra);
             sp++, offset++)
            sp->init(rt, this, js::HeapSlot::Element, offset, js::MagicValue(JS_ELEMENTS_HOLE));
        initlen = index + extra;
    }
}

template<typename MallocProviderType>
JSObject::EnsureDenseResult
JSObject::extendDenseElements(MallocProviderType *cx,
                              uint32_t requiredCapacity, uint32_t extra)
{
    /*
     * Don't grow elements for non-extensible objects or watched objects. Dense
     * elements can be added/written with no extensible or watchpoint checks as
     * long as there is capacity for them.
     */
    if (!isExtensible() || watched()) {
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
JSObject::parExtendDenseElements(js::Allocator *alloc, js::Value *v, uint32_t extra)
{
    JS_ASSERT(isNative());
    JS_ASSERT_IF(isArray(), arrayLengthIsWritable());

    js::ObjectElements *header = getElementsHeader();
    uint32_t initializedLength = header->initializedLength;
    uint32_t requiredCapacity = initializedLength + extra;
    if (requiredCapacity < initializedLength)
        return ED_SPARSE; /* Overflow. */

    if (requiredCapacity > header->capacity) {
        EnsureDenseResult edr = extendDenseElements(alloc, requiredCapacity, extra);
        if (edr != ED_OK)
            return edr;
    }

    // Watch out lest the header has been reallocated by
    // extendDenseElements():
    header = getElementsHeader();

    js::HeapSlot *sp = elements + initializedLength;
    if (v) {
        for (uint32_t i = 0; i < extra; i++)
            sp[i].init(runtime(), this, js::HeapSlot::Element, initializedLength+i, v[i]);
    } else {
        for (uint32_t i = 0; i < extra; i++) {
            sp[i].init(runtime(), this, js::HeapSlot::Element,
                       initializedLength + i, js::MagicValue(JS_ELEMENTS_HOLE));
        }
    }
    header->initializedLength = requiredCapacity;
    if (header->length < requiredCapacity)
        header->length = requiredCapacity;
    return ED_OK;
}

inline JSObject::EnsureDenseResult
JSObject::ensureDenseElements(JSContext *cx, uint32_t index, uint32_t extra)
{
    JS_ASSERT(isNative());

    uint32_t currentCapacity = getDenseCapacity();

    uint32_t requiredCapacity;
    if (extra == 1) {
        /* Optimize for the common case. */
        if (index < currentCapacity) {
            ensureDenseInitializedLength(cx, index, 1);
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
            ensureDenseInitializedLength(cx, index, extra);
            return ED_OK;
        }
    }

    EnsureDenseResult edr = extendDenseElements(cx, requiredCapacity, extra);
    if (edr != ED_OK)
        return edr;

    ensureDenseInitializedLength(cx, index, extra);
    return ED_OK;
}

namespace js {

/*
 * Any name atom for a function which will be added as a DeclEnv object to the
 * scope chain above call objects for fun.
 */
static inline JSAtom *
CallObjectLambdaName(JSFunction &fun)
{
    return fun.isNamedLambda() ? fun.atom() : NULL;
}

} /* namespace js */

inline const js::Value &
JSObject::getDateUTCTime() const
{
    JS_ASSERT(isDate());
    return getFixedSlot(JSSLOT_DATE_UTC_TIME);
}

inline void
JSObject::setDateUTCTime(const js::Value &time)
{
    JS_ASSERT(isDate());
    setFixedSlot(JSSLOT_DATE_UTC_TIME, time);
}

/* static */ inline bool
JSObject::setSingletonType(JSContext *cx, js::HandleObject obj)
{
    JS_ASSERT(!IsInsideNursery(cx->runtime(), obj.get()));

    if (!cx->typeInferenceEnabled())
        return true;

    js::types::TypeObject *type =
        cx->compartment()->getLazyType(cx, obj->getClass(), obj->getTaggedProto());
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

    js::types::TypeObject *type = cx->compartment()->getNewType(cx, obj->getClass(), NULL);
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
    JS_ASSERT(newType);
    JS_ASSERT_IF(getClass()->emulatesUndefined(),
                 newType->hasAnyFlags(js::types::OBJECT_FLAG_EMULATES_UNDEFINED));
    JS_ASSERT(!hasSingletonType());
    type_ = newType;
}

inline JSObject *
JSObject::getProto() const
{
    JS_ASSERT(!isProxy());
    return js::ObjectImpl::getProto();
}

/* static */ inline bool
JSObject::getProto(JSContext *cx, js::HandleObject obj, js::MutableHandleObject protop)
{
    if (obj->getTaggedProto().isLazy()) {
        JS_ASSERT(obj->isProxy());
        return js::Proxy::getPrototypeOf(cx, obj, protop);
    } else {
        protop.set(obj->js::ObjectImpl::getProto());
        return true;
    }
}

inline bool JSObject::setIteratedSingleton(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::ITERATED_SINGLETON);
}

inline bool JSObject::setDelegate(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::DELEGATE, GENERATE_SHAPE);
}

inline bool JSObject::isVarObj()
{
    if (isDebugScope())
        return asDebugScope().scope().isVarObj();
    return lastProperty()->hasObjectFlag(js::BaseShape::VAROBJ);
}

inline bool JSObject::setVarObj(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::VAROBJ);
}

inline bool JSObject::setWatched(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::WATCHED, GENERATE_SHAPE);
}

inline bool JSObject::hasUncacheableProto() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::UNCACHEABLE_PROTO);
}

inline bool JSObject::setUncacheableProto(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::UNCACHEABLE_PROTO, GENERATE_SHAPE);
}

inline bool JSObject::hadElementsAccess() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::HAD_ELEMENTS_ACCESS);
}

inline bool JSObject::setHadElementsAccess(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::HAD_ELEMENTS_ACCESS);
}

inline bool JSObject::isBoundFunction() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::BOUND_FUNCTION);
}

inline bool JSObject::isIndexed() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::INDEXED);
}

inline bool JSObject::watched() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::WATCHED);
}

inline bool JSObject::isArray() const { return hasClass(&js::ArrayClass); }
inline bool JSObject::isArguments() const { return isNormalArguments() || isStrictArguments(); }
inline bool JSObject::isArrayBuffer() const { return hasClass(&js::ArrayBufferClass); }
inline bool JSObject::isBlock() const { return hasClass(&js::BlockClass); }
inline bool JSObject::isBoolean() const { return hasClass(&js::BooleanClass); }
inline bool JSObject::isCall() const { return hasClass(&js::CallClass); }
inline bool JSObject::isClonedBlock() const { return isBlock() && !!getProto(); }
inline bool JSObject::isDataView() const { return hasClass(&js::DataViewClass); }
inline bool JSObject::isDate() const { return hasClass(&js::DateClass); }
inline bool JSObject::isDeclEnv() const { return hasClass(&js::DeclEnvClass); }
inline bool JSObject::isElementIterator() const { return hasClass(&js::ElementIteratorClass); }
inline bool JSObject::isError() const { return hasClass(&js::ErrorClass); }
inline bool JSObject::isFunction() const { return hasClass(&js::FunctionClass); }
inline bool JSObject::isFunctionProxy() const { return hasClass(&js::FunctionProxyClass); }
inline bool JSObject::isGenerator() const { return hasClass(&js::GeneratorClass); }
inline bool JSObject::isMapIterator() const { return hasClass(&js::MapIteratorClass); }
inline bool JSObject::isModule() const { return hasClass(&js::ModuleClass); }
inline bool JSObject::isNestedScope() const { return isBlock() || isWith(); }
inline bool JSObject::isNormalArguments() const { return hasClass(&js::NormalArgumentsObjectClass); }
inline bool JSObject::isNumber() const { return hasClass(&js::NumberClass); }
inline bool JSObject::isObject() const { return hasClass(&js::ObjectClass); }
inline bool JSObject::isPrimitive() const { return isNumber() || isString() || isBoolean(); }
inline bool JSObject::isRegExp() const { return hasClass(&js::RegExpClass); }
inline bool JSObject::isRegExpStatics() const { return hasClass(&js::RegExpStaticsClass); }
inline bool JSObject::isScope() const { return isCall() || isDeclEnv() || isNestedScope(); }
inline bool JSObject::isScriptSource() const { return hasClass(&js::ScriptSourceClass); }
inline bool JSObject::isSetIterator() const { return hasClass(&js::SetIteratorClass); }
inline bool JSObject::isStaticBlock() const { return isBlock() && !getProto(); }
inline bool JSObject::isStopIteration() const { return hasClass(&js::StopIterationClass); }
inline bool JSObject::isStrictArguments() const { return hasClass(&js::StrictArgumentsObjectClass); }
inline bool JSObject::isString() const { return hasClass(&js::StringClass); }
inline bool JSObject::isTypedArray() const { return IsTypedArrayClass(getClass()); }
inline bool JSObject::isWeakMap() const { return hasClass(&js::WeakMapClass); }
inline bool JSObject::isWith() const { return hasClass(&js::WithClass); }

inline js::NumberObject &
JSObject::asNumber()
{
    JS_ASSERT(isNumber());
    return *static_cast<js::NumberObject *>(this);
}

inline js::StringObject &
JSObject::asString()
{
    JS_ASSERT(isString());
    return *static_cast<js::StringObject *>(this);
}

inline js::ScriptSourceObject &
JSObject::asScriptSource()
{
    JS_ASSERT(isScriptSource());
    return *static_cast<js::ScriptSourceObject *>(this);
}

inline js::Module &
JSObject::asModule()
{
    JS_ASSERT(isModule());
    return *static_cast<js::Module *>(this);
}

inline bool
JSObject::isDebugScope() const
{
    extern bool js_IsDebugScopeSlow(JSObject *obj);
    return getClass() == &js::ObjectProxyClass && js_IsDebugScopeSlow(const_cast<JSObject*>(this));
}

/* static */ inline JSObject *
JSObject::create(JSContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                 js::HandleShape shape, js::HandleTypeObject type,
                 js::HeapSlot *extantSlots /* = NULL */)
{
    /*
     * Callers must use dynamicSlotsCount to size the initial slot array of the
     * object. We can't check the allocated capacity of the dynamic slots, but
     * make sure their presence is consistent with the shape.
     */
    JS_ASSERT(shape && type);
    JS_ASSERT(type->clasp == shape->getObjectClass());
    JS_ASSERT(type->clasp != &js::ArrayClass);
    JS_ASSERT(js::gc::GetGCKindSlots(kind, type->clasp) == shape->numFixedSlots());
    JS_ASSERT_IF(type->clasp->flags & JSCLASS_BACKGROUND_FINALIZE, IsBackgroundFinalized(kind));
    JS_ASSERT_IF(type->clasp->finalize, heap == js::gc::TenuredHeap);
    JS_ASSERT_IF(extantSlots, dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan()));

    js::HeapSlot *slots = extantSlots;
    if (!slots) {
        size_t nDynamicSlots = dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan());
        if (nDynamicSlots) {
            slots = cx->pod_malloc<js::HeapSlot>(nDynamicSlots);
            if (!slots)
                return NULL;
            js::Debug_SetSlotRangeToCrashOnTouch(slots, nDynamicSlots);
        }
    }

    JSObject *obj = js_NewGCObject<js::CanGC>(cx, kind, heap);
    if (!obj) {
        js_free(slots);
        return NULL;
    }

#ifdef JSGC_GENERATIONAL
    cx->runtime()->gcNursery.notifyInitialSlots(obj, slots);
#endif

    obj->shape_.init(shape);
    obj->type_.init(type);
    obj->slots = slots;
    obj->elements = js::emptyObjectElements;

    const js::Class *clasp = type->clasp;
    if (clasp->hasPrivate())
        obj->privateRef(shape->numFixedSlots()) = NULL;

    size_t span = shape->slotSpan();
    if (span && clasp != &js::ArrayBufferClass)
        obj->initializeSlotRange(0, span);

    return obj;
}

/* static */ inline JSObject *
JSObject::createArray(JSContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                      js::HandleShape shape, js::HandleTypeObject type,
                      uint32_t length)
{
    JS_ASSERT(shape && type);
    JS_ASSERT(type->clasp == shape->getObjectClass());
    JS_ASSERT(type->clasp == &js::ArrayClass);
    JS_ASSERT_IF(type->clasp->finalize, heap == js::gc::TenuredHeap);

    /*
     * Arrays use their fixed slots to store elements, and must have enough
     * space for the elements header and also be marked as having no space for
     * named properties stored in those fixed slots.
     */
    JS_ASSERT(shape->numFixedSlots() == 0);

    /*
     * The array initially stores its elements inline, there must be enough
     * space for an elements header.
     */
    JS_ASSERT(js::gc::GetGCKindSlots(kind) >= js::ObjectElements::VALUES_PER_HEADER);

    uint32_t capacity = js::gc::GetGCKindSlots(kind) - js::ObjectElements::VALUES_PER_HEADER;

    JSObject *obj = js_NewGCObject<js::CanGC>(cx, kind, heap);
    if (!obj) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    obj->shape_.init(shape);
    obj->type_.init(type);
    obj->slots = NULL;
    obj->setFixedElements();
    new (obj->getElementsHeader()) js::ObjectElements(capacity, length);

    return obj;
}

inline void
JSObject::finish(js::FreeOp *fop)
{
    if (hasDynamicSlots())
        fop->free_(slots);
    if (hasDynamicElements()) {
        js::ObjectElements *elements = getElementsHeader();
        if (JS_UNLIKELY(elements->isAsmJSArrayBuffer()))
            js::ArrayBufferObject::releaseAsmJSArrayBuffer(fop, this);
        else
            fop->free_(elements);
    }
}

/* static */ inline bool
JSObject::hasProperty(JSContext *cx, js::HandleObject obj,
                      js::HandleId id, bool *foundp, unsigned flags)
{
    JS::RootedObject pobj(cx);
    js::RootedShape prop(cx);
    JSAutoResolveFlags rf(cx, flags);
    if (!lookupGeneric(cx, obj, id, &pobj, &prop)) {
        *foundp = false;  /* initialize to shut GCC up */
        return false;
    }
    *foundp = !!prop;
    return true;
}

inline bool
JSObject::isCallable()
{
    return getClass()->isCallable();
}

inline void
JSObject::nativeSetSlot(uint32_t slot, const js::Value &value)
{
    JS_ASSERT(isNative());
    JS_ASSERT(slot < slotSpan());
    return setSlot(slot, value);
}

/* static */ inline void
JSObject::nativeSetSlotWithType(JSContext *cx, js::HandleObject obj, js::Shape *shape,
                                const js::Value &value)
{
    obj->nativeSetSlot(shape->slot(), value);
    js::types::AddTypePropertyId(cx, obj, shape->propid(), value);
}

inline bool
JSObject::nativeEmpty() const
{
    return lastProperty()->isEmptyShape();
}

inline uint32_t
JSObject::propertyCount() const
{
    return lastProperty()->entryCount();
}

inline bool
JSObject::hasShapeTable() const
{
    return lastProperty()->hasTable();
}

/* static */ inline JSBool
JSObject::lookupGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    /* NB: The logic of lookupGeneric is implicitly reflected in IonBuilder.cpp's
     *     |CanEffectlesslyCallLookupGenericOnObject| logic.
     *     If this changes, please remember to update the logic there as well.
     */
    js::LookupGenericOp op = obj->getOps()->lookupGeneric;
    if (op)
        return op(cx, obj, id, objp, propp);
    return js::baseops::LookupProperty<js::CanGC>(cx, obj, id, objp, propp);
}

/* static */ inline JSBool
JSObject::lookupProperty(JSContext *cx, js::HandleObject obj, js::PropertyName *name,
                         js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    JS::RootedId id(cx, js::NameToId(name));
    return lookupGeneric(cx, obj, id, objp, propp);
}

/* static */ inline JSBool
JSObject::defineGeneric(JSContext *cx, js::HandleObject obj,
                        js::HandleId id, js::HandleValue value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    JS_ASSERT(!(attrs & JSPROP_NATIVE_ACCESSORS));
    js::DefineGenericOp op = obj->getOps()->defineGeneric;
    return (op ? op : js::baseops::DefineGeneric)(cx, obj, id, value, getter, setter, attrs);
}

/* static */ inline JSBool
JSObject::defineProperty(JSContext *cx, js::HandleObject obj,
                         js::PropertyName *name, js::HandleValue value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    JS::RootedId id(cx, js::NameToId(name));
    return defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

/* static */ inline JSBool
JSObject::defineElement(JSContext *cx, js::HandleObject obj,
                        uint32_t index, js::HandleValue value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    js::DefineElementOp op = obj->getOps()->defineElement;
    return (op ? op : js::baseops::DefineElement)(cx, obj, index, value, getter, setter, attrs);
}

/* static */ inline JSBool
JSObject::defineSpecial(JSContext *cx, js::HandleObject obj, js::SpecialId sid, js::HandleValue value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

/* static */ inline JSBool
JSObject::lookupElement(JSContext *cx, js::HandleObject obj, uint32_t index,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::LookupElementOp op = obj->getOps()->lookupElement;
    return (op ? op : js::baseops::LookupElement)(cx, obj, index, objp, propp);
}

/* static */ inline JSBool
JSObject::lookupSpecial(JSContext *cx, js::HandleObject obj, js::SpecialId sid,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return lookupGeneric(cx, obj, id, objp, propp);
}

/* static */ inline JSBool
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

/* static */ inline JSBool
JSObject::getElementNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                         uint32_t index, js::Value *vp)
{
    js::ElementIdOp op = obj->getOps()->getElement;
    if (op)
        return false;

    jsid id;
    if (!js::IndexToIdNoGC(cx, index, &id))
        return false;
    return getGenericNoGC(cx, obj, receiver, id, vp);
}

/* static */ inline JSBool
JSObject::getElementIfPresent(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                              uint32_t index, js::MutableHandleValue vp,
                              bool *present)
{
    js::ElementIfPresentOp op = obj->getOps()->getElementIfPresent;
    if (op)
        return op(cx, obj, receiver, index, vp, present);

    /*
     * For now, do the index-to-id conversion just once, then use
     * lookupGeneric/getGeneric.  Once lookupElement and getElement stop both
     * doing index-to-id conversions, we can use those here.
     */
    JS::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;

    JS::RootedObject obj2(cx);
    js::RootedShape prop(cx);
    if (!lookupGeneric(cx, obj, id, &obj2, &prop))
        return false;

    if (!prop) {
        *present = false;
        return true;
    }

    *present = true;
    return getGeneric(cx, obj, receiver, id, vp);
}

/* static */ inline JSBool
JSObject::getSpecial(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::SpecialId sid, js::MutableHandleValue vp)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return getGeneric(cx, obj, receiver, id, vp);
}

/* static */ inline JSBool
JSObject::getGenericAttributes(JSContext *cx, js::HandleObject obj,
                               js::HandleId id, unsigned *attrsp)
{
    js::GenericAttributesOp op = obj->getOps()->getGenericAttributes;
    return (op ? op : js::baseops::GetAttributes)(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::getPropertyAttributes(JSContext *cx, js::HandleObject obj,
                                js::PropertyName *name, unsigned *attrsp)
{
    JS::RootedId id(cx, js::NameToId(name));
    return getGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::getElementAttributes(JSContext *cx, js::HandleObject obj,
                               uint32_t index, unsigned *attrsp)
{
    JS::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::getSpecialAttributes(JSContext *cx, js::HandleObject obj,
                               js::SpecialId sid, unsigned *attrsp)
{
    JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return getGenericAttributes(cx, obj, id, attrsp);
}

inline bool
JSObject::isCrossCompartmentWrapper() const
{
    return js::IsCrossCompartmentWrapper(const_cast<JSObject*>(this));
}

inline bool
JSObject::isWrapper() const
{
    return js::IsWrapper(const_cast<JSObject*>(this));
}

inline js::GlobalObject &
JSObject::global() const
{
#ifdef DEBUG
    JSObject *obj = const_cast<JSObject *>(this);
    while (JSObject *parent = obj->getParent())
        obj = parent;
#endif
    return *compartment()->maybeGlobal();
}

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.toObject().isCallable();
}

namespace js {

PropDesc::PropDesc(const Value &getter, const Value &setter,
                   Enumerability enumerable, Configurability configurable)
  : pd_(UndefinedValue()),
    value_(UndefinedValue()),
    get_(getter), set_(setter),
    attrs(JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED |
          (enumerable ? JSPROP_ENUMERATE : 0) |
          (configurable ? 0 : JSPROP_PERMANENT)),
    hasGet_(true), hasSet_(true),
    hasValue_(false), hasWritable_(false), hasEnumerable_(true), hasConfigurable_(true),
    isUndefined_(false)
{
    MOZ_ASSERT(getter.isUndefined() || js_IsCallable(getter));
    MOZ_ASSERT(setter.isUndefined() || js_IsCallable(setter));
}

inline JSObject *
GetInnerObject(JSContext *cx, HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        return op(cx, obj);
    return obj;
}

inline JSObject *
GetOuterObject(JSContext *cx, HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.outerObject)
        return op(cx, obj);
    return obj;
}

static inline bool
IsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().isStopIteration();
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v)
{
    return v.isObject() && v.toObject().isFunction();
}

static JS_ALWAYS_INLINE bool
IsFunctionObject(const js::Value &v, JSFunction **fun)
{
    if (v.isObject() && v.toObject().isFunction()) {
        *fun = v.toObject().toFunction();
        return true;
    }
    return false;
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v)
{
    JSFunction *fun;
    return IsFunctionObject(v, &fun) && fun->isNative();
}

static JS_ALWAYS_INLINE bool
IsNativeFunction(const js::Value &v, JSFunction **fun)
{
    return IsFunctionObject(v, fun) && (*fun)->isNative();
}

static JS_ALWAYS_INLINE bool
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
static JS_ALWAYS_INLINE bool
ClassMethodIsNative(JSContext *cx, JSObject *obj, Class *clasp, jsid methodid, JSNative native)
{
    JS_ASSERT(!obj->isProxy());
    JS_ASSERT(obj->getClass() == clasp);

    Value v;
    if (!HasDataProperty(cx, obj, methodid, &v)) {
        JSObject *proto = obj->getProto();
        if (!proto || proto->getClass() != clasp || !HasDataProperty(cx, proto, methodid, &v))
            return false;
    }

    return js::IsNativeFunction(v, native);
}

/* ES5 9.1 ToPrimitive(input). */
static JS_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, MutableHandleValue vp)
{
    if (vp.isPrimitive())
        return true;

    JSObject *obj = &vp.toObject();

    /* Optimize new String(...).valueOf(). */
    if (obj->isString()) {
        jsid id = NameToId(cx->names().valueOf);
        if (ClassMethodIsNative(cx, obj, &StringClass, id, js_str_toString)) {
            vp.setString(obj->asString().unbox());
            return true;
        }
    }

    /* Optimize new Number(...).valueOf(). */
    if (obj->isNumber()) {
        jsid id = NameToId(cx->names().valueOf);
        if (ClassMethodIsNative(cx, obj, &NumberClass, id, js_num_valueOf)) {
            vp.setNumber(obj->asNumber().unbox());
            return true;
        }
    }

    RootedObject objRoot(cx, obj);
    return JSObject::defaultValue(cx, objRoot, JSTYPE_VOID, vp);
}

/* ES5 9.1 ToPrimitive(input, PreferredType). */
static JS_ALWAYS_INLINE bool
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
    JSFunction *fun = funobj->toFunction();
    return fun->isLambda() && !funobj->getParent();
}

class AutoPropDescArrayRooter : private AutoGCRooter
{
  public:
    AutoPropDescArrayRooter(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTORS), descriptors(cx), skip(cx, &descriptors)
    { }

    PropDesc *append() {
        if (!descriptors.append(PropDesc()))
            return NULL;
        return &descriptors.back();
    }

    bool reserve(size_t n) {
        return descriptors.reserve(n);
    }

    PropDesc& operator[](size_t i) {
        JS_ASSERT(i < descriptors.length());
        return descriptors[i];
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    PropDescArray descriptors;
    SkipRoot skip;
};

class AutoPropertyDescriptorRooter : private AutoGCRooter, public PropertyDescriptor
{
    SkipRoot skip;

    AutoPropertyDescriptorRooter *thisDuringConstruction() { return this; }

  public:
    AutoPropertyDescriptorRooter(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTOR), skip(cx, thisDuringConstruction())
    {
        obj = NULL;
        attrs = 0;
        getter = (PropertyOp) NULL;
        setter = (StrictPropertyOp) NULL;
        value.setUndefined();
    }

    AutoPropertyDescriptorRooter(JSContext *cx, PropertyDescriptor *desc)
      : AutoGCRooter(cx, DESCRIPTOR), skip(cx, thisDuringConstruction())
    {
        obj = desc->obj;
        attrs = desc->attrs;
        getter = desc->getter;
        setter = desc->setter;
        value = desc->value;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
};

inline void
NewObjectCache::copyCachedToObject(JSObject *dst, JSObject *src, gc::AllocKind kind)
{
    js_memcpy(dst, src, gc::Arena::thingSize(kind));
#ifdef JSGC_GENERATIONAL
    Shape::writeBarrierPost(dst->shape_, &dst->shape_);
    types::TypeObject::writeBarrierPost(dst->type_, &dst->type_);
#endif
}

static inline bool
CanBeFinalizedInBackground(gc::AllocKind kind, Class *clasp)
{
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the finalize kind. For example,
     * FINALIZE_OBJECT0 calls the finalizer on the main thread,
     * FINALIZE_OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundFinalized is called to prevent recursively incrementing
     * the finalize kind; kind may already be a background finalize kind.
     */
    return (!gc::IsBackgroundFinalized(kind) &&
            (!clasp->finalize || (clasp->flags & JSCLASS_BACKGROUND_FINALIZE)));
}

/*
 * Make an object with the specified prototype. If parent is null, it will
 * default to the prototype's global if the prototype is non-null.
 */
JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, TaggedProto proto, JSObject *parent,
                        gc::AllocKind allocKind, NewObjectKind newKind);

inline JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, TaggedProto proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewObjectWithGivenProto(cx, clasp, proto, parent, allocKind, newKind);
}

inline JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    return NewObjectWithGivenProto(cx, clasp, TaggedProto(proto), parent, newKind);
}

inline JSProtoKey
GetClassProtoKey(js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

inline bool
FindProto(JSContext *cx, js::Class *clasp, MutableHandleObject proto)
{
    JSProtoKey protoKey = GetClassProtoKey(clasp);
    if (!js_GetClassPrototype(cx, protoKey, proto, clasp))
        return false;
    if (!proto && !js_GetClassPrototype(cx, JSProto_Object, proto))
        return false;
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
NewObjectWithClassProtoCommon(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                              gc::AllocKind allocKind, NewObjectKind newKind);

inline JSObject *
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        gc::AllocKind allocKind, NewObjectKind newKind = GenericObject)
{
    return NewObjectWithClassProtoCommon(cx, clasp, proto, parent, allocKind, newKind);
}

inline JSObject *
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewObjectWithClassProto(cx, clasp, proto, parent, allocKind, newKind);
}

/*
 * Create a native instance of the given class with parent and proto set
 * according to the context's active global.
 */
inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp, gc::AllocKind allocKind,
                        NewObjectKind newKind = GenericObject)
{
    return NewObjectWithClassProto(cx, clasp, NULL, NULL, allocKind, newKind);
}

inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp, NewObjectKind newKind = GenericObject)
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    return NewBuiltinClassInstance(cx, clasp, allocKind, newKind);
}

bool
FindClassPrototype(JSContext *cx, HandleObject scope, JSProtoKey protoKey,
                   MutableHandleObject protop, Class *clasp);

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
JSObject *
NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind allocKind,
                  NewObjectKind newKind = GenericObject);

// Used to optimize calls to (new Object())
bool
NewObjectScriptedCall(JSContext *cx, MutableHandleObject obj);

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, HandleObject baseobj, NewObjectKind newKind = GenericObject)
{
    JS_ASSERT(baseobj->getClass() == &ObjectClass);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind allocKind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    allocKind = gc::GetBackgroundAllocKind(allocKind);
    JS_ASSERT_IF(baseobj->isTenured(), allocKind == baseobj->tenuredGetAllocKind());
    RootedObject obj(cx);
    obj = NewBuiltinClassInstance(cx, &ObjectClass, allocKind, newKind);
    if (!obj)
        return NULL;

    RootedObject metadata(cx, obj->getMetadata());
    RootedShape lastProp(cx, baseobj->lastProperty());
    if (!JSObject::setLastProperty(cx, obj, lastProp))
        return NULL;
    if (metadata && !JSObject::setMetadata(cx, obj, metadata))
        return NULL;

    return obj;
}

JSObject *
NewReshapedObject(JSContext *cx, HandleTypeObject type, JSObject *parent,
                  gc::AllocKind kind, HandleShape shape);

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
DefineConstructorAndPrototype(JSContext *cx, Handle<GlobalObject*> global,
                              JSProtoKey key, HandleObject ctor, HandleObject proto)
{
    JS_ASSERT(!global->nativeEmpty()); /* reserved slots already allocated */
    JS_ASSERT(ctor);
    JS_ASSERT(proto);

    RootedId id(cx, NameToId(ClassName(key, cx)));
    JS_ASSERT(!global->nativeLookup(cx, id));

    /* Set these first in case AddTypePropertyId looks for this class. */
    global->setSlot(key, ObjectValue(*ctor));
    global->setSlot(key + JSProto_LIMIT, ObjectValue(*proto));
    global->setSlot(key + JSProto_LIMIT * 2, ObjectValue(*ctor));

    types::AddTypePropertyId(cx, global, id, ObjectValue(*ctor));
    if (!global->addDataProperty(cx, id, key + JSProto_LIMIT * 2, 0)) {
        global->setSlot(key, UndefinedValue());
        global->setSlot(key + JSProto_LIMIT, UndefinedValue());
        global->setSlot(key + JSProto_LIMIT * 2, UndefinedValue());
        return false;
    }

    return true;
}

inline bool
ObjectClassIs(HandleObject obj, ESClassValue classValue, JSContext *cx)
{
    if (JS_UNLIKELY(obj->isProxy()))
        return Proxy::objectClassIs(obj, classValue, cx);

    switch (classValue) {
      case ESClass_Array: return obj->isArray();
      case ESClass_Number: return obj->isNumber();
      case ESClass_String: return obj->isString();
      case ESClass_Boolean: return obj->isBoolean();
      case ESClass_RegExp: return obj->isRegExp();
      case ESClass_ArrayBuffer: return obj->isArrayBuffer();
      case ESClass_Date: return obj->isDate();
    }
    JS_NOT_REACHED("bad classValue");
    return false;
}

inline bool
IsObjectWithClass(const Value &v, ESClassValue classValue, JSContext *cx)
{
    if (!v.isObject())
        return false;
    RootedObject obj(cx, &v.toObject());
    return ObjectClassIs(obj, classValue, cx);
}

static JS_ALWAYS_INLINE bool
ValueMightBeSpecial(const Value &propval)
{
    return propval.isObject();
}

static JS_ALWAYS_INLINE bool
ValueIsSpecial(JSObject *obj, MutableHandleValue propval, MutableHandle<SpecialId> sidp,
               JSContext *cx)
{
    return false;
}

JSObject *
DefineConstructorAndPrototype(JSContext *cx, HandleObject obj, JSProtoKey key, HandleAtom atom,
                              JSObject *protoProto, Class *clasp,
                              Native constructor, unsigned nargs,
                              const JSPropertySpec *ps, const JSFunctionSpec *fs,
                              const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
                              JSObject **ctorp = NULL,
                              gc::AllocKind ctorKind = JSFunction::FinalizeKind);

static JS_ALWAYS_INLINE JSObject *
NewObjectMetadata(JSContext *cx)
{
    // The metadata callback is invoked before each created object, except when
    // analysis is active as the callback may reenter JS.
    if (JS_UNLIKELY((size_t)cx->compartment()->objectMetadataCallback) &&
        !cx->compartment()->activeAnalysis)
    {
        gc::AutoSuppressGC suppress(cx);
        return cx->compartment()->objectMetadataCallback(cx);
    }
    return NULL;
}

} /* namespace js */

extern JSObject *
js_InitClass(JSContext *cx, js::HandleObject obj, JSObject *parent_proto,
             js::Class *clasp, JSNative constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
             JSObject **ctorp = NULL,
             js::gc::AllocKind ctorKind = JSFunction::FinalizeKind);

/*
 * js_PurgeScopeChain does nothing if obj is not itself a prototype or parent
 * scope, else it reshapes the scope and prototype chains it links. It calls
 * js_PurgeScopeChainHelper, which asserts that obj is flagged as a delegate
 * (i.e., obj has ever been on a prototype or parent chain).
 */
extern bool
js_PurgeScopeChainHelper(JSContext *cx, JS::HandleObject obj, JS::HandleId id);

inline bool
js_PurgeScopeChain(JSContext *cx, JS::HandleObject obj, JS::HandleId id)
{
    if (obj->isDelegate())
        return js_PurgeScopeChainHelper(cx, obj, id);
    return true;
}

inline void
js::DestroyIdArray(FreeOp *fop, JSIdArray *ida)
{
    fop->free_(ida);
}

#endif /* jsobjinlines_h___ */
