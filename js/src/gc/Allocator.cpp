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

#include "gc/Heap-inl.h"

using namespace js;
using namespace gc;

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

    MOZ_ASSERT_IF(nDynamicSlots != 0, clasp->isNative() || clasp->isProxy());

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
    MOZ_ASSERT(isNurseryAllocAllowed());
    MOZ_ASSERT(!cx->zone()->usedByExclusiveThread);
    MOZ_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSObject* obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
    if (obj)
        return obj;

    if (allowGC && !rt->mainThread.suppressGC) {
        minorGC(JS::gcreason::OUT_OF_NURSERY);

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
        if (MOZ_UNLIKELY(!slots)) {
            if (allowGC)
                ReportOutOfMemory(cx);
            return nullptr;
        }
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

#define DECL_ALLOCATOR_INSTANCES(allocKind, traceKind, type, sizedType) \
    template type* js::Allocate<type, NoGC>(ExclusiveContext* cx);\
    template type* js::Allocate<type, CanGC>(ExclusiveContext* cx);
FOR_EACH_NONOBJECT_ALLOCKIND(DECL_ALLOCATOR_INSTANCES)
#undef DECL_ALLOCATOR_INSTANCES

template <typename T, AllowGC allowGC>
/* static */ T*
GCRuntime::tryNewTenuredThing(ExclusiveContext* cx, AllocKind kind, size_t thingSize)
{
    // Bump allocate in the arena's current free-list span.
    T* t = reinterpret_cast<T*>(cx->arenas()->allocateFromFreeList(kind, thingSize));
    if (MOZ_UNLIKELY(!t)) {
        // Get the next available free list and allocate out of it. This may
        // acquire a new arena, which will lock the chunk list. If there are no
        // chunks available it may also allocate new memory directly.
        t = reinterpret_cast<T*>(refillFreeListFromAnyThread(cx, kind, thingSize));

        if (MOZ_UNLIKELY(!t && allowGC && cx->isJSContext())) {
            // We have no memory available for a new chunk; perform an
            // all-compartments, non-incremental, shrinking GC and wait for
            // sweeping to finish.
            JS::PrepareForFullGC(cx->asJSContext());
            AutoKeepAtoms keepAtoms(cx->perThreadData);
            cx->asJSContext()->gc.gc(GC_SHRINK, JS::gcreason::LAST_DITCH);
            cx->asJSContext()->gc.waitBackgroundSweepOrAllocEnd();

            t = tryNewTenuredThing<T, NoGC>(cx, kind, thingSize);
            if (!t)
                ReportOutOfMemory(cx);
        }
    }

    checkIncrementalZoneState(cx, t);
    TraceTenuredAlloc(t, kind);
    return t;
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
    MOZ_ASSERT_IF(cx->compartment()->isAtomsCompartment(),
                  kind == AllocKind::ATOM ||
                  kind == AllocKind::FAT_INLINE_ATOM ||
                  kind == AllocKind::SYMBOL ||
                  kind == AllocKind::JITCODE ||
                  kind == AllocKind::SCOPE);
    MOZ_ASSERT_IF(!cx->compartment()->isAtomsCompartment(),
                  kind != AllocKind::ATOM &&
                  kind != AllocKind::FAT_INLINE_ATOM);
    MOZ_ASSERT(!rt->isHeapBusy());
    MOZ_ASSERT(isAllocAllowed());
#endif

    // Crash if we perform a GC action when it is not safe.
    if (allowGC && !rt->mainThread.suppressGC)
        rt->gc.verifyIsSafeToGC();

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
        gcIfRequested();

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

template <typename T>
/* static */ void
GCRuntime::checkIncrementalZoneState(ExclusiveContext* cx, T* t)
{
#ifdef DEBUG
    if (!cx->isJSContext())
        return;

    Zone* zone = cx->asJSContext()->zone();
    MOZ_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                  t->asTenured().arena()->allocatedDuringIncremental);
#endif
}


// ///////////  Arena -> Thing Allocator  //////////////////////////////////////

void
GCRuntime::startBackgroundAllocTaskIfIdle()
{
    AutoLockHelperThreadState helperLock;
    if (allocTask.isRunningWithLockHeld(helperLock))
        return;

    // Join the previous invocation of the task. This will return immediately
    // if the thread has never been started.
    allocTask.joinWithLockHeld(helperLock);
    allocTask.startWithLockHeld(helperLock);
}

/* static */ TenuredCell*
GCRuntime::refillFreeListFromAnyThread(ExclusiveContext* cx, AllocKind thingKind, size_t thingSize)
{
    cx->arenas()->checkEmptyFreeList(thingKind);

    if (cx->isJSContext())
        return refillFreeListFromMainThread(cx->asJSContext(), thingKind, thingSize);

    return refillFreeListOffMainThread(cx, thingKind);
}

/* static */ TenuredCell*
GCRuntime::refillFreeListFromMainThread(JSContext* cx, AllocKind thingKind, size_t thingSize)
{
    // It should not be possible to allocate on the main thread while we are
    // inside a GC.
    Zone *zone = cx->zone();
    MOZ_ASSERT(!cx->runtime()->isHeapBusy(), "allocating while under GC");

    AutoMaybeStartBackgroundAllocation maybeStartBGAlloc;
    return cx->arenas()->allocateFromArena(zone, thingKind, CheckThresholds, maybeStartBGAlloc);
}

/* static */ TenuredCell*
GCRuntime::refillFreeListOffMainThread(ExclusiveContext* cx, AllocKind thingKind)
{
    // A GC may be happening on the main thread, but zones used by exclusive
    // contexts are never collected.
    Zone* zone = cx->zone();
    MOZ_ASSERT(!zone->wasGCStarted());

    AutoMaybeStartBackgroundAllocation maybeStartBGAlloc;
    return cx->arenas()->allocateFromArena(zone, thingKind, CheckThresholds, maybeStartBGAlloc);
}

/* static */ TenuredCell*
GCRuntime::refillFreeListInGC(Zone* zone, AllocKind thingKind)
{
    /*
     * Called by compacting GC to refill a free list while we are in a GC.
     */

    zone->arenas.checkEmptyFreeList(thingKind);
    mozilla::DebugOnly<JSRuntime*> rt = zone->runtimeFromMainThread();
    MOZ_ASSERT(rt->isHeapCollecting());
    MOZ_ASSERT_IF(!rt->isHeapMinorCollecting(), !rt->gc.isBackgroundSweeping());

    AutoMaybeStartBackgroundAllocation maybeStartBackgroundAllocation;
    return zone->arenas.allocateFromArena(zone, thingKind, DontCheckThresholds,
                                          maybeStartBackgroundAllocation);
}

TenuredCell*
ArenaLists::allocateFromArena(JS::Zone* zone, AllocKind thingKind,
                              ShouldCheckThresholds checkThresholds,
                              AutoMaybeStartBackgroundAllocation& maybeStartBGAlloc)
{
    JSRuntime* rt = zone->runtimeFromAnyThread();

    mozilla::Maybe<AutoLockGC> maybeLock;

    // See if we can proceed without taking the GC lock.
    if (backgroundFinalizeState[thingKind] != BFS_DONE)
        maybeLock.emplace(rt);

    ArenaList& al = arenaLists[thingKind];
    Arena* arena = al.takeNextArena();
    if (arena) {
        // Empty arenas should be immediately freed.
        MOZ_ASSERT(!arena->isEmpty());

        return allocateFromArenaInner(zone, arena, thingKind);
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
    arena = rt->gc.allocateArena(chunk, zone, thingKind, checkThresholds, maybeLock.ref());
    if (!arena)
        return nullptr;

    MOZ_ASSERT(al.isCursorAtEnd());
    al.insertBeforeCursor(arena);

    return allocateFromArenaInner(zone, arena, thingKind);
}

inline TenuredCell*
ArenaLists::allocateFromArenaInner(JS::Zone* zone, Arena* arena, AllocKind kind)
{
    size_t thingSize = Arena::thingSize(kind);

    freeLists[kind] = arena->getFirstFreeSpan();

    if (MOZ_UNLIKELY(zone->wasGCStarted()))
        zone->runtimeFromAnyThread()->gc.arenaAllocatedDuringGC(zone, arena);
    TenuredCell* thing = freeLists[kind]->allocate(thingSize);
    MOZ_ASSERT(thing); // This allocation is infallible.
    return thing;
}

void
GCRuntime::arenaAllocatedDuringGC(JS::Zone* zone, Arena* arena)
{
    if (zone->needsIncrementalBarrier()) {
        arena->allocatedDuringIncremental = true;
        marker.delayMarkingArena(arena);
    } else if (zone->isGCSweeping()) {
        arena->setNextAllocDuringSweep(arenasAllocatedDuringSweep);
        arenasAllocatedDuringSweep = arena;
    }
}


// ///////////  Chunk -> Arena Allocator  //////////////////////////////////////

bool
GCRuntime::wantBackgroundAllocation(const AutoLockGC& lock) const
{
    // To minimize memory waste, we do not want to run the background chunk
    // allocation if we already have some empty chunks or when the runtime has
    // a small heap size (and therefore likely has a small growth rate).
    return allocTask.enabled() &&
           emptyChunks(lock).count() < tunables.minEmptyChunkCount(lock) &&
           (fullChunks(lock).count() + availableChunks(lock).count()) >= 4;
}

Arena*
GCRuntime::allocateArena(Chunk* chunk, Zone* zone, AllocKind thingKind,
                         ShouldCheckThresholds checkThresholds, const AutoLockGC& lock)
{
    MOZ_ASSERT(chunk->hasAvailableArenas());

    // Fail the allocation if we are over our heap size limits.
    if (checkThresholds && usage.gcBytes() >= tunables.gcMaxBytes())
        return nullptr;

    Arena* arena = chunk->allocateArena(rt, zone, thingKind, lock);
    zone->usage.addGCArena();

    // Trigger an incremental slice if needed.
    if (checkThresholds)
        maybeAllocTriggerZoneGC(zone, lock);

    return arena;
}

Arena*
Chunk::allocateArena(JSRuntime* rt, Zone* zone, AllocKind thingKind, const AutoLockGC& lock)
{
    Arena* arena = info.numArenasFreeCommitted > 0
                   ? fetchNextFreeArena(rt)
                   : fetchNextDecommittedArena();
    arena->init(zone, thingKind);
    updateChunkListAfterAlloc(rt, lock);
    return arena;
}

inline void
GCRuntime::updateOnFreeArenaAlloc(const ChunkInfo& info)
{
    MOZ_ASSERT(info.numArenasFreeCommitted <= numArenasFreeCommitted);
    --numArenasFreeCommitted;
}

Arena*
Chunk::fetchNextFreeArena(JSRuntime* rt)
{
    MOZ_ASSERT(info.numArenasFreeCommitted > 0);
    MOZ_ASSERT(info.numArenasFreeCommitted <= info.numArenasFree);

    Arena* arena = info.freeArenasHead;
    info.freeArenasHead = arena->next;
    --info.numArenasFreeCommitted;
    --info.numArenasFree;
    rt->gc.updateOnFreeArenaAlloc(info);

    return arena;
}

Arena*
Chunk::fetchNextDecommittedArena()
{
    MOZ_ASSERT(info.numArenasFreeCommitted == 0);
    MOZ_ASSERT(info.numArenasFree > 0);

    unsigned offset = findDecommittedArenaOffset();
    info.lastDecommittedArenaOffset = offset + 1;
    --info.numArenasFree;
    decommittedArenas.unset(offset);

    Arena* arena = &arenas[offset];
    MarkPagesInUse(arena, ArenaSize);
    arena->setAsNotAllocated();

    return arena;
}

/*
 * Search for and return the next decommitted Arena. Our goal is to keep
 * lastDecommittedArenaOffset "close" to a free arena. We do this by setting
 * it to the most recently freed arena when we free, and forcing it to
 * the last alloc + 1 when we allocate.
 */
uint32_t
Chunk::findDecommittedArenaOffset()
{
    /* Note: lastFreeArenaOffset can be past the end of the list. */
    for (unsigned i = info.lastDecommittedArenaOffset; i < ArenasPerChunk; i++) {
        if (decommittedArenas.get(i))
            return i;
    }
    for (unsigned i = 0; i < info.lastDecommittedArenaOffset; i++) {
        if (decommittedArenas.get(i))
            return i;
    }
    MOZ_CRASH("No decommitted arenas found.");
}


// ///////////  System -> Chunk Allocator  /////////////////////////////////////

Chunk*
GCRuntime::getOrAllocChunk(const AutoLockGC& lock,
                           AutoMaybeStartBackgroundAllocation& maybeStartBackgroundAllocation)
{
    Chunk* chunk = emptyChunks(lock).pop();
    if (!chunk) {
        chunk = Chunk::allocate(rt);
        if (!chunk)
            return nullptr;
        MOZ_ASSERT(chunk->info.numArenasFreeCommitted == 0);
    }

    if (wantBackgroundAllocation(lock))
        maybeStartBackgroundAllocation.tryToStartBackgroundAllocation(rt->gc);

    return chunk;
}

void
GCRuntime::recycleChunk(Chunk* chunk, const AutoLockGC& lock)
{
    emptyChunks(lock).push(chunk);
}

Chunk*
GCRuntime::pickChunk(const AutoLockGC& lock,
                     AutoMaybeStartBackgroundAllocation& maybeStartBackgroundAllocation)
{
    if (availableChunks(lock).count())
        return availableChunks(lock).head();

    Chunk* chunk = getOrAllocChunk(lock, maybeStartBackgroundAllocation);
    if (!chunk)
        return nullptr;

    chunk->init(rt);
    MOZ_ASSERT(chunk->info.numArenasFreeCommitted == 0);
    MOZ_ASSERT(chunk->unused());
    MOZ_ASSERT(!fullChunks(lock).contains(chunk));
    MOZ_ASSERT(!availableChunks(lock).contains(chunk));

    chunkAllocationSinceLastGC = true;

    availableChunks(lock).push(chunk);

    return chunk;
}

BackgroundAllocTask::BackgroundAllocTask(JSRuntime* rt, ChunkPool& pool)
  : runtime(rt),
    chunkPool_(pool),
    enabled_(CanUseExtraThreads() && GetCPUCount() >= 2)
{
}

/* virtual */ void
BackgroundAllocTask::run()
{
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logAllocation(logger, TraceLogger_GCAllocation);

    AutoLockGC lock(runtime);
    while (!cancel_ && runtime->gc.wantBackgroundAllocation(lock)) {
        Chunk* chunk;
        {
            AutoUnlockGC unlock(lock);
            chunk = Chunk::allocate(runtime);
            if (!chunk)
                break;
            chunk->init(runtime);
        }
        chunkPool_.push(chunk);
    }
}

/* static */ Chunk*
Chunk::allocate(JSRuntime* rt)
{
    Chunk* chunk = static_cast<Chunk*>(MapAlignedPages(ChunkSize, ChunkSize));
    if (!chunk)
        return nullptr;
    rt->gc.stats.count(gcstats::STAT_NEW_CHUNK);
    return chunk;
}

void
Chunk::init(JSRuntime* rt)
{
    JS_POISON(this, JS_FRESH_TENURED_PATTERN, ChunkSize);

    /*
     * We clear the bitmap to guard against JS::GCThingIsMarkedGray being called
     * on uninitialized data, which would happen before the first GC cycle.
     */
    bitmap.clear();

    /*
     * Decommit the arenas. We do this after poisoning so that if the OS does
     * not have to recycle the pages, we still get the benefit of poisoning.
     */
    decommitAllArenas(rt);

    /* Initialize the chunk info. */
    info.init();
    new (&trailer) ChunkTrailer(rt);

    /* The rest of info fields are initialized in pickChunk. */
}

void Chunk::decommitAllArenas(JSRuntime* rt)
{
    decommittedArenas.clear(true);
    MarkPagesUnused(&arenas[0], ArenasPerChunk * ArenaSize);

    info.freeArenasHead = nullptr;
    info.lastDecommittedArenaOffset = 0;
    info.numArenasFree = ArenasPerChunk;
    info.numArenasFreeCommitted = 0;
}
