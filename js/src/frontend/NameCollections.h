/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_NameCollections_h
#define frontend_NameCollections_h

#include <type_traits>

#include "ds/InlineTable.h"
#include "frontend/NameAnalysisTypes.h"
#include "js/Vector.h"

namespace js {
namespace frontend {

class FunctionBox;

// A pool of recyclable containers for use in the frontend. The Parser and
// BytecodeEmitter create many maps for name analysis that are short-lived
// (i.e., for the duration of parsing or emitting a lexical scope). Making
// them recyclable cuts down significantly on allocator churn.
template <typename RepresentativeCollection, typename ConcreteCollectionPool>
class CollectionPool {
  using RecyclableCollections = Vector<void*, 32, SystemAllocPolicy>;

  RecyclableCollections all_;
  RecyclableCollections recyclable_;

  static RepresentativeCollection* asRepresentative(void* p) {
    return reinterpret_cast<RepresentativeCollection*>(p);
  }

  RepresentativeCollection* allocate() {
    size_t newAllLength = all_.length() + 1;
    if (!all_.reserve(newAllLength) || !recyclable_.reserve(newAllLength)) {
      return nullptr;
    }

    RepresentativeCollection* collection = js_new<RepresentativeCollection>();
    if (collection) {
      all_.infallibleAppend(collection);
    }
    return collection;
  }

 public:
  ~CollectionPool() { purgeAll(); }

  void purgeAll() {
    void** end = all_.end();
    for (void** it = all_.begin(); it != end; ++it) {
      js_delete(asRepresentative(*it));
    }

    all_.clearAndFree();
    recyclable_.clearAndFree();
  }

  // Fallibly aquire one of the supported collection types from the pool.
  template <typename Collection>
  Collection* acquire(JSContext* cx) {
    ConcreteCollectionPool::template assertInvariants<Collection>();

    RepresentativeCollection* collection;
    if (recyclable_.empty()) {
      collection = allocate();
      if (!collection) {
        ReportOutOfMemory(cx);
      }
    } else {
      collection = asRepresentative(recyclable_.popCopy());
      collection->clear();
    }
    return reinterpret_cast<Collection*>(collection);
  }

  // Release a collection back to the pool.
  template <typename Collection>
  void release(Collection** collection) {
    ConcreteCollectionPool::template assertInvariants<Collection>();
    MOZ_ASSERT(*collection);

#ifdef DEBUG
    bool ok = false;
    // Make sure the collection is in |all_| but not already in |recyclable_|.
    for (void** it = all_.begin(); it != all_.end(); ++it) {
      if (*it == *collection) {
        ok = true;
        break;
      }
    }
    MOZ_ASSERT(ok);
    for (void** it = recyclable_.begin(); it != recyclable_.end(); ++it) {
      MOZ_ASSERT(*it != *collection);
    }
#endif

    MOZ_ASSERT(recyclable_.length() < all_.length());
    // Reserved in allocateFresh.
    recyclable_.infallibleAppend(*collection);
    *collection = nullptr;
  }
};

template <typename Wrapped>
struct RecyclableAtomMapValueWrapper {
  using WrappedType = Wrapped;

  union {
    Wrapped wrapped;
    uint64_t dummy;
  };

  static void assertInvariant() {
    static_assert(sizeof(Wrapped) <= sizeof(uint64_t),
                  "Can only recycle atom maps with values smaller than uint64");
  }

  RecyclableAtomMapValueWrapper() : dummy(0) { assertInvariant(); }

  MOZ_IMPLICIT RecyclableAtomMapValueWrapper(Wrapped w) : wrapped(w) {
    assertInvariant();
  }

  MOZ_IMPLICIT operator Wrapped&() { return wrapped; }

  MOZ_IMPLICIT operator Wrapped&() const { return wrapped; }

  Wrapped* operator->() { return &wrapped; }

  const Wrapped* operator->() const { return &wrapped; }
};

struct NameMapHasher : public DefaultHasher<JSAtom*> {
  static inline HashNumber hash(const Lookup& l) {
    // Name maps use the atom's precomputed hash code, which is based on
    // the atom's contents rather than its pointer value. This is necessary
    // to preserve iteration order while recording/replaying: iteration can
    // affect generated script bytecode and the order in which e.g. lookup
    // property hooks are performed on the associated global.
    return l->hash();
  }
};

template <typename MapValue>
using RecyclableNameMap =
    InlineMap<JSAtom*, RecyclableAtomMapValueWrapper<MapValue>, 24,
              NameMapHasher, SystemAllocPolicy>;

using DeclaredNameMap = RecyclableNameMap<DeclaredNameInfo>;
using NameLocationMap = RecyclableNameMap<NameLocation>;
using AtomIndexMap = RecyclableNameMap<uint32_t>;

template <typename RepresentativeTable>
class InlineTablePool
    : public CollectionPool<RepresentativeTable,
                            InlineTablePool<RepresentativeTable>> {
  template <typename>
  struct IsRecyclableAtomMapValueWrapper : std::false_type {};

  template <typename T>
  struct IsRecyclableAtomMapValueWrapper<RecyclableAtomMapValueWrapper<T>>
      : std::true_type {};

 public:
  template <typename Table>
  static void assertInvariants() {
    static_assert(
        Table::SizeOfInlineEntries == RepresentativeTable::SizeOfInlineEntries,
        "Only tables with the same size for inline entries are usable in the "
        "pool.");

    using EntryType = typename Table::Table::Entry;
    using KeyType = typename EntryType::KeyType;
    using ValueType = typename EntryType::ValueType;

    static_assert(IsRecyclableAtomMapValueWrapper<ValueType>::value,
                  "Please adjust the static assertions below if you need to "
                  "support other types than RecyclableAtomMapValueWrapper");

    using WrappedType = typename ValueType::WrappedType;

    // We can't directly check |std::is_trivial<EntryType>|, because neither
    // mozilla::HashMapEntry nor IsRecyclableAtomMapValueWrapper are trivially
    // default constructible. Instead we check that the key and the unwrapped
    // value are trivial and additionally ensure that the entry itself is
    // trivially copyable and destructible.

    static_assert(std::is_trivial_v<KeyType>,
                  "Only tables with trivial keys are usable in the pool.");
    static_assert(std::is_trivial_v<WrappedType>,
                  "Only tables with trivial values are usable in the pool.");

    static_assert(
        std::is_trivially_copyable_v<EntryType>,
        "Only tables with trivially copyable entries are usable in the pool.");
    static_assert(std::is_trivially_destructible_v<EntryType>,
                  "Only tables with trivially destructible entries are usable "
                  "in the pool.");
  }
};

template <typename RepresentativeVector>
class VectorPool : public CollectionPool<RepresentativeVector,
                                         VectorPool<RepresentativeVector>> {
 public:
  template <typename Vector>
  static void assertInvariants() {
    static_assert(
        Vector::sMaxInlineStorage == RepresentativeVector::sMaxInlineStorage,
        "Only vectors with the same size for inline entries are usable in the "
        "pool.");

    using ElementType = typename Vector::ElementType;

    static_assert(std::is_trivial_v<ElementType>,
                  "Only vectors of trivial values are usable in the pool.");
    static_assert(std::is_trivially_destructible_v<ElementType>,
                  "Only vectors of trivially destructible values are usable in "
                  "the pool.");

    static_assert(
        sizeof(ElementType) ==
            sizeof(typename RepresentativeVector::ElementType),
        "Only vectors with same-sized elements are usable in the pool.");
  }
};

class NameCollectionPool {
  InlineTablePool<AtomIndexMap> mapPool_;
  VectorPool<AtomVector> vectorPool_;
  uint32_t activeCompilations_;

 public:
  NameCollectionPool() : activeCompilations_(0) {}

  bool hasActiveCompilation() const { return activeCompilations_ != 0; }

  void addActiveCompilation() { activeCompilations_++; }

  void removeActiveCompilation() {
    MOZ_ASSERT(hasActiveCompilation());
    activeCompilations_--;
  }

  template <typename Map>
  Map* acquireMap(JSContext* cx) {
    MOZ_ASSERT(hasActiveCompilation());
    return mapPool_.acquire<Map>(cx);
  }

  template <typename Map>
  void releaseMap(Map** map) {
    MOZ_ASSERT(hasActiveCompilation());
    MOZ_ASSERT(map);
    if (*map) {
      mapPool_.release(map);
    }
  }

  template <typename Vector>
  Vector* acquireVector(JSContext* cx) {
    MOZ_ASSERT(hasActiveCompilation());
    return vectorPool_.acquire<Vector>(cx);
  }

  template <typename Vector>
  void releaseVector(Vector** vec) {
    MOZ_ASSERT(hasActiveCompilation());
    MOZ_ASSERT(vec);
    if (*vec) {
      vectorPool_.release(vec);
    }
  }

  void purge() {
    if (!hasActiveCompilation()) {
      mapPool_.purgeAll();
      vectorPool_.purgeAll();
    }
  }
};

template <typename T, template <typename> typename Impl>
class PooledCollectionPtr {
  NameCollectionPool& pool_;
  T* collection_ = nullptr;

 protected:
  ~PooledCollectionPtr() { Impl<T>::releaseCollection(pool_, &collection_); }

  T& collection() {
    MOZ_ASSERT(collection_);
    return *collection_;
  }

  const T& collection() const {
    MOZ_ASSERT(collection_);
    return *collection_;
  }

 public:
  explicit PooledCollectionPtr(NameCollectionPool& pool) : pool_(pool) {}

  bool acquire(JSContext* cx) {
    MOZ_ASSERT(!collection_);
    collection_ = Impl<T>::acquireCollection(cx, pool_);
    return !!collection_;
  }

  explicit operator bool() const { return !!collection_; }

  T* operator->() { return &collection(); }

  const T* operator->() const { return &collection(); }

  T& operator*() { return collection(); }

  const T& operator*() const { return collection(); }
};

template <typename Map>
class PooledMapPtr : public PooledCollectionPtr<Map, PooledMapPtr> {
  friend class PooledCollectionPtr<Map, PooledMapPtr>;

  static Map* acquireCollection(JSContext* cx, NameCollectionPool& pool) {
    return pool.acquireMap<Map>(cx);
  }

  static void releaseCollection(NameCollectionPool& pool, Map** ptr) {
    pool.releaseMap(ptr);
  }

  using Base = PooledCollectionPtr<Map, PooledMapPtr>;

 public:
  using Base::Base;

  ~PooledMapPtr() = default;
};

template <typename Vector>
class PooledVectorPtr : public PooledCollectionPtr<Vector, PooledVectorPtr> {
  friend class PooledCollectionPtr<Vector, PooledVectorPtr>;

  static Vector* acquireCollection(JSContext* cx, NameCollectionPool& pool) {
    return pool.acquireVector<Vector>(cx);
  }

  static void releaseCollection(NameCollectionPool& pool, Vector** ptr) {
    pool.releaseVector(ptr);
  }

  using Base = PooledCollectionPtr<Vector, PooledVectorPtr>;
  using Base::collection;

 public:
  using Base::Base;

  ~PooledVectorPtr() = default;

  typename Vector::ElementType& operator[](size_t index) {
    return collection()[index];
  }

  const typename Vector::ElementType& operator[](size_t index) const {
    return collection()[index];
  }
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_NameCollections_h
