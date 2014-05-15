/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_StoreBuffer_h
#define gc_StoreBuffer_h

#ifdef JSGC_GENERATIONAL

#ifndef JSGC_USE_EXACT_ROOTING
# error "Generational GC requires exact rooting."
#endif

#include "mozilla/DebugOnly.h"
#include "mozilla/ReentrancyGuard.h"

#include "jsalloc.h"

#include "ds/LifoAlloc.h"
#include "gc/Nursery.h"
#include "gc/Tracer.h"
#include "js/MemoryMetrics.h"

namespace js {

void
CrashAtUnhandlableOOM(const char *reason);

namespace gc {

/*
 * BufferableRef represents an abstract reference for use in the generational
 * GC's remembered set. Entries in the store buffer that cannot be represented
 * with the simple pointer-to-a-pointer scheme must derive from this class and
 * use the generic store buffer interface.
 */
class BufferableRef
{
  public:
    virtual void mark(JSTracer *trc) = 0;
    bool maybeInRememberedSet(const Nursery &) const { return true; }
};

/*
 * HashKeyRef represents a reference to a HashMap key. This should normally
 * be used through the HashTableWriteBarrierPost function.
 */
template <typename Map, typename Key>
class HashKeyRef : public BufferableRef
{
    Map *map;
    Key key;

  public:
    HashKeyRef(Map *m, const Key &k) : map(m), key(k) {}

    void mark(JSTracer *trc) {
        Key prior = key;
        typename Map::Ptr p = map->lookup(key);
        if (!p)
            return;
        trc->setTracingLocation(&*p);
        Mark(trc, &key, "HashKeyRef");
        map->rekeyIfMoved(prior, key);
    }
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> EdgeSet;

/* The size of a single block of store buffer storage space. */
static const size_t LifoAllocBlockSize = 1 << 16; /* 64KiB */

/*
 * The StoreBuffer observes all writes that occur in the system and performs
 * efficient filtering of them to derive a remembered set for nursery GC.
 */
class StoreBuffer
{
    friend class mozilla::ReentrancyGuard;

    /* The size at which a block is about to overflow. */
    static const size_t MinAvailableSize = (size_t)(LifoAllocBlockSize * 1.0 / 8.0);

    /*
     * This buffer holds only a single type of edge. Using this buffer is more
     * efficient than the generic buffer when many writes will be to the same
     * type of edge: e.g. Value or Cell*.
     */
    template<typename T>
    struct MonoTypeBuffer
    {
        LifoAlloc *storage_;
        size_t usedAtLastCompact_;

        explicit MonoTypeBuffer() : storage_(nullptr), usedAtLastCompact_(0) {}
        ~MonoTypeBuffer() { js_delete(storage_); }

        bool init() {
            if (!storage_)
                storage_ = js_new<LifoAlloc>(LifoAllocBlockSize);
            clear();
            return bool(storage_);
        }

        void clear() {
            if (!storage_)
                return;

            storage_->used() ? storage_->releaseAll() : storage_->freeAll();
            usedAtLastCompact_ = 0;
        }

        bool isAboutToOverflow() const {
            return !storage_->isEmpty() && storage_->availableInCurrentChunk() < MinAvailableSize;
        }

        void handleOverflow(StoreBuffer *owner);

        /* Compaction algorithms. */
        void compactRemoveDuplicates(StoreBuffer *owner);

        /*
         * Attempts to reduce the usage of the buffer by removing unnecessary
         * entries.
         */
        virtual void compact(StoreBuffer *owner);

        /* Compacts if any entries have been added since the last compaction. */
        void maybeCompact(StoreBuffer *owner);

        /* Add one item to the buffer. */
        void put(StoreBuffer *owner, const T &t) {
            JS_ASSERT(storage_);

            T *tp = storage_->new_<T>(t);
            if (!tp)
                CrashAtUnhandlableOOM("Failed to allocate for MonoTypeBuffer::put.");

            if (isAboutToOverflow())
                handleOverflow(owner);
        }

        /* Mark the source of all edges in the store buffer. */
        void mark(StoreBuffer *owner, JSTracer *trc);

        size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
            return storage_ ? storage_->sizeOfIncludingThis(mallocSizeOf) : 0;
        }

      private:
        MonoTypeBuffer &operator=(const MonoTypeBuffer& other) MOZ_DELETE;
    };

    /*
     * Overrides the MonoTypeBuffer to support pointers that may be moved in
     * memory outside of the GC's control.
     */
    template <typename T>
    struct RelocatableMonoTypeBuffer : public MonoTypeBuffer<T>
    {
        /* Override compaction to filter out removed items. */
        void compactMoved(StoreBuffer *owner);
        virtual void compact(StoreBuffer *owner) MOZ_OVERRIDE;

        /* Record a removal from the buffer. */
        void unput(StoreBuffer *owner, const T &v) {
            MonoTypeBuffer<T>::put(owner, v.tagged());
        }
    };

    struct GenericBuffer
    {
        LifoAlloc *storage_;

        explicit GenericBuffer() : storage_(nullptr) {}
        ~GenericBuffer() { js_delete(storage_); }

        bool init() {
            if (!storage_)
                storage_ = js_new<LifoAlloc>(LifoAllocBlockSize);
            clear();
            return bool(storage_);
        }

        void clear() {
            if (!storage_)
                return;

            storage_->used() ? storage_->releaseAll() : storage_->freeAll();
        }

        bool isAboutToOverflow() const {
            return !storage_->isEmpty() && storage_->availableInCurrentChunk() < MinAvailableSize;
        }

        /* Mark all generic edges. */
        void mark(StoreBuffer *owner, JSTracer *trc);

        template <typename T>
        void put(StoreBuffer *owner, const T &t) {
            JS_ASSERT(storage_);

            /* Ensure T is derived from BufferableRef. */
            (void)static_cast<const BufferableRef*>(&t);

            unsigned size = sizeof(T);
            unsigned *sizep = storage_->newPod<unsigned>();
            if (!sizep)
                CrashAtUnhandlableOOM("Failed to allocate for GenericBuffer::put.");
            *sizep = size;

            T *tp = storage_->new_<T>(t);
            if (!tp)
                CrashAtUnhandlableOOM("Failed to allocate for GenericBuffer::put.");

            if (isAboutToOverflow())
                owner->setAboutToOverflow();
        }

        size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
            return storage_ ? storage_->sizeOfIncludingThis(mallocSizeOf) : 0;
        }

      private:
        GenericBuffer &operator=(const GenericBuffer& other) MOZ_DELETE;
    };

    template <typename Edge>
    struct PointerEdgeHasher
    {
        typedef Edge Lookup;
        static HashNumber hash(const Lookup &l) { return uintptr_t(l.edge) >> 3; }
        static bool match(const Edge &k, const Lookup &l) { return k == l; }
    };

    struct CellPtrEdge
    {
        Cell **edge;

        explicit CellPtrEdge(Cell **v) : edge(v) {}
        bool operator==(const CellPtrEdge &other) const { return edge == other.edge; }
        bool operator!=(const CellPtrEdge &other) const { return edge != other.edge; }

        bool maybeInRememberedSet(const Nursery &nursery) const {
            JS_ASSERT(IsInsideNursery(*edge));
            return !nursery.isInside(edge);
        }

        void mark(JSTracer *trc);

        CellPtrEdge tagged() const { return CellPtrEdge((Cell **)(uintptr_t(edge) | 1)); }
        CellPtrEdge untagged() const { return CellPtrEdge((Cell **)(uintptr_t(edge) & ~1)); }
        bool isTagged() const { return bool(uintptr_t(edge) & 1); }

        typedef PointerEdgeHasher<CellPtrEdge> Hasher;
    };

    struct ValueEdge
    {
        JS::Value *edge;

        explicit ValueEdge(JS::Value *v) : edge(v) {}
        bool operator==(const ValueEdge &other) const { return edge == other.edge; }
        bool operator!=(const ValueEdge &other) const { return edge != other.edge; }

        Cell *deref() const { return edge->isGCThing() ? static_cast<Cell *>(edge->toGCThing()) : nullptr; }

        bool maybeInRememberedSet(const Nursery &nursery) const {
            JS_ASSERT(IsInsideNursery(deref()));
            return !nursery.isInside(edge);
        }

        void mark(JSTracer *trc);

        ValueEdge tagged() const { return ValueEdge((JS::Value *)(uintptr_t(edge) | 1)); }
        ValueEdge untagged() const { return ValueEdge((JS::Value *)(uintptr_t(edge) & ~1)); }
        bool isTagged() const { return bool(uintptr_t(edge) & 1); }

        typedef PointerEdgeHasher<ValueEdge> Hasher;
    };

    struct SlotsEdge
    {
        // These definitions must match those in HeapSlot::Kind.
        const static int SlotKind = 0;
        const static int ElementKind = 1;

        uintptr_t objectAndKind_; // JSObject* | Kind
        int32_t start_;
        int32_t count_;

        SlotsEdge(JSObject *object, int kind, int32_t start, int32_t count)
          : objectAndKind_(uintptr_t(object) | kind), start_(start), count_(count)
        {
            JS_ASSERT((uintptr_t(object) & 1) == 0);
            JS_ASSERT(kind <= 1);
            JS_ASSERT(start >= 0);
            JS_ASSERT(count > 0);
        }

        JSObject *object() const { return reinterpret_cast<JSObject *>(objectAndKind_ & ~1); }
        int kind() const { return (int)(objectAndKind_ & 1); }

        bool operator==(const SlotsEdge &other) const {
            return objectAndKind_ == other.objectAndKind_ &&
                   start_ == other.start_ &&
                   count_ == other.count_;
        }

        bool operator!=(const SlotsEdge &other) const {
            return !(*this == other);
        }

        bool maybeInRememberedSet(const Nursery &) const {
            return !IsInsideNursery(JS::AsCell(object()));
        }

        void mark(JSTracer *trc);

        typedef struct {
            typedef SlotsEdge Lookup;
            static HashNumber hash(const Lookup &l) { return l.objectAndKind_ ^ l.start_ ^ l.count_; }
            static bool match(const SlotsEdge &k, const Lookup &l) { return k == l; }
        } Hasher;
    };

    struct WholeCellEdges
    {
        Cell *edge;

        explicit WholeCellEdges(Cell *cell) : edge(cell) {
            JS_ASSERT(edge->isTenured());
        }

        bool operator==(const WholeCellEdges &other) const { return edge == other.edge; }
        bool operator!=(const WholeCellEdges &other) const { return edge != other.edge; }

        bool maybeInRememberedSet(const Nursery &) const { return true; }

        static bool supportsDeduplication() { return true; }
        void *deduplicationKey() const { return (void *)edge; }

        void mark(JSTracer *trc);

        typedef PointerEdgeHasher<WholeCellEdges> Hasher;
    };

    template <typename Key>
    struct CallbackRef : public BufferableRef
    {
        typedef void (*MarkCallback)(JSTracer *trc, Key *key, void *data);

        CallbackRef(MarkCallback cb, Key *k, void *d) : callback(cb), key(k), data(d) {}

        virtual void mark(JSTracer *trc) {
            callback(trc, key, data);
        }

      private:
        MarkCallback callback;
        Key *key;
        void *data;
    };

    bool isOkayToUseBuffer() const {
        /*
         * Disabled store buffers may not have a valid state; e.g. when stored
         * inline in the ChunkTrailer.
         */
        if (!isEnabled())
            return false;

        /*
         * The concurrent parsing thread cannot validly insert into the buffer,
         * but it should not activate the re-entrancy guard either.
         */
        if (!CurrentThreadCanAccessRuntime(runtime_))
            return false;

        return true;
    }

    template <typename Buffer, typename Edge>
    void putFromAnyThread(Buffer &buffer, const Edge &edge) {
        if (!isOkayToUseBuffer())
            return;
        mozilla::ReentrancyGuard g(*this);
        if (edge.maybeInRememberedSet(nursery_))
            buffer.put(this, edge);
    }

    template <typename Buffer, typename Edge>
    void unputFromAnyThread(Buffer &buffer, const Edge &edge) {
        if (!isOkayToUseBuffer())
            return;
        mozilla::ReentrancyGuard g(*this);
        buffer.unput(this, edge);
    }

    template <typename Buffer, typename Edge>
    void putFromMainThread(Buffer &buffer, const Edge &edge) {
        if (!isEnabled())
            return;
        JS_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
        mozilla::ReentrancyGuard g(*this);
        if (edge.maybeInRememberedSet(nursery_))
            buffer.put(this, edge);
    }

    MonoTypeBuffer<ValueEdge> bufferVal;
    MonoTypeBuffer<CellPtrEdge> bufferCell;
    MonoTypeBuffer<SlotsEdge> bufferSlot;
    MonoTypeBuffer<WholeCellEdges> bufferWholeCell;
    RelocatableMonoTypeBuffer<ValueEdge> bufferRelocVal;
    RelocatableMonoTypeBuffer<CellPtrEdge> bufferRelocCell;
    GenericBuffer bufferGeneric;

    JSRuntime *runtime_;
    const Nursery &nursery_;

    bool aboutToOverflow_;
    bool enabled_;
    mozilla::DebugOnly<bool> entered; /* For ReentrancyGuard. */

  public:
    explicit StoreBuffer(JSRuntime *rt, const Nursery &nursery)
      : bufferVal(), bufferCell(), bufferSlot(), bufferWholeCell(),
        bufferRelocVal(), bufferRelocCell(), bufferGeneric(),
        runtime_(rt), nursery_(nursery), aboutToOverflow_(false), enabled_(false),
        entered(false)
    {
    }

    bool enable();
    void disable();
    bool isEnabled() const { return enabled_; }

    bool clear();

    /* Get the overflowed status. */
    bool isAboutToOverflow() const { return aboutToOverflow_; }

    /* Insert a single edge into the buffer/remembered set. */
    void putValueFromAnyThread(JS::Value *valuep) { putFromAnyThread(bufferVal, ValueEdge(valuep)); }
    void putCellFromAnyThread(Cell **cellp) { putFromAnyThread(bufferCell, CellPtrEdge(cellp)); }
    void putSlotFromAnyThread(JSObject *obj, int kind, int32_t start, int32_t count) {
        putFromAnyThread(bufferSlot, SlotsEdge(obj, kind, start, count));
    }
    void putWholeCellFromMainThread(Cell *cell) {
        JS_ASSERT(cell->isTenured());
        putFromMainThread(bufferWholeCell, WholeCellEdges(cell));
    }

    /* Insert or update a single edge in the Relocatable buffer. */
    void putRelocatableValueFromAnyThread(JS::Value *valuep) {
        putFromAnyThread(bufferRelocVal, ValueEdge(valuep));
    }
    void removeRelocatableValueFromAnyThread(JS::Value *valuep) {
        unputFromAnyThread(bufferRelocVal, ValueEdge(valuep));
    }
    void putRelocatableCellFromAnyThread(Cell **cellp) {
        putFromAnyThread(bufferRelocCell, CellPtrEdge(cellp));
    }
    void removeRelocatableCellFromAnyThread(Cell **cellp) {
        unputFromAnyThread(bufferRelocCell, CellPtrEdge(cellp));
    }

    /* Insert an entry into the generic buffer. */
    template <typename T>
    void putGeneric(const T &t) { putFromAnyThread(bufferGeneric, t);}

    /* Insert or update a callback entry. */
    template <typename Key>
    void putCallback(void (*callback)(JSTracer *trc, Key *key, void *data), Key *key, void *data) {
        putFromAnyThread(bufferGeneric, CallbackRef<Key>(callback, key, data));
    }

    /* Methods to mark the source of all edges in the store buffer. */
    void markAll(JSTracer *trc);
    void markValues(JSTracer *trc)            { bufferVal.mark(this, trc); }
    void markCells(JSTracer *trc)             { bufferCell.mark(this, trc); }
    void markSlots(JSTracer *trc)             { bufferSlot.mark(this, trc); }
    void markWholeCells(JSTracer *trc)        { bufferWholeCell.mark(this, trc); }
    void markRelocatableValues(JSTracer *trc) { bufferRelocVal.mark(this, trc); }
    void markRelocatableCells(JSTracer *trc)  { bufferRelocCell.mark(this, trc); }
    void markGenericEntries(JSTracer *trc)    { bufferGeneric.mark(this, trc); }

    /* We cannot call InParallelSection directly because of a circular dependency. */
    bool inParallelSection() const;

    /* For use by our owned buffers and for testing. */
    void setAboutToOverflow();

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::GCSizes *sizes);
};

} /* namespace gc */
} /* namespace js */

#endif /* JSGC_GENERATIONAL */

#endif /* gc_StoreBuffer_h */
