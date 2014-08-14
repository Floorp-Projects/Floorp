/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone.h"

#include "jsapi-tests/tests.h"

#ifdef JSGC_USE_EXACT_ROOTING

BEGIN_TEST(testWeakMap_basicOperations)
{
    JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
    CHECK(IsWeakMapObject(map));

    JS::RootedObject key(cx, newKey());
    CHECK(key);
    CHECK(!IsWeakMapObject(key));

    JS::RootedValue r(cx);
    CHECK(GetWeakMapEntry(cx, map, key, &r));
    CHECK(r.isUndefined());

    CHECK(checkSize(map, 0));

    JS::RootedValue val(cx, JS::Int32Value(1));
    CHECK(SetWeakMapEntry(cx, map, key, val));

    CHECK(GetWeakMapEntry(cx, map, key, &r));
    CHECK(r == val);
    CHECK(checkSize(map, 1));

    JS_GC(rt);

    CHECK(GetWeakMapEntry(cx, map, key, &r));
    CHECK(r == val);
    CHECK(checkSize(map, 1));

    key = nullptr;
    JS_GC(rt);

    CHECK(checkSize(map, 0));

    return true;
}

JSObject *newKey()
{
    return JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
}

bool
checkSize(JS::HandleObject map, uint32_t expected)
{
    JS::RootedObject keys(cx);
    CHECK(JS_NondeterministicGetWeakMapKeys(cx, map, &keys));

    uint32_t length;
    CHECK(JS_GetArrayLength(cx, keys, &length));
    CHECK(length == expected);

    return true;
}
END_TEST(testWeakMap_basicOperations)

BEGIN_TEST(testWeakMap_keyDelegates)
{
    JS_SetGCParameter(rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_GC(rt);

    JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
    CHECK(map);

    JS::RootedObject key(cx, newKey());
    CHECK(key);

    JS::RootedObject delegate(cx, newDelegate());
    CHECK(delegate);

    SetKeyDelegate(key, delegate);

    /*
     * Perform an incremental GC, introducing an unmarked CCW to force the map
     * zone to finish marking before the delegate zone.
     */
    CHECK(newCCW(map, delegate));
    rt->gc.gcDebugSlice(true, 1000000);
#ifdef DEBUG
    CHECK(map->zone()->lastZoneGroupIndex() < delegate->zone()->lastZoneGroupIndex());
#endif

    /* Add our entry to the weakmap. */
    JS::RootedValue val(cx, JS::Int32Value(1));
    CHECK(SetWeakMapEntry(cx, map, key, val));
    CHECK(checkSize(map, 1));

    /* Check the delegate keeps the entry alive even if the key is not reachable. */
    key = nullptr;
    CHECK(newCCW(map, delegate));
    rt->gc.gcDebugSlice(true, 100000);
    CHECK(checkSize(map, 1));

    /*
     * Check that the zones finished marking at the same time, which is
     * neccessary because of the presence of the delegate and the CCW.
     */
#ifdef DEBUG
    CHECK(map->zone()->lastZoneGroupIndex() == delegate->zone()->lastZoneGroupIndex());
#endif

    /* Check that when the delegate becomes unreachable the entry is removed. */
    delegate = nullptr;
    JS_GC(rt);
    CHECK(checkSize(map, 0));

    return true;
}

static void SetKeyDelegate(JSObject *key, JSObject *delegate)
{
    JS_SetPrivate(key, delegate);
}

static JSObject *GetKeyDelegate(JSObject *obj)
{
    return static_cast<JSObject*>(JS_GetPrivate(obj));
}

JSObject *newKey()
{
    static const js::Class keyClass = {
        "keyWithDelgate",
        JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
        JS_PropertyStub,         /* addProperty */
        JS_DeletePropertyStub,   /* delProperty */
        JS_PropertyStub,         /* getProperty */
        JS_StrictPropertyStub,   /* setProperty */
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        JS_NULL_CLASS_SPEC,
        {
            nullptr,
            nullptr,
            nullptr,
            false,
            GetKeyDelegate
        },
        JS_NULL_OBJECT_OPS
    };

    JS::RootedObject key(cx);
    key = JS_NewObject(cx,
                       reinterpret_cast<const JSClass *>(&keyClass),
                       JS::NullPtr(),
                       JS::NullPtr());
    if (!key)
        return nullptr;

    SetKeyDelegate(key, nullptr);

    return key;
}

JSObject *newCCW(JS::HandleObject sourceZone, JS::HandleObject destZone)
{
    /*
     * Now ensure that this zone will be swept first by adding a cross
     * compartment wrapper to a new objct in the same zone as the
     * delegate obejct.
     */
    JS::RootedObject object(cx);
    {
        JSAutoCompartment ac(cx, destZone);
        object = JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr());
        if (!object)
            return nullptr;
    }
    {
        JSAutoCompartment ac(cx, sourceZone);
        if (!JS_WrapObject(cx, &object))
            return nullptr;
    }
    return object;
}

JSObject *newDelegate()
{
    static const JSClass delegateClass = {
        "delegate",
        JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_RESERVED_SLOTS(1),
        JS_PropertyStub,
        JS_DeletePropertyStub,
        JS_PropertyStub,
        JS_StrictPropertyStub,
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        JS_GlobalObjectTraceHook
    };

    /* Create the global object. */
    JS::CompartmentOptions options;
    options.setVersion(JSVERSION_LATEST);
    JS::RootedObject global(cx);
    global = JS_NewGlobalObject(cx, &delegateClass, nullptr, JS::FireOnNewGlobalHook, options);
    JS_SetReservedSlot(global, 0, JS::Int32Value(42));

    /*
     * Ensure the delegate is not in the nursery because for the purpose of this
     * test we're going to put it in a private slot where it won't get updated.
     */
    JS_GC(rt);

    return global;
}

bool
checkSize(JS::HandleObject map, uint32_t expected)
{
    JS::RootedObject keys(cx);
    CHECK(JS_NondeterministicGetWeakMapKeys(cx, map, &keys));

    uint32_t length;
    CHECK(JS_GetArrayLength(cx, keys, &length));
    CHECK(length == expected);

    return true;
}
END_TEST(testWeakMap_keyDelegates)

#endif // JSGC_USE_EXACT_ROOTING
