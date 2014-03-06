/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/OldDebugAPI.h"
#include "jsapi-tests/tests.h"

static JSPrincipals *sOriginPrincipalsInErrorReporter = nullptr;
static TestJSPrincipals prin1(1);
static TestJSPrincipals prin2(1);

BEGIN_TEST(testOriginPrincipals)
{
    /*
     * Currently, the only way to set a non-trivial originPrincipal is to use
     * JS_EvaluateUCScriptForPrincipalsVersionOrigin. This does not expose the
     * compiled script, so we can only test nested scripts.
     */

    CHECK(testOuter("function f() {return 1}; f;"));
    CHECK(testOuter("function outer() { return (function () {return 2}); }; outer();"));
    CHECK(testOuter("eval('(function() {return 3})');"));
    CHECK(testOuter("(function (){ return eval('(function() {return 4})'); })()"));
    CHECK(testOuter("(function (){ return eval('(function() { return eval(\"(function(){return 5})\") })()'); })()"));
    CHECK(testOuter("new Function('return 6')"));
    CHECK(testOuter("function f() { return new Function('return 7') }; f();"));
    CHECK(testOuter("eval('new Function(\"return 8\")')"));
    CHECK(testOuter("(new Function('return eval(\"(function(){return 9})\")'))()"));
    CHECK(testOuter("(function(){return function(){return 10}}).bind()()"));
    CHECK(testOuter("var e = eval; (function() { return e('(function(){return 11})') })()"));

    JS_SetErrorReporter(cx, ErrorReporter);
    CHECK(testError("eval(-)"));
    CHECK(testError("-"));
    CHECK(testError("new Function('x', '-')"));
    CHECK(testError("eval('new Function(\"x\", \"-\")')"));

    /*
     * NB: uncaught exceptions, when reported, have nothing on the stack so
     * both the filename and originPrincipals are null. E.g., this would fail:
     *
     *   CHECK(testError("throw 3"));
     */
    return true;
}

static void
ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    sOriginPrincipalsInErrorReporter = report->originPrincipals;
}

bool
eval(const char *asciiChars, JSPrincipals *principals, JSPrincipals *originPrincipals, JS::MutableHandleValue rval)
{
    size_t len = strlen(asciiChars);
    jschar *chars = new jschar[len+1];
    for (size_t i = 0; i < len; ++i)
        chars[i] = asciiChars[i];
    chars[len] = 0;

    JS::RootedObject global(cx, JS_NewGlobalObject(cx, getGlobalClass(), principals, JS::FireOnNewGlobalHook));
    CHECK(global);
    JSAutoCompartment ac(cx, global);
    CHECK(JS_InitStandardClasses(cx, global));


    JS::CompileOptions options(cx);
    options.setOriginPrincipals(originPrincipals)
           .setFileAndLine("", 0);

    bool ok = JS::Evaluate(cx, global, options, chars, len, rval.address());

    delete[] chars;
    return ok;
}

bool
testOuter(const char *asciiChars)
{
    CHECK(testInner(asciiChars, &prin1, &prin1));
    CHECK(testInner(asciiChars, &prin1, &prin2));
    return true;
}

bool
testInner(const char *asciiChars, JSPrincipals *principal, JSPrincipals *originPrincipal)
{
    JS::RootedValue rval(cx);
    CHECK(eval(asciiChars, principal, originPrincipal, &rval));

    JSScript *script = JS_GetFunctionScript(cx, &rval.toObject().as<JSFunction>());
    CHECK(JS_GetScriptPrincipals(script) == principal);
    CHECK(JS_GetScriptOriginPrincipals(script) == originPrincipal);

    return true;
}

bool
testError(const char *asciiChars)
{
    JS::RootedValue rval(cx);
    CHECK(!eval(asciiChars, &prin1, &prin2 /* = originPrincipals */, &rval));
    CHECK(JS_ReportPendingException(cx));
    CHECK(sOriginPrincipalsInErrorReporter == &prin2);
    return true;
}
END_TEST(testOriginPrincipals)
