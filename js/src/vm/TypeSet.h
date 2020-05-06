/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TypeSet classes and functions. */

#ifndef vm_TypeSet_h
#define vm_TypeSet_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_ALWAYS_INLINE
#include "mozilla/Likely.h"      // MOZ_UNLIKELY

#include <initializer_list>
#include <stdint.h>  // intptr_t, uintptr_t, uint8_t, uint32_t
#include <stdio.h>   // FILE

#include "jstypes.h"  // JS_BITS_PER_WORD, JS_PUBLIC_API

#include "jit/IonTypes.h"  // jit::MIRType
#include "jit/JitOptions.h"
#include "js/GCAnnotations.h"  // JS_HAZ_GC_POINTER
#include "js/Id.h"
#include "js/TracingAPI.h"  // JSTracer
#include "js/TypeDecls.h"   // IF_BIGINT
#include "js/Utility.h"     // UniqueChars
#include "js/Value.h"       // JSVAL_TYPE_*
#include "js/Vector.h"      // js::Vector
#include "util/DiagnosticAssertions.h"
#include "vm/TaggedProto.h"  // js::TaggedProto

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;

namespace JS {

class JS_PUBLIC_API Compartment;
class JS_PUBLIC_API Realm;
class JS_PUBLIC_API Zone;

}  // namespace JS

namespace js {

namespace jit {

class IonScript;
class TempAllocator;

}  // namespace jit

class AutoClearTypeInferenceStateOnOOM;
class AutoSweepBase;
class AutoSweepObjectGroup;
class CompilerConstraintList;
class HeapTypeSetKey;
class LifoAlloc;
class ObjectGroup;
class SystemAllocPolicy;
class TypeConstraint;
class TypeNewScript;
class TypeZone;

/*
 * Type inference memory management overview.
 *
 * Type information about the values observed within scripts and about the
 * contents of the heap is accumulated as the program executes. Compilation
 * accumulates constraints relating type information on the heap with the
 * compilations that should be invalidated when those types change. Type
 * information and constraints are allocated in the zone's typeLifoAlloc,
 * and on GC all data referring to live things is copied into a new allocator.
 * Thus, type set and constraints only hold weak references.
 */

/* Flags and other state stored in TypeSet::flags */
enum : uint32_t {
  TYPE_FLAG_UNDEFINED = 0x1,
  TYPE_FLAG_NULL = 0x2,
  TYPE_FLAG_BOOLEAN = 0x4,
  TYPE_FLAG_INT32 = 0x8,
  TYPE_FLAG_DOUBLE = 0x10,
  TYPE_FLAG_STRING = 0x20,
  TYPE_FLAG_SYMBOL = 0x40,
  TYPE_FLAG_BIGINT = 0x80,
  TYPE_FLAG_LAZYARGS = 0x100,
  TYPE_FLAG_ANYOBJECT = 0x200,

  /* Mask containing all "immediate" primitives (not heap-allocated) */
  TYPE_FLAG_PRIMITIVE_IMMEDIATE = TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_BOOLEAN | TYPE_FLAG_INT32 |
                                  TYPE_FLAG_DOUBLE,
  /* Mask containing all GCThing primitives (heap-allocated) */
  TYPE_FLAG_PRIMITIVE_GCTHING =
      TYPE_FLAG_STRING | TYPE_FLAG_SYMBOL | TYPE_FLAG_BIGINT,

  /* Mask containing all primitives */
  TYPE_FLAG_PRIMITIVE =
      TYPE_FLAG_PRIMITIVE_IMMEDIATE | TYPE_FLAG_PRIMITIVE_GCTHING,

  /* Mask/shift for the number of objects in objectSet */
  TYPE_FLAG_OBJECT_COUNT_MASK = 0x3c00,
  TYPE_FLAG_OBJECT_COUNT_SHIFT = 10,
  TYPE_FLAG_OBJECT_COUNT_LIMIT = 7,
  TYPE_FLAG_DOMOBJECT_COUNT_LIMIT =
      TYPE_FLAG_OBJECT_COUNT_MASK >> TYPE_FLAG_OBJECT_COUNT_SHIFT,

  /* Whether the contents of this type set are totally unknown. */
  TYPE_FLAG_UNKNOWN = 0x00004000,

  /* Mask of normal type flags on a type set. */
  TYPE_FLAG_BASE_MASK = TYPE_FLAG_PRIMITIVE | TYPE_FLAG_LAZYARGS |
                        TYPE_FLAG_ANYOBJECT | TYPE_FLAG_UNKNOWN,

  /* Additional flags for HeapTypeSet sets. */

  /*
   * Whether the property has ever been deleted or reconfigured to behave
   * differently from a plain data property, other than making the property
   * non-writable.
   */
  TYPE_FLAG_NON_DATA_PROPERTY = 0x00008000,

  /* Whether the property has ever been made non-writable. */
  TYPE_FLAG_NON_WRITABLE_PROPERTY = 0x00010000,

  /* Whether the property might not be constant. */
  TYPE_FLAG_NON_CONSTANT_PROPERTY = 0x00020000,

  /*
   * Whether the property is definitely in a particular slot on all objects
   * from which it has not been deleted or reconfigured. For singletons
   * this may be a fixed or dynamic slot, and for other objects this will be
   * a fixed slot.
   *
   * If the property is definite, mask and shift storing the slot + 1.
   * Otherwise these bits are clear.
   */
  TYPE_FLAG_DEFINITE_MASK = 0xfffc0000,
  TYPE_FLAG_DEFINITE_SHIFT = 18
};
using TypeFlags = uint32_t;

static_assert(
    TYPE_FLAG_PRIMITIVE < TYPE_FLAG_ANYOBJECT &&
        TYPE_FLAG_LAZYARGS < TYPE_FLAG_ANYOBJECT,
    "TYPE_FLAG_ANYOBJECT should be greater than primitive type flags");

/* Flags and other state stored in ObjectGroup::Flags */
enum : uint32_t {
  /* Whether this group is associated with some allocation site. */
  OBJECT_FLAG_FROM_ALLOCATION_SITE = 0x1,

  /* Whether this group is associated with a single object. */
  OBJECT_FLAG_SINGLETON = 0x2,

  /*
   * Whether this group is used by objects whose singleton groups have not
   * been created yet.
   */
  OBJECT_FLAG_LAZY_SINGLETON = 0x4,

  /* Mask/shift for the number of properties in propertySet */
  OBJECT_FLAG_PROPERTY_COUNT_MASK = 0xfff8,
  OBJECT_FLAG_PROPERTY_COUNT_SHIFT = 3,
  OBJECT_FLAG_PROPERTY_COUNT_LIMIT =
      OBJECT_FLAG_PROPERTY_COUNT_MASK >> OBJECT_FLAG_PROPERTY_COUNT_SHIFT,

  /* Whether any objects this represents may have sparse indexes. */
  OBJECT_FLAG_SPARSE_INDEXES = 0x00010000,

  /* Whether any objects this represents may not have packed dense elements. */
  OBJECT_FLAG_NON_PACKED = 0x00020000,

  /*
   * Whether any objects this represents may be arrays whose length does not
   * fit in an int32.
   */
  OBJECT_FLAG_LENGTH_OVERFLOW = 0x00040000,

  /* Whether any objects have been iterated over. */
  OBJECT_FLAG_ITERATED = 0x00080000,

  /* Whether any object this represents may have non-extensible elements. */
  OBJECT_FLAG_NON_EXTENSIBLE_ELEMENTS = 0x00100000,

  // (0x00200000 is unused)

  // (0x00400000 is unused)

  /*
   * Whether objects with this type should be allocated directly in the
   * tenured heap.
   */
  OBJECT_FLAG_PRE_TENURE = 0x00800000,

  /* Whether objects with this type might have copy on write elements. */
  OBJECT_FLAG_COPY_ON_WRITE = 0x01000000,

  /* Whether this type has had its 'new' script cleared in the past. */
  OBJECT_FLAG_NEW_SCRIPT_CLEARED = 0x02000000,

  /*
   * Whether all properties of this object are considered unknown.
   * If set, all other flags in DYNAMIC_MASK will also be set.
   */
  OBJECT_FLAG_UNKNOWN_PROPERTIES = 0x04000000,

  /* Flags which indicate dynamic properties of represented objects. */
  OBJECT_FLAG_DYNAMIC_MASK = 0x07ff0000,

  // Mask/shift for the kind of addendum attached to this group.
  OBJECT_FLAG_ADDENDUM_MASK = 0x38000000,
  OBJECT_FLAG_ADDENDUM_SHIFT = 27,

  // Mask/shift for this group's generation. If out of sync with the
  // TypeZone's generation, this group hasn't been swept yet.
  OBJECT_FLAG_GENERATION_MASK = 0x40000000,
  OBJECT_FLAG_GENERATION_SHIFT = 30,
};
using ObjectGroupFlags = uint32_t;

class StackTypeSet;
class HeapTypeSet;
class TemporaryTypeSet;

/*
 * [SMDOC] Type-Inference TypeSet
 *
 * Information about the set of types associated with an lvalue. There are
 * three kinds of type sets:
 *
 * - StackTypeSet are associated with JitScripts, for arguments and values
 *   observed at property reads. These are implicitly frozen on compilation
 *   and only have constraints added to them which can trigger invalidation of
 *   TypeNewScript information.
 *
 * - HeapTypeSet are associated with the properties of ObjectGroups. These
 *   may have constraints added to them to trigger invalidation of either
 *   compiled code or TypeNewScript information.
 *
 * - TemporaryTypeSet are created during compilation and do not outlive
 *   that compilation.
 *
 * The contents of a type set completely describe the values that a particular
 * lvalue might have, except for the following cases:
 *
 * - If an object's prototype or class is dynamically mutated, its group will
 *   change. Type sets containing the old group will not necessarily contain
 *   the new group. When this occurs, the properties of the old and new group
 *   will both be marked as unknown, which will prevent Ion from optimizing
 *   based on the object's type information.
 */
class TypeSet {
 protected:
  /* Flags for this type set. */
  TypeFlags flags = 0;

 public:
  class ObjectKey;

 protected:
  /* Possible objects this type set can represent. */
  ObjectKey** objectSet = nullptr;

 public:
  TypeSet() = default;

  // Type set entry for either a JSObject with singleton type or a
  // non-singleton ObjectGroup.
  class ObjectKey {
   public:
    static intptr_t keyBits(ObjectKey* obj) { return (intptr_t)obj; }
    static ObjectKey* getKey(ObjectKey* obj) { return obj; }

    static inline ObjectKey* get(JSObject* obj);
    static inline ObjectKey* get(ObjectGroup* group);

    bool isGroup() { return (uintptr_t(this) & 1) == 0; }
    bool isSingleton() { return (uintptr_t(this) & 1) != 0; }
    static constexpr uintptr_t TypeHashSetMarkBit = 1 << 1;

    inline ObjectGroup* group();
    inline JSObject* singleton();

    inline ObjectGroup* groupNoBarrier();
    inline JSObject* singletonNoBarrier();

    const JSClass* clasp();
    TaggedProto proto();

    bool unknownProperties();
    bool hasFlags(CompilerConstraintList* constraints, ObjectGroupFlags flags);
    bool hasStableClassAndProto(CompilerConstraintList* constraints);
    void watchStateChangeForTypedArrayData(CompilerConstraintList* constraints);
    HeapTypeSetKey property(jsid id);
    void ensureTrackedProperty(JSContext* cx, jsid id);

    ObjectGroup* maybeGroup();

    JS::Compartment* maybeCompartment();
  } JS_HAZ_GC_POINTER;

  // Information about a single concrete type. We pack this into one word,
  // where small values are particular primitive or other singleton types and
  // larger values are either specific JS objects or object groups.
  class Type {
    friend class TypeSet;

    uintptr_t data;
    explicit Type(uintptr_t data) : data(data) {}

   public:
    uintptr_t raw() const { return data; }

    bool isPrimitive() const { return data < JSVAL_TYPE_OBJECT; }

    bool isPrimitive(ValueType type) const {
      MOZ_ASSERT(type != ValueType::Object);
      return uintptr_t(type) == data;
    }

    ValueType primitive() const {
      MOZ_ASSERT(isPrimitive());
      return ValueType(data);
    }

    bool isSomeObject() const {
      return data == JSVAL_TYPE_OBJECT || data > JSVAL_TYPE_UNKNOWN;
    }

    bool isAnyObject() const { return data == JSVAL_TYPE_OBJECT; }

    bool isUnknown() const { return data == JSVAL_TYPE_UNKNOWN; }

    /* Accessors for types that are either JSObject or ObjectGroup. */

    bool isObject() const {
      MOZ_ASSERT(!isAnyObject() && !isUnknown());
      return data > JSVAL_TYPE_UNKNOWN;
    }

    bool isObjectUnchecked() const { return data > JSVAL_TYPE_UNKNOWN; }

    inline ObjectKey* objectKey() const;

    /* Accessors for JSObject types */

    bool isSingleton() const { return isObject() && !!(data & 1); }
    bool isSingletonUnchecked() const {
      return isObjectUnchecked() && !!(data & 1);
    }

    inline JSObject* singleton() const;
    inline JSObject* singletonNoBarrier() const;

    /* Accessors for ObjectGroup types */

    bool isGroup() const { return isObject() && !(data & 1); }
    bool isGroupUnchecked() const { return isObjectUnchecked() && !(data & 1); }

    inline ObjectGroup* group() const;
    inline ObjectGroup* groupNoBarrier() const;

    inline void trace(JSTracer* trc);

    JS::Compartment* maybeCompartment();

    bool operator==(Type o) const { return data == o.data; }
    bool operator!=(Type o) const { return data != o.data; }
  } JS_HAZ_GC_POINTER;

  static inline Type UndefinedType() { return Type(JSVAL_TYPE_UNDEFINED); }
  static inline Type NullType() { return Type(JSVAL_TYPE_NULL); }
  static inline Type BooleanType() { return Type(JSVAL_TYPE_BOOLEAN); }
  static inline Type Int32Type() { return Type(JSVAL_TYPE_INT32); }
  static inline Type DoubleType() { return Type(JSVAL_TYPE_DOUBLE); }
  static inline Type StringType() { return Type(JSVAL_TYPE_STRING); }
  static inline Type SymbolType() { return Type(JSVAL_TYPE_SYMBOL); }
  static inline Type BigIntType() { return Type(JSVAL_TYPE_BIGINT); }
  static inline Type MagicArgType() { return Type(JSVAL_TYPE_MAGIC); }
  static inline Type AnyObjectType() { return Type(JSVAL_TYPE_OBJECT); }
  static inline Type UnknownType() { return Type(JSVAL_TYPE_UNKNOWN); }

 protected:
  static inline Type PrimitiveTypeFromTypeFlag(TypeFlags flag);

 public:
  // Returns the Type corresponding to a primitive JS::Value or MIRType. The
  // argument must not be a magic value we can't represent directly (see
  // IsUntrackedValueType and IsUntrackedMIRType).
  static inline Type PrimitiveType(const JS::Value& val);
  static inline Type PrimitiveType(jit::MIRType type);

  // Like PrimitiveType above but also accepts MIRType::Object.
  static inline Type PrimitiveOrAnyObjectType(jit::MIRType type);

  static inline Type ObjectType(const JSObject* obj);
  static inline Type ObjectType(const ObjectGroup* group);
  static inline Type ObjectType(const ObjectKey* key);

  static const char* NonObjectTypeString(Type type);

  static JS::UniqueChars TypeString(const Type type);
  static JS::UniqueChars ObjectGroupString(const ObjectGroup* group);

  void print(FILE* fp = stderr);

  /* Whether this set contains a specific type. */
  MOZ_ALWAYS_INLINE bool hasType(Type type) const;

  TypeFlags baseFlags() const { return flags & TYPE_FLAG_BASE_MASK; }
  bool unknown() const { return !!(flags & TYPE_FLAG_UNKNOWN); }
  bool unknownObject() const {
    return !!(flags & (TYPE_FLAG_UNKNOWN | TYPE_FLAG_ANYOBJECT));
  }
  bool empty() const { return !baseFlags() && !baseObjectCount(); }

  bool hasAnyFlag(TypeFlags flags) const {
    MOZ_ASSERT((flags & TYPE_FLAG_BASE_MASK) == flags);
    return !!(baseFlags() & flags);
  }

  bool nonDataProperty() const { return flags & TYPE_FLAG_NON_DATA_PROPERTY; }
  bool nonWritableProperty() const {
    return flags & TYPE_FLAG_NON_WRITABLE_PROPERTY;
  }
  bool nonConstantProperty() const {
    return flags & TYPE_FLAG_NON_CONSTANT_PROPERTY;
  }
  bool definiteProperty() const { return flags & TYPE_FLAG_DEFINITE_MASK; }
  unsigned definiteSlot() const {
    MOZ_ASSERT(definiteProperty());
    return (flags >> TYPE_FLAG_DEFINITE_SHIFT) - 1;
  }

  // Join two type sets into a new set. The result should not be modified
  // further.
  static TemporaryTypeSet* unionSets(TypeSet* a, TypeSet* b, LifoAlloc* alloc);

  // Return the intersection of the 2 TypeSets. The result should not be
  // modified further.
  static TemporaryTypeSet* intersectSets(TemporaryTypeSet* a,
                                         TemporaryTypeSet* b, LifoAlloc* alloc);

  /*
   * Returns a copy of TypeSet a excluding/removing the types in TypeSet b.
   * TypeSet b can only contain primitives or be any object. No support for
   * specific objects. The result should not be modified further.
   */
  static TemporaryTypeSet* removeSet(TemporaryTypeSet* a, TemporaryTypeSet* b,
                                     LifoAlloc* alloc);

  /* Add a type to this set using the specified allocator. */
  void addType(Type type, LifoAlloc* alloc);

  /* Get a list of all types in this set. */
  using TypeList = Vector<Type, 1, SystemAllocPolicy>;
  bool enumerateTypes(TypeList* list) const;

  /*
   * Iterate through the objects in this set. getObjectCount overapproximates
   * in the hash case (see SET_ARRAY_SIZE in TypeInference-inl.h), and
   * getObject may return nullptr.
   */
  inline unsigned getObjectCount() const;
  inline bool hasGroup(unsigned i) const;
  inline bool hasSingleton(unsigned i) const;
  inline ObjectKey* getObject(unsigned i) const;
  inline JSObject* getSingleton(unsigned i) const;
  inline ObjectGroup* getGroup(unsigned i) const;

  /*
   * Get the objects in the set without triggering barriers. This is
   * potentially unsafe and you should only call these methods if you know
   * what you're doing. For JIT compilation, use the following methods
   * instead:
   *  - MacroAssembler::getSingletonAndDelayBarrier()
   *  - MacroAssembler::getGroupAndDelayBarrier()
   */
  inline JSObject* getSingletonNoBarrier(unsigned i) const;
  inline ObjectGroup* getGroupNoBarrier(unsigned i) const;

  /* The Class of an object in this set. */
  inline const JSClass* getObjectClass(unsigned i) const;

  bool canSetDefinite(unsigned slot) {
    // Note: the cast is required to work around an MSVC issue.
    return (slot + 1) <=
           (unsigned(TYPE_FLAG_DEFINITE_MASK) >> TYPE_FLAG_DEFINITE_SHIFT);
  }
  void setDefinite(unsigned slot) {
    MOZ_ASSERT(canSetDefinite(slot));
    flags |= ((slot + 1) << TYPE_FLAG_DEFINITE_SHIFT);
    MOZ_ASSERT(definiteSlot() == slot);
  }

  /* Whether any values in this set might have the specified type. */
  bool mightBeMIRType(jit::MIRType type) const;

  /* Get whether this type set is known to be a subset of the given types. */
  bool isSubset(std::initializer_list<jit::MIRType> types) const;

  /*
   * Get whether this type set is known to be a subset of other.
   * This variant doesn't freeze constraints. That variant is called knownSubset
   */
  bool isSubset(const TypeSet* other) const;

  /*
   * Get whether the objects in this TypeSet are a subset of the objects
   * in other.
   */
  bool objectsAreSubset(TypeSet* other);

  /* Whether this TypeSet contains exactly the same types as other. */
  bool equals(const TypeSet* other) const {
    return this->isSubset(other) && other->isSubset(this);
  }

  bool objectsIntersect(const TypeSet* other) const;

  /* Forward all types in this set to the specified constraint. */
  bool addTypesToConstraint(JSContext* cx, TypeConstraint* constraint);

  // Clone a type set into an arbitrary allocator.
  TemporaryTypeSet* clone(LifoAlloc* alloc) const;

  // |*result| is not even partly initialized when this function is called:
  // this function placement-new's its contents into existence.
  bool cloneIntoUninitialized(LifoAlloc* alloc, TemporaryTypeSet* result) const;

  // Create a new TemporaryTypeSet where undefined and/or null has been filtered
  // out.
  TemporaryTypeSet* filter(LifoAlloc* alloc, bool filterUndefined,
                           bool filterNull) const;
  // Create a new TemporaryTypeSet where the type has been set to object.
  TemporaryTypeSet* cloneObjectsOnly(LifoAlloc* alloc);
  TemporaryTypeSet* cloneWithoutObjects(LifoAlloc* alloc);

  JS::Compartment* maybeCompartment();

  // Trigger a read barrier on all the contents of a type set.
  static void readBarrier(const TypeSet* types);

 protected:
  uint32_t baseObjectCount() const {
    return (flags & TYPE_FLAG_OBJECT_COUNT_MASK) >>
           TYPE_FLAG_OBJECT_COUNT_SHIFT;
  }
  inline void setBaseObjectCount(uint32_t count);

  void clearObjects();

 public:
  static inline Type GetValueType(const JS::Value& val);

  // An 'untracked' type is a value type we can't represent in a TypeSet
  // (all magic types except JS_OPTIMIZED_ARGUMENTS).
  static inline bool IsUntrackedValue(const JS::Value& val);
  static inline bool IsUntrackedMIRType(jit::MIRType type);

  // Get the type of a possibly untracked value. Returns UnknownType() for
  // such untracked values.
  static inline Type GetMaybeUntrackedValueType(const JS::Value& val);
  static inline Type GetMaybeUntrackedType(jit::MIRType type);

  static bool IsTypeMarked(JSRuntime* rt, Type* v);
  static bool IsTypeAboutToBeFinalized(Type* v);
} JS_HAZ_GC_POINTER;

#if JS_BITS_PER_WORD == 32
static const uintptr_t BaseTypeInferenceMagic = 0xa1a2b3b4;
#else
static const uintptr_t BaseTypeInferenceMagic = 0xa1a2b3b4c5c6d7d8;
#endif
static const uintptr_t TypeConstraintMagic = BaseTypeInferenceMagic + 1;
static const uintptr_t ConstraintTypeSetMagic = BaseTypeInferenceMagic + 2;

#ifdef JS_CRASH_DIAGNOSTICS
extern void ReportMagicWordFailure(uintptr_t actual, uintptr_t expected);
extern void ReportMagicWordFailure(uintptr_t actual, uintptr_t expected,
                                   uintptr_t flags, uintptr_t objectSet);
#endif

/*
 * A constraint which listens to additions to a type set and propagates those
 * changes to other type sets.
 */
class TypeConstraint {
 private:
#ifdef JS_CRASH_DIAGNOSTICS
  uintptr_t magic_ = TypeConstraintMagic;
#endif

  /* Next constraint listening to the same type set. */
  TypeConstraint* next_ = nullptr;

 public:
  TypeConstraint() = default;

  void checkMagic() const {
#ifdef JS_CRASH_DIAGNOSTICS
    if (MOZ_UNLIKELY(magic_ != TypeConstraintMagic)) {
      ReportMagicWordFailure(magic_, TypeConstraintMagic);
    }
#endif
  }

  TypeConstraint* next() const {
    checkMagic();
    if (next_) {
      next_->checkMagic();
    }
    return next_;
  }
  void setNext(TypeConstraint* next) {
    MOZ_ASSERT(!next_);
    checkMagic();
    if (next) {
      next->checkMagic();
    }
    next_ = next;
  }

  /* Debugging name for this kind of constraint. */
  virtual const char* kind() = 0;

  /* Register a new type for the set this constraint is listening to. */
  virtual void newType(JSContext* cx, TypeSet* source, TypeSet::Type type) = 0;

  /*
   * For constraints attached to an object property's type set, mark the
   * property as having changed somehow.
   */
  virtual void newPropertyState(JSContext* cx, TypeSet* source) {}

  /*
   * For constraints attached to the JSID_EMPTY type set on an object,
   * indicate a change in one of the object's dynamic property flags or other
   * state.
   */
  virtual void newObjectState(JSContext* cx, ObjectGroup* group) {}

  /*
   * If the data this constraint refers to is still live, copy it into the
   * zone's new allocator. Type constraints only hold weak references.
   */
  virtual bool sweep(TypeZone& zone, TypeConstraint** res) = 0;

  /* The associated compartment, if any. */
  virtual JS::Compartment* maybeCompartment() = 0;
};

/* Superclass common to stack and heap type sets. */
class ConstraintTypeSet : public TypeSet {
 private:
#ifdef JS_CRASH_DIAGNOSTICS
  uintptr_t magic_ = ConstraintTypeSetMagic;
#endif

 protected:
  /* Chain of constraints which propagate changes out from this type set. */
  TypeConstraint* constraintList_ = nullptr;

 public:
  ConstraintTypeSet() = default;

  void checkMagic() const {
#ifdef JS_CRASH_DIAGNOSTICS
    if (MOZ_UNLIKELY(magic_ != ConstraintTypeSetMagic)) {
      ReportMagicWordFailure(magic_, ConstraintTypeSetMagic, uintptr_t(flags),
                             uintptr_t(objectSet));
    }
#endif
  }

  // This takes a reference to AutoSweepBase to ensure we swept the owning
  // ObjectGroup or JitScript.
  TypeConstraint* constraintList(const AutoSweepBase& sweep) const {
    checkMagic();
    if (constraintList_) {
      constraintList_->checkMagic();
    }
    return constraintList_;
  }

  /*
   * Add a type to this set, calling any constraint handlers if this is a new
   * possible type.
   */
  void addType(const AutoSweepBase& sweep, JSContext* cx, Type type);

  /* Generalize to any type. */
  void makeUnknown(const AutoSweepBase& sweep, JSContext* cx) {
    addType(sweep, cx, UnknownType());
  }

  // Trigger a post barrier when writing to this set, if necessary.
  // addType(cx, type) takes care of this automatically.
  void postWriteBarrier(JSContext* cx, Type type);

  /* Add a new constraint to this set. */
  bool addConstraint(JSContext* cx, TypeConstraint* constraint,
                     bool callExisting = true);

  inline void sweep(const AutoSweepBase& sweep, JS::Zone* zone);
  inline void trace(JS::Zone* zone, JSTracer* trc);
};

class StackTypeSet : public ConstraintTypeSet {
 public:
};

class HeapTypeSet : public ConstraintTypeSet {
  inline void newPropertyState(const AutoSweepObjectGroup& sweep,
                               JSContext* cx);

 public:
  /* Mark this type set as representing a non-data property. */
  inline void setNonDataProperty(const AutoSweepObjectGroup& sweep,
                                 JSContext* cx);

  /* Mark this type set as representing a non-writable property. */
  inline void setNonWritableProperty(const AutoSweepObjectGroup& sweep,
                                     JSContext* cx);

  // Mark this type set as being non-constant.
  inline void setNonConstantProperty(const AutoSweepObjectGroup& sweep,
                                     JSContext* cx);

  // Trigger freeze constraints for this property because a lexical binding was
  // added to the global lexical environment.
  inline void markLexicalBindingExists(const AutoSweepObjectGroup& sweep,
                                       JSContext* cx);
};

enum class DOMObjectKind : uint8_t { Proxy, Native, Unknown };

class TemporaryTypeSet : public TypeSet {
 public:
  TemporaryTypeSet() { MOZ_ASSERT(!jit::JitOptions.warpBuilder); }
  TemporaryTypeSet(LifoAlloc* alloc, Type type);

  TemporaryTypeSet(uint32_t flags, ObjectKey** objectSet) {
    MOZ_ASSERT(!jit::JitOptions.warpBuilder);
    this->flags = flags;
    this->objectSet = objectSet;
  }

  inline TemporaryTypeSet(LifoAlloc* alloc, jit::MIRType type);

  /*
   * Constraints for JIT compilation.
   *
   * Methods for JIT compilation. These must be used when a script is
   * currently being compiled (see AutoEnterCompilation) and will add
   * constraints ensuring that if the return value change in the future due
   * to new type information, the script's jitcode will be discarded.
   */

  /* Get any type tag which all values in this set must have. */
  jit::MIRType getKnownMIRType();

  /* Whether this value may be an object. */
  bool maybeObject() { return unknownObject() || baseObjectCount() > 0; }

  /*
   * Whether this typeset represents a potentially sentineled object value:
   * the value may be an object or null or undefined.
   * Returns false if the value cannot ever be an object.
   */
  bool objectOrSentinel() {
    TypeFlags flags =
        TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL | TYPE_FLAG_ANYOBJECT;
    if (baseFlags() & (~flags & TYPE_FLAG_BASE_MASK)) {
      return false;
    }

    return hasAnyFlag(TYPE_FLAG_ANYOBJECT) || baseObjectCount() > 0;
  }

  /* Whether the type set contains objects with any of a set of flags. */
  bool hasObjectFlags(CompilerConstraintList* constraints,
                      ObjectGroupFlags flags);

  /* Get the class shared by all objects in this set, or nullptr. */
  const JSClass* getKnownClass(CompilerConstraintList* constraints);

  /*
   * Get the realm shared by all objects in this set, or nullptr. Returns
   * nullptr if the set contains proxies (because cross-compartment wrappers
   * don't have a single realm associated with them).
   */
  JS::Realm* getKnownRealm(CompilerConstraintList* constraints);

  /* Result returned from forAllClasses */
  enum ForAllResult {
    EMPTY = 1,  // Set empty
    ALL_TRUE,   // Set not empty and predicate returned true for all classes
    ALL_FALSE,  // Set not empty and predicate returned false for all classes
    MIXED,      // Set not empty and predicate returned false for some classes
                // and true for others, or set contains an unknown or non-object
                // type
  };

  /* Apply func to the members of the set and return an appropriate result.
   * The iteration may end early if the result becomes known early.
   */
  ForAllResult forAllClasses(CompilerConstraintList* constraints,
                             bool (*func)(const JSClass* clasp));

  /*
   * Returns true if all objects in this set have the same prototype, and
   * assigns this object to *proto. The proto can be nullptr.
   */
  bool getCommonPrototype(CompilerConstraintList* constraints,
                          JSObject** proto);

  /* Whether the buffer mapped by a TypedArray is shared memory or not */
  enum TypedArraySharedness {
    UnknownSharedness = 1,  // We can't determine sharedness
    KnownShared,            // We know for sure the buffer is shared
    KnownUnshared           // We know for sure the buffer is unshared
  };

  /*
   * Get the typed array type of all objects in this set, or
   * Scalar::MaxTypedArrayViewType. If there is such a common type and
   * sharedness is not nullptr then *sharedness is set to what we know about the
   * sharedness of the memory.
   */
  Scalar::Type getTypedArrayType(CompilerConstraintList* constraints,
                                 TypedArraySharedness* sharedness = nullptr);

  // Whether all objects have JSCLASS_IS_DOMJSCLASS set.
  bool isDOMClass(CompilerConstraintList* constraints, DOMObjectKind* kind);

  // Whether clasp->isCallable() is true for one or more objects in this set.
  bool maybeCallable(CompilerConstraintList* constraints);

  // Whether clasp->emulatesUndefined() is true for one or more objects in
  // this set.
  bool maybeEmulatesUndefined(CompilerConstraintList* constraints);

  // Get the single value which can appear in this type set, otherwise
  // nullptr.
  JSObject* maybeSingleton();
  ObjectKey* maybeSingleObject();

  // Whether any objects in the type set needs a barrier on id.
  bool propertyNeedsBarrier(CompilerConstraintList* constraints, jsid id);

  // Whether this set contains all types in other, except (possibly) the
  // specified type.
  bool filtersType(const TemporaryTypeSet* other, Type type) const;

  enum DoubleConversion {
    /* All types in the set should use eager double conversion. */
    AlwaysConvertToDoubles,

    /* Some types in the set should use eager double conversion. */
    MaybeConvertToDoubles,

    /* No types should use eager double conversion. */
    DontConvertToDoubles,

    /* Some types should use eager double conversion, others cannot. */
    AmbiguousDoubleConversion
  };

  /*
   * Whether known double optimizations are possible for element accesses on
   * objects in this type set.
   */
  DoubleConversion convertDoubleElements(CompilerConstraintList* constraints);

 private:
  void getTypedArraySharedness(CompilerConstraintList* constraints,
                               TypedArraySharedness* sharedness);
};

// Representation of a heap type property which may or may not be instantiated.
// Heap properties for singleton types are instantiated lazily as they are used
// by the compiler, but this is only done on the main thread. If we are
// compiling off thread and use a property which has not yet been instantiated,
// it will be treated as empty and non-configured and will be instantiated when
// rejoining to the main thread. If it is in fact not empty, the compilation
// will fail; to avoid this, we try to instantiate singleton property types
// during generation of baseline caches.
class HeapTypeSetKey {
  friend class TypeSet::ObjectKey;

  // Object and property being accessed.
  TypeSet::ObjectKey* object_;
  jsid id_;

  // If instantiated, the underlying heap type set.
  HeapTypeSet* maybeTypes_;

 public:
  HeapTypeSetKey() : object_(nullptr), id_(JSID_EMPTY), maybeTypes_(nullptr) {}

  TypeSet::ObjectKey* object() const { return object_; }
  jsid id() const { return id_; }

  HeapTypeSet* maybeTypes() const {
    if (maybeTypes_) {
      maybeTypes_->checkMagic();
    }
    return maybeTypes_;
  }

  bool instantiate(JSContext* cx);

  void freeze(CompilerConstraintList* constraints);
  jit::MIRType knownMIRType(CompilerConstraintList* constraints);
  bool nonData(CompilerConstraintList* constraints);
  bool nonWritable(CompilerConstraintList* constraints);
  bool isOwnProperty(CompilerConstraintList* constraints,
                     bool allowEmptyTypesForGlobal = false);
  JSObject* singleton(CompilerConstraintList* constraints);
  bool needsBarrier(CompilerConstraintList* constraints);
  bool constant(CompilerConstraintList* constraints, Value* valOut);
  bool couldBeConstant(CompilerConstraintList* constraints);
};

} /* namespace js */

#endif /* vm_TypeSet_h */
