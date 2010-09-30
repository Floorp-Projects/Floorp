/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Test script cloning.
 */

#include "tests.h"
#include "jsapi.h"

BEGIN_TEST(test_cloneScript)
{
    JSObject *A, *B;

    CHECK(A = createGlobal());
    CHECK(B = createGlobal());

    const char *source = 
        "var i = 0;\n"
        "var sum = 0;\n"
        "while (i < 10) {\n"
        "    sum += i;\n"
        "    ++i;\n"
        "}\n"
        "(sum);\n";

    JSObject *obj;

    // compile for A
    {
        JSAutoEnterCompartment a;
        if (!a.enter(cx, A))
            return false;

        JSFunction *fun;
        CHECK(fun = JS_CompileFunction(cx, A, "f", 0, NULL, source, strlen(source), __FILE__, 1));
        CHECK(obj = JS_GetFunctionObject(fun));
    }

    // clone into B
    {
        JSAutoEnterCompartment b;
        if (!b.enter(cx, B))
            return false;

        CHECK(JS_CloneFunctionObject(cx, obj, B));
    }

    return true;
}
END_TEST(test_cloneScript)
