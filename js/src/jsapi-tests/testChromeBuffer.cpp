#include "tests.h"

static void
Destroy(JSContext *cx, JSPrincipals *prin);

JSPrincipals system_principals = {
    (char *)"", 1, Destroy, NULL
};

static void
Destroy(JSContext *cx, JSPrincipals *prin)
{
    JS_ASSERT(prin == &system_principals);
}

JSClass global_class = {
    "global",
    JSCLASS_IS_GLOBAL | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JS::Anchor<JSObject *> trusted_glob, trusted_fun;

JSBool
CallTrusted(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!JS_SaveFrameChain(cx))
        return JS_FALSE;

    JSBool ok = JS_FALSE;
    {
        JSAutoEnterCompartment ac;
        ok = ac.enter(cx, trusted_glob.get());
        if (!ok)
            goto out;
        ok = JS_CallFunctionValue(cx, NULL, OBJECT_TO_JSVAL(trusted_fun.get()),
                                  0, NULL, vp);
    }
  out:
    JS_RestoreFrameChain(cx);
    return ok;
}

BEGIN_TEST(testChromeBuffer)
{
    JS_SetTrustedPrincipals(rt, &system_principals);

    JSFunction *fun;
    JSObject *o;

    CHECK(o = JS_NewCompartmentAndGlobalObject(cx, &global_class, &system_principals));
    trusted_glob.set(o);

    /*
     * Check that, even after untrusted content has exhausted the stack, code
     * compiled with "trusted principals" can run using reserved trusted-only
     * buffer space.
     */
    {
        {
            JSAutoEnterCompartment ac;
            CHECK(ac.enter(cx, trusted_glob.get()));
            const char *paramName = "x";
            const char *bytes = "return x ? 1 + trusted(x-1) : 0";
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, trusted_glob.get(), &system_principals,
                                                        "trusted", 1, &paramName, bytes, strlen(bytes),
                                                        "", 0));
            trusted_fun.set(JS_GetFunctionObject(fun));
        }

        jsval v = OBJECT_TO_JSVAL(trusted_fun.get());
        CHECK(JS_WrapValue(cx, &v));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(100);                     "
                            "}                                          ";
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        jsval rval;
        CHECK(JS_CallFunction(cx, NULL, fun, 1, &v, &rval));
        CHECK(JSVAL_TO_INT(rval) == 100);
    }

    /*
     * Check that content called from chrome in the reserved-buffer space
     * immediately ooms.
     */
    {
        {
            JSAutoEnterCompartment ac;
            CHECK(ac.enter(cx, trusted_glob.get()));
            const char *paramName = "untrusted";
            const char *bytes = "try {                                  "
                                "  untrusted();                         "
                                "} catch (e) {                          "
                                "  return 'From trusted: ' + e;         "
                                "}                                      ";
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, trusted_glob.get(), &system_principals,
                                                        "trusted", 1, &paramName, bytes, strlen(bytes),
                                                        "", 0));
            trusted_fun.set(JS_GetFunctionObject(fun));
        }

        jsval v = OBJECT_TO_JSVAL(trusted_fun.get());
        CHECK(JS_WrapValue(cx, &v));

        const char *paramName = "trusted";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return trusted(untrusted);               "
                            "}                                          ";
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        jsval rval;
        CHECK(JS_CallFunction(cx, NULL, fun, 1, &v, &rval));
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
            JSAutoEnterCompartment ac;
            CHECK(ac.enter(cx, trusted_glob.get()));
            const char *bytes = "return 42";
            CHECK(fun = JS_CompileFunctionForPrincipals(cx, trusted_glob.get(), &system_principals,
                                                        "trusted", 0, NULL, bytes, strlen(bytes),
                                                        "", 0));
            trusted_fun.set(JS_GetFunctionObject(fun));
        }

        JSFunction *fun = JS_NewFunction(cx, CallTrusted, 0, 0, global, "callTrusted");
        JS::Anchor<JSObject *> callTrusted(JS_GetFunctionObject(fun));

        const char *paramName = "f";
        const char *bytes = "try {                                      "
                            "  return untrusted(trusted);               "
                            "} catch (e) {                              "
                            "  return f();                              "
                            "}                                          ";
        CHECK(fun = JS_CompileFunction(cx, global, "untrusted", 1, &paramName,
                                       bytes, strlen(bytes), "", 0));

        jsval arg = OBJECT_TO_JSVAL(callTrusted.get());
        jsval rval;
        CHECK(JS_CallFunction(cx, NULL, fun, 1, &arg, &rval));
        CHECK(JSVAL_TO_INT(rval) == 42);
    }

    return true;
}
END_TEST(testChromeBuffer)
