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
#include "js/Tracer.h"

namespace js {
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
    bool inRememberedSet(const Nursery &) const { return true; }
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
        JS_SET_TRACING_LOCATION(trc, (void*)&*p);
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
    class MonoTypeBuffer
    {
        friend class StoreBuffer;

        LifoAlloc *storage_;

        explicit MonoTypeBuffer() : storage_(nullptr) {}
        ~MonoTypeBuffer() { js_delete(storage_); }

        MonoTypeBuffer &operator=(const MonoTypeBuffer& other) MOZ_DELETE;

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

        /* Compaction algorithms. */
        void compactRemoveDuplicates(StoreBuffer *owner);

        /*
         * Attempts to reduce the usage of the buffer by removing unnecessary
         * entries.
         */
        virtual void compact(StoreBuffer *owner);

        /* Add one item to the buffer. */
        void put(StoreBuffer *owner, const T &t) {
            JS_ASSERT(storage_);

            T *tp = storage_->new_<T>(t);
            if (!tp)
                MOZ_CRASH();

            if (isAboutToOverflow()) {
                compact(owner);
                if (isAboutToOverflow())
                    owner->setAboutToOverflow();
            }
        }

        /* Mark the source of all edges in the store buffer. */
        void mark(StoreBuffer *owner, JSTracer *trc);
    };

    /*
     * Overrides the MonoTypeBuffer to support pointers that may be moved in
     * memory outside of the GC's control.
     */
    template <typename T>
    class RelocatableMonoTypeBuffer : public MonoTypeBuffer<T>
    {
        friend class StoreBuffer;

        /* Override compaction to filter out removed items. */
        void compactMoved(StoreBuffer *owner);
        virtual void compact(StoreBuffer *owner) MOZ_OVERRIDE;

        /* Record a removal from the buffer. */
        void unput(StoreBuffer *owner, const T &v) {
            MonoTypeBuffer<T>::put(owner, v.tagged());
        }
    };

    class GenericBuffer
    {
        friend class StoreBuffer;

        LifoAlloc *storage_;

        explicit GenericBuffer() : storage_(nullptr) {}
        ~GenericBuffer() { js_delete(storage_); }

        GenericBuffer &operator=(const GenericBuffer& other) MOZ_DELETE;

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
                MOZ_CRASH();
            *sizep = size;

            T *tp = storage_->new_<T>(t);
            if (!tp)
                MOZ_CRASH();

            if (isAboutToOverflow())
                owner->setAboutToOverflow();
        }
    };

    class CellPtrEdge
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<CellPtrEdge>;
        friend class StoreBuffer::RelocatableMonoTypeBuffer<CellPtrEdge>;

        Cell **edge;

        explicit CellPtrEdge(Cell **v) : edge(v) {}
        bool operator==(const CellPtrEdge &other) const { return edge == other.edge; }
        bool operator!=(const CellPtrEdge &other) const { return edge != other.edge; }

        void *location() const { return (void *)untagged().edge; }

        bool inRememberedSet(const Nursery &nursery) const {
            return !nursery.isInside(edge) && nursery.isInside(*edge);
        }

        bool isNullEdge() const {
            return !*edge;
        }

        void mark(JSTracer *trc);

        CellPtrEdge tagged() const { return CellPtrEdge((Cell **)(uintptr_t(edge) | 1)); }
        CellPtrEdge untagged() const { return CellPtrEdge((Cell **)(uintptr_t(edge) & ~1)); }
        bool isTagged() const { return bool(uintptr_t(edge) & 1); }
    };

    class ValueEdge
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<ValueEdge>;
        friend class StoreBuffer::RelocatableMonoTypeBuffer<ValueEdge>;

        JS::Value *edge;

        explicit ValueEdge(JS::Value *v) : edge(v) {}
        bool operator==(const ValueEdge &other) const { return edge == other.edge; }
        bool operator!=(const ValueEdge &other) const { return edge != other.edge; }

        void *deref() const { return edge->isGCThing() ? edge->toGCThing() : nullptr; }
        void *location() const { return (void *)untagged().edge; }

        bool inRememberedSet(const Nursery &nursery) const {
            return !nursery.isInside(edge) && nursery.isInside(deref());
        }

        bool isNullEdge() const {
            return !deref();
        }

        void mark(JSTracer *trc);

        ValueEdge tagged() const { return ValueEdge((JS::Value *)(uintptr_t(edge) | 1)); }
        ValueEdge untagged() const { return ValueEdge((JS::Value *)(uintptr_t(edge) & ~1)); }
        bool isTagged() const { return bool(uintptr_t(edge) & 1); }
    };

    struct SlotEdge
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<SlotEdge>;

        JSObject *object;
        uint32_t offset;
        int kind; // this is really just HeapSlot::Kind, but we can't see that type easily here

        SlotEdge(JSObject *object, int kind, uint32_t offset)
          : object(object), offset(offset), kind(kind)
        {}

        bool operator==(const SlotEdge &other) const {
            return object == other.object && offset == other.offset && kind == other.kind;
        }

        bool operator!=(const SlotEdge &other) const {
            return object != other.object || offset != other.offset || kind != other.kind;
        }

        JS_ALWAYS_INLINE HeapSlot *slotLocation() const;

        JS_ALWAYS_INLINE void *deref() const;
        JS_ALWAYS_INLINE void *location() const;
        bool inRememberedSet(const Nursery &nursery) const;
        JS_ALWAYS_INLINE bool isNullEdge() const;

        void mark(JSTracer *trc);
    };

    class WholeCellEdges
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<WholeCellEdges>;

        Cell *tenured;

        WholeCellEdges(Cell *cell) : tenured(cell) {
            JS_ASSERT(tenured->isTenured());
        }

        bool operator==(const WholeCellEdges &other) const { return tenured == other.tenured; }
        bool operator!=(const WholeCellEdges &other) const { return tenured != other.tenured; }

        bool inRememberedSet(const Nursery &nursery) const { return true; }

        /* This is used by RemoveDuplicates as a unique pointer to this Edge. */
        void *location() const { return (void *)tenured; }

        bool isNullEdge() const { return false; }

        void mark(JSTracer *trc);
    };

    class CallbackRef : public BufferableRef
    {
      public:
        typedef void (*MarkCallback)(JSTracer *trc, void *key, void *data);

        CallbackRef(MarkCallback cb, void *k, void *d) : callback(cb), key(k), data(d) {}

        virtual void mark(JSTracer *trc) {
            callback(trc, key, data);
        }

      private:
        MarkCallback callback;
        void *key;
        void *data;
    };

    template <typename Edge>
    bool isOkayToUseBuffer(const Edge &edge) const {
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
        if (!CurrentThreadCanAccessRuntime(runtime_)) {
            JS_ASSERT(!edge.inRememberedSet(nursery_));
            return false;
        }

        return true;
    }

    template <typename Buffer, typename Edge>
    void put(Buffer &buffer, const Edge &edge) {
        if (!isOkayToUseBuffer(edge))
            return;
        mozilla::ReentrancyGuard g(*this);
        if (edge.inRememberedSet(nursery_))
            buffer.put(this, edge);
    }

    template <typename Buffer, typename Edge>
    void unput(Buffer &buffer, const Edge &edge) {
        if (!isOkayToUseBuffer(edge))
            return;
        mozilla::ReentrancyGuard g(*this);
        buffer.unput(this, edge);
    }

    MonoTypeBuffer<ValueEdge> bufferVal;
    MonoTypeBuffer<CellPtrEdge> bufferCell;
    MonoTypeBuffer<SlotEdge> bufferSlot;
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
    void putValue(JS::Value *valuep) { put(bufferVal, ValueEdge(valuep)); }
    void putCell(Cell **cellp) { put(bufferCell, CellPtrEdge(cellp)); }
    void putSlot(JSObject *obj, int kind, uint32_t slot, void *target) {
        put(bufferSlot, SlotEdge(obj, kind, slot));
    }
    void putWholeCell(Cell *cell) {
        JS_ASSERT(cell->isTenured());
        put(bufferWholeCell, WholeCellEdges(cell));
    }

    /* Insert or update a single edge in the Relocatable buffer. */
    void putRelocatableValue(JS::Value *valuep) { put(bufferRelocVal, ValueEdge(valuep)); }
    void putRelocatableCell(Cell **cellp) { put(bufferRelocCell, CellPtrEdge(cellp)); }
    void removeRelocatableValue(JS::Value *valuep) { unput(bufferRelocVal, ValueEdge(valuep)); }
    void removeRelocatableCell(Cell **cellp) { unput(bufferRelocCell, CellPtrEdge(cellp)); }

    /* Insert an entry into the generic buffer. */
    template <typename T>
    void putGeneric(const T &t) { put(bufferGeneric, t);}

    /* Insert or update a callback entry. */
    void putCallback(CallbackRef::MarkCallback callback, Cell *key, void *data) {
        put(bufferGeneric, CallbackRef(callback, key, data));
    }

    /* Mark the source of all edges in the store buffer. */
    void mark(JSTracer *trc);

    /* We cannot call InParallelSection directly because of a circular dependency. */
    bool inParallelSection() const;

    /* For use by our owned buffers and for testing. */
    void setAboutToOverflow();
    void setOverflowed();
};

} /* namespace gc */
} /* namespace js */

#endif /* JSGC_GENERATIONAL */

#endif /* gc_StoreBuffer_h */
