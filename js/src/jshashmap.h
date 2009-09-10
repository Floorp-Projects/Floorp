/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * July 17, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Luke Wagner <lw@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jshashmap_h_
#define jshashmap_h_

#include "jspool.h"
#include "jsbit.h"

namespace js {

/* Default hashing policies. */
template <class Key> struct DefaultHasher {
    static uint32 hash(const Key &key) {
        /* hash if can implicitly cast to hash number type. */
        return key;
    }
};
template <class T> struct DefaultHasher<T *> {
    static uint32 hash(const T *key) {
        /* hash pointers like JS_DHashVoidPtrKeyStub. */
        return (uint32)(unsigned long)key >> 2;
    }
};

/*
 * JS-friendly, STL-like container providing a hash table with parameterized
 * error handling. HashMap calls the constructors/destructors of all key/value
 * objects added, so non-PODs may be used safely.
 *
 * Key/Value requirements:
 *  - default constructible, destructible, assignable, copy constructible
 *  - operations do not throw
 * Hasher requirements:
 *  - static member function: static HashNumber hash(const Key &);
 * N:
 *  - specifies the number of elements to store in-line before the first
 *    dynamic allocation. (default 0)
 * AllocPolicy:
 *  - see "Allocation policies" in jstl.h (default ContextAllocPolicy)
 *
 * N.B: HashMap is not reentrant: T member functions called during HashMap
 * member functions must not call back into the same object.
 */
template <class Key, class Value, class Hasher,
          size_t N, class AllocPolicy>
class HashMap
{
  public:
    /* public element interface */
    struct Element
    {
        const Key   key;
        Value       value;
    };

  private:
    /* utilities */
    void initTable();
    void destroyAll();

    typedef uint32 HashNumber;

    struct HashEntry : Element
    {
        HashEntry   *next;
        HashNumber  keyHash;
    };

    struct InternalRange;
    InternalRange internalAll() const;

    /* share the AllocPolicy object stored in the Pool */
    void *malloc(size_t bytes) { return entryPool.allocPolicy().malloc(bytes); }
    void free(void *p) { entryPool.allocPolicy().free(p); }
    void reportAllocOverflow() { entryPool.allocPolicy().reportAllocOverflow(); }

    /* magic constants */
    static const size_t sHashBits = 32;

    /* compute constants */

    /*
     * Since the goal is to not resize for the first N entries, increase the
     * table size according so that 'overloaded(N-1)' is false.
     */
    static const size_t sInlineCount =
        tl::Max<2, tl::RoundUpPow2<(8 * N) / 7>::result>::result;

    static const size_t sInlineTableShift =
        sHashBits - tl::FloorLog2<sInlineCount>::result;

    static const size_t sTableElemBytes =
        sizeof(HashEntry *);

    static const size_t sInlineBytes =
        sInlineCount * sTableElemBytes;

    static const uint32 sGoldenRatio = 0x9E3779B9U; /* taken from jshash.h */

    static const size_t sMinHashShrinkLimit =
        tl::Max<64, sInlineCount>::result;

    /* member data */

    /* number of elements that have been added to the hash table */
    size_t numElem;

    /* size of table = size_t(1) << (sHashBits - shift) */
    uint32 shift;

    /* inline storage for the first |sInlineCount| buckets */
    char tableBuf[sInlineBytes];

    /* pointer to array of buckets */
    HashEntry **table;

    /* pool of hash table elements */
    Pool<HashEntry, N, AllocPolicy> entryPool;

#ifdef DEBUG
    friend class ReentrancyGuard;
    bool entered;
#endif

    /* more utilities */

    uint32 tableCapacity() const {
        JS_ASSERT(shift > 0 && shift <= sHashBits);
        return uint32(1) << (sHashBits - shift);
    }

    bool overloaded(size_t tableCapacity) const {
        /* lifted from OVERLOADED macro in jshash.cpp */
        return numElem >= (tableCapacity - (tableCapacity >> 3));
    }

    bool underloaded(size_t tableCapacity) const {
        /* lifted from UNDERLOADED macro in jshash.cpp */
        return numElem < (tableCapacity >> 2) &&
               tableCapacity > sMinHashShrinkLimit;
    }

    HashEntry **hashToBucket(HashNumber hn) const {
        JS_ASSERT(shift > 0 && shift < sHashBits);
        return table + ((hn * sGoldenRatio) >> shift);
    }

  public:
    /* construction / destruction */

    HashMap(AllocPolicy = AllocPolicy());
    ~HashMap();

    HashMap(const HashMap &);
    HashMap &operator=(const HashMap &);

    /*
     * Type representing a pointer either to an element (if !null()) or a
     * pointer to where an element might be inserted (if null()). Pointers
     * become invalid after every HashMap operation (even lookup).
     */
    class Pointer {
        typedef void (Pointer::* ConvertibleToBool)();

        friend class HashMap;
        void nonNull() {}
        Pointer(HashEntry **e, HashNumber h) : hepp(e), keyHash(h) {}

        HashEntry **hepp;
        HashNumber keyHash;

      public:
        typedef Element ElementType;

        bool operator==(const Pointer &rhs) const  { return *hepp == *rhs.hepp; }
        bool operator!=(const Pointer &rhs) const  { return *hepp != *rhs.hepp; }

        bool null() const                          { return !*hepp; }
        operator ConvertibleToBool();

        /* dereference; assumes non-null */
        ElementType &operator*() const                 { return **hepp; }
        ElementType *operator->() const                { return *hepp; }
    };

    /* Type representing a range of elements in the hash table. */
    class Range {
        friend class HashMap;
        Range(HashEntry *hep, HashEntry **te, HashEntry **end)
          : hep(hep), tableEntry(te), tableEnd(end) {}

        HashEntry *hep, **tableEntry, **tableEnd;

      public:
        typedef Element ElementType;

        /* !empty() is a precondition for calling front() and popFront(). */
        bool empty() const              { return tableEntry == tableEnd; }
        ElementType &front() const      { return *hep; }
        void popFront();
    };

    /* enumeration: all() returns a Range containing count() elements. */
    Range all() const;
    bool empty() const { return numElem == 0; }
    size_t count() const { return numElem; }

    /*
     * lookup: query whether there is a (key,value) pair with the given key in
     * the table. The 'keyHash' overload allows the user to use a pre-computed
     * hash for the given key. If |p = lookup(k)| and |p.null()|, then
     * |addAfterMiss(k,v,p)| may be called to efficiently add (k,v).
     */
    Pointer lookup(const Key &k) const;
    Pointer lookup(const Key &k, HashNumber keyHash) const;

    /*
     * put: put the given (key,value) pair to the table, returning 'true' if
     * the operation succeeded. When the given key is already present, the
     * existing value will be overwritten and the call will succeed. The
     * 'keyHash' overload allows the user to supply a pre-computed hash for the
     * given key. The 'addAfterMiss' overload preserves 'ptr' so that, if the
     * call succeeds, 'ptr' points to the newly-added (key,value) pair.
     */
    bool put(const Key &k, const Value &v);
    bool put(const Key &k, const Value &v, HashNumber keyHash);
    bool addAfterMiss(const Key &k, const Value &v, Pointer &ptr);

    /*
     * lookup and put: analogous to the std::map operation. Lookup the given
     * key and, if found, return a pointer to the value. Otherwise, construct a
     * new (key, value) pair with the given key and default-constructed value
     * and return a pointer to the value. Return NULL on failure.
     */
    Value *findOrAdd(const Key &k);

    /*
     * remove: remove a (key,value) pair with the given key, if it exists. The
     * 'keyHash' overload allows the user to supply a pre-computed hash for the
     * given key.
     */
    void remove(const Key &k);
    void remove(const Key &k, HashNumber);
    void remove(Pointer);

    /*
     * Remove all elements, optionally freeing the underlying memory cache.
     *
     * N.B. for PODs, freeing the underlying cache is more efficient (O(1))
     * since the underlying pool can be freed as a whole instead of freeing
     * each individual element's allocation (O(n)).
     */
    void clear(bool freeCache = true);
};

/* Implementation */

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::initTable()
{
    shift = sInlineTableShift;
    table = reinterpret_cast<HashEntry **>(tableBuf);
    for (HashEntry **p = table, **end = table + sInlineCount; p != end; ++p)
        *p = NULL;
}

template <class K, class V, class H, size_t N, class AP>
inline
HashMap<K,V,H,N,AP>::HashMap(AP ap)
  : numElem(0),
    entryPool(ap)
#ifdef DEBUG
    , entered(false)
#endif
{
    initTable();
}

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::destroyAll()
{
    /* Non-PODs need explicit destruction. */
    if (!tl::IsPodType<K>::result || !tl::IsPodType<V>::result)
        entryPool.clear(internalAll());
    else
        entryPool.freeRawMemory();

    /* Free memory if memory was allocated. */
    if ((void *)table != (void *)tableBuf)
        this->free(table);
}

template <class K, class V, class H, size_t N, class AP>
inline
HashMap<K,V,H,N,AP>::~HashMap()
{
    ReentrancyGuard g(*this);
    destroyAll();
}

template <class K, class V, class H, size_t N, class AP>
inline
HashMap<K,V,H,N,AP>::Pointer::operator ConvertibleToBool()
{
    return null() ? NULL : &Pointer::nonNull;
}

template <class K, class V, class H, size_t N, class AP>
inline typename HashMap<K,V,H,N,AP>::InternalRange
HashMap<K,V,H,N,AP>::internalAll() const
{
    HashEntry **p = table, **end = table + tableCapacity();
    while (p != end && !*p)
        ++p;

    /* For a clean crash on misuse, set hepp to NULL for empty ranges. */
    return Range(p == end ? NULL : *p, p, end);
}

template <class K, class V, class H, size_t N, class AP>
inline typename HashMap<K,V,H,N,AP>::Range
HashMap<K,V,H,N,AP>::all() const
{
    JS_ASSERT(!entered);
    return internalAll();
}

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::Range::popFront()
{
    if ((hep = hep->next))
        return;
    ++tableEntry;
    for (; tableEntry != tableEnd; ++tableEntry) {
        if ((hep = *tableEntry))
            return;
    }
    hep = NULL;
}

template <class K, class V, class H, size_t N, class AP>
inline typename HashMap<K,V,H,N,AP>::Pointer
HashMap<K,V,H,N,AP>::lookup(const K &k, HashNumber hash) const
{
    JS_ASSERT(!entered);
    HashEntry **bucket = hashToBucket(hash), **p = bucket;

    /* search bucket for match */
    for (HashEntry *e = *p; e; p = &e->next, e = *p) {
        if (e->keyHash == hash && k == e->key) {
            /* Move to front of chain */
            *p = e->next;
            e->next = *bucket;
            *bucket = e;
            return Pointer(bucket, hash);
        }
    }

    /* miss, return insertion point */
    return Pointer(p, hash);
}

template <class K, class V, class H, size_t N, class AP>
inline typename HashMap<K,V,H,N,AP>::Pointer
HashMap<K,V,H,N,AP>::lookup(const K &k) const
{
    return lookup(k, H::hash(k));
}

template <class K, class V, class H, size_t N, class AP>
inline bool
HashMap<K,V,H,N,AP>::addAfterMiss(const K &k, const V &v, Pointer &ptr)
{
    ReentrancyGuard g(*this);
    JS_ASSERT(ptr.null());

    /* Resize on overflow. */
    uint32 cap = tableCapacity();
    if (overloaded(cap)) {
        /* Compute bytes to allocate, checking for overflow. */
        if (shift <= 1 + tl::CeilingLog2<sTableElemBytes>::result) {
            this->reportAllocOverflow();
            return false;
        }
        size_t bytes = cap * 2 * sTableElemBytes;

        /* Allocate/clear new table. */
        HashEntry **newTable = (HashEntry **)this->malloc(bytes);
        if (!newTable)
            return false;
        memset(newTable, NULL, bytes);

        /* Swap in new table before calling hashToBucket. */
        HashEntry **oldTable = table;
        table = newTable;
        --shift;
        JS_ASSERT(shift > 0);

        /* Insert old table into new. */
        for (HashEntry **p = oldTable, **end = oldTable + cap; p != end; ++p) {
            for (HashEntry *e = *p, *next; e; e = next) {
                next = e->next;
                HashEntry **dstBucket = hashToBucket(e->keyHash);
                e->next = *dstBucket;
                *dstBucket = e;
            }
        }

        /* Free old table. */
        if ((void *)oldTable != (void *)tableBuf) {
            /* Table was dynamically allocated, so free. */
            this->free(oldTable);
        } else {
            /* Table was inline buffer, so recycle buffer into pool. */
            size_t usable = sInlineBytes - sInlineBytes % sizeof(HashEntry);
            entryPool.lendUnusedMemory(tableBuf, tableBuf + usable);
        }

        /* Maintain 'ptr'. */
        ptr.hepp = hashToBucket(ptr.keyHash);
    }

    /* Allocate and insert new hash entry. */
    HashEntry *alloc = entryPool.create();
    if (!alloc)
        return false;
    const_cast<K &>(alloc->key) = k;
    alloc->value = v;
    alloc->keyHash = ptr.keyHash;
    alloc->next = *ptr.hepp;  /* Could be nonnull after table realloc. */
    *ptr.hepp = alloc;
    ++numElem;

    return true;
}

template <class K, class V, class H, size_t N, class AP>
inline bool
HashMap<K,V,H,N,AP>::put(const K &k, const V &v, HashNumber hn)
{
    JS_ASSERT(!entered);
    Pointer p = lookup(k, hn);
    if (p.null())
        return addAfterMiss(k, v, p);
    p->value = v;
    return true;
}

template <class K, class V, class H, size_t N, class AP>
inline bool
HashMap<K,V,H,N,AP>::put(const K &k, const V &v)
{
    return put(k, v, H::hash(k));
}

template <class K, class V, class H, size_t N, class AP>
inline V *
HashMap<K,V,H,N,AP>::findOrAdd(const K &k)
{
    JS_ASSERT(!entered);
    Pointer p = lookup(k);
    if (p.null() && !addAfterMiss(k, V(), p))
        return NULL;
    JS_ASSERT(!p.null());
    return &p->value;
}

/*
 * The static return type of r.front() is Element, even though we know that the
 * elements are actually HashEntry objects. This class makes the cast.
 */
template <class K, class V, class H, size_t N, class AP>
struct HashMap<K,V,H,N,AP>::InternalRange : Range {
    InternalRange(const Range &r) : Range(r) {}
    HashEntry &front() { return static_cast<HashEntry &>(Range::front()); }
};

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::remove(Pointer p)
{
    ReentrancyGuard g(*this);

    /* Remove entry */
    HashEntry *e = *p.hepp;
    *p.hepp = e->next;
    --numElem;
    entryPool.destroy(e);
    size_t cap = tableCapacity();

    if (underloaded(cap)) {
        /* This function is infalliable; so leave HashMap unmodified on fail. */
        JS_ASSERT((void *)table != (void *)tableBuf);

        /* Allocate new table or go back to inline buffer. */
        size_t newCap = cap >> 1;
        size_t tableBytes;
        HashEntry **newTable;
        if (newCap <= sInlineCount) {
            newCap = sInlineCount;
            tableBytes = sInlineBytes;
            newTable = (HashEntry **)tableBuf;
        } else {
            tableBytes = newCap * sTableElemBytes;
            newTable = (HashEntry **)this->malloc(tableBytes);
            if (!newTable)
                return;
        }

        /* Consolidate elements into new contiguous pool buffer. */
        InternalRange r = internalAll();
        HashEntry *array = static_cast<HashEntry *>(
                                entryPool.consolidate(r, numElem));
        if (!array) {
            if ((void *)newTable != (void *)tableBuf)
                this->free(newTable);
            return;
        }

        /* Not going to fail so update state now, before calling hashToBucket. */
        this->free(table);
        table = newTable;
        ++shift;
        memset(newTable, NULL, tableBytes);

        /* Fill the new table. */
        for (HashEntry *p = array, *end = array + numElem; p != end; ++p) {
            HashEntry **dstBucket = hashToBucket(p->keyHash);
            p->next = *dstBucket;
            *dstBucket = p;
        }
    }
}

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::remove(const K &k, HashNumber hash)
{
    JS_ASSERT(!entered);
    if (Pointer p = lookup(k, hash))
        remove(p);
}

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::remove(const K &k)
{
    return remove(k, H::hash(k));
}

template <class K, class V, class H, size_t N, class AP>
inline void
HashMap<K,V,H,N,AP>::clear(bool freeCache)
{
    ReentrancyGuard g(*this);
    destroyAll();
    numElem = 0;
    initTable();
}

}  /* namespace js */

#endif
