/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testJSEvaluateScript)
{
    jsvalRoot proto(cx);
    JSObject *obj = JS_NewObject(cx, NULL, NULL, global);
    CHECK(obj);

    uint32_t options = JS_GetOptions(cx);
    CHECK(options & JSOPTION_VAROBJFIX);

    static const char src[] = "var x = 5;";

    JS::Value retval;
    CHECK(JS_EvaluateScript(cx, obj, src, sizeof(src) - 1, __FILE__, __LINE__,
                            &retval));

    JSBool hasProp = JS_TRUE;
    CHECK(JS_AlreadyHasOwnProperty(cx, obj, "x", &hasProp));
    CHECK(!hasProp);

    hasProp = JS_FALSE;
    CHECK(JS_HasProperty(cx, global, "x", &hasProp));
    CHECK(hasProp);

    // Now do the same thing, but without JSOPTION_VAROBJFIX
    JS_SetOptions(cx, options & ~JSOPTION_VAROBJFIX);

    static const char src2[] = "var y = 5;";

    CHECK(JS_EvaluateScript(cx, obj, src2, sizeof(src2) - 1, __FILE__, __LINE__,
                            &retval));

    hasProp = JS_FALSE;
    CHECK(JS_AlreadyHasOwnProperty(cx, obj, "y", &hasProp));
    CHECK(hasProp);

    hasProp = JS_TRUE;
    CHECK(JS_AlreadyHasOwnProperty(cx, global, "y", &hasProp));
    CHECK(!hasProp);

    return true;
}
END_TEST(testJSEvaluateScript)


