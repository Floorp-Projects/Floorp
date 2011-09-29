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
#include "jsstaticcheck.h"
#include "jstypedarray.h"
#include "jsxml.h"

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
#include "jsprobes.h"
#include "jsscopeinlines.h"

inline bool
JSObject::preventExtensions(JSContext *cx, js::AutoIdVector *props)
{
    JS_ASSERT(isExtensible());

    if (js::FixOp fix = getOps()->fix) {
        bool success;
        if (!fix(cx, this, &success, props))
            return false;
        if (!success) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CHANGE_EXTENSIBILITY);
            return false;
        }
    } else {
        if (!GetPropertyNames(cx, this, JSITER_HIDDEN | JSITER_OWNONLY, props))
            return false;
    }

    if (isNative())
        extensibleShapeChange(cx);

    flags |= NOT_EXTENSIBLE;
    return true;
}

inline bool
JSObject::brand(JSContext *cx)
{
    JS_ASSERT(!generic());
    JS_ASSERT(!branded());
    JS_ASSERT(isNative());
    JS_ASSERT(!cx->typeInferenceEnabled());
    generateOwnShape(cx);
    if (js_IsPropertyCacheDisabled(cx))  // check for rt->shapeGen overflow
        return false;
    flags |= BRANDED;
    return true;
}

inline bool
JSObject::unbrand(JSContext *cx)
{
    JS_ASSERT(isNative());
    if (branded()) {
        generateOwnShape(cx);
        if (js_IsPropertyCacheDisabled(cx))  // check for rt->shapeGen overflow
            return false;
        flags &= ~BRANDED;
    }
    setGeneric();
    return true;
}

inline JSBool
JSObject::setAttributes(JSContext *cx, jsid id, uintN *attrsp)
{
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::AttributesOp op = getOps()->setAttributes;
    return (op ? op : js_SetAttributes)(cx, this, id, attrsp);
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
JSObject::deleteProperty(JSContext *cx, jsid id, js::Value *rval, JSBool strict)
{
    js::types::AddTypePropertyId(cx, this, id,
                                 js::types::Type::UndefinedType());
    js::types::MarkTypePropertyConfigured(cx, this, id);
    js::DeleteIdOp op = getOps()->deleteProperty;
    return (op ? op : js_DeleteProperty)(cx, this, id, rval, strict);
}

inline void
JSObject::syncSpecialEquality()
{
    if (getClass()->ext.equality) {
        flags |= JSObject::HAS_EQUALITY;
        JS_ASSERT_IF(!hasLazyType(), type()->hasAnyFlags(js::types::OBJECT_FLAG_SPECIAL_EQUALITY));
    }
}

inline void
JSObject::finalize(JSContext *cx)
{
    /* Cope with stillborn objects that have no map. */
    if (isNewborn())
        return;

    js::Probes::finalizeObject(this);

    /* Finalize obj first, in case it needs map and slots. */
    js::Class *clasp = getClass();
    if (clasp->finalize)
        clasp->finalize(cx, this);

    finish(cx);
}

/* 
 * Initializer for Call objects for functions and eval frames. Set class,
 * parent, map, and shape, and allocate slots.
 */
inline void
JSObject::initCall(JSContext *cx, const js::Bindings &bindings, JSObject *parent)
{
    init(cx, &js::CallClass, &js::types::emptyTypeObject, parent, NULL, false);
    lastProp = bindings.lastShape();

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see js::Bindings::extensibleParents.
     */
    if (bindings.extensibleParents())
        setOwnShape(js_GenerateShape(cx));
    else
        objShape = lastProp->shapeid;
}

/*
 * Initializer for cloned block objects. Set class, prototype, frame, map, and
 * shape.
 */
inline void
JSObject::initClonedBlock(JSContext *cx, js::types::TypeObject *type, js::StackFrame *frame)
{
    init(cx, &js::BlockClass, type, NULL, frame, false);

    /* Cloned blocks copy their prototype's map; it had better be shareable. */
    JS_ASSERT(!getProto()->inDictionaryMode() || getProto()->lastProp->frozen());
    lastProp = getProto()->lastProp;

    /*
     * If the prototype has its own shape, that means the clone should, too; see
     * js::Bindings::extensibleParents.
     */
    if (getProto()->hasOwnShape())
        setOwnShape(js_GenerateShape(cx));
    else
        objShape = lastProp->shapeid;
}

/* 
 * Mark a compile-time block as OWN_SHAPE, indicating that its run-time clones
 * also need unique shapes. See js::Bindings::extensibleParents.
 */
inline void
JSObject::setBlockOwnShape(JSContext *cx) {
    JS_ASSERT(isStaticBlock());
    setOwnShape(js_GenerateShape(cx));
}

/*
 * Property read barrier for deferred cloning of compiler-created function
 * objects optimized as typically non-escaping, ad-hoc methods in obj.
 */
inline const js::Shape *
JSObject::methodReadBarrier(JSContext *cx, const js::Shape &shape, js::Value *vp)
{
    JS_ASSERT(canHaveMethodBarrier());
    JS_ASSERT(hasMethodBarrier());
    JS_ASSERT(nativeContains(cx, shape));
    JS_ASSERT(shape.isMethod());
    JS_ASSERT(shape.methodObject() == vp->toObject());
    JS_ASSERT(shape.writable());
    JS_ASSERT(shape.slot != SHAPE_INVALID_SLOT);
    JS_ASSERT(shape.hasDefaultSetter());
    JS_ASSERT(!isGlobal());  /* i.e. we are not changing the global shape */

    JSObject *funobj = &vp->toObject();
    JSFunction *fun = funobj->getFunctionPrivate();
    JS_ASSERT(fun == funobj);
    JS_ASSERT(fun->isNullClosure());

    funobj = CloneFunctionObject(cx, fun);
    if (!funobj)
        return NULL;
    funobj->setMethodObj(*this);

    /*
     * Replace the method property with an ordinary data property. This is
     * equivalent to this->setProperty(cx, shape.id, vp) except that any
     * watchpoint on the property is not triggered.
     */
    uint32 slot = shape.slot;
    const js::Shape *newshape = methodShapeChange(cx, shape);
    if (!newshape)
        return NULL;
    JS_ASSERT(!newshape->isMethod());
    JS_ASSERT(newshape->slot == slot);
    vp->setObject(*funobj);
    nativeSetSlot(slot, *vp);
    return newshape;
}

static JS_ALWAYS_INLINE bool
ChangesMethodValue(const js::Value &prev, const js::Value &v)
{
    JSObject *prevObj;
    return prev.isObject() && (prevObj = &prev.toObject())->isFunction() &&
           (!v.isObject() || &v.toObject() != prevObj);
}

inline const js::Shape *
JSObject::methodWriteBarrier(JSContext *cx, const js::Shape &shape, const js::Value &v)
{
    if (brandedOrHasMethodBarrier() && shape.slot != SHAPE_INVALID_SLOT) {
        const js::Value &prev = nativeGetSlot(shape.slot);

        if (ChangesMethodValue(prev, v))
            return methodShapeChange(cx, shape);
    }
    return &shape;
}

inline bool
JSObject::methodWriteBarrier(JSContext *cx, uint32 slot, const js::Value &v)
{
    if (brandedOrHasMethodBarrier()) {
        const js::Value &prev = nativeGetSlot(slot);

        if (ChangesMethodValue(prev, v))
            return methodShapeChange(cx, slot);
    }
    return true;
}

inline const js::Value *
JSObject::getRawSlots()
{
    JS_ASSERT(isGlobal());
    return slots;
}

inline const js::Value *
JSObject::getRawSlot(size_t slot, const js::Value *slots)
{
    JS_ASSERT(isGlobal());
    size_t fixed = numFixedSlots();
    if (slot < fixed)
        return fixedSlots() + slot;
    return slots + slot - fixed;
}

inline bool
JSObject::isFixedSlot(size_t slot)
{
    JS_ASSERT(!isDenseArray());
    return slot < numFixedSlots();
}

inline size_t
JSObject::numDynamicSlots(size_t capacity) const
{
    JS_ASSERT(capacity >= numFixedSlots());
    return isDenseArray() ? capacity : capacity - numFixedSlots();
}

inline size_t
JSObject::dynamicSlotIndex(size_t slot)
{
    JS_ASSERT(!isDenseArray() && slot >= numFixedSlots());
    return slot - numFixedSlots();
}

inline bool
JSObject::ensureClassReservedSlots(JSContext *cx)
{
    return !nativeEmpty() || ensureClassReservedSlotsForEmptyObject(cx);
}

inline js::Value
JSObject::getReservedSlot(uintN index) const
{
    return (index < numSlots()) ? getSlot(index) : js::UndefinedValue();
}

inline void
JSObject::setReservedSlot(uintN index, const js::Value &v)
{
    JS_ASSERT(index < JSSLOT_FREE(getClass()));
    setSlot(index, v);
}

inline bool
JSObject::canHaveMethodBarrier() const
{
    return isObject() || isFunction() || isPrimitive() || isDate();
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
JSObject::hasSlotsArray() const
{
    JS_ASSERT_IF(!slots, !isDenseArray());
    JS_ASSERT_IF(slots == fixedSlots(), isDenseArray() || isArrayBuffer());
    return slots && slots != fixedSlots();
}

inline bool
JSObject::hasContiguousSlots(size_t start, size_t count) const
{
    /*
     * Check that the range [start, start+count) is either all inline or all
     * out of line.
     */
    JS_ASSERT(start + count <= numSlots());
    return (start + count <= numFixedSlots()) || (start >= numFixedSlots());
}

inline size_t
JSObject::structSize() const
{
    return (isFunction() && !getPrivate())
           ? sizeof(JSFunction)
           : (sizeof(JSObject) + sizeof(js::Value) * numFixedSlots());
}

inline size_t
JSObject::slotsAndStructSize() const
{
    int ndslots = 0;
    if (isDenseArray()) {
        if (!denseArrayHasInlineSlots())
            ndslots = numSlots();
    } else {
        if (slots)
            ndslots = numSlots() - numFixedSlots();
    }

    return structSize() + sizeof(js::Value) * ndslots;
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return (uint32)(uintptr_t) getPrivate();
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

    setPrivate((void*)(uintptr_t) length);
}

inline void
JSObject::setDenseArrayLength(uint32 length)
{
    /* Variant of setArrayLength for use on dense arrays where the length cannot overflow int32. */
    JS_ASSERT(isDenseArray());
    JS_ASSERT(length <= INT32_MAX);
    setPrivate((void*)(uintptr_t) length);
}

inline uint32
JSObject::getDenseArrayCapacity()
{
    JS_ASSERT(isDenseArray());
    return numSlots();
}

inline const js::Value *
JSObject::getDenseArrayElements()
{
    JS_ASSERT(isDenseArray());
    return slots;
}

inline const js::Value &
JSObject::getDenseArrayElement(uintN idx)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    return slots[idx];
}

inline void
JSObject::setDenseArrayElement(uintN idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    slots[idx] = val;
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
    JS_ASSERT(isDenseArray());
    copySlotRange(dstStart, src, count);
}

inline void
JSObject::moveDenseArrayElements(uintN dstStart, uintN srcStart, uintN count)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(dstStart + count <= capacity);
    JS_ASSERT(srcStart + count <= capacity);
    memmove(slots + dstStart, slots + srcStart, count * sizeof(js::Value));
}

inline void
JSObject::shrinkDenseArrayElements(JSContext *cx, uintN cap)
{
    JS_ASSERT(isDenseArray());
    shrinkSlots(cx, cap);
}

inline bool
JSObject::denseArrayHasInlineSlots() const
{
    JS_ASSERT(isDenseArray() && slots);
    return slots == fixedSlots();
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

inline js::Value *
JSObject::getFlatClosureUpvars() const
{
#ifdef DEBUG
    JSFunction *fun = getFunctionPrivate();
    JS_ASSERT(fun->isFlatClosure());
    JS_ASSERT(fun->script()->bindings.countUpvars() == fun->script()->upvars()->length);
#endif
    return (js::Value *) getFixedSlot(JSSLOT_FLAT_CLOSURE_UPVARS).toPrivate();
}

inline void
JSObject::finalizeUpvarsIfFlatClosure()
{
    /*
     * Cloned function objects may be flat closures with upvars to free.
     *
     * We do not record in the closure objects any flags. Rather we use flags
     * stored in the compiled JSFunction that we get via getFunctionPrivate()
     * to distinguish between closure types. Then during finalization we must
     * ensure that the compiled JSFunction always finalized after the closures
     * so we can safely access it here. Currently the GC ensures that through
     * finalizing JSFunction instances after finalizing any other objects even
     * during the background finalization.
     *
     * But we must not access JSScript here that is stored in JSFunction. The
     * script can be finalized before the function or closure instances. So we
     * just check if JSSLOT_FLAT_CLOSURE_UPVARS holds a private value encoded
     * as a double. We must also ignore newborn closures that do not have the
     * private pointer set.
     *
     * FIXME bug 648320 - allocate upvars on the GC heap to avoid doing it
     * here explicitly.
     */
    JSFunction *fun = getFunctionPrivate();
    if (fun && fun != this && fun->isFlatClosure()) {
        const js::Value &v = getSlot(JSSLOT_FLAT_CLOSURE_UPVARS);
        if (v.isDouble())
            js::Foreground::free_(v.toPrivate());
    }
}

inline js::Value
JSObject::getFlatClosureUpvar(uint32 i) const
{
    JS_ASSERT(i < getFunctionPrivate()->script()->bindings.countUpvars());
    return getFlatClosureUpvars()[i];
}

inline const js::Value &
JSObject::getFlatClosureUpvar(uint32 i)
{
    JS_ASSERT(i < getFunctionPrivate()->script()->bindings.countUpvars());
    return getFlatClosureUpvars()[i];
}

inline void
JSObject::setFlatClosureUpvar(uint32 i, const js::Value &v)
{
    JS_ASSERT(i < getFunctionPrivate()->script()->bindings.countUpvars());
    getFlatClosureUpvars()[i] = v;
}

inline void
JSObject::setFlatClosureUpvars(js::Value *upvars)
{
    JS_ASSERT(isFunction());
    JS_ASSERT(getFunctionPrivate()->isFlatClosure());
    setFixedSlot(JSSLOT_FLAT_CLOSURE_UPVARS, js::PrivateValue(upvars));
}

inline bool
JSObject::hasMethodObj(const JSObject& obj) const
{
    return JSSLOT_FUN_METHOD_OBJ < numSlots() &&
           getFixedSlot(JSSLOT_FUN_METHOD_OBJ).isObject() &&
           getFixedSlot(JSSLOT_FUN_METHOD_OBJ).toObject() == obj;
}

inline void
JSObject::setMethodObj(JSObject& obj)
{
    setFixedSlot(JSSLOT_FUN_METHOD_OBJ, js::ObjectValue(obj));
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

    JS_ASSERT(!lastProp->previous());
    JS_ASSERT(!hasLazyType());
    JS_ASSERT_IF(getProto(), type() == getProto()->getNewType(cx, NULL));

    flags |= SINGLETON_TYPE | LAZY_TYPE;

    return true;
}

inline js::types::TypeObject *
JSObject::getType(JSContext *cx)
{
    if (hasLazyType())
        makeLazyType(cx);
    return type_;
}

inline js::types::TypeObject *
JSObject::getNewType(JSContext *cx, JSFunction *fun, bool markUnknown)
{
    if (isDenseArray() && !makeDenseArraySlow(cx))
        return NULL;
    if (newType) {
        /*
         * If set, the newType's newScript indicates the script used to create
         * all objects in existence which have this type. If there are objects
         * in existence which are not created by calling 'new' on newScript,
         * we must clear the new script information from the type and will not
         * be able to assume any definite properties for instances of the type.
         * This case is rare, but can happen if, for example, two scripted
         * functions have the same value for their 'prototype' property, or if
         * Object.create is called with a prototype object that is also the
         * 'prototype' property of some scripted function.
         */
        if (newType->newScript && newType->newScript->fun != fun)
            newType->clearNewScript(cx);
        if (markUnknown && cx->typeInferenceEnabled() && !newType->unknownProperties())
            newType->markUnknown(cx);
    } else {
        makeNewType(cx, fun, markUnknown);
    }
    return newType;
}

inline void
JSObject::clearType()
{
    JS_ASSERT(!hasSingletonType());
    type_ = &js::types::emptyTypeObject;
}

inline void
JSObject::setType(js::types::TypeObject *newType)
{
#ifdef DEBUG
    JS_ASSERT(newType);
    for (JSObject *obj = newType->proto; obj; obj = obj->getProto())
        JS_ASSERT(obj != this);
#endif
    JS_ASSERT_IF(hasSpecialEquality(), newType->hasAnyFlags(js::types::OBJECT_FLAG_SPECIAL_EQUALITY));
    JS_ASSERT(!hasSingletonType());
    type_ = newType;
}

inline void
JSObject::init(JSContext *cx, js::Class *aclasp, js::types::TypeObject *type,
               JSObject *parent, void *priv, bool denseArray)
{
    clasp = aclasp;
    flags = capacity << FIXED_SLOTS_SHIFT;

    JS_ASSERT(denseArray == (aclasp == &js::ArrayClass));

#ifdef DEBUG
    /*
     * NB: objShape must not be set here; rather, the caller must call setMap
     * or setSharedNonNativeMap after calling init. To defend this requirement
     * we set objShape to a value that obj->shape() is asserted never to return.
     */
    objShape = INVALID_SHAPE;
#endif

    privateData = priv;

    /*
     * Fill the fixed slots with undefined if needed.  This object must
     * already have its capacity filled in, as by js_NewGCObject. If inference
     * is disabled, NewArray will backfill holes up to the array's capacity
     * and unset the PACKED_ARRAY flag.
     */
    slots = NULL;
    if (denseArray) {
        slots = fixedSlots();
        flags |= PACKED_ARRAY;
    } else {
        js::ClearValueRange(fixedSlots(), capacity, denseArray);
    }

    newType = NULL;
    JS_ASSERT(initializedLength == 0);

    setType(type);
    setParent(parent);
}

inline void
JSObject::finish(JSContext *cx)
{
    if (hasSlotsArray())
        cx->free_(slots);
}

inline bool
JSObject::initSharingEmptyShape(JSContext *cx,
                                js::Class *aclasp,
                                js::types::TypeObject *type,
                                JSObject *parent,
                                void *privateValue,
                                js::gc::AllocKind kind)
{
    init(cx, aclasp, type, parent, privateValue, false);

    JS_ASSERT(!isDenseArray());

    js::EmptyShape *empty = type->getEmptyShape(cx, aclasp, kind);
    if (!empty)
        return false;

    setMap(empty);
    return true;
}

inline bool
JSObject::hasProperty(JSContext *cx, jsid id, bool *foundp, uintN flags)
{
    JSObject *pobj;
    JSProperty *prop;
    JSAutoResolveFlags rf(cx, flags);
    if (!lookupProperty(cx, id, &pobj, &prop))
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
    return lastProp->slotSpan;
}

inline bool
JSObject::containsSlot(uint32 slot) const
{
    return slot < slotSpan();
}

inline void
JSObject::setMap(js::Shape *amap)
{
    JS_ASSERT(!hasOwnShape());
    lastProp = amap;
    objShape = lastProp->shapeid;
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
    nativeSetSlot(shape->slot, value);
    js::types::AddTypePropertyId(cx, this, shape->propid, value);
}

inline bool
JSObject::isNative() const
{
    return lastProp->isNative();
}

inline bool
JSObject::isNewborn() const
{
    return !lastProp;
}

inline void
JSObject::clearOwnShape()
{
    flags &= ~OWN_SHAPE;
    objShape = lastProp->shapeid;
}

inline void
JSObject::setOwnShape(uint32 s)
{
    flags |= OWN_SHAPE;
    objShape = s;
}

inline js::Shape **
JSObject::nativeSearch(JSContext *cx, jsid id, bool adding)
{
    return js::Shape::search(cx, &lastProp, id, adding);
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
    return nativeLookup(cx, shape.propid) == &shape;
}

inline const js::Shape *
JSObject::lastProperty() const
{
    JS_ASSERT(isNative());
    JS_ASSERT(!JSID_IS_VOID(lastProp->propid));
    return lastProp;
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

/*
 * FIXME: shape must not be null, should use a reference here and other places.
 */
inline void
JSObject::setLastProperty(const js::Shape *shape)
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(!JSID_IS_VOID(shape->propid));
    JS_ASSERT_IF(lastProp, !JSID_IS_VOID(lastProp->propid));
    JS_ASSERT(shape->compartment() == compartment());

    lastProp = const_cast<js::Shape *>(shape);
}

inline void
JSObject::removeLastProperty()
{
    JS_ASSERT(!inDictionaryMode());
    JS_ASSERT(!JSID_IS_VOID(lastProp->parent->propid));

    lastProp = lastProp->parent;
}

inline void
JSObject::setSharedNonNativeMap()
{
    setMap(&js::Shape::sharedNonNative);
}

inline JSBool
JSObject::lookupElement(JSContext *cx, uint32 index, JSObject **objp, JSProperty **propp)
{
    js::LookupElementOp op = getOps()->lookupElement;
    return (op ? op : js_LookupElement)(cx, this, index, objp, propp);
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
JSObject::deleteElement(JSContext *cx, uint32 index, js::Value *rval, JSBool strict)
{
    jsid id;
    if (!js::IndexToId(cx, index, &id))
        return false;
    return deleteProperty(cx, id, rval, strict);
}

inline JSBool
JSObject::getSpecial(JSContext *cx, js::SpecialId sid, js::Value *vp)
{
    return getGeneric(cx, SPECIALID_TO_JSID(sid), vp);
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
InitScopeForObject(JSContext* cx, JSObject* obj, js::Class *clasp, js::types::TypeObject *type,
                   gc::AllocKind kind)
{
    JS_ASSERT(clasp->isNative());

    /* Share proto's emptyShape only if obj is similar to proto. */
    js::EmptyShape *empty = NULL;

    uint32 freeslot = JSSLOT_FREE(clasp);
    if (freeslot > obj->numSlots() && !obj->allocSlots(cx, freeslot))
        goto bad;

    if (type->canProvideEmptyShape(clasp))
        empty = type->getEmptyShape(cx, clasp, kind);
    else
        empty = js::EmptyShape::create(cx, clasp);
    if (!empty)
        goto bad;

    obj->setMap(empty);
    return true;

  bad:
    /* The GC nulls map initially. It should still be null on error. */
    JS_ASSERT(obj->isNewborn());
    return false;
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
    if (!gc::IsBackgroundAllocKind(kind) &&
        (!clasp->finalize || clasp->flags & JSCLASS_CONCURRENT_FINALIZER)) {
        return true;
    }
#endif
    return false;
}

/*
 * Helper optimized for creating a native instance of the given class (not the
 * class's prototype object). Use this in preference to NewObject, but use
 * NewBuiltinClassInstance if you need the default class prototype as proto,
 * and its parent global as parent.
 */
static inline JSObject *
NewNativeClassInstance(JSContext *cx, Class *clasp, JSObject *proto,
                       JSObject *parent, gc::AllocKind kind)
{
    JS_ASSERT(proto);
    JS_ASSERT(parent);
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

    if (obj) {
        /*
         * Default parent to the parent of the prototype, which was set from
         * the parent of the prototype's constructor.
         */
        bool denseArray = (clasp == &ArrayClass);
        obj->init(cx, clasp, type, parent, NULL, denseArray);

        JS_ASSERT(type->canProvideEmptyShape(clasp));
        js::EmptyShape *empty = type->getEmptyShape(cx, clasp, kind);

        if (empty)
            obj->setMap(empty);
        else
            obj = NULL;
    }

    return obj;
}

static inline JSObject *
NewNativeClassInstance(JSContext *cx, Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
    return NewNativeClassInstance(cx, clasp, proto, parent, kind);
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
        global = cx->globalObject;
        if (!NULLABLE_OBJ_TO_INNER_OBJECT(cx, global))
            return NULL;
    } else {
        global = cx->fp()->scopeChain().getGlobal();
    }
    JS_ASSERT(global->isGlobal());

    const Value &v = global->getReservedSlot(JSProto_LIMIT + protoKey);
    JSObject *proto;
    if (v.isObject()) {
        proto = &v.toObject();
        JS_ASSERT(proto->getParent() == global);
    } else {
        if (!FindClassPrototype(cx, global, protoKey, &proto, clasp))
            return NULL;
    }

    return NewNativeClassInstance(cx, clasp, proto, global, kind);
}

static inline JSObject *
NewBuiltinClassInstance(JSContext *cx, Class *clasp)
{
    gc::AllocKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
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

namespace detail
{
template <bool withProto, bool isFunction>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          gc::AllocKind kind)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (withProto == WithProto::Class && !proto) {
        if (!FindProto(cx, clasp, parent, &proto))
          return NULL;
    }

    types::TypeObject *type = proto ? proto->getNewType(cx) : &js::types::emptyTypeObject;
    if (!type)
        return NULL;

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     *
     * The should be specialized by the template.
     */

    if (!isFunction && CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = isFunction ? js_NewGCFunction(cx) : js_NewGCObject(cx, kind);
    if (!obj)
        goto out;

    /* This needs to match up with the size of JSFunction::data_padding. */
    JS_ASSERT_IF(isFunction, kind == gc::FINALIZE_OBJECT2);

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(cx, clasp, type,
              (!parent && proto) ? proto->getParent() : parent,
              NULL, clasp == &ArrayClass);

    if (clasp->isNative()) {
        if (!InitScopeForObject(cx, obj, clasp, type, kind)) {
            obj = NULL;
            goto out;
        }
    } else {
        obj->setSharedNonNativeMap();
    }

out:
    Probes::createObject(cx, obj);
    return obj;
}
} /* namespace detail */

static JS_ALWAYS_INLINE JSObject *
NewFunction(JSContext *cx, js::GlobalObject &global)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, &global, JSProto_Function, &proto))
        return NULL;
    return detail::NewObject<WithProto::Given, true>(cx, &FunctionClass, proto, &global,
                                                     gc::FINALIZE_OBJECT2);
}

static JS_ALWAYS_INLINE JSObject *
NewFunction(JSContext *cx, JSObject *parent)
{
    return detail::NewObject<WithProto::Class, true>(cx, &FunctionClass, NULL, parent,
                                                     gc::FINALIZE_OBJECT2);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
               gc::AllocKind kind)
{
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          gc::AllocKind kind)
{
    if (clasp == &FunctionClass)
        return detail::NewObject<withProto, true>(cx, clasp, proto, parent, kind);
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::AllocKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
    return NewObject<withProto>(cx, clasp, proto, parent, kind);
}

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
static JS_ALWAYS_INLINE JSObject *
NewObjectWithType(JSContext *cx, types::TypeObject *type, JSObject *parent, gc::AllocKind kind)
{
    JS_ASSERT(type == type->proto->newType);

    if (CanBeFinalizedInBackground(kind, &ObjectClass))
        kind = GetBackgroundAllocKind(kind);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        goto out;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(cx, &ObjectClass, type,
              (!parent && type->proto) ? type->proto->getParent() : parent,
              NULL, false);

    if (!InitScopeForObject(cx, obj, &ObjectClass, type, kind)) {
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
GuessObjectGCKind(size_t numSlots, bool isArray)
{
    if (numSlots)
        return gc::GetGCObjectKind(numSlots, isArray);
    return isArray ? gc::FINALIZE_OBJECT8 : gc::FINALIZE_OBJECT4;
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

    if (!obj->initSharingEmptyShape(cx, clasp, type, proto->getParent(), NULL, kind))
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

    if (!obj || !obj->ensureSlots(cx, baseobj->numSlots()))
        return NULL;

    obj->setType(type);
    obj->flags = baseobj->flags;
    obj->lastProp = baseobj->lastProp;
    obj->objShape = baseobj->objShape;

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
NonGenericMethodGuard(JSContext *cx, CallArgs args, Class *clasp, bool *ok)
{
    const Value &thisv = args.thisv();
    if (thisv.isObject()) {
        JSObject &obj = thisv.toObject();
        if (obj.getClass() == clasp) {
            *ok = true;  /* quell gcc overwarning */
            return &obj;
        }
    }

    *ok = HandleNonGenericMethodClassMismatch(cx, args, clasp);
    return NULL;
}

template <typename T>
inline bool
BoxedPrimitiveMethodGuard(JSContext *cx, CallArgs args, T *v, bool *ok)
{
    typedef detail::PrimitiveBehavior<T> Behavior;

    const Value &thisv = args.thisv();
    if (Behavior::isType(thisv)) {
        *v = Behavior::extract(thisv);
        return true;
    }

    if (!NonGenericMethodGuard(cx, args, Behavior::getClass(), ok))
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

} /* namespace js */

inline JSObject *
js_GetProtoIfDenseArray(JSObject *obj)
{
    return obj->isDenseArray() ? obj->getProto() : obj;
}

#endif /* jsobjinlines_h___ */
