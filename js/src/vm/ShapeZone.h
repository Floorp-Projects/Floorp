/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ShapeZone_h
#define vm_ShapeZone_h

#include "mozilla/MemoryReporting.h"

#include "gc/Barrier.h"
#include "js/GCHashTable.h"
#include "vm/PropertyKey.h"
#include "vm/Shape.h"
#include "vm/TaggedProto.h"

namespace js {

class PropertyTree {
  friend class ::JSFunction;

#ifdef DEBUG
  JS::Zone* zone_;
#endif

  bool insertChild(JSContext* cx, Shape* parent, Shape* child);

  PropertyTree();

 public:
  /*
   * Use a lower limit for objects that are accessed using SETELEM (o[x] = y).
   * These objects are likely used as hashmaps and dictionary mode is more
   * efficient in this case.
   */
  enum { MAX_HEIGHT = 512, MAX_HEIGHT_WITH_ELEMENTS_ACCESS = 128 };

  explicit PropertyTree(JS::Zone* zone)
#ifdef DEBUG
      : zone_(zone)
#endif
  {
  }

  MOZ_ALWAYS_INLINE Shape* inlinedGetChild(JSContext* cx, Shape* parent,
                                           JS::Handle<StackShape> childSpec);
  Shape* getChild(JSContext* cx, Shape* parent, JS::Handle<StackShape> child);
};

// Hash policy for the per-zone baseShapes set.
struct BaseShapeHasher {
  struct Lookup {
    const JSClass* clasp;
    JS::Realm* realm;
    TaggedProto proto;

    Lookup(const JSClass* clasp, JS::Realm* realm, TaggedProto proto)
        : clasp(clasp), realm(realm), proto(proto) {}
  };

  static HashNumber hash(const Lookup& lookup) {
    HashNumber hash = MovableCellHasher<TaggedProto>::hash(lookup.proto);
    return mozilla::AddToHash(hash, lookup.clasp, lookup.realm);
  }
  static bool match(const WeakHeapPtr<BaseShape*>& key, const Lookup& lookup) {
    return key.unbarrieredGet()->clasp() == lookup.clasp &&
           key.unbarrieredGet()->realm() == lookup.realm &&
           key.unbarrieredGet()->proto() == lookup.proto;
  }
};
using BaseShapeSet = JS::WeakCache<
    JS::GCHashSet<WeakHeapPtr<BaseShape*>, BaseShapeHasher, SystemAllocPolicy>>;

// Hash policy for the per-zone initialShapes set storing initial shapes for
// objects in the zone.
//
// These are empty shapes, except for certain classes (e.g. String, RegExp)
// which may add certain baked-in properties. See insertInitialShape.
struct InitialShapeHasher {
  struct Lookup {
    const JSClass* clasp;
    JS::Realm* realm;
    TaggedProto proto;
    uint32_t nfixed;
    ObjectFlags objectFlags;

    Lookup(const JSClass* clasp, JS::Realm* realm, const TaggedProto& proto,
           uint32_t nfixed, ObjectFlags objectFlags)
        : clasp(clasp),
          realm(realm),
          proto(proto),
          nfixed(nfixed),
          objectFlags(objectFlags) {}
  };

  static HashNumber hash(const Lookup& lookup) {
    HashNumber hash = MovableCellHasher<TaggedProto>::hash(lookup.proto);
    return mozilla::AddToHash(hash, lookup.clasp, lookup.realm, lookup.nfixed,
                              lookup.objectFlags.toRaw());
  }
  static bool match(const WeakHeapPtr<Shape*>& key, const Lookup& lookup) {
    const Shape* shape = key.unbarrieredGet();
    return lookup.clasp == shape->getObjectClass() &&
           lookup.realm == shape->realm() && lookup.proto == shape->proto() &&
           lookup.nfixed == shape->numFixedSlots() &&
           lookup.objectFlags == shape->objectFlags();
  }
};
using InitialShapeSet = JS::WeakCache<
    JS::GCHashSet<WeakHeapPtr<Shape*>, InitialShapeHasher, SystemAllocPolicy>>;

// Per-Zone information about Shapes and BaseShapes.
struct ShapeZone {
  // Shared Shape property tree.
  PropertyTree propertyTree;

  // Set of all base shapes in the Zone.
  BaseShapeSet baseShapes;

  // Set of initial shapes in the Zone.
  InitialShapeSet initialShapes;

  explicit ShapeZone(Zone* zone);

  void clearTables();
  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkTablesAfterMovingGC();
#endif
};

}  // namespace js

#endif /* vm_ShapeZone_h */
