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

#ifdef INCLUDE_MOZILLA_DTRACE
#include "jsdtracef.h"
#endif

#include "jsscopeinlines.h"

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

inline void JSObject::staticAssertArrayLengthIsInPrivateSlot()
{
    JS_STATIC_ASSERT(JSSLOT_ARRAY_LENGTH == JSSLOT_PRIVATE);
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return uint32(fslots[JSSLOT_ARRAY_LENGTH]);
}

inline void 
JSObject::setArrayLength(uint32 length)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_LENGTH] = length;
}

inline uint32 
JSObject::getArrayCount() const
{
    JS_ASSERT(isArray());
    return uint32(fslots[JSSLOT_ARRAY_COUNT]);
}

inline void 
JSObject::setArrayCount(uint32 count)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT] = count;
}

inline void 
JSObject::voidDenseArrayCount()
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_ARRAY_COUNT] = JSVAL_VOID;
}

inline void 
JSObject::incArrayCountBy(uint32 posDelta)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT] += posDelta;
}

inline void 
JSObject::decArrayCountBy(uint32 negDelta)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_COUNT] -= negDelta;
}

inline uint32
JSObject::getDenseArrayCapacity() const
{
    JS_ASSERT(isDenseArray());
    return dslots ? uint32(dslots[-1]) : 0;
}

inline void
JSObject::setDenseArrayCapacity(uint32 capacity)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(dslots);
    dslots[-1] = capacity;
}

inline jsval
JSObject::getDenseArrayElement(uint32 i) const
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    return dslots[i];
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
    if (dslots) {
        cx->free(dslots - 1);
        dslots = NULL;
    }
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
JSObject::isArgsLengthOverridden()
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

inline void
JSObject::initSharingEmptyScope(JSClass *clasp, JSObject *proto, JSObject *parent,
                                jsval privateSlotValue)
{
    init(clasp, proto, parent, privateSlotValue);

    JSEmptyScope *emptyScope = proto->scope()->emptyScope;
    JS_ASSERT(emptyScope->clasp == clasp);
    emptyScope->hold();
    map = emptyScope;
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
        scope->setGeneric();
        JS_UNLOCK_SCOPE(cx, scope);
    }
    return true;
}

namespace js {

typedef Vector<PropertyDescriptor, 1> PropertyDescriptorArray;

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

        /* Let JSScope::create set freeslot so as to reserve slots. */
        JS_ASSERT(scope->freeslot >= JSSLOT_PRIVATE);
        if (scope->freeslot > JS_INITIAL_NSLOTS &&
            !js_AllocSlots(cx, obj, scope->freeslot)) {
            scope->destroy(cx);
            goto bad;
        }
    }

    obj->map = scope;
    return true;

  bad:
    /* The GC nulls map initially. It should still be null on error. */
    JS_ASSERT(!obj->map);
    return false;
}

static inline JSObject *
NewObjectWithGivenProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                        JSObject *parent, size_t objectSize = 0)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_START_ENABLED())
        jsdtrace_object_create_start(cx->fp, clasp);
#endif

    /* Assert that the class is a proper class. */
    JS_ASSERT_IF(clasp->flags & JSCLASS_IS_EXTENDED,
                 ((JSExtendedClass *)clasp)->equality);

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
    if (clasp == &js_FunctionClass && !objectSize) {
        obj = (JSObject*) js_NewGCFunction(cx);
#ifdef DEBUG
        if (obj) {
            memset((uint8 *) obj + sizeof(JSObject), JS_FREE_PATTERN,
                   sizeof(JSFunction) - sizeof(JSObject));
        }
#endif
    } else {
        JS_ASSERT(!objectSize || objectSize == sizeof(JSObject));
        obj = (clasp == &js_IteratorClass)
            ? js_NewGCIter(cx)
            : js_NewGCObject(cx);
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
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_CREATE_ENABLED())
        jsdtrace_object_create(cx, clasp, obj);
    if (JAVASCRIPT_OBJECT_CREATE_DONE_ENABLED())
        jsdtrace_object_create_done(cx->fp, clasp);
#endif
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

static inline JSObject *
NewObject(JSContext *cx, JSClass *clasp, JSObject *proto,
          JSObject *parent, size_t objectSize = 0)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (!proto) {
        JSProtoKey protoKey = GetClassProtoKey(clasp);
        if (!js_GetClassPrototype(cx, parent, protoKey, &proto, clasp))
            return NULL;
        if (!proto &&
            !js_GetClassPrototype(cx, parent, JSProto_Object, &proto)) {
            return NULL;
        }
    }

    return NewObjectWithGivenProto(cx, clasp, proto, parent, objectSize);
}

}

#endif /* jsobjinlines_h___ */
