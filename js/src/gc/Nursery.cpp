/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "gc/Nursery-inl.h"

#include "mozilla/IntegerPrintfMacros.h"

#include "jscompartment.h"
#include "jsgc.h"
#include "jsinfer.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "gc/GCInternals.h"
#include "gc/Memory.h"
#include "jit/IonFrames.h"
#include "vm/ArrayObject.h"
#include "vm/Debugger.h"
#if defined(DEBUG)
#include "vm/ScopeObject.h"
#endif
#include "vm/TypedArrayObject.h"

#include "jsgcinlines.h"

#include "vm/ObjectImpl-inl.h"

using namespace js;
using namespace gc;

using mozilla::ArrayLength;
using mozilla::PodCopy;
using mozilla::PodZero;

//#define PROFILE_NURSERY

#ifdef PROFILE_NURSERY
/*
 * Print timing information for minor GCs that take longer than this time in microseconds.
 */
static int64_t GCReportThreshold = INT64_MAX;
#endif

bool
js::Nursery::init(uint32_t maxNurseryBytes)
{
    /* maxNurseryBytes parameter is rounded down to a multiple of chunk size. */
    numNurseryChunks_ = maxNurseryBytes >> ChunkShift;

    /* If no chunks are specified then the nursery is permenantly disabled. */
    if (numNurseryChunks_ == 0)
        return true;

    if (!hugeSlots.init())
        return false;

    void *heap = MapAlignedPages(nurserySize(), Alignment);
    if (!heap)
        return false;

    heapStart_ = uintptr_t(heap);
    heapEnd_ = heapStart_ + nurserySize();
    currentStart_ = start();
    numActiveChunks_ = 1;
    JS_POISON(heap, JS_FRESH_NURSERY_PATTERN, nurserySize());
    setCurrentChunk(0);
    updateDecommittedRegion();

#ifdef PROFILE_NURSERY
    char *env = getenv("JS_MINORGC_TIME");
    if (env)
        GCReportThreshold = atoi(env);
#endif

    JS_ASSERT(isEnabled());
    return true;
}

js::Nursery::~Nursery()
{
    if (start())
        UnmapPages((void *)start(), nurserySize());
}

void
js::Nursery::updateDecommittedRegion()
{
#ifndef JS_GC_ZEAL
    if (numActiveChunks_ < numNurseryChunks_) {
        // Bug 994054: madvise on MacOS is too slow to make this
        //             optimization worthwhile.
# ifndef XP_MACOSX
        uintptr_t decommitStart = chunk(numActiveChunks_).start();
        uintptr_t decommitSize = heapEnd() - decommitStart;
        JS_ASSERT(decommitStart == AlignBytes(decommitStart, Alignment));
        JS_ASSERT(decommitSize == AlignBytes(decommitStart, Alignment));
        MarkPagesUnused((void *)decommitStart, decommitSize);
# endif
    }
#endif
}

void
js::Nursery::enable()
{
    JS_ASSERT(isEmpty());
    if (isEnabled())
        return;
    numActiveChunks_ = 1;
    setCurrentChunk(0);
    currentStart_ = position();
#ifdef JS_GC_ZEAL
    if (runtime()->gcZeal() == ZealGenerationalGCValue)
        enterZealMode();
#endif
}

void
js::Nursery::disable()
{
    JS_ASSERT(isEmpty());
    if (!isEnabled())
        return;
    numActiveChunks_ = 0;
    currentEnd_ = 0;
    updateDecommittedRegion();
}

bool
js::Nursery::isEmpty() const
{
    JS_ASSERT(runtime_);
    if (!isEnabled())
        return true;
    JS_ASSERT_IF(runtime_->gcZeal() != ZealGenerationalGCValue, currentStart_ == start());
    return position() == currentStart_;
}

#ifdef JS_GC_ZEAL
void
js::Nursery::enterZealMode() {
    if (isEnabled())
        numActiveChunks_ = numNurseryChunks_;
}

void
js::Nursery::leaveZealMode() {
    if (isEnabled()) {
        JS_ASSERT(isEmpty());
        setCurrentChunk(0);
        currentStart_ = start();
    }
}
#endif // JS_GC_ZEAL

JSObject *
js::Nursery::allocateObject(JSContext *cx, size_t size, size_t numDynamic)
{
    /* Ensure there's enough space to replace the contents with a RelocationOverlay. */
    JS_ASSERT(size >= sizeof(RelocationOverlay));

    /* Attempt to allocate slots contiguously after object, if possible. */
    if (numDynamic && numDynamic <= MaxNurserySlots) {
        size_t totalSize = size + sizeof(HeapSlot) * numDynamic;
        JSObject *obj = static_cast<JSObject *>(allocate(totalSize));
        if (obj) {
            obj->setInitialSlots(reinterpret_cast<HeapSlot *>(size_t(obj) + size));
            TraceNurseryAlloc(obj, size);
            return obj;
        }
        /* If we failed to allocate as a block, retry with out-of-line slots. */
    }

    HeapSlot *slots = nullptr;
    if (numDynamic) {
        slots = allocateHugeSlots(cx->zone(), numDynamic);
        if (MOZ_UNLIKELY(!slots))
            return nullptr;
    }

    JSObject *obj = static_cast<JSObject *>(allocate(size));

    if (obj)
        obj->setInitialSlots(slots);
    else
        freeSlots(slots);

    TraceNurseryAlloc(obj, size);
    return obj;
}

void *
js::Nursery::allocate(size_t size)
{
    JS_ASSERT(isEnabled());
    JS_ASSERT(!runtime()->isHeapBusy());
    JS_ASSERT(position() >= currentStart_);

    if (currentEnd() < position() + size) {
        if (currentChunk_ + 1 == numActiveChunks_)
            return nullptr;
        setCurrentChunk(currentChunk_ + 1);
    }

    void *thing = (void *)position();
    position_ = position() + size;

    JS_EXTRA_POISON(thing, JS_ALLOCATED_NURSERY_PATTERN, size);
    return thing;
}

/* Internally, this function is used to allocate elements as well as slots. */
HeapSlot *
js::Nursery::allocateSlots(JSObject *obj, uint32_t nslots)
{
    JS_ASSERT(obj);
    JS_ASSERT(nslots > 0);

    if (!IsInsideNursery(obj))
        return obj->zone()->pod_malloc<HeapSlot>(nslots);

    if (nslots > MaxNurserySlots)
        return allocateHugeSlots(obj->zone(), nslots);

    size_t size = sizeof(HeapSlot) * nslots;
    HeapSlot *slots = static_cast<HeapSlot *>(allocate(size));
    if (slots)
        return slots;

    return allocateHugeSlots(obj->zone(), nslots);
}

ObjectElements *
js::Nursery::allocateElements(JSObject *obj, uint32_t nelems)
{
    JS_ASSERT(nelems >= ObjectElements::VALUES_PER_HEADER);
    return reinterpret_cast<ObjectElements *>(allocateSlots(obj, nelems));
}

HeapSlot *
js::Nursery::reallocateSlots(JSObject *obj, HeapSlot *oldSlots,
                             uint32_t oldCount, uint32_t newCount)
{
    if (!IsInsideNursery(obj))
        return obj->zone()->pod_realloc<HeapSlot>(oldSlots, oldCount, newCount);

    if (!isInside(oldSlots)) {
        HeapSlot *newSlots = obj->zone()->pod_realloc<HeapSlot>(oldSlots, oldCount, newCount);
        if (newSlots && oldSlots != newSlots) {
            hugeSlots.remove(oldSlots);
            /* If this put fails, we will only leak the slots. */
            (void)hugeSlots.put(newSlots);
        }
        return newSlots;
    }

    /* The nursery cannot make use of the returned slots data. */
    if (newCount < oldCount)
        return oldSlots;

    HeapSlot *newSlots = allocateSlots(obj, newCount);
    if (newSlots)
        PodCopy(newSlots, oldSlots, oldCount);
    return newSlots;
}

ObjectElements *
js::Nursery::reallocateElements(JSObject *obj, ObjectElements *oldHeader,
                                uint32_t oldCount, uint32_t newCount)
{
    HeapSlot *slots = reallocateSlots(obj, reinterpret_cast<HeapSlot *>(oldHeader),
                                      oldCount, newCount);
    return reinterpret_cast<ObjectElements *>(slots);
}

void
js::Nursery::freeSlots(HeapSlot *slots)
{
    if (!isInside(slots)) {
        hugeSlots.remove(slots);
        js_free(slots);
    }
}

HeapSlot *
js::Nursery::allocateHugeSlots(JS::Zone *zone, size_t nslots)
{
    HeapSlot *slots = zone->pod_malloc<HeapSlot>(nslots);
    /* If this put fails, we will only leak the slots. */
    if (slots)
        (void)hugeSlots.put(slots);
    return slots;
}

namespace js {
namespace gc {

class MinorCollectionTracer : public JSTracer
{
  public:
    Nursery *nursery;
    AutoTraceSession session;

    /* Amount of data moved to the tenured generation during collection. */
    size_t tenuredSize;

    /*
     * This list is threaded through the Nursery using the space from already
     * moved things. The list is used to fix up the moved things and to find
     * things held live by intra-Nursery pointers.
     */
    RelocationOverlay *head;
    RelocationOverlay **tail;

    /* Save and restore all of the runtime state we use during MinorGC. */
    bool savedRuntimeNeedBarrier;
    AutoDisableProxyCheck disableStrictProxyChecking;
    AutoEnterOOMUnsafeRegion oomUnsafeRegion;
    ArrayBufferVector liveArrayBuffers;

    /* Insert the given relocation entry into the list of things to visit. */
    MOZ_ALWAYS_INLINE void insertIntoFixupList(RelocationOverlay *entry) {
        *tail = entry;
        tail = &entry->next_;
        *tail = nullptr;
    }

    MinorCollectionTracer(JSRuntime *rt, Nursery *nursery)
      : JSTracer(rt, Nursery::MinorGCCallback, TraceWeakMapKeysValues),
        nursery(nursery),
        session(rt, MinorCollecting),
        tenuredSize(0),
        head(nullptr),
        tail(&head),
        savedRuntimeNeedBarrier(rt->needsIncrementalBarrier()),
        disableStrictProxyChecking(rt)
    {
        rt->gc.incGcNumber();

        /*
         * We disable the runtime needsIncrementalBarrier() check so that
         * pre-barriers do not fire on objects that have been relocated. The
         * pre-barrier's call to obj->zone() will try to look through shape_,
         * which is now the relocation magic and will crash. However,
         * zone->needsIncrementalBarrier() must still be set correctly so that
         * allocations we make in minor GCs between incremental slices will
         * allocate their objects marked.
         */
        rt->setNeedsIncrementalBarrier(false);

        /*
         * We use the live array buffer lists to track traced buffers so we can
         * sweep their dead views. Incremental collection also use these lists,
         * so we may need to save and restore their contents here.
         */
        if (rt->gc.state() != NO_INCREMENTAL) {
            for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
                if (!ArrayBufferObject::saveArrayBufferList(c, liveArrayBuffers))
                    CrashAtUnhandlableOOM("OOM while saving live array buffers");
                ArrayBufferObject::resetArrayBufferList(c);
            }
        }
    }

    ~MinorCollectionTracer() {
        runtime()->setNeedsIncrementalBarrier(savedRuntimeNeedBarrier);
        if (runtime()->gc.state() != NO_INCREMENTAL)
            ArrayBufferObject::restoreArrayBufferLists(liveArrayBuffers);
    }
};

} /* namespace gc */
} /* namespace js */

static AllocKind
GetObjectAllocKindForCopy(JSRuntime *rt, JSObject *obj)
{
    if (obj->is<ArrayObject>()) {
        JS_ASSERT(obj->numFixedSlots() == 0);

        /* Use minimal size object if we are just going to copy the pointer. */
        if (!rt->gc.nursery.isInside(obj->getElementsHeader()))
            return FINALIZE_OBJECT0_BACKGROUND;

        size_t nelements = obj->getDenseCapacity();
        return GetBackgroundAllocKind(GetGCArrayKind(nelements));
    }

    if (obj->is<JSFunction>())
        return obj->as<JSFunction>().getAllocKind();

    /*
     * Typed arrays in the nursery may have a lazily allocated buffer, make
     * sure there is room for the array's fixed data when moving the array.
     */
    if (obj->is<TypedArrayObject>() && !obj->as<TypedArrayObject>().buffer()) {
        size_t nbytes = obj->as<TypedArrayObject>().byteLength();
        return GetBackgroundAllocKind(TypedArrayObject::AllocKindForLazyBuffer(nbytes));
    }

    AllocKind kind = GetGCObjectFixedSlotsKind(obj->numFixedSlots());
    JS_ASSERT(!IsBackgroundFinalized(kind));
    JS_ASSERT(CanBeFinalizedInBackground(kind, obj->getClass()));
    return GetBackgroundAllocKind(kind);
}

void *
js::Nursery::allocateFromTenured(Zone *zone, AllocKind thingKind)
{
    void *t = zone->allocator.arenas.allocateFromFreeList(thingKind, Arena::thingSize(thingKind));
    if (t)
        return t;
    zone->allocator.arenas.checkEmptyFreeList(thingKind);
    return zone->allocator.arenas.allocateFromArena(zone, thingKind);
}

void
js::Nursery::setSlotsForwardingPointer(HeapSlot *oldSlots, HeapSlot *newSlots, uint32_t nslots)
{
    JS_ASSERT(nslots > 0);
    JS_ASSERT(isInside(oldSlots));
    JS_ASSERT(!isInside(newSlots));
    *reinterpret_cast<HeapSlot **>(oldSlots) = newSlots;
}

void
js::Nursery::setElementsForwardingPointer(ObjectElements *oldHeader, ObjectElements *newHeader,
                                          uint32_t nelems)
{
    /*
     * If the JIT has hoisted a zero length pointer, then we do not need to
     * relocate it because reads and writes to/from this pointer are invalid.
     */
    if (nelems - ObjectElements::VALUES_PER_HEADER < 1)
        return;
    JS_ASSERT(isInside(oldHeader));
    JS_ASSERT(!isInside(newHeader));
    *reinterpret_cast<HeapSlot **>(oldHeader->elements()) = newHeader->elements();
}

#ifdef DEBUG
static bool IsWriteableAddress(void *ptr)
{
    volatile uint64_t *vPtr = reinterpret_cast<volatile uint64_t *>(ptr);
    *vPtr = *vPtr;
    return true;
}
#endif

void
js::Nursery::forwardBufferPointer(HeapSlot **pSlotsElems)
{
    HeapSlot *old = *pSlotsElems;

    if (!isInside(old))
        return;

    /*
     * If the elements buffer is zero length, the "first" item could be inside
     * of the next object or past the end of the allocable area.  However,
     * since we always store the runtime as the last word in the nursery,
     * isInside will still be true, even if this zero-size allocation abuts the
     * end of the allocable area. Thus, it is always safe to read the first
     * word of |old| here.
     */
    *pSlotsElems = *reinterpret_cast<HeapSlot **>(old);
    JS_ASSERT(!isInside(*pSlotsElems));
    JS_ASSERT(IsWriteableAddress(*pSlotsElems));
}

// Structure for counting how many times objects of a particular type have been
// tenured during a minor collection.
struct TenureCount
{
    types::TypeObject *type;
    int count;
};

// Keep rough track of how many times we tenure objects of particular types
// during minor collections, using a fixed size hash for efficiency at the cost
// of potential collisions.
struct Nursery::TenureCountCache
{
    TenureCount entries[16];

    TenureCountCache() { PodZero(this); }

    TenureCount &findEntry(types::TypeObject *type) {
        return entries[PointerHasher<types::TypeObject *, 3>::hash(type) % ArrayLength(entries)];
    }
};

void
js::Nursery::collectToFixedPoint(MinorCollectionTracer *trc, TenureCountCache &tenureCounts)
{
    for (RelocationOverlay *p = trc->head; p; p = p->next()) {
        JSObject *obj = static_cast<JSObject*>(p->forwardingAddress());
        traceObject(trc, obj);

        TenureCount &entry = tenureCounts.findEntry(obj->type());
        if (entry.type == obj->type()) {
            entry.count++;
        } else if (!entry.type) {
            entry.type = obj->type();
            entry.count = 1;
        }
    }
}

MOZ_ALWAYS_INLINE void
js::Nursery::traceObject(MinorCollectionTracer *trc, JSObject *obj)
{
    const Class *clasp = obj->getClass();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (!obj->isNative())
        return;

    if (!obj->hasEmptyElements())
        markSlots(trc, obj->getDenseElements(), obj->getDenseInitializedLength());

    HeapSlot *fixedStart, *fixedEnd, *dynStart, *dynEnd;
    obj->getSlotRange(0, obj->slotSpan(), &fixedStart, &fixedEnd, &dynStart, &dynEnd);
    markSlots(trc, fixedStart, fixedEnd);
    markSlots(trc, dynStart, dynEnd);
}

MOZ_ALWAYS_INLINE void
js::Nursery::markSlots(MinorCollectionTracer *trc, HeapSlot *vp, uint32_t nslots)
{
    markSlots(trc, vp, vp + nslots);
}

MOZ_ALWAYS_INLINE void
js::Nursery::markSlots(MinorCollectionTracer *trc, HeapSlot *vp, HeapSlot *end)
{
    for (; vp != end; ++vp)
        markSlot(trc, vp);
}

MOZ_ALWAYS_INLINE void
js::Nursery::markSlot(MinorCollectionTracer *trc, HeapSlot *slotp)
{
    if (!slotp->isObject())
        return;

    JSObject *obj = &slotp->toObject();
    if (!IsInsideNursery(obj))
        return;

    if (getForwardedPointer(&obj)) {
        slotp->unsafeGet()->setObject(*obj);
        return;
    }

    JSObject *tenured = static_cast<JSObject*>(moveToTenured(trc, obj));
    slotp->unsafeGet()->setObject(*tenured);
}

void *
js::Nursery::moveToTenured(MinorCollectionTracer *trc, JSObject *src)
{
    Zone *zone = src->zone();
    AllocKind dstKind = GetObjectAllocKindForCopy(trc->runtime(), src);
    JSObject *dst = static_cast<JSObject *>(allocateFromTenured(zone, dstKind));
    if (!dst)
        CrashAtUnhandlableOOM("Failed to allocate object while tenuring.");

    trc->tenuredSize += moveObjectToTenured(dst, src, dstKind);

    RelocationOverlay *overlay = reinterpret_cast<RelocationOverlay *>(src);
    overlay->forwardTo(dst);
    trc->insertIntoFixupList(overlay);

    TracePromoteToTenured(src, dst);
    return static_cast<void *>(dst);
}

size_t
js::Nursery::moveObjectToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    size_t srcSize = Arena::thingSize(dstKind);
    size_t tenuredSize = srcSize;

    /*
     * Arrays do not necessarily have the same AllocKind between src and dst.
     * We deal with this by copying elements manually, possibly re-inlining
     * them if there is adequate room inline in dst.
     *
     * For Arrays we're reducing tenuredSize to the smaller srcSize
     * because moveElementsToTenured() accounts for all Array elements,
     * even if they are inlined.
     */
    if (src->is<ArrayObject>())
        tenuredSize = srcSize = sizeof(ObjectImpl);

    js_memcpy(dst, src, srcSize);
    tenuredSize += moveSlotsToTenured(dst, src, dstKind);
    tenuredSize += moveElementsToTenured(dst, src, dstKind);

    if (src->is<TypedArrayObject>())
        forwardTypedArrayPointers(dst, src);

    /* The shape's list head may point into the old object. */
    if (&src->shape_ == dst->shape_->listp)
        dst->shape_->listp = &dst->shape_;

    return tenuredSize;
}

void
js::Nursery::forwardTypedArrayPointers(JSObject *dst, JSObject *src)
{
    /*
     * Typed array data may be stored inline inside the object's fixed slots. If
     * so, we need update the private pointer and leave a forwarding pointer at
     * the start of the data.
     */
    TypedArrayObject &typedArray = src->as<TypedArrayObject>();
    JS_ASSERT_IF(typedArray.buffer(), !isInside(src->getPrivate()));
    if (typedArray.buffer())
        return;

    void *srcData = src->fixedData(TypedArrayObject::FIXED_DATA_START);
    void *dstData = dst->fixedData(TypedArrayObject::FIXED_DATA_START);
    JS_ASSERT(src->getPrivate() == srcData);
    dst->setPrivate(dstData);

    /*
     * We don't know the number of slots here, but
     * TypedArrayObject::AllocKindForLazyBuffer ensures that it's always at
     * least one.
     */
    size_t nslots = 1;
    setSlotsForwardingPointer(reinterpret_cast<HeapSlot*>(srcData),
                              reinterpret_cast<HeapSlot*>(dstData),
                              nslots);
}

size_t
js::Nursery::moveSlotsToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    /* Fixed slots have already been copied over. */
    if (!src->hasDynamicSlots())
        return 0;

    if (!isInside(src->slots)) {
        hugeSlots.remove(src->slots);
        return 0;
    }

    Zone *zone = src->zone();
    size_t count = src->numDynamicSlots();
    dst->slots = zone->pod_malloc<HeapSlot>(count);
    if (!dst->slots)
        CrashAtUnhandlableOOM("Failed to allocate slots while tenuring.");
    PodCopy(dst->slots, src->slots, count);
    setSlotsForwardingPointer(src->slots, dst->slots, count);
    return count * sizeof(HeapSlot);
}

size_t
js::Nursery::moveElementsToTenured(JSObject *dst, JSObject *src, AllocKind dstKind)
{
    if (src->hasEmptyElements())
        return 0;

    Zone *zone = src->zone();
    ObjectElements *srcHeader = src->getElementsHeader();
    ObjectElements *dstHeader;

    /* TODO Bug 874151: Prefer to put element data inline if we have space. */
    if (!isInside(srcHeader)) {
        JS_ASSERT(src->elements == dst->elements);
        hugeSlots.remove(reinterpret_cast<HeapSlot*>(srcHeader));
        return 0;
    }

    size_t nslots = ObjectElements::VALUES_PER_HEADER + srcHeader->capacity;

    /* Unlike other objects, Arrays can have fixed elements. */
    if (src->is<ArrayObject>() && nslots <= GetGCKindSlots(dstKind)) {
        dst->setFixedElements();
        dstHeader = dst->getElementsHeader();
        js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
        setElementsForwardingPointer(srcHeader, dstHeader, nslots);
        return nslots * sizeof(HeapSlot);
    }

    JS_ASSERT(nslots >= 2);
    size_t nbytes = nslots * sizeof(HeapValue);
    dstHeader = static_cast<ObjectElements *>(zone->malloc_(nbytes));
    if (!dstHeader)
        CrashAtUnhandlableOOM("Failed to allocate elements while tenuring.");
    js_memcpy(dstHeader, srcHeader, nslots * sizeof(HeapSlot));
    setElementsForwardingPointer(srcHeader, dstHeader, nslots);
    dst->elements = dstHeader->elements();
    return nslots * sizeof(HeapSlot);
}

static bool
ShouldMoveToTenured(MinorCollectionTracer *trc, void **thingp)
{
    Cell *cell = static_cast<Cell *>(*thingp);
    Nursery &nursery = *trc->nursery;
    return !nursery.isInside(thingp) && IsInsideNursery(cell) &&
           !nursery.getForwardedPointer(thingp);
}

/* static */ void
js::Nursery::MinorGCCallback(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    MinorCollectionTracer *trc = static_cast<MinorCollectionTracer *>(jstrc);
    if (ShouldMoveToTenured(trc, thingp))
        *thingp = trc->nursery->moveToTenured(trc, static_cast<JSObject *>(*thingp));
}

static void
CheckHashTablesAfterMovingGC(JSRuntime *rt)
{
#ifdef JS_GC_ZEAL
    if (rt->gcZeal() == ZealCheckHashTablesOnMinorGC) {
        /* Check that internal hash tables no longer have any pointers into the nursery. */
        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
            c->checkNewTypeObjectTableAfterMovingGC();
            c->checkInitialShapesTableAfterMovingGC();
            c->checkWrapperMapAfterMovingGC();
            if (c->debugScopes)
                c->debugScopes->checkHashTablesAfterMovingGC(rt);
        }
    }
#endif
}

#ifdef PROFILE_NURSERY
#define TIME_START(name) int64_t timstampStart_##name = PRMJ_Now()
#define TIME_END(name) int64_t timstampEnd_##name = PRMJ_Now()
#define TIME_TOTAL(name) (timstampEnd_##name - timstampStart_##name)
#else
#define TIME_START(name)
#define TIME_END(name)
#define TIME_TOTAL(name)
#endif

void
js::Nursery::collect(JSRuntime *rt, JS::gcreason::Reason reason, TypeObjectList *pretenureTypes)
{
    if (rt->mainThread.suppressGC)
        return;

    JS_AbortIfWrongThread(rt);

    StoreBuffer &sb = rt->gc.storeBuffer;
    if (!isEnabled() || isEmpty()) {
        /*
         * Our barriers are not always exact, and there may be entries in the
         * storebuffer even when the nursery is disabled or empty. It's not
         * safe to keep these entries as they may refer to tenured cells which
         * may be freed after this point.
         */
        sb.clear();
        return;
    }

    rt->gc.stats.count(gcstats::STAT_MINOR_GC);

    TraceMinorGCStart();

    TIME_START(total);

    AutoStopVerifyingBarriers av(rt, false);

    // Move objects pointed to by roots from the nursery to the major heap.
    MinorCollectionTracer trc(rt, this);

    // Mark the store buffer. This must happen first.
    TIME_START(markValues);
    sb.markValues(&trc);
    TIME_END(markValues);

    TIME_START(markCells);
    sb.markCells(&trc);
    TIME_END(markCells);

    TIME_START(markSlots);
    sb.markSlots(&trc);
    TIME_END(markSlots);

    TIME_START(markWholeCells);
    sb.markWholeCells(&trc);
    TIME_END(markWholeCells);

    TIME_START(markRelocatableValues);
    sb.markRelocatableValues(&trc);
    TIME_END(markRelocatableValues);

    TIME_START(markRelocatableCells);
    sb.markRelocatableCells(&trc);
    TIME_END(markRelocatableCells);

    TIME_START(markGenericEntries);
    sb.markGenericEntries(&trc);
    TIME_END(markGenericEntries);

    TIME_START(checkHashTables);
    CheckHashTablesAfterMovingGC(rt);
    TIME_END(checkHashTables);

    TIME_START(markRuntime);
    rt->gc.markRuntime(&trc);
    TIME_END(markRuntime);

    TIME_START(markDebugger);
    Debugger::markAll(&trc);
    TIME_END(markDebugger);

    TIME_START(clearNewObjectCache);
    rt->newObjectCache.clearNurseryObjects(rt);
    TIME_END(clearNewObjectCache);

    TIME_START(clearRegExpTestCache);
    rt->regExpTestCache.purge();
    TIME_END(clearRegExpTestCache);

    // Most of the work is done here. This loop iterates over objects that have
    // been moved to the major heap. If these objects have any outgoing pointers
    // to the nursery, then those nursery objects get moved as well, until no
    // objects are left to move. That is, we iterate to a fixed point.
    TIME_START(collectToFP);
    TenureCountCache tenureCounts;
    collectToFixedPoint(&trc, tenureCounts);
    TIME_END(collectToFP);

    // Update the array buffer object's view lists.
    TIME_START(sweepArrayBufferViewList);
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        if (!c->gcLiveArrayBuffers.empty())
            ArrayBufferObject::sweep(c);
    }
    TIME_END(sweepArrayBufferViewList);

    // Update any slot or element pointers whose destination has been tenured.
    TIME_START(updateJitActivations);
    js::jit::UpdateJitActivationsForMinorGC<Nursery>(&rt->mainThread, &trc);
    TIME_END(updateJitActivations);

    // Resize the nursery.
    TIME_START(resize);
    double promotionRate = trc.tenuredSize / double(allocationEnd() - start());
    if (promotionRate > 0.05)
        growAllocableSpace();
    else if (promotionRate < 0.01)
        shrinkAllocableSpace();
    TIME_END(resize);

    // If we are promoting the nursery, or exhausted the store buffer with
    // pointers to nursery things, which will force a collection well before
    // the nursery is full, look for object types that are getting promoted
    // excessively and try to pretenure them.
    TIME_START(pretenure);
    if (pretenureTypes && (promotionRate > 0.8 || reason == JS::gcreason::FULL_STORE_BUFFER)) {
        for (size_t i = 0; i < ArrayLength(tenureCounts.entries); i++) {
            const TenureCount &entry = tenureCounts.entries[i];
            if (entry.count >= 3000)
                pretenureTypes->append(entry.type); // ignore alloc failure
        }
    }
    TIME_END(pretenure);

    // Sweep.
    TIME_START(freeHugeSlots);
    freeHugeSlots();
    TIME_END(freeHugeSlots);

    TIME_START(sweep);
    sweep();
    TIME_END(sweep);

    TIME_START(clearStoreBuffer);
    rt->gc.storeBuffer.clear();
    TIME_END(clearStoreBuffer);

    // We ignore gcMaxBytes when allocating for minor collection. However, if we
    // overflowed, we disable the nursery. The next time we allocate, we'll fail
    // because gcBytes >= gcMaxBytes.
    if (rt->gc.usage.gcBytes() >= rt->gc.tunables.gcMaxBytes())
        disable();

    TIME_END(total);

    TraceMinorGCEnd();

#ifdef PROFILE_NURSERY
    int64_t totalTime = TIME_TOTAL(total);

    if (totalTime >= GCReportThreshold) {
        static bool printedHeader = false;
        if (!printedHeader) {
            fprintf(stderr,
                    "MinorGC: Reason               PRate  Size Time   mkVals mkClls mkSlts mkWCll mkRVal mkRCll mkGnrc ckTbls mkRntm mkDbgr clrNOC collct swpABO updtIn resize pretnr frSlts clrSB  sweep\n");
            printedHeader = true;
        }

#define FMT " %6" PRIu64
        fprintf(stderr,
                "MinorGC: %20s %5.1f%% %4d" FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT FMT "\n",
                js::gcstats::ExplainReason(reason),
                promotionRate * 100,
                numActiveChunks_,
                totalTime,
                TIME_TOTAL(markValues),
                TIME_TOTAL(markCells),
                TIME_TOTAL(markSlots),
                TIME_TOTAL(markWholeCells),
                TIME_TOTAL(markRelocatableValues),
                TIME_TOTAL(markRelocatableCells),
                TIME_TOTAL(markGenericEntries),
                TIME_TOTAL(checkHashTables),
                TIME_TOTAL(markRuntime),
                TIME_TOTAL(markDebugger),
                TIME_TOTAL(clearNewObjectCache),
                TIME_TOTAL(collectToFP),
                TIME_TOTAL(sweepArrayBufferViewList),
                TIME_TOTAL(updateJitActivations),
                TIME_TOTAL(resize),
                TIME_TOTAL(pretenure),
                TIME_TOTAL(freeHugeSlots),
                TIME_TOTAL(clearStoreBuffer),
                TIME_TOTAL(sweep));
#undef FMT
    }
#endif
}

#undef TIME_START
#undef TIME_END
#undef TIME_TOTAL

void
js::Nursery::freeHugeSlots()
{
    FreeOp *fop = runtime()->defaultFreeOp();
    for (HugeSlotsSet::Range r = hugeSlots.all(); !r.empty(); r.popFront())
        fop->free_(r.front());
    hugeSlots.clear();
}

void
js::Nursery::sweep()
{
#ifdef JS_GC_ZEAL
    /* Poison the nursery contents so touching a freed object will crash. */
    JS_POISON((void *)start(), JS_SWEPT_NURSERY_PATTERN, nurserySize());
    for (int i = 0; i < numNurseryChunks_; ++i)
        initChunk(i);

    if (runtime()->gcZeal() == ZealGenerationalGCValue) {
        MOZ_ASSERT(numActiveChunks_ == numNurseryChunks_);

        /* Only reset the alloc point when we are close to the end. */
        if (currentChunk_ + 1 == numNurseryChunks_)
            setCurrentChunk(0);
    } else
#endif
    {
#ifdef JS_CRASH_DIAGNOSTICS
        JS_POISON((void *)start(), JS_SWEPT_NURSERY_PATTERN, allocationEnd() - start());
        for (int i = 0; i < numActiveChunks_; ++i)
            chunk(i).trailer.runtime = runtime();
#endif
        setCurrentChunk(0);
    }

    /* Set current start position for isEmpty checks. */
    currentStart_ = position();
}

void
js::Nursery::growAllocableSpace()
{
#ifdef JS_GC_ZEAL
    MOZ_ASSERT_IF(runtime()->gcZeal() == ZealGenerationalGCValue,
                  numActiveChunks_ == numNurseryChunks_);
#endif
    numActiveChunks_ = Min(numActiveChunks_ * 2, numNurseryChunks_);
}

void
js::Nursery::shrinkAllocableSpace()
{
#ifdef JS_GC_ZEAL
    if (runtime()->gcZeal() == ZealGenerationalGCValue)
        return;
#endif
    numActiveChunks_ = Max(numActiveChunks_ - 1, 1);
    updateDecommittedRegion();
}

#endif /* JSGC_GENERATIONAL */
