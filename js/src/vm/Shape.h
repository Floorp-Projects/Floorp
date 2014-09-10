/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Shape_h
#define vm_Shape_h

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TemplateLib.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsinfer.h"
#include "jspropertytree.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "gc/Barrier.h"
#include "gc/Heap.h"
#include "gc/Marking.h"
#include "gc/Rooting.h"
#include "js/HashTable.h"
#include "js/MemoryMetrics.h"
#include "js/RootingAPI.h"
#include "vm/PropDesc.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

/*
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
 *    are unshared, private to a single object, and immutable except for their
 *    links in the dictionary list.
 *
 * All shape lineages are bi-directionally linked, via the |parent| and
 * |kids|/|listp| members.
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
 * Shape in the property tree exceeds MAX_LINEAR_SEARCHES and the Shape's
 * lineage has (excluding the EmptyShape) at least MIN_ENTRIES, we create an
 * auxiliary hash table -- the ShapeTable -- that allows faster lookup.
 * Furthermore, a ShapeTable is always created for dictionary mode lists,
 * and it is attached to the last Shape in the lineage. Shape tables for
 * property tree Shapes never change, but shape tables for dictionary mode
 * Shapes can grow and shrink.
 *
 * There used to be a long, math-heavy comment here explaining why property
 * trees are more space-efficient than alternatives.  This was removed in bug
 * 631138; see that bug for the full details.
 *
 * Because many Shapes have similar data, there is actually a secondary type
 * called a BaseShape that holds some of a Shape's data.  Many shapes can share
 * a single BaseShape.
 */

#define JSSLOT_FREE(clasp)  JSCLASS_RESERVED_SLOTS(clasp)

namespace js {

class Bindings;
class Debugger;
class Nursery;
class ObjectImpl;
class StaticBlockObject;

namespace gc {
class ForkJoinNursery;
}

typedef JSPropertyOp         PropertyOp;
typedef JSStrictPropertyOp   StrictPropertyOp;
typedef JSPropertyDescriptor PropertyDescriptor;

/* Limit on the number of slotful properties in an object. */
static const uint32_t SHAPE_INVALID_SLOT = JS_BIT(24) - 1;
static const uint32_t SHAPE_MAXIMUM_SLOT = JS_BIT(24) - 2;

/*
 * Shapes use multiplicative hashing, but specialized to
 * minimize footprint.
 */
struct ShapeTable {
    static const uint32_t HASH_BITS     = mozilla::tl::BitSize<HashNumber>::value;
    static const uint32_t MIN_ENTRIES   = 11;

    // This value is low because it's common for a ShapeTable to be created
    // with an entryCount of zero.
    static const uint32_t MIN_SIZE_LOG2 = 2;
    static const uint32_t MIN_SIZE      = JS_BIT(MIN_SIZE_LOG2);

    int             hashShift;          /* multiplicative hash shift */

    uint32_t        entryCount;         /* number of entries in table */
    uint32_t        removedCount;       /* removed entry sentinels in table */
    uint32_t        freelist;           /* SHAPE_INVALID_SLOT or head of slot
                                           freelist in owning dictionary-mode
                                           object */
    js::Shape       **entries;          /* table of ptrs to shared tree nodes */

    explicit ShapeTable(uint32_t nentries)
      : hashShift(HASH_BITS - MIN_SIZE_LOG2),
        entryCount(nentries),
        removedCount(0),
        freelist(SHAPE_INVALID_SLOT)
    {
        /* NB: entries is set by init, which must be called. */
    }

    ~ShapeTable() {
        js_free(entries);
    }

    /* By definition, hashShift = HASH_BITS - log2(capacity). */
    uint32_t capacity() const { return JS_BIT(HASH_BITS - hashShift); }

    /*
     * This counts the ShapeTable object itself (which must be
     * heap-allocated) and its |entries| array.
     */
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(this) + mallocSizeOf(entries);
    }

    /* Whether we need to grow.  We want to do this if the load factor is >= 0.75 */
    bool needsToGrow() const {
        uint32_t size = capacity();
        return entryCount + removedCount >= size - (size >> 2);
    }

    /*
     * Try to grow the table.  On failure, reports out of memory on cx
     * and returns false.  This will make any extant pointers into the
     * table invalid.  Don't call this unless needsToGrow() is true.
     */
    bool grow(ThreadSafeContext *cx);

    /*
     * NB: init and change are fallible but do not report OOM, so callers can
     * cope or ignore. They do however use the context's calloc method in
     * order to update the malloc counter on success.
     */
    bool            init(ThreadSafeContext *cx, Shape *lastProp);
    bool            change(int log2Delta, ThreadSafeContext *cx);
    Shape           **search(jsid id, bool adding);

#ifdef JSGC_COMPACTING
    /* Update entries whose shapes have been moved */
    void            fixupAfterMovingGC();
#endif
};

/*
 * Reuse the API-only JSPROP_INDEX attribute to mean shadowability.
 */
#define JSPROP_SHADOWABLE       JSPROP_INDEX

/*
 * Shapes encode information about both a property lineage *and* a particular
 * property. This information is split across the Shape and the BaseShape
 * at shape->base(). Both Shape and BaseShape can be either owned or unowned
 * by, respectively, the Object or Shape referring to them.
 *
 * Owned Shapes are used in dictionary objects, and form a doubly linked list
 * whose entries are all owned by that dictionary. Unowned Shapes are all in
 * the property tree.
 *
 * Owned BaseShapes are used for shapes which have shape tables, including
 * the last properties in all dictionaries. Unowned BaseShapes compactly store
 * information common to many shapes. In a given compartment there is a single
 * BaseShape for each combination of BaseShape information. This information
 * is cloned in owned BaseShapes so that information can be quickly looked up
 * for a given object or shape without regard to whether the base shape is
 * owned or not.
 *
 * All combinations of owned/unowned Shapes/BaseShapes are possible:
 *
 * Owned Shape, Owned BaseShape:
 *
 *     Last property in a dictionary object. The BaseShape is transferred from
 *     property to property as the object's last property changes.
 *
 * Owned Shape, Unowned BaseShape:
 *
 *     Property in a dictionary object other than the last one.
 *
 * Unowned Shape, Owned BaseShape:
 *
 *     Property in the property tree which has a shape table.
 *
 * Unowned Shape, Unowned BaseShape:
 *
 *     Property in the property tree which does not have a shape table.
 *
 * BaseShapes additionally encode some information about the referring object
 * itself. This includes the object's class, parent and various flags that may
 * be set for the object. Except for the class, this information is mutable and
 * may change when the object has an established property lineage. On such
 * changes the entire property lineage is not updated, but rather only the
 * last property (and its base shape). This works because only the object's
 * last property is used to query information about the object. Care must be
 * taken to call JSObject::canRemoveLastProperty when unwinding an object to
 * an earlier property, however.
 */

class Shape;
class UnownedBaseShape;
struct StackBaseShape;

namespace gc {
void MergeCompartments(JSCompartment *source, JSCompartment *target);
}

static inline void
GetterSetterWriteBarrierPost(JSRuntime *rt, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    JS_ASSERT(objp);
    JS_ASSERT(*objp);
    gc::Cell **cellp = reinterpret_cast<gc::Cell **>(objp);
    gc::StoreBuffer *storeBuffer = (*cellp)->storeBuffer();
    if (storeBuffer)
        storeBuffer->putRelocatableCellFromAnyThread(cellp);
#endif
}

static inline void
GetterSetterWriteBarrierPostRemove(JSRuntime *rt, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    JS::shadow::Runtime *shadowRuntime = JS::shadow::Runtime::asShadowRuntime(rt);
    shadowRuntime->gcStoreBufferPtr()->removeRelocatableCellFromAnyThread(reinterpret_cast<gc::Cell **>(objp));
#endif
}

class BaseShape : public gc::BarrieredCell<BaseShape>
{
  public:
    friend class Shape;
    friend struct StackBaseShape;
    friend struct StackShape;
    friend void gc::MergeCompartments(JSCompartment *source, JSCompartment *target);

    enum Flag {
        /* Owned by the referring shape. */
        OWNED_SHAPE        = 0x1,

        /* getterObj/setterObj are active in unions below. */
        HAS_GETTER_OBJECT  = 0x2,
        HAS_SETTER_OBJECT  = 0x4,

        /*
         * Flags set which describe the referring object. Once set these cannot
         * be unset (except during object densification of sparse indexes), and
         * are transferred from shape to shape as the object's last property
         * changes.
         *
         * If you add a new flag here, please add appropriate code to
         * JSObject::dump to dump it as part of object representation.
         */

        DELEGATE            =    0x8,
        NOT_EXTENSIBLE      =   0x10,
        INDEXED             =   0x20,
        BOUND_FUNCTION      =   0x40,
        HAD_ELEMENTS_ACCESS =   0x80,
        WATCHED             =  0x100,
        ITERATED_SINGLETON  =  0x200,
        NEW_TYPE_UNKNOWN    =  0x400,
        UNCACHEABLE_PROTO   =  0x800,

        // These two flags control which scope a new variables ends up on in the
        // scope chain. If the variable is "qualified" (i.e., if it was defined
        // using var, let, or const) then it ends up on the lowest scope in the
        // chain that has the QUALIFIED_VAROBJ flag set. If it's "unqualified"
        // (i.e., if it was introduced without any var, let, or const, which
        // incidentally is an error in strict mode) then it goes on the lowest
        // scope in the chain with the UNQUALIFIED_VAROBJ flag set (which is
        // typically the global).
        QUALIFIED_VAROBJ    = 0x1000,
        UNQUALIFIED_VAROBJ  = 0x2000,

        OBJECT_FLAG_MASK    = 0x3ff8
    };

  private:
    const Class         *clasp_;        /* Class of referring object. */
    HeapPtrObject       parent;         /* Parent of referring object. */
    HeapPtrObject       metadata;       /* Optional holder of metadata about
                                         * the referring object. */
    JSCompartment       *compartment_;  /* Compartment shape belongs to. */
    uint32_t            flags;          /* Vector of above flags. */
    uint32_t            slotSpan_;      /* Object slot span for BaseShapes at
                                         * dictionary last properties. */

    union {
        PropertyOp      rawGetter;      /* getter hook for shape */
        JSObject        *getterObj;     /* user-defined callable "get" object or
                                           null if shape->hasGetterValue() */
    };

    union {
        StrictPropertyOp rawSetter;     /* setter hook for shape */
        JSObject        *setterObj;     /* user-defined callable "set" object or
                                           null if shape->hasSetterValue() */
    };

    /* For owned BaseShapes, the canonical unowned BaseShape. */
    HeapPtrUnownedBaseShape unowned_;

    /* For owned BaseShapes, the shape's shape table. */
    ShapeTable       *table_;

    BaseShape(const BaseShape &base) MOZ_DELETE;

  public:
    void finalize(FreeOp *fop);

    BaseShape(JSCompartment *comp, const Class *clasp, JSObject *parent, JSObject *metadata,
              uint32_t objectFlags)
    {
        JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
        mozilla::PodZero(this);
        this->clasp_ = clasp;
        this->parent = parent;
        this->metadata = metadata;
        this->flags = objectFlags;
        this->compartment_ = comp;
    }

    BaseShape(JSCompartment *comp, const Class *clasp, JSObject *parent, JSObject *metadata,
              uint32_t objectFlags, uint8_t attrs,
              PropertyOp rawGetter, StrictPropertyOp rawSetter)
    {
        JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
        mozilla::PodZero(this);
        this->clasp_ = clasp;
        this->parent = parent;
        this->metadata = metadata;
        this->flags = objectFlags;
        this->rawGetter = rawGetter;
        this->rawSetter = rawSetter;
        if ((attrs & JSPROP_GETTER) && rawGetter) {
            this->flags |= HAS_GETTER_OBJECT;
            GetterSetterWriteBarrierPost(runtimeFromMainThread(), &this->getterObj);
        }
        if ((attrs & JSPROP_SETTER) && rawSetter) {
            this->flags |= HAS_SETTER_OBJECT;
            GetterSetterWriteBarrierPost(runtimeFromMainThread(), &this->setterObj);
        }
        this->compartment_ = comp;
    }

    explicit inline BaseShape(const StackBaseShape &base);

    /* Not defined: BaseShapes must not be stack allocated. */
    ~BaseShape();

    BaseShape &operator=(const BaseShape &other) {
        clasp_ = other.clasp_;
        parent = other.parent;
        metadata = other.metadata;
        flags = other.flags;
        slotSpan_ = other.slotSpan_;
        if (flags & HAS_GETTER_OBJECT) {
            getterObj = other.getterObj;
            GetterSetterWriteBarrierPost(runtimeFromMainThread(), &getterObj);
        } else {
            if (rawGetter)
                GetterSetterWriteBarrierPostRemove(runtimeFromMainThread(), &getterObj);
            rawGetter = other.rawGetter;
        }
        if (flags & HAS_SETTER_OBJECT) {
            setterObj = other.setterObj;
            GetterSetterWriteBarrierPost(runtimeFromMainThread(), &setterObj);
        } else {
            if (rawSetter)
                GetterSetterWriteBarrierPostRemove(runtimeFromMainThread(), &setterObj);
            rawSetter = other.rawSetter;
        }
        compartment_ = other.compartment_;
        return *this;
    }

    const Class *clasp() const { return clasp_; }

    bool isOwned() const { return !!(flags & OWNED_SHAPE); }

    bool matchesGetterSetter(PropertyOp rawGetter, StrictPropertyOp rawSetter) const {
        return rawGetter == this->rawGetter && rawSetter == this->rawSetter;
    }

    inline void adoptUnowned(UnownedBaseShape *other);

    void setOwned(UnownedBaseShape *unowned) {
        flags |= OWNED_SHAPE;
        this->unowned_ = unowned;
    }

    JSObject *getObjectParent() const { return parent; }
    JSObject *getObjectMetadata() const { return metadata; }
    uint32_t getObjectFlags() const { return flags & OBJECT_FLAG_MASK; }

    bool hasGetterObject() const { return !!(flags & HAS_GETTER_OBJECT); }
    JSObject *getterObject() const { JS_ASSERT(hasGetterObject()); return getterObj; }

    bool hasSetterObject() const { return !!(flags & HAS_SETTER_OBJECT); }
    JSObject *setterObject() const { JS_ASSERT(hasSetterObject()); return setterObj; }

    bool hasTable() const { JS_ASSERT_IF(table_, isOwned()); return table_ != nullptr; }
    ShapeTable &table() const { JS_ASSERT(table_ && isOwned()); return *table_; }
    void setTable(ShapeTable *table) { JS_ASSERT(isOwned()); table_ = table; }

    uint32_t slotSpan() const { JS_ASSERT(isOwned()); return slotSpan_; }
    void setSlotSpan(uint32_t slotSpan) { JS_ASSERT(isOwned()); slotSpan_ = slotSpan; }

    JSCompartment *compartment() const { return compartment_; }

    /*
     * Lookup base shapes from the compartment's baseShapes table, adding if
     * not already found.
     */
    static UnownedBaseShape* getUnowned(ExclusiveContext *cx, StackBaseShape &base);

    /*
     * Lookup base shapes from the compartment's baseShapes table, returning
     * nullptr if not found.
     */
    static UnownedBaseShape *lookupUnowned(ThreadSafeContext *cx, const StackBaseShape &base);

    /* Get the canonical base shape. */
    inline UnownedBaseShape* unowned();

    /* Get the canonical base shape for an owned one. */
    inline UnownedBaseShape* baseUnowned();

    /* Get the canonical base shape for an unowned one (i.e. identity). */
    inline UnownedBaseShape* toUnowned();

    /* Check that an owned base shape is consistent with its unowned base. */
    void assertConsistency();

    /* For JIT usage */
    static inline size_t offsetOfParent() { return offsetof(BaseShape, parent); }
    static inline size_t offsetOfFlags() { return offsetof(BaseShape, flags); }

    static inline ThingRootKind rootKind() { return THING_ROOT_BASE_SHAPE; }

    void markChildren(JSTracer *trc) {
        if (hasGetterObject())
            gc::MarkObjectUnbarriered(trc, &getterObj, "getter");

        if (hasSetterObject())
            gc::MarkObjectUnbarriered(trc, &setterObj, "setter");

        if (isOwned())
            gc::MarkBaseShape(trc, &unowned_, "base");

        if (parent)
            gc::MarkObject(trc, &parent, "parent");

        if (metadata)
            gc::MarkObject(trc, &metadata, "metadata");
    }

#ifdef JSGC_COMPACTING
    void fixupAfterMovingGC();
#endif

  private:
    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(BaseShape, clasp_) == offsetof(js::shadow::BaseShape, clasp_));
    }
};

class UnownedBaseShape : public BaseShape {};

inline void
BaseShape::adoptUnowned(UnownedBaseShape *other)
{
    // This is a base shape owned by a dictionary object, update it to reflect the
    // unowned base shape of a new last property.
    JS_ASSERT(isOwned());

    uint32_t span = slotSpan();
    ShapeTable *table = &this->table();

    *this = *other;
    setOwned(other);
    setTable(table);
    setSlotSpan(span);

    assertConsistency();
}

UnownedBaseShape *
BaseShape::unowned()
{
    return isOwned() ? baseUnowned() : toUnowned();
}

UnownedBaseShape *
BaseShape::toUnowned()
{
    JS_ASSERT(!isOwned() && !unowned_); return static_cast<UnownedBaseShape *>(this);
}

UnownedBaseShape*
BaseShape::baseUnowned()
{
    JS_ASSERT(isOwned() && unowned_); return unowned_;
}

/* Entries for the per-compartment baseShapes set of unowned base shapes. */
struct StackBaseShape : public DefaultHasher<ReadBarrieredUnownedBaseShape>
{
    typedef const StackBaseShape *Lookup;

    uint32_t flags;
    const Class *clasp;
    JSObject *parent;
    JSObject *metadata;
    PropertyOp rawGetter;
    StrictPropertyOp rawSetter;
    JSCompartment *compartment;

    explicit StackBaseShape(BaseShape *base)
      : flags(base->flags & BaseShape::OBJECT_FLAG_MASK),
        clasp(base->clasp_),
        parent(base->parent),
        metadata(base->metadata),
        rawGetter(nullptr),
        rawSetter(nullptr),
        compartment(base->compartment())
    {}

    inline StackBaseShape(ThreadSafeContext *cx, const Class *clasp,
                          JSObject *parent, JSObject *metadata, uint32_t objectFlags);
    explicit inline StackBaseShape(Shape *shape);

    void updateGetterSetter(uint8_t attrs, PropertyOp rawGetter, StrictPropertyOp rawSetter) {
        flags &= ~(BaseShape::HAS_GETTER_OBJECT | BaseShape::HAS_SETTER_OBJECT);
        if ((attrs & JSPROP_GETTER) && rawGetter) {
            JS_ASSERT(!IsPoisonedPtr(rawGetter));
            flags |= BaseShape::HAS_GETTER_OBJECT;
        }
        if ((attrs & JSPROP_SETTER) && rawSetter) {
            JS_ASSERT(!IsPoisonedPtr(rawSetter));
            flags |= BaseShape::HAS_SETTER_OBJECT;
        }

        this->rawGetter = rawGetter;
        this->rawSetter = rawSetter;
    }

    static inline HashNumber hash(const StackBaseShape *lookup);
    static inline bool match(UnownedBaseShape *key, const StackBaseShape *lookup);

    // For RootedGeneric<StackBaseShape*>
    void trace(JSTracer *trc);
};

inline
BaseShape::BaseShape(const StackBaseShape &base)
{
    mozilla::PodZero(this);
    this->clasp_ = base.clasp;
    this->parent = base.parent;
    this->metadata = base.metadata;
    this->flags = base.flags;
    this->rawGetter = base.rawGetter;
    this->rawSetter = base.rawSetter;
    if ((base.flags & HAS_GETTER_OBJECT) && base.rawGetter)
        GetterSetterWriteBarrierPost(runtimeFromMainThread(), &this->getterObj);
    if ((base.flags & HAS_SETTER_OBJECT) && base.rawSetter)
        GetterSetterWriteBarrierPost(runtimeFromMainThread(), &this->setterObj);
    this->compartment_ = base.compartment;
}

typedef HashSet<ReadBarrieredUnownedBaseShape,
                StackBaseShape,
                SystemAllocPolicy> BaseShapeSet;


class Shape : public gc::BarrieredCell<Shape>
{
    friend class ::JSObject;
    friend class ::JSFunction;
    friend class js::Bindings;
    friend class js::Nursery;
    friend class js::gc::ForkJoinNursery;
    friend class js::ObjectImpl;
    friend class js::PropertyTree;
    friend class js::StaticBlockObject;
    friend struct js::StackShape;
    friend struct js::StackBaseShape;

  protected:
    HeapPtrBaseShape    base_;
    PreBarrieredId      propid_;

    JS_ENUM_HEADER(SlotInfo, uint32_t)
    {
        /* Number of fixed slots in objects with this shape. */
        // FIXED_SLOTS_MAX is the biggest count of fixed slots a Shape can store
        FIXED_SLOTS_MAX        = 0x1f,
        FIXED_SLOTS_SHIFT      = 27,
        FIXED_SLOTS_MASK       = uint32_t(FIXED_SLOTS_MAX << FIXED_SLOTS_SHIFT),

        /*
         * numLinearSearches starts at zero and is incremented initially on
         * search() calls. Once numLinearSearches reaches LINEAR_SEARCHES_MAX,
         * the table is created on the next search() call. The table can also
         * be created when hashifying for dictionary mode.
         */
        LINEAR_SEARCHES_MAX    = 0x7,
        LINEAR_SEARCHES_SHIFT  = 24,
        LINEAR_SEARCHES_MASK   = LINEAR_SEARCHES_MAX << LINEAR_SEARCHES_SHIFT,

        /*
         * Mask to get the index in object slots for shapes which hasSlot().
         * For !hasSlot() shapes in the property tree with a parent, stores the
         * parent's slot index (which may be invalid), and invalid for all
         * other shapes.
         */
        SLOT_MASK              = JS_BIT(24) - 1
    } JS_ENUM_FOOTER(SlotInfo);

    uint32_t            slotInfo;       /* mask of above info */
    uint8_t             attrs;          /* attributes, see jsapi.h JSPROP_* */
    uint8_t             flags;          /* flags, see below for defines */

    HeapPtrShape        parent;        /* parent node, reverse for..in order */
    /* kids is valid when !inDictionary(), listp is valid when inDictionary(). */
    union {
        KidsPointer kids;       /* null, single child, or a tagged ptr
                                   to many-kids data structure */
        HeapPtrShape *listp;    /* dictionary list starting at shape_
                                   has a double-indirect back pointer,
                                   either to the next shape's parent if not
                                   last, else to obj->shape_ */
    };

    static inline Shape *search(ExclusiveContext *cx, Shape *start, jsid id,
                                Shape ***pspp, bool adding = false);
    static inline Shape *searchThreadLocal(ThreadSafeContext *cx, Shape *start, jsid id,
                                           Shape ***pspp, bool adding = false);
    static inline Shape *searchNoHashify(Shape *start, jsid id);

    void removeFromDictionary(ObjectImpl *obj);
    void insertIntoDictionary(HeapPtrShape *dictp);

    void initDictionaryShape(const StackShape &child, uint32_t nfixed, HeapPtrShape *dictp) {
        new (this) Shape(child, nfixed);
        this->flags |= IN_DICTIONARY;

        this->listp = nullptr;
        insertIntoDictionary(dictp);
    }

    /* Replace the base shape of the last shape in a non-dictionary lineage with base. */
    static Shape *replaceLastProperty(ExclusiveContext *cx, StackBaseShape &base,
                                      TaggedProto proto, HandleShape shape);

    /*
     * This function is thread safe if every shape in the lineage of |shape|
     * is thread local, which is the case when we clone the entire shape
     * lineage in preparation for converting an object to dictionary mode.
     */
    static bool hashify(ThreadSafeContext *cx, Shape *shape);
    void handoffTableTo(Shape *newShape);

    void setParent(Shape *p) {
        JS_ASSERT_IF(p && !p->hasMissingSlot() && !inDictionary(),
                     p->maybeSlot() <= maybeSlot());
        JS_ASSERT_IF(p && !inDictionary(),
                     hasSlot() == (p->maybeSlot() != maybeSlot()));
        parent = p;
    }

    bool ensureOwnBaseShape(ThreadSafeContext *cx) {
        if (base()->isOwned())
            return true;
        return makeOwnBaseShape(cx);
    }

    bool makeOwnBaseShape(ThreadSafeContext *cx);

  public:
    bool hasTable() const { return base()->hasTable(); }
    ShapeTable &table() const { return base()->table(); }

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                JS::ClassInfo *info) const
    {
        if (hasTable()) {
            if (inDictionary())
                info->shapesMallocHeapDictTables += table().sizeOfIncludingThis(mallocSizeOf);
            else
                info->shapesMallocHeapTreeTables += table().sizeOfIncludingThis(mallocSizeOf);
        }

        if (!inDictionary() && kids.isHash())
            info->shapesMallocHeapTreeKids += kids.toHash()->sizeOfIncludingThis(mallocSizeOf);
    }

    bool isNative() const {
        JS_ASSERT(!(flags & NON_NATIVE) == getObjectClass()->isNative());
        return !(flags & NON_NATIVE);
    }

    const HeapPtrShape &previous() const { return parent; }
    JSCompartment *compartment() const { return base()->compartment(); }

    template <AllowGC allowGC>
    class Range {
      protected:
        friend class Shape;

        typename MaybeRooted<Shape*, allowGC>::RootType cursor;

      public:
        Range(ExclusiveContext *cx, Shape *shape) : cursor(cx, shape) {
            JS_STATIC_ASSERT(allowGC == CanGC);
        }

        explicit Range(Shape *shape) : cursor((ExclusiveContext *) nullptr, shape) {
            JS_STATIC_ASSERT(allowGC == NoGC);
        }

        bool empty() const {
            return !cursor || cursor->isEmptyShape();
        }

        Shape &front() const {
            JS_ASSERT(!empty());
            return *cursor;
        }

        void popFront() {
            JS_ASSERT(!empty());
            cursor = cursor->parent;
        }
    };

    const Class *getObjectClass() const {
        return base()->clasp_;
    }
    JSObject *getObjectParent() const { return base()->parent; }
    JSObject *getObjectMetadata() const { return base()->metadata; }

    static Shape *setObjectParent(ExclusiveContext *cx,
                                  JSObject *obj, TaggedProto proto, Shape *last);
    static Shape *setObjectMetadata(JSContext *cx,
                                    JSObject *metadata, TaggedProto proto, Shape *last);
    static Shape *setObjectFlag(ExclusiveContext *cx,
                                BaseShape::Flag flag, TaggedProto proto, Shape *last);

    uint32_t getObjectFlags() const { return base()->getObjectFlags(); }
    bool hasObjectFlag(BaseShape::Flag flag) const {
        JS_ASSERT(!(flag & ~BaseShape::OBJECT_FLAG_MASK));
        return !!(base()->flags & flag);
    }

  protected:
    /*
     * Implementation-private bits stored in shape->flags. See public: enum {}
     * flags further below, which were allocated FCFS over time, so interleave
     * with these bits.
     */
    enum {
        /* Property is placeholder for a non-native class. */
        NON_NATIVE      = 0x01,

        /* Property stored in per-object dictionary, not shared property tree. */
        IN_DICTIONARY   = 0x02,

        /*
         * Slotful property was stored to more than once. This is used as a
         * hint for type inference.
         */
        OVERWRITTEN     = 0x04,

        UNUSED_BITS     = 0x38
    };

    /* Get a shape identical to this one, without parent/kids information. */
    inline Shape(const StackShape &other, uint32_t nfixed);

    /* Used by EmptyShape (see jsscopeinlines.h). */
    inline Shape(UnownedBaseShape *base, uint32_t nfixed);

    /* Copy constructor disabled, to avoid misuse of the above form. */
    Shape(const Shape &other) MOZ_DELETE;

    /*
     * Whether this shape has a valid slot value. This may be true even if
     * !hasSlot() (see SlotInfo comment above), and may be false even if
     * hasSlot() if the shape is being constructed and has not had a slot
     * assigned yet. After construction, hasSlot() implies !hasMissingSlot().
     */
    bool hasMissingSlot() const { return maybeSlot() == SHAPE_INVALID_SLOT; }

  public:
    bool inDictionary() const {
        return (flags & IN_DICTIONARY) != 0;
    }

    PropertyOp getter() const { return base()->rawGetter; }
    bool hasDefaultGetter() const {return !base()->rawGetter; }
    PropertyOp getterOp() const { JS_ASSERT(!hasGetterValue()); return base()->rawGetter; }
    JSObject *getterObject() const { JS_ASSERT(hasGetterValue()); return base()->getterObj; }

    // Per ES5, decode null getterObj as the undefined value, which encodes as null.
    Value getterValue() const {
        JS_ASSERT(hasGetterValue());
        return base()->getterObj ? ObjectValue(*base()->getterObj) : UndefinedValue();
    }

    Value getterOrUndefined() const {
        return (hasGetterValue() && base()->getterObj)
               ? ObjectValue(*base()->getterObj)
               : UndefinedValue();
    }

    StrictPropertyOp setter() const { return base()->rawSetter; }
    bool hasDefaultSetter() const  { return !base()->rawSetter; }
    StrictPropertyOp setterOp() const { JS_ASSERT(!hasSetterValue()); return base()->rawSetter; }
    JSObject *setterObject() const { JS_ASSERT(hasSetterValue()); return base()->setterObj; }

    // Per ES5, decode null setterObj as the undefined value, which encodes as null.
    Value setterValue() const {
        JS_ASSERT(hasSetterValue());
        return base()->setterObj ? ObjectValue(*base()->setterObj) : UndefinedValue();
    }

    Value setterOrUndefined() const {
        return (hasSetterValue() && base()->setterObj)
               ? ObjectValue(*base()->setterObj)
               : UndefinedValue();
    }

    void setOverwritten() {
        flags |= OVERWRITTEN;
    }
    bool hadOverwrite() const {
        return flags & OVERWRITTEN;
    }

    void update(PropertyOp getter, StrictPropertyOp setter, uint8_t attrs);

    bool matches(const Shape *other) const {
        return propid_.get() == other->propid_.get() &&
               matchesParamsAfterId(other->base(), other->maybeSlot(), other->attrs, other->flags);
    }

    inline bool matches(const StackShape &other) const;

    bool matchesParamsAfterId(BaseShape *base, uint32_t aslot, unsigned aattrs, unsigned aflags) const
    {
        return base->unowned() == this->base()->unowned() &&
               maybeSlot() == aslot &&
               attrs == aattrs;
    }

    bool get(JSContext* cx, HandleObject receiver, JSObject *obj, JSObject *pobj, MutableHandleValue vp);
    bool set(JSContext* cx, HandleObject obj, HandleObject receiver, bool strict, MutableHandleValue vp);

    BaseShape *base() const { return base_.get(); }

    bool hasSlot() const {
        return (attrs & JSPROP_SHARED) == 0;
    }
    uint32_t slot() const { JS_ASSERT(hasSlot() && !hasMissingSlot()); return maybeSlot(); }
    uint32_t maybeSlot() const {
        return slotInfo & SLOT_MASK;
    }

    bool isEmptyShape() const {
        JS_ASSERT_IF(JSID_IS_EMPTY(propid_), hasMissingSlot());
        return JSID_IS_EMPTY(propid_);
    }

    uint32_t slotSpan(const Class *clasp) const {
        JS_ASSERT(!inDictionary());
        uint32_t free = JSSLOT_FREE(clasp);
        return hasMissingSlot() ? free : Max(free, maybeSlot() + 1);
    }

    uint32_t slotSpan() const {
        return slotSpan(getObjectClass());
    }

    void setSlot(uint32_t slot) {
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        slotInfo = slotInfo & ~Shape::SLOT_MASK;
        slotInfo = slotInfo | slot;
    }

    uint32_t numFixedSlots() const {
        return slotInfo >> FIXED_SLOTS_SHIFT;
    }

    void setNumFixedSlots(uint32_t nfixed) {
        JS_ASSERT(nfixed < FIXED_SLOTS_MAX);
        slotInfo = slotInfo & ~FIXED_SLOTS_MASK;
        slotInfo = slotInfo | (nfixed << FIXED_SLOTS_SHIFT);
    }

    uint32_t numLinearSearches() const {
        return (slotInfo & LINEAR_SEARCHES_MASK) >> LINEAR_SEARCHES_SHIFT;
    }

    void incrementNumLinearSearches() {
        uint32_t count = numLinearSearches();
        JS_ASSERT(count < LINEAR_SEARCHES_MAX);
        slotInfo = slotInfo & ~LINEAR_SEARCHES_MASK;
        slotInfo = slotInfo | ((count + 1) << LINEAR_SEARCHES_SHIFT);
    }

    const PreBarrieredId &propid() const {
        JS_ASSERT(!isEmptyShape());
        JS_ASSERT(!JSID_IS_VOID(propid_));
        return propid_;
    }
    PreBarrieredId &propidRef() { JS_ASSERT(!JSID_IS_VOID(propid_)); return propid_; }
    jsid propidRaw() const {
        // Return the actual jsid, not an internal reference.
        return propid();
    }

    uint8_t attributes() const { return attrs; }
    bool configurable() const { return (attrs & JSPROP_PERMANENT) == 0; }
    bool enumerable() const { return (attrs & JSPROP_ENUMERATE) != 0; }
    bool writable() const {
        return (attrs & JSPROP_READONLY) == 0;
    }
    bool hasGetterValue() const { return attrs & JSPROP_GETTER; }
    bool hasSetterValue() const { return attrs & JSPROP_SETTER; }

    bool isDataDescriptor() const {
        return (attrs & (JSPROP_SETTER | JSPROP_GETTER)) == 0;
    }
    bool isAccessorDescriptor() const {
        return (attrs & (JSPROP_SETTER | JSPROP_GETTER)) != 0;
    }

    PropDesc::Writability writability() const {
        return (attrs & JSPROP_READONLY) ? PropDesc::NonWritable : PropDesc::Writable;
    }
    PropDesc::Enumerability enumerability() const {
        return (attrs & JSPROP_ENUMERATE) ? PropDesc::Enumerable : PropDesc::NonEnumerable;
    }
    PropDesc::Configurability configurability() const {
        return (attrs & JSPROP_PERMANENT) ? PropDesc::NonConfigurable : PropDesc::Configurable;
    }

    /*
     * For ES5 compatibility, we allow properties with PropertyOp-flavored
     * setters to be shadowed when set. The "own" property thereby created in
     * the directly referenced object will have the same getter and setter as
     * the prototype property. See bug 552432.
     */
    bool shadowable() const {
        JS_ASSERT_IF(isDataDescriptor(), writable());
        return hasSlot() || (attrs & JSPROP_SHADOWABLE);
    }

    uint32_t entryCount() {
        if (hasTable())
            return table().entryCount;
        uint32_t count = 0;
        for (Shape::Range<NoGC> r(this); !r.empty(); r.popFront())
            ++count;
        return count;
    }

    bool isBigEnoughForAShapeTable() {
        JS_ASSERT(!hasTable());
        Shape *shape = this;
        uint32_t count = 0;
        for (Shape::Range<NoGC> r(shape); !r.empty(); r.popFront()) {
            ++count;
            if (count >= ShapeTable::MIN_ENTRIES)
                return true;
        }
        return false;
    }

#ifdef DEBUG
    void dump(JSContext *cx, FILE *fp) const;
    void dumpSubtree(JSContext *cx, int level, FILE *fp) const;
#endif

    void sweep();
    void finalize(FreeOp *fop);
    void removeChild(Shape *child);

    static inline ThingRootKind rootKind() { return THING_ROOT_SHAPE; }

    void markChildren(JSTracer *trc) {
        MarkBaseShape(trc, &base_, "base");
        gc::MarkId(trc, &propidRef(), "propid");
        if (parent)
            MarkShape(trc, &parent, "parent");
    }

    inline Shape *search(ExclusiveContext *cx, jsid id);
    inline Shape *searchLinear(jsid id);

#ifdef JSGC_COMPACTING
    void fixupAfterMovingGC();
#endif

    /* For JIT usage */
    static inline size_t offsetOfBase() { return offsetof(Shape, base_); }

  private:
#ifdef JSGC_COMPACTING
    void fixupDictionaryShapeAfterMovingGC();
    void fixupShapeTreeAfterMovingGC();
#endif

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(Shape, base_) == offsetof(js::shadow::Shape, base));
        JS_STATIC_ASSERT(offsetof(Shape, slotInfo) == offsetof(js::shadow::Shape, slotInfo));
        JS_STATIC_ASSERT(FIXED_SLOTS_SHIFT == js::shadow::Shape::FIXED_SLOTS_SHIFT);
        static_assert(js::shadow::Object::MAX_FIXED_SLOTS <= FIXED_SLOTS_MAX,
                      "verify numFixedSlots() bitfield is big enough");
    }
};

inline
StackBaseShape::StackBaseShape(Shape *shape)
  : flags(shape->getObjectFlags()),
    clasp(shape->getObjectClass()),
    parent(shape->getObjectParent()),
    metadata(shape->getObjectMetadata()),
    compartment(shape->compartment())
{
    updateGetterSetter(shape->attrs, shape->getter(), shape->setter());
}

class AutoRooterGetterSetter
{
    class Inner : private JS::CustomAutoRooter
    {
      public:
        inline Inner(ThreadSafeContext *cx, uint8_t attrs,
                     PropertyOp *pgetter_, StrictPropertyOp *psetter_);

      private:
        virtual void trace(JSTracer *trc);

        uint8_t attrs;
        PropertyOp *pgetter;
        StrictPropertyOp *psetter;
    };

  public:
    inline AutoRooterGetterSetter(ThreadSafeContext *cx, uint8_t attrs,
                                  PropertyOp *pgetter, StrictPropertyOp *psetter
                                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  private:
    mozilla::Maybe<Inner> inner;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct EmptyShape : public js::Shape
{
    EmptyShape(UnownedBaseShape *base, uint32_t nfixed)
      : js::Shape(base, nfixed)
    {
        // Only empty shapes can be NON_NATIVE.
        if (!getObjectClass()->isNative())
            flags |= NON_NATIVE;
    }

    /*
     * Lookup an initial shape matching the given parameters, creating an empty
     * shape if none was found.
     */
    static Shape *getInitialShape(ExclusiveContext *cx, const Class *clasp,
                                  TaggedProto proto, JSObject *metadata,
                                  JSObject *parent, size_t nfixed, uint32_t objectFlags = 0);
    static Shape *getInitialShape(ExclusiveContext *cx, const Class *clasp,
                                  TaggedProto proto, JSObject *metadata,
                                  JSObject *parent, gc::AllocKind kind, uint32_t objectFlags = 0);

    /*
     * Reinsert an alternate initial shape, to be returned by future
     * getInitialShape calls, until the new shape becomes unreachable in a GC
     * and the table entry is purged.
     */
    static void insertInitialShape(ExclusiveContext *cx, HandleShape shape, HandleObject proto);

    /*
     * Some object subclasses are allocated with a built-in set of properties.
     * The first time such an object is created, these built-in properties must
     * be set manually, to compute an initial shape.  Afterward, that initial
     * shape can be reused for newly-created objects that use the subclass's
     * standard prototype.  This method should be used in a post-allocation
     * init method, to ensure that objects of such subclasses compute and cache
     * the initial shape, if it hasn't already been computed.
     */
    template<class ObjectSubclass>
    static inline bool
    ensureInitialCustomShape(ExclusiveContext *cx, Handle<ObjectSubclass*> obj);
};

/*
 * Entries for the per-compartment initialShapes set indexing initial shapes
 * for objects in the compartment and the associated types.
 */
struct InitialShapeEntry
{
    /*
     * Initial shape to give to the object. This is an empty shape, except for
     * certain classes (e.g. String, RegExp) which may add certain baked-in
     * properties.
     */
    ReadBarrieredShape shape;

    /*
     * Matching prototype for the entry. The shape of an object determines its
     * prototype, but the prototype cannot be determined from the shape itself.
     */
    TaggedProto proto;

    /* State used to determine a match on an initial shape. */
    struct Lookup {
        const Class *clasp;
        TaggedProto hashProto;
        TaggedProto matchProto;
        JSObject *hashParent;
        JSObject *matchParent;
        JSObject *hashMetadata;
        JSObject *matchMetadata;
        uint32_t nfixed;
        uint32_t baseFlags;
        Lookup(const Class *clasp, TaggedProto proto, JSObject *parent, JSObject *metadata,
               uint32_t nfixed, uint32_t baseFlags)
          : clasp(clasp),
            hashProto(proto), matchProto(proto),
            hashParent(parent), matchParent(parent),
            hashMetadata(metadata), matchMetadata(metadata),
            nfixed(nfixed), baseFlags(baseFlags)
        {}

#ifdef JSGC_GENERATIONAL
        /*
         * For use by generational GC post barriers. Look up an entry whose
         * parent and metadata fields may have been moved, but was hashed with
         * the original values.
         */
        Lookup(const Class *clasp, TaggedProto proto,
               JSObject *hashParent, JSObject *matchParent,
               JSObject *hashMetadata, JSObject *matchMetadata,
               uint32_t nfixed, uint32_t baseFlags)
          : clasp(clasp),
            hashProto(proto), matchProto(proto),
            hashParent(hashParent), matchParent(matchParent),
            hashMetadata(hashMetadata), matchMetadata(matchMetadata),
            nfixed(nfixed), baseFlags(baseFlags)
        {}
#endif
    };

    inline InitialShapeEntry();
    inline InitialShapeEntry(const ReadBarrieredShape &shape, TaggedProto proto);

    inline Lookup getLookup() const;

    static inline HashNumber hash(const Lookup &lookup);
    static inline bool match(const InitialShapeEntry &key, const Lookup &lookup);
    static void rekey(InitialShapeEntry &k, const InitialShapeEntry& newKey) { k = newKey; }
};

typedef HashSet<InitialShapeEntry, InitialShapeEntry, SystemAllocPolicy> InitialShapeSet;

struct StackShape
{
    /* For performance, StackShape only roots when absolutely necessary. */
    UnownedBaseShape *base;
    jsid             propid;
    uint32_t         slot_;
    uint8_t          attrs;
    uint8_t          flags;

    explicit StackShape(UnownedBaseShape *base, jsid propid, uint32_t slot,
                        unsigned attrs, unsigned flags)
      : base(base),
        propid(propid),
        slot_(slot),
        attrs(uint8_t(attrs)),
        flags(uint8_t(flags))
    {
        JS_ASSERT(base);
        JS_ASSERT(!JSID_IS_VOID(propid));
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        JS_ASSERT_IF(attrs & (JSPROP_GETTER | JSPROP_SETTER), attrs & JSPROP_SHARED);
    }

    explicit StackShape(Shape *shape)
      : base(shape->base()->unowned()),
        propid(shape->propidRef()),
        slot_(shape->maybeSlot()),
        attrs(shape->attrs),
        flags(shape->flags)
    {}

    bool hasSlot() const { return (attrs & JSPROP_SHARED) == 0; }
    bool hasMissingSlot() const { return maybeSlot() == SHAPE_INVALID_SLOT; }

    uint32_t slot() const { JS_ASSERT(hasSlot() && !hasMissingSlot()); return slot_; }
    uint32_t maybeSlot() const { return slot_; }

    uint32_t slotSpan() const {
        uint32_t free = JSSLOT_FREE(base->clasp_);
        return hasMissingSlot() ? free : (maybeSlot() + 1);
    }

    void setSlot(uint32_t slot) {
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        slot_ = slot;
    }

    HashNumber hash() const {
        HashNumber hash = uintptr_t(base);

        /* Accumulate from least to most random so the low bits are most random. */
        hash = mozilla::RotateLeft(hash, 4) ^ attrs;
        hash = mozilla::RotateLeft(hash, 4) ^ slot_;
        hash = mozilla::RotateLeft(hash, 4) ^ JSID_BITS(propid);
        return hash;
    }

    // For RootedGeneric<StackShape*>
    void trace(JSTracer *trc);
};

} /* namespace js */

/* js::Shape pointer tag bit indicating a collision. */
#define SHAPE_COLLISION                 (uintptr_t(1))
#define SHAPE_REMOVED                   ((js::Shape *) SHAPE_COLLISION)

/* Functions to get and set shape pointer values and collision flags. */

inline bool
SHAPE_IS_FREE(js::Shape *shape)
{
    return shape == nullptr;
}

inline bool
SHAPE_IS_REMOVED(js::Shape *shape)
{
    return shape == SHAPE_REMOVED;
}

inline bool
SHAPE_IS_LIVE(js::Shape *shape)
{
    return shape > SHAPE_REMOVED;
}

inline void
SHAPE_FLAG_COLLISION(js::Shape **spp, js::Shape *shape)
{
    *spp = reinterpret_cast<js::Shape*>(uintptr_t(shape) | SHAPE_COLLISION);
}

inline bool
SHAPE_HAD_COLLISION(js::Shape *shape)
{
    return uintptr_t(shape) & SHAPE_COLLISION;
}

inline js::Shape *
SHAPE_CLEAR_COLLISION(js::Shape *shape)
{
    return reinterpret_cast<js::Shape*>(uintptr_t(shape) & ~SHAPE_COLLISION);
}

inline js::Shape *
SHAPE_FETCH(js::Shape **spp)
{
    return SHAPE_CLEAR_COLLISION(*spp);
}

inline void
SHAPE_STORE_PRESERVING_COLLISION(js::Shape **spp, js::Shape *shape)
{
    *spp = reinterpret_cast<js::Shape*>(uintptr_t(shape) |
                                        uintptr_t(SHAPE_HAD_COLLISION(*spp)));
}

namespace js {

inline
Shape::Shape(const StackShape &other, uint32_t nfixed)
  : base_(other.base),
    propid_(other.propid),
    slotInfo(other.maybeSlot() | (nfixed << FIXED_SLOTS_SHIFT)),
    attrs(other.attrs),
    flags(other.flags),
    parent(nullptr)
{
    JS_ASSERT_IF(attrs & (JSPROP_GETTER | JSPROP_SETTER), attrs & JSPROP_SHARED);
    kids.setNull();
}

inline
Shape::Shape(UnownedBaseShape *base, uint32_t nfixed)
  : base_(base),
    propid_(JSID_EMPTY),
    slotInfo(SHAPE_INVALID_SLOT | (nfixed << FIXED_SLOTS_SHIFT)),
    attrs(JSPROP_SHARED),
    flags(0),
    parent(nullptr)
{
    JS_ASSERT(base);
    kids.setNull();
}

inline Shape *
Shape::searchLinear(jsid id)
{
    /*
     * Non-dictionary shapes can acquire a table at any point the main thread
     * is operating on it, so other threads inspecting such shapes can't use
     * their table without racing. This function can be called from any thread
     * on any non-dictionary shape.
     */
    JS_ASSERT(!inDictionary());

    for (Shape *shape = this; shape; ) {
        if (shape->propidRef() == id)
            return shape;
        shape = shape->parent;
    }

    return nullptr;
}

/*
 * Keep this function in sync with search. It neither hashifies the start
 * shape nor increments linear search count.
 */
inline Shape *
Shape::searchNoHashify(Shape *start, jsid id)
{
    /*
     * If we have a table, search in the shape table, else do a linear
     * search. We never hashify into a table in parallel.
     */
    if (start->hasTable()) {
        Shape **spp = start->table().search(id, false);
        return SHAPE_FETCH(spp);
    }

    return start->searchLinear(id);
}

inline bool
Shape::matches(const StackShape &other) const
{
    return propid_.get() == other.propid &&
           matchesParamsAfterId(other.base, other.slot_, other.attrs, other.flags);
}

template<> struct RootKind<Shape *> : SpecificRootKind<Shape *, THING_ROOT_SHAPE> {};
template<> struct RootKind<BaseShape *> : SpecificRootKind<BaseShape *, THING_ROOT_BASE_SHAPE> {};

// Property lookup hooks on objects are required to return a non-nullptr shape
// to signify that the property has been found. For cases where the property is
// not actually represented by a Shape, use a dummy value. This includes all
// properties of non-native objects, and dense elements for native objects.
// Use separate APIs for these two cases.

static inline void
MarkNonNativePropertyFound(MutableHandleShape propp)
{
    propp.set(reinterpret_cast<Shape*>(1));
}

template <AllowGC allowGC>
static inline void
MarkDenseOrTypedArrayElementFound(typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp)
{
    propp.set(reinterpret_cast<Shape*>(1));
}

static inline bool
IsImplicitDenseOrTypedArrayElement(Shape *prop)
{
    return prop == reinterpret_cast<Shape*>(1);
}

} // namespace js

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

namespace JS {
template<> class AnchorPermitted<js::Shape *> { };
template<> class AnchorPermitted<const js::Shape *> { };
}

#endif /* vm_Shape_h */
