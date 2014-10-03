/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectImpl_inl_h
#define vm_ObjectImpl_inl_h

#include "vm/ObjectImpl.h"

#include "jscntxt.h"

#include "builtin/TypedObject.h"
#include "proxy/Proxy.h"
#include "vm/ProxyObject.h"
#include "vm/TypedArrayObject.h"

#include "jsobjinlines.h"

namespace js {

inline uint8_t *
NativeObject::fixedData(size_t nslots) const
{
    MOZ_ASSERT(ClassCanHaveFixedData(getClass()));
    MOZ_ASSERT(nslots == numFixedSlots() + (hasPrivate() ? 1 : 0));
    return reinterpret_cast<uint8_t *>(&fixedSlots()[nslots]);
}

/* static */ inline bool
NativeObject::changePropertyAttributes(JSContext *cx, HandleNativeObject obj,
                                       HandleShape shape, unsigned attrs)
{
    return !!changeProperty<SequentialExecution>(cx, obj, shape, attrs, 0,
                                                 shape->getter(), shape->setter());
}

inline void
NativeObject::removeLastProperty(ExclusiveContext *cx)
{
    MOZ_ASSERT(canRemoveLastProperty());
    RootedNativeObject self(cx, this);
    RootedShape prev(cx, lastProperty()->previous());
    JS_ALWAYS_TRUE(setLastProperty(cx, self, prev));
}

inline bool
NativeObject::canRemoveLastProperty()
{
    /*
     * Check that the information about the object stored in the last
     * property's base shape is consistent with that stored in the previous
     * shape. If not consistent, then the last property cannot be removed as it
     * will induce a change in the object itself, and the object must be
     * converted to dictionary mode instead. See BaseShape comment in jsscope.h
     */
    MOZ_ASSERT(!inDictionaryMode());
    Shape *previous = lastProperty()->previous().get();
    return previous->getObjectParent() == lastProperty()->getObjectParent()
        && previous->getObjectMetadata() == lastProperty()->getObjectMetadata()
        && previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline void
NativeObject::setShouldConvertDoubleElements()
{
    MOZ_ASSERT(is<ArrayObject>() && !hasEmptyElements());
    getElementsHeader()->setShouldConvertDoubleElements();
}

inline void
NativeObject::clearShouldConvertDoubleElements()
{
    MOZ_ASSERT(is<ArrayObject>() && !hasEmptyElements());
    getElementsHeader()->clearShouldConvertDoubleElements();
}

inline bool
NativeObject::setDenseElementIfHasType(uint32_t index, const Value &val)
{
    if (!types::HasTypePropertyId(this, JSID_VOID, val))
        return false;
    setDenseElementMaybeConvertDouble(index, val);
    return true;
}

inline void
NativeObject::setDenseElementWithType(ExclusiveContext *cx, uint32_t index,
                                      const Value &val)
{
    // Avoid a slow AddTypePropertyId call if the type is the same as the type
    // of the previous element.
    types::Type thisType = types::GetValueType(val);
    if (index == 0 || types::GetValueType(elements[index - 1]) != thisType)
        types::AddTypePropertyId(cx, this, JSID_VOID, thisType);
    setDenseElementMaybeConvertDouble(index, val);
}

inline void
NativeObject::initDenseElementWithType(ExclusiveContext *cx, uint32_t index,
                                       const Value &val)
{
    MOZ_ASSERT(!shouldConvertDoubleElements());
    types::AddTypePropertyId(cx, this, JSID_VOID, val);
    initDenseElement(index, val);
}

inline void
NativeObject::setDenseElementHole(ExclusiveContext *cx, uint32_t index)
{
    types::MarkTypeObjectFlags(cx, this, types::OBJECT_FLAG_NON_PACKED);
    setDenseElement(index, MagicValue(JS_ELEMENTS_HOLE));
}

/* static */ inline void
NativeObject::removeDenseElementForSparseIndex(ExclusiveContext *cx,
                                               HandleNativeObject obj, uint32_t index)
{
    types::MarkTypeObjectFlags(cx, obj,
                               types::OBJECT_FLAG_NON_PACKED |
                               types::OBJECT_FLAG_SPARSE_INDEXES);
    if (obj->containsDenseElement(index))
        obj->setDenseElement(index, MagicValue(JS_ELEMENTS_HOLE));
}

inline bool
NativeObject::writeToIndexWouldMarkNotPacked(uint32_t index)
{
    return getElementsHeader()->initializedLength < index;
}

inline void
NativeObject::markDenseElementsNotPacked(ExclusiveContext *cx)
{
    MOZ_ASSERT(isNative());
    MarkTypeObjectFlags(cx, this, types::OBJECT_FLAG_NON_PACKED);
}

inline void
NativeObject::ensureDenseInitializedLengthNoPackedCheck(ThreadSafeContext *cx, uint32_t index,
                                                        uint32_t extra)
{
    MOZ_ASSERT(cx->isThreadLocal(this));
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());

    /*
     * Ensure that the array's contents have been initialized up to index, and
     * mark the elements through 'index + extra' as initialized in preparation
     * for a write.
     */
    MOZ_ASSERT(index + extra <= getDenseCapacity());
    uint32_t &initlen = getElementsHeader()->initializedLength;

    if (initlen < index + extra) {
        size_t offset = initlen;
        for (HeapSlot *sp = elements + initlen;
             sp != elements + (index + extra);
             sp++, offset++)
        {
            sp->init(this, HeapSlot::Element, offset, MagicValue(JS_ELEMENTS_HOLE));
        }
        initlen = index + extra;
    }
}

inline void
NativeObject::ensureDenseInitializedLength(ExclusiveContext *cx, uint32_t index, uint32_t extra)
{
    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);
    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
}

inline void
NativeObject::ensureDenseInitializedLengthPreservePackedFlag(ThreadSafeContext *cx,
                                                             uint32_t index, uint32_t extra)
{
    MOZ_ASSERT(!writeToIndexWouldMarkNotPacked(index));
    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
}

NativeObject::EnsureDenseResult
NativeObject::extendDenseElements(ThreadSafeContext *cx,
                                  uint32_t requiredCapacity, uint32_t extra)
{
    MOZ_ASSERT(cx->isThreadLocal(this));
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());

    /*
     * Don't grow elements for non-extensible objects or watched objects. Dense
     * elements can be added/written with no extensible or watchpoint checks as
     * long as there is capacity for them.
     */
    if (!nonProxyIsExtensible() || watched()) {
        MOZ_ASSERT(getDenseCapacity() == 0);
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

inline NativeObject::EnsureDenseResult
NativeObject::ensureDenseElementsNoPackedCheck(ThreadSafeContext *cx, uint32_t index, uint32_t extra)
{
    MOZ_ASSERT(isNative());

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

inline NativeObject::EnsureDenseResult
NativeObject::ensureDenseElements(ExclusiveContext *cx, uint32_t index, uint32_t extra)
{
    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);
    return ensureDenseElementsNoPackedCheck(cx, index, extra);
}

inline NativeObject::EnsureDenseResult
NativeObject::ensureDenseElementsPreservePackedFlag(ThreadSafeContext *cx, uint32_t index,
                                                    uint32_t extra)
{
    MOZ_ASSERT(!writeToIndexWouldMarkNotPacked(index));
    return ensureDenseElementsNoPackedCheck(cx, index, extra);
}

inline Value
NativeObject::getDenseOrTypedArrayElement(uint32_t idx)
{
    if (is<TypedArrayObject>())
        return as<TypedArrayObject>().getElement(idx);
    if (is<SharedTypedArrayObject>())
        return as<SharedTypedArrayObject>().getElement(idx);
    return getDenseElement(idx);
}

inline void
NativeObject::initDenseElementsUnbarriered(uint32_t dstStart, const Value *src, uint32_t count) {
    /*
     * For use by parallel threads, which since they cannot see nursery
     * things do not require a barrier.
     */
    MOZ_ASSERT(dstStart + count <= getDenseCapacity());
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
#if defined(DEBUG) && defined(JSGC_GENERATIONAL)
    /*
     * This asserts a global invariant: parallel code does not
     * observe objects inside the generational GC's nursery.
     */
    MOZ_ASSERT(!gc::IsInsideGGCNursery(this));
    for (uint32_t index = 0; index < count; ++index) {
        const Value& value = src[index];
        if (value.isMarkable())
            MOZ_ASSERT(!gc::IsInsideGGCNursery(static_cast<gc::Cell *>(value.toGCThing())));
    }
#endif
    memcpy(&elements[dstStart], src, count * sizeof(HeapSlot));
}

/* static */ inline NativeObject *
NativeObject::copy(ExclusiveContext *cx, gc::AllocKind kind, gc::InitialHeap heap,
                   HandleNativeObject templateObject)
{
    RootedShape shape(cx, templateObject->lastProperty());
    RootedTypeObject type(cx, templateObject->type());
    MOZ_ASSERT(!templateObject->denseElementsAreCopyOnWrite());

    JSObject *baseObj = create(cx, kind, heap, shape, type);
    if (!baseObj)
        return nullptr;
    NativeObject *obj = &baseObj->as<NativeObject>();

    size_t span = shape->slotSpan();
    if (span) {
        uint32_t numFixed = templateObject->numFixedSlots();
        const Value *fixed = &templateObject->getSlot(0);
        MOZ_ASSERT(numFixed <= span);
        obj->copySlotRange(0, fixed, numFixed);

        if (numFixed < span) {
            uint32_t numSlots = span - numFixed;
            const Value *slots = &templateObject->getSlot(numFixed);
            obj->copySlotRange(numFixed, slots, numSlots);
        }
    }

    return obj;
}

inline bool
NativeObject::setSlotIfHasType(Shape *shape, const Value &value, bool overwriting)
{
    if (!types::HasTypePropertyId(this, shape->propid(), value))
        return false;
    setSlot(shape->slot(), value);

    if (overwriting)
        shape->setOverwritten();

    return true;
}

inline void
NativeObject::setSlotWithType(ExclusiveContext *cx, Shape *shape,
                              const Value &value, bool overwriting)
{
    setSlot(shape->slot(), value);

    if (overwriting)
        shape->setOverwritten();

    types::AddTypePropertyId(cx, this, shape->propid(), value);
}

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline NativeObject *
CopyInitializerObject(JSContext *cx, HandleNativeObject baseobj, NewObjectKind newKind = GenericObject)
{
    MOZ_ASSERT(baseobj->getClass() == &JSObject::class_);
    MOZ_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind allocKind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    allocKind = gc::GetBackgroundAllocKind(allocKind);
    MOZ_ASSERT_IF(baseobj->isTenured(), allocKind == baseobj->asTenured().getAllocKind());
    JSObject *baseObj = NewBuiltinClassInstance(cx, &JSObject::class_, allocKind, newKind);
    if (!baseObj)
        return nullptr;
    RootedNativeObject obj(cx, &baseObj->as<NativeObject>());

    RootedObject metadata(cx, obj->getMetadata());
    RootedShape lastProp(cx, baseobj->lastProperty());
    if (!NativeObject::setLastProperty(cx, obj, lastProp))
        return nullptr;
    if (metadata && !JSObject::setMetadata(cx, obj, metadata))
        return nullptr;

    return obj;
}

inline NativeObject *
NewNativeObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp,
                              TaggedProto proto, JSObject *parent,
                              gc::AllocKind allocKind, NewObjectKind newKind)
{
    return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, parent, allocKind, newKind));
}

inline NativeObject *
NewNativeObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp,
                              TaggedProto proto, JSObject *parent,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, parent, newKind));
}

inline NativeObject *
NewNativeObjectWithGivenProto(ExclusiveContext *cx, const js::Class *clasp,
                              JSObject *proto, JSObject *parent,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, parent, newKind));
}

inline NativeObject *
NewNativeBuiltinClassInstance(ExclusiveContext *cx, const Class *clasp,
                              gc::AllocKind allocKind, NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewBuiltinClassInstance(cx, clasp, allocKind, newKind));
}

inline NativeObject *
NewNativeBuiltinClassInstance(ExclusiveContext *cx, const Class *clasp,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewBuiltinClassInstance(cx, clasp, newKind));
}

inline NativeObject *
NewNativeObjectWithClassProto(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                              gc::AllocKind allocKind, NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithClassProto(cx, clasp, proto, parent, allocKind, newKind));
}

inline NativeObject *
NewNativeObjectWithClassProto(ExclusiveContext *cx, const js::Class *clasp, JSObject *proto, JSObject *parent,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithClassProto(cx, clasp, proto, parent, newKind));
}

inline NativeObject *
NewNativeObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind allocKind,
                        NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithType(cx, type, parent, allocKind, newKind));
}

inline NativeObject *
NewNativeObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent,
                        NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithType(cx, type, parent, newKind));
}

} // namespace js

inline uint8_t *
JSObject::fakeNativeFixedData(size_t nslots) const
{
    return static_cast<const js::NativeObject*>(this)->fixedData(nslots);
}

#endif /* vm_ObjectImpl_inl_h */
