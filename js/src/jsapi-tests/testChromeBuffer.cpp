/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static TestJSPrincipals system_principals(1);

static const JSClassOps global_classOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    JS_GlobalObjectTraceHook
};

static const JSClass global_class = {
    "global",
    JSCLASS_IS_GLOBAL | JSCLASS_GLOBAL_FLAGS,
    &global_classOps
};

static JS::PersistentRootedObject trusted_glob;
static JS::PersistentRootedObject trusted_fun;

static bool
CallTrusted(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    bool ok = false;
    {
        JSAutoCompartment ac(cx, trusted_glob);
        JS::RootedValue funVal(cx, JS::ObjectValue(*trusted_fun));
        ok = JS_CallFunctionValue(cx, nullptr, funVal, JS::HandleValueArray::empty(), args.rval());
    }
    return ok;
}

BEGIN_TEST(testChromeBuffer)
{
    JS_SetTrustedPrincipals(rt, &system_principals);

    JS::CompartmentOptions options;
    trusted_glob.init(cx, JS_NewGlobalObject(cx, &global_class, &system_principals,
                                             JS::FireOnNewGlobalHook, options));
    CHECK(trusted_glob);

    JS::RootedFunction fun(cx);

    /*
     * Check that, even after untrusted content has exhausted the stack, code
     * compiled with "trusted principals" can run using reserved trusted-only
     * buffer space.
     */
    {
        // Disable the JIT because if we don't this test fails.  See bug 1160414.
        JS::RuntimeOptions oldOptions = JS::RuntimeOptionsRef(rt);
        JS::RuntimeOptionsRef(rt).setIon(false).setBaseline(false);
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char* paramName = "x";
            const char* bytes = "return x ? 1 + trusted(x-1) : 0";
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0);
            JS::AutoObjectVector emptyScopeChain(cx);
            CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "trusted",
                                      1, &paramName, bytes, strlen(bytes), &fun));
            CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun, JSPROP_ENUMERATE));
            trusted_fun.init(cx, JS_GetFunctionObject(fun));
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, &v));

        const char* paramName = "trusted";
        const char* bytes = "try {                                      "
                            "    return untrusted(trusted);             "
                            "} catch (e) {                              "
                            "    try {                                  "
                            "        return trusted(100);               "
                            "    } catch(e) {                           "
                            "        return -1;                         "
                            "    }                                      "
                            "}                                          ";
        JS::CompileOptions options(cx);
        options.setFileAndLine("", 0);
        JS::AutoObjectVector emptyScopeChain(cx);
        CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                                  &paramName, bytes, strlen(bytes), &fun));
        CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(v), &rval));
        CHECK(rval.toInt32() == 100);
        JS::RuntimeOptionsRef(rt) = oldOptions;
    }

    /*
     * Check that content called from chrome in the reserved-buffer space
     * immediately ooms.
     */
    {
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char* paramName = "untrusted";
            const char* bytes = "try {                                  "
                                "  untrusted();                         "
                                "} catch (e) {                          "
                                "  /*                                   "
                                "   * Careful!  We must not reenter JS  "
                                "   * that might try to push a frame.   "
                                "   */                                  "
                                "  return 'From trusted: ' +            "
                                "         e.name + ': ' + e.message;    "
                                "}                                      ";
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0);
            JS::AutoObjectVector emptyScopeChain(cx);
            CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "trusted",
                                      1, &paramName, bytes, strlen(bytes), &fun));
            CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun, JSPROP_ENUMERATE));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, &v));

        const char* paramName = "trusted";
        const char* bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(untrusted);               "
                            "}                                          ";
        JS::CompileOptions options(cx);
        options.setFileAndLine("", 0);
        JS::AutoObjectVector emptyScopeChain(cx);
        CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                                 &paramName, bytes, strlen(bytes), &fun));
        CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(v), &rval));
        bool match;
        CHECK(JS_StringEqualsAscii(cx, rval.toString(), "From trusted: InternalError: too much recursion", &match));
        CHECK(match);
    }

    {
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char* bytes = "return 42";
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0);
            JS::AutoObjectVector emptyScopeChain(cx);
            CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "trusted",
                                      0, nullptr, bytes, strlen(bytes), &fun));
            CHECK(JS_DefineProperty(cx, trusted_glob, "trusted", fun, JSPROP_ENUMERATE));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedFunction fun(cx, JS_NewFunction(cx, CallTrusted, 0, 0, "callTrusted"));
        JS::RootedObject callTrusted(cx, JS_GetFunctionObject(fun));

        const char* paramName = "f";
        const char* bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return f();                              "
                            "}                                          ";
        JS::CompileOptions options(cx);
        options.setFileAndLine("", 0);
        JS::AutoObjectVector emptyScopeChain(cx);
        CHECK(JS::CompileFunction(cx, emptyScopeChain, options, "untrusted", 1,
                                  &paramName, bytes, strlen(bytes), &fun));
        CHECK(JS_DefineProperty(cx, global, "untrusted", fun, JSPROP_ENUMERATE));

        JS::RootedValue arg(cx, JS::ObjectValue(*callTrusted));
        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, JS::HandleValueArray(arg), &rval));
        CHECK(rval.toInt32() == 42);
    }

    return true;
}
END_TEST(testChromeBuffer)
