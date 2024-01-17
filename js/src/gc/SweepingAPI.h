/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SweepingAPI_h
#define js_SweepingAPI_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"

#include "jstypes.h"

#include "js/GCAnnotations.h"
#include "js/GCHashTable.h"
#include "js/GCPolicyAPI.h"
#include "js/RootingAPI.h"

namespace js {
namespace gc {

JS_PUBLIC_API void LockStoreBuffer(JSRuntime* runtime);
JS_PUBLIC_API void UnlockStoreBuffer(JSRuntime* runtim);

class AutoLockStoreBuffer {
  JSRuntime* runtime;

 public:
  explicit AutoLockStoreBuffer(JSRuntime* runtime) : runtime(runtime) {
    LockStoreBuffer(runtime);
  }
  ~AutoLockStoreBuffer() { UnlockStoreBuffer(runtime); }
};

class WeakCacheBase;
JS_PUBLIC_API void RegisterWeakCache(JS::Zone* zone, WeakCacheBase* cachep);
JS_PUBLIC_API void RegisterWeakCache(JSRuntime* rt, WeakCacheBase* cachep);

class WeakCacheBase : public mozilla::LinkedListElement<WeakCacheBase> {
  WeakCacheBase() = delete;
  explicit WeakCacheBase(const WeakCacheBase&) = delete;

 public:
  enum NeedsLock : bool { LockStoreBuffer = true, DontLockStoreBuffer = false };

  explicit WeakCacheBase(JS::Zone* zone) { RegisterWeakCache(zone, this); }
  explicit WeakCacheBase(JSRuntime* rt) { RegisterWeakCache(rt, this); }
  WeakCacheBase(WeakCacheBase&& other) = default;
  virtual ~WeakCacheBase() = default;

  virtual size_t traceWeak(JSTracer* trc, NeedsLock needLock) = 0;

  // Sweeping will be skipped if the cache is empty already.
  virtual bool empty() = 0;

  // Enable/disable read barrier during incremental sweeping and set the tracer
  // to use.
  virtual bool setIncrementalBarrierTracer(JSTracer* trc) {
    // Derived classes do not support incremental barriers by default.
    return false;
  }
  virtual bool needsIncrementalBarrier() const {
    // Derived classes do not support incremental barriers by default.
    return false;
  }
};

}  // namespace gc

// A WeakCache stores the given Sweepable container and links itself into a
// list of such caches that are swept during each GC. A WeakCache can be
// specific to a zone, or across a whole runtime, depending on which
// constructor is used.
template <typename T>
class WeakCache : protected gc::WeakCacheBase,
                  public js::MutableWrappedPtrOperations<T, WeakCache<T>> {
  T cache;

 public:
  using Type = T;

  template <typename... Args>
  explicit WeakCache(Zone* zone, Args&&... args)
      : WeakCacheBase(zone), cache(std::forward<Args>(args)...) {}
  template <typename... Args>
  explicit WeakCache(JSRuntime* rt, Args&&... args)
      : WeakCacheBase(rt), cache(std::forward<Args>(args)...) {}

  const T& get() const { return cache; }
  T& get() { return cache; }

  size_t traceWeak(JSTracer* trc, NeedsLock needsLock) override {
    // Take the store buffer lock in case sweeping triggers any generational
    // post barriers. This is not always required and WeakCache specializations
    // may delay or skip taking the lock as appropriate.
    mozilla::Maybe<js::gc::AutoLockStoreBuffer> lock;
    if (needsLock) {
      lock.emplace(trc->runtime());
    }

    JS::GCPolicy<T>::traceWeak(trc, &cache);
    return 0;
  }

  bool empty() override { return cache.empty(); }
} JS_HAZ_NON_GC_POINTER;

// Specialize WeakCache for GCHashMap to provide a barriered map that does not
// need to be swept immediately.
template <typename Key, typename Value, typename HashPolicy,
          typename AllocPolicy, typename MapEntryGCPolicy>
class WeakCache<
    GCHashMap<Key, Value, HashPolicy, AllocPolicy, MapEntryGCPolicy>>
    final : protected gc::WeakCacheBase {
  using Map = GCHashMap<Key, Value, HashPolicy, AllocPolicy, MapEntryGCPolicy>;
  using Self = WeakCache<Map>;

  Map map;
  JSTracer* barrierTracer = nullptr;

 public:
  template <typename... Args>
  explicit WeakCache(Zone* zone, Args&&... args)
      : WeakCacheBase(zone), map(std::forward<Args>(args)...) {}
  template <typename... Args>
  explicit WeakCache(JSRuntime* rt, Args&&... args)
      : WeakCacheBase(rt), map(std::forward<Args>(args)...) {}
  ~WeakCache() { MOZ_ASSERT(!barrierTracer); }

  bool empty() override { return map.empty(); }

  size_t traceWeak(JSTracer* trc, NeedsLock needsLock) override {
    size_t steps = map.count();

    // Create an Enum and sweep the table entries.
    mozilla::Maybe<typename Map::Enum> e;
    e.emplace(map);
    map.traceWeakEntries(trc, e.ref());

    // Potentially take a lock while the Enum's destructor is called as this can
    // rehash/resize the table and access the store buffer.
    mozilla::Maybe<js::gc::AutoLockStoreBuffer> lock;
    if (needsLock) {
      lock.emplace(trc->runtime());
    }
    e.reset();

    return steps;
  }

  bool setIncrementalBarrierTracer(JSTracer* trc) override {
    MOZ_ASSERT(bool(barrierTracer) != bool(trc));
    barrierTracer = trc;
    return true;
  }

  bool needsIncrementalBarrier() const override { return barrierTracer; }

 private:
  using Entry = typename Map::Entry;

  static bool entryNeedsSweep(JSTracer* barrierTracer, const Entry& prior) {
    Key key(prior.key());
    Value value(prior.value());
    bool needsSweep = !MapEntryGCPolicy::traceWeak(barrierTracer, &key, &value);
    MOZ_ASSERT_IF(!needsSweep,
                  prior.key() == key);  // We shouldn't update here.
    MOZ_ASSERT_IF(!needsSweep,
                  prior.value() == value);  // We shouldn't update here.
    return needsSweep;
  }

 public:
  using Lookup = typename Map::Lookup;
  using Ptr = typename Map::Ptr;
  using AddPtr = typename Map::AddPtr;

  // Iterator over the whole collection.
  struct Range {
    explicit Range(Self& self) : cache(self), range(self.map.all()) {
      settle();
    }
    Range() = default;

    bool empty() const { return range.empty(); }
    const Entry& front() const { return range.front(); }

    void popFront() {
      range.popFront();
      settle();
    }

   private:
    Self& cache;
    typename Map::Range range;

    void settle() {
      if (JSTracer* trc = cache.barrierTracer) {
        while (!empty() && entryNeedsSweep(trc, front())) {
          popFront();
        }
      }
    }
  };

  struct Enum : public Map::Enum {
    explicit Enum(Self& cache) : Map::Enum(cache.map) {
      // This operation is not allowed while barriers are in place as we
      // may also need to enumerate the set for sweeping.
      MOZ_ASSERT(!cache.barrierTracer);
    }
  };

  Ptr lookup(const Lookup& l) const {
    Ptr ptr = map.lookup(l);
    if (barrierTracer && ptr && entryNeedsSweep(barrierTracer, *ptr)) {
      const_cast<Map&>(map).remove(ptr);
      return Ptr();
    }
    return ptr;
  }

  AddPtr lookupForAdd(const Lookup& l) {
    AddPtr ptr = map.lookupForAdd(l);
    if (barrierTracer && ptr && entryNeedsSweep(barrierTracer, *ptr)) {
      const_cast<Map&>(map).remove(ptr);
      return map.lookupForAdd(l);
    }
    return ptr;
  }

  Range all() const { return Range(*const_cast<Self*>(this)); }

  bool empty() const {
    // This operation is not currently allowed while barriers are in place
    // as it would require iterating the map and the caller expects a
    // constant time operation.
    MOZ_ASSERT(!barrierTracer);
    return map.empty();
  }

  uint32_t count() const {
    // This operation is not currently allowed while barriers are in place
    // as it would require iterating the set and the caller expects a
    // constant time operation.
    MOZ_ASSERT(!barrierTracer);
    return map.count();
  }

  size_t capacity() const { return map.capacity(); }

  bool has(const Lookup& l) const { return lookup(l).found(); }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return map.sizeOfExcludingThis(mallocSizeOf);
  }
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + map.shallowSizeOfExcludingThis(mallocSizeOf);
  }

  void clear() {
    // This operation is not currently allowed while barriers are in place
    // since it doesn't make sense to clear a cache while it is being swept.
    MOZ_ASSERT(!barrierTracer);
    map.clear();
  }

  void clearAndCompact() {
    // This operation is not currently allowed while barriers are in place
    // since it doesn't make sense to clear a cache while it is being swept.
    MOZ_ASSERT(!barrierTracer);
    map.clearAndCompact();
  }

  void remove(Ptr p) {
    // This currently supports removing entries during incremental
    // sweeping. If we allow these tables to be swept incrementally this may
    // no longer be possible.
    map.remove(p);
  }

  void remove(const Lookup& l) {
    Ptr p = lookup(l);
    if (p) {
      remove(p);
    }
  }

  template <typename KeyInput, typename ValueInput>
  bool add(AddPtr& p, KeyInput&& k, ValueInput&& v) {
    return map.add(p, std::forward<KeyInput>(k), std::forward<ValueInput>(v));
  }

  template <typename KeyInput, typename ValueInput>
  bool relookupOrAdd(AddPtr& p, KeyInput&& k, ValueInput&& v) {
    return map.relookupOrAdd(p, std::forward<KeyInput>(k),
                             std::forward<ValueInput>(v));
  }

  template <typename KeyInput, typename ValueInput>
  bool put(KeyInput&& k, ValueInput&& v) {
    return map.put(std::forward<KeyInput>(k), std::forward<ValueInput>(v));
  }

  template <typename KeyInput, typename ValueInput>
  bool putNew(KeyInput&& k, ValueInput&& v) {
    return map.putNew(std::forward<KeyInput>(k), std::forward<ValueInput>(v));
  }
} JS_HAZ_NON_GC_POINTER;

// Specialize WeakCache for GCHashSet to provide a barriered set that does not
// need to be swept immediately.
template <typename T, typename HashPolicy, typename AllocPolicy>
class WeakCache<GCHashSet<T, HashPolicy, AllocPolicy>> final
    : protected gc::WeakCacheBase {
  using Set = GCHashSet<T, HashPolicy, AllocPolicy>;
  using Self = WeakCache<Set>;

  Set set;
  JSTracer* barrierTracer = nullptr;

 public:
  using Entry = typename Set::Entry;

  template <typename... Args>
  explicit WeakCache(Zone* zone, Args&&... args)
      : WeakCacheBase(zone), set(std::forward<Args>(args)...) {}
  template <typename... Args>
  explicit WeakCache(JSRuntime* rt, Args&&... args)
      : WeakCacheBase(rt), set(std::forward<Args>(args)...) {}

  size_t traceWeak(JSTracer* trc, NeedsLock needsLock) override {
    size_t steps = set.count();

    // Create an Enum and sweep the table entries. It's not necessary to take
    // the store buffer lock yet.
    mozilla::Maybe<typename Set::Enum> e;
    e.emplace(set);
    set.traceWeakEntries(trc, e.ref());

    // Destroy the Enum, potentially rehashing or resizing the table. Since this
    // can access the store buffer, we need to take a lock for this if we're
    // called off main thread.
    mozilla::Maybe<js::gc::AutoLockStoreBuffer> lock;
    if (needsLock) {
      lock.emplace(trc->runtime());
    }
    e.reset();

    return steps;
  }

  bool empty() override { return set.empty(); }

  bool setIncrementalBarrierTracer(JSTracer* trc) override {
    MOZ_ASSERT(bool(barrierTracer) != bool(trc));
    barrierTracer = trc;
    return true;
  }

  bool needsIncrementalBarrier() const override { return barrierTracer; }

 private:
  static bool entryNeedsSweep(JSTracer* barrierTracer, const Entry& prior) {
    Entry entry(prior);
    bool needsSweep = !JS::GCPolicy<T>::traceWeak(barrierTracer, &entry);
    MOZ_ASSERT_IF(!needsSweep, prior == entry);  // We shouldn't update here.
    return needsSweep;
  }

 public:
  using Lookup = typename Set::Lookup;
  using Ptr = typename Set::Ptr;
  using AddPtr = typename Set::AddPtr;

  // Iterator over the whole collection.
  struct Range {
    explicit Range(Self& self) : cache(self), range(self.set.all()) {
      settle();
    }
    Range() = default;

    bool empty() const { return range.empty(); }
    const Entry& front() const { return range.front(); }

    void popFront() {
      range.popFront();
      settle();
    }

   private:
    Self& cache;
    typename Set::Range range;

    void settle() {
      if (JSTracer* trc = cache.barrierTracer) {
        while (!empty() && entryNeedsSweep(trc, front())) {
          popFront();
        }
      }
    }
  };

  struct Enum : public Set::Enum {
    explicit Enum(Self& cache) : Set::Enum(cache.set) {
      // This operation is not allowed while barriers are in place as we
      // may also need to enumerate the set for sweeping.
      MOZ_ASSERT(!cache.barrierTracer);
    }
  };

  Ptr lookup(const Lookup& l) const {
    Ptr ptr = set.lookup(l);
    if (barrierTracer && ptr && entryNeedsSweep(barrierTracer, *ptr)) {
      const_cast<Set&>(set).remove(ptr);
      return Ptr();
    }
    return ptr;
  }

  AddPtr lookupForAdd(const Lookup& l) {
    AddPtr ptr = set.lookupForAdd(l);
    if (barrierTracer && ptr && entryNeedsSweep(barrierTracer, *ptr)) {
      const_cast<Set&>(set).remove(ptr);
      return set.lookupForAdd(l);
    }
    return ptr;
  }

  Range all() const { return Range(*const_cast<Self*>(this)); }

  bool empty() const {
    // This operation is not currently allowed while barriers are in place
    // as it would require iterating the set and the caller expects a
    // constant time operation.
    MOZ_ASSERT(!barrierTracer);
    return set.empty();
  }

  uint32_t count() const {
    // This operation is not currently allowed while barriers are in place
    // as it would require iterating the set and the caller expects a
    // constant time operation.
    MOZ_ASSERT(!barrierTracer);
    return set.count();
  }

  size_t capacity() const { return set.capacity(); }

  bool has(const Lookup& l) const { return lookup(l).found(); }

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return set.shallowSizeOfExcludingThis(mallocSizeOf);
  }
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + set.shallowSizeOfExcludingThis(mallocSizeOf);
  }

  void clear() {
    // This operation is not currently allowed while barriers are in place
    // since it doesn't make sense to clear a cache while it is being swept.
    MOZ_ASSERT(!barrierTracer);
    set.clear();
  }

  void clearAndCompact() {
    // This operation is not currently allowed while barriers are in place
    // since it doesn't make sense to clear a cache while it is being swept.
    MOZ_ASSERT(!barrierTracer);
    set.clearAndCompact();
  }

  void remove(Ptr p) {
    // This currently supports removing entries during incremental
    // sweeping. If we allow these tables to be swept incrementally this may
    // no longer be possible.
    set.remove(p);
  }

  void remove(const Lookup& l) {
    Ptr p = lookup(l);
    if (p) {
      remove(p);
    }
  }

  template <typename TInput>
  void replaceKey(Ptr p, const Lookup& l, TInput&& newValue) {
    set.replaceKey(p, l, std::forward<TInput>(newValue));
  }

  template <typename TInput>
  bool add(AddPtr& p, TInput&& t) {
    return set.add(p, std::forward<TInput>(t));
  }

  template <typename TInput>
  bool relookupOrAdd(AddPtr& p, const Lookup& l, TInput&& t) {
    return set.relookupOrAdd(p, l, std::forward<TInput>(t));
  }

  template <typename TInput>
  bool put(TInput&& t) {
    return set.put(std::forward<TInput>(t));
  }

  template <typename TInput>
  bool putNew(TInput&& t) {
    return set.putNew(std::forward<TInput>(t));
  }

  template <typename TInput>
  bool putNew(const Lookup& l, TInput&& t) {
    return set.putNew(l, std::forward<TInput>(t));
  }
} JS_HAZ_NON_GC_POINTER;

}  // namespace js

#endif  // js_SweepingAPI_h
