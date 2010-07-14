/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testSeal_bug535703)
{
    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    CHECK(obj);
    JS_SealObject(cx, obj, JS_TRUE);  // don't crash
    return true;
}
END_TEST(testSeal_bug535703)
