/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgc_nursery_h___
#define jsgc_nursery_h___

#ifdef JSGC_GENERATIONAL

#include "ds/BitArray.h"
#include "js/HashTable.h"

#include "jsgc.h"
#include "jspubtd.h"

namespace js {

class ObjectElements;

namespace gc {
class MinorCollectionTracer;
} /* namespace gc */

namespace ion {
class CodeGenerator;
class MacroAssembler;
}

class Nursery
{
  public:
    const static size_t Alignment = gc::ChunkSize;
    const static size_t NurserySize = gc::ChunkSize;
    const static size_t NurseryMask = NurserySize - 1;

    explicit Nursery(JSRuntime *rt)
      : runtime_(rt),
        position_(0)
    {}
    ~Nursery();

    bool enable();
    void disable();
    bool isEnabled() const { return bool(start()); }

    template <typename T>
    JS_ALWAYS_INLINE bool isInside(const T *p) const {
        return uintptr_t(p) >= start() && uintptr_t(p) < end();
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

  private:
    /*
     * The start and end pointers are stored under the runtime so that we can
     * inline the isInsideNursery check into embedder code. Use the start()
     * and end() functions to access these values.
     */
    JSRuntime *runtime_;

    /* Pointer to the first unallocated byte in the nursery. */
    uintptr_t position_;

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
    const static size_t NurseryUsableSize = NurserySize - sizeof(JSRuntime *);

    struct Layout {
        char data[NurseryUsableSize];
        JSRuntime *runtime;
    };
    Layout &asLayout() {
        JS_STATIC_ASSERT(sizeof(Layout) == NurserySize);
        JS_ASSERT(start());
        return *reinterpret_cast<Layout *>(start());
    }

    JS_ALWAYS_INLINE uintptr_t start() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryStart_;
    }

    JS_ALWAYS_INLINE uintptr_t end() const {
        JS_ASSERT(runtime_);
        return ((JS::shadow::Runtime *)runtime_)->gcNurseryEnd_;
    }
    void *addressOfCurrentEnd() const {
        JS_ASSERT(runtime_);
        return (void*)&((JS::shadow::Runtime *)runtime_)->gcNurseryEnd_;
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
    void *moveToTenured(gc::MinorCollectionTracer *trc, JSObject *src);
    void moveObjectToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    void moveElementsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);
    void moveSlotsToTenured(JSObject *dst, JSObject *src, gc::AllocKind dstKind);

    /* Handle fallback marking. See the comment in MarkStoreBuffer. */
    void markFallback(gc::Cell *cell);
    void moveFallbackToTenured(gc::MinorCollectionTracer *trc);

    void markStoreBuffer(gc::MinorCollectionTracer *trc);

    /*
     * Frees all non-live nursery-allocated things at the end of a minor
     * collection. This operation takes time proportional to the number of
     * dead things.
     */
    void sweep(FreeOp *fop);

    static void MinorGCCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind);
    static void MinorFallbackMarkingCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind);
    static void MinorFallbackFixupCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind);

    friend class gc::MinorCollectionTracer;
    friend class ion::CodeGenerator;
    friend class ion::MacroAssembler;
};

} /* namespace js */

#endif /* JSGC_GENERATIONAL */
#endif /* jsgc_nursery_h___ */
