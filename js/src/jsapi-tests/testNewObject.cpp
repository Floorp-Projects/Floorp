/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool
constructHook(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    // Check that arguments were passed properly from JS_New.

    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
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
    JS::RootedObject callee(cx, &args.callee());
    if (!JS_SetElement(cx, callee, 0, value))
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
    static const size_t N = 1000;
    JS::AutoValueVector argv(cx);
    CHECK(argv.resize(N));

    JS::RootedValue v(cx);
    EVAL("Array", &v);
    JS::RootedObject Array(cx, v.toObjectOrNull());

    // With no arguments.
    JS::RootedObject obj(cx, JS_New(cx, Array, JS::HandleValueArray::empty()));
    CHECK(obj);
    JS::RootedValue rt(cx, JS::ObjectValue(*obj));
    CHECK(JS_IsArrayObject(cx, obj));
    uint32_t len;
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK_EQUAL(len, 0u);

    // With one argument.
    argv[0].setInt32(4);
    obj = JS_New(cx, Array, JS::HandleValueArray::subarray(argv, 0, 1));
    CHECK(obj);
    rt = OBJECT_TO_JSVAL(obj);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK_EQUAL(len, 4u);

    // With N arguments.
    for (size_t i = 0; i < N; i++)
        argv[i].setInt32(i);
    obj = JS_New(cx, Array, JS::HandleValueArray::subarray(argv, 0, N));
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
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, constructHook
    };
    JS::RootedObject ctor(cx, JS_NewObject(cx, &cls));
    CHECK(ctor);
    JS::RootedValue rt2(cx, OBJECT_TO_JSVAL(ctor));
    obj = JS_New(cx, ctor, JS::HandleValueArray::subarray(argv, 0, 3));
    CHECK(obj);
    CHECK(JS_GetElement(cx, ctor, 0, &v));
    CHECK_SAME(v, JSVAL_ZERO);

    return true;
}
END_TEST(testNewObject_1)
