/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

const size_t N = 1000;
static jsval argv[N];

static JSBool
constructHook(JSContext *cx, JSObject *thisobj, uintN argc, jsval *argv, jsval *rval)
{
    // Check that arguments were passed properly from JS_New.
    JSObject *callee = JSVAL_TO_OBJECT(JS_ARGV_CALLEE(argv));
    if (!thisobj) {
        JS_ReportError(cx, "test failed, null 'this'");
        return false;
    }
    if (strcmp(JS_GET_CLASS(cx, thisobj)->name, "Object") != 0) {
        JS_ReportError(cx, "test failed, wrong class for 'this'");
        return false;
    }
    if (argc != 3) {
        JS_ReportError(cx, "test failed, argc == %d", argc);
        return false;
    }
    if (!JSVAL_IS_INT(argv[2]) || JSVAL_TO_INT(argv[2]) != 2) {
        JS_ReportError(cx, "test failed, wrong value in argv[2]");
        return false;
    }
    if (!JS_IsConstructing(cx)) {
        JS_ReportError(cx, "test failed, not constructing");
        return false;
    }

    // Perform a side-effect to indicate that this hook was actually called.
    if (!JS_SetElement(cx, callee, 0, &argv[0]))
        return false;

    *rval = OBJECT_TO_JSVAL(callee); // return the callee, perversely
    argv[0] = argv[1] = argv[2] = JSVAL_VOID;  // trash the argv, perversely
    return true;
}

BEGIN_TEST(testNewObject_1)
{
    jsval v;
    EVAL("Array", &v);
    JSObject *Array = JSVAL_TO_OBJECT(v);

    // With no arguments.
    JSObject *obj = JS_New(cx, Array, 0, NULL);
    CHECK(obj);
    jsvalRoot rt(cx, OBJECT_TO_JSVAL(obj));
    CHECK(JS_IsArrayObject(cx, obj));
    jsuint len;
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK(len == 0);

    // With one argument.
    argv[0] = INT_TO_JSVAL(4);
    obj = JS_New(cx, Array, 1, argv);
    CHECK(obj);
    rt = OBJECT_TO_JSVAL(obj);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK(len == 4);

    // With N arguments.
    JS_ASSERT(INT_FITS_IN_JSVAL(N));
    for (size_t i = 0; i < N; i++)
        argv[i] = INT_TO_JSVAL(i);
    obj = JS_New(cx, Array, N, argv);
    CHECK(obj);
    rt = OBJECT_TO_JSVAL(obj);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetArrayLength(cx, obj, &len));
    CHECK(len == N);
    CHECK(JS_GetElement(cx, obj, N - 1, &v));
    CHECK_SAME(v, INT_TO_JSVAL(N - 1));

    // With JSClass.construct.
    static JSClass cls = {
        "testNewObject_1",
        0,
        JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
        NULL, NULL, NULL, constructHook, NULL, NULL, NULL, NULL
    };
    JSObject *ctor = JS_NewObject(cx, &cls, NULL, NULL);
    CHECK(ctor);
    jsvalRoot rt2(cx, OBJECT_TO_JSVAL(ctor));
    obj = JS_New(cx, ctor, 3, argv);
    CHECK(obj);
    CHECK(obj == ctor);  // constructHook returns ctor, perversely
    CHECK(JS_GetElement(cx, ctor, 0, &v));
    CHECK_SAME(v, JSVAL_ZERO);
    CHECK_SAME(argv[0], JSVAL_ZERO);  // original argv should not have been trashed
    CHECK_SAME(argv[1], JSVAL_ONE);
    return true;
}
END_TEST(testNewObject_1)
