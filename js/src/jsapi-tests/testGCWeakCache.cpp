/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Policy.h"
#include "gc/Zone.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/SweepingAPI.h"

#include "jsapi-tests/tests.h"

using namespace js;

// Exercise WeakCache<GCHashSet>.
BEGIN_TEST(testWeakCacheSet) {
  // Create two objects tenured and two in the nursery. If zeal is on,
  // this may fail and we'll get more tenured objects. That's fine:
  // the test will continue to work, it will just not test as much.
  JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
  JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
  JS_GC(cx);
  JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
  JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

  using ObjectSet =
      GCHashSet<JS::Heap<JSObject*>, MovableCellHasher<JS::Heap<JSObject*>>,
                SystemAllocPolicy>;
  using Cache = JS::WeakCache<ObjectSet>;
  Cache cache(JS::GetObjectZone(tenured1));

  cache.put(tenured1);
  cache.put(tenured2);
  cache.put(nursery1);
  cache.put(nursery2);

  // Verify relocation and that we don't sweep too aggressively.
  JS_GC(cx);
  CHECK(cache.has(tenured1));
  CHECK(cache.has(tenured2));
  CHECK(cache.has(nursery1));
  CHECK(cache.has(nursery2));

  // Unroot two entries and verify that they get removed.
  tenured2 = nursery2 = nullptr;
  JS_GC(cx);
  CHECK(cache.has(tenured1));
  CHECK(cache.has(nursery1));
  CHECK(cache.count() == 2);

  return true;
}
END_TEST(testWeakCacheSet)

// Exercise WeakCache<GCHashMap>.
BEGIN_TEST(testWeakCacheMap) {
  // Create two objects tenured and two in the nursery. If zeal is on,
  // this may fail and we'll get more tenured objects. That's fine:
  // the test will continue to work, it will just not test as much.
  JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
  JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
  JS_GC(cx);
  JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
  JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

  using ObjectMap = js::GCHashMap<JS::Heap<JSObject*>, uint32_t,
                                  js::MovableCellHasher<JS::Heap<JSObject*>>>;
  using Cache = JS::WeakCache<ObjectMap>;
  Cache cache(JS::GetObjectZone(tenured1), cx);

  cache.put(tenured1, 1);
  cache.put(tenured2, 2);
  cache.put(nursery1, 3);
  cache.put(nursery2, 4);

  JS_GC(cx);
  CHECK(cache.has(tenured1));
  CHECK(cache.has(tenured2));
  CHECK(cache.has(nursery1));
  CHECK(cache.has(nursery2));

  tenured2 = nursery2 = nullptr;
  JS_GC(cx);
  CHECK(cache.has(tenured1));
  CHECK(cache.has(nursery1));
  CHECK(cache.count() == 2);

  return true;
}
END_TEST(testWeakCacheMap)

// Exercise WeakCache<GCVector>.
BEGIN_TEST(testWeakCacheGCVector) {
  // Create two objects tenured and two in the nursery. If zeal is on,
  // this may fail and we'll get more tenured objects. That's fine:
  // the test will continue to work, it will just not test as much.
  JS::RootedObject tenured1(cx, JS_NewPlainObject(cx));
  JS::RootedObject tenured2(cx, JS_NewPlainObject(cx));
  JS_GC(cx);
  JS::RootedObject nursery1(cx, JS_NewPlainObject(cx));
  JS::RootedObject nursery2(cx, JS_NewPlainObject(cx));

  using ObjectVector = JS::WeakCache<GCVector<JS::Heap<JSObject*>>>;
  ObjectVector cache(JS::GetObjectZone(tenured1), cx);

  CHECK(cache.append(tenured1));
  CHECK(cache.append(tenured2));
  CHECK(cache.append(nursery1));
  CHECK(cache.append(nursery2));

  JS_GC(cx);
  CHECK(cache.get().length() == 4);
  CHECK(cache.get()[0] == tenured1);
  CHECK(cache.get()[1] == tenured2);
  CHECK(cache.get()[2] == nursery1);
  CHECK(cache.get()[3] == nursery2);

  tenured2 = nursery2 = nullptr;
  JS_GC(cx);
  CHECK(cache.get().length() == 2);
  CHECK(cache.get()[0] == tenured1);
  CHECK(cache.get()[1] == nursery1);

  return true;
}
END_TEST(testWeakCacheGCVector)

#ifdef JS_GC_ZEAL

// A simple structure that embeds an object pointer. We cripple the hash
// implementation so that we can test hash table collisions.
struct ObjectEntry {
  JS::Heap<JSObject*> obj;
  explicit ObjectEntry(JSObject* o) : obj(o) {}
  bool operator==(const ObjectEntry& other) const { return obj == other.obj; }
  bool needsSweep() {
    return JS::GCPolicy<JS::Heap<JSObject*>>::needsSweep(&obj);
  }
};

namespace js {
template <>
struct MovableCellHasher<ObjectEntry> {
  using Key = ObjectEntry;
  using Lookup = JSObject*;

  static bool hasHash(const Lookup& l) {
    return MovableCellHasher<JSObject*>::hasHash(l);
  }
  static bool ensureHash(const Lookup& l) {
    return MovableCellHasher<JSObject*>::ensureHash(l);
  }
  static HashNumber hash(const Lookup& l) {
    // Reduce hash code to single bit to generate hash collisions.
    return MovableCellHasher<JS::Heap<JSObject*>>::hash(l) & 0x1;
  }
  static bool match(const Key& k, const Lookup& l) {
    return MovableCellHasher<JS::Heap<JSObject*>>::match(k.obj, l);
  }
};
}  // namespace js

// A structure that contains a pointer to a JSObject but is keyed based on an
// integer. This lets us test replacing dying entries in a set.
struct NumberAndObjectEntry {
  uint32_t number;
  JS::Heap<JSObject*> obj;

  NumberAndObjectEntry(uint32_t n, JSObject* o) : number(n), obj(o) {}
  bool operator==(const NumberAndObjectEntry& other) const {
    return number == other.number && obj == other.obj;
  }
  bool needsSweep() {
    return JS::GCPolicy<JS::Heap<JSObject*>>::needsSweep(&obj);
  }
};

struct NumberAndObjectLookup {
  uint32_t number;
  JS::Heap<JSObject*> obj;

  NumberAndObjectLookup(uint32_t n, JSObject* o) : number(n), obj(o) {}
  MOZ_IMPLICIT NumberAndObjectLookup(const NumberAndObjectEntry& entry)
      : number(entry.number), obj(entry.obj) {}
};

namespace js {
template <>
struct MovableCellHasher<NumberAndObjectEntry> {
  using Key = NumberAndObjectEntry;
  using Lookup = NumberAndObjectLookup;

  static bool hasHash(const Lookup& l) {
    return MovableCellHasher<JSObject*>::hasHash(l.obj);
  }
  static bool ensureHash(const Lookup& l) {
    return MovableCellHasher<JSObject*>::ensureHash(l.obj);
  }
  static HashNumber hash(const Lookup& l) {
    // Reduce hash code to single bit to generate hash collisions.
    return MovableCellHasher<JS::Heap<JSObject*>>::hash(l.obj) ^ l.number;
  }
  static bool match(const Key& k, const Lookup& l) {
    return k.number == l.number &&
           MovableCellHasher<JS::Heap<JSObject*>>::match(k.obj, l.obj);
  }
};
}  // namespace js

BEGIN_TEST(testIncrementalWeakCacheSweeping) {
  AutoLeaveZeal nozeal(cx);

  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, true);
  JS_SetGCZeal(cx, 17, 1000000);

  CHECK(TestSet());
  CHECK(TestMap());
  CHECK(TestReplaceDyingInSet());
  CHECK(TestReplaceDyingInMap());
  CHECK(TestUniqueIDLookups());

  JS_SetGCZeal(cx, 0, 0);
  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, false);

  return true;
}

template <typename Cache>
bool GCUntilCacheSweep(JSContext* cx, const Cache& cache) {
  CHECK(!IsIncrementalGCInProgress(cx));

  JS::Zone* zone = JS::GetObjectZone(global);
  JS::PrepareZoneForGC(cx, zone);
  SliceBudget budget(WorkBudget(1));
  cx->runtime()->gc.startDebugGC(JS::GCOptions::Normal, budget);

  CHECK(IsIncrementalGCInProgress(cx));
  CHECK(zone->isGCSweeping());
  CHECK(cache.needsIncrementalBarrier());

  return true;
}

template <typename Cache>
bool SweepCacheAndFinishGC(JSContext* cx, const Cache& cache) {
  CHECK(IsIncrementalGCInProgress(cx));

  PrepareForIncrementalGC(cx);
  IncrementalGCSlice(cx, JS::GCReason::API);

  JS::Zone* zone = JS::GetObjectZone(global);
  CHECK(!IsIncrementalGCInProgress(cx));
  CHECK(!zone->isCollecting());
  CHECK(!cache.needsIncrementalBarrier());

  return true;
}

bool TestSet() {
  using ObjectSet =
      GCHashSet<JS::Heap<JSObject*>, MovableCellHasher<JS::Heap<JSObject*>>,
                TempAllocPolicy>;
  using Cache = JS::WeakCache<ObjectSet>;
  Cache cache(JS::GetObjectZone(global), cx);

  // Sweep empty cache.

  CHECK(cache.empty());
  JS_GC(cx);
  CHECK(cache.empty());

  // Add an entry while sweeping.

  JS::RootedObject obj1(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj2(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj3(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj4(cx, JS_NewPlainObject(cx));
  CHECK(obj1);
  CHECK(obj2);
  CHECK(obj3);
  CHECK(obj4);

  CHECK(!cache.has(obj1));
  CHECK(cache.put(obj1));
  CHECK(cache.count() == 1);
  CHECK(cache.has(obj1));
  CHECK(*cache.lookup(obj1) == obj1);

  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(!cache.has(obj2));
  CHECK(cache.put(obj2));
  CHECK(cache.has(obj2));
  CHECK(*cache.lookup(obj2) == obj2);

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 2);
  CHECK(cache.has(obj1));
  CHECK(cache.has(obj2));

  // Test dying entries are not found while sweeping.

  CHECK(cache.put(obj3));
  CHECK(cache.put(obj4));
  void* old3 = obj3;
  void* old4 = obj4;
  obj3 = obj4 = nullptr;

  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(cache.has(obj1));
  CHECK(cache.has(obj2));
  CHECK(!cache.has(static_cast<JSObject*>(old3)));
  CHECK(!cache.has(static_cast<JSObject*>(old4)));

  size_t count = 0;
  for (auto r = cache.all(); !r.empty(); r.popFront()) {
    CHECK(r.front() == obj1 || r.front() == obj2);
    count++;
  }
  CHECK(count == 2);

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 2);

  // Test lookupForAdd while sweeping.

  obj3 = JS_NewPlainObject(cx);
  obj4 = JS_NewPlainObject(cx);
  CHECK(obj3);
  CHECK(obj4);

  CHECK(cache.lookupForAdd(obj1));
  CHECK(*cache.lookupForAdd(obj1) == obj1);

  auto addp = cache.lookupForAdd(obj3);
  CHECK(!addp);
  CHECK(cache.add(addp, obj3));
  CHECK(cache.has(obj3));

  CHECK(GCUntilCacheSweep(cx, cache));

  addp = cache.lookupForAdd(obj4);
  CHECK(!addp);
  CHECK(cache.add(addp, obj4));
  CHECK(cache.has(obj4));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 4);
  CHECK(cache.has(obj3));
  CHECK(cache.has(obj4));

  // Test remove while sweeping.

  cache.remove(obj3);

  CHECK(GCUntilCacheSweep(cx, cache));

  cache.remove(obj4);

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 2);
  CHECK(!cache.has(obj3));
  CHECK(!cache.has(obj4));

  // Test putNew while sweeping.

  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(cache.putNew(obj3));
  CHECK(cache.putNew(obj4, obj4));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 4);
  CHECK(cache.has(obj3));
  CHECK(cache.has(obj4));

  cache.clear();

  return true;
}

bool TestMap() {
  using ObjectMap =
      GCHashMap<JS::Heap<JSObject*>, uint32_t,
                MovableCellHasher<JS::Heap<JSObject*>>, TempAllocPolicy>;
  using Cache = JS::WeakCache<ObjectMap>;
  Cache cache(JS::GetObjectZone(global), cx);

  // Sweep empty cache.

  CHECK(cache.empty());
  JS_GC(cx);
  CHECK(cache.empty());

  // Add an entry while sweeping.

  JS::RootedObject obj1(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj2(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj3(cx, JS_NewPlainObject(cx));
  JS::RootedObject obj4(cx, JS_NewPlainObject(cx));
  CHECK(obj1);
  CHECK(obj2);
  CHECK(obj3);
  CHECK(obj4);

  CHECK(!cache.has(obj1));
  CHECK(cache.put(obj1, 1));
  CHECK(cache.count() == 1);
  CHECK(cache.has(obj1));
  CHECK(cache.lookup(obj1)->key() == obj1);

  CHECK(GCUntilCacheSweep(cx, cache));
  CHECK(cache.needsIncrementalBarrier());

  CHECK(!cache.has(obj2));
  CHECK(cache.put(obj2, 2));
  CHECK(cache.has(obj2));
  CHECK(cache.lookup(obj2)->key() == obj2);

  CHECK(SweepCacheAndFinishGC(cx, cache));
  CHECK(!cache.needsIncrementalBarrier());

  CHECK(cache.count() == 2);
  CHECK(cache.has(obj1));
  CHECK(cache.has(obj2));

  // Test iteration.

  CHECK(cache.put(obj3, 3));
  CHECK(cache.put(obj4, 4));
  void* old3 = obj3;
  void* old4 = obj4;
  obj3 = obj4 = nullptr;

  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(cache.has(obj1));
  CHECK(cache.has(obj2));
  CHECK(!cache.has(static_cast<JSObject*>(old3)));
  CHECK(!cache.has(static_cast<JSObject*>(old4)));

  size_t count = 0;
  for (auto r = cache.all(); !r.empty(); r.popFront()) {
    CHECK(r.front().key() == obj1 || r.front().key() == obj2);
    count++;
  }
  CHECK(count == 2);

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 2);

  // Test lookupForAdd while sweeping.

  obj3 = JS_NewPlainObject(cx);
  obj4 = JS_NewPlainObject(cx);
  CHECK(obj3);
  CHECK(obj4);

  CHECK(cache.lookupForAdd(obj1));
  CHECK(cache.lookupForAdd(obj1)->key() == obj1);

  auto addp = cache.lookupForAdd(obj3);
  CHECK(!addp);
  CHECK(cache.add(addp, obj3, 3));
  CHECK(cache.has(obj3));

  CHECK(GCUntilCacheSweep(cx, cache));

  addp = cache.lookupForAdd(obj4);
  CHECK(!addp);
  CHECK(cache.add(addp, obj4, 4));
  CHECK(cache.has(obj4));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 4);
  CHECK(cache.has(obj3));
  CHECK(cache.has(obj4));

  // Test remove while sweeping.

  cache.remove(obj3);

  CHECK(GCUntilCacheSweep(cx, cache));

  cache.remove(obj4);

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 2);
  CHECK(!cache.has(obj3));
  CHECK(!cache.has(obj4));

  // Test putNew while sweeping.

  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(cache.putNew(obj3, 3));
  CHECK(cache.putNew(obj4, 4));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 4);
  CHECK(cache.has(obj3));
  CHECK(cache.has(obj4));

  cache.clear();

  return true;
}

bool TestReplaceDyingInSet() {
  // Test replacing dying entries with ones that have the same key using the
  // various APIs.

  using Cache = JS::WeakCache<
      GCHashSet<NumberAndObjectEntry, MovableCellHasher<NumberAndObjectEntry>,
                TempAllocPolicy>>;
  Cache cache(JS::GetObjectZone(global), cx);

  RootedObject value1(cx, JS_NewPlainObject(cx));
  RootedObject value2(cx, JS_NewPlainObject(cx));
  CHECK(value1);
  CHECK(value2);

  CHECK(cache.put(NumberAndObjectEntry(1, value1)));
  CHECK(cache.put(NumberAndObjectEntry(2, value2)));
  CHECK(cache.put(NumberAndObjectEntry(3, value2)));
  CHECK(cache.put(NumberAndObjectEntry(4, value2)));
  CHECK(cache.put(NumberAndObjectEntry(5, value2)));
  CHECK(cache.put(NumberAndObjectEntry(6, value2)));
  CHECK(cache.put(NumberAndObjectEntry(7, value2)));

  value2 = nullptr;
  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(!cache.has(NumberAndObjectLookup(2, value1)));
  CHECK(!cache.has(NumberAndObjectLookup(3, value1)));
  CHECK(!cache.has(NumberAndObjectLookup(4, value1)));
  CHECK(!cache.has(NumberAndObjectLookup(5, value1)));
  CHECK(!cache.has(NumberAndObjectLookup(6, value1)));

  auto ptr = cache.lookupForAdd(NumberAndObjectLookup(2, value1));
  CHECK(!ptr);
  CHECK(cache.add(ptr, NumberAndObjectEntry(2, value1)));

  auto ptr2 = cache.lookupForAdd(NumberAndObjectLookup(3, value1));
  CHECK(!ptr2);
  CHECK(cache.relookupOrAdd(ptr2, NumberAndObjectLookup(3, value1),
                            NumberAndObjectEntry(3, value1)));

  CHECK(cache.put(NumberAndObjectEntry(4, value1)));
  CHECK(cache.putNew(NumberAndObjectEntry(5, value1)));

  CHECK(cache.putNew(NumberAndObjectLookup(6, value1),
                     NumberAndObjectEntry(6, value1)));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 6);
  CHECK(cache.has(NumberAndObjectLookup(1, value1)));
  CHECK(cache.has(NumberAndObjectLookup(2, value1)));
  CHECK(cache.has(NumberAndObjectLookup(3, value1)));
  CHECK(cache.has(NumberAndObjectLookup(4, value1)));
  CHECK(cache.has(NumberAndObjectLookup(5, value1)));
  CHECK(cache.has(NumberAndObjectLookup(6, value1)));

  return true;
}

bool TestReplaceDyingInMap() {
  // Test replacing dying entries with ones that have the same key using the
  // various APIs.

  using Cache =
      JS::WeakCache<GCHashMap<uint32_t, JS::Heap<JSObject*>,
                              DefaultHasher<uint32_t>, TempAllocPolicy>>;
  Cache cache(JS::GetObjectZone(global), cx);

  RootedObject value1(cx, JS_NewPlainObject(cx));
  RootedObject value2(cx, JS_NewPlainObject(cx));
  CHECK(value1);
  CHECK(value2);

  CHECK(cache.put(1, value1));
  CHECK(cache.put(2, value2));
  CHECK(cache.put(3, value2));
  CHECK(cache.put(4, value2));
  CHECK(cache.put(5, value2));
  CHECK(cache.put(6, value2));

  value2 = nullptr;
  CHECK(GCUntilCacheSweep(cx, cache));

  CHECK(!cache.has(2));
  CHECK(!cache.has(3));
  CHECK(!cache.has(4));
  CHECK(!cache.has(5));
  CHECK(!cache.has(6));

  auto ptr = cache.lookupForAdd(2);
  CHECK(!ptr);
  CHECK(cache.add(ptr, 2, value1));

  auto ptr2 = cache.lookupForAdd(3);
  CHECK(!ptr2);
  CHECK(cache.add(ptr2, 3, JS::Heap<JSObject*>()));

  auto ptr3 = cache.lookupForAdd(4);
  CHECK(!ptr3);
  CHECK(cache.relookupOrAdd(ptr3, 4, value1));

  CHECK(cache.put(5, value1));
  CHECK(cache.putNew(6, value1));

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == 6);
  CHECK(cache.lookup(1)->value() == value1);
  CHECK(cache.lookup(2)->value() == value1);
  CHECK(cache.lookup(3)->value() == nullptr);
  CHECK(cache.lookup(4)->value() == value1);
  CHECK(cache.lookup(5)->value() == value1);
  CHECK(cache.lookup(6)->value() == value1);

  return true;
}

bool TestUniqueIDLookups() {
  // Test hash table lookups during incremental sweeping where the hash is
  // generated based on a unique ID. The problem is that the unique ID table
  // will have already been swept by this point so looking up a dead pointer
  // in the table will fail. This lookup happens if we try to match a live key
  // against a dead table entry with the same hash code.

  const size_t DeadFactor = 3;
  const size_t ObjectCount = 100;

  using Cache = JS::WeakCache<
      GCHashSet<ObjectEntry, MovableCellHasher<ObjectEntry>, TempAllocPolicy>>;
  Cache cache(JS::GetObjectZone(global), cx);

  Rooted<GCVector<JSObject*, 0, SystemAllocPolicy>> liveObjects(cx);

  for (size_t j = 0; j < ObjectCount; j++) {
    JSObject* obj = JS_NewPlainObject(cx);
    CHECK(obj);
    CHECK(cache.put(obj));
    if (j % DeadFactor == 0) {
      CHECK(liveObjects.get().append(obj));
    }
  }

  CHECK(cache.count() == ObjectCount);

  CHECK(GCUntilCacheSweep(cx, cache));

  for (size_t j = 0; j < liveObjects.length(); j++) {
    CHECK(cache.has(liveObjects[j]));
  }

  CHECK(SweepCacheAndFinishGC(cx, cache));

  CHECK(cache.count() == liveObjects.length());

  return true;
}

END_TEST(testIncrementalWeakCacheSweeping)

#endif  // defined JS_GC_ZEAL
