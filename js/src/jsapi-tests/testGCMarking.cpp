/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

class CCWTestTracer : public JS::CallbackTracer {
    static void staticCallback(JS::CallbackTracer* trc, void** thingp, JS::TraceKind kind) {
        static_cast<CCWTestTracer*>(trc)->callback(thingp, kind);
    }

    void callback(void** thingp, JS::TraceKind kind) {
        numberOfThingsTraced++;

        printf("*thingp         = %p\n", *thingp);
        printf("*expectedThingp = %p\n", *expectedThingp);

        printf("kind         = %d\n", static_cast<int>(kind));
        printf("expectedKind = %d\n", static_cast<int>(expectedKind));

        if (*thingp != *expectedThingp || kind != expectedKind)
            okay = false;
    }

  public:
    bool          okay;
    size_t        numberOfThingsTraced;
    void**        expectedThingp;
    JS::TraceKind expectedKind;

    CCWTestTracer(JSContext* cx, void** expectedThingp, JS::TraceKind expectedKind)
      : JS::CallbackTracer(JS_GetRuntime(cx), staticCallback),
        okay(true),
        numberOfThingsTraced(0),
        expectedThingp(expectedThingp),
        expectedKind(expectedKind)
    { }
};

BEGIN_TEST(testTracingIncomingCCWs)
{
    // Get two globals, in two different zones.

    JS::RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    JS::RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                    JS::FireOnNewGlobalHook));
    CHECK(global2);
    CHECK(global1->zone() != global2->zone());

    // Define an object in one zone, that is wrapped by a CCW in another zone.

    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    CHECK(obj->zone() == global1->zone());

    JSAutoCompartment ac(cx, global2);
    JS::RootedObject wrapper(cx, obj);
    CHECK(JS_WrapObject(cx, &wrapper));
    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    CHECK(JS_SetProperty(cx, global2, "ccw", v));

    // Ensure that |JS_TraceIncomingCCWs| finds the object wrapped by the CCW.

    JS::ZoneSet zones;
    CHECK(zones.init());
    CHECK(zones.put(global1->zone()));

    void* thing = obj.get();
    CCWTestTracer trc(cx, &thing, JS::TraceKind::Object);
    JS_TraceIncomingCCWs(&trc, zones);
    CHECK(trc.numberOfThingsTraced == 1);
    CHECK(trc.okay);

    return true;
}
END_TEST(testTracingIncomingCCWs)
