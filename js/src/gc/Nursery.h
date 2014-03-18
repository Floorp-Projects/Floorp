/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_h
#define gc_Nursery_h

#ifdef JSGC_GENERATIONAL

#include "jsalloc.h"
#include "jspubtd.h"

#include "ds/BitArray.h"
#include "gc/Heap.h"
#include "gc/Memory.h"
#include "js/GCAPI.h"
#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/Value.h"
#include "js/Vector.h"

namespace JS {
struct Zone;
}

namespace js {

class ObjectElements;
class HeapSlot;
void SetGCZeal(JSRuntime *, uint8_t, uint32_t);

namespace gc {
class Cell;
class MinorCollectionTracer;
} /* namespace gc */

namespace types {
struct TypeObject;
}

namespace jit {
class CodeGenerator;
class MacroAssembler;
class ICStubCompiler;
class BaselineCompiler;
}

class Nursery
{
  public:
    static const int NumNurseryChunks = 16;
    static const int LastNurseryChunk = NumNurseryChunks - 1;
    static const size_t Alignment = gc::ChunkSize;
    static const size_t NurserySize = gc::ChunkSize * NumNurseryChunks;

    explicit Nursery(JSRuntime *rt)
      : runtime_(rt),
        position_(0),
        currentStart_(0),
        currentEnd_(0),
        currentChunk_(0),
        numActiveChunks_(0)
    {}
    ~Nursery();

    bool init();

    void enable();
    void disable();
    bool isEnabled() const { return numActiveChunks_ != 0; }

    /* Return true if no allocations have been made since the last collection. */
    bool isEmpty() const;

    template <typename T>
    MOZ_ALWAYS_INLINE bool isInside(const T *p) const {
        return gc::IsInsideNursery((JS::shadow::Runtime *)runtime_, p);
    }

    /*
     * Allocate and return a pointer to a new GC object with its |slots|
     * pointer pre-filled. Returns nullptr if the Nursery is full.
     */
    JSObject *allocateObject(JSContext *cx, size_t size, size_t numDynamic);

    /* Allocate a slots array for the given object. */
    HeapSlot *allocateSlots(JSContext *cx, JSObject *obj, uint32_t nslots);

    /* Allocate an elements vector for the given object. */
    ObjectElements *allocateElements(JSContext *cx, JSObject *obj, uint32_t nelems);

    /* Resize an existing slots array. */
    HeapSlot *reallocateSlots(JSContext *cx, JSObject *obj, HeapSlot *oldSlots,
                              uint32_t oldCount, uint32_t newCount);

    /* Resize an existing elements vector. */
    ObjectElements *reallocateElements(JSContext *cx, JSObject *obj, ObjectElements *oldHeader,
                                       uint32_t oldCount, uint32_t newCount);

    /* Free a slots array. */
    void freeSlots(JSContext *cx, HeapSlot *slots);

    /* Add a slots to our tracking list if it is out-of-line. */
    void notifyInitialSlots(gc::Cell *cell, HeapSlot *slots);

    typedef Vector<types::TypeObject *, 0, SystemAllocPolicy> TypeObjectList;

    /*
     * Do a minor collection, optionally specifying a list to store types which
     * should be pretenured afterwards.
     */
    void collect(JSRuntime *rt, JS::gcreason::Reason reason, TypeObjectList *pretenureTypes);

    /*
     * Check if the thing at |*ref| in the Nursery has been forwarded. If so,
     * sets |*ref| to the new location of the object and returns true. Otherwise
     * returns false and leaves |*ref| unset.
     */
    template <typename T>
    MOZ_ALWAYS_INLINE bool getForwardedPointer(T **ref);

    /* Forward a slots/elements pointer stored in an Ion frame. */
    void forwardBufferPointer(HeapSlot **pSlotsElems);

    size_t sizeOfHeapCommitted() { return numActiveChunks_ * gc::ChunkSize; }
    size_t sizeOfHeapDecommitted() { return (NumNurseryChunks - numActiveChunks_) * gc::ChunkSize; }

  private:
    /*
     * The start and end pointers are stored under the runtime so that we can
     * inline the isInsideNursery check into embedder code. Use the start()
     * and heapEnd() functions to access these values.
     */
    JSRuntime *runtime_;

    /* Pointer to the first unallocated byte in the nursery. */
    uintptr_t position_;

    /* Pointer to the logical start of the Nursery. */
    uintptr_t currentStart_;

    /* Pointer to the last byte of space in the current chunk. */
    uintptr_t currentEnd_;

    /* The index of the chunk that is currently being allocated from. */
    int currentChunk_;

    /* The index after the last chunk that we will allocate from. */
    int numActiveChunks_;

    /*
     * The set of externally malloced slots potentially kept live by objects
     * stored in the nursery. Any external slots that do not belong to a
     * tenured thing at the end of a minor GC must be freed.
     */
    typedef HashSet<HeapSlot *, PointerHasher<HeapSlot *, 3>, SystemAllocPolicy> HugeSlotsSet;
    HugeSlotsSet hugeSlots;

    /* The maximum number of slots allowed to reside inline in the nursery. */
    static const size_t MaxNurserySlots = 128;

    /* The amount of space in the mapped nursery available to allocations. */
    static const size_t NurseryChunkUsableSize = gc::ChunkSize - sizeof(gc::ChunkTrailer);

    struct NurseryChunkLayout {
        char data[NurseryChunkUsableSize];
        gc::ChunkTrailer trailer;
        uintptr_t start() { return uintptr_t(&data); }
        uintptr_t end() { return uintptr_t(&trailer); }
    };
    static_assert(sizeof(NurseryChunkLayout) == gc::ChunkSize,
                  "Nursery chunk size must match gc::Chunk size.");
    NurseryChunkLayout &chunk(int index) const {
        JS_ASSERT(index < NumNurseryChunks);
        JS_ASSERT(start());
        return reinterpret_cast<NurseryChunkLayout *>(start())[index];
    }

    MOZ_ALWAYS_INLINE uintptr_t start() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryStart_;
    }

    MOZ_ALWAYS_INLINE uintptr_t heapEnd() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryEnd_;
    }

    MOZ_ALWAYS_INLINE void setCurrentChunk(int chunkno) {
        JS_ASSERT(chunkno < NumNurseryChunks);
        JS_ASSERT(chunkno < numActiveChunks_);
        currentChunk_ = chunkno;
        position_ = chunk(chunkno).start();
        currentEnd_ = chunk(chunkno).end();
        chunk(chunkno).trailer.runtime = runtime();
    }

    void updateDecommittedRegion() {
#ifndef JS_GC_ZEAL
        if (numActiveChunks_ < NumNurseryChunks) {
            uintptr_t decommitStart = chunk(numActiveChunks_).start();
            JS_ASSERT(decommitStart == AlignBytes(decommitStart, 1 << 20));
            gc::MarkPagesUnused(runtime(), (void *)decommitStart, heapEnd() - decommitStart);
        }
#endif
    }

    MOZ_ALWAYS_INLINE uintptr_t allocationEnd() const {
        JS_ASSERT(numActiveChunks_ > 0);
        return chunk(numActiveChunks_ - 1).end();
    }

    MOZ_ALWAYS_INLINE bool isFullyGrown() const {
        return numActiveChunks_ == NumNurseryChunks;
    }

    MOZ_ALWAYS_INLINE uintptr_t currentEnd() const {
        JS_ASSERT(runtime_);
        JS_ASSERT(currentEnd_ == chunk(currentChunk_).end());
        return currentEnd_;
    }
    void *addressOfCurrentEnd() const {
        JS_ASSERT(runtime_);
        return (void *)&currentEnd_;
    }

    uintptr_t position() const { return position_; }
    void *addressOfPosition() const { return (void*)&position_; }

    JSRuntime *runtime() const { return runtime_; }

    /* Allocates and registers external slots with the nursery. */
    HeapSlot *allocateHugeSlots(JSContext *cx, size_t nslots);

    /* Allocates a new GC thing from the tenured generation during minor GC. */
    void *allocateFromTenured(JS::Zone *zone, gc::AllocKind thingKind);

    struct TenureCountCache;

    /* Common internal allocator function. */
    void *allocate(size_t size);

    /*
     * Move the object at |src| in the Nursery to an already-allocated cell
     * |dst| in Tenured.
     */
    void collectToFixedPoint(gc::MinorCollectionTracer *trc, TenureCountCache &tenureCounts);
    MOZ_ALWAYS_INLINE void traceObject(gc::MinorCollectionTracer *trc, JSObject *src);
    MOZ_ALWAYS_INLINE void markSlots(gc::MinorCollectionTracer *trc, HeapSlot *vp, uint32_t nslots);
    MOZ_ALWAYS_INLINE void markSlots(gc::MinorCollectionTracer *trc, HeapSlot *vp, HeapSlot *end);
    MOZ_ALWAYS_INLINE void markSlot(gc::MinorCollectionTracer *trc, HeapSlot *slotp);
    void *moveToTenured(gc::MinorCollectionTracer *trc, JSObject *src);
    size_t moveObjectToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t moveElementsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t moveSlotsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);

    /* Handle relocation of slots/elements pointers stored in Ion frames. */
    void setSlotsForwardingPointer(HeapSlot *oldSlots, HeapSlot *newSlots, uint32_t nslots);
    void setElementsForwardingPointer(ObjectElements *oldHeader, ObjectElements *newHeader,
                                      uint32_t nelems);

    /* Free malloced pointers owned by freed things in the nursery. */
    void freeHugeSlots(JSRuntime *rt);

    /*
     * Frees all non-live nursery-allocated things at the end of a minor
     * collection.
     */
    void sweep(JSRuntime *rt);

    /* Change the allocable space provided by the nursery. */
    void growAllocableSpace();
    void shrinkAllocableSpace();

    static void MinorGCCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind);

#ifdef JS_GC_ZEAL
    /*
     * In debug and zeal builds, these bytes indicate the state of an unused
     * segment of nursery-allocated memory.
     */
    static const uint8_t FreshNursery = 0x2a;
    static const uint8_t SweptNursery = 0x2b;
    static const uint8_t AllocatedThing = 0x2c;
    void enterZealMode() {
        if (isEnabled())
            numActiveChunks_ = NumNurseryChunks;
    }
    void leaveZealMode() {
        if (isEnabled()) {
            JS_ASSERT(isEmpty());
            setCurrentChunk(0);
            currentStart_ = start();
        }
    }
#else
    void enterZealMode() {}
    void leaveZealMode() {}
#endif

    friend class gc::MinorCollectionTracer;
    friend class jit::CodeGenerator;
    friend class jit::MacroAssembler;
    friend class jit::ICStubCompiler;
    friend class jit::BaselineCompiler;
    friend void SetGCZeal(JSRuntime *, uint8_t, uint32_t);
};

} /* namespace js */

#endif /* JSGC_GENERATIONAL */
#endif /* gc_Nursery_h */
