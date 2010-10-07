/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsstr.h"

BEGIN_TEST(testIntString_bug515273)
{
    jsvalRoot v(cx);

    EVAL("'1';", v.addr());
    JSString *str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(strcmp(JS_GetStringBytes(str), "1") == 0);

    EVAL("'42';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(strcmp(JS_GetStringBytes(str), "42") == 0);

    EVAL("'111';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(strcmp(JS_GetStringBytes(str), "111") == 0);

    /* Test other types of static strings. */
    EVAL("'a';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(strcmp(JS_GetStringBytes(str), "a") == 0);

    EVAL("'bc';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(strcmp(JS_GetStringBytes(str), "bc") == 0);

    return true;
}
END_TEST(testIntString_bug515273)
