/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsscript.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testBug795104)
{
    JS::CompileOptions opts(cx);
    JS::CompartmentOptionsRef(cx->compartment()).setDiscardSource(true);
    const size_t strLen = 60002;
    char *s = static_cast<char *>(JS_malloc(cx, strLen));
    CHECK(s);
    s[0] = '"';
    memset(s + 1, 'x', strLen - 2);
    s[strLen - 1] = '"';
    CHECK(JS::Evaluate(cx, global, opts, s, strLen));
    JS::RootedFunction fun(cx);
    CHECK(JS::CompileFunction(cx, global, opts, "f", 0, nullptr, s, strLen, &fun));
    CHECK(fun);
    JS_free(cx, s);

    return true;
}
END_TEST(testBug795104)

static const char *const simpleSource = "var x = 4;";

BEGIN_TEST(testScriptSourceReentrant)
{
    JS::CompileOptions opts(cx);
    bool match = false;
    JS_SetNewScriptHook(rt, NewScriptHook, &match);
    CHECK(JS::Evaluate(cx, global, opts, simpleSource, strlen(simpleSource)));
    CHECK(match);
    JS_SetNewScriptHook(rt, nullptr, nullptr);

    return true;
}

static void
NewScriptHook(JSContext *cx, const char *fn, unsigned lineno,
              JSScript *script, JSFunction *fun, void *data)
{
    if (!JS_StringEqualsAscii(cx, script->sourceData(cx), simpleSource, (bool *)data))
        *((bool *)data) = false;
}
END_TEST(testScriptSourceReentrant)
