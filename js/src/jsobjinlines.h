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
#include "jsxml.h"

#include "jsdtracef.h"

#include "jsscopeinlines.h"

inline void
JSObject::dropProperty(JSContext *cx, JSProperty *prop)
{
    JS_ASSERT(prop);
    if (isNative())
        JS_UNLOCK_OBJ(cx, this);
}

inline js::Value
JSObject::getSlotMT(JSContext *cx, uintN slot)
{
#ifdef JS_THREADSAFE
    /*
     * If thread-safe, define a getSlotMT() that bypasses, for a native
     * object, the lock-free "fast path" test of
     * (obj->scope()->ownercx == cx), to avoid needlessly switching from
     * lock-free to lock-full scope when doing GC on a different context
     * from the last one to own the scope.  The caller in this case is
     * probably a Class.mark function, e.g., fun_mark, or maybe a
     * finalizer.
     */
    OBJ_CHECK_SLOT(this, slot);
    return (scope()->title.ownercx == cx)
         ? this->lockedGetSlot(slot)
         : js::Valueify(js_GetSlotThreadSafe(cx, this, slot));
#else
    return this->lockedGetSlot(slot);
#endif
}

inline void
JSObject::setSlotMT(JSContext *cx, uintN slot, const js::Value &value)
{
#ifdef JS_THREADSAFE
    /* Thread-safe way to set a slot. */
    OBJ_CHECK_SLOT(this, slot);
    if (scope()->title.ownercx == cx)
        this->lockedSetSlot(slot, value);
    else
        js_SetSlotThreadSafe(cx, this, slot, js::Jsvalify(value));
#else
    this->lockedSetSlot(slot, value);
#endif
}

inline js::Value
JSObject::getReservedSlot(uintN index) const
{
    uint32 slot = JSSLOT_START(getClass()) + index;
    return (slot < numSlots()) ? getSlot(slot) : js::Value(js::UndefinedTag());
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
    return fslots[JSSLOT_PRIMITIVE_THIS];
}

inline void 
JSObject::setPrimitiveThis(const js::Value &pthis)
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

    // This function can be called while the LENGTH and MINLENCAP slots are
    // still set to JSVAL_VOID and there are no dslots (ie. the capacity is
    // zero).  If 'strictAboutLength' is false we allow this.
    if (!strictAboutLength &&
        fslots[JSSLOT_ARRAY_LENGTH].isUndefined() &&
        uncheckedGetDenseArrayCapacity() == 0) {
        return true;
    }

    uint32 length = uncheckedGetArrayLength();
    uint32 capacity = uncheckedGetDenseArrayCapacity();
    uint32 minLenCap = fslots[JSSLOT_DENSE_ARRAY_MINLENCAP].asPrivateUint32();
    return minLenCap == JS_MIN(length, capacity);
}

inline uint32
JSObject::uncheckedGetArrayLength() const
{
    return fslots[JSSLOT_ARRAY_LENGTH].asPrivateUint32();
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
    fslots[JSSLOT_ARRAY_LENGTH].setPrivateUint32(length);
    uint32 capacity = uncheckedGetDenseArrayCapacity();
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP].setPrivateUint32(JS_MIN(length, capacity));
}

inline void 
JSObject::setSlowArrayLength(uint32 length)
{
    JS_ASSERT(isSlowArray());
    fslots[JSSLOT_ARRAY_LENGTH].setPrivateUint32(length);
}

inline uint32 
JSObject::getDenseArrayCount() const
{
    JS_ASSERT(isDenseArray());
    return fslots[JSSLOT_DENSE_ARRAY_COUNT].asPrivateUint32();
}

inline void 
JSObject::setDenseArrayCount(uint32 count)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT].setPrivateUint32(count);
}

inline void 
JSObject::incDenseArrayCountBy(uint32 posDelta)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT].asPrivateUint32Ref() += posDelta;
}

inline void 
JSObject::decDenseArrayCountBy(uint32 negDelta)
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT].asPrivateUint32Ref() -= negDelta;
}

inline uint32
JSObject::uncheckedGetDenseArrayCapacity() const
{
    return dslots ? dslots[-1].asPrivateUint32Ref() : 0;
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
    dslots[-1].setPrivateUint32(capacity);
    uint32 length = uncheckedGetArrayLength();
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP].setPrivateUint32(JS_MIN(length, capacity));
}

inline const js::Value &
JSObject::getDenseArrayElement(uint32 i) const
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    return dslots[i];
}

inline js::Value *
JSObject::addressOfDenseArrayElement(uint32 i)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    return &dslots[i];
}

inline void
JSObject::setDenseArrayElement(uint32 i, const js::Value &v)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(i < getDenseArrayCapacity());
    dslots[i] = v;
}

inline js::Value *
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
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP].setPrivateUint32(0);
    JS_ASSERT(isDenseArrayMinLenCapOk(/* strictAboutLength = */false));
}

inline void 
JSObject::voidDenseOnlyArraySlots()
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_COUNT].setUndefined();
    fslots[JSSLOT_DENSE_ARRAY_MINLENCAP].setUndefined();
}

inline void
JSObject::setArgsLength(uint32 argc)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    fslots[JSSLOT_ARGS_LENGTH].setInt32(argc << 1);
    JS_ASSERT(!isArgsLengthOverridden());
}

inline uint32
JSObject::getArgsLength() const
{
    JS_ASSERT(isArguments());
    uint32 argc = uint32(fslots[JSSLOT_ARGS_LENGTH].asInt32()) >> 1;
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    return argc;
}

inline void
JSObject::setArgsLengthOverridden()
{
    JS_ASSERT(isArguments());
    fslots[JSSLOT_ARGS_LENGTH].asInt32Ref() |= 1;
}

inline bool
JSObject::isArgsLengthOverridden() const
{
    JS_ASSERT(isArguments());
    const js::Value &v = fslots[JSSLOT_ARGS_LENGTH];
    return (v.asInt32() & 1) != 0;
}

inline const js::Value & 
JSObject::getArgsCallee() const
{
    JS_ASSERT(isArguments());
    return fslots[JSSLOT_ARGS_CALLEE];
}

inline void 
JSObject::setArgsCallee(const js::Value &callee)
{
    JS_ASSERT(isArguments());
    fslots[JSSLOT_ARGS_CALLEE] = callee;
}

inline const js::Value &
JSObject::getArgsElement(uint32 i) const
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < numSlots() - JS_INITIAL_NSLOTS);
    return dslots[i];
}

inline js::Value *
JSObject::addressOfArgsElement(uint32 i) const
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < numSlots() - JS_INITIAL_NSLOTS);
    return &dslots[i];
}

inline void
JSObject::setArgsElement(uint32 i, const js::Value &v)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < numSlots() - JS_INITIAL_NSLOTS);
    dslots[i] = v;
}

inline const js::Value &
JSObject::getDateLocalTime() const
{
    JS_ASSERT(isDate());
    return fslots[JSSLOT_DATE_LOCAL_TIME];
}

inline void 
JSObject::setDateLocalTime(const js::Value &time)
{
    JS_ASSERT(isDate());
    fslots[JSSLOT_DATE_LOCAL_TIME] = time;
}

inline const js::Value &
JSObject::getDateUTCTime() const
{
    JS_ASSERT(isDate());
    return fslots[JSSLOT_DATE_UTC_TIME];
}

inline void 
JSObject::setDateUTCTime(const js::Value &time)
{
    JS_ASSERT(isDate());
    fslots[JSSLOT_DATE_UTC_TIME] = time;
}

inline const js::Value &
JSObject::getRegExpLastIndex() const
{
    JS_ASSERT(isRegExp());
    return fslots[JSSLOT_REGEXP_LAST_INDEX];
}

inline void
JSObject::setRegExpLastIndex(const js::Value &v)
{
    JS_ASSERT(isRegExp());
    fslots[JSSLOT_REGEXP_LAST_INDEX] = v;
}

inline void 
JSObject::zeroRegExpLastIndex()
{
    JS_ASSERT(isRegExp());
    fslots[JSSLOT_REGEXP_LAST_INDEX].setInt32(0);
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
    return js::Jsvalify(fslots[JSSLOT_NAME_PREFIX]);
}

inline void
JSObject::setNamePrefix(jsval prefix)
{
    JS_ASSERT(isNamespace() || isQName());
    fslots[JSSLOT_NAME_PREFIX] = js::Valueify(prefix);
}

inline jsval
JSObject::getNameURI() const
{
    JS_ASSERT(isNamespace() || isQName());
    return js::Jsvalify(fslots[JSSLOT_NAME_URI]);
}

inline void
JSObject::setNameURI(jsval uri)
{
    JS_ASSERT(isNamespace() || isQName());
    fslots[JSSLOT_NAME_URI] = js::Valueify(uri);
}

inline jsval
JSObject::getNamespaceDeclared() const
{
    JS_ASSERT(isNamespace());
    return js::Jsvalify(fslots[JSSLOT_NAMESPACE_DECLARED]);
}

inline void
JSObject::setNamespaceDeclared(jsval decl)
{
    JS_ASSERT(isNamespace());
    fslots[JSSLOT_NAMESPACE_DECLARED] = js::Valueify(decl);
}

inline jsval
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    return js::Jsvalify(fslots[JSSLOT_QNAME_LOCAL_NAME]);
}

inline void
JSObject::setQNameLocalName(jsval name)
{
    JS_ASSERT(isQName());
    fslots[JSSLOT_QNAME_LOCAL_NAME] = js::Valueify(name);
}

inline void
JSObject::initSharingEmptyScope(js::Class *clasp,
                                const js::Value &proto,
                                const js::Value &parent,
                                const js::Value &privateSlotValue)
{
    init(clasp, proto, parent, privateSlotValue);

    JSEmptyScope *emptyScope = proto.asObject().scope()->emptyScope;
    JS_ASSERT(emptyScope->clasp == clasp);
    map = emptyScope->hold();
}

inline void
JSObject::freeSlotsArray(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    JS_ASSERT(dslots[-1].asPrivateUint32() > JS_INITIAL_NSLOTS);
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

inline bool
JSObject::isCallable()
{
    if (isNative())
        return isFunction() || getClass()->call;

    return !!map->ops->call;
}

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.asObject().isCallable();
}

inline bool
JSObject::thisObject(JSContext *cx, const js::Value &v, js::Value *vp)
{
    if (JSObjectOp thisOp = v.asObject().map->ops->thisObject) {
        JSObject *pobj = thisOp(cx, &v.asObject());
        if (!pobj)
            return false;
        vp->setObject(*pobj);
    } else {
        *vp = v;
    }
    return true;
}

namespace js {

typedef Vector<PropDesc, 1> PropDescArray;

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
        getter = setter = (PropertyOp) NULL;
        value.setUndefined();
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
};

static inline bool
InitScopeForObject(JSContext* cx, JSObject* obj, js::Class *clasp, JSObject* proto, JSObjectOps* ops)
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
            !obj->allocSlots(cx, scope->freeslot)) {
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
NewObjectWithGivenProto(JSContext *cx, js::Class *clasp, JSObject *proto,
                        JSObject *parent, size_t objectSize = 0)
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
        obj = js_NewGCObject(cx);
    }
    if (!obj)
        goto out;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(clasp,
              js::ObjectOrNullTag(proto),
              (!parent && proto) ? proto->getParentValue()
                                 : ObjectOrNullTag(parent),
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
        AutoObjectRooter tvr(cx, obj);
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
GetClassProtoKey(js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

static inline JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          size_t objectSize = 0)
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

} /* namespace js */

#endif /* jsobjinlines_h___ */
