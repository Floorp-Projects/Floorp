/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
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

namespace js {

bool
WeakMapBase::markAllIteratively(JSTracer *tracer)
{
    bool markedAny = false;
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next) {
        if (m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

void
WeakMapBase::sweepAll(JSTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next)
        m->sweep(tracer);
}

void
WeakMapBase::traceAllMappings(WeakMapTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (WeakMapBase *m = rt->gcWeakMapList; m; m = m->next)
        m->traceMappings(tracer);
}

void
WeakMapBase::resetWeakMapList(JSRuntime *rt)
{
    JS_ASSERT(WeakMapNotInList != NULL);

    WeakMapBase *m = rt->gcWeakMapList;
    rt->gcWeakMapList = NULL;
    while (m) {
        WeakMapBase *n = m->next;
        m->next = WeakMapNotInList;
        m = n;
    }
}

bool
WeakMapBase::saveWeakMapList(JSRuntime *rt, WeakMapVector &vector)
{
    WeakMapBase *m = rt->gcWeakMapList;
    while (m) {
        if (!vector.append(m))
            return false;
        m = m->next;
    }
    return true;
}

void
WeakMapBase::restoreWeakMapList(JSRuntime *rt, WeakMapVector &vector)
{
    JS_ASSERT(!rt->gcWeakMapList);
    for (WeakMapBase **p = vector.begin(); p != vector.end(); p++) {
        WeakMapBase *m = *p;
        JS_ASSERT(m->next == WeakMapNotInList);
        m->next = rt->gcWeakMapList;
        rt->gcWeakMapList = m;
    }
}

} /* namespace js */

typedef WeakMap<HeapPtrObject, HeapValue> ObjectValueMap;

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
    JSObject &key = vp->toObject();

    // If the key is from another compartment, and we store the wrapper as the key
    // the wrapper might be GC-ed since it is not strong referenced (Bug 673468).
    // To avoid this we always use the unwrapped object as the key instead of its
    // security wrapper. This also means that if the keys are ever exposed they must
    // be re-wrapped (see: JS_NondeterministicGetWeakMapKeys).
    return JS_UnwrapObject(&key);
}

static bool
IsWeakMap(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&WeakMapClass);
}

static bool
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
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            args.rval() = BooleanValue(true);
            return true;
        }
    }

    args.rval() = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsWeakMap, WeakMap_has_impl, args);
}

static bool
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
            args.rval() = ptr->value;
            return true;
        }
    }

    args.rval() = (args.length() > 1) ? args[1] : UndefinedValue();
    return true;
}

static JSBool
WeakMap_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsWeakMap, WeakMap_get_impl, args);
}

static bool
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
            args.rval() = BooleanValue(true);
            return true;
        }
    }

    args.rval() = BooleanValue(false);
    return true;
}

static JSBool
WeakMap_delete(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsWeakMap, WeakMap_delete_impl, args);
}

static bool
WeakMap_set_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.set", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    Value value = (args.length() > 1) ? args[1] : UndefinedValue();

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
    ObjectValueMap *map = GetObjectMap(thisObj);
    if (!map) {
        map = cx->new_<ObjectValueMap>(cx, thisObj.get());
        if (!map->init()) {
            cx->delete_(map);
            JS_ReportOutOfMemory(cx);
            return false;
        }
        thisObj->setPrivate(map);
    }

    if (!map->put(key, value)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    // Preserve wrapped native keys to prevent wrapper optimization.
    if (key->getClass()->ext.isWrappedNative) {
        if (!cx->runtime->preserveWrapperCallback ||
            !cx->runtime->preserveWrapperCallback(cx, key)) {
            JS_ReportWarning(cx, "Failed to preserve wrapper of wrapped native weak map key.");
        }
    }

    args.rval().setUndefined();
    return true;
}

static JSBool
WeakMap_set(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsWeakMap, WeakMap_set_impl, args);
}

JS_FRIEND_API(JSBool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, JSObject *obj, JSObject **ret)
{
    if (!obj || !obj->isWeakMap()) {
        *ret = NULL;
        return true;
    }
    RootedObject arr(cx, NewDenseEmptyArray(cx));
    if (!arr)
        return false;
    ObjectValueMap *map = GetObjectMap(obj);
    if (map) {
        for (ObjectValueMap::Range r = map->nondeterministicAll(); !r.empty(); r.popFront()) {
            JSObject *key = r.front().key;
            // Re-wrapping the key (see comment of GetKeyArg)
            if (!JS_WrapObject(cx, &key))
                return false;

            if (!js_NewbornArrayPush(cx, arr, ObjectValue(*key)))
                return false;
        }
    }
    *ret = arr;
    return true;
}

static void
WeakMap_mark(JSTracer *trc, JSObject *obj)
{
    if (ObjectValueMap *map = GetObjectMap(obj))
        map->trace(trc);
}

static void
WeakMap_finalize(FreeOp *fop, JSObject *obj)
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
    JS_PropertyStub,         /* delProperty */
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

static JSFunctionSpec weak_map_methods[] = {
    JS_FN("has",    WeakMap_has, 1, 0),
    JS_FN("get",    WeakMap_get, 2, 0),
    JS_FN("delete", WeakMap_delete, 1, 0),
    JS_FN("set",    WeakMap_set, 2, 0),
    JS_FS_END
};

JSObject *
js_InitWeakMapClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    RootedObject weakMapProto(cx, global->createBlankPrototype(cx, &WeakMapClass));
    if (!weakMapProto)
        return NULL;

    RootedFunction ctor(cx, global->createConstructor(cx, WeakMap_construct,
                                                      CLASS_NAME(cx, WeakMap), 0));
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
