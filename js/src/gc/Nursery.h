/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_h
#define gc_Nursery_h

#ifdef JSGC_GENERATIONAL

#include "jsgc.h"
#include "jspubtd.h"

#include "ds/BitArray.h"
#include "js/HashTable.h"

namespace js {

class ObjectElements;

namespace gc {
class MinorCollectionTracer;
} /* namespace gc */

namespace jit {
class CodeGenerator;
class MacroAssembler;
class ICStubCompiler;
class BaselineCompiler;
}

class Nursery
{
  public:
    const static int NumNurseryChunks = 16;
    const static int LastNurseryChunk = NumNurseryChunks - 1;
    const static size_t Alignment = gc::ChunkSize;
    const static size_t NurserySize = gc::ChunkSize * NumNurseryChunks;

    explicit Nursery(JSRuntime *rt)
      : runtime_(rt),
        position_(0),
        currentEnd_(0),
        currentChunk_(0),
        numActiveChunks_(0)
    {}
    ~Nursery();

    bool init();

    void enable();
    void disable();
    bool isEnabled() const { return numActiveChunks_ != 0; }

    template <typename T>
    JS_ALWAYS_INLINE bool isInside(const T *p) const {
        return uintptr_t(p) >= start() && uintptr_t(p) < heapEnd();
    }

    /*
     * Allocate and return a pointer to a new GC thing. Returns NULL if the
     * Nursery is full.
     */
    void *allocate(size_t size);

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

    /* Add a slots to our tracking list if it is out-of-line. */
    void notifyInitialSlots(gc::Cell *cell, HeapSlot *slots);

    /* Do a minor collection. */
    void collect(JSRuntime *rt, JS::gcreason::Reason reason);

    /*
     * Check if the thing at |*ref| in the Nursery has been forwarded. If so,
     * sets |*ref| to the new location of the object and returns true. Otherwise
     * returns false and leaves |*ref| unset.
     */
    template <typename T>
    JS_ALWAYS_INLINE bool getForwardedPointer(T **ref);

    /* Forward a slots/elements pointer stored in an Ion frame. */
    void forwardBufferPointer(HeapSlot **pSlotsElems);

  private:
    /*
     * The start and end pointers are stored under the runtime so that we can
     * inline the isInsideNursery check into embedder code. Use the start()
     * and heapEnd() functions to access these values.
     */
    JSRuntime *runtime_;

    /* Pointer to the first unallocated byte in the nursery. */
    uintptr_t position_;

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

    /* The marking bitmap for the fallback marker. */
    const static size_t ThingAlignment = sizeof(Value);
    const static size_t FallbackBitmapBits = NurserySize / ThingAlignment;
    BitArray<FallbackBitmapBits> fallbackBitmap;

#ifdef DEBUG
    /*
     * In DEBUG builds, these bytes indicate the state of an unused segment of
     * nursery-allocated memory.
     */
    const static uint8_t FreshNursery = 0x2a;
    const static uint8_t SweptNursery = 0x2b;
    const static uint8_t AllocatedThing = 0x2c;
#endif

    /* The maximum number of slots allowed to reside inline in the nursery. */
    const static size_t MaxNurserySlots = 100;

    /* The amount of space in the mapped nursery available to allocations. */
    const static size_t NurseryChunkUsableSize = gc::ChunkSize - sizeof(JSRuntime *);

    struct NurseryChunkLayout {
        char data[NurseryChunkUsableSize];
        JSRuntime *runtime;
        uintptr_t start() { return uintptr_t(&data); }
        uintptr_t end() { return uintptr_t(&runtime); }
    };
    NurseryChunkLayout &chunk(int index) const {
        JS_STATIC_ASSERT(sizeof(NurseryChunkLayout) == gc::ChunkSize);
        JS_ASSERT(index < NumNurseryChunks);
        JS_ASSERT(start());
        return reinterpret_cast<NurseryChunkLayout *>(start())[index];
    }

    JS_ALWAYS_INLINE uintptr_t start() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryStart_;
    }

    JS_ALWAYS_INLINE uintptr_t heapEnd() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryEnd_;
    }

    JS_ALWAYS_INLINE void setCurrentChunk(int chunkno) {
        JS_ASSERT(chunkno < NumNurseryChunks);
        JS_ASSERT(chunkno < numActiveChunks_);
        currentChunk_ = chunkno;
        position_ = chunk(chunkno).start();
        currentEnd_ = chunk(chunkno).end();
    }

    JS_ALWAYS_INLINE uintptr_t allocationEnd() const {
        JS_ASSERT(numActiveChunks_ > 0);
        return chunk(numActiveChunks_ - 1).end();
    }

    JS_ALWAYS_INLINE uintptr_t currentEnd() const {
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
    void *allocateFromTenured(Zone *zone, gc::AllocKind thingKind);

    /*
     * Move the object at |src| in the Nursery to an already-allocated cell
     * |dst| in Tenured.
     */
    void collectToFixedPoint(gc::MinorCollectionTracer *trc);
    JS_ALWAYS_INLINE void traceObject(gc::MinorCollectionTracer *trc, JSObject *src);
    JS_ALWAYS_INLINE void markSlots(gc::MinorCollectionTracer *trc, HeapSlot *vp, uint32_t nslots);
    JS_ALWAYS_INLINE void markSlots(gc::MinorCollectionTracer *trc, HeapSlot *vp, HeapSlot *end);
    JS_ALWAYS_INLINE void markSlot(gc::MinorCollectionTracer *trc, HeapSlot *slotp);
    void *moveToTenured(gc::MinorCollectionTracer *trc, JSObject *src);
    size_t moveObjectToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t moveElementsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    size_t moveSlotsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);

    /* Handle relocation of slots/elements pointers stored in Ion frames. */
    void setSlotsForwardingPointer(HeapSlot *oldSlots, HeapSlot *newSlots, uint32_t nslots);
    void setElementsForwardingPointer(ObjectElements *oldHeader, ObjectElements *newHeader,
                                      uint32_t nelems);

    /*
     * Frees all non-live nursery-allocated things at the end of a minor
     * collection. This operation takes time proportional to the number of
     * dead things.
     */
    void sweep(FreeOp *fop);

    /* Change the allocable space provided by the nursery. */
    void growAllocableSpace();
    void shrinkAllocableSpace();

    static void MinorGCCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind);

    friend class gc::MinorCollectionTracer;
    friend class jit::CodeGenerator;
    friend class jit::MacroAssembler;
    friend class jit::ICStubCompiler;
    friend class jit::BaselineCompiler;
};

} /* namespace js */

#endif /* JSGC_GENERATIONAL */
#endif /* gc_Nursery_h */
