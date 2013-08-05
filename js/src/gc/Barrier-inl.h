/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Barrier_inl_h
#define gc_Barrier_inl_h

#include "gc/Barrier.h"

#include "gc/Marking.h"
#include "gc/StoreBuffer.h"

#include "vm/String-inl.h"

namespace js {

JS_ALWAYS_INLINE JS::Zone *
ZoneOfValue(const JS::Value &value)
{
    JS_ASSERT(value.isMarkable());
    if (value.isObject())
        return value.toObject().zone();
    return static_cast<js::gc::Cell *>(value.toGCThing())->tenuredZone();
}

template <typename T, typename Unioned>
void
EncapsulatedPtr<T, Unioned>::pre()
{
    T::writeBarrierPre(value);
}

template <typename T>
inline void
RelocatablePtr<T>::post()
{
#ifdef JSGC_GENERATIONAL
    JS_ASSERT(this->value);
    T::writeBarrierPostRelocate(this->value, &this->value);
#endif
}

template <typename T>
inline void
RelocatablePtr<T>::relocate(JSRuntime *rt)
{
#ifdef JSGC_GENERATIONAL
    T::writeBarrierPostRemove(this->value, &this->value);
#endif
}

inline
EncapsulatedValue::~EncapsulatedValue()
{
    pre();
}

inline EncapsulatedValue &
EncapsulatedValue::operator=(const Value &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    return *this;
}

inline EncapsulatedValue &
EncapsulatedValue::operator=(const EncapsulatedValue &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v.get();
    return *this;
}

inline void
EncapsulatedValue::writeBarrierPre(const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (value.isMarkable() && runtimeFromAnyThread(value)->needsBarrier())
        writeBarrierPre(ZoneOfValue(value), value);
#endif
}

inline void
EncapsulatedValue::writeBarrierPre(Zone *zone, const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (zone->needsBarrier()) {
        JS_ASSERT_IF(value.isMarkable(), runtimeFromMainThread(value)->needsBarrier());
        Value tmp(value);
        js::gc::MarkValueUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == value);
    }
#endif
}

inline void
EncapsulatedValue::pre()
{
    writeBarrierPre(value);
}

inline void
EncapsulatedValue::pre(Zone *zone)
{
    writeBarrierPre(zone, value);
}

inline
HeapValue::HeapValue()
    : EncapsulatedValue(UndefinedValue())
{
    post();
}

inline
HeapValue::HeapValue(const Value &v)
    : EncapsulatedValue(v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    post();
}

inline
HeapValue::HeapValue(const HeapValue &v)
    : EncapsulatedValue(v.value)
{
    JS_ASSERT(!IsPoisonedValue(v.value));
    post();
}

inline
HeapValue::~HeapValue()
{
    pre();
}

inline void
HeapValue::init(const Value &v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post();
}

inline void
HeapValue::init(JSRuntime *rt, const Value &v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(rt);
}

inline HeapValue &
HeapValue::operator=(const Value &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post();
    return *this;
}

inline HeapValue &
HeapValue::operator=(const HeapValue &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v.value));
    value = v.value;
    post();
    return *this;
}

inline void
HeapValue::set(Zone *zone, const Value &v)
{
#ifdef DEBUG
    if (value.isMarkable()) {
        JS_ASSERT(ZoneOfValue(value) == zone ||
                  ZoneOfValue(value) == zone->runtimeFromMainThread()->atomsCompartment->zone());
    }
#endif

    pre(zone);
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(zone->runtimeFromAnyThread());
}

inline void
HeapValue::writeBarrierPost(const Value &value, Value *addr)
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable())
        runtimeFromMainThread(value)->gcStoreBuffer.putValue(addr);
#endif
}

inline void
HeapValue::writeBarrierPost(JSRuntime *rt, const Value &value, Value *addr)
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable())
        rt->gcStoreBuffer.putValue(addr);
#endif
}

inline void
HeapValue::post()
{
    writeBarrierPost(value, &value);
}

inline void
HeapValue::post(JSRuntime *rt)
{
    writeBarrierPost(rt, value, &value);
}

inline
RelocatableValue::RelocatableValue()
    : EncapsulatedValue(UndefinedValue())
{
}

inline
RelocatableValue::RelocatableValue(const Value &v)
    : EncapsulatedValue(v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    if (v.isMarkable())
        post();
}

inline
RelocatableValue::RelocatableValue(const RelocatableValue &v)
    : EncapsulatedValue(v.value)
{
    JS_ASSERT(!IsPoisonedValue(v.value));
    if (v.value.isMarkable())
        post();
}

inline
RelocatableValue::~RelocatableValue()
{
    if (value.isMarkable())
        relocate(runtimeFromMainThread(value));
}

inline RelocatableValue &
RelocatableValue::operator=(const Value &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    if (v.isMarkable()) {
        value = v;
        post();
    } else if (value.isMarkable()) {
        JSRuntime *rt = runtimeFromMainThread(value);
        value = v;
        relocate(rt);
    } else {
        value = v;
    }
    return *this;
}

inline RelocatableValue &
RelocatableValue::operator=(const RelocatableValue &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v.value));
    if (v.value.isMarkable()) {
        value = v.value;
        post();
    } else if (value.isMarkable()) {
        JSRuntime *rt = runtimeFromMainThread(value);
        value = v.value;
        relocate(rt);
    } else {
        value = v.value;
    }
    return *this;
}

inline void
RelocatableValue::post()
{
#ifdef JSGC_GENERATIONAL
    JS_ASSERT(value.isMarkable());
    runtimeFromMainThread(value)->gcStoreBuffer.putRelocatableValue(&value);
#endif
}

inline void
RelocatableValue::relocate(JSRuntime *rt)
{
#ifdef JSGC_GENERATIONAL
    rt->gcStoreBuffer.removeRelocatableValue(&value);
#endif
}

inline
HeapSlot::HeapSlot(JSObject *obj, Kind kind, uint32_t slot, const Value &v)
    : EncapsulatedValue(v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    post(obj, kind, slot, v);
}

inline
HeapSlot::HeapSlot(JSObject *obj, Kind kind, uint32_t slot, const HeapSlot &s)
    : EncapsulatedValue(s.value)
{
    JS_ASSERT(!IsPoisonedValue(s.value));
    post(obj, kind, slot, s);
}

inline
HeapSlot::~HeapSlot()
{
    pre();
}

inline void
HeapSlot::init(JSObject *obj, Kind kind, uint32_t slot, const Value &v)
{
    value = v;
    post(obj, kind, slot, v);
}

inline void
HeapSlot::init(JSRuntime *rt, JSObject *obj, Kind kind, uint32_t slot, const Value &v)
{
    value = v;
    post(rt, obj, kind, slot, v);
}

inline void
HeapSlot::set(JSObject *obj, Kind kind, uint32_t slot, const Value &v)
{
    JS_ASSERT_IF(kind == Slot, &obj->getSlotRef(slot) == this);
    JS_ASSERT_IF(kind == Element, &obj->getDenseElement(slot) == (const Value *)this);

    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(obj, kind, slot, v);
}

inline void
HeapSlot::set(Zone *zone, JSObject *obj, Kind kind, uint32_t slot, const Value &v)
{
    JS_ASSERT_IF(kind == Slot, &obj->getSlotRef(slot) == this);
    JS_ASSERT_IF(kind == Element, &obj->getDenseElement(slot) == (const Value *)this);
    JS_ASSERT(obj->zone() == zone);

    pre(zone);
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(zone->runtimeFromAnyThread(), obj, kind, slot, v);
}

inline void
HeapSlot::writeBarrierPost(JSObject *obj, Kind kind, uint32_t slot, Value target)
{
#ifdef JSGC_GENERATIONAL
    writeBarrierPost(obj->runtimeFromAnyThread(), obj, kind, slot, target);
#endif
}

inline void
HeapSlot::writeBarrierPost(JSRuntime *rt, JSObject *obj, Kind kind, uint32_t slot, Value target)
{
#ifdef JSGC_GENERATIONAL
    JS_ASSERT_IF(kind == Slot, obj->getSlotAddressUnchecked(slot)->get() == target);
    JS_ASSERT_IF(kind == Element,
                 static_cast<HeapSlot *>(obj->getDenseElements() + slot)->get() == target);

    if (target.isObject())
        rt->gcStoreBuffer.putSlot(obj, kind, slot, &target.toObject());
#endif
}

inline void
HeapSlot::post(JSObject *owner, Kind kind, uint32_t slot, Value target)
{
    HeapSlot::writeBarrierPost(owner, kind, slot, target);
}

inline void
HeapSlot::post(JSRuntime *rt, JSObject *owner, Kind kind, uint32_t slot, Value target)
{
    HeapSlot::writeBarrierPost(rt, owner, kind, slot, target);
}

#ifdef JSGC_GENERATIONAL
class DenseRangeRef : public gc::BufferableRef
{
    JSObject *owner;
    uint32_t start;
    uint32_t end;

  public:
    DenseRangeRef(JSObject *obj, uint32_t start, uint32_t end)
      : owner(obj), start(start), end(end)
    {
        JS_ASSERT(start < end);
    }

    void mark(JSTracer *trc) {
        /* Apply forwarding, if we have already visited owner. */
        IsObjectMarked(&owner);
        uint32_t initLen = owner->getDenseInitializedLength();
        uint32_t clampedStart = Min(start, initLen);
        gc::MarkArraySlots(trc, Min(end, initLen) - clampedStart,
                           owner->getDenseElements() + clampedStart, "element");
    }
};
#endif

inline void
DenseRangeWriteBarrierPost(JSRuntime *rt, JSObject *obj, uint32_t start, uint32_t count)
{
#ifdef JSGC_GENERATIONAL
    if (count > 0)
        rt->gcStoreBuffer.putGeneric(DenseRangeRef(obj, start, start + count));
#endif
}

/*
 * This is a post barrier for HashTables whose key is a GC pointer. Any
 * insertion into a HashTable not marked as part of the runtime, with a GC
 * pointer as a key, must call this immediately after each insertion.
 */
template <class Map, class Key>
inline void
HashTableWriteBarrierPost(JSRuntime *rt, Map *map, const Key &key)
{
#ifdef JSGC_GENERATIONAL
    if (key && IsInsideNursery(rt, key))
        rt->gcStoreBuffer.putGeneric(gc::HashKeyRef<Map, Key>(map, key));
#endif
}


inline
EncapsulatedId::~EncapsulatedId()
{
    pre();
}

inline EncapsulatedId &
EncapsulatedId::operator=(const EncapsulatedId &v)
{
    if (v.value != value)
        pre();
    JS_ASSERT(!IsPoisonedId(v.value));
    value = v.value;
    return *this;
}

inline void
EncapsulatedId::pre()
{
#ifdef JSGC_INCREMENTAL
    if (JSID_IS_OBJECT(value)) {
        JSObject *obj = JSID_TO_OBJECT(value);
        Zone *zone = obj->zone();
        if (zone->needsBarrier()) {
            js::gc::MarkObjectUnbarriered(zone->barrierTracer(), &obj, "write barrier");
            JS_ASSERT(obj == JSID_TO_OBJECT(value));
        }
    } else if (JSID_IS_STRING(value)) {
        JSString *str = JSID_TO_STRING(value);
        Zone *zone = str->zone();
        if (zone->needsBarrier()) {
            js::gc::MarkStringUnbarriered(zone->barrierTracer(), &str, "write barrier");
            JS_ASSERT(str == JSID_TO_STRING(value));
        }
    }
#endif
}

inline
RelocatableId::~RelocatableId()
{
    pre();
}

inline RelocatableId &
RelocatableId::operator=(jsid id)
{
    if (id != value)
        pre();
    JS_ASSERT(!IsPoisonedId(id));
    value = id;
    return *this;
}

inline RelocatableId &
RelocatableId::operator=(const RelocatableId &v)
{
    if (v.value != value)
        pre();
    JS_ASSERT(!IsPoisonedId(v.value));
    value = v.value;
    return *this;
}

inline
HeapId::HeapId(jsid id)
    : EncapsulatedId(id)
{
    JS_ASSERT(!IsPoisonedId(id));
    post();
}

inline
HeapId::~HeapId()
{
    pre();
}

inline void
HeapId::init(jsid id)
{
    JS_ASSERT(!IsPoisonedId(id));
    value = id;
    post();
}

inline void
HeapId::post()
{
}

inline HeapId &
HeapId::operator=(jsid id)
{
    if (id != value)
        pre();
    JS_ASSERT(!IsPoisonedId(id));
    value = id;
    post();
    return *this;
}

inline HeapId &
HeapId::operator=(const HeapId &v)
{
    if (v.value != value)
        pre();
    JS_ASSERT(!IsPoisonedId(v.value));
    value = v.value;
    post();
    return *this;
}

inline const Value &
ReadBarrieredValue::get() const
{
    if (value.isObject())
        JSObject::readBarrier(&value.toObject());
    else if (value.isString())
        JSString::readBarrier(value.toString());
    else
        JS_ASSERT(!value.isMarkable());

    return value;
}

inline
ReadBarrieredValue::operator const Value &() const
{
    return get();
}

inline JSObject &
ReadBarrieredValue::toObject() const
{
    return get().toObject();
}

} /* namespace js */

#endif /* gc_Barrier_inl_h */
