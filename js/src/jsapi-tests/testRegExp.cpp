/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

BEGIN_TEST(testObjectIsRegExp)
{
    jsvalRoot val(cx);
    JSObject *obj;

    EVAL("new Object", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK(!JS_ObjectIsRegExp(cx, obj));

    EVAL("/foopy/", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK(JS_ObjectIsRegExp(cx, obj));

    return true;
}
END_TEST(testObjectIsRegExp)

BEGIN_TEST(testGetRegExpFlags)
{
    jsvalRoot val(cx);
    JSObject *obj;

    EVAL("/foopy/", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK_EQUAL(JS_GetRegExpFlags(cx, obj), 0);

    EVAL("/foopy/g", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK_EQUAL(JS_GetRegExpFlags(cx, obj), JSREG_GLOB);

    EVAL("/foopy/gi", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    CHECK_EQUAL(JS_GetRegExpFlags(cx, obj), (JSREG_FOLD | JSREG_GLOB));

    return true;
}
END_TEST(testGetRegExpFlags)

BEGIN_TEST(testGetRegExpSource)
{
    jsvalRoot val(cx);
    JSObject *obj;

    EVAL("/foopy/", val.addr());
    obj = JSVAL_TO_OBJECT(val.value());
    JSString *source = JS_GetRegExpSource(cx, obj);
    CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(source), "foopy"));

    return true;
}
END_TEST(testGetRegExpSource)
