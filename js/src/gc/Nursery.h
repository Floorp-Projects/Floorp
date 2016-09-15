/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Nursery_h
#define gc_Nursery_h

#include "mozilla/EnumeratedArray.h"

#include "jsalloc.h"
#include "jspubtd.h"

#include "ds/BitArray.h"
#include "gc/Heap.h"
#include "gc/Memory.h"
#include "js/Class.h"
#include "js/GCAPI.h"
#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/Value.h"
#include "js/Vector.h"
#include "vm/SharedMem.h"

#define FOR_EACH_NURSERY_PROFILE_TIME(_)                                      \
   /* Key                       Header text */                                \
    _(Total,                    "total")                                      \
    _(CancelIonCompilations,    "canIon")                                     \
    _(TraceValues,              "mkVals")                                     \
    _(TraceCells,               "mkClls")                                     \
    _(TraceSlots,               "mkSlts")                                     \
    _(TraceWholeCells,          "mcWCll")                                     \
    _(TraceGenericEntries,      "mkGnrc")                                     \
    _(CheckHashTables,          "ckTbls")                                     \
    _(MarkRuntime,              "mkRntm")                                     \
    _(MarkDebugger,             "mkDbgr")                                     \
    _(ClearNewObjectCache,      "clrNOC")                                     \
    _(CollectToFP,              "collct")                                     \
    _(ObjectsTenuredCallback,   "tenCB")                                      \
    _(SweepArrayBufferViewList, "swpABO")                                     \
    _(UpdateJitActivations,     "updtIn")                                     \
    _(FreeMallocedBuffers,      "frSlts")                                     \
    _(ClearStoreBuffer,         "clrSB")                                      \
    _(Sweep,                    "sweep")                                      \
    _(Resize,                   "resize")                                     \
    _(Pretenure,                "pretnr")

namespace JS {
struct Zone;
} // namespace JS

namespace js {

class ObjectElements;
class NativeObject;
class Nursery;
class HeapSlot;

void SetGCZeal(JSRuntime*, uint8_t, uint32_t);

namespace gc {
struct Cell;
class MinorCollectionTracer;
class RelocationOverlay;
struct TenureCountCache;
} /* namespace gc */

namespace jit {
class MacroAssembler;
} // namespace jit

class TenuringTracer : public JSTracer
{
    friend class Nursery;
    Nursery& nursery_;

    // Amount of data moved to the tenured generation during collection.
    size_t tenuredSize;

    // This list is threaded through the Nursery using the space from already
    // moved things. The list is used to fix up the moved things and to find
    // things held live by intra-Nursery pointers.
    gc::RelocationOverlay* head;
    gc::RelocationOverlay** tail;

    TenuringTracer(JSRuntime* rt, Nursery* nursery);

  public:
    const Nursery& nursery() const { return nursery_; }

    // Returns true if the pointer was updated.
    template <typename T> void traverse(T** thingp);
    template <typename T> void traverse(T* thingp);

    void insertIntoFixupList(gc::RelocationOverlay* entry);

    // The store buffers need to be able to call these directly.
    void traceObject(JSObject* src);
    void traceObjectSlots(NativeObject* nobj, uint32_t start, uint32_t length);
    void traceSlots(JS::Value* vp, uint32_t nslots) { traceSlots(vp, vp + nslots); }

  private:
    Nursery& nursery() { return nursery_; }

    JSObject* moveToTenured(JSObject* src);
    size_t moveObjectToTenured(JSObject* dst, JSObject* src, gc::AllocKind dstKind);
    size_t moveElementsToTenured(NativeObject* dst, NativeObject* src, gc::AllocKind dstKind);
    size_t moveSlotsToTenured(NativeObject* dst, NativeObject* src, gc::AllocKind dstKind);

    void traceSlots(JS::Value* vp, JS::Value* end);
};

/*
 * Classes with JSCLASS_SKIP_NURSERY_FINALIZE or Wrapper classes with
 * CROSS_COMPARTMENT flags will not have their finalizer called if they are
 * nursery allocated and not promoted to the tenured heap. The finalizers for
 * these classes must do nothing except free data which was allocated via
 * Nursery::allocateBuffer.
 */
inline bool
CanNurseryAllocateFinalizedClass(const js::Class* const clasp)
{
    MOZ_ASSERT(clasp->hasFinalize());
    return clasp->flags & JSCLASS_SKIP_NURSERY_FINALIZE;
}

class Nursery
{
  public:
    static const size_t Alignment = gc::ChunkSize;
    static const size_t ChunkShift = gc::ChunkShift;

    explicit Nursery(JSRuntime* rt);
    ~Nursery();

    MOZ_MUST_USE bool init(uint32_t maxNurseryBytes, AutoLockGC& lock);

    unsigned maxChunks() const { return maxNurseryChunks_; }
    unsigned numChunks() const { return chunks_.length(); }

    bool exists() const { return maxChunks() != 0; }
    size_t nurserySize() const { return maxChunks() << ChunkShift; }

    void enable();
    void disable();
    bool isEnabled() const { return numChunks() != 0; }

    /* Return true if no allocations have been made since the last collection. */
    bool isEmpty() const;

    /*
     * Check whether an arbitrary pointer is within the nursery. This is
     * slower than IsInsideNursery(Cell*), but works on all types of pointers.
     */
    MOZ_ALWAYS_INLINE bool isInside(gc::Cell* cellp) const = delete;
    MOZ_ALWAYS_INLINE bool isInside(const void* p) const {
        for (auto chunk : chunks_) {
            if (uintptr_t(p) - chunk->start() < gc::ChunkSize)
                return true;
        }
        return false;
    }
    template<typename T>
    bool isInside(const SharedMem<T>& p) const {
        return isInside(p.unwrap(/*safe - used for value in comparison above*/));
    }

    /*
     * Allocate and return a pointer to a new GC object with its |slots|
     * pointer pre-filled. Returns nullptr if the Nursery is full.
     */
    JSObject* allocateObject(JSContext* cx, size_t size, size_t numDynamic, const js::Class* clasp);

    /* Allocate a buffer for a given zone, using the nursery if possible. */
    void* allocateBuffer(JS::Zone* zone, uint32_t nbytes);

    /*
     * Allocate a buffer for a given object, using the nursery if possible and
     * obj is in the nursery.
     */
    void* allocateBuffer(JSObject* obj, uint32_t nbytes);

    /* Resize an existing object buffer. */
    void* reallocateBuffer(JSObject* obj, void* oldBuffer,
                           uint32_t oldBytes, uint32_t newBytes);

    /* Free an object buffer. */
    void freeBuffer(void* buffer);

    /* The maximum number of bytes allowed to reside in nursery buffers. */
    static const size_t MaxNurseryBufferSize = 1024;

    /* Do a minor collection. */
    void collect(JSRuntime* rt, JS::gcreason::Reason reason);

    /*
     * Check if the thing at |*ref| in the Nursery has been forwarded. If so,
     * sets |*ref| to the new location of the object and returns true. Otherwise
     * returns false and leaves |*ref| unset.
     */
    MOZ_ALWAYS_INLINE MOZ_MUST_USE bool getForwardedPointer(JSObject** ref) const;

    /* Forward a slots/elements pointer stored in an Ion frame. */
    void forwardBufferPointer(HeapSlot** pSlotsElems);

    void maybeSetForwardingPointer(JSTracer* trc, void* oldData, void* newData, bool direct) {
        if (trc->isTenuringTracer() && isInside(oldData))
            setForwardingPointer(oldData, newData, direct);
    }

    /* Mark a malloced buffer as no longer needing to be freed. */
    void removeMallocedBuffer(void* buffer) {
        mallocedBuffers.remove(buffer);
    }

    void waitBackgroundFreeEnd();

    MOZ_MUST_USE bool addedUniqueIdToCell(gc::Cell* cell) {
        if (!IsInsideNursery(cell) || !isEnabled())
            return true;
        MOZ_ASSERT(cellsWithUid_.initialized());
        MOZ_ASSERT(!cellsWithUid_.has(cell));
        return cellsWithUid_.put(cell);
    }

    using SweepThunk = void (*)(void *data);
    void queueSweepAction(SweepThunk thunk, void* data);

    MOZ_MUST_USE bool queueDictionaryModeObjectToSweep(NativeObject* obj);

    size_t sizeOfHeapCommitted() const {
        return numChunks() * gc::ChunkSize;
    }
    size_t sizeOfMallocedBuffers(mozilla::MallocSizeOf mallocSizeOf) const {
        size_t total = 0;
        for (MallocedBuffersSet::Range r = mallocedBuffers.all(); !r.empty(); r.popFront())
            total += mallocSizeOf(r.front());
        total += mallocedBuffers.sizeOfExcludingThis(mallocSizeOf);
        return total;
    }

    // The number of bytes from the start position to the end of the nursery.
    size_t spaceToEnd() const;

    // Free space remaining, not counting chunk trailers.
    MOZ_ALWAYS_INLINE size_t freeSpace() const {
        MOZ_ASSERT(currentEnd_ - position_ <= NurseryChunkUsableSize);
        return (currentEnd_ - position_) +
               (numChunks() - currentChunk_ - 1) * NurseryChunkUsableSize;
    }

#ifdef JS_GC_ZEAL
    void enterZealMode();
    void leaveZealMode();
#endif

    /* Print total profile times on shutdown. */
    void printTotalProfileTimes();

  private:
    /* The amount of space in the mapped nursery available to allocations. */
    static const size_t NurseryChunkUsableSize = gc::ChunkSize - sizeof(gc::ChunkTrailer);

    struct NurseryChunk {
        char data[NurseryChunkUsableSize];
        gc::ChunkTrailer trailer;
        static NurseryChunk* fromChunk(gc::Chunk* chunk);
        void init(JSRuntime* rt);
        void poisonAndInit(JSRuntime* rt, uint8_t poison);
        uintptr_t start() const { return uintptr_t(&data); }
        uintptr_t end() const { return uintptr_t(&trailer); }
        gc::Chunk* toChunk(JSRuntime* rt);
    };
    static_assert(sizeof(NurseryChunk) == gc::ChunkSize,
                  "Nursery chunk size must match gc::Chunk size.");

    /*
     * The start and end pointers are stored under the runtime so that we can
     * inline the isInsideNursery check into embedder code. Use the start()
     * and heapEnd() functions to access these values.
     */
    JSRuntime* runtime_;

    /* Vector of allocated chunks to allocate from. */
    Vector<NurseryChunk*, 0, SystemAllocPolicy> chunks_;

    /* Pointer to the first unallocated byte in the nursery. */
    uintptr_t position_;

    /* Pointer to the logical start of the Nursery. */
    unsigned currentStartChunk_;
    uintptr_t currentStartPosition_;

    /* Pointer to the last byte of space in the current chunk. */
    uintptr_t currentEnd_;

    /* The index of the chunk that is currently being allocated from. */
    unsigned currentChunk_;

    /* Maximum number of chunks to allocate for the nursery. */
    unsigned maxNurseryChunks_;

    /* Promotion rate for the previous minor collection. */
    double previousPromotionRate_;

    /* Report minor collections taking at least this many us, if enabled. */
    int64_t profileThreshold_;
    bool enableProfiling_;

    /* Report ObjectGroups with at lest this many instances tenured. */
    int64_t reportTenurings_;

    /* Profiling data. */

    enum class ProfileKey
    {
#define DEFINE_TIME_KEY(name, text)                                           \
        name,
        FOR_EACH_NURSERY_PROFILE_TIME(DEFINE_TIME_KEY)
#undef DEFINE_TIME_KEY
        KeyCount
    };

    using ProfileTimes = mozilla::EnumeratedArray<ProfileKey, ProfileKey::KeyCount, int64_t>;

    ProfileTimes startTimes_;
    ProfileTimes profileTimes_;
    ProfileTimes totalTimes_;
    uint64_t minorGcCount_;

    /*
     * The set of externally malloced buffers potentially kept live by objects
     * stored in the nursery. Any external buffers that do not belong to a
     * tenured thing at the end of a minor GC must be freed.
     */
    typedef HashSet<void*, PointerHasher<void*, 3>, SystemAllocPolicy> MallocedBuffersSet;
    MallocedBuffersSet mallocedBuffers;

    /* A task structure used to free the malloced bufers on a background thread. */
    struct FreeMallocedBuffersTask;
    FreeMallocedBuffersTask* freeMallocedBuffersTask;

    /*
     * During a collection most hoisted slot and element buffers indicate their
     * new location with a forwarding pointer at the base. This does not work
     * for buffers whose length is less than pointer width, or when different
     * buffers might overlap each other. For these, an entry in the following
     * table is used.
     */
    typedef HashMap<void*, void*, PointerHasher<void*, 1>, SystemAllocPolicy> ForwardedBufferMap;
    ForwardedBufferMap forwardedBuffers;

    /*
     * When we assign a unique id to cell in the nursery, that almost always
     * means that the cell will be in a hash table, and thus, held live,
     * automatically moving the uid from the nursery to its new home in
     * tenured. It is possible, if rare, for an object that acquired a uid to
     * be dead before the next collection, in which case we need to know to
     * remove it when we sweep.
     *
     * Note: we store the pointers as Cell* here, resulting in an ugly cast in
     *       sweep. This is because this structure is used to help implement
     *       stable object hashing and we have to break the cycle somehow.
     */
    using CellsWithUniqueIdSet = HashSet<gc::Cell*, PointerHasher<gc::Cell*, 3>, SystemAllocPolicy>;
    CellsWithUniqueIdSet cellsWithUid_;

    struct SweepAction;
    SweepAction* sweepActions_;

    using NativeObjectVector = Vector<NativeObject*, 0, SystemAllocPolicy>;
    NativeObjectVector dictionaryModeObjects_;

#ifdef JS_GC_ZEAL
    struct Canary;
    Canary* lastCanary_;
#endif

    NurseryChunk* allocChunk();

    NurseryChunk& chunk(unsigned index) const {
        return *chunks_[index];
    }

    void setCurrentChunk(unsigned chunkno);
    void setStartPosition();

    void updateNumChunks(unsigned newCount);
    void updateNumChunksLocked(unsigned newCount, AutoLockGC& lock);

    MOZ_ALWAYS_INLINE uintptr_t allocationEnd() const {
        MOZ_ASSERT(numChunks() > 0);
        return chunks_.back()->end();
    }

    MOZ_ALWAYS_INLINE uintptr_t currentEnd() const {
        MOZ_ASSERT(runtime_);
        MOZ_ASSERT(currentEnd_ == chunk(currentChunk_).end());
        return currentEnd_;
    }
    void* addressOfCurrentEnd() const {
        MOZ_ASSERT(runtime_);
        return (void*)&currentEnd_;
    }

    uintptr_t position() const { return position_; }
    void* addressOfPosition() const { return (void*)&position_; }

    JSRuntime* runtime() const { return runtime_; }

    /* Allocates a new GC thing from the tenured generation during minor GC. */
    gc::TenuredCell* allocateFromTenured(JS::Zone* zone, gc::AllocKind thingKind);

    /* Common internal allocator function. */
    void* allocate(size_t size);

    double doCollection(JSRuntime* rt, JS::gcreason::Reason reason,
                        gc::TenureCountCache& tenureCounts);

    /*
     * Move the object at |src| in the Nursery to an already-allocated cell
     * |dst| in Tenured.
     */
    void collectToFixedPoint(TenuringTracer& trc, gc::TenureCountCache& tenureCounts);

    /* Handle relocation of slots/elements pointers stored in Ion frames. */
    void setForwardingPointer(void* oldData, void* newData, bool direct);

    void setSlotsForwardingPointer(HeapSlot* oldSlots, HeapSlot* newSlots, uint32_t nslots);
    void setElementsForwardingPointer(ObjectElements* oldHeader, ObjectElements* newHeader,
                                      uint32_t nelems);

    /* Free malloced pointers owned by freed things in the nursery. */
    void freeMallocedBuffers();

    /*
     * Frees all non-live nursery-allocated things at the end of a minor
     * collection.
     */
    void sweep();

    void runSweepActions();
    void sweepDictionaryModeObjects();

    /* Change the allocable space provided by the nursery. */
    void maybeResizeNursery(JS::gcreason::Reason reason, double promotionRate);
    void growAllocableSpace();
    void shrinkAllocableSpace();

    /* Profile recording and printing. */
    void startProfile(ProfileKey key);
    void endProfile(ProfileKey key);
    void maybeStartProfile(ProfileKey key);
    void maybeEndProfile(ProfileKey key);
    static void printProfileHeader();
    static void printProfileTimes(const ProfileTimes& times);

    friend class TenuringTracer;
    friend class gc::MinorCollectionTracer;
    friend class jit::MacroAssembler;
};

} /* namespace js */

#endif /* gc_Nursery_h */
