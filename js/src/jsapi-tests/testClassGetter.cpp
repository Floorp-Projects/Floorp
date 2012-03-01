/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Tests the JSClass::getProperty hook
 */

#include "tests.h"

int called_test_fn;
int called_test_prop_get;

static JSBool test_prop_get( JSContext *cx, JSObject *obj, jsid id, jsval *vp )
{
    called_test_prop_get++;
    return JS_TRUE;
}

static JSBool
PTest(JSContext* cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_NewObjectForConstructor(cx, vp);
    if (!obj)
        return JS_FALSE;
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
    return JS_TRUE;
}

static JSClass ptestClass = {
    "PTest",
    JSCLASS_HAS_PRIVATE,

    JS_PropertyStub,       // add
    JS_PropertyStub,       // delete
    test_prop_get,         // get
    JS_StrictPropertyStub, // set
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool test_fn(JSContext *cx, unsigned argc, jsval *vp)
{
    called_test_fn++;
    return JS_TRUE;
}

static JSFunctionSpec ptestFunctions[] = {
    JS_FS( "test_fn", test_fn, 0, 0 ),
    JS_FS_END
};

BEGIN_TEST(testClassGetter_isCalled)
{
    CHECK(JS_InitClass(cx, JS_GetGlobalObject(cx), NULL, &ptestClass, PTest, 0,
                       NULL, ptestFunctions, NULL, NULL));

    EXEC("function check() { var o = new PTest(); o.test_fn(); o.test_value1; o.test_value2; o.test_value1; }");

    for (int i = 1; i < 9; i++) {
        jsvalRoot rval(cx);
        CHECK(JS_CallFunctionName(cx, global, "check", 0, NULL, rval.addr()));
        CHECK_SAME(INT_TO_JSVAL(called_test_fn), INT_TO_JSVAL(i));
        CHECK_SAME(INT_TO_JSVAL(called_test_prop_get), INT_TO_JSVAL(4 * i));
    }
    return true;
}
END_TEST(testClassGetter_isCalled)
