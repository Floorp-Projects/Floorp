/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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

/*
 * JS symbol tables.
 */
#include <new>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h"
#include "jsbit.h"
#include "jsclist.h"
#include "jsdhash.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"      /* for JS_ARGS_LENGTH_MAX */
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstracer.h"

#include "jsdbgapiinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;
using namespace js::gc;

uint32
js_GenerateShape(JSContext *cx, bool gcLocked)
{
    JSRuntime *rt;
    uint32 shape;

    rt = cx->runtime;
    shape = JS_ATOMIC_INCREMENT(&rt->shapeGen);
    JS_ASSERT(shape != 0);
    if (shape >= SHAPE_OVERFLOW_BIT) {
        /*
         * FIXME bug 440834: The shape id space has overflowed. Currently we
         * cope badly with this and schedule the GC on the every call. But
         * first we make sure that increments from other threads would not
         * have a chance to wrap around shapeGen to zero.
         */
        rt->shapeGen = SHAPE_OVERFLOW_BIT;
        shape = SHAPE_OVERFLOW_BIT;

#ifdef JS_THREADSAFE
        Conditionally<AutoLockGC> lockIf(!gcLocked, rt);
#endif
        TriggerGC(rt);
    }
    return shape;
}

bool
JSObject::ensureClassReservedSlotsForEmptyObject(JSContext *cx)
{
    JS_ASSERT(nativeEmpty());

    /*
     * Subtle rule: objects that call JSObject::ensureInstanceReservedSlots
     * must either:
     *
     * (a) never escape anywhere an ad-hoc property could be set on them; or
     *
     * (b) protect their instance-reserved slots with shapes, at least a custom
     * empty shape with the right slotSpan member.
     *
     * Block objects are the only objects that fall into category (a). While
     * Call objects cannot escape, they can grow ad-hoc properties via eval
     * of a var declaration, or due to a function statement being evaluated,
     * but they have slots mapped by compiler-created shapes, and thus (b) no
     * problem predicting first ad-hoc property slot. Bound Function objects
     * have a custom empty shape.
     *
     * (Note that Block, Call, and bound Function objects are the only native
     * class objects that are allowed to call ensureInstanceReservedSlots.)
     */
    uint32 nfixed = JSSLOT_FREE(getClass());
    if (nfixed > numSlots() && !allocSlots(cx, nfixed))
        return false;

    return true;
}

#define PROPERTY_TABLE_NBYTES(n) ((n) * sizeof(Shape *))

#ifdef DEBUG
JS_FRIEND_DATA(JSScopeStats) js_scope_stats = {0};

# define METER(x)       JS_ATOMIC_INCREMENT(&js_scope_stats.x)
#else
# define METER(x)       /* nothing */
#endif

bool
PropertyTable::init(Shape *lastProp, JSContext *cx)
{
    int sizeLog2;

    if (entryCount > HASH_THRESHOLD) {
        /*
         * Either we're creating a table for a large scope that was populated
         * via property cache hit logic under JSOP_INITPROP, JSOP_SETNAME, or
         * JSOP_SETPROP; or else calloc failed at least once already. In any
         * event, let's try to grow, overallocating to hold at least twice the
         * current population.
         */
        sizeLog2 = JS_CeilingLog2(2 * entryCount);
    } else {
        JS_ASSERT(hashShift == JS_DHASH_BITS - MIN_SIZE_LOG2);
        sizeLog2 = MIN_SIZE_LOG2;
    }

    /*
     * Use cx->runtime->calloc for memory accounting and overpressure handling
     * without OOM reporting. See PropertyTable::change.
     */
    entries = (Shape **) cx->runtime->calloc(JS_BIT(sizeLog2) * sizeof(Shape *));
    if (!entries) {
        METER(tableAllocFails);
        return false;
    }

    hashShift = JS_DHASH_BITS - sizeLog2;
    for (Shape::Range r = lastProp->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        METER(searches);
        METER(initSearches);
        Shape **spp = search(shape.id, true);

        /*
         * Beware duplicate args and arg vs. var conflicts: the youngest shape
         * (nearest to lastProp) must win. See bug 600067.
         */
        if (!SHAPE_FETCH(spp))
            SHAPE_STORE_PRESERVING_COLLISION(spp, &shape);
    }
    return true;
}

bool
Shape::maybeHash(JSContext *cx)
{
    JS_ASSERT(!table);
    uint32 nentries = entryCount();
    if (nentries >= PropertyTable::HASH_THRESHOLD) {
        table = cx->create<PropertyTable>(nentries);
        return table && table->init(this, cx);
    }
    return true;
}

#ifdef DEBUG
# include "jsprf.h"
# define LIVE_SCOPE_METER(cx,expr) JS_LOCK_RUNTIME_VOID(cx->runtime,expr)
#else
# define LIVE_SCOPE_METER(cx,expr) /* nothing */
#endif

static inline bool
InitField(JSContext *cx, EmptyShape *JSRuntime:: *field, Class *clasp, uint32 shape)
{
    if (EmptyShape *emptyShape = EmptyShape::create(cx, clasp)) {
        cx->runtime->*field = emptyShape;
        JS_ASSERT(emptyShape->shape == shape);
        return true;
    }
    return false;
}

/* static */
bool
Shape::initRuntimeState(JSContext *cx)
{
    /*
     * NewArguments allocates dslots to have enough room for the argc of the
     * particular arguments object being created.
     * never mutated, it's safe to pretend to have all the slots possible.
     *
     * Note how the fast paths in jsinterp.cpp for JSOP_LENGTH and JSOP_GETELEM
     * bypass resolution of scope properties for length and element indices on
     * arguments objects. This helps ensure that any arguments object needing
     * its own mutable scope (with unique shape) is a rare event.
     */
    if (!InitField(cx, &JSRuntime::emptyArgumentsShape, &js_ArgumentsClass,
                   Shape::EMPTY_ARGUMENTS_SHAPE)) {
        return false;
    }

    if (!InitField(cx, &JSRuntime::emptyBlockShape, &js_BlockClass, Shape::EMPTY_BLOCK_SHAPE))
        return false;

    /*
     * Initialize the shared scope for all empty Call objects so gets for args
     * and vars do not force the creation of a mutable scope for the particular
     * call object being accessed.
     */
    if (!InitField(cx, &JSRuntime::emptyCallShape, &js_CallClass, Shape::EMPTY_CALL_SHAPE))
        return false;

    /* A DeclEnv object holds the name binding for a named function expression. */
    if (!InitField(cx, &JSRuntime::emptyDeclEnvShape, &js_DeclEnvClass,
                   Shape::EMPTY_DECL_ENV_SHAPE)) {
        return false;
    }

    /* Non-escaping native enumerator objects share this empty scope. */
    if (!InitField(cx, &JSRuntime::emptyEnumeratorShape, &js_IteratorClass,
                   Shape::EMPTY_ENUMERATOR_SHAPE)) {
        return false;
    }

    /* Same drill for With objects. */
    if (!InitField(cx, &JSRuntime::emptyWithShape, &js_WithClass, Shape::EMPTY_WITH_SHAPE))
        return false;

    return true;
}

/* static */
void
Shape::finishRuntimeState(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    rt->emptyArgumentsShape = NULL;
    rt->emptyBlockShape = NULL;
    rt->emptyCallShape = NULL;
    rt->emptyDeclEnvShape = NULL;
    rt->emptyEnumeratorShape = NULL;
    rt->emptyWithShape = NULL;
}

JS_STATIC_ASSERT(sizeof(JSHashNumber) == 4);
JS_STATIC_ASSERT(sizeof(jsid) == JS_BYTES_PER_WORD);

#if JS_BYTES_PER_WORD == 4
# define HASH_ID(id) ((JSHashNumber)(JSID_BITS(id)))
#elif JS_BYTES_PER_WORD == 8
# define HASH_ID(id) ((JSHashNumber)(JSID_BITS(id)) ^ (JSHashNumber)((JSID_BITS(id)) >> 32))
#else
# error "Unsupported configuration"
#endif

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.  The inputs to multiplicative hash are
 * the golden ratio, expressed as a fixed-point 32 bit fraction, and the id
 * itself.
 */
#define HASH0(id)               (HASH_ID(id) * JS_GOLDEN_RATIO)
#define HASH1(hash0,shift)      ((hash0) >> (shift))
#define HASH2(hash0,log2,shift) ((((hash0) << (log2)) >> (shift)) | 1)

Shape **
PropertyTable::search(jsid id, bool adding)
{
    JSHashNumber hash0, hash1, hash2;
    int sizeLog2;
    Shape *stored, *shape, **spp, **firstRemoved;
    uint32 sizeMask;

    JS_ASSERT(entries);
    JS_ASSERT(!JSID_IS_VOID(id));

    /* Compute the primary hash address. */
    METER(hashes);
    hash0 = HASH0(id);
    hash1 = HASH1(hash0, hashShift);
    spp = entries + hash1;

    /* Miss: return space for a new entry. */
    stored = *spp;
    if (SHAPE_IS_FREE(stored)) {
        METER(misses);
        METER(hashMisses);
        return spp;
    }

    /* Hit: return entry. */
    shape = SHAPE_CLEAR_COLLISION(stored);
    if (shape && shape->id == id) {
        METER(hits);
        METER(hashHits);
        return spp;
    }

    /* Collision: double hash. */
    sizeLog2 = JS_DHASH_BITS - hashShift;
    hash2 = HASH2(hash0, sizeLog2, hashShift);
    sizeMask = JS_BITMASK(sizeLog2);

#ifdef DEBUG
    jsuword collision_flag = SHAPE_COLLISION;
#endif

    /* Save the first removed entry pointer so we can recycle it if adding. */
    if (SHAPE_IS_REMOVED(stored)) {
        firstRemoved = spp;
    } else {
        firstRemoved = NULL;
        if (adding && !SHAPE_HAD_COLLISION(stored))
            SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
        collision_flag &= jsuword(*spp) & SHAPE_COLLISION;
#endif
    }

    for (;;) {
        METER(steps);
        hash1 -= hash2;
        hash1 &= sizeMask;
        spp = entries + hash1;

        stored = *spp;
        if (SHAPE_IS_FREE(stored)) {
            METER(misses);
            METER(stepMisses);
            return (adding && firstRemoved) ? firstRemoved : spp;
        }

        shape = SHAPE_CLEAR_COLLISION(stored);
        if (shape && shape->id == id) {
            METER(hits);
            METER(stepHits);
            JS_ASSERT(collision_flag);
            return spp;
        }

        if (SHAPE_IS_REMOVED(stored)) {
            if (!firstRemoved)
                firstRemoved = spp;
        } else {
            if (adding && !SHAPE_HAD_COLLISION(stored))
                SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
            collision_flag &= jsuword(*spp) & SHAPE_COLLISION;
#endif
        }
    }

    /* NOTREACHED */
    return NULL;
}

bool
PropertyTable::change(int log2Delta, JSContext *cx)
{
    int oldlog2, newlog2;
    uint32 oldsize, newsize, nbytes;
    Shape **newTable, **oldTable, **spp, **oldspp, *shape;

    JS_ASSERT(entries);

    /*
     * Grow, shrink, or compress by changing this->entries. Here, we prefer
     * cx->runtime->calloc to js_calloc, which on OOM waits for a background
     * thread to finish sweeping and retry, if appropriate. Avoid cx->calloc
     * so our caller can be in charge of whether to JS_ReportOutOfMemory.
     */
    oldlog2 = JS_DHASH_BITS - hashShift;
    newlog2 = oldlog2 + log2Delta;
    oldsize = JS_BIT(oldlog2);
    newsize = JS_BIT(newlog2);
    nbytes = PROPERTY_TABLE_NBYTES(newsize);
    newTable = (Shape **) cx->runtime->calloc(nbytes);
    if (!newTable) {
        METER(tableAllocFails);
        return false;
    }

    /* Now that we have newTable allocated, update members. */
    hashShift = JS_DHASH_BITS - newlog2;
    removedCount = 0;
    oldTable = entries;
    entries = newTable;

    /* Copy only live entries, leaving removed and free ones behind. */
    for (oldspp = oldTable; oldsize != 0; oldspp++) {
        shape = SHAPE_FETCH(oldspp);
        if (shape) {
            METER(searches);
            METER(changeSearches);
            spp = search(shape->id, true);
            JS_ASSERT(SHAPE_IS_FREE(*spp));
            *spp = shape;
        }
        oldsize--;
    }

    /*
     * Finally, free the old entries storage. Note that cx->runtime->free just
     * calls js_free. Use js_free here to match PropertyTable::~PropertyTable,
     * which cannot have a cx or rt parameter.
     */
    js_free(oldTable);
    return true;
}

bool
PropertyTable::grow(JSContext *cx)
{
    JS_ASSERT(needsToGrow());

    uint32 size = capacity();
    int delta = removedCount < size >> 2;
    if (!delta)
        METER(compresses);
    else
        METER(grows);

    if (!change(delta, cx) && entryCount + removedCount == size - 1) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

Shape *
Shape::getChild(JSContext *cx, const js::Shape &child, Shape **listp)
{
    JS_ASSERT(!JSID_IS_VOID(child.id));
    JS_ASSERT(!child.inDictionary());

    if (inDictionary()) {
        Shape *oldShape = *listp;
        PropertyTable *table = oldShape ? oldShape->table : NULL;

        /*
         * Attempt to grow table if needed before extending *listp, rather than
         * risking OOM under table->grow after newDictionaryShape succeeds, and
         * then have to fix up *listp.
         */
        if (table && table->needsToGrow() && !table->grow(cx))
            return NULL;

        if (newDictionaryShape(cx, child, listp)) {
            Shape *newShape = *listp;

            JS_ASSERT(oldShape == newShape->parent);
            if (table) {
                /* Add newShape to the property table. */
                METER(searches);
                Shape **spp = table->search(newShape->id, true);

                /*
                 * Beware duplicate formal parameters, allowed by ECMA-262 in
                 * non-strict mode. Otherwise we know that JSFunction::addLocal
                 * (our caller) won't pass an id already in the table to us. In
                 * the case of duplicate formals, the last one wins, so while
                 * we must not overcount entries, we must store newShape.
                 */
                if (!SHAPE_FETCH(spp))
                    ++table->entryCount;
                SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);

                /* Hand the table off from oldShape to newShape. */
                oldShape->setTable(NULL);
                newShape->setTable(table);
            } else {
                if (!newShape->table)
                    newShape->maybeHash(cx);
            }
            return newShape;
        }

        return NULL;
    }

    if ((*listp)->entryCount() >= PropertyTree::MAX_HEIGHT) {
        Shape *dprop = Shape::newDictionaryList(cx, listp);
        if (!dprop)
            return NULL;
        return dprop->getChild(cx, child, listp);
    }

    Shape *shape = JS_PROPERTY_TREE(cx).getChild(cx, this, child);
    if (shape) {
        JS_ASSERT(shape->parent == this);
        JS_ASSERT(this == *listp);
        if (!shape->table)
            shape->maybeHash(cx);
        *listp = shape;
    }
    return shape;
}

/*
 * Get or create a property-tree or dictionary child property of parent, which
 * must be lastProp if inDictionaryMode(), else parent must be one of lastProp
 * or lastProp->parent.
 */
Shape *
JSObject::getChildProperty(JSContext *cx, Shape *parent, Shape &child)
{
    JS_ASSERT(!JSID_IS_VOID(child.id));
    JS_ASSERT(!child.inDictionary());

    /*
     * Aliases share another property's slot, passed in the |slot| parameter.
     * Shared properties have no slot. Unshared properties that do not alias
     * another property's slot allocate a slot here, but may lose it due to a
     * JS_ClearScope call.
     */
    if (!child.isAlias()) {
        if (child.attrs & JSPROP_SHARED) {
            child.slot = SHAPE_INVALID_SLOT;
        } else {
            /*
             * We may have set slot from a nearly-matching shape, above. If so,
             * we're overwriting that nearly-matching shape, so we can reuse
             * its slot -- we don't need to allocate a new one. Similarly, we
             * use a specific slot if provided by the caller.
             */
            if (child.slot == SHAPE_INVALID_SLOT && !allocSlot(cx, &child.slot))
                return NULL;
        }
    }

    Shape *shape;

    if (inDictionaryMode()) {
        JS_ASSERT(parent == lastProp);
        if (parent->frozen()) {
            parent = Shape::newDictionaryList(cx, &lastProp);
            if (!parent)
                return NULL;
            JS_ASSERT(!parent->frozen());
        }
        shape = Shape::newDictionaryShape(cx, child, &lastProp);
        if (!shape)
            return NULL;
    } else {
        shape = JS_PROPERTY_TREE(cx).getChild(cx, parent, child);
        if (shape) {
            JS_ASSERT(shape->parent == parent);
            JS_ASSERT_IF(parent != lastProp, parent == lastProp->parent);
            setLastProperty(shape);
        }
    }

    updateFlags(shape);
    updateShape(cx);
    return shape;
}

Shape *
Shape::newDictionaryShape(JSContext *cx, const Shape &child, Shape **listp)
{
    Shape *dprop = JS_PROPERTY_TREE(cx).newShape(cx);
    if (!dprop)
        return NULL;

    new (dprop) Shape(child.id, child.rawGetter, child.rawSetter, child.slot, child.attrs,
                      (child.flags & ~FROZEN) | IN_DICTIONARY, child.shortid,
                      js_GenerateShape(cx, false), child.slotSpan);

    dprop->listp = NULL;
    dprop->insertIntoDictionary(listp);

    JS_RUNTIME_METER(cx->runtime, liveDictModeNodes);
    return dprop;
}

Shape *
Shape::newDictionaryList(JSContext *cx, Shape **listp)
{
    Shape *shape = *listp;
    Shape *list = shape;

    Shape **childp = listp;
    *childp = NULL;

    while (shape) {
        JS_ASSERT_IF(!shape->frozen(), !shape->inDictionary());

        Shape *dprop = Shape::newDictionaryShape(cx, *shape, childp);
        if (!dprop) {
            METER(toDictFails);
            *listp = list;
            return NULL;
        }

        JS_ASSERT(!dprop->table);
        childp = &dprop->parent;
        shape = shape->parent;
    }

    list = *listp;
    JS_ASSERT(list->inDictionary());
    list->maybeHash(cx);
    return list;
}

bool
JSObject::toDictionaryMode(JSContext *cx)
{
    JS_ASSERT(!inDictionaryMode());
    if (!Shape::newDictionaryList(cx, &lastProp))
        return false;

    clearOwnShape();
    return true;
}

/*
 * Normalize stub getter and setter values for faster is-stub testing in the
 * SHAPE_CALL_[GS]ETTER macros.
 */
static inline bool
NormalizeGetterAndSetter(JSContext *cx, JSObject *obj,
                         jsid id, uintN attrs, uintN flags,
                         PropertyOp &getter,
                         PropertyOp &setter)
{
    if (setter == PropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_SETTER));
        setter = NULL;
    }
    if (flags & Shape::METHOD) {
        /* Here, getter is the method, a function object reference. */
        JS_ASSERT(getter);
        JS_ASSERT(!setter || setter == js_watch_set);
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    } else {
        if (getter == PropertyStub) {
            JS_ASSERT(!(attrs & JSPROP_GETTER));
            getter = NULL;
        }
    }

    return true;
}

#ifdef DEBUG
# define CHECK_SHAPE_CONSISTENCY(obj) obj->checkShapeConsistency()

void
JSObject::checkShapeConsistency()
{
    static int throttle = -1;
    if (throttle < 0) {
        if (const char *var = getenv("JS_CHECK_SHAPE_THROTTLE"))
            throttle = atoi(var);
        if (throttle < 0)
            throttle = 0;
    }
    if (throttle == 0)
        return;

    JS_ASSERT(isNative());
    if (hasOwnShape())
        JS_ASSERT(objShape != lastProp->shape);
    else
        JS_ASSERT(objShape == lastProp->shape);

    Shape *shape = lastProp;
    Shape *prev = NULL;

    if (inDictionaryMode()) {
        if (PropertyTable *table = shape->table) {
            for (uint32 fslot = table->freelist; fslot != SHAPE_INVALID_SLOT;
                 fslot = getSlotRef(fslot).toPrivateUint32()) {
                JS_ASSERT(fslot < shape->slotSpan);
            }

            for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
                JS_ASSERT_IF(shape != lastProp, !shape->table);

                Shape **spp = table->search(shape->id, false);
                JS_ASSERT(SHAPE_FETCH(spp) == shape);
            }
        } else {
            shape = shape->parent;
            for (int n = throttle; --n >= 0 && shape; shape = shape->parent)
                JS_ASSERT(!shape->table);
        }

        shape = lastProp;
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            JS_ASSERT_IF(shape->slot != SHAPE_INVALID_SLOT, shape->slot < shape->slotSpan);
            if (!prev) {
                JS_ASSERT(shape == lastProp);
                JS_ASSERT(shape->listp == &lastProp);
            } else {
                JS_ASSERT(shape->listp == &prev->parent);
                JS_ASSERT(prev->slotSpan >= shape->slotSpan);
            }
            prev = shape;
        }
    } else {
        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            if (PropertyTable *table = shape->table) {
                JS_ASSERT(shape->parent);
                for (Shape::Range r(shape); !r.empty(); r.popFront()) {
                    Shape **spp = table->search(r.front().id, false);
                    JS_ASSERT(SHAPE_FETCH(spp) == &r.front());
                }
            }
            if (prev) {
                JS_ASSERT(prev->slotSpan >= shape->slotSpan);
                shape->kids.checkConsistency(prev);
            }
            prev = shape;
        }

        if (throttle == 0) {
            JS_ASSERT(!shape->table);
            JS_ASSERT(JSID_IS_EMPTY(shape->id));
            JS_ASSERT(shape->slot == SHAPE_INVALID_SLOT);
        }
    }
}
#else
# define CHECK_SHAPE_CONSISTENCY(obj) ((void)0)
#endif

const Shape *
JSObject::addProperty(JSContext *cx, jsid id,
                      PropertyOp getter, PropertyOp setter,
                      uint32 slot, uintN attrs,
                      uintN flags, intN shortid)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    if (!isExtensible()) {
        reportNotExtensible(cx);
        return NULL;
    }

    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /* Search for id with adding = true in order to claim its entry. */
    Shape **spp = nativeSearch(id, true);
    JS_ASSERT(!SHAPE_FETCH(spp));
    const Shape *shape = addPropertyInternal(cx, id, getter, setter, slot, attrs, 
                                             flags, shortid, spp);
    if (!shape)
        return NULL;

    /* Update any watchpoints referring to this property. */
    if (!js_UpdateWatchpointsForShape(cx, this, shape)) {
        METER(wrapWatchFails);
        return NULL;
    }

    return shape;
}

const Shape *
JSObject::addPropertyInternal(JSContext *cx, jsid id,
                              PropertyOp getter, PropertyOp setter,
                              uint32 slot, uintN attrs,
                              uintN flags, intN shortid,
                              Shape **spp)
{
    JS_ASSERT_IF(inDictionaryMode(), !lastProp->frozen());

    PropertyTable *table = NULL;
    if (!inDictionaryMode()) {
        if (lastProp->entryCount() >= PropertyTree::MAX_HEIGHT) {
            if (!toDictionaryMode(cx))
                return NULL;
            spp = nativeSearch(id, true);
            table = lastProp->table;
        }
    } else if ((table = lastProp->table) != NULL) {
        if (table->needsToGrow()) {
            if (!table->grow(cx))
                return NULL;

            METER(searches);
            METER(changeSearches);
            spp = table->search(id, true);
            JS_ASSERT(!SHAPE_FETCH(spp));
        }
    }

    /* Find or create a property tree node labeled by our arguments. */
    const Shape *shape;
    {
        Shape child(id, getter, setter, slot, attrs, flags, shortid);
        shape = getChildProperty(cx, lastProp, child);
    }

    if (shape) {
        JS_ASSERT(shape == lastProp);

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SHAPE_STORE_PRESERVING_COLLISION(spp, shape);
            ++table->entryCount;

            /* Pass the table along to the new lastProp, namely shape. */
            JS_ASSERT(shape->parent->table == table);
            shape->parent->setTable(NULL);
            shape->setTable(table);
        }
#ifdef DEBUG
        LIVE_SCOPE_METER(cx, ++cx->runtime->liveObjectProps);
        JS_RUNTIME_METER(cx->runtime, totalObjectProps);
#endif

        /*
         * If we reach the hashing threshold, try to allocate lastProp->table.
         * If we can't (a rare event, preceded by swapping to death on most
         * modern OSes), stick with linear search rather than whining about
         * this little set-back.  Therefore we must test !lastProp->table and
         * entry count >= PropertyTable::HASH_THRESHOLD, not merely whether the
         * entry count just reached the threshold.
         */
        if (!lastProp->table)
            lastProp->maybeHash(cx);

        CHECK_SHAPE_CONSISTENCY(this);
        METER(adds);
        return shape;
    }

    CHECK_SHAPE_CONSISTENCY(this);
    METER(addFails);
    return NULL;
}

const Shape *
JSObject::putProperty(JSContext *cx, jsid id,
                      PropertyOp getter, PropertyOp setter,
                      uint32 slot, uintN attrs,
                      uintN flags, intN shortid)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    /*
     * Horrid non-strict eval, debuggers, and |default xml namespace ...| may
     * extend Call objects.
     */
    if (lastProp->frozen()) {
        if (!Shape::newDictionaryList(cx, &lastProp))
            return NULL;
        JS_ASSERT(!lastProp->frozen());
    }

    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /* Search for id in order to claim its entry if table has been allocated. */
    Shape **spp = nativeSearch(id, true);
    Shape *shape = SHAPE_FETCH(spp);
    if (!shape) {
        /*
         * You can't add properties to a non-extensible object, but you can change
         * attributes of properties in such objects.
         */
        if (!isExtensible()) {
            reportNotExtensible(cx);
            return NULL;
        }

        const Shape *new_shape =
            addPropertyInternal(cx, id, getter, setter, slot, attrs, flags, shortid, spp);
        if (!js_UpdateWatchpointsForShape(cx, this, new_shape)) {
            METER(wrapWatchFails);
            return NULL;
        }
        return new_shape;
    }

    /* Property exists: search must have returned a valid *spp. */
    JS_ASSERT(!SHAPE_IS_REMOVED(*spp));

    /*
     * If the caller wants to allocate a slot, but doesn't care which slot,
     * copy the existing shape's slot into slot so we can match shape, if all
     * other members match.
     */
    bool hadSlot = !shape->isAlias() && shape->hasSlot();
    uint32 oldSlot = shape->slot;
    if (!(attrs & JSPROP_SHARED) && slot == SHAPE_INVALID_SLOT && hadSlot)
        slot = oldSlot;

    /*
     * Now that we've possibly preserved slot, check whether all members match.
     * If so, this is a redundant "put" and we can return without more work.
     */
    if (shape->matchesParamsAfterId(getter, setter, slot, attrs, flags, shortid)) {
        METER(redundantPuts);
        return shape;
    }

    /*
     * Overwriting a non-last property requires switching to dictionary mode.
     * The shape tree is shared immutable, and we can't removeProperty and then
     * addPropertyInternal because a failure under add would lose data.
     */
    if (shape != lastProp && !inDictionaryMode()) {
        if (!toDictionaryMode(cx))
            return NULL;
        spp = nativeSearch(shape->id);
        shape = SHAPE_FETCH(spp);
    }

    /*
     * Now that we have passed the lastProp->frozen() check at the top of this
     * method, and the non-last-property conditioning just above, we are ready
     * to overwrite.
     *
     * Optimize the case of a non-frozen dictionary-mode object based on the
     * property that dictionaries exclusively own their mutable shape structs,
     * each of which has a unique shape number (not shared via a shape tree).
     *
     * This is more than an optimization: it is required to preserve for-in
     * enumeration order (see bug 601399).
     */
    if (inDictionaryMode()) {
        /* FIXME bug 593129 -- slot allocation and JSObject *this must move out of here! */
        if (slot == SHAPE_INVALID_SLOT && !(attrs & JSPROP_SHARED) && !(flags & Shape::ALIAS)) {
            if (!allocSlot(cx, &slot))
                return NULL;
        }

        shape->slot = slot;
        if (slot != SHAPE_INVALID_SLOT && slot >= shape->slotSpan) {
            shape->slotSpan = slot + 1;

            for (Shape *temp = lastProp; temp != shape; temp = temp->parent) {
                if (temp->slotSpan <= slot)
                    temp->slotSpan = slot + 1;
            }
        }

        shape->rawGetter = getter;
        shape->rawSetter = setter;
        shape->attrs = uint8(attrs);
        shape->flags = flags | Shape::IN_DICTIONARY;
        shape->shortid = int16(shortid);

        /*
         * We are done updating shape and lastProp. Now we may need to update
         * flags and we will need to update objShape, which is no longer "own".
         * In the last non-dictionary property case in the else clause just
         * below, getChildProperty handles this for us. First update flags.
         */
        updateFlags(shape);

        /*
         * We have just mutated shape in place, but nothing caches it based on
         * shape->shape unless shape is lastProp and !hasOwnShape()). Therefore
         * we regenerate only lastProp->shape. We will clearOwnShape(), which
         * sets objShape to lastProp->shape.
         */
        lastProp->shape = js_GenerateShape(cx, false);
        clearOwnShape();
    } else {
        /*
         * Updating lastProp in a non-dictionary-mode object. Such objects
         * share their shapes via a tree rooted at a prototype emptyShape, or
         * perhaps a well-known compartment-wide singleton emptyShape.
         *
         * If any shape in the tree has a property hashtable, it is shared and
         * immutable too, therefore we must not update *spp.
         */
        JS_ASSERT(shape == lastProp);
        removeLastProperty();

        /* Find or create a property tree node labeled by our arguments. */
        Shape child(id, getter, setter, slot, attrs, flags, shortid);

        Shape *newShape = getChildProperty(cx, lastProp, child);
        if (!newShape) {
            setLastProperty(shape);
            CHECK_SHAPE_CONSISTENCY(this);
            METER(putFails);
            return NULL;
        }

        shape = newShape;

        if (!shape->table) {
            /* See JSObject::addPropertyInternal comment about ignoring OOM. */
            shape->maybeHash(cx);
        }
    }

    /*
     * Can't fail now, so free the previous incarnation's slot if the new shape
     * has no slot. But we do not need to free oldSlot (and must not, as trying
     * to will botch an assertion in JSObject::freeSlot) if the new lastProp
     * (shape here) has a slotSpan that does not cover it.
     */
    if (hadSlot && !shape->hasSlot()) {
        if (oldSlot < shape->slotSpan)
            freeSlot(cx, oldSlot);
#ifdef DEBUG
        else
            getSlotRef(oldSlot).setUndefined();
#endif
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }

    CHECK_SHAPE_CONSISTENCY(this);
    METER(puts);

    if (!js_UpdateWatchpointsForShape(cx, this, shape)) {
        METER(wrapWatchFails);
        return NULL;
    }

    return shape;
}

const Shape *
JSObject::changeProperty(JSContext *cx, const Shape *shape, uintN attrs, uintN mask,
                         PropertyOp getter, PropertyOp setter)
{
    JS_ASSERT_IF(inDictionaryMode(), !lastProp->frozen());
    JS_ASSERT(!JSID_IS_VOID(shape->id));
    JS_ASSERT(nativeContains(*shape));

    attrs |= shape->attrs & mask;

    /* Allow only shared (slotless) => unshared (slotful) transition. */
    JS_ASSERT(!((attrs ^ shape->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    /* Don't allow method properties to be changed to have a getter. */
    JS_ASSERT_IF(getter != shape->rawGetter, !shape->isMethod());

    if (getter == PropertyStub)
        getter = NULL;
    if (setter == PropertyStub)
        setter = NULL;
    if (shape->attrs == attrs && shape->getter() == getter && shape->setter() == setter)
        return shape;

    Shape child(shape->id, getter, setter, shape->slot, attrs, shape->flags, shape->shortid);

    const Shape *newShape;

    if (inDictionaryMode()) {
        shape->removeFromDictionary(this);
        newShape = Shape::newDictionaryShape(cx, child, &lastProp);
        if (newShape) {
            JS_ASSERT(newShape == lastProp);

            /*
             * Let tableShape be the shape with non-null table, either the one
             * we removed or the parent of lastProp.
             */
            const Shape *tableShape = shape->table ? shape : lastProp->parent;

            if (PropertyTable *table = tableShape->table) {
                /* Overwrite shape with newShape in the property table. */
                Shape **spp = table->search(shape->id, true);
                SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);

                /* Hand the table off from tableShape to newShape. */
                tableShape->setTable(NULL);
                newShape->setTable(table);
            }

            updateFlags(newShape);
            updateShape(cx);

            if (!js_UpdateWatchpointsForShape(cx, this, newShape)) {
                METER(wrapWatchFails);
                return NULL;
            }
        }
    } else if (shape == lastProp) {
        newShape = getChildProperty(cx, shape->parent, child);
#ifdef DEBUG
        if (newShape) {
            JS_ASSERT(newShape == lastProp);
            if (newShape->table) {
                Shape **spp = nativeSearch(shape->id);
                JS_ASSERT(SHAPE_FETCH(spp) == newShape);
            }
        }
#endif
    } else {
        /*
         * Let JSObject::putProperty handle this |overwriting| case, including
         * the conservation of shape->slot (if it's valid). We must not call
         * removeProperty because it will free an allocated shape->slot, and
         * putProperty won't re-allocate it.
         */
        newShape = putProperty(cx, child.id, child.rawGetter, child.rawSetter, child.slot,
                               child.attrs, child.flags, child.shortid);
#ifdef DEBUG
        if (newShape)
            METER(changePuts);
#endif
    }

#ifdef DEBUG
    CHECK_SHAPE_CONSISTENCY(this);
    if (newShape)
        METER(changes);
    else
        METER(changeFails);
#endif
    return newShape;
}

bool
JSObject::removeProperty(JSContext *cx, jsid id)
{
    Shape **spp = nativeSearch(id);
    Shape *shape = SHAPE_FETCH(spp);
    if (!shape) {
        METER(uselessRemoves);
        return true;
    }

    /* First, if shape is unshared and not has a slot, free its slot number. */
    bool hadSlot = !shape->isAlias() && shape->hasSlot();
    if (hadSlot) {
        freeSlot(cx, shape->slot);
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }


    /* If shape is not the last property added, switch to dictionary mode. */
    if (shape != lastProp && !inDictionaryMode()) {
        if (!toDictionaryMode(cx))
            return false;
        spp = nativeSearch(shape->id);
        shape = SHAPE_FETCH(spp);
    }

    /*
     * A dictionary-mode object owns mutable, unique shapes on a non-circular
     * doubly linked list, optionally hashed by lastProp->table. So we can edit
     * the list and hash in place.
     */
    if (inDictionaryMode()) {
        PropertyTable *table = lastProp->table;

        if (SHAPE_HAD_COLLISION(*spp)) {
            JS_ASSERT(table);
            *spp = SHAPE_REMOVED;
            ++table->removedCount;
            --table->entryCount;
        } else {
            METER(removeFrees);
            if (table) {
                *spp = NULL;
                --table->entryCount;

#ifdef DEBUG
                /*
                 * Check the consistency of the table but limit the number of
                 * checks not to alter significantly the complexity of the
                 * delete in debug builds, see bug 534493.
                 */
                const Shape *aprop = lastProp;
                for (int n = 50; --n >= 0 && aprop->parent; aprop = aprop->parent)
                    JS_ASSERT_IF(aprop != shape, nativeContains(*aprop));
#endif
            }
        }

        /*
         * Remove shape from its non-circular doubly linked list, setting this
         * object's OWN_SHAPE flag so the updateShape(cx) further below will
         * generate a fresh shape id for this object, distinct from the id of
         * any shape in the list. We need a fresh shape for all deletions, even
         * of lastProp. Otherwise, a shape number could replay and caches might
         * return get deleted DictionaryShapes! See bug 595365.
         */
        flags |= OWN_SHAPE;

        Shape *oldLastProp = lastProp;
        shape->removeFromDictionary(this);
        if (table) {
            if (shape == oldLastProp) {
                JS_ASSERT(shape->table == table);
                JS_ASSERT(shape->parent == lastProp);
                JS_ASSERT(shape->slotSpan >= lastProp->slotSpan);
                JS_ASSERT_IF(hadSlot, shape->slot + 1 <= shape->slotSpan);

                /*
                 * If the dictionary table's freelist is non-empty, we must
                 * preserve lastProp->slotSpan. We can't reduce slotSpan even
                 * by one or we might lose non-decreasing slotSpan order.
                 */
                if (table->freelist != SHAPE_INVALID_SLOT)
                    lastProp->slotSpan = shape->slotSpan;
            }

            /* Hand off table from old to new lastProp. */
            oldLastProp->setTable(NULL);
            lastProp->setTable(table);
        }
    } else {
        /*
         * Non-dictionary-mode property tables are shared immutables, so all we
         * need do is retract lastProp and we'll either get or else lazily make
         * via a later maybeHash the exact table for the new property lineage.
         */
        JS_ASSERT(shape == lastProp);
        removeLastProperty();

        /*
         * Revert to fixed slots if this was the first dynamically allocated slot,
         * preserving invariant that objects with the same shape use the fixed
         * slots in the same way.
         */
        size_t fixed = numFixedSlots();
        if (shape->slot == fixed) {
            JS_ASSERT_IF(!lastProp->isEmptyShape() && lastProp->hasSlot(),
                         lastProp->slot == fixed - 1);
            revertToFixedSlots(cx);
        }
    }
    updateShape(cx);

    /* On the way out, consider shrinking table if its load factor is <= .25. */
    if (PropertyTable *table = lastProp->table) {
        uint32 size = table->capacity();
        if (size > PropertyTable::MIN_SIZE && table->entryCount <= size >> 2) {
            METER(shrinks);
            (void) table->change(-1, cx);
        }
    }

    CHECK_SHAPE_CONSISTENCY(this);
    LIVE_SCOPE_METER(cx, --cx->runtime->liveObjectProps);
    METER(removes);
    return true;
}

void
JSObject::clear(JSContext *cx)
{
    LIVE_SCOPE_METER(cx, cx->runtime->liveObjectProps -= propertyCount());

    Shape *shape = lastProp;
    JS_ASSERT(inDictionaryMode() == shape->inDictionary());

    while (shape->parent) {
        shape = shape->parent;
        JS_ASSERT(inDictionaryMode() == shape->inDictionary());
    }
    JS_ASSERT(shape->isEmptyShape());

    if (inDictionaryMode())
        shape->listp = &lastProp;

    /*
     * Revert to fixed slots if we have cleared below the first dynamically
     * allocated slot, preserving invariant that objects with the same shape
     * use the fixed slots in the same way.
     */
    if (hasSlotsArray() && JSSLOT_FREE(getClass()) <= numFixedSlots())
        revertToFixedSlots(cx);

    /*
     * We have rewound to a uniquely-shaped empty scope, so we don't need an
     * override for this object's shape.
     */
    clearOwnShape();
    setMap(shape);

    LeaveTraceIfGlobalObject(cx, this);
    JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    CHECK_SHAPE_CONSISTENCY(this);
}

void
JSObject::generateOwnShape(JSContext *cx)
{
#ifdef JS_TRACER
    JS_ASSERT_IF(!parent && JS_ON_TRACE(cx), cx->bailExit);
     LeaveTraceIfGlobalObject(cx, this);

    /*
     * If we are recording, here is where we forget already-guarded shapes.
     * Any subsequent property operation upon object on the trace currently
     * being recorded will re-guard (and re-memoize).
     */
    TraceMonitor *tm = &JS_TRACE_MONITOR(cx);
    if (TraceRecorder *tr = tm->recorder)
        tr->forgetGuardedShapesForObject(this);
#endif

    setOwnShape(js_GenerateShape(cx, false));
}

void
JSObject::deletingShapeChange(JSContext *cx, const Shape &shape)
{
    JS_ASSERT(!JSID_IS_VOID(shape.id));
    generateOwnShape(cx);
}

bool
JSObject::methodShapeChange(JSContext *cx, const Shape &shape)
{
    JS_ASSERT(!JSID_IS_VOID(shape.id));
    if (shape.isMethod()) {
#ifdef DEBUG
        const Value &prev = nativeGetSlot(shape.slot);
        JS_ASSERT(&shape.methodObject() == &prev.toObject());
        JS_ASSERT(canHaveMethodBarrier());
        JS_ASSERT(hasMethodBarrier());
        JS_ASSERT(!shape.rawSetter || shape.rawSetter == js_watch_set);
#endif

        /*
         * Pass null to make a stub getter, but pass along shape.rawSetter to
         * preserve watchpoints. Clear Shape::METHOD from flags as we are
         * despecializing from a method memoized in the property tree to a
         * plain old function-valued property.
         */
        if (!putProperty(cx, shape.id, NULL, shape.rawSetter, shape.slot,
                         shape.attrs,
                         shape.getFlags() & ~Shape::METHOD,
                         shape.shortid)) {
            return false;
        }
    }

    generateOwnShape(cx);
    return true;
}

bool
JSObject::methodShapeChange(JSContext *cx, uint32 slot)
{
    if (!hasMethodBarrier()) {
        generateOwnShape(cx);
    } else {
        for (Shape::Range r = lastProp->all(); !r.empty(); r.popFront()) {
            const Shape &shape = r.front();
            JS_ASSERT(!JSID_IS_VOID(shape.id));
            if (shape.slot == slot)
                return methodShapeChange(cx, shape);
        }
    }
    return true;
}

void
JSObject::protoShapeChange(JSContext *cx)
{
    generateOwnShape(cx);
}

void
JSObject::shadowingShapeChange(JSContext *cx, const Shape &shape)
{
    JS_ASSERT(!JSID_IS_VOID(shape.id));
    generateOwnShape(cx);
}

bool
JSObject::globalObjectOwnShapeChange(JSContext *cx)
{
    generateOwnShape(cx);
    return !js_IsPropertyCacheDisabled(cx);
}

#ifdef DEBUG
static void
PrintPropertyGetterOrSetter(JSTracer *trc, char *buf, size_t bufsize)
{
    Shape *shape;
    jsid id;
    size_t n;
    const char *name;

    JS_ASSERT(trc->debugPrinter == PrintPropertyGetterOrSetter);
    shape = (Shape *)trc->debugPrintArg;
    id = shape->id;
    JS_ASSERT(!JSID_IS_VOID(id));
    name = trc->debugPrintIndex ? js_setter_str : js_getter_str;

    if (JSID_IS_ATOM(id)) {
        n = PutEscapedString(buf, bufsize - 1, JSID_TO_STRING(id), 0);
        if (n < bufsize - 1)
            JS_snprintf(buf + n, bufsize - n, " %s", name);
    } else if (JSID_IS_INT(shape->id)) {
        JS_snprintf(buf, bufsize, "%d %s", JSID_TO_INT(id), name);
    } else {
        JS_snprintf(buf, bufsize, "<object> %s", name);
    }
}

static void
PrintPropertyMethod(JSTracer *trc, char *buf, size_t bufsize)
{
    Shape *shape;
    jsid id;
    size_t n;

    JS_ASSERT(trc->debugPrinter == PrintPropertyMethod);
    shape = (Shape *)trc->debugPrintArg;
    id = shape->id;
    JS_ASSERT(!JSID_IS_VOID(id));

    JS_ASSERT(JSID_IS_ATOM(id));
    n = PutEscapedString(buf, bufsize - 1, JSID_TO_STRING(id), 0);
    if (n < bufsize - 1)
        JS_snprintf(buf + n, bufsize - n, " method");
}
#endif

void
Shape::trace(JSTracer *trc) const
{
    if (IS_GC_MARKING_TRACER(trc))
        mark();

    MarkId(trc, id, "id");

    if (attrs & (JSPROP_GETTER | JSPROP_SETTER)) {
        if ((attrs & JSPROP_GETTER) && rawGetter) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 0);
            Mark(trc, getterObject());
        }
        if ((attrs & JSPROP_SETTER) && rawSetter) {
            JS_SET_TRACING_DETAILS(trc, PrintPropertyGetterOrSetter, this, 1);
            Mark(trc, setterObject());
        }
    }

    if (isMethod()) {
        JS_SET_TRACING_DETAILS(trc, PrintPropertyMethod, this, 0);
        Mark(trc, &methodObject());
    }
}
