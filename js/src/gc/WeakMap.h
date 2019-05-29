/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_WeakMap_h
#define gc_WeakMap_h

#include "mozilla/LinkedList.h"

#include "gc/Barrier.h"
#include "gc/DeletePolicy.h"
#include "gc/Zone.h"
#include "js/HashTable.h"

namespace JS {
class Zone;
}  // namespace JS

namespace js {

class GCMarker;
class WeakMapBase;
struct WeakMapTracer;

namespace gc {

struct WeakMarkable;

#if defined(JS_GC_ZEAL) || defined(DEBUG)
// Check whether a weak map entry is marked correctly.
bool CheckWeakMapEntryMarking(const WeakMapBase* map, Cell* key, Cell* value);
#endif

}  // namespace gc

// A subclass template of js::HashMap whose keys and values may be
// garbage-collected. When a key is collected, the table entry disappears,
// dropping its reference to the value.
//
// More precisely:
//
//     A WeakMap entry is live if and only if both the WeakMap and the entry's
//     key are live. An entry holds a strong reference to its value.
//
// You must call this table's 'trace' method when its owning object is reached
// by the garbage collection tracer. Once a table is known to be live, the
// implementation takes care of the special weak marking (ie, marking through
// the implicit edges stored in the map) and of removing (sweeping) table
// entries when collection is complete.

typedef HashSet<WeakMapBase*, DefaultHasher<WeakMapBase*>, SystemAllocPolicy>
    WeakMapSet;

// Common base class for all WeakMap specializations, used for calling
// subclasses' GC-related methods.
class WeakMapBase : public mozilla::LinkedListElement<WeakMapBase> {
  friend class js::GCMarker;

 public:
  WeakMapBase(JSObject* memOf, JS::Zone* zone);
  virtual ~WeakMapBase();

  JS::Zone* zone() const { return zone_; }

  // Garbage collector entry points.

  // Unmark all weak maps in a zone.
  static void unmarkZone(JS::Zone* zone);

  // Trace all the weakmaps in a zone.
  static void traceZone(JS::Zone* zone, JSTracer* tracer);

  // Check all weak maps in a zone that have been marked as live in this garbage
  // collection, and mark the values of all entries that have become strong
  // references to them. Return true if we marked any new values, indicating
  // that we need to make another pass. In other words, mark my marked maps'
  // marked members' mid-collection.
  static bool markZoneIteratively(JS::Zone* zone, GCMarker* marker);

  // Add zone edges for weakmaps with key delegates in a different zone.
  static MOZ_MUST_USE bool findSweepGroupEdges(JS::Zone* zone);

  // Sweep the weak maps in a zone, removing dead weak maps and removing
  // entries of live weak maps whose keys are dead.
  static void sweepZone(JS::Zone* zone);

  // Trace all weak map bindings. Used by the cycle collector.
  static void traceAllMappings(WeakMapTracer* tracer);

  // Save information about which weak maps are marked for a zone.
  static bool saveZoneMarkedWeakMaps(JS::Zone* zone,
                                     WeakMapSet& markedWeakMaps);

  // Restore information about which weak maps are marked for many zones.
  static void restoreMarkedWeakMaps(WeakMapSet& markedWeakMaps);

#if defined(JS_GC_ZEAL) || defined(DEBUG)
  static bool checkMarkingForZone(JS::Zone* zone);
#endif

  static JSObject* getDelegate(JSObject* key);
  static JSObject* getDelegate(JSScript* script);
  static JSObject* getDelegate(LazyScript* script);

 protected:
  // Instance member functions called by the above. Instantiations of WeakMap
  // override these with definitions appropriate for their Key and Value types.
  virtual void trace(JSTracer* tracer) = 0;
  virtual bool findZoneEdges() = 0;
  virtual void sweep() = 0;
  virtual void traceMappings(WeakMapTracer* tracer) = 0;
  virtual void clearAndCompact() = 0;

  // Any weakmap key types that want to participate in the non-iterative
  // ephemeron marking must override this method.
  virtual void markEntry(GCMarker* marker, gc::Cell* markedCell,
                         JS::GCCellPtr l) = 0;

  virtual bool markIteratively(GCMarker* marker) = 0;

#ifdef JS_GC_ZEAL
  virtual bool checkMarking() const = 0;
  virtual bool allowKeysInOtherZones() const { return false; }
  friend bool gc::CheckWeakMapEntryMarking(const WeakMapBase*, gc::Cell*,
                                           gc::Cell*);
#endif

  // Object that this weak map is part of, if any.
  GCPtrObject memberOf;

  // Zone containing this weak map.
  JS::Zone* zone_;

  // Whether this object has been marked during garbage collection and which
  // color it was marked.
  bool marked;
  gc::MarkColor markColor;
};

template <class Key, class Value>
class WeakMap
    : private HashMap<Key, Value, MovableCellHasher<Key>, ZoneAllocPolicy>,
      public WeakMapBase {
 public:
  using Base = HashMap<Key, Value, MovableCellHasher<Key>, ZoneAllocPolicy>;

  using Lookup = typename Base::Lookup;
  using Entry = typename Base::Entry;
  using Range = typename Base::Range;
  using Ptr = typename Base::Ptr;
  using AddPtr = typename Base::AddPtr;

  struct Enum : public Base::Enum {
    explicit Enum(WeakMap& map) : Base::Enum(static_cast<Base&>(map)) {}
  };

  using Base::all;
  using Base::clear;
  using Base::has;
  using Base::shallowSizeOfExcludingThis;

  // Resolve ambiguity with LinkedListElement<>::remove.
  using Base::remove;

  explicit WeakMap(JSContext* cx, JSObject* memOf = nullptr);

  // Add a read barrier to prevent an incorrectly gray value from escaping the
  // weak map. See the UnmarkGrayTracer::onChild comment in gc/Marking.cpp.
  Ptr lookup(const Lookup& l) const {
    Ptr p = Base::lookup(l);
    if (p) {
      exposeGCThingToActiveJS(p->value());
    }
    return p;
  }

  AddPtr lookupForAdd(const Lookup& l) {
    AddPtr p = Base::lookupForAdd(l);
    if (p) {
      exposeGCThingToActiveJS(p->value());
    }
    return p;
  }

  template <typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool put(KeyInput&& key, ValueInput&& value) {
    MOZ_ASSERT(key);
    return Base::put(std::forward<KeyInput>(key),
                     std::forward<ValueInput>(value));
  }

  template <typename KeyInput, typename ValueInput>
  MOZ_MUST_USE bool relookupOrAdd(AddPtr& ptr, KeyInput&& key,
                                  ValueInput&& value) {
    MOZ_ASSERT(key);
    return Base::relookupOrAdd(ptr, std::forward<KeyInput>(key),
                               std::forward<ValueInput>(value));
  }

  void markEntry(GCMarker* marker, gc::Cell* markedCell,
                 JS::GCCellPtr origKey) override;

  void trace(JSTracer* trc) override;

 protected:
  static void addWeakEntry(GCMarker* marker, JS::GCCellPtr key,
                           const gc::WeakMarkable& markable);

  bool markIteratively(GCMarker* marker) override;

  /**
   * If a wrapper is used as a key in a weakmap, the garbage collector should
   * keep that object around longer than it otherwise would. We want to avoid
   * collecting the wrapper (and removing the weakmap entry) as long as the
   * wrapped object is alive (because the object can be rewrapped and looked up
   * again). As long as the wrapper is used as a weakmap key, it will not be
   * collected (and remain in the weakmap) until the wrapped object is
   * collected.
   */
 private:
  void exposeGCThingToActiveJS(const JS::Value& v) const {
    JS::ExposeValueToActiveJS(v);
  }
  void exposeGCThingToActiveJS(JSObject* obj) const {
    JS::ExposeObjectToActiveJS(obj);
  }

  bool keyNeedsMark(GCMarker* marker, JSObject* key) const;
  bool keyNeedsMark(GCMarker* marker, JSScript* script) const;
  bool keyNeedsMark(GCMarker* marker, LazyScript* script) const;

  bool findZoneEdges() override {
    // This is overridden by ObjectValueMap.
    return true;
  }

  void sweep() override;

  void clearAndCompact() override {
    Base::clear();
    Base::compact();
  }

  // memberOf can be nullptr, which means that the map is not part of a
  // JSObject.
  void traceMappings(WeakMapTracer* tracer) override;

 protected:
#if DEBUG
  void assertEntriesNotAboutToBeFinalized();
#endif

#ifdef JS_GC_ZEAL
  bool checkMarking() const override;
#endif
};

class ObjectValueMap : public WeakMap<HeapPtr<JSObject*>, HeapPtr<Value>> {
 public:
  ObjectValueMap(JSContext* cx, JSObject* obj) : WeakMap(cx, obj) {}

  bool findZoneEdges() override;

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

// Generic weak map for mapping objects to other objects.
class ObjectWeakMap {
  ObjectValueMap map;

 public:
  explicit ObjectWeakMap(JSContext* cx);

  JS::Zone* zone() const { return map.zone(); }

  JSObject* lookup(const JSObject* obj);
  bool add(JSContext* cx, JSObject* obj, JSObject* target);
  void clear();

  void trace(JSTracer* trc);
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
  }

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAfterMovingGC();
#endif
};

} /* namespace js */

namespace JS {

template <>
struct DeletePolicy<js::ObjectValueMap>
    : public js::GCManagedDeletePolicy<js::ObjectValueMap> {};

} /* namespace JS */

#endif /* gc_WeakMap_h */
