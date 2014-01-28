/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgcinlines_h
#define jsgcinlines_h

#include "jsgc.h"

#include "gc/Zone.h"

namespace js {

class Shape;

/*
 * This auto class should be used around any code that might cause a mark bit to
 * be set on an object in a dead zone. See AutoMaybeTouchDeadZones
 * for more details.
 */
struct AutoMarkInDeadZone
{
    AutoMarkInDeadZone(JS::Zone *zone)
      : zone(zone),
        scheduled(zone->scheduledForDestruction)
    {
        JSRuntime *rt = zone->runtimeFromMainThread();
        if (rt->gcManipulatingDeadZones && zone->scheduledForDestruction) {
            rt->gcObjectsMarkedInDeadZones++;
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

inline Allocator *const
ThreadSafeContext::allocator()
{
    JS_ASSERT_IF(isJSContext(), &asJSContext()->zone()->allocator == allocator_);
    return allocator_;
}

template <typename T>
inline bool
ThreadSafeContext::isThreadLocal(T thing) const
{
    if (!isForkJoinSlice())
        return true;

    if (!IsInsideNursery(runtime_, thing) &&
        allocator_->arenas.containsArena(runtime_, thing->arenaHeader()))
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

inline JSGCTraceKind
GetGCThingTraceKind(const void *thing)
{
    JS_ASSERT(thing);
    const Cell *cell = static_cast<const Cell *>(thing);
#ifdef JSGC_GENERATIONAL
    if (IsInsideNursery(cell->runtimeFromMainThread(), cell))
        return JSTRACE_OBJECT;
#endif
    return MapAllocToTraceKind(cell->tenuredGetAllocKind());
}

static inline void
GCPoke(JSRuntime *rt)
{
    rt->gcPoke = true;

#ifdef JS_GC_ZEAL
    /* Schedule a GC to happen "soon" after a GC poke. */
    if (rt->gcZeal() == js::gc::ZealPokeValue)
        rt->gcNextScheduled = 1;
#endif
}

class ArenaIter
{
    ArenaHeader *aheader;
    ArenaHeader *remainingHeader;

  public:
    ArenaIter() {
        init();
    }

    ArenaIter(JS::Zone *zone, AllocKind kind) {
        init(zone, kind);
    }

    void init() {
        aheader = nullptr;
        remainingHeader = nullptr;
    }

    void init(ArenaHeader *aheaderArg) {
        aheader = aheaderArg;
        remainingHeader = nullptr;
    }

    void init(JS::Zone *zone, AllocKind kind) {
        aheader = zone->allocator.arenas.getFirstArena(kind);
        remainingHeader = zone->allocator.arenas.getFirstArenaToSweep(kind);
        if (!aheader) {
            aheader = remainingHeader;
            remainingHeader = nullptr;
        }
    }

    bool done() {
        return !aheader;
    }

    ArenaHeader *get() {
        return aheader;
    }

    void next() {
        aheader = aheader->next;
        if (!aheader) {
            aheader = remainingHeader;
            remainingHeader = nullptr;
        }
    }
};

class CellIterImpl
{
    size_t firstThingOffset;
    size_t thingSize;
    ArenaIter aiter;
    FreeSpan firstSpan;
    const FreeSpan *span;
    uintptr_t thing;
    Cell *cell;

  protected:
    CellIterImpl() {
    }

    void initSpan(JS::Zone *zone, AllocKind kind) {
        JS_ASSERT(zone->allocator.arenas.isSynchronizedFreeList(kind));
        firstThingOffset = Arena::firstThingOffset(kind);
        thingSize = Arena::thingSize(kind);
        firstSpan.initAsEmpty();
        span = &firstSpan;
        thing = span->first;
    }

    void init(ArenaHeader *singleAheader) {
        initSpan(singleAheader->zone, singleAheader->getAllocKind());
        aiter.init(singleAheader);
        next();
        aiter.init();
    }

    void init(JS::Zone *zone, AllocKind kind) {
        initSpan(zone, kind);
        aiter.init(zone, kind);
        next();
    }

  public:
    bool done() const {
        return !cell;
    }

    template<typename T> T *get() const {
        JS_ASSERT(!done());
        return static_cast<T *>(cell);
    }

    Cell *getCell() const {
        JS_ASSERT(!done());
        return cell;
    }

    void next() {
        for (;;) {
            if (thing != span->first)
                break;
            if (MOZ_LIKELY(span->hasNext())) {
                thing = span->last + thingSize;
                span = span->nextSpan();
                break;
            }
            if (aiter.done()) {
                cell = nullptr;
                return;
            }
            ArenaHeader *aheader = aiter.get();
            firstSpan = aheader->getFirstFreeSpan();
            span = &firstSpan;
            thing = aheader->arenaAddress() | firstThingOffset;
            aiter.next();
        }
        cell = reinterpret_cast<Cell *>(thing);
        thing += thingSize;
    }
};

class CellIterUnderGC : public CellIterImpl
{
  public:
    CellIterUnderGC(JS::Zone *zone, AllocKind kind) {
#ifdef JSGC_GENERATIONAL
        JS_ASSERT(zone->runtimeFromAnyThread()->gcNursery.isEmpty());
#endif
        JS_ASSERT(zone->runtimeFromAnyThread()->isHeapBusy());
        init(zone, kind);
    }

    CellIterUnderGC(ArenaHeader *aheader) {
        JS_ASSERT(aheader->zone->runtimeFromAnyThread()->isHeapBusy());
        init(aheader);
    }
};

class CellIter : public CellIterImpl
{
    ArenaLists *lists;
    AllocKind kind;
#ifdef DEBUG
    size_t *counter;
#endif
  public:
    CellIter(JS::Zone *zone, AllocKind kind)
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
        if (!rt->gcNursery.isEmpty())
            MinorGC(rt, JS::gcreason::EVICT_NURSERY);
#endif

        if (lists->isSynchronizedFreeList(kind)) {
            lists = nullptr;
        } else {
            JS_ASSERT(!zone->runtimeFromMainThread()->isHeapBusy());
            lists->copyFreeListToArena(kind);
        }

#ifdef DEBUG
        /* Assert that no GCs can occur while a CellIter is live. */
        counter = &zone->runtimeFromAnyThread()->noGCOrAllocationCheck;
        ++*counter;
#endif

        init(zone, kind);
    }

    ~CellIter() {
#ifdef DEBUG
        JS_ASSERT(*counter > 0);
        --*counter;
#endif
        if (lists)
            lists->clearFreeListInArena(kind);
    }
};

class GCZonesIter
{
  private:
    ZonesIter zone;

  public:
    GCZonesIter(JSRuntime *rt) : zone(rt, WithAtoms) {
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
    GCZoneGroupIter(JSRuntime *rt) {
        JS_ASSERT(rt->isHeapBusy());
        current = rt->gcCurrentZoneGroup;
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
template <typename T, AllowGC allowGC>
inline T *
TryNewNurseryGCThing(ThreadSafeContext *cxArg, size_t thingSize)
{
    /* TODO: Integrate PJS with generational GC. */
    JSContext *cx = cxArg->asJSContext();

    JS_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSRuntime *rt = cx->runtime();
    Nursery &nursery = rt->gcNursery;
    T *t = static_cast<T *>(nursery.allocate(thingSize));
    if (t)
        return t;
    if (allowGC && !rt->mainThread.suppressGC) {
        MinorGC(cx, JS::gcreason::OUT_OF_NURSERY);

        /* Exceeding gcMaxBytes while tenuring can disable the Nursery. */
        if (nursery.isEnabled()) {
            t = static_cast<T *>(nursery.allocate(thingSize));
            JS_ASSERT(t);
            return t;
        }
    }
    return nullptr;
}
#endif /* JSGC_GENERATIONAL */

/*
 * Allocates a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */
template <typename T, AllowGC allowGC>
inline T *
NewGCThing(ThreadSafeContext *cx, AllocKind kind, size_t thingSize, InitialHeap heap)
{
    JS_ASSERT(thingSize == js::gc::Arena::thingSize(kind));

    if (cx->isJSContext()) {
        JSContext *ncx = cx->asJSContext();
#if defined(JS_GC_ZEAL) || defined(DEBUG)
        JSRuntime *rt = ncx->runtime();
        JS_ASSERT_IF(rt->isAtomsCompartment(ncx->compartment()),
                     kind == FINALIZE_STRING ||
                     kind == FINALIZE_SHORT_STRING ||
                     kind == FINALIZE_JITCODE);
        JS_ASSERT(!rt->isHeapBusy());
        JS_ASSERT(!rt->noGCOrAllocationCheck);
#endif

        /* For testing out of memory conditions */
        JS_OOM_POSSIBLY_FAIL_REPORT(ncx);

#ifdef JS_GC_ZEAL
        if (allowGC && rt->needZealousGC())
            js::gc::RunDebugGC(ncx);
#endif

        if (allowGC)
            MaybeCheckStackRoots(ncx);
    }

#ifdef JSGC_GENERATIONAL
    if (cx->hasNursery() && ShouldNurseryAllocate(cx->nursery(), kind, heap)) {
        T *t = TryNewNurseryGCThing<T, allowGC>(cx, thingSize);
        if (t)
            return t;
    }
#endif

    T *t = static_cast<T *>(cx->allocator()->arenas.allocateFromFreeList(kind, thingSize));
    if (!t)
        t = static_cast<T *>(js::gc::ArenaLists::refillFreeList<allowGC>(cx, kind));

#ifdef DEBUG
    if (cx->isJSContext()) {
        Zone *zone = cx->asJSContext()->zone();
        JS_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                     t->arenaHeader()->allocatedDuringIncremental);
    }
#endif

    return t;
}

} /* namespace gc */
} /* namespace js */

template <js::AllowGC allowGC>
inline JSObject *
js_NewGCObject(js::ThreadSafeContext *cx, js::gc::AllocKind kind, js::gc::InitialHeap heap)
{
    JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind <= js::gc::FINALIZE_OBJECT_LAST);
    return js::gc::NewGCThing<JSObject, allowGC>(cx, kind, js::gc::Arena::thingSize(kind), heap);
}

template <js::AllowGC allowGC>
inline JSString *
js_NewGCString(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<JSString, allowGC>(cx, js::gc::FINALIZE_STRING,
                                                 sizeof(JSString), js::gc::TenuredHeap);
}

template <js::AllowGC allowGC>
inline JSShortString *
js_NewGCShortString(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<JSShortString, allowGC>(cx, js::gc::FINALIZE_SHORT_STRING,
                                                      sizeof(JSShortString), js::gc::TenuredHeap);
}

inline JSExternalString *
js_NewGCExternalString(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<JSExternalString, js::CanGC>(cx, js::gc::FINALIZE_EXTERNAL_STRING,
                                                           sizeof(JSExternalString), js::gc::TenuredHeap);
}

inline JSScript *
js_NewGCScript(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<JSScript, js::CanGC>(cx, js::gc::FINALIZE_SCRIPT,
                                                   sizeof(JSScript), js::gc::TenuredHeap);
}

inline js::LazyScript *
js_NewGCLazyScript(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<js::LazyScript, js::CanGC>(cx, js::gc::FINALIZE_LAZY_SCRIPT,
                                                         sizeof(js::LazyScript), js::gc::TenuredHeap);
}

inline js::Shape *
js_NewGCShape(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<js::Shape, js::CanGC>(cx, js::gc::FINALIZE_SHAPE,
                                                    sizeof(js::Shape), js::gc::TenuredHeap);
}

template <js::AllowGC allowGC>
inline js::BaseShape *
js_NewGCBaseShape(js::ThreadSafeContext *cx)
{
    return js::gc::NewGCThing<js::BaseShape, allowGC>(cx, js::gc::FINALIZE_BASE_SHAPE,
                                                      sizeof(js::BaseShape), js::gc::TenuredHeap);
}

#endif /* jsgcinlines_h */
