/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_HashTable_h
#define gc_HashTable_h

#include "js/HashTable.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"

namespace js {

// A TraceableHashMap is a HashMap with an additional trace method that knows
// how to visit all keys and values in the table. HashMaps that contain GC
// pointers that must be traced to be kept alive will generally want to use
// this TraceableHashMap specializeation in lieu of HashMap.
//
// Most types of GC pointers as keys and values can be traced with no extra
// infrastructure.  For structs and non-gc-pointer members, ensure that there
// is a specialization of DefaultTracer<T> with an appropriate trace method
// available to handle the custom type.
//
// Note that although this HashMap's trace will deal correctly with moved keys,
// it does not itself know when to barrier or trace keys. To function properly
// it must either be used with Rooted, or barriered and traced manually.
template <typename Key,
          typename Value,
          typename HashPolicy = DefaultHasher<Key>,
          typename AllocPolicy = TempAllocPolicy,
          typename KeyTraceFunc = DefaultTracer<Key>,
          typename ValueTraceFunc = DefaultTracer<Value>>
class TraceableHashMap : public HashMap<Key, Value, HashPolicy, AllocPolicy>,
                         public JS::Traceable
{
    using Base = HashMap<Key, Value, HashPolicy, AllocPolicy>;

  public:
    explicit TraceableHashMap(AllocPolicy a = AllocPolicy()) : Base(a)  {}

    static void trace(TraceableHashMap* map, JSTracer* trc) { map->trace(trc); }
    void trace(JSTracer* trc) {
        if (!this->initialized())
            return;
        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            ValueTraceFunc::trace(trc, &e.front().value(), "hashmap value");
            Key key = e.front().key();
            KeyTraceFunc::trace(trc, &key, "hashmap key");
            if (key != e.front().key())
                e.rekeyFront(key);
        }
    }

    // TraceableHashMap is movable
    TraceableHashMap(TraceableHashMap&& rhs) : Base(mozilla::Forward<TraceableHashMap>(rhs)) {}
    void operator=(TraceableHashMap&& rhs) {
        MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
        Base::operator=(mozilla::Forward<TraceableHashMap>(rhs));
    }

  private:
    // TraceableHashMap is not copyable or assignable
    TraceableHashMap(const TraceableHashMap& hm) = delete;
    TraceableHashMap& operator=(const TraceableHashMap& hm) = delete;
};

template <typename Outer, typename... Args>
class TraceableHashMapOperations
{
    using Map = TraceableHashMap<Args...>;
    using Lookup = typename Map::Lookup;
    using Ptr = typename Map::Ptr;
    using AddPtr = typename Map::AddPtr;
    using Range = typename Map::Range;
    using Enum = typename Map::Enum;

    const Map& map() const { return static_cast<const Outer*>(this)->get(); }

  public:
    bool initialized() const                   { return map().initialized(); }
    Ptr lookup(const Lookup& l) const          { return map().lookup(l); }
    AddPtr lookupForAdd(const Lookup& l) const { return map().lookupForAdd(l); }
    Range all() const                          { return map().all(); }
    bool empty() const                         { return map().empty(); }
    uint32_t count() const                     { return map().count(); }
    size_t capacity() const                    { return map().capacity(); }
    uint32_t generation() const                { return map().generation(); }
    bool has(const Lookup& l) const            { return map().lookup(l).found(); }
};

template <typename Outer, typename... Args>
class MutableTraceableHashMapOperations
  : public TraceableHashMapOperations<Outer, Args...>
{
    using Map = TraceableHashMap<Args...>;
    using Lookup = typename Map::Lookup;
    using Ptr = typename Map::Ptr;
    using AddPtr = typename Map::AddPtr;
    using Range = typename Map::Range;
    using Enum = typename Map::Enum;

    Map& map() { return static_cast<Outer*>(this)->get(); }

  public:
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

template <typename A, typename B, typename C, typename D, typename E, typename F>
class RootedBase<TraceableHashMap<A,B,C,D,E,F>>
  : public MutableTraceableHashMapOperations<JS::Rooted<TraceableHashMap<A,B,C,D,E,F>>, A,B,C,D,E,F>
{};

template <typename A, typename B, typename C, typename D, typename E, typename F>
class MutableHandleBase<TraceableHashMap<A,B,C,D,E,F>>
  : public MutableTraceableHashMapOperations<JS::MutableHandle<TraceableHashMap<A,B,C,D,E,F>>,
                                             A,B,C,D,E,F>
{};

template <typename A, typename B, typename C, typename D, typename E, typename F>
class HandleBase<TraceableHashMap<A,B,C,D,E,F>>
  : public TraceableHashMapOperations<JS::Handle<TraceableHashMap<A,B,C,D,E,F>>, A,B,C,D,E,F>
{};

// A TraceableHashSet is a HashSet with an additional trace method that knows
// how to visit all set element.  HashSets that contain GC pointers that must
// be traced to be kept alive will generally want to use this TraceableHashSet
// specializeation in lieu of HashSet.
//
// Most types of GC pointers can be traced with no extra infrastructure.  For
// structs and non-gc-pointer members, ensure that there is a specialization of
// DefaultTracer<T> with an appropriate trace method available to handle the
// custom type.
//
// Note that although this HashSet's trace will deal correctly with moved
// elements, it does not itself know when to barrier or trace elements.  To
// function properly it must either be used with Rooted or barriered and traced
// manually.
template <typename T,
          typename HashPolicy = DefaultHasher<T>,
          typename AllocPolicy = TempAllocPolicy,
          typename ElemTraceFunc = DefaultTracer<T>>
class TraceableHashSet : public HashSet<T, HashPolicy, AllocPolicy>,
                         public JS::Traceable
{
    using Base = HashSet<T, HashPolicy, AllocPolicy>;

  public:
    explicit TraceableHashSet(AllocPolicy a = AllocPolicy()) : Base(a)  {}

    static void trace(TraceableHashSet* set, JSTracer* trc) { set->trace(trc); }
    void trace(JSTracer* trc) {
        if (!this->initialized())
            return;
        for (typename Base::Enum e(*this); !e.empty(); e.popFront()) {
            T elem = e.front();
            ElemTraceFunc::trace(trc, &elem, "hashset element");
            if (elem != e.front())
                e.rekeyFront(elem);
        }
    }

    // TraceableHashSet is movable
    TraceableHashSet(TraceableHashSet&& rhs) : Base(mozilla::Forward<TraceableHashSet>(rhs)) {}
    void operator=(TraceableHashSet&& rhs) {
        MOZ_ASSERT(this != &rhs, "self-move assignment is prohibited");
        Base::operator=(mozilla::Forward<TraceableHashSet>(rhs));
    }

  private:
    // TraceableHashSet is not copyable or assignable
    TraceableHashSet(const TraceableHashSet& hs) = delete;
    TraceableHashSet& operator=(const TraceableHashSet& hs) = delete;
};

template <typename Outer, typename... Args>
class TraceableHashSetOperations
{
    using Set = TraceableHashSet<Args...>;
    using Lookup = typename Set::Lookup;
    using Ptr = typename Set::Ptr;
    using AddPtr = typename Set::AddPtr;
    using Range = typename Set::Range;
    using Enum = typename Set::Enum;

    const Set& set() const { return static_cast<const Outer*>(this)->extract(); }

  public:
    bool initialized() const                   { return set().initialized(); }
    Ptr lookup(const Lookup& l) const          { return set().lookup(l); }
    AddPtr lookupForAdd(const Lookup& l) const { return set().lookupForAdd(l); }
    Range all() const                          { return set().all(); }
    bool empty() const                         { return set().empty(); }
    uint32_t count() const                     { return set().count(); }
    size_t capacity() const                    { return set().capacity(); }
    uint32_t generation() const                { return set().generation(); }
    bool has(const Lookup& l) const            { return set().lookup(l).found(); }
};

template <typename Outer, typename... Args>
class MutableTraceableHashSetOperations
  : public TraceableHashSetOperations<Outer, Args...>
{
    using Set = TraceableHashSet<Args...>;
    using Lookup = typename Set::Lookup;
    using Ptr = typename Set::Ptr;
    using AddPtr = typename Set::AddPtr;
    using Range = typename Set::Range;
    using Enum = typename Set::Enum;

    Set& set() { return static_cast<Outer*>(this)->extract(); }

  public:
    bool init(uint32_t len = 16) { return set().init(len); }
    void clear()                 { set().clear(); }
    void finish()                { set().finish(); }
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

template <typename T, typename HP, typename AP, typename TF>
class RootedBase<TraceableHashSet<T, HP, AP, TF>>
  : public MutableTraceableHashSetOperations<JS::Rooted<TraceableHashSet<T, HP, AP, TF>>, T, HP, AP, TF>
{
    using Set = TraceableHashSet<T, HP, AP, TF>;

    friend class TraceableHashSetOperations<JS::Rooted<Set>, T, HP, AP, TF>;
    const Set& extract() const { return *static_cast<const JS::Rooted<Set>*>(this)->address(); }

    friend class MutableTraceableHashSetOperations<JS::Rooted<Set>, T, HP, AP, TF>;
    Set& extract() { return *static_cast<JS::Rooted<Set>*>(this)->address(); }
};

template <typename T, typename HP, typename AP, typename TF>
class MutableHandleBase<TraceableHashSet<T, HP, AP, TF>>
  : public MutableTraceableHashSetOperations<JS::MutableHandle<TraceableHashSet<T, HP, AP, TF>>,
                                             T, HP, AP, TF>
{
    using Set = TraceableHashSet<T, HP, AP, TF>;

    friend class TraceableHashSetOperations<JS::MutableHandle<Set>, T, HP, AP, TF>;
    const Set& extract() const {
        return *static_cast<const JS::MutableHandle<Set>*>(this)->address();
    }

    friend class MutableTraceableHashSetOperations<JS::MutableHandle<Set>, T, HP, AP, TF>;
    Set& extract() { return *static_cast<JS::MutableHandle<Set>*>(this)->address(); }
};

template <typename T, typename HP, typename AP, typename TF>
class HandleBase<TraceableHashSet<T, HP, AP, TF>>
  : public TraceableHashSetOperations<JS::Handle<TraceableHashSet<T, HP, AP, TF>>, T, HP, AP, TF>
{
    using Set = TraceableHashSet<T, HP, AP, TF>;
    friend class TraceableHashSetOperations<JS::Handle<Set>, T, HP, AP, TF>;
    const Set& extract() const { return *static_cast<const JS::Handle<Set>*>(this)->address(); }
};

} /* namespace js */

#endif /* gc_HashTable_h */
