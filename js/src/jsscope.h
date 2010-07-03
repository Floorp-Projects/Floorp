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
#ifdef DEBUG
#include <stdio.h>
#endif

#include "jstypes.h"
#include "jscntxt.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jspropertycache.h"
#include "jspropertytree.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800)
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

JS_BEGIN_EXTERN_C

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
 * labeled X, and set scope->lastProp to point at that node.  If any non-empty
 * scope whose most recently added property is labeled Y has another property
 * labeled Z added, find or create a node for Z under the node that was added
 * for Y, and set scope->lastProp to point at that node.
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
 * tree, so that the ancestor line can be iterated from scope->lastProp when
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
 * jsscope.c; others must treat JSScopeProperty.kids as opaque.  We leave it
 * strongly typed for debug-ability of the common (null or one-kid) cases.
 *
 * One final twist (can you stand it?): the mean number of entries per scope
 * in Mozilla is < 5, with a large standard deviation (~8).  Instead of always
 * allocating scope->table, we leave it null while initializing all the other
 * scope members as if it were non-null and minimal-length.  Until a property
 * is added that crosses the threshold of 6 or more entries for hashing, we use
 * linear search from scope->lastProp to find a given id, and save on the space
 * overhead of a hash table.
 *
 * See jspropertytree.{h,cpp} for the actual PropertyTree implementation. This
 * file contains object property map (historical misnomer: "scope" AKA JSScope)
 * and property tree node ("sprop", JSScopeProperty) declarations.
 */

struct JSEmptyScope;

#define SPROP_INVALID_SLOT              0xffffffff

struct JSScope : public JSObjectMap
{
#ifdef JS_THREADSAFE
    JSTitle         title;              /* lock state */
#endif
    JSObject        *object;            /* object that owns this scope */
    uint32          freeslot;           /* index of next free slot in object */
  protected:
    uint8           flags;              /* flags, see below */
  public:
    int8            hashShift;          /* multiplicative hash shift */

    uint16          spare;              /* reserved */
    uint32          entryCount;         /* number of entries in table */
    uint32          removedCount;       /* removed entry sentinels in table */
    JSScopeProperty **table;            /* table of ptrs to shared tree nodes */
    JSEmptyScope    *emptyScope;        /* cache for getEmptyScope below */

    /*
     * A little information hiding for scope->lastProp, in case it ever becomes
     * a tagged pointer again.
     */
    inline JSScopeProperty *lastProperty() const;

  private:
    JSScopeProperty *getChildProperty(JSContext *cx, JSScopeProperty *parent,
                                      JSScopeProperty &child);

    JSScopeProperty *newDictionaryProperty(JSContext *cx, const JSScopeProperty &child,
                                           JSScopeProperty **childp);

    bool toDictionaryMode(JSContext *cx, JSScopeProperty *&aprop);

    /*
     * Private pointer to the last added property and methods to manipulate the
     * list it links among properties in this scope. The {remove,insert} pair
     * for DictionaryProperties assert that the scope is in dictionary mode and
     * any reachable properties are flagged as dictionary properties.
     *
     * NB: these private methods do *not* update this scope's shape to track
     * lastProp->shape after they finish updating the linked list in the case
     * where lastProp is updated. It is up to calling code in jsscope.cpp to
     * call updateShape(cx) after updating lastProp.
     */
    JSScopeProperty *lastProp;

    /* These four inline methods are defined further below in this .h file. */
    inline void setLastProperty(JSScopeProperty *sprop);
    inline void removeLastProperty();
    inline void removeDictionaryProperty(JSScopeProperty *sprop);
    inline void insertDictionaryProperty(JSScopeProperty *sprop, JSScopeProperty **childp);

    /* Defined in jsscopeinlines.h to avoid including implementation dependencies here. */
    inline void updateShape(JSContext *cx);
    inline void updateFlags(const JSScopeProperty *sprop, bool isDefinitelyAtom = false);

  protected:
    void initMinimal(JSContext *cx, uint32 newShape);

  private:
    bool createTable(JSContext *cx, bool report);
    bool changeTable(JSContext *cx, int change);
    void reportReadOnlyScope(JSContext *cx);

    void setOwnShape()          { flags |= OWN_SHAPE; }
    void clearOwnShape()        { flags &= ~OWN_SHAPE; }
    void generateOwnShape(JSContext *cx);

    JSScopeProperty **searchTable(jsid id, bool adding);
    inline JSScopeProperty **search(jsid id, bool adding);
    inline JSEmptyScope *createEmptyScope(JSContext *cx, JSClass *clasp);

    JSScopeProperty *addPropertyHelper(JSContext *cx, jsid id,
                                       JSPropertyOp getter, JSPropertyOp setter,
                                       uint32 slot, uintN attrs,
                                       uintN flags, intN shortid,
                                       JSScopeProperty **spp);

  public:
    JSScope(const JSObjectOps *ops, JSObject *obj)
      : JSObjectMap(ops, 0), object(obj) {}

    /* Create a mutable, owned, empty scope. */
    static JSScope *create(JSContext *cx, const JSObjectOps *ops,
                           JSClass *clasp, JSObject *obj, uint32 shape);

    void destroy(JSContext *cx);

    /*
     * Return an immutable, shareable, empty scope with the same ops as this
     * and the same freeslot as this had when empty.
     *
     * If |this| is the scope of an object |proto|, the resulting scope can be
     * used as the scope of a new object whose prototype is |proto|.
     */
    inline JSEmptyScope *getEmptyScope(JSContext *cx, JSClass *clasp);

    inline bool ensureEmptyScope(JSContext *cx, JSClass *clasp);

    inline bool canProvideEmptyScope(JSObjectOps *ops, JSClass *clasp);

    JSScopeProperty *lookup(jsid id);

    inline bool hasProperty(jsid id) { return lookup(id) != NULL; }
    inline bool hasProperty(JSScopeProperty *sprop);

    /* Add a property whose id is not yet in this scope. */
    JSScopeProperty *addProperty(JSContext *cx, jsid id,
                                 JSPropertyOp getter, JSPropertyOp setter,
                                 uint32 slot, uintN attrs,
                                 uintN flags, intN shortid);

    /* Add a data property whose id is not yet in this scope. */
    JSScopeProperty *addDataProperty(JSContext *cx, jsid id, uint32 slot, uintN attrs) {
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        return addProperty(cx, id, NULL, NULL, slot, attrs, 0, 0);
    }

    /* Add or overwrite a property for id in this scope. */
    JSScopeProperty *putProperty(JSContext *cx, jsid id,
                                 JSPropertyOp getter, JSPropertyOp setter,
                                 uint32 slot, uintN attrs,
                                 uintN flags, intN shortid);

    /* Change the given property into a sibling with the same id in this scope. */
    JSScopeProperty *changeProperty(JSContext *cx, JSScopeProperty *sprop,
                                    uintN attrs, uintN mask,
                                    JSPropertyOp getter, JSPropertyOp setter);

    /* Remove id from this scope. */
    bool removeProperty(JSContext *cx, jsid id);

    /* Clear the scope, making it empty. */
    void clear(JSContext *cx);

    /* Extend this scope to have sprop as its last-added property. */
    void extend(JSContext *cx, JSScopeProperty *sprop, bool isDefinitelyAtom = false);

    /*
     * Read barrier to clone a joined function object stored as a method.
     * Defined in jsscopeinlines.h, but not declared inline per standard style
     * in order to avoid gcc warnings.
     */
    bool methodReadBarrier(JSContext *cx, JSScopeProperty *sprop, jsval *vp);

    /*
     * Write barrier to check for a method value change. Defined inline below
     * after methodReadBarrier. Two flavors to handle JSOP_*GVAR, which deals
     * in slots not sprops, while not deoptimizing to map slot to sprop unless
     * flags show this is necessary. The methodShapeChange overload (directly
     * below) parallels this.
     */
    bool methodWriteBarrier(JSContext *cx, JSScopeProperty *sprop, jsval v);
    bool methodWriteBarrier(JSContext *cx, uint32 slot, jsval v);

    void trace(JSTracer *trc);

    void deletingShapeChange(JSContext *cx, JSScopeProperty *sprop);
    bool methodShapeChange(JSContext *cx, JSScopeProperty *sprop);
    bool methodShapeChange(JSContext *cx, uint32 slot);
    void protoShapeChange(JSContext *cx);
    void shadowingShapeChange(JSContext *cx, JSScopeProperty *sprop);
    bool globalObjectOwnShapeChange(JSContext *cx);

/* By definition, hashShift = JS_DHASH_BITS - log2(capacity). */
#define SCOPE_CAPACITY(scope)   JS_BIT(JS_DHASH_BITS-(scope)->hashShift)

    enum {
        DICTIONARY_MODE         = 0x0001,
        SEALED                  = 0x0002,
        BRANDED                 = 0x0004,
        INDEXED_PROPERTIES      = 0x0008,
        OWN_SHAPE               = 0x0010,
        METHOD_BARRIER          = 0x0020,

        /*
         * This flag toggles with each shape-regenerating GC cycle.
         * See JSRuntime::gcRegenShapesScopeFlag.
         */
        SHAPE_REGEN             = 0x0040,

        /* The anti-branded flag, to avoid overspecializing. */
        GENERIC                 = 0x0080
    };

    bool inDictionaryMode()     { return flags & DICTIONARY_MODE; }
    void setDictionaryMode()    { flags |= DICTIONARY_MODE; }
    void clearDictionaryMode()  { flags &= ~DICTIONARY_MODE; }

    /*
     * Don't define clearSealed, as it can't be done safely because JS_LOCK_OBJ
     * will avoid taking the lock if the object owns its scope and the scope is
     * sealed.
     */
    bool sealed()               { return flags & SEALED; }

    void seal(JSContext *cx) {
        JS_ASSERT(!isSharedEmpty());
        JS_ASSERT(!sealed());
        generateOwnShape(cx);
        flags |= SEALED;
    }

    /*
     * A branded scope's object contains plain old methods (function-valued
     * properties without magic getters and setters), and its scope->shape
     * evolves whenever a function value changes.
     */
    bool branded()              { return flags & BRANDED; }

    bool brand(JSContext *cx, uint32 slot, jsval v) {
        JS_ASSERT(!generic());
        JS_ASSERT(!branded());
        generateOwnShape(cx);
        if (js_IsPropertyCacheDisabled(cx))  // check for rt->shapeGen overflow
            return false;
        flags |= BRANDED;
        return true;
    }

    bool generic()              { return flags & GENERIC; }

    /*
     * Here and elsewhere "unbrand" means "make generic". We never actually
     * clear the BRANDED bit on any object. Once branded, there's no point in
     * being generic, since the shape has already evolved unpredictably. So
     * obj->unbrand() on a branded object does nothing.
     */
    void unbrand(JSContext *cx) {
        if (!branded())
            flags |= GENERIC;
    }

    bool hadIndexedProperties() { return flags & INDEXED_PROPERTIES; }
    void setIndexedProperties() { flags |= INDEXED_PROPERTIES; }

    bool hasOwnShape()          { return flags & OWN_SHAPE; }

    bool hasRegenFlag(uint8 regenFlag) { return (flags & SHAPE_REGEN) == regenFlag; }

    /*
     * A scope has a method barrier when some compiler-created "null closure"
     * function objects (functions that do not use lexical bindings above their
     * scope, only free variable names) that have a correct JSSLOT_PARENT value
     * thanks to the COMPILE_N_GO optimization are stored as newly added direct
     * property values of the scope's object.
     *
     * The de-facto standard JS language requires each evaluation of such a
     * closure to result in a unique (according to === and observable effects)
     * function object. ES3 tried to allow implementations to "join" such
     * objects to a single compiler-created object, but this makes an overt
     * mutation hazard, also an "identity hazard" against interoperation among
     * implementations that join and do not join.
     *
     * To stay compatible with the de-facto standard, we store the compiler-
     * created function object as the method value and set the METHOD_BARRIER
     * flag.
     *
     * The method value is part of the method property tree node's identity, so
     * it effectively  brands the scope with a predictable shape corresponding
     * to the method value, but without the overhead of setting the BRANDED
     * flag, which requires assigning a new shape peculiar to each branded
     * scope. Instead the shape is shared via the property tree among all the
     * scopes referencing the method property tree node.
     *
     * Then when reading from a scope for which scope->hasMethodBarrier() is
     * true, we count on the scope's qualified/guarded shape being unique and
     * add a read barrier that clones the compiler-created function object on
     * demand, reshaping the scope.
     *
     * This read barrier is bypassed when evaluating the callee sub-expression
     * of a call expression (see the JOF_CALLOP opcodes in jsopcode.tbl), since
     * such ops do not present an identity or mutation hazard. The compiler
     * performs this optimization only for null closures that do not use their
     * own name or equivalent built-in references (arguments.callee).
     *
     * The BRANDED write barrier, JSScope::methodWriteBarrer, must check for
     * METHOD_BARRIER too, and regenerate this scope's shape if the method's
     * value is in fact changing.
     */
    bool hasMethodBarrier()     { return flags & METHOD_BARRIER; }
    void setMethodBarrier()     { flags |= METHOD_BARRIER; }

    /*
     * Test whether this scope may be branded due to method calls, which means
     * any assignment to a function-valued property must regenerate shape; else
     * test whether this scope has method properties, which require a method
     * write barrier.
     */
    bool
    brandedOrHasMethodBarrier() { return flags & (BRANDED | METHOD_BARRIER); }

    bool isSharedEmpty() const  { return !object; }

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
};

struct JSEmptyScope : public JSScope
{
    JSClass * const clasp;
    jsrefcount      nrefs;              /* count of all referencing objects */

    JSEmptyScope(JSContext *cx, const JSObjectOps *ops, JSClass *clasp);

    JSEmptyScope *hold() {
        /* The method is only called for already held objects. */
        JS_ASSERT(nrefs >= 1);
        JS_ATOMIC_INCREMENT(&nrefs);
        return this;
    }

    void drop(JSContext *cx) {
        JS_ASSERT(nrefs >= 1);
        JS_ATOMIC_DECREMENT(&nrefs);
        if (nrefs == 0)
            destroy(cx);
    }

    /*
     * Optimized version of the drop method to use from the object finalizer
     * to skip expensive JS_ATOMIC_DECREMENT.
     */
    void dropFromGC(JSContext *cx) {
#ifdef JS_THREADSAFE
        JS_ASSERT(CX_THREAD_IS_RUNNING_GC(cx));
#endif
        JS_ASSERT(nrefs >= 1);
        --nrefs;
        if (nrefs == 0)
            destroy(cx);
    }
};

inline bool
JS_IS_SCOPE_LOCKED(JSContext *cx, JSScope *scope)
{
    return JS_IS_TITLE_LOCKED(cx, &scope->title);
}

inline JSScope *
JSObject::scope() const
{
    JS_ASSERT(isNative());
    return (JSScope *) map;
}

inline uint32
JSObject::shape() const
{
    JS_ASSERT(map->shape != JSObjectMap::SHAPELESS);
    return map->shape;
}

inline jsval
JSObject::lockedGetSlot(uintN slot) const
{
    OBJ_CHECK_SLOT(this, slot);
    return this->getSlot(slot);
}

inline void
JSObject::lockedSetSlot(uintN slot, jsval value)
{
    OBJ_CHECK_SLOT(this, slot);
    this->setSlot(slot, value);
}

/*
 * Helpers for reinterpreting JSPropertyOp as JSObject* for scripted getters
 * and setters.
 */
namespace js {

inline JSObject *
CastAsObject(JSPropertyOp op)
{
    return JS_FUNC_TO_DATA_PTR(JSObject *, op);
}

inline jsval
CastAsObjectJSVal(JSPropertyOp op)
{
    return OBJECT_TO_JSVAL(JS_FUNC_TO_DATA_PTR(JSObject *, op));
}

class PropertyTree;

} /* namespace js */

struct JSScopeProperty {
    friend struct JSScope;
    friend class js::PropertyTree;
    friend JSDHashOperator js::RemoveNodeIfDead(JSDHashTable *table, JSDHashEntryHdr *hdr,
                                                uint32 number, void *arg);
    friend void js::SweepScopeProperties(JSContext *cx);

    jsid            id;                 /* int-tagged jsval/untagged JSAtom* */

  private:
    union {
        JSPropertyOp    rawGetter;      /* getter and setter hooks or objects */
        JSObject        *getterObj;     /* user-defined callable "get" object or
                                           null if sprop->hasGetterValue() */
        JSScopeProperty *next;          /* next node in freelist */
    };

    union {
        JSPropertyOp    rawSetter;      /* getter is JSObject* and setter is 0
                                           if sprop->isMethod() */
        JSObject        *setterObj;     /* user-defined callable "set" object or
                                           null if sprop->hasSetterValue() */
        JSScopeProperty **prevp;        /* pointer to previous node's next, or
                                           pointer to head of freelist */
    };

    void insertFree(JSScopeProperty *&list) {
        id = JSVAL_NULL;
        next = list;
        prevp = &list;
        if (list)
            list->prevp = &next;
        list = this;
    }

    void removeFree() {
        JS_ASSERT(JSVAL_IS_NULL(id));
        *prevp = next;
        if (next)
            next->prevp = prevp;
    }

  public:
    uint32          slot;               /* abstract index in object slots */
  private:
    uint8           attrs;              /* attributes, see jsapi.h JSPROP_* */
    uint8           flags;              /* flags, see below for defines */
  public:
    int16           shortid;            /* tinyid, or local arg/var index */
    JSScopeProperty *parent;            /* parent node, reverse for..in order */
    union {
        JSScopeProperty *kids;          /* null, single child, or a tagged ptr
                                           to many-kids data structure */
        JSScopeProperty **childp;       /* dictionary list starting at lastProp
                                           has a double-indirect back pointer,
                                           either to sprop->parent if not last,
                                           else to scope->lastProp */
    };
    uint32          shape;              /* property cache shape identifier */

  private:
    /*
     * Implementation-private bits stored in sprop->flags. See public: enum {}
     * flags further below, which were allocated FCFS over time, so interleave
     * with these bits.
     */
    enum {
        /* GC mark flag. */
        MARK            = 0x01,

        /*
         * Set during a shape-regenerating GC if the shape has already been
         * regenerated. Unlike JSScope::SHAPE_REGEN, this does not toggle with
         * each GC. js::SweepScopeProperties clears it.
         */
        SHAPE_REGEN     = 0x08,

        /* Property stored in per-object dictionary, not shared property tree. */
        IN_DICTIONARY   = 0x20
    };

    JSScopeProperty(jsid id, JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                    uintN attrs, uintN flags, intN shortid);

    bool marked() const { return (flags & MARK) != 0; }
    void mark() { flags |= MARK; }
    void clearMark() { flags &= ~MARK; }

    bool hasRegenFlag() const { return (flags & SHAPE_REGEN) != 0; }
    void setRegenFlag() { flags |= SHAPE_REGEN; }
    void clearRegenFlag() { flags &= ~SHAPE_REGEN; }

    bool inDictionary() const { return (flags & IN_DICTIONARY) != 0; }

  public:
    /* Public bits stored in sprop->flags. */
    enum {
        ALIAS           = 0x02,
        HAS_SHORTID     = 0x04,
        METHOD          = 0x10,
        PUBLIC_FLAGS    = ALIAS | HAS_SHORTID | METHOD
    };

    uintN getFlags() const  { return flags & PUBLIC_FLAGS; }
    bool isAlias() const    { return (flags & ALIAS) != 0; }
    bool hasShortID() const { return (flags & HAS_SHORTID) != 0; }
    bool isMethod() const   { return (flags & METHOD) != 0; }

    JSObject *methodObject() const { JS_ASSERT(isMethod()); return getterObj; }
    jsval methodValue() const      { return OBJECT_TO_JSVAL(methodObject()); }

    JSPropertyOp getter() const    { return rawGetter; }
    bool hasDefaultGetter() const  { return !rawGetter; }
    JSPropertyOp getterOp() const  { JS_ASSERT(!hasGetterValue()); return rawGetter; }
    JSObject *getterObject() const { JS_ASSERT(hasGetterValue()); return getterObj; }

    // Per ES5, decode null getterObj as the undefined value, which encodes as null.
    jsval getterValue() const {
        JS_ASSERT(hasGetterValue());
        return getterObj ? OBJECT_TO_JSVAL(getterObj) : JSVAL_VOID;
    }

    JSPropertyOp setter() const    { return rawSetter; }
    bool hasDefaultSetter() const  { return !rawSetter; }
    JSPropertyOp setterOp() const  { JS_ASSERT(!hasSetterValue()); return rawSetter; }
    JSObject *setterObject() const { JS_ASSERT(hasSetterValue()); return setterObj; }

    // Per ES5, decode null setterObj as the undefined value, which encodes as null.
    jsval setterValue() const {
        JS_ASSERT(hasSetterValue());
        return setterObj ? OBJECT_TO_JSVAL(setterObj) : JSVAL_VOID;
    }

    inline JSDHashNumber hash() const;
    inline bool matches(const JSScopeProperty *p) const;
    inline bool matchesParamsAfterId(JSPropertyOp agetter, JSPropertyOp asetter, uint32 aslot,
                                     uintN aattrs, uintN aflags, intN ashortid) const;

    bool get(JSContext* cx, JSObject* obj, JSObject *pobj, jsval* vp);
    bool set(JSContext* cx, JSObject* obj, jsval* vp);

    inline bool isSharedPermanent() const;

    void trace(JSTracer *trc);

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

#ifdef DEBUG
    void dump(JSContext *cx, FILE *fp);
    void dumpSubtree(JSContext *cx, int level, FILE *fp);
#endif
};

/* JSScopeProperty pointer tag bit indicating a collision. */
#define SPROP_COLLISION                 ((jsuword)1)
#define SPROP_REMOVED                   ((JSScopeProperty *) SPROP_COLLISION)

/* Macros to get and set sprop pointer values and collision flags. */
#define SPROP_IS_FREE(sprop)            ((sprop) == NULL)
#define SPROP_IS_REMOVED(sprop)         ((sprop) == SPROP_REMOVED)
#define SPROP_IS_LIVE(sprop)            ((sprop) > SPROP_REMOVED)
#define SPROP_FLAG_COLLISION(spp,sprop) (*(spp) = (JSScopeProperty *)         \
                                         ((jsuword)(sprop) | SPROP_COLLISION))
#define SPROP_HAD_COLLISION(sprop)      ((jsuword)(sprop) & SPROP_COLLISION)
#define SPROP_FETCH(spp)                SPROP_CLEAR_COLLISION(*(spp))

#define SPROP_CLEAR_COLLISION(sprop)                                          \
    ((JSScopeProperty *) ((jsuword)(sprop) & ~SPROP_COLLISION))

#define SPROP_STORE_PRESERVING_COLLISION(spp, sprop)                          \
    (*(spp) = (JSScopeProperty *) ((jsuword)(sprop)                           \
                                   | SPROP_HAD_COLLISION(*(spp))))

inline JSScopeProperty *
JSScope::lookup(jsid id)
{
    return SPROP_FETCH(search(id, false));
}

inline bool
JSScope::hasProperty(JSScopeProperty *sprop)
{
    return lookup(sprop->id) == sprop;
}

inline JSScopeProperty *
JSScope::lastProperty() const
{
    JS_ASSERT_IF(lastProp, !JSVAL_IS_NULL(lastProp->id));
    return lastProp;
}

/*
 * Note that sprop must not be null, as emptying a scope requires extra work
 * done only by methods in jsscope.cpp.
 */
inline void
JSScope::setLastProperty(JSScopeProperty *sprop)
{
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));
    JS_ASSERT_IF(lastProp, !JSVAL_IS_NULL(lastProp->id));

    lastProp = sprop;
}

inline void
JSScope::removeLastProperty()
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT_IF(lastProp->parent, !JSVAL_IS_NULL(lastProp->parent->id));

    lastProp = lastProp->parent;
    --entryCount;
}

inline void
JSScope::removeDictionaryProperty(JSScopeProperty *sprop)
{
    JS_ASSERT(inDictionaryMode());
    JS_ASSERT(sprop->inDictionary());
    JS_ASSERT(sprop->childp);
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));

    JS_ASSERT(lastProp->inDictionary());
    JS_ASSERT(lastProp->childp == &lastProp);
    JS_ASSERT_IF(lastProp != sprop, !JSVAL_IS_NULL(lastProp->id));
    JS_ASSERT_IF(lastProp->parent, !JSVAL_IS_NULL(lastProp->parent->id));

    if (sprop->parent)
        sprop->parent->childp = sprop->childp;
    *sprop->childp = sprop->parent;
    --entryCount;
    sprop->childp = NULL;
}

inline void
JSScope::insertDictionaryProperty(JSScopeProperty *sprop, JSScopeProperty **childp)
{
    /*
     * Don't assert inDictionaryMode() here because we may be called from
     * toDictionaryMode via newDictionaryProperty.
     */
    JS_ASSERT(sprop->inDictionary());
    JS_ASSERT(!sprop->childp);
    JS_ASSERT(!JSVAL_IS_NULL(sprop->id));

    JS_ASSERT_IF(*childp, (*childp)->inDictionary());
    JS_ASSERT_IF(lastProp, lastProp->inDictionary());
    JS_ASSERT_IF(lastProp, lastProp->childp == &lastProp);
    JS_ASSERT_IF(lastProp, !JSVAL_IS_NULL(lastProp->id));

    sprop->parent = *childp;
    *childp = sprop;
    if (sprop->parent)
        sprop->parent->childp = &sprop->parent;
    sprop->childp = childp;
    ++entryCount;
}

/*
 * If SHORTID is set in sprop->flags, we use sprop->shortid rather
 * than id when calling sprop's getter or setter.
 */
#define SPROP_USERID(sprop)                                                   \
    ((sprop)->hasShortID() ? INT_TO_JSVAL((sprop)->shortid)                   \
                           : ID_TO_VALUE((sprop)->id))

#define SLOT_IN_SCOPE(slot,scope)         ((slot) < (scope)->freeslot)
#define SPROP_HAS_VALID_SLOT(sprop,scope) SLOT_IN_SCOPE((sprop)->slot, scope)

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
    jsrefcount          steps;
    jsrefcount          stepHits;
    jsrefcount          stepMisses;
    jsrefcount          tableAllocFails;
    jsrefcount          toDictFails;
    jsrefcount          wrapWatchFails;
    jsrefcount          adds;
    jsrefcount          addFails;
    jsrefcount          puts;
    jsrefcount          redundantPuts;
    jsrefcount          putFails;
    jsrefcount          changes;
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

inline JSScopeProperty **
JSScope::search(jsid id, bool adding)
{
    JSScopeProperty *sprop, **spp;

    METER(searches);
    if (!table) {
        /* Not enough properties to justify hashing: search from lastProp. */
        for (spp = &lastProp; (sprop = *spp); spp = &sprop->parent) {
            if (sprop->id == id) {
                METER(hits);
                return spp;
            }
        }
        METER(misses);
        return spp;
    }
    return searchTable(id, adding);
}

#undef METER

inline bool
JSScope::canProvideEmptyScope(JSObjectOps *ops, JSClass *clasp)
{
    /*
     * An empty scope cannot provide another empty scope, or wrongful two-level
     * prototype shape sharing ensues -- see bug 497789.
     */
    if (!object)
        return false;
    return this->ops == ops && (!emptyScope || emptyScope->clasp == clasp);
}

inline bool
JSScopeProperty::isSharedPermanent() const
{
    return (~attrs & (JSPROP_SHARED | JSPROP_PERMANENT)) == 0;
}

extern JSScope *
js_GetMutableScope(JSContext *cx, JSObject *obj);

extern void
js_TraceId(JSTracer *trc, jsid id);

JS_END_EXTERN_C

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

#endif /* jsscope_h___ */
