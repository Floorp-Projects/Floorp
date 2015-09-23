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

#include "js/GCAPI.h"
#include "vm/GlobalObject.h"
#include "vm/WeakMapObject.h"

#include "jsobjinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::UniquePtr;

WeakMapBase::WeakMapBase(JSObject* memOf, Zone* zone)
  : memberOf(memOf),
    zone(zone),
    next(WeakMapNotInList),
    marked(false)
{
    MOZ_ASSERT_IF(memberOf, memberOf->compartment()->zone() == zone);
}

WeakMapBase::~WeakMapBase()
{
    MOZ_ASSERT(CurrentThreadIsGCSweeping() || CurrentThreadIsHandlingInitFailure());
    MOZ_ASSERT_IF(CurrentThreadIsGCSweeping(), !isInList());
    if (isInList())
        removeWeakMapFromList(this);
}

void
WeakMapBase::trace(JSTracer* tracer)
{
    MOZ_ASSERT(isInList());
    if (tracer->isMarkingTracer()) {
        marked = true;
        if (tracer->weakMapAction() == DoNotTraceWeakMaps) {
            // Do not trace any WeakMap entries at this time. Just record the
            // fact that the WeakMap has been marked. Entries are marked in the
            // iterative marking phase by markAllIteratively(), after as many
            // keys as possible have been marked already.
        } else {
            MOZ_ASSERT(tracer->weakMapAction() == ExpandWeakMaps);
            markEphemeronEntries(tracer);
        }
    } else {
        // If we're not actually doing garbage collection, the keys won't be marked
        // nicely as needed by the true ephemeral marking algorithm --- custom tracers
        // such as the cycle collector must use their own means for cycle detection.
        // So here we do a conservative approximation: pretend all keys are live.
        if (tracer->weakMapAction() == DoNotTraceWeakMaps)
            return;

        nonMarkingTraceValues(tracer);
        if (tracer->weakMapAction() == TraceWeakMapKeysValues)
            nonMarkingTraceKeys(tracer);
    }
}

void
WeakMapBase::unmarkZone(JS::Zone* zone)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next)
        m->marked = false;
}

void
WeakMapBase::markAll(JS::Zone* zone, JSTracer* tracer)
{
    MOZ_ASSERT(tracer->weakMapAction() != DoNotTraceWeakMaps);
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        m->trace(tracer);
        if (m->memberOf)
            TraceEdge(tracer, &m->memberOf, "memberOf");
    }
}

bool
WeakMapBase::markZoneIteratively(JS::Zone* zone, JSTracer* tracer)
{
    bool markedAny = false;
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (m->marked && m->markIteratively(tracer))
            markedAny = true;
    }
    return markedAny;
}

bool
WeakMapBase::findInterZoneEdges(JS::Zone* zone)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (!m->findZoneEdges())
            return false;
    }
    return true;
}

void
WeakMapBase::sweepZone(JS::Zone* zone)
{
    WeakMapBase** tailPtr = &zone->gcWeakMapList;
    for (WeakMapBase* m = zone->gcWeakMapList; m; ) {
        WeakMapBase* next = m->next;
        if (m->marked) {
            m->sweep();
            *tailPtr = m;
            tailPtr = &m->next;
        } else {
            /* Destroy the hash map now to catch any use after this point. */
            m->finish();
            m->next = WeakMapNotInList;
        }
        m = next;
    }
    *tailPtr = nullptr;

#ifdef DEBUG
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next)
        MOZ_ASSERT(m->isInList() && m->marked);
#endif
}

void
WeakMapBase::traceAllMappings(WeakMapTracer* tracer)
{
    JSRuntime* rt = tracer->runtime;
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
            // The WeakMapTracer callback is not allowed to GC.
            JS::AutoSuppressGCAnalysis nogc;
            m->traceMappings(tracer);
        }
    }
}

bool
WeakMapBase::saveZoneMarkedWeakMaps(JS::Zone* zone, WeakMapSet& markedWeakMaps)
{
    for (WeakMapBase* m = zone->gcWeakMapList; m; m = m->next) {
        if (m->marked && !markedWeakMaps.put(m))
            return false;
    }
    return true;
}

void
WeakMapBase::restoreMarkedWeakMaps(WeakMapSet& markedWeakMaps)
{
    for (WeakMapSet::Range r = markedWeakMaps.all(); !r.empty(); r.popFront()) {
        WeakMapBase* map = r.front();
        MOZ_ASSERT(map->zone->isGCMarking());
        MOZ_ASSERT(!map->marked);
        map->marked = true;
    }
}

void
WeakMapBase::removeWeakMapFromList(WeakMapBase* weakmap)
{
    JS::Zone* zone = weakmap->zone;
    for (WeakMapBase** p = &zone->gcWeakMapList; *p; p = &(*p)->next) {
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
     * edge to ensure that the delegate zone finishes marking before the key
     * zone.
     */
    JS::AutoSuppressGCAnalysis nogc;
    for (Range r = all(); !r.empty(); r.popFront()) {
        JSObject* key = r.front().key();
        if (key->asTenured().isMarked(BLACK) && !key->asTenured().isMarked(GRAY))
            continue;
        JSWeakmapKeyDelegateOp op = key->getClass()->ext.weakmapKeyDelegateOp;
        if (!op)
            continue;
        JSObject* delegate = op(key);
        if (!delegate)
            continue;
        Zone* delegateZone = delegate->zone();
        if (delegateZone == zone)
            continue;
        if (!delegateZone->gcZoneGroupEdges.put(key->zone()))
            return false;
    }
    return true;
}

MOZ_ALWAYS_INLINE bool
IsWeakMap(HandleValue v)
{
    return v.isObject() && v.toObject().is<WeakMapObject>();
}

MOZ_ALWAYS_INLINE bool
WeakMap_has_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    if (!args.get(0).isObject()) {
        args.rval().setBoolean(false);
        return true;
    }

    if (ObjectValueMap* map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        JSObject* key = &args[0].toObject();
        if (map->has(key)) {
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

bool
js::WeakMap_has(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_has_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_clear_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    // We can't js_delete the weakmap because the data gathered during GC is
    // used by the Cycle Collector.
    if (ObjectValueMap* map = args.thisv().toObject().as<WeakMapObject>().getMap())
        map->clear();

    args.rval().setUndefined();
    return true;
}

bool
js::WeakMap_clear(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_clear_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_get_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    if (!args.get(0).isObject()) {
        args.rval().setUndefined();
        return true;
    }

    if (ObjectValueMap* map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        JSObject* key = &args[0].toObject();
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            args.rval().set(ptr->value());
            return true;
        }
    }

    args.rval().setUndefined();
    return true;
}

bool
js::WeakMap_get(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_get_impl>(cx, args);
}

MOZ_ALWAYS_INLINE bool
WeakMap_delete_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    if (!args.get(0).isObject()) {
        args.rval().setBoolean(false);
        return true;
    }

    if (ObjectValueMap* map = args.thisv().toObject().as<WeakMapObject>().getMap()) {
        JSObject* key = &args[0].toObject();
        if (ObjectValueMap::Ptr ptr = map->lookup(key)) {
            map->remove(ptr);
            args.rval().setBoolean(true);
            return true;
        }
    }

    args.rval().setBoolean(false);
    return true;
}

bool
js::WeakMap_delete(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_delete_impl>(cx, args);
}

static bool
TryPreserveReflector(JSContext* cx, HandleObject obj)
{
    if (obj->getClass()->ext.isWrappedNative ||
        (obj->getClass()->flags & JSCLASS_IS_DOMJSCLASS) ||
        (obj->is<ProxyObject>() &&
         obj->as<ProxyObject>().handler()->family() == GetDOMProxyHandlerFamily()))
    {
        MOZ_ASSERT(cx->runtime()->preserveWrapperCallback);
        if (!cx->runtime()->preserveWrapperCallback(cx, obj)) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_WEAKMAP_KEY);
            return false;
        }
    }
    return true;
}

static inline void
WeakMapPostWriteBarrier(JSRuntime* rt, ObjectValueMap* weakMap, JSObject* key)
{
    if (key && IsInsideNursery(key))
        rt->gc.storeBuffer.putGeneric(gc::HashKeyRef<ObjectValueMap, JSObject*>(weakMap, key));
}

static MOZ_ALWAYS_INLINE bool
SetWeakMapEntryInternal(JSContext* cx, Handle<WeakMapObject*> mapObj,
                        HandleObject key, HandleValue value)
{
    ObjectValueMap* map = mapObj->getMap();
    if (!map) {
        AutoInitGCManagedObject<ObjectValueMap> newMap(
            cx->make_unique<ObjectValueMap>(cx, mapObj.get()));
        if (!newMap)
            return false;
        if (!newMap->init()) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        map = newMap.release();
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

    MOZ_ASSERT(key->compartment() == mapObj->compartment());
    MOZ_ASSERT_IF(value.isObject(), value.toObject().compartment() == mapObj->compartment());
    if (!map->put(key, value)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    WeakMapPostWriteBarrier(cx->runtime(), map, key.get());
    return true;
}

MOZ_ALWAYS_INLINE bool
WeakMap_set_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    if (!args.get(0).isObject()) {
        UniquePtr<char[], JS::FreePolicy> bytes =
            DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, args.get(0), nullptr);
        if (!bytes)
            return false;
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT, bytes.get());
        return false;
    }

    RootedObject key(cx, &args[0].toObject());
    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
    Rooted<WeakMapObject*> map(cx, &thisObj->as<WeakMapObject>());

    if (!SetWeakMapEntryInternal(cx, map, key, args.get(1)))
        return false;
    args.rval().set(args.thisv());
    return true;
}

bool
js::WeakMap_set(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsWeakMap, WeakMap_set_impl>(cx, args);
}

JS_FRIEND_API(bool)
JS_NondeterministicGetWeakMapKeys(JSContext* cx, HandleObject objArg, MutableHandleObject ret)
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
    ObjectValueMap* map = obj->as<WeakMapObject>().getMap();
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
WeakMap_mark(JSTracer* trc, JSObject* obj)
{
    if (ObjectValueMap* map = obj->as<WeakMapObject>().getMap())
        map->trace(trc);
}

static void
WeakMap_finalize(FreeOp* fop, JSObject* obj)
{
    if (ObjectValueMap* map = obj->as<WeakMapObject>().getMap()) {
#ifdef DEBUG
        map->~ObjectValueMap();
        memset(static_cast<void*>(map), 0xdc, sizeof(*map));
        fop->free_(map);
#else
        fop->delete_(map);
#endif
    }
}

JS_PUBLIC_API(JSObject*)
JS::NewWeakMapObject(JSContext* cx)
{
    return NewBuiltinClassInstance(cx, &WeakMapObject::class_);
}

JS_PUBLIC_API(bool)
JS::IsWeakMapObject(JSObject* obj)
{
    return obj->is<WeakMapObject>();
}

JS_PUBLIC_API(bool)
JS::GetWeakMapEntry(JSContext* cx, HandleObject mapObj, HandleObject key,
                    MutableHandleValue rval)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, key);
    rval.setUndefined();
    ObjectValueMap* map = mapObj->as<WeakMapObject>().getMap();
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
JS::SetWeakMapEntry(JSContext* cx, HandleObject mapObj, HandleObject key,
                    HandleValue val)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, key, val);
    Rooted<WeakMapObject*> rootedMap(cx, &mapObj->as<WeakMapObject>());
    return SetWeakMapEntryInternal(cx, rootedMap, key, val);
}

static bool
WeakMap_construct(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // ES6 draft rev 31 (15 Jan 2015) 23.3.1.1 step 1.
    if (!ThrowIfNotConstructing(cx, args, "WeakMap"))
        return false;

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &WeakMapObject::class_));
    if (!obj)
        return false;

    // Steps 5-6, 11.
    if (!args.get(0).isNullOrUndefined()) {
        // Steps 7a-b.
        RootedValue adderVal(cx);
        if (!GetProperty(cx, obj, obj, cx->names().set, &adderVal))
            return false;

        // Step 7c.
        if (!IsCallable(adderVal))
            return ReportIsNotFunction(cx, adderVal);

        bool isOriginalAdder = IsNativeFunction(adderVal, WeakMap_set);
        RootedValue mapVal(cx, ObjectValue(*obj));
        FastInvokeGuard fig(cx, adderVal);
        InvokeArgs& args2 = fig.args();

        // Steps 7d-e.
        JS::ForOfIterator iter(cx);
        if (!iter.init(args[0]))
            return false;

        RootedValue pairVal(cx);
        RootedObject pairObject(cx);
        RootedValue keyVal(cx);
        RootedObject keyObject(cx);
        RootedValue val(cx);
        while (true) {
            // Steps 12a-e.
            bool done;
            if (!iter.next(&pairVal, &done))
                return false;
            if (done)
                break;

            // Step 12f.
            if (!pairVal.isObject()) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                                     JSMSG_INVALID_MAP_ITERABLE, "WeakMap");
                return false;
            }

            pairObject = &pairVal.toObject();
            if (!pairObject)
                return false;

            // Steps 12g-h.
            if (!GetElement(cx, pairObject, pairObject, 0, &keyVal))
                return false;

            // Steps 12i-j.
            if (!GetElement(cx, pairObject, pairObject, 1, &val))
                return false;

            // Steps 12k-l.
            if (isOriginalAdder) {
                if (keyVal.isPrimitive()) {
                    UniquePtr<char[], JS::FreePolicy> bytes =
                        DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, keyVal, nullptr);
                    if (!bytes)
                        return false;
                    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT, bytes.get());
                    return false;
                }

                keyObject = &keyVal.toObject();
                if (!SetWeakMapEntry(cx, obj, keyObject, val))
                    return false;
            } else {
                if (!args2.init(2))
                    return false;

                args2.setCallee(adderVal);
                args2.setThis(mapVal);
                args2[0].set(keyVal);
                args2[1].set(val);

                if (!fig.invoke(cx))
                    return false;
            }
        }
    }

    args.rval().setObject(*obj);
    return true;
}

const Class WeakMapObject::class_ = {
    "WeakMap",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakMap),
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    nullptr, /* convert */
    WeakMap_finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WeakMap_mark
};

static const JSFunctionSpec weak_map_methods[] = {
    JS_FN("has",    WeakMap_has, 1, 0),
    JS_FN("get",    WeakMap_get, 1, 0),
    JS_FN("delete", WeakMap_delete, 1, 0),
    JS_FN("set",    WeakMap_set, 2, 0),
    JS_FN("clear",  WeakMap_clear, 0, 0),
    JS_FS_END
};

static JSObject*
InitWeakMapClass(JSContext* cx, HandleObject obj, bool defineMembers)
{
    MOZ_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedPlainObject proto(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, global->createConstructor(cx, WeakMap_construct,
                                                      cx->names().WeakMap, 0));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (defineMembers) {
        if (!DefinePropertiesAndFunctions(cx, proto, nullptr, weak_map_methods))
            return nullptr;
    }

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_WeakMap, ctor, proto))
        return nullptr;
    return proto;
}

JSObject*
js::InitWeakMapClass(JSContext* cx, HandleObject obj)
{
    return InitWeakMapClass(cx, obj, true);
}

JSObject*
js::InitBareWeakMapCtor(JSContext* cx, HandleObject obj)
{
    return InitWeakMapClass(cx, obj, false);
}

ObjectWeakMap::ObjectWeakMap(JSContext* cx)
  : map(cx, nullptr)
{}

bool
ObjectWeakMap::init()
{
    return map.init();
}

ObjectWeakMap::~ObjectWeakMap()
{
    WeakMapBase::removeWeakMapFromList(&map);
}

JSObject*
ObjectWeakMap::lookup(const JSObject* obj)
{
    MOZ_ASSERT(map.initialized());
    if (ObjectValueMap::Ptr p = map.lookup(const_cast<JSObject*>(obj)))
        return &p->value().toObject();
    return nullptr;
}

bool
ObjectWeakMap::add(JSContext* cx, JSObject* obj, JSObject* target)
{
    MOZ_ASSERT(obj && target);
    MOZ_ASSERT(map.initialized());

    MOZ_ASSERT(!map.has(obj));
    if (!map.put(obj, ObjectValue(*target))) {
        ReportOutOfMemory(cx);
        return false;
    }
    if (IsInsideNursery(obj))
        cx->runtime()->gc.storeBuffer.putGeneric(StoreBufferRef(&map, obj));

    return true;
}

void
ObjectWeakMap::clear()
{
    MOZ_ASSERT(map.initialized());
    map.clear();
}

void
ObjectWeakMap::trace(JSTracer* trc)
{
    MOZ_ASSERT(map.initialized());
    map.trace(trc);
}

size_t
ObjectWeakMap::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    MOZ_ASSERT(map.initialized());
    return map.sizeOfExcludingThis(mallocSizeOf);
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
ObjectWeakMap::checkAfterMovingGC()
{
    MOZ_ASSERT(map.initialized());
    for (ObjectValueMap::Range r = map.all(); !r.empty(); r.popFront()) {
        CheckGCThingAfterMovingGC(r.front().key().get());
        CheckGCThingAfterMovingGC(&r.front().value().toObject());
    }
}
#endif // JSGC_HASH_TABLE_CHECKS
