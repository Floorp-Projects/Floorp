/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"
#include "jsdbgapi.h"
#include "jsobjinlines.h"

JSPrincipals *sCurrentGlobalPrincipals = NULL;

JSPrincipals *
ObjectPrincipalsFinder(JSObject *)
{
    return sCurrentGlobalPrincipals;
}

static const JSSecurityCallbacks seccb = {
    NULL,
    NULL,
    ObjectPrincipalsFinder,
    NULL
};

JSPrincipals *sOriginPrincipalsInErrorReporter = NULL;

static void
ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    sOriginPrincipalsInErrorReporter = report->originPrincipals;
}

JSPrincipals prin1 = { 1 };
JSPrincipals prin2 = { 1 };

BEGIN_TEST(testOriginPrincipals)
{
    JS_SetSecurityCallbacks(rt, &seccb);

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

bool
eval(const char *asciiChars, JSPrincipals *principals, JSPrincipals *originPrincipals, jsval *rval)
{
    size_t len = strlen(asciiChars);
    jschar *chars = new jschar[len+1];
    for (size_t i = 0; i < len; ++i)
        chars[i] = asciiChars[i];
    chars[len] = 0;

    bool ok = JS_EvaluateUCScriptForPrincipalsVersionOrigin(cx, global,
                                                            principals,
                                                            originPrincipals,
                                                            chars, len, "", 0, rval,
                                                            JSVERSION_DEFAULT);
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
    sCurrentGlobalPrincipals = principal;

    jsval rval;
    CHECK(eval(asciiChars, principal, originPrincipal, &rval));

    JSScript *script = JS_GetFunctionScript(cx, JSVAL_TO_OBJECT(rval)->toFunction());
    CHECK(JS_GetScriptPrincipals(script) == principal);
    CHECK(JS_GetScriptOriginPrincipals(script) == originPrincipal);

    return true;
}

bool
testError(const char *asciiChars)
{
    jsval rval;
    CHECK(!eval(asciiChars, &prin1, &prin2 /* = originPrincipals */, &rval));
    CHECK(JS_ReportPendingException(cx));
    CHECK(sOriginPrincipalsInErrorReporter == &prin2);
    return true;
}
END_TEST(testOriginPrincipals)
