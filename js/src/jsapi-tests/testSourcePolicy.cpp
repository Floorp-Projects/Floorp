/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsscript.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testBug795104)
{
    JS::CompileOptions opts(cx);
    opts.setSourcePolicy(JS::CompileOptions::NO_SOURCE);
    const size_t strLen = 60002;
    char *s = static_cast<char *>(JS_malloc(cx, strLen));
    CHECK(s);
    s[0] = '"';
    memset(s + 1, 'x', strLen - 2);
    s[strLen - 1] = '"';
    CHECK(JS::Evaluate(cx, global, opts, s, strLen, NULL));
    CHECK(JS::CompileFunction(cx, global, opts, "f", 0, NULL, s, strLen));
    JS_free(cx, s);

    return true;
}
END_TEST(testBug795104)

const char *simpleSource = "var x = 4;";

static void
newScriptHook(JSContext *cx, const char *fn, unsigned lineno,
              JSScript *script, JSFunction *fun, void *data)
{
    if (!JS_StringEqualsAscii(cx, script->sourceData(cx), simpleSource, (bool *)data))
        *((bool *)data) = false;
}

BEGIN_TEST(testScriptSourceReentrant)
{
    JS::CompileOptions opts(cx);
    bool match = false;
    JS_SetNewScriptHook(rt, newScriptHook, &match);
    CHECK(JS::Evaluate(cx, global, opts, simpleSource, strlen(simpleSource), NULL));
    CHECK(match);
    JS_SetNewScriptHook(rt, NULL, NULL);

    return true;
}
END_TEST(testScriptSourceReentrant)
