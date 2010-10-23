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
#include "jshashtable.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jspropertytree.h"

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
 * What about thread safety?  If the property tree operations done by requests
 * are find-node and insert-node, then the only hazard is duplicate insertion.
 * This is harmless except for minor bloat.  When all requests have ended or
 * been suspended, the GC is free to sweep the tree after marking all nodes
 * reachable from scopes, performing remove-node operations as needed.
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
 * searching it is likely, indicating the need for hashing (but with increased
 * thread safety costs).
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
 * One final twist (can you stand it?): the mean number of entries per scope
 * in Mozilla is < 5, with a large standard deviation (~8).  Instead of always
 * allocating scope->table, we leave it null while initializing all the other
 * scope members as if it were non-null and minimal-length.  Until a property
 * is added that crosses the threshold of 6 or more entries for hashing, we use
 * linear search from obj->lastProp to find a given id, and save on the space
 * overhead of a hash table.
 */

#define SHAPE_INVALID_SLOT              0xffffffff

namespace js {

/*
 * Shapes use multiplicative hashing, _a la_ jsdhash.[ch], but specialized to
 * minimize footprint.  But if a Shape lineage has fewer than HASH_THRESHOLD
 * entries, we use linear search and avoid allocating scope->table.
 */
struct PropertyTable {
    enum {
        HASH_THRESHOLD  = 6,
        MIN_SIZE_LOG2   = 4,
        MIN_SIZE        = JS_BIT(MIN_SIZE_LOG2)
    };

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
        js_free(entries);
    }

    /* By definition, hashShift = JS_DHASH_BITS - log2(capacity). */
    uint32 capacity() const { return JS_BIT(JS_DHASH_BITS - hashShift); }

    /*
     * NB: init and change are fallible but do not report OOM, so callers can
     * cope or ignore. They do update the malloc counter on success.
     */
    bool            init(JSContext *cx, js::Shape *lastProp);
    bool            change(JSContext *cx, int change);
    js::Shape       **search(jsid id, bool adding);
};

} /* namespace js */

struct JSObject;

namespace js {

class PropertyTree;

static inline PropertyOp
CastAsPropertyOp(js::Class *clasp)
{
    return JS_DATA_TO_FUNC_PTR(PropertyOp, clasp);
}

struct Shape : public JSObjectMap
{
    friend struct ::JSObject;
    friend struct ::JSFunction;
    friend class js::PropertyTree;
    friend bool HasUnreachableGCThings(TreeFragment *f);

  protected:
    mutable js::PropertyTable *table;

  public:
    inline void freeTable(JSContext *cx);

    static bool initRuntimeState(JSContext *cx);
    static void finishRuntimeState(JSContext *cx);

    enum {
        EMPTY_ARGUMENTS_SHAPE   = 1,
        EMPTY_BLOCK_SHAPE       = 2,
        EMPTY_CALL_SHAPE        = 3,
        EMPTY_DECL_ENV_SHAPE    = 4,
        EMPTY_ENUMERATOR_SHAPE  = 5,
        EMPTY_WITH_SHAPE        = 6,
        LAST_RESERVED_SHAPE     = 6
    };

    jsid                id;

  protected:
    union {
        js::PropertyOp  rawGetter;      /* getter and setter hooks or objects */
        JSObject        *getterObj;     /* user-defined callable "get" object or
                                           null if shape->hasGetterValue(); or
                                           joined function object if METHOD flag
                                           is set. */
        js::Class       *clasp;         /* prototype class for empty scope */
    };

    union {
        js::PropertyOp  rawSetter;      /* getter is JSObject* and setter is 0
                                           if shape->isMethod() */
        JSObject        *setterObj;     /* user-defined callable "set" object or
                                           null if shape->hasSetterValue() */
    };

  public:
    uint32              slot;           /* abstract index in object slots */
  private:
    uint8               attrs;          /* attributes, see jsapi.h JSPROP_* */
    mutable uint8       flags;          /* flags, see below for defines */
  public:
    int16               shortid;        /* tinyid, or local arg/var index */

  protected:
    mutable js::Shape   *parent;        /* parent node, reverse for..in order */
    union {
        mutable js::KidsPointer kids;   /* null, single child, or a tagged ptr
                                           to many-kids data structure */
        mutable js::Shape **listp;      /* dictionary list starting at lastProp
                                           has a double-indirect back pointer,
                                           either to shape->parent if not last,
                                           else to obj->lastProp */
    };

    static inline js::Shape **search(js::Shape **startp, jsid id, bool adding = false);
    static js::Shape *newDictionaryShape(JSContext *cx, const js::Shape &child, js::Shape **listp);
    static js::Shape *newDictionaryList(JSContext *cx, js::Shape **listp);

    inline void removeFromDictionary(JSObject *obj) const;
    inline void insertIntoDictionary(js::Shape **dictp);

    js::Shape *getChild(JSContext *cx, const js::Shape &child, js::Shape **listp);

    bool maybeHash(JSContext *cx);

    void setTable(js::PropertyTable *t) const {
        JS_ASSERT_IF(t && t->freelist != SHAPE_INVALID_SLOT, t->freelist < slotSpan);
        table = t;
    }

    /*
     * Setter for parent. The challenge is to maintain JSObjectMap::slotSpan in
     * the face of arbitrary slot order.
     *
     * By induction, an empty shape has a slotSpan member correctly computed as
     * JSCLASS_FREE(clasp) -- see EmptyShape's constructor in jsscopeinlines.h.
     * This is the basis case, where p is null.
     *
     * Any child shape, whether in a shape tree or in a dictionary list, must
     * have a slotSpan either one greater than its slot value (if the child's
     * slot is SHAPE_INVALID_SLOT, this will yield 0; the static assertion
     * below enforces this), or equal to its parent p's slotSpan, whichever is
     * greater. This is the inductive step.
     *
     * If we maintained shape paths such that parent slot was always one less
     * than child slot, possibly with an exception for SHAPE_INVALID_SLOT slot
     * values where we would use another way of computing slotSpan based on the
     * PropertyTable (as JSC does), then we would not need to store slotSpan in
     * Shape (to be precise, in its base struct, JSobjectMap).
     *
     * But we currently scramble slots along shape paths due to resolve-based
     * creation of shapes mapping reserved slots, and we do not have the needed
     * PropertyTable machinery to use as an alternative when parent slot is not
     * one less than child slot. This machinery is neither simple nor free, as
     * it must involve creating a table for any slot-less transition and then
     * pinning the table to its shape.
     *
     * Use of 'delete' can scramble slots along the shape lineage too, although
     * it always switches the target object to dictionary mode, so the cost of
     * a pinned table is less onerous.
     *
     * Note that allocating a uint32 slotSpan member in JSObjectMap takes no
     * net extra space on 64-bit targets (it packs with shape). And on 32-bit
     * targets, adding slotSpan to JSObjectMap takes no gross extra space,
     * because Shape rounds up to an even number of 32-bit words (required for
     * GC-thing and js::Value allocation in any event) on 32-bit targets.
     *
     * So in terms of space, we can afford to maintain both slotSpan and slot,
     * but it might be better if we eliminated slotSpan using slot combined
     * with an auxiliary mechanism based on table.
     */
    void setParent(js::Shape *p) {
        JS_STATIC_ASSERT(uint32(SHAPE_INVALID_SLOT) == ~uint32(0));
        if (p)
            slotSpan = JS_MAX(p->slotSpan, slot + 1);
        JS_ASSERT(slotSpan < JSObject::NSLOTS_LIMIT);
        parent = p;
    }

    void insertFree(js::Shape **freep) {
#ifdef DEBUG
        memset(this, JS_FREE_PATTERN, sizeof *this);
#endif
        id = JSID_VOID;
        parent = *freep;
        if (parent)
            parent->listp = &parent;
        listp = freep;
        *freep = this;
    }

    void removeFree() {
        JS_ASSERT(JSID_IS_VOID(id));
        *listp = parent;
        if (parent)
            parent->listp = listp;
    }

  public:
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
            JS_ASSERT_IF(!cursor->parent, JSID_IS_EMPTY(cursor->id));
            return !cursor->parent;
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

  protected:
    /*
     * Implementation-private bits stored in shape->flags. See public: enum {}
     * flags further below, which were allocated FCFS over time, so interleave
     * with these bits.
     */
    enum {
        /* GC mark flag. */
        MARK            = 0x01,

        SHARED_EMPTY    = 0x02,

        /*
         * Set during a shape-regenerating GC if the shape has already been
         * regenerated.
         */
        SHAPE_REGEN     = 0x04,

        /* Property stored in per-object dictionary, not shared property tree. */
        IN_DICTIONARY   = 0x08,

        /* Prevent unwanted mutation of shared JSFunction::u.i.names nodes. */
        FROZEN          = 0x10
    };

    Shape(jsid id, js::PropertyOp getter, js::PropertyOp setter, uint32 slot, uintN attrs,
          uintN flags, intN shortid, uint32 shape = INVALID_SHAPE, uint32 slotSpan = 0);

    /* Used by EmptyShape (see jsscopeinlines.h). */
    Shape(JSContext *cx, Class *aclasp);

    bool marked() const         { return (flags & MARK) != 0; }
    void mark() const           { flags |= MARK; }
    void clearMark()            { flags &= ~MARK; }

    bool hasRegenFlag() const   { return (flags & SHAPE_REGEN) != 0; }
    void setRegenFlag()         { flags |= SHAPE_REGEN; }
    void clearRegenFlag()       { flags &= ~SHAPE_REGEN; }

    bool inDictionary() const   { return (flags & IN_DICTIONARY) != 0; }
    bool frozen() const         { return (flags & FROZEN) != 0; }
    void setFrozen()            { flags |= FROZEN; }

    bool isEmptyShape() const   { JS_ASSERT_IF(!parent, JSID_IS_EMPTY(id)); return !parent; }

  public:
    /* Public bits stored in shape->flags. */
    enum {
        ALIAS           = 0x20,
        HAS_SHORTID     = 0x40,
        METHOD          = 0x80,
        PUBLIC_FLAGS    = ALIAS | HAS_SHORTID | METHOD
    };

    uintN getFlags() const  { return flags & PUBLIC_FLAGS; }
    bool isAlias() const    { return (flags & ALIAS) != 0; }
    bool hasShortID() const { return (flags & HAS_SHORTID) != 0; }
    bool isMethod() const   { return (flags & METHOD) != 0; }

    JSObject &methodObject() const { JS_ASSERT(isMethod()); return *getterObj; }

    js::PropertyOp getter() const { return rawGetter; }
    bool hasDefaultGetter() const  { return !rawGetter; }
    js::PropertyOp getterOp() const { JS_ASSERT(!hasGetterValue()); return rawGetter; }
    JSObject *getterObject() const { JS_ASSERT(hasGetterValue()); return getterObj; }

    // Per ES5, decode null getterObj as the undefined value, which encodes as null.
    js::Value getterValue() const {
        JS_ASSERT(hasGetterValue());
        return getterObj ? js::ObjectValue(*getterObj) : js::UndefinedValue();
    }

    js::Value getterOrUndefined() const {
        return hasGetterValue() && getterObj ? js::ObjectValue(*getterObj) : js::UndefinedValue();
    }

    js::PropertyOp setter() const { return rawSetter; }
    bool hasDefaultSetter() const  { return !rawSetter; }
    js::PropertyOp setterOp() const { JS_ASSERT(!hasSetterValue()); return rawSetter; }
    JSObject *setterObject() const { JS_ASSERT(hasSetterValue()); return setterObj; }

    // Per ES5, decode null setterObj as the undefined value, which encodes as null.
    js::Value setterValue() const {
        JS_ASSERT(hasSetterValue());
        return setterObj ? js::ObjectValue(*setterObj) : js::UndefinedValue();
    }

    js::Value setterOrUndefined() const {
        return hasSetterValue() && setterObj ? js::ObjectValue(*setterObj) : js::UndefinedValue();
    }

    inline JSDHashNumber hash() const;
    inline bool matches(const js::Shape *p) const;
    inline bool matchesParamsAfterId(js::PropertyOp agetter, js::PropertyOp asetter,
                                     uint32 aslot, uintN aattrs, uintN aflags,
                                     intN ashortid) const;

    bool get(JSContext* cx, JSObject *obj, JSObject *pobj, js::Value* vp) const;
    bool set(JSContext* cx, JSObject *obj, js::Value* vp) const;

    inline bool isSharedPermanent() const;

    void trace(JSTracer *trc) const;

    bool hasSlot() const { return (attrs & JSPROP_SHARED) == 0; }

    uint8 attributes() const { return attrs; }
    bool configurable() const { return (attrs & JSPROP_PERMANENT) == 0; }
    bool enumerable() const { return (attrs & JSPROP_ENUMERATE) != 0; }
    bool writable() const {
        // JS_ASSERT(isDataDescriptor());
        return (attrs & JSPROP_READONLY) == 0;
    }
    bool hasGetterValue() const { return attrs & JSPROP_GETTER; }
    bool hasSetterValue() const { return attrs & JSPROP_SETTER; }

    bool hasDefaultGetterOrIsMethod() const {
        return hasDefaultGetter() || isMethod();
    }

    bool isDataDescriptor() const {
        return (attrs & (JSPROP_SETTER | JSPROP_GETTER)) == 0;
    }
    bool isAccessorDescriptor() const {
        return (attrs & (JSPROP_SETTER | JSPROP_GETTER)) != 0;
    }

    uint32 entryCount() const {
        if (table)
            return table->entryCount;

        const js::Shape *shape = this;
        uint32 count = 0;
        for (js::Shape::Range r = shape->all(); !r.empty(); r.popFront())
            ++count;
        return count;
    }

#ifdef DEBUG
    void dump(JSContext *cx, FILE *fp) const;
    void dumpSubtree(JSContext *cx, int level, FILE *fp) const;
#endif
};

struct EmptyShape : public js::Shape
{
    EmptyShape(JSContext *cx, js::Class *aclasp);

    js::Class *getClass() const { return clasp; };

    static EmptyShape *create(JSContext *cx, js::Class *clasp) {
        js::Shape *eprop = JS_PROPERTY_TREE(cx).newShape(cx);
        if (!eprop)
            return NULL;
        return new (eprop) EmptyShape(cx, clasp);
    }
};

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

inline js::Shape **
JSObject::nativeSearch(jsid id, bool adding)
{
    return js::Shape::search(&lastProp, id, adding);
}

inline const js::Shape *
JSObject::nativeLookup(jsid id)
{
    return SHAPE_FETCH(nativeSearch(id));
}

inline bool
JSObject::nativeContains(jsid id)
{
    return nativeLookup(id) != NULL;
}

inline bool
JSObject::nativeContains(const js::Shape &shape)
{
    return nativeLookup(shape.id) == &shape;
}

inline const js::Shape *
JSObject::lastProperty() const
{
    JS_ASSERT(isNative());
    JS_ASSERT(!JSID_IS_VOID(lastProp->id));
    return lastProp;
}

inline bool
JSObject::nativeEmpty() const
{
    return lastProperty()->isEmptyShape();
}

inline bool
JSObject::inDictionaryMode() const
{
    return lastProperty()->inDictionary();
}

inline uint32
JSObject::propertyCount() const
{
    return lastProperty()->entryCount();
}

inline bool
JSObject::hasPropertyTable() const
{
    return !!lastProperty()->table;
}

/*
 * FIXME: shape must not be null, should use a reference here and other places.
 */
inline void
JSObject::setLastProperty(const js::Shape *shape)
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(!JSID_IS_VOID(shape->id));
    JS_ASSERT_IF(lastProp, !JSID_IS_VOID(lastProp->id));

    lastProp = const_cast<js::Shape *>(shape);
}

inline void
JSObject::removeLastProperty()
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(!JSID_IS_VOID(lastProp->parent->id));

    lastProp = lastProp->parent;
}

namespace js {

inline void
Shape::removeFromDictionary(JSObject *obj) const
{
    JS_ASSERT(!frozen());
    JS_ASSERT(inDictionary());
    JS_ASSERT(obj->inDictionaryMode());
    JS_ASSERT(listp);
    JS_ASSERT(!JSID_IS_VOID(id));

    JS_ASSERT(obj->lastProp->inDictionary());
    JS_ASSERT(obj->lastProp->listp == &obj->lastProp);
    JS_ASSERT_IF(obj->lastProp != this, !JSID_IS_VOID(obj->lastProp->id));
    JS_ASSERT_IF(obj->lastProp->parent, !JSID_IS_VOID(obj->lastProp->parent->id));

    if (parent)
        parent->listp = listp;
    *listp = parent;
    listp = NULL;
}

inline void
Shape::insertIntoDictionary(js::Shape **dictp)
{
    /*
     * Don't assert inDictionaryMode() here because we may be called from
     * JSObject::toDictionaryMode via JSObject::newDictionaryShape.
     */
    JS_ASSERT(inDictionary());
    JS_ASSERT(!listp);
    JS_ASSERT(!JSID_IS_VOID(id));

    JS_ASSERT_IF(*dictp, !(*dictp)->frozen());
    JS_ASSERT_IF(*dictp, (*dictp)->inDictionary());
    JS_ASSERT_IF(*dictp, (*dictp)->listp == dictp);
    JS_ASSERT_IF(*dictp, !JSID_IS_VOID((*dictp)->id));

    setParent(*dictp);
    if (parent)
        parent->listp = &parent;
    listp = dictp;
    *dictp = this;
}

} /* namespace js */

/*
 * If SHORTID is set in shape->flags, we use shape->shortid rather
 * than id when calling shape's getter or setter.
 */
#define SHAPE_USERID(shape)                                                   \
    ((shape)->hasShortID() ? INT_TO_JSID((shape)->shortid)                    \
                           : (shape)->id)

#ifndef JS_THREADSAFE
# define js_GenerateShape(cx, gcLocked)    js_GenerateShape (cx)
#endif

extern uint32
js_GenerateShape(JSContext *cx, bool gcLocked);

#ifdef DEBUG
struct JSScopeStats {
    jsrefcount          searches;
    jsrefcount          hits;
    jsrefcount          misses;
    jsrefcount          hashes;
    jsrefcount          hashHits;
    jsrefcount          hashMisses;
    jsrefcount          steps;
    jsrefcount          stepHits;
    jsrefcount          stepMisses;
    jsrefcount          initSearches;
    jsrefcount          changeSearches;
    jsrefcount          tableAllocFails;
    jsrefcount          toDictFails;
    jsrefcount          wrapWatchFails;
    jsrefcount          adds;
    jsrefcount          addFails;
    jsrefcount          puts;
    jsrefcount          redundantPuts;
    jsrefcount          putFails;
    jsrefcount          changes;
    jsrefcount          changePuts;
    jsrefcount          changeFails;
    jsrefcount          compresses;
    jsrefcount          grows;
    jsrefcount          removes;
    jsrefcount          removeFrees;
    jsrefcount          uselessRemoves;
    jsrefcount          shrinks;
};

extern JS_FRIEND_DATA(JSScopeStats) js_scope_stats;

# define METER(x)       JS_ATOMIC_INCREMENT(&js_scope_stats.x)
#else
# define METER(x)       /* nothing */
#endif

namespace js {

JS_ALWAYS_INLINE js::Shape **
Shape::search(js::Shape **startp, jsid id, bool adding)
{
    METER(searches);
    if (!(*startp)->table) {
        /*
         * Not enough properties to justify hashing: search from *startp.
         *
         * We don't use a Range here, or stop at null parent (the empty shape
         * at the end), to avoid an extra load per iteration just to save a
         * load and id test at the end (when missing).
         */
        js::Shape **spp;

        for (spp = startp; js::Shape *shape = *spp; spp = &shape->parent) {
            if (shape->id == id) {
                METER(hits);
                return spp;
            }
        }
        METER(misses);
        return spp;
    }
    return (*startp)->table->search(id, adding);
}

#undef METER

inline bool
Shape::isSharedPermanent() const
{
    return (~attrs & (JSPROP_SHARED | JSPROP_PERMANENT)) == 0;
}

}

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

#endif /* jsscope_h___ */
