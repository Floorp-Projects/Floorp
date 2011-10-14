/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsscopeinlines_h___
#define jsscopeinlines_h___

#include <new>
#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsgc.h"

#include "vm/ArgumentsObject.h"
#include "vm/StringObject.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"

inline js::EmptyShape *
js::types::TypeObject::getEmptyShape(JSContext *cx, js::Class *aclasp, gc::AllocKind kind)
{
    JS_ASSERT(!singleton);

    /*
     * Empty shapes can only be on the default 'new' type for a prototype.
     * Objects with a common prototype use the same shape lineage, even if
     * their prototypes differ.
     */
    JS_ASSERT(proto->hasNewType(this));

    if (!emptyShapes) {
        emptyShapes = cx->new_<ShapeKindArray>();
        if (!emptyShapes)
            return NULL;

        /*
         * Always fill in the first empty shape, so canProvideEmptyShape works.
         * Other empty shapes are filled in lazily.
         */
        Shape *first = EmptyShape::create(cx, aclasp, proto->getParent(), 0);
        if (!first) {
            cx->delete_(emptyShapes);
            emptyShapes = NULL;
            return NULL;
        }
        emptyShapes->get(gc::FINALIZE_OBJECT0) = first;
    }

    JS_ASSERT(aclasp == emptyShapes->get(gc::FINALIZE_OBJECT0)->getObjectClass());

    Shape *&empty = emptyShapes->get(kind);
    if (!empty) {
        empty = EmptyShape::create(cx, aclasp, proto->getParent(),
                                   gc::GetGCKindSlots(kind, aclasp));
    }

    return static_cast<EmptyShape *>(empty);
}

inline bool
js::types::TypeObject::canProvideEmptyShape(js::Class *aclasp)
{
    return proto && !singleton &&
        (!emptyShapes || emptyShapes->get(gc::FINALIZE_OBJECT0)->getObjectClass() == aclasp);
}

inline void
JSObject::updateFlags(const js::Shape *shape, bool isDefinitelyAtom)
{
    jsuint index;
    if (!isDefinitelyAtom && js_IdIsIndex(shape->propid(), &index))
        setIndexed();
}

inline bool
JSObject::extend(JSContext *cx, const js::Shape *shape, bool isDefinitelyAtom)
{
    if (!setLastProperty(cx, shape))
        return false;
    updateFlags(shape, isDefinitelyAtom);
    return true;
}

namespace js {

inline bool
StringObject::init(JSContext *cx, JSString *str)
{
    JS_ASSERT(nativeEmpty());
    JS_ASSERT(getAllocKind() == gc::FINALIZE_OBJECT2);

    const js::Shape *shape = BaseShape::lookupInitialShape(cx, getClass(), getParent(),
                                                           gc::FINALIZE_OBJECT2, lastProperty());
    if (!shape)
        return false;
    if (shape != lastProperty()) {
        setLastPropertyInfallible(shape);
    } else {
        shape = assignInitialShape(cx);
        if (!shape)
            return false;
        BaseShape::insertInitialShape(cx, gc::FINALIZE_OBJECT2, shape);
    }
    JS_ASSERT(!nativeEmpty());
    JS_ASSERT(nativeLookup(cx, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom))->slot() == LENGTH_SLOT);

    setStringThis(str);
    return true;
}

inline void
BaseShape::adoptUnowned(UnownedBaseShape *other)
{
    /*
     * This is a base shape owned by a dictionary object, update it to reflect the
     * unowned base shape of a new last property.
     */
    JS_ASSERT(isOwned());
    JS_ASSERT(parent == other->parent);

    uint32 span = slotSpan();
    PropertyTable *table = &this->table();

    *this = *static_cast<BaseShape *>(other);
    setOwned(other);
    setTable(table);
    setSlotSpan(span);
}

inline
Shape::Shape(BaseShape *base, jsid propid, uint32 slot, uint32 nfixed,
             uintN attrs, uintN flags, intN shortid)
  : base_(base),
    propid_(propid),
    slotInfo(slot | (nfixed << FIXED_SLOTS_SHIFT)),
    attrs(uint8(attrs)),
    flags(uint8(flags)),
    shortid_(int16(shortid)),
    parent(NULL)
{
    JS_ASSERT(base);
    JS_ASSERT(!JSID_IS_VOID(propid));
    JS_ASSERT_IF(isMethod(), !base->rawGetter);
    kids.setNull();
}

inline
Shape::Shape(const Shape *other)
  : base_(other->base()->unowned()),
    propid_(other->maybePropid()),
    slotInfo(other->slotInfo & ~LINEAR_SEARCHES_MASK),
    attrs(other->attrs),
    flags(other->flags),
    shortid_(other->maybeShortid()),
    parent(NULL)
{
    kids.setNull();
}

inline
Shape::Shape(BaseShape *base, uint32 nfixed)
  : base_(base),
    propid_(JSID_EMPTY),
    slotInfo(SHAPE_INVALID_SLOT | (nfixed << FIXED_SLOTS_SHIFT)),
    attrs(JSPROP_SHARED),
    flags(0),
    shortid_(0),
    parent(NULL)
{
    JS_ASSERT(base);
    kids.setNull();
}

inline JSDHashNumber
Shape::hash() const
{
    JSDHashNumber hash = jsuword(base()->unowned());

    /* Accumulate from least to most random so the low bits are most random. */
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (flags & PUBLIC_FLAGS);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ attrs;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ shortid_;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ maybeSlot();
    hash = JS_ROTATE_LEFT32(hash, 4) ^ JSID_BITS(propid_);
    return hash;
}

inline bool
Shape::matches(const js::Shape *other) const
{
    return propid_ == other->propid_ &&
           matchesParamsAfterId(other->base(), other->maybeSlot(), other->attrs,
                                other->flags, other->shortid_);
}

inline bool
Shape::matchesParamsAfterId(BaseShape *base, uint32 aslot,
                            uintN aattrs, uintN aflags, intN ashortid) const
{
    return base->unowned() == this->base()->unowned() &&
           maybeSlot() == aslot &&
           attrs == aattrs &&
           ((flags ^ aflags) & PUBLIC_FLAGS) == 0 &&
           shortid_ == ashortid;
}

inline bool
Shape::get(JSContext* cx, JSObject *receiver, JSObject* obj, JSObject *pobj, js::Value* vp) const
{
    JS_ASSERT(!hasDefaultGetter());

    if (hasGetterValue()) {
        JS_ASSERT(!isMethod());
        js::Value fval = getterValue();
        return js::InvokeGetterOrSetter(cx, receiver, fval, 0, 0, vp);
    }

    if (isMethod()) {
        vp->setObject(*pobj->nativeGetMethod(this));
        return pobj->methodReadBarrier(cx, *this, vp);
    }

    /*
     * |with (it) color;| ends up here, as do XML filter-expressions.
     * Avoid exposing the With object to native getters.
     */
    if (obj->isWith())
        obj = js_UnwrapWithObject(cx, obj);
    return js::CallJSPropertyOp(cx, getterOp(), receiver, getUserId(), vp);
}

inline bool
Shape::set(JSContext* cx, JSObject* obj, bool strict, js::Value* vp) const
{
    JS_ASSERT_IF(hasDefaultSetter(), hasGetterValue());

    if (attrs & JSPROP_SETTER) {
        js::Value fval = setterValue();
        return js::InvokeGetterOrSetter(cx, obj, fval, 1, vp, vp);
    }

    if (attrs & JSPROP_GETTER)
        return js_ReportGetterOnlyAssignment(cx);

    /* See the comment in js::Shape::get as to why we check for With. */
    if (obj->isWith())
        obj = js_UnwrapWithObject(cx, obj);
    return js::CallJSPropertyOpSetter(cx, setterOp(), obj, getUserId(), strict, vp);
}

inline void
Shape::removeFromDictionary(JSObject *obj)
{
    JS_ASSERT(inDictionary());
    JS_ASSERT(obj->inDictionaryMode());
    JS_ASSERT(listp);

    JS_ASSERT(obj->shape_->inDictionary());
    JS_ASSERT(obj->shape_->listp == &obj->shape_);

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

    JS_ASSERT_IF(*dictp, (*dictp)->inDictionary());
    JS_ASSERT_IF(*dictp, (*dictp)->listp == dictp);
    JS_ASSERT_IF(*dictp, compartment() == (*dictp)->compartment());

    setParent(*dictp);
    if (parent)
        parent->listp = &parent;
    listp = dictp;
    *dictp = this;
}

void
Shape::initDictionaryShape(const Shape &child, Shape **dictp)
{
    UnownedBaseShape *base = child.base()->unowned();

    new (this) Shape(base, child.maybePropid(),
                     child.maybeSlot(), child.numFixedSlots(), child.attrs,
                     child.flags | IN_DICTIONARY, child.maybeShortid());

    this->listp = NULL;
    insertIntoDictionary(dictp);
}

inline
EmptyShape::EmptyShape(BaseShape *base, uint32 nfixed)
  : js::Shape(base, nfixed)
{
    /* Only empty shapes can be NON_NATIVE. */
    if (!getObjectClass()->isNative())
        flags |= NON_NATIVE;
}

} /* namespace js */

#endif /* jsscopeinlines_h___ */
