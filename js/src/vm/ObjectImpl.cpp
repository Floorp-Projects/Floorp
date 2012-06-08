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

#include "js/TemplateLib.h"

#include "Debugger.h"
#include "ObjectImpl.h"

#include "gc/Barrier-inl.h"

#include "ObjectImpl-inl.h"

using namespace js;

PropDesc::PropDesc()
  : pd_(UndefinedValue()),
    value_(UndefinedValue()),
    get_(UndefinedValue()),
    set_(UndefinedValue()),
    attrs(0),
    hasGet_(false),
    hasSet_(false),
    hasValue_(false),
    hasWritable_(false),
    hasEnumerable_(false),
    hasConfigurable_(false),
    isUndefined_(true)
{
}

bool
PropDesc::checkGetter(JSContext *cx)
{
    if (hasGet_) {
        if (!js_IsCallable(get_) && !get_.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                                 js_getter_str);
            return false;
        }
    }
    return true;
}

bool
PropDesc::checkSetter(JSContext *cx)
{
    if (hasSet_) {
        if (!js_IsCallable(set_) && !set_.isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                                 js_setter_str);
            return false;
        }
    }
    return true;
}

static bool
CheckArgCompartment(JSContext *cx, JSObject *obj, const Value &v,
                    const char *methodname, const char *propname)
{
    if (v.isObject() && v.toObject().compartment() != obj->compartment()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_COMPARTMENT_MISMATCH,
                             methodname, propname);
        return false;
    }
    return true;
}

/*
 * Convert Debugger.Objects in desc to debuggee values.
 * Reject non-callable getters and setters.
 */
bool
PropDesc::unwrapDebuggerObjectsInto(JSContext *cx, Debugger *dbg, JSObject *obj,
                                    PropDesc *unwrapped) const
{
    MOZ_ASSERT(!isUndefined());

    *unwrapped = *this;

    if (unwrapped->hasValue()) {
        if (!dbg->unwrapDebuggeeValue(cx, &unwrapped->value_) ||
            !CheckArgCompartment(cx, obj, unwrapped->value_, "defineProperty", "value"))
        {
            return false;
        }
    }

    if (unwrapped->hasGet()) {
        if (!dbg->unwrapDebuggeeValue(cx, &unwrapped->get_) ||
            !CheckArgCompartment(cx, obj, unwrapped->get_, "defineProperty", "get"))
        {
            return false;
        }
    }

    if (unwrapped->hasSet()) {
        if (!dbg->unwrapDebuggeeValue(cx, &unwrapped->set_) ||
            !CheckArgCompartment(cx, obj, unwrapped->set_, "defineProperty", "set"))
        {
            return false;
        }
    }

    return true;
}

/*
 * Rewrap *idp and the fields of *desc for the current compartment.  Also:
 * defining a property on a proxy requires pd_ to contain a descriptor object,
 * so reconstitute desc->pd_ if needed.
 */
bool
PropDesc::wrapInto(JSContext *cx, JSObject *obj, const jsid &id, jsid *wrappedId,
                   PropDesc *desc) const
{
    MOZ_ASSERT(!isUndefined());

    JSCompartment *comp = cx->compartment;

    *wrappedId = id;
    if (!comp->wrapId(cx, wrappedId))
        return false;

    *desc = *this;
    if (!comp->wrap(cx, &desc->value_))
        return false;
    if (!comp->wrap(cx, &desc->get_))
        return false;
    if (!comp->wrap(cx, &desc->set_))
        return false;
    return !obj->isProxy() || desc->makeObject(cx);
}

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

        ShapeTable &table = shape->table();
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
                ShapeTable &table = shape->table();
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
DenseElementsHeader::getOwnElement(JSContext *cx, ObjectImpl *obj, uint32_t index, PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    uint32_t len = initializedLength();
    if (index >= len) {
        *desc = PropDesc::undefined();
        return true;
    }

    HeapSlot &slot = obj->elements[index];
    if (slot.isMagic(JS_ARRAY_HOLE)) {
        *desc = PropDesc::undefined();
        return true;
    }

    *desc = PropDesc(slot, PropDesc::Writable, PropDesc::Enumerable, PropDesc::Configurable);
    return true;
}

bool
SparseElementsHeader::getOwnElement(JSContext *cx, ObjectImpl *obj, uint32_t index, PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

template<typename T>
static Value
ElementToValue(const T &t)
{
    return NumberValue(t);
}

template<>
/* static */ Value
ElementToValue(const uint8_clamped &u)
{
    return NumberValue(uint8_t(u));
}

template<typename T>
bool
TypedElementsHeader<T>::getOwnElement(JSContext *cx, ObjectImpl *obj, uint32_t index,
                                      PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    if (index >= length()) {
        *desc = PropDesc::undefined();
        return true;
    }

    *desc = PropDesc(ElementToValue(getElement(index)), PropDesc::Writable,
                     PropDesc::Enumerable, PropDesc::Configurable);
    return false;
}

bool
ArrayBufferElementsHeader::getOwnElement(JSContext *cx, ObjectImpl *obj, uint32_t index,
                                         PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

bool
SparseElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj, uint32_t index,
                                    const PropDesc &desc, bool shouldThrow, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

bool
DenseElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj, uint32_t index,
                                   const PropDesc &desc, bool shouldThrow, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSERT_IF(desc.hasGet() || desc.hasSet(), !desc.hasValue() && !desc.hasWritable());
    MOZ_ASSERT_IF(desc.hasValue() || desc.hasWritable(), !desc.hasGet() && !desc.hasSet());

    /*
     * If desc is an accessor descriptor or a data descriptor with atypical
     * attributes, convert to sparse and retry.
     */
    if (desc.hasGet() || desc.hasSet() ||
        (desc.hasEnumerable() && !desc.enumerable()) ||
        (desc.hasConfigurable() && !desc.configurable()) ||
        (desc.hasWritable() && !desc.writable()))
    {
        if (!obj->makeElementsSparse(cx))
            return false;
        SparseElementsHeader &elts = obj->elementsHeader().asSparseElements();
        return elts.defineElement(cx, obj, index, desc, shouldThrow, succeeded);
    }

    /* Does the element exist?  All behavior depends upon this. */
    uint32_t initLen = initializedLength();
    if (index < initLen) {
        HeapSlot &slot = obj->elements[index];
        if (!slot.isMagic(JS_ARRAY_HOLE)) {
            /*
             * The element exists with attributes { [[Enumerable]]: true,
             * [[Configurable]]: true, [[Writable]]: true, [[Value]]: slot }.
             */
            // XXX jwalden fill this in!
        }
    }

    /*
     * If the element doesn't exist, we can only add it if the object is
     * extensible.
     */
    if (!obj->isExtensible()) {
        *succeeded = false;
        if (!shouldThrow)
            return true;
        MOZ_ALWAYS_FALSE(js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_OBJECT_NOT_EXTENSIBLE,
                                                  JSDVG_IGNORE_STACK,
                                                  ObjectValue(*obj),
                                                  NULL, NULL, NULL));
        return false;
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
        return elts.defineElement(cx, obj, index, desc, shouldThrow, succeeded);
    }

    /* But if we were able to ensure the element's existence, we're good. */
    MOZ_ASSERT(res == ObjectImpl::Succeeded);
    obj->elements[index].set(obj->asObjectPtr(), index, desc.value());
    *succeeded = true;
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
TypedElementsHeader<T>::defineElement(JSContext *cx, ObjectImpl *obj,
                                      uint32_t index, const PropDesc &desc, bool shouldThrow,
                                      bool *succeeded)
{
    /* XXX jwalden This probably isn't how typed arrays should behave... */
    *succeeded = false;
    js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_OBJECT_NOT_EXTENSIBLE,
                             JSDVG_IGNORE_STACK,
                             ObjectValue(*obj),
                             NULL, NULL, NULL);
    return false;
}

bool
ArrayBufferElementsHeader::defineElement(JSContext *cx, ObjectImpl *obj,
                                         uint32_t index, const PropDesc &desc, bool shouldThrow,
                                         bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    JSObject *delegate = ArrayBufferDelegate(cx, obj);
    if (!delegate)
        return false;
    return DefineElement(cx, delegate, index, desc, shouldThrow, succeeded);
}

bool
js::GetOwnElement(JSContext *cx, ObjectImpl *obj, uint32_t index, PropDesc *desc)
{
    ElementsHeader &header = obj->elementsHeader();
    switch (header.kind()) {
      case DenseElements:
        return header.asDenseElements().getOwnElement(cx, obj, index, desc);
      case SparseElements:
        return header.asSparseElements().getOwnElement(cx, obj, index, desc);
      case Uint8Elements:
        return header.asUint8Elements().getOwnElement(cx, obj, index, desc);
      case Int8Elements:
        return header.asInt8Elements().getOwnElement(cx, obj, index, desc);
      case Uint16Elements:
        return header.asUint16Elements().getOwnElement(cx, obj, index, desc);
      case Int16Elements:
        return header.asInt16Elements().getOwnElement(cx, obj, index, desc);
      case Uint32Elements:
        return header.asUint32Elements().getOwnElement(cx, obj, index, desc);
      case Int32Elements:
        return header.asInt32Elements().getOwnElement(cx, obj, index, desc);
      case Uint8ClampedElements:
        return header.asUint8ClampedElements().getOwnElement(cx, obj, index, desc);
      case Float32Elements:
        return header.asFloat32Elements().getOwnElement(cx, obj, index, desc);
      case Float64Elements:
        return header.asFloat64Elements().getOwnElement(cx, obj, index, desc);
      case ArrayBufferElements:
        return header.asArrayBufferElements().getOwnElement(cx, obj, index, desc);
    }

    MOZ_NOT_REACHED("bad elements kind!");
    return false;
}

bool
js::GetElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver, uint32_t index,
               Value *vp)
{
    NEW_OBJECT_REPRESENTATION_ONLY();

    do {
        MOZ_ASSERT(obj);

        if (static_cast<JSObject *>(obj)->isProxy()) { // XXX
            MOZ_NOT_REACHED("NYI: proxy [[GetP]]");
            return false;
        }

        PropDesc desc;
        if (!GetOwnElement(cx, obj, index, &desc))
            return false;

        /* No property?  Recur or bottom out. */
        if (desc.isUndefined()) {
            obj = obj->getProto();
            if (obj)
                continue;

            vp->setUndefined();
            return true;
        }

        /* If it's a data property, return the value. */
        if (desc.isDataDescriptor()) {
            *vp = desc.value();
            return true;
        }

        /* If it's an accessor property, call its [[Get]] with the receiver. */
        if (desc.isAccessorDescriptor()) {
            Value get = desc.getterValue();
            if (get.isUndefined()) {
                vp->setUndefined();
                return true;
            }

            InvokeArgsGuard args;
            if (!cx->stack.pushInvokeArgs(cx, 0, &args))
                return false;

            /* Push get, receiver, and no args. */
            args.calleev() = get;
            args.thisv() = ObjectValue(*receiver);

            bool ok = Invoke(cx, args);
            *vp = args.rval();
            return ok;
        }

        /* Otherwise it's a PropertyOp-based property.  XXX handle this! */
        MOZ_NOT_REACHED("NYI: handle PropertyOp'd properties here");
        return false;
    } while (false);

    MOZ_NOT_REACHED("buggy control flow");
    return false;
}

bool
js::HasElement(JSContext *cx, ObjectImpl *obj, uint32_t index, bool *found)
{
    NEW_OBJECT_REPRESENTATION_ONLY();

    do {
        MOZ_ASSERT(obj);

        if (static_cast<JSObject *>(obj)->isProxy()) { // XXX
            MOZ_NOT_REACHED("NYI: proxy [[HasProperty]]");
            return false;
        }

        PropDesc prop;
        if (!GetOwnElement(cx, obj, index, &prop))
            return false;

        if (!prop.isUndefined()) {
            *found = true;
            return true;
        }

        obj = obj->getProto();
        if (obj)
            continue;

        *found = false;
        return true;
    } while (false);

    MOZ_NOT_REACHED("buggy control flow");
    return false;
}

bool
js::DefineElement(JSContext *cx, ObjectImpl *obj, uint32_t index, const PropDesc &desc,
                  bool shouldThrow, bool *succeeded)
{
    NEW_OBJECT_REPRESENTATION_ONLY();

    ElementsHeader &header = obj->elementsHeader();

    switch (header.kind()) {
      case DenseElements:
        return header.asDenseElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      succeeded);
      case SparseElements:
        return header.asSparseElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       succeeded);
      case Uint8Elements:
        return header.asUint8Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      succeeded);
      case Int8Elements:
        return header.asInt8Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                     succeeded);
      case Uint16Elements:
        return header.asUint16Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       succeeded);
      case Int16Elements:
        return header.asInt16Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      succeeded);
      case Uint32Elements:
        return header.asUint32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       succeeded);
      case Int32Elements:
        return header.asInt32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      succeeded);
      case Uint8ClampedElements:
        return header.asUint8ClampedElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                             succeeded);
      case Float32Elements:
        return header.asFloat32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                        succeeded);
      case Float64Elements:
        return header.asFloat64Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                        succeeded);
      case ArrayBufferElements:
        return header.asArrayBufferElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                            succeeded);
    }

    MOZ_NOT_REACHED("bad elements kind!");
    return false;
}

bool
SparseElementsHeader::setElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver,
                                 uint32_t index, const Value &v, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

bool
DenseElementsHeader::setElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver,
                                uint32_t index, const Value &v, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_NOT_REACHED("NYI");
    return false;
}

template <typename T>
bool
TypedElementsHeader<T>::setElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver,
                                   uint32_t index, const Value &v, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    uint32_t len = length();
    if (index >= len) {
        /*
         * Silent ignore is better than an exception here, because at some
         * point we may want to support other properties on these objects.
         */
        *succeeded = true;
        return true;
    }

    /* Convert the value being set to the element type. */
    double d;
    if (v.isNumber()) {
        d = v.toNumber();
    } else if (v.isNull()) {
        d = 0.0;
    } else if (v.isPrimitive()) {
        if (v.isString()) {
            if (!ToNumber(cx, v, &d))
                return false;
        } else if (v.isUndefined()) {
            d = js_NaN;
        } else {
            d = double(v.toBoolean());
        }
    } else {
        // non-primitive assignments become NaN or 0 (for float/int arrays)
        d = js_NaN;
    }

    assign(index, d);
    *succeeded = true;
    return true;
}

bool
ArrayBufferElementsHeader::setElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver,
                                      uint32_t index, const Value &v, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    JSObject *delegate = ArrayBufferDelegate(cx, obj);
    if (!delegate)
        return false;
    return SetElement(cx, obj, receiver, index, v, succeeded);
}

bool
js::SetElement(JSContext *cx, ObjectImpl *obj, ObjectImpl *receiver, uint32_t index,
               const Value &v, bool *succeeded)
{
    NEW_OBJECT_REPRESENTATION_ONLY();

    do {
        MOZ_ASSERT(obj);

        if (static_cast<JSObject *>(obj)->isProxy()) { // XXX
            MOZ_NOT_REACHED("NYI: proxy [[SetP]]");
            return false;
        }

        PropDesc ownDesc;
        if (!GetOwnElement(cx, obj, index, &ownDesc))
            return false;

        if (!ownDesc.isUndefined()) {
            if (ownDesc.isDataDescriptor()) {
                if (!ownDesc.writable()) {
                    *succeeded = false;
                    return true;
                }

                if (receiver == obj) {
                    PropDesc updateDesc = PropDesc::valueOnly(v);
                    return DefineElement(cx, receiver, index, updateDesc, false, succeeded);
                }

                PropDesc newDesc;
                return DefineElement(cx, receiver, index, newDesc, false, succeeded);
            }

            if (ownDesc.isAccessorDescriptor()) {
                Value setter = ownDesc.setterValue();
                if (setter.isUndefined()) {
                    *succeeded = false;
                    return true;
                }

                InvokeArgsGuard args;
                if (!cx->stack.pushInvokeArgs(cx, 1, &args))
                    return false;

                /* Push set, receiver, and v as the sole argument. */
                args.calleev() = setter;
                args.thisv() = ObjectValue(*receiver);
                args[0] = v;

                *succeeded = true;
                return Invoke(cx, args);
            }

            MOZ_NOT_REACHED("NYI: setting PropertyOp-based property");
            return false;
        }

        obj = obj->getProto();
        if (obj)
            continue;

        PropDesc newDesc(v, PropDesc::Writable, PropDesc::Enumerable, PropDesc::Configurable);
        return DefineElement(cx, receiver, index, newDesc, false, succeeded);
    } while (false);

    MOZ_NOT_REACHED("buggy control flow");
    return false;
}
