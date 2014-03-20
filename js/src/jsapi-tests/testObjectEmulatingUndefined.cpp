/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static const JSClass ObjectEmulatingUndefinedClass = {
    "ObjectEmulatingUndefined",
    JSCLASS_EMULATES_UNDEFINED,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static bool
ObjectEmulatingUndefinedConstructor(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject *obj = JS_NewObjectForConstructor(cx, &ObjectEmulatingUndefinedClass, args);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

BEGIN_TEST(testObjectEmulatingUndefined_truthy)
{
    CHECK(JS_InitClass(cx, global, js::NullPtr(), &ObjectEmulatingUndefinedClass,
                       ObjectEmulatingUndefinedConstructor, 0,
                       nullptr, nullptr, nullptr, nullptr));

    JS::RootedValue result(cx);

    EVAL("if (new ObjectEmulatingUndefined()) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_FALSE);

    EVAL("if (!new ObjectEmulatingUndefined()) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("var obj = new ObjectEmulatingUndefined(); \n"
         "var res = []; \n"
         "for (var i = 0; i < 50; i++) \n"
         "  res.push(Boolean(obj)); \n"
         "res.every(function(v) { return v === false; });",
         result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    return true;
}
END_TEST(testObjectEmulatingUndefined_truthy)

BEGIN_TEST(testObjectEmulatingUndefined_equal)
{
    CHECK(JS_InitClass(cx, global, js::NullPtr(), &ObjectEmulatingUndefinedClass,
                       ObjectEmulatingUndefinedConstructor, 0,
                       nullptr, nullptr, nullptr, nullptr));

    JS::RootedValue result(cx);

    EVAL("if (new ObjectEmulatingUndefined() == undefined) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("if (new ObjectEmulatingUndefined() == null) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("if (new ObjectEmulatingUndefined() != undefined) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_FALSE);

    EVAL("if (new ObjectEmulatingUndefined() != null) true; else false;", result.address());
    CHECK_SAME(result, JSVAL_FALSE);

    EVAL("var obj = new ObjectEmulatingUndefined(); \n"
         "var res = []; \n"
         "for (var i = 0; i < 50; i++) \n"
         "  res.push(obj == undefined); \n"
         "res.every(function(v) { return v === true; });",
         result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("var obj = new ObjectEmulatingUndefined(); \n"
         "var res = []; \n"
         "for (var i = 0; i < 50; i++) \n"
         "  res.push(obj == null); \n"
         "res.every(function(v) { return v === true; });",
         result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("var obj = new ObjectEmulatingUndefined(); \n"
         "var res = []; \n"
         "for (var i = 0; i < 50; i++) \n"
         "  res.push(obj != undefined); \n"
         "res.every(function(v) { return v === false; });",
         result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    EVAL("var obj = new ObjectEmulatingUndefined(); \n"
         "var res = []; \n"
         "for (var i = 0; i < 50; i++) \n"
         "  res.push(obj != null); \n"
         "res.every(function(v) { return v === false; });",
         result.address());
    CHECK_SAME(result, JSVAL_TRUE);

    return true;
}
END_TEST(testObjectEmulatingUndefined_equal)
