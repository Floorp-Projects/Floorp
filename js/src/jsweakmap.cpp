/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsweakmap.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsobj.h"
#include "jswrapper.h"

#include "vm/GlobalObject.h"
#include "vm/WeakMapObject.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

WeakMapBase::WeakMapBase(JSObject *memOf, JSCompartment *c)
  : memberOf(memOf),
    compartment(c),
    next(WeakMapNotInList),
    marked(false)
{
    JS_ASSERT_IF(memberOf, memberOf->compartment() == c);
}

WeakMapBase::~WeakMapBase()
{
    JS_ASSERT(!isInList());
}

void
WeakMapBase::trace(JSTracer *tracer)
{
    JS_ASSERT(isInList());
    if (IS_GC_MARKING_TRACER(tracer)) {
        // We don't trace any of the WeakMap entries at this time, just record
        // record the fact that the WeakMap has been marked. Enties are marked
        // in the iterative marking phase by markAllIteratively(), which happens
        // when many keys as possible have been marked already.
        JS_ASSERT(tracer->eagerlyTraceWeakMaps() == DoNotTraceWeakMaps);
        marked = true;
    } else {
        // If we're not actually doing garbage collection, the keys won't be marked
        // nicely as needed by the true ephemeral marking algorithm --- custom tracers
        // such as the cycle collector must use their own means for cycle detection.
        // So here we do a conservative approximation: pretend all keys are live.
        if (tracer->eagerlyTraceWeakMaps() == DoNotTraceWeakMaps)
            return;

        nonMarkingTraceValues(tracer);
        if (tracer->eagerlyTraceWeakMaps() == TraceWeakMapKeysValues)
            nonMarkingTraceKeys(tracer);
    }
}

void
WeakMapBase::unmarkCompartment(JSCompartment *c)
{
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next)
        m->marked = false;
}

bool
WeakMapBase::markCompartmentIteratively(JSCompartment *c, JSTracer *tracer)
{
    bool markedAny = false;
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next) {
        if (m->marked && m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

bool
WeakMapBase::findZoneEdgesForCompartment(JSCompartment *c)
{
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next) {
        if (!m->findZoneEdges())
            return false;
    }
    return true;
}

void
WeakMapBase::sweepCompartment(JSCompartment *c)
{
    WeakMapBase **tailPtr = &c->gcWeakMapList;
    for (WeakMapBase *m = c->gcWeakMapList, *next; m; m = next) {
        next = m->next;
        if (m->marked) {
            m->sweep();
            *tailPtr = m;
            tailPtr = &m->next;
        } else {
            /* Destroy the hash map now to catch any use after this point. */
            m->finish();
            m->next = WeakMapNotInList;
        }
    }
    *tailPtr = nullptr;

#ifdef DEBUG
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next)
        JS_ASSERT(m->isInList() && m->marked);
#endif
}

void
WeakMapBase::traceAllMappings(WeakMapTracer *tracer)
{
    JSRuntime *rt = tracer->runtime;
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next)
            m->traceMappings(tracer);
    }
}

bool
WeakMapBase::saveCompartmentMarkedWeakMaps(JSCompartment *c, WeakMapSet &markedWeakMaps)
{
    for (WeakMapBase *m = c->gcWeakMapList; m; m = m->next) {
        if (m->marked && !markedWeakMaps.put(m))
            return false;
    }
    return true;
}

void
WeakMapBase::restoreCompartmentMarkedWeakMaps(WeakMapSet &markedWeakMaps)
{
    for (WeakMapSet::Range r = markedWeakMaps.all(); !r.empty(); r.popFront()) {
        WeakMapBase *map = r.front();
        JS_ASSERT(map->compartment->zone()->isGCMarking());
        JS_ASSERT(!map->marked);
        map->marked = true;
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

bool
ObjectValueMap::findZoneEdges()
{
    /*
     * For unmarked weakmap keys with delegates in a different zone, add a zone
     * edge to ensure that the delegate zone does finish marking after the key
     * zone.
     */
    JS::AutoSuppressGCAnalysis nogc;
    Zone *mapZone = compartment->zone();
    for (Range r = all(); !r.empty(); r.popFront()) {
        JSObject *key = r.front().key();
        if (key->isMarked(BLACK) && !key->isMarked(GRAY))
            continue;
        JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp;
        if (!op)
            continue;
        JSObject *delegate = op(key);
        if (!delegate)
            continue;
        Zone *delegateZone = delegate->zone();
        if (delegateZone == mapZone)
            continue;
        if (!delegateZone->gcZoneGroupEdges.put(key->zone()))
            return false;
    }
    return true;
}

static JSObject *
GetKeyArg(JSContext *cx, CallArgs &args)
{
    if (args[0].isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT);
        return nullptr;
    }
    return &args[0].toObject();
}

MOZ_ALWAYS_INLINE bool
IsWeakMap(HandleValue v)
{
    return v.isObject() && v.toObject().is<WeakMapObject>();
}

MOZ_ALWAYS_INLINE bool
WeakMap_has_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.has", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        if (map->has(key)) {
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

static bool
WeakMap_has(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_has_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_clear_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    // We can't js_delete the weakmap because the data gathered during GC is
    // used by the Cycle Collector.
    if (ObjectValueMap *map = args.thisv().toObject().as<WeakMapObject>().getMap())
        map->clear();

    args.rval().setUndefined();
    return true;
}

static bool
WeakMap_clear(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_clear_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_get_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.get", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            args.rval().set(ptr->value());
            return true;
        }
    }

    args.rval().set((args.length() > 1) ? args[1] : UndefinedValue());
    return true;
}

static bool
WeakMap_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_get_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_delete_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.delete", "0", "s");
        return false;
    }
    JSObject *key = GetKeyArg(cx, args);
    if (!key)
        return false;

    if (ObjectValueMap *map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            map->remove(ptr);
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

static bool
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
        (obj->is<ProxyObject>() &&
         obj->as<ProxyObject>().handler()->family() == GetDOMProxyHandlerFamily()))
    {
        JS_ASSERT(cx->runtime()->preserveWrapperCallback);
        if (!cx->runtime()->preserveWrapperCallback(cx, obj)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_WEAKMAP_KEY);
            return false;
        }
    }
    return true;
}

static inline void
WeakMapPostWriteBarrier(JSRuntime *rt, ObjectValueMap *weakMap, JSObject *key)
{
#ifdef JSGC_GENERATIONAL
    /*
     * Strip the barriers from the type before inserting into the store buffer.
     * This will automatically ensure that barriers do not fire during GC.
     *
     * Some compilers complain about instantiating the WeakMap class for
     * unbarriered type arguments, so we cast to a HashMap instead.  Because of
     * WeakMap's multiple inheritace, We need to do this in two stages, first to
     * the HashMap base class and then to the unbarriered version.
     */
    ObjectValueMap::Base *baseHashMap = static_cast<ObjectValueMap::Base *>(weakMap);

    typedef HashMap<JSObject *, Value> UnbarrieredMap;
    UnbarrieredMap *unbarrieredMap = reinterpret_cast<UnbarrieredMap *>(baseHashMap);

    typedef HashKeyRef<UnbarrieredMap, JSObject *> Ref;
    if (key && IsInsideNursery(key))
        rt->gc.storeBuffer.putGeneric(Ref((unbarrieredMap), key));
#endif
}

MOZ_ALWAYS_INLINE bool
SetWeakMapEntryInternal(JSContext *cx, Handle<WeakMapObject*> mapObj,
                        HandleObject key, HandleValue value)
{
    ObjectValueMap *map = mapObj->getMap();
    if (!map) {
        map = cx->new_<ObjectValueMap>(cx, mapObj.get());
        if (!map)
            return false;
        if (!map->init()) {
            js_delete(map);
            JS_ReportOutOfMemory(cx);
            return false;
        }
        mapObj->setPrivate(map);
    }

    // Preserve wrapped native keys to prevent wrapper optimization.
    if (!TryPreserveReflector(cx, key))
        return false;

    if (JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp) {
        RootedObject delegate(cx, op(key));
        if (delegate && !TryPreserveReflector(cx, delegate))
            return false;
    }

    JS_ASSERT(key->compartment() == mapObj->compartment());
    JS_ASSERT_IF(value.isObject(), value.toObject().compartment() == mapObj->compartment());
    if (!map->put(key, value)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    WeakMapPostWriteBarrier(cx->runtime(), map, key.get());
    return true;
}

MOZ_ALWAYS_INLINE bool
WeakMap_set_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsWeakMap(args.thisv()));

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "WeakMap.set", "0", "s");
        return false;
    }
    RootedObject key(cx, GetKeyArg(cx, args));
    if (!key)
        return false;

    RootedValue value(cx, (args.length() > 1) ? args[1] : UndefinedValue());
    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
    Rooted<WeakMapObject*> map(cx, &thisObj->as<WeakMapObject>());

    if (!SetWeakMapEntryInternal(cx, map, key, value))
        return false;
    args.rval().set(args.thisv());
    return true;
}

static bool
WeakMap_set(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_set_impl>(cx, args);
}

JS_FRIEND_API(bool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, HandleObject objArg, MutableHandleObject ret)
{
    RootedObject obj(cx, objArg);
    obj = UncheckedUnwrap(obj);
    if (!obj || !obj->is<WeakMapObject>()) {
        ret.set(nullptr);
        return true;
    }
    RootedObject arr(cx, NewDenseEmptyArray(cx));
    if (!arr)
        return false;
    ObjectValueMap *map = obj->as<WeakMapObject>().getMap();
    if (map) {
        // Prevent GC from mutating the weakmap while iterating.
        AutoSuppressGC suppress(cx);
        for (ObjectValueMap::Base::Range r = map->all(); !r.empty(); r.popFront()) {
            JS::ExposeObjectToActiveJS(r.front().key());
            RootedObject key(cx, r.front().key());
            if (!cx->compartment()->wrap(cx, &key))
                return false;
            if (!NewbornArrayPush(cx, arr, ObjectValue(*key)))
                return false;
        }
    }
    ret.set(arr);
    return true;
}

static void
WeakMap_mark(JSTracer *trc, JSObject *obj)
{
    if (ObjectValueMap *map = obj->as<WeakMapObject>().getMap())
        map->trace(trc);
}

static void
WeakMap_finalize(FreeOp *fop, JSObject *obj)
{
    if (ObjectValueMap *map = obj->as<WeakMapObject>().getMap()) {
#ifdef DEBUG
        map->~ObjectValueMap();
        memset(static_cast<void *>(map), 0xdc, sizeof(*map));
        fop->free_(map);
#else
        fop->delete_(map);
#endif
    }
}

JS_PUBLIC_API(JSObject*)
JS::NewWeakMapObject(JSContext *cx)
{
    return NewBuiltinClassInstance(cx, &WeakMapObject::class_);
}

JS_PUBLIC_API(bool)
JS::IsWeakMapObject(JSObject *obj)
{
    return obj->is<WeakMapObject>();
}

JS_PUBLIC_API(bool)
JS::GetWeakMapEntry(JSContext *cx, HandleObject mapObj, HandleObject key,
                    MutableHandleValue rval)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, key);
    rval.setUndefined();
    ObjectValueMap *map = mapObj->as<WeakMapObject>().getMap();
    if (!map)
        return true;
    if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
        // Read barrier to prevent an incorrectly gray value from escaping the
        // weak map. See the comment before UnmarkGrayChildren in gc/Marking.cpp
        ExposeValueToActiveJS(ptr->value().get());
        rval.set(ptr->value());
    }
    return true;
}

JS_PUBLIC_API(bool)
JS::SetWeakMapEntry(JSContext *cx, HandleObject mapObj, HandleObject key,
                    HandleValue val)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, key, val);
    Rooted<WeakMapObject*> rootedMap(cx, &mapObj->as<WeakMapObject>());
    return SetWeakMapEntryInternal(cx, rootedMap, key, val);
}

static bool
WeakMap_construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = NewBuiltinClassInstance(cx, &WeakMapObject::class_);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

const Class WeakMapObject::class_ = {
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
    nullptr,                 /* call        */
    nullptr,                 /* construct   */
    nullptr,                 /* xdrObject   */
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

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject weakMapProto(cx, global->createBlankPrototype(cx, &WeakMapObject::class_));
    if (!weakMapProto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, WeakMap_construct,
                                                      cx->names().WeakMap, 1));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, weakMapProto))
        return nullptr;

    if (!DefinePropertiesAndFunctions(cx, weakMapProto, nullptr, weak_map_methods))
        return nullptr;

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_WeakMap, ctor, weakMapProto))
        return nullptr;
    return weakMapProto;
}

