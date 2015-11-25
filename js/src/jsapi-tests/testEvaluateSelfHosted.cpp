/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TestingFunctions.h"

#include "jsapi-tests/tests.h"

static bool contentGlobalFunctionCalled = false;

static bool
ContentGlobalFunction(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    contentGlobalFunctionCalled = true;
    args.rval().setUndefined();
    return true;
}

BEGIN_TEST(testEvaluateSelfHosted)
{
    CHECK(js::DefineTestingFunctions(cx, global, false, false));
    CHECK(JS_DefineFunction(cx, global, "contentGlobalFunction", ContentGlobalFunction, 0, 0));

    JS::RootedValue v(cx);
    EVAL("getSelfHostedValue('customSelfHostedFunction')();", &v);
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(contentGlobalFunctionCalled == true);

    return true;
}

virtual JSContext* createContext() override {
    JSContext* cx = JS_NewContext(rt, 8192);
    if (!cx)
        return nullptr;
    const char *selfHostedCode = "function customSelfHostedFunction()"
                                 "{callFunction(global.contentGlobalFunction, global);}";
    if (!JS::EvaluateSelfHosted(rt, selfHostedCode, strlen(selfHostedCode), "testing-sh"))
    {
        return nullptr;
    }
    return cx;
}
END_TEST(testEvaluateSelfHosted)
