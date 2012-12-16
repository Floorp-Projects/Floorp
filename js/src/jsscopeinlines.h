/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsscopeinlines_h___
#define jsscopeinlines_h___

#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscope.h"

#include "gc/Marking.h"
#include "vm/ArgumentsObject.h"
#include "vm/ScopeObject.h"
#include "vm/StringObject.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "vm/ScopeObject-inl.h"

namespace js {

static inline void
GetterSetterWriteBarrierPost(JSCompartment *comp, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    comp->gcStoreBuffer.putRelocatableCell(reinterpret_cast<gc::Cell **>(objp));
#endif
}

static inline void
GetterSetterWriteBarrierPostRemove(JSCompartment *comp, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    comp->gcStoreBuffer.removeRelocatableCell(reinterpret_cast<gc::Cell **>(objp));
#endif
}

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
        GetterSetterWriteBarrierPost(compartment(), &this->getterObj);
    }
    if ((attrs & JSPROP_SETTER) && rawSetter) {
        this->flags |= HAS_SETTER_OBJECT;
        GetterSetterWriteBarrierPost(compartment(), &this->setterObj);
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
        GetterSetterWriteBarrierPost(compartment(), &this->getterObj);
    }
    if ((base.flags & HAS_SETTER_OBJECT) && base.rawSetter) {
        GetterSetterWriteBarrierPost(compartment(), &this->setterObj);
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
        GetterSetterWriteBarrierPost(compartment(), &getterObj);
    } else {
        rawGetter = other.rawGetter;
        GetterSetterWriteBarrierPostRemove(compartment(), &getterObj);
    }
    if (flags & HAS_SETTER_OBJECT) {
        setterObj = other.setterObj;
        GetterSetterWriteBarrierPost(compartment(), &setterObj);
    } else {
        rawSetter = other.rawSetter;
        GetterSetterWriteBarrierPostRemove(compartment(), &setterObj);
    }
    return *this;
}

inline bool
BaseShape::matchesGetterSetter(PropertyOp rawGetter, StrictPropertyOp rawSetter) const
{
    return rawGetter == this->rawGetter && rawSetter == this->rawSetter;
}

inline
StackBaseShape::StackBaseShape(UnrootedShape shape)
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
    if ((attrs & JSPROP_GETTER) && rawGetter) {
        JS_ASSERT(!IsPoisonedPtr(rawGetter));
        flags |= BaseShape::HAS_GETTER_OBJECT;
    }
    if ((attrs & JSPROP_SETTER) && rawSetter) {
        JS_ASSERT(!IsPoisonedPtr(rawSetter));
        flags |= BaseShape::HAS_SETTER_OBJECT;
    }

    this->rawGetter = rawGetter;
    this->rawSetter = rawSetter;
}

inline void
BaseShape::adoptUnowned(UnrootedUnownedBaseShape other)
{
    /*
     * This is a base shape owned by a dictionary object, update it to reflect the
     * unowned base shape of a new last property.
     */
    JS_ASSERT(isOwned());
    mozilla::DebugOnly<uint32_t> flags = getObjectFlags();
    JS_ASSERT((flags & other->getObjectFlags()) == flags);

    uint32_t span = slotSpan();
    ShapeTable *table = &this->table();

    *this = *other;
    setOwned(other);
    setTable(table);
    setSlotSpan(span);

    assertConsistency();
}

inline void
BaseShape::setOwned(UnrootedUnownedBaseShape unowned)
{
    flags |= OWNED_SHAPE;
    this->unowned_ = unowned;
}

inline void
BaseShape::assertConsistency()
{
#ifdef DEBUG
    if (isOwned()) {
        UnrootedUnownedBaseShape unowned = baseUnowned();
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
Shape::Shape(UnrootedUnownedBaseShape base, uint32_t nfixed)
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

inline HashNumber
StackShape::hash() const
{
    HashNumber hash = uintptr_t(base);

    /* Accumulate from least to most random so the low bits are most random. */
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (flags & Shape::PUBLIC_FLAGS);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ attrs;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ shortid;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ slot_;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ JSID_BITS(propid);
    return hash;
}

inline bool
Shape::matches(const UnrootedShape other) const
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
Shape::matchesParamsAfterId(UnrootedBaseShape base, uint32_t aslot,
                            unsigned aattrs, unsigned aflags, int ashortid) const
{
    return base->unowned() == this->base()->unowned() &&
           maybeSlot() == aslot &&
           attrs == aattrs &&
           ((flags ^ aflags) & PUBLIC_FLAGS) == 0 &&
           shortid_ == ashortid;
}

inline bool
Shape::getUserId(JSContext *cx, jsid *idp) const
{
    AssertCanGC();
    const Shape *self = this;
#ifdef DEBUG
    {
        SkipRoot skip(cx, &self);
        MaybeCheckStackRoots(cx);
    }
#endif
    if (self->hasShortID()) {
        int16_t id = self->shortid();
        if (id < 0)
            return ValueToId(cx, Int32Value(id), idp);
        *idp = INT_TO_JSID(id);
    } else {
        *idp = self->propid();
    }
    return true;
}

inline bool
Shape::get(JSContext* cx, HandleObject receiver, JSObject* obj, JSObject *pobj, MutableHandleValue vp)
{
    JS_ASSERT(!hasDefaultGetter());

    if (hasGetterValue()) {
        Value fval = getterValue();
        return InvokeGetterOrSetter(cx, receiver, fval, 0, 0, vp.address());
    }

    Rooted<Shape *> self(cx, this);
    RootedId id(cx);
    if (!self->getUserId(cx, id.address()))
        return false;

    return CallJSPropertyOp(cx, self->getterOp(), receiver, id, vp);
}

inline bool
Shape::set(JSContext* cx, HandleObject obj, HandleObject receiver, bool strict, MutableHandleValue vp)
{
    JS_ASSERT_IF(hasDefaultSetter(), hasGetterValue());

    if (attrs & JSPROP_SETTER) {
        Value fval = setterValue();
        return InvokeGetterOrSetter(cx, receiver, fval, 1, vp.address(), vp.address());
    }

    if (attrs & JSPROP_GETTER)
        return js_ReportGetterOnlyAssignment(cx);

    Rooted<Shape *> self(cx, this);
    RootedId id(cx);
    if (!self->getUserId(cx, id.address()))
        return false;

    /*
     * |with (it) color;| ends up here, as do XML filter-expressions.
     * Avoid exposing the With object to native setters.
     */
    if (obj->isWith()) {
        RootedObject nobj(cx, &obj->asWith().object());
        return CallJSPropertyOpSetter(cx, self->setterOp(), nobj, id, strict, vp);
    }

    return CallJSPropertyOpSetter(cx, self->setterOp(), obj, id, strict, vp);
}

inline void
Shape::setParent(UnrootedShape p)
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

    setParent(dictp->get());
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
EmptyShape::EmptyShape(UnrootedUnownedBaseShape base, uint32_t nfixed)
  : js::Shape(base, nfixed)
{
    /* Only empty shapes can be NON_NATIVE. */
    if (!getObjectClass()->isNative())
        flags |= NON_NATIVE;
}

inline void
Shape::writeBarrierPre(UnrootedShape shape)
{
#ifdef JSGC_INCREMENTAL
    if (!shape)
        return;

    JSCompartment *comp = shape->compartment();
    if (comp->needsBarrier()) {
        UnrootedShape tmp = shape;
        MarkShapeUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == shape);
    }
#endif
}

inline void
Shape::writeBarrierPost(UnrootedShape shape, void *addr)
{
}

inline void
Shape::readBarrier(UnrootedShape shape)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = shape->compartment();
    if (comp->needsBarrier()) {
        UnrootedShape tmp = shape;
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
BaseShape::writeBarrierPre(RawBaseShape base)
{
#ifdef JSGC_INCREMENTAL
    if (!base)
        return;

    JSCompartment *comp = base->compartment();
    if (comp->needsBarrier()) {
        RawBaseShape tmp = base;
        MarkBaseShapeUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == base);
    }
#endif
}

inline void
BaseShape::writeBarrierPost(RawBaseShape shape, void *addr)
{
}

inline void
BaseShape::readBarrier(RawBaseShape base)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = base->compartment();
    if (comp->needsBarrier()) {
        RawBaseShape tmp = base;
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
