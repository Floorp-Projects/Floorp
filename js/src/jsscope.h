/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsscope_h___
#define jsscope_h___
/*
 * JS symbol tables.
 */
#include <new>
#ifdef DEBUG
#include <stdio.h>
#endif

#include "jstypes.h"

#include "jscntxt.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jspropertytree.h"

#include "js/HashTable.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

/*
 * Given P independent, non-unique properties each of size S words mapped by
 * all scopes in a runtime, construct a property tree of N nodes each of size
 * S+L words (L for tree linkage).  A nominal L value is 2 for leftmost-child
 * and right-sibling links.  We hope that the N < P by enough that the space
 * overhead of L, and the overhead of scope entries pointing at property tree
 * nodes, is worth it.
 *
 * The tree construction goes as follows.  If any empty scope in the runtime
 * has a property X added to it, find or create a node under the tree root
 * labeled X, and set obj->lastProp to point at that node.  If any non-empty
 * scope whose most recently added property is labeled Y has another property
 * labeled Z added, find or create a node for Z under the node that was added
 * for Y, and set obj->lastProp to point at that node.
 *
 * A property is labeled by its members' values: id, getter, setter, slot,
 * attributes, tiny or short id, and a field telling for..in order.  Note that
 * labels are not unique in the tree, but they are unique among a node's kids
 * (barring rare and benign multi-threaded race condition outcomes, see below)
 * and along any ancestor line from the tree root to a given leaf node (except
 * for the hard case of duplicate formal parameters to a function).
 *
 * Thus the root of the tree represents all empty scopes, and the first ply
 * of the tree represents all scopes containing one property, etc.  Each node
 * in the tree can stand for any number of scopes having the same ordered set
 * of properties, where that node was the last added to the scope.  (We need
 * not store the root of the tree as a node, and do not -- all we need are
 * links to its kids.)
 *
 * Sidebar on for..in loop order: ECMA requires no particular order, but this
 * implementation has promised and delivered property definition order, and
 * compatibility is king.  We could use an order number per property, which
 * would require a sort in js_Enumerate, and an entry order generation number
 * per scope.  An order number beats a list, which should be doubly-linked for
 * O(1) delete.  An even better scheme is to use a parent link in the property
 * tree, so that the ancestor line can be iterated from obj->lastProp when
 * filling in a JSIdArray from back to front.  This parent link also helps the
 * GC to sweep properties iteratively.
 *
 * What if a property Y is deleted from a scope?  If Y is the last property in
 * the scope, we simply adjust the scope's lastProp member after we remove the
 * scope's hash-table entry pointing at that property node.  The parent link
 * mentioned in the for..in sidebar above makes this adjustment O(1).  But if
 * Y comes between X and Z in the scope, then we might have to "fork" the tree
 * at X, leaving X->Y->Z in case other scopes have those properties added in
 * that order; and to finish the fork, we'd add a node labeled Z with the path
 * X->Z, if it doesn't exist.  This could lead to lots of extra nodes, and to
 * O(n^2) growth when deleting lots of properties.
 *
 * Rather, for O(1) growth all around, we should share the path X->Y->Z among
 * scopes having those three properties added in that order, and among scopes
 * having only X->Z where Y was deleted.  All such scopes have a lastProp that
 * points to the Z child of Y.  But a scope in which Y was deleted does not
 * have a table entry for Y, and when iterating that scope by traversing the
 * ancestor line from Z, we will have to test for a table entry for each node,
 * skipping nodes that lack entries.
 *
 * What if we add Y again?  X->Y->Z->Y is wrong and we'll enumerate Y twice.
 * Therefore we must fork in such a case if not earlier, or do something else.
 * We used to fork on the theory that set after delete is rare, but the Web is
 * a harsh mistress, and we now convert the scope to a "dictionary" on first
 * delete, to avoid O(n^2) growth in the property tree.
 *
 * Is the property tree worth it compared to property storage in each table's
 * entries?  To decide, we must find the relation <> between the words used
 * with a property tree and the words required without a tree.
 *
 * Model all scopes as one super-scope of capacity T entries (T a power of 2).
 * Let alpha be the load factor of this double hash-table.  With the property
 * tree, each entry in the table is a word-sized pointer to a node that can be
 * shared by many scopes.  But all such pointers are overhead compared to the
 * situation without the property tree, where the table stores property nodes
 * directly, as entries each of size S words.  With the property tree, we need
 * L=2 extra words per node for siblings and kids pointers.  Without the tree,
 * (1-alpha)*S*T words are wasted on free or removed sentinel-entries required
 * by double hashing.
 *
 * Therefore,
 *
 *      (property tree)                 <> (no property tree)
 *      N*(S+L) + T                     <> S*T
 *      N*(S+L) + T                     <> P*S + (1-alpha)*S*T
 *      N*(S+L) + alpha*T + (1-alpha)*T <> P*S + (1-alpha)*S*T
 *
 * Note that P is alpha*T by definition, so
 *
 *      N*(S+L) + P + (1-alpha)*T <> P*S + (1-alpha)*S*T
 *      N*(S+L)                   <> P*S - P + (1-alpha)*S*T - (1-alpha)*T
 *      N*(S+L)                   <> (P + (1-alpha)*T) * (S-1)
 *      N*(S+L)                   <> (P + (1-alpha)*P/alpha) * (S-1)
 *      N*(S+L)                   <> P * (1/alpha) * (S-1)
 *
 * Let N = P*beta for a compression ratio beta, beta <= 1:
 *
 *      P*beta*(S+L) <> P * (1/alpha) * (S-1)
 *      beta*(S+L)   <> (S-1)/alpha
 *      beta         <> (S-1)/((S+L)*alpha)
 *
 * For S = 6 (32-bit architectures) and L = 2, the property tree wins iff
 *
 *      beta < 5/(8*alpha)
 *
 * We ensure that alpha <= .75, so the property tree wins if beta < .83_.  An
 * average beta from recent Mozilla browser startups was around .6.
 *
 * Can we reduce L?  Observe that the property tree degenerates into a list of
 * lists if at most one property Y follows X in all scopes.  In or near such a
 * case, we waste a word on the right-sibling link outside of the root ply of
 * the tree.  Note also that the root ply tends to be large, so O(n^2) growth
 * searching it is likely, indicating the need for hashing.
 *
 * If only K out of N nodes in the property tree have more than one child, we
 * could eliminate the sibling link and overlay a children list or hash-table
 * pointer on the leftmost-child link (which would then be either null or an
 * only-child link; the overlay could be tagged in the low bit of the pointer,
 * or flagged elsewhere in the property tree node, although such a flag must
 * not be considered when comparing node labels during tree search).
 *
 * For such a system, L = 1 + (K * averageChildrenTableSize) / N instead of 2.
 * If K << N, L approaches 1 and the property tree wins if beta < .95.
 *
 * We observe that fan-out below the root ply of the property tree appears to
 * have extremely low degree (see the MeterPropertyTree code that histograms
 * child-counts in jsscope.c), so instead of a hash-table we use a linked list
 * of child node pointer arrays ("kid chunks").  The details are isolated in
 * jspropertytree.h/.cpp; others must treat js::Shape.kids as opaque.
 *
 * One final twist (can you stand it?): the vast majority (~95% or more) of
 * scopes are looked up fewer than three times;  in these cases, initializing
 * scope->table isn't worth it.  So instead of always allocating scope->table,
 * we leave it null while initializing all the other scope members as if it
 * were non-null and minimal-length.  Until a scope is searched
 * LINEAR_SEARCHES_MAX times, we use linear search from obj->lastProp to find a
 * given id, and save on the time and space overhead of creating a hash table.
 * Also, we don't create tables for property tree Shapes that have shape
 * lineages smaller than MIN_ENTRIES.
 */

namespace js {

/* Limit on the number of slotful properties in an object. */
static const uint32 SHAPE_INVALID_SLOT = JS_BIT(24) - 1;
static const uint32 SHAPE_MAXIMUM_SLOT = JS_BIT(24) - 2;

/*
 * Shapes use multiplicative hashing, _a la_ jsdhash.[ch], but specialized to
 * minimize footprint.
 */
struct PropertyTable {
    static const uint32 MIN_ENTRIES         = 7;
    static const uint32 MIN_SIZE_LOG2       = 4;
    static const uint32 MIN_SIZE            = JS_BIT(MIN_SIZE_LOG2);

    int             hashShift;          /* multiplicative hash shift */

    uint32          entryCount;         /* number of entries in table */
    uint32          removedCount;       /* removed entry sentinels in table */
    uint32          freelist;           /* SHAPE_INVALID_SLOT or head of slot
                                           freelist in owning dictionary-mode
                                           object */
    js::Shape       **entries;          /* table of ptrs to shared tree nodes */

    PropertyTable(uint32 nentries)
      : hashShift(JS_DHASH_BITS - MIN_SIZE_LOG2),
        entryCount(nentries),
        removedCount(0),
        freelist(SHAPE_INVALID_SLOT)
    {
        /* NB: entries is set by init, which must be called. */
    }

    ~PropertyTable() {
        js::UnwantedForeground::free_(entries);
    }

    /* By definition, hashShift = JS_DHASH_BITS - log2(capacity). */
    uint32 capacity() const { return JS_BIT(JS_DHASH_BITS - hashShift); }

    /* Computes the size of the entries array for a given capacity. */
    static size_t sizeOfEntries(size_t cap) { return cap * sizeof(Shape *); }

    /*
     * This counts the PropertyTable object itself (which must be
     * heap-allocated) and its |entries| array.
     */
    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const {
        return mallocSizeOf(this, sizeof(PropertyTable)) +
               mallocSizeOf(entries, sizeOfEntries(capacity()));
    }

    /* Whether we need to grow.  We want to do this if the load factor is >= 0.75 */
    bool needsToGrow() const {
        uint32 size = capacity();
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
 * Owned BaseShapes are used for shapes which have property tables, including
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
 *     Property in the property tree which has a property table.
 *
 * Unowned Shape, Unowned BaseShape:
 *
 *     Property in the property tree which does not have a property table.
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
    friend struct BaseShapeEntry;

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
    uint32              flags;          /* Vector of above flags. */
    uint32              slotSpan_;      /* Object slot span for BaseShapes at
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

    /* For owned BaseShapes, the shape's property table. */
    PropertyTable       *table_;

  public:
    void finalize(JSContext *cx, bool background);

    inline BaseShape(Class *clasp, JSObject *parent, uint32 objectFlags);
    inline BaseShape(Class *clasp, JSObject *parent, uint32 objectFlags,
                     uint8 attrs, PropertyOp rawGetter, StrictPropertyOp rawSetter);

    bool isOwned() const { return !!(flags & OWNED_SHAPE); }

    inline bool matchesGetterSetter(PropertyOp rawGetter,
                                    StrictPropertyOp rawSetter) const;

    inline void adoptUnowned(UnownedBaseShape *other);
    inline void setOwned(UnownedBaseShape *unowned);

    inline void setParent(JSObject *obj);
    JSObject *getObjectParent() { return parent; }

    void setObjectFlag(Flag flag) { JS_ASSERT(!(flag & ~OBJECT_FLAG_MASK)); flags |= flag; }

    bool hasGetterObject() const { return !!(flags & HAS_GETTER_OBJECT); }
    JSObject *getterObject() const { JS_ASSERT(hasGetterObject()); return getterObj; }

    bool hasSetterObject() const { return !!(flags & HAS_SETTER_OBJECT); }
    JSObject *setterObject() const { JS_ASSERT(hasSetterObject()); return setterObj; }

    bool hasTable() const { JS_ASSERT_IF(table_, isOwned()); return table_ != NULL; }
    PropertyTable &table() const { JS_ASSERT(table_ && isOwned()); return *table_; }
    void setTable(PropertyTable *table) { JS_ASSERT(isOwned()); table_ = table; }

    uint32 slotSpan() const { JS_ASSERT(isOwned()); return slotSpan_; }
    void setSlotSpan(uint32 slotSpan) { JS_ASSERT(isOwned()); slotSpan_ = slotSpan; }

    /* Lookup base shapes from the compartment's baseShapes table. */
    static UnownedBaseShape *getUnowned(JSContext *cx, const BaseShape &base);

    /* Get the canonical base shape. */
    inline UnownedBaseShape *unowned();

    /* Get the canonical base shape for an owned one. */
    inline UnownedBaseShape *baseUnowned();

    /* Get the canonical base shape for an unowned one (i.e. identity). */
    inline UnownedBaseShape *toUnowned();

    /* For JIT usage */
    static inline size_t offsetOfClass() { return offsetof(BaseShape, clasp); }
    static inline size_t offsetOfParent() { return offsetof(BaseShape, parent); }
    static inline size_t offsetOfFlags() { return offsetof(BaseShape, flags); }

    static inline void writeBarrierPre(BaseShape *shape);
    static inline void writeBarrierPost(BaseShape *shape, void *addr);
    static inline void readBarrier(BaseShape *shape);

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
struct BaseShapeEntry
{
    typedef const BaseShape *Lookup;

    static inline HashNumber hash(const BaseShape *base);
    static inline bool match(UnownedBaseShape *key, const BaseShape *lookup);
};
typedef HashSet<UnownedBaseShape *, BaseShapeEntry, SystemAllocPolicy> BaseShapeSet;

struct Shape : public js::gc::Cell
{
    friend struct ::JSObject;
    friend struct ::JSFunction;
    friend class js::PropertyTree;
    friend class js::Bindings;
    friend bool IsShapeAboutToBeFinalized(JSContext *cx, const js::Shape *shape);

  protected:
    HeapPtrBaseShape    base_;
    HeapId              propid_;

    JS_ENUM_HEADER(SlotInfo, uint32)
    {
        /* Number of fixed slots in objects with this shape. */
        FIXED_SLOTS_MAX        = 0x1f,
        FIXED_SLOTS_SHIFT      = 27,
        FIXED_SLOTS_MASK       = FIXED_SLOTS_MAX << FIXED_SLOTS_SHIFT,

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

    uint32              slotInfo;       /* mask of above info */
    uint8               attrs;          /* attributes, see jsapi.h JSPROP_* */
    uint8               flags;          /* flags, see below for defines */
    int16               shortid_;       /* tinyid, or local arg/var index */

    HeapPtrShape        parent;        /* parent node, reverse for..in order */
    /* kids is valid when !inDictionary(), listp is valid when inDictionary(). */
    union {
        KidsPointer kids;       /* null, single child, or a tagged ptr
                                   to many-kids data structure */
        HeapPtrShape *listp;    /* dictionary list starting at lastProp
                                   has a double-indirect back pointer,
                                   either to shape->parent if not last,
                                   else to obj->lastProp */
    };

    static inline Shape **search(JSContext *cx, HeapPtrShape *pstart, jsid id,
                                 bool adding = false);
    static js::Shape *newDictionaryList(JSContext *cx, HeapPtrShape *listp);

    inline void removeFromDictionary(JSObject *obj);
    inline void insertIntoDictionary(HeapPtrShape *dictp);

    inline void initDictionaryShape(const Shape &child, HeapPtrShape *dictp);

    Shape *getChildBinding(JSContext *cx, const Shape &child, HeapPtrShape *lastBinding);

    /* Replace the base shape of the last shape in a non-dictionary lineage with base. */
    static bool replaceLastProperty(JSContext *cx, const BaseShape &base, JSObject *proto, HeapPtrShape *lastp);

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
    js::PropertyTable &table() const { return base()->table(); }

    size_t sizeOfPropertyTable(JSMallocSizeOfFun mallocSizeOf) const {
        return hasTable() ? table().sizeOfIncludingThis(mallocSizeOf) : 0;
    }

    size_t sizeOfKids(JSMallocSizeOfFun mallocSizeOf) const {
        JS_ASSERT(!inDictionary());
        return kids.isHash()
             ? kids.toHash()->sizeOfIncludingThis(mallocSizeOf)
             : 0;
    }

    bool isNative() const {
        JS_ASSERT(!(flags & NON_NATIVE) == getObjectClass()->isNative());
        return !(flags & NON_NATIVE);
    }

    const js::Shape *previous() const {
        return parent;
    }

    class Range {
      protected:
        friend struct Shape;

        const Shape *cursor;
        const Shape *end;

      public:
        Range(const Shape *shape) : cursor(shape) { }

        bool empty() const {
            return cursor->isEmptyShape();
        }

        const Shape &front() const {
            JS_ASSERT(!empty());
            return *cursor;
        }

        void popFront() {
            JS_ASSERT(!empty());
            cursor = cursor->parent;
        }
    };

    Range all() const {
        return Range(this);
    }

    Class *getObjectClass() const { return base()->clasp; }
    JSObject *getObjectParent() const { return base()->parent; }

    static bool setObjectParent(JSContext *cx, JSObject *obj, JSObject *proto, HeapPtrShape *listp);
    static bool setObjectFlag(JSContext *cx, BaseShape::Flag flag, JSObject *proto, HeapPtrShape *listp);

    uint32 getObjectFlags() const { return base()->flags & BaseShape::OBJECT_FLAG_MASK; }
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

    Shape(BaseShape *base, jsid id, uint32 slot, uint32 nfixed, uintN attrs, uintN flags, intN shortid);

    /* Get a shape identical to this one, without parent/kids information. */
    Shape(const Shape *other);

    /* Used by EmptyShape (see jsscopeinlines.h). */
    Shape(BaseShape *base, uint32 nfixed);

    /* Copy constructor disabled, to avoid misuse of the above form. */
    Shape(const Shape &other);

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
        METHOD          = 0x80,
        PUBLIC_FLAGS    = HAS_SHORTID | METHOD
    };

    bool inDictionary() const   { return (flags & IN_DICTIONARY) != 0; }
    uintN getFlags() const  { return flags & PUBLIC_FLAGS; }
    bool hasShortID() const { return (flags & HAS_SHORTID) != 0; }

    /*
     * A shape has a method barrier when some compiler-created "null closure"
     * function objects (functions that do not use lexical bindings above their
     * scope, only free variable names) that have a correct JSSLOT_PARENT value
     * thanks to the COMPILE_N_GO optimization are stored in objects without
     * cloning.
     *
     * The de-facto standard JS language requires each evaluation of such a
     * closure to result in a unique (according to === and observable effects)
     * function object. When storing a function to a property, we use method
     * shapes to speculate that these effects will never be observed: the
     * property will only be used in calls, and f.callee will not be used
     * to get a handle on the object.
     *
     * If either a non-call use or callee access occurs, then the function is
     * cloned and the object is reshaped with a non-method property.
     *
     * Note that method shapes do not imply the object has a particular
     * uncloned function, just that the object has *some* uncloned function
     * in the shape's slot.
     */
    bool isMethod() const {
        JS_ASSERT_IF(flags & METHOD, !base()->rawGetter);
        return (flags & METHOD) != 0;
    }

    PropertyOp getter() const { return base()->rawGetter; }
    bool hasDefaultGetterOrIsMethod() const { return !base()->rawGetter; }
    bool hasDefaultGetter() const  { return !base()->rawGetter && !isMethod(); }
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

    void update(js::PropertyOp getter, js::StrictPropertyOp setter, uint8 attrs);

    inline JSDHashNumber hash() const;
    inline bool matches(const js::Shape *p) const;
    inline bool matchesParamsAfterId(BaseShape *base,
                                     uint32 aslot, uintN aattrs, uintN aflags,
                                     intN ashortid) const;

    bool get(JSContext* cx, JSObject *receiver, JSObject *obj, JSObject *pobj, js::Value* vp) const;
    bool set(JSContext* cx, JSObject *obj, bool strict, js::Value* vp) const;

    BaseShape *base() const { return base_; }

    bool hasSlot() const { return (attrs & JSPROP_SHARED) == 0; }
    uint32 slot() const { JS_ASSERT(hasSlot() && !hasMissingSlot()); return maybeSlot(); }
    uint32 maybeSlot() const { return slotInfo & SLOT_MASK; }

    bool isEmptyShape() const {
        JS_ASSERT_IF(JSID_IS_EMPTY(propid_), hasMissingSlot());
        return JSID_IS_EMPTY(propid_);
    }

    uint32 slotSpan() const {
        JS_ASSERT(!inDictionary());
        uint32 free = JSSLOT_FREE(getObjectClass());
        return hasMissingSlot() ? free : Max(free, maybeSlot() + 1);
    }

    void setSlot(uint32 slot) {
        JS_ASSERT(slot <= SHAPE_INVALID_SLOT);
        slotInfo = slotInfo & ~SLOT_MASK;
        slotInfo = slotInfo | slot;
    }

    uint32 numFixedSlots() const {
        return (slotInfo >> FIXED_SLOTS_SHIFT);
    }

    void setNumFixedSlots(uint32 nfixed) {
        JS_ASSERT(nfixed < FIXED_SLOTS_MAX);
        slotInfo = slotInfo & ~FIXED_SLOTS_MASK;
        slotInfo = slotInfo | (nfixed << FIXED_SLOTS_SHIFT);
    }

    uint32 numLinearSearches() const {
        return (slotInfo & LINEAR_SEARCHES_MASK) >> LINEAR_SEARCHES_SHIFT;
    }

    void incrementNumLinearSearches() {
        uint32 count = numLinearSearches();
        JS_ASSERT(count < LINEAR_SEARCHES_MAX);
        slotInfo = slotInfo & ~LINEAR_SEARCHES_MASK;
        slotInfo = slotInfo | ((count + 1) << LINEAR_SEARCHES_SHIFT);
    }

    jsid propid() const { JS_ASSERT(!isEmptyShape()); return maybePropid(); }
    jsid maybePropid() const { JS_ASSERT(!JSID_IS_VOID(propid_)); return propid_; }

    int16 shortid() const { JS_ASSERT(hasShortID()); return maybeShortid(); }
    int16 maybeShortid() const { return shortid_; }

    /*
     * If SHORTID is set in shape->flags, we use shape->shortid rather
     * than id when calling shape's getter or setter.
     */
    jsid getUserId() const {
        return hasShortID() ? INT_TO_JSID(shortid()) : propid();
    }

    uint8 attributes() const { return attrs; }
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
     * means the seach path tail (everything but the first object in the path)
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
    static bool setExtensibleParents(JSContext *cx, HeapPtrShape *listp);
    bool extensibleParents() const { return !!(base()->flags & BaseShape::EXTENSIBLE_PARENTS); }

    uint32 entryCount() const {
        if (hasTable())
            return table().entryCount;

        const js::Shape *shape = this;
        uint32 count = 0;
        for (js::Shape::Range r = shape->all(); !r.empty(); r.popFront())
            ++count;
        return count;
    }

    bool isBigEnoughForAPropertyTable() const {
        JS_ASSERT(!hasTable());
        const js::Shape *shape = this;
        uint32 count = 0;
        for (js::Shape::Range r = shape->all(); !r.empty(); r.popFront()) {
            ++count;
            if (count >= PropertyTable::MIN_ENTRIES)
                return true;
        }
        return false;
    }

#ifdef DEBUG
    void dump(JSContext *cx, FILE *fp) const;
    void dumpSubtree(JSContext *cx, int level, FILE *fp) const;
#endif

    void finalize(JSContext *cx, bool background);
    void removeChild(js::Shape *child);

    static inline void writeBarrierPre(const Shape *shape);
    static inline void writeBarrierPost(const Shape *shape, void *addr);

    /*
     * All weak references need a read barrier for incremental GC. This getter
     * method implements the read barrier. It's used to obtain initial shapes
     * from the compartment.
     */
    static inline void readBarrier(const Shape *shape);

    /* For JIT usage */
    static inline size_t offsetOfBase() { return offsetof(Shape, base_); }

  private:
    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(Shape, base_) == offsetof(js::shadow::Shape, base));
        JS_STATIC_ASSERT(offsetof(Shape, slotInfo) == offsetof(js::shadow::Shape, slotInfo));
        JS_STATIC_ASSERT(FIXED_SLOTS_SHIFT == js::shadow::Shape::FIXED_SLOTS_SHIFT);
    }
};

struct EmptyShape : public js::Shape
{
    EmptyShape(BaseShape *base, uint32 nfixed);

    /*
     * Lookup an initial shape matching the given parameters, creating an empty
     * shape if none was found.
     */
    static Shape *getInitialShape(JSContext *cx, Class *clasp, JSObject *proto,
                                  JSObject *parent, gc::AllocKind kind, uint32 objectFlags = 0);

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
    js::Shape *shape;

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
        uint32 nfixed;
        uint32 baseFlags;
        Lookup(Class *clasp, JSObject *proto, JSObject *parent, uint32 nfixed, uint32 baseFlags)
            : clasp(clasp), proto(proto), parent(parent),
              nfixed(nfixed), baseFlags(baseFlags)
        {}
    };

    static inline HashNumber hash(const Lookup &lookup);
    static inline bool match(const InitialShapeEntry &key, const Lookup &lookup);
};
typedef HashSet<InitialShapeEntry, InitialShapeEntry, SystemAllocPolicy> InitialShapeSet;

} /* namespace js */

/* js::Shape pointer tag bit indicating a collision. */
#define SHAPE_COLLISION                 (jsuword(1))
#define SHAPE_REMOVED                   ((js::Shape *) SHAPE_COLLISION)

/* Macros to get and set shape pointer values and collision flags. */
#define SHAPE_IS_FREE(shape)            ((shape) == NULL)
#define SHAPE_IS_REMOVED(shape)         ((shape) == SHAPE_REMOVED)
#define SHAPE_IS_LIVE(shape)            ((shape) > SHAPE_REMOVED)
#define SHAPE_FLAG_COLLISION(spp,shape) (*(spp) = (js::Shape *)               \
                                         (jsuword(shape) | SHAPE_COLLISION))
#define SHAPE_HAD_COLLISION(shape)      (jsuword(shape) & SHAPE_COLLISION)
#define SHAPE_FETCH(spp)                SHAPE_CLEAR_COLLISION(*(spp))

#define SHAPE_CLEAR_COLLISION(shape)                                          \
    ((js::Shape *) (jsuword(shape) & ~SHAPE_COLLISION))

#define SHAPE_STORE_PRESERVING_COLLISION(spp, shape)                          \
    (*(spp) = (js::Shape *) (jsuword(shape) | SHAPE_HAD_COLLISION(*(spp))))

namespace js {

/*
 * The search succeeds if it finds a Shape with the given id.  There are two
 * success cases:
 * - If the Shape is the last in its shape lineage, we return |startp|, which
 *   is &obj->lastProp or something similar.
 * - Otherwise, we return &shape->parent, where |shape| is the successor to the
 *   found Shape.
 *
 * There is one failure case:  we return &emptyShape->parent, where
 * |emptyShape| is the EmptyShape at the start of the shape lineage.
 */
JS_ALWAYS_INLINE Shape **
Shape::search(JSContext *cx, HeapPtrShape *pstart, jsid id, bool adding)
{
    Shape *start = *pstart;
    if (start->hasTable())
        return start->table().search(id, adding);

    if (start->numLinearSearches() == LINEAR_SEARCHES_MAX) {
        if (start->isBigEnoughForAPropertyTable() && start->hashify(cx))
            return start->table().search(id, adding);
        /* 
         * No table built -- there weren't enough entries, or OOM occurred.
         * Don't increment numLinearSearches, to keep hasTable() false.
         */
        JS_ASSERT(!start->hasTable());
    } else {
        start->incrementNumLinearSearches();
    }

    /*
     * Not enough searches done so far to justify hashing: search linearly
     * from start.
     *
     * We don't use a Range here, or stop at null parent (the empty shape at
     * the end).  This avoids an extra load per iteration at the cost (if the
     * search fails) of an extra load and id test at the end.
     */
    HeapPtrShape *spp;
    for (spp = pstart; js::Shape *shape = *spp; spp = &shape->parent) {
        if (shape->maybePropid() == id)
            return spp->unsafeGet();
    }
    return spp->unsafeGet();
}

} // namespace js

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

inline js::Class *
JSObject::getClass() const
{
    return lastProperty()->getObjectClass();
}

inline JSClass *
JSObject::getJSClass() const
{
    return Jsvalify(getClass());
}

inline bool
JSObject::hasClass(const js::Class *c) const
{
    return getClass() == c;
}

inline const js::ObjectOps *
JSObject::getOps() const
{
    return &getClass()->ops;
}

namespace JS {
    template<> class AnchorPermitted<js::Shape *> { };
    template<> class AnchorPermitted<const js::Shape *> { };
}

#endif /* jsscope_h___ */
