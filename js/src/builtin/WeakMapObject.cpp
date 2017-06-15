/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/WeakMapObject.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "vm/SelfHosting.h"

#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::gc;

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
    if (obj->getClass()->isWrappedNative() ||
        obj->getClass()->isDOMClass() ||
        (obj->is<ProxyObject>() &&
         obj->as<ProxyObject>().handler()->family() == GetDOMProxyHandlerFamily()))
    {
        MOZ_ASSERT(cx->runtime()->preserveWrapperCallback);
        if (!cx->runtime()->preserveWrapperCallback(cx, obj)) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_WEAKMAP_KEY);
            return false;
        }
    }
    return true;
}

static MOZ_ALWAYS_INLINE bool
SetWeakMapEntryInternal(JSContext* cx, Handle<WeakMapObject*> mapObj,
                        HandleObject key, HandleValue value)
{
    ObjectValueMap* map = mapObj->getMap();
    if (!map) {
        auto newMap = cx->make_unique<ObjectValueMap>(cx, mapObj.get());
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

    if (JSWeakmapKeyDelegateOp op = key->getClass()->extWeakmapKeyDelegateOp()) {
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
    return true;
}

MOZ_ALWAYS_INLINE bool
WeakMap_set_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsWeakMap(args.thisv()));

    if (!args.get(0).isObject()) {
        ReportNotObjectWithName(cx, "WeakMap key", args.get(0));
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
WeakMap_trace(JSTracer* trc, JSObject* obj)
{
    if (ObjectValueMap* map = obj->as<WeakMapObject>().getMap())
        map->trace(trc);
}

static void
WeakMap_finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->maybeOnHelperThread());
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

    RootedObject newTarget(cx, &args.newTarget().toObject());
    RootedObject obj(cx, CreateThis(cx, &WeakMapObject::class_, newTarget));
    if (!obj)
        return false;

    // Steps 5-6, 11.
    if (!args.get(0).isNullOrUndefined()) {
        FixedInvokeArgs<1> args2(cx);
        args2[0].set(args[0]);

        RootedValue thisv(cx, ObjectValue(*obj));
        if (!CallSelfHostedFunction(cx, cx->names().WeakMapConstructorInit, thisv, args2, args2.rval()))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

static const ClassOps WeakMapObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    WeakMap_finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    WeakMap_trace
};

const Class WeakMapObject::class_ = {
    "WeakMap",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_WeakMap) |
    JSCLASS_BACKGROUND_FINALIZE,
    &WeakMapObjectClassOps
};

static const JSFunctionSpec weak_map_methods[] = {
    JS_FN("has",    WeakMap_has, 1, 0),
    JS_FN("get",    WeakMap_get, 1, 0),
    JS_FN("delete", WeakMap_delete, 1, 0),
    JS_FN("set",    WeakMap_set, 2, 0),
    JS_FS_END
};

static JSObject*
InitWeakMapClass(JSContext* cx, HandleObject obj, bool defineMembers)
{
    MOZ_ASSERT(obj->isNative());

    Handle<GlobalObject*> global = obj.as<GlobalObject>();

    RootedPlainObject proto(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!proto)
        return nullptr;

    RootedFunction ctor(cx, GlobalObject::createConstructor(cx, WeakMap_construct,
                                                            cx->names().WeakMap, 0));
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (defineMembers) {
        if (!DefinePropertiesAndFunctions(cx, proto, nullptr, weak_map_methods))
            return nullptr;
        if (!DefineToStringTag(cx, proto, cx->names().WeakMap))
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

