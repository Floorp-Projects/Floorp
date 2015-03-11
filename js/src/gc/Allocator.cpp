/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Allocator.h"

#include "mozilla/UniquePtr.h"

#include "jscntxt.h"

#include "gc/GCTrace.h"
#include "gc/Nursery.h"
#include "jit/JitCompartment.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using namespace js;
using namespace gc;

typedef mozilla::UniquePtr<HeapSlot, JS::FreePolicy> UniqueSlots;

static inline UniqueSlots
MakeSlotArray(ExclusiveContext *cx, size_t count)
{
    HeapSlot *slots = nullptr;
    if (count) {
        slots = cx->zone()->pod_malloc<HeapSlot>(count);
        if (slots)
            Debug_SetSlotRangeToCrashOnTouch(slots, count);
    }
    return UniqueSlots(slots);
}

// Attempt to allocate a new GC thing out of the nursery. If there is not enough
// room in the nursery or there is an OOM, this method will return nullptr.
template <AllowGC allowGC>
JSObject *
GCRuntime::tryNewNurseryObject(JSContext *cx, size_t thingSize, size_t nDynamicSlots, const Class *clasp)
{
    MOZ_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
    if (obj)
        return obj;

    if (allowGC && !rt->mainThread.suppressGC) {
        minorGC(cx, JS::gcreason::OUT_OF_NURSERY);

        // Exceeding gcMaxBytes while tenuring can disable the Nursery.
        if (nursery.isEnabled()) {
            JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
            MOZ_ASSERT(obj);
            return obj;
        }
    }
    return nullptr;
}

template <AllowGC allowGC>
JSObject *
GCRuntime::tryNewTenuredObject(ExclusiveContext *cx, AllocKind kind, size_t thingSize,
                               size_t nDynamicSlots)
{
    UniqueSlots slots = MakeSlotArray(cx, nDynamicSlots);
    if (nDynamicSlots && !slots)
        return nullptr;

    JSObject *obj = tryNewTenuredThing<JSObject, allowGC>(cx, kind, thingSize);

    if (obj)
        obj->setInitialSlotsMaybeNonNative(slots.release());

    return obj;
}

bool
GCRuntime::gcIfNeededPerAllocation(JSContext *cx)
{
#ifdef JS_GC_ZEAL
    if (needZealousGC())
        runDebugGC();
#endif

    // Invoking the interrupt callback can fail and we can't usefully
    // handle that here. Just check in case we need to collect instead.
    if (rt->hasPendingInterrupt())
        gcIfRequested(cx);

    // If we have grown past our GC heap threshold while in the middle of
    // an incremental GC, we're growing faster than we're GCing, so stop
    // the world and do a full, non-incremental GC right now, if possible.
    if (isIncrementalGCInProgress() &&
        cx->zone()->usage.gcBytes() > cx->zone()->threshold.gcTriggerBytes())
    {
        PrepareZoneForGC(cx->zone());
        AutoKeepAtoms keepAtoms(cx->perThreadData);
        gc(GC_NORMAL, JS::gcreason::INCREMENTAL_TOO_SLOW);
    }

    return true;
}

template <AllowGC allowGC>
bool
GCRuntime::checkAllocatorState(JSContext *cx, AllocKind kind)
{
    if (allowGC) {
        if (!gcIfNeededPerAllocation(cx))
            return false;
    }

#if defined(JS_GC_ZEAL) || defined(DEBUG)
    MOZ_ASSERT_IF(rt->isAtomsCompartment(cx->compartment()),
                  kind == FINALIZE_STRING ||
                  kind == FINALIZE_FAT_INLINE_STRING ||
                  kind == FINALIZE_SYMBOL ||
                  kind == FINALIZE_JITCODE);
    MOZ_ASSERT(!rt->isHeapBusy());
    MOZ_ASSERT(isAllocAllowed());
#endif

    // Crash if we perform a GC action when it is not safe.
    if (allowGC && !rt->mainThread.suppressGC)
        JS::AutoAssertOnGC::VerifyIsSafeToGC(rt);

    // For testing out of memory conditions
    if (js::oom::ShouldFailWithOOM()) {
        ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

template <typename T>
/* static */ void
GCRuntime::checkIncrementalZoneState(ExclusiveContext *cx, T *t)
{
#ifdef DEBUG
    if (!cx->isJSContext())
        return;

    Zone *zone = cx->asJSContext()->zone();
    MOZ_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                  t->asTenured().arenaHeader()->allocatedDuringIncremental);
#endif
}

/*
 * Allocate a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */

template <typename T, AllowGC allowGC /* = CanGC */>
JSObject *
js::Allocate(ExclusiveContext *cx, AllocKind kind, size_t nDynamicSlots, InitialHeap heap,
             const Class *clasp)
{
    static_assert(mozilla::IsConvertible<T *, JSObject *>::value, "must be JSObject derived");
    MOZ_ASSERT(kind >= FINALIZE_OBJECT0 && kind <= FINALIZE_OBJECT_LAST);
    size_t thingSize = Arena::thingSize(kind);

    MOZ_ASSERT(thingSize == Arena::thingSize(kind));
    MOZ_ASSERT(thingSize >= sizeof(JSObject_Slots0));
    static_assert(sizeof(JSObject_Slots0) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    // Off-main-thread alloc cannot trigger GC or make runtime assertions.
    if (!cx->isJSContext())
        return GCRuntime::tryNewTenuredObject<NoGC>(cx, kind, thingSize, nDynamicSlots);

    JSContext *ncx = cx->asJSContext();
    JSRuntime *rt = ncx->runtime();
    if (!rt->gc.checkAllocatorState<allowGC>(ncx, kind))
        return nullptr;

    if (ncx->nursery().isEnabled() && heap != TenuredHeap) {
        JSObject *obj = rt->gc.tryNewNurseryObject<allowGC>(ncx, thingSize, nDynamicSlots, clasp);
        if (obj)
            return obj;

        // Our most common non-jit allocation path is NoGC; thus, if we fail the
        // alloc and cannot GC, we *must* return nullptr here so that the caller
        // will do a CanGC allocation to clear the nursery. Failing to do so will
        // cause all allocations on this path to land in Tenured, and we will not
        // get the benefit of the nursery.
        if (!allowGC)
            return nullptr;
    }

    return GCRuntime::tryNewTenuredObject<allowGC>(cx, kind, thingSize, nDynamicSlots);
}
template JSObject *js::Allocate<JSObject, NoGC>(ExclusiveContext *cx, gc::AllocKind kind,
                                                size_t nDynamicSlots, gc::InitialHeap heap,
                                                const Class *clasp);
template JSObject *js::Allocate<JSObject, CanGC>(ExclusiveContext *cx, gc::AllocKind kind,
                                                 size_t nDynamicSlots, gc::InitialHeap heap,
                                                 const Class *clasp);

template <typename T, AllowGC allowGC /* = CanGC */>
T *
js::Allocate(ExclusiveContext *cx)
{
    static_assert(!mozilla::IsConvertible<T*, JSObject*>::value, "must not be JSObject derived");
    static_assert(sizeof(T) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    AllocKind kind = MapTypeToFinalizeKind<T>::kind;
    size_t thingSize = sizeof(T);
    MOZ_ASSERT(thingSize == Arena::thingSize(kind));

    if (cx->isJSContext()) {
        JSContext *ncx = cx->asJSContext();
        if (!ncx->runtime()->gc.checkAllocatorState<allowGC>(ncx, kind))
            return nullptr;
    }

    return GCRuntime::tryNewTenuredThing<T, allowGC>(cx, kind, thingSize);
}

#define FOR_ALL_NON_OBJECT_GC_LAYOUTS(macro) \
    macro(JS::Symbol) \
    macro(JSExternalString) \
    macro(JSFatInlineString) \
    macro(JSScript) \
    macro(JSString) \
    macro(js::AccessorShape) \
    macro(js::BaseShape) \
    macro(js::LazyScript) \
    macro(js::ObjectGroup) \
    macro(js::Shape) \
    macro(js::jit::JitCode)

#define DECL_ALLOCATOR_INSTANCES(type) \
    template type *js::Allocate<type, NoGC>(ExclusiveContext *cx);\
    template type *js::Allocate<type, CanGC>(ExclusiveContext *cx);
FOR_ALL_NON_OBJECT_GC_LAYOUTS(DECL_ALLOCATOR_INSTANCES)
#undef DECL_ALLOCATOR_INSTANCES

template <typename T, AllowGC allowGC>
/* static */ T *
GCRuntime::tryNewTenuredThing(ExclusiveContext *cx, AllocKind kind, size_t thingSize)
{
    T *t = reinterpret_cast<T *>(cx->arenas()->allocateFromFreeList(kind, thingSize));
    if (!t)
        t = reinterpret_cast<T *>(refillFreeListFromAnyThread<allowGC>(cx, kind));

    checkIncrementalZoneState(cx, t);
    TraceTenuredAlloc(t, kind);
    return t;
}
