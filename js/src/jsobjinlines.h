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
#include "jsscope.h"
#include "jsstaticcheck.h"
#include "jsxml.h"

/* Headers included for inline implementations used by this header. */
#include "jsbool.h"
#include "jscntxt.h"
#include "jsnum.h"
#include "jsscopeinlines.h"
#include "jsstr.h"

#include "jsgcinlines.h"
#include "jsprobes.h"

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
    if (clasp->ext.equality)
        flags |= JSObject::HAS_EQUALITY;
}

inline void
JSObject::finalize(JSContext *cx)
{
    /* Cope with stillborn objects that have no map. */
    if (!map)
        return;

    /* Finalize obj first, in case it needs map and slots. */
    js::Class *clasp = getClass();
    if (clasp->finalize)
        clasp->finalize(cx, this);

    js::Probes::finalizeObject(this);

    finish(cx);
}

/*
 * Property read barrier for deferred cloning of compiler-created function
 * objects optimized as typically non-escaping, ad-hoc methods in obj.
 */
inline bool
JSObject::methodReadBarrier(JSContext *cx, const js::Shape &shape, js::Value *vp)
{
    JS_ASSERT(canHaveMethodBarrier());
    JS_ASSERT(hasMethodBarrier());
    JS_ASSERT(nativeContains(shape));
    JS_ASSERT(shape.isMethod());
    JS_ASSERT(&shape.methodObject() == &vp->toObject());

    JSObject *funobj = &vp->toObject();
    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
    JS_ASSERT(fun == funobj && FUN_NULL_CLOSURE(fun));

    funobj = CloneFunctionObject(cx, fun, funobj->getParent());
    if (!funobj)
        return false;
    funobj->setMethodObj(*this);

    vp->setObject(*funobj);
    if (!js_SetPropertyHelper(cx, this, shape.id, 0, vp, false))
        return false;

#ifdef DEBUG
    if (cx->runtime->functionMeterFilename) {
        JS_FUNCTION_METER(cx, mreadbarrier);

        typedef JSRuntime::FunctionCountMap HM;
        HM &h = cx->runtime->methodReadBarrierCountMap;
        HM::AddPtr p = h.lookupForAdd(fun);
        if (!p) {
            h.add(p, fun, 1);
        } else {
            JS_ASSERT(p->key == fun);
            ++p->value;
        }
    }
#endif
    return true;
}

static JS_ALWAYS_INLINE bool
ChangesMethodValue(const js::Value &prev, const js::Value &v)
{
    JSObject *prevObj;
    return prev.isObject() && (prevObj = &prev.toObject())->isFunction() &&
           (!v.isObject() || &v.toObject() != prevObj);
}

inline bool
JSObject::methodWriteBarrier(JSContext *cx, const js::Shape &shape, const js::Value &v)
{
    if (brandedOrHasMethodBarrier() && shape.slot != SHAPE_INVALID_SLOT) {
        const js::Value &prev = nativeGetSlot(shape.slot);

        if (ChangesMethodValue(prev, v)) {
            JS_FUNCTION_METER(cx, mwritebarrier);
            return methodShapeChange(cx, shape);
        }
    }
    return true;
}

inline bool
JSObject::methodWriteBarrier(JSContext *cx, uint32 slot, const js::Value &v)
{
    if (brandedOrHasMethodBarrier()) {
        const js::Value &prev = nativeGetSlot(slot);

        if (ChangesMethodValue(prev, v)) {
            JS_FUNCTION_METER(cx, mwslotbarrier);
            return methodShapeChange(cx, slot);
        }
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
    return js::gc::FinalizeKind(arena()->header()->thingKind);
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
           + isFun ? sizeof(JSFunction) : sizeof(JSObject);
}

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return (uint32)(size_t) getPrivate();
}

inline void
JSObject::setArrayLength(uint32 length)
{
    JS_ASSERT(isArray());
    setPrivate((void*) length);
}

inline uint32
JSObject::getDenseArrayCapacity()
{
    JS_ASSERT(isDenseArray());
    return numSlots();
}

inline js::Value*
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

inline js::Value *
JSObject::addressOfDenseArrayElement(uintN idx)
{
    JS_ASSERT(isDenseArray());
    return &getSlotRef(idx);
}

inline void
JSObject::setDenseArrayElement(uintN idx, const js::Value &val)
{
    JS_ASSERT(isDenseArray());
    setSlot(idx, val);
}

inline void
JSObject::shrinkDenseArrayElements(JSContext *cx, uintN cap)
{
    JS_ASSERT(isDenseArray());
    shrinkSlots(cx, cap);
}

inline void
JSObject::setArgsLength(uint32 argc)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    JS_ASSERT(UINT32_MAX > (uint64(argc) << ARGS_PACKED_BITS_COUNT));
    getSlotRef(JSSLOT_ARGS_LENGTH).setInt32(argc << ARGS_PACKED_BITS_COUNT);
    JS_ASSERT(!isArgsLengthOverridden());
}

inline uint32
JSObject::getArgsInitialLength() const
{
    JS_ASSERT(isArguments());
    uint32 argc = uint32(getSlot(JSSLOT_ARGS_LENGTH).toInt32()) >> ARGS_PACKED_BITS_COUNT;
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    return argc;
}

inline void
JSObject::setArgsLengthOverridden()
{
    JS_ASSERT(isArguments());
    getSlotRef(JSSLOT_ARGS_LENGTH).getInt32Ref() |= ARGS_LENGTH_OVERRIDDEN_BIT;
}

inline bool
JSObject::isArgsLengthOverridden() const
{
    JS_ASSERT(isArguments());
    const js::Value &v = getSlot(JSSLOT_ARGS_LENGTH);
    return v.toInt32() & ARGS_LENGTH_OVERRIDDEN_BIT;
}

inline js::ArgumentsData *
JSObject::getArgsData() const
{
    JS_ASSERT(isArguments());
    return (js::ArgumentsData *) getSlot(JSSLOT_ARGS_DATA).toPrivate();
}

inline void
JSObject::setArgsData(js::ArgumentsData *data)
{
    JS_ASSERT(isArguments());
    getSlotRef(JSSLOT_ARGS_DATA).setPrivate(data);
}

inline const js::Value &
JSObject::getArgsCallee() const
{
    return getArgsData()->callee;
}

inline void
JSObject::setArgsCallee(const js::Value &callee)
{
    getArgsData()->callee = callee;
}

inline const js::Value &
JSObject::getArgsElement(uint32 i) const
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < getArgsInitialLength());
    return getArgsData()->slots[i];
}

inline js::Value *
JSObject::getArgsElements() const
{
    JS_ASSERT(isArguments());
    return getArgsData()->slots;
}

inline js::Value *
JSObject::addressOfArgsElement(uint32 i)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < getArgsInitialLength());
    return &getArgsData()->slots[i];
}

inline void
JSObject::setArgsElement(uint32 i, const js::Value &v)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(i < getArgsInitialLength());
    getArgsData()->slots[i] = v;
}

inline void
JSObject::setCallObjCallee(JSObject &callee)
{
    JS_ASSERT(isCall());
    JS_ASSERT(callee.isFunction());
    return getSlotRef(JSSLOT_CALL_CALLEE).setObject(callee);
}

inline JSObject &
JSObject::getCallObjCallee() const
{
    JS_ASSERT(isCall());
    return getSlot(JSSLOT_CALL_CALLEE).toObject();
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
    return getSlot(JSSLOT_CALL_ARGUMENTS);
}

inline void
JSObject::setCallObjArguments(const js::Value &v)
{
    JS_ASSERT(isCall());
    setSlot(JSSLOT_CALL_ARGUMENTS, v);
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
    JS_ASSERT(isFunction());
    JS_ASSERT(FUN_FLAT_CLOSURE(getFunctionPrivate()));
    return (js::Value *) getSlot(JSSLOT_FLAT_CLOSURE_UPVARS).toPrivate();
}

inline js::Value
JSObject::getFlatClosureUpvar(uint32 i) const
{
    return getFlatClosureUpvars()[i];
}

inline void
JSObject::setFlatClosureUpvars(js::Value *upvars)
{
    JS_ASSERT(isFunction());
    JS_ASSERT(FUN_FLAT_CLOSURE(getFunctionPrivate()));
    getSlotRef(JSSLOT_FLAT_CLOSURE_UPVARS).setPrivate(upvars);
}

inline bool
JSObject::hasMethodObj(const JSObject& obj) const
{
    return JSSLOT_FUN_METHOD_OBJ < numSlots() &&
        getSlot(JSSLOT_FUN_METHOD_OBJ).isObject() &&
        &getSlot(JSSLOT_FUN_METHOD_OBJ).toObject() == &obj;
}

inline void
JSObject::setMethodObj(JSObject& obj)
{
    getSlotRef(JSSLOT_FUN_METHOD_OBJ).setObject(obj);
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

inline jsval
JSObject::getNamePrefix() const
{
    JS_ASSERT(isNamespace() || isQName());
    return js::Jsvalify(getSlot(JSSLOT_NAME_PREFIX));
}

inline void
JSObject::setNamePrefix(jsval prefix)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_PREFIX, js::Valueify(prefix));
}

inline jsval
JSObject::getNameURI() const
{
    JS_ASSERT(isNamespace() || isQName());
    return js::Jsvalify(getSlot(JSSLOT_NAME_URI));
}

inline void
JSObject::setNameURI(jsval uri)
{
    JS_ASSERT(isNamespace() || isQName());
    setSlot(JSSLOT_NAME_URI, js::Valueify(uri));
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

inline jsval
JSObject::getQNameLocalName() const
{
    JS_ASSERT(isQName());
    return js::Jsvalify(getSlot(JSSLOT_QNAME_LOCAL_NAME));
}

inline void
JSObject::setQNameLocalName(jsval name)
{
    JS_ASSERT(isQName());
    setSlot(JSSLOT_QNAME_LOCAL_NAME, js::Valueify(name));
}

inline JSObject *
JSObject::getWithThis() const
{
    return &getSlot(JSSLOT_WITH_THIS).toObject();
}

inline void
JSObject::setWithThis(JSObject *thisp)
{
    getSlotRef(JSSLOT_WITH_THIS).setObject(*thisp);
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
     * we set map to null in DEBUG builds, and set objShape to a value we then
     * assert obj->shape() never returns.
     */
    map = NULL;
    objShape = JSObjectMap::INVALID_SHAPE;
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
#ifdef DEBUG
    if (isNative())
        JS_LOCK_RUNTIME_VOID(cx->runtime, cx->runtime->liveObjectProps -= propertyCount());
#endif
    if (hasSlotsArray())
        freeSlotsArray(cx);
    if (emptyShapes)
        cx->free(emptyShapes);
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
    cx->free(slots);
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
        getter = setter = (PropertyOp) NULL;
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
    JS_ASSERT(!obj->map);
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
    JS_ASSERT(proto->isNative());
    JS_ASSERT(parent);

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC.
     */
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
        OBJ_TO_INNER_OBJECT(cx, global);
        if (!global)
            return NULL;
    } else {
        global = cx->fp()->scopeChain().getGlobal();
    }
    JS_ASSERT(global->getClass()->flags & JSCLASS_IS_GLOBAL);

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
namespace detail
{
template <bool withProto, bool isFunction>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent,
          gc::FinalizeKind kind)
{
    /* Bootstrap the ur-object, and make it the default prototype object. */
    if (withProto == WithProto::Class && !proto) {
        JSProtoKey protoKey = GetClassProtoKey(clasp);
        if (!js_GetClassPrototype(cx, parent, protoKey, &proto, clasp))
            return NULL;
        if (!proto && !js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
            return NULL;
    }

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC. Functions have a
     * larger non-standard allocation size.
     *
     * The should be specialized by the template.
     */
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

/* Creates a new array with a zero length and the given finalize kind. */
static inline JSObject *
NewArrayWithKind(JSContext* cx, gc::FinalizeKind kind)
{
    return NewNonFunction<WithProto::Class>(cx, &js_ArrayClass, NULL, NULL, kind);
}

/*
 * As for js_GetGCObjectKind, where numSlots is a guess at the final size of
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

} /* namespace js */

#endif /* jsobjinlines_h___ */
