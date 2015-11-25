/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TestingFunctions.h"

#include "jsapi-tests/tests.h"

static int32_t contentGlobalFunctionCallResult = 0;

static bool
IntrinsicFunction(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    args.rval().setInt32(42);
    return true;
}

static bool
ContentGlobalFunction(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    contentGlobalFunctionCallResult = args[0].toInt32();
    args.rval().setUndefined();
    return true;
}

static const JSFunctionSpec intrinsic_functions[] = {
    JS_FN("intrinsicFunction", IntrinsicFunction, 0,0),
    JS_FS_END
};

BEGIN_TEST(testEvaluateSelfHosted)
{
    CHECK(js::DefineTestingFunctions(cx, global, false, false));
    CHECK(JS_DefineFunction(cx, global, "contentGlobalFunction", ContentGlobalFunction, 0, 0));

    JS::RootedValue v(cx);
    EVAL("getSelfHostedValue('customSelfHostedFunction')();", &v);
    CHECK(!JS_IsExceptionPending(cx));
    CHECK(contentGlobalFunctionCallResult == 42);

    return true;
}

virtual JSContext* createContext() override {
    JSContext* cx = JS_NewContext(rt, 8192);
    if (!cx)
        return nullptr;
    const char *selfHostedCode = "function customSelfHostedFunction() {"
                                 "let intrinsicResult = intrinsicFunction();"
                                 "callFunction(global.contentGlobalFunction, global,"
                                 "             intrinsicResult);"
                                 "}";
    if (!JS::AddSelfHostingIntrinsics(rt, intrinsic_functions) ||
        !JS::EvaluateSelfHosted(rt, selfHostedCode, strlen(selfHostedCode), "testing-sh"))
    {
        return nullptr;
    }
    return cx;
}
END_TEST(testEvaluateSelfHosted)
