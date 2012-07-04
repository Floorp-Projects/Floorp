/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobjinlines_h___
#define jsobjinlines_h___

#include <new>

#include "jsapi.h"
#include "jsarray.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsprobes.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstypedarray.h"
#include "jsxml.h"
#include "jswrapper.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "gc/Root.h"
#include "js/TemplateLib.h"
#include "vm/BooleanObject.h"
#include "vm/GlobalObject.h"
#include "vm/NumberObject.h"
#include "vm/RegExpStatics.h"
#include "vm/StringObject.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "gc/Barrier-inl.h"

#include "vm/ObjectImpl-inl.h"
#include "vm/RegExpStatics-inl.h"
#include "vm/String-inl.h"

inline bool
JSObject::enumerate(JSContext *cx, JSIterateOp iterop, js::Value *statep, jsid *idp)
{
    JSNewEnumerateOp op = getOps()->enumerate;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : JS_EnumerateState)(cx, obj, iterop, statep, idp);
}

inline bool
JSObject::defaultValue(JSContext *cx, JSType hint, js::Value *vp)
{
    js::RootedObject self(cx, this);

    JSConvertOp op = getClass()->convert;
    bool ok;
    if (op == JS_ConvertStub)
        ok = js::DefaultValue(cx, self, hint, vp);
    else
        ok = op(cx, self, hint, vp);
    JS_ASSERT_IF(ok, vp->isPrimitive());
    return ok;
}

inline JSType
JSObject::typeOf(JSContext *cx)
{
    js::TypeOfOp op = getOps()->typeOf;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::TypeOf)(cx, obj);
}

inline JSObject *
JSObject::thisObject(JSContext *cx)
{
    JSObjectOp op = getOps()->thisObject;
    js::Rooted<JSObject*> obj(cx, this);
    return op ? op(cx, obj) : this;
}

inline JSBool
JSObject::setGeneric(JSContext *cx, js::Handle<JSObject*> receiver, js::HandleId id, js::Value *vp,
                     JSBool strict)
{
    if (getOps()->setGeneric)
        return nonNativeSetProperty(cx, id, vp, strict);
    js::Rooted<JSObject*> obj(cx, this);
    return js::baseops::SetPropertyHelper(cx, obj, receiver, id, 0, vp, strict);
}

inline JSBool
JSObject::setProperty(JSContext *cx, js::Handle<JSObject*> receiver, js::PropertyName *name,
                      js::Value *vp, JSBool strict)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return setGeneric(cx, receiver, id, vp, strict);
}

inline JSBool
JSObject::setElement(JSContext *cx, js::Handle<JSObject*> receiver, uint32_t index, js::Value *vp,
                     JSBool strict)
{
    if (getOps()->setElement)
        return nonNativeSetElement(cx, index, vp, strict);
    js::Rooted<JSObject*> obj(cx, this);
    return js::baseops::SetElementHelper(cx, obj, receiver, index, 0, vp, strict);
}

inline JSBool
JSObject::setSpecial(JSContext *cx, js::HandleObject receiver, js::SpecialId sid, js::Value *vp,
                     JSBool strict)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return setGeneric(cx, receiver, id, vp, strict);
}

inline JSBool
JSObject::setGenericAttributes(JSContext *cx, js::HandleId id, unsigned *attrsp)
{
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::GenericAttributesOp op = getOps()->setGenericAttributes;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::SetAttributes)(cx, obj, id, attrsp);
}

inline JSBool
JSObject::setPropertyAttributes(JSContext *cx, js::PropertyName *name, unsigned *attrsp)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return setGenericAttributes(cx, id, attrsp);
}

inline JSBool
JSObject::setElementAttributes(JSContext *cx, uint32_t index, unsigned *attrsp)
{
    js::ElementAttributesOp op = getOps()->setElementAttributes;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::SetElementAttributes)(cx, obj, index, attrsp);
}

inline JSBool
JSObject::setSpecialAttributes(JSContext *cx, js::SpecialId sid, unsigned *attrsp)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return setGenericAttributes(cx, id, attrsp);
}

inline bool
JSObject::changePropertyAttributes(JSContext *cx, js::Shape *shape, unsigned attrs)
{
    return !!changeProperty(cx, shape, attrs, 0, shape->getter(), shape->setter());
}

inline JSBool
JSObject::getGeneric(JSContext *cx, js::HandleObject receiver, js::HandleId id, js::Value *vp)
{
    js::RootedObject self(cx, this);

    js::GenericIdOp op = getOps()->getGeneric;
    if (op) {
        if (!op(cx, self, receiver, id, vp))
            return false;
    } else {
        if (!js::baseops::GetProperty(cx, self, receiver, id, vp))
            return false;
    }
    return true;
}

inline JSBool
JSObject::getProperty(JSContext *cx, js::HandleObject receiver, js::PropertyName *name, js::Value *vp)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return getGeneric(cx, receiver, id, vp);
}

inline JSBool
JSObject::getGeneric(JSContext *cx, js::HandleId id, js::Value *vp)
{
    js::Rooted<JSObject*> obj(cx, this);
    return getGeneric(cx, obj, id, vp);
}

inline JSBool
JSObject::getProperty(JSContext *cx, js::PropertyName *name, js::Value *vp)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return getGeneric(cx, id, vp);
}

inline bool
JSObject::deleteProperty(JSContext *cx, js::HandlePropertyName name, js::Value *rval, bool strict)
{
    jsid id = js::NameToId(name);
    js::types::AddTypePropertyId(cx, this, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::DeletePropertyOp op = getOps()->deleteProperty;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::DeleteProperty)(cx, obj, name, rval, strict);
}

inline bool
JSObject::deleteElement(JSContext *cx, uint32_t index, js::Value *rval, bool strict)
{
    js::RootedObject self(cx, this);

    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    js::types::AddTypePropertyId(cx, self, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, self, id);
    js::DeleteElementOp op = self->getOps()->deleteElement;
    return (op ? op : js::baseops::DeleteElement)(cx, self, index, rval, strict);
}

inline bool
JSObject::deleteSpecial(JSContext *cx, js::HandleSpecialId sid, js::Value *rval, bool strict)
{
    jsid id = SPECIALID_TO_JSID(sid);
    js::types::AddTypePropertyId(cx, this, id, js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::DeleteSpecialOp op = getOps()->deleteSpecial;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::DeleteSpecial)(cx, obj, sid, rval, strict);
}

inline void
JSObject::finalize(js::FreeOp *fop)
{
    js::Probes::finalizeObject(this);

    if (!fop->onBackgroundThread()) {
        /*
         * Finalize obj first, in case it needs map and slots. Objects with
         * finalize hooks are not finalized in the background, as the class is
         * stored in the object's shape, which may have already been destroyed.
         */
        js::Class *clasp = getClass();
        if (clasp->finalize)
            clasp->finalize(fop, this);
    }

    finish(fop);
}

inline JSObject *
JSObject::getParent() const
{
    return lastProperty()->getObjectParent();
}

inline JSObject *
JSObject::enclosingScope()
{
    return isScope()
           ? &asScope().enclosingScope()
           : isDebugScope()
           ? &asDebugScope().enclosingScope()
           : getParent();
}

inline bool
JSObject::isFixedSlot(size_t slot)
{
    JS_ASSERT(!isDenseArray());
    return slot < numFixedSlots();
}

inline size_t
JSObject::dynamicSlotIndex(size_t slot)
{
    JS_ASSERT(!isDenseArray() && slot >= numFixedSlots());
    return slot - numFixedSlots();
}

inline void
JSObject::setLastPropertyInfallible(js::Shape *shape)
{
    JS_ASSERT(!shape->inDictionary());
    JS_ASSERT(shape->compartment() == compartment());
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(slotSpan() == shape->slotSpan());
    JS_ASSERT(numFixedSlots() == shape->numFixedSlots());

    shape_ = shape;
}

inline void
JSObject::removeLastProperty(JSContext *cx)
{
    JS_ASSERT(canRemoveLastProperty());
    JS_ALWAYS_TRUE(setLastProperty(cx, lastProperty()->previous()));
}

inline bool
JSObject::canRemoveLastProperty()
{
    /*
     * Check that the information about the object stored in the last
     * property's base shape is consistent with that stored in the previous
     * shape. If not consistent, then the last property cannot be removed as it
     * will induce a change in the object itself, and the object must be
     * converted to dictionary mode instead. See BaseShape comment in jsscope.h
     */
    JS_ASSERT(!inDictionaryMode());
    js::Shape *previous = lastProperty()->previous();
    return previous->getObjectParent() == lastProperty()->getObjectParent()
        && previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline const js::HeapSlot *
JSObject::getRawSlots()
{
    JS_ASSERT(isGlobal());
    return slots;
}

inline const js::Value &
JSObject::getReservedSlot(unsigned index) const
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    return getSlot(index);
}

inline js::HeapSlot &
JSObject::getReservedSlotRef(unsigned index)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    return getSlotRef(index);
}

inline void
JSObject::setReservedSlot(unsigned index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    setSlot(index, v);
}

inline void
JSObject::initReservedSlot(unsigned index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    initSlot(index, v);
}

inline void
JSObject::prepareSlotRangeForOverwrite(size_t start, size_t end)
{
    for (size_t i = start; i < end; i++)
        getSlotAddressUnchecked(i)->js::HeapSlot::~HeapSlot();
}

inline void
JSObject::prepareElementRangeForOverwrite(size_t start, size_t end)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(end <= getDenseArrayInitializedLength());
    for (size_t i = start; i < end; i++)
        elements[i].js::HeapSlot::~HeapSlot();
}

inline uint32_t
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return getElementsHeader()->length;
}

inline void
JSObject::setArrayLength(JSContext *cx, uint32_t length)
{
    JS_ASSERT(isArray());

    if (length > INT32_MAX) {
        /*
         * Mark the type of this object as possibly not a dense array, per the
         * requirements of OBJECT_FLAG_NON_DENSE_ARRAY.
         */
        js::types::MarkTypeObjectFlags(cx, this,
                                       js::types::OBJECT_FLAG_NON_PACKED_ARRAY |
                                       js::types::OBJECT_FLAG_NON_DENSE_ARRAY);
        jsid lengthId = js::NameToId(cx->runtime->atomState.lengthAtom);
        js::types::AddTypePropertyId(cx, this, lengthId,
                                     js::types::Type::DoubleType());
    }

    getElementsHeader()->length = length;
}

inline void
JSObject::setDenseArrayLength(uint32_t length)
{
    /* Variant of setArrayLength for use on dense arrays where the length cannot overflow int32. */
    JS_ASSERT(isDenseArray());
    JS_ASSERT(length <= INT32_MAX);
    getElementsHeader()->length = length;
}

inline void
JSObject::setDenseArrayInitializedLength(uint32_t length)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(length <= getDenseArrayCapacity());
    prepareElementRangeForOverwrite(length, getElementsHeader()->initializedLength);
    getElementsHeader()->initializedLength = length;
}

inline uint32_t
JSObject::getDenseArrayCapacity()
{
    JS_ASSERT(isDenseArray());
    return getElementsHeader()->capacity;
}

inline bool
JSObject::ensureElements(JSContext *cx, uint32_t capacity)
{
    if (capacity > getDenseArrayCapacity())
        return growElements(cx, capacity);
    return true;
}

inline void
JSObject::setDenseArrayElement(unsigned idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    elements[idx].set(this, idx, val);
}

inline void
JSObject::initDenseArrayElement(unsigned idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    elements[idx].init(this, idx, val);
}

inline void
JSObject::setDenseArrayElementWithType(JSContext *cx, unsigned idx, const js::Value &val)
{
    js::types::AddTypePropertyId(cx, this, JSID_VOID, val);
    setDenseArrayElement(idx, val);
}

inline void
JSObject::initDenseArrayElementWithType(JSContext *cx, unsigned idx, const js::Value &val)
{
    js::types::AddTypePropertyId(cx, this, JSID_VOID, val);
    initDenseArrayElement(idx, val);
}

inline void
JSObject::copyDenseArrayElements(unsigned dstStart, const js::Value *src, unsigned count)
{
    JS_ASSERT(dstStart + count <= getDenseArrayCapacity());
    JSCompartment *comp = compartment();
    for (unsigned i = 0; i < count; ++i)
        elements[dstStart + i].set(comp, this, dstStart + i, src[i]);
}

inline void
JSObject::initDenseArrayElements(unsigned dstStart, const js::Value *src, unsigned count)
{
    JS_ASSERT(dstStart + count <= getDenseArrayCapacity());
    JSCompartment *comp = compartment();
    for (unsigned i = 0; i < count; ++i)
        elements[dstStart + i].init(comp, this, dstStart + i, src[i]);
}

inline void
JSObject::moveDenseArrayElements(unsigned dstStart, unsigned srcStart, unsigned count)
{
    JS_ASSERT(dstStart + count <= getDenseArrayCapacity());
    JS_ASSERT(srcStart + count <= getDenseArrayInitializedLength());

    /*
     * Using memmove here would skip write barriers. Also, we need to consider
     * an array containing [A, B, C], in the following situation:
     *
     * 1. Incremental GC marks slot 0 of array (i.e., A), then returns to JS code.
     * 2. JS code moves slots 1..2 into slots 0..1, so it contains [B, C, C].
     * 3. Incremental GC finishes by marking slots 1 and 2 (i.e., C).
     *
     * Since normal marking never happens on B, it is very important that the
     * write barrier is invoked here on B, despite the fact that it exists in
     * the array before and after the move.
    */
    JSCompartment *comp = compartment();
    if (comp->needsBarrier()) {
        if (dstStart < srcStart) {
            js::HeapSlot *dst = elements + dstStart;
            js::HeapSlot *src = elements + srcStart;
            for (unsigned i = 0; i < count; i++, dst++, src++)
                dst->set(comp, this, dst - elements, *src);
        } else {
            js::HeapSlot *dst = elements + dstStart + count - 1;
            js::HeapSlot *src = elements + srcStart + count - 1;
            for (unsigned i = 0; i < count; i++, dst--, src--)
                dst->set(comp, this, dst - elements, *src);
        }
    } else {
        memmove(elements + dstStart, elements + srcStart, count * sizeof(js::HeapSlot));
        SlotRangeWriteBarrierPost(comp, this, dstStart, count);
    }
}

inline void
JSObject::moveDenseArrayElementsUnbarriered(unsigned dstStart, unsigned srcStart, unsigned count)
{
    JS_ASSERT(!compartment()->needsBarrier());

    JS_ASSERT(dstStart + count <= getDenseArrayCapacity());
    JS_ASSERT(srcStart + count <= getDenseArrayCapacity());

    memmove(elements + dstStart, elements + srcStart, count * sizeof(js::Value));
}

inline bool
JSObject::denseArrayHasInlineSlots() const
{
    JS_ASSERT(isDenseArray());
    return elements == fixedElements();
}

namespace js {

/*
 * Any name atom for a function which will be added as a DeclEnv object to the
 * scope chain above call objects for fun.
 */
static inline JSAtom *
CallObjectLambdaName(JSFunction &fun)
{
    return (fun.flags & JSFUN_LAMBDA) ? fun.atom.get() : NULL;
}

} /* namespace js */

inline const js::Value &
JSObject::getDateUTCTime() const
{
    JS_ASSERT(isDate());
    return getFixedSlot(JSSLOT_DATE_UTC_TIME);
}

inline void
JSObject::setDateUTCTime(const js::Value &time)
{
    JS_ASSERT(isDate());
    setFixedSlot(JSSLOT_DATE_UTC_TIME, time);
}

inline js::NativeIterator *
JSObject::getNativeIterator() const
{
    return (js::NativeIterator *) getPrivate();
}

inline void
JSObject::setNativeIterator(js::NativeIterator *ni)
{
    setPrivate(ni);
}

#if JS_HAS_XML_SUPPORT

inline JSLinearString *
JSObject::getNamePrefix() const
{
    JS_ASSERT(isNamespace() || isQName());
    const js::Value &v = getSlot(JSSLOT_NAME_PREFIX);
    return !v.isUndefined() ? &v.toString()->asLinear() : NULL;
}

inline jsval
JSObject::getNamePrefixVal() const
{
    JS_ASSERT(isNamespace() || isQName());
    return getSlot(JSSLOT_NAME_PREFIX);
}

inline void
JSObject::setNamePrefix(JSLinearString *prefix)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_PREFIX, prefix ? js::StringValue(prefix) : js::UndefinedValue());
}

inline void
JSObject::clearNamePrefix()
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_PREFIX, js::UndefinedValue());
}

inline JSLinearString *
JSObject::getNameURI() const
{
    JS_ASSERT(isNamespace() || isQName());
    const js::Value &v = getSlot(JSSLOT_NAME_URI);
    return !v.isUndefined() ? &v.toString()->asLinear() : NULL;
}

inline jsval
JSObject::getNameURIVal() const
{
    JS_ASSERT(isNamespace() || isQName());
    return getSlot(JSSLOT_NAME_URI);
}

inline void
JSObject::setNameURI(JSLinearString *uri)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_URI, uri ? js::StringValue(uri) : js::UndefinedValue());
}

inline jsval
JSObject::getNamespaceDeclared() const
{
    JS_ASSERT(isNamespace());
    return getSlot(JSSLOT_NAMESPACE_DECLARED);
}

inline void
JSObject::setNamespaceDeclared(jsval decl)
{
    JS_ASSERT(isNamespace());
    setSlot(JSSLOT_NAMESPACE_DECLARED, decl);
}

inline JSAtom *
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    const js::Value &v = getSlot(JSSLOT_QNAME_LOCAL_NAME);
    return !v.isUndefined() ? &v.toString()->asAtom() : NULL;
}

inline jsval
JSObject::getQNameLocalNameVal() const
{
    JS_ASSERT(isQName());
    return getSlot(JSSLOT_QNAME_LOCAL_NAME);
}

inline void
JSObject::setQNameLocalName(JSAtom *name)
{
    JS_ASSERT(isQName());
    setSlot(JSSLOT_QNAME_LOCAL_NAME, name ? js::StringValue(name) : js::UndefinedValue());
}

#endif

inline bool
JSObject::setSingletonType(JSContext *cx)
{
    if (!cx->typeInferenceEnabled())
        return true;

    JS::Rooted<JSObject*> self(cx, this);
    JS_ASSERT(!hasLazyType());
    JS_ASSERT_IF(self->getProto(), self->type() == self->getProto()->getNewType(cx, NULL));

    js::types::TypeObject *type = cx->compartment->getLazyType(cx, self->getProto());
    if (!type)
        return false;

    self->type_ = type;
    return true;
}

inline js::types::TypeObject *
JSObject::getType(JSContext *cx)
{
    JS::RootedObject self(cx, this);
    if (self->hasLazyType())
        self->makeLazyType(cx);
    return self->type_;
}

inline bool
JSObject::clearType(JSContext *cx)
{
    JS_ASSERT(!hasSingletonType());

    js::types::TypeObject *type = cx->compartment->getEmptyType(cx);
    if (!type)
        return false;

    type_ = type;
    return true;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
#ifdef DEBUG
    JS_ASSERT(newType);
    for (JSObject *obj = newType->proto; obj; obj = obj->getProto())
        JS_ASSERT(obj != this);
#endif
    JS_ASSERT_IF(hasSpecialEquality(),
                 newType->hasAnyFlags(js::types::OBJECT_FLAG_SPECIAL_EQUALITY));
    JS_ASSERT(!hasSingletonType());
    type_ = newType;
}

inline bool JSObject::setIteratedSingleton(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::ITERATED_SINGLETON);
}

inline bool JSObject::isSystem() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::SYSTEM);
}

inline bool JSObject::setSystem(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::SYSTEM);
}

inline bool JSObject::setDelegate(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::DELEGATE, GENERATE_SHAPE);
}

inline bool JSObject::isVarObj()
{
    if (isDebugScope())
        return asDebugScope().scope().isVarObj();
    return lastProperty()->hasObjectFlag(js::BaseShape::VAROBJ);
}

inline bool JSObject::setVarObj(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::VAROBJ);
}

inline bool JSObject::setWatched(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::WATCHED, GENERATE_SHAPE);
}

inline bool JSObject::hasUncacheableProto() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::UNCACHEABLE_PROTO);
}

inline bool JSObject::setUncacheableProto(JSContext *cx)
{
    return setFlag(cx, js::BaseShape::UNCACHEABLE_PROTO, GENERATE_SHAPE);
}

inline bool JSObject::isBoundFunction() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::BOUND_FUNCTION);
}

inline bool JSObject::isIndexed() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::INDEXED);
}

inline bool JSObject::watched() const
{
    return lastProperty()->hasObjectFlag(js::BaseShape::WATCHED);
}

inline bool JSObject::hasSpecialEquality() const
{
    return !!getClass()->ext.equality;
}

inline bool JSObject::isArguments() const { return isNormalArguments() || isStrictArguments(); }
inline bool JSObject::isArrayBuffer() const { return hasClass(&js::ArrayBufferClass); }
inline bool JSObject::isBlock() const { return hasClass(&js::BlockClass); }
inline bool JSObject::isBoolean() const { return hasClass(&js::BooleanClass); }
inline bool JSObject::isCall() const { return hasClass(&js::CallClass); }
inline bool JSObject::isClonedBlock() const { return isBlock() && !!getProto(); }
inline bool JSObject::isDataView() const { return hasClass(&js::DataViewClass); }
inline bool JSObject::isDate() const { return hasClass(&js::DateClass); }
inline bool JSObject::isDeclEnv() const { return hasClass(&js::DeclEnvClass); }
inline bool JSObject::isElementIterator() const { return hasClass(&js::ElementIteratorClass); }
inline bool JSObject::isError() const { return hasClass(&js::ErrorClass); }
inline bool JSObject::isFunction() const { return hasClass(&js::FunctionClass); }
inline bool JSObject::isFunctionProxy() const { return hasClass(&js::FunctionProxyClass); }
inline bool JSObject::isGenerator() const { return hasClass(&js::GeneratorClass); }
inline bool JSObject::isIterator() const { return hasClass(&js::IteratorClass); }
inline bool JSObject::isNestedScope() const { return isBlock() || isWith(); }
inline bool JSObject::isNormalArguments() const { return hasClass(&js::NormalArgumentsObjectClass); }
inline bool JSObject::isNumber() const { return hasClass(&js::NumberClass); }
inline bool JSObject::isObject() const { return hasClass(&js::ObjectClass); }
inline bool JSObject::isPrimitive() const { return isNumber() || isString() || isBoolean(); }
inline bool JSObject::isRegExp() const { return hasClass(&js::RegExpClass); }
inline bool JSObject::isRegExpStatics() const { return hasClass(&js::RegExpStaticsClass); }
inline bool JSObject::isScope() const { return isCall() || isDeclEnv() || isNestedScope(); }
inline bool JSObject::isStaticBlock() const { return isBlock() && !getProto(); }
inline bool JSObject::isStopIteration() const { return hasClass(&js::StopIterationClass); }
inline bool JSObject::isStrictArguments() const { return hasClass(&js::StrictArgumentsObjectClass); }
inline bool JSObject::isString() const { return hasClass(&js::StringClass); }
inline bool JSObject::isTypedArray() const { return IsTypedArrayClass(getClass()); }
inline bool JSObject::isWeakMap() const { return hasClass(&js::WeakMapClass); }
inline bool JSObject::isWith() const { return hasClass(&js::WithClass); }

inline bool
JSObject::isDebugScope() const
{
    extern bool js_IsDebugScopeSlow(const JSObject *obj);
    return getClass() == &js::ObjectProxyClass && js_IsDebugScopeSlow(this);
}

#if JS_HAS_XML_SUPPORT
inline bool JSObject::isNamespace() const { return hasClass(&js::NamespaceClass); }
inline bool JSObject::isXML() const { return hasClass(&js::XMLClass); }

inline bool
JSObject::isXMLId() const
{
    return hasClass(&js::QNameClass)
        || hasClass(&js::AttributeNameClass)
        || hasClass(&js::AnyNameClass);
}

inline bool
JSObject::isQName() const
{
    return hasClass(&js::QNameClass)
        || hasClass(&js::AttributeNameClass)
        || hasClass(&js::AnyNameClass);
}
#endif /* JS_HAS_XML_SUPPORT */

/* static */ inline JSObject *
JSObject::create(JSContext *cx, js::gc::AllocKind kind,
                 js::HandleShape shape, js::HandleTypeObject type, js::HeapSlot *slots)
{
    /*
     * Callers must use dynamicSlotsCount to size the initial slot array of the
     * object. We can't check the allocated capacity of the dynamic slots, but
     * make sure their presence is consistent with the shape.
     */
    JS_ASSERT(shape && type);
    JS_ASSERT(!!dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan()) == !!slots);
    JS_ASSERT(js::gc::GetGCKindSlots(kind, shape->getObjectClass()) == shape->numFixedSlots());

    JSObject *obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    obj->shape_.init(shape);
    obj->type_.init(type);
    obj->slots = slots;
    obj->elements = js::emptyObjectElements;

    const js::Class *clasp = shape->getObjectClass();
    if (clasp->hasPrivate())
        obj->privateRef(shape->numFixedSlots()) = NULL;

    size_t span = shape->slotSpan();
    if (span && clasp != &js::ArrayBufferClass)
        obj->initializeSlotRange(0, span);

    return obj;
}

/* static */ inline JSObject *
JSObject::createDenseArray(JSContext *cx, js::gc::AllocKind kind,
                           js::HandleShape shape, js::HandleTypeObject type,
                           uint32_t length)
{
    JS_ASSERT(shape && type);
    JS_ASSERT(shape->getObjectClass() == &js::ArrayClass);

    /*
     * Dense arrays are non-native, and never have properties to store.
     * The number of fixed slots in the shape of such objects is zero.
     */
    JS_ASSERT(shape->numFixedSlots() == 0);

    /*
     * The array initially stores its elements inline, there must be enough
     * space for an elements header.
     */
    JS_ASSERT(js::gc::GetGCKindSlots(kind) >= js::ObjectElements::VALUES_PER_HEADER);

    uint32_t capacity = js::gc::GetGCKindSlots(kind) - js::ObjectElements::VALUES_PER_HEADER;

    JSObject *obj = js_NewGCObject(cx, kind);
    if (!obj) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    obj->shape_.init(shape);
    obj->type_.init(type);
    obj->slots = NULL;
    obj->setFixedElements();
    new (obj->getElementsHeader()) js::ObjectElements(capacity, length);

    return obj;
}

inline void
JSObject::finish(js::FreeOp *fop)
{
    if (hasDynamicSlots())
        fop->free_(slots);
    if (hasDynamicElements())
        fop->free_(getElementsHeader());
}

inline bool
JSObject::hasProperty(JSContext *cx, js::HandleId id, bool *foundp, unsigned flags)
{
    js::RootedObject pobj(cx);
    js::RootedShape prop(cx);
    JSAutoResolveFlags rf(cx, flags);
    if (!lookupGeneric(cx, id, &pobj, &prop))
        return false;
    *foundp = !!prop;
    return true;
}

inline bool
JSObject::isCallable()
{
    return isFunction() || getClass()->call;
}

inline JSPrincipals *
JSObject::principals(JSContext *cx)
{
    if (JSObjectPrincipalsFinder find = cx->runtime->securityCallbacks->findObjectPrincipals)
        return find(this);
    return cx->compartment ? cx->compartment->principals : NULL;
}

inline void
JSObject::nativeSetSlot(unsigned slot, const js::Value &value)
{
    JS_ASSERT(isNative());
    JS_ASSERT(slot < slotSpan());
    return setSlot(slot, value);
}

inline void
JSObject::nativeSetSlotWithType(JSContext *cx, js::Shape *shape, const js::Value &value)
{
    nativeSetSlot(shape->slot(), value);
    js::types::AddTypePropertyId(cx, this, shape->propid(), value);
}

inline bool
JSObject::nativeContains(JSContext *cx, jsid id)
{
    return nativeLookup(cx, id) != NULL;
}

inline bool
JSObject::nativeContains(JSContext *cx, const js::Shape &shape)
{
    return nativeLookup(cx, shape.propid()) == &shape;
}

inline bool
JSObject::nativeEmpty() const
{
    return lastProperty()->isEmptyShape();
}

inline uint32_t
JSObject::propertyCount() const
{
    return lastProperty()->entryCount();
}

inline bool
JSObject::hasShapeTable() const
{
    return lastProperty()->hasTable();
}

inline size_t
JSObject::computedSizeOfThisSlotsElements() const
{
    size_t n = sizeOfThis();

    if (hasDynamicSlots())
        n += numDynamicSlots() * sizeof(js::Value);

    if (hasDynamicElements())
        n += (js::ObjectElements::VALUES_PER_HEADER + getElementsHeader()->capacity) *
             sizeof(js::Value);

    return n;
}

inline void
JSObject::sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf,
                              size_t *slotsSize, size_t *elementsSize,
                              size_t *miscSize) const
{
    *slotsSize = 0;
    if (hasDynamicSlots()) {
        *slotsSize += mallocSizeOf(slots);
    }

    *elementsSize = 0;
    if (hasDynamicElements()) {
        *elementsSize += mallocSizeOf(getElementsHeader());
    }

    /* Other things may be measured in the future if DMD indicates it is worthwhile. */
    *miscSize = 0;
    if (isArguments()) {
        *miscSize += asArguments().sizeOfMisc(mallocSizeOf);
    } else if (isRegExpStatics()) {
        *miscSize += js::SizeOfRegExpStaticsData(this, mallocSizeOf);
    }
}

inline JSBool
JSObject::lookupGeneric(JSContext *cx, js::HandleId id,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::RootedObject self(cx, this);

    js::LookupGenericOp op = getOps()->lookupGeneric;
    if (op)
        return op(cx, self, id, objp, propp);
    return js::baseops::LookupProperty(cx, self, id, objp, propp);
}

inline JSBool
JSObject::lookupProperty(JSContext *cx, js::PropertyName *name,
                         js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return lookupGeneric(cx, id, objp, propp);
}

inline JSBool
JSObject::defineGeneric(JSContext *cx, js::HandleId id, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    js::RootedObject self(cx, this);

    JS_ASSERT(!(attrs & JSPROP_NATIVE_ACCESSORS));
    js::DefineGenericOp op = getOps()->defineGeneric;
    return (op ? op : js::baseops::DefineGeneric)(cx, self, id, &value, getter, setter, attrs);
}

inline JSBool
JSObject::defineProperty(JSContext *cx, js::PropertyName *name, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return defineGeneric(cx, id, value, getter, setter, attrs);
}

inline JSBool
JSObject::defineElement(JSContext *cx, uint32_t index, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    js::RootedObject self(cx, this);

    js::DefineElementOp op = getOps()->defineElement;
    return (op ? op : js::baseops::DefineElement)(cx, self, index, &value, getter, setter, attrs);
}

inline JSBool
JSObject::defineSpecial(JSContext *cx, js::SpecialId sid, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        unsigned attrs /* = JSPROP_ENUMERATE */)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return defineGeneric(cx, id, value, getter, setter, attrs);
}

inline JSBool
JSObject::lookupElement(JSContext *cx, uint32_t index,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::RootedObject self(cx, this);

    js::LookupElementOp op = getOps()->lookupElement;
    return (op ? op : js::baseops::LookupElement)(cx, self, index, objp, propp);
}

inline JSBool
JSObject::lookupSpecial(JSContext *cx, js::SpecialId sid,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return lookupGeneric(cx, id, objp, propp);
}

inline JSBool
JSObject::getElement(JSContext *cx, js::HandleObject receiver, uint32_t index, js::Value *vp)
{
    js::RootedObject self(cx, this);

    js::ElementIdOp op = getOps()->getElement;
    if (op)
        return op(cx, self, receiver, index, vp);

    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, id.address()))
        return false;
    return self->getGeneric(cx, receiver, id, vp);
}

inline JSBool
JSObject::getElement(JSContext *cx, uint32_t index, js::Value *vp)
{
    js::Rooted<JSObject*> obj(cx, this);
    return getElement(cx, obj, index, vp);
}

inline JSBool
JSObject::getElementIfPresent(JSContext *cx, js::HandleObject receiver, uint32_t index, js::Value *vp,
                              bool *present)
{
    js::RootedObject self(cx, this);

    js::ElementIfPresentOp op = getOps()->getElementIfPresent;
    if (op)
        return op(cx, self, receiver, index, vp, present);

    /*
     * For now, do the index-to-id conversion just once, then use
     * lookupGeneric/getGeneric.  Once lookupElement and getElement stop both
     * doing index-to-id conversions, we can use those here.
     */
    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, id.address()))
        return false;

    js::RootedObject obj2(cx);
    js::RootedShape prop(cx);
    if (!self->lookupGeneric(cx, id, &obj2, &prop))
        return false;

    if (!prop) {
        *present = false;
        js::Debug_SetValueRangeToCrashOnTouch(vp, 1);
        return true;
    }

    *present = true;
    return self->getGeneric(cx, receiver, id, vp);
}

inline JSBool
JSObject::getSpecial(JSContext *cx, js::HandleObject receiver, js::SpecialId sid, js::Value *vp)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return getGeneric(cx, receiver, id, vp);
}

inline JSBool
JSObject::getGenericAttributes(JSContext *cx, js::HandleId id, unsigned *attrsp)
{
    js::GenericAttributesOp op = getOps()->getGenericAttributes;
    js::Rooted<JSObject*> obj(cx, this);
    return (op ? op : js::baseops::GetAttributes)(cx, obj, id, attrsp);
}

inline JSBool
JSObject::getPropertyAttributes(JSContext *cx, js::PropertyName *name, unsigned *attrsp)
{
    js::Rooted<jsid> id(cx, js::NameToId(name));
    return getGenericAttributes(cx, id, attrsp);
}

inline JSBool
JSObject::getElementAttributes(JSContext *cx, uint32_t index, unsigned *attrsp)
{
    js::RootedId id(cx);
    if (!js::IndexToId(cx, index, id.address()))
        return false;
    return getGenericAttributes(cx, id, attrsp);
}

inline JSBool
JSObject::getSpecialAttributes(JSContext *cx, js::SpecialId sid, unsigned *attrsp)
{
    js::Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return getGenericAttributes(cx, id, attrsp);
}

inline bool
JSObject::isProxy() const
{
    return js::IsProxy(this);
}

inline bool
JSObject::isCrossCompartmentWrapper() const
{
    return js::IsCrossCompartmentWrapper(this);
}

inline bool
JSObject::isWrapper() const
{
    return js::IsWrapper(this);
}

inline js::GlobalObject &
JSObject::global() const
{
    JSObject *obj = const_cast<JSObject *>(this);
    while (JSObject *parent = obj->getParent())
        obj = parent;
    JS_ASSERT(&obj->asGlobal() == compartment()->maybeGlobal());
    return obj->asGlobal();
}

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.toObject().isCallable();
}

namespace js {

PropDesc::PropDesc(const Value &getter, const Value &setter,
                   Enumerability enumerable, Configurability configurable)
  : pd_(UndefinedValue()),
    value_(UndefinedValue()),
    get_(getter), set_(setter),
    attrs(JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED |
          (enumerable ? JSPROP_ENUMERATE : 0) |
          (configurable ? 0 : JSPROP_PERMANENT)),
    hasGet_(true), hasSet_(true),
    hasValue_(false), hasWritable_(false), hasEnumerable_(true), hasConfigurable_(true),
    isUndefined_(false)
{
    MOZ_ASSERT(getter.isUndefined() || js_IsCallable(getter));
    MOZ_ASSERT(setter.isUndefined() || js_IsCallable(setter));
}

inline JSObject *
GetInnerObject(JSContext *cx, HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        return op(cx, obj);
    return obj;
}

inline JSObject *
GetOuterObject(JSContext *cx, HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.outerObject)
        return op(cx, obj);
    return obj;
}

#if JS_HAS_XML_SUPPORT
/*
 * Methods to test whether an object or a value is of type "xml" (per typeof).
 */

#define VALUE_IS_XML(v)     ((v).isObject() && (v).toObject().isXML())

static inline bool
IsXML(const js::Value &v)
{
    return v.isObject() && v.toObject().isXML();
}

#else

#define VALUE_IS_XML(v)     false

#endif /* JS_HAS_XML_SUPPORT */

static inline bool
IsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().isStopIteration();
}

/* ES5 9.1 ToPrimitive(input). */
static JS_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, Value *vp)
{
    if (vp->isPrimitive())
        return true;
    return vp->toObject().defaultValue(cx, JSTYPE_VOID, vp);
}

/* ES5 9.1 ToPrimitive(input, PreferredType). */
static JS_ALWAYS_INLINE bool
ToPrimitive(JSContext *cx, JSType preferredType, Value *vp)
{
    JS_ASSERT(preferredType != JSTYPE_VOID); /* Use the other ToPrimitive! */
    if (vp->isPrimitive())
        return true;
    return vp->toObject().defaultValue(cx, preferredType, vp);
}

/*
 * Return true if this is a compiler-created internal function accessed by
 * its own object. Such a function object must not be accessible to script
 * or embedding code.
 */
inline bool
IsInternalFunctionObject(JSObject *funobj)
{
    JSFunction *fun = funobj->toFunction();
    return (fun->flags & JSFUN_LAMBDA) && !funobj->getParent();
}

class AutoPropDescArrayRooter : private AutoGCRooter
{
  public:
    AutoPropDescArrayRooter(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTORS), descriptors(cx), skip(cx, &descriptors)
    { }

    PropDesc *append() {
        if (!descriptors.append(PropDesc()))
            return NULL;
        return &descriptors.back();
    }

    bool reserve(size_t n) {
        return descriptors.reserve(n);
    }

    PropDesc& operator[](size_t i) {
        JS_ASSERT(i < descriptors.length());
        return descriptors[i];
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    PropDescArray descriptors;
    SkipRoot skip;
};

class AutoPropertyDescriptorRooter : private AutoGCRooter, public PropertyDescriptor
{
    SkipRoot skip;

    AutoPropertyDescriptorRooter *thisDuringConstruction() { return this; }

  public:
    AutoPropertyDescriptorRooter(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTOR), skip(cx, thisDuringConstruction())
    {
        obj = NULL;
        attrs = 0;
        getter = (PropertyOp) NULL;
        setter = (StrictPropertyOp) NULL;
        value.setUndefined();
    }

    AutoPropertyDescriptorRooter(JSContext *cx, PropertyDescriptor *desc)
      : AutoGCRooter(cx, DESCRIPTOR), skip(cx, thisDuringConstruction())
    {
        obj = desc->obj;
        attrs = desc->attrs;
        getter = desc->getter;
        setter = desc->setter;
        value = desc->value;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
};

inline void
NewObjectCache::copyCachedToObject(JSObject *dst, JSObject *src)
{
    js_memcpy(dst, src, dst->sizeOfThis());
#ifdef JSGC_GENERATIONAL
    Shape::writeBarrierPost(dst->shape_, &dst->shape_);
    types::TypeObject::writeBarrierPost(dst->type_, &dst->type_);
#endif
}

static inline bool
CanBeFinalizedInBackground(gc::AllocKind kind, Class *clasp)
{
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the finalize kind. For example,
     * FINALIZE_OBJECT0 calls the finalizer on the main thread,
     * FINALIZE_OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundAllocKind is called to prevent recursively incrementing
     * the finalize kind; kind may already be a background finalize kind.
     */
    return (!gc::IsBackgroundAllocKind(kind) && !clasp->finalize);
}

/*
 * Make an object with the specified prototype. If parent is null, it will
 * default to the prototype's global if the prototype is non-null.
 */
JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        gc::AllocKind kind);

inline JSObject *
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObjectWithGivenProto(cx, clasp, proto, parent, kind);
}

inline JSProtoKey
GetClassProtoKey(js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

inline bool
FindProto(JSContext *cx, js::Class *clasp, HandleObject parent, MutableHandleObject proto)
{
    JSProtoKey protoKey = GetClassProtoKey(clasp);
    if (!js_GetClassPrototype(cx, parent, protoKey, proto, clasp))
        return false;
    if (!proto && !js_GetClassPrototype(cx, parent, JSProto_Object, proto))
        return false;
    return true;
}

/*
 * Make an object with the prototype set according to the specified prototype or class:
 *
 * if proto is non-null:
 *   use the specified proto
 * for a built-in class:
 *   use the memoized original value of the class constructor .prototype
 *   property object
 * else if available
 *   the current value of .prototype
 * else
 *   Object.prototype.
 *
 * The class prototype will be fetched from the parent's global. If global is
 * null, the context's active global will be used, and the resulting object's
 * parent will be that global.
 */
JSObject *
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
                        gc::AllocKind kind);

inline JSObject *
NewObjectWithClassProto(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObjectWithClassProto(cx, clasp, proto, parent, kind);
}

/*
 * Create a native instance of the given class with parent and proto set
 * according to the context's active global.
 */
inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp, gc::AllocKind kind)
{
    return NewObjectWithClassProto(cx, clasp, NULL, NULL, kind);
}

inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewBuiltinClassInstance(cx, clasp, kind);
}

inline GlobalObject *
GetCurrentGlobal(JSContext *cx)
{
    JSObject *scopeChain = (cx->hasfp()) ? cx->fp()->scopeChain() : cx->globalObject;
    return scopeChain ? &scopeChain->global() : NULL;
}

bool
FindClassPrototype(JSContext *cx, HandleObject scope, JSProtoKey protoKey,
                   MutableHandleObject protop, Class *clasp);

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
JSObject *
NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind kind);

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, HandleObject baseobj)
{
    JS_ASSERT(baseobj->getClass() == &ObjectClass);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind kind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
    kind = gc::GetBackgroundAllocKind(kind);
    JS_ASSERT(kind == baseobj->getAllocKind());
    JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);

    if (!obj)
        return NULL;

    if (!obj->setLastProperty(cx, baseobj->lastProperty()))
        return NULL;

    return obj;
}

JSObject *
NewReshapedObject(JSContext *cx, HandleTypeObject type, JSObject *parent,
                  gc::AllocKind kind, Shape *shape);

/*
 * As for gc::GetGCObjectKind, where numSlots is a guess at the final size of
 * the object, zero if the final size is unknown. This should only be used for
 * objects that do not require any fixed slots.
 */
static inline gc::AllocKind
GuessObjectGCKind(size_t numSlots)
{
    if (numSlots)
        return gc::GetGCObjectKind(numSlots);
    return gc::FINALIZE_OBJECT4;
}

static inline gc::AllocKind
GuessArrayGCKind(size_t numSlots)
{
    if (numSlots)
        return gc::GetGCArrayKind(numSlots);
    return gc::FINALIZE_OBJECT8;
}

/*
 * Get the GC kind to use for scripted 'new' on the given class.
 * FIXME bug 547327: estimate the size from the allocation site.
 */
static inline gc::AllocKind
NewObjectGCKind(JSContext *cx, js::Class *clasp)
{
    if (clasp == &ArrayClass || clasp == &SlowArrayClass)
        return gc::FINALIZE_OBJECT8;
    if (clasp == &FunctionClass)
        return gc::FINALIZE_OBJECT2;
    return gc::FINALIZE_OBJECT4;
}

/*
 * Fill slots with the initial slot array to use for a newborn object which
 * may or may not need dynamic slots.
 */
inline bool
PreallocateObjectDynamicSlots(JSContext *cx, Shape *shape, HeapSlot **slots)
{
    if (size_t count = JSObject::dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan())) {
        *slots = (HeapSlot *) cx->malloc_(count * sizeof(HeapSlot));
        if (!*slots)
            return false;
        Debug_SetSlotRangeToCrashOnTouch(*slots, count);
        return true;
    }
    *slots = NULL;
    return true;
}

inline bool
DefineConstructorAndPrototype(JSContext *cx, GlobalObject *global,
                              JSProtoKey key, JSObject *ctor, JSObject *proto)
{
    JS_ASSERT(!global->nativeEmpty()); /* reserved slots already allocated */
    JS_ASSERT(ctor);
    JS_ASSERT(proto);

    jsid id = NameToId(cx->runtime->atomState.classAtoms[key]);
    JS_ASSERT(!global->nativeLookupNoAllocation(cx, id));

    /* Set these first in case AddTypePropertyId looks for this class. */
    global->setSlot(key, ObjectValue(*ctor));
    global->setSlot(key + JSProto_LIMIT, ObjectValue(*proto));
    global->setSlot(key + JSProto_LIMIT * 2, ObjectValue(*ctor));

    types::AddTypePropertyId(cx, global, id, ObjectValue(*ctor));
    if (!global->addDataProperty(cx, id, key + JSProto_LIMIT * 2, 0)) {
        global->setSlot(key, UndefinedValue());
        global->setSlot(key + JSProto_LIMIT, UndefinedValue());
        global->setSlot(key + JSProto_LIMIT * 2, UndefinedValue());
        return false;
    }

    return true;
}

inline bool
ObjectClassIs(JSObject &obj, ESClassValue classValue, JSContext *cx)
{
    if (JS_UNLIKELY(obj.isProxy()))
        return Proxy::objectClassIs(&obj, classValue, cx);

    switch (classValue) {
      case ESClass_Array: return obj.isArray();
      case ESClass_Number: return obj.isNumber();
      case ESClass_String: return obj.isString();
      case ESClass_Boolean: return obj.isBoolean();
      case ESClass_RegExp: return obj.isRegExp();
      case ESClass_ArrayBuffer: return obj.isArrayBuffer();
    }
    JS_NOT_REACHED("bad classValue");
    return false;
}

inline bool
IsObjectWithClass(const Value &v, ESClassValue classValue, JSContext *cx)
{
    if (!v.isObject())
        return false;
    return ObjectClassIs(v.toObject(), classValue, cx);
}

static JS_ALWAYS_INLINE bool
ValueIsSpecial(JSObject *obj, Value *propval, SpecialId *sidp, JSContext *cx)
{
#if JS_HAS_XML_SUPPORT
    if (!propval->isObject())
        return false;

    if (obj->isXML()) {
        *sidp = SpecialId(propval->toObject());
        return true;
    }

    JSObject &propobj = propval->toObject();
    JSAtom *name;
    if (propobj.isQName() && GetLocalNameFromFunctionQName(&propobj, &name, cx)) {
        propval->setString(name);
        return false;
    }
#endif

    return false;
}

JSObject *
DefineConstructorAndPrototype(JSContext *cx, HandleObject obj, JSProtoKey key, HandleAtom atom,
                              JSObject *protoProto, Class *clasp,
                              Native constructor, unsigned nargs,
                              JSPropertySpec *ps, JSFunctionSpec *fs,
                              JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
                              JSObject **ctorp = NULL,
                              gc::AllocKind ctorKind = JSFunction::FinalizeKind);

} /* namespace js */

extern JSObject *
js_InitClass(JSContext *cx, js::HandleObject obj, JSObject *parent_proto,
             js::Class *clasp, JSNative constructor, unsigned nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs,
             JSObject **ctorp = NULL,
             js::gc::AllocKind ctorKind = JSFunction::FinalizeKind);

/*
 * js_PurgeScopeChain does nothing if obj is not itself a prototype or parent
 * scope, else it reshapes the scope and prototype chains it links. It calls
 * js_PurgeScopeChainHelper, which asserts that obj is flagged as a delegate
 * (i.e., obj has ever been on a prototype or parent chain).
 */
extern bool
js_PurgeScopeChainHelper(JSContext *cx, JSObject *obj, jsid id);

inline bool
js_PurgeScopeChain(JSContext *cx, JSObject *obj, jsid id)
{
    if (obj->isDelegate())
        return js_PurgeScopeChainHelper(cx, obj, id);
    return true;
}

inline void
js::DestroyIdArray(FreeOp *fop, JSIdArray *ida)
{
    fop->free_(ida);
}

#endif /* jsobjinlines_h___ */
