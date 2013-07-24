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

#include "mozilla/ReentrancyGuard.h"

#include "jsalloc.h"
#include "jsgc.h"
#include "jsobj.h"

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

        T *base;      /* Pointer to the start of the buffer. */
        T *pos;       /* Pointer to the current insertion position. */
        T *top;       /* Pointer to one element after the end. */

        /*
         * If the buffer's insertion position goes over the high-water-mark,
         * we trigger a minor GC at the next operation callback.
         */
        T *highwater;

        /*
         * This set stores duplicates found when compacting. We create the set
         * here, rather than local to the algorithm to avoid malloc overhead in
         * the common case.
         */
        EdgeSet duplicates;

        bool entered;

        MonoTypeBuffer(StoreBuffer *owner)
          : owner(owner), base(NULL), pos(NULL), top(NULL), entered(false)
        {
            duplicates.init();
        }

        MonoTypeBuffer &operator=(const MonoTypeBuffer& other) MOZ_DELETE;

        bool enable(uint8_t *region, size_t len);
        void disable();
        void clear();

        bool isEmpty() const { return pos == base; }
        bool isFull() const { JS_ASSERT(pos <= top); return pos == top; }
        bool isAboutToOverflow() const { return pos >= highwater; }

        /* Compaction algorithms. */
        void compactNotInSet(const Nursery &nursery);
        void compactRemoveDuplicates();

        /*
         * Attempts to reduce the usage of the buffer by removing unnecessary
         * entries.
         */
        virtual void compact();

        /* Add one item to the buffer. */
        void put(const T &v) {
            mozilla::ReentrancyGuard g(*this);
            JS_ASSERT(!owner->inParallelSection());

            /* Check if we have been enabled. */
            if (!pos)
                return;

            /*
             * Note: it is sometimes valid for a put to happen in the middle of a GC,
             * e.g. a rekey of a Relocatable may end up here. In general, we do not
             * care about these new entries or any overflows they cause.
             */
            *pos++ = v;
            if (isAboutToOverflow()) {
                owner->setAboutToOverflow();
                if (isFull()) {
                    compact();
                    if (isFull()) {
                        owner->setOverflowed();
                        clear();
                    }
                }
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

        RelocatableMonoTypeBuffer(StoreBuffer *owner)
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

        uint8_t *base; /* Pointer to start of buffer. */
        uint8_t *pos;  /* Pointer to current buffer position. */
        uint8_t *top;  /* Pointer to one past the last entry. */

        bool entered;

        GenericBuffer(StoreBuffer *owner)
          : owner(owner), base(NULL), pos(NULL), top(NULL), entered(false)
        {}

        GenericBuffer &operator=(const GenericBuffer& other) MOZ_DELETE;

        bool enable(uint8_t *region, size_t len);
        void disable();
        void clear();

        /* Mark all generic edges. */
        void mark(JSTracer *trc);

        template <typename T>
        void put(const T &t) {
            mozilla::ReentrancyGuard g(*this);
            JS_ASSERT(!owner->inParallelSection());

            /* Ensure T is derived from BufferableRef. */
            (void)static_cast<const BufferableRef*>(&t);

            /* Check if we have been enabled. */
            if (!pos)
                return;

            /* Check for overflow. */
            if (unsigned(top - pos) < unsigned(sizeof(unsigned) + sizeof(T))) {
                owner->setOverflowed();
                return;
            }

            *((unsigned *)pos) = sizeof(T);
            pos += sizeof(unsigned);

            T *p = (T *)pos;
            new (p) T(t);
            pos += sizeof(T);
        }
    };

    class CellPtrEdge
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<CellPtrEdge>;
        friend class StoreBuffer::RelocatableMonoTypeBuffer<CellPtrEdge>;

        Cell **edge;

        CellPtrEdge(Cell **v) : edge(v) {}
        bool operator==(const CellPtrEdge &other) const { return edge == other.edge; }
        bool operator!=(const CellPtrEdge &other) const { return edge != other.edge; }

        void *location() const { return (void *)edge; }

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

        ValueEdge(Value *v) : edge(v) {}
        bool operator==(const ValueEdge &other) const { return edge == other.edge; }
        bool operator!=(const ValueEdge &other) const { return edge != other.edge; }

        void *deref() const { return edge->isGCThing() ? edge->toGCThing() : NULL; }
        void *location() const { return (void *)edge; }

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

    JSRuntime *runtime;

    void *buffer;

    bool aboutToOverflow;
    bool overflowed;
    bool enabled;

    /* TODO: profile to find the ideal size for these. */
    static const size_t ValueBufferSize = 1 * 1024 * sizeof(ValueEdge);
    static const size_t CellBufferSize = 8 * 1024 * sizeof(CellPtrEdge);
    static const size_t SlotBufferSize = 2 * 1024 * sizeof(SlotEdge);
    static const size_t WholeCellBufferSize = 2 * 1024 * sizeof(WholeCellEdges);
    static const size_t RelocValueBufferSize = 1 * 1024 * sizeof(ValueEdge);
    static const size_t RelocCellBufferSize = 1 * 1024 * sizeof(CellPtrEdge);
    static const size_t GenericBufferSize = 1 * 1024 * sizeof(int);
    static const size_t TotalSize = ValueBufferSize + CellBufferSize +
                                    SlotBufferSize + WholeCellBufferSize +
                                    RelocValueBufferSize + RelocCellBufferSize +
                                    GenericBufferSize;

  public:
    explicit StoreBuffer(JSRuntime *rt)
      : bufferVal(this), bufferCell(this), bufferSlot(this), bufferWholeCell(this),
        bufferRelocVal(this), bufferRelocCell(this), bufferGeneric(this),
        runtime(rt), buffer(NULL), aboutToOverflow(false), overflowed(false),
        enabled(false)
    {}

    bool enable();
    void disable();
    bool isEnabled() { return enabled; }

    bool clear();

    /* Get the overflowed status. */
    bool isAboutToOverflow() const { return aboutToOverflow; }
    bool hasOverflowed() const { return overflowed; }

    /* Insert a single edge into the buffer/remembered set. */
    void putValue(Value *v) {
        bufferVal.put(v);
    }
    void putCell(Cell **o) {
        bufferCell.put(o);
    }
    void putSlot(JSObject *obj, HeapSlot::Kind kind, uint32_t slot) {
        bufferSlot.put(SlotEdge(obj, kind, slot));
    }
    void putWholeCell(Cell *cell) {
        bufferWholeCell.put(WholeCellEdges(cell));
    }

    /* Insert or update a single edge in the Relocatable buffer. */
    void putRelocatableValue(Value *v) {
        bufferRelocVal.put(v);
    }
    void putRelocatableCell(Cell **c) {
        bufferRelocCell.put(c);
    }
    void removeRelocatableValue(Value *v) {
        bufferRelocVal.unput(v);
    }
    void removeRelocatableCell(Cell **c) {
        bufferRelocCell.unput(c);
    }

    /* Insert an entry into the generic buffer. */
    template <typename T>
    void putGeneric(const T &t) {
        bufferGeneric.put(t);
    }

    /* Insert or update a callback entry. */
    void putCallback(CallbackRef::MarkCallback callback, Cell *key, void *data) {
        if (!key->isTenured())
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
