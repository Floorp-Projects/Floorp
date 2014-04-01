/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Class.h"
#include "jsapi-tests/tests.h"

static int iterCount = 0;

static bool
IterNext(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (iterCount++ == 100)
        return JS_ThrowStopIteration(cx);
    args.rval().setInt32(iterCount);
    return true;
}

static JSObject *
IterHook(JSContext *cx, JS::HandleObject obj, bool keysonly)
{
    JS::RootedObject iterObj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    if (!iterObj)
        return nullptr;
    if (!JS_DefineFunction(cx, iterObj, "next", IterNext, 0, 0))
        return nullptr;
    return iterObj;
}

const js::Class HasCustomIterClass = {
    "HasCustomIter",
    0,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr, /* mark */
    JS_NULL_CLASS_SPEC,
    {
        nullptr,     /* outerObject */
        nullptr,     /* innerObject */
        IterHook,
        false        /* isWrappedNative */
    }
};

static bool
IterClassConstructor(JSContext *cx, unsigned argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JSObject *obj = JS_NewObjectForConstructor(cx, Jsvalify(&HasCustomIterClass), args);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

BEGIN_TEST(testCustomIterator_bug612523)
{
    CHECK(JS_InitClass(cx, global, js::NullPtr(), Jsvalify(&HasCustomIterClass),
                       IterClassConstructor, 0, nullptr, nullptr, nullptr, nullptr));

    JS::RootedValue result(cx);
    EVAL("var o = new HasCustomIter(); \n"
         "var j = 0; \n"
         "for (var i in o) { ++j; }; \n"
         "j;", &result);

    CHECK(JSVAL_IS_INT(result));
    CHECK_EQUAL(JSVAL_TO_INT(result), 100);
    CHECK_EQUAL(iterCount, 101);

    return true;
}
END_TEST(testCustomIterator_bug612523)
