/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool
InterruptCallback(JSContext* cx)
{
    return false;
}

static unsigned sRemain;

static bool
RequestInterruptCallback(JSContext* cx, unsigned argc, jsval* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (!sRemain--)
        JS_RequestInterruptCallback(cx);
    args.rval().setUndefined();
    return true;
}

BEGIN_TEST(testSlowScript)
{
    JS_AddInterruptCallback(cx, InterruptCallback);
    JS_DefineFunction(cx, global, "requestInterruptCallback", RequestInterruptCallback, 0, 0);

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    requestInterruptCallback();");

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    for (j in [0,0,0,0])"
         "      requestInterruptCallback();");

    test("while (true)"
         "  for (i in [0,0,0,0])"
         "    for (j in [0,0,0,0])"
         "      for (k in [0,0,0,0])"
         "        requestInterruptCallback();");

    test("function f() { while (true) yield requestInterruptCallback() }"
         "for (i in f()) ;");

    test("function f() { while (true) yield 1 }"
         "for (i in f())"
         "  requestInterruptCallback();");

    test("(function() {"
         "  while (true)"
         "    let (x = 1) { eval('requestInterruptCallback()'); }"
         "})()");

    return true;
}

bool
test(const char* bytes)
{
    jsval v;

    JS_SetOptions(cx, JS_GetOptions(cx) & ~(JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS));
    sRemain = 0;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    sRemain = 1000;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_METHODJIT | JSOPTION_METHODJIT_ALWAYS);

    sRemain = 0;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    sRemain = 1000;
    CHECK(!evaluate(bytes, __FILE__, __LINE__, &v));
    CHECK(!JS_IsExceptionPending(cx));

    return true;
}
END_TEST(testSlowScript)
