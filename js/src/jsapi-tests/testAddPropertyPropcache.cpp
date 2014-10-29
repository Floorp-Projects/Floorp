/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static int callCount = 0;

static bool
AddProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::MutableHandleValue vp)
{
    callCount++;
    return true;
}

static const JSClass AddPropertyClass = {
    "AddPropertyTester",
    0,
    AddProperty,
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

BEGIN_TEST(testAddPropertyHook)
{
    /*
     * Do the test a bunch of times, because sometimes we seem to randomly
     * miss the propcache.
     */
    static const int ExpectedCount = 100;

    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    CHECK(obj);
    JS::RootedValue proto(cx, OBJECT_TO_JSVAL(obj));
    JS_InitClass(cx, global, obj, &AddPropertyClass, nullptr, 0, nullptr, nullptr, nullptr,
                 nullptr);

    obj = JS_NewArrayObject(cx, 0);
    CHECK(obj);
    JS::RootedValue arr(cx, OBJECT_TO_JSVAL(obj));

    CHECK(JS_DefineProperty(cx, global, "arr", arr,
                            JSPROP_ENUMERATE,
                            JS_STUBGETTER, JS_STUBSETTER));

    JS::RootedObject arrObj(cx, &arr.toObject());
    for (int i = 0; i < ExpectedCount; ++i) {
        obj = JS_NewObject(cx, &AddPropertyClass, JS::NullPtr(), JS::NullPtr());
        CHECK(obj);
        CHECK(JS_DefineElement(cx, arrObj, i, obj,
                               JSPROP_ENUMERATE,
                               JS_STUBGETTER, JS_STUBSETTER));
    }

    // Now add a prop to each of the objects, but make sure to do
    // so at the same bytecode location so we can hit the propcache.
    EXEC("'use strict';                                     \n"
         "for (var i = 0; i < arr.length; ++i)              \n"
         "  arr[i].prop = 42;                               \n"
         );

    CHECK(callCount == ExpectedCount);

    return true;
}
END_TEST(testAddPropertyHook)

