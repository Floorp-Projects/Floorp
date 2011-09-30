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
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstracer.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;
using namespace js::gc;

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

bool
PropertyTable::init(JSRuntime *rt, Shape *lastProp)
{
    /*
     * Either we're creating a table for a large scope that was populated
     * via property cache hit logic under JSOP_INITPROP, JSOP_SETNAME, or
     * JSOP_SETPROP; or else calloc failed at least once already. In any
     * event, let's try to grow, overallocating to hold at least twice the
     * current population.
     */
    uint32 sizeLog2 = JS_CeilingLog2(2 * entryCount);
    if (sizeLog2 < MIN_SIZE_LOG2)
        sizeLog2 = MIN_SIZE_LOG2;

    /*
     * Use rt->calloc_ for memory accounting and overpressure handling
     * without OOM reporting. See PropertyTable::change.
     */
    entries = (Shape **) rt->calloc_(sizeOfEntries(JS_BIT(sizeLog2)));
    if (!entries)
        return false;

    hashShift = JS_DHASH_BITS - sizeLog2;
    for (Shape::Range r = lastProp->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        Shape **spp = search(shape.propid(), true);

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
Shape::makeOwnBaseShape(JSContext *cx)
{
    JS_ASSERT(!base()->isOwned());

    BaseShape *nbase = js_NewGCBaseShape(cx);
    if (!nbase)
        return false;

    new (nbase) BaseShape(*base());
    nbase->setOwned(base());

    this->base_ = nbase;

    return true;
}

void
Shape::handoffTable(Shape *shape)
{
    JS_ASSERT(inDictionary() && shape->inDictionary());

    if (this == shape)
        return;

    JS_ASSERT(base()->isOwned() && !shape->base()->isOwned());

    BaseShape *nbase = base();

    /* Update the slot span when growing dictionaries. */
    uint32 span = nbase->slotSpan;
    if (shape->hasSlot())
        span = JS_MAX(span, shape->slot() + 1);

    PropertyTable *table = nbase->table();
    JS_ASSERT(table);

    this->base_ = nbase->base;

    new (nbase) BaseShape(shape->base(), table, span);

    shape->base_ = nbase;
}

bool
Shape::hashify(JSContext *cx)
{
    JS_ASSERT(!hasTable());

    if (!ensureOwnBaseShape(cx))
        return false;

    JSRuntime *rt = cx->runtime;
    PropertyTable *table = rt->new_<PropertyTable>(entryCount());
    if (!table)
        return false;

    if (!table->init(rt, this)) {
        rt->free_(table);
        return false;
    }

    base()->setTable(table);
    return true;
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
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
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /* Compute the primary hash address. */
    hash0 = HashId(id);
    hash1 = HASH1(hash0, hashShift);
    spp = entries + hash1;

    /* Miss: return space for a new entry. */
    stored = *spp;
    if (SHAPE_IS_FREE(stored))
        return spp;

    /* Hit: return entry. */
    shape = SHAPE_CLEAR_COLLISION(stored);
    if (shape && shape->propid() == id)
        return spp;

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
        hash1 -= hash2;
        hash1 &= sizeMask;
        spp = entries + hash1;

        stored = *spp;
        if (SHAPE_IS_FREE(stored))
            return (adding && firstRemoved) ? firstRemoved : spp;

        shape = SHAPE_CLEAR_COLLISION(stored);
        if (shape && shape->propid() == id) {
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
    JS_ASSERT(entries);

    /*
     * Grow, shrink, or compress by changing this->entries.
     */
    int oldlog2 = JS_DHASH_BITS - hashShift;
    int newlog2 = oldlog2 + log2Delta;
    uint32 oldsize = JS_BIT(oldlog2);
    uint32 newsize = JS_BIT(newlog2);
    Shape **newTable = (Shape **) cx->calloc_(sizeOfEntries(newsize));
    if (!newTable)
        return false;

    /* Now that we have newTable allocated, update members. */
    hashShift = JS_DHASH_BITS - newlog2;
    removedCount = 0;
    Shape **oldTable = entries;
    entries = newTable;

    /* Copy only live entries, leaving removed and free ones behind. */
    for (Shape **oldspp = oldTable; oldsize != 0; oldspp++) {
        Shape *shape = SHAPE_FETCH(oldspp);
        if (shape) {
            Shape **spp = search(shape->propid(), true);
            JS_ASSERT(SHAPE_IS_FREE(*spp));
            *spp = shape;
        }
        oldsize--;
    }

    /* Finally, free the old entries storage. */
    cx->free_(oldTable);
    return true;
}

bool
PropertyTable::grow(JSContext *cx)
{
    JS_ASSERT(needsToGrow());

    uint32 size = capacity();
    int delta = removedCount < size >> 2;

    if (!change(delta, cx) && entryCount + removedCount == size - 1) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

Shape *
Shape::getChild(JSContext *cx, const js::Shape &child, Shape **listp, bool allowDictionary)
{
    JS_ASSERT(!child.inDictionary());

    if (inDictionary()) {
        Shape *oldShape = *listp;
        PropertyTable *table = oldShape->getTable();

        /*
         * Attempt to grow table if needed before extending *listp, rather than
         * risking OOM under table->grow after initDictionaryShape, and then
         * have to fix up *listp.
         */
        if (table->needsToGrow() && !table->grow(cx))
            return NULL;

        Shape *newShape = js_NewGCShape(cx);
        if (!newShape)
            return NULL;

        newShape->initDictionaryShape(child, listp);

        JS_ASSERT(oldShape == newShape->parent);

        /* Add newShape to the property table. */
        Shape **spp = table->search(newShape->propid(), true);

        /*
         * Beware duplicate formal parameters, allowed by ECMA-262 in
         * non-strict mode. Otherwise we know that Bindings::add (our caller)
         * won't pass an id already in the table to us. In the case of
         * duplicate formals, the last one wins, so while we must not overcount
         * entries, we must store newShape.
         */
        if (!SHAPE_FETCH(spp))
            ++table->entryCount;
        SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);

        /* Hand the table off from oldShape to newShape. */
        oldShape->handoffTable(newShape);

        return newShape;
    }

    if (allowDictionary && (*listp)->entryCount() >= PropertyTree::MAX_HEIGHT) {
        Shape *dprop = Shape::newDictionaryList(cx, listp);
        if (!dprop)
            return NULL;
        return dprop->getChild(cx, child, listp);
    }

    Shape *shape = JS_PROPERTY_TREE(cx).getChild(cx, this, child);
    if (shape) {
        JS_ASSERT(shape->parent == this);
        JS_ASSERT(this == *listp);
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
    /*
     * Shared properties have no slot, but slot_ will reflect that of parent.
     * Unshared properties allocate a slot here but may lose it due to a
     * JS_ClearScope call.
     */
    if (!child.hasSlot()) {
        child.slot_ = parent->maybeSlot();
    } else {
        if (child.hasMissingSlot()) {
            uint32 slot;
            if (!allocSlot(cx, &slot))
                return NULL;
            child.slot_ = slot;
        } else {
            /* Slots can only be allocated out of order on objects in dictionary mode. */
            JS_ASSERT(inDictionaryMode() ||
                      lastProp->hasMissingSlot() ||
                      child.slot() == lastProp->slot() + 1);
        }
    }

    Shape *shape;

    if (inDictionaryMode()) {
        JS_ASSERT(parent == lastProp);
        shape = js_NewGCShape(cx);
        if (!shape)
            return NULL;
        shape->initDictionaryShape(child, &lastProp);
    } else {
        shape = JS_PROPERTY_TREE(cx).getChild(cx, parent, child);
        if (!shape)
            return NULL;
        JS_ASSERT(shape->parent == parent);
        JS_ASSERT_IF(parent != lastProp, parent == lastProp->parent);
        setLastProperty(shape);
    }

    updateFlags(shape);
    return shape;
}

Shape *
Shape::newDictionaryList(JSContext *cx, Shape **listp)
{
    Shape *shape = *listp;
    Shape *list = shape;

    /*
     * We temporarily create the dictionary shapes using a root located on the
     * stack. This way, the GC doesn't see any intermediate state until we
     * switch listp at the end.
     */
    Shape *root = NULL;
    Shape **childp = &root;

    while (shape) {
        JS_ASSERT(!shape->inDictionary());

        Shape *dprop = js_NewGCShape(cx);
        if (!dprop) {
            *listp = list;
            return NULL;
        }
        dprop->initDictionaryShape(*shape, childp);

        JS_ASSERT(!dprop->hasTable());
        childp = &dprop->parent;
        shape = shape->parent;
    }

    *listp = root;
    root->listp = listp;

    JS_ASSERT(root->inDictionary());
    root->hashify(cx);
    return root;
}

bool
JSObject::toDictionaryMode(JSContext *cx)
{
    JS_ASSERT(!inDictionaryMode());

    /* We allocate the shapes from cx->compartment, so make sure it's right. */
    JS_ASSERT(compartment() == cx->compartment);

    uint32 span = slotSpan();

    if (!Shape::newDictionaryList(cx, &lastProp))
        return false;

    JS_ASSERT(lastProp->hasTable());
    lastProp->base()->slotSpan = span;

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
                         StrictPropertyOp &setter)
{
    if (setter == JS_StrictPropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_SETTER));
        setter = NULL;
    }
    if (flags & Shape::METHOD) {
        JS_ASSERT_IF(getter, getter == JS_PropertyStub);
        JS_ASSERT(!setter);
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        getter = NULL;
    } else {
        if (getter == JS_PropertyStub) {
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

    Shape *shape = lastProp;
    Shape *prev = NULL;

    if (inDictionaryMode()) {
        JS_ASSERT(shape->hasTable());

        PropertyTable *table = shape->getTable();
        for (uint32 fslot = table->freelist; fslot != SHAPE_INVALID_SLOT;
             fslot = getSlot(fslot).toPrivateUint32()) {
            JS_ASSERT(fslot < slotSpan());
        }

        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            JS_ASSERT_IF(shape != lastProp, !shape->hasTable());

            Shape **spp = table->search(shape->propid(), false);
            JS_ASSERT(SHAPE_FETCH(spp) == shape);
        }

        shape = lastProp;
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            JS_ASSERT_IF(shape->slot() != SHAPE_INVALID_SLOT, shape->slot() < slotSpan());
            if (!prev) {
                JS_ASSERT(shape == lastProp);
                JS_ASSERT(shape->listp == &lastProp);
            } else {
                JS_ASSERT(shape->listp == &prev->parent);
            }
            prev = shape;
        }
    } else {
        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            if (shape->hasTable()) {
                PropertyTable *table = shape->getTable();
                JS_ASSERT(shape->parent);
                for (Shape::Range r(shape); !r.empty(); r.popFront()) {
                    Shape **spp = table->search(r.front().propid(), false);
                    JS_ASSERT(SHAPE_FETCH(spp) == &r.front());
                }
            }
            if (prev) {
                JS_ASSERT(prev->maybeSlot() >= shape->maybeSlot());
                shape->kids.checkConsistency(prev);
            }
            prev = shape;
        }
    }
}
#else
# define CHECK_SHAPE_CONSISTENCY(obj) ((void)0)
#endif

const Shape *
JSObject::addProperty(JSContext *cx, jsid id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32 slot, uintN attrs,
                      uintN flags, intN shortid, bool allowDictionary)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    if (!isExtensible()) {
        reportNotExtensible(cx);
        return NULL;
    }

    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /* Search for id with adding = true in order to claim its entry. */
    Shape **spp = nativeSearch(cx, id, true);
    JS_ASSERT(!SHAPE_FETCH(spp));
    return addPropertyInternal(cx, id, getter, setter, slot, attrs, flags, shortid, spp, allowDictionary);
}

const Shape *
JSObject::addPropertyInternal(JSContext *cx, jsid id,
                              PropertyOp getter, StrictPropertyOp setter,
                              uint32 slot, uintN attrs,
                              uintN flags, intN shortid,
                              Shape **spp, bool allowDictionary)
{
    JS_ASSERT_IF(!allowDictionary, !inDictionaryMode());

    PropertyTable *table = NULL;
    if (!inDictionaryMode()) {
        bool stableSlot =
            (slot == SHAPE_INVALID_SLOT) ||
            lastProp->hasMissingSlot() ||
            (slot == lastProp->maybeSlot() + 1);
        JS_ASSERT_IF(!allowDictionary, stableSlot);
        if (allowDictionary &&
            (!stableSlot || lastProp->entryCount() >= PropertyTree::MAX_HEIGHT)) {
            if (!toDictionaryMode(cx))
                return NULL;
            spp = nativeSearch(cx, id, true);
            table = lastProp->getTable();
        }
    } else if (lastProp->hasTable()) {
        table = lastProp->getTable();
        if (table->needsToGrow()) {
            if (!table->grow(cx))
                return NULL;

            spp = table->search(id, true);
            JS_ASSERT(!SHAPE_FETCH(spp));
        }
    }

    /* Find or create a property tree node labeled by our arguments. */
    Shape *shape;
    {
        BaseShape base(getClass(), attrs, getter, setter);
        BaseShape *nbase = BaseShape::lookup(cx, base);
        if (!nbase)
            return NULL;

        Shape child(nbase, id, slot, attrs, flags, shortid);
        shape = getChildProperty(cx, lastProp, child);
    }

    if (shape) {
        JS_ASSERT(shape == lastProp);

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SHAPE_STORE_PRESERVING_COLLISION(spp, shape);
            ++table->entryCount;

            /* Pass the table along to the new lastProp, namely shape. */
            JS_ASSERT(shape->parent->getTable() == table);
            shape->parent->handoffTable(shape);
        }

        CHECK_SHAPE_CONSISTENCY(this);
        return shape;
    }

    CHECK_SHAPE_CONSISTENCY(this);
    return NULL;
}

/*
 * Check and adjust the new attributes for the shape to make sure that our
 * slot access optimizations are sound. It is responsibility of the callers to
 * enforce all restrictions from ECMA-262 v5 8.12.9 [[DefineOwnProperty]].
 */
inline bool
CheckCanChangeAttrs(JSContext *cx, JSObject *obj, const Shape *shape, uintN *attrsp)
{
    if (shape->configurable())
        return true;

    /* A permanent property must stay permanent. */
    *attrsp |= JSPROP_PERMANENT;

    /* Reject attempts to remove a slot from the permanent data property. */
    if (shape->isDataDescriptor() && shape->hasSlot() &&
        (*attrsp & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED))) {
        obj->reportNotConfigurable(cx, shape->propid());
        return false;
    }

    return true;
}

const Shape *
JSObject::putProperty(JSContext *cx, jsid id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32 slot, uintN attrs,
                      uintN flags, intN shortid)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    NormalizeGetterAndSetter(cx, this, id, attrs, flags, getter, setter);

    /* Search for id in order to claim its entry if table has been allocated. */
    Shape **spp = nativeSearch(cx, id, true);
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

        return addPropertyInternal(cx, id, getter, setter, slot, attrs, flags, shortid, spp, true);
    }

    /* Property exists: search must have returned a valid *spp. */
    JS_ASSERT(!SHAPE_IS_REMOVED(*spp));

    if (!CheckCanChangeAttrs(cx, this, shape, &attrs))
        return NULL;
    
    /*
     * If the caller wants to allocate a slot, but doesn't care which slot,
     * copy the existing shape's slot into slot so we can match shape, if all
     * other members match.
     */
    bool hadSlot = shape->hasSlot();
    uint32 oldSlot = shape->maybeSlot();
    if (!(attrs & JSPROP_SHARED) && slot == SHAPE_INVALID_SLOT && hadSlot)
        slot = oldSlot;

    BaseShape *nbase;
    {
        BaseShape base(getClass(), attrs, getter, setter);
        nbase = BaseShape::lookup(cx, base);
        if (!nbase)
            return NULL;
    }

    /*
     * Now that we've possibly preserved slot, check whether all members match.
     * If so, this is a redundant "put" and we can return without more work.
     */
    if (shape->matchesParamsAfterId(nbase, slot, attrs, flags, shortid))
        return shape;

    /*
     * Overwriting a non-last property requires switching to dictionary mode.
     * The shape tree is shared immutable, and we can't removeProperty and then
     * addPropertyInternal because a failure under add would lose data.
     */
    if (shape != lastProp && !inDictionaryMode()) {
        if (!toDictionaryMode(cx))
            return NULL;
        spp = nativeSearch(cx, shape->propid());
        shape = SHAPE_FETCH(spp);
    }

    JS_ASSERT_IF(shape->hasSlot() && !(attrs & JSPROP_SHARED), shape->slot() == slot);

    /*
     * Now that we have passed the lastProp->frozen() check at the top of this
     * method, and the non-last-property conditioning just above, we are ready
     * to overwrite.
     *
     * Optimize the case of a non-frozen dictionary-mode object based on the
     * property that dictionaries exclusively own their mutable shape structs,
     * each of which has a unique shape (not shared via a shape tree).
     *
     * This is more than an optimization: it is required to preserve for-in
     * enumeration order (see bug 601399).
     */
    if (inDictionaryMode()) {
        bool updateLast = (shape == lastProp);
        if (!generateOwnShape(cx))
            return NULL;
        if (updateLast)
            shape = lastProp;

        /* FIXME bug 593129 -- slot allocation and JSObject *this must move out of here! */
        if (slot == SHAPE_INVALID_SLOT && !(attrs & JSPROP_SHARED)) {
            if (!allocSlot(cx, &slot))
                return NULL;
        }

        if (shape == lastProp) {
            uint32 span = shape->base()->slotSpan;
            PropertyTable *table = shape->base()->table();
            new (shape->base()) BaseShape(nbase, table, span);
        } else {
            shape->base_ = nbase;
        }

        shape->slot_ = slot;
        shape->attrs = uint8(attrs);
        shape->flags = flags | Shape::IN_DICTIONARY;
        shape->shortid_ = int16(shortid);

        /*
         * We are done updating shape and lastProp. Now we may need to update
         * flags and we will need to update objShape, which is no longer "own".
         * In the last non-dictionary property case in the else clause just
         * below, getChildProperty handles this for us. First update flags.
         */
        updateFlags(shape);
    } else {
        /*
         * Updating lastProp in a non-dictionary-mode object. Such objects
         * share their shapes via a tree rooted at a prototype emptyShape, or
         * perhaps a well-known compartment-wide singleton emptyShape.
         *
         * If any shape in the tree has a property hashtable, it is shared and
         * immutable too, therefore we must not update *spp.
         */
        BaseShape base(getClass(), attrs, getter, setter);
        BaseShape *nbase = BaseShape::lookup(cx, base);
        if (!nbase)
            return NULL;

        JS_ASSERT(shape == lastProp);
        removeLastProperty();

        /* Find or create a property tree node labeled by our arguments. */
        Shape child(nbase, id, slot, attrs, flags, shortid);

        Shape *newShape = getChildProperty(cx, lastProp, child);
        if (!newShape) {
            setLastProperty(shape);
            CHECK_SHAPE_CONSISTENCY(this);
            return NULL;
        }

        shape = newShape;
    }

    /*
     * Can't fail now, so free the previous incarnation's slot if the new shape
     * has no slot. But we do not need to free oldSlot (and must not, as trying
     * to will botch an assertion in JSObject::freeSlot) if the new lastProp
     * (shape here) has a slotSpan that does not cover it.
     */
    if (hadSlot && !shape->hasSlot()) {
        if (oldSlot < slotSpan())
            freeSlot(cx, oldSlot);
        else
            setSlot(oldSlot, UndefinedValue());
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }

    CHECK_SHAPE_CONSISTENCY(this);

    return shape;
}

const Shape *
JSObject::changeProperty(JSContext *cx, const Shape *shape, uintN attrs, uintN mask,
                         PropertyOp getter, StrictPropertyOp setter)
{
    JS_ASSERT(nativeContains(cx, *shape));

    attrs |= shape->attrs & mask;

    /* Allow only shared (slotless) => unshared (slotful) transition. */
    JS_ASSERT(!((attrs ^ shape->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    /* Don't allow method properties to be changed to have a getter or setter. */
    JS_ASSERT_IF(shape->isMethod(), !getter && !setter);

    types::MarkTypePropertyConfigured(cx, this, shape->propid());
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
        types::AddTypePropertyId(cx, this, shape->propid(), types::Type::UnknownType());

    if (getter == JS_PropertyStub)
        getter = NULL;
    if (setter == JS_StrictPropertyStub)
        setter = NULL;

    if (!CheckCanChangeAttrs(cx, this, shape, &attrs))
        return NULL;
    
    if (shape->attrs == attrs && shape->getter() == getter && shape->setter() == setter)
        return shape;

    /*
     * Let JSObject::putProperty handle this |overwriting| case, including
     * the conservation of shape->slot (if it's valid). We must not call
     * removeProperty because it will free an allocated shape->slot, and
     * putProperty won't re-allocate it.
     */
    const Shape *newShape = putProperty(cx, shape->propid(), getter, setter, shape->maybeSlot(),
                                        attrs, shape->flags, shape->maybeShortid());

    CHECK_SHAPE_CONSISTENCY(this);
    return newShape;
}

bool
JSObject::removeProperty(JSContext *cx, jsid id)
{
    Shape **spp = nativeSearch(cx, id);
    Shape *shape = SHAPE_FETCH(spp);
    if (!shape)
        return true;

    /* If shape is not the last property added, switch to dictionary mode. */
    if (shape != lastProp && !inDictionaryMode()) {
        if (!toDictionaryMode(cx))
            return false;
        spp = nativeSearch(cx, shape->propid());
        shape = SHAPE_FETCH(spp);
    }

    /*
     * If in dictionary mode, get a new shape for the last property after the
     * removal. We need a fresh shape for all dictionary deletions, even of
     * lastProp. Otherwise, a shape number could replay and caches might
     * return deleted DictionaryShapes! See bug 595365. Do this before changing
     * the object or table, so the remaining removal is infallible.
     */
    Shape *spare = NULL;
    if (inDictionaryMode()) {
        spare = js_NewGCShape(cx);
        if (!spare)
            return false;
        PodZero(spare);
    }

    /* If shape has a slot, free its slot number. */
    if (shape->hasSlot()) {
        freeSlot(cx, shape->slot());
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }

    /*
     * A dictionary-mode object owns mutable, unique shapes on a non-circular
     * doubly linked list, hashed by lastProp->table. So we can edit the list
     * and hash in place.
     */
    if (inDictionaryMode()) {
        PropertyTable *table = lastProp->getTable();

        if (SHAPE_HAD_COLLISION(*spp)) {
            *spp = SHAPE_REMOVED;
            ++table->removedCount;
            --table->entryCount;
        } else {
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
                JS_ASSERT_IF(aprop != shape, nativeContains(cx, *aprop));
#endif
        }

        /* Remove shape from its non-circular doubly linked list. */
        Shape *oldLastProp = lastProp;
        shape->removeFromDictionary(this);

        /* Hand off table from old to new lastProp. */
        oldLastProp->handoffTable(lastProp);

        /* Generate a new shape for the object, infallibly. */
        JS_ALWAYS_TRUE(generateOwnShape(cx, spare));

        /* Consider shrinking table if its load factor is <= .25. */
        uint32 size = table->capacity();
        if (size > PropertyTable::MIN_SIZE && table->entryCount <= size >> 2)
            (void) table->change(-1, cx);
    } else {
        /*
         * Non-dictionary-mode property tables are shared immutables, so all we
         * need do is retract lastProp and we'll either get or else lazily make
         * via a later hashify the exact table for the new property lineage.
         */
        JS_ASSERT(shape == lastProp);
        removeLastProperty();
    }

    /* Consider shrinking object slots if 25% or more are unused. */
    if (hasSlotsArray()) {
        JS_ASSERT(slotSpan() <= numSlots());
        if ((slotSpan() + (slotSpan() >> 2)) < numSlots())
            shrinkSlots(cx, slotSpan());
    }

    CHECK_SHAPE_CONSISTENCY(this);
    return true;
}

void
JSObject::clear(JSContext *cx)
{
    Shape *shape = lastProp;
    JS_ASSERT(inDictionaryMode() == shape->inDictionary());

    while (shape->parent) {
        shape = shape->parent;
        JS_ASSERT(inDictionaryMode() == shape->inDictionary());
    }
    JS_ASSERT(shape->isEmptyShape());

    if (inDictionaryMode())
        shape->listp = &lastProp;

    setMap(shape);

    LeaveTraceIfGlobalObject(cx, this);
    JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    CHECK_SHAPE_CONSISTENCY(this);
}

void
JSObject::rollbackProperties(JSContext *cx, uint32 slotSpan)
{
    /* Remove properties from this object until it has a matching slot span. */
    JS_ASSERT(!inDictionaryMode() && !hasSlotsArray() && slotSpan <= this->slotSpan());
    while (this->slotSpan() != slotSpan) {
        JS_ASSERT(lastProp->hasSlot() && getSlot(lastProp->slot()).isUndefined());
        removeLastProperty();
    }
}

bool
JSObject::generateOwnShape(JSContext *cx, Shape *newShape)
{
#ifdef JS_TRACER
    JS_ASSERT_IF(!parent && JS_ON_TRACE(cx), JS_TRACE_MONITOR_ON_TRACE(cx)->bailExit);
    LeaveTraceIfGlobalObject(cx, this);

    /*
     * If we are recording, here is where we forget already-guarded shapes.
     * Any subsequent property operation upon object on the trace currently
     * being recorded will re-guard (and re-memoize).
     */
    if (TraceRecorder *tr = TRACE_RECORDER(cx))
        tr->forgetGuardedShapesForObject(this);
#endif

    if (!inDictionaryMode() && !toDictionaryMode(cx))
        return false;

    if (!newShape) {
        newShape = js_NewGCShape(cx);
        if (!newShape)
            return false;
    }

    PropertyTable *table = lastProp->getTable();
    Shape **spp = lastProp->isEmptyShape() ? NULL : table->search(lastProp->maybePropid(), false);

    Shape *oldShape = lastProp;
    newShape->initDictionaryShape(*oldShape, &lastProp);

    JS_ASSERT(newShape->parent == oldShape);
    oldShape->removeFromDictionary(this);

    oldShape->handoffTable(newShape);

    if (spp) {
        if (SHAPE_HAD_COLLISION(*spp))
            SHAPE_FLAG_COLLISION(spp, newShape);
        else
            *spp = newShape;
    }
    return true;
}

const Shape *
JSObject::methodShapeChange(JSContext *cx, const Shape &shape)
{
    const Shape *result = &shape;

    if (!inDictionaryMode() && !toDictionaryMode(cx))
        return NULL;

    Shape *spare = js_NewGCShape(cx);
    if (!spare)
        return NULL;

    if (shape.isMethod()) {
#ifdef DEBUG
        JS_ASSERT(canHaveMethodBarrier());
        JS_ASSERT(!shape.setter());
        JS_ASSERT(!shape.hasShortID());
#endif

        /*
         * Pass null to make a stub getter, but pass along shape.rawSetter to
         * preserve watchpoints. Clear Shape::METHOD from flags as we are
         * despecializing from a method memoized in the property tree to a
         * plain old function-valued property.
         */
        result = putProperty(cx, shape.propid(), NULL, NULL, shape.slot(),
                             shape.attrs,
                             shape.getFlags() & ~Shape::METHOD,
                             0);
        if (!result)
            return NULL;
    }

    JS_ALWAYS_TRUE(generateOwnShape(cx, spare));
    return result;
}

bool
JSObject::protoShapeChange(JSContext *cx)
{
    return generateOwnShape(cx);
}

bool
JSObject::shadowingShapeChange(JSContext *cx, const Shape &shape)
{
    return generateOwnShape(cx);
}

/* static */ inline HashNumber
JSCompartment::BaseShapeEntry::hash(const js::BaseShape *base)
{
    JS_ASSERT(!base->isOwned() && !base->table());

    JSDHashNumber hash = base->flags;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(base->clasp);
    if (base->rawGetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(base->rawGetter);
    if (base->rawSetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(base->rawSetter);
    return hash;
}

/* static */ inline bool
JSCompartment::BaseShapeEntry::match(const BaseShapeEntry &entry, const BaseShape *lookup)
{
    BaseShape *key = entry.base;
    JS_ASSERT(!key->isOwned() && !lookup->isOwned());

    return key->flags == lookup->flags
        && key->clasp == lookup->clasp
        && key->getterObj == lookup->getterObj
        && key->setterObj == lookup->setterObj;
}

static inline JSCompartment::BaseShapeEntry *
LookupBaseShape(JSContext *cx, const BaseShape &base)
{
    JSCompartment::BaseShapeSet &table = cx->compartment->baseShapes;

    if (!table.initialized() && !table.init())
        return false;

    JSCompartment::BaseShapeSet::AddPtr p = table.lookupForAdd(&base);
    if (p)
        return &const_cast<JSCompartment::BaseShapeEntry &>(*p);

    BaseShape *nbase = js_NewGCBaseShape(cx);
    if (!nbase)
        return false;
    new (nbase) BaseShape(base);

    JSCompartment::BaseShapeEntry entry;
    entry.base = nbase;
    entry.empty = NULL;

    p = table.lookupForAdd(&base);
    if (!table.add(p, entry))
        return NULL;

    return &const_cast<JSCompartment::BaseShapeEntry &>(*p);
}

/* static */ BaseShape *
BaseShape::lookup(JSContext *cx, const BaseShape &base)
{
    JSCompartment::BaseShapeEntry *entry = LookupBaseShape(cx, base);
    return entry ? entry->base : NULL;
}

/* static */ EmptyShape *
BaseShape::lookupEmpty(JSContext *cx, Class *clasp)
{
    js::BaseShape base(clasp);
    JSCompartment::BaseShapeEntry *entry = LookupBaseShape(cx, base);
    if (!entry)
        return NULL;
    if (entry->empty)
        return entry->empty;

    entry->empty = (EmptyShape *) JS_PROPERTY_TREE(cx).newShape(cx);
    if (!entry->empty)
        return NULL;
    return new (entry->empty) EmptyShape(entry->base);
}

void
JSCompartment::sweepBaseShapeTable(JSContext *cx)
{
    if (baseShapes.initialized()) {
        for (BaseShapeSet::Enum e(baseShapes); !e.empty(); e.popFront()) {
            JSCompartment::BaseShapeEntry &entry =
                const_cast<JSCompartment::BaseShapeEntry &>(e.front());
            if (!entry.base->isMarked())
                e.removeFront();
            else if (entry.empty && !entry.empty->isMarked())
                entry.empty = NULL;
        }
    }
}

void
BaseShape::finalize(JSContext *cx, bool background)
{
    if (table_) {
        cx->delete_(table_);
        table_ = NULL;
    }
}

/* static */ bool
Shape::setExtensibleParents(JSContext *cx, Shape **pshape)
{
    Shape *shape = *pshape;
    JS_ASSERT(!shape->inDictionary());

    BaseShape base(shape->getClass(), shape->attrs, shape->getter(), shape->setter());
    base.flags |= BaseShape::EXTENSIBLE_PARENTS;
    BaseShape *nbase = BaseShape::lookup(cx, base);
    if (!nbase)
        return false;

    Shape child(nbase, shape->maybePropid(), shape->maybeSlot(),
                shape->attrs, shape->flags, shape->maybeShortid());
    Shape *newShape;
    if (shape->parent) {
        newShape = JS_PROPERTY_TREE(cx).getChild(cx, shape->parent, child);
        if (!newShape)
            return false;
    } else {
        newShape = js_NewGCShape(cx);
        if (!newShape)
            return false;
        new (newShape) Shape(child);
    }

    *pshape = newShape;

    return true;
}

bool
Bindings::setExtensibleParents(JSContext *cx)
{
    if (!ensureShape(cx))
        return false;
    return Shape::setExtensibleParents(cx, &lastBinding);
}
