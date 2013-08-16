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

namespace js {
namespace gc {

class AccumulateEdgesTracer;

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
        map->rekey(prior, key);
    }
};

typedef HashSet<void *, PointerHasher<void *, 3>, SystemAllocPolicy> EdgeSet;

/*
 * The StoreBuffer observes all writes that occur in the system and performs
 * efficient filtering of them to derive a remembered set for nursery GC.
 */
class StoreBuffer
{
    /* The size of a single block of store buffer storage space. */
    const static size_t ChunkSize = 1 << 16; /* 64KiB */

    /* The size at which a block is about to overflow. */
    const static size_t MinAvailableSize = (size_t)(ChunkSize * 1.0 / 8.0);

    /*
     * This buffer holds only a single type of edge. Using this buffer is more
     * efficient than the generic buffer when many writes will be to the same
     * type of edge: e.g. Value or Cell*.
     */
    template<typename T>
    class MonoTypeBuffer
    {
        friend class StoreBuffer;
        friend class mozilla::ReentrancyGuard;

        StoreBuffer *owner;

        LifoAlloc storage_;
        bool enabled_;
        mozilla::DebugOnly<bool> entered;

        explicit MonoTypeBuffer(StoreBuffer *owner)
          : owner(owner), storage_(ChunkSize), enabled_(false), entered(false)
        {}

        MonoTypeBuffer &operator=(const MonoTypeBuffer& other) MOZ_DELETE;

        void enable() { enabled_ = true; }
        void disable() { enabled_ = false; clear(); }
        void clear() { storage_.used() ? storage_.releaseAll() : storage_.freeAll(); }

        bool isAboutToOverflow() const {
            return !storage_.isEmpty() && storage_.availableInCurrentChunk() < MinAvailableSize;
        }

        /* Compaction algorithms. */
        void compactRemoveDuplicates();

        /*
         * Attempts to reduce the usage of the buffer by removing unnecessary
         * entries.
         */
        virtual void compact();

        /* Add one item to the buffer. */
        void put(const T &t) {
            mozilla::ReentrancyGuard g(*this);
            JS_ASSERT(!owner->inParallelSection());

            if (!enabled_)
                return;

            T *tp = storage_.new_<T>(t);
            if (!tp)
                MOZ_CRASH();

            if (isAboutToOverflow()) {
                compact();
                if (isAboutToOverflow())
                    owner->setAboutToOverflow();
            }
        }

        /* Mark the source of all edges in the store buffer. */
        void mark(JSTracer *trc);
    };

    /*
     * Overrides the MonoTypeBuffer to support pointers that may be moved in
     * memory outside of the GC's control.
     */
    template <typename T>
    class RelocatableMonoTypeBuffer : public MonoTypeBuffer<T>
    {
        friend class StoreBuffer;

        explicit RelocatableMonoTypeBuffer(StoreBuffer *owner)
          : MonoTypeBuffer<T>(owner)
        {}

        /* Override compaction to filter out removed items. */
        void compactMoved();
        virtual void compact();

        /* Record a removal from the buffer. */
        void unput(const T &v) {
            JS_ASSERT(!this->owner->inParallelSection());
            MonoTypeBuffer<T>::put(v.tagged());
        }
    };

    class GenericBuffer
    {
        friend class StoreBuffer;
        friend class mozilla::ReentrancyGuard;

        StoreBuffer *owner;
        LifoAlloc storage_;
        bool enabled_;
        mozilla::DebugOnly<bool> entered;

        explicit GenericBuffer(StoreBuffer *owner)
          : owner(owner), storage_(ChunkSize), enabled_(false), entered(false)
        {}

        GenericBuffer &operator=(const GenericBuffer& other) MOZ_DELETE;

        void enable() { enabled_ = true; }
        void disable() { enabled_ = false; clear(); }
        void clear() { storage_.used() ? storage_.releaseAll() : storage_.freeAll(); }

        bool isAboutToOverflow() const {
            return !storage_.isEmpty() && storage_.availableInCurrentChunk() < MinAvailableSize;
        }

        /* Mark all generic edges. */
        void mark(JSTracer *trc);

        template <typename T>
        void put(const T &t) {
            mozilla::ReentrancyGuard g(*this);
            JS_ASSERT(!owner->inParallelSection());

            /* Ensure T is derived from BufferableRef. */
            (void)static_cast<const BufferableRef*>(&t);

            if (!enabled_)
                return;

            unsigned size = sizeof(T);
            unsigned *sizep = storage_.newPod<unsigned>();
            if (!sizep)
                MOZ_CRASH();
            *sizep = size;

            T *tp = storage_.new_<T>(t);
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

        Value *edge;

        explicit ValueEdge(Value *v) : edge(v) {}
        bool operator==(const ValueEdge &other) const { return edge == other.edge; }
        bool operator!=(const ValueEdge &other) const { return edge != other.edge; }

        void *deref() const { return edge->isGCThing() ? edge->toGCThing() : NULL; }
        void *location() const { return (void *)untagged().edge; }

        bool inRememberedSet(const Nursery &nursery) const {
            return !nursery.isInside(edge) && nursery.isInside(deref());
        }

        bool isNullEdge() const {
            return !deref();
        }

        void mark(JSTracer *trc);

        ValueEdge tagged() const { return ValueEdge((Value *)(uintptr_t(edge) | 1)); }
        ValueEdge untagged() const { return ValueEdge((Value *)(uintptr_t(edge) & ~1)); }
        bool isTagged() const { return bool(uintptr_t(edge) & 1); }
    };

    struct SlotEdge
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<SlotEdge>;

        JSObject *object;
        uint32_t offset;
        HeapSlot::Kind kind;

        SlotEdge(JSObject *object, HeapSlot::Kind kind, uint32_t offset)
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
        JS_ALWAYS_INLINE bool inRememberedSet(const Nursery &nursery) const;
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

    MonoTypeBuffer<ValueEdge> bufferVal;
    MonoTypeBuffer<CellPtrEdge> bufferCell;
    MonoTypeBuffer<SlotEdge> bufferSlot;
    MonoTypeBuffer<WholeCellEdges> bufferWholeCell;
    RelocatableMonoTypeBuffer<ValueEdge> bufferRelocVal;
    RelocatableMonoTypeBuffer<CellPtrEdge> bufferRelocCell;
    GenericBuffer bufferGeneric;

    /* This set is used as temporary storage by the buffers when compacting. */
    EdgeSet edgeSet;

    JSRuntime *runtime;
    const Nursery &nursery_;

    bool aboutToOverflow;
    bool enabled;

  public:
    explicit StoreBuffer(JSRuntime *rt, const Nursery &nursery)
      : bufferVal(this), bufferCell(this), bufferSlot(this), bufferWholeCell(this),
        bufferRelocVal(this), bufferRelocCell(this), bufferGeneric(this),
        runtime(rt), nursery_(nursery), aboutToOverflow(false), enabled(false)
    {
        edgeSet.init();
    }

    bool enable();
    void disable();
    bool isEnabled() { return enabled; }

    bool clear();

    /* Get the overflowed status. */
    bool isAboutToOverflow() const { return aboutToOverflow; }

    /* Insert a single edge into the buffer/remembered set. */
    void putValue(Value *valuep) {
        ValueEdge edge(valuep);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferVal.put(edge);
    }
    void putCell(Cell **cellp) {
        CellPtrEdge edge(cellp);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferCell.put(edge);
    }
    void putSlot(JSObject *obj, HeapSlot::Kind kind, uint32_t slot, void *target) {
        SlotEdge edge(obj, kind, slot);
        /* This is manually inlined because slotLocation cannot be defined here. */
        if (nursery_.isInside(obj) || !nursery_.isInside(target))
            return;
        bufferSlot.put(edge);
    }
    void putWholeCell(Cell *cell) {
        bufferWholeCell.put(WholeCellEdges(cell));
    }

    /* Insert or update a single edge in the Relocatable buffer. */
    void putRelocatableValue(Value *valuep) {
        ValueEdge edge(valuep);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferRelocVal.put(edge);
    }
    void putRelocatableCell(Cell **cellp) {
        CellPtrEdge edge(cellp);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferRelocCell.put(edge);
    }
    void removeRelocatableValue(Value *valuep) {
        ValueEdge edge(valuep);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferRelocVal.unput(edge);
    }
    void removeRelocatableCell(Cell **cellp) {
        CellPtrEdge edge(cellp);
        if (!edge.inRememberedSet(nursery_))
            return;
        bufferRelocCell.unput(edge);
    }

    /* Insert an entry into the generic buffer. */
    template <typename T>
    void putGeneric(const T &t) {
        bufferGeneric.put(t);
    }

    /* Insert or update a callback entry. */
    void putCallback(CallbackRef::MarkCallback callback, Cell *key, void *data) {
        if (nursery_.isInside(key))
            bufferGeneric.put(CallbackRef(callback, key, data));
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
