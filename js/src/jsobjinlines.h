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
#include "jsscriptinlines.h"
#include "jsstr.h"

#include "vm/GlobalObject.h"

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

inline void
JSObject::syncSpecialEquality()
{
    if (getClass()->ext.equality)
        flags |= JSObject::HAS_EQUALITY;
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
    init(cx, &js_CallClass, NULL, parent, NULL, false);
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
JSObject::initClonedBlock(JSContext *cx, JSObject *proto, js::StackFrame *frame)
{
    init(cx, &js_BlockClass, proto, NULL, frame, false);

    /* Cloned blocks copy their prototype's map; it had better be shareable. */
    JS_ASSERT(!proto->inDictionaryMode() || proto->lastProp->frozen());
    lastProp = proto->lastProp;

    /*
     * If the prototype has its own shape, that means the clone should, too; see
     * js::Bindings::extensibleParents.
     */
    if (proto->hasOwnShape())
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
    JS_ASSERT(nativeContains(shape));
    JS_ASSERT(shape.isMethod());
    JS_ASSERT(shape.methodObject() == vp->toObject());
    JS_ASSERT(shape.writable());
    JS_ASSERT(shape.slot != SHAPE_INVALID_SLOT);
    JS_ASSERT(shape.hasDefaultSetter());
    JS_ASSERT(!isGlobal());  /* i.e. we are not changing the global shape */

    JSObject *funobj = &vp->toObject();
    JSFunction *fun = funobj->getFunctionPrivate();
    JS_ASSERT(fun == funobj);
    JS_ASSERT(FUN_NULL_CLOSURE(fun));

    funobj = CloneFunctionObject(cx, fun, funobj->getParent());
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

inline bool
JSObject::isPrimitive() const
{
    return isNumber() || isString() || isBoolean();
}

inline const js::Value &
JSObject::getPrimitiveThis() const
{
    JS_ASSERT(isPrimitive());
    return getSlot(JSSLOT_PRIMITIVE_THIS);
}

inline void
JSObject::setPrimitiveThis(const js::Value &pthis)
{
    JS_ASSERT(isPrimitive());
    setSlot(JSSLOT_PRIMITIVE_THIS, pthis);
}

inline /* gc::FinalizeKind */ unsigned
JSObject::finalizeKind() const
{
    return js::gc::FinalizeKind(arenaHeader()->getThingKind());
}

inline size_t
JSObject::numFixedSlots() const
{
    if (isFunction())
        return JSObject::FUN_CLASS_RESERVED_SLOTS;
    if (!hasSlotsArray())
        return capacity;
    return js::gc::GetGCKindSlots(js::gc::FinalizeKind(finalizeKind()));
}

inline size_t
JSObject::slotsAndStructSize(uint32 nslots) const
{
    bool isFun = isFunction() && this == (JSObject*) getPrivate();

    int ndslots = hasSlotsArray() ? nslots : 0;
    int nfslots = isFun ? 0 : numFixedSlots();

    return sizeof(js::Value) * (ndslots + nfslots)
           + (isFun ? sizeof(JSFunction) : sizeof(JSObject));
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return (uint32)(uintptr_t) getPrivate();
}

inline void
JSObject::setArrayLength(uint32 length)
{
    JS_ASSERT(isArray());
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
    return getSlots();
}

inline const js::Value &
JSObject::getDenseArrayElement(uintN idx)
{
    JS_ASSERT(isDenseArray());
    return getSlot(idx);
}

inline void
JSObject::setDenseArrayElement(uintN idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray());
    setSlot(idx, val);
}

inline void
JSObject::copyDenseArrayElements(uintN dstStart, const js::Value *src, uintN count)
{
    JS_ASSERT(isDenseArray());
    copySlots(dstStart, src, count);
}

inline void
JSObject::moveDenseArrayElements(uintN dstStart, uintN srcStart, uintN count)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(dstStart + count <= capacity);
    JS_ASSERT(srcStart + count <= capacity);
    memmove(slots + dstStart, getSlots() + srcStart, count * sizeof(js::Value));
}

inline void
JSObject::shrinkDenseArrayElements(JSContext *cx, uintN cap)
{
    JS_ASSERT(isDenseArray());
    shrinkSlots(cx, cap);
}

inline bool
JSObject::callIsForEval() const
{
    JS_ASSERT(isCall());
    JS_ASSERT(getSlot(JSSLOT_CALL_CALLEE).isObjectOrNull());
    JS_ASSERT_IF(getSlot(JSSLOT_CALL_CALLEE).isObject(),
                 getSlot(JSSLOT_CALL_CALLEE).toObject().isFunction());
    return getSlot(JSSLOT_CALL_CALLEE).isNull();
}

inline js::StackFrame *
JSObject::maybeCallObjStackFrame() const
{
    JS_ASSERT(isCall());
    return reinterpret_cast<js::StackFrame *>(getPrivate());
}

inline void
JSObject::setCallObjCallee(JSObject *callee)
{
    JS_ASSERT(isCall());
    JS_ASSERT_IF(callee, callee->isFunction());
    setSlot(JSSLOT_CALL_CALLEE, js::ObjectOrNullValue(callee));
}

inline JSObject *
JSObject::getCallObjCallee() const
{
    JS_ASSERT(isCall());
    return getSlot(JSSLOT_CALL_CALLEE).toObjectOrNull();
}

inline JSFunction *
JSObject::getCallObjCalleeFunction() const
{
    JS_ASSERT(isCall());
    return getSlot(JSSLOT_CALL_CALLEE).toObject().getFunctionPrivate();
}

inline const js::Value &
JSObject::getCallObjArguments() const
{
    JS_ASSERT(isCall());
    JS_ASSERT(!callIsForEval());
    return getSlot(JSSLOT_CALL_ARGUMENTS);
}

inline void
JSObject::setCallObjArguments(const js::Value &v)
{
    JS_ASSERT(isCall());
    JS_ASSERT(!callIsForEval());
    setSlot(JSSLOT_CALL_ARGUMENTS, v);
}

inline const js::Value &
JSObject::callObjArg(uintN i) const
{
    JS_ASSERT(isCall());
    JS_ASSERT(i < getCallObjCalleeFunction()->nargs);
    return getSlot(JSObject::CALL_RESERVED_SLOTS + i);
}

inline void
JSObject::setCallObjArg(uintN i, const js::Value &v)
{
    JS_ASSERT(isCall());
    JS_ASSERT(i < getCallObjCalleeFunction()->nargs);
    setSlot(JSObject::CALL_RESERVED_SLOTS + i, v);
}

inline const js::Value &
JSObject::callObjVar(uintN i) const
{
    JSFunction *fun = getCallObjCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());
    JS_ASSERT(i < fun->script()->bindings.countVars());
    return getSlot(JSObject::CALL_RESERVED_SLOTS + fun->nargs + i);
}

inline void
JSObject::setCallObjVar(uintN i, const js::Value &v)
{
    JSFunction *fun = getCallObjCalleeFunction();
    JS_ASSERT(fun->nargs == fun->script()->bindings.countArgs());
    JS_ASSERT(i < fun->script()->bindings.countVars());
    setSlot(JSObject::CALL_RESERVED_SLOTS + fun->nargs + i, v);
}

inline const js::Value &
JSObject::getDateUTCTime() const
{
    JS_ASSERT(isDate());
    return getSlot(JSSLOT_DATE_UTC_TIME);
}

inline void 
JSObject::setDateUTCTime(const js::Value &time)
{
    JS_ASSERT(isDate());
    setSlot(JSSLOT_DATE_UTC_TIME, time);
}

inline js::Value *
JSObject::getFlatClosureUpvars() const
{
#ifdef DEBUG
    JSFunction *fun = getFunctionPrivate();
    JS_ASSERT(fun->isFlatClosure());
    JS_ASSERT(fun->script()->bindings.countUpvars() == fun->script()->upvars()->length);
#endif
    return (js::Value *) getSlot(JSSLOT_FLAT_CLOSURE_UPVARS).toPrivate();
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
    JS_ASSERT(FUN_FLAT_CLOSURE(getFunctionPrivate()));
    setSlot(JSSLOT_FLAT_CLOSURE_UPVARS, PrivateValue(upvars));
}

inline bool
JSObject::hasMethodObj(const JSObject& obj) const
{
    return JSSLOT_FUN_METHOD_OBJ < numSlots() &&
           getSlot(JSSLOT_FUN_METHOD_OBJ).isObject() &&
           getSlot(JSSLOT_FUN_METHOD_OBJ).toObject() == obj;
}

inline void
JSObject::setMethodObj(JSObject& obj)
{
    setSlot(JSSLOT_FUN_METHOD_OBJ, js::ObjectValue(obj));
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
    return js::Jsvalify(getSlot(JSSLOT_NAME_PREFIX));
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
    return js::Jsvalify(getSlot(JSSLOT_NAME_URI));
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
    return js::Jsvalify(getSlot(JSSLOT_NAMESPACE_DECLARED));
}

inline void
JSObject::setNamespaceDeclared(jsval decl)
{
    JS_ASSERT(isNamespace());
    setSlot(JSSLOT_NAMESPACE_DECLARED, js::Valueify(decl));
}

inline JSLinearString *
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    const js::Value &v = getSlot(JSSLOT_QNAME_LOCAL_NAME);
    return !v.isUndefined() ? &v.toString()->asLinear() : NULL;
}

inline jsval
JSObject::getQNameLocalNameVal() const
{
    JS_ASSERT(isQName());
    return js::Jsvalify(getSlot(JSSLOT_QNAME_LOCAL_NAME));
}

inline void
JSObject::setQNameLocalName(JSLinearString *name)
{
    JS_ASSERT(isQName());
    setSlot(JSSLOT_QNAME_LOCAL_NAME, name ? js::StringValue(name) : js::UndefinedValue());
}

inline JSObject *
JSObject::getWithThis() const
{
    return &getSlot(JSSLOT_WITH_THIS).toObject();
}

inline void
JSObject::setWithThis(JSObject *thisp)
{
    setSlot(JSSLOT_WITH_THIS, js::ObjectValue(*thisp));
}

inline void
JSObject::init(JSContext *cx, js::Class *aclasp, JSObject *proto, JSObject *parent,
               void *priv, bool useHoles)
{
    clasp = aclasp;
    flags = 0;

#ifdef DEBUG
    /*
     * NB: objShape must not be set here; rather, the caller must call setMap
     * or setSharedNonNativeMap after calling init. To defend this requirement
     * we set objShape to a value that obj->shape() is asserted never to return.
     */
    objShape = INVALID_SHAPE;
#endif

    setProto(proto);
    setParent(parent);

    privateData = priv;
    slots = fixedSlots();

    /*
     * Fill the fixed slots with undefined or array holes.  This object must
     * already have its capacity filled in, as by js_NewGCObject.
     */
    JS_ASSERT(capacity == numFixedSlots());
    ClearValueRange(slots, capacity, useHoles);

    emptyShapes = NULL;
}

inline void
JSObject::finish(JSContext *cx)
{
    if (hasSlotsArray())
        freeSlotsArray(cx);
    if (emptyShapes)
        cx->free_(emptyShapes);
}

inline bool
JSObject::initSharingEmptyShape(JSContext *cx,
                                js::Class *aclasp,
                                JSObject *proto,
                                JSObject *parent,
                                void *privateValue,
                                /* js::gc::FinalizeKind */ unsigned kind)
{
    init(cx, aclasp, proto, parent, privateValue, false);

    JS_ASSERT(!isDenseArray());

    js::EmptyShape *empty = proto->getEmptyShape(cx, aclasp, kind);
    if (!empty)
        return false;

    setMap(empty);
    return true;
}

inline void
JSObject::freeSlotsArray(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    cx->free_(slots);
}

inline void
JSObject::revertToFixedSlots(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    size_t fixed = numFixedSlots();
    JS_ASSERT(capacity >= fixed);
    memcpy(fixedSlots(), slots, fixed * sizeof(js::Value));
    freeSlotsArray(cx);
    slots = fixedSlots();
    capacity = fixed;
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
JSObject::nativeSearch(jsid id, bool adding)
{
    return js::Shape::search(compartment()->rt, &lastProp, id, adding);
}

inline const js::Shape *
JSObject::nativeLookup(jsid id)
{
    JS_ASSERT(isNative());
    return SHAPE_FETCH(nativeSearch(id));
}

inline bool
JSObject::nativeContains(jsid id)
{
    return nativeLookup(id) != NULL;
}

inline bool
JSObject::nativeContains(const js::Shape &shape)
{
    return nativeLookup(shape.propid) == &shape;
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

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.toObject().isCallable();
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
InitScopeForObject(JSContext* cx, JSObject* obj, js::Class *clasp, JSObject* proto,
                   gc::FinalizeKind kind)
{
    JS_ASSERT(clasp->isNative());
    JS_ASSERT(proto == obj->getProto());

    /* Share proto's emptyShape only if obj is similar to proto. */
    js::EmptyShape *empty = NULL;

    if (proto) {
        if (proto->canProvideEmptyShape(clasp)) {
            empty = proto->getEmptyShape(cx, clasp, kind);
            if (!empty)
                goto bad;
        }
    }

    if (!empty) {
        empty = js::EmptyShape::create(cx, clasp);
        if (!empty)
            goto bad;
        uint32 freeslot = JSSLOT_FREE(clasp);
        if (freeslot > obj->numSlots() && !obj->allocSlots(cx, freeslot))
            goto bad;
    }

    obj->setMap(empty);
    return true;

  bad:
    /* The GC nulls map initially. It should still be null on error. */
    JS_ASSERT(obj->isNewborn());
    return false;
}

static inline bool
CanBeFinalizedInBackground(gc::FinalizeKind kind, Class *clasp)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the finalize kind. For example,
     * FINALIZE_OBJECT0 calls the finalizer on the main thread,
     * FINALIZE_OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * kind % 2 prevents from recursivly incrementing the finalize kind because
     * we can call NewObject with a background finalize kind.
     */
    if (kind % 2 == 0 && (!clasp->finalize || clasp->flags & JSCLASS_CONCURRENT_FINALIZER))
        return true;
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
                       JSObject *parent, gc::FinalizeKind kind)
{
    JS_ASSERT(proto);
    JS_ASSERT(parent);
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC.
     */

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = (gc::FinalizeKind)(kind + 1);

    JSObject* obj = js_NewGCObject(cx, kind);

    if (obj) {
        /*
         * Default parent to the parent of the prototype, which was set from
         * the parent of the prototype's constructor.
         */
        bool useHoles = (clasp == &js_ArrayClass);
        obj->init(cx, clasp, proto, parent, NULL, useHoles);

        JS_ASSERT(proto->canProvideEmptyShape(clasp));
        js::EmptyShape *empty = proto->getEmptyShape(cx, clasp, kind);

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
    gc::FinalizeKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
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
NewBuiltinClassInstance(JSContext *cx, Class *clasp, gc::FinalizeKind kind)
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
    gc::FinalizeKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
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
          gc::FinalizeKind kind)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (withProto == WithProto::Class && !proto) {
        if (!FindProto(cx, clasp, parent, &proto))
          return NULL;
    }

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     *
     * The should be specialized by the template.
     */

    if (!isFunction && CanBeFinalizedInBackground(kind, clasp))
        kind = (gc::FinalizeKind)(kind + 1);

    JSObject* obj = isFunction ? js_NewGCFunction(cx) : js_NewGCObject(cx, kind);
    if (!obj)
        goto out;

    /* This needs to match up with the size of JSFunction::data_padding. */
    JS_ASSERT_IF(isFunction, kind == gc::FINALIZE_OBJECT2);

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(cx, clasp, proto,
              (!parent && proto) ? proto->getParent() : parent,
              NULL, clasp == &js_ArrayClass);

    if (clasp->isNative()) {
        if (!InitScopeForObject(cx, obj, clasp, proto, kind)) {
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
    return detail::NewObject<WithProto::Given, true>(cx, &js_FunctionClass, proto, &global,
                                                     gc::FINALIZE_OBJECT2);
}

static JS_ALWAYS_INLINE JSObject *
NewFunction(JSContext *cx, JSObject *parent)
{
    return detail::NewObject<WithProto::Class, true>(cx, &js_FunctionClass, NULL, parent,
                                                     gc::FINALIZE_OBJECT2);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
               gc::FinalizeKind kind)
{
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::FinalizeKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          gc::FinalizeKind kind)
{
    if (clasp == &js_FunctionClass)
        return detail::NewObject<withProto, true>(cx, clasp, proto, parent, kind);
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent, kind);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    gc::FinalizeKind kind = gc::GetGCObjectKind(JSCLASS_RESERVED_SLOTS(clasp));
    return NewObject<withProto>(cx, clasp, proto, parent, kind);
}

/*
 * As for gc::GetGCObjectKind, where numSlots is a guess at the final size of
 * the object, zero if the final size is unknown.
 */
static inline gc::FinalizeKind
GuessObjectGCKind(size_t numSlots, bool isArray)
{
    if (numSlots)
        return gc::GetGCObjectKind(numSlots);
    return isArray ? gc::FINALIZE_OBJECT8 : gc::FINALIZE_OBJECT4;
}

/*
 * Get the GC kind to use for scripted 'new' on the given class.
 * FIXME bug 547327: estimate the size from the allocation site.
 */
static inline gc::FinalizeKind
NewObjectGCKind(JSContext *cx, js::Class *clasp)
{
    if (clasp == &js_ArrayClass || clasp == &js_SlowArrayClass)
        return gc::FINALIZE_OBJECT8;
    if (clasp == &js_FunctionClass)
        return gc::FINALIZE_OBJECT2;
    return gc::FINALIZE_OBJECT4;
}

static JS_ALWAYS_INLINE JSObject*
NewObjectWithClassProto(JSContext *cx, Class *clasp, JSObject *proto,
                        /*gc::FinalizeKind*/ unsigned _kind)
{
    JS_ASSERT(clasp->isNative());
    gc::FinalizeKind kind = gc::FinalizeKind(_kind);

    if (CanBeFinalizedInBackground(kind, clasp))
        kind = (gc::FinalizeKind)(kind + 1);

    JSObject* obj = js_NewGCObject(cx, kind);
    if (!obj)
        return NULL;

    if (!obj->initSharingEmptyShape(cx, clasp, proto, proto->getParent(), NULL, kind))
        return NULL;
    return obj;
}

/* Make an object with pregenerated shape from a NEWOBJECT bytecode. */
static inline JSObject *
CopyInitializerObject(JSContext *cx, JSObject *baseobj)
{
    JS_ASSERT(baseobj->getClass() == &js_ObjectClass);
    JS_ASSERT(!baseobj->inDictionaryMode());

    gc::FinalizeKind kind = gc::FinalizeKind(baseobj->finalizeKind());
    JSObject *obj = NewBuiltinClassInstance(cx, &js_ObjectClass, kind);

    if (!obj || !obj->ensureSlots(cx, baseobj->numSlots()))
        return NULL;

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
    JS_ASSERT(!global->nativeLookup(id));

    if (!global->addDataProperty(cx, id, key + JSProto_LIMIT * 2, 0))
        return false;

    global->setSlot(key, ObjectValue(*ctor));
    global->setSlot(key + JSProto_LIMIT, ObjectValue(*proto));
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

} /* namespace js */

#endif /* jsobjinlines_h___ */
