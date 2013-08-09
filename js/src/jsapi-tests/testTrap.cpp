/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsdbgapi.h"

#include "jsapi-tests/tests.h"

static int emptyTrapCallCount = 0;

static JSTrapStatus
EmptyTrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                 jsval closureArg)
{
    JS::RootedValue closure(cx, closureArg);
    JS_GC(JS_GetRuntime(cx));
    if (JSVAL_IS_STRING(closure))
        ++emptyTrapCallCount;
    return JSTRAP_CONTINUE;
}

BEGIN_TEST(testTrap_gc)
{
    static const char source[] =
"var i = 0;\n"
"var sum = 0;\n"
"while (i < 10) {\n"
"    sum += i;\n"
"    ++i;\n"
"}\n"
"({ result: sum });\n"
        ;

    // compile
    JS::RootedScript script(cx, JS_CompileScript(cx, global, source, strlen(source), __FILE__, 1));
    CHECK(script);

    // execute
    JS::RootedValue v2(cx);
    CHECK(JS_ExecuteScript(cx, global, script, v2.address()));
    CHECK(v2.isObject());
    CHECK_EQUAL(emptyTrapCallCount, 0);

    // Enable debug mode
    CHECK(JS_SetDebugMode(cx, true));

    static const char trapClosureText[] = "some trap closure";

    // scope JSScript  usage to make sure that it is not used after
    // JS_ExecuteScript. This way we avoid using Anchor.
    JS::RootedString trapClosure(cx);
    {
        jsbytecode *line2 = JS_LineNumberToPC(cx, script, 1);
        CHECK(line2);

        jsbytecode *line6 = JS_LineNumberToPC(cx, script, 5);
        CHECK(line2);

        trapClosure = JS_NewStringCopyZ(cx, trapClosureText);
        CHECK(trapClosure);
        JS_SetTrap(cx, script, line2, EmptyTrapHandler, STRING_TO_JSVAL(trapClosure));
        JS_SetTrap(cx, script, line6, EmptyTrapHandler, STRING_TO_JSVAL(trapClosure));

        JS_GC(rt);

        CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(trapClosure), trapClosureText));
    }

    // execute
    CHECK(JS_ExecuteScript(cx, global, script, v2.address()));
    CHECK_EQUAL(emptyTrapCallCount, 11);

    JS_GC(rt);

    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(trapClosure), trapClosureText));

    return true;
}
END_TEST(testTrap_gc)

