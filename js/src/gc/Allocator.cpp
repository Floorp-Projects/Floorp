/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Allocator.h"

#include "jscntxt.h"

#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Nursery.h"
#include "jit/JitCompartment.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using namespace js;
using namespace gc;

bool
GCRuntime::gcIfNeededPerAllocation(JSContext* cx)
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
GCRuntime::checkAllocatorState(JSContext* cx, AllocKind kind)
{
    if (allowGC) {
        if (!gcIfNeededPerAllocation(cx))
            return false;
    }

#if defined(JS_GC_ZEAL) || defined(DEBUG)
    MOZ_ASSERT_IF(rt->isAtomsCompartment(cx->compartment()),
                  kind == AllocKind::STRING ||
                  kind == AllocKind::FAT_INLINE_STRING ||
                  kind == AllocKind::SYMBOL ||
                  kind == AllocKind::JITCODE);
    MOZ_ASSERT(!rt->isHeapBusy());
    MOZ_ASSERT(isAllocAllowed());
#endif

    // Crash if we perform a GC action when it is not safe.
    if (allowGC && !rt->mainThread.suppressGC)
        JS::AutoAssertOnGC::VerifyIsSafeToGC(rt);

    // For testing out of memory conditions
    if (js::oom::ShouldFailWithOOM()) {
        // If we are doing a fallible allocation, percolate up the OOM
        // instead of reporting it.
        if (allowGC)
            ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

template <typename T>
/* static */ void
GCRuntime::checkIncrementalZoneState(ExclusiveContext* cx, T* t)
{
#ifdef DEBUG
    if (!cx->isJSContext())
        return;

    Zone* zone = cx->asJSContext()->zone();
    MOZ_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                  t->asTenured().arenaHeader()->allocatedDuringIncremental);
#endif
}

template <typename T, AllowGC allowGC /* = CanGC */>
JSObject*
js::Allocate(ExclusiveContext* cx, AllocKind kind, size_t nDynamicSlots, InitialHeap heap,
             const Class* clasp)
{
    static_assert(mozilla::IsConvertible<T*, JSObject*>::value, "must be JSObject derived");
    MOZ_ASSERT(IsObjectAllocKind(kind));
    size_t thingSize = Arena::thingSize(kind);

    MOZ_ASSERT(thingSize == Arena::thingSize(kind));
    MOZ_ASSERT(thingSize >= sizeof(JSObject_Slots0));
    static_assert(sizeof(JSObject_Slots0) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    // Off-main-thread alloc cannot trigger GC or make runtime assertions.
    if (!cx->isJSContext())
        return GCRuntime::tryNewTenuredObject<NoGC>(cx, kind, thingSize, nDynamicSlots);

    JSContext* ncx = cx->asJSContext();
    JSRuntime* rt = ncx->runtime();
    if (!rt->gc.checkAllocatorState<allowGC>(ncx, kind))
        return nullptr;

    if (ncx->nursery().isEnabled() && heap != TenuredHeap) {
        JSObject* obj = rt->gc.tryNewNurseryObject<allowGC>(ncx, thingSize, nDynamicSlots, clasp);
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
template JSObject* js::Allocate<JSObject, NoGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                size_t nDynamicSlots, gc::InitialHeap heap,
                                                const Class* clasp);
template JSObject* js::Allocate<JSObject, CanGC>(ExclusiveContext* cx, gc::AllocKind kind,
                                                 size_t nDynamicSlots, gc::InitialHeap heap,
                                                 const Class* clasp);

// Attempt to allocate a new GC thing out of the nursery. If there is not enough
// room in the nursery or there is an OOM, this method will return nullptr.
template <AllowGC allowGC>
JSObject*
GCRuntime::tryNewNurseryObject(JSContext* cx, size_t thingSize, size_t nDynamicSlots, const Class* clasp)
{
    MOZ_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSObject* obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
    if (obj)
        return obj;

    if (allowGC && !rt->mainThread.suppressGC) {
        minorGC(cx, JS::gcreason::OUT_OF_NURSERY);

        // Exceeding gcMaxBytes while tenuring can disable the Nursery.
        if (nursery.isEnabled()) {
            JSObject* obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
            MOZ_ASSERT(obj);
            return obj;
        }
    }
    return nullptr;
}

template <AllowGC allowGC>
JSObject*
GCRuntime::tryNewTenuredObject(ExclusiveContext* cx, AllocKind kind, size_t thingSize,
                               size_t nDynamicSlots)
{
    HeapSlot* slots = nullptr;
    if (nDynamicSlots) {
        slots = cx->zone()->pod_malloc<HeapSlot>(nDynamicSlots);
        if (MOZ_UNLIKELY(!slots))
            return nullptr;
        Debug_SetSlotRangeToCrashOnTouch(slots, nDynamicSlots);
    }

    JSObject* obj = tryNewTenuredThing<JSObject, allowGC>(cx, kind, thingSize);

    if (obj)
        obj->setInitialSlotsMaybeNonNative(slots);
    else
        js_free(slots);

    return obj;
}

template <typename T, AllowGC allowGC /* = CanGC */>
T*
js::Allocate(ExclusiveContext* cx)
{
    static_assert(!mozilla::IsConvertible<T*, JSObject*>::value, "must not be JSObject derived");
    static_assert(sizeof(T) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    AllocKind kind = MapTypeToFinalizeKind<T>::kind;
    size_t thingSize = sizeof(T);
    MOZ_ASSERT(thingSize == Arena::thingSize(kind));

    if (cx->isJSContext()) {
        JSContext* ncx = cx->asJSContext();
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
    template type* js::Allocate<type, NoGC>(ExclusiveContext* cx);\
    template type* js::Allocate<type, CanGC>(ExclusiveContext* cx);
FOR_ALL_NON_OBJECT_GC_LAYOUTS(DECL_ALLOCATOR_INSTANCES)
#undef DECL_ALLOCATOR_INSTANCES

template <typename T, AllowGC allowGC>
/* static */ T*
GCRuntime::tryNewTenuredThing(ExclusiveContext* cx, AllocKind kind, size_t thingSize)
{
    T* t = reinterpret_cast<T*>(cx->arenas()->allocateFromFreeList(kind, thingSize));
    if (!t)
        t = reinterpret_cast<T*>(refillFreeListFromAnyThread<allowGC>(cx, kind, thingSize));

    checkIncrementalZoneState(cx, t);
    TraceTenuredAlloc(t, kind);
    return t;
}

template <AllowGC allowGC>
/* static */ void*
GCRuntime::refillFreeListFromAnyThread(ExclusiveContext* cx, AllocKind thingKind, size_t thingSize)
{
    MOZ_ASSERT(cx->arenas()->freeLists[thingKind].isEmpty());

    if (cx->isJSContext())
        return refillFreeListFromMainThread<allowGC>(cx->asJSContext(), thingKind, thingSize);

    return refillFreeListOffMainThread(cx, thingKind);
}

template <AllowGC allowGC>
/* static */ void*
GCRuntime::refillFreeListFromMainThread(JSContext* cx, AllocKind thingKind, size_t thingSize)
{
    JSRuntime* rt = cx->runtime();
    MOZ_ASSERT(!rt->isHeapBusy(), "allocating while under GC");
    MOZ_ASSERT_IF(allowGC, !rt->currentThreadHasExclusiveAccess());

    // Try to allocate; synchronize with background GC threads if necessary.
    void* thing = tryRefillFreeListFromMainThread(cx, thingKind);
    if (MOZ_LIKELY(thing))
        return thing;

    // Perform a last-ditch GC to hopefully free up some memory.
    {
        // If we are doing a fallible allocation, percolate up the OOM
        // instead of reporting it.
        if (!allowGC)
            return nullptr;

        JS::PrepareForFullGC(rt);
        AutoKeepAtoms keepAtoms(cx->perThreadData);
        rt->gc.gc(GC_SHRINK, JS::gcreason::LAST_DITCH);
    }

    // Retry the allocation after the last-ditch GC.
    // Note that due to GC callbacks we might already have allocated an arena
    // for this thing kind!
    thing = cx->arenas()->allocateFromFreeList(thingKind, thingSize);
    if (!thing)
        thing = tryRefillFreeListFromMainThread(cx, thingKind);
    if (thing)
        return thing;

    // We are really just totally out of memory.
    MOZ_ASSERT(allowGC, "A fallible allocation must not report OOM on failure.");
    ReportOutOfMemory(cx);
    return nullptr;
}

/* static */ void*
GCRuntime::tryRefillFreeListFromMainThread(JSContext* cx, AllocKind thingKind)
{
    ArenaLists* arenas = cx->arenas();
    Zone* zone = cx->zone();

    AutoMaybeStartBackgroundAllocation maybeStartBGAlloc;

    void* thing = arenas->allocateFromArena(zone, thingKind, maybeStartBGAlloc);
    if (MOZ_LIKELY(thing))
        return thing;

    // Even if allocateFromArena failed due to OOM, a background
    // finalization or allocation task may be running freeing more memory
    // or adding more available memory to our free pool; wait for them to
    // finish, then try to allocate again in case they made more memory
    // available.
    cx->runtime()->gc.waitBackgroundSweepOrAllocEnd();

    thing = arenas->allocateFromArena(zone, thingKind, maybeStartBGAlloc);
    if (thing)
        return thing;

    return nullptr;
}

/* static */ void*
GCRuntime::refillFreeListOffMainThread(ExclusiveContext* cx, AllocKind thingKind)
{
    ArenaLists* arenas = cx->arenas();
    Zone* zone = cx->zone();
    JSRuntime* rt = zone->runtimeFromAnyThread();

    AutoMaybeStartBackgroundAllocation maybeStartBGAlloc;

    // If we're off the main thread, we try to allocate once and return
    // whatever value we get. We need to first ensure the main thread is not in
    // a GC session.
    AutoLockHelperThreadState lock;
    while (rt->isHeapBusy())
        HelperThreadState().wait(GlobalHelperThreadState::PRODUCER);

    void* thing = arenas->allocateFromArena(zone, thingKind, maybeStartBGAlloc);
    if (thing)
        return thing;

    ReportOutOfMemory(cx);
    return nullptr;
}

TenuredCell*
ArenaLists::allocateFromArena(JS::Zone* zone, AllocKind thingKind,
                              AutoMaybeStartBackgroundAllocation& maybeStartBGAlloc)
{
    JSRuntime* rt = zone->runtimeFromAnyThread();
    mozilla::Maybe<AutoLockGC> maybeLock;

    // See if we can proceed without taking the GC lock.
    if (backgroundFinalizeState[thingKind] != BFS_DONE)
        maybeLock.emplace(rt);

    ArenaList& al = arenaLists[thingKind];
    ArenaHeader* aheader = al.takeNextArena();
    if (aheader) {
        // Empty arenas should be immediately freed.
        MOZ_ASSERT(!aheader->isEmpty());

        return allocateFromArenaInner<HasFreeThings>(zone, aheader, thingKind);
    }

    // Parallel threads have their own ArenaLists, but chunks are shared;
    // if we haven't already, take the GC lock now to avoid racing.
    if (maybeLock.isNothing())
        maybeLock.emplace(rt);

    Chunk* chunk = rt->gc.pickChunk(maybeLock.ref(), maybeStartBGAlloc);
    if (!chunk)
        return nullptr;

    // Although our chunk should definitely have enough space for another arena,
    // there are other valid reasons why Chunk::allocateArena() may fail.
    aheader = rt->gc.allocateArena(chunk, zone, thingKind, maybeLock.ref());
    if (!aheader)
        return nullptr;

    MOZ_ASSERT(!maybeLock->wasUnlocked());
    MOZ_ASSERT(al.isCursorAtEnd());
    al.insertAtCursor(aheader);

    return allocateFromArenaInner<IsEmpty>(zone, aheader, thingKind);
}

template <ArenaLists::ArenaAllocMode hasFreeThings>
TenuredCell*
ArenaLists::allocateFromArenaInner(JS::Zone* zone, ArenaHeader* aheader, AllocKind kind)
{
    size_t thingSize = Arena::thingSize(kind);

    FreeSpan span;
    if (hasFreeThings) {
        MOZ_ASSERT(aheader->hasFreeThings());
        span = aheader->getFirstFreeSpan();
        aheader->setAsFullyUsed();
    } else {
        MOZ_ASSERT(!aheader->hasFreeThings());
        Arena* arena = aheader->getArena();
        span.initFinal(arena->thingsStart(kind), arena->thingsEnd() - thingSize, thingSize);
    }
    freeLists[kind].setHead(&span);

    if (MOZ_UNLIKELY(zone->wasGCStarted()))
        zone->runtimeFromAnyThread()->gc.arenaAllocatedDuringGC(zone, aheader);
    TenuredCell* thing = freeLists[kind].allocate(thingSize);
    MOZ_ASSERT(thing); // This allocation is infallible.
    return thing;
}

void
GCRuntime::arenaAllocatedDuringGC(JS::Zone* zone, ArenaHeader* arena)
{
    if (zone->needsIncrementalBarrier()) {
        arena->allocatedDuringIncremental = true;
        marker.delayMarkingArena(arena);
    } else if (zone->isGCSweeping()) {
        arena->setNextAllocDuringSweep(arenasAllocatedDuringSweep);
        arenasAllocatedDuringSweep = arena;
    }
}


