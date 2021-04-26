/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Shape_h
#define vm_Shape_h

#include "js/shadow/Shape.h"  // JS::shadow::Shape, JS::shadow::BaseShape

#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TemplateLib.h"

#include <algorithm>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/Barrier.h"
#include "gc/FreeOp.h"
#include "gc/MaybeRooted.h"
#include "gc/Rooting.h"
#include "js/HashTable.h"
#include "js/Id.h"  // JS::PropertyKey
#include "js/MemoryMetrics.h"
#include "js/RootingAPI.h"
#include "js/UbiNode.h"
#include "util/EnumFlags.h"
#include "vm/JSAtom.h"
#include "vm/Printer.h"
#include "vm/StringType.h"
#include "vm/SymbolType.h"

/*
 * [SMDOC] Shapes
 *
 * In isolation, a Shape represents a property that exists in one or more
 * objects; it has an id, flags, etc. (But it doesn't represent the property's
 * value.)  However, Shapes are always stored in linked linear sequence of
 * Shapes, called "shape lineages". Each shape lineage represents the layout of
 * an entire object.
 *
 * Every JSObject has a pointer, |shape_|, accessible via lastProperty(), to
 * the last Shape in a shape lineage, which identifies the property most
 * recently added to the object.  This pointer permits fast object layout
 * tests. The shape lineage order also dictates the enumeration order for the
 * object; ECMA requires no particular order but this implementation has
 * promised and delivered property definition order.
 *
 * Shape lineages occur in two kinds of data structure.
 *
 * 1. N-ary property trees. Each path from a non-root node to the root node in
 *    a property tree is a shape lineage. Property trees permit full (or
 *    partial) sharing of Shapes between objects that have fully (or partly)
 *    identical layouts. The root is an EmptyShape whose identity is determined
 *    by the object's class, compartment and prototype. These Shapes are shared
 *    and immutable.
 *
 * 2. Dictionary mode lists. Shapes in such lists are said to be "in
 *    dictionary mode", as are objects that point to such Shapes. These Shapes
 *    are unshared, private to a single object, and mutable. (Mutations do
 *    require changing the object's last-property shape, to properly invalidate
 *    JIT inline caches and other shape guards.)
 *
 * All shape lineages are bi-directionally linked, via the |parent| and
 * |children|/|listp| members.
 *
 * Shape lineages start out life in the property tree. They can be converted
 * (by copying) to dictionary mode lists in the following circumstances.
 *
 * 1. The shape lineage's size reaches MAX_HEIGHT. This reasonable limit avoids
 *    potential worst cases involving shape lineage mutations.
 *
 * 2. A property represented by a non-last Shape in a shape lineage is removed
 *    from an object. (In the last Shape case, obj->shape_ can be easily
 *    adjusted to point to obj->shape_->parent.)  We originally tried lazy
 *    forking of the property tree, but this blows up for delete/add
 *    repetitions.
 *
 * 3. A property represented by a non-last Shape in a shape lineage has its
 *    attributes modified.
 *
 * To find the Shape for a particular property of an object initially requires
 * a linear search. But if the number of searches starting at any particular
 * Shape in the property tree exceeds LINEAR_SEARCHES_MAX and the Shape's
 * lineage has (excluding the EmptyShape) at least MIN_ENTRIES, we create an
 * auxiliary hash table -- the ShapeTable -- that allows faster lookup.
 * Furthermore, a ShapeTable is always created for dictionary mode lists,
 * and it is attached to the last Shape in the lineage. Shape tables for
 * property tree Shapes never change, but shape tables for dictionary mode
 * Shapes can grow and shrink.
 *
 * To save memory, shape tables can be discarded on GC and recreated when
 * needed. AutoKeepShapeCaches can be used to avoid discarding shape tables
 * for a particular zone. Methods operating on ShapeTables take either an
 * AutoCheckCannotGC or AutoKeepShapeCaches argument, to help ensure tables
 * are not purged while we're using them.
 *
 * There used to be a long, math-heavy comment here explaining why property
 * trees are more space-efficient than alternatives.  This was removed in bug
 * 631138; see that bug for the full details.
 *
 * Because many Shapes have similar data, there is actually a secondary type
 * called a BaseShape that holds some of a Shape's data.  Many shapes can share
 * a single BaseShape.
 */

MOZ_ALWAYS_INLINE size_t JSSLOT_FREE(const JSClass* clasp) {
  // Proxy classes have reserved slots, but proxies manage their own slot
  // layout.
  MOZ_ASSERT(!clasp->isProxyObject());
  return JSCLASS_RESERVED_SLOTS(clasp);
}

namespace js {

/* Limit on the number of slotful properties in an object. */
static const uint32_t SHAPE_INVALID_SLOT = Bit(24) - 1;
static const uint32_t SHAPE_MAXIMUM_SLOT = Bit(24) - 2;

class Shape;
struct StackShape;

// ShapeProperty contains information (attributes, slot number) for a property
// stored in the Shape tree. Property lookups on NativeObjects return a
// ShapeProperty.
class ShapeProperty {
  uint32_t slot_;
  uint8_t attrs_;

 public:
  ShapeProperty(uint8_t attrs, uint32_t slot) : slot_(slot), attrs_(attrs) {}

  // Note: this returns true only for plain data properties with a slot. Returns
  // false for custom data properties. See JSPROP_CUSTOM_DATA_PROP.
  bool isDataProperty() const {
    return !(attrs_ &
             (JSPROP_GETTER | JSPROP_SETTER | JSPROP_CUSTOM_DATA_PROP));
  }
  bool isCustomDataProperty() const { return attrs_ & JSPROP_CUSTOM_DATA_PROP; }
  bool isAccessorProperty() const {
    return attrs_ & (JSPROP_GETTER | JSPROP_SETTER);
  }

  // Note: unlike isDataProperty, this returns true also for custom data
  // properties. See JSPROP_CUSTOM_DATA_PROP.
  bool isDataDescriptor() const {
    return isDataProperty() || isCustomDataProperty();
  }

  bool hasSlot() const { return !isCustomDataProperty(); }

  uint32_t slot() const {
    MOZ_ASSERT(hasSlot());
    MOZ_ASSERT(slot_ < SHAPE_INVALID_SLOT);
    return slot_;
  }

  uint8_t attributes() const { return attrs_; }
  bool writable() const { return !(attrs_ & JSPROP_READONLY); }
  bool configurable() const { return !(attrs_ & JSPROP_PERMANENT); }
  bool enumerable() const { return attrs_ & JSPROP_ENUMERATE; }

  bool operator==(const ShapeProperty& other) const {
    return slot_ == other.slot_ && attrs_ == other.attrs_;
  }
  bool operator!=(const ShapeProperty& other) const {
    return !operator==(other);
  }
};

class ShapePropertyWithKey : public ShapeProperty {
  PropertyKey key_;

 public:
  ShapePropertyWithKey(uint8_t attrs, uint32_t slot, PropertyKey key)
      : ShapeProperty(attrs, slot), key_(key) {}

  PropertyKey key() const { return key_; }

  void trace(JSTracer* trc) {
    TraceRoot(trc, &key_, "ShapePropertyWithKey-key");
  }
};

template <class Wrapper>
class WrappedPtrOperations<ShapePropertyWithKey, Wrapper> {
  const ShapePropertyWithKey& value() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isDataProperty() const { return value().isDataProperty(); }
  uint32_t slot() const { return value().slot(); }
  PropertyKey key() const { return value().key(); }
  uint8_t attributes() const { return value().attributes(); }
};

struct ShapeHasher : public DefaultHasher<Shape*> {
  using Key = Shape*;
  using Lookup = StackShape;

  static MOZ_ALWAYS_INLINE HashNumber hash(const Lookup& l);
  static MOZ_ALWAYS_INLINE bool match(Key k, const Lookup& l);
};

using ShapeSet = HashSet<Shape*, ShapeHasher, SystemAllocPolicy>;

// A tagged pointer to null, a single child, or a many-children data structure.
class ShapeChildren {
  // Tag bits must not overlap with DictionaryShapeLink.
  enum { SINGLE_SHAPE = 0, SHAPE_SET = 1, MASK = 3 };

  uintptr_t bits = 0;

 public:
  bool isNone() const { return !bits; }
  void setNone() { bits = 0; }

  bool isSingleShape() const {
    return (bits & MASK) == SINGLE_SHAPE && !isNone();
  }
  Shape* toSingleShape() const {
    MOZ_ASSERT(isSingleShape());
    return reinterpret_cast<Shape*>(bits & ~uintptr_t(MASK));
  }
  void setSingleShape(Shape* shape) {
    MOZ_ASSERT(shape);
    MOZ_ASSERT((uintptr_t(shape) & MASK) == 0);
    bits = uintptr_t(shape) | SINGLE_SHAPE;
  }

  bool isShapeSet() const { return (bits & MASK) == SHAPE_SET; }
  ShapeSet* toShapeSet() const {
    MOZ_ASSERT(isShapeSet());
    return reinterpret_cast<ShapeSet*>(bits & ~uintptr_t(MASK));
  }
  void setShapeSet(ShapeSet* hash) {
    MOZ_ASSERT(hash);
    MOZ_ASSERT((uintptr_t(hash) & MASK) == 0);
    bits = uintptr_t(hash) | SHAPE_SET;
  }

#ifdef DEBUG
  void checkHasChild(Shape* child) const;
#endif
} JS_HAZ_GC_POINTER;

// For dictionary mode shapes, a tagged pointer to the next shape or associated
// object if this is the last shape.
class DictionaryShapeLink {
  // Tag bits must not overlap with ShapeChildren.
  enum { SHAPE = 2, OBJECT = 3, MASK = 3 };

  uintptr_t bits = 0;

 public:
  // XXX Using = default on the default ctor causes rooting hazards for some
  // reason.
  DictionaryShapeLink() {}
  explicit DictionaryShapeLink(JSObject* obj) { setObject(obj); }
  explicit DictionaryShapeLink(Shape* shape) { setShape(shape); }

  bool isNone() const { return !bits; }
  void setNone() { bits = 0; }

  bool isShape() const { return (bits & MASK) == SHAPE; }
  Shape* toShape() const {
    MOZ_ASSERT(isShape());
    return reinterpret_cast<Shape*>(bits & ~uintptr_t(MASK));
  }
  void setShape(Shape* shape) {
    MOZ_ASSERT(shape);
    MOZ_ASSERT((uintptr_t(shape) & MASK) == 0);
    bits = uintptr_t(shape) | SHAPE;
  }

  bool isObject() const { return (bits & MASK) == OBJECT; }
  JSObject* toObject() const {
    MOZ_ASSERT(isObject());
    return reinterpret_cast<JSObject*>(bits & ~uintptr_t(MASK));
  }
  void setObject(JSObject* obj) {
    MOZ_ASSERT(obj);
    MOZ_ASSERT((uintptr_t(obj) & MASK) == 0);
    bits = uintptr_t(obj) | OBJECT;
  }

  bool operator==(const DictionaryShapeLink& other) const {
    return bits == other.bits;
  }
  bool operator!=(const DictionaryShapeLink& other) const {
    return !((*this) == other);
  }

  inline Shape* prev();
  inline void setPrev(Shape* shape);
} JS_HAZ_GC_POINTER;

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

class TenuringTracer;

class AutoKeepShapeCaches;

/*
 * ShapeIC uses a small array that is linearly searched.
 */
class ShapeIC {
 public:
  friend class NativeObject;
  friend class BaseShape;
  friend class Shape;

  ShapeIC() : size_(0), nextFreeIndex_(0), entries_(nullptr) {}

  ~ShapeIC() = default;

  bool isFull() const {
    MOZ_ASSERT(nextFreeIndex_ <= size_);
    return size_ == nextFreeIndex_;
  }

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + mallocSizeOf(entries_.get());
  }

  uint32_t entryCount() { return nextFreeIndex_; }

  bool init(JSContext* cx);
  void trace(JSTracer* trc);

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAfterMovingGC();
#endif

  MOZ_ALWAYS_INLINE bool search(jsid id, Shape** foundShape);

  MOZ_ALWAYS_INLINE bool appendEntry(jsid id, Shape* shape) {
    MOZ_ASSERT(nextFreeIndex_ <= size_);
    if (nextFreeIndex_ == size_) {
      return false;
    }

    entries_[nextFreeIndex_].id_ = id;
    entries_[nextFreeIndex_].shape_ = shape;
    nextFreeIndex_++;
    return true;
  }

 private:
  static const uint32_t MAX_SIZE = 7;

  class Entry {
   public:
    jsid id_;
    Shape* shape_;

    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
  };

  uint8_t size_;
  uint8_t nextFreeIndex_;

  /* table of ptrs to {jsid,Shape*} pairs */
  UniquePtr<Entry[], JS::FreePolicy> entries_;
};

class ShapeTable {
 public:
  friend class NativeObject;
  friend class BaseShape;
  friend class Shape;
  friend class ShapeCachePtr;

 private:
  struct Hasher : public DefaultHasher<Shape*> {
    using Key = Shape*;
    using Lookup = PropertyKey;
    static MOZ_ALWAYS_INLINE HashNumber hash(PropertyKey key);
    static MOZ_ALWAYS_INLINE bool match(Shape* shape, PropertyKey key);
  };
  using Set = HashSet<Shape*, Hasher, SystemAllocPolicy>;
  Set set_;

  // SHAPE_INVALID_SLOT or head of slot freelist in owning dictionary-mode
  // object.
  uint32_t freeList_ = SHAPE_INVALID_SLOT;

  MOZ_ALWAYS_INLINE Set::Ptr searchUnchecked(jsid id) {
    return set_.lookup(id);
  }

 public:
  using Ptr = Set::Ptr;

  ShapeTable() = default;
  ~ShapeTable() = default;

  uint32_t entryCount() const { return set_.count(); }

  uint32_t freeList() const { return freeList_; }
  void setFreeList(uint32_t slot) { freeList_ = slot; }

  // This counts the ShapeTable object itself (which must be heap-allocated) and
  // its HashSet.
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + set_.shallowSizeOfExcludingThis(mallocSizeOf);
  }

  // init() is fallible and reports OOM to the context.
  bool init(JSContext* cx, Shape* lastProp);

  MOZ_ALWAYS_INLINE Set::Ptr search(jsid id, const AutoKeepShapeCaches&) {
    return searchUnchecked(id);
  }
  MOZ_ALWAYS_INLINE Set::Ptr search(jsid id, const JS::AutoCheckCannotGC&) {
    return searchUnchecked(id);
  }

  bool add(JSContext* cx, PropertyKey key, Shape* shape) {
    if (!set_.putNew(key, shape)) {
      ReportOutOfMemory(cx);
      return false;
    }
    return true;
  }

  void remove(Ptr ptr) { set_.remove(ptr); }
  void remove(PropertyKey key) { set_.remove(key); }

  void replaceShape(Ptr ptr, PropertyKey key, Shape* newShape) {
    MOZ_ASSERT(*ptr != newShape);
    set_.replaceKey(ptr, key, newShape);
  }

  void compact() { set_.compact(); }

  void trace(JSTracer* trc);
#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAfterMovingGC();
#endif
};

/*
 *  Wrapper class to either ShapeTable or ShapeIC optimization.
 *
 *  Shapes are initially cached in a linear cache from the ShapeIC class that is
 *  lazily initialized after LINEAR_SEARCHES_MAX searches have been reached, and
 *  the Shape has at least MIN_ENTRIES parents in the lineage.
 *
 *  We use the population of the cache as an indicator of whether the ShapeIC is
 *  working or not.  Once it is full, it is destroyed and a ShapeTable is
 * created instead.
 *
 *  For dictionaries, the linear cache is skipped entirely and hashify is used
 *  to generate the ShapeTable immediately.
 */
class ShapeCachePtr {
  // To reduce impact on memory usage, p is the only data member for this class.
  uintptr_t p;

  enum class CacheType {
    IC = 0x1,
    Table = 0x2,
  };

  static const uint32_t MASK_BITS = 0x3;
  static const uintptr_t CACHETYPE_MASK = 0x3;

  void* getPointer() const {
    uintptr_t ptrVal = p & ~CACHETYPE_MASK;
    return reinterpret_cast<void*>(ptrVal);
  }

  CacheType getType() const {
    return static_cast<CacheType>(p & CACHETYPE_MASK);
  }

 public:
  static const uint32_t MIN_ENTRIES = 3;

  ShapeCachePtr() : p(0) {}

  MOZ_ALWAYS_INLINE bool search(jsid id, Shape* start, Shape** foundShape);

  bool isIC() const { return (getType() == CacheType::IC); }
  bool isTable() const { return (getType() == CacheType::Table); }
  bool isInitialized() const { return isTable() || isIC(); }

  ShapeTable* getTablePointer() const {
    MOZ_ASSERT(isTable());
    return reinterpret_cast<ShapeTable*>(getPointer());
  }

  ShapeIC* getICPointer() const {
    MOZ_ASSERT(isIC());
    return reinterpret_cast<ShapeIC*>(getPointer());
  }

  // Use ShapeTable implementation.
  // The caller must have purged any existing IC implementation.
  void initializeTable(ShapeTable* table) {
    MOZ_ASSERT(!isTable() && !isIC());

    uintptr_t tableptr = uintptr_t(table);

    // Double check that pointer is 4 byte aligned.
    MOZ_ASSERT((tableptr & CACHETYPE_MASK) == 0);

    tableptr |= static_cast<uintptr_t>(CacheType::Table);
    p = tableptr;
  }

  // Use ShapeIC implementation.
  // This cannot clobber an existing Table implementation.
  void initializeIC(ShapeIC* ic) {
    MOZ_ASSERT(!isTable() && !isIC());

    uintptr_t icptr = uintptr_t(ic);

    // Double check that pointer is 4 byte aligned.
    MOZ_ASSERT((icptr & CACHETYPE_MASK) == 0);

    icptr |= static_cast<uintptr_t>(CacheType::IC);
    p = icptr;
  }

  void destroy(JSFreeOp* fop, Shape* shape);

  void maybePurgeCache(JSFreeOp* fop, Shape* shape);

  void trace(JSTracer* trc);

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    size_t size = 0;
    if (isIC()) {
      size = getICPointer()->sizeOfIncludingThis(mallocSizeOf);
    } else if (isTable()) {
      size = getTablePointer()->sizeOfIncludingThis(mallocSizeOf);
    }
    return size;
  }

  uint32_t entryCount() {
    uint32_t count = 0;
    if (isIC()) {
      count = getICPointer()->entryCount();
    } else if (isTable()) {
      count = getTablePointer()->entryCount();
    }
    return count;
  }

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAfterMovingGC();
#endif
};

// Ensures no shape tables are purged in the current zone.
class MOZ_RAII AutoKeepShapeCaches {
  JSContext* cx_;
  bool prev_;

 public:
  void operator=(const AutoKeepShapeCaches&) = delete;
  AutoKeepShapeCaches(const AutoKeepShapeCaches&) = delete;
  explicit inline AutoKeepShapeCaches(JSContext* cx);
  inline ~AutoKeepShapeCaches();
};

/*
 * Shapes encode information about both a property lineage *and* a particular
 * property. This information is split across the Shape and the BaseShape
 * at shape->base(). Shapes can be either owned or unowned by the object
 * referring to them. BaseShapes are always unowned and immutable.
 *
 * Owned Shapes are used in dictionary objects, and form a doubly linked list
 * whose entries are all owned by that dictionary. Unowned Shapes are all in
 * the property tree.
 *
 * BaseShapes store the object's class, realm and prototype. The prototype is
 * mutable and may change when the object has an established property lineage.
 * On such changes the entire property lineage is not updated, but rather only
 * the last property (and its base shape). This works because only the object's
 * last property is used to query information about the object. Care must be
 * taken to call JSObject::canRemoveLastProperty when unwinding an object to an
 * earlier property, however.
 */

class Shape;
struct StackBaseShape;

// Flags set on the Shape which describe the referring object. Once set these
// cannot be unset (except during object densification of sparse indexes), and
// are transferred from shape to shape as the object's last property changes.
//
// If you add a new flag here, please add appropriate code to JSObject::dump to
// dump it as part of the object representation.
enum class ObjectFlag : uint16_t {
  IsUsedAsPrototype = 1 << 0,
  NotExtensible = 1 << 1,
  Indexed = 1 << 2,
  HasInterestingSymbol = 1 << 3,
  HadElementsAccess = 1 << 4,
  FrozenElements = 1 << 5,  // See ObjectElements::FROZEN comment.
  UncacheableProto = 1 << 6,
  ImmutablePrototype = 1 << 7,

  // See JSObject::isQualifiedVarObj().
  QualifiedVarObj = 1 << 8,

  // If set, the object may have a non-writable property or an accessor
  // property.
  //
  // * This is only set for PlainObjects because we only need it for these
  //   objects and setting it for other objects confuses insertInitialShape.
  //
  // * This flag does not account for properties named "__proto__". This is
  //   because |Object.prototype| has a "__proto__" accessor property and we
  //   don't want to include it because it would result in the flag being set on
  //   most proto chains. Code using this flag must check for "__proto__"
  //   property names separately.
  HasNonWritableOrAccessorPropExclProto = 1 << 9,

  // If set, the object either mutated or deleted an accessor property. This is
  // used to invalidate IC/Warp code specializing on specific getter/setter
  // objects. See also the SMDOC comment in vm/GetterSetter.h.
  HadGetterSetterChange = 1 << 10,
};

using ObjectFlags = EnumFlags<ObjectFlag>;

class BaseShape : public gc::TenuredCellWithNonGCPointer<const JSClass> {
 public:
  friend class Shape;
  friend struct StackBaseShape;
  friend struct StackShape;

  /* Class of referring object, stored in the cell header */
  const JSClass* clasp() const { return headerPtr(); }

 private:
  JS::Realm* realm_;
  GCPtr<TaggedProto> proto_;

  BaseShape(const BaseShape& base) = delete;
  BaseShape& operator=(const BaseShape& other) = delete;

 public:
  void finalize(JSFreeOp* fop) {}

  explicit inline BaseShape(const StackBaseShape& base);

  /* Not defined: BaseShapes must not be stack allocated. */
  ~BaseShape() = delete;

  JS::Realm* realm() const { return realm_; }
  JS::Compartment* compartment() const {
    return JS::GetCompartmentForRealm(realm());
  }
  JS::Compartment* maybeCompartment() const { return compartment(); }

  TaggedProto proto() const { return proto_; }

  void setRealmForMergeRealms(JS::Realm* realm) { realm_ = realm; }
  void setProtoForMergeRealms(TaggedProto proto) { proto_ = proto; }

  /*
   * Lookup base shapes from the zone's baseShapes table, adding if not
   * already found.
   */
  static BaseShape* get(JSContext* cx, Handle<StackBaseShape> base);

  static const JS::TraceKind TraceKind = JS::TraceKind::BaseShape;

  void traceChildren(JSTracer* trc);

  static constexpr size_t offsetOfClasp() { return offsetOfHeaderPtr(); }

  static constexpr size_t offsetOfRealm() {
    return offsetof(BaseShape, realm_);
  }

  static constexpr size_t offsetOfProto() {
    return offsetof(BaseShape, proto_);
  }

 private:
  static void staticAsserts() {
    static_assert(offsetOfClasp() == offsetof(JS::shadow::BaseShape, clasp));
    static_assert(offsetOfRealm() == offsetof(JS::shadow::BaseShape, realm));
    static_assert(sizeof(BaseShape) % gc::CellAlignBytes == 0,
                  "Things inheriting from gc::Cell must have a size that's "
                  "a multiple of gc::CellAlignBytes");
    // Sanity check BaseShape size is what we expect.
#ifdef JS_64BIT
    static_assert(sizeof(BaseShape) == 3 * sizeof(void*));
#else
    static_assert(sizeof(BaseShape) == 4 * sizeof(void*));
#endif
  }
};

/* Entries for the per-zone baseShapes set. */
struct StackBaseShape : public DefaultHasher<WeakHeapPtr<BaseShape*>> {
  const JSClass* clasp;
  JS::Realm* realm;
  TaggedProto proto;

  inline StackBaseShape(const JSClass* clasp, JS::Realm* realm,
                        TaggedProto proto);

  struct Lookup {
    const JSClass* clasp;
    JS::Realm* realm;
    TaggedProto proto;

    MOZ_IMPLICIT Lookup(const StackBaseShape& base)
        : clasp(base.clasp), realm(base.realm), proto(base.proto) {}

    MOZ_IMPLICIT Lookup(BaseShape* base)
        : clasp(base->clasp()), realm(base->realm()), proto(base->proto()) {}
  };

  static HashNumber hash(const Lookup& lookup) {
    HashNumber hash = MovableCellHasher<TaggedProto>::hash(lookup.proto);
    return mozilla::AddToHash(hash,
                              mozilla::HashGeneric(lookup.clasp, lookup.realm));
  }
  static inline bool match(const WeakHeapPtr<BaseShape*>& key,
                           const Lookup& lookup) {
    return key.unbarrieredGet()->clasp() == lookup.clasp &&
           key.unbarrieredGet()->realm() == lookup.realm &&
           key.unbarrieredGet()->proto() == lookup.proto;
  }

  // StructGCPolicy implementation.
  void trace(JSTracer* trc);
};

template <typename Wrapper>
class WrappedPtrOperations<StackBaseShape, Wrapper> {};

static MOZ_ALWAYS_INLINE js::HashNumber HashId(jsid id) {
  // HashGeneric alone would work, but bits of atom and symbol addresses
  // could then be recovered from the hash code. See bug 1330769.
  if (MOZ_LIKELY(JSID_IS_ATOM(id))) {
    return id.toAtom()->hash();
  }
  if (JSID_IS_SYMBOL(id)) {
    return JSID_TO_SYMBOL(id)->hash();
  }
  return mozilla::HashGeneric(JSID_BITS(id));
}

}  // namespace js

namespace mozilla {

template <>
struct DefaultHasher<jsid> {
  using Lookup = jsid;
  static HashNumber hash(jsid id) { return js::HashId(id); }
  static bool match(jsid id1, jsid id2) { return id1 == id2; }
};

}  // namespace mozilla

namespace js {

using BaseShapeSet = JS::WeakCache<
    JS::GCHashSet<WeakHeapPtr<BaseShape*>, StackBaseShape, SystemAllocPolicy>>;

class Shape : public gc::CellWithTenuredGCPointer<gc::TenuredCell, BaseShape> {
  friend class ::JSObject;
  friend class ::JSFunction;
  friend class GCMarker;
  friend class NativeObject;
  friend class PropertyTree;
  friend class TenuringTracer;
  friend struct StackBaseShape;
  friend struct StackShape;
  friend class JS::ubi::Concrete<Shape>;
  friend class js::gc::RelocationOverlay;
  friend class js::ShapeTable;

 public:
  // Base shape, stored in the cell header.
  BaseShape* base() const { return headerPtr(); }

 protected:
  const GCPtr<PropertyKey> propid_;

  // Flags that are not modified after the Shape is created. Off-thread Ion
  // compilation can access the immutableFlags word, so we don't want any
  // mutable state here to avoid (TSan) races.
  enum ImmutableFlags : uint32_t {
    // Mask to get the index in object slots for hasSlot() shapes (all property
    // shapes except custom data properties).
    // For other shapes in the property tree with a parent, stores the
    // parent's slot index (which may be invalid), and invalid for all
    // other shapes.
    SLOT_MASK = BitMask(24),

    // Number of fixed slots in objects with this shape.
    // FIXED_SLOTS_MAX is the biggest count of fixed slots a Shape can store.
    FIXED_SLOTS_MAX = 0x1f,
    FIXED_SLOTS_SHIFT = 24,
    FIXED_SLOTS_MASK = uint32_t(FIXED_SLOTS_MAX << FIXED_SLOTS_SHIFT),

    // Property stored in per-object dictionary, not shared property tree.
    IN_DICTIONARY = 1 << 29,
  };

  // Flags stored in mutableFlags.
  enum MutableFlags : uint8_t {
    // numLinearSearches starts at zero and is incremented initially on
    // search() calls. Once numLinearSearches reaches LINEAR_SEARCHES_MAX,
    // the inline cache is created on the next search() call.  Once the
    // cache is full, it self transforms into a hash table. The hash table
    // can also be created directly when hashifying for dictionary mode.
    LINEAR_SEARCHES_MAX = 0x5,
    LINEAR_SEARCHES_MASK = 0x7,

    // Flags used to speed up isBigEnoughForAShapeTable().
    HAS_CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE = 0x08,
    CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE = 0x10,
  };

 private:
  uint32_t immutableFlags;  /* immutable flags, see above */
  ObjectFlags objectFlags_; /* immutable object flags, see ObjectFlags */
  uint8_t attrs;            /* attributes, see jsapi.h JSPROP_* */
  uint8_t mutableFlags;     /* mutable flags, see below for defines */

  GCPtrShape parent; /* parent node, reverse for..in order */
  friend class DictionaryShapeLink;

  // The shape's ShapeTable or ShapeIC.
  ShapeCachePtr cache_;

  union {
    // Valid when !inDictionary().
    ShapeChildren children;

    // Valid when inDictionary().
    DictionaryShapeLink dictNext;
  };

  void setNextDictionaryShape(Shape* shape);
  void setDictionaryObject(JSObject* obj);
  void setDictionaryNextPtr(DictionaryShapeLink next);
  void clearDictionaryNextPtr();
  void dictNextPreWriteBarrier();

  static MOZ_ALWAYS_INLINE Shape* search(JSContext* cx, Shape* start, jsid id);

  [[nodiscard]] static inline bool search(JSContext* cx, Shape* start, jsid id,
                                          const AutoKeepShapeCaches&,
                                          Shape** pshape, ShapeTable** ptable,
                                          ShapeTable::Ptr* pptr);

  static inline Shape* searchNoHashify(Shape* start, jsid id);

  void removeFromDictionary(NativeObject* obj);
  void insertIntoDictionaryBefore(DictionaryShapeLink next);

  inline void initDictionaryShape(const StackShape& child, uint32_t nfixed,
                                  DictionaryShapeLink next);

  // Replace the last shape in a non-dictionary lineage with a shape that has
  // the passed objectFlags and proto.
  static Shape* replaceLastProperty(JSContext* cx, ObjectFlags objectFlags,
                                    TaggedProto proto, HandleShape shape);

  /*
   * This function is thread safe if every shape in the lineage of |shape|
   * is thread local, which is the case when we clone the entire shape
   * lineage in preparation for converting an object to dictionary mode.
   */
  static bool hashify(JSContext* cx, Shape* shape);
  static bool cachify(JSContext* cx, Shape* shape);
  void handoffTableTo(Shape* newShape);

  void setParent(Shape* p) {
    // For non-dictionary shapes: if the parent has a slot number, it must be
    // less than or equal to the child's slot number. (Parent and child can have
    // the same slot number if the child is a custom data property, these don't
    // have a slot.)
    MOZ_ASSERT_IF(p && !p->hasMissingSlot() && !inDictionary(),
                  p->maybeSlot() <= maybeSlot());
    // For non-dictionary shapes: if the child has a slot, its slot number
    // must not be the same as the parent's slot number. If the child does not
    // have a slot, its slot number must match the parent's slot number.
    MOZ_ASSERT_IF(p && !inDictionary(),
                  hasSlot() == (p->maybeSlot() != maybeSlot()));
    parent = p;
  }

  [[nodiscard]] MOZ_ALWAYS_INLINE bool maybeCreateCacheForLookup(JSContext* cx);

  void setObjectFlags(ObjectFlags flags) {
    MOZ_ASSERT(inDictionary());
    objectFlags_ = flags;
  }

 public:
  bool hasTable() const { return cache_.isTable(); }

  bool hasIC() const { return cache_.isIC(); }

  void setTable(ShapeTable* table) { cache_.initializeTable(table); }

  void setIC(ShapeIC* ic) { cache_.initializeIC(ic); }

  ShapeCachePtr getCache(const AutoKeepShapeCaches&) const { return cache_; }

  ShapeCachePtr getCache(const JS::AutoCheckCannotGC&) const { return cache_; }

  ShapeTable* maybeTable(const AutoKeepShapeCaches&) const {
    return cache_.isTable() ? cache_.getTablePointer() : nullptr;
  }

  ShapeTable* maybeTable(const JS::AutoCheckCannotGC&) const {
    return cache_.isTable() ? cache_.getTablePointer() : nullptr;
  }

  ShapeIC* maybeIC(const AutoKeepShapeCaches&) const {
    return cache_.isIC() ? cache_.getICPointer() : nullptr;
  }

  ShapeIC* maybeIC(const JS::AutoCheckCannotGC&) const {
    return cache_.isIC() ? cache_.getICPointer() : nullptr;
  }

  void maybePurgeCache(JSFreeOp* fop) { cache_.maybePurgeCache(fop, this); }

  bool appendShapeToIC(jsid id, Shape* shape,
                       const JS::AutoCheckCannotGC& check) {
    MOZ_ASSERT(hasIC());
    ShapeCachePtr cache = getCache(check);
    return cache.getICPointer()->appendEntry(id, shape);
  }

  template <typename T>
  [[nodiscard]] ShapeTable* ensureTableForDictionary(JSContext* cx,
                                                     const T& nogc) {
    MOZ_ASSERT(inDictionary());
    if (ShapeTable* table = maybeTable(nogc)) {
      return table;
    }
    if (!hashify(cx, this)) {
      return nullptr;
    }
    ShapeTable* table = maybeTable(nogc);
    MOZ_ASSERT(table);
    return table;
  }

  void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              JS::ShapeInfo* info) const {
    JS::AutoCheckCannotGC nogc;
    if (inDictionary()) {
      info->shapesMallocHeapDictTables +=
          getCache(nogc).sizeOfExcludingThis(mallocSizeOf);
    } else {
      info->shapesMallocHeapTreeTables +=
          getCache(nogc).sizeOfExcludingThis(mallocSizeOf);
    }

    if (!inDictionary() && children.isShapeSet()) {
      info->shapesMallocHeapTreeChildren +=
          children.toShapeSet()->shallowSizeOfIncludingThis(mallocSizeOf);
    }
  }

  const GCPtrShape& previous() const { return parent; }

  template <AllowGC allowGC>
  class Range {
   protected:
    friend class Shape;

    typename MaybeRooted<Shape*, allowGC>::RootType cursor;

   public:
    Range(JSContext* cx, Shape* shape) : cursor(cx, shape) {
      static_assert(allowGC == CanGC);
    }

    explicit Range(Shape* shape) : cursor(nullptr, shape) {
      static_assert(allowGC == NoGC);
    }

    bool empty() const { return !cursor || cursor->isEmptyShape(); }

    Shape& front() const {
      MOZ_ASSERT(!empty());
      return *cursor;
    }

    void popFront() {
      MOZ_ASSERT(!empty());
      cursor = cursor->parent;
    }
  };

  const JSClass* getObjectClass() const { return base()->clasp(); }
  JS::Realm* realm() const { return base()->realm(); }

  JS::Compartment* compartment() const { return base()->compartment(); }
  JS::Compartment* maybeCompartment() const {
    return base()->maybeCompartment();
  }

  TaggedProto proto() const { return base()->proto(); }

  static Shape* setObjectFlag(JSContext* cx, ObjectFlag flag, Shape* last);
  static Shape* setProto(JSContext* cx, TaggedProto proto, Shape* last);

  ObjectFlags objectFlags() const { return objectFlags_; }
  bool hasObjectFlag(ObjectFlag flag) const {
    return objectFlags_.hasFlag(flag);
  }

 protected:
  /* Get a shape identical to this one, without parent/children information. */
  inline Shape(const StackShape& other, uint32_t nfixed);

  /* Used by EmptyShape (see jsscopeinlines.h). */
  inline Shape(BaseShape* base, ObjectFlags objectFlags, uint32_t nfixed);

  /* Copy constructor disabled, to avoid misuse of the above form. */
  Shape(const Shape& other) = delete;

  /* Allocate a new shape based on the given StackShape. */
  static inline Shape* new_(JSContext* cx, Handle<StackShape> other,
                            uint32_t nfixed);

  // Whether this shape has a valid slot value. This may be true even if
  // !hasSlot() (see SlotInfo comment above), and may be false even if
  // hasSlot() if the shape is being constructed and has not had a slot
  // assigned yet. After construction, hasSlot() implies !hasMissingSlot().
  bool hasMissingSlot() const { return maybeSlot() == SHAPE_INVALID_SLOT; }

 public:
  bool inDictionary() const { return immutableFlags & IN_DICTIONARY; }

  bool matches(const Shape* other) const {
    return propid_.get() == other->propid_.get() &&
           matchesParamsAfterId(other->base(), other->objectFlags(),
                                other->maybeSlot(), other->attrs);
  }

  inline bool matches(const StackShape& other) const;

  bool matchesParamsAfterId(BaseShape* base, ObjectFlags aobjectFlags,
                            uint32_t aslot, unsigned aattrs) const {
    return base == this->base() && objectFlags() == aobjectFlags &&
           matchesPropertyParamsAfterId(aslot, aattrs);
  }
  bool matchesPropertyParamsAfterId(uint32_t aslot, unsigned aattrs) const {
    return maybeSlot() == aslot && attrs == aattrs;
  }

 private:
  uint32_t slot() const {
    MOZ_ASSERT(hasSlot());
    return maybeSlot();
  }
  uint32_t maybeSlot() const { return immutableFlags & SLOT_MASK; }

  bool hasSlot() const {
    MOZ_ASSERT(!isEmptyShape());
    MOZ_ASSERT_IF(!isCustomDataProperty(), !hasMissingSlot());
    return !isCustomDataProperty();
  }

  bool isCustomDataProperty() const { return attrs & JSPROP_CUSTOM_DATA_PROP; }

 public:
  bool isEmptyShape() const {
    MOZ_ASSERT_IF(JSID_IS_EMPTY(propid_), hasMissingSlot());
    return JSID_IS_EMPTY(propid_);
  }

  uint32_t slotSpan() const {
    MOZ_ASSERT(!inDictionary());
    // slotSpan is only defined for native objects. Proxy classes have reserved
    // slots, but proxies manage their own slot layout.
    const JSClass* clasp = getObjectClass();
    MOZ_ASSERT(clasp->isNativeObject());
    uint32_t free = JSSLOT_FREE(clasp);
    return hasMissingSlot() ? free : std::max(free, maybeSlot() + 1);
  }

  void setSlot(uint32_t slot) {
    MOZ_ASSERT(slot <= SHAPE_INVALID_SLOT);
    immutableFlags = (immutableFlags & ~Shape::SLOT_MASK) | slot;
  }

  uint32_t numFixedSlots() const {
    return (immutableFlags & FIXED_SLOTS_MASK) >> FIXED_SLOTS_SHIFT;
  }

  void setNumFixedSlots(uint32_t nfixed) {
    MOZ_ASSERT(nfixed < FIXED_SLOTS_MAX);
    immutableFlags = immutableFlags & ~FIXED_SLOTS_MASK;
    immutableFlags = immutableFlags | (nfixed << FIXED_SLOTS_SHIFT);
  }

  uint32_t numLinearSearches() const {
    return mutableFlags & LINEAR_SEARCHES_MASK;
  }

  void incrementNumLinearSearches() {
    uint32_t count = numLinearSearches();
    MOZ_ASSERT(count < LINEAR_SEARCHES_MAX);
    mutableFlags = (mutableFlags & ~LINEAR_SEARCHES_MASK) | (count + 1);
  }

 private:
  const GCPtrId& propid() const {
    MOZ_ASSERT(!isEmptyShape());
    MOZ_ASSERT(!JSID_IS_VOID(propid_));
    return propid_;
  }
  const GCPtrId& propidRef() {
    MOZ_ASSERT(!JSID_IS_VOID(propid_));
    return propid_;
  }
  jsid propidRaw() const {
    // Return the actual jsid, not an internal reference.
    return propid();
  }

 public:
  ShapeProperty property() const {
    MOZ_ASSERT(!isEmptyShape());
    return ShapeProperty(attrs, maybeSlot());
  }

  ShapePropertyWithKey propertyWithKey() const {
    return ShapePropertyWithKey(attrs, maybeSlot(), propid());
  }

 private:
  uint8_t attributes() const { return attrs; }

 public:
  uint32_t entryCount() {
    JS::AutoCheckCannotGC nogc;
    if (ShapeTable* table = maybeTable(nogc)) {
      return table->entryCount();
    }
    uint32_t count = 0;
    for (Shape::Range<NoGC> r(this); !r.empty(); r.popFront()) {
      ++count;
    }
    return count;
  }

 private:
  void setBase(BaseShape* base) {
    MOZ_ASSERT(base);
    MOZ_ASSERT(inDictionary());
    setHeaderPtr(base);
  }

  bool isBigEnoughForAShapeTableSlow() {
    uint32_t count = 0;
    for (Shape::Range<NoGC> r(this); !r.empty(); r.popFront()) {
      ++count;
      if (count >= ShapeCachePtr::MIN_ENTRIES) {
        return true;
      }
    }
    return false;
  }
  void clearCachedBigEnoughForShapeTable() {
    mutableFlags &= ~(HAS_CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE |
                      CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE);
  }

 public:
  bool isBigEnoughForAShapeTable() {
    MOZ_ASSERT(!hasTable());

    // isBigEnoughForAShapeTableSlow is pretty inefficient so we only call
    // it once and cache the result.

    if (mutableFlags & HAS_CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE) {
      bool res = mutableFlags & CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE;
      MOZ_ASSERT(res == isBigEnoughForAShapeTableSlow());
      return res;
    }

    MOZ_ASSERT(!(mutableFlags & CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE));

    bool res = isBigEnoughForAShapeTableSlow();
    if (res) {
      mutableFlags |= CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE;
    }
    mutableFlags |= HAS_CACHED_BIG_ENOUGH_FOR_SHAPE_TABLE;
    return res;
  }

#ifdef DEBUG
  void dump(js::GenericPrinter& out) const;
  void dump() const;
  void dumpSubtree(int level, js::GenericPrinter& out) const;
#endif

  void sweep(JSFreeOp* fop);
  void finalize(JSFreeOp* fop);
  void removeChild(JSFreeOp* fop, Shape* child);

  static const JS::TraceKind TraceKind = JS::TraceKind::Shape;

  void traceChildren(JSTracer* trc);

#ifdef DEBUG
  bool canSkipMarkingShapeCache();
#endif

  MOZ_ALWAYS_INLINE Shape* search(JSContext* cx, jsid id);
  MOZ_ALWAYS_INLINE Shape* searchLinear(jsid id);

  void fixupAfterMovingGC();
  void updateBaseShapeAfterMovingGC();

  // For JIT usage.
  static constexpr size_t offsetOfBaseShape() { return offsetOfHeaderPtr(); }

  static constexpr size_t offsetOfObjectFlags() {
    return offsetof(Shape, objectFlags_);
  }

#ifdef DEBUG
  static inline size_t offsetOfImmutableFlags() {
    return offsetof(Shape, immutableFlags);
  }
  static inline uint32_t fixedSlotsMask() { return FIXED_SLOTS_MASK; }
#endif

 private:
  void fixupDictionaryShapeAfterMovingGC();
  void fixupShapeTreeAfterMovingGC();

  static void staticAsserts() {
    static_assert(offsetOfBaseShape() == offsetof(JS::shadow::Shape, base));
    static_assert(offsetof(Shape, immutableFlags) ==
                  offsetof(JS::shadow::Shape, immutableFlags));
    static_assert(FIXED_SLOTS_SHIFT == JS::shadow::Shape::FIXED_SLOTS_SHIFT);
    static_assert(FIXED_SLOTS_MASK == JS::shadow::Shape::FIXED_SLOTS_MASK);
    // Sanity check Shape size is what we expect.
#ifdef JS_64BIT
    static_assert(sizeof(Shape) == 6 * sizeof(void*));
#else
    static_assert(sizeof(Shape) == 8 * sizeof(void*));
#endif
  }
};

struct EmptyShape : public js::Shape {
  EmptyShape(BaseShape* base, ObjectFlags objectFlags, uint32_t nfixed)
      : js::Shape(base, objectFlags, nfixed) {}

  static Shape* new_(JSContext* cx, Handle<BaseShape*> base,
                     ObjectFlags objectFlags, uint32_t nfixed);

  /*
   * Lookup an initial shape matching the given parameters, creating an empty
   * shape if none was found.
   */
  static Shape* getInitialShape(JSContext* cx, const JSClass* clasp,
                                JS::Realm* realm, TaggedProto proto,
                                size_t nfixed, ObjectFlags objectFlags = {});
  static Shape* getInitialShape(JSContext* cx, const JSClass* clasp,
                                JS::Realm* realm, TaggedProto proto,
                                gc::AllocKind kind,
                                ObjectFlags objectFlags = {});

  /*
   * Reinsert an alternate initial shape, to be returned by future
   * getInitialShape calls, until the new shape becomes unreachable in a GC
   * and the table entry is purged.
   */
  static void insertInitialShape(JSContext* cx, HandleShape shape);

  /*
   * Some object subclasses are allocated with a built-in set of properties.
   * The first time such an object is created, these built-in properties must
   * be set manually, to compute an initial shape.  Afterward, that initial
   * shape can be reused for newly-created objects that use the subclass's
   * standard prototype.  This method should be used in a post-allocation
   * init method, to ensure that objects of such subclasses compute and cache
   * the initial shape, if it hasn't already been computed.
   */
  template <class ObjectSubclass>
  static inline bool ensureInitialCustomShape(JSContext* cx,
                                              Handle<ObjectSubclass*> obj);
};

/*
 * Entries for the per-zone initialShapes set indexing initial shapes for
 * objects in the zone and the associated types.
 */
struct InitialShapeEntry {
  /*
   * Initial shape to give to the object. This is an empty shape, except for
   * certain classes (e.g. String, RegExp) which may add certain baked-in
   * properties.
   */
  WeakHeapPtr<Shape*> shape;

  /* State used to determine a match on an initial shape. */
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

  inline InitialShapeEntry();
  inline explicit InitialShapeEntry(Shape* shape);

  static HashNumber hash(const Lookup& lookup) {
    HashNumber hash = MovableCellHasher<TaggedProto>::hash(lookup.proto);
    return mozilla::AddToHash(
        hash, mozilla::HashGeneric(lookup.clasp, lookup.realm, lookup.nfixed,
                                   lookup.objectFlags.toRaw()));
  }
  static inline bool match(const InitialShapeEntry& key, const Lookup& lookup) {
    const Shape* shape = key.shape.unbarrieredGet();
    return lookup.clasp == shape->getObjectClass() &&
           lookup.realm == shape->realm() &&
           lookup.nfixed == shape->numFixedSlots() &&
           lookup.objectFlags == shape->objectFlags() &&
           lookup.proto == shape->proto();
  }
  static void rekey(InitialShapeEntry& k, const InitialShapeEntry& newKey) {
    k = newKey;
  }

  bool needsSweep() {
    Shape* ushape = shape.unbarrieredGet();
    return gc::IsAboutToBeFinalizedUnbarriered(&ushape);
  }

  bool operator==(const InitialShapeEntry& other) const {
    return shape == other.shape;
  }
};

using InitialShapeSet = JS::WeakCache<
    JS::GCHashSet<InitialShapeEntry, InitialShapeEntry, SystemAllocPolicy>>;

struct StackShape {
  /* For performance, StackShape only roots when absolutely necessary. */
  BaseShape* base;
  jsid propid;
  uint32_t immutableFlags;
  ObjectFlags objectFlags;
  uint8_t attrs;
  uint8_t mutableFlags;

  explicit StackShape(BaseShape* base, ObjectFlags objectFlags, jsid propid,
                      uint32_t slot, unsigned attrs)
      : base(base),
        propid(propid),
        immutableFlags(slot),
        objectFlags(objectFlags),
        attrs(uint8_t(attrs)),
        mutableFlags(0) {
    MOZ_ASSERT(base);
    MOZ_ASSERT(!JSID_IS_VOID(propid));
    MOZ_ASSERT(slot <= SHAPE_INVALID_SLOT);
  }

  explicit StackShape(Shape* shape)
      : base(shape->base()),
        propid(shape->propidRef()),
        immutableFlags(shape->immutableFlags),
        objectFlags(shape->objectFlags()),
        attrs(shape->attrs),
        mutableFlags(shape->mutableFlags) {}

  bool hasMissingSlot() const { return maybeSlot() == SHAPE_INVALID_SLOT; }

  bool isCustomDataProperty() const { return attrs & JSPROP_CUSTOM_DATA_PROP; }

  uint32_t slot() const {
    MOZ_ASSERT(!hasMissingSlot());
    return maybeSlot();
  }
  uint32_t maybeSlot() const { return immutableFlags & Shape::SLOT_MASK; }

  void setSlot(uint32_t slot) {
    MOZ_ASSERT(slot <= SHAPE_INVALID_SLOT);
    immutableFlags = (immutableFlags & ~Shape::SLOT_MASK) | slot;
  }

  HashNumber hash() const {
    HashNumber hash = HashId(propid);
    return mozilla::AddToHash(
        hash,
        mozilla::HashGeneric(base, objectFlags.toRaw(), attrs, maybeSlot()));
  }

  // StructGCPolicy implementation.
  void trace(JSTracer* trc);
};

template <typename Wrapper>
class WrappedPtrOperations<StackShape, Wrapper> {
  const StackShape& ss() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isCustomDataProperty() const { return ss().isCustomDataProperty(); }
  bool hasMissingSlot() const { return ss().hasMissingSlot(); }
  uint32_t slot() const { return ss().slot(); }
  uint32_t maybeSlot() const { return ss().maybeSlot(); }
  uint32_t slotSpan() const { return ss().slotSpan(); }
  uint8_t attrs() const { return ss().attrs; }
  ObjectFlags objectFlags() const { return ss().objectFlags; }
  jsid propid() const { return ss().propid; }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<StackShape, Wrapper>
    : public WrappedPtrOperations<StackShape, Wrapper> {
  StackShape& ss() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void setSlot(uint32_t slot) { ss().setSlot(slot); }
  void setBase(BaseShape* base) { ss().base = base; }
  void setAttrs(uint8_t attrs) { ss().attrs = attrs; }
  void setObjectFlags(ObjectFlags objectFlags) {
    ss().objectFlags = objectFlags;
  }
};

inline Shape::Shape(const StackShape& other, uint32_t nfixed)
    : CellWithTenuredGCPointer(other.base),
      propid_(other.propid),
      immutableFlags(other.immutableFlags),
      objectFlags_(other.objectFlags),
      attrs(other.attrs),
      mutableFlags(other.mutableFlags),
      parent(nullptr) {
  setNumFixedSlots(nfixed);

  MOZ_ASSERT_IF(!isEmptyShape(), AtomIsMarked(zone(), propid()));

  children.setNone();
}

inline Shape::Shape(BaseShape* base, ObjectFlags objectFlags, uint32_t nfixed)
    : CellWithTenuredGCPointer(base),
      propid_(JSID_EMPTY),
      immutableFlags(SHAPE_INVALID_SLOT | (nfixed << FIXED_SLOTS_SHIFT)),
      objectFlags_(objectFlags),
      attrs(0),
      mutableFlags(0),
      parent(nullptr) {
  MOZ_ASSERT(base);
  children.setNone();
}

inline Shape* Shape::searchLinear(jsid id) {
  for (Shape* shape = this; shape;) {
    if (shape->propidRef() == id) {
      return shape;
    }
    shape = shape->parent;
  }

  return nullptr;
}

inline bool Shape::matches(const StackShape& other) const {
  return propid_.get() == other.propid &&
         matchesParamsAfterId(other.base, other.objectFlags, other.maybeSlot(),
                              other.attrs);
}

MOZ_ALWAYS_INLINE bool ShapeCachePtr::search(jsid id, Shape* start,
                                             Shape** foundShape) {
  bool found = false;
  if (isIC()) {
    ShapeIC* ic = getICPointer();
    found = ic->search(id, foundShape);
  } else if (isTable()) {
    ShapeTable* table = getTablePointer();
    auto p = table->searchUnchecked(id);
    *foundShape = p ? *p : nullptr;
    found = true;
  }
  return found;
}

MOZ_ALWAYS_INLINE bool ShapeIC::search(jsid id, Shape** foundShape) {
  // This loop needs to be as fast as possible, so use a direct pointer
  // to the array instead of going through the UniquePtr methods.
  Entry* entriesArray = entries_.get();
  for (uint8_t i = 0; i < nextFreeIndex_; i++) {
    Entry& entry = entriesArray[i];
    if (entry.id_ == id) {
      *foundShape = entry.shape_;
      return true;
    }
  }

  return false;
}

using ShapePropertyVector = GCVector<ShapePropertyWithKey, 8>;

// Iterator for iterating over a shape's properties. It can be used like this:
//
//   for (ShapePropertyIter<NoGC> iter(nobj->shape()); !iter.done(); iter++) {
//     PropertyKey key = iter->key();
//     if (iter->isDataProperty() && iter->enumerable()) { .. }
//   }
//
// Properties are iterated in reverse order (i.e., iteration starts at the most
// recently added property).
template <AllowGC allowGC>
class MOZ_RAII ShapePropertyIter {
 protected:
  friend class Shape;

  typename MaybeRooted<Shape*, allowGC>::RootType cursor_;

 public:
  ShapePropertyIter(JSContext* cx, Shape* shape) : cursor_(cx, shape) {
    static_assert(allowGC == CanGC);
    MOZ_ASSERT(shape->getObjectClass()->isNativeObject());
  }

  explicit ShapePropertyIter(Shape* shape) : cursor_(nullptr, shape) {
    static_assert(allowGC == NoGC);
    MOZ_ASSERT(shape->getObjectClass()->isNativeObject());
  }

  bool done() const { return cursor_->isEmptyShape(); }

  void operator++(int) {
    MOZ_ASSERT(!done());
    cursor_ = cursor_->previous();
  }

  ShapePropertyWithKey get() const {
    MOZ_ASSERT(!done());
    return cursor_->propertyWithKey();
  }

  ShapePropertyWithKey operator*() const { return get(); }

  // Fake pointer struct to make operator-> work.
  // See https://stackoverflow.com/a/52856349.
  struct FakePtr {
    ShapePropertyWithKey val_;
    const ShapePropertyWithKey* operator->() const { return &val_; }
  };
  FakePtr operator->() const { return {get()}; }
};

MOZ_ALWAYS_INLINE HashNumber ShapeTable::Hasher::hash(PropertyKey key) {
  return HashId(key);
}
MOZ_ALWAYS_INLINE bool ShapeTable::Hasher::match(Shape* shape,
                                                 PropertyKey key) {
  return shape->propidRaw() == key;
}

}  // namespace js

// JS::ubi::Nodes can point to Shapes and BaseShapes; they're js::gc::Cell
// instances that occupy a compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::Shape> : TracerConcrete<js::Shape> {
 protected:
  explicit Concrete(js::Shape* ptr) : TracerConcrete<js::Shape>(ptr) {}

 public:
  static void construct(void* storage, js::Shape* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

template <>
class Concrete<js::BaseShape> : TracerConcrete<js::BaseShape> {
 protected:
  explicit Concrete(js::BaseShape* ptr) : TracerConcrete<js::BaseShape>(ptr) {}

 public:
  static void construct(void* storage, js::BaseShape* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif /* vm_Shape_h */
