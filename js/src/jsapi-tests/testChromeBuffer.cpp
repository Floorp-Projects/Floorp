/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static TestJSPrincipals system_principals(1);

static const JSClass global_class = {
    "global",
    JSCLASS_IS_GLOBAL | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static JSObject *trusted_glob = nullptr;
static JSObject *trusted_fun = nullptr;

static bool
CallTrusted(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!JS_SaveFrameChain(cx))
        return false;

    bool ok = false;
    {
        JSAutoCompartment ac(cx, trusted_glob);
        ok = JS_CallFunctionValue(cx, nullptr, JS::ObjectValue(*trusted_fun),
                                  JS::EmptyValueArray, vp);
    }
    JS_RestoreFrameChain(cx);
    return ok;
}

BEGIN_TEST(testChromeBuffer)
{
    JS_SetTrustedPrincipals(rt, &system_principals);

    trusted_glob = JS_NewGlobalObject(cx, &global_class, &system_principals, JS::FireOnNewGlobalHook);
    CHECK(trusted_glob);

    if (!JS_AddNamedObjectRoot(cx, &trusted_glob, "trusted-global"))
        return false;

    JSFunction *fun;

    /*
     * Check that, even after untrusted content has exhausted the stack, code
     * compiled with "trusted principals" can run using reserved trusted-only
     * buffer space.
     */
    {
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char *paramName = "x";
            const char *bytes = "return x ? 1 + trusted(x-1) : 0";
            JS::HandleObject global = JS::HandleObject::fromMarkedLocation(&trusted_glob);
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0)
                   .setPrincipals(&system_principals);
            CHECK(fun = JS_CompileFunction(cx, global, "trusted", 1, &paramName,
                                           bytes, strlen(bytes), options));
            trusted_fun = JS_GetFunctionObject(fun);
            if (!JS_AddNamedObjectRoot(cx, &trusted_fun, "trusted-function"))
                return false;
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, &v));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
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
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), options));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, v, rval.address()));
        CHECK(JSVAL_TO_INT(rval) == 100);
    }

    /*
     * Check that content called from chrome in the reserved-buffer space
     * immediately ooms.
     */
    {
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char *paramName = "untrusted";
            const char *bytes = "try {                                  "
                                "  untrusted();                         "
                                "} catch (e) {                          "
                                "  return 'From trusted: ' + e;         "
                                "}                                      ";
            JS::HandleObject global = JS::HandleObject::fromMarkedLocation(&trusted_glob);
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0)
                   .setPrincipals(&system_principals);
            CHECK(fun = JS_CompileFunction(cx, global, "trusted", 1, &paramName,
                                           bytes, strlen(bytes), options));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, &v));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(untrusted);               "
                            "}                                          ";
        JS::CompileOptions options(cx);
        options.setFileAndLine("", 0);
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), options));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, v, rval.address()));
        bool match;
        CHECK(JS_StringEqualsAscii(cx, JSVAL_TO_STRING(rval), "From trusted: InternalError: too much recursion", &match));
        CHECK(match);
    }

    /*
     * Check that JS_SaveFrameChain called on the way from content to chrome
     * (say, as done by XPCJSContextSTack::Push) works.
     */
    {
        {
            JSAutoCompartment ac(cx, trusted_glob);
            const char *bytes = "return 42";
            JS::HandleObject global = JS::HandleObject::fromMarkedLocation(&trusted_glob);
            JS::CompileOptions options(cx);
            options.setFileAndLine("", 0)
                   .setPrincipals(&system_principals);
            CHECK(fun = JS_CompileFunction(cx, global, "trusted", 0, nullptr,
                                           bytes, strlen(bytes), options));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedFunction fun(cx, JS_NewFunction(cx, CallTrusted, 0, 0, global, "callTrusted"));
        JS::RootedObject callTrusted(cx, JS_GetFunctionObject(fun));

        const char *paramName = "f";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return f();                              "
                            "}                                          ";
        JS::CompileOptions options(cx);
        options.setFileAndLine("", 0);
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), options));

        JS::RootedValue arg(cx, JS::ObjectValue(*callTrusted));
        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, nullptr, fun, arg, rval.address()));
        CHECK(JSVAL_TO_INT(rval) == 42);
    }

    return true;
}
virtual void uninit() {
    JS_RemoveObjectRoot(cx, &trusted_glob);
    JS_RemoveObjectRoot(cx, &trusted_fun);
    JSAPITest::uninit();
}
END_TEST(testChromeBuffer)
