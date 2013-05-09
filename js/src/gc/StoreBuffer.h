/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL
#ifndef jsgc_storebuffer_h___
#define jsgc_storebuffer_h___

#ifndef JSGC_USE_EXACT_ROOTING
# error "Generational GC requires exact rooting."
#endif

#include "jsgc.h"
#include "jsalloc.h"
#include "jsobj.h"

namespace js {
namespace gc {

class AccumulateEdgesTracer;

#ifdef JS_GC_ZEAL
/*
 * Note: this is a stub Nursery that does not actually contain a heap, just a
 * set of pointers which are "inside" the nursery to implement verification.
 */
class VerifierNursery
{
    HashSet<const void *, PointerHasher<const void *, 3>, SystemAllocPolicy> nursery;

  public:
    explicit VerifierNursery() : nursery() {}

    bool enable() {
        if (!nursery.initialized())
            return nursery.init();
        return true;
    }

    void disable() {
        if (!nursery.initialized())
            return;
        nursery.finish();
    }

    bool isEnabled() const {
        return nursery.initialized();
    }

    bool clear() {
        disable();
        return enable();
    }

    bool isInside(const void *cell) const {
        return nursery.initialized() && nursery.has(cell);
    }

    void insertPointer(void *cell) {
        nursery.putNew(cell);
    }
};
#endif /* JS_GC_ZEAL */

/*
 * BufferableRef represents an abstract reference for use in the generational
 * GC's remembered set. Entries in the store buffer that cannot be represented
 * with the simple pointer-to-a-pointer scheme must derive from this class and
 * use the generic store buffer interface.
 */
class BufferableRef
{
  public:
    virtual bool match(void *location) = 0;
    virtual void mark(JSTracer *trc) = 0;
};

/*
 * HashKeyRef represents a reference to a HashTable key. Manual HashTable
 * barriers should should instantiate this template with their own table/key
 * type to insert into the generic buffer with putGeneric.
 */
template <typename Map, typename Key>
class HashKeyRef : public BufferableRef
{
    Map *map;
    Key key;

    typedef typename Map::Entry::ValueType ValueType;
    typedef typename Map::Ptr Ptr;

  public:
    HashKeyRef(Map *m, const Key &k) : map(m), key(k) {}

    bool match(void *location) {
        Ptr p = map->lookup(key);
        if (!p)
            return false;
        return &p->key == location;
    }

    void mark(JSTracer *trc) {
        Key prior = key;
        typename Map::Ptr p = map->lookup(key);
        if (!p)
            return;
        ValueType value = p->value;
        Mark(trc, &key, "HashKeyRef");
        if (prior != key) {
            map->remove(prior);
            map->put(key, value);
        }
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

        MonoTypeBuffer(StoreBuffer *owner)
          : owner(owner), base(NULL), pos(NULL), top(NULL)
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
        template <typename NurseryType>
        void compactNotInSet(NurseryType *nursery);
        void compactRemoveDuplicates();

        /*
         * Attempts to reduce the usage of the buffer by removing unnecessary
         * entries.
         */
        virtual void compact();

        /* Add one item to the buffer. */
        void put(const T &v) {
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

        /* For verification. */
        bool accumulateEdges(EdgeSet &edges);
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
            MonoTypeBuffer<T>::put(v.tagged());
        }
    };

    class GenericBuffer
    {
        friend class StoreBuffer;

        StoreBuffer *owner;

        uint8_t *base; /* Pointer to start of buffer. */
        uint8_t *pos;  /* Pointer to current buffer position. */
        uint8_t *top;  /* Pointer to one past the last entry. */

        GenericBuffer(StoreBuffer *owner)
          : owner(owner)
        {}

        GenericBuffer &operator=(const GenericBuffer& other) MOZ_DELETE;

        bool enable(uint8_t *region, size_t len);
        void disable();
        void clear();

        /* Mark all generic edges. */
        void mark(JSTracer *trc);

        /* Check if a pointer is present in the buffer. */
        bool containsEdge(void *location) const;

        template <typename T>
        void put(const T &t) {
            /* Check if we have been enabled. */
            if (!pos)
                return;

            /* Check for overflow. */
            if (top - pos < (unsigned)(sizeof(unsigned) + sizeof(T))) {
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

        template <typename NurseryType>
        bool inRememberedSet(NurseryType *nursery) const {
            return !nursery->isInside(edge) && nursery->isInside(*edge);
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

        template <typename NurseryType>
        bool inRememberedSet(NurseryType *nursery) const {
            return !nursery->isInside(edge) && nursery->isInside(deref());
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

        template <typename NurseryType>
        JS_ALWAYS_INLINE bool inRememberedSet(NurseryType *nursery) const;

        JS_ALWAYS_INLINE bool isNullEdge() const;

        void mark(JSTracer *trc);
    };

    class WholeObjectEdges
    {
        friend class StoreBuffer;
        friend class StoreBuffer::MonoTypeBuffer<WholeObjectEdges>;

        JSObject *tenured;

        WholeObjectEdges(JSObject *obj) : tenured(obj) {
            JS_ASSERT(tenured->isTenured());
        }

        bool operator==(const WholeObjectEdges &other) const { return tenured == other.tenured; }
        bool operator!=(const WholeObjectEdges &other) const { return tenured != other.tenured; }

        template <typename NurseryType>
        bool inRememberedSet(NurseryType *nursery) const { return true; }

        /* This is used by RemoveDuplicates as a unique pointer to this Edge. */
        void *location() const { return (void *)tenured; }

        bool isNullEdge() const { return false; }

        void mark(JSTracer *trc);
    };

    MonoTypeBuffer<ValueEdge> bufferVal;
    MonoTypeBuffer<CellPtrEdge> bufferCell;
    MonoTypeBuffer<SlotEdge> bufferSlot;
    MonoTypeBuffer<WholeObjectEdges> bufferWholeObject;
    RelocatableMonoTypeBuffer<ValueEdge> bufferRelocVal;
    RelocatableMonoTypeBuffer<CellPtrEdge> bufferRelocCell;
    GenericBuffer bufferGeneric;

    JSRuntime *runtime;

    void *buffer;

    bool aboutToOverflow;
    bool overflowed;
    bool enabled;

    /* For the verifier. */
    EdgeSet edgeSet;

    /* TODO: profile to find the ideal size for these. */
    static const size_t ValueBufferSize = 1 * 1024 * sizeof(ValueEdge);
    static const size_t CellBufferSize = 2 * 1024 * sizeof(CellPtrEdge);
    static const size_t SlotBufferSize = 2 * 1024 * sizeof(SlotEdge);
    static const size_t WholeObjectBufferSize = 2 * 1024 * sizeof(WholeObjectEdges);
    static const size_t RelocValueBufferSize = 1 * 1024 * sizeof(ValueEdge);
    static const size_t RelocCellBufferSize = 1 * 1024 * sizeof(CellPtrEdge);
    static const size_t GenericBufferSize = 1 * 1024 * sizeof(int);
    static const size_t TotalSize = ValueBufferSize + CellBufferSize +
                                    SlotBufferSize + WholeObjectBufferSize +
                                    RelocValueBufferSize + RelocCellBufferSize +
                                    GenericBufferSize;

    /* For use by our owned buffers. */
    void setAboutToOverflow();
    void setOverflowed();

  public:
    explicit StoreBuffer(JSRuntime *rt)
      : bufferVal(this), bufferCell(this), bufferSlot(this), bufferWholeObject(this),
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
    void putWholeObject(JSObject *obj) {
        bufferWholeObject.put(WholeObjectEdges(obj));
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

    /* Mark the source of all edges in the store buffer. */
    void mark(JSTracer *trc);

    /* For the verifier. */
    bool coalesceForVerification();
    void releaseVerificationData();
    bool containsEdgeAt(void *loc) const;
};

} /* namespace gc */
} /* namespace js */

#endif /* jsgc_storebuffer_h___ */
#endif /* JSGC_GENERATIONAL */
