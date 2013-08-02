/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

JSPrincipals system_principals = {
    1
};

JSClass global_class = {
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

JSObject *trusted_glob = NULL;
JSObject *trusted_fun = NULL;

JSBool
CallTrusted(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!JS_SaveFrameChain(cx))
        return JS_FALSE;

    JSBool ok = JS_FALSE;
    {
        JSAutoCompartment ac(cx, trusted_glob);
        ok = JS_CallFunctionValue(cx, NULL, JS::ObjectValue(*trusted_fun),
                                  0, NULL, vp);
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
    if (!JS_AddNamedObjectRoot(cx, &trusted_fun, "trusted-function"))
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
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, global, &system_principals,
                                                        "trusted", 1, &paramName, bytes, strlen(bytes),
                                                        "", 0));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, v.address()));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(100);                     "
                            "}                                          ";
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, NULL, fun, 1, v.address(), rval.address()));
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
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, global, &system_principals,
                                                        "trusted", 1, &paramName, bytes, strlen(bytes),
                                                        "", 0));
            trusted_fun = JS_GetFunctionObject(fun);
        }

        JS::RootedValue v(cx, JS::ObjectValue(*trusted_fun));
        CHECK(JS_WrapValue(cx, v.address()));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(untrusted);               "
                            "}                                          ";
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, NULL, fun, 1, v.address(), rval.address()));
        JSBool match;
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
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, global, &system_principals,
                                                        "trusted", 0, NULL, bytes, strlen(bytes),
                                                        "", 0));
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
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        JS::RootedValue arg(cx, JS::ObjectValue(*callTrusted));
        JS::RootedValue rval(cx);
        CHECK(JS_CallFunction(cx, NULL, fun, 1, arg.address(), rval.address()));
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
