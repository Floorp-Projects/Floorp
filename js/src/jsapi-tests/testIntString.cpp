/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "vm/String.h"

BEGIN_TEST(testIntString_bug515273)
{
    JS::RootedValue v(cx);

    EVAL("'1';", v.address());
    JSString *str = JSVAL_TO_STRING(v);
    CHECK(JS_StringHasBeenInterned(cx, str));
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(str), "1"));

    EVAL("'42';", v.address());
    str = JSVAL_TO_STRING(v);
    CHECK(JS_StringHasBeenInterned(cx, str));
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(str), "42"));

    EVAL("'111';", v.address());
    str = JSVAL_TO_STRING(v);
    CHECK(JS_StringHasBeenInterned(cx, str));
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(str), "111"));

    /* Test other types of static strings. */
    EVAL("'a';", v.address());
    str = JSVAL_TO_STRING(v);
    CHECK(JS_StringHasBeenInterned(cx, str));
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(str), "a"));

    EVAL("'bc';", v.address());
    str = JSVAL_TO_STRING(v);
    CHECK(JS_StringHasBeenInterned(cx, str));
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(str), "bc"));

    return true;
}
END_TEST(testIntString_bug515273)
