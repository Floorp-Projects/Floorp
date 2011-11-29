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

#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;
using namespace js::gc;

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
    uint32 sizeLog2 = JS_CEILING_LOG2W(2 * entryCount);
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
    nbase->setOwned(base()->toUnowned());

    this->base_ = nbase;

    return true;
}

void
Shape::handoffTableTo(Shape *shape)
{
    JS_ASSERT(inDictionary() && shape->inDictionary());

    if (this == shape)
        return;

    JS_ASSERT(base()->isOwned() && !shape->base()->isOwned());

    BaseShape *nbase = base();

    JS_ASSERT_IF(shape->hasSlot(), nbase->slotSpan() > shape->slot());

    this->base_ = nbase->baseUnowned();
    nbase->adoptUnowned(shape->base()->toUnowned());

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
Shape::getChildBinding(JSContext *cx, const js::Shape &child, HeapPtrShape *lastBinding)
{
    JS_ASSERT(!inDictionary());
    JS_ASSERT(!child.inDictionary());

    Shape *shape = JS_PROPERTY_TREE(cx).getChild(cx, this, child);
    if (shape) {
        JS_ASSERT(shape->parent == this);
        JS_ASSERT(this == *lastBinding);
        *lastBinding = shape;

        /*
         * Update the number of fixed slots which bindings of this shape will
         * have. Bindings are constructed as new properties come in, so the
         * call object allocation class is not known ahead of time. Compute
         * the fixed slot count here, which will feed into call objects created
         * off of the bindings.
         */
        uint32 slots = child.slotSpan() + 1;  /* Add one for private data. */
        gc::AllocKind kind = gc::GetGCObjectKind(slots);

        /*
         * Make sure that the arguments and variables in the call object all
         * end up in a contiguous range of slots. We need this to be able to
         * embed the args/vars arrays in the TypeScriptNesting for the function
         * after the call object's frame has finished.
         */
        uint32 nfixed = gc::GetGCKindSlots(kind);
        if (nfixed < slots) {
            nfixed = CallObject::RESERVED_SLOTS + 1;
            JS_ASSERT(gc::GetGCKindSlots(gc::GetGCObjectKind(nfixed)) == CallObject::RESERVED_SLOTS + 1);
        }

        shape->setNumFixedSlots(nfixed - 1);
    }
    return shape;
}

/* static */ bool
Shape::replaceLastProperty(JSContext *cx, const BaseShape &base, JSObject *proto, HeapPtrShape *lastp)
{
    Shape *shape = *lastp;
    JS_ASSERT(!shape->inDictionary());

    if (!shape->parent) {
        /* Treat as resetting the initial property of the shape hierarchy. */
        AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
        Shape *newShape =
            EmptyShape::getInitialShape(cx, base.clasp, proto,
                                        base.parent, kind,
                                        base.flags & BaseShape::OBJECT_FLAG_MASK);
        if (!newShape)
            return false;
        JS_ASSERT(newShape->numFixedSlots() == shape->numFixedSlots());
        *lastp = newShape;
        return true;
    }

    BaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return false;

    Shape child(shape);
    child.base_ = nbase;

    Shape *newShape = JS_PROPERTY_TREE(cx).getChild(cx, shape->parent, child);
    if (!newShape)
        return false;

    *lastp = newShape;
    return true;
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
        child.setSlot(parent->maybeSlot());
    } else {
        if (child.hasMissingSlot()) {
            uint32 slot;
            if (!allocSlot(cx, &slot))
                return NULL;
            child.setSlot(slot);
        } else {
            /* Slots can only be allocated out of order on objects in dictionary mode. */
            JS_ASSERT(inDictionaryMode() ||
                      parent->hasMissingSlot() ||
                      child.slot() == parent->maybeSlot() + 1);
        }
    }

    Shape *shape;

    if (inDictionaryMode()) {
        JS_ASSERT(parent == lastProperty());
        shape = js_NewGCShape(cx);
        if (!shape)
            return NULL;
        if (child.hasSlot() && child.slot() >= lastProperty()->base()->slotSpan()) {
            if (!setSlotSpan(cx, child.slot() + 1))
                return NULL;
        }
        shape->initDictionaryShape(child, &shape_);
    } else {
        shape = JS_PROPERTY_TREE(cx).getChild(cx, parent, child);
        if (!shape)
            return NULL;
        JS_ASSERT(shape->parent == parent);
        JS_ASSERT_IF(parent != lastProperty(), parent == lastProperty()->parent);
        if (!setLastProperty(cx, shape))
            return NULL;
    }

    return shape;
}

Shape *
Shape::newDictionaryList(JSContext *cx, HeapPtrShape *listp)
{
    Shape *shape = *listp;
    Shape *list = shape;

    /*
     * We temporarily create the dictionary shapes using a root located on the
     * stack. This way, the GC doesn't see any intermediate state until we
     * switch listp at the end.
     */
    HeapPtrShape root(NULL);
    HeapPtrShape *childp = &root;

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

    /*
     * Clone the shapes into a new dictionary list. Don't update the
     * last property of this object until done, otherwise a GC
     * triggered while creating the dictionary will get the wrong
     * slot span for this object.
     */
    HeapPtrShape last;
    last.init(lastProperty());
    if (!Shape::newDictionaryList(cx, &last))
        return false;

    JS_ASSERT(last->listp == &last);
    last->listp = &shape_;
    shape_ = last;

    JS_ASSERT(lastProperty()->hasTable());
    lastProperty()->base()->setSlotSpan(span);

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

    Shape *shape = lastProperty();
    Shape *prev = NULL;

    if (inDictionaryMode()) {
        JS_ASSERT(shape->hasTable());

        PropertyTable &table = shape->table();
        for (uint32 fslot = table.freelist; fslot != SHAPE_INVALID_SLOT;
             fslot = getSlot(fslot).toPrivateUint32()) {
            JS_ASSERT(fslot < slotSpan());
        }

        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            JS_ASSERT_IF(shape != lastProperty(), !shape->hasTable());

            Shape **spp = table.search(shape->propid(), false);
            JS_ASSERT(SHAPE_FETCH(spp) == shape);
        }

        shape = lastProperty();
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            JS_ASSERT_IF(shape->slot() != SHAPE_INVALID_SLOT, shape->slot() < slotSpan());
            if (!prev) {
                JS_ASSERT(shape == lastProperty());
                JS_ASSERT(shape->listp == &shape_);
            } else {
                JS_ASSERT(shape->listp == &prev->parent);
            }
            prev = shape;
        }
    } else {
        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            if (shape->hasTable()) {
                PropertyTable &table = shape->table();
                JS_ASSERT(shape->parent);
                for (Shape::Range r(shape); !r.empty(); r.popFront()) {
                    Shape **spp = table.search(r.front().propid(), false);
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

Shape *
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

Shape *
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
            lastProperty()->hasMissingSlot() ||
            (slot == lastProperty()->maybeSlot() + 1);
        JS_ASSERT_IF(!allowDictionary, stableSlot);
        if (allowDictionary &&
            (!stableSlot || lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT)) {
            if (!toDictionaryMode(cx))
                return NULL;
            spp = nativeSearch(cx, id, true);
            table = &lastProperty()->table();
        }
    } else if (lastProperty()->hasTable()) {
        table = &lastProperty()->table();
        if (table->needsToGrow()) {
            if (!table->grow(cx))
                return NULL;

            spp = table->search(id, true);
            JS_ASSERT(!SHAPE_FETCH(spp));
        }
    }

    if (!maybeSetIndexed(cx, id))
        return NULL;

    /* Find or create a property tree node labeled by our arguments. */
    Shape *shape;
    {
        BaseShape *nbase;
        if (lastProperty()->base()->matchesGetterSetter(getter, setter)) {
            nbase = lastProperty()->base();
        } else {
            BaseShape base(getClass(), getParent(), lastProperty()->getObjectFlags(),
                           attrs, getter, setter);
            nbase = BaseShape::getUnowned(cx, base);
            if (!nbase)
                return NULL;
        }

        Shape child(nbase, id, slot, numFixedSlots(), attrs, flags, shortid);
        shape = getChildProperty(cx, lastProperty(), child);
    }

    if (shape) {
        JS_ASSERT(shape == lastProperty());

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SHAPE_STORE_PRESERVING_COLLISION(spp, shape);
            ++table->entryCount;

            /* Pass the table along to the new last property, namely shape. */
            JS_ASSERT(&shape->parent->table() == table);
            shape->parent->handoffTableTo(shape);
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

Shape *
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

    UnownedBaseShape *nbase;
    {
        BaseShape base(getClass(), getParent(), lastProperty()->getObjectFlags(),
                       attrs, getter, setter);
        nbase = BaseShape::getUnowned(cx, base);
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
    if (shape != lastProperty() && !inDictionaryMode()) {
        if (!toDictionaryMode(cx))
            return NULL;
        spp = nativeSearch(cx, shape->propid());
        shape = SHAPE_FETCH(spp);
    }

    JS_ASSERT_IF(shape->hasSlot() && !(attrs & JSPROP_SHARED), shape->slot() == slot);

    /*
     * Optimize the case of a dictionary-mode object based on the property that
     * dictionaries exclusively own their mutable shape structs, each of which
     * has a unique shape (not shared via a shape tree). We can update the
     * shape in place, though after each modification we need to generate a new
     * last property to invalidate shape guards.
     *
     * This is more than an optimization: it is required to preserve for-in
     * enumeration order (see bug 601399).
     */
    if (inDictionaryMode()) {
        bool updateLast = (shape == lastProperty());
        if (!generateOwnShape(cx))
            return NULL;
        if (updateLast)
            shape = lastProperty();

        /* FIXME bug 593129 -- slot allocation and JSObject *this must move out of here! */
        if (slot == SHAPE_INVALID_SLOT && !(attrs & JSPROP_SHARED)) {
            if (!allocSlot(cx, &slot))
                return NULL;
        }

        if (shape == lastProperty())
            shape->base()->adoptUnowned(nbase);
        else
            shape->base_ = nbase;

        shape->setSlot(slot);
        shape->attrs = uint8(attrs);
        shape->flags = flags | Shape::IN_DICTIONARY;
        shape->shortid_ = int16(shortid);

        /*
         * We are done updating shape and the last property. Now we may need to
         * update flags. In the last non-dictionary property case in the else
         * clause just below, getChildProperty handles this for us. First update
         * flags.
         */
        jsuint index;
        if (js_IdIsIndex(shape->propid(), &index))
            shape->base()->setObjectFlag(BaseShape::INDEXED);
    } else {
        /*
         * Updating the last property in a non-dictionary-mode object. Such
         * objects share their shapes via a tree rooted at a prototype
         * emptyShape, or perhaps a well-known compartment-wide singleton
         * emptyShape.
         *
         * If any shape in the tree has a property hashtable, it is shared and
         * immutable too, therefore we must not update *spp.
         */
        BaseShape base(getClass(), getParent(), lastProperty()->getObjectFlags(),
                       attrs, getter, setter);
        BaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return NULL;

        JS_ASSERT(shape == lastProperty());

        /* Find or create a property tree node labeled by our arguments. */
        Shape child(nbase, id, slot, numFixedSlots(), attrs, flags, shortid);
        Shape *newShape = getChildProperty(cx, shape->parent, child);

        if (!newShape) {
            CHECK_SHAPE_CONSISTENCY(this);
            return NULL;
        }

        shape = newShape;
    }

    /*
     * Can't fail now, so free the previous incarnation's slot if the new shape
     * has no slot. But we do not need to free oldSlot (and must not, as trying
     * to will botch an assertion in JSObject::freeSlot) if the new last
     * property (shape here) has a slotSpan that does not cover it.
     */
    if (hadSlot && !shape->hasSlot()) {
        if (oldSlot < slotSpan())
            freeSlot(cx, oldSlot);
        JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    }

    CHECK_SHAPE_CONSISTENCY(this);

    return shape;
}

Shape *
JSObject::changeProperty(JSContext *cx, Shape *shape, uintN attrs, uintN mask,
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
    Shape *newShape = putProperty(cx, shape->propid(), getter, setter, shape->maybeSlot(),
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

    /*
     * If shape is not the last property added, or the last property cannot
     * be removed, switch to dictionary mode.
     */
    if (!inDictionaryMode() && (shape != lastProperty() || !canRemoveLastProperty())) {
        if (!toDictionaryMode(cx))
            return false;
        spp = nativeSearch(cx, shape->propid());
        shape = SHAPE_FETCH(spp);
    }

    /*
     * If in dictionary mode, get a new shape for the last property after the
     * removal. We need a fresh shape for all dictionary deletions, even of
     * the last property. Otherwise, a shape could replay and caches might
     * return deleted DictionaryShapes! See bug 595365. Do this before changing
     * the object or table, so the remaining removal is infallible.
     */
    Shape *spare = NULL;
    if (inDictionaryMode()) {
        spare = js_NewGCShape(cx);
        if (!spare)
            return false;
        new (spare) Shape(shape->base(), 0);
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
        PropertyTable &table = lastProperty()->table();

        if (SHAPE_HAD_COLLISION(*spp)) {
            *spp = SHAPE_REMOVED;
            ++table.removedCount;
            --table.entryCount;
        } else {
            *spp = NULL;
            --table.entryCount;

#ifdef DEBUG
            /*
             * Check the consistency of the table but limit the number of
             * checks not to alter significantly the complexity of the
             * delete in debug builds, see bug 534493.
             */
            const Shape *aprop = lastProperty();
            for (int n = 50; --n >= 0 && aprop->parent; aprop = aprop->parent)
                JS_ASSERT_IF(aprop != shape, nativeContains(cx, *aprop));
#endif
        }

        /* Remove shape from its non-circular doubly linked list. */
        Shape *oldLastProp = lastProperty();
        shape->removeFromDictionary(this);

        /* Hand off table from the old to new last property. */
        oldLastProp->handoffTableTo(lastProperty());

        /* Generate a new shape for the object, infallibly. */
        JS_ALWAYS_TRUE(generateOwnShape(cx, spare));

        /* Consider shrinking table if its load factor is <= .25. */
        uint32 size = table.capacity();
        if (size > PropertyTable::MIN_SIZE && table.entryCount <= size >> 2)
            (void) table.change(-1, cx);
    } else {
        /*
         * Non-dictionary-mode property tables are shared immutables, so all we
         * need do is retract the last property and we'll either get or else
         * lazily make via a later hashify the exact table for the new property
         * lineage.
         */
        JS_ASSERT(shape == lastProperty());
        removeLastProperty(cx);
    }

    CHECK_SHAPE_CONSISTENCY(this);
    return true;
}

void
JSObject::clear(JSContext *cx)
{
    Shape *shape = lastProperty();
    JS_ASSERT(inDictionaryMode() == shape->inDictionary());

    while (shape->parent) {
        shape = shape->parent;
        JS_ASSERT(inDictionaryMode() == shape->inDictionary());
    }
    JS_ASSERT(shape->isEmptyShape());

    if (inDictionaryMode())
        shape->listp = &shape_;

    JS_ALWAYS_TRUE(setLastProperty(cx, shape));

    JS_ATOMIC_INCREMENT(&cx->runtime->propertyRemovals);
    CHECK_SHAPE_CONSISTENCY(this);
}

void
JSObject::rollbackProperties(JSContext *cx, uint32 slotSpan)
{
    /*
     * Remove properties from this object until it has a matching slot span.
     * The object cannot have escaped in a way which would prevent safe
     * removal of the last properties.
     */
    JS_ASSERT(!inDictionaryMode() && slotSpan <= this->slotSpan());
    while (this->slotSpan() != slotSpan) {
        JS_ASSERT(lastProperty()->hasSlot() && getSlot(lastProperty()->slot()).isUndefined());
        removeLastProperty(cx);
    }
}

bool
JSObject::generateOwnShape(JSContext *cx, Shape *newShape)
{
    if (!inDictionaryMode() && !toDictionaryMode(cx))
        return false;

    if (!newShape) {
        newShape = js_NewGCShape(cx);
        if (!newShape)
            return false;
        new (newShape) Shape(lastProperty()->base(), 0);
    }

    PropertyTable &table = lastProperty()->table();
    Shape **spp = lastProperty()->isEmptyShape() ? NULL : table.search(lastProperty()->maybePropid(), false);

    Shape *oldShape = lastProperty();
    newShape->initDictionaryShape(*oldShape, &shape_);

    JS_ASSERT(newShape->parent == oldShape);
    oldShape->removeFromDictionary(this);

    oldShape->handoffTableTo(newShape);

    if (spp)
        SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);
    return true;
}

Shape *
JSObject::methodShapeChange(JSContext *cx, const Shape &shape)
{
    JS_ASSERT(shape.isMethod());

    if (!inDictionaryMode() && !toDictionaryMode(cx))
        return NULL;

    Shape *spare = js_NewGCShape(cx);
    if (!spare)
        return NULL;
    new (spare) Shape(shape.base(), 0);

#ifdef DEBUG
    JS_ASSERT(canHaveMethodBarrier());
    JS_ASSERT(!shape.setter());
    JS_ASSERT(!shape.hasShortID());
#endif

    /*
     * Clear Shape::METHOD from flags as we are despecializing from a
     * method memoized in the property tree to a plain old function-valued
     * property.
     */
    Shape *result =
        putProperty(cx, shape.propid(), NULL, NULL, shape.slot(),
                    shape.attrs,
                    shape.getFlags() & ~Shape::METHOD,
                    0);
    if (!result)
        return NULL;

    if (result != lastProperty())
        JS_ALWAYS_TRUE(generateOwnShape(cx, spare));

    return result;
}

bool
JSObject::shadowingShapeChange(JSContext *cx, const Shape &shape)
{
    return generateOwnShape(cx);
}

bool
JSObject::clearParent(JSContext *cx)
{
    return setParent(cx, NULL);
}

bool
JSObject::setParent(JSContext *cx, JSObject *parent)
{
    if (parent && !parent->setDelegate(cx))
        return false;

    if (inDictionaryMode()) {
        lastProperty()->base()->setParent(parent);
        return true;
    }

    return Shape::setObjectParent(cx, parent, getProto(), &shape_);
}

/* static */ bool
Shape::setObjectParent(JSContext *cx, JSObject *parent, JSObject *proto, HeapPtrShape *listp)
{
    if ((*listp)->getObjectParent() == parent)
        return true;

    BaseShape base(*(*listp)->base()->unowned());
    base.setParent(parent);

    return replaceLastProperty(cx, base, proto, listp);
}

bool
JSObject::preventExtensions(JSContext *cx, js::AutoIdVector *props)
{
    JS_ASSERT(isExtensible());

    if (props) {
        if (js::FixOp fix = getOps()->fix) {
            bool success;
            if (!fix(cx, this, &success, props))
                return false;
            if (!success) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CHANGE_EXTENSIBILITY);
                return false;
            }
        } else {
            if (!js::GetPropertyNames(cx, this, JSITER_HIDDEN | JSITER_OWNONLY, props))
                return false;
        }
    }

    return setFlag(cx, BaseShape::NOT_EXTENSIBLE, GENERATE_SHAPE);
}

bool
JSObject::setFlag(JSContext *cx, /*BaseShape::Flag*/ uint32 flag_, GenerateShape generateShape)
{
    BaseShape::Flag flag = (BaseShape::Flag) flag_;

    if (lastProperty()->getObjectFlags() & flag)
        return true;

    if (inDictionaryMode()) {
        if (generateShape == GENERATE_SHAPE && !generateOwnShape(cx))
            return false;
        lastProperty()->base()->setObjectFlag(flag);
        return true;
    }

    return Shape::setObjectFlag(cx, flag, getProto(), &shape_);
}

/* static */ bool
Shape::setObjectFlag(JSContext *cx, BaseShape::Flag flag, JSObject *proto, HeapPtrShape *listp)
{
    if ((*listp)->getObjectFlags() & flag)
        return true;

    BaseShape base(*(*listp)->base()->unowned());
    base.flags |= flag;

    return replaceLastProperty(cx, base, proto, listp);
}

/* static */ inline HashNumber
BaseShapeEntry::hash(const js::BaseShape *base)
{
    JS_ASSERT(!base->isOwned());

    JSDHashNumber hash = base->flags;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (jsuword(base->clasp) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (jsuword(base->parent.get()) >> 3);
    if (base->rawGetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(base->rawGetter);
    if (base->rawSetter)
        hash = JS_ROTATE_LEFT32(hash, 4) ^ jsuword(base->rawSetter);
    return hash;
}

/* static */ inline bool
BaseShapeEntry::match(UnownedBaseShape *key, const BaseShape *lookup)
{
    JS_ASSERT(!lookup->isOwned());

    return key->flags == lookup->flags
        && key->clasp == lookup->clasp
        && key->parent == lookup->parent
        && key->getterObj == lookup->getterObj
        && key->setterObj == lookup->setterObj;
}

/* static */ UnownedBaseShape *
BaseShape::getUnowned(JSContext *cx, const BaseShape &base)
{
    BaseShapeSet &table = cx->compartment->baseShapes;

    if (!table.initialized() && !table.init())
        return NULL;

    BaseShapeSet::AddPtr p = table.lookupForAdd(&base);

    if (p) {
        UnownedBaseShape *base = *p;

        if (cx->compartment->needsBarrier())
            BaseShape::readBarrier(base);

        return base;
    }

    BaseShape *nbase_ = js_NewGCBaseShape(cx);
    if (!nbase_)
        return NULL;
    new (nbase_) BaseShape(base);

    UnownedBaseShape *nbase = static_cast<UnownedBaseShape *>(nbase_);

    if (!table.relookupOrAdd(p, &base, nbase))
        return NULL;

    return nbase;
}

void
JSCompartment::sweepBaseShapeTable(JSContext *cx)
{
    if (baseShapes.initialized()) {
        for (BaseShapeSet::Enum e(baseShapes); !e.empty(); e.popFront()) {
            UnownedBaseShape *base = e.front();
            if (!base->isMarked())
                e.removeFront();
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
Shape::setExtensibleParents(JSContext *cx, HeapPtrShape *listp)
{
    Shape *shape = *listp;
    JS_ASSERT(!shape->inDictionary());

    BaseShape base(*shape->base()->unowned());
    base.flags |= BaseShape::EXTENSIBLE_PARENTS;

    /* This is only used for Block and Call objects, which have a NULL proto. */
    return replaceLastProperty(cx, base, NULL, listp);
}

bool
Bindings::setExtensibleParents(JSContext *cx)
{
    if (!ensureShape(cx))
        return false;
    return Shape::setExtensibleParents(cx, &lastBinding);
}

bool
Bindings::setParent(JSContext *cx, JSObject *obj)
{
    if (!ensureShape(cx))
        return false;

    /* This is only used for Block objects, which have a NULL proto. */
    return Shape::setObjectParent(cx, obj, NULL, &lastBinding);
}

/* static */ inline HashNumber
InitialShapeEntry::hash(const Lookup &lookup)
{
    JSDHashNumber hash = jsuword(lookup.clasp) >> 3;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (jsuword(lookup.proto) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (jsuword(lookup.parent) >> 3);
    return hash + lookup.nfixed;
}

/* static */ inline bool
InitialShapeEntry::match(const InitialShapeEntry &key, const Lookup &lookup)
{
    return lookup.clasp == key.shape->getObjectClass()
        && lookup.proto == key.proto
        && lookup.parent == key.shape->getObjectParent()
        && lookup.nfixed == key.shape->numFixedSlots()
        && lookup.baseFlags == key.shape->getObjectFlags();
}

/* static */ Shape *
EmptyShape::getInitialShape(JSContext *cx, Class *clasp, JSObject *proto, JSObject *parent,
                            AllocKind kind, uint32 objectFlags)
{
    InitialShapeSet &table = cx->compartment->initialShapes;

    if (!table.initialized() && !table.init())
        return NULL;

    size_t nfixed = GetGCKindSlots(kind, clasp);
    InitialShapeEntry::Lookup lookup(clasp, proto, parent, nfixed, objectFlags);

    InitialShapeSet::AddPtr p = table.lookupForAdd(lookup);

    if (p) {
        Shape *shape = p->shape;

        if (cx->compartment->needsBarrier())
            Shape::readBarrier(shape);

        return shape;
    }

    BaseShape base(clasp, parent, objectFlags);
    BaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return NULL;

    Shape *shape = JS_PROPERTY_TREE(cx).newShape(cx);
    if (!shape)
        return NULL;
    new (shape) EmptyShape(nbase, nfixed);

    InitialShapeEntry entry;
    entry.shape = shape;
    entry.proto = proto;

    if (!table.relookupOrAdd(p, lookup, entry))
        return NULL;

    return shape;
}

void
NewObjectCache::invalidateEntriesForShape(JSContext *cx, Shape *shape, JSObject *proto)
{
    Class *clasp = shape->getObjectClass();

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    GlobalObject *global = shape->getObjectParent()->getGlobal();
    types::TypeObject *type = proto->getNewType(cx);

    EntryIndex entry;
    if (lookupGlobal(clasp, global, kind, &entry))
        PodZero(&entries[entry]);
    if (!proto->isGlobal() && lookupProto(clasp, proto, kind, &entry))
        PodZero(&entries[entry]);
    if (lookupType(clasp, type, kind, &entry))
        PodZero(&entries[entry]);
}

/* static */ void
EmptyShape::insertInitialShape(JSContext *cx, Shape *shape, JSObject *proto)
{
    InitialShapeEntry::Lookup lookup(shape->getObjectClass(), proto, shape->getObjectParent(),
                                     shape->numFixedSlots(), shape->getObjectFlags());

    InitialShapeSet::Ptr p = cx->compartment->initialShapes.lookup(lookup);
    JS_ASSERT(p);

    InitialShapeEntry &entry = const_cast<InitialShapeEntry &>(*p);
    JS_ASSERT(entry.shape->isEmptyShape());

    /* The new shape had better be rooted at the old one. */
#ifdef DEBUG
    const Shape *nshape = shape;
    while (!nshape->isEmptyShape())
        nshape = nshape->previous();
    JS_ASSERT(nshape == entry.shape);
#endif

    entry.shape = shape;

    /*
     * This affects the shape that will be produced by the various NewObject
     * methods, so clear any cache entry referring to the old shape. This is
     * not required for correctness (though it may bust on the above asserts):
     * the NewObject must always check for a nativeEmpty() result and generate
     * the appropriate properties if found. Clearing the cache entry avoids
     * this duplicate regeneration.
     */
    cx->compartment->newObjectCache.invalidateEntriesForShape(cx, shape, proto);
}

void
JSCompartment::sweepInitialShapeTable(JSContext *cx)
{
    if (initialShapes.initialized()) {
        for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
            const InitialShapeEntry &entry = e.front();
            if (!entry.shape->isMarked() || (entry.proto && !entry.proto->isMarked()))
                e.removeFront();
        }
    }
}
