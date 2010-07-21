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

#include "jsbool.h"
#include "jsdate.h"
#include "jsiter.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstaticcheck.h"
#include "jsxml.h"

#include "jsdtracef.h"

#include "jscntxt.h"
#include "jsscopeinlines.h"

inline void
JSObject::dropProperty(JSContext *cx, JSProperty *prop)
{
    JS_ASSERT(prop);
    if (isNative())
        JS_UNLOCK_OBJ(cx, this);
}

inline jsval
JSObject::getSlotMT(JSContext *cx, uintN slot)
{
#ifdef JS_THREADSAFE
    /*
     * If thread-safe, define a getSlotMT() that bypasses, for a native
     * object, the lock-free "fast path" test of
     * (obj->scope()->ownercx == cx), to avoid needlessly switching from
     * lock-free to lock-full scope when doing GC on a different context
     * from the last one to own the scope.  The caller in this case is
     * probably a JSClass.mark function, e.g., fun_mark, or maybe a
     * finalizer.
     */
    OBJ_CHECK_SLOT(this, slot);
    return (scope()->title.ownercx == cx)
           ? this->lockedGetSlot(slot)
           : js_GetSlotThreadSafe(cx, this, slot);
#else
    return this->lockedGetSlot(slot);
#endif
}

inline void
JSObject::setSlotMT(JSContext *cx, uintN slot, jsval value)
{
#ifdef JS_THREADSAFE
    /* Thread-safe way to set a slot. */
    OBJ_CHECK_SLOT(this, slot);
    if (scope()->title.ownercx == cx)
        this->lockedSetSlot(slot, value);
    else
        js_SetSlotThreadSafe(cx, this, slot, value);
#else
    this->lockedSetSlot(slot, value);
#endif
}

inline jsval
JSObject::getReservedSlot(uintN index) const
{
    uint32 slot = JSSLOT_START(getClass()) + index;
    return (slot < numSlots()) ? getSlot(slot) : JSVAL_VOID;
}

inline bool
JSObject::isPrimitive() const
{
    return isNumber() || isString() || isBoolean();
}

inline jsval
JSObject::getPrimitiveThis() const
{
    JS_ASSERT(isPrimitive());
    return fslots[JSSLOT_PRIMITIVE_THIS];
}

inline void 
JSObject::setPrimitiveThis(jsval pthis)
{
    JS_ASSERT(isPrimitive());
    fslots[JSSLOT_PRIMITIVE_THIS] = pthis;
}

inline void
JSObject::staticAssertArrayLengthIsInPrivateSlot()
{
    JS_STATIC_ASSERT(JSSLOT_ARRAY_LENGTH == JSSLOT_PRIVATE);
}

inline bool
JSObject::isDenseArrayMinLenCapOk(bool strictAboutLength) const
{
    JS_ASSERT(isDenseArray());
    uint32 length = uncheckedGetArrayLength();
    uint32 capacity = uncheckedGetDenseArrayCapacity();
    uint32 minLenCap = uint32(fslots[JSSLOT_DENSE_ARRAY_MINLENCAP]);

    // This function can be called while the LENGTH and MINLENCAP slots are
    // still set to JSVAL_VOID and there are no dslots (ie. the capacity is
    // zero).  If 'strictAboutLength' is false we allow this.
    return minLenCap == JS_MIN(length, capacity) ||
           (!strictAboutLength && minLenCap == uint32(JSVAL_VOID) &&
            length == uint32(JSVAL_VOID) && capacity == 0);
}

inline uint32
JSObject::uncheckedGetArrayLength() const
{
    return uint32(fslots[JSSLOT_ARRAY_LENGTH]);
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    JS_ASSERT_IF(isDenseArray(), isDenseArrayMinLenCapOk());
    return uncheckedGetArrayLength();
}

inline void 
JSObject::setDenseArrayLength(uint32 length)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_ARRAY_LENGTH] = length;
    uint32 capacity = uncheckedGetDenseArrayCapacity();
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP] = JS_MIN(length, capacity);
}

inline void 
JSObject::setSlowArrayLength(uint32 length)
{
    JS_ASSERT(isSlowArray());
    fslots[JSSLOT_ARRAY_LENGTH] = length;
}

inline uint32 
JSObject::getDenseArrayCount() const
{
    JS_ASSERT(isDenseArray());
    return uint32(fslots[JSSLOT_DENSE_ARRAY_COUNT]);
}

inline void 
JSObject::setDenseArrayCount(uint32 count)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT] = count;
}

inline void 
JSObject::incDenseArrayCountBy(uint32 posDelta)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT] += posDelta;
}

inline void 
JSObject::decDenseArrayCountBy(uint32 negDelta)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT] -= negDelta;
}

inline uint32
JSObject::uncheckedGetDenseArrayCapacity() const
{
    return dslots ? uint32(dslots[-1]) : 0;
}

inline uint32
JSObject::getDenseArrayCapacity() const
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(isDenseArrayMinLenCapOk(/* strictAboutLength = */false));
    return uncheckedGetDenseArrayCapacity();
}

inline void
JSObject::setDenseArrayCapacity(uint32 capacity)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(dslots);
    dslots[-1] = capacity;
    uint32 length = uncheckedGetArrayLength();
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP] = JS_MIN(length, capacity);
}

inline jsval
JSObject::getDenseArrayElement(uint32 i) const
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    return dslots[i];
}

inline jsval *
JSObject::addressOfDenseArrayElement(uint32 i)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    return &dslots[i];
}

inline void
JSObject::setDenseArrayElement(uint32 i, jsval v)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    dslots[i] = v;
}

inline jsval *
JSObject::getDenseArrayElements() const
{
    JS_ASSERT(isDenseArray());
    return dslots;
}

inline void
JSObject::freeDenseArrayElements(JSContext *cx)
{
    JS_ASSERT(isDenseArray());
    if (dslots) {
        cx->free(dslots - 1);
        dslots = NULL;
    }
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP] = 0;
    JS_ASSERT(isDenseArrayMinLenCapOk());
}

inline void 
JSObject::voidDenseOnlyArraySlots()
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT] = JSVAL_VOID;
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP] = JSVAL_VOID;
}

inline void
JSObject::setArgsLength(uint32 argc)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    fslots[JSSLOT_ARGS_LENGTH] = INT_TO_JSVAL(argc << 1);
    JS_ASSERT(!isArgsLengthOverridden());
}

inline uint32
JSObject::getArgsLength() const
{
    JS_ASSERT(isArguments());
    uint32 argc = uint32(JSVAL_TO_INT(fslots[JSSLOT_ARGS_LENGTH])) >> 1;
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    return argc;
}

inline void
JSObject::setArgsLengthOverridden()
{
    JS_ASSERT(isArguments());
    jsval v = fslots[JSSLOT_ARGS_LENGTH];
    v = INT_TO_JSVAL(JSVAL_TO_INT(v) | 1);
    JS_ASSERT(JSVAL_IS_INT(v));
    fslots[JSSLOT_ARGS_LENGTH] = v;
}

inline bool
JSObject::isArgsLengthOverridden() const
{
    JS_ASSERT(isArguments());
    jsval v = fslots[JSSLOT_ARGS_LENGTH];
    return (JSVAL_TO_INT(v) & 1) != 0;
}

inline jsval 
JSObject::getArgsCallee() const
{
    JS_ASSERT(isArguments());
    return fslots[JSSLOT_ARGS_CALLEE];
}

inline void 
JSObject::setArgsCallee(jsval callee)
{
    JS_ASSERT(isArguments());
    fslots[JSSLOT_ARGS_CALLEE] = callee;
}

inline jsval
JSObject::getArgsElement(uint32 i) const
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < numSlots() - JS_INITIAL_NSLOTS);
    return dslots[i];
}

inline void
JSObject::setArgsElement(uint32 i, jsval v)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < numSlots() - JS_INITIAL_NSLOTS);
    dslots[i] = v;
}

inline jsval
JSObject::getDateLocalTime() const
{
    JS_ASSERT(isDate());
    return fslots[JSSLOT_DATE_LOCAL_TIME];
}

inline jsval *
JSObject::addressOfDateLocalTime()
{
    JS_ASSERT(isDate());
    return &fslots[JSSLOT_DATE_LOCAL_TIME];
}

inline void 
JSObject::setDateLocalTime(jsval time)
{
    JS_ASSERT(isDate());
    fslots[JSSLOT_DATE_LOCAL_TIME] = time;
}

inline jsval
JSObject::getDateUTCTime() const
{
    JS_ASSERT(isDate());
    return fslots[JSSLOT_DATE_UTC_TIME];
}

inline jsval *
JSObject::addressOfDateUTCTime()
{
    JS_ASSERT(isDate());
    return &fslots[JSSLOT_DATE_UTC_TIME];
}

inline void 
JSObject::setDateUTCTime(jsval time)
{
    JS_ASSERT(isDate());
    fslots[JSSLOT_DATE_UTC_TIME] = time;
}

inline jsval
JSObject::getRegExpLastIndex() const
{
    JS_ASSERT(isRegExp());
    return fslots[JSSLOT_REGEXP_LAST_INDEX];
}

inline jsval *
JSObject::addressOfRegExpLastIndex()
{
    JS_ASSERT(isRegExp());
    return &fslots[JSSLOT_REGEXP_LAST_INDEX];
}

inline void 
JSObject::zeroRegExpLastIndex()
{
    JS_ASSERT(isRegExp());
    fslots[JSSLOT_REGEXP_LAST_INDEX] = JSVAL_ZERO;
}

inline NativeIterator *
JSObject::getNativeIterator() const
{
    return (NativeIterator *) getPrivate();
}

inline void
JSObject::setNativeIterator(NativeIterator *ni)
{
    setPrivate(ni);
}

inline jsval
JSObject::getNamePrefix() const
{
    JS_ASSERT(isNamespace() || isQName());
    return fslots[JSSLOT_NAME_PREFIX];
}

inline void
JSObject::setNamePrefix(jsval prefix)
{
    JS_ASSERT(isNamespace() || isQName());
    fslots[JSSLOT_NAME_PREFIX] = prefix;
}

inline jsval
JSObject::getNameURI() const
{
    JS_ASSERT(isNamespace() || isQName());
    return fslots[JSSLOT_NAME_URI];
}

inline void
JSObject::setNameURI(jsval uri)
{
    JS_ASSERT(isNamespace() || isQName());
    fslots[JSSLOT_NAME_URI] = uri;
}

inline jsval
JSObject::getNamespaceDeclared() const
{
    JS_ASSERT(isNamespace());
    return fslots[JSSLOT_NAMESPACE_DECLARED];
}

inline void
JSObject::setNamespaceDeclared(jsval decl)
{
    JS_ASSERT(isNamespace());
    fslots[JSSLOT_NAMESPACE_DECLARED] = decl;
}

inline jsval
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    return fslots[JSSLOT_QNAME_LOCAL_NAME];
}

inline void
JSObject::setQNameLocalName(jsval name)
{
    JS_ASSERT(isQName());
    fslots[JSSLOT_QNAME_LOCAL_NAME] = name;
}

inline JSObject *
JSObject::getWithThis() const
{
    return JSVAL_TO_OBJECT(fslots[JSSLOT_WITH_THIS]);
}

inline void
JSObject::setWithThis(JSObject *thisp)
{
    fslots[JSSLOT_WITH_THIS] = OBJECT_TO_JSVAL(thisp);
}

inline void
JSObject::initSharingEmptyScope(JSClass *clasp, JSObject *proto, JSObject *parent,
                                jsval privateSlotValue)
{
    init(clasp, proto, parent, privateSlotValue);

    JSEmptyScope *emptyScope = proto->scope()->emptyScope;
    JS_ASSERT(emptyScope->clasp == clasp);
    map = emptyScope->hold();
}

inline void
JSObject::freeSlotsArray(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    JS_ASSERT(size_t(dslots[-1]) > JS_INITIAL_NSLOTS);
    cx->free(dslots - 1);
}

inline bool
JSObject::unbrand(JSContext *cx)
{
    if (this->isNative()) {
        JS_LOCK_OBJ(cx, this);
        JSScope *scope = this->scope();
        if (scope->isSharedEmpty()) {
            scope = js_GetMutableScope(cx, this);
            if (!scope) {
                JS_UNLOCK_OBJ(cx, this);
                return false;
            }
        }
        scope->unbrand(cx);
        JS_UNLOCK_SCOPE(cx, scope);
    }
    return true;
}

namespace js {

class AutoDescriptorArray : private AutoGCRooter
{
  public:
    AutoDescriptorArray(JSContext *cx)
      : AutoGCRooter(cx, DESCRIPTORS), descriptors(cx)
    { }

    PropertyDescriptor *append() {
        if (!descriptors.append(PropertyDescriptor()))
            return NULL;
        return &descriptors.back();
    }

    PropertyDescriptor& operator[](size_t i) {
        JS_ASSERT(i < descriptors.length());
        return descriptors[i];
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  private:
    PropertyDescriptorArray descriptors;
};

class AutoDescriptor : private AutoGCRooter, public JSPropertyDescriptor
{
  public:
    AutoDescriptor(JSContext *cx) : AutoGCRooter(cx, DESCRIPTOR) {
        obj = NULL;
        attrs = 0;
        getter = setter = (JSPropertyOp) NULL;
        value = JSVAL_VOID;
    }

    AutoDescriptor(JSContext *cx, JSPropertyDescriptor *desc) : AutoGCRooter(cx, DESCRIPTOR) {
        obj = desc->obj;
        attrs = desc->attrs;
        getter = desc->getter;
        setter = desc->setter;
        value = desc->value;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
};

static inline bool
InitScopeForObject(JSContext* cx, JSObject* obj, JSClass *clasp, JSObject* proto, JSObjectOps* ops)
{
    JS_ASSERT(ops->isNative());
    JS_ASSERT(proto == obj->getProto());

    /* Share proto's emptyScope only if obj is similar to proto. */
    JSScope *scope = NULL;

    if (proto && proto->isNative()) {
        JS_LOCK_OBJ(cx, proto);
        scope = proto->scope();
        if (scope->canProvideEmptyScope(ops, clasp)) {
            JSScope *emptyScope = scope->getEmptyScope(cx, clasp);
            JS_UNLOCK_SCOPE(cx, scope);
            if (!emptyScope)
                goto bad;
            scope = emptyScope;
        } else {
            JS_UNLOCK_SCOPE(cx, scope);
            scope = NULL;
        }
    }

    if (!scope) {
        scope = JSScope::create(cx, ops, clasp, obj, js_GenerateShape(cx, false));
        if (!scope)
            goto bad;
        uint32 freeslot = JSSLOT_FREE(clasp);
        JS_ASSERT(freeslot >= scope->freeslot);
        if (freeslot > JS_INITIAL_NSLOTS && !obj->allocSlots(cx, freeslot))
            goto bad;
        scope->freeslot = freeslot;
#ifdef DEBUG
        if (freeslot < obj->numSlots())
            obj->setSlot(freeslot, JSVAL_VOID);
#endif
    }

    obj->map = scope;
    return true;

  bad:
    /* The GC nulls map initially. It should still be null on error. */
    JS_ASSERT(!obj->map);
    return false;
}

/*
 * Helper optimized for creating a native instance of the given class (not the
 * class's prototype object). Use this in preference to NewObjectWithGivenProto
 * and NewObject, but use NewBuiltinClassInstance if you need the default class
 * prototype as proto, and its parent global as parent.
 */
static inline JSObject *
NewNativeClassInstance(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JS_ASSERT(proto);
    JS_ASSERT(proto->isNative());
    JS_ASSERT(parent);

    DTrace::ObjectCreationScope objectCreationScope(cx, cx->fp, clasp);

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     */
    JSObject* obj = js_NewGCObject(cx);
    if (obj) {
        /*
         * Default parent to the parent of the prototype, which was set from
         * the parent of the prototype's constructor.
         */
        obj->init(clasp, proto, parent, JSObject::defaultPrivate(clasp));

        JS_LOCK_OBJ(cx, proto);
        JSScope *scope = proto->scope();
        JS_ASSERT(scope->canProvideEmptyScope(&js_ObjectOps, clasp));
        scope = scope->getEmptyScope(cx, clasp);
        JS_UNLOCK_OBJ(cx, proto);

        if (!scope) {
            obj = NULL;
        } else {
            obj->map = scope;

            /*
             * Do not call debug hooks on trace, because we might be in a non-_FAIL
             * builtin. See bug 481444.
             */
            if (cx->debugHooks->objectHook && !JS_ON_TRACE(cx)) {
                AutoValueRooter tvr(cx, obj);
                AutoKeepAtoms keep(cx->runtime);
                cx->debugHooks->objectHook(cx, obj, JS_TRUE,
                                           cx->debugHooks->objectHookData);
                cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = obj;
            }
        }
    }

    objectCreationScope.handleCreation(obj);
    return obj;
}

bool
FindClassPrototype(JSContext *cx, JSObject *scope, JSProtoKey protoKey, JSObject **protop,
                   JSClass *clasp);

/*
 * Helper used to create Boolean, Date, RegExp, etc. instances of built-in
 * classes with class prototypes of the same JSClass. See, e.g., jsdate.cpp,
 * jsregexp.cpp, and js_PrimitiveToObject in jsobj.cpp. Use this to get the
 * right default proto and parent for clasp in cx.
 */
static inline JSObject *
NewBuiltinClassInstance(JSContext *cx, JSClass *clasp)
{
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JSProtoKey protoKey = JSCLASS_CACHED_PROTO_KEY(clasp);
    JS_ASSERT(protoKey != JSProto_Null);

    /* NB: inline-expanded and specialized version of js_GetClassPrototype. */
    JSObject *global;
    if (!cx->fp) {
        global = cx->globalObject;
        OBJ_TO_INNER_OBJECT(cx, global);
        if (!global)
            return NULL;
    } else {
        global = cx->fp->scopeChain->getGlobal();
    }
    JS_ASSERT(global->getClass()->flags & JSCLASS_IS_GLOBAL);

    jsval v = global->getReservedSlot(JSProto_LIMIT + protoKey);
    JSObject *proto;
    if (!JSVAL_IS_PRIMITIVE(v)) {
        proto = JSVAL_TO_OBJECT(v);
        JS_ASSERT(proto->getParent() == global);
    } else {
        if (!FindClassPrototype(cx, global, protoKey, &proto, clasp))
            return NULL;
    }

    return NewNativeClassInstance(cx, clasp, proto, global);
}

/*
 * Like NewObject but with exactly the given proto. A null parent defaults to
 * proto->getParent() if proto is non-null (else to null). NB: only this helper
 * and NewObject can be used to construct full-sized JSFunction instances.
 */
static inline JSObject *
NewObjectWithGivenProto(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    DTrace::ObjectCreationScope objectCreationScope(cx, cx->fp, clasp);

    /* Always call the class's getObjectOps hook if it has one. */
    JSObjectOps *ops = clasp->getObjectOps
                       ? clasp->getObjectOps(cx, clasp)
                       : &js_ObjectOps;

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     */
    JSObject* obj;
    if (clasp == &js_FunctionClass) {
        obj = (JSObject*) js_NewGCFunction(cx);
#ifdef DEBUG
        if (obj) {
            memset((uint8 *) obj + sizeof(JSObject), JS_FREE_PATTERN,
                   sizeof(JSFunction) - sizeof(JSObject));
        }
#endif
    } else {
        obj = js_NewGCObject(cx);
    }
    if (!obj)
        goto out;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(clasp,
              proto,
              (!parent && proto) ? proto->getParent() : parent,
              JSObject::defaultPrivate(clasp));

    if (ops->isNative()) {
        if (!InitScopeForObject(cx, obj, clasp, proto, ops)) {
            obj = NULL;
            goto out;
        }
    } else {
        JS_ASSERT(ops->objectMap->ops == ops);
        obj->map = const_cast<JSObjectMap *>(ops->objectMap);
    }

    /*
     * Do not call debug hooks on trace, because we might be in a non-_FAIL
     * builtin. See bug 481444.
     */
    if (cx->debugHooks->objectHook && !JS_ON_TRACE(cx)) {
        AutoValueRooter tvr(cx, obj);
        AutoKeepAtoms keep(cx->runtime);
        cx->debugHooks->objectHook(cx, obj, JS_TRUE,
                                   cx->debugHooks->objectHookData);
        cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = obj;
    }

out:
    objectCreationScope.handleCreation(obj);
    return obj;
}

static inline JSProtoKey
GetClassProtoKey(JSClass *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

/*
 * Create an instance of any class, native or not, JSFunction-sized or not.
 *
 * If proto is null, use the memoized original value of the class constructor
 * .prototype property object for a built-in class, else the current value of
 * .prototype if available, else Object.prototype.
 *
 * Default parent is null to proto's parent (null if proto is null too).
 */
static inline JSObject *
NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (!proto) {
        JSProtoKey protoKey = GetClassProtoKey(clasp);
        if (!js_GetClassPrototype(cx, parent, protoKey, &proto, clasp))
            return NULL;
        if (!proto && !js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
            return NULL;
    }

    return NewObjectWithGivenProto(cx, clasp, proto, parent);
}

} /* namespace js */

#endif /* jsobjinlines_h___ */
