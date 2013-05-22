/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "jscompartment.h"
#include "jsgc.h"

#include "gc/GCInternals.h"
#include "gc/Memory.h"
#include "vm/Debugger.h"

#include "gc/Barrier-inl.h"
#include "gc/Nursery-inl.h"

using namespace js;
using namespace gc;
using namespace mozilla;

bool
js::Nursery::enable()
{
    if (isEnabled())
        return true;

    if (!hugeSlots.init())
        return false;

    fallbackBitmap.clear(false);

    void *heap = MapAlignedPages(NurserySize, Alignment);
    if (!heap)
        return false;

    JSRuntime *rt = runtime();
    rt->gcNurseryStart_ = position_ = uintptr_t(heap);
    rt->gcNurseryEnd_ = start() + NurseryUsableSize;
    asLayout().runtime = rt;
    JS_POISON(asLayout().data, FreshNursery, sizeof(asLayout().data));

    JS_ASSERT(isEnabled());
    return true;
}

void
js::Nursery::disable()
{
    if (!isEnabled())
        return;

    hugeSlots.finish();
    JS_ASSERT(start());
    UnmapPages((void *)start(), NurserySize);
    runtime()->gcNurseryStart_ = runtime()->gcNurseryEnd_ = position_ = 0;
}

js::Nursery::~Nursery()
{
    disable();
}

void *
js::Nursery::allocate(size_t size)
{
    JS_ASSERT(size % ThingAlignment == 0);
    JS_ASSERT(position() % ThingAlignment == 0);
    JS_ASSERT(!runtime()->isHeapBusy());

    if (position() + size > end())
        return NULL;

    void *thing = (void *)position();
    position_ = position() + size;

    JS_POISON(thing, AllocatedThing, size);
    return thing;
}

/* Internally, this function is used to allocate elements as well as slots. */
HeapSlot *
js::Nursery::allocateSlots(JSContext *cx, JSObject *obj, uint32_t nslots)
{
    JS_ASSERT(obj);
    JS_ASSERT(nslots > 0);

    if (!isInside(obj))
        return cx->pod_malloc<HeapSlot>(nslots);

    if (nslots > MaxNurserySlots)
        return allocateHugeSlots(cx, nslots);

    size_t size = sizeof(HeapSlot) * nslots;
    HeapSlot *slots = static_cast<HeapSlot *>(allocate(size));
    if (slots)
        return slots;

    return allocateHugeSlots(cx, nslots);
}

ObjectElements *
js::Nursery::allocateElements(JSContext *cx, JSObject *obj, uint32_t nelems)
{
    return reinterpret_cast<ObjectElements *>(allocateSlots(cx, obj, nelems));
}

HeapSlot *
js::Nursery::reallocateSlots(JSContext *cx, JSObject *obj, HeapSlot *oldSlots,
                             uint32_t oldCount, uint32_t newCount)
{
    size_t oldSize = oldCount * sizeof(HeapSlot);
    size_t newSize = newCount * sizeof(HeapSlot);

    if (!isInside(obj))
        return static_cast<HeapSlot *>(cx->realloc_(oldSlots, oldSize, newSize));

    if (!isInside(oldSlots)) {
        HeapSlot *newSlots = static_cast<HeapSlot *>(cx->realloc_(oldSlots, oldSize, newSize));
        if (oldSlots != newSlots) {
            hugeSlots.remove(oldSlots);
            /* If this put fails, we will only leak the slots. */
            (void)hugeSlots.put(newSlots);
        }
        return newSlots;
    }

    /* The nursery cannot make use of the returned slots data. */
    if (newCount < oldCount)
        return oldSlots;

    HeapSlot *newSlots = allocateSlots(cx, obj, newCount);
    PodCopy(newSlots, oldSlots, oldCount);
    return newSlots;
}

ObjectElements *
js::Nursery::reallocateElements(JSContext *cx, JSObject *obj, ObjectElements *oldHeader,
                                uint32_t oldCount, uint32_t newCount)
{
    HeapSlot *slots = reallocateSlots(cx, obj, reinterpret_cast<HeapSlot *>(oldHeader),
                                      oldCount, newCount);
    return reinterpret_cast<ObjectElements *>(slots);
}

HeapSlot *
js::Nursery::allocateHugeSlots(JSContext *cx, size_t nslots)
{
    HeapSlot *slots = cx->pod_malloc<HeapSlot>(nslots);
    /* If this put fails, we will only leak the slots. */
    (void)hugeSlots.put(slots);
    return slots;
}

void
js::Nursery::notifyInitialSlots(Cell *cell, HeapSlot *slots)
{
    if (isInside(cell) && !isInside(slots)) {
        /* If this put fails, we will only leak the slots. */
        (void)hugeSlots.put(slots);
    }
}

namespace js {
namespace gc {

class MinorCollectionTracer : public JSTracer
{
  public:
    Nursery *nursery;
    AutoTraceSession session;

    /*
     * This list is threaded through the Nursery using the space from already
     * moved things. The list is used to fix up the moved things and to find
     * things held live by intra-Nursery pointers.
     */
    RelocationOverlay *head;
    RelocationOverlay **tail;

    /* Save and restore all of the runtime state we use during MinorGC. */
    bool savedNeedsBarrier;
    AutoDisableProxyCheck disableStrictProxyChecking;

    /* Insert the given relocation entry into the list of things to visit. */
    JS_ALWAYS_INLINE void insertIntoFixupList(RelocationOverlay *entry) {
        *tail = entry;
        tail = &entry->next_;
        *tail = NULL;
    }

    MinorCollectionTracer(JSRuntime *rt, Nursery *nursery)
      : JSTracer(),
        nursery(nursery),
        session(rt, MinorCollecting),
        head(NULL),
        tail(&head),
        savedNeedsBarrier(rt->needsBarrier()),
        disableStrictProxyChecking(rt)
    {
        JS_TracerInit(this, rt, Nursery::MinorGCCallback);
        eagerlyTraceWeakMaps = TraceWeakMapKeysValues;

        rt->gcNumber++;
        rt->setNeedsBarrier(false);
        for (ZonesIter zone(rt); !zone.done(); zone.next())
            zone->saveNeedsBarrier(false);
    }

    ~MinorCollectionTracer() {
        runtime->setNeedsBarrier(savedNeedsBarrier);
        for (ZonesIter zone(runtime); !zone.done(); zone.next())
            zone->restoreNeedsBarrier();
    }
};

} /* namespace gc */
} /* namespace js */

static AllocKind
GetObjectAllocKindForCopy(JSRuntime *rt, JSObject *obj)
{
    if (obj->isArray()) {
        JS_ASSERT(obj->numFixedSlots() == 0);

        /* Use minimal size object if we are just going to copy the pointer. */
        if (!IsInsideNursery(rt, (void *)obj->getElementsHeader()))
            return FINALIZE_OBJECT0_BACKGROUND;

        size_t nelements = obj->getDenseCapacity();
        return GetBackgroundAllocKind(GetGCArrayKind(nelements));
    }

    if (obj->isFunction())
        return obj->toFunction()->getAllocKind();

    AllocKind kind = GetGCObjectFixedSlotsKind(obj->numFixedSlots());
    if (CanBeFinalizedInBackground(kind, obj->getClass()))
        kind = GetBackgroundAllocKind(kind);
    return kind;
}

void *
js::Nursery::allocateFromTenured(Zone *zone, AllocKind thingKind)
{
    void *t = zone->allocator.arenas.allocateFromFreeList(thingKind, Arena::thingSize(thingKind));
    if (!t) {
        zone->allocator.arenas.checkEmptyFreeList(thingKind);
        t = zone->allocator.arenas.allocateFromArena(zone, thingKind);
    }

    /*
     * Pre barriers are disabled during minor collection, however, we still
     * want objects to be allocated black if an incremental GC is in progress.
     */
    if (zone->savedNeedsBarrier())
        static_cast<Cell *>(t)->markIfUnmarked();

    return t;
}

void *
js::Nursery::moveToTenured(MinorCollectionTracer *trc, JSObject *src)
{
    Zone *zone = src->zone();
    AllocKind dstKind = GetObjectAllocKindForCopy(trc->runtime, src);
    JSObject *dst = static_cast<JSObject *>(allocateFromTenured(zone, dstKind));
    if (!dst)
        MOZ_CRASH();

    moveObjectToTenured(dst, src, dstKind);

    RelocationOverlay *overlay = reinterpret_cast<RelocationOverlay *>(src);
    overlay->forwardTo(dst);
    trc->insertIntoFixupList(overlay);

    return static_cast<void *>(dst);
}

void
js::Nursery::moveObjectToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    size_t srcSize = Arena::thingSize(dstKind);

    /*
     * Arrays do not necessarily have the same AllocKind between src and dst.
     * We deal with this by copying elements manually, possibly re-inlining
     * them if there is adequate room inline in dst.
     */
    if (src->isArray())
        srcSize = sizeof(ObjectImpl);

    js_memcpy(dst, src, srcSize);
    moveSlotsToTenured(dst, src, dstKind);
    moveElementsToTenured(dst, src, dstKind);

    /* The shape's list head may point into the old object. */
    if (&src->shape_ == dst->shape_->listp)
        dst->shape_->listp = &dst->shape_;
}

void
js::Nursery::moveSlotsToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    /* Fixed slots have already been copied over. */
    if (!src->hasDynamicSlots())
        return;

    if (!isInside(src->slots)) {
        hugeSlots.remove(src->slots);
        return;
    }

    Allocator *alloc = &src->zone()->allocator;
    size_t count = src->numDynamicSlots();
    dst->slots = alloc->pod_malloc<HeapSlot>(count);
    PodCopy(dst->slots, src->slots, count);
}

void
js::Nursery::moveElementsToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    if (src->hasEmptyElements())
        return;

    Allocator *alloc = &src->zone()->allocator;
    ObjectElements *srcHeader = src->getElementsHeader();
    ObjectElements *dstHeader;

    /* TODO Bug 874151: Prefer to put element data inline if we have space. */
    if (!isInside(srcHeader)) {
        JS_ASSERT(src->elements == dst->elements);
        hugeSlots.remove(reinterpret_cast<HeapSlot*>(srcHeader));
        return;
    }

    /* ArrayBuffer stores byte-length, not Value count. */
    if (src->isArrayBuffer()) {
        size_t nbytes = sizeof(ObjectElements) + srcHeader->initializedLength;
        if (src->hasDynamicElements()) {
            dstHeader = static_cast<ObjectElements *>(alloc->malloc_(nbytes));
            if (!dstHeader)
                MOZ_CRASH();
        } else {
            dst->setFixedElements();
            dstHeader = dst->getElementsHeader();
        }
        js_memcpy(dstHeader, srcHeader, nbytes);
        dst->elements = dstHeader->elements();
        return;
    }

    size_t nslots = ObjectElements::VALUES_PER_HEADER + srcHeader->capacity;

    /* Unlike other objects, Arrays can have fixed elements. */
    if (src->isArray() && nslots <= GetGCKindSlots(dstKind)) {
        dst->setFixedElements();
        dstHeader = dst->getElementsHeader();
        js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
        return;
    }

    size_t nbytes = nslots * sizeof(HeapValue);
    dstHeader = static_cast<ObjectElements *>(alloc->malloc_(nbytes));
    if (!dstHeader)
        MOZ_CRASH();
    js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
    dst->elements = dstHeader->elements();
}

static bool
ShouldMoveToTenured(MinorCollectionTracer *trc, void **thingp)
{
    Cell *cell = static_cast<Cell *>(*thingp);
    Nursery &nursery = *trc->nursery;
    return !nursery.isInside(thingp) && nursery.isInside(cell) &&
           !nursery.getForwardedPointer(thingp);
}

/* static */ void
js::Nursery::MinorGCCallback(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    MinorCollectionTracer *trc = static_cast<MinorCollectionTracer *>(jstrc);
    if (ShouldMoveToTenured(trc, thingp))
        *thingp = trc->nursery->moveToTenured(trc, static_cast<JSObject *>(*thingp));
}

void
js::Nursery::markFallback(Cell *cell)
{
    JS_ASSERT(uintptr_t(cell) >= start());
    size_t offset = uintptr_t(cell) - start();
    JS_ASSERT(offset < end() - start());
    JS_ASSERT(offset % ThingAlignment == 0);
    fallbackBitmap.set(offset / ThingAlignment);
}

void
js::Nursery::moveFallbackToTenured(gc::MinorCollectionTracer *trc)
{
    for (size_t i = 0; i < FallbackBitmapBits; ++i) {
        if (fallbackBitmap.get(i)) {
            JSObject *src = reinterpret_cast<JSObject *>(start() + i * ThingAlignment);
            moveToTenured(trc, src);
        }
    }
    fallbackBitmap.clear(false);
}

/* static */ void
js::Nursery::MinorFallbackMarkingCallback(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    MinorCollectionTracer *trc = static_cast<MinorCollectionTracer *>(jstrc);
    if (ShouldMoveToTenured(trc, thingp))
        trc->nursery->markFallback(static_cast<JSObject *>(*thingp));
}

/* static */ void
js::Nursery::MinorFallbackFixupCallback(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    MinorCollectionTracer *trc = static_cast<MinorCollectionTracer *>(jstrc);
    if (trc->nursery->isInside(*thingp))
        trc->nursery->getForwardedPointer(thingp);
}

static void
TraceHeapWithCallback(JSTracer *trc, JSTraceCallback callback)
{
    JSTraceCallback prior = trc->callback;

    AutoCopyFreeListToArenas copy(trc->runtime);
    trc->callback = callback;
    for (ZonesIter zone(trc->runtime); !zone.done(); zone.next()) {
        for (size_t i = 0; i < FINALIZE_LIMIT; ++i) {
            AllocKind kind = AllocKind(i);
            for (CellIterUnderGC cells(zone, kind); !cells.done(); cells.next())
                JS_TraceChildren(trc, cells.getCell(), MapAllocToTraceKind(kind));
        }
    }

    trc->callback = prior;
}

void
js::Nursery::markStoreBuffer(MinorCollectionTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    if (!rt->gcStoreBuffer.hasOverflowed()) {
        rt->gcStoreBuffer.mark(trc);
        return;
    }

    /*
     * If the store buffer has overflowed, we need to walk the full heap to
     * discover cross-generation edges. Since we cannot easily walk the heap
     * while simultaneously allocating, we use a three pass algorithm:
     *   1) Walk the major heap and mark live things in the nursery in a
     *      pre-allocated bitmap.
     *   2) Use the bitmap to move all live nursery things to the tenured
     *      heap.
     *   3) Walk the heap a second time to find and update all of the moved
     *      references in the tenured heap.
     */
    TraceHeapWithCallback(trc, MinorFallbackMarkingCallback);
    moveFallbackToTenured(trc);
    TraceHeapWithCallback(trc, MinorFallbackFixupCallback);
}

void
js::Nursery::collect(JSRuntime *rt, JS::gcreason::Reason reason)
{
    JS_AbortIfWrongThread(rt);

    if (!isEnabled())
        return;

    if (position() == start())
        return;

    rt->gcHelperThread.waitBackgroundSweepEnd();

    /* Move objects pointed to by roots from the nursery to the major heap. */
    MinorCollectionTracer trc(rt, this);
    MarkRuntime(&trc);
    Debugger::markAll(&trc);
    for (CompartmentsIter comp(rt); !comp.done(); comp.next()) {
        comp->markAllCrossCompartmentWrappers(&trc);
        comp->markAllInitialShapeTableEntries(&trc);
    }
    markStoreBuffer(&trc);
    rt->newObjectCache.clearNurseryObjects(rt);

    /*
     * Most of the work is done here. This loop iterates over objects that have
     * been moved to the major heap. If these objects have any outgoing pointers
     * to the nursery, then those nursery objects get moved as well, until no
     * objects are left to move. That is, we iterate to a fixed point.
     */
    for (RelocationOverlay *p = trc.head; p; p = p->next()) {
        JSObject *obj = static_cast<JSObject*>(p->forwardingAddress());
        JS_TraceChildren(&trc, obj, MapAllocToTraceKind(obj->tenuredGetAllocKind()));
    }

    /* Sweep. */
    sweep(rt->defaultFreeOp());
    rt->gcStoreBuffer.clear();

    /*
     * We ignore gcMaxBytes when allocating for minor collection. However, if we
     * overflowed, we disable the nursery. The next time we allocate, we'll fail
     * because gcBytes >= gcMaxBytes.
     */
    if (rt->gcBytes >= rt->gcMaxBytes)
        disable();
}


void
js::Nursery::sweep(FreeOp *fop)
{
    for (HugeSlotsSet::Range r = hugeSlots.all(); !r.empty(); r.popFront())
        fop->free_(r.front());
    hugeSlots.clear();

    JS_POISON((void *)start(), SweptNursery, NurserySize - sizeof(JSRuntime *));

    position_ = start();
}

#endif /* JSGC_GENERATIONAL */
