/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ObjectImpl-inl.h"

#include "gc/Marking.h"
#include "js/Value.h"
#include "vm/Debugger.h"

#include "jsobjinlines.h"

using namespace js;

using JS::GenericNaN;

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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_GET_SET_FIELD,
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_GET_SET_FIELD,
                                 js_setter_str);
            return false;
        }
    }
    return true;
}

static bool
CheckArgCompartment(JSContext *cx, JSObject *obj, HandleValue v,
                    const char *methodname, const char *propname)
{
    if (v.isObject() && v.toObject().compartment() != obj->compartment()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_DEBUG_COMPARTMENT_MISMATCH,
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
PropDesc::unwrapDebuggerObjectsInto(JSContext *cx, Debugger *dbg, HandleObject obj,
                                    PropDesc *unwrapped) const
{
    MOZ_ASSERT(!isUndefined());

    *unwrapped = *this;

    if (unwrapped->hasValue()) {
        RootedValue value(cx, unwrapped->value_);
        if (!dbg->unwrapDebuggeeValue(cx, &value) ||
            !CheckArgCompartment(cx, obj, value, "defineProperty", "value"))
        {
            return false;
        }
        unwrapped->value_ = value;
    }

    if (unwrapped->hasGet()) {
        RootedValue get(cx, unwrapped->get_);
        if (!dbg->unwrapDebuggeeValue(cx, &get) ||
            !CheckArgCompartment(cx, obj, get, "defineProperty", "get"))
        {
            return false;
        }
        unwrapped->get_ = get;
    }

    if (unwrapped->hasSet()) {
        RootedValue set(cx, unwrapped->set_);
        if (!dbg->unwrapDebuggeeValue(cx, &set) ||
            !CheckArgCompartment(cx, obj, set, "defineProperty", "set"))
        {
            return false;
        }
        unwrapped->set_ = set;
    }

    return true;
}

/*
 * Rewrap *idp and the fields of *desc for the current compartment.  Also:
 * defining a property on a proxy requires pd_ to contain a descriptor object,
 * so reconstitute desc->pd_ if needed.
 */
bool
PropDesc::wrapInto(JSContext *cx, HandleObject obj, const jsid &id, jsid *wrappedId,
                   PropDesc *desc) const
{
    MOZ_ASSERT(!isUndefined());

    JSCompartment *comp = cx->compartment();

    *wrappedId = id;
    if (!comp->wrapId(cx, wrappedId))
        return false;

    *desc = *this;
    RootedValue value(cx, desc->value_);
    RootedValue get(cx, desc->get_);
    RootedValue set(cx, desc->set_);

    if (!comp->wrap(cx, &value) || !comp->wrap(cx, &get) || !comp->wrap(cx, &set))
        return false;

    desc->value_ = value;
    desc->get_ = get;
    desc->set_ = set;
    return !obj->is<ProxyObject>() || desc->makeObject(cx);
}

static const ObjectElements emptyElementsHeader(0, 0);

/* Objects with no elements share one empty set of elements. */
HeapSlot *const js::emptyObjectElements =
    reinterpret_cast<HeapSlot *>(uintptr_t(&emptyElementsHeader) + sizeof(ObjectElements));

/* static */ bool
ObjectElements::ConvertElementsToDoubles(JSContext *cx, uintptr_t elementsPtr)
{
    /*
     * This function is infallible, but has a fallible interface so that it can
     * be called directly from Ion code. Only arrays can have their dense
     * elements converted to doubles, and arrays never have empty elements.
     */
    HeapSlot *elementsHeapPtr = (HeapSlot *) elementsPtr;
    JS_ASSERT(elementsHeapPtr != emptyObjectElements);

    ObjectElements *header = ObjectElements::fromElements(elementsHeapPtr);
    JS_ASSERT(!header->shouldConvertDoubleElements());

    Value *vp = (Value *) elementsPtr;
    for (size_t i = 0; i < header->initializedLength; i++) {
        if (vp[i].isInt32())
            vp[i].setDouble(vp[i].toInt32());
    }

    header->setShouldConvertDoubleElements();
    return true;
}

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
    Shape *prev = nullptr;

    if (inDictionaryMode()) {
        MOZ_ASSERT(shape->hasTable());

        ShapeTable &table = shape->table();
        for (uint32_t fslot = table.freelist; fslot != SHAPE_INVALID_SLOT;
             fslot = getSlot(fslot).toPrivateUint32()) {
            MOZ_ASSERT(fslot < slotSpan());
        }

        for (int n = throttle; --n >= 0 && shape->parent; shape = shape->parent) {
            MOZ_ASSERT_IF(lastProperty() != shape, !shape->hasTable());

            Shape **spp = table.search(shape->propid(), false);
            MOZ_ASSERT(SHAPE_FETCH(spp) == shape);
        }

        shape = lastProperty();
        for (int n = throttle; --n >= 0 && shape; shape = shape->parent) {
            MOZ_ASSERT_IF(shape->slot() != SHAPE_INVALID_SLOT, shape->slot() < slotSpan());
            if (!prev) {
                MOZ_ASSERT(lastProperty() == shape);
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
                for (Shape::Range<NoGC> r(shape); !r.empty(); r.popFront()) {
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
js::ObjectImpl::initializeSlotRange(uint32_t start, uint32_t length)
{
    /*
     * No bounds check, as this is used when the object's shape does not
     * reflect its allocated slots (updateSlotsForSpan).
     */
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRangeUnchecked(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);

    JSRuntime *rt = runtimeFromAnyThread();
    uint32_t offset = start;
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, offset++, UndefinedValue());
}

void
js::ObjectImpl::initSlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    JSRuntime *rt = runtimeFromAnyThread();
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(rt, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
}

void
js::ObjectImpl::copySlotRange(uint32_t start, const Value *vector, uint32_t length)
{
    JS::Zone *zone = this->zone();
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->set(zone, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->set(zone, this->asObjectPtr(), HeapSlot::Slot, start++, *vector++);
}

#ifdef DEBUG
bool
js::ObjectImpl::isProxy() const
{
    return asObjectPtr()->is<ProxyObject>();
}

bool
js::ObjectImpl::slotInRange(uint32_t slot, SentinelAllowed sentinel) const
{
    uint32_t capacity = numFixedSlots() + numDynamicSlots();
    if (sentinel == SENTINEL_ALLOWED)
        return slot <= capacity;
    return slot < capacity;
}
#endif /* DEBUG */

// See bug 844580.
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500
/*
 * Work around a compiler bug in MSVC9 and above, where inlining this function
 * causes stack pointer offsets to go awry and spp to refer to something higher
 * up the stack.
 */
MOZ_NEVER_INLINE
#endif
Shape *
js::ObjectImpl::nativeLookup(ExclusiveContext *cx, jsid id)
{
    MOZ_ASSERT(isNative());
    Shape **spp;
    return Shape::search(cx, lastProperty(), id, &spp);
}

#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

Shape *
js::ObjectImpl::nativeLookupPure(jsid id)
{
    MOZ_ASSERT(isNative());
    return Shape::searchNoHashify(lastProperty(), id);
}

void
js::ObjectImpl::markChildren(JSTracer *trc)
{
    MarkTypeObject(trc, &type_, "type");

    MarkShape(trc, &shape_, "shape");

    const Class *clasp = type_->clasp;
    JSObject *obj = asObjectPtr();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (shape_->isNative()) {
        MarkObjectSlots(trc, obj, 0, obj->slotSpan());
        gc::MarkArraySlots(trc, obj->getDenseInitializedLength(), obj->getDenseElements(), "objectElements");
    }
}

bool
DenseElementsHeader::getOwnElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                   unsigned resolveFlags, PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    uint32_t len = initializedLength();
    if (index >= len) {
        *desc = PropDesc::undefined();
        return true;
    }

    HeapSlot &slot = obj->elements[index];
    if (slot.isMagic(JS_ELEMENTS_HOLE)) {
        *desc = PropDesc::undefined();
        return true;
    }

    *desc = PropDesc(slot, PropDesc::Writable, PropDesc::Enumerable, PropDesc::Configurable);
    return true;
}

bool
SparseElementsHeader::getOwnElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                    unsigned resolveFlags, PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSUME_UNREACHABLE("NYI");
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
TypedElementsHeader<T>::getOwnElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                      unsigned resolveFlags, PropDesc *desc)
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
ArrayBufferElementsHeader::getOwnElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                         unsigned resolveFlags, PropDesc *desc)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
SparseElementsHeader::defineElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                    const PropDesc &desc, bool shouldThrow, unsigned resolveFlags,
                                    bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
DenseElementsHeader::defineElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index,
                                   const PropDesc &desc, bool shouldThrow, unsigned resolveFlags,
                                   bool *succeeded)
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
        return elts.defineElement(cx, obj, index, desc, shouldThrow, resolveFlags, succeeded);
    }

    /* Does the element exist?  All behavior depends upon this. */
    uint32_t initLen = initializedLength();
    if (index < initLen) {
        HeapSlot &slot = obj->elements[index];
        if (!slot.isMagic(JS_ELEMENTS_HOLE)) {
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
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (!extensible) {
        *succeeded = false;
        if (!shouldThrow)
            return true;
        RootedValue val(cx, ObjectValue(*obj));
        MOZ_ALWAYS_FALSE(js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_OBJECT_NOT_EXTENSIBLE,
                                                  JSDVG_IGNORE_STACK,
                                                  val, NullPtr(),
                                                  nullptr, nullptr));
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
        return elts.defineElement(cx, obj, index, desc, shouldThrow, resolveFlags, succeeded);
    }

    /* But if we were able to ensure the element's existence, we're good. */
    MOZ_ASSERT(res == ObjectImpl::Succeeded);
    obj->elements[index].set(obj->asObjectPtr(), HeapSlot::Element, index, desc.value());
    *succeeded = true;
    return true;
}

JSObject *
js::ArrayBufferDelegate(JSContext *cx, Handle<ObjectImpl*> obj)
{
    MOZ_ASSERT(obj->hasClass(&ArrayBufferObject::class_));
    if (obj->getPrivate())
        return static_cast<JSObject *>(obj->getPrivate());
    JSObject *delegate = NewObjectWithGivenProto(cx, &JSObject::class_,
                                                 obj->getProto(), nullptr);
    obj->setPrivateGCThing(delegate);
    return delegate;
}

template <typename T>
bool
TypedElementsHeader<T>::defineElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                      uint32_t index, const PropDesc &desc, bool shouldThrow,
                                      unsigned resolveFlags, bool *succeeded)
{
    /* XXX jwalden This probably isn't how typed arrays should behave... */
    *succeeded = false;

    RootedValue val(cx, ObjectValue(*obj));
    js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_OBJECT_NOT_EXTENSIBLE,
                             JSDVG_IGNORE_STACK,
                             val, NullPtr(), nullptr, nullptr);
    return false;
}

bool
ArrayBufferElementsHeader::defineElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                         uint32_t index, const PropDesc &desc, bool shouldThrow,
                                         unsigned resolveFlags, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    Rooted<JSObject*> delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return DefineElement(cx, delegate, index, desc, shouldThrow, resolveFlags, succeeded);
}

bool
js::GetOwnProperty(JSContext *cx, Handle<ObjectImpl*> obj, PropertyId pid_, unsigned resolveFlags,
                   PropDesc *desc)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    JS_CHECK_RECURSION(cx, return false);

    Rooted<PropertyId> pid(cx, pid_);

    if (Downcast(obj)->is<ProxyObject>())
        MOZ_ASSUME_UNREACHABLE("NYI: proxy [[GetOwnProperty]]");

    RootedShape shape(cx, obj->nativeLookup(cx, pid));
    if (!shape) {
        /* Not found: attempt to resolve it. */
        const Class *clasp = obj->getClass();
        JSResolveOp resolve = clasp->resolve;
        if (resolve != JS_ResolveStub) {
            Rooted<jsid> id(cx, pid.get().asId());
            Rooted<JSObject*> robj(cx, static_cast<JSObject*>(obj.get()));
            if (clasp->flags & JSCLASS_NEW_RESOLVE) {
                Rooted<JSObject*> obj2(cx, nullptr);
                JSNewResolveOp op = reinterpret_cast<JSNewResolveOp>(resolve);
                if (!op(cx, robj, id, resolveFlags, &obj2))
                    return false;
            } else {
                if (!resolve(cx, robj, id))
                    return false;
            }
        }

        /* Now look it up again. */
        shape = obj->nativeLookup(cx, pid);
        if (!shape) {
            desc->setUndefined();
            return true;
        }
    }

    if (shape->isDataDescriptor()) {
        *desc = PropDesc(obj->nativeGetSlot(shape->slot()), shape->writability(),
                         shape->enumerability(), shape->configurability());
        return true;
    }

    if (shape->isAccessorDescriptor()) {
        *desc = PropDesc(shape->getterValue(), shape->setterValue(),
                         shape->enumerability(), shape->configurability());
        return true;
    }

    MOZ_ASSUME_UNREACHABLE("NYI: PropertyOp-based properties");
}

bool
js::GetOwnElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index, unsigned resolveFlags,
                  PropDesc *desc)
{
    ElementsHeader &header = obj->elementsHeader();
    switch (header.kind()) {
      case DenseElements:
        return header.asDenseElements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case SparseElements:
        return header.asSparseElements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Uint8Elements:
        return header.asUint8Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Int8Elements:
        return header.asInt8Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Uint16Elements:
        return header.asUint16Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Int16Elements:
        return header.asInt16Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Uint32Elements:
        return header.asUint32Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Int32Elements:
        return header.asInt32Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Uint8ClampedElements:
        return header.asUint8ClampedElements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Float32Elements:
        return header.asFloat32Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case Float64Elements:
        return header.asFloat64Elements().getOwnElement(cx, obj, index, resolveFlags, desc);
      case ArrayBufferElements:
        return header.asArrayBufferElements().getOwnElement(cx, obj, index, resolveFlags, desc);
    }

    MOZ_ASSUME_UNREACHABLE("bad elements kind!");
}

bool
js::GetProperty(JSContext *cx, Handle<ObjectImpl*> obj, Handle<ObjectImpl*> receiver,
                Handle<PropertyId> pid, unsigned resolveFlags, MutableHandle<Value> vp)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    MOZ_ASSERT(receiver);

    Rooted<ObjectImpl*> current(cx, obj);

    do {
        MOZ_ASSERT(obj);

        if (Downcast(current)->is<ProxyObject>())
            MOZ_ASSUME_UNREACHABLE("NYI: proxy [[GetP]]");

        AutoPropDescRooter desc(cx);
        if (!GetOwnProperty(cx, current, pid, resolveFlags, &desc.getPropDesc()))
            return false;

        /* No property?  Recur or bottom out. */
        if (desc.isUndefined()) {
            current = current->getProto();
            if (current)
                continue;

            vp.setUndefined();
            return true;
        }

        /* If it's a data property, return the value. */
        if (desc.isDataDescriptor()) {
            vp.set(desc.value());
            return true;
        }

        /* If it's an accessor property, call its [[Get]] with the receiver. */
        if (desc.isAccessorDescriptor()) {
            Rooted<Value> get(cx, desc.getterValue());
            if (get.isUndefined()) {
                vp.setUndefined();
                return true;
            }

            InvokeArgs args(cx);
            if (!args.init(0))
                return false;

            args.setCallee(get);
            args.setThis(ObjectValue(*receiver));

            bool ok = Invoke(cx, args);
            vp.set(args.rval());
            return ok;
        }

        /* Otherwise it's a PropertyOp-based property.  XXX handle this! */
        MOZ_ASSUME_UNREACHABLE("NYI: handle PropertyOp'd properties here");
    } while (false);

    MOZ_ASSUME_UNREACHABLE("buggy control flow");
}

bool
js::GetElement(JSContext *cx, Handle<ObjectImpl*> obj, Handle<ObjectImpl*> receiver, uint32_t index,
               unsigned resolveFlags, Value *vp)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    Rooted<ObjectImpl*> current(cx, obj);

    RootedValue getter(cx);
    do {
        MOZ_ASSERT(current);

        if (Downcast(current)->is<ProxyObject>())
            MOZ_ASSUME_UNREACHABLE("NYI: proxy [[GetP]]");

        PropDesc desc;
        if (!GetOwnElement(cx, current, index, resolveFlags, &desc))
            return false;

        /* No property?  Recur or bottom out. */
        if (desc.isUndefined()) {
            current = current->getProto();
            if (current)
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
            getter = desc.getterValue();
            if (getter.isUndefined()) {
                vp->setUndefined();
                return true;
            }

            InvokeArgs args(cx);
            if (!args.init(0))
                return false;

            /* Push getter, receiver, and no args. */
            args.setCallee(getter);
            args.setThis(ObjectValue(*current));

            bool ok = Invoke(cx, args);
            *vp = args.rval();
            return ok;
        }

        /* Otherwise it's a PropertyOp-based property.  XXX handle this! */
        MOZ_ASSUME_UNREACHABLE("NYI: handle PropertyOp'd properties here");
    } while (false);

    MOZ_ASSUME_UNREACHABLE("buggy control flow");
}

bool
js::HasElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index, unsigned resolveFlags,
               bool *found)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    Rooted<ObjectImpl*> current(cx, obj);

    do {
        MOZ_ASSERT(current);

        if (Downcast(current)->is<ProxyObject>())
            MOZ_ASSUME_UNREACHABLE("NYI: proxy [[HasProperty]]");

        PropDesc prop;
        if (!GetOwnElement(cx, current, index, resolveFlags, &prop))
            return false;

        if (!prop.isUndefined()) {
            *found = true;
            return true;
        }

        current = current->getProto();
        if (current)
            continue;

        *found = false;
        return true;
    } while (false);

    MOZ_ASSUME_UNREACHABLE("buggy control flow");
}

bool
js::DefineElement(JSContext *cx, Handle<ObjectImpl*> obj, uint32_t index, const PropDesc &desc,
                  bool shouldThrow, unsigned resolveFlags, bool *succeeded)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    ElementsHeader &header = obj->elementsHeader();

    switch (header.kind()) {
      case DenseElements:
        return header.asDenseElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      resolveFlags, succeeded);
      case SparseElements:
        return header.asSparseElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       resolveFlags, succeeded);
      case Uint8Elements:
        return header.asUint8Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      resolveFlags, succeeded);
      case Int8Elements:
        return header.asInt8Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                     resolveFlags, succeeded);
      case Uint16Elements:
        return header.asUint16Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       resolveFlags, succeeded);
      case Int16Elements:
        return header.asInt16Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      resolveFlags, succeeded);
      case Uint32Elements:
        return header.asUint32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                       resolveFlags, succeeded);
      case Int32Elements:
        return header.asInt32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                      resolveFlags, succeeded);
      case Uint8ClampedElements:
        return header.asUint8ClampedElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                             resolveFlags, succeeded);
      case Float32Elements:
        return header.asFloat32Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                        resolveFlags, succeeded);
      case Float64Elements:
        return header.asFloat64Elements().defineElement(cx, obj, index, desc, shouldThrow,
                                                        resolveFlags, succeeded);
      case ArrayBufferElements:
        return header.asArrayBufferElements().defineElement(cx, obj, index, desc, shouldThrow,
                                                            resolveFlags, succeeded);
    }

    MOZ_ASSUME_UNREACHABLE("bad elements kind!");
}

bool
SparseElementsHeader::setElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                 Handle<ObjectImpl*> receiver, uint32_t index, const Value &v,
                                 unsigned resolveFlags, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSUME_UNREACHABLE("NYI");
}

bool
DenseElementsHeader::setElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                Handle<ObjectImpl*> receiver, uint32_t index, const Value &v,
                                unsigned resolveFlags, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    MOZ_ASSUME_UNREACHABLE("NYI");
}

template <typename T>
bool
TypedElementsHeader<T>::setElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                   Handle<ObjectImpl*> receiver, uint32_t index, const Value &v,
                                   unsigned resolveFlags, bool *succeeded)
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
            if (!StringToNumber(cx, v.toString(), &d))
                return false;
        } else if (v.isUndefined()) {
            d = GenericNaN();
        } else {
            d = double(v.toBoolean());
        }
    } else {
        // non-primitive assignments become NaN or 0 (for float/int arrays)
        d = GenericNaN();
    }

    assign(index, d);
    *succeeded = true;
    return true;
}

bool
ArrayBufferElementsHeader::setElement(JSContext *cx, Handle<ObjectImpl*> obj,
                                      Handle<ObjectImpl*> receiver, uint32_t index, const Value &v,
                                      unsigned resolveFlags, bool *succeeded)
{
    MOZ_ASSERT(this == &obj->elementsHeader());

    JSObject *delegate = ArrayBufferDelegate(cx, obj);
    if (!delegate)
        return false;
    return SetElement(cx, obj, receiver, index, v, resolveFlags, succeeded);
}

bool
js::SetElement(JSContext *cx, Handle<ObjectImpl*> obj, Handle<ObjectImpl*> receiver,
               uint32_t index, const Value &v, unsigned resolveFlags, bool *succeeded)
{
    JS_NEW_OBJECT_REPRESENTATION_ONLY();

    Rooted<ObjectImpl*> current(cx, obj);
    RootedValue setter(cx);

    MOZ_ASSERT(receiver);

    do {
        MOZ_ASSERT(current);

        if (Downcast(current)->is<ProxyObject>())
            MOZ_ASSUME_UNREACHABLE("NYI: proxy [[SetP]]");

        PropDesc ownDesc;
        if (!GetOwnElement(cx, current, index, resolveFlags, &ownDesc))
            return false;

        if (!ownDesc.isUndefined()) {
            if (ownDesc.isDataDescriptor()) {
                if (!ownDesc.writable()) {
                    *succeeded = false;
                    return true;
                }

                if (receiver == current) {
                    PropDesc updateDesc = PropDesc::valueOnly(v);
                    return DefineElement(cx, receiver, index, updateDesc, false, resolveFlags,
                                         succeeded);
                }

                PropDesc newDesc;
                return DefineElement(cx, receiver, index, newDesc, false, resolveFlags, succeeded);
            }

            if (ownDesc.isAccessorDescriptor()) {
                setter = ownDesc.setterValue();
                if (setter.isUndefined()) {
                    *succeeded = false;
                    return true;
                }

                InvokeArgs args(cx);
                if (!args.init(1))
                    return false;

                /* Push set, receiver, and v as the sole argument. */
                args.setCallee(setter);
                args.setThis(ObjectValue(*current));
                args[0].set(v);

                *succeeded = true;
                return Invoke(cx, args);
            }

            MOZ_ASSUME_UNREACHABLE("NYI: setting PropertyOp-based property");
        }

        current = current->getProto();
        if (current)
            continue;

        PropDesc newDesc(v, PropDesc::Writable, PropDesc::Enumerable, PropDesc::Configurable);
        return DefineElement(cx, receiver, index, newDesc, false, resolveFlags, succeeded);
    } while (false);

    MOZ_ASSUME_UNREACHABLE("buggy control flow");
}

void
AutoPropDescRooter::trace(JSTracer *trc)
{
    gc::MarkValueRoot(trc, &propDesc.pd_, "AutoPropDescRooter pd");
    gc::MarkValueRoot(trc, &propDesc.value_, "AutoPropDescRooter value");
    gc::MarkValueRoot(trc, &propDesc.get_, "AutoPropDescRooter get");
    gc::MarkValueRoot(trc, &propDesc.set_, "AutoPropDescRooter set");
}
