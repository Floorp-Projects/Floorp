/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testDeepFreeze_bug535703)
{
    jsval v;
    EVAL("var x = {}; x;", &v);
    CHECK(JS_DeepFreezeObject(cx, JSVAL_TO_OBJECT(v)));  // don't crash
    EVAL("Object.isFrozen(x)", &v);
    CHECK_SAME(v, JSVAL_TRUE);
    return true;
}
END_TEST(testDeepFreeze_bug535703)

BEGIN_TEST(testDeepFreeze_deep)
{
    jsval a, o;
    EXEC("var a = {}, o = a;\n"
         "for (var i = 0; i < 10000; i++)\n"
         "    a = {x: a, y: a};\n");
    EVAL("a", &a);
    EVAL("o", &o);

    CHECK(JS_DeepFreezeObject(cx, JSVAL_TO_OBJECT(a)));

    jsval b;
    EVAL("Object.isFrozen(a)", &b);
    CHECK_SAME(b, JSVAL_TRUE);
    EVAL("Object.isFrozen(o)", &b);
    CHECK_SAME(b, JSVAL_TRUE);
    return true;
}
END_TEST(testDeepFreeze_deep)

BEGIN_TEST(testDeepFreeze_loop)
{
    jsval x, y;
    EXEC("var x = [], y = {x: x}; y.y = y; x.push(x, y);");
    EVAL("x", &x);
    EVAL("y", &y);

    CHECK(JS_DeepFreezeObject(cx, JSVAL_TO_OBJECT(x)));

    jsval b;
    EVAL("Object.isFrozen(x)", &b);
    CHECK_SAME(b, JSVAL_TRUE);
    EVAL("Object.isFrozen(y)", &b);
    CHECK_SAME(b, JSVAL_TRUE);
    return true;
}
END_TEST(testDeepFreeze_loop)
