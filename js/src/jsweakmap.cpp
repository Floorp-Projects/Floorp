/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsweakmap.h"

#include "gc/Marking.h"
#include "vm/GlobalObject.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;

WeakMapBase::WeakMapBase(JSObject *memOf, JSCompartment *c)
  : memberOf(memOf),
    compartment(c),
    next(WeakMapNotInList)
{
    JS_ASSERT_IF(memberOf, memberOf->compartment() == c);
}

WeakMapBase::~WeakMapBase()
{
    JS_ASSERT(next == WeakMapNotInList);
}

bool
WeakMapBase::markCompartmentIteratively(JSCompartment *c, JSTracer *tracer)
{
    bool markedAny = false;
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next) {
        if (m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

void
WeakMapBase::sweepCompartment(JSCompartment *c)
{
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next)
        m->sweep();
}

void
WeakMapBase::traceAllMappings(WeakMapTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next)
            m->traceMappings(tracer);
    }
}

void
WeakMapBase::resetCompartmentWeakMapList(JSCompartment *c)
{
    JS_ASSERT(WeakMapNotInList != NULL);

    WeakMapBase *m = c->gcWeakMapList;
    c->gcWeakMapList = NULL;
    while (m) {
        WeakMapBase *n = m->next;
        m->next = WeakMapNotInList;
        m = n;
    }
}

bool
WeakMapBase::saveCompartmentWeakMapList(JSCompartment *c, WeakMapVector &vector)
{
    WeakMapBase *m = c->gcWeakMapList;
    while (m) {
        if (!vector.append(m))
            return false;
        m = m->next;
    }
    return true;
}

void
WeakMapBase::restoreCompartmentWeakMapLists(WeakMapVector &vector)
{
    for (WeakMapBase **p = vector.begin(); p != vector.end(); p++) {
        WeakMapBase *m = *p;
        JS_ASSERT(m->next == WeakMapNotInList);
        JSCompartment *c = m->compartment;
        m->next = c->gcWeakMapList;
        c->gcWeakMapList = m;
    }
}

void
WeakMapBase::removeWeakMapFromList(WeakMapBase *weakmap)
{
    JSCompartment *c = weakmap->compartment;
    for (WeakMapBase **p = &c->gcWeakMapList; *p; p = &(*p)->next) {
        if (*p == weakmap) {
            *p = (*p)->next;
            weakmap->next = WeakMapNotInList;
            break;
        }
    }
}

typedef WeakMap<EncapsulatedPtrObject, RelocatableValue> ObjectValueMap;

static ObjectValueMap *
GetObjectMap(JSObject *obj)
{
    JS_ASSERT(obj->isWeakMap());
    return (ObjectValueMap *)obj->getPrivate();
}

static JSObject *
GetKeyArg(JSContext *cx, CallArgs &args)
{
    Value *vp = &args[0];
    if (vp->isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
        return NULL;
    }
    return &vp->toObject();
}

JS_ALWAYS_INLINE bool
IsWeakMap(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&WeakMapClass);
}

JS_ALWAYS_INLINE bool
WeakMap_has_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.has", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = GetObjectMap(&args.thisv().toObject())) {
        if (map->has(key)) {
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

JSBool
WeakMap_has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_has_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
WeakMap_clear_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    // We can't js_delete the weakmap because the data gathered during GC
    // is used by the Cycle Collector
    if (ObjectValueMap *map = GetObjectMap(&args.thisv().toObject())) {
        map->clear();
    }

    args.rval().setUndefined();
    return true;
}

JSBool
WeakMap_clear(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_clear_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
WeakMap_get_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.get", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = GetObjectMap(&args.thisv().toObject())) {
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            // Read barrier to prevent an incorrectly gray value from escaping the
            // weak map. See the comment before UnmarkGrayChildren in gc/Marking.cpp
            ExposeValueToActiveJS(ptr->value.get());

            args.rval().set(ptr->value);
            return true;
        }
    }

    args.rval().set((args.length() > 1) ? args[1] : UndefinedValue());
    return true;
}

JSBool
WeakMap_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_get_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
WeakMap_delete_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.delete", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = GetObjectMap(&args.thisv().toObject())) {
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            map->remove(ptr);
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

JSBool
WeakMap_delete(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_delete_impl>(cx, args);
}

static bool
TryPreserveReflector(JSContext *cx, HandleObject obj)
{
    if (obj->getClass()->ext.isWrappedNative ||
        (obj->getClass()->flags & JSCLASS_IS_DOMJSCLASS) ||
        (obj->isProxy() && GetProxyHandler(obj)->family() == GetListBaseHandlerFamily()))
    {
        JS_ASSERT(cx->runtime->preserveWrapperCallback);
        if (!cx->runtime->preserveWrapperCallback(cx, obj)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_WEAKMAP_KEY);
            return false;
        }
    }
    return true;
}

JS_ALWAYS_INLINE bool
WeakMap_set_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.set", "0", "s");
        return false;
    }
    RootedObject key(cx, GetKeyArg(cx, args));
    if (!key)
        return false;

    RootedValue value(cx, (args.length() > 1) ? args[1] : UndefinedValue());

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
    ObjectValueMap *map = GetObjectMap(thisObj);
    if (!map) {
        map = cx->new_<ObjectValueMap>(cx, thisObj.get());
        if (!map->init()) {
            js_delete(map);
            JS_ReportOutOfMemory(cx);
            return false;
        }
        thisObj->setPrivate(map);
    }

    // Preserve wrapped native keys to prevent wrapper optimization.
    if (!TryPreserveReflector(cx, key))
        return false;

    if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp) {
        RootedObject delegate(cx, op(key));
        if (delegate && !TryPreserveReflector(cx, delegate))
            return false;
    }

    JS_ASSERT(key->compartment() == thisObj->compartment());
    JS_ASSERT_IF(value.isObject(), value.toObject().compartment() == thisObj->compartment());
    if (!map->put(key, value)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    HashTableWriteBarrierPost(cx->runtime, map, key.get());

    args.rval().setUndefined();
    return true;
}

JSBool
WeakMap_set(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_set_impl>(cx, args);
}

JS_FRIEND_API(JSBool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, JSObject *objArg, JSObject **ret)
{
    RootedObject obj(cx, objArg);
    obj = UncheckedUnwrap(obj);
    if (!obj || !obj->isWeakMap()) {
        *ret = NULL;
        return true;
    }
    RootedObject arr(cx, NewDenseEmptyArray(cx));
    if (!arr)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        for (ObjectValueMap::Base::Range r = map->all(); !r.empty(); r.popFront()) {
            RootedObject key(cx, r.front().key);
            if (!JS_WrapObject(cx, key.address()))
                return false;
            if (!js_NewbornArrayPush(cx, arr, ObjectValue(*key)))
                return false;
        }
    }
    *ret = arr;
    return true;
}

static void
WeakMap_mark(JSTracer *trc, RawObject obj)
{
    if (ObjectValueMap *map = GetObjectMap(obj))
        map->trace(trc);
}

static void
WeakMap_finalize(FreeOp *fop, RawObject obj)
{
    if (ObjectValueMap *map = GetObjectMap(obj)) {
        map->check();
#ifdef DEBUG
        map->~ObjectValueMap();
        memset(static_cast<void *>(map), 0xdc, sizeof(*map));
        fop->free_(map);
#else
        fop->delete_(map);
#endif
    }
}

static JSBool
WeakMap_construct(JSContext *cx, unsigned argc, Value *vp)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &WeakMapClass);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
}

Class js::WeakMapClass = {
    "WeakMap",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakMap),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    WeakMap_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* xdrObject   */
    WeakMap_mark
};

static const JSFunctionSpec weak_map_methods[] = {
    JS_FN("has",    WeakMap_has, 1, 0),
    JS_FN("get",    WeakMap_get, 2, 0),
    JS_FN("delete", WeakMap_delete, 1, 0),
    JS_FN("set",    WeakMap_set, 2, 0),
    JS_FN("clear",  WeakMap_clear, 0, 0),
    JS_FS_END
};

JSObject *
js_InitWeakMapClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    RootedObject weakMapProto(cx, global->createBlankPrototype(cx, &WeakMapClass));
    if (!weakMapProto)
        return NULL;

    RootedFunction ctor(cx, global->createConstructor(cx, WeakMap_construct,
                                                      cx->names().WeakMap, 0));
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, weakMapProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, weakMapProto, NULL, weak_map_methods))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_WeakMap, ctor, weakMapProto))
        return NULL;
    return weakMapProto;
}
