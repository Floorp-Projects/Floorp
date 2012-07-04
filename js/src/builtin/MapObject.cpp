/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/MapObject.h"

#include "jscntxt.h"
#include "jsiter.h"
#include "jsobj.h"

#include "gc/Marking.h"
#include "js/Utility.h"
#include "vm/GlobalObject.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

using namespace js;


/*** OrderedHashTable ****************************************************************************/

/*
 * Define two collection templates, js::OrderedHashMap and js::OrderedHashSet.
 * They are like js::HashMap and js::HashSet except that:
 *
 *   - Iterating over an Ordered hash table visits the entries in the order in
 *     which they were inserted. This means that unlike a HashMap, the behavior
 *     of an OrderedHashMap is deterministic (as long as the HashPolicy methods
 *     are effect-free and consistent); the hashing is a pure performance
 *     optimization.
 *
 *   - Range objects over Ordered tables remain valid even when entries are
 *     added or removed or the table is resized. (However in the case of
 *     removing entries, note the warning on class Range below.)
 *
 *   - The API is a little different, so it's not a drop-in replacement.
 *     In particular, the hash policy is a little different.
 *     Also, the Ordered templates lack the Ptr and AddPtr types.
 *
 * Hash policies
 *
 * See the comment about "Hash policy" in HashTable.h for general features that
 * hash policy classes must provide. Hash policies for OrderedHashMaps and Sets
 * must additionally provide a distinguished "empty" key value and the
 * following static member functions:
 *     bool isEmpty(const Key &);
 *     void makeEmpty(Key *);
 */

namespace js {

namespace detail {

/*
 * detail::OrderedHashTable is the underlying data structure used to implement both
 * OrderedHashMap and OrderedHashSet. Programs should use one of those two
 * templates rather than OrderedHashTable.
 */
template <class T, class Ops, class AllocPolicy>
class OrderedHashTable
{
  public:
    typedef typename Ops::KeyType Key;
    typedef typename Ops::Lookup Lookup;

    struct Data
    {
        T element;
        Data *chain;

        Data(const T &e, Data *c) : element(e), chain(c) {}
        Data(MoveRef<T> e, Data *c) : element(e), chain(c) {}
    };

    class Range;
    friend class Range;

  private:
    Data **hashTable;           // hash table (has hashBuckets() elements)
    Data *data;                 // data vector, an array of Data objects
                                // data[0:dataLength] are constructed
    uint32_t dataLength;        // number of constructed elements in data
    uint32_t dataCapacity;      // size of data, in elements
    uint32_t liveCount;         // dataLength less empty (removed) entries
    uint32_t hashShift;         // multiplicative hash shift
    Range *ranges;              // list of all live Ranges on this table
    AllocPolicy alloc;

  public:
    OrderedHashTable(AllocPolicy &ap)
        : hashTable(NULL), data(NULL), dataLength(0), ranges(NULL), alloc(ap) {}

    bool init() {
        MOZ_ASSERT(!hashTable, "init must be called at most once");

        uint32_t buckets = initialBuckets();
        Data **tableAlloc = static_cast<Data **>(alloc.malloc_(buckets * sizeof(Data *)));
        if (!tableAlloc)
            return false;
        for (uint32_t i = 0; i < buckets; i++)
            tableAlloc[i] = NULL;

        uint32_t capacity = uint32_t(buckets * fillFactor());
        Data *dataAlloc = static_cast<Data *>(alloc.malloc_(capacity * sizeof(Data)));
        if (!dataAlloc) {
            alloc.free_(tableAlloc);
            return false;
        }

        hashTable = tableAlloc;
        data = dataAlloc;
        dataLength = 0;
        dataCapacity = capacity;
        liveCount = 0;
        hashShift = HashNumberSizeBits - initialBucketsLog2();
        JS_ASSERT(hashBuckets() == buckets);
        return true;
    }

    ~OrderedHashTable() {
        alloc.free_(hashTable);
        freeData(data, dataLength);
    }

    /* Return the number of elements in the table. */
    uint32_t count() const { return liveCount; }

    /* True if any element matches l. */
    bool has(const Lookup &l) const {
        return lookup(l) != NULL;
    }

    /* Return a pointer to the element, if any, that matches l, else NULL. */
    T *get(const Lookup &l) {
        Data *e = lookup(l, prepareHash(l));
        return e ? &e->element : NULL;
    }

    /* Return a pointer to the element, if any, that matches l, else NULL. */
    const T *get(const Lookup &l) const {
        return const_cast<OrderedHashTable *>(this)->get(l);
    }

    /*
     * If the table already contains an entry that matches |element|,
     * replace that entry with |element|. Otherwise add a new entry.
     *
     * On success, return true, whether there was already a matching element or
     * not. On allocation failure, return false. If this returns false, it
     * means the element was not added to the table.
     */
    bool put(const T &element) {
        HashNumber h = prepareHash(Ops::getKey(element));
        if (Data *e = lookup(Ops::getKey(element), h)) {
            e->element = element;
            return true;
        }

        if (dataLength == dataCapacity) {
            // If the hashTable is more than 1/4 deleted data, simply rehash in
            // place to free up some space. Otherwise, grow the table.
            uint32_t newHashShift = liveCount >= dataCapacity * 0.75 ? hashShift - 1 : hashShift;
            if (!rehash(newHashShift))
                return false;
        }

        h >>= hashShift;
        liveCount++;
        Data *e = &data[dataLength++];
        new (e) Data(element, hashTable[h]);
        hashTable[h] = e;
        return true;
    }

    /*
     * If the table contains an element matching l, remove it and set *foundp
     * to true. Otherwise set *foundp to false.
     *
     * Return true on success, false if we tried to shrink the table and hit an
     * allocation failure. Even if this returns false, *foundp is set correctly
     * and the matching element was removed. Shrinking is an optimization and
     * it's OK for it to fail.
     */
    bool remove(const Lookup &l, bool *foundp) {
        // Note: This could be optimized so that removing the last entry,
        // data[dataLength - 1], decrements dataLength. LIFO use cases would
        // benefit.

        // If a matching entry exists, empty it.
        Data *e = lookup(l, prepareHash(l));
        if (e == NULL) {
            *foundp = false;
            return true;
        }

        *foundp = true;
        liveCount--;
        Ops::makeEmpty(&e->element);

        // Update active Ranges.
        uint32_t pos = e - data;
        for (Range *r = ranges; r; r = r->next)
            r->onRemove(pos);

        // If many entries have been removed, try to shrink the table.
        if (hashBuckets() > initialBuckets() && liveCount < dataLength * minDataFill()) {
            if (!rehash(hashShift + 1))
                return false;
        }
        return true;
    }

    /*
     * Ranges are used to iterate over OrderedHashTables.
     *
     * Suppose 'Map' is some instance of OrderedHashMap, and 'map' is a Map.
     * Then you can walk all the key-value pairs like this:
     *
     *     for (Map::Range r = map.all(); !r.empty(); r.popFront()) {
     *         Map::Entry &pair = r.front();
     *         ... do something with pair ...
     *     }
     *
     * Ranges remain valid for the lifetime of the OrderedHashTable, even if
     * entries are added or removed or the table is resized.
     *
     * Warning: The behavior when the current front() entry is removed from the
     * table is subtly different from js::HashTable<>::Enum::removeFront()!
     * HashTable::Enum doesn't skip any entries when you removeFront() and then
     * popFront(). OrderedHashTable::Range does! (This is useful for using a
     * Range to implement JS Map.prototype.iterator.)
     *
     * The workaround is to call popFront() as soon as possible,
     * before there's any possibility of modifying the table:
     *
     *     for (Map::Range r = map.all(); !r.empty(); ) {
     *         Key key = r.front().key;         // this won't modify map
     *         Value val = r.front().value;     // this won't modify map
     *         r.popFront();
     *         // ...do things that might modify map...
     *     }
     */
    class Range {
        friend class OrderedHashTable;

        OrderedHashTable &ht;

        /* The index of front() within ht.data. */
        uint32_t i;

        /*
         * The number of nonempty entries in ht.data to the left of front().
         * This is used when the table is resized or compacted.
         */
        uint32_t count;

        /*
         * Links in the doubly-linked list of active Ranges on ht.
         *
         * prevp points to the previous Range's .next field;
         *   or to ht.ranges if this is the first Range in the list.
         * next points to the next Range;
         *   or NULL if this is the last Range in the list.
         *
         * Invariant: *prevp == this.
         */
        Range **prevp;
        Range *next;

        /*
         * Create a Range over all the entries in ht.
         * (This is private on purpose. End users must use ht.all().)
         */
        Range(OrderedHashTable &ht) : ht(ht), i(0), count(0), prevp(&ht.ranges), next(ht.ranges) {
            *prevp = this;
            if (next)
                next->prevp = &next;
            seek();
        }

      public:
        Range(const Range &other)
            : ht(other.ht), i(other.i), count(other.count), prevp(&ht.ranges), next(ht.ranges)
        {
            *prevp = this;
            if (next)
                next->prevp = &next;
        }

        ~Range() {
            *prevp = next;
            if (next)
                next->prevp = prevp;
        }

      private:
        // Prohibit copy assignment.
        Range &operator=(const Range &other) MOZ_DELETE;

        void seek() {
            while (i < ht.dataLength && Ops::isEmpty(Ops::getKey(ht.data[i].element)))
                i++;
        }

        /*
         * The hash table calls this when an entry is removed.
         * j is the index of the removed entry.
         */
        void onRemove(uint32_t j) {
            if (j < i)
                count--;
            if (j == i)
                seek();
        }

        /*
         * The hash table calls this when the table is resized or compacted.
         * Since |count| is the number of nonempty entries to the left of
         * front(), discarding the empty entries will not affect count, and it
         * will make i and count equal.
         */
        void onCompact() {
            i = count;
        }

      public:
        bool empty() const { return i >= ht.dataLength; }

        /*
         * Return the first element in the range. This must not be called if
         * this->empty().
         *
         * Warning: Removing an entry from the table also removes it from any
         * live Ranges, and a Range can become empty that way, rendering
         * front() invalid. If in doubt, check empty() before calling front().
         */
        T &front() {
            MOZ_ASSERT(!empty());
            return ht.data[i].element;
        }

        /*
         * Remove the first element from this range.
         * This must not be called if this->empty().
         *
         * Warning: Removing an entry from the table also removes it from any
         * live Ranges, and a Range can become empty that way, rendering
         * popFront() invalid. If in doubt, check empty() before calling
         * popFront().
         */
        void popFront() {
            MOZ_ASSERT(!empty());
            MOZ_ASSERT(!Ops::isEmpty(Ops::getKey(ht.data[i].element)));
            count++;
            i++;
            seek();
        }

        /*
         * Change the key of the front entry.
         *
         * This calls Ops::hash on both the current key and the new key.
         * Ops::hash on the current key must return the same hash code as
         * when the entry was added to the table.
         */
        void rekeyFront(const Key &k) {
            Data &entry = ht.data[i];
            HashNumber oldHash = prepareHash(Ops::getKey(entry.element)) >> ht.hashShift;
            HashNumber newHash = prepareHash(k) >> ht.hashShift;
            Ops::setKey(entry.element, k);
            if (newHash != oldHash) {
                // Remove this entry from its old hash chain. (If this crashes
                // reading NULL, it would mean we did not find this entry on
                // the hash chain where we expected it. That probably means the
                // key's hash code changed since it was inserted, breaking the
                // hash code invariant.)
                Data **ep = &ht.hashTable[oldHash];
                while (*ep != &entry)
                    ep = &(*ep)->chain;
                *ep = entry.chain;

                // Add it to the new hash chain. We could just insert it at the
                // beginning of the chain. Instead, we do a bit of work to
                // preserve the invariant that hash chains always go in reverse
                // insertion order (descending memory order). No code currently
                // depends on this invariant, so it's fine to kill it if
                // needed.
                ep = &ht.hashTable[newHash];
                while (*ep && *ep > &entry)
                    ep = &(*ep)->chain;
                entry.chain = *ep;
                *ep = &entry;
            }
        }

        /*
         * Change the key of the front entry without calling Ops::hash on the
         * entry's current key. The caller must ensure that k has the same hash
         * code that the current key had when it was inserted.
         */
        void rekeyFrontWithSameHashCode(const Key &k) {
#ifdef DEBUG
            // Assert that k falls in the same hash bucket as the old key.
            HashNumber h = Ops::hash(k) >> ht.hashShift;
            Data *e = ht.hashTable[h];
            while (e && e != &ht.data[i])
                e = e->chain;
            JS_ASSERT(e == &ht.data[i]);
#endif
            Ops::setKey(ht.data[i].element, k);
        }
    };

    Range all() { return Range(*this); }

  private:
    /* Logarithm base 2 of the number of buckets in the hash table initially. */
    static uint32_t initialBucketsLog2() { return 1; }
    static uint32_t initialBuckets() { return 1 << initialBucketsLog2(); }

    /*
     * The maximum load factor (mean number of entries per bucket).
     * It is an invariant that
     *     dataCapacity == floor(hashBuckets() * fillFactor()).
     *
     * The fill factor should be between 2 and 4, and it should be chosen so that
     * the fill factor times sizeof(Data) is close to but <= a power of 2.
     * This fixed fill factor was chosen to make the size of the data
     * array, in bytes, close to a power of two when sizeof(T) is 16.
     */
    static double fillFactor() { return 8.0 / 3.0; }

    /*
     * The minimum permitted value of (liveCount / dataLength).
     * If that ratio drops below this value, we shrink the table.
     */
    static double minDataFill() { return 0.25; }

    static HashNumber prepareHash(const Lookup &l) {
        return ScrambleHashCode(Ops::hash(l));
    }

    /* The size of hashTable, in elements. Always a power of two. */
    uint32_t hashBuckets() const {
        return 1 << (HashNumberSizeBits - hashShift);
    }

    void freeData(Data *data, uint32_t length) {
        for (Data *p = data + length; p != data; )
            (--p)->~Data();
        alloc.free_(data);
    }

    Data *lookup(const Lookup &l, HashNumber h) {
        for (Data *e = hashTable[h >> hashShift]; e; e = e->chain) {
            if (Ops::match(Ops::getKey(e->element), l))
                return e;
        }
        return NULL;
    }

    const Data *lookup(const Lookup &l) const {
        return const_cast<OrderedHashTable *>(this)->lookup(l, prepareHash(l));
    }

    /* This is called after rehashing the table. */
    void compacted() {
        // If we had any empty entries, compacting may have moved live entries
        // to the left within |data|. Notify all live Ranges of the change.
        for (Range *r = ranges; r; r = r->next)
            r->onCompact();
    }

    /* Compact the entries in |data| and rehash them. */
    void rehashInPlace() {
        for (uint32_t i = 0, N = hashBuckets(); i < N; i++)
            hashTable[i] = NULL;
        Data *wp = data, *end = data + dataLength;
        for (Data *rp = data; rp != end; rp++) {
            if (!Ops::isEmpty(Ops::getKey(rp->element))) {
                HashNumber h = prepareHash(Ops::getKey(rp->element)) >> hashShift;
                if (rp != wp)
                    wp->element = Move(rp->element);
                wp->chain = hashTable[h];
                hashTable[h] = wp;
                wp++;
            }
        }
        MOZ_ASSERT(wp == data + liveCount);

        while (wp != end)
            (--end)->~Data();
        dataLength = liveCount;
        compacted();
    }

    /*
     * Grow, shrink, or compact both |hashTable| and |data|.
     *
     * On success, this returns true, dataLength == liveCount, and there are no
     * empty elements in data[0:dataLength]. On allocation failure, this
     * leaves everything as it was and returns false.
     */
    bool rehash(uint32_t newHashShift) {
        // If the size of the table is not changing, rehash in place to avoid
        // allocating memory.
        if (newHashShift == hashShift) {
            rehashInPlace();
            return true;
        }

        size_t newHashBuckets = 1 << (HashNumberSizeBits - newHashShift);
        Data **newHashTable = static_cast<Data **>(alloc.malloc_(newHashBuckets * sizeof(Data *)));
        if (!newHashTable)
            return false;
        for (uint32_t i = 0; i < newHashBuckets; i++)
            newHashTable[i] = NULL;

        uint32_t newCapacity = uint32_t(newHashBuckets * fillFactor());
        Data *newData = static_cast<Data *>(alloc.malloc_(newCapacity * sizeof(Data)));
        if (!newData) {
            alloc.free_(newHashTable);
            return false;
        }

        Data *wp = newData;
        for (Data *p = data, *end = data + dataLength; p != end; p++) {
            if (!Ops::isEmpty(Ops::getKey(p->element))) {
                HashNumber h = prepareHash(Ops::getKey(p->element)) >> newHashShift;
                new (wp) Data(Move(p->element), newHashTable[h]);
                newHashTable[h] = wp;
                wp++;
            }
        }
        MOZ_ASSERT(wp == newData + liveCount);

        alloc.free_(hashTable);
        freeData(data, dataLength);

        hashTable = newHashTable;
        data = newData;
        dataLength = liveCount;
        dataCapacity = newCapacity;
        hashShift = newHashShift;
        JS_ASSERT(hashBuckets() == newHashBuckets);

        compacted();
        return true;
    }

    // Not copyable.
    OrderedHashTable &operator=(const OrderedHashTable &) MOZ_DELETE;
    OrderedHashTable(const OrderedHashTable &) MOZ_DELETE;
};

}  // namespace detail

template <class Key, class Value, class OrderedHashPolicy, class AllocPolicy>
class OrderedHashMap
{
  public:
    class Entry
    {
        template <class, class, class> friend class detail::OrderedHashTable;
        void operator=(const Entry &rhs) {
            const_cast<Key &>(key) = rhs.key;
            value = rhs.value;
        }

        void operator=(MoveRef<Entry> rhs) {
            const_cast<Key &>(key) = Move(rhs->key);
            value = Move(rhs->value);
        }

      public:
        Entry() : key(), value() {}
        Entry(const Key &k, const Value &v) : key(k), value(v) {}
        Entry(MoveRef<Entry> rhs) : key(Move(rhs->key)), value(Move(rhs->value)) {}

        const Key key;
        Value value;
    };

  private:
    struct MapOps : OrderedHashPolicy
    {
        typedef Key KeyType;
        static void makeEmpty(Entry *e) {
            OrderedHashPolicy::makeEmpty(const_cast<Key *>(&e->key));

            // Clear the value. Destroying it is another possibility, but that
            // would complicate class Entry considerably.
            e->value = Value();
        }
        static const Key &getKey(const Entry &e) { return e.key; }
        static void setKey(Entry &e, const Key &k) { const_cast<Key &>(e.key) = k; }
    };

    typedef detail::OrderedHashTable<Entry, MapOps, AllocPolicy> Impl;
    Impl impl;

  public:
    typedef typename Impl::Range Range;

    OrderedHashMap(AllocPolicy ap = AllocPolicy()) : impl(ap) {}
    bool init()                                     { return impl.init(); }
    uint32_t count() const                          { return impl.count(); }
    bool has(const Key &key) const                  { return impl.has(key); }
    Range all()                                     { return impl.all(); }
    const Entry *get(const Key &key) const          { return impl.get(key); }
    Entry *get(const Key &key)                      { return impl.get(key); }
    bool put(const Key &key, const Value &value)    { return impl.put(Entry(key, value)); }
    bool remove(const Key &key, bool *foundp)       { return impl.remove(key, foundp); }
};

template <class T, class OrderedHashPolicy, class AllocPolicy>
class OrderedHashSet
{
  private:
    struct SetOps : OrderedHashPolicy
    {
        typedef const T KeyType;
        static const T &getKey(const T &v) { return v; }
        static void setKey(const T &e, const T &v) { const_cast<T &>(e) = v; }
    };

    typedef detail::OrderedHashTable<T, SetOps, AllocPolicy> Impl;
    Impl impl;

  public:
    typedef typename Impl::Range Range;

    OrderedHashSet(AllocPolicy ap = AllocPolicy()) : impl(ap) {}
    bool init()                                     { return impl.init(); }
    uint32_t count() const                          { return impl.count(); }
    bool has(const T &value) const                  { return impl.has(value); }
    Range all()                                     { return impl.all(); }
    bool put(const T &value)                        { return impl.put(value); }
    bool remove(const T &value, bool *foundp)       { return impl.remove(value, foundp); }
};

}  // namespace js


/*** HashableValue *******************************************************************************/

bool
HashableValue::setValue(JSContext *cx, const Value &v)
{
    if (v.isString() && v.toString()->isRope()) {
        // Flatten this rope so that equals() is infallible.
        JSString *str = v.toString()->ensureLinear(cx);
        if (!str)
            return false;
        value = StringValue(str);
    } else if (v.isDouble()) {
        double d = v.toDouble();
        int32_t i;
        if (MOZ_DOUBLE_IS_INT32(d, &i)) {
            // Normalize int32-valued doubles to int32 for faster hashing and testing.
            value = Int32Value(i);
        } else if (MOZ_DOUBLE_IS_NaN(d)) {
            // NaNs with different bits must hash and test identically.
            value = DoubleValue(js_NaN);
        } else {
            value = v;
        }
    } else {
        value = v;
    }

    JS_ASSERT(value.isUndefined() || value.isNull() || value.isBoolean() ||
              value.isNumber() || value.isString() || value.isObject());
    return true;
}

HashNumber
HashableValue::hash() const
{
    // HashableValue::setValue normalizes values so that the SameValue relation
    // on HashableValues is the same as the == relationship on
    // value.data.asBits, except for strings.
    if (value.isString()) {
        JSLinearString &s = value.toString()->asLinear();
        return HashChars(s.chars(), s.length());
    }

    // Having dispensed with strings, we can just hash asBits.
    uint64_t u = value.asRawBits();
    return HashNumber((u >> 3) ^
                      (u >> (HashNumberSizeBits + 3)) ^
                      (u << (HashNumberSizeBits - 3)));
}

bool
HashableValue::equals(const HashableValue &other) const
{
    // Two HashableValues are equal if they have equal bits or they're equal strings.
    bool b = (value.asRawBits() == other.value.asRawBits()) ||
              (value.isString() &&
               other.value.isString() &&
               EqualStrings(&value.toString()->asLinear(),
                            &other.value.toString()->asLinear()));

#ifdef DEBUG
    bool same;
    JS_ASSERT(SameValue(NULL, value, other.value, &same));
    JS_ASSERT(same == b);
#endif
    return b;
}

HashableValue
HashableValue::mark(JSTracer *trc) const
{
    HashableValue hv(*this);
    JS_SET_TRACING_LOCATION(trc, (void *)this);
    gc::MarkValue(trc, &hv.value, "key");
    return hv;
}


/*** MapIterator *********************************************************************************/

class js::MapIteratorObject : public JSObject {
  public:
    enum { TargetSlot, RangeSlot, SlotCount };
    static JSFunctionSpec methods[];
    static MapIteratorObject *create(JSContext *cx, HandleObject mapobj, ValueMap *data);
    static void finalize(FreeOp *fop, JSObject *obj);

  private:
    static inline bool is(const Value &v);
    inline ValueMap::Range *range();
    static bool next_impl(JSContext *cx, CallArgs args);
    static JSBool next(JSContext *cx, unsigned argc, Value *vp);
};

inline js::MapIteratorObject &
JSObject::asMapIterator()
{
    JS_ASSERT(isMapIterator());
    return *static_cast<js::MapIteratorObject *>(this);
}

Class js::MapIteratorClass = {
    "Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(MapIteratorObject::SlotCount),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    MapIteratorObject::finalize
};

JSFunctionSpec MapIteratorObject::methods[] = {
    JS_FN("next", next, 0, 0),
    JS_FS_END
};

inline ValueMap::Range *
MapIteratorObject::range()
{
    return static_cast<ValueMap::Range *>(getSlot(RangeSlot).toPrivate());
}

bool
GlobalObject::initMapIteratorProto(JSContext *cx, Handle<GlobalObject *> global)
{
    JSObject *base = global->getOrCreateIteratorPrototype(cx);
    if (!base)
        return false;
    Rooted<JSObject*> proto(cx,
        NewObjectWithGivenProto(cx, &MapIteratorClass, base, global));
    if (!proto)
        return false;
    proto->setSlot(MapIteratorObject::RangeSlot, PrivateValue(NULL));
    if (!JS_DefineFunctions(cx, proto, MapIteratorObject::methods))
        return false;
    global->setReservedSlot(MAP_ITERATOR_PROTO, ObjectValue(*proto));
    return true;
}

MapIteratorObject *
MapIteratorObject::create(JSContext *cx, HandleObject mapobj, ValueMap *data)
{
    Rooted<GlobalObject *> global(cx, &mapobj->global());
    Rooted<JSObject*> proto(cx, global->getOrCreateMapIteratorPrototype(cx));
    if (!proto)
        return false;

    ValueMap::Range *range = cx->new_<ValueMap::Range>(data->all());
    if (!range)
        return false;

    JSObject *iterobj = NewObjectWithGivenProto(cx, &MapIteratorClass, proto, global);
    if (!iterobj) {
        cx->delete_(range);
        return false;
    }
    iterobj->setSlot(TargetSlot, ObjectValue(*mapobj));
    iterobj->setSlot(RangeSlot, PrivateValue(range));
    return static_cast<MapIteratorObject *>(iterobj);
}

void
MapIteratorObject::finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_(obj->asMapIterator().range());
}

bool
MapIteratorObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&MapIteratorClass);
}

bool
MapIteratorObject::next_impl(JSContext *cx, CallArgs args)
{
    MapIteratorObject &thisobj = args.thisv().toObject().asMapIterator();
    ValueMap::Range *range = thisobj.range();
    if (!range)
        return js_ThrowStopIteration(cx);
    if (range->empty()) {
        cx->delete_(range);
        thisobj.setReservedSlot(RangeSlot, PrivateValue(NULL));
        return js_ThrowStopIteration(cx);
    }

    Value pair[2] = { range->front().key.get(), range->front().value };
    JSObject *pairobj = NewDenseCopiedArray(cx, 2, pair);
    if (!pairobj)
        return false;
    range->popFront();
    args.rval().setObject(*pairobj);
    return true;
}

JSBool
MapIteratorObject::next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, next_impl, args);
}


/*** Map *****************************************************************************************/

inline js::MapObject &
JSObject::asMap()
{
    JS_ASSERT(hasClass(&MapObject::class_));
    return *static_cast<js::MapObject *>(this);
}

Class MapObject::class_ = {
    "Map",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Map),
    JS_PropertyStub,         // addProperty
    JS_PropertyStub,         // delProperty
    JS_PropertyStub,         // getProperty
    JS_StrictPropertyStub,   // setProperty
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    // checkAccess
    NULL,                    // call
    NULL,                    // construct
    NULL,                    // hasInstance
    mark
};

JSFunctionSpec MapObject::methods[] = {
    JS_FN("size", size, 0, 0),
    JS_FN("get", get, 1, 0),
    JS_FN("has", has, 1, 0),
    JS_FN("set", set, 2, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FN("iterator", iterator, 0, 0),
    JS_FS_END
};

static JSObject *
InitClass(JSContext *cx, Handle<GlobalObject*> global, Class *clasp, JSProtoKey key, Native construct,
          JSFunctionSpec *methods)
{
    Rooted<JSObject*> proto(cx, global->createBlankPrototype(cx, clasp));
    if (!proto)
        return NULL;
    proto->setPrivate(NULL);

    JSAtom *atom = cx->runtime->atomState.classAtoms[key];
    Rooted<JSFunction*> ctor(cx, global->createConstructor(cx, construct, atom, 1));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndBrand(cx, proto, NULL, methods) ||
        !DefineConstructorAndPrototype(cx, global, key, ctor, proto))
    {
        return NULL;
    }
    return proto;
}

JSObject *
MapObject::initClass(JSContext *cx, JSObject *obj)
{
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());
    return InitClass(cx, global, &class_, JSProto_Map, construct, methods);
}

template <class Range>
static void
MarkKey(Range &r, const HashableValue &key, JSTracer *trc)
{
    HashableValue newKey = key.mark(trc);

    if (newKey.get() != key.get()) {
        if (newKey.get().isString()) {
            // GC moved a string. The key stored in the OrderedHashTable must
            // be updated to point to the string's new location, but rekeyFront
            // would not work because it would access the string's old
            // location.
            //
            // So as a specially gruesome hack, overwrite the key in place.
            // FIXME bug 769504.
            r.rekeyFrontWithSameHashCode(newKey);
        } else {
            // GC moved an object. It must be rekeyed, and rekeying is safe
            // because the old key's hash() method is still safe to call (it
            // does not access the object's old location).

            JS_ASSERT(newKey.get().isObject());
            r.rekeyFront(newKey);
        }
    }
}

void
MapObject::mark(JSTracer *trc, JSObject *obj)
{
    if (ValueMap *map = obj->asMap().getData()) {
        for (ValueMap::Range r = map->all(); !r.empty(); r.popFront()) {
            MarkKey(r, r.front().key, trc);
            gc::MarkValue(trc, &r.front().value, "value");
        }
    }
}

void
MapObject::finalize(FreeOp *fop, JSObject *obj)
{
    if (ValueMap *map = obj->asMap().getData())
        fop->delete_(map);
}

JSBool
MapObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    Rooted<JSObject*> obj(cx, NewBuiltinClassInstance(cx, &class_));
    if (!obj)
        return false;

    ValueMap *map = cx->new_<ValueMap>(cx->runtime);
    if (!map)
        return false;
    if (!map->init()) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    obj->setPrivate(map);

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.hasDefined(0)) {
        ForOfIterator iter(cx, args[0]);
        while (iter.next()) {
            JSObject *pairobj = js_ValueToNonNullObject(cx, iter.value());
            if (!pairobj)
                return false;

            Value key;
            if (!pairobj->getElement(cx, 0, &key))
                return false;
            HashableValue hkey;
            if (!hkey.setValue(cx, key))
                return false;

            HashableValue::AutoRooter hkeyRoot(cx, &hkey);

            Value val;
            if (!pairobj->getElement(cx, 1, &val))
                return false;

            RelocatableValue rval(val);
            if (!map->put(hkey, rval)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
        }
        if (!iter.close())
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

bool
MapObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&class_) && v.toObject().getPrivate();
}

#define ARG0_KEY(cx, args, key)                                               \
    HashableValue key;                                                        \
    if (args.length() > 0 && !key.setValue(cx, args[0]))                      \
        return false

ValueMap &
MapObject::extract(CallReceiver call)
{
    JS_ASSERT(call.thisv().isObject());
    JS_ASSERT(call.thisv().toObject().hasClass(&MapObject::class_));
    return *call.thisv().toObject().asMap().getData();
}

bool
MapObject::size_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(MapObject::is(args.thisv()));

    ValueMap &map = extract(args);
    JS_STATIC_ASSERT(sizeof map.count() <= sizeof(uint32_t));
    args.rval().setNumber(map.count());
    return true;
}

JSBool
MapObject::size(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, size_impl, args);
}

bool
MapObject::get_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(MapObject::is(args.thisv()));

    ValueMap &map = extract(args);
    ARG0_KEY(cx, args, key);
    if (ValueMap::Entry *p = map.get(key))
        args.rval() = p->value;
    else
        args.rval().setUndefined();
    return true;
}

JSBool
MapObject::get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, get_impl, args);
}

bool
MapObject::has_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(MapObject::is(args.thisv()));

    ValueMap &map = extract(args);
    ARG0_KEY(cx, args, key);
    args.rval().setBoolean(map.has(key));
    return true;
}

JSBool
MapObject::has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, has_impl, args);
}

bool
MapObject::set_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(MapObject::is(args.thisv()));

    ValueMap &map = extract(args);
    ARG0_KEY(cx, args, key);
    RelocatableValue rval(args.length() > 1 ? args[1] : UndefinedValue());
    if (!map.put(key, rval)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    args.rval().setUndefined();
    return true;
}

JSBool
MapObject::set(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, set_impl, args);
}

bool
MapObject::delete_impl(JSContext *cx, CallArgs args)
{
    // MapObject::mark does not mark deleted entries. Incremental GC therefore
    // requires that no RelocatableValue objects pointing to heap values be
    // left alive in the ValueMap.
    //
    // OrderedHashMap::remove() doesn't destroy the removed entry. It merely
    // calls OrderedHashMap::MapOps::makeEmpty. But that is sufficient, because
    // makeEmpty clears the value by doing e->value = Value(), and in the case
    // of a ValueMap, Value() means RelocatableValue(), which is the same as
    // RelocatableValue(UndefinedValue()).
    JS_ASSERT(MapObject::is(args.thisv()));

    ValueMap &map = extract(args);
    ARG0_KEY(cx, args, key);
    bool found;
    if (!map.remove(key, &found))
        return false;
    args.rval().setBoolean(found);
    return true;
}

JSBool
MapObject::delete_(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, delete_impl, args);
}

bool
MapObject::iterator_impl(JSContext *cx, CallArgs args)
{
    Rooted<MapObject*> mapobj(cx, &args.thisv().toObject().asMap());
    ValueMap &map = *mapobj->getData();
    Rooted<JSObject*> iterobj(cx, MapIteratorObject::create(cx, mapobj, &map));
    if (!iterobj)
        return false;
    args.rval().setObject(*iterobj);
    return true;
}

JSBool
MapObject::iterator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, iterator_impl, args);
}

JSObject *
js_InitMapClass(JSContext *cx, JSObject *obj)
{
    return MapObject::initClass(cx, obj);
}


/*** SetIterator *********************************************************************************/

class js::SetIteratorObject : public JSObject {
  public:
    enum { TargetSlot, RangeSlot, SlotCount };
    static JSFunctionSpec methods[];
    static SetIteratorObject *create(JSContext *cx, HandleObject setobj, ValueSet *data);
    static void finalize(FreeOp *fop, JSObject *obj);

  private:
    static inline bool is(const Value &v);
    inline ValueSet::Range *range();
    static bool next_impl(JSContext *cx, CallArgs args);
    static JSBool next(JSContext *cx, unsigned argc, Value *vp);
};

inline js::SetIteratorObject &
JSObject::asSetIterator()
{
    JS_ASSERT(isSetIterator());
    return *static_cast<js::SetIteratorObject *>(this);
}

Class js::SetIteratorClass = {
    "Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(SetIteratorObject::SlotCount),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    SetIteratorObject::finalize
};

JSFunctionSpec SetIteratorObject::methods[] = {
    JS_FN("next", next, 0, 0),
    JS_FS_END
};

inline ValueSet::Range *
SetIteratorObject::range()
{
    return static_cast<ValueSet::Range *>(getSlot(RangeSlot).toPrivate());
}

bool
GlobalObject::initSetIteratorProto(JSContext *cx, Handle<GlobalObject*> global)
{
    JSObject *base = global->getOrCreateIteratorPrototype(cx);
    if (!base)
        return false;
    JSObject *proto = NewObjectWithGivenProto(cx, &SetIteratorClass, base, global);
    if (!proto)
        return false;
    proto->setSlot(SetIteratorObject::RangeSlot, PrivateValue(NULL));
    if (!JS_DefineFunctions(cx, proto, SetIteratorObject::methods))
        return false;
    global->setReservedSlot(SET_ITERATOR_PROTO, ObjectValue(*proto));
    return true;
}

SetIteratorObject *
SetIteratorObject::create(JSContext *cx, HandleObject setobj, ValueSet *data)
{
    Rooted<GlobalObject *> global(cx, &setobj->global());
    Rooted<JSObject*> proto(cx, global->getOrCreateSetIteratorPrototype(cx));
    if (!proto)
        return false;

    ValueSet::Range *range = cx->new_<ValueSet::Range>(data->all());
    if (!range)
        return false;

    JSObject *iterobj = NewObjectWithGivenProto(cx, &SetIteratorClass, proto, global);
    if (!iterobj) {
        cx->delete_(range);
        return false;
    }
    iterobj->setSlot(TargetSlot, ObjectValue(*setobj));
    iterobj->setSlot(RangeSlot, PrivateValue(range));
    return static_cast<SetIteratorObject *>(iterobj);
}

void
SetIteratorObject::finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_(obj->asSetIterator().range());
}

bool
SetIteratorObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&SetIteratorClass);
}

bool
SetIteratorObject::next_impl(JSContext *cx, CallArgs args)
{
    SetIteratorObject &thisobj = args.thisv().toObject().asSetIterator();
    ValueSet::Range *range = thisobj.range();
    if (!range)
        return js_ThrowStopIteration(cx);
    if (range->empty()) {
        cx->delete_(range);
        thisobj.setReservedSlot(RangeSlot, PrivateValue(NULL));
        return js_ThrowStopIteration(cx);
    }

    args.rval() = range->front().get();
    range->popFront();
    return true;
}

JSBool
SetIteratorObject::next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, next_impl, args);
}


/*** Set *****************************************************************************************/

inline js::SetObject &
JSObject::asSet()
{
    JS_ASSERT(hasClass(&SetObject::class_));
    return *static_cast<js::SetObject *>(this);
}

Class SetObject::class_ = {
    "Set",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Set),
    JS_PropertyStub,         // addProperty
    JS_PropertyStub,         // delProperty
    JS_PropertyStub,         // getProperty
    JS_StrictPropertyStub,   // setProperty
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    // checkAccess
    NULL,                    // call
    NULL,                    // construct
    NULL,                    // hasInstance
    mark
};

JSFunctionSpec SetObject::methods[] = {
    JS_FN("size", size, 0, 0),
    JS_FN("has", has, 1, 0),
    JS_FN("add", add, 1, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FN("iterator", iterator, 0, 0),
    JS_FS_END
};

JSObject *
SetObject::initClass(JSContext *cx, JSObject *obj)
{
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());
    return InitClass(cx, global, &class_, JSProto_Set, construct, methods);
}

void
SetObject::mark(JSTracer *trc, JSObject *obj)
{
    SetObject *setobj = static_cast<SetObject *>(obj);
    if (ValueSet *set = setobj->getData()) {
        for (ValueSet::Range r = set->all(); !r.empty(); r.popFront())
            MarkKey(r, r.front(), trc);
    }
}

void
SetObject::finalize(FreeOp *fop, JSObject *obj)
{
    SetObject *setobj = static_cast<SetObject *>(obj);
    if (ValueSet *set = setobj->getData())
        fop->delete_(set);
}

JSBool
SetObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    Rooted<JSObject*> obj(cx, NewBuiltinClassInstance(cx, &class_));
    if (!obj)
        return false;

    ValueSet *set = cx->new_<ValueSet>(cx->runtime);
    if (!set)
        return false;
    if (!set->init()) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    obj->setPrivate(set);

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.hasDefined(0)) {
        ForOfIterator iter(cx, args[0]);
        while (iter.next()) {
            HashableValue key;
            if (!key.setValue(cx, iter.value()))
                return false;
            if (!set->put(key)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
        }
        if (!iter.close())
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

bool
SetObject::is(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&class_) && v.toObject().getPrivate();
}

ValueSet &
SetObject::extract(CallReceiver call)
{
    JS_ASSERT(call.thisv().isObject());
    JS_ASSERT(call.thisv().toObject().hasClass(&SetObject::class_));
    return *static_cast<SetObject&>(call.thisv().toObject()).getData();
}

bool
SetObject::size_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    ValueSet &set = extract(args);
    JS_STATIC_ASSERT(sizeof set.count() <= sizeof(uint32_t));
    args.rval().setNumber(set.count());
    return true;
}

JSBool
SetObject::size(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, size_impl, args);
}

bool
SetObject::has_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    ValueSet &set = extract(args);
    ARG0_KEY(cx, args, key);
    args.rval().setBoolean(set.has(key));
    return true;
}

JSBool
SetObject::has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, has_impl, args);
}

bool
SetObject::add_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    ValueSet &set = extract(args);
    ARG0_KEY(cx, args, key);
    if (!set.put(key)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    args.rval().setUndefined();
    return true;
}

JSBool
SetObject::add(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, add_impl, args);
}

bool
SetObject::delete_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(is(args.thisv()));

    ValueSet &set = extract(args);
    ARG0_KEY(cx, args, key);
    bool found;
    if (!set.remove(key, &found))
        return false;
    args.rval().setBoolean(found);
    return true;
}

JSBool
SetObject::delete_(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, delete_impl, args);
}

bool
SetObject::iterator_impl(JSContext *cx, CallArgs args)
{
    Rooted<SetObject*> setobj(cx, &args.thisv().toObject().asSet());
    ValueSet &set = *setobj->getData();
    Rooted<JSObject*> iterobj(cx, SetIteratorObject::create(cx, setobj, &set));
    if (!iterobj)
        return false;
    args.rval().setObject(*iterobj);
    return true;
}

JSBool
SetObject::iterator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, is, iterator_impl, args);
}

JSObject *
js_InitSetClass(JSContext *cx, JSObject *obj)
{
    return SetObject::initClass(cx, obj);
}
