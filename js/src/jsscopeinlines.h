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
#include "jsgcmark.h"

#include "vm/ArgumentsObject.h"
#include "vm/ScopeObject.h"
#include "vm/StringObject.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "vm/ScopeObject-inl.h"

namespace js {

inline
BaseShape::BaseShape(Class *clasp, JSObject *parent, uint32_t objectFlags)
{
    JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
    PodZero(this);
    this->clasp = clasp;
    this->parent = parent;
    this->flags = objectFlags;
}

inline
BaseShape::BaseShape(Class *clasp, JSObject *parent, uint32_t objectFlags,
                     uint8_t attrs, js::PropertyOp rawGetter, js::StrictPropertyOp rawSetter)
{
    JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
    PodZero(this);
    this->clasp = clasp;
    this->parent = parent;
    this->flags = objectFlags;
    this->rawGetter = rawGetter;
    this->rawSetter = rawSetter;
    if ((attrs & JSPROP_GETTER) && rawGetter) {
        this->flags |= HAS_GETTER_OBJECT;
        JSObject::writeBarrierPost(this->getterObj, &this->getterObj);
    }
    if ((attrs & JSPROP_SETTER) && rawSetter) {
        this->flags |= HAS_SETTER_OBJECT;
        JSObject::writeBarrierPost(this->setterObj, &this->setterObj);
    }
}

inline
BaseShape::BaseShape(const StackBaseShape &base)
{
    PodZero(this);
    this->clasp = base.clasp;
    this->parent = base.parent;
    this->flags = base.flags;
    this->rawGetter = base.rawGetter;
    this->rawSetter = base.rawSetter;
    if ((base.flags & HAS_GETTER_OBJECT) && base.rawGetter) {
        JSObject::writeBarrierPost(this->getterObj, &this->getterObj);
    }
    if ((base.flags & HAS_SETTER_OBJECT) && base.rawSetter) {
        JSObject::writeBarrierPost(this->setterObj, &this->setterObj);
    }
}

inline BaseShape &
BaseShape::operator=(const BaseShape &other)
{
    clasp = other.clasp;
    parent = other.parent;
    flags = other.flags;
    slotSpan_ = other.slotSpan_;
    if (flags & HAS_GETTER_OBJECT) {
        getterObj = other.getterObj;
        JSObject::writeBarrierPost(getterObj, &getterObj);
    } else {
        rawGetter = other.rawGetter;
    }
    if (flags & HAS_SETTER_OBJECT) {
        setterObj = other.setterObj;
        JSObject::writeBarrierPost(setterObj, &setterObj);
    } else {
        rawSetter = other.rawSetter;
    }
    return *this;
}

inline bool
BaseShape::matchesGetterSetter(PropertyOp rawGetter, StrictPropertyOp rawSetter) const
{
    return rawGetter == this->rawGetter && rawSetter == this->rawSetter;
}

inline
StackBaseShape::StackBaseShape(Shape *shape)
  : flags(shape->getObjectFlags()),
    clasp(shape->getObjectClass()),
    parent(shape->getObjectParent())
{
    updateGetterSetter(shape->attrs, shape->getter(), shape->setter());
}

inline void
StackBaseShape::updateGetterSetter(uint8_t attrs,
                                   PropertyOp rawGetter,
                                   StrictPropertyOp rawSetter)
{
    flags &= ~(BaseShape::HAS_GETTER_OBJECT | BaseShape::HAS_SETTER_OBJECT);
    if ((attrs & JSPROP_GETTER) && rawGetter)
        flags |= BaseShape::HAS_GETTER_OBJECT;
    if ((attrs & JSPROP_SETTER) && rawSetter)
        flags |= BaseShape::HAS_SETTER_OBJECT;

    this->rawGetter = rawGetter;
    this->rawSetter = rawSetter;
}

inline void
BaseShape::adoptUnowned(UnownedBaseShape *other)
{
    /*
     * This is a base shape owned by a dictionary object, update it to reflect the
     * unowned base shape of a new last property.
     */
    JS_ASSERT(isOwned());
    DebugOnly<uint32_t> flags = getObjectFlags();
    JS_ASSERT((flags & other->getObjectFlags()) == flags);

    uint32_t span = slotSpan();
    PropertyTable *table = &this->table();

    *this = *other;
    setOwned(other);
    setTable(table);
    setSlotSpan(span);

    assertConsistency();
}

inline void
BaseShape::setOwned(UnownedBaseShape *unowned)
{
    flags |= OWNED_SHAPE;
    this->unowned_ = unowned;
}

inline void
BaseShape::assertConsistency()
{
#ifdef DEBUG
    if (isOwned()) {
        UnownedBaseShape *unowned = baseUnowned();
        JS_ASSERT(hasGetterObject() == unowned->hasGetterObject());
        JS_ASSERT(hasSetterObject() == unowned->hasSetterObject());
        JS_ASSERT_IF(hasGetterObject(), getterObject() == unowned->getterObject());
        JS_ASSERT_IF(hasSetterObject(), setterObject() == unowned->setterObject());
        JS_ASSERT(getObjectParent() == unowned->getObjectParent());
        JS_ASSERT(getObjectFlags() == unowned->getObjectFlags());
    }
#endif
}

inline
Shape::Shape(const StackShape &other, uint32_t nfixed)
  : base_(other.base),
    propid_(other.propid),
    slotInfo(other.maybeSlot() | (nfixed << FIXED_SLOTS_SHIFT)),
    attrs(other.attrs),
    flags(other.flags),
    shortid_(other.shortid),
    parent(NULL)
{
    kids.setNull();
}

inline
Shape::Shape(UnownedBaseShape *base, uint32_t nfixed)
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
StackShape::hash() const
{
    JSDHashNumber hash = uintptr_t(base);

    /* Accumulate from least to most random so the low bits are most random. */
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (flags & Shape::PUBLIC_FLAGS);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ attrs;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ shortid;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ slot_;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ JSID_BITS(propid);
    return hash;
}

inline bool
Shape::matches(const js::Shape *other) const
{
    return propid_.get() == other->propid_.get() &&
           matchesParamsAfterId(other->base(), other->maybeSlot(), other->attrs,
                                other->flags, other->shortid_);
}

inline bool
Shape::matches(const StackShape &other) const
{
    return propid_.get() == other.propid &&
           matchesParamsAfterId(other.base, other.slot_, other.attrs, other.flags, other.shortid);
}

inline bool
Shape::matchesParamsAfterId(BaseShape *base, uint32_t aslot,
                            unsigned aattrs, unsigned aflags, int ashortid) const
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
        obj = &obj->asWith().object();
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
        obj = &obj->asWith().object();
    return js::CallJSPropertyOpSetter(cx, setterOp(), obj, getUserId(), strict, vp);
}

inline void
Shape::setParent(js::Shape *p)
{
    JS_ASSERT_IF(p && !p->hasMissingSlot() && !inDictionary(),
                 p->maybeSlot() <= maybeSlot());
    JS_ASSERT_IF(p && !inDictionary(),
                 hasSlot() == (p->maybeSlot() != maybeSlot()));
    parent = p;
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
Shape::insertIntoDictionary(HeapPtrShape *dictp)
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
    listp = (HeapPtrShape *) dictp;
    *dictp = this;
}

void
Shape::initDictionaryShape(const StackShape &child, uint32_t nfixed, HeapPtrShape *dictp)
{
    new (this) Shape(child, nfixed);
    this->flags |= IN_DICTIONARY;

    this->listp = NULL;
    insertIntoDictionary(dictp);
}

inline
EmptyShape::EmptyShape(UnownedBaseShape *base, uint32_t nfixed)
  : js::Shape(base, nfixed)
{
    /* Only empty shapes can be NON_NATIVE. */
    if (!getObjectClass()->isNative())
        flags |= NON_NATIVE;
}

inline void
Shape::writeBarrierPre(const js::Shape *shape)
{
#ifdef JSGC_INCREMENTAL
    if (!shape)
        return;

    JSCompartment *comp = shape->compartment();
    if (comp->needsBarrier()) {
        Shape *tmp = const_cast<Shape *>(shape);
        MarkShapeUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == shape);
    }
#endif
}

inline void
Shape::writeBarrierPost(const js::Shape *shape, void *addr)
{
}

inline void
Shape::readBarrier(const Shape *shape)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = shape->compartment();
    if (comp->needsBarrier()) {
        Shape *tmp = const_cast<Shape *>(shape);
        MarkShapeUnbarriered(comp->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == shape);
    }
#endif
}

inline void
Shape::markChildren(JSTracer *trc)
{
    MarkBaseShape(trc, &base_, "base");
    gc::MarkId(trc, &propidRef(), "propid");
    if (parent)
        MarkShape(trc, &parent, "parent");
}

inline void
BaseShape::writeBarrierPre(BaseShape *base)
{
#ifdef JSGC_INCREMENTAL
    if (!base)
        return;

    JSCompartment *comp = base->compartment();
    if (comp->needsBarrier()) {
        BaseShape *tmp = base;
        MarkBaseShapeUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == base);
    }
#endif
}

inline void
BaseShape::writeBarrierPost(BaseShape *shape, void *addr)
{
}

inline void
BaseShape::readBarrier(BaseShape *base)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = base->compartment();
    if (comp->needsBarrier()) {
        BaseShape *tmp = base;
        MarkBaseShapeUnbarriered(comp->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == base);
    }
#endif
}

inline void
BaseShape::markChildren(JSTracer *trc)
{
    if (hasGetterObject())
        MarkObjectUnbarriered(trc, &getterObj, "getter");

    if (hasSetterObject())
        MarkObjectUnbarriered(trc, &setterObj, "setter");

    if (isOwned())
        MarkBaseShape(trc, &unowned_, "base");

    if (parent)
        MarkObject(trc, &parent, "parent");
}

} /* namespace js */

#endif /* jsscopeinlines_h___ */
