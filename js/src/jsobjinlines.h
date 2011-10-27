/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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

#ifndef jsobjinlines_h___
#define jsobjinlines_h___

#include <new>
#include "jsarray.h"
#include "jsdate.h"
#include "jsfun.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsprobes.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jstypedarray.h"
#include "jsxml.h"
#include "jswrapper.h"

/* Headers included for inline implementations used by this header. */
#include "jsbool.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsinferinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstr.h"

#include "vm/GlobalObject.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsscopeinlines.h"

inline bool
JSObject::hasPrivate() const
{
    return getClass()->flags & JSCLASS_HAS_PRIVATE;
}

inline void *&
JSObject::privateAddress(uint32 nfixed) const
{
    /*
     * The private pointer of an object can hold any word sized value.
     * Private pointers are stored immediately after the last fixed slot of
     * the object.
     */
    JS_ASSERT(nfixed == numFixedSlots());
    JS_ASSERT_IF(!isNewborn(), hasPrivate());
    js::Value *end = &fixedSlots()[nfixed];
    return *reinterpret_cast<void**>(end);
}

inline void *
JSObject::getPrivate() const { return privateAddress(numFixedSlots()); }

inline void *
JSObject::getPrivate(size_t nfixed) const { return privateAddress(nfixed); }

inline void
JSObject::setPrivate(void *data)
{
    privateAddress(numFixedSlots()) = data;
}

inline bool
JSObject::enumerate(JSContext *cx, JSIterateOp iterop, js::Value *statep, jsid *idp)
{
    JSNewEnumerateOp op = getOps()->enumerate;
    return (op ? op : JS_EnumerateState)(cx, this, iterop, statep, idp);
}

inline bool
JSObject::defaultValue(JSContext *cx, JSType hint, js::Value *vp)
{
    JSConvertOp op = getClass()->convert;
    bool ok = (op == JS_ConvertStub ? js::DefaultValue : op)(cx, this, hint, vp);
    JS_ASSERT_IF(ok, vp->isPrimitive());
    return ok;
}

inline JSType
JSObject::typeOf(JSContext *cx)
{
    js::TypeOfOp op = getOps()->typeOf;
    return (op ? op : js_TypeOf)(cx, this);
}

inline JSObject *
JSObject::thisObject(JSContext *cx)
{
    JSObjectOp op = getOps()->thisObject;
    return op ? op(cx, this) : this;
}

inline JSBool
JSObject::setGeneric(JSContext *cx, jsid id, js::Value *vp, JSBool strict)
{
    if (getOps()->setGeneric)
        return nonNativeSetProperty(cx, id, vp, strict);
    return js_SetPropertyHelper(cx, this, id, 0, vp, strict);
}

inline JSBool
JSObject::setProperty(JSContext *cx, js::PropertyName *name, js::Value *vp, JSBool strict)
{
    return setGeneric(cx, ATOM_TO_JSID(name), vp, strict);
}

inline JSBool
JSObject::setElement(JSContext *cx, uint32 index, js::Value *vp, JSBool strict)
{
    if (getOps()->setElement)
        return nonNativeSetElement(cx, index, vp, strict);
    return js_SetElementHelper(cx, this, index, 0, vp, strict);
}

inline JSBool
JSObject::setSpecial(JSContext *cx, js::SpecialId sid, js::Value *vp, JSBool strict)
{
    return setGeneric(cx, SPECIALID_TO_JSID(sid), vp, strict);
}

inline JSBool
JSObject::setGenericAttributes(JSContext *cx, jsid id, uintN *attrsp)
{
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::GenericAttributesOp op = getOps()->setGenericAttributes;
    return (op ? op : js_SetAttributes)(cx, this, id, attrsp);
}

inline JSBool
JSObject::setPropertyAttributes(JSContext *cx, js::PropertyName *name, uintN *attrsp)
{
    return setGenericAttributes(cx, ATOM_TO_JSID(name), attrsp);
}

inline JSBool
JSObject::setElementAttributes(JSContext *cx, uint32 index, uintN *attrsp)
{
    js::ElementAttributesOp op = getOps()->setElementAttributes;
    return (op ? op : js_SetElementAttributes)(cx, this, index, attrsp);
}

inline JSBool
JSObject::setSpecialAttributes(JSContext *cx, js::SpecialId sid, uintN *attrsp)
{
    return setGenericAttributes(cx, SPECIALID_TO_JSID(sid), attrsp);
}

inline JSBool
JSObject::getGeneric(JSContext *cx, JSObject *receiver, jsid id, js::Value *vp)
{
    js::GenericIdOp op = getOps()->getGeneric;
    if (op) {
        if (!op(cx, this, receiver, id, vp))
            return false;
    } else {
        if (!js_GetProperty(cx, this, receiver, id, vp))
            return false;
    }
    return true;
}

inline JSBool
JSObject::getProperty(JSContext *cx, JSObject *receiver, js::PropertyName *name, js::Value *vp)
{
    return getGeneric(cx, receiver, ATOM_TO_JSID(name), vp);
}

inline JSBool
JSObject::getGeneric(JSContext *cx, jsid id, js::Value *vp)
{
    return getGeneric(cx, this, id, vp);
}

inline JSBool
JSObject::getProperty(JSContext *cx, js::PropertyName *name, js::Value *vp)
{
    return getGeneric(cx, ATOM_TO_JSID(name), vp);
}

inline JSBool
JSObject::deleteGeneric(JSContext *cx, jsid id, js::Value *rval, JSBool strict)
{
    js::types::AddTypePropertyId(cx, this, id,
                                 js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::DeleteGenericOp op = getOps()->deleteGeneric;
    return (op ? op : js_DeleteProperty)(cx, this, id, rval, strict);
}

inline JSBool
JSObject::deleteProperty(JSContext *cx, js::PropertyName *name, js::Value *rval, JSBool strict)
{
    return deleteGeneric(cx, ATOM_TO_JSID(name), rval, strict);
}

inline JSBool
JSObject::deleteElement(JSContext *cx, uint32 index, js::Value *rval, JSBool strict)
{
    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    return deleteGeneric(cx, id, rval, strict);
}

inline JSBool
JSObject::deleteSpecial(JSContext *cx, js::SpecialId sid, js::Value *rval, JSBool strict)
{
    return deleteGeneric(cx, SPECIALID_TO_JSID(sid), rval, strict);
}

inline void
JSObject::finalize(JSContext *cx, bool background)
{
    /* Cope with stillborn objects that have no map. */
    if (isNewborn())
        return;

    js::Probes::finalizeObject(this);

    if (!background) {
        /*
         * Finalize obj first, in case it needs map and slots. Objects with
         * finalize hooks are not finalized in the background, as the class is
         * stored in the object's shape, which may have already been destroyed.
         */
        js::Class *clasp = getClass();
        if (clasp->finalize)
            clasp->finalize(cx, this);
    }

    finish(cx);
}

inline JSObject *
JSObject::getParent() const
{
    JS_ASSERT(!isScope());
    return lastProperty()->getObjectParent();
}

inline bool
JSObject::isScope() const
{
    return isCall() || isDeclEnv() || isBlock() || isWith();
}

inline JSObject *
JSObject::scopeChain() const
{
    JS_ASSERT(isScope());
    return &getFixedSlot(0).toObject();
}

inline JSObject *
JSObject::getParentOrScopeChain() const
{
    return isScope() ? scopeChain() : getParent();
}

inline JSObject *
JSObject::getStaticBlockScopeChain() const
{
    /*
     * Unlike other scope objects, static blocks not nested in one another
     * do not have a scope chain.
     */
    JS_ASSERT(isStaticBlock());
    return getFixedSlot(0).isObject() ? &getFixedSlot(0).toObject() : NULL;
}

inline JSObject *
JSObject::getParentMaybeScope() const
{
    return lastProperty()->getObjectParent();
}

inline void
JSObject::setScopeChain(JSObject *obj)
{
    JS_ASSERT(isScope());
    setFixedSlot(0, JS::ObjectValue(*obj));
}

/*static*/ inline size_t
JSObject::offsetOfScopeChain()
{
    return getFixedSlotOffset(0);
}

/* 
 * Initializer for Call objects for functions and eval frames. Set class,
 * parent, map, and shape, and allocate slots.
 */
inline bool
JSObject::initCall(JSContext *cx, const js::Bindings &bindings, JSObject *parent)
{
    js::types::TypeObject *type = cx->compartment->getEmptyType(cx);
    if (!type)
        return false;

    init(cx, type);
    if (!setInitialProperty(cx, bindings.lastShape()))
        return false;

    /*
     * Update the parent for bindings associated with non-compileAndGo scripts,
     * whose call objects do not have a consistent global variable and need
     * to be updated dynamically.
     */
    JSObject *global = parent->getGlobal();
    if (global != getParentMaybeScope()) {
        JS_ASSERT(getParentMaybeScope() == NULL);
        if (!setParent(cx, global))
            return false;
    }

    JS_ASSERT(isCall());
    JS_ASSERT(!inDictionaryMode());

    setScopeChain(parent);

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see js::BaseShape::extensibleParents.
     */
    if (lastProperty()->extensibleParents())
        return generateOwnShape(cx);
    return true;
}

/*
 * Initializer for cloned block objects. Set class, prototype, frame, map, and
 * shape.
 */
inline bool
JSObject::initClonedBlock(JSContext *cx, js::types::TypeObject *type, js::StackFrame *frame)
{
    init(cx, type);

    if (!setInitialProperty(cx, getProto()->lastProperty()))
        return false;

    /* Set the parent if necessary, as for call objects. */
    JSObject *global = frame->scopeChain().getGlobal();
    if (global != getParentMaybeScope()) {
        JS_ASSERT(getParentMaybeScope() == NULL);
        if (!setParent(cx, global))
            return false;
    }

    setPrivate(frame);

    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(isClonedBlock());

    if (lastProperty()->extensibleParents())
        return generateOwnShape(cx);
    return true;
}

/*
 * Property read barrier for deferred cloning of compiler-created function
 * objects optimized as typically non-escaping, ad-hoc methods in obj.
 */
inline const js::Shape *
JSObject::methodReadBarrier(JSContext *cx, const js::Shape &shape, js::Value *vp)
{
    JS_ASSERT(nativeContains(cx, shape));
    JS_ASSERT(shape.isMethod());
    JS_ASSERT(shape.writable());
    JS_ASSERT(shape.hasSlot());
    JS_ASSERT(shape.hasDefaultSetter());
    JS_ASSERT(!isGlobal());  /* i.e. we are not changing the global shape */

    JSFunction *fun = vp->toObject().toFunction();
    JS_ASSERT(!fun->isClonedMethod());
    JS_ASSERT(fun->isNullClosure());

    fun = js::CloneFunctionObject(cx, fun);
    if (!fun)
        return NULL;
    fun->setMethodObj(*this);

    /*
     * Replace the method property with an ordinary data property. This is
     * equivalent to this->setProperty(cx, shape.id, vp) except that any
     * watchpoint on the property is not triggered.
     */
    uint32 slot = shape.slot();
    const js::Shape *newshape = methodShapeChange(cx, shape);
    if (!newshape)
        return NULL;
    JS_ASSERT(!newshape->isMethod());
    JS_ASSERT(newshape->slot() == slot);
    vp->setObject(*fun);
    nativeSetSlot(slot, *vp);
    return newshape;
}

inline bool
JSObject::canHaveMethodBarrier() const
{
    return isObject() || isFunction() || isPrimitive() || isDate();
}

inline const js::Value *
JSObject::getRawSlots()
{
    JS_ASSERT(isGlobal());
    return slots;
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

/*static*/ inline size_t
JSObject::dynamicSlotsCount(size_t nfixed, size_t span)
{
    if (span <= nfixed)
        return 0;
    span -= nfixed;
    if (span <= SLOT_CAPACITY_MIN)
        return SLOT_CAPACITY_MIN;

    size_t log2;
    JS_FLOOR_LOG2(log2, span - 1);
    size_t slots = 1 << (log2 + 1);
    JS_ASSERT(slots >= span);
    return slots;
}

inline size_t
JSObject::numDynamicSlots() const
{
    return dynamicSlotsCount(numFixedSlots(), slotSpan());
}

inline void
JSObject::setLastPropertyInfallible(const js::Shape *shape)
{
    JS_ASSERT(!shape->inDictionary());
    JS_ASSERT(shape->compartment() == compartment());
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(slotSpan() == shape->slotSpan());
    JS_ASSERT(numFixedSlots() == shape->numFixedSlots());

    shape_ = const_cast<js::Shape *>(shape);
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
     * Some information stored in shapes describes the object itself, and can
     * be changed via replaceLastProperty without converting to a dictionary.
     * Parent shapes in the property tree may not have this information set,
     * and we need to ensure when unwinding properties that the per-object
     * information is not accidentally reset.
     */
    JS_ASSERT(!inDictionaryMode());
    const js::Shape *previous = lastProperty()->previous();
    return previous->getObjectParent() == lastProperty()->getObjectParent()
        && previous->getObjectFlags() == lastProperty()->getObjectFlags();
}

inline js::Value
JSObject::getReservedSlot(uintN index) const
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    return getSlot(index);
}

inline void
JSObject::setReservedSlot(uintN index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    setSlot(index, v);
}

inline const js::Value &
JSObject::getPrimitiveThis() const
{
    JS_ASSERT(isPrimitive());
    return getFixedSlot(JSSLOT_PRIMITIVE_THIS);
}

inline void
JSObject::setPrimitiveThis(const js::Value &pthis)
{
    JS_ASSERT(isPrimitive());
    setFixedSlot(JSSLOT_PRIMITIVE_THIS, pthis);
}

inline bool
JSObject::hasContiguousSlots(size_t start, size_t count) const
{
    /*
     * Check that the range [start, start+count) is either all inline or all
     * out of line.
     */
    JS_ASSERT(slotInRange(start + count, /* sentinelAllowed = */ true));
    return (start + count <= numFixedSlots()) || (start >= numFixedSlots());
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return getElementsHeader()->length;
}

inline void
JSObject::setArrayLength(JSContext *cx, uint32 length)
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
        jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
        js::types::AddTypePropertyId(cx, this, lengthId,
                                     js::types::Type::DoubleType());
    }

    getElementsHeader()->length = length;
}

inline void
JSObject::setDenseArrayLength(uint32 length)
{
    /* Variant of setArrayLength for use on dense arrays where the length cannot overflow int32. */
    JS_ASSERT(isDenseArray());
    JS_ASSERT(length <= INT32_MAX);
    getElementsHeader()->length = length;
}

inline uint32
JSObject::getDenseArrayInitializedLength()
{
    JS_ASSERT(isDenseArray());
    return getElementsHeader()->initializedLength;
}

inline void
JSObject::setDenseArrayInitializedLength(uint32 length)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(length <= getDenseArrayCapacity());
    getElementsHeader()->initializedLength = length;
}

inline uint32
JSObject::getDenseArrayCapacity()
{
    JS_ASSERT(isDenseArray());
    return getElementsHeader()->capacity;
}

inline bool
JSObject::ensureElements(JSContext *cx, uint32 capacity)
{
    if (capacity > getDenseArrayCapacity())
        return growElements(cx, capacity);
    return true;
}

inline const js::Value *
JSObject::getDenseArrayElements()
{
    JS_ASSERT(isDenseArray());
    return elements;
}

inline const js::Value &
JSObject::getDenseArrayElement(uintN idx)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    return elements[idx];
}

inline void
JSObject::setDenseArrayElement(uintN idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    elements[idx] = val;
}

inline void
JSObject::setDenseArrayElementWithType(JSContext *cx, uintN idx, const js::Value &val)
{
    js::types::AddTypePropertyId(cx, this, JSID_VOID, val);
    setDenseArrayElement(idx, val);
}

inline void
JSObject::copyDenseArrayElements(uintN dstStart, const js::Value *src, uintN count)
{
    JS_ASSERT(dstStart + count <= getDenseArrayCapacity());
    memcpy(elements + dstStart, src, count * sizeof(js::Value));
}

inline void
JSObject::moveDenseArrayElements(uintN dstStart, uintN srcStart, uintN count)
{
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
CallObjectLambdaName(JSFunction *fun)
{
    return (fun->flags & JSFUN_LAMBDA) ? fun->atom : NULL;
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

inline JSObject *
JSObject::getWithThis() const
{
    return &getFixedSlot(JSSLOT_WITH_THIS).toObject();
}

inline void
JSObject::setWithThis(JSObject *thisp)
{
    getFixedSlotRef(JSSLOT_WITH_THIS).setObject(*thisp);
}

inline bool
JSObject::setSingletonType(JSContext *cx)
{
    if (!cx->typeInferenceEnabled())
        return true;

    JS_ASSERT(!lastProperty()->previous());
    JS_ASSERT(!hasLazyType());
    JS_ASSERT_IF(getProto(), type() == getProto()->getNewType(cx, NULL));

    js::types::TypeObject *type = cx->compartment->getLazyType(cx, getProto());
    if (!type)
        return false;

    type_ = type;
    return true;
}

inline js::types::TypeObject *
JSObject::getType(JSContext *cx)
{
    if (hasLazyType())
        makeLazyType(cx);
    return type_;
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
    JS_ASSERT_IF(!isNewborn() && hasSpecialEquality(),
                 newType->hasAnyFlags(js::types::OBJECT_FLAG_SPECIAL_EQUALITY));
    JS_ASSERT_IF(!isNewborn(), !hasSingletonType());
    type_ = newType;
}

inline bool JSObject::setIteratedSingleton(JSContext *cx) {
    return setFlag(cx, js::BaseShape::ITERATED_SINGLETON);
}

inline bool JSObject::setSystem(JSContext *cx) {
    return setFlag(cx, js::BaseShape::SYSTEM);
}

inline bool JSObject::setDelegate(JSContext *cx) {
    return setFlag(cx, js::BaseShape::DELEGATE, GENERATE_SHAPE);
}

inline bool JSObject::setIndexed(JSContext *cx) {
    return setFlag(cx, js::BaseShape::INDEXED);
}

inline bool JSObject::setVarObj(JSContext *cx) {
    return setFlag(cx, js::BaseShape::VAROBJ);
}

inline bool JSObject::setWatched(JSContext *cx) {
    return setFlag(cx, js::BaseShape::WATCHED, GENERATE_SHAPE);
}

inline bool JSObject::isSystem() const { return lastProperty()->hasObjectFlag(js::BaseShape::SYSTEM); }
inline bool JSObject::isDelegate() const { return lastProperty()->hasObjectFlag(js::BaseShape::DELEGATE); }
inline bool JSObject::isVarObj() const { return lastProperty()->hasObjectFlag(js::BaseShape::VAROBJ); }
inline bool JSObject::isExtensible() const { return !lastProperty()->hasObjectFlag(js::BaseShape::NOT_EXTENSIBLE); }
inline bool JSObject::isBoundFunction() const { return lastProperty()->hasObjectFlag(js::BaseShape::BOUND_FUNCTION); }
inline bool JSObject::isIndexed() const { return lastProperty()->hasObjectFlag(js::BaseShape::INDEXED); }
inline bool JSObject::watched() const { return lastProperty()->hasObjectFlag(js::BaseShape::WATCHED); }

inline bool JSObject::hasSpecialEquality() const {
    return !!getClass()->ext.equality;
}

inline bool JSObject::isArguments() const { return isNormalArguments() || isStrictArguments(); }
inline bool JSObject::isArrayBuffer() const { return hasClass(&js::ArrayBufferClass); }
inline bool JSObject::isNormalArguments() const { return hasClass(&js::NormalArgumentsObjectClass); }
inline bool JSObject::isStrictArguments() const { return hasClass(&js::StrictArgumentsObjectClass); }
inline bool JSObject::isNumber() const { return hasClass(&js::NumberClass); }
inline bool JSObject::isBoolean() const { return hasClass(&js::BooleanClass); }
inline bool JSObject::isString() const { return hasClass(&js::StringClass); }
inline bool JSObject::isPrimitive() const { return isNumber() || isString() || isBoolean(); }
inline bool JSObject::isDate() const { return hasClass(&js::DateClass); }
inline bool JSObject::isFunction() const { return hasClass(&js::FunctionClass); }
inline bool JSObject::isObject() const { return hasClass(&js::ObjectClass); }
inline bool JSObject::isWith() const { return hasClass(&js::WithClass); }
inline bool JSObject::isBlock() const { return hasClass(&js::BlockClass); }
inline bool JSObject::isStaticBlock() const { return isBlock() && !getProto(); }
inline bool JSObject::isClonedBlock() const { return isBlock() && !!getProto(); }
inline bool JSObject::isCall() const { return hasClass(&js::CallClass); }
inline bool JSObject::isDeclEnv() const { return hasClass(&js::DeclEnvClass); }
inline bool JSObject::isRegExp() const { return hasClass(&js::RegExpClass); }
inline bool JSObject::isScript() const { return hasClass(&js::ScriptClass); }
inline bool JSObject::isGenerator() const { return hasClass(&js::GeneratorClass); }
inline bool JSObject::isIterator() const { return hasClass(&js::IteratorClass); }
inline bool JSObject::isStopIteration() const { return hasClass(&js::StopIterationClass); }
inline bool JSObject::isError() const { return hasClass(&js::ErrorClass); }
inline bool JSObject::isXML() const { return hasClass(&js::XMLClass); }
inline bool JSObject::isNamespace() const { return hasClass(&js::NamespaceClass); }
inline bool JSObject::isWeakMap() const { return hasClass(&js::WeakMapClass); }
inline bool JSObject::isFunctionProxy() const { return hasClass(&js::FunctionProxyClass); }

inline bool JSObject::isArray() const
{
    return isSlowArray() || isDenseArray();
}

inline bool JSObject::isDenseArray() const
{
    bool result = hasClass(&js::ArrayClass);
    JS_ASSERT_IF(result, elements != js::emptyObjectElements);
    return result;
}

inline bool JSObject::isSlowArray() const
{
    bool result = hasClass(&js::SlowArrayClass);
    JS_ASSERT_IF(result, elements != js::emptyObjectElements);
    return result;
}

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

inline void
JSObject::init(JSContext *cx, js::types::TypeObject *type)
{
    slots = NULL;
    elements = js::emptyObjectElements;

    setType(type);
}

inline void
JSObject::initDenseArray()
{
    JS_ASSERT(hasClass(&js::ArrayClass));

    JS_STATIC_ASSERT(sizeof(js::ObjectElements) == 2 * sizeof(js::Value));
    JS_ASSERT(numFixedSlots() >= 2);

    /* Fill in the object's inline elements header. */
    elements = fixedElements();
    new (getElementsHeader()) js::ObjectElements(numFixedSlots() - 2);
}

inline void
JSObject::finish(JSContext *cx)
{
    if (hasDynamicSlots())
        cx->free_(slots);
    if (hasDynamicElements())
        cx->free_(getElementsHeader());
}

inline bool
JSObject::initSharingEmptyShape(JSContext *cx,
                                js::Class *aclasp,
                                js::types::TypeObject *type,
                                void *privateValue,
                                js::gc::AllocKind kind)
{
    init(cx, type);

    js::EmptyShape *empty = type->getEmptyShape(cx, aclasp, kind);
    if (!empty)
        return false;

    if (!setInitialProperty(cx, empty))
        return false;

    if (privateValue)
        setPrivate(privateValue);

    JS_ASSERT(!isDenseArray());
    return true;
}

inline bool
JSObject::hasProperty(JSContext *cx, jsid id, bool *foundp, uintN flags)
{
    JSObject *pobj;
    JSProperty *prop;
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
    JSSecurityCallbacks *cb = JS_GetSecurityCallbacks(cx);
    if (JSObjectPrincipalsFinder finder = cb ? cb->findObjectPrincipals : NULL)
        return finder(cx, this);
    return cx->compartment ? cx->compartment->principals : NULL;
}

inline uint32
JSObject::slotSpan() const
{
    JS_ASSERT(!isNewborn());
    if (inDictionaryMode())
        return lastProperty()->base()->slotSpan();
    return lastProperty()->slotSpan();
}

inline bool
JSObject::containsSlot(uint32 slot) const
{
    return slot < slotSpan();
}

inline js::Value &
JSObject::nativeGetSlotRef(uintN slot)
{
    JS_ASSERT(isNative());
    JS_ASSERT(containsSlot(slot));
    return getSlotRef(slot);
}

inline const js::Value &
JSObject::nativeGetSlot(uintN slot) const
{
    JS_ASSERT(isNative());
    JS_ASSERT(containsSlot(slot));
    return getSlot(slot);
}

inline JSFunction *
JSObject::nativeGetMethod(const js::Shape *shape) const
{
    /*
     * For method shapes, this object must have an uncloned function object in
     * the shape's slot.
     */
    JS_ASSERT(shape->isMethod());
#ifdef DEBUG
    JSObject *obj = &nativeGetSlot(shape->slot()).toObject();
    JS_ASSERT(obj->isFunction() && !obj->toFunction()->isClonedMethod());
#endif

    return static_cast<JSFunction *>(&nativeGetSlot(shape->slot()).toObject());
}

inline void
JSObject::nativeSetSlot(uintN slot, const js::Value &value)
{
    JS_ASSERT(isNative());
    JS_ASSERT(containsSlot(slot));
    return setSlot(slot, value);
}

inline void
JSObject::nativeSetSlotWithType(JSContext *cx, const js::Shape *shape, const js::Value &value)
{
    nativeSetSlot(shape->slot(), value);
    js::types::AddTypePropertyId(cx, this, shape->propid(), value);
}

inline bool
JSObject::isNative() const
{
    return lastProperty()->isNative();
}

inline js::Shape **
JSObject::nativeSearch(JSContext *cx, jsid id, bool adding)
{
    return js::Shape::search(cx, &shape_, id, adding);
}

inline const js::Shape *
JSObject::nativeLookup(JSContext *cx, jsid id)
{
    JS_ASSERT(isNative());
    return SHAPE_FETCH(nativeSearch(cx, id));
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

inline bool
JSObject::inDictionaryMode() const
{
    return lastProperty()->inDictionary();
}

inline uint32
JSObject::propertyCount() const
{
    return lastProperty()->entryCount();
}

inline bool
JSObject::hasPropertyTable() const
{
    return lastProperty()->hasTable();
}

inline size_t
JSObject::structSize() const
{
    if (isFunction())
        return sizeof(JSFunction);
    uint32 nfixed = numFixedSlots() + (hasPrivate() ? 1 : 0);
    return sizeof(JSObject) + (nfixed * sizeof(js::Value));
}

inline size_t
JSObject::slotsAndStructSize() const
{
    int ndslots = numDynamicSlots();
    if (hasDynamicElements())
        ndslots += getElementsHeader()->capacity;
    return structSize() + ndslots * sizeof(js::Value);
}

inline JSBool
JSObject::lookupGeneric(JSContext *cx, jsid id, JSObject **objp, JSProperty **propp)
{
    js::LookupGenericOp op = getOps()->lookupGeneric;
    return (op ? op : js_LookupProperty)(cx, this, id, objp, propp);
}

inline JSBool
JSObject::lookupProperty(JSContext *cx, js::PropertyName *name, JSObject **objp, JSProperty **propp)
{
    return lookupGeneric(cx, ATOM_TO_JSID(name), objp, propp);
}

inline JSBool
JSObject::defineGeneric(JSContext *cx, jsid id, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        uintN attrs /* = JSPROP_ENUMERATE */)
{
    js::DefineGenericOp op = getOps()->defineGeneric;
    return (op ? op : js_DefineProperty)(cx, this, id, &value, getter, setter, attrs);
}

inline JSBool
JSObject::defineProperty(JSContext *cx, js::PropertyName *name, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        uintN attrs /* = JSPROP_ENUMERATE */)
{
    return defineGeneric(cx, ATOM_TO_JSID(name), value, getter, setter, attrs);
}

inline JSBool
JSObject::defineElement(JSContext *cx, uint32 index, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        uintN attrs /* = JSPROP_ENUMERATE */)
{
    js::DefineElementOp op = getOps()->defineElement;
    return (op ? op : js_DefineElement)(cx, this, index, &value, getter, setter, attrs);
}

inline JSBool
JSObject::defineSpecial(JSContext *cx, js::SpecialId sid, const js::Value &value,
                        JSPropertyOp getter /* = JS_PropertyStub */,
                        JSStrictPropertyOp setter /* = JS_StrictPropertyStub */,
                        uintN attrs /* = JSPROP_ENUMERATE */)
{
    return defineGeneric(cx, SPECIALID_TO_JSID(sid), value, getter, setter, attrs);
}

inline JSBool
JSObject::lookupElement(JSContext *cx, uint32 index, JSObject **objp, JSProperty **propp)
{
    js::LookupElementOp op = getOps()->lookupElement;
    return (op ? op : js_LookupElement)(cx, this, index, objp, propp);
}

inline JSBool
JSObject::lookupSpecial(JSContext *cx, js::SpecialId sid, JSObject **objp, JSProperty **propp)
{
    return lookupGeneric(cx, SPECIALID_TO_JSID(sid), objp, propp);
}

inline JSBool
JSObject::getElement(JSContext *cx, JSObject *receiver, uint32 index, js::Value *vp)
{
    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGeneric(cx, receiver, id, vp);
}

inline JSBool
JSObject::getElement(JSContext *cx, uint32 index, js::Value *vp)
{
    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGeneric(cx, id, vp);
}

inline JSBool
JSObject::getSpecial(JSContext *cx, js::SpecialId sid, js::Value *vp)
{
    return getGeneric(cx, SPECIALID_TO_JSID(sid), vp);
}

inline JSBool
JSObject::getGenericAttributes(JSContext *cx, jsid id, uintN *attrsp)
{
    js::GenericAttributesOp op = getOps()->getGenericAttributes;
    return (op ? op : js_GetAttributes)(cx, this, id, attrsp);    
}

inline JSBool
JSObject::getPropertyAttributes(JSContext *cx, js::PropertyName *name, uintN *attrsp)
{
    return getGenericAttributes(cx, ATOM_TO_JSID(name), attrsp);
}

inline JSBool
JSObject::getElementAttributes(JSContext *cx, uint32 index, uintN *attrsp)
{
    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    return getGenericAttributes(cx, id, attrsp);
}

inline JSBool
JSObject::getSpecialAttributes(JSContext *cx, js::SpecialId sid, uintN *attrsp)
{
    return getGenericAttributes(cx, SPECIALID_TO_JSID(sid), attrsp);
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

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.toObject().isCallable();
}

inline JSObject *
js_UnwrapWithObject(JSContext *cx, JSObject *withobj)
{
    JS_ASSERT(withobj->isWith());
    return withobj->getProto();
}

namespace js {

inline void
OBJ_TO_INNER_OBJECT(JSContext *cx, JSObject *&obj)
{
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        obj = op(cx, obj);
}

inline void
OBJ_TO_OUTER_OBJECT(JSContext *cx, JSObject *&obj)
{
    if (JSObjectOp op = obj->getClass()->ext.outerObject)
        obj = op(cx, obj);
}

/*
 * Methods to test whether an object or a value is of type "xml" (per typeof).
 */

#define VALUE_IS_XML(v)      (!JSVAL_IS_PRIMITIVE(v) && JSVAL_TO_OBJECT(v)->isXML())

static inline bool
IsXML(const js::Value &v)
{
    return v.isObject() && v.toObject().isXML();
}

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
      : AutoGCRooter(cx, DESCRIPTORS), descriptors(cx)
    { }

    PropDesc *append() {
        if (!descriptors.append(PropDesc()))
            return NULL;
        return &descriptors.back();
    }

    PropDesc& operator[](size_t i) {
        JS_ASSERT(i < descriptors.length());
        return descriptors[i];
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    PropDescArray descriptors;
};

class AutoPropertyDescriptorRooter : private AutoGCRooter, public PropertyDescriptor
{
  public:
    AutoPropertyDescriptorRooter(JSContext *cx) : AutoGCRooter(cx, DESCRIPTOR) {
        obj = NULL;
        attrs = 0;
        getter = (PropertyOp) NULL;
        setter = (StrictPropertyOp) NULL;
        value.setUndefined();
    }

    AutoPropertyDescriptorRooter(JSContext *cx, PropertyDescriptor *desc)
      : AutoGCRooter(cx, DESCRIPTOR)
    {
        obj = desc->obj;
        attrs = desc->attrs;
        getter = desc->getter;
        setter = desc->setter;
        value = desc->value;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
};

static inline bool
InitScopeForObject(JSContext* cx, JSObject* obj, js::Class *clasp, JSObject *parent,
                   js::types::TypeObject *type, gc::AllocKind kind)
{
    JS_ASSERT(clasp->isNative());

    /* Share proto's emptyShape only if obj is similar to proto. */
    js::EmptyShape *empty = NULL;

    if (type->canProvideEmptyShape(clasp) && parent == type->proto->getParent())
        empty = type->getEmptyShape(cx, clasp, kind);
    else
        empty = js::EmptyShape::create(cx, clasp, parent, gc::GetGCKindSlots(kind, clasp));
    if (!empty) {
        JS_ASSERT(obj->isNewborn());
        return false;
    }

    return obj->setInitialProperty(cx, empty);
}

static inline bool
InitScopeForNonNativeObject(JSContext *cx, JSObject *obj, js::Class *clasp, JSObject *parent,
                            gc::AllocKind kind)
{
    JS_ASSERT(!clasp->isNative());

    const js::Shape *empty = js::BaseShape::lookupInitialShape(cx, clasp, parent, kind);
    if (!empty)
        return false;
    JS_ASSERT(empty->isEmptyShape());

    obj->setInitialPropertyInfallible(empty);
    return true;
}

static inline bool
CanBeFinalizedInBackground(gc::AllocKind kind, Class *clasp)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the finalize kind. For example,
     * FINALIZE_OBJECT0 calls the finalizer on the main thread,
     * FINALIZE_OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundAllocKind is called to prevent recursively incrementing
     * the finalize kind; kind may already be a background finalize kind.
     */
    if (!gc::IsBackgroundAllocKind(kind) && !clasp->finalize)
        return true;
#endif
    return false;
}

/*
 * Helper optimized for creating a native instance of the given class (not the
 * class's prototype object). Use this in preference to NewObject, but use
 * NewBuiltinClassInstance if you need the default class prototype as proto.
 */
static inline JSObject *
NewNativeClassInstance(JSContext *cx, Class *clasp, JSObject *proto, gc::AllocKind kind)
{
    JS_ASSERT(proto);
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);

    types::TypeObject *type = proto->getNewType(cx);
    if (!type)
        return NULL;

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC.
     */

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(cx, type);

    JS_ASSERT(type->canProvideEmptyShape(clasp));

    js::EmptyShape *empty = type->getEmptyShape(cx, clasp, kind);
    if (!empty || !obj->setInitialProperty(cx, empty))
        return NULL;

    return obj;
}

static inline JSObject *
NewNativeClassInstance(JSContext *cx, Class *clasp, JSObject *proto)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewNativeClassInstance(cx, clasp, proto, kind);
}

inline GlobalObject *
GetCurrentGlobal(JSContext *cx)
{
    JSObject *scopeChain = (cx->hasfp()) ? &cx->fp()->scopeChain() : cx->globalObject;
    return scopeChain ? scopeChain->getGlobal() : NULL;
}

bool
FindClassPrototype(JSContext *cx, JSObject *scope, JSProtoKey protoKey, JSObject **protop,
                   Class *clasp);

/*
 * Helper used to create Boolean, Date, RegExp, etc. instances of built-in
 * classes with class prototypes of the same Class. See, e.g., jsdate.cpp,
 * jsregexp.cpp, and js_PrimitiveToObject in jsobj.cpp. Use this to get the
 * right default proto and parent for clasp in cx.
 */
static inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp, gc::AllocKind kind)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JSProtoKey protoKey = JSCLASS_CACHED_PROTO_KEY(clasp);
    JS_ASSERT(protoKey != JSProto_Null);

    /* NB: inline-expanded and specialized version of js_GetClassPrototype. */
    JSObject *global;
    if (!cx->hasfp()) {
        global = JS_ObjectToInnerObject(cx, cx->globalObject);
        if (!global)
            return NULL;
    } else {
        global = cx->fp()->scopeChain().getGlobal();
    }
    JS_ASSERT(global->isGlobal());

    const Value &v = global->getReservedSlot(JSProto_LIMIT + protoKey);
    JSObject *proto;
    if (v.isObject()) {
        proto = &v.toObject();
    } else {
        if (!FindClassPrototype(cx, global, protoKey, &proto, clasp))
            return NULL;
    }
    JS_ASSERT(proto->getParent() == global);

    return NewNativeClassInstance(cx, clasp, proto, kind);
}

static inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewBuiltinClassInstance(cx, clasp, kind);
}

static inline JSProtoKey
GetClassProtoKey(js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

namespace WithProto {
    enum e {
        Class = 0,
        Given = 1
    };
}

/*
 * Create an instance of any class, native or not, JSFunction-sized or not.
 *
 * If withProto is 'Class':
 *    If proto is null:
 *      for a built-in class:
 *        use the memoized original value of the class constructor .prototype
 *        property object
 *      else if available
 *        the current value of .prototype
 *      else
 *        Object.prototype.
 *
 *    If parent is null, default it to proto->getParent() if proto is non
 *    null, else to null.
 *
 * If withProto is 'Given':
 *    We allocate an object with exactly the given proto.  A null parent
 *    defaults to proto->getParent() if proto is non-null (else to null).
 *
 * If isFunction is true, return a JSFunction-sized object. If isFunction is
 * false, return a normal object.
 *
 * Note that as a template, there will be lots of instantiations, which means
 * the internals will be specialized based on the template parameters.
 */
static JS_ALWAYS_INLINE bool
FindProto(JSContext *cx, js::Class *clasp, JSObject *parent, JSObject ** proto)
{
    JSProtoKey protoKey = GetClassProtoKey(clasp);
    if (!js_GetClassPrototype(cx, parent, protoKey, proto, clasp))
        return false;
    if (!(*proto) && !js_GetClassPrototype(cx, parent, JSProto_Object, proto))
        return false;

    return true;
}

template <bool withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          gc::AllocKind kind)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (withProto == WithProto::Class && !proto) {
        if (!FindProto(cx, clasp, parent, &proto))
          return NULL;
    }

    types::TypeObject *type = proto ? proto->getNewType(cx) : cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     *
     * The should be specialized by the template.
     */

    JS_ASSERT_IF(clasp == &FunctionClass,
                 kind == JSFunction::FinalizeKind || kind == JSFunction::ExtendedFinalizeKind);

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        goto out;

    obj->init(cx, type);

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    if (!parent && proto)
        parent = proto->getParent();

    if (clasp->isNative()
        ? !InitScopeForObject(cx, obj, clasp, parent, type, kind)
        : !InitScopeForNonNativeObject(cx, obj, clasp, parent, kind)) {
        obj = NULL;
    }

out:
    Probes::createObject(cx, obj);
    return obj;
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObject<withProto>(cx, clasp, proto, parent, kind);
}

static JS_ALWAYS_INLINE JSFunction *
NewFunction(JSContext *cx, JSObject *parent, gc::AllocKind kind)
{
    JSObject *obj = NewObject<WithProto::Class>(cx, &FunctionClass, NULL, parent, kind);
    return static_cast<JSFunction *>(obj);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
               gc::AllocKind kind)
{
    return NewObject<withProto>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(clasp);
    return NewObject<withProto>(cx, clasp, proto, parent, kind);
}

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
static JS_ALWAYS_INLINE JSObject *
NewObjectWithType(JSContext *cx, types::TypeObject *type, JSObject *parent, gc::AllocKind kind)
{
    JS_ASSERT(type->proto->hasNewType(type));

    if (CanBeFinalizedInBackground(kind, &ObjectClass))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        goto out;

    obj->init(cx, type);

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    if (!parent && type->proto)
        parent = type->proto->getParent();

    if (!InitScopeForObject(cx, obj, &ObjectClass, parent, type, kind)) {
        obj = NULL;
        goto out;
    }

out:
    Probes::createObject(cx, obj);
    return obj;
}

extern JSObject *
NewReshapedObject(JSContext *cx, js::types::TypeObject *type, JSObject *parent,
                  gc::AllocKind kind, const Shape *shape);

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

static JS_ALWAYS_INLINE JSObject*
NewObjectWithClassProto(JSContext *cx, Class *clasp, JSObject *proto,
                        gc::AllocKind kind)
{
    JS_ASSERT(clasp->isNative());

    types::TypeObject *type = proto->getNewType(cx);
    if (!type)
        return NULL;

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    if (!obj->initSharingEmptyShape(cx, clasp, type, NULL, kind))
        return NULL;
    return obj;
}

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, JSObject *baseobj, types::TypeObject *type)
{
    JS_ASSERT(baseobj->getClass() == &ObjectClass);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::AllocKind kind = gc::GetGCObjectFixedSlotsKind(baseobj->numFixedSlots());
#ifdef JS_THREADSAFE
    kind = gc::GetBackgroundAllocKind(kind);
#endif
    JS_ASSERT(kind == baseobj->getAllocKind());
    JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);

    if (!obj)
        return NULL;

    obj->setType(type);

    if (!obj->setLastProperty(cx, baseobj->lastProperty()))
        return NULL;

    return obj;
}

inline bool
DefineConstructorAndPrototype(JSContext *cx, GlobalObject *global,
                              JSProtoKey key, JSObject *ctor, JSObject *proto)
{
    JS_ASSERT(!global->nativeEmpty()); /* reserved slots already allocated */
    JS_ASSERT(ctor);
    JS_ASSERT(proto);

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classAtoms[key]);
    JS_ASSERT(!global->nativeLookup(cx, id));

    /* Set these first in case AddTypePropertyId looks for this class. */
    global->setSlot(key, ObjectValue(*ctor));
    global->setSlot(key + JSProto_LIMIT, ObjectValue(*proto));

    types::AddTypePropertyId(cx, global, id, ObjectValue(*ctor));
    if (!global->addDataProperty(cx, id, key + JSProto_LIMIT * 2, 0)) {
        global->setSlot(key, UndefinedValue());
        global->setSlot(key + JSProto_LIMIT, UndefinedValue());
        return false;
    }

    global->setSlot(key + JSProto_LIMIT * 2, ObjectValue(*ctor));
    return true;
}

bool
PropDesc::checkGetter(JSContext *cx)
{
    if (hasGet && !js_IsCallable(get) && !get.isUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                             js_getter_str);
        return false;
    }
    return true;
}

bool
PropDesc::checkSetter(JSContext *cx)
{
    if (hasSet && !js_IsCallable(set) && !set.isUndefined()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GET_SET_FIELD,
                             js_setter_str);
        return false;
    }
    return true;
}

namespace detail {

template<typename T> class PrimitiveBehavior { };

template<>
class PrimitiveBehavior<JSString *> {
  public:
    static inline bool isType(const Value &v) { return v.isString(); }
    static inline JSString *extract(const Value &v) { return v.toString(); }
    static inline Class *getClass() { return &StringClass; }
};

template<>
class PrimitiveBehavior<bool> {
  public:
    static inline bool isType(const Value &v) { return v.isBoolean(); }
    static inline bool extract(const Value &v) { return v.toBoolean(); }
    static inline Class *getClass() { return &BooleanClass; }
};

template<>
class PrimitiveBehavior<double> {
  public:
    static inline bool isType(const Value &v) { return v.isNumber(); }
    static inline double extract(const Value &v) { return v.toNumber(); }
    static inline Class *getClass() { return &NumberClass; }
};

} /* namespace detail */

inline JSObject *
NonGenericMethodGuard(JSContext *cx, CallArgs args, Native native, Class *clasp, bool *ok)
{
    const Value &thisv = args.thisv();
    if (thisv.isObject()) {
        JSObject &obj = thisv.toObject();
        if (obj.getClass() == clasp) {
            *ok = true;  /* quell gcc overwarning */
            return &obj;
        }
    }

    *ok = HandleNonGenericMethodClassMismatch(cx, args, native, clasp);
    return NULL;
}

template <typename T>
inline bool
BoxedPrimitiveMethodGuard(JSContext *cx, CallArgs args, Native native, T *v, bool *ok)
{
    typedef detail::PrimitiveBehavior<T> Behavior;

    const Value &thisv = args.thisv();
    if (Behavior::isType(thisv)) {
        *v = Behavior::extract(thisv);
        return true;
    }

    if (!NonGenericMethodGuard(cx, args, native, Behavior::getClass(), ok))
        return false;

    *v = Behavior::extract(thisv.toObject().getPrimitiveThis());
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
    }
    JS_NOT_REACHED("bad classValue");
    return false;
}

static JS_ALWAYS_INLINE bool
ValueIsSpecial(JSObject *obj, Value *propval, SpecialId *sidp, JSContext *cx)
{
    if (!propval->isObject())
        return false;

#if JS_HAS_XML_SUPPORT
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

} /* namespace js */

inline JSObject *
js_GetProtoIfDenseArray(JSObject *obj)
{
    return obj->isDenseArray() ? obj->getProto() : obj;
}

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

#endif /* jsobjinlines_h___ */
