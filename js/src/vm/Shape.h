/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Shape_h___
#define Shape_h___

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"

#include "jsobj.h"
#include "jspropertytree.h"
#include "jstypes.h"

#include "gc/Heap.h"
#include "js/HashTable.h"
#include "js/RootingAPI.h"

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

class JSObject;

namespace js {

class Bindings;
class Nursery;

/* Limit on the number of slotful properties in an object. */
static const uint32_t SHAPE_INVALID_SLOT = JS_BIT(24) - 1;
static const uint32_t SHAPE_MAXIMUM_SLOT = JS_BIT(24) - 2;

/*
 * Shapes use multiplicative hashing, but specialized to
 * minimize footprint.
 */
struct ShapeTable {
    static const uint32_t HASH_BITS     = tl::BitSize<HashNumber>::result;
    static const uint32_t MIN_ENTRIES   = 7;
    static const uint32_t MIN_SIZE_LOG2 = 4;
    static const uint32_t MIN_SIZE      = JS_BIT(MIN_SIZE_LOG2);

    int             hashShift;          /* multiplicative hash shift */

    uint32_t        entryCount;         /* number of entries in table */
    uint32_t        removedCount;       /* removed entry sentinels in table */
    uint32_t        freelist;           /* SHAPE_INVALID_SLOT or head of slot
                                           freelist in owning dictionary-mode
                                           object */
    js::Shape       **entries;          /* table of ptrs to shared tree nodes */

    ShapeTable(uint32_t nentries)
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

    /* Computes the size of the entries array for a given capacity. */
    static size_t sizeOfEntries(size_t cap) { return cap * sizeof(Shape *); }

    /*
     * This counts the ShapeTable object itself (which must be
     * heap-allocated) and its |entries| array.
     */
    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const {
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
    bool grow(JSContext *cx);

    /*
     * NB: init and change are fallible but do not report OOM, so callers can
     * cope or ignore. They do however use JSRuntime's calloc_ method in order
     * to update the malloc counter on success.
     */
    bool            init(JSRuntime *rt, Shape *lastProp);
    bool            change(int log2Delta, JSContext *cx);
    Shape           **search(jsid id, bool adding);
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

class BaseShape : public js::gc::Cell
{
  public:
    friend class Shape;
    friend struct StackBaseShape;
    friend struct StackShape;

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
         */

        DELEGATE            =    0x8,
        NOT_EXTENSIBLE      =   0x10,
        INDEXED             =   0x20,
        BOUND_FUNCTION      =   0x40,
        VAROBJ              =   0x80,
        WATCHED             =  0x100,
        ITERATED_SINGLETON  =  0x200,
        NEW_TYPE_UNKNOWN    =  0x400,
        UNCACHEABLE_PROTO   =  0x800,
        HAD_ELEMENTS_ACCESS = 0x1000,

        OBJECT_FLAG_MASK    = 0x1ff8
    };

  private:
    Class               *clasp;         /* Class of referring object. */
    HeapPtrObject       parent;         /* Parent of referring object. */
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
    HeapPtr<UnownedBaseShape> unowned_;

    /* For owned BaseShapes, the shape's shape table. */
    ShapeTable       *table_;

#if JS_BITS_PER_WORD == 32
    void             *padding;
#endif

    BaseShape(const BaseShape &base) MOZ_DELETE;

  public:
    void finalize(FreeOp *fop);

    inline BaseShape(JSCompartment *comp, Class *clasp, JSObject *parent, uint32_t objectFlags);
    inline BaseShape(JSCompartment *comp, Class *clasp, JSObject *parent, uint32_t objectFlags,
                     uint8_t attrs, PropertyOp rawGetter, StrictPropertyOp rawSetter);
    inline BaseShape(const StackBaseShape &base);

    /* Not defined: BaseShapes must not be stack allocated. */
    ~BaseShape();

    inline BaseShape &operator=(const BaseShape &other);

    bool isOwned() const { return !!(flags & OWNED_SHAPE); }

    inline bool matchesGetterSetter(PropertyOp rawGetter,
                                    StrictPropertyOp rawSetter) const;

    inline void adoptUnowned(UnownedBaseShape *other);
    inline void setOwned(UnownedBaseShape *unowned);

    JSObject *getObjectParent() const { return parent; }
    uint32_t getObjectFlags() const { return flags & OBJECT_FLAG_MASK; }

    bool hasGetterObject() const { return !!(flags & HAS_GETTER_OBJECT); }
    JSObject *getterObject() const { JS_ASSERT(hasGetterObject()); return getterObj; }

    bool hasSetterObject() const { return !!(flags & HAS_SETTER_OBJECT); }
    JSObject *setterObject() const { JS_ASSERT(hasSetterObject()); return setterObj; }

    bool hasTable() const { JS_ASSERT_IF(table_, isOwned()); return table_ != NULL; }
    ShapeTable &table() const { JS_ASSERT(table_ && isOwned()); return *table_; }
    void setTable(ShapeTable *table) { JS_ASSERT(isOwned()); table_ = table; }

    uint32_t slotSpan() const { JS_ASSERT(isOwned()); return slotSpan_; }
    void setSlotSpan(uint32_t slotSpan) { JS_ASSERT(isOwned()); slotSpan_ = slotSpan; }

    JSCompartment *compartment() const { return compartment_; }
    JS::Zone *zone() const { return tenuredZone(); }

    /* Lookup base shapes from the compartment's baseShapes table. */
    static UnownedBaseShape* getUnowned(JSContext *cx, const StackBaseShape &base);

    /* Get the canonical base shape. */
    inline UnownedBaseShape* unowned();

    /* Get the canonical base shape for an owned one. */
    inline UnownedBaseShape* baseUnowned();

    /* Get the canonical base shape for an unowned one (i.e. identity). */
    inline UnownedBaseShape* toUnowned();

    /* Check that an owned base shape is consistent with its unowned base. */
    inline void assertConsistency();

    /* For JIT usage */
    static inline size_t offsetOfParent() { return offsetof(BaseShape, parent); }
    static inline size_t offsetOfFlags() { return offsetof(BaseShape, flags); }

    static inline void writeBarrierPre(BaseShape *shape);
    static inline void writeBarrierPost(BaseShape *shape, void *addr);
    static inline void readBarrier(BaseShape *shape);

    static inline ThingRootKind rootKind() { return THING_ROOT_BASE_SHAPE; }

    inline void markChildren(JSTracer *trc);

  private:
    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(BaseShape, clasp) == offsetof(js::shadow::BaseShape, clasp));
    }
};

class UnownedBaseShape : public BaseShape {};

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
struct StackBaseShape
{
    typedef const StackBaseShape *Lookup;

    uint32_t flags;
    Class *clasp;
    JSObject *parent;
    PropertyOp rawGetter;
    StrictPropertyOp rawSetter;
    JSCompartment *compartment;

    explicit StackBaseShape(BaseShape *base)
      : flags(base->flags & BaseShape::OBJECT_FLAG_MASK),
        clasp(base->clasp),
        parent(base->parent),
        rawGetter(NULL),
        rawSetter(NULL),
        compartment(base->compartment())
    {}

    StackBaseShape(JSCompartment *comp, Class *clasp, JSObject *parent, uint32_t objectFlags)
      : flags(objectFlags),
        clasp(clasp),
        parent(parent),
        rawGetter(NULL),
        rawSetter(NULL),
        compartment(comp)
    {}

    inline StackBaseShape(Shape *shape);

    inline void updateGetterSetter(uint8_t attrs,
                                   PropertyOp rawGetter,
                                   StrictPropertyOp rawSetter);

    static inline HashNumber hash(const StackBaseShape *lookup);
    static inline bool match(UnownedBaseShape *key, const StackBaseShape *lookup);

    class AutoRooter : private JS::CustomAutoRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, const StackBaseShape *base_
                            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
          : CustomAutoRooter(cx), base(base_), skip(cx, base_)
        {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        }

      private:
        virtual void trace(JSTracer *trc);

        const StackBaseShape *base;
        SkipRoot skip;
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
};

typedef HashSet<ReadBarriered<UnownedBaseShape>,
                StackBaseShape,
                SystemAllocPolicy> BaseShapeSet;


class Shape : public js::gc::Cell
{
    friend class ::JSObject;
    friend class ::JSFunction;
    friend class js::Bindings;
    friend class js::Nursery;
    friend class js::ObjectImpl;
    friend class js::PropertyTree;
    friend class js::StaticBlockObject;
    friend struct js::StackShape;
    friend struct js::StackBaseShape;

  protected:
    HeapPtrBaseShape    base_;
    EncapsulatedId      propid_;

    JS_ENUM_HEADER(SlotInfo, uint32_t)
    {
        /* Number of fixed slots in objects with this shape. */
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
    int16_t             shortid_;       /* tinyid, or local arg/var index */

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

    static inline Shape *search(JSContext *cx, Shape *start, jsid id,
                                  Shape ***pspp, bool adding = false);
    static inline Shape *searchNoHashify(Shape *start, jsid id);

    inline void removeFromDictionary(ObjectImpl *obj);
    inline void insertIntoDictionary(HeapPtrShape *dictp);

    inline void initDictionaryShape(const StackShape &child, uint32_t nfixed,
                                    HeapPtrShape *dictp);

    Shape *getChildBinding(JSContext *cx, const StackShape &child);

    /* Replace the base shape of the last shape in a non-dictionary lineage with base. */
    static Shape *replaceLastProperty(JSContext *cx, const StackBaseShape &base,
                                        TaggedProto proto, HandleShape shape);

    static bool hashify(JSContext *cx, Shape *shape);
    void handoffTableTo(Shape *newShape);

    inline void setParent(Shape *p);

    bool ensureOwnBaseShape(JSContext *cx) {
        if (base()->isOwned())
            return true;
        return makeOwnBaseShape(cx);
    }

    bool makeOwnBaseShape(JSContext *cx);

  public:
    bool hasTable() const { return base()->hasTable(); }
    ShapeTable &table() const { return base()->table(); }

    void sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf,
                             size_t *propTableSize, size_t *kidsSize) const {
        *propTableSize = hasTable() ? table().sizeOfIncludingThis(mallocSizeOf) : 0;
        *kidsSize = !inDictionary() && kids.isHash()
                  ? kids.toHash()->sizeOfIncludingThis(mallocSizeOf)
                  : 0;
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
        Range(JSContext *cx, Shape *shape) : cursor(cx, shape) {
            JS_STATIC_ASSERT(allowGC == CanGC);
        }

        Range(Shape *shape) : cursor(NULL, shape) {
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

    Class *getObjectClass() const { return base()->clasp; }
    JSObject *getObjectParent() const { return base()->parent; }

    static Shape *setObjectParent(JSContext *cx, JSObject *obj, TaggedProto proto, Shape *last);
    static Shape *setObjectFlag(JSContext *cx, BaseShape::Flag flag, TaggedProto proto, Shape *last);

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

        UNUSED_BITS     = 0x3C
    };

    /* Get a shape identical to this one, without parent/kids information. */
    Shape(const StackShape &other, uint32_t nfixed);

    /* Used by EmptyShape (see jsscopeinlines.h). */
    Shape(UnownedBaseShape *base, uint32_t nfixed);

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
    /* Public bits stored in shape->flags. */
    enum {
        HAS_SHORTID     = 0x40,
        PUBLIC_FLAGS    = HAS_SHORTID
    };

    bool inDictionary() const   { return (flags & IN_DICTIONARY) != 0; }
    unsigned getFlags() const  { return flags & PUBLIC_FLAGS; }
    bool hasShortID() const { return (flags & HAS_SHORTID) != 0; }

    PropertyOp getter() const { return base()->rawGetter; }
    bool hasDefaultGetter() const  { return !base()->rawGetter; }
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

    void update(PropertyOp getter, StrictPropertyOp setter, uint8_t attrs);

    inline bool matches(const Shape *other) const;
    inline bool matches(const StackShape &other) const;
    inline bool matchesParamsAfterId(BaseShape *base,
                                     uint32_t aslot, unsigned aattrs, unsigned aflags,
                                     int ashortid) const;

    bool get(JSContext* cx, HandleObject receiver, JSObject *obj, JSObject *pobj, MutableHandleValue vp);
    bool set(JSContext* cx, HandleObject obj, HandleObject receiver, bool strict, MutableHandleValue vp);

    BaseShape *base() const { return base_.get(); }

    bool hasSlot() const { return (attrs & JSPROP_SHARED) == 0; }
    uint32_t slot() const { JS_ASSERT(hasSlot() && !hasMissingSlot()); return maybeSlot(); }
    uint32_t maybeSlot() const { return slotInfo & SLOT_MASK; }

    bool isEmptyShape() const {
        JS_ASSERT_IF(JSID_IS_EMPTY(propid_), hasMissingSlot());
        return JSID_IS_EMPTY(propid_);
    }

    uint32_t slotSpan() const {
        JS_ASSERT(!inDictionary());
        uint32_t free = JSSLOT_FREE(getObjectClass());
        return hasMissingSlot() ? free : Max(free, maybeSlot() + 1);
    }

    void setSlot(uint32_t slot) {
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        slotInfo = slotInfo & ~Shape::SLOT_MASK;
        slotInfo = slotInfo | slot;
    }

    uint32_t numFixedSlots() const {
        return (slotInfo >> FIXED_SLOTS_SHIFT);
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

    const EncapsulatedId &propid() const {
        JS_ASSERT(!isEmptyShape());
        JS_ASSERT(!JSID_IS_VOID(propid_));
        return propid_;
    }
    EncapsulatedId &propidRef() { JS_ASSERT(!JSID_IS_VOID(propid_)); return propid_; }

    int16_t shortid() const { JS_ASSERT(hasShortID()); return maybeShortid(); }
    int16_t maybeShortid() const { return shortid_; }

    /*
     * If SHORTID is set in shape->flags, we use shape->shortid rather
     * than id when calling shape's getter or setter.
     */
    inline bool getUserId(JSContext *cx, MutableHandleId idp) const;

    uint8_t attributes() const { return attrs; }
    bool configurable() const { return (attrs & JSPROP_PERMANENT) == 0; }
    bool enumerable() const { return (attrs & JSPROP_ENUMERATE) != 0; }
    bool writable() const {
        // JS_ASSERT(isDataDescriptor());
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

        Shape *shape = this;
        uint32_t count = 0;
        for (Shape::Range<NoGC> r(shape); !r.empty(); r.popFront())
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

    JS::Zone *zone() const { return tenuredZone(); }

    static inline void writeBarrierPre(Shape *shape);
    static inline void writeBarrierPost(Shape *shape, void *addr);

    /*
     * All weak references need a read barrier for incremental GC. This getter
     * method implements the read barrier. It's used to obtain initial shapes
     * from the compartment.
     */
    static inline void readBarrier(Shape *shape);

    static inline ThingRootKind rootKind() { return THING_ROOT_SHAPE; }

    inline void markChildren(JSTracer *trc);

    inline Shape *search(JSContext *cx, jsid id) {
        Shape **_;
        return search(cx, this, id, &_);
    }

    /* For JIT usage */
    static inline size_t offsetOfBase() { return offsetof(Shape, base_); }

  private:
    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(Shape, base_) == offsetof(js::shadow::Shape, base));
        JS_STATIC_ASSERT(offsetof(Shape, slotInfo) == offsetof(js::shadow::Shape, slotInfo));
        JS_STATIC_ASSERT(FIXED_SLOTS_SHIFT == js::shadow::Shape::FIXED_SLOTS_SHIFT);
    }
};

class AutoRooterGetterSetter
{
    class Inner : private JS::CustomAutoRooter
    {
      public:
        Inner(JSContext *cx, uint8_t attrs,
              PropertyOp *pgetter_, StrictPropertyOp *psetter_)
          : CustomAutoRooter(cx), attrs(attrs),
            pgetter(pgetter_), psetter(psetter_),
            getterRoot(cx, pgetter_), setterRoot(cx, psetter_)
        {
            JS_ASSERT_IF(attrs & JSPROP_GETTER, !IsPoisonedPtr(*pgetter));
            JS_ASSERT_IF(attrs & JSPROP_SETTER, !IsPoisonedPtr(*psetter));
        }

      private:
        virtual void trace(JSTracer *trc);

        uint8_t attrs;
        PropertyOp *pgetter;
        StrictPropertyOp *psetter;
        SkipRoot getterRoot, setterRoot;
    };

  public:
    explicit AutoRooterGetterSetter(JSContext *cx, uint8_t attrs,
                                    PropertyOp *pgetter, StrictPropertyOp *psetter
                                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
            inner.construct(cx, attrs, pgetter, psetter);
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

  private:
    mozilla::Maybe<Inner> inner;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct EmptyShape : public js::Shape
{
    EmptyShape(UnownedBaseShape *base, uint32_t nfixed);

    /*
     * Lookup an initial shape matching the given parameters, creating an empty
     * shape if none was found.
     */
    static Shape *getInitialShape(JSContext *cx, Class *clasp, TaggedProto proto,
                                  JSObject *parent, size_t nfixed, uint32_t objectFlags = 0);
    static Shape *getInitialShape(JSContext *cx, Class *clasp, TaggedProto proto,
                                  JSObject *parent, gc::AllocKind kind, uint32_t objectFlags = 0);

    /*
     * Reinsert an alternate initial shape, to be returned by future
     * getInitialShape calls, until the new shape becomes unreachable in a GC
     * and the table entry is purged.
     */
    static void insertInitialShape(JSContext *cx, HandleShape shape, HandleObject proto);
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
    ReadBarriered<Shape> shape;

    /*
     * Matching prototype for the entry. The shape of an object determines its
     * prototype, but the prototype cannot be determined from the shape itself.
     */
    TaggedProto proto;

    /* State used to determine a match on an initial shape. */
    struct Lookup {
        Class *clasp;
        TaggedProto proto;
        JSObject *parent;
        uint32_t nfixed;
        uint32_t baseFlags;
        Lookup(Class *clasp, TaggedProto proto, JSObject *parent, uint32_t nfixed,
               uint32_t baseFlags)
            : clasp(clasp), proto(proto), parent(parent),
              nfixed(nfixed), baseFlags(baseFlags)
        {}
    };

    inline InitialShapeEntry();
    inline InitialShapeEntry(const ReadBarriered<Shape> &shape, TaggedProto proto);

    inline Lookup getLookup() const;

    static inline HashNumber hash(const Lookup &lookup);
    static inline bool match(const InitialShapeEntry &key, const Lookup &lookup);
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
    int16_t          shortid;

    explicit StackShape(UnownedBaseShape *base, jsid propid, uint32_t slot,
                        uint32_t nfixed, unsigned attrs, unsigned flags, int shortid)
      : base(base),
        propid(propid),
        slot_(slot),
        attrs(uint8_t(attrs)),
        flags(uint8_t(flags)),
        shortid(int16_t(shortid))
    {
        JS_ASSERT(base);
        JS_ASSERT(!JSID_IS_VOID(propid));
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
    }

    StackShape(Shape *const &shape)
      : base(shape->base()->unowned()),
        propid(shape->propidRef()),
        slot_(shape->slotInfo & Shape::SLOT_MASK),
        attrs(shape->attrs),
        flags(shape->flags),
        shortid(shape->shortid_)
    {}

    bool hasSlot() const { return (attrs & JSPROP_SHARED) == 0; }
    bool hasMissingSlot() const { return maybeSlot() == SHAPE_INVALID_SLOT; }

    uint32_t slot() const { JS_ASSERT(hasSlot() && !hasMissingSlot()); return slot_; }
    uint32_t maybeSlot() const { return slot_; }

    uint32_t slotSpan() const {
        uint32_t free = JSSLOT_FREE(base->clasp);
        return hasMissingSlot() ? free : (maybeSlot() + 1);
    }

    void setSlot(uint32_t slot) {
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        slot_ = slot;
    }

    inline HashNumber hash() const;

    class AutoRooter : private JS::CustomAutoRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, const StackShape *shape_
                            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
          : CustomAutoRooter(cx), shape(shape_), skip(cx, shape_)
        {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        }

      private:
        virtual void trace(JSTracer *trc);

        const StackShape *shape;
        SkipRoot skip;
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
 };

} /* namespace js */

/* js::Shape pointer tag bit indicating a collision. */
#define SHAPE_COLLISION                 (uintptr_t(1))
#define SHAPE_REMOVED                   ((Shape *) SHAPE_COLLISION)

/* Macros to get and set shape pointer values and collision flags. */
#define SHAPE_IS_FREE(shape)            ((shape) == NULL)
#define SHAPE_IS_REMOVED(shape)         ((shape) == SHAPE_REMOVED)
#define SHAPE_IS_LIVE(shape)            ((shape) > SHAPE_REMOVED)
#define SHAPE_FLAG_COLLISION(spp,shape) (*(spp) = (Shape *)                  \
                                         (uintptr_t(shape) | SHAPE_COLLISION))
#define SHAPE_HAD_COLLISION(shape)      (uintptr_t(shape) & SHAPE_COLLISION)
#define SHAPE_FETCH(spp)                SHAPE_CLEAR_COLLISION(*(spp))

#define SHAPE_CLEAR_COLLISION(shape)                                          \
    ((Shape *) (uintptr_t(shape) & ~SHAPE_COLLISION))

#define SHAPE_STORE_PRESERVING_COLLISION(spp, shape)                          \
    (*(spp) = (Shape *) (uintptr_t(shape) | SHAPE_HAD_COLLISION(*(spp))))

namespace js {

inline Shape *
Shape::search(JSContext *cx, Shape *start, jsid id, Shape ***pspp, bool adding)
{
    if (start->inDictionary()) {
        *pspp = start->table().search(id, adding);
        return SHAPE_FETCH(*pspp);
    }

    *pspp = NULL;

    if (start->hasTable()) {
        Shape **spp = start->table().search(id, adding);
        return SHAPE_FETCH(spp);
    }

    if (start->numLinearSearches() == LINEAR_SEARCHES_MAX) {
        if (start->isBigEnoughForAShapeTable()) {
            if (Shape::hashify(cx, start)) {
                Shape **spp = start->table().search(id, adding);
                return SHAPE_FETCH(spp);
            }
        }
        /*
         * No table built -- there weren't enough entries, or OOM occurred.
         * Don't increment numLinearSearches, to keep hasTable() false.
         */
        JS_ASSERT(!start->hasTable());
    } else {
        start->incrementNumLinearSearches();
    }

    for (Shape *shape = start; shape; shape = shape->parent) {
        if (shape->propidRef() == id)
            return shape;
    }

    return NULL;
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

    for (Shape *shape = start; shape; shape = shape->parent) {
        if (shape->propidRef() == id)
            return shape;
    }

    return NULL;
}

template<> struct RootKind<Shape *> : SpecificRootKind<Shape *, THING_ROOT_SHAPE> {};
template<> struct RootKind<BaseShape *> : SpecificRootKind<BaseShape *, THING_ROOT_BASE_SHAPE> {};

} // namespace js

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

namespace JS {
template<> class AnchorPermitted<js::Shape *> { };
template<> class AnchorPermitted<const js::Shape *> { };
}

#endif /* Shape_h___ */
