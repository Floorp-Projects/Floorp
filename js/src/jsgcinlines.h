/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgcinlines_h
#define jsgcinlines_h

#include "jsgc.h"

#include "gc/Zone.h"
#include "vm/ForkJoin.h"

namespace js {

class Shape;

/*
 * This auto class should be used around any code that might cause a mark bit to
 * be set on an object in a dead zone. See AutoMaybeTouchDeadZones
 * for more details.
 */
struct AutoMarkInDeadZone
{
    explicit AutoMarkInDeadZone(JS::Zone *zone)
      : zone(zone),
        scheduled(zone->scheduledForDestruction)
    {
        gc::GCRuntime &gc = zone->runtimeFromMainThread()->gc;
        if (gc.isManipulatingDeadZones() && zone->scheduledForDestruction) {
            gc.incObjectsMarkedInDeadZone();
            zone->scheduledForDestruction = false;
        }
    }

    ~AutoMarkInDeadZone() {
        zone->scheduledForDestruction = scheduled;
    }

  private:
    JS::Zone *zone;
    bool scheduled;
};

inline Allocator *
ThreadSafeContext::allocator() const
{
    JS_ASSERT_IF(isJSContext(), &asJSContext()->zone()->allocator == allocator_);
    return allocator_;
}

template <typename T>
inline bool
ThreadSafeContext::isThreadLocal(T thing) const
{
    if (!isForkJoinContext())
        return true;

#ifdef JSGC_FJGENERATIONAL
    ForkJoinContext *cx = static_cast<ForkJoinContext*>(const_cast<ThreadSafeContext*>(this));
    if (cx->nursery().isInsideNewspace(thing))
        return true;
#endif

    // Global invariant
    JS_ASSERT(!IsInsideNursery(thing));

    // The thing is not in the nursery, but is it in the private tenured area?
    if (allocator_->arenas.containsArena(runtime_, thing->arenaHeader()))
    {
        // GC should be suppressed in preparation for mutating thread local
        // objects, as we don't want to trip any barriers.
        JS_ASSERT(!thing->zoneFromAnyThread()->needsBarrier());
        JS_ASSERT(!thing->runtimeFromAnyThread()->needsBarrier());

        return true;
    }

    return false;
}

namespace gc {

static inline AllocKind
GetGCObjectKind(const Class *clasp)
{
    if (clasp == FunctionClassPtr)
        return JSFunction::FinalizeKind;
    uint32_t nslots = JSCLASS_RESERVED_SLOTS(clasp);
    if (clasp->flags & JSCLASS_HAS_PRIVATE)
        nslots++;
    return GetGCObjectKind(nslots);
}

#ifdef JSGC_GENERATIONAL
inline bool
ShouldNurseryAllocate(const Nursery &nursery, AllocKind kind, InitialHeap heap)
{
    return nursery.isEnabled() && IsNurseryAllocable(kind) && heap != TenuredHeap;
}
#endif

#ifdef JSGC_FJGENERATIONAL
inline bool
ShouldFJNurseryAllocate(const ForkJoinNursery &nursery, AllocKind kind, InitialHeap heap)
{
    return IsFJNurseryAllocable(kind) && heap != TenuredHeap;
}
#endif

inline JSGCTraceKind
GetGCThingTraceKind(const void *thing)
{
    JS_ASSERT(thing);
    const Cell *cell = static_cast<const Cell *>(thing);
#ifdef JSGC_GENERATIONAL
    if (IsInsideNursery(cell))
        return JSTRACE_OBJECT;
#endif
    return MapAllocToTraceKind(cell->tenuredGetAllocKind());
}

inline void
GCRuntime::poke()
{
    poked = true;

#ifdef JS_GC_ZEAL
    /* Schedule a GC to happen "soon" after a GC poke. */
    if (zealMode == ZealPokeValue)
        nextScheduled = 1;
#endif
}

class ArenaIter
{
    ArenaHeader *aheader;
    ArenaHeader *remainingHeader;

  public:
    ArenaIter() {
        aheader = nullptr;
        remainingHeader = nullptr;
    }

    ArenaIter(JS::Zone *zone, AllocKind kind) {
        init(zone, kind);
    }

    void init(Allocator *allocator, AllocKind kind) {
        aheader = allocator->arenas.getFirstArena(kind);
        remainingHeader = allocator->arenas.getFirstArenaToSweep(kind);
        if (!aheader) {
            aheader = remainingHeader;
            remainingHeader = nullptr;
        }
    }

    void init(JS::Zone *zone, AllocKind kind) {
        init(&zone->allocator, kind);
    }

    bool done() const {
        return !aheader;
    }

    ArenaHeader *get() const {
        return aheader;
    }

    void next() {
        JS_ASSERT(!done());
        aheader = aheader->next;
        if (!aheader) {
            aheader = remainingHeader;
            remainingHeader = nullptr;
        }
    }
};

class ArenaCellIterImpl
{
    // These three are set in initUnsynchronized().
    size_t firstThingOffset;
    size_t thingSize;
#ifdef DEBUG
    bool isInited;
#endif

    // These three are set in reset() (which is called by init()).
    FreeSpan span;
    uintptr_t thing;
    uintptr_t limit;

    // Upon entry, |thing| points to any thing (free or used) and finds the
    // first used thing, which may be |thing|.
    void moveForwardIfFree() {
        JS_ASSERT(!done());
        JS_ASSERT(thing);
        // Note: if |span| is empty, this test will fail, which is what we want
        // -- |span| being empty means that we're past the end of the last free
        // thing, all the remaining things in the arena are used, and we'll
        // never need to move forward.
        if (thing == span.first) {
            thing = span.last + thingSize;
            span = *span.nextSpan();
        }
    }

  public:
    ArenaCellIterImpl()
      : firstThingOffset(0)     // Squelch
      , thingSize(0)            //   warnings
    {
    }

    void initUnsynchronized(ArenaHeader *aheader) {
        AllocKind kind = aheader->getAllocKind();
#ifdef DEBUG
        isInited = true;
#endif
        firstThingOffset = Arena::firstThingOffset(kind);
        thingSize = Arena::thingSize(kind);
        reset(aheader);
    }

    void init(ArenaHeader *aheader) {
#ifdef DEBUG
        AllocKind kind = aheader->getAllocKind();
        JS_ASSERT(aheader->zone->allocator.arenas.isSynchronizedFreeList(kind));
#endif
        initUnsynchronized(aheader);
    }

    // Use this to move from an Arena of a particular kind to another Arena of
    // the same kind.
    void reset(ArenaHeader *aheader) {
        JS_ASSERT(isInited);
        span = aheader->getFirstFreeSpan();
        uintptr_t arenaAddr = aheader->arenaAddress();
        thing = arenaAddr + firstThingOffset;
        limit = arenaAddr + ArenaSize;
        moveForwardIfFree();
    }

    bool done() const {
        return thing == limit;
    }

    Cell *getCell() const {
        JS_ASSERT(!done());
        return reinterpret_cast<Cell *>(thing);
    }

    template<typename T> T *get() const {
        JS_ASSERT(!done());
        return static_cast<T *>(getCell());
    }

    void next() {
        MOZ_ASSERT(!done());
        thing += thingSize;
        if (thing < limit)
            moveForwardIfFree();
    }
};

class ArenaCellIterUnderGC : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderGC(ArenaHeader *aheader) {
        JS_ASSERT(aheader->zone->runtimeFromAnyThread()->isHeapBusy());
        init(aheader);
    }
};

class ArenaCellIterUnderFinalize : public ArenaCellIterImpl
{
  public:
    explicit ArenaCellIterUnderFinalize(ArenaHeader *aheader) {
        initUnsynchronized(aheader);
    }
};

class ZoneCellIterImpl
{
    ArenaIter arenaIter;
    ArenaCellIterImpl cellIter;

  protected:
    ZoneCellIterImpl() {}

    void init(JS::Zone *zone, AllocKind kind) {
        JS_ASSERT(zone->allocator.arenas.isSynchronizedFreeList(kind));
        arenaIter.init(zone, kind);
        if (!arenaIter.done())
            cellIter.init(arenaIter.get());
    }

  public:
    bool done() const {
        return arenaIter.done();
    }

    template<typename T> T *get() const {
        JS_ASSERT(!done());
        return cellIter.get<T>();
    }

    Cell *getCell() const {
        JS_ASSERT(!done());
        return cellIter.getCell();
    }

    void next() {
        JS_ASSERT(!done());
        cellIter.next();
        if (cellIter.done()) {
            JS_ASSERT(!arenaIter.done());
            arenaIter.next();
            if (!arenaIter.done())
                cellIter.reset(arenaIter.get());
        }
    }
};

class ZoneCellIterUnderGC : public ZoneCellIterImpl
{
  public:
    ZoneCellIterUnderGC(JS::Zone *zone, AllocKind kind) {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(zone->runtimeFromAnyThread()->gc.nursery.isEmpty());
#endif
        JS_ASSERT(zone->runtimeFromAnyThread()->isHeapBusy());
        init(zone, kind);
    }
};

/* In debug builds, assert that no allocation occurs. */
class AutoAssertNoAlloc
{
#ifdef JS_DEBUG
    GCRuntime *gc;

  public:
    AutoAssertNoAlloc() : gc(nullptr) {}
    explicit AutoAssertNoAlloc(JSRuntime *rt) : gc(nullptr) {
        disallowAlloc(rt);
    }
    void disallowAlloc(JSRuntime *rt) {
        JS_ASSERT(!gc);
        gc = &rt->gc;
        gc->disallowAlloc();
    }
    ~AutoAssertNoAlloc() {
        if (gc)
            gc->allowAlloc();
    }
#else
  public:
    AutoAssertNoAlloc() {}
    explicit AutoAssertNoAlloc(JSRuntime *) {}
    void disallowAlloc(JSRuntime *rt) {}
#endif
};

class ZoneCellIter : public ZoneCellIterImpl
{
    AutoAssertNoAlloc noAlloc;
    ArenaLists *lists;
    AllocKind kind;

  public:
    ZoneCellIter(JS::Zone *zone, AllocKind kind)
      : lists(&zone->allocator.arenas),
        kind(kind)
    {
        /*
         * We have a single-threaded runtime, so there's no need to protect
         * against other threads iterating or allocating. However, we do have
         * background finalization; we have to wait for this to finish if it's
         * currently active.
         */
        if (IsBackgroundFinalized(kind) &&
            zone->allocator.arenas.needBackgroundFinalizeWait(kind))
        {
            gc::FinishBackgroundFinalize(zone->runtimeFromMainThread());
        }

#ifdef JSGC_GENERATIONAL
        /* Evict the nursery before iterating so we can see all things. */
        JSRuntime *rt = zone->runtimeFromMainThread();
        if (!rt->gc.nursery.isEmpty())
            MinorGC(rt, JS::gcreason::EVICT_NURSERY);
#endif

        if (lists->isSynchronizedFreeList(kind)) {
            lists = nullptr;
        } else {
            JS_ASSERT(!zone->runtimeFromMainThread()->isHeapBusy());
            lists->copyFreeListToArena(kind);
        }

        /* Assert that no GCs can occur while a ZoneCellIter is live. */
        noAlloc.disallowAlloc(zone->runtimeFromMainThread());

        init(zone, kind);
    }

    ~ZoneCellIter() {
        if (lists)
            lists->clearFreeListInArena(kind);
    }
};

class GCZonesIter
{
  private:
    ZonesIter zone;

  public:
    explicit GCZonesIter(JSRuntime *rt) : zone(rt, WithAtoms) {
        if (!zone->isCollecting())
            next();
    }

    bool done() const { return zone.done(); }

    void next() {
        JS_ASSERT(!done());
        do {
            zone.next();
        } while (!zone.done() && !zone->isCollecting());
    }

    JS::Zone *get() const {
        JS_ASSERT(!done());
        return zone;
    }

    operator JS::Zone *() const { return get(); }
    JS::Zone *operator->() const { return get(); }
};

typedef CompartmentsIterT<GCZonesIter> GCCompartmentsIter;

/* Iterates over all zones in the current zone group. */
class GCZoneGroupIter {
  private:
    JS::Zone *current;

  public:
    explicit GCZoneGroupIter(JSRuntime *rt) {
        JS_ASSERT(rt->isHeapBusy());
        current = rt->gc.getCurrentZoneGroup();
    }

    bool done() const { return !current; }

    void next() {
        JS_ASSERT(!done());
        current = current->nextNodeInGroup();
    }

    JS::Zone *get() const {
        JS_ASSERT(!done());
        return current;
    }

    operator JS::Zone *() const { return get(); }
    JS::Zone *operator->() const { return get(); }
};

typedef CompartmentsIterT<GCZoneGroupIter> GCCompartmentGroupIter;

#ifdef JSGC_GENERATIONAL
/*
 * Attempt to allocate a new GC thing out of the nursery. If there is not enough
 * room in the nursery or there is an OOM, this method will return nullptr.
 */
template <AllowGC allowGC>
inline JSObject *
TryNewNurseryObject(ThreadSafeContext *cxArg, size_t thingSize, size_t nDynamicSlots)
{
    JSContext *cx = cxArg->asJSContext();

    JS_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSRuntime *rt = cx->runtime();
    Nursery &nursery = rt->gc.nursery;
    JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots);
    if (obj)
        return obj;
    if (allowGC && !rt->mainThread.suppressGC) {
        MinorGC(cx, JS::gcreason::OUT_OF_NURSERY);

        /* Exceeding gcMaxBytes while tenuring can disable the Nursery. */
        if (nursery.isEnabled()) {
            JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots);
            JS_ASSERT(obj);
            return obj;
        }
    }
    return nullptr;
}
#endif /* JSGC_GENERATIONAL */

#ifdef JSGC_FJGENERATIONAL
template <AllowGC allowGC>
inline JSObject *
TryNewFJNurseryObject(ForkJoinContext *cx, size_t thingSize, size_t nDynamicSlots)
{
    ForkJoinNursery &nursery = cx->nursery();
    bool tooLarge = false;
    JSObject *obj = nursery.allocateObject(thingSize, nDynamicSlots, tooLarge);
    if (obj)
        return obj;

    if (!tooLarge && allowGC) {
        nursery.minorGC();
        obj = nursery.allocateObject(thingSize, nDynamicSlots, tooLarge);
        if (obj)
            return obj;
    }

    return nullptr;
}
#endif /* JSGC_FJGENERATIONAL */

static inline bool
PossiblyFail()
{
    JS_OOM_POSSIBLY_FAIL();
    return true;
}

template <AllowGC allowGC>
static inline bool
CheckAllocatorState(ThreadSafeContext *cx, AllocKind kind)
{
    if (!cx->isJSContext())
        return true;

    JSContext *ncx = cx->asJSContext();
    JSRuntime *rt = ncx->runtime();
#if defined(JS_GC_ZEAL) || defined(DEBUG)
    JS_ASSERT_IF(rt->isAtomsCompartment(ncx->compartment()),
                 kind == FINALIZE_STRING ||
                 kind == FINALIZE_FAT_INLINE_STRING ||
                 kind == FINALIZE_SYMBOL ||
                 kind == FINALIZE_JITCODE);
    JS_ASSERT(!rt->isHeapBusy());
    JS_ASSERT(rt->gc.isAllocAllowed());
#endif

    // Crash if we perform a GC action when it is not safe.
    if (allowGC && !rt->mainThread.suppressGC)
        JS::AutoAssertOnGC::VerifyIsSafeToGC(rt);

    // For testing out of memory conditions
    if (!PossiblyFail()) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    if (allowGC) {
#ifdef JS_GC_ZEAL
        if (rt->gc.needZealousGC())
            js::gc::RunDebugGC(ncx);
#endif

        if (rt->interrupt) {
            // Invoking the interrupt callback can fail and we can't usefully
            // handle that here. Just check in case we need to collect instead.
            js::gc::GCIfNeeded(ncx);
        }
    }

    return true;
}

template <typename T>
static inline void
CheckIncrementalZoneState(ThreadSafeContext *cx, T *t)
{
#ifdef DEBUG
    if (!cx->isJSContext())
        return;

    Zone *zone = cx->asJSContext()->zone();
    JS_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                 t->arenaHeader()->allocatedDuringIncremental);
#endif
}

/*
 * Allocate a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */

template <AllowGC allowGC>
inline JSObject *
AllocateObject(ThreadSafeContext *cx, AllocKind kind, size_t nDynamicSlots, InitialHeap heap)
{
    size_t thingSize = Arena::thingSize(kind);

    JS_ASSERT(thingSize == Arena::thingSize(kind));
    JS_ASSERT(thingSize >= sizeof(JSObject));
    static_assert(sizeof(JSObject) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    if (!CheckAllocatorState<allowGC>(cx, kind))
        return nullptr;

#ifdef JSGC_GENERATIONAL
    if (cx->isJSContext() &&
        ShouldNurseryAllocate(cx->asJSContext()->nursery(), kind, heap)) {
        JSObject *obj = TryNewNurseryObject<allowGC>(cx, thingSize, nDynamicSlots);
        if (obj)
            return obj;
    }
#endif
#ifdef JSGC_FJGENERATIONAL
    if (cx->isForkJoinContext() &&
        ShouldFJNurseryAllocate(cx->asForkJoinContext()->nursery(), kind, heap))
    {
        JSObject *obj =
            TryNewFJNurseryObject<allowGC>(cx->asForkJoinContext(), thingSize, nDynamicSlots);
        if (obj)
            return obj;
    }
#endif

    HeapSlot *slots = nullptr;
    if (nDynamicSlots) {
        slots = cx->pod_malloc<HeapSlot>(nDynamicSlots);
        if (MOZ_UNLIKELY(!slots))
            return nullptr;
        js::Debug_SetSlotRangeToCrashOnTouch(slots, nDynamicSlots);
    }

    JSObject *obj = static_cast<JSObject *>(cx->allocator()->arenas.allocateFromFreeList(kind, thingSize));
    if (!obj)
        obj = static_cast<JSObject *>(js::gc::ArenaLists::refillFreeList<allowGC>(cx, kind));

    if (obj)
        obj->setInitialSlots(slots);
    else
        js_free(slots);

    CheckIncrementalZoneState(cx, obj);
    return obj;
}

template <typename T, AllowGC allowGC>
inline T *
AllocateNonObject(ThreadSafeContext *cx)
{
    static_assert(sizeof(T) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    AllocKind kind = MapTypeToFinalizeKind<T>::kind;
    size_t thingSize = sizeof(T);

    JS_ASSERT(thingSize == Arena::thingSize(kind));
    if (!CheckAllocatorState<allowGC>(cx, kind))
        return nullptr;

    T *t = static_cast<T *>(cx->allocator()->arenas.allocateFromFreeList(kind, thingSize));
    if (!t)
        t = static_cast<T *>(js::gc::ArenaLists::refillFreeList<allowGC>(cx, kind));

    CheckIncrementalZoneState(cx, t);
    return t;
}

/*
 * When allocating for initialization from a cached object copy, we will
 * potentially destroy the cache entry we want to copy if we allow GC. On the
 * other hand, since these allocations are extremely common, we don't want to
 * delay GC from these allocation sites. Instead we allow the GC, but still
 * fail the allocation, forcing the non-cached path.
 *
 * Observe this won't be used for ForkJoin allocation, as it takes a JSContext*
 */
template <AllowGC allowGC>
inline JSObject *
AllocateObjectForCacheHit(JSContext *cx, AllocKind kind, InitialHeap heap)
{
#ifdef JSGC_GENERATIONAL
    if (ShouldNurseryAllocate(cx->nursery(), kind, heap)) {
        size_t thingSize = Arena::thingSize(kind);

        JS_ASSERT(thingSize == Arena::thingSize(kind));
        if (!CheckAllocatorState<NoGC>(cx, kind))
            return nullptr;

        JSObject *obj = TryNewNurseryObject<NoGC>(cx, thingSize, 0);
        if (!obj && allowGC) {
            MinorGC(cx, JS::gcreason::OUT_OF_NURSERY);
            return nullptr;
        }
        return obj;
    }
#endif

    JSObject *obj = AllocateObject<NoGC>(cx, kind, 0, heap);
    if (!obj && allowGC) {
        MaybeGC(cx);
        return nullptr;
    }

    return obj;
}

inline bool
IsInsideGGCNursery(const js::gc::Cell *cell)
{
#ifdef JSGC_GENERATIONAL
    if (!cell)
        return false;
    uintptr_t addr = uintptr_t(cell);
    addr &= ~js::gc::ChunkMask;
    addr |= js::gc::ChunkLocationOffset;
    uint32_t location = *reinterpret_cast<uint32_t *>(addr);
    JS_ASSERT(location != 0);
    return location & js::gc::ChunkLocationBitNursery;
#else
    return false;
#endif
}

} /* namespace gc */

template <js::AllowGC allowGC>
inline JSObject *
NewGCObject(js::ThreadSafeContext *cx, js::gc::AllocKind kind, size_t nDynamicSlots, js::gc::InitialHeap heap)
{
    JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind <= js::gc::FINALIZE_OBJECT_LAST);
    return js::gc::AllocateObject<allowGC>(cx, kind, nDynamicSlots, heap);
}

template <js::AllowGC allowGC>
inline jit::JitCode *
NewJitCode(js::ThreadSafeContext *cx)
{
    return gc::AllocateNonObject<jit::JitCode, allowGC>(cx);
}

inline
types::TypeObject *
NewTypeObject(js::ThreadSafeContext *cx)
{
    return gc::AllocateNonObject<types::TypeObject, js::CanGC>(cx);
}

template <js::AllowGC allowGC>
inline JSString *
NewGCString(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<JSString, allowGC>(cx);
}

template <js::AllowGC allowGC>
inline JSFatInlineString *
NewGCFatInlineString(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<JSFatInlineString, allowGC>(cx);
}

inline JSExternalString *
NewGCExternalString(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<JSExternalString, js::CanGC>(cx);
}

} /* namespace js */

inline JSScript *
js_NewGCScript(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<JSScript, js::CanGC>(cx);
}

inline js::LazyScript *
js_NewGCLazyScript(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<js::LazyScript, js::CanGC>(cx);
}

inline js::Shape *
js_NewGCShape(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<js::Shape, js::CanGC>(cx);
}

template <js::AllowGC allowGC>
inline js::BaseShape *
js_NewGCBaseShape(js::ThreadSafeContext *cx)
{
    return js::gc::AllocateNonObject<js::BaseShape, allowGC>(cx);
}

#endif /* jsgcinlines_h */
