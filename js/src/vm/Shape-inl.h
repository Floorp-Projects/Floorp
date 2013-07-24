/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Shape_inl_h
#define vm_Shape_inl_h

#include "vm/Shape.h"

#include "mozilla/PodOperations.h"

#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"

#include "gc/Marking.h"
#include "vm/ArgumentsObject.h"
#include "vm/ScopeObject.h"
#include "vm/StringObject.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsgcinlines.h"

namespace js {

static inline void
GetterSetterWriteBarrierPost(JSRuntime *rt, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    rt->gcStoreBuffer.putRelocatableCell(reinterpret_cast<gc::Cell **>(objp));
#endif
}

static inline void
GetterSetterWriteBarrierPostRemove(JSRuntime *rt, JSObject **objp)
{
#ifdef JSGC_GENERATIONAL
    rt->gcStoreBuffer.removeRelocatableCell(reinterpret_cast<gc::Cell **>(objp));
#endif
}

inline
BaseShape::BaseShape(JSCompartment *comp, Class *clasp, JSObject *parent, JSObject *metadata,
                     uint32_t objectFlags)
{
    JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
    mozilla::PodZero(this);
    this->clasp = clasp;
    this->parent = parent;
    this->metadata = metadata;
    this->flags = objectFlags;
    this->compartment_ = comp;
}

inline
BaseShape::BaseShape(JSCompartment *comp, Class *clasp, JSObject *parent, JSObject *metadata,
                     uint32_t objectFlags, uint8_t attrs,
                     PropertyOp rawGetter, StrictPropertyOp rawSetter)
{
    JS_ASSERT(!(objectFlags & ~OBJECT_FLAG_MASK));
    mozilla::PodZero(this);
    this->clasp = clasp;
    this->parent = parent;
    this->metadata = metadata;
    this->flags = objectFlags;
    this->rawGetter = rawGetter;
    this->rawSetter = rawSetter;
    if ((attrs & JSPROP_GETTER) && rawGetter) {
        this->flags |= HAS_GETTER_OBJECT;
        GetterSetterWriteBarrierPost(runtime(), &this->getterObj);
    }
    if ((attrs & JSPROP_SETTER) && rawSetter) {
        this->flags |= HAS_SETTER_OBJECT;
        GetterSetterWriteBarrierPost(runtime(), &this->setterObj);
    }
    this->compartment_ = comp;
}

inline
BaseShape::BaseShape(const StackBaseShape &base)
{
    mozilla::PodZero(this);
    this->clasp = base.clasp;
    this->parent = base.parent;
    this->metadata = base.metadata;
    this->flags = base.flags;
    this->rawGetter = base.rawGetter;
    this->rawSetter = base.rawSetter;
    if ((base.flags & HAS_GETTER_OBJECT) && base.rawGetter)
        GetterSetterWriteBarrierPost(runtime(), &this->getterObj);
    if ((base.flags & HAS_SETTER_OBJECT) && base.rawSetter)
        GetterSetterWriteBarrierPost(runtime(), &this->setterObj);
    this->compartment_ = base.compartment;
}

inline BaseShape &
BaseShape::operator=(const BaseShape &other)
{
    clasp = other.clasp;
    parent = other.parent;
    metadata = other.metadata;
    flags = other.flags;
    slotSpan_ = other.slotSpan_;
    if (flags & HAS_GETTER_OBJECT) {
        getterObj = other.getterObj;
        GetterSetterWriteBarrierPost(runtime(), &getterObj);
    } else {
        if (rawGetter)
            GetterSetterWriteBarrierPostRemove(runtime(), &getterObj);
        rawGetter = other.rawGetter;
    }
    if (flags & HAS_SETTER_OBJECT) {
        setterObj = other.setterObj;
        GetterSetterWriteBarrierPost(runtime(), &setterObj);
    } else {
        if (rawSetter)
            GetterSetterWriteBarrierPostRemove(runtime(), &setterObj);
        rawSetter = other.rawSetter;
    }
    compartment_ = other.compartment_;
    return *this;
}

inline
StackBaseShape::StackBaseShape(ExclusiveContext *cx, Class *clasp,
                               JSObject *parent, JSObject *metadata, uint32_t objectFlags)
  : flags(objectFlags),
    clasp(clasp),
    parent(parent),
    metadata(metadata),
    rawGetter(NULL),
    rawSetter(NULL),
    compartment(cx->compartment_)
{}

inline
StackBaseShape::StackBaseShape(Shape *shape)
  : flags(shape->getObjectFlags()),
    clasp(shape->getObjectClass()),
    parent(shape->getObjectParent()),
    metadata(shape->getObjectMetadata()),
    compartment(shape->compartment())
{
    updateGetterSetter(shape->attrs, shape->getter(), shape->setter());
}

inline void
BaseShape::adoptUnowned(UnownedBaseShape *other)
{
    /*
     * This is a base shape owned by a dictionary object, update it to reflect the
     * unowned base shape of a new last property.
     */
    JS_ASSERT(isOwned());

    uint32_t span = slotSpan();
    ShapeTable *table = &this->table();

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
        JS_ASSERT(getObjectMetadata() == unowned->getObjectMetadata());
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

inline bool
Shape::matches(const StackShape &other) const
{
    return propid_.get() == other.propid &&
           matchesParamsAfterId(other.base, other.slot_, other.attrs, other.flags, other.shortid);
}

inline bool
Shape::getUserId(JSContext *cx, MutableHandleId idp) const
{
    const Shape *self = this;
#ifdef DEBUG
    {
        SkipRoot skip(cx, &self);
        MaybeCheckStackRoots(cx);
    }
#endif
    if (self->hasShortID()) {
        int16_t id = self->shortid();
        if (id < 0) {
            RootedValue v(cx, Int32Value(id));
            return ValueToId<CanGC>(cx, v, idp);
        }
        idp.set(INT_TO_JSID(id));
    } else {
        idp.set(self->propid());
    }
    return true;
}

inline bool
Shape::get(JSContext* cx, HandleObject receiver, JSObject* obj, JSObject *pobj,
           MutableHandleValue vp)
{
    JS_ASSERT(!hasDefaultGetter());

    if (hasGetterValue()) {
        Value fval = getterValue();
        return InvokeGetterOrSetter(cx, receiver, fval, 0, 0, vp);
    }

    Rooted<Shape *> self(cx, this);
    RootedId id(cx);
    if (!self->getUserId(cx, &id))
        return false;

    return CallJSPropertyOp(cx, self->getterOp(), receiver, id, vp);
}

inline bool
Shape::set(JSContext* cx, HandleObject obj, HandleObject receiver, bool strict,
           MutableHandleValue vp)
{
    JS_ASSERT_IF(hasDefaultSetter(), hasGetterValue());

    if (attrs & JSPROP_SETTER) {
        Value fval = setterValue();
        return InvokeGetterOrSetter(cx, receiver, fval, 1, vp.address(), vp);
    }

    if (attrs & JSPROP_GETTER)
        return js_ReportGetterOnlyAssignment(cx, strict);

    Rooted<Shape *> self(cx, this);
    RootedId id(cx);
    if (!self->getUserId(cx, &id))
        return false;

    /*
     * |with (it) color='red';| ends up here.
     * Avoid exposing the With object to native setters.
     */
    if (obj->is<WithObject>()) {
        RootedObject nobj(cx, &obj->as<WithObject>().object());
        return CallJSPropertyOpSetter(cx, self->setterOp(), nobj, id, strict, vp);
    }

    return CallJSPropertyOpSetter(cx, self->setterOp(), obj, id, strict, vp);
}

inline void
Shape::setParent(Shape *p)
{
    JS_ASSERT_IF(p && !p->hasMissingSlot() && !inDictionary(),
                 p->maybeSlot() <= maybeSlot());
    JS_ASSERT_IF(p && !inDictionary(),
                 hasSlot() == (p->maybeSlot() != maybeSlot()));
    parent = p;
}

inline void
Shape::removeFromDictionary(ObjectImpl *obj)
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
EmptyShape::EmptyShape(UnownedBaseShape *base, uint32_t nfixed)
  : js::Shape(base, nfixed)
{
    /* Only empty shapes can be NON_NATIVE. */
    if (!getObjectClass()->isNative())
        flags |= NON_NATIVE;
}

inline void
Shape::writeBarrierPre(Shape *shape)
{
#ifdef JSGC_INCREMENTAL
    if (!shape || !shape->runtime()->needsBarrier())
        return;

    JS::Zone *zone = shape->zone();
    if (zone->needsBarrier()) {
        Shape *tmp = shape;
        MarkShapeUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == shape);
    }
#endif
}

inline void
Shape::readBarrier(Shape *shape)
{
#ifdef JSGC_INCREMENTAL
    JS::Zone *zone = shape->zone();
    if (zone->needsBarrier()) {
        Shape *tmp = shape;
        MarkShapeUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
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
    if (!base || !base->runtime()->needsBarrier())
        return;

    JS::Zone *zone = base->zone();
    if (zone->needsBarrier()) {
        BaseShape *tmp = base;
        MarkBaseShapeUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == base);
    }
#endif
}

inline void
BaseShape::readBarrier(BaseShape *base)
{
#ifdef JSGC_INCREMENTAL
    JS::Zone *zone = base->zone();
    if (zone->needsBarrier()) {
        BaseShape *tmp = base;
        MarkBaseShapeUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
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

    if (metadata)
        MarkObject(trc, &metadata, "metadata");
}

inline
AutoRooterGetterSetter::Inner::Inner(ExclusiveContext *cx, uint8_t attrs,
                                     PropertyOp *pgetter_, StrictPropertyOp *psetter_)
  : CustomAutoRooter(cx), attrs(attrs),
    pgetter(pgetter_), psetter(psetter_),
    getterRoot(cx, pgetter_), setterRoot(cx, psetter_)
{
    JS_ASSERT_IF(attrs & JSPROP_GETTER, !IsPoisonedPtr(*pgetter));
    JS_ASSERT_IF(attrs & JSPROP_SETTER, !IsPoisonedPtr(*psetter));
}

inline
AutoRooterGetterSetter::AutoRooterGetterSetter(ExclusiveContext *cx, uint8_t attrs,
                                               PropertyOp *pgetter, StrictPropertyOp *psetter
                                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
{
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
        inner.construct(cx, attrs, pgetter, psetter);
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

inline
StackBaseShape::AutoRooter::AutoRooter(ExclusiveContext *cx, const StackBaseShape *base_
                                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : CustomAutoRooter(cx), base(base_), skip(cx, base_)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

inline
StackShape::AutoRooter::AutoRooter(ExclusiveContext *cx, const StackShape *shape_
                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : CustomAutoRooter(cx), shape(shape_), skip(cx, shape_)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
}

} /* namespace js */

#endif /* vm_Shape_inl_h */
