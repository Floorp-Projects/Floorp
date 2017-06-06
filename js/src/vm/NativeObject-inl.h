/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_NativeObject_inl_h
#define vm_NativeObject_inl_h

#include "vm/NativeObject.h"

#include "jscntxt.h"

#include "builtin/TypedObject.h"
#include "proxy/Proxy.h"
#include "vm/ProxyObject.h"
#include "vm/TypedArrayObject.h"

#include "jsobjinlines.h"

#include "gc/Heap-inl.h"

namespace js {

inline uint8_t*
NativeObject::fixedData(size_t nslots) const
{
    MOZ_ASSERT(ClassCanHaveFixedData(getClass()));
    MOZ_ASSERT(nslots == numFixedSlots() + (hasPrivate() ? 1 : 0));
    return reinterpret_cast<uint8_t*>(&fixedSlots()[nslots]);
}

inline void
NativeObject::removeLastProperty(JSContext* cx)
{
    MOZ_ASSERT(canRemoveLastProperty());
    JS_ALWAYS_TRUE(setLastProperty(cx, lastProperty()->previous()));
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
    Shape* previous = lastProperty()->previous().get();
    return previous->getObjectFlags() == lastProperty()->getObjectFlags();
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

inline void
NativeObject::setDenseElementWithType(JSContext* cx, uint32_t index, const Value& val)
{
    // Avoid a slow AddTypePropertyId call if the type is the same as the type
    // of the previous element.
    TypeSet::Type thisType = TypeSet::GetValueType(val);
    if (index == 0 || TypeSet::GetValueType(elements_[index - 1]) != thisType)
        AddTypePropertyId(cx, this, JSID_VOID, thisType);
    setDenseElementMaybeConvertDouble(index, val);
}

inline void
NativeObject::initDenseElementWithType(JSContext* cx, uint32_t index, const Value& val)
{
    MOZ_ASSERT(!shouldConvertDoubleElements());
    if (val.isMagic(JS_ELEMENTS_HOLE))
        markDenseElementsNotPacked(cx);
    else
        AddTypePropertyId(cx, this, JSID_VOID, val);
    initDenseElement(index, val);
}

inline void
NativeObject::setDenseElementHole(JSContext* cx, uint32_t index)
{
    MarkObjectGroupFlags(cx, this, OBJECT_FLAG_NON_PACKED);
    setDenseElement(index, MagicValue(JS_ELEMENTS_HOLE));
}

/* static */ inline void
NativeObject::removeDenseElementForSparseIndex(JSContext* cx,
                                               HandleNativeObject obj, uint32_t index)
{
    MarkObjectGroupFlags(cx, obj, OBJECT_FLAG_NON_PACKED | OBJECT_FLAG_SPARSE_INDEXES);
    if (obj->containsDenseElement(index))
        obj->setDenseElementUnchecked(index, MagicValue(JS_ELEMENTS_HOLE));
}

inline bool
NativeObject::writeToIndexWouldMarkNotPacked(uint32_t index)
{
    return getElementsHeader()->initializedLength < index;
}

inline void
NativeObject::markDenseElementsNotPacked(JSContext* cx)
{
    MOZ_ASSERT(isNative());
    MarkObjectGroupFlags(cx, this, OBJECT_FLAG_NON_PACKED);
}

inline void
NativeObject::elementsRangeWriteBarrierPost(uint32_t start, uint32_t count)
{
    for (size_t i = 0; i < count; i++) {
        const Value& v = elements_[start + i];
        if (v.isObject() && IsInsideNursery(&v.toObject())) {
            zone()->group()->storeBuffer().putSlot(this, HeapSlot::Element,
                                                   unshiftedIndex(start + i),
                                                   count - i);
            return;
        }
    }
}

inline void
NativeObject::copyDenseElements(uint32_t dstStart, const Value* src, uint32_t count)
{
    MOZ_ASSERT(dstStart + count <= getDenseCapacity());
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());
#ifdef DEBUG
    for (uint32_t i = 0; i < count; ++i)
        checkStoredValue(src[i]);
#endif
    if (JS::shadow::Zone::asShadowZone(zone())->needsIncrementalBarrier()) {
        uint32_t numShifted = getElementsHeader()->numShiftedElements();
        for (uint32_t i = 0; i < count; ++i) {
            elements_[dstStart + i].set(this, HeapSlot::Element,
                                        dstStart + i + numShifted,
                                        src[i]);
        }
    } else {
        memcpy(&elements_[dstStart], src, count * sizeof(HeapSlot));
        elementsRangeWriteBarrierPost(dstStart, count);
    }
}

inline void
NativeObject::initDenseElements(uint32_t dstStart, const Value* src, uint32_t count)
{
    MOZ_ASSERT(dstStart + count <= getDenseCapacity());
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());
#ifdef DEBUG
    for (uint32_t i = 0; i < count; ++i)
        checkStoredValue(src[i]);
#endif
    memcpy(&elements_[dstStart], src, count * sizeof(HeapSlot));
    elementsRangeWriteBarrierPost(dstStart, count);
}

inline bool
NativeObject::tryShiftDenseElements(uint32_t count)
{
    ObjectElements* header = getElementsHeader();
    if (header->initializedLength == count ||
        count > ObjectElements::MaxShiftedElements ||
        header->isCopyOnWrite() ||
        header->isFrozen() ||
        header->hasNonwritableArrayLength())
    {
        return false;
    }

    shiftDenseElementsUnchecked(count);
    return true;
}

inline void
NativeObject::shiftDenseElementsUnchecked(uint32_t count)
{
    ObjectElements* header = getElementsHeader();
    MOZ_ASSERT(count > 0);
    MOZ_ASSERT(count < header->initializedLength);

    if (MOZ_UNLIKELY(header->numShiftedElements() + count > ObjectElements::MaxShiftedElements)) {
        moveShiftedElements();
        header = getElementsHeader();
    }

    prepareElementRangeForOverwrite(0, count);
    header->addShiftedElements(count);

    elements_ += count;
    ObjectElements* newHeader = getElementsHeader();
    memmove(newHeader, header, sizeof(ObjectElements));
}

inline void
NativeObject::moveDenseElements(uint32_t dstStart, uint32_t srcStart, uint32_t count)
{
    MOZ_ASSERT(dstStart + count <= getDenseCapacity());
    MOZ_ASSERT(srcStart + count <= getDenseInitializedLength());
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());

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
    if (JS::shadow::Zone::asShadowZone(zone())->needsIncrementalBarrier()) {
        uint32_t numShifted = getElementsHeader()->numShiftedElements();
        if (dstStart < srcStart) {
            HeapSlot* dst = elements_ + dstStart;
            HeapSlot* src = elements_ + srcStart;
            for (uint32_t i = 0; i < count; i++, dst++, src++)
                dst->set(this, HeapSlot::Element, dst - elements_ + numShifted, *src);
        } else {
            HeapSlot* dst = elements_ + dstStart + count - 1;
            HeapSlot* src = elements_ + srcStart + count - 1;
            for (uint32_t i = 0; i < count; i++, dst--, src--)
                dst->set(this, HeapSlot::Element, dst - elements_ + numShifted, *src);
        }
    } else {
        memmove(elements_ + dstStart, elements_ + srcStart, count * sizeof(HeapSlot));
        elementsRangeWriteBarrierPost(dstStart, count);
    }
}

inline void
NativeObject::moveDenseElementsNoPreBarrier(uint32_t dstStart, uint32_t srcStart, uint32_t count)
{
    MOZ_ASSERT(!shadowZone()->needsIncrementalBarrier());

    MOZ_ASSERT(dstStart + count <= getDenseCapacity());
    MOZ_ASSERT(srcStart + count <= getDenseCapacity());
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());

    memmove(elements_ + dstStart, elements_ + srcStart, count * sizeof(Value));
    elementsRangeWriteBarrierPost(dstStart, count);
}

inline void
NativeObject::ensureDenseInitializedLengthNoPackedCheck(JSContext* cx, uint32_t index,
                                                        uint32_t extra)
{
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());

    /*
     * Ensure that the array's contents have been initialized up to index, and
     * mark the elements through 'index + extra' as initialized in preparation
     * for a write.
     */
    MOZ_ASSERT(index + extra <= getDenseCapacity());
    uint32_t& initlen = getElementsHeader()->initializedLength;

    if (initlen < index + extra) {
        uint32_t numShifted = getElementsHeader()->numShiftedElements();
        size_t offset = initlen;
        for (HeapSlot* sp = elements_ + initlen;
             sp != elements_ + (index + extra);
             sp++, offset++)
        {
            sp->init(this, HeapSlot::Element, offset + numShifted, MagicValue(JS_ELEMENTS_HOLE));
        }
        initlen = index + extra;
    }
}

inline void
NativeObject::ensureDenseInitializedLength(JSContext* cx, uint32_t index, uint32_t extra)
{
    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);
    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
}

DenseElementResult
NativeObject::extendDenseElements(JSContext* cx,
                                  uint32_t requiredCapacity, uint32_t extra)
{
    MOZ_ASSERT(!denseElementsAreCopyOnWrite());
    MOZ_ASSERT(!denseElementsAreFrozen());

    /*
     * Don't grow elements for non-extensible objects or watched objects. Dense
     * elements can be added/written with no extensible or watchpoint checks as
     * long as there is capacity for them.
     */
    if (!nonProxyIsExtensible() || watched()) {
        MOZ_ASSERT(getDenseCapacity() == 0);
        return DenseElementResult::Incomplete;
    }

    /*
     * Don't grow elements for objects which already have sparse indexes.
     * This avoids needing to count non-hole elements in willBeSparseElements
     * every time a new index is added.
     */
    if (isIndexed())
        return DenseElementResult::Incomplete;

    /*
     * We use the extra argument also as a hint about number of non-hole
     * elements to be inserted.
     */
    if (requiredCapacity > MIN_SPARSE_INDEX &&
        willBeSparseElements(requiredCapacity, extra)) {
        return DenseElementResult::Incomplete;
    }

    if (!growElements(cx, requiredCapacity))
        return DenseElementResult::Failure;

    return DenseElementResult::Success;
}

inline DenseElementResult
NativeObject::ensureDenseElements(JSContext* cx, uint32_t index, uint32_t extra)
{
    MOZ_ASSERT(isNative());

    if (writeToIndexWouldMarkNotPacked(index))
        markDenseElementsNotPacked(cx);

    if (!maybeCopyElementsForWrite(cx))
        return DenseElementResult::Failure;

    uint32_t currentCapacity = getDenseCapacity();

    uint32_t requiredCapacity;
    if (extra == 1) {
        /* Optimize for the common case. */
        if (index < currentCapacity) {
            ensureDenseInitializedLengthNoPackedCheck(cx, index, 1);
            return DenseElementResult::Success;
        }
        requiredCapacity = index + 1;
        if (requiredCapacity == 0) {
            /* Overflow. */
            return DenseElementResult::Incomplete;
        }
    } else {
        requiredCapacity = index + extra;
        if (requiredCapacity < index) {
            /* Overflow. */
            return DenseElementResult::Incomplete;
        }
        if (requiredCapacity <= currentCapacity) {
            ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
            return DenseElementResult::Success;
        }
    }

    DenseElementResult result = extendDenseElements(cx, requiredCapacity, extra);
    if (result != DenseElementResult::Success)
        return result;

    ensureDenseInitializedLengthNoPackedCheck(cx, index, extra);
    return DenseElementResult::Success;
}

inline Value
NativeObject::getDenseOrTypedArrayElement(uint32_t idx)
{
    if (is<TypedArrayObject>())
        return as<TypedArrayObject>().getElement(idx);
    return getDenseElement(idx);
}

/* static */ inline NativeObject*
NativeObject::copy(JSContext* cx, gc::AllocKind kind, gc::InitialHeap heap,
                   HandleNativeObject templateObject)
{
    RootedShape shape(cx, templateObject->lastProperty());
    RootedObjectGroup group(cx, templateObject->group());
    MOZ_ASSERT(!templateObject->denseElementsAreCopyOnWrite());

    JSObject* baseObj;
    JS_TRY_VAR_OR_RETURN_NULL(cx, baseObj, create(cx, kind, heap, shape, group));

    NativeObject* obj = &baseObj->as<NativeObject>();

    size_t span = shape->slotSpan();
    if (span) {
        uint32_t numFixed = templateObject->numFixedSlots();
        const Value* fixed = &templateObject->getSlot(0);
        // Only copy elements which are registered in the shape, even if the
        // number of fixed slots is larger.
        if (span < numFixed)
            numFixed = span;
        obj->copySlotRange(0, fixed, numFixed);

        if (numFixed < span) {
            uint32_t numSlots = span - numFixed;
            const Value* slots = &templateObject->getSlot(numFixed);
            obj->copySlotRange(numFixed, slots, numSlots);
        }
    }

    return obj;
}

inline void
NativeObject::setSlotWithType(JSContext* cx, Shape* shape,
                              const Value& value, bool overwriting)
{
    setSlot(shape->slot(), value);

    if (overwriting)
        shape->setOverwritten();

    AddTypePropertyId(cx, this, shape->propid(), value);
}

inline void
NativeObject::updateShapeAfterMovingGC()
{
    Shape* shape = shape_;
    if (IsForwarded(shape))
        shape_.unsafeSet(Forwarded(shape));
}

inline bool
NativeObject::isInWholeCellBuffer() const
{
    const gc::TenuredCell* cell = &asTenured();
    gc::ArenaCellSet* cells = cell->arena()->bufferedCells();
    return cells && cells->hasCell(cell);
}

/* static */ inline JS::Result<NativeObject*, JS::OOM&>
NativeObject::create(JSContext* cx, js::gc::AllocKind kind, js::gc::InitialHeap heap,
                     js::HandleShape shape, js::HandleObjectGroup group)
{
    debugCheckNewObject(group, shape, kind, heap);

    const js::Class* clasp = group->clasp();
    MOZ_ASSERT(clasp->isNative());

    size_t nDynamicSlots = dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(), clasp);

    JSObject* obj = js::Allocate<JSObject>(cx, kind, nDynamicSlots, heap, clasp);
    if (!obj)
        return cx->alreadyReportedOOM();

    NativeObject* nobj = static_cast<NativeObject*>(obj);
    nobj->group_.init(group);
    nobj->initShape(shape);

    // Note: slots are created and assigned internally by Allocate<JSObject>.
    nobj->setInitialElementsMaybeNonNative(js::emptyObjectElements);

    if (clasp->hasPrivate())
        nobj->privateRef(shape->numFixedSlots()) = nullptr;

    if (size_t span = shape->slotSpan())
        nobj->initializeSlotRange(0, span);

    // JSFunction's fixed slots expect POD-style initialization.
    if (clasp->isJSFunction()) {
        MOZ_ASSERT(kind == js::gc::AllocKind::FUNCTION ||
                   kind == js::gc::AllocKind::FUNCTION_EXTENDED);
        size_t size =
            kind == js::gc::AllocKind::FUNCTION ? sizeof(JSFunction) : sizeof(js::FunctionExtended);
        memset(nobj->as<JSFunction>().fixedSlots(), 0, size - sizeof(js::NativeObject));
        if (kind == js::gc::AllocKind::FUNCTION_EXTENDED) {
            // SetNewObjectMetadata may gc, which will be unhappy if flags &
            // EXTENDED doesn't match the arena's AllocKind.
            nobj->as<JSFunction>().setFlags(JSFunction::EXTENDED);
        }
    }

    if (clasp->shouldDelayMetadataBuilder())
        cx->compartment()->setObjectPendingMetadata(cx, nobj);
    else
        nobj = SetNewObjectMetadata(cx, nobj);

    js::gc::TraceCreateObject(nobj);

    return nobj;
}

MOZ_ALWAYS_INLINE bool
NativeObject::updateSlotsForSpan(JSContext* cx, size_t oldSpan, size_t newSpan)
{
    MOZ_ASSERT(oldSpan != newSpan);

    size_t oldCount = dynamicSlotsCount(numFixedSlots(), oldSpan, getClass());
    size_t newCount = dynamicSlotsCount(numFixedSlots(), newSpan, getClass());

    if (oldSpan < newSpan) {
        if (oldCount < newCount && !growSlots(cx, oldCount, newCount))
            return false;

        if (newSpan == oldSpan + 1)
            initSlotUnchecked(oldSpan, UndefinedValue());
        else
            initializeSlotRange(oldSpan, newSpan - oldSpan);
    } else {
        /* Trigger write barriers on the old slots before reallocating. */
        prepareSlotRangeForOverwrite(newSpan, oldSpan);
        invalidateSlotRange(newSpan, oldSpan - newSpan);

        if (oldCount > newCount)
            shrinkSlots(cx, oldCount, newCount);
    }

    return true;
}

MOZ_ALWAYS_INLINE bool
NativeObject::setLastProperty(JSContext* cx, Shape* shape)
{
    MOZ_ASSERT(!inDictionaryMode());
    MOZ_ASSERT(!shape->inDictionary());
    MOZ_ASSERT(shape->zone() == zone());
    MOZ_ASSERT(shape->numFixedSlots() == numFixedSlots());
    MOZ_ASSERT(shape->getObjectClass() == getClass());

    size_t oldSpan = lastProperty()->slotSpan();
    size_t newSpan = shape->slotSpan();

    if (oldSpan == newSpan) {
        shape_ = shape;
        return true;
    }

    if (MOZ_UNLIKELY(!updateSlotsForSpan(cx, oldSpan, newSpan)))
        return false;

    shape_ = shape;
    return true;
}

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline PlainObject*
CopyInitializerObject(JSContext* cx, HandlePlainObject baseobj, NewObjectKind newKind = GenericObject)
{
    MOZ_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind allocKind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    allocKind = gc::GetBackgroundAllocKind(allocKind);
    MOZ_ASSERT_IF(baseobj->isTenured(), allocKind == baseobj->asTenured().getAllocKind());
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, newKind));
    if (!obj)
        return nullptr;

    if (!obj->setLastProperty(cx, baseobj->lastProperty()))
        return nullptr;

    return obj;
}

inline NativeObject*
NewNativeObjectWithGivenTaggedProto(JSContext* cx, const Class* clasp,
                                    Handle<TaggedProto> proto,
                                    gc::AllocKind allocKind, NewObjectKind newKind)
{
    return MaybeNativeObject(NewObjectWithGivenTaggedProto(cx, clasp, proto, allocKind,
                                                           newKind));
}

inline NativeObject*
NewNativeObjectWithGivenTaggedProto(JSContext* cx, const Class* clasp,
                                    Handle<TaggedProto> proto,
                                    NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithGivenTaggedProto(cx, clasp, proto, newKind));
}

inline NativeObject*
NewNativeObjectWithGivenProto(JSContext* cx, const Class* clasp,
                              HandleObject proto,
                              gc::AllocKind allocKind, NewObjectKind newKind)
{
    return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, allocKind, newKind));
}

inline NativeObject*
NewNativeObjectWithGivenProto(JSContext* cx, const Class* clasp,
                              HandleObject proto,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithGivenProto(cx, clasp, proto, newKind));
}

inline NativeObject*
NewNativeObjectWithClassProto(JSContext* cx, const Class* clasp, HandleObject proto,
                              gc::AllocKind allocKind,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithClassProto(cx, clasp, proto, allocKind, newKind));
}

inline NativeObject*
NewNativeObjectWithClassProto(JSContext* cx, const Class* clasp, HandleObject proto,
                              NewObjectKind newKind = GenericObject)
{
    return MaybeNativeObject(NewObjectWithClassProto(cx, clasp, proto, newKind));
}

/*
 * Call obj's resolve hook.
 *
 * cx and id are the parameters initially passed to the ongoing lookup;
 * propp and recursedp are its out parameters.
 *
 * There are four possible outcomes:
 *
 *   - On failure, report an error or exception and return false.
 *
 *   - If we are already resolving a property of obj, set *recursedp = true,
 *     and return true.
 *
 *   - If the resolve hook finds or defines the sought property, set propp
 *      appropriately, set *recursedp = false, and return true.
 *
 *   - Otherwise no property was resolved. Set propp to nullptr and
 *     *recursedp = false and return true.
 */
static MOZ_ALWAYS_INLINE bool
CallResolveOp(JSContext* cx, HandleNativeObject obj, HandleId id,
              MutableHandle<PropertyResult> propp, bool* recursedp)
{
    // Avoid recursion on (obj, id) already being resolved on cx.
    AutoResolving resolving(cx, obj, id);
    if (resolving.alreadyStarted()) {
        // Already resolving id in obj, suppress recursion.
        *recursedp = true;
        return true;
    }
    *recursedp = false;

    bool resolved = false;
    if (!obj->getClass()->getResolve()(cx, obj, id, &resolved))
        return false;

    if (!resolved)
        return true;

    // Assert the mayResolve hook, if there is one, returns true for this
    // property.
    MOZ_ASSERT_IF(obj->getClass()->getMayResolve(),
                  obj->getClass()->getMayResolve()(cx->names(), id, obj));

    if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
        propp.setDenseOrTypedArrayElement();
        return true;
    }

    MOZ_ASSERT(!obj->is<TypedArrayObject>());

    RootedShape shape(cx, obj->lookup(cx, id));
    if (shape)
        propp.setNativeProperty(shape);
    else
        propp.setNotFound();

    return true;
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE bool
LookupOwnPropertyInline(JSContext* cx,
                        typename MaybeRooted<NativeObject*, allowGC>::HandleType obj,
                        typename MaybeRooted<jsid, allowGC>::HandleType id,
                        typename MaybeRooted<PropertyResult, allowGC>::MutableHandleType propp,
                        bool* donep)
{
    // Check for a native dense element.
    if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
        propp.setDenseOrTypedArrayElement();
        *donep = true;
        return true;
    }

    // Check for a typed array element. Integer lookups always finish here
    // so that integer properties on the prototype are ignored even for out
    // of bounds accesses.
    if (obj->template is<TypedArrayObject>()) {
        uint64_t index;
        if (IsTypedArrayIndex(id, &index)) {
            if (index < obj->template as<TypedArrayObject>().length())
                propp.setDenseOrTypedArrayElement();
            else
                propp.setNotFound();
            *donep = true;
            return true;
        }
    }

    // Check for a native property.
    if (Shape* shape = obj->lookup(cx, id)) {
        propp.setNativeProperty(shape);
        *donep = true;
        return true;
    }

    // id was not found in obj. Try obj's resolve hook, if any.
    if (obj->getClass()->getResolve()) {
        MOZ_ASSERT(!cx->helperThread());
        if (!allowGC)
            return false;

        bool recursed;
        if (!CallResolveOp(cx,
                           MaybeRooted<NativeObject*, allowGC>::toHandle(obj),
                           MaybeRooted<jsid, allowGC>::toHandle(id),
                           MaybeRooted<PropertyResult, allowGC>::toMutableHandle(propp),
                           &recursed))
        {
            return false;
        }

        if (recursed) {
            propp.setNotFound();
            *donep = true;
            return true;
        }

        if (propp) {
            *donep = true;
            return true;
        }
    }

    propp.setNotFound();
    *donep = false;
    return true;
}

/*
 * Simplified version of LookupOwnPropertyInline that doesn't call resolve
 * hooks.
 */
static inline void
NativeLookupOwnPropertyNoResolve(JSContext* cx, HandleNativeObject obj, HandleId id,
                                 MutableHandle<PropertyResult> result)
{
    // Check for a native dense element.
    if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
        result.setDenseOrTypedArrayElement();
        return;
    }

    // Check for a typed array element.
    if (obj->is<TypedArrayObject>()) {
        uint64_t index;
        if (IsTypedArrayIndex(id, &index)) {
            if (index < obj->as<TypedArrayObject>().length())
                result.setDenseOrTypedArrayElement();
            else
                result.setNotFound();
            return;
        }
    }

    // Check for a native property.
    if (Shape* shape = obj->lookup(cx, id))
        result.setNativeProperty(shape);
    else
        result.setNotFound();
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE bool
LookupPropertyInline(JSContext* cx,
                     typename MaybeRooted<NativeObject*, allowGC>::HandleType obj,
                     typename MaybeRooted<jsid, allowGC>::HandleType id,
                     typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
                     typename MaybeRooted<PropertyResult, allowGC>::MutableHandleType propp)
{
    /* Search scopes starting with obj and following the prototype link. */
    typename MaybeRooted<NativeObject*, allowGC>::RootType current(cx, obj);

    while (true) {
        bool done;
        if (!LookupOwnPropertyInline<allowGC>(cx, current, id, propp, &done))
            return false;
        if (done) {
            if (propp)
                objp.set(current);
            else
                objp.set(nullptr);
            return true;
        }

        typename MaybeRooted<JSObject*, allowGC>::RootType proto(cx, current->staticPrototype());

        if (!proto)
            break;
        if (!proto->isNative()) {
            MOZ_ASSERT(!cx->helperThread());
            if (!allowGC)
                return false;
            return LookupProperty(cx,
                                  MaybeRooted<JSObject*, allowGC>::toHandle(proto),
                                  MaybeRooted<jsid, allowGC>::toHandle(id),
                                  MaybeRooted<JSObject*, allowGC>::toMutableHandle(objp),
                                  MaybeRooted<PropertyResult, allowGC>::toMutableHandle(propp));
        }

        current = &proto->template as<NativeObject>();
    }

    objp.set(nullptr);
    propp.setNotFound();
    return true;
}

inline bool
ThrowIfNotConstructing(JSContext *cx, const CallArgs &args, const char *builtinName)
{
    if (args.isConstructing())
        return true;
    return JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_ERROR, GetErrorMessage, nullptr,
                                             JSMSG_BUILTIN_CTOR_NO_NEW, builtinName);
}

inline bool
IsPackedArray(JSObject* obj)
{
    return obj->is<ArrayObject>() && !obj->hasLazyGroup() &&
           !obj->group()->hasAllFlags(OBJECT_FLAG_NON_PACKED) &&
           obj->as<ArrayObject>().getDenseInitializedLength() == obj->as<ArrayObject>().length();
}

} // namespace js

#endif /* vm_NativeObject_inl_h */
