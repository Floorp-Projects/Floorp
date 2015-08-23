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

} /* namespace js */

#endif /* gc_HashTable_h */
