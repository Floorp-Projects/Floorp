/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

const size_t N = 1000;
static jsval argv[N];

static bool
constructHook(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    // Check that arguments were passed properly from JS_New.

    JS::RootedObject obj(cx, JS_NewObject(cx, js::Jsvalify(&JSObject::class_), NULL, NULL));
    if (!obj) {
        JS_ReportError(cx, "test failed, could not construct object");
        return false;
    }
    if (strcmp(JS_GetClass(obj)->name, "Object") != 0) {
        JS_ReportError(cx, "test failed, wrong class for 'this'");
        return false;
    }
    if (args.length() != 3) {
        JS_ReportError(cx, "test failed, argc == %d", args.length());
        return false;
    }
    if (!args[0].isInt32() || args[2].toInt32() != 2) {
        JS_ReportError(cx, "test failed, wrong value in args[2]");
        return false;
    }
    if (!args.isConstructing()) {
        JS_ReportError(cx, "test failed, not constructing");
        return false;
    }

    // Perform a side-effect to indicate that this hook was actually called.
    JS::RootedValue value(cx, args[0]);
    if (!JS_SetElement(cx, &args.callee(), 0, &value))
        return false;

    args.rval().setObject(*obj);

    // trash the argv, perversely
    args[0].setUndefined();
    args[1].setUndefined();
    args[2].setUndefined();

    return true;
}

BEGIN_TEST(testNewObject_1)
{
    // Root the global argv test array. Only the first 2 entries really need to
    // be rooted, since we're only putting integers in the rest.
    CHECK(JS_AddNamedValueRoot(cx, &argv[0], "argv0"));
    CHECK(JS_AddNamedValueRoot(cx, &argv[1], "argv1"));

    JS::RootedValue v(cx);
    EVAL("Array", v.address());
    JS::RootedObject Array(cx, JSVAL_TO_OBJECT(v));

    // With no arguments.
    JS::RootedObject obj(cx, JS_New(cx, Array, 0, NULL));
    CHECK(obj);
    JS::RootedValue rt(cx, OBJECT_TO_JSVAL(obj));
    CHECK(JS_IsArrayObject(cx, obj));
    uint32_t len;
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK_EQUAL(len, 0);

    // With one argument.
    argv[0] = INT_TO_JSVAL(4);
    obj = JS_New(cx, Array, 1, argv);
    CHECK(obj);
    rt = OBJECT_TO_JSVAL(obj);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK_EQUAL(len, 4);

    // With N arguments.
    for (size_t i = 0; i < N; i++)
        argv[i] = INT_TO_JSVAL(i);
    obj = JS_New(cx, Array, N, argv);
    CHECK(obj);
    rt = OBJECT_TO_JSVAL(obj);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK_EQUAL(len, N);
    CHECK(JS_GetElement(cx, obj, N - 1, &v));
    CHECK_SAME(v, INT_TO_JSVAL(N - 1));

    // With JSClass.construct.
    static const JSClass cls = {
        "testNewObject_1",
        0,
        JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
        JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
        NULL, NULL, NULL, constructHook
    };
    JS::RootedObject ctor(cx, JS_NewObject(cx, &cls, NULL, NULL));
    CHECK(ctor);
    JS::RootedValue rt2(cx, OBJECT_TO_JSVAL(ctor));
    obj = JS_New(cx, ctor, 3, argv);
    CHECK(obj);
    CHECK(JS_GetElement(cx, ctor, 0, &v));
    CHECK_SAME(v, JSVAL_ZERO);

    JS_RemoveValueRoot(cx, &argv[0]);
    JS_RemoveValueRoot(cx, &argv[1]);

    return true;
}
END_TEST(testNewObject_1)
