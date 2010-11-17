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
    CHECK(JS_MatchStringAndAscii(str, "1"));

    EVAL("'42';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(JS_MatchStringAndAscii(str, "42"));

    EVAL("'111';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(JS_MatchStringAndAscii(str, "111"));

    /* Test other types of static strings. */
    EVAL("'a';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(JS_MatchStringAndAscii(str, "a"));

    EVAL("'bc';", v.addr());
    str = JSVAL_TO_STRING(v.value());
    CHECK(JSString::isStatic(str));
    CHECK(JS_MatchStringAndAscii(str, "bc"));

    return true;
}
END_TEST(testIntString_bug515273)
