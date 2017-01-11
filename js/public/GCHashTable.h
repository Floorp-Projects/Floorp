/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GCHashTable_h
#define GCHashTable_h

#include "js/GCPolicyAPI.h"
#include "js/HashTable.h"
#include "js/RootingAPI.h"
#include "js/SweepingAPI.h"
#include "js/TracingAPI.h"

namespace JS {

// Define a reasonable default GC policy for GC-aware Maps.
template <typename Key, typename Value>
struct DefaultMapSweepPolicy {
    static bool needsSweep(Key* key, Value* value) {
        return GCPolicy<Key>::needsSweep(key) || GCPolicy<Value>::needsSweep(value);
    }
};

// A GCHashMap is a GC-aware HashMap, meaning that it has additional trace and
// sweep methods that know how to visit all keys and values in the table.
// HashMaps that contain GC pointers will generally want to use this GCHashMap
// specialization instead of HashMap, because this conveniently supports tracing
// keys and values, and cleaning up weak entries.
//
// GCHashMap::trace applies GCPolicy<T>::trace to each entry's key and value.
// Most types of GC pointers already have appropriate specializations of
// GCPolicy, so they should just work as keys and values. Any struct type with a
// default constructor and trace and sweep functions should work as well. If you
// need to define your own GCPolicy specialization, generic helpers can be found
// in js/public/TracingAPI.h.
//
// The MapSweepPolicy template parameter controls how the table drops entries
// when swept. GCHashMap::sweep applies MapSweepPolicy::needsSweep to each table
// entry; if it returns true, the entry is dropped. The default MapSweepPolicy
// drops the entry if either the key or value is about to be finalized,
// according to its GCPolicy<T>::needsSweep method. (This default is almost
// always fine: it's hard to imagine keeping such an entry around anyway.)
//
// Note that this HashMap only knows *how* to trace and sweep, but it does not
// itself cause tracing or sweeping to be invoked. For tracing, it must be used
// with Rooted or PersistentRooted, or barriered and traced manually. For
// sweeping, currently it requires an explicit call to <map>.sweep().
template <typename Key,
          typename Value,
          typename HashPolicy = js::DefaultHasher<Key>,
          typename AllocPolicy = js::TempAllocPolicy,
          typename MapSweepPolicy = DefaultMapSweepPolicy<Key, Value>>
class GCHashMap : public js::HashMap<Key, Value, HashPolicy, AllocPolicy>
{
    using Base = js::HashMap<Key, Value, HashPolicy, AllocPolicy>;

  public:
    explicit GCHashMap(AllocPolicy a = AllocPolicy()) : Base(a)  {}

    static void trace(GCHashMap* map, JSTracer* trc) { map->trace(trc); }
    void trace(JSTracer* trc) {
        if (!this->initialized())
            return;
        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            GCPolicy<Value>::trace(trc, &e.front().value(), "hashmap value");
            GCPolicy<Key>::trace(trc, &e.front().mutableKey(), "hashmap key");
        }
    }

    void sweep() {
        if (!this->initialized())
            return;

        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            if (MapSweepPolicy::needsSweep(&e.front().mutableKey(), &e.front().value()))
                e.removeFront();
        }
    }

    // GCHashMap is movable
    GCHashMap(GCHashMap&& rhs) : Base(mozilla::Move(rhs)) {}
    void operator=(GCHashMap&& rhs) {
        MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
        Base::operator=(mozilla::Move(rhs));
    }

  private:
    // GCHashMap is not copyable or assignable
    GCHashMap(const GCHashMap& hm) = delete;
    GCHashMap& operator=(const GCHashMap& hm) = delete;
};

} // namespace JS

namespace js {

// HashMap that supports rekeying.
//
// If your keys are pointers to something like JSObject that can be tenured or
// compacted, prefer to use GCHashMap with MovableCellHasher, which takes
// advantage of the Zone's stable id table to make rekeying unnecessary.
template <typename Key,
          typename Value,
          typename HashPolicy = DefaultHasher<Key>,
          typename AllocPolicy = TempAllocPolicy,
          typename MapSweepPolicy = JS::DefaultMapSweepPolicy<Key, Value>>
class GCRekeyableHashMap : public JS::GCHashMap<Key, Value, HashPolicy, AllocPolicy, MapSweepPolicy>
{
    using Base = JS::GCHashMap<Key, Value, HashPolicy, AllocPolicy>;

  public:
    explicit GCRekeyableHashMap(AllocPolicy a = AllocPolicy()) : Base(a)  {}

    void sweep() {
        if (!this->initialized())
            return;

        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            Key key(e.front().key());
            if (MapSweepPolicy::needsSweep(&key, &e.front().value()))
                e.removeFront();
            else if (!HashPolicy::match(key, e.front().key()))
                e.rekeyFront(key);
        }
    }

    // GCRekeyableHashMap is movable
    GCRekeyableHashMap(GCRekeyableHashMap&& rhs) : Base(mozilla::Move(rhs)) {}
    void operator=(GCRekeyableHashMap&& rhs) {
        MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
        Base::operator=(mozilla::Move(rhs));
    }
};

template <typename Wrapper, typename... Args>
class WrappedPtrOperations<JS::GCHashMap<Args...>, Wrapper>
{
    using Map = JS::GCHashMap<Args...>;
    using Lookup = typename Map::Lookup;

    const Map& map() const { return static_cast<const Wrapper*>(this)->get(); }

  public:
    using AddPtr = typename Map::AddPtr;
    using Ptr = typename Map::Ptr;
    using Range = typename Map::Range;

    bool initialized() const                   { return map().initialized(); }
    Ptr lookup(const Lookup& l) const          { return map().lookup(l); }
    AddPtr lookupForAdd(const Lookup& l) const { return map().lookupForAdd(l); }
    Range all() const                          { return map().all(); }
    bool empty() const                         { return map().empty(); }
    uint32_t count() const                     { return map().count(); }
    size_t capacity() const                    { return map().capacity(); }
    bool has(const Lookup& l) const            { return map().lookup(l).found(); }
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return map().sizeOfExcludingThis(mallocSizeOf);
    }
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(this) + map().sizeOfExcludingThis(mallocSizeOf);
    }
};

template <typename Wrapper, typename... Args>
class MutableWrappedPtrOperations<JS::GCHashMap<Args...>, Wrapper>
  : public WrappedPtrOperations<JS::GCHashMap<Args...>, Wrapper>
{
    using Map = JS::GCHashMap<Args...>;
    using Lookup = typename Map::Lookup;

    Map& map() { return static_cast<Wrapper*>(this)->get(); }

  public:
    using AddPtr = typename Map::AddPtr;
    struct Enum : public Map::Enum { explicit Enum(Wrapper& o) : Map::Enum(o.map()) {} };
    using Ptr = typename Map::Ptr;
    using Range = typename Map::Range;

    bool init(uint32_t len = 16) { return map().init(len); }
    void clear()                 { map().clear(); }
    void finish()                { map().finish(); }
    void remove(Ptr p)           { map().remove(p); }

    template<typename KeyInput, typename ValueInput>
    bool add(AddPtr& p, KeyInput&& k, ValueInput&& v) {
        return map().add(p, mozilla::Forward<KeyInput>(k), mozilla::Forward<ValueInput>(v));
    }

    template<typename KeyInput>
    bool add(AddPtr& p, KeyInput&& k) {
        return map().add(p, mozilla::Forward<KeyInput>(k), Map::Value());
    }

    template<typename KeyInput, typename ValueInput>
    bool relookupOrAdd(AddPtr& p, KeyInput&& k, ValueInput&& v) {
        return map().relookupOrAdd(p, k,
                                   mozilla::Forward<KeyInput>(k),
                                   mozilla::Forward<ValueInput>(v));
    }

    template<typename KeyInput, typename ValueInput>
    bool put(KeyInput&& k, ValueInput&& v) {
        return map().put(mozilla::Forward<KeyInput>(k), mozilla::Forward<ValueInput>(v));
    }

    template<typename KeyInput, typename ValueInput>
    bool putNew(KeyInput&& k, ValueInput&& v) {
        return map().putNew(mozilla::Forward<KeyInput>(k), mozilla::Forward<ValueInput>(v));
    }
};

} // namespace js

namespace JS {

// A GCHashSet is a HashSet with an additional trace method that knows
// be traced to be kept alive will generally want to use this GCHashSet
// specialization in lieu of HashSet.
//
// Most types of GC pointers can be traced with no extra infrastructure. For
// structs and non-gc-pointer members, ensure that there is a specialization of
// GCPolicy<T> with an appropriate trace method available to handle the custom
// type. Generic helpers can be found in js/public/TracingAPI.h.
//
// Note that although this HashSet's trace will deal correctly with moved
// elements, it does not itself know when to barrier or trace elements. To
// function properly it must either be used with Rooted or barriered and traced
// manually.
template <typename T,
          typename HashPolicy = js::DefaultHasher<T>,
          typename AllocPolicy = js::TempAllocPolicy>
class GCHashSet : public js::HashSet<T, HashPolicy, AllocPolicy>
{
    using Base = js::HashSet<T, HashPolicy, AllocPolicy>;

  public:
    explicit GCHashSet(AllocPolicy a = AllocPolicy()) : Base(a)  {}

    static void trace(GCHashSet* set, JSTracer* trc) { set->trace(trc); }
    void trace(JSTracer* trc) {
        if (!this->initialized())
            return;
        for (typename Base::Enum e(*this); !e.empty(); e.popFront())
            GCPolicy<T>::trace(trc, &e.mutableFront(), "hashset element");
    }

    void sweep() {
        if (!this->initialized())
            return;
        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            if (GCPolicy<T>::needsSweep(&e.mutableFront()))
                e.removeFront();
        }
    }

    // GCHashSet is movable
    GCHashSet(GCHashSet&& rhs) : Base(mozilla::Move(rhs)) {}
    void operator=(GCHashSet&& rhs) {
        MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
        Base::operator=(mozilla::Move(rhs));
    }

  private:
    // GCHashSet is not copyable or assignable
    GCHashSet(const GCHashSet& hs) = delete;
    GCHashSet& operator=(const GCHashSet& hs) = delete;
};

} // namespace JS

namespace js {

template <typename Wrapper, typename... Args>
class WrappedPtrOperations<JS::GCHashSet<Args...>, Wrapper>
{
    using Set = JS::GCHashSet<Args...>;
    using Lookup = typename Set::Lookup;

    const Set& set() const { return static_cast<const Wrapper*>(this)->get(); }

  public:
    using AddPtr = typename Set::AddPtr;
    using Entry = typename Set::Entry;
    using Ptr = typename Set::Ptr;
    using Range = typename Set::Range;

    bool initialized() const                   { return set().initialized(); }
    Ptr lookup(const Lookup& l) const          { return set().lookup(l); }
    AddPtr lookupForAdd(const Lookup& l) const { return set().lookupForAdd(l); }
    Range all() const                          { return set().all(); }
    bool empty() const                         { return set().empty(); }
    uint32_t count() const                     { return set().count(); }
    size_t capacity() const                    { return set().capacity(); }
    bool has(const Lookup& l) const            { return set().lookup(l).found(); }
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return set().sizeOfExcludingThis(mallocSizeOf);
    }
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(this) + set().sizeOfExcludingThis(mallocSizeOf);
    }
};

template <typename Wrapper, typename... Args>
class MutableWrappedPtrOperations<JS::GCHashSet<Args...>, Wrapper>
  : public WrappedPtrOperations<JS::GCHashSet<Args...>, Wrapper>
{
    using Set = JS::GCHashSet<Args...>;
    using Lookup = typename Set::Lookup;

    Set& set() { return static_cast<Wrapper*>(this)->get(); }

  public:
    using AddPtr = typename Set::AddPtr;
    using Entry = typename Set::Entry;
    struct Enum : public Set::Enum { explicit Enum(Wrapper& o) : Set::Enum(o.set()) {} };
    using Ptr = typename Set::Ptr;
    using Range = typename Set::Range;

    bool init(uint32_t len = 16) { return set().init(len); }
    void clear()                 { set().clear(); }
    void finish()                { set().finish(); }
    void remove(Ptr p)           { set().remove(p); }
    void remove(const Lookup& l) { set().remove(l); }

    template<typename TInput>
    bool add(AddPtr& p, TInput&& t) {
        return set().add(p, mozilla::Forward<TInput>(t));
    }

    template<typename TInput>
    bool relookupOrAdd(AddPtr& p, const Lookup& l, TInput&& t) {
        return set().relookupOrAdd(p, l, mozilla::Forward<TInput>(t));
    }

    template<typename TInput>
    bool put(TInput&& t) {
        return set().put(mozilla::Forward<TInput>(t));
    }

    template<typename TInput>
    bool putNew(TInput&& t) {
        return set().putNew(mozilla::Forward<TInput>(t));
    }

    template<typename TInput>
    bool putNew(const Lookup& l, TInput&& t) {
        return set().putNew(l, mozilla::Forward<TInput>(t));
    }
};

} /* namespace js */

#endif /* GCHashTable_h */
