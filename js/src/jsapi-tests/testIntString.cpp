/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testIntString_bug515273)
{
    jsvalRoot v(cx);
    EVAL("'42';", v.addr());

    JSString *str = JSVAL_TO_STRING(v.value());
    const char *bytes = JS_GetStringBytes(str);
    CHECK(strcmp(bytes, "42") == 0);
    return true;
}
END_TEST(testIntString_bug515273)
