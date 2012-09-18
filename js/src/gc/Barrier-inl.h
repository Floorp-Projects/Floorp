/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgc_barrier_inl_h___
#define jsgc_barrier_inl_h___

#include "gc/Barrier.h"
#include "gc/Marking.h"

#include "vm/ObjectImpl-inl.h"
#include "vm/String-inl.h"

namespace js {

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
    this->value->compartment()->gcStoreBuffer.putRelocatableCell((gc::Cell **)&this->value);
#endif
}

template <typename T>
inline void
RelocatablePtr<T>::relocate(JSCompartment *comp)
{
#ifdef JSGC_GENERATIONAL
    comp->gcStoreBuffer.removeRelocatableCell((gc::Cell **)&this->value);
#endif
}

inline void
EncapsulatedValue::writeBarrierPre(const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        writeBarrierPre(cell->compartment(), value);
    }
#endif
}

inline void
EncapsulatedValue::writeBarrierPre(JSCompartment *comp, const Value &value)
{
#ifdef JSGC_INCREMENTAL
    if (comp->needsBarrier()) {
        Value tmp(value);
        js::gc::MarkValueUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
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
EncapsulatedValue::pre(JSCompartment *comp)
{
    writeBarrierPre(comp, value);
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
HeapValue::init(JSCompartment *comp, const Value &v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(comp);
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
HeapValue::set(JSCompartment *comp, const Value &v)
{
#ifdef DEBUG
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        JS_ASSERT(cell->compartment() == comp ||
                  cell->compartment() == comp->rt->atomsCompartment);
    }
#endif

    pre(comp);
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(comp);
}

inline void
HeapValue::writeBarrierPost(const Value &value, Value *addr)
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        cell->compartment()->gcStoreBuffer.putValue(addr);
    }
#endif
}

inline void
HeapValue::writeBarrierPost(JSCompartment *comp, const Value &value, Value *addr)
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable())
        comp->gcStoreBuffer.putValue(addr);
#endif
}

inline void
HeapValue::post()
{
    writeBarrierPost(value, &value);
}

inline void
HeapValue::post(JSCompartment *comp)
{
    writeBarrierPost(comp, value, &value);
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
    post();
}

inline
RelocatableValue::RelocatableValue(const RelocatableValue &v)
    : EncapsulatedValue(v.value)
{
    JS_ASSERT(!IsPoisonedValue(v.value));
    post();
}

inline
RelocatableValue::~RelocatableValue()
{
    pre();
    relocate();
}

inline RelocatableValue &
RelocatableValue::operator=(const Value &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post();
    return *this;
}

inline RelocatableValue &
RelocatableValue::operator=(const RelocatableValue &v)
{
    pre();
    JS_ASSERT(!IsPoisonedValue(v.value));
    value = v.value;
    post();
    return *this;
}

inline void
RelocatableValue::post()
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        cell->compartment()->gcStoreBuffer.putRelocatableValue(&value);
    }
#endif
}

inline void
RelocatableValue::post(JSCompartment *comp)
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable())
        comp->gcStoreBuffer.putRelocatableValue(&value);
#endif
}

inline void
RelocatableValue::relocate()
{
#ifdef JSGC_GENERATIONAL
    if (value.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)value.toGCThing();
        cell->compartment()->gcStoreBuffer.removeRelocatableValue(&value);
    }
#endif
}

inline
HeapSlot::HeapSlot(JSObject *obj, uint32_t slot, const Value &v)
    : EncapsulatedValue(v)
{
    JS_ASSERT(!IsPoisonedValue(v));
    post(obj, slot);
}

inline
HeapSlot::HeapSlot(JSObject *obj, uint32_t slot, const HeapSlot &s)
    : EncapsulatedValue(s.value)
{
    JS_ASSERT(!IsPoisonedValue(s.value));
    post(obj, slot);
}

inline
HeapSlot::~HeapSlot()
{
    pre();
}

inline void
HeapSlot::init(JSObject *obj, uint32_t slot, const Value &v)
{
    value = v;
    post(obj, slot);
}

inline void
HeapSlot::init(JSCompartment *comp, JSObject *obj, uint32_t slot, const Value &v)
{
    value = v;
    post(comp, obj, slot);
}

inline void
HeapSlot::set(JSObject *obj, uint32_t slot, const Value &v)
{
    JS_ASSERT_IF(!obj->isArray(), &obj->getSlotRef(slot) == this);
    JS_ASSERT_IF(obj->isDenseArray(), &obj->getDenseArrayElement(slot) == (const Value *)this);

    pre();
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(obj, slot);
}

inline void
HeapSlot::set(JSCompartment *comp, JSObject *obj, uint32_t slot, const Value &v)
{
    JS_ASSERT_IF(!obj->isArray(), &const_cast<JSObject *>(obj)->getSlotRef(slot) == this);
    JS_ASSERT_IF(obj->isDenseArray(), &obj->getDenseArrayElement(slot) == (const Value *)this);
    JS_ASSERT(obj->compartment() == comp);

    pre(comp);
    JS_ASSERT(!IsPoisonedValue(v));
    value = v;
    post(comp, obj, slot);
}

inline void
HeapSlot::writeBarrierPost(JSObject *obj, uint32_t slot)
{
#ifdef JSGC_GENERATIONAL
    obj->compartment()->gcStoreBuffer.putSlot(obj, slot);
#endif
}

inline void
HeapSlot::writeBarrierPost(JSCompartment *comp, JSObject *obj, uint32_t slot)
{
#ifdef JSGC_GENERATIONAL
    comp->gcStoreBuffer.putSlot(obj, slot);
#endif
}

inline void
HeapSlot::post(JSObject *owner, uint32_t slot)
{
    HeapSlot::writeBarrierPost(owner, slot);
}

inline void
HeapSlot::post(JSCompartment *comp, JSObject *owner, uint32_t slot)
{
    HeapSlot::writeBarrierPost(comp, owner, slot);
}

#ifdef JSGC_GENERATIONAL
class SlotRangeRef : public gc::BufferableRef
{
    JSObject *owner;
    uint32_t start;
    uint32_t end;

  public:
    SlotRangeRef(JSObject *obj, uint32_t start, uint32_t end)
      : owner(obj), start(start), end(end)
    {
        JS_ASSERT(start < end);
    }

    bool match(void *location) {
        if (owner->isDenseArray()) {
            uint32_t len = owner->getDenseArrayInitializedLength();
            for (uint32_t i = Min(start, len); i < Min(end, len); ++i) {
                if (&owner->getDenseArrayElement(i) == location)
                    return true;
            }
            return false;
        }
        uint32_t span = owner->slotSpan();
        for (uint32_t i = Min(start, span); i < Min(end, span); ++i) {
            if (owner->getSlotAddress(i) == location)
                return true;
        }
        return false;
    }

    void mark(JSTracer *trc) {
        /* Apply forwarding, if we have already visited owner. */
        IsObjectMarked(&owner);
        if (owner->isDenseArray()) {
            uint32_t initLen = owner->getDenseArrayInitializedLength();
            uint32_t clampedStart = Min(start, initLen);
            gc::MarkArraySlots(trc, Min(end, initLen) - clampedStart,
                               owner->getDenseArrayElements() + clampedStart, "element");
            return;
        }
        uint32_t span = owner->slotSpan();
        uint32_t clampedStart = Min(start, span);
        MarkObjectSlots(trc, owner, clampedStart, Min(end, span) - clampedStart);
    }
};
#endif

inline void
SlotRangeWriteBarrierPost(JSCompartment *comp, JSObject *obj, uint32_t start, uint32_t count)
{
#ifdef JSGC_GENERATIONAL
    if (count > 0)
        comp->gcStoreBuffer.putGeneric(SlotRangeRef(obj, start, start + count));
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
        JSCompartment *comp = obj->compartment();
        if (comp->needsBarrier()) {
            js::gc::MarkObjectUnbarriered(comp->barrierTracer(), &obj, "write barrier");
            JS_ASSERT(obj == JSID_TO_OBJECT(value));
        }
    } else if (JSID_IS_STRING(value)) {
        JSString *str = JSID_TO_STRING(value);
        JSCompartment *comp = str->compartment();
        if (comp->needsBarrier()) {
            js::gc::MarkStringUnbarriered(comp->barrierTracer(), &str, "write barrier");
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

#endif /* jsgc_barrier_inl_h___ */
