/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsdbgapi.h"

static int emptyTrapCallCount = 0;

static JSTrapStatus
EmptyTrapHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                 jsval closure)
{
    JS_GC(cx);
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
    JSScript *script = JS_CompileScript(cx, global, source, strlen(source), __FILE__, 1);
    CHECK(script);
    JSObject *scrobj = JS_NewScriptObject(cx, script);
    CHECK(scrobj);
    jsvalRoot v(cx, OBJECT_TO_JSVAL(scrobj));

    // execute
    jsvalRoot v2(cx);
    CHECK(JS_ExecuteScript(cx, global, script, v2.addr()));
    CHECK(JSVAL_IS_OBJECT(v2));
    CHECK(emptyTrapCallCount == 0);

    // Disable JIT for debugging
    JS_SetOptions(cx, JS_GetOptions(cx) & ~JSOPTION_JIT);

    // Enable debug mode
    CHECK(JS_SetDebugMode(cx, JS_TRUE));

    jsbytecode *line2 = JS_LineNumberToPC(cx, script, 1);
    CHECK(line2);

    jsbytecode *line6 = JS_LineNumberToPC(cx, script, 5);
    CHECK(line2);

    static const char trapClosureText[] = "some trap closure";
    JSString *trapClosure = JS_NewStringCopyZ(cx, trapClosureText);
    CHECK(trapClosure);
    JS_SetTrap(cx, script, line2, EmptyTrapHandler, STRING_TO_JSVAL(trapClosure));
    JS_SetTrap(cx, script, line6, EmptyTrapHandler, STRING_TO_JSVAL(trapClosure));

    JS_GC(cx);

    CHECK(0 == strcmp(trapClosureText, JS_GetStringBytes(trapClosure)));

    // execute
    CHECK(JS_ExecuteScript(cx, global, script, v2.addr()));
    CHECK(emptyTrapCallCount == 11);

    JS_GC(cx);

    CHECK(0 == strcmp(trapClosureText, JS_GetStringBytes(trapClosure)));

    return true;
}
END_TEST(testTrap_gc)

