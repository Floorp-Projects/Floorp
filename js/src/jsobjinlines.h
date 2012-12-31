/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobjinlines_h___
#define jsobjinlines_h___

#include "jsapi.h"
#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsprobes.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstypedarray.h"
#include "jsxml.h"
#include "jswrapper.h"

#include "builtin/MapObject.h"
#include "builtin/Iterator-inl.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Root.h"
#include "js/MemoryMetrics.h"
#include "js/TemplateLib.h"
#include "vm/BooleanObject.h"
#include "vm/GlobalObject.h"
#include "vm/NumberObject.h"
#include "vm/RegExpStatics.h"
#include "vm/StringObject.h"

#include "jsatominlines.h"
#include "jscompartmentinlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "gc/Barrier-inl.h"

#include "vm/ObjectImpl-inl.h"
#include "vm/RegExpStatics-inl.h"
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

/* static */ inline JSType
JSObject::typeOf(JSContext *cx, js::HandleObject obj)
{
    js::TypeOfOp op = obj->getOps()->typeOf;
    return (op ? op : js::baseops::TypeOf)(cx, obj);
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
    js::RootedId id(cx, js::NameToId(name));
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
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
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
    js::RootedId id(cx, js::NameToId(name));
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
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return setGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline bool
JSObject::changePropertyAttributes(JSContext *cx, js::HandleObject obj,
                                   js::Shape *shape, unsigned attrs)
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
JSObject::getProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                      js::PropertyName *name, js::MutableHandleValue vp)
{
    js::RootedId id(cx, js::NameToId(name));
    return getGeneric(cx, obj, receiver, id, vp);
}

/* static */ inline bool
JSObject::deleteProperty(JSContext *cx, js::HandleObject obj,
                         js::HandlePropertyName name, js::MutableHandleValue rval, bool strict)
{
    js::RootedId id(cx, js::NameToId(name));
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeletePropertyOp op = obj->getOps()->deleteProperty;
    return (op ? op : js::baseops::DeleteProperty)(cx, obj, name, rval, strict);
}

/* static */ inline bool
JSObject::deleteElement(JSContext *cx, js::HandleObject obj,
                        uint32_t index, js::MutableHandleValue rval, bool strict)
{
    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeleteElementOp op = obj->getOps()->deleteElement;
    return (op ? op : js::baseops::DeleteElement)(cx, obj, index, rval, strict);
}

/* static */ inline bool
JSObject::deleteSpecial(JSContext *cx, js::HandleObject obj,
                        js::HandleSpecialId sid, js::MutableHandleValue rval, bool strict)
{
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
    js::types::AddTypePropertyId(cx, obj, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, obj, id);
    js::DeleteSpecialOp op = obj->getOps()->deleteSpecial;
    return (op ? op : js::baseops::DeleteSpecial)(cx, obj, sid, rval, strict);
}

inline void
JSObject::finalize(js::FreeOp *fop)
{
    js::Probes::finalizeObject(this);

    if (!IsBackgroundFinalized(getAllocKind())) {
        /* Assert we're on the main thread. */
        fop->runtime()->assertValidThread();

        /*
         * Finalize obj first, in case it needs map and slots. Objects with
         * finalize hooks are not finalized in the background, as the class is
         * stored in the object's shape, which may have already been destroyed.
         */
        js::Class *clasp = getClass();
        if (clasp->finalize)
            clasp->finalize(fop, this);
    }

    finish(fop);
}

inline JSObject *
JSObject::getParent() const
{
    return lastProperty()->getObjectParent();
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
JSObject::setLastPropertyInfallible(js::UnrootedShape shape)
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
    js::RootedObject self(cx, this);
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
    js::UnrootedShape previous = lastProperty()->previous().get();
    return previous->getObjectParent() == lastProperty()->getObjectParent()
        && previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline const js::HeapSlot *
JSObject::getRawSlots()
{
    JS_ASSERT(isGlobal());
    return slots;
}

inline const js::Value &
JSObject::getReservedSlot(unsigned index) const
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    return getSlot(index);
}

inline js::HeapSlot &
JSObject::getReservedSlotRef(unsigned index)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    return getSlotRef(index);
}

inline void
JSObject::setReservedSlot(unsigned index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    setSlot(index, v);
}

inline void
JSObject::initReservedSlot(unsigned index, const js::Value &v)
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

/* static */ inline void
JSObject::setArrayLength(JSContext *cx, js::HandleObject obj, uint32_t length)
{
    JS_ASSERT(obj->isArray());

    if (length > INT32_MAX) {
        /*
         * Mark the type of this object as possibly not a dense array, per the
         * requirements of OBJECT_FLAG_NON_DENSE_ARRAY.
         */
        js::types::MarkTypeObjectFlags(cx, obj,
                                       js::types::OBJECT_FLAG_NON_PACKED_ARRAY |
                                       js::types::OBJECT_FLAG_NON_DENSE_ARRAY);
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
    return getElementsHeader()->capacity;
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
JSObject::setDenseElement(unsigned idx, const js::Value &val)
{
    JS_ASSERT(isNative() && idx < getDenseInitializedLength());
    elements[idx].set(this, js::HeapSlot::Element, idx, val);
}

inline void
JSObject::initDenseElement(unsigned idx, const js::Value &val)
{
    JS_ASSERT(isNative() && idx < getDenseInitializedLength());
    elements[idx].init(this, js::HeapSlot::Element, idx, val);
}

/* static */ inline void
JSObject::setDenseElementWithType(JSContext *cx, js::HandleObject obj, unsigned idx,
                                  const js::Value &val)
{
    js::types::AddTypePropertyId(cx, obj, JSID_VOID, val);
    obj->setDenseElement(idx, val);
}

/* static */ inline void
JSObject::initDenseElementWithType(JSContext *cx, js::HandleObject obj, unsigned idx,
                                   const js::Value &val)
{
    js::types::AddTypePropertyId(cx, obj, JSID_VOID, val);
    obj->initDenseElement(idx, val);
}

/* static */ inline void
JSObject::setDenseElementHole(JSContext *cx, js::HandleObject obj, unsigned idx)
{
    js::types::MarkTypeObjectFlags(cx, obj, js::types::OBJECT_FLAG_NON_PACKED_ARRAY);
    obj->setDenseElement(idx, js::MagicValue(JS_ELEMENTS_HOLE));
}

/* static */ inline void
JSObject::removeDenseElementForSparseIndex(JSContext *cx, js::HandleObject obj, unsigned idx)
{
    js::types::MarkTypeObjectFlags(cx, obj,
                                   js::types::OBJECT_FLAG_NON_PACKED_ARRAY |
                                   js::types::OBJECT_FLAG_NON_DENSE_ARRAY);
    if (obj->containsDenseElement(idx))
        obj->setDenseElement(idx, js::MagicValue(JS_ELEMENTS_HOLE));
}

inline void
JSObject::copyDenseElements(unsigned dstStart, const js::Value *src, unsigned count)
{
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JSCompartment *comp = compartment();
    for (unsigned i = 0; i < count; ++i)
        elements[dstStart + i].set(comp, this, js::HeapSlot::Element, dstStart + i, src[i]);
}

inline void
JSObject::initDenseElements(unsigned dstStart, const js::Value *src, unsigned count)
{
    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JSCompartment *comp = compartment();
    for (unsigned i = 0; i < count; ++i)
        elements[dstStart + i].init(comp, this, js::HeapSlot::Element, dstStart + i, src[i]);
}

inline void
JSObject::moveDenseElements(unsigned dstStart, unsigned srcStart, unsigned count)
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
    JSCompartment *comp = compartment();
    if (comp->needsBarrier()) {
        if (dstStart < srcStart) {
            js::HeapSlot *dst = elements + dstStart;
            js::HeapSlot *src = elements + srcStart;
            for (unsigned i = 0; i < count; i++, dst++, src++)
                dst->set(comp, this, js::HeapSlot::Element, dst - elements, *src);
        } else {
            js::HeapSlot *dst = elements + dstStart + count - 1;
            js::HeapSlot *src = elements + srcStart + count - 1;
            for (unsigned i = 0; i < count; i++, dst--, src--)
                dst->set(comp, this, js::HeapSlot::Element, dst - elements, *src);
        }
    } else {
        memmove(elements + dstStart, elements + srcStart, count * sizeof(js::HeapSlot));
        DenseRangeWriteBarrierPost(comp, this, dstStart, count);
    }
}

inline void
JSObject::moveDenseElementsUnbarriered(unsigned dstStart, unsigned srcStart, unsigned count)
{
    JS_ASSERT(!compartment()->needsBarrier());

    JS_ASSERT(dstStart + count <= getDenseCapacity());
    JS_ASSERT(srcStart + count <= getDenseCapacity());

    memmove(elements + dstStart, elements + srcStart, count * sizeof(js::Value));
}

inline void
JSObject::markDenseElementsNotPacked(JSContext *cx)
{
    JS_ASSERT(isNative());
    MarkTypeObjectFlags(cx, this, js::types::OBJECT_FLAG_NON_PACKED_ARRAY);
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
        JSCompartment *comp = compartment();
        size_t offset = initlen;
        for (js::HeapSlot *sp = elements + initlen;
             sp != elements + (index + extra);
             sp++, offset++)
            sp->init(comp, this, js::HeapSlot::Element, offset, js::MagicValue(JS_ELEMENTS_HOLE));
        initlen = index + extra;
    }
}

inline JSObject::EnsureDenseResult
JSObject::ensureDenseElements(JSContext *cx, unsigned index, unsigned extra)
{
    JS_ASSERT(isNative());

    unsigned currentCapacity = getDenseCapacity();

    unsigned requiredCapacity;
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

    /*
     * Don't grow elements for non-extensible objects or watched objects. Dense
     * elements can be added/written with no extensible or watchpoint checks as
     * long as there is capacity for them.
     */
    if (!isExtensible() || watched()) {
        JS_ASSERT(currentCapacity == 0);
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

#if JS_HAS_XML_SUPPORT

inline JSLinearString *
JSObject::getNamePrefix() const
{
    JS_ASSERT(isNamespace() || isQName());
    const js::Value &v = getSlot(JSSLOT_NAME_PREFIX);
    return !v.isUndefined() ? &v.toString()->asLinear() : NULL;
}

inline jsval
JSObject::getNamePrefixVal() const
{
    JS_ASSERT(isNamespace() || isQName());
    return getSlot(JSSLOT_NAME_PREFIX);
}

inline void
JSObject::setNamePrefix(JSLinearString *prefix)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_PREFIX, prefix ? js::StringValue(prefix) : js::UndefinedValue());
}

inline void
JSObject::clearNamePrefix()
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_PREFIX, js::UndefinedValue());
}

inline JSLinearString *
JSObject::getNameURI() const
{
    JS_ASSERT(isNamespace() || isQName());
    const js::Value &v = getSlot(JSSLOT_NAME_URI);
    return !v.isUndefined() ? &v.toString()->asLinear() : NULL;
}

inline jsval
JSObject::getNameURIVal() const
{
    JS_ASSERT(isNamespace() || isQName());
    return getSlot(JSSLOT_NAME_URI);
}

inline void
JSObject::setNameURI(JSLinearString *uri)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_URI, uri ? js::StringValue(uri) : js::UndefinedValue());
}

inline jsval
JSObject::getNamespaceDeclared() const
{
    JS_ASSERT(isNamespace());
    return getSlot(JSSLOT_NAMESPACE_DECLARED);
}

inline void
JSObject::setNamespaceDeclared(jsval decl)
{
    JS_ASSERT(isNamespace());
    setSlot(JSSLOT_NAMESPACE_DECLARED, decl);
}

inline JSAtom *
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    const js::Value &v = getSlot(JSSLOT_QNAME_LOCAL_NAME);
    return !v.isUndefined() ? &v.toString()->asAtom() : NULL;
}

inline jsval
JSObject::getQNameLocalNameVal() const
{
    JS_ASSERT(isQName());
    return getSlot(JSSLOT_QNAME_LOCAL_NAME);
}

inline void
JSObject::setQNameLocalName(JSAtom *name)
{
    JS_ASSERT(isQName());
    setSlot(JSSLOT_QNAME_LOCAL_NAME, name ? js::StringValue(name) : js::UndefinedValue());
}

#endif

/* static */ inline bool
JSObject::setSingletonType(JSContext *cx, js::HandleObject obj)
{
    if (!cx->typeInferenceEnabled())
        return true;

    JS_ASSERT(!obj->hasLazyType());
    JS_ASSERT_IF(obj->getTaggedProto().isObject(),
                 obj->type() == obj->getTaggedProto().toObject()->getNewType(cx, NULL));

    js::Rooted<js::TaggedProto> objProto(cx, obj->getTaggedProto());
    js::types::TypeObject *type = cx->compartment->getLazyType(cx, objProto);
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline js::types::TypeObject *
JSObject::getType(JSContext *cx)
{
    JS_ASSERT(cx->compartment == compartment());
    if (hasLazyType())
        return makeLazyType(cx);
    return type_;
}

/* static */ inline bool
JSObject::clearType(JSContext *cx, js::HandleObject obj)
{
    JS_ASSERT(!obj->hasSingletonType());
    JS_ASSERT(cx->compartment == obj->compartment());

    js::types::TypeObject *type = cx->compartment->getNewType(cx, NULL);
    if (!type)
        return false;

    obj->type_ = type;
    return true;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
    JS_ASSERT(newType);
    JS_ASSERT_IF(hasSpecialEquality(),
                 newType->hasAnyFlags(js::types::OBJECT_FLAG_SPECIAL_EQUALITY));
    JS_ASSERT_IF(getClass()->emulatesUndefined(),
                 newType->hasAnyFlags(js::types::OBJECT_FLAG_EMULATES_UNDEFINED));
    JS_ASSERT(!hasSingletonType());
    JS_ASSERT(compartment() == newType->compartment());
    type_ = newType;
}

inline js::TaggedProto
JSObject::getTaggedProto() const
{
    return js::TaggedProto(js::ObjectImpl::getProto());
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
        return js::Proxy::getPrototypeOf(cx, obj, protop.address());
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

inline bool JSObject::hasSpecialEquality() const
{
    return !!getClass()->ext.equality;
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
inline bool JSObject::isSetIterator() const { return hasClass(&js::SetIteratorClass); }
inline bool JSObject::isStaticBlock() const { return isBlock() && !getProto(); }
inline bool JSObject::isStopIteration() const { return hasClass(&js::StopIterationClass); }
inline bool JSObject::isStrictArguments() const { return hasClass(&js::StrictArgumentsObjectClass); }
inline bool JSObject::isString() const { return hasClass(&js::StringClass); }
inline bool JSObject::isTypedArray() const { return IsTypedArrayClass(getClass()); }
inline bool JSObject::isWeakMap() const { return hasClass(&js::WeakMapClass); }
inline bool JSObject::isWith() const { return hasClass(&js::WithClass); }

inline bool
JSObject::isDebugScope() const
{
    extern bool js_IsDebugScopeSlow(js::RawObject obj);
    return getClass() == &js::ObjectProxyClass && js_IsDebugScopeSlow(const_cast<JSObject*>(this));
}

#if JS_HAS_XML_SUPPORT
inline bool JSObject::isNamespace() const { return hasClass(&js::NamespaceClass); }
inline bool JSObject::isXML() const { return hasClass(&js::XMLClass); }

inline bool
JSObject::isXMLId() const
{
    return hasClass(&js::QNameClass)
        || hasClass(&js::AttributeNameClass)
        || hasClass(&js::AnyNameClass);
}

inline bool
JSObject::isQName() const
{
    return hasClass(&js::QNameClass)
        || hasClass(&js::AttributeNameClass)
        || hasClass(&js::AnyNameClass);
}
#endif /* JS_HAS_XML_SUPPORT */

/* static */ inline JSObject *
JSObject::create(JSContext *cx, js::gc::AllocKind kind,
                 js::HandleShape shape, js::HandleTypeObject type, js::HeapSlot *slots)
{
    /*
     * Callers must use dynamicSlotsCount to size the initial slot array of the
     * object. We can't check the allocated capacity of the dynamic slots, but
     * make sure their presence is consistent with the shape.
     */
    JS_ASSERT(shape && type);
    JS_ASSERT(shape->getObjectClass() != &js::ArrayClass);
    JS_ASSERT(!!dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan()) == !!slots);
    JS_ASSERT(js::gc::GetGCKindSlots(kind, shape->getObjectClass()) == shape->numFixedSlots());
    JS_ASSERT(cx->compartment == type->compartment());

    JSObject *obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    obj->shape_.init(shape);
    obj->type_.init(type);
    obj->slots = slots;
    obj->elements = js::emptyObjectElements;

    const js::Class *clasp = shape->getObjectClass();
    if (clasp->hasPrivate())
        obj->privateRef(shape->numFixedSlots()) = NULL;

    size_t span = shape->slotSpan();
    if (span && clasp != &js::ArrayBufferClass)
        obj->initializeSlotRange(0, span);

    return obj;
}

/* static */ inline JSObject *
JSObject::createArray(JSContext *cx, js::gc::AllocKind kind,
                      js::HandleShape shape, js::HandleTypeObject type,
                      uint32_t length)
{
    JS_ASSERT(shape && type);
    JS_ASSERT(shape->getObjectClass() == &js::ArrayClass);
    JS_ASSERT(cx->compartment == type->compartment());

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

    JSObject *obj = js_NewGCObject(cx, kind);
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
    if (hasDynamicElements())
        fop->free_(getElementsHeader());
}

/* static */ inline bool
JSObject::hasProperty(JSContext *cx, js::HandleObject obj,
                      js::HandleId id, bool *foundp, unsigned flags)
{
    js::RootedObject pobj(cx);
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
    return isFunction() || getClass()->call;
}

inline void
JSObject::nativeSetSlot(unsigned slot, const js::Value &value)
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

inline size_t
JSObject::computedSizeOfThisSlotsElements() const
{
    size_t n = sizeOfThis();

    if (hasDynamicSlots())
        n += numDynamicSlots() * sizeof(js::Value);

    if (hasDynamicElements())
        n += (js::ObjectElements::VALUES_PER_HEADER + getElementsHeader()->capacity) *
             sizeof(js::Value);

    return n;
}

inline void
JSObject::sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf, JS::ObjectsExtraSizes *sizes)
{
    if (hasDynamicSlots())
        sizes->slots = mallocSizeOf(slots);

    if (hasDynamicElements())
        sizes->elements = mallocSizeOf(getElementsHeader());

    // Other things may be measured in the future if DMD indicates it is worthwhile.
    // Note that sizes->private_ is measured elsewhere.
    if (isArguments()) {
        sizes->argumentsData = asArguments().sizeOfMisc(mallocSizeOf);
    } else if (isRegExpStatics()) {
        sizes->regExpStatics = js::SizeOfRegExpStaticsData(this, mallocSizeOf);
    } else if (isPropertyIterator()) {
        sizes->propertyIteratorData = asPropertyIterator().sizeOfMisc(mallocSizeOf);
#ifdef JS_HAS_CTYPES
    } else {
        // This must be the last case.
        sizes->ctypesData = js::SizeOfDataIfCDataObject(mallocSizeOf, const_cast<JSObject *>(this));
#endif
    }
}

/* static */ inline JSBool
JSObject::lookupGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::LookupGenericOp op = obj->getOps()->lookupGeneric;
    if (op)
        return op(cx, obj, id, objp, propp);
    return js::baseops::LookupProperty(cx, obj, id, objp, propp);
}

/* static */ inline JSBool
JSObject::lookupProperty(JSContext *cx, js::HandleObject obj, js::PropertyName *name,
                         js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::RootedId id(cx, js::NameToId(name));
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
    js::RootedId id(cx, js::NameToId(name));
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
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
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
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return lookupGeneric(cx, obj, id, objp, propp);
}

/* static */ inline JSBool
JSObject::getElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     uint32_t index, js::MutableHandleValue vp)
{
    js::ElementIdOp op = obj->getOps()->getElement;
    if (op)
        return op(cx, obj, receiver, index, vp);

    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGeneric(cx, obj, receiver, id, vp);
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
    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;

    js::RootedObject obj2(cx);
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
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
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
    js::RootedId id(cx, js::NameToId(name));
    return getGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::getElementAttributes(JSContext *cx, js::HandleObject obj,
                               uint32_t index, unsigned *attrsp)
{
    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGenericAttributes(cx, obj, id, attrsp);
}

/* static */ inline JSBool
JSObject::getSpecialAttributes(JSContext *cx, js::HandleObject obj,
                               js::SpecialId sid, unsigned *attrsp)
{
    js::RootedId id(cx, SPECIALID_TO_JSID(sid));
    return getGenericAttributes(cx, obj, id, attrsp);
}

inline bool
JSObject::isProxy() const
{
    return js::IsProxy(const_cast<JSObject*>(this));
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
    JS_ASSERT(&obj->asGlobal() == compartment()->maybeGlobal());
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

#if JS_HAS_XML_SUPPORT
/*
 * Methods to test whether an object or a value is of type "xml" (per typeof).
 */

#define VALUE_IS_XML(v)     ((v).isObject() && (v).toObject().isXML())

static inline bool
IsXML(const js::Value &v)
{
    return v.isObject() && v.toObject().isXML();
}

#else

#define VALUE_IS_XML(v)     false

#endif /* JS_HAS_XML_SUPPORT */

static inline bool
IsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().isStopIteration();
}

/* ES5 9.1 ToPrimitive(input). */
static JS_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, Value *vp)
{
    if (vp->isPrimitive())
        return true;
    RootedObject obj(cx, &vp->toObject());
    RootedValue value(cx, *vp);
    if (!JSObject::defaultValue(cx, obj, JSTYPE_VOID, &value))
        return false;
    *vp = value;
    return true;
}

/* ES5 9.1 ToPrimitive(input, PreferredType). */
static JS_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, JSType preferredType, Value *vp)
{
    JS_ASSERT(preferredType != JSTYPE_VOID); /* Use the other ToPrimitive! */
    if (vp->isPrimitive())
        return true;
    RootedObject obj(cx, &vp->toObject());
    RootedValue value(cx, *vp);
    if (!JSObject::defaultValue(cx, obj, preferredType, &value))
        return false;
    *vp = value;
    return true;
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
NewObjectCache::copyCachedToObject(JSObject *dst, JSObject *src)
{
    js_memcpy(dst, src, dst->sizeOfThis());
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
    return (!gc::IsBackgroundFinalized(kind) && !clasp->finalize);
}

/*
 * Make an object with the specified prototype. If parent is null, it will
 * default to the prototype's global if the prototype is non-null.
 */
JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, TaggedProto proto, JSObject *parent,
                        gc::AllocKind kind);

inline JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, TaggedProto proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObjectWithGivenProto(cx, clasp, proto, parent, kind);
}

inline JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    return NewObjectWithGivenProto(cx, clasp, TaggedProto(proto), parent);
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
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        gc::AllocKind kind);

inline JSObject *
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObjectWithClassProto(cx, clasp, proto, parent, kind);
}

/*
 * Create a native instance of the given class with parent and proto set
 * according to the context's active global.
 */
inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp, gc::AllocKind kind)
{
    return NewObjectWithClassProto(cx, clasp, NULL, NULL, kind);
}

inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewBuiltinClassInstance(cx, clasp, kind);
}

bool
FindClassPrototype(JSContext *cx, HandleObject scope, JSProtoKey protoKey,
                   MutableHandleObject protop, Class *clasp);

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
JSObject *
NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind kind);

// Used to optimize calls to (new Object())
bool
NewObjectScriptedCall(JSContext *cx, MutableHandleObject obj);

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, HandleObject baseobj)
{
    JS_ASSERT(baseobj->getClass() == &ObjectClass);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind kind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    kind = gc::GetBackgroundAllocKind(kind);
    JS_ASSERT(kind == baseobj->getAllocKind());
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ObjectClass, kind));

    if (!obj)
        return NULL;

    RootedShape lastProp(cx, baseobj->lastProperty());
    if (!JSObject::setLastProperty(cx, obj, lastProp))
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

/*
 * Fill slots with the initial slot array to use for a newborn object which
 * may or may not need dynamic slots.
 */
inline bool
PreallocateObjectDynamicSlots(JSContext *cx, UnrootedShape shape, HeapSlot **slots)
{
    if (size_t count = JSObject::dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan())) {
        *slots = cx->pod_malloc<HeapSlot>(count);
        if (!*slots)
            return false;
        Debug_SetSlotRangeToCrashOnTouch(*slots, count);
        return true;
    }
    *slots = NULL;
    return true;
}

inline bool
DefineConstructorAndPrototype(JSContext *cx, Handle<GlobalObject*> global,
                              JSProtoKey key, JSObject *ctor, JSObject *proto)
{
    JS_ASSERT(!global->nativeEmpty()); /* reserved slots already allocated */
    JS_ASSERT(ctor);
    JS_ASSERT(proto);

    RootedId id(cx, NameToId(ClassName(key, cx)));
    JS_ASSERT(!global->nativeLookupNoAllocation(id));

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
ObjectClassIs(JSObject &obj, ESClassValue classValue, JSContext *cx)
{
    if (JS_UNLIKELY(obj.isProxy()))
        return Proxy::objectClassIs(&obj, classValue, cx);

    switch (classValue) {
      case ESClass_Array: return obj.isArray();
      case ESClass_Number: return obj.isNumber();
      case ESClass_String: return obj.isString();
      case ESClass_Boolean: return obj.isBoolean();
      case ESClass_RegExp: return obj.isRegExp();
      case ESClass_ArrayBuffer: return obj.isArrayBuffer();
    }
    JS_NOT_REACHED("bad classValue");
    return false;
}

inline bool
IsObjectWithClass(const Value &v, ESClassValue classValue, JSContext *cx)
{
    if (!v.isObject())
        return false;
    return ObjectClassIs(v.toObject(), classValue, cx);
}

static JS_ALWAYS_INLINE bool
ValueIsSpecial(JSObject *obj, MutableHandleValue propval, MutableHandle<SpecialId> sidp,
               JSContext *cx)
{
#if JS_HAS_XML_SUPPORT
    if (!propval.isObject())
        return false;

    if (obj->isXML()) {
        sidp.set(SpecialId(propval.toObject()));
        return true;
    }

    JSObject &propobj = propval.toObject();
    JSAtom *name;
    if (propobj.isQName() && GetLocalNameFromFunctionQName(&propobj, &name, cx)) {
        propval.setString(name);
        return false;
    }
#endif

    return false;
}

JSObject *
DefineConstructorAndPrototype(JSContext *cx, HandleObject obj, JSProtoKey key, HandleAtom atom,
                              JSObject *protoProto, Class *clasp,
                              Native constructor, unsigned nargs,
                              JSPropertySpec *ps, JSFunctionSpec *fs,
                              JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
                              JSObject **ctorp = NULL,
                              gc::AllocKind ctorKind = JSFunction::FinalizeKind);

} /* namespace js */

extern JSObject *
js_InitClass(JSContext *cx, js::HandleObject obj, JSObject *parent_proto,
             js::Class *clasp, JSNative constructor, unsigned nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
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
