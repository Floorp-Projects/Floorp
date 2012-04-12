/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include "jsscope.h"
#include "jsobjinlines.h"

#include "ObjectImpl.h"

#include "gc/Barrier-inl.h"

#include "ObjectImpl-inl.h"

using namespace js;

static ObjectElements emptyElementsHeader(0, 0);

/* Objects with no elements share one empty set of elements. */
HeapSlot *js::emptyObjectElements =
    reinterpret_cast<HeapSlot *>(uintptr_t(&emptyElementsHeader) + sizeof(ObjectElements));

#ifdef DEBUG
void
js::ObjectImpl::checkShapeConsistency()
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

    MOZ_ASSERT(isNative());

    Shape *shape = lastProperty();
    Shape *prev = NULL;

    if (inDictionaryMode()) {
        MOZ_ASSERT(shape->hasTable());

        PropertyTable &table = shape->table();
        for (uint32_t fslot = table.freelist; fslot != SHAPE_INVALID_SLOT;
             fslot = getSlot(fslot).toPrivateUint32()) {
            MOZ_ASSERT(fslot < slotSpan());
        }

        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            MOZ_ASSERT_IF(shape != lastProperty(), !shape->hasTable());

            Shape **spp = table.search(shape->propid(), false);
            MOZ_ASSERT(SHAPE_FETCH(spp) == shape);
        }

        shape = lastProperty();
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            MOZ_ASSERT_IF(shape->slot() != SHAPE_INVALID_SLOT, shape->slot() < slotSpan());
            if (!prev) {
                MOZ_ASSERT(shape == lastProperty());
                MOZ_ASSERT(shape->listp == &shape_);
            } else {
                MOZ_ASSERT(shape->listp == &prev->parent);
            }
            prev = shape;
        }
    } else {
        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            if (shape->hasTable()) {
                PropertyTable &table = shape->table();
                MOZ_ASSERT(shape->parent);
                for (Shape::Range r(shape); !r.empty(); r.popFront()) {
                    Shape **spp = table.search(r.front().propid(), false);
                    MOZ_ASSERT(SHAPE_FETCH(spp) == &r.front());
                }
            }
            if (prev) {
                MOZ_ASSERT(prev->maybeSlot() >= shape->maybeSlot());
                shape->kids.checkConsistency(prev);
            }
            prev = shape;
        }
    }
}
#endif

void
js::ObjectImpl::initSlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    JSCompartment *comp = compartment();
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(comp, this->asObjectPtr(), start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(comp, this->asObjectPtr(), start++, *vector++);
}

void
js::ObjectImpl::copySlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    JSCompartment *comp = compartment();
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->set(comp, this->asObjectPtr(), start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->set(comp, this->asObjectPtr(), start++, *vector++);
}

#ifdef DEBUG
bool
js::ObjectImpl::slotInRange(uint32_t slot, SentinelAllowed sentinel) const
{
    uint32_t capacity = numFixedSlots() + numDynamicSlots();
    if (sentinel == SENTINEL_ALLOWED)
        return slot <= capacity;
    return slot < capacity;
}
#endif /* DEBUG */

#if defined(_MSC_VER) && _MSC_VER >= 1500
/*
 * Work around a compiler bug in MSVC9 and above, where inlining this function
 * causes stack pointer offsets to go awry and spp to refer to something higher
 * up the stack.
 */
MOZ_NEVER_INLINE
#endif
const Shape *
js::ObjectImpl::nativeLookup(JSContext *cx, jsid id)
{
    MOZ_ASSERT(isNative());
    Shape **spp;
    return Shape::search(cx, lastProperty(), id, &spp);
}

#ifdef DEBUG
const Shape *
js::ObjectImpl::nativeLookupNoAllocation(JSContext *cx, jsid id)
{
    MOZ_ASSERT(isNative());
    return Shape::searchNoAllocation(cx, lastProperty(), id);
}
#endif

void
js::ObjectImpl::markChildren(JSTracer *trc)
{
    MarkTypeObject(trc, &type_, "type");

    MarkShape(trc, &shape_, "shape");

    Class *clasp = shape_->getObjectClass();
    JSObject *obj = asObjectPtr();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (shape_->isNative())
        MarkObjectSlots(trc, obj, 0, obj->slotSpan());
}

bool
js::SparseElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj,
                                        uint32_t index, const Value &value,
                                        PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

bool
js::DenseElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj,
                                       uint32_t index, const Value &value,
                                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    /*
     * If the new property doesn't have the right attributes, or an atypical
     * getter or setter is being used, go sparse.
     */
    if (attrs != JSPROP_ENUMERATE ||
        (attrs & (JSPROP_GETTER | JSPROP_SETTER)) || getter || setter)
    {
        if (!obj->makeElementsSparse(cx))
            return false;
        SparseElementsHeader &elts = obj->elementsHeader().asSparseElements();
        return elts.defineElement(cx, obj, index, value, getter, setter, attrs);
    }

    /* If space for the dense element already exists, we only need set it. */
    uint32_t initLen = initializedLength();
    if (index < initLen) {
        obj->elements[index].set(obj->asObjectPtr(), index, value);
        return true;
    }

    /* Otherwise we ensure space for it exists and that it's initialized. */
    ObjectImpl::DenseElementsResult res = obj->ensureDenseElementsInitialized(cx, index, 0);

    /* Propagate any error. */
    if (res == ObjectImpl::Failure)
        return false;

    /* Otherwise, if the index was too far out of range, go sparse. */
    if (res == ObjectImpl::ConvertToSparse) {
        if (!obj->makeElementsSparse(cx))
            return false;
        SparseElementsHeader &elts = obj->elementsHeader().asSparseElements();
        return elts.defineElement(cx, obj, index, value, getter, setter, attrs);
    }

    /* But if we were able to ensure the element's existence, we're good. */
    MOZ_ASSERT(res == ObjectImpl::Succeeded);
    obj->elements[index].set(obj->asObjectPtr(), index, value);
    return true;
}

static JSObject *
ArrayBufferDelegate(JSContext *cx, ObjectImpl *obj)
{
    MOZ_ASSERT(obj->hasClass(&ArrayBufferClass));
    if (obj->getPrivate())
        return static_cast<JSObject *>(obj->getPrivate());
    JSObject *delegate = NewObjectWithGivenProto(cx, &ObjectClass, obj->getProto(), NULL);
    obj->setPrivate(delegate);
    return delegate;
}

template <typename T>
bool
js::TypedElementsHeader<T>::defineElement(JSContext *cx, ObjectImpl *obj,
                                       uint32_t index, const Value &value,
                                       PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    /*
     * XXX This isn't really a good error message, if this is even how typed
     *     arrays should behave...
     */
    js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_OBJECT_NOT_EXTENSIBLE,
                             JSDVG_IGNORE_STACK, ObjectValue(*(JSObject*)obj), // XXX
                             NULL, NULL, NULL);
    return false;
}

bool
js::ArrayBufferElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj,
                                             uint32_t index, const Value &value,
                                             PropertyOp getter, StrictPropertyOp setter,
                                             unsigned attrs)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    JSObject *delegate = ArrayBufferDelegate(cx, obj);
    if (!delegate)
        return false;
    return DefineElement(cx, delegate, index, value, getter, setter, attrs);
}

bool
js::DefineElement(JSContext *cx, ObjectImpl *obj, uint32_t index, const Value &value,
                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    NEW_OBJECT_REPRESENTATION_ONLY();

    ElementsHeader &header = obj->elementsHeader();

    switch (header.kind()) {
      case DenseElements:
        return header.asDenseElements().defineElement(cx, obj, index, value, getter, setter,
                                                      attrs);
      case SparseElements:
        return header.asSparseElements().defineElement(cx, obj, index, value, getter, setter,
                                                       attrs);
      case Uint8Elements:
        return header.asUint8Elements().defineElement(cx, obj, index, value, getter, setter,
                                                      attrs);
      case Int8Elements:
        return header.asInt8Elements().defineElement(cx, obj, index, value, getter, setter,
                                                     attrs);
      case Uint16Elements:
        return header.asUint16Elements().defineElement(cx, obj, index, value, getter, setter,
                                                       attrs);
      case Int16Elements:
        return header.asInt16Elements().defineElement(cx, obj, index, value, getter, setter,
                                                      attrs);
      case Uint32Elements:
        return header.asUint32Elements().defineElement(cx, obj, index, value, getter, setter,
                                                       attrs);
      case Int32Elements:
        return header.asInt32Elements().defineElement(cx, obj, index, value, getter, setter,
                                                      attrs);
      case Uint8ClampedElements:
        return header.asUint8ClampedElements().defineElement(cx, obj, index, value,
                                                             getter, setter, attrs);
      case Float32Elements:
        return header.asFloat32Elements().defineElement(cx, obj, index, value, getter, setter,
                                                        attrs);
      case Float64Elements:
        return header.asFloat64Elements().defineElement(cx, obj, index, value, getter, setter,
                                                        attrs);
      case ArrayBufferElements:
        return header.asArrayBufferElements().defineElement(cx, obj, index, value, getter, setter,
                                                            attrs);
    }

    MOZ_NOT_REACHED("bad elements kind!");
    return false;
}
