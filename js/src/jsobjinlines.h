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

inline void
JSObject::dropProperty(JSContext *cx, JSProperty *prop)
{
    JS_ASSERT(prop);
    if (isNative())
        JS_UNLOCK_OBJ(cx, this);
}

inline void
JSObject::seal(JSContext *cx)
{
    JS_ASSERT(!sealed());
    if (isNative())
        generateOwnShape(cx);
    flags |= SEALED;
}

inline bool
JSObject::brand(JSContext *cx, uint32 slot, js::Value v)
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
    if (!branded())
        setGeneric();
    return true;
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
    if (!js_SetPropertyHelper(cx, this, shape.id, 0, vp))
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
    if (flags & (BRANDED | METHOD_BARRIER)) {
        const js::Value &prev = lockedGetSlot(shape.slot);

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
    if (flags & (BRANDED | METHOD_BARRIER)) {
        const js::Value &prev = lockedGetSlot(slot);

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
JSObject::getSlotMT(JSContext *cx, uintN slot)
{
#ifdef JS_THREADSAFE
    /*
     * If thread-safe, define a getSlotMT() that bypasses, for a native object,
     * the lock-free "fast path" test of (obj->title.ownercx == cx), to avoid
     * needlessly switching from lock-free to lock-full scope when doing GC on
     * a different context from the last one to own the scope. The caller in
     * this case is probably a JSClass.mark function, e.g., fun_mark, or maybe
     * a finalizer.
     */
    OBJ_CHECK_SLOT(this, slot);
    return (title.ownercx == cx)
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
    if (title.ownercx == cx)
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
    return (slot < numSlots()) ? getSlot(slot) : js::UndefinedValue();
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

inline uint32
JSObject::getArrayLength() const
{
    JS_ASSERT(isArray());
    return fslots[JSSLOT_ARRAY_LENGTH].toPrivateUint32();
}

inline void
JSObject::setArrayLength(uint32 length)
{
    JS_ASSERT(isArray());
    fslots[JSSLOT_ARRAY_LENGTH].setPrivateUint32(length);
}

inline uint32
JSObject::getDenseArrayCapacity() const
{
    JS_ASSERT(isDenseArray());
    return fslots[JSSLOT_DENSE_ARRAY_CAPACITY].toPrivateUint32();
}

inline void
JSObject::setDenseArrayCapacity(uint32 capacity)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(!capacity == !dslots);
    fslots[JSSLOT_DENSE_ARRAY_CAPACITY].setPrivateUint32(capacity);
}

inline size_t
JSObject::slotsAndStructSize(uint32 nslots) const
{
    int ndslots;
    if (isDenseArray())
        ndslots = getDenseArrayCapacity() + 1;
    else {
        ndslots = nslots - JS_INITIAL_NSLOTS;
        if (ndslots <= 0)
            ndslots = 0;
        else
            ndslots++; /* number of total slots is stored at index -1 */
    }

    return sizeof(js::Value) * ndslots
           + (isFunction() && !getPrivate()) ? sizeof(JSFunction) : sizeof(JSObject);
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
}

inline void
JSObject::voidDenseOnlyArraySlots()
{
    JS_ASSERT(isDenseArray());
    fslots[JSSLOT_DENSE_ARRAY_CAPACITY].setUndefined();
}

inline void
JSObject::setArgsLength(uint32 argc)
{
    JS_ASSERT(isArguments());
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    JS_ASSERT(UINT32_MAX > (uint64(argc) << ARGS_PACKED_BITS_COUNT));
    fslots[JSSLOT_ARGS_LENGTH].setInt32(argc << ARGS_PACKED_BITS_COUNT);
    JS_ASSERT(!isArgsLengthOverridden());
}

inline uint32
JSObject::getArgsInitialLength() const
{
    JS_ASSERT(isArguments());
    uint32 argc = uint32(fslots[JSSLOT_ARGS_LENGTH].toInt32()) >> ARGS_PACKED_BITS_COUNT;
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    return argc;
}

inline void
JSObject::setArgsLengthOverridden()
{
    JS_ASSERT(isArguments());
    fslots[JSSLOT_ARGS_LENGTH].getInt32Ref() |= ARGS_LENGTH_OVERRIDDEN_BIT;
}

inline bool
JSObject::isArgsLengthOverridden() const
{
    JS_ASSERT(isArguments());
    const js::Value &v = fslots[JSSLOT_ARGS_LENGTH];
    return v.toInt32() & ARGS_LENGTH_OVERRIDDEN_BIT;
}

inline js::ArgumentsData *
JSObject::getArgsData() const
{
    JS_ASSERT(isArguments());
    return (js::ArgumentsData *) fslots[JSSLOT_ARGS_DATA].toPrivate();
}

inline void
JSObject::setArgsData(js::ArgumentsData *data)
{
    JS_ASSERT(isArguments());
    fslots[JSSLOT_ARGS_DATA].setPrivate(data);
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
JSObject::addressOfArgsElement(uint32 i) const
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

inline js::Value *
JSObject::getFlatClosureUpvars() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(FUN_FLAT_CLOSURE(getFunctionPrivate()));
    return (js::Value *) fslots[JSSLOT_FLAT_CLOSURE_UPVARS].toPrivate();
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
    fslots[JSSLOT_FLAT_CLOSURE_UPVARS].setPrivate(upvars);
}

inline bool
JSObject::hasMethodObj(const JSObject& obj) const
{
    return fslots[JSSLOT_FUN_METHOD_OBJ].isObject() &&
           &fslots[JSSLOT_FUN_METHOD_OBJ].toObject() == &obj;
}

inline void
JSObject::setMethodObj(JSObject& obj)
{
    fslots[JSSLOT_FUN_METHOD_OBJ].setObject(obj);
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

inline JSObject *
JSObject::getWithThis() const
{
    return &fslots[JSSLOT_WITH_THIS].toObject();
}

inline void
JSObject::setWithThis(JSObject *thisp)
{
    fslots[JSSLOT_WITH_THIS].setObject(*thisp);
}

inline void
JSObject::initCommon(js::Class *aclasp, JSObject *proto, JSObject *parent,
                     JSContext *cx)
{
    JS_STATIC_ASSERT(JSSLOT_PRIVATE + 3 == JS_INITIAL_NSLOTS);

    clasp = aclasp;
    flags = 0;
    freeslot = JSSLOT_START(aclasp);

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
    fslots[JSSLOT_PRIVATE + 1].setUndefined();
    fslots[JSSLOT_PRIVATE + 2].setUndefined();

    dslots = NULL;

#ifdef JS_THREADSAFE
    js_InitTitle(cx, &title);
#endif

    emptyShape = NULL;
}

inline void
JSObject::init(js::Class *aclasp, JSObject *proto, JSObject *parent,
               const js::Value &privateSlotValue, JSContext *cx)
{
    initCommon(aclasp, proto, parent, cx);
    fslots[JSSLOT_PRIVATE] = privateSlotValue;
}

inline void
JSObject::init(js::Class *aclasp, JSObject *proto, JSObject *parent,
               void *priv, JSContext *cx)
{
    initCommon(aclasp, proto, parent, cx);
    *(void **)&fslots[JSSLOT_PRIVATE] = priv;
}

inline void
JSObject::init(js::Class *aclasp, JSObject *proto, JSObject *parent,
               JSContext *cx)
{
    initCommon(aclasp, proto, parent, cx);
    if (clasp->flags & JSCLASS_HAS_PRIVATE)
        *(void **)&fslots[JSSLOT_PRIVATE] = NULL;
    else
        fslots[JSSLOT_PRIVATE].setUndefined();
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
#ifdef JS_THREADSAFE
    js_FinishTitle(cx, &title);
#endif
}

inline void
JSObject::initSharingEmptyShape(js::Class *aclasp,
                                JSObject *proto,
                                JSObject *parent,
                                const js::Value &privateSlotValue,
                                JSContext *cx)
{
    init(aclasp, proto, parent, privateSlotValue, cx);

    js::EmptyShape *empty = proto->emptyShape;
    JS_ASSERT(empty->getClass() == aclasp);
    setMap(empty);
}

inline void
JSObject::initSharingEmptyShape(js::Class *aclasp,
                                JSObject *proto,
                                JSObject *parent,
                                void *priv,
                                JSContext *cx)
{
    init(aclasp, proto, parent, priv, cx);

    js::EmptyShape *empty = proto->emptyShape;
    JS_ASSERT(empty->getClass() == aclasp);
    setMap(empty);
}

inline void
JSObject::freeSlotsArray(JSContext *cx)
{
    JS_ASSERT(hasSlotsArray());
    JS_ASSERT(dslots[-1].toPrivateUint32() > JS_INITIAL_NSLOTS);
    cx->free(dslots - 1);
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
    if (prop)
        pobj->dropProperty(cx, prop);
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

inline size_t
JSObject::flagsOffset()
{
    static size_t offset = 0;
    if (offset)
        return offset;

    /* 
     * We can't address a bitfield, so instead we create a struct, set only
     * the field we care about, then search for it.
     */
    JSObject fakeObj;
    memset(&fakeObj, 0, sizeof(fakeObj));
    fakeObj.flags = 1;
    for (unsigned testOffset = 0; testOffset < sizeof(fakeObj); testOffset += sizeof(uint32)) {
        uint32 *ptr = reinterpret_cast<uint32 *>(reinterpret_cast<char *>(&fakeObj) + testOffset);
        if (*ptr) {
            JS_ASSERT(*ptr == 1);
            offset = testOffset;
            return offset;
        }
    }
    JS_NOT_REACHED("memory weirdness");
    return 0;
}

inline uint32
JSObject::flagsAndFreeslot()
{
    size_t offset = flagsOffset();
    char *ptr = offset + (char*) this;
    return *(uint32*)ptr;
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
InitScopeForObject(JSContext* cx, JSObject* obj, js::Class *clasp, JSObject* proto)
{
    JS_ASSERT(clasp->isNative());
    JS_ASSERT(proto == obj->getProto());

    /* Share proto's emptyShape only if obj is similar to proto. */
    js::EmptyShape *empty = NULL;

    if (proto) {
        JS_LOCK_OBJ(cx, proto);
        if (proto->canProvideEmptyShape(clasp)) {
            empty = proto->getEmptyShape(cx, clasp);
            JS_UNLOCK_OBJ(cx, proto);
            if (!empty)
                goto bad;
        } else {
            JS_UNLOCK_OBJ(cx, proto);
        }
    }

    if (!empty) {
        uint32 freeslot = JSSLOT_FREE(clasp);
        JS_ASSERT(freeslot >= JSSLOT_PRIVATE);

        empty = js::EmptyShape::create(cx, clasp);
        if (!empty)
            goto bad;
        if (freeslot > JS_INITIAL_NSLOTS && !obj->allocSlots(cx, freeslot))
            goto bad;
        obj->freeslot = freeslot;
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
NewNativeClassInstance(JSContext *cx, Class *clasp, JSObject *proto, JSObject *parent)
{
    JS_ASSERT(proto);
    JS_ASSERT(proto->isNative());
    JS_ASSERT(parent);

    /*
     * Allocate an object from the GC heap and initialize all its fields before
     * doing any operation that can potentially trigger GC.
     */
    JSObject* obj = js_NewGCObject(cx);

    if (obj) {
        /*
         * Default parent to the parent of the prototype, which was set from
         * the parent of the prototype's constructor.
         */
        obj->init(clasp, proto, parent, cx);

        JS_LOCK_OBJ(cx, proto);
        JS_ASSERT(proto->canProvideEmptyShape(clasp));
        js::EmptyShape *empty = proto->getEmptyShape(cx, clasp);
        JS_UNLOCK_OBJ(cx, proto);

        if (empty)
            obj->setMap(empty);
        else
            obj = NULL;
    }

    return obj;
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
NewBuiltinClassInstance(JSContext *cx, Class *clasp)
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
        global = cx->fp()->getScopeChain()->getGlobal();
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

    return NewNativeClassInstance(cx, clasp, proto, global);
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
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
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
    JSObject* obj = isFunction ? js_NewGCFunction(cx) : js_NewGCObject(cx);
    if (!obj)
        goto out;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    obj->init(clasp, proto, (!parent && proto) ? proto->getParent() : parent, cx);

    if (clasp->isNative()) {
        if (!InitScopeForObject(cx, obj, clasp, proto)) {
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
}

static JS_ALWAYS_INLINE JSObject *
NewFunction(JSContext *cx, JSObject *parent)
{
    return detail::NewObject<WithProto::Class, true>(cx, &js_FunctionClass, NULL, parent);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewNonFunction(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    return detail::NewObject<withProto, false>(cx, clasp, proto, parent);
}

template <WithProto::e withProto>
static JS_ALWAYS_INLINE JSObject *
NewObject(JSContext *cx, js::Class *clasp, JSObject *proto, JSObject *parent)
{
    return (clasp == &js_FunctionClass)
           ? detail::NewObject<withProto, true>(cx, clasp, proto, parent)
           : detail::NewObject<withProto, false>(cx, clasp, proto, parent);
}

} /* namespace js */

#endif /* jsobjinlines_h___ */
