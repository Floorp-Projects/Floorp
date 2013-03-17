/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ObjectImpl_inl_h___
#define ObjectImpl_inl_h___

#include "mozilla/Assertions.h"

#include "jscompartment.h"
#include "jsgc.h"
#include "jsinterp.h"

#include "gc/Heap.h"
#include "gc/Marking.h"
#include "js/TemplateLib.h"

#include "ObjectImpl.h"

namespace js {

static MOZ_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *vec, uint32_t len)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) vec, len);
#endif
}

static MOZ_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *begin, HeapSlot *end)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) begin, end - begin);
#endif
}

} // namespace js

inline js::RawShape
js::ObjectImpl::nativeLookup(JSContext *cx, PropertyId pid)
{
    return nativeLookup(cx, pid.asId());
}

inline js::RawShape
js::ObjectImpl::nativeLookup(JSContext *cx, PropertyName *name)
{
    return nativeLookup(cx, NameToId(name));
}

inline bool
js::ObjectImpl::nativeContains(JSContext *cx, jsid id)
{
    return nativeLookup(cx, id) != NULL;
}

inline bool
js::ObjectImpl::nativeContains(JSContext *cx, PropertyName *name)
{
    return nativeLookup(cx, name) != NULL;
}

inline bool
js::ObjectImpl::nativeContains(JSContext *cx, Shape *shape)
{
    return nativeLookup(cx, shape->propid()) == shape;
}

inline bool
js::ObjectImpl::isExtensible() const
{
    return !lastProperty()->hasObjectFlag(BaseShape::NOT_EXTENSIBLE);
}

inline uint32_t
js::ObjectImpl::getDenseInitializedLength()
{
    MOZ_ASSERT(isNative());
    return getElementsHeader()->initializedLength;
}

inline js::HeapSlotArray
js::ObjectImpl::getDenseElements()
{
    MOZ_ASSERT(isNative());
    return HeapSlotArray(elements);
}

inline const js::Value &
js::ObjectImpl::getDenseElement(uint32_t idx)
{
    MOZ_ASSERT(isNative() && idx < getDenseInitializedLength());
    return elements[idx];
}

inline bool
js::ObjectImpl::containsDenseElement(uint32_t idx)
{
    MOZ_ASSERT(isNative());
    return idx < getDenseInitializedLength() && !elements[idx].isMagic(JS_ELEMENTS_HOLE);
}

inline void
js::ObjectImpl::getSlotRangeUnchecked(uint32_t start, uint32_t length,
                                      HeapSlot **fixedStart, HeapSlot **fixedEnd,
                                      HeapSlot **slotsStart, HeapSlot **slotsEnd)
{
    MOZ_ASSERT(start + length >= start);

    uint32_t fixed = numFixedSlots();
    if (start < fixed) {
        if (start + length < fixed) {
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + length];
            *slotsStart = *slotsEnd = NULL;
        } else {
            uint32_t localCopy = fixed - start;
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + localCopy];
            *slotsStart = &slots[0];
            *slotsEnd = &slots[length - localCopy];
        }
    } else {
        *fixedStart = *fixedEnd = NULL;
        *slotsStart = &slots[start - fixed];
        *slotsEnd = &slots[start - fixed + length];
    }
}

inline void
js::ObjectImpl::getSlotRange(uint32_t start, uint32_t length,
                             HeapSlot **fixedStart, HeapSlot **fixedEnd,
                             HeapSlot **slotsStart, HeapSlot **slotsEnd)
{
    MOZ_ASSERT(slotInRange(start + length, SENTINEL_ALLOWED));
    getSlotRangeUnchecked(start, length, fixedStart, fixedEnd, slotsStart, slotsEnd);
}

inline void
js::ObjectImpl::invalidateSlotRange(uint32_t start, uint32_t length)
{
#ifdef DEBUG
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    Debug_SetSlotRangeToCrashOnTouch(fixedStart, fixedEnd);
    Debug_SetSlotRangeToCrashOnTouch(slotsStart, slotsEnd);
#endif /* DEBUG */
}

inline void
js::ObjectImpl::initializeSlotRange(uint32_t start, uint32_t length)
{
    /*
     * No bounds check, as this is used when the object's shape does not
     * reflect its allocated slots (updateSlotsForSpan).
     */
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRangeUnchecked(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);

    JSRuntime *rt = runtime();
    uint32_t offset = start;
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
}

inline bool
js::ObjectImpl::isNative() const
{
    return lastProperty()->isNative();
}

inline js::HeapSlot &
js::ObjectImpl::nativeGetSlotRef(uint32_t slot)
{
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(slot < slotSpan());
    return getSlotRef(slot);
}

inline const js::Value &
js::ObjectImpl::nativeGetSlot(uint32_t slot) const
{
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(slot < slotSpan());
    return getSlot(slot);
}

#ifdef DEBUG
inline bool
IsObjectValueInCompartment(js::Value v, JSCompartment *comp)
{
    if (!v.isObject())
        return true;
    return v.toObject().compartment() == comp;
}
#endif

inline void
js::ObjectImpl::setSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(slotInRange(slot));
    MOZ_ASSERT(IsObjectValueInCompartment(value, asObjectPtr()->compartment()));
    getSlotRef(slot).set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
}

inline void
js::ObjectImpl::setCrossCompartmentSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(slotInRange(slot));
    getSlotRef(slot).set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
}

inline void
js::ObjectImpl::initSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(getSlot(slot).isUndefined());
    MOZ_ASSERT(slotInRange(slot));
    MOZ_ASSERT(IsObjectValueInCompartment(value, asObjectPtr()->compartment()));
    initSlotUnchecked(slot, value);
}

inline void
js::ObjectImpl::initCrossCompartmentSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(getSlot(slot).isUndefined());
    MOZ_ASSERT(slotInRange(slot));
    initSlotUnchecked(slot, value);
}

inline void
js::ObjectImpl::initSlotUnchecked(uint32_t slot, const js::Value &value)
{
    getSlotAddressUnchecked(slot)->init(this->asObjectPtr(), HeapSlot::Slot, slot, value);
}

inline void
js::ObjectImpl::setFixedSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(slot < numFixedSlots());
    fixedSlots()[slot].set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
}

inline void
js::ObjectImpl::initFixedSlot(uint32_t slot, const js::Value &value)
{
    MOZ_ASSERT(slot < numFixedSlots());
    fixedSlots()[slot].init(this->asObjectPtr(), HeapSlot::Slot, slot, value);
}

inline uint32_t
js::ObjectImpl::slotSpan() const
{
    if (inDictionaryMode())
        return lastProperty()->base()->slotSpan();
    return lastProperty()->slotSpan();
}

inline uint32_t
js::ObjectImpl::numDynamicSlots() const
{
    return dynamicSlotsCount(numFixedSlots(), slotSpan());
}

inline JSClass *
js::ObjectImpl::getJSClass() const
{
    return Jsvalify(getClass());
}

inline bool
js::ObjectImpl::hasClass(const Class *c) const
{
    return getClass() == c;
}

inline const js::ObjectOps *
js::ObjectImpl::getOps() const
{
    return &getClass()->ops;
}

inline bool
js::ObjectImpl::isDelegate() const
{
    return lastProperty()->hasObjectFlag(BaseShape::DELEGATE);
}

inline bool
js::ObjectImpl::inDictionaryMode() const
{
    return lastProperty()->inDictionary();
}

/* static */ inline uint32_t
js::ObjectImpl::dynamicSlotsCount(uint32_t nfixed, uint32_t span)
{
    if (span <= nfixed)
        return 0;
    span -= nfixed;
    if (span <= SLOT_CAPACITY_MIN)
        return SLOT_CAPACITY_MIN;

    uint32_t slots = RoundUpPow2(span);
    MOZ_ASSERT(slots >= span);
    return slots;
}

inline size_t
js::ObjectImpl::sizeOfThis() const
{
    return js::gc::Arena::thingSize(getAllocKind());
}

/* static */ inline void
js::ObjectImpl::readBarrier(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    Zone *zone = obj->zone();
    if (zone->needsBarrier()) {
        MOZ_ASSERT(!zone->rt->isHeapBusy());
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
        MOZ_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPre(void **old)
{
#ifdef JSGC_INCREMENTAL
    Zone *zone = this->zone();
    if (zone->needsBarrier()) {
        if (*old && getClass()->trace)
            getClass()->trace(zone->barrierTracer(), this->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPost(void **pprivate)
{
#ifdef JSGC_GENERATIONAL
    runtime()->gcStoreBuffer.putCell(reinterpret_cast<js::gc::Cell **>(pprivate));
#endif
}

/* static */ inline void
js::ObjectImpl::writeBarrierPre(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    /*
     * This would normally be a null test, but TypeScript::global uses 0x1 as a
     * special value.
     */
    if (IsNullTaggedPointer(obj))
        return;

    Zone *zone = obj->zone();
    if (zone->needsBarrier()) {
        MOZ_ASSERT(!zone->rt->isHeapBusy());
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        MOZ_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

/* static */ inline void
js::ObjectImpl::writeBarrierPost(ObjectImpl *obj, void *addr)
{
#ifdef JSGC_GENERATIONAL
    if (IsNullTaggedPointer(obj))
        return;
    obj->runtime()->gcStoreBuffer.putCell((Cell **)addr);
#endif
}

inline bool
js::ObjectImpl::hasPrivate() const
{
    return getClass()->hasPrivate();
}

inline void *&
js::ObjectImpl::privateRef(uint32_t nfixed) const
{
    /*
     * The private pointer of an object can hold any word sized value.
     * Private pointers are stored immediately after the last fixed slot of
     * the object.
     */
    MOZ_ASSERT(nfixed == numFixedSlots());
    MOZ_ASSERT(hasPrivate());
    HeapSlot *end = &fixedSlots()[nfixed];
    return *reinterpret_cast<void**>(end);
}

inline void *
js::ObjectImpl::getPrivate() const
{
    return privateRef(numFixedSlots());
}

inline void *
js::ObjectImpl::getPrivate(uint32_t nfixed) const
{
    return privateRef(nfixed);
}

inline void
js::ObjectImpl::setPrivate(void *data)
{
    void **pprivate = &privateRef(numFixedSlots());
    privateWriteBarrierPre(pprivate);
    *pprivate = data;
}

inline void
js::ObjectImpl::setPrivateGCThing(js::gc::Cell *cell)
{
    void **pprivate = &privateRef(numFixedSlots());
    privateWriteBarrierPre(pprivate);
    *pprivate = reinterpret_cast<void *>(cell);
    privateWriteBarrierPost(pprivate);
}

inline void
js::ObjectImpl::setPrivateUnbarriered(void *data)
{
    void **pprivate = &privateRef(numFixedSlots());
    *pprivate = data;
}

inline void
js::ObjectImpl::initPrivate(void *data)
{
    privateRef(numFixedSlots()) = data;
}

#endif /* ObjectImpl_inl_h__ */
