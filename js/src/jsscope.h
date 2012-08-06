/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsscope_h___
#define jsscope_h___
/*
 * JS symbol tables.
 */
#include <new>
#ifdef DEBUG
#include <stdio.h>
#endif

#include "jsobj.h"
#include "jspropertytree.h"
#include "jstypes.h"

#include "js/HashTable.h"
#include "gc/Root.h"
#include "mozilla/Attributes.h"

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

namespace js {

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
        js::UnwantedForeground::free_(entries);
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
    bool            init(JSRuntime *rt, js::Shape *lastProp);
    bool            change(int log2Delta, JSContext *cx);
    js::Shape       **search(jsid id, bool adding);
};

} /* namespace js */

struct JSObject;

namespace js {

class PropertyTree;

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

class UnownedBaseShape;

class BaseShape : public js::gc::Cell
{
  public:
    friend struct Shape;
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
         * be unset, and are transferred from shape to shape as the object's
         * last property changes.
         */

        EXTENSIBLE_PARENTS =    0x8,
        DELEGATE           =   0x10,
        SYSTEM             =   0x20,
        NOT_EXTENSIBLE     =   0x40,
        INDEXED            =   0x80,
        BOUND_FUNCTION     =  0x100,
        VAROBJ             =  0x200,
        WATCHED            =  0x400,
        ITERATED_SINGLETON =  0x800,
        NEW_TYPE_UNKNOWN   = 0x1000,
        UNCACHEABLE_PROTO  = 0x2000,

        OBJECT_FLAG_MASK   = 0x3ff8
    };

  private:
    Class               *clasp;         /* Class of referring object. */
    HeapPtrObject       parent;         /* Parent of referring object. */
    uint32_t            flags;          /* Vector of above flags. */
    uint32_t            slotSpan_;      /* Object slot span for BaseShapes at
                                         * dictionary last properties. */

    union {
        js::PropertyOp  rawGetter;      /* getter hook for shape */
        JSObject        *getterObj;     /* user-defined callable "get" object or
                                           null if shape->hasGetterValue() */
    };

    union {
        js::StrictPropertyOp rawSetter; /* setter hook for shape */
        JSObject        *setterObj;     /* user-defined callable "set" object or
                                           null if shape->hasSetterValue() */
    };

    /* For owned BaseShapes, the canonical unowned BaseShape. */
    HeapPtr<UnownedBaseShape> unowned_;

    /* For owned BaseShapes, the shape's shape table. */
    ShapeTable       *table_;

    BaseShape(const BaseShape &base) MOZ_DELETE;

  public:
    void finalize(FreeOp *fop);

    inline BaseShape(Class *clasp, JSObject *parent, uint32_t objectFlags);
    inline BaseShape(Class *clasp, JSObject *parent, uint32_t objectFlags,
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

    /* Lookup base shapes from the compartment's baseShapes table. */
    static UnownedBaseShape *getUnowned(JSContext *cx, const StackBaseShape &base);

    /* Get the canonical base shape. */
    inline UnownedBaseShape *unowned();

    /* Get the canonical base shape for an owned one. */
    inline UnownedBaseShape *baseUnowned();

    /* Get the canonical base shape for an unowned one (i.e. identity). */
    inline UnownedBaseShape *toUnowned();

    /* Check that an owned base shape is consistent with its unowned base. */
    inline void assertConsistency();

    /* For JIT usage */
    static inline size_t offsetOfClass() { return offsetof(BaseShape, clasp); }
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

UnownedBaseShape *
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

    StackBaseShape(BaseShape *base)
      : flags(base->flags & BaseShape::OBJECT_FLAG_MASK),
        clasp(base->clasp),
        parent(base->parent),
        rawGetter(NULL),
        rawSetter(NULL)
    {}

    StackBaseShape(Class *clasp, JSObject *parent, uint32_t objectFlags)
      : flags(objectFlags),
        clasp(clasp),
        parent(parent),
        rawGetter(NULL),
        rawSetter(NULL)
    {}

    inline StackBaseShape(Shape *shape);

    inline void updateGetterSetter(uint8_t attrs,
                                   PropertyOp rawGetter,
                                   StrictPropertyOp rawSetter);

    static inline HashNumber hash(const StackBaseShape *lookup);
    static inline bool match(UnownedBaseShape *key, const StackBaseShape *lookup);

    class AutoRooter : private AutoGCRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, const StackBaseShape *base_
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
          : AutoGCRooter(cx, STACKBASESHAPE), base(base_), skip(cx, base_)
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }

        friend void AutoGCRooter::trace(JSTracer *trc);

      private:
        const StackBaseShape *base;
        SkipRoot skip;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
};

typedef HashSet<ReadBarriered<UnownedBaseShape>,
                StackBaseShape,
                SystemAllocPolicy> BaseShapeSet;

struct Shape : public js::gc::Cell
{
    friend struct ::JSObject;
    friend struct ::JSFunction;
    friend class js::Bindings;
    friend class js::ObjectImpl;
    friend class js::PropertyTree;
    friend class js::StaticBlockObject;
    friend struct js::StackShape;
    friend struct js::StackBaseShape;

  protected:
    HeapPtrBaseShape    base_;
    HeapId              propid_;

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

    static inline Shape *searchNoAllocation(Shape *start, jsid id);

    inline void removeFromDictionary(JSObject *obj);
    inline void insertIntoDictionary(HeapPtrShape *dictp);

    inline void initDictionaryShape(const StackShape &child, uint32_t nfixed,
                                    HeapPtrShape *dictp);

    Shape *getChildBinding(JSContext *cx, const StackShape &child);

    /* Replace the base shape of the last shape in a non-dictionary lineage with base. */
    static Shape *replaceLastProperty(JSContext *cx, const StackBaseShape &base,
                                      JSObject *proto, Shape *shape);

    bool hashify(JSContext *cx);
    void handoffTableTo(Shape *newShape);

    inline void setParent(js::Shape *p);

    bool ensureOwnBaseShape(JSContext *cx) {
        if (base()->isOwned())
            return true;
        return makeOwnBaseShape(cx);
    }

    bool makeOwnBaseShape(JSContext *cx);

  public:
    bool hasTable() const { return base()->hasTable(); }
    js::ShapeTable &table() const { return base()->table(); }

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

    const HeapPtrShape &previous() const {
        return parent;
    }

    class Range {
      protected:
        friend struct Shape;
        Shape *cursor;

      public:
        Range(Shape *shape) : cursor(shape) { }

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

        class AutoRooter : private AutoGCRooter
        {
          public:
            explicit AutoRooter(JSContext *cx, Range *r_
                                JS_GUARD_OBJECT_NOTIFIER_PARAM)
              : AutoGCRooter(cx, SHAPERANGE), r(r_), skip(cx, r_)
            {
                JS_GUARD_OBJECT_NOTIFIER_INIT;
            }

            friend void AutoGCRooter::trace(JSTracer *trc);
            void trace(JSTracer *trc);

          private:
            Range *r;
            SkipRoot skip;
            JS_DECL_USE_GUARD_OBJECT_NOTIFIER
        };
    };

    Range all() {
        return Range(this);
    }

    Class *getObjectClass() const { return base()->clasp; }
    JSObject *getObjectParent() const { return base()->parent; }

    static Shape *setObjectParent(JSContext *cx, JSObject *obj, JSObject *proto, Shape *last);
    static Shape *setObjectFlag(JSContext *cx, BaseShape::Flag flag, JSObject *proto, Shape *last);

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
        return base()->getterObj ? js::ObjectValue(*base()->getterObj) : js::UndefinedValue();
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
        return base()->setterObj ? js::ObjectValue(*base()->setterObj) : js::UndefinedValue();
    }

    Value setterOrUndefined() const {
        return (hasSetterValue() && base()->setterObj)
               ? ObjectValue(*base()->setterObj)
               : UndefinedValue();
    }

    void update(js::PropertyOp getter, js::StrictPropertyOp setter, uint8_t attrs);

    inline bool matches(const Shape *other) const;
    inline bool matches(const StackShape &other) const;
    inline bool matchesParamsAfterId(BaseShape *base,
                                     uint32_t aslot, unsigned aattrs, unsigned aflags,
                                     int ashortid) const;

    bool get(JSContext* cx, HandleObject receiver, JSObject *obj, JSObject *pobj, MutableHandleValue vp);
    bool set(JSContext* cx, HandleObject obj, HandleObject receiver, bool strict, MutableHandleValue vp);

    BaseShape *base() const { return base_; }

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

    const HeapId &propid() const {
        JS_ASSERT(!isEmptyShape());
        JS_ASSERT(!JSID_IS_VOID(propid_));
        return propid_;
    }
    HeapId &propidRef() { JS_ASSERT(!JSID_IS_VOID(propid_)); return propid_; }

    int16_t shortid() const { JS_ASSERT(hasShortID()); return maybeShortid(); }
    int16_t maybeShortid() const { return shortid_; }

    /*
     * If SHORTID is set in shape->flags, we use shape->shortid rather
     * than id when calling shape's getter or setter.
     */
    inline bool getUserId(JSContext *cx, jsid *idp) const;

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

    /*
     * Sometimes call objects and run-time block objects need unique shapes, but
     * sometimes they don't.
     *
     * Property cache entries only record the shapes of the first and last
     * objects along the search path, so if the search traverses more than those
     * two objects, then those first and last shapes must determine the shapes
     * of everything else along the path. The js_PurgeScopeChain stuff takes
     * care of making this work, but that suffices only because we require that
     * start points with the same shape have the same successor object in the
     * search path --- a cache hit means the starting shapes were equal, which
     * means the search path tail (everything but the first object in the path)
     * was shared, which in turn means the effects of a purge will be seen by
     * all affected starting search points.
     *
     * For call and run-time block objects, the "successor object" is the scope
     * chain parent. Unlike prototype objects (of which there are usually few),
     * scope chain parents are created frequently (possibly on every call), so
     * following the shape-implies-parent rule blindly would lead one to give
     * every call and block its own shape.
     *
     * In many cases, however, it's not actually necessary to give call and
     * block objects their own shapes, and we can do better. If the code will
     * always be used with the same global object, and none of the enclosing
     * call objects could have bindings added to them at runtime (by direct eval
     * calls or function statements), then we can use a fixed set of shapes for
     * those objects. You could think of the shapes in the functions' bindings
     * and compile-time blocks as uniquely identifying the global object(s) at
     * the end of the scope chain.
     *
     * (In fact, some JSScripts we do use against multiple global objects (see
     * bug 618497), and using the fixed shapes isn't sound there.)
     *
     * In deciding whether a call or block has any extensible parents, we
     * actually only need to consider enclosing calls; blocks are never
     * extensible, and the other sorts of objects that appear in the scope
     * chains ('with' blocks, say) are not CacheableNonGlobalScopes.
     *
     * If the hasExtensibleParents flag is set for the last property in a
     * script's bindings or a compiler-generated Block object, then created
     * Call or Block objects need unique shapes. If the flag is clear, then we
     * can use lastBinding's shape.
     */
    static Shape *setExtensibleParents(JSContext *cx, Shape *shape);
    bool extensibleParents() const { return !!(base()->flags & BaseShape::EXTENSIBLE_PARENTS); }

    uint32_t entryCount() {
        if (hasTable())
            return table().entryCount;

        js::Shape *shape = this;
        uint32_t count = 0;
        for (js::Shape::Range r = shape->all(); !r.empty(); r.popFront())
            ++count;
        return count;
    }

    bool isBigEnoughForAShapeTable() {
        JS_ASSERT(!hasTable());
        js::Shape *shape = this;
        uint32_t count = 0;
        for (js::Shape::Range r = shape->all(); !r.empty(); r.popFront()) {
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

    void finalize(FreeOp *fop);
    void removeChild(js::Shape *child);

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
    class Inner : private AutoGCRooter
    {
      public:
        Inner(JSContext *cx, uint8_t attrs,
              PropertyOp *pgetter_, StrictPropertyOp *psetter_)
            : AutoGCRooter(cx, GETTERSETTER), attrs(attrs),
              pgetter(pgetter_), psetter(psetter_),
              getterRoot(cx, pgetter_), setterRoot(cx, psetter_)
        {
            JS_ASSERT_IF(attrs & JSPROP_GETTER, !IsPoisonedPtr(*pgetter));
            JS_ASSERT_IF(attrs & JSPROP_SETTER, !IsPoisonedPtr(*psetter));
        }

        friend void AutoGCRooter::trace(JSTracer *trc);

      private:
        uint8_t attrs;
        PropertyOp *pgetter;
        StrictPropertyOp *psetter;
        SkipRoot getterRoot, setterRoot;
    };

  public:
    explicit AutoRooterGetterSetter(JSContext *cx, uint8_t attrs,
                                    PropertyOp *pgetter, StrictPropertyOp *psetter
                                    JS_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
            inner.construct(cx, attrs, pgetter, psetter);
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    Maybe<Inner> inner;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct EmptyShape : public js::Shape
{
    EmptyShape(UnownedBaseShape *base, uint32_t nfixed);

    /*
     * Lookup an initial shape matching the given parameters, creating an empty
     * shape if none was found.
     */
    static Shape *getInitialShape(JSContext *cx, Class *clasp, JSObject *proto,
                                  JSObject *parent, gc::AllocKind kind, uint32_t objectFlags = 0);

    /*
     * Reinsert an alternate initial shape, to be returned by future
     * getInitialShape calls, until the new shape becomes unreachable in a GC
     * and the table entry is purged.
     */
    static void insertInitialShape(JSContext *cx, Shape *shape, JSObject *proto);
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
    JSObject *proto;

    /* State used to determine a match on an initial shape. */
    struct Lookup {
        Class *clasp;
        JSObject *proto;
        JSObject *parent;
        uint32_t nfixed;
        uint32_t baseFlags;
        Lookup(Class *clasp, JSObject *proto, JSObject *parent, uint32_t nfixed,
               uint32_t baseFlags)
            : clasp(clasp), proto(proto), parent(parent),
              nfixed(nfixed), baseFlags(baseFlags)
        {}
    };

    inline InitialShapeEntry();
    inline InitialShapeEntry(const ReadBarriered<Shape> &shape, JSObject *proto);

    inline Lookup getLookup();

    static inline HashNumber hash(const Lookup &lookup);
    static inline bool match(const InitialShapeEntry &key, const Lookup &lookup);
};

typedef HashSet<InitialShapeEntry, InitialShapeEntry, SystemAllocPolicy> InitialShapeSet;

struct StackShape
{
    UnownedBaseShape *base;
    jsid             propid;
    uint32_t         slot_;
    uint8_t          attrs;
    uint8_t          flags;
    int16_t          shortid;

    StackShape(UnownedBaseShape *base, jsid propid, uint32_t slot,
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

    StackShape(const Shape *shape)
      : base(shape->base()->unowned()),
        propid(const_cast<Shape *>(shape)->propidRef()),
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

    class AutoRooter : private AutoGCRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, const StackShape *shape_
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
          : AutoGCRooter(cx, STACKSHAPE), shape(shape_), skip(cx, shape_)
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }

        friend void AutoGCRooter::trace(JSTracer *trc);

      private:
        const StackShape *shape;
        SkipRoot skip;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
 };

} /* namespace js */

/* js::Shape pointer tag bit indicating a collision. */
#define SHAPE_COLLISION                 (uintptr_t(1))
#define SHAPE_REMOVED                   ((js::Shape *) SHAPE_COLLISION)

/* Macros to get and set shape pointer values and collision flags. */
#define SHAPE_IS_FREE(shape)            ((shape) == NULL)
#define SHAPE_IS_REMOVED(shape)         ((shape) == SHAPE_REMOVED)
#define SHAPE_IS_LIVE(shape)            ((shape) > SHAPE_REMOVED)
#define SHAPE_FLAG_COLLISION(spp,shape) (*(spp) = (js::Shape *)               \
                                         (uintptr_t(shape) | SHAPE_COLLISION))
#define SHAPE_HAD_COLLISION(shape)      (uintptr_t(shape) & SHAPE_COLLISION)
#define SHAPE_FETCH(spp)                SHAPE_CLEAR_COLLISION(*(spp))

#define SHAPE_CLEAR_COLLISION(shape)                                          \
    ((js::Shape *) (uintptr_t(shape) & ~SHAPE_COLLISION))

#define SHAPE_STORE_PRESERVING_COLLISION(spp, shape)                          \
    (*(spp) = (js::Shape *) (uintptr_t(shape) | SHAPE_HAD_COLLISION(*(spp))))

namespace js {

inline Shape *
Shape::search(JSContext *cx, Shape *start, jsid id, Shape ***pspp, bool adding)
{
#ifdef DEBUG
    {
        SkipRoot skip0(cx, &start);
        SkipRoot skip1(cx, &id);
        MaybeCheckStackRoots(cx);
    }
#endif

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
            RootedShape startRoot(cx, start);
            RootedId idRoot(cx, id);
            if (startRoot->hashify(cx)) {
                Shape **spp = startRoot->table().search(idRoot, adding);
                return SHAPE_FETCH(spp);
            }
            start = startRoot;
            id = idRoot;
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

/* static */ inline Shape *
Shape::searchNoAllocation(Shape *start, jsid id)
{
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

void
MarkNonNativePropertyFound(HandleObject obj, MutableHandleShape propp);

} // namespace js

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

namespace JS {
    template<> class AnchorPermitted<js::Shape *> { };
    template<> class AnchorPermitted<const js::Shape *> { };

    template<> struct RootKind<js::Shape *> { static ThingRootKind rootKind() { return THING_ROOT_SHAPE; }; };
    template<> struct RootKind<js::BaseShape *> { static ThingRootKind rootKind() { return THING_ROOT_BASE_SHAPE; }; };
}

#endif /* jsscope_h___ */
