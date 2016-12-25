/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Barrier.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "builtin/TypedObject.h"
#include "gc/Policy.h"
#include "gc/Zone.h"
#include "js/HashTable.h"
#include "js/Value.h"
#include "vm/EnvironmentObject.h"
#include "vm/SharedArrayObject.h"
#include "vm/Symbol.h"
#include "wasm/WasmJS.h"

namespace js {

bool
RuntimeFromMainThreadIsHeapMajorCollecting(JS::shadow::Zone* shadowZone)
{
    return shadowZone->runtimeFromMainThread()->isHeapMajorCollecting();
}

#ifdef DEBUG

bool
IsMarkedBlack(NativeObject* obj)
{
    // Note: we assume conservatively that Nursery things will be live.
    if (!obj->isTenured())
        return true;

    gc::TenuredCell& tenured = obj->asTenured();
    if (tenured.isMarked(gc::BLACK) || tenured.arena()->allocatedDuringIncremental)
        return true;

    return false;
}

bool
HeapSlot::preconditionForSet(NativeObject* owner, Kind kind, uint32_t slot) const
{
    return kind == Slot
         ? &owner->getSlotRef(slot) == this
         : &owner->getDenseElement(slot) == (const Value*)this;
}

void
HeapSlot::assertPreconditionForWriteBarrierPost(NativeObject* obj, Kind kind, uint32_t slot,
                                                const Value& target) const
{
    if (kind == Slot)
        MOZ_ASSERT(obj->getSlotAddressUnchecked(slot)->get() == target);
    else
        MOZ_ASSERT(static_cast<HeapSlot*>(obj->getDenseElements() + slot)->get() == target);

    MOZ_ASSERT_IF(target.isMarkable() && IsMarkedBlack(obj),
                  !JS::GCThingIsMarkedGray(JS::GCCellPtr(target)));
}

bool
CurrentThreadIsIonCompiling()
{
    return TlsPerThreadData.get()->ionCompiling;
}

bool
CurrentThreadIsIonCompilingSafeForMinorGC()
{
    return TlsPerThreadData.get()->ionCompilingSafeForMinorGC;
}

bool
CurrentThreadIsGCSweeping()
{
    return TlsPerThreadData.get()->gcSweeping;
}

#endif // DEBUG

template <typename S>
template <typename T>
void
ReadBarrierFunctor<S>::operator()(T* t)
{
    InternalBarrierMethods<T*>::readBarrier(t);
}

// All GC things may be held in a Value, either publicly or as a private GC
// thing.
#define JS_EXPAND_DEF(name, type, _) \
template void ReadBarrierFunctor<JS::Value>::operator()<type>(type*);
JS_FOR_EACH_TRACEKIND(JS_EXPAND_DEF);
#undef JS_EXPAND_DEF

template <typename S>
template <typename T>
void
PreBarrierFunctor<S>::operator()(T* t)
{
    InternalBarrierMethods<T*>::preBarrier(t);
}

// All GC things may be held in a Value, either publicly or as a private GC
// thing.
#define JS_EXPAND_DEF(name, type, _) \
template void PreBarrierFunctor<JS::Value>::operator()<type>(type*);
JS_FOR_EACH_TRACEKIND(JS_EXPAND_DEF);
#undef JS_EXPAND_DEF

template void PreBarrierFunctor<jsid>::operator()<JS::Symbol>(JS::Symbol*);
template void PreBarrierFunctor<jsid>::operator()<JSString>(JSString*);

template <typename T>
/* static */ bool
MovableCellHasher<T>::hasHash(const Lookup& l)
{
    if (!l)
        return true;

    return l->zoneFromAnyThread()->hasUniqueId(l);
}

template <typename T>
/* static */ bool
MovableCellHasher<T>::ensureHash(const Lookup& l)
{
    if (!l)
        return true;

    uint64_t unusedId;
    return l->zoneFromAnyThread()->getUniqueId(l, &unusedId);
}

template <typename T>
/* static */ HashNumber
MovableCellHasher<T>::hash(const Lookup& l)
{
    if (!l)
        return 0;

    // We have to access the zone from-any-thread here: a worker thread may be
    // cloning a self-hosted object from the main-thread-runtime-owned self-
    // hosting zone into the off-main-thread runtime. The zone's uid lock will
    // protect against multiple workers doing this simultaneously.
    MOZ_ASSERT(CurrentThreadCanAccessZone(l->zoneFromAnyThread()) ||
               l->zoneFromAnyThread()->isSelfHostingZone());

    return l->zoneFromAnyThread()->getHashCodeInfallible(l);
}

template <typename T>
/* static */ bool
MovableCellHasher<T>::match(const Key& k, const Lookup& l)
{
    // Return true if both are null or false if only one is null.
    if (!k)
        return !l;
    if (!l)
        return false;

    MOZ_ASSERT(k);
    MOZ_ASSERT(l);
    MOZ_ASSERT(CurrentThreadCanAccessZone(l->zoneFromAnyThread()) ||
               l->zoneFromAnyThread()->isSelfHostingZone());

    Zone* zone = k->zoneFromAnyThread();
    if (zone != l->zoneFromAnyThread())
        return false;
    MOZ_ASSERT(zone->hasUniqueId(k));
    MOZ_ASSERT(zone->hasUniqueId(l));

    // Since both already have a uid (from hash), the get is infallible.
    return zone->getUniqueIdInfallible(k) == zone->getUniqueIdInfallible(l);
}

template struct MovableCellHasher<JSObject*>;
template struct MovableCellHasher<GlobalObject*>;
template struct MovableCellHasher<SavedFrame*>;
template struct MovableCellHasher<EnvironmentObject*>;
template struct MovableCellHasher<WasmInstanceObject*>;
template struct MovableCellHasher<JSScript*>;

} // namespace js

JS_PUBLIC_API(void)
JS::HeapObjectPostBarrier(JSObject** objp, JSObject* prev, JSObject* next)
{
    MOZ_ASSERT(objp);
    js::InternalBarrierMethods<JSObject*>::postBarrier(objp, prev, next);
}

JS_PUBLIC_API(void)
JS::HeapValuePostBarrier(JS::Value* valuep, const Value& prev, const Value& next)
{
    MOZ_ASSERT(valuep);
    js::InternalBarrierMethods<JS::Value>::postBarrier(valuep, prev, next);
}
