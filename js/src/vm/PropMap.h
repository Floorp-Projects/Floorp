/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PropMap_h
#define vm_PropMap_h

#include "gc/Cell.h"
#include "js/UbiNode.h"

// [SMDOC] Property Maps
//
// Property maps are used to store information about native object properties.
// Each property map represents an ordered list of (PropertyKey, PropertyInfo)
// tuples.
//
// Each property map can store up to 8 properties (see PropMap::Capacity). To
// store more than eight properties, multiple maps must be linked together with
// the |previous| pointer.
//
// Shapes and Property Maps
// ------------------------
// Native object shapes represent property information as a (PropMap*, length)
// tuple. When there are no properties yet, the shape's map will be nullptr and
// the length is zero.
//
// For example, consider the following objects:
//
//   o1 = {x: 1, y: 2}
//   o2 = {x: 3, y: 4, z: 5}
//
// This is stored as follows:
//
//   +-------------+      +--------------+     +-------------------+
//   | JSObject o1 |      | Shape S1     |     | PropMap M1        |
//   |-------------+      +--------------+     +-------------------+
//   | shape: S1  -+--->  | map: M1     -+--+> | key 0: x (slot 0) |
//   | slot 0: 1   |      | mapLength: 2 |  |  | key 1: y (slot 1) |
//   | slot 1: 2   |      +--------------+  |  | key 2: z (slot 2) |
//   +-------------+                        |  | ...               |
//                                          |  +-------------------+
//                                          |
//   +-------------+      +--------------+  |
//   | JSObject o2 |      | Shape S2     |  |
//   |-------------+      +--------------+  |
//   | shape: S2  -+--->  | map: M1     -+--+
//   | slot 0: 3   |      | mapLength: 3 |
//   | slot 1: 4   |      +--------------+
//   | slot 2: 5   |
//   +-------------+
//
// There's a single map M1 shared by shapes S1 and S2. Shape S1 includes only
// the first two properties and shape S2 includes all three properties.
//
// Class Hierarchy
// ---------------
// Property maps have the following C++ class hierarchy:
//
//   PropMap (abstract)
//    |
//    +-- SharedPropMap (abstract)
//    |      |
//    |      +-- CompactPropMap
//    |      |
//    |      +-- NormalPropMap
//    |
//    +-- DictionaryPropMap
//
// * PropMap: base class. It has a flags word and an array of PropertyKeys.
//
// * SharedPropMap: base class for all shared property maps. See below for more
//                  information on shared maps.
//
// * CompactPropMap: a shared map that stores its information more compactly
//                   than the other maps. It saves four words by not storing a
//                   PropMapTable, previous pointer, and by using a more compact
//                   PropertyInfo type for slot numbers that fit in one byte.
//
// * NormalPropMap: a shared map, used when CompactPropMap can't be used.
//
// * DictionaryPropMap: an unshared map (used by a single object/shape). See
//                      below for more information on dictionary maps.
//
// Secondary hierarchy
// -------------------
// NormalPropMap and DictionaryPropMap store property information in the same
// way. This means property lookups don't have to distinguish between these two
// types. This is represented with a second class hierarchy:
//
//   PropMap (abstract)
//    |
//    +-- CompactPropMap
//    |
//    +-- LinkedPropMap (NormalPropMap or DictionaryPropMap)
//
// Property lookup and property iteration are very performance sensitive and use
// this Compact vs Linked "view" so that they don't have to handle the three map
// types separately.
//
// LinkedPropMap also stores the PropMapTable and a pointer to the |previous|
// map. Compact maps don't have these fields.
//
// To summarize these map types:
//
//   +-------------------+-------------+--------+
//   | Concrete type     | Shared/tree | Linked |
//   +-------------------+-------------+--------+
//   | CompactPropMap    | yes         | no     |
//   | NormalPropMap     | yes         | yes    |
//   | DictionaryPropMap | no          | yes    |
//   +-------------------+-------------+--------+
//
// PropMapTable
// ------------
// Finding the PropertyInfo for a particular PropertyKey requires a linear
// search if the map is small. For larger maps we can create a PropMapTable, a
// hash table that maps from PropertyKey to PropMap + index, to speed up
// property lookups.
//
// To save memory, property map tables can be discarded on GC and recreated when
// needed. AutoKeepPropMapTables can be used to avoid discarding tables in a
// particular zone. Methods to access a PropMapTable take either an
// AutoCheckCannotGC or AutoKeepPropMapTables argument, to help ensure tables
// are not purged while we're using them.
//
// Shared Property Maps
// --------------------
// Shared property maps can be shared per-Zone by objects with the same property
// keys, flags, and slot numbers. To make this work, shared maps form a tree:
//
// - Each Zone has a table that maps from first PropertyKey + PropertyInfo to
//   a SharedPropMap that begins with that property. This is used to lookup the
//   the map to use when adding the first property.
//   See ShapeZone::initialPropMaps.
//
// - When adding a property other than the first one, the property is stored in
//   the next entry of the same map when possible. If the map is full or the
//   next entry already stores a different property, a child map is created and
//   linked to the parent map.
//
// For example, imagine we want to create these objects:
//
//   o1 = {x: 1, y: 2, z: 3}
//   o2 = {x: 1, y: 2, foo: 4}
//
// This will result in the following maps being created:
//
//     +---------------------+    +---------------------+
//     | SharedPropMap M1    |    | SharedPropMap M2    |
//     +---------------------+    +---------------------+
//     | Child M2 (index 1) -+--> | Parent M1 (index 1) |
//     +---------------------+    +---------------------+
//     | 0: x                |    | 0: x                |
//     | 1: y                |    | 1: y                |
//     | 2: z                |    | 2: foo              |
//     | ...                 |    | ...                 |
//     +---------------------+    +---------------------+
//
// M1 is the map used for initial property "x". Properties "y" and "z" can be
// stored inline. When later adding "foo" following "y", the map has to be
// forked: a child map M2 is created and M1 remembers this transition at
// property index 1 so that M2 will be used the next time properties "x", "y",
// and "foo" are added to an object.
//
// Shared maps contain a TreeData struct that stores the parent and children
// links for the SharedPropMap tree. The parent link is a tagged pointer that
// stores both the parent map and the property index of the last used property
// in the parent map before the branch. The children are stored similarly: the
// parent map can store a single child map and index, or a set of children.
// See SharedChildrenPtr.
//
// Looking up a child map can then be done based on the index of the last
// property in the parent map and the new property's key and flags. So for the
// example above, the lookup key for M1 => M2 is (index 1, "foo", <flags>).
//
// Note: shared maps can have both a |previous| map and a |parent| map. They are
// equal when the previous map was full, but can be different maps when
// branching in the middle of a map like in the example above: M2 has parent M1
// but does not have a |previous| map (because it only has three properties).
//
// Dictionary Property Maps
// ------------------------
// Certain operations can't be implemented (efficiently) for shared property
// maps, for example changing or deleting a property other than the last one.
// When this happens the map is copied as a DictionaryPropMap.
//
// Dictionary maps are unshared so can be mutated in place (after generating a
// new shape for the object).
//
// Unlike shared maps, dictionary maps can have holes between two property keys
// after removing a property. When there are more holes than properties, the
// map is compacted. See DictionaryPropMap::maybeCompact.

namespace js {

class DictionaryPropMap;
class SharedPropMap;
class LinkedPropMap;
class CompactPropMap;
class NormalPropMap;

// Template class for storing a PropMap* and a property index as tagged pointer.
template <typename T>
class MapAndIndex {
  uintptr_t data_ = 0;

  static constexpr uintptr_t IndexMask = 0b111;

 public:
  MapAndIndex() = default;

  MapAndIndex(const T* map, uint32_t index) : data_(uintptr_t(map) | index) {
    MOZ_ASSERT((uintptr_t(map) & IndexMask) == 0);
    MOZ_ASSERT(index <= IndexMask);
  }
  explicit MapAndIndex(uintptr_t data) : data_(data) {}

  void setNone() { data_ = 0; }

  bool isNone() const { return data_ == 0; }

  uintptr_t raw() const { return data_; }
  T* maybeMap() const { return reinterpret_cast<T*>(data_ & ~IndexMask); }

  uint32_t index() const {
    MOZ_ASSERT(!isNone());
    return data_ & IndexMask;
  }
  T* map() const {
    MOZ_ASSERT(!isNone());
    return maybeMap();
  }

  bool operator==(const MapAndIndex<T>& other) const {
    return data_ == other.data_;
  }
  bool operator!=(const MapAndIndex<T>& other) const {
    return !operator==(other);
  }
} JS_HAZ_GC_POINTER;
using PropMapAndIndex = MapAndIndex<PropMap>;
using SharedPropMapAndIndex = MapAndIndex<SharedPropMap>;

class PropMap : public gc::TenuredCellWithFlags {
 public:
  static constexpr size_t Capacity = 8;

 protected:
  // XXX temporary
  uint64_t padding1;
  uint64_t padding2;

  static_assert(gc::CellFlagBitsReservedForGC == 3,
                "PropMap must reserve enough bits for Cell");
  // Flags word, stored in the cell header. Note that this hides the
  // Cell::flags() method.
  uintptr_t flags() const { return headerFlagsField(); }

 public:
  static const JS::TraceKind TraceKind = JS::TraceKind::PropMap;

  void traceChildren(JSTracer* trc);
};

class SharedPropMap : public PropMap {
 public:
  void fixupAfterMovingGC();
  void sweep(JSFreeOp* fop);
  void finalize(JSFreeOp* fop);
};

class CompactPropMap final : public SharedPropMap {};

class LinkedPropMap final : public PropMap {};

class NormalPropMap final : public SharedPropMap {};

class DictionaryPropMap final : public PropMap {
 public:
  void fixupAfterMovingGC();
  void sweep(JSFreeOp* fop);
  void finalize(JSFreeOp* fop);
};

}  // namespace js

// JS::ubi::Nodes can point to PropMaps; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::PropMap> : TracerConcrete<js::PropMap> {
 protected:
  explicit Concrete(js::PropMap* ptr) : TracerConcrete<js::PropMap>(ptr) {}

 public:
  static void construct(void* storage, js::PropMap* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif  // vm_PropMap_h
