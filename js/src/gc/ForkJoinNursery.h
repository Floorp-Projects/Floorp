/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_ForkJoinNursery_h
#define gc_ForkJoinNursery_h

#ifdef JSGC_FJGENERATIONAL

#ifndef JSGC_GENERATIONAL
#error "JSGC_GENERATIONAL is required for the ForkJoinNursery"
#endif

#include "jsalloc.h"
#include "jspubtd.h"

#include "gc/Heap.h"
#include "gc/Memory.h"
#include "gc/Nursery.h"

#include "js/HashTable.h"
#include "js/TracingAPI.h"

namespace js {
class ObjectElements;
class HeapSlot;
class ForkJoinShared;
}

namespace js {
namespace gc {

class ForkJoinGCShared;
class ForkJoinNursery;
class ForkJoinNurseryCollectionTracer;

// This tracer comes into play when a class has a tracer function, but
// is otherwise unused and has no other functionality.
//
// It could look like this could be merged into ForkJoinNursery by
// making the latter derive from JSTracer; I've decided to keep them
// separate for now, since it allows for multiple instantiations of
// this class with different parameters, for different purposes.  That
// may change.

class ForkJoinNurseryCollectionTracer : public JSTracer
{
    friend class ForkJoinNursery;

  public:
    ForkJoinNurseryCollectionTracer(JSRuntime *rt, ForkJoinNursery *nursery);

  private:
    ForkJoinNursery *const nursery_;
};

// The layout for a chunk used by the ForkJoinNursery.

struct ForkJoinNurseryChunk
{
    // The amount of space in the mapped nursery available to allocations
    static const size_t UsableSize = ChunkSize - sizeof(ChunkTrailer);

    char data[UsableSize];
    ChunkTrailer trailer;
    uintptr_t start() { return uintptr_t(&data); }
    uintptr_t end() { return uintptr_t(&trailer); }
};

// A GC adapter to ForkJoinShared, which is a complex class hidden
// inside ForkJoin.cpp.

class ForkJoinGCShared
{
  public:
    ForkJoinGCShared(ForkJoinShared *shared) : shared_(shared) {}

    JSRuntime *runtime();
    JS::Zone *zone();

    // The updatable object (the ForkJoin result array), or nullptr.
    JSObject *updatable();

    // allocateNurseryChunk() returns nullptr on oom.
    ForkJoinNurseryChunk *allocateNurseryChunk();

    // p must have been obtained through allocateNurseryChunk.
    void freeNurseryChunk(ForkJoinNurseryChunk *p);

    // GC statistics output.
    void spewGC(const char *fmt, ...);

  private:
    ForkJoinShared *const shared_;
};

// There is one ForkJoinNursery per ForkJoin worker.
//
// See the comment in ForkJoinNursery.cpp about how it works.

class ForkJoinNursery
{
    friend class ForkJoinNurseryCollectionTracer;
    friend class RelocationOverlay;

    static_assert(sizeof(ForkJoinNurseryChunk) == ChunkSize,
                  "ForkJoinNursery chunk size must match Chunk size.");
  public:
    ForkJoinNursery(ForkJoinContext *cx, ForkJoinGCShared *shared, Allocator *tenured);
    ~ForkJoinNursery();

    // Attempt to allocate initial storage, returns false on failure
    bool initialize();

    // Perform a collection within the nursery, and if that for some reason
    // cannot be done then perform an evacuating collection.
    void minorGC();

    // Evacuate the live data from the nursery into the tenured area;
    // do not recreate the nursery.
    void evacuatingGC();

    // Allocate an object with a number of dynamic slots.  Returns an
    // object, or nullptr in one of two circumstances:
    //
    //  - The nursery was full, the collector must be run, and the
    //    allocation must be retried.  tooLarge is set to 'false'.
    //  - The number of dynamic slots requested is too large and
    //    the object should be allocated in the tenured area.
    //    tooLarge is set to 'true'.
    //
    // This method will never run the garbage collector.
    JSObject *allocateObject(size_t size, size_t numDynamic, bool& tooLarge);

    // Allocate and reallocate slot and element arrays for existing
    // objects.  These will create or maintain the arrays within the
    // nursery if possible and appropriate, and otherwise will fall
    // back to allocating in the tenured area.  They will return
    // nullptr only if memory is exhausted.  If the reallocate methods
    // return nullptr then the old array is still live.
    //
    // These methods will never run the garbage collector.
    HeapSlot *allocateSlots(JSObject *obj, uint32_t nslots);
    HeapSlot *reallocateSlots(JSObject *obj, HeapSlot *oldSlots,
                              uint32_t oldCount, uint32_t newCount);
    ObjectElements *allocateElements(JSObject *obj, uint32_t nelems);
    ObjectElements *reallocateElements(JSObject *obj, ObjectElements *oldHeader,
                                       uint32_t oldCount, uint32_t newCount);

    // Free a slots array.
    void freeSlots(HeapSlot *slots);

    // The method embedded in a ForkJoinNurseryCollectionTracer
    static void MinorGCCallback(JSTracer *trcArg, void **thingp, JSGCTraceKind kind);

    // A method called from the JIT frame updater
    static void forwardBufferPointer(JSTracer *trc, HeapSlot **pSlotsElems);

    // Return true iff obj is inside the current newspace.
    MOZ_ALWAYS_INLINE bool isInsideNewspace(const void *obj);

    // Return true iff collection is ongoing and obj is inside the current fromspace.
    MOZ_ALWAYS_INLINE bool isInsideFromspace(const void *obj);

    template <typename T>
    MOZ_ALWAYS_INLINE bool getForwardedPointer(T **ref);

    static size_t offsetOfPosition() {
        return offsetof(ForkJoinNursery, position_);
    }

    static size_t offsetOfCurrentEnd() {
        return offsetof(ForkJoinNursery, currentEnd_);
    }

  private:
    // The largest slot arrays that will be allocated in the nursery.
    // On the one hand we want this limit to be large, to avoid
    // managing many hugeSlots.  On the other hand, slot arrays have
    // to be copied during GC and will induce some external
    // fragmentation in the nursery at chunk boundaries.
    static const size_t MaxNurserySlots = 2048;

    // The fixed limit on the per-worker nursery, in chunks.
    //
    // For production runs, 16 may be good - programs that need it,
    // really need it, and as allocation is lazy programs that don't
    // need it won't suck up a lot of resources.
    //
    // For debugging runs, 1 or 2 may sometimes be good, because it
    // will more easily provoke bugs in the evacuation paths.
    static const size_t MaxNurseryChunks = 16;

    // The inverse load factor in the per-worker nursery.  Grow the nursery
    // or schedule an evacuation if more than 1/NurseryLoadFactor of the
    // current nursery size is live after minor GC.
    static const int NurseryLoadFactor = 3;

    // Allocate an object in the nursery's newspace.  Return nullptr
    // when allocation fails (ie the object can't fit in the current
    // chunk and the number of chunks it at its maximum).
    void *allocate(size_t size);

    // Allocate an external slot array and register it with this nursery.
    HeapSlot *allocateHugeSlots(size_t nslots);

    // Reallocate an external slot array, unregister the old array and
    // register the new array.  If the allocation fails then leave
    // everything unchanged.
    HeapSlot *reallocateHugeSlots(HeapSlot *oldSlots, uint32_t oldSize, uint32_t newSize);

    // Walk the list of registered slot arrays and free them all.
    void sweepHugeSlots();

    // Set the position/end pointers to correspond to the numbered
    // chunk.  Returns false if the chunk could not be allocated, either
    // because we're OOM or because the nursery capacity is exhausted.
    bool setCurrentChunk(int index);

    enum PJSCollectionOp {
        Evacuate = 1,
        Collect = 2,
        Recreate = 4
    };

    // Misc GC internals.
    void pjsCollection(int op /* A combination of PJSCollectionOp bits */);
    bool initNewspace();
    void flip();
    void forwardFromRoots(ForkJoinNurseryCollectionTracer *trc);
    void forwardFromUpdatable(ForkJoinNurseryCollectionTracer *trc);
    void forwardFromStack(ForkJoinNurseryCollectionTracer *trc);
    void forwardFromTenured(ForkJoinNurseryCollectionTracer *trc);
    void forwardFromRematerializedFrames(ForkJoinNurseryCollectionTracer *trc);
    void collectToFixedPoint(ForkJoinNurseryCollectionTracer *trc);
    void freeFromspace();
    void computeNurserySizeAfterGC(size_t live, const char **msg);

    AllocKind getObjectAllocKind(JSObject *src);
    void *allocateInTospace(AllocKind thingKind);
    void *allocateInTospace(size_t nelem, size_t elemSize);
    void *allocateInTospaceInfallible(size_t thingSize);
    MOZ_ALWAYS_INLINE bool shouldMoveObject(void **thingp);
    void *moveObjectToTospace(JSObject *src);
    size_t copyObjectToTospace(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t copyElementsToTospace(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t copySlotsToTospace(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    MOZ_ALWAYS_INLINE void insertIntoFixupList(RelocationOverlay *entry);

    void setSlotsForwardingPointer(HeapSlot *oldSlots, HeapSlot *newSlots, uint32_t nslots);
    void setElementsForwardingPointer(ObjectElements *oldHeader, ObjectElements *newHeader,
                                      uint32_t nelems);

    MOZ_ALWAYS_INLINE void traceObject(ForkJoinNurseryCollectionTracer *trc, JSObject *obj);
    MOZ_ALWAYS_INLINE void markSlots(HeapSlot *vp, uint32_t nslots);
    MOZ_ALWAYS_INLINE void markSlots(HeapSlot *vp, HeapSlot *end);
    MOZ_ALWAYS_INLINE void markSlot(HeapSlot *slotp);

    ForkJoinContext *const cx_;      // The context that owns this nursery
    Allocator *const tenured_;       // Private tenured area
    ForkJoinGCShared *const shared_; // Common to all nurseries belonging to a ForkJoin instance
    JS::Zone *evacuationZone_;       // During evacuating GC this is non-NULL: the Zone we
                                     // allocate into

    uintptr_t currentStart_;         // Start of current area in newspace
    uintptr_t currentEnd_;           // End of current area in newspace (last byte + 1)
    uintptr_t position_;             // Next free byte in current newspace chunk
    unsigned currentChunk_;          // Index of current / highest numbered chunk in newspace
    unsigned numActiveChunks_;       // Number of active chunks in newspace, not all may be allocated
    unsigned numFromspaceChunks_;    // Number of active chunks in fromspace, all are allocated
    bool mustEvacuate_;              // Set to true after GC when the /next/ minor GC must evacuate

    bool isEvacuating_;              // Set to true when the current minor GC is evacuating
    size_t movedSize_;               // Bytes copied during the current minor GC
    RelocationOverlay *head_;        // First node of relocation list
    RelocationOverlay **tail_;       // Pointer to 'next_' field of last node of relocation list

    typedef HashSet<HeapSlot *, PointerHasher<HeapSlot *, 3>, SystemAllocPolicy> HugeSlotsSet;

    HugeSlotsSet hugeSlots[2];       // Hash sets for huge slots

    int hugeSlotsNew;                // Huge slot arrays in the newspace (index in hugeSlots)
    int hugeSlotsFrom;               // Huge slot arrays in the fromspace (index in hugeSlots)

    ForkJoinNurseryChunk *newspace[MaxNurseryChunks];  // All allocation happens here
    ForkJoinNurseryChunk *fromspace[MaxNurseryChunks]; // Meaningful during GC: the previous newspace
};

} // namespace gc
} // namespace js

#endif // JSGC_FJGENERATIONAL

#endif // gc_ForkJoinNursery_h
