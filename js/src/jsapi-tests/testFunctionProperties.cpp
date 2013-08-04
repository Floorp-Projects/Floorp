/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testFunctionProperties)
{
    JS::RootedValue x(cx);
    EVAL("(function f() {})", x.address());

    JS::RootedObject obj(cx, JSVAL_TO_OBJECT(x));

    JS::RootedValue y(cx);
    CHECK(JS_GetProperty(cx, obj, "arguments", &y));
    CHECK_SAME(y, JSVAL_NULL);

    CHECK(JS_GetProperty(cx, obj, "caller", &y));
    CHECK_SAME(y, JSVAL_NULL);

    return true;
}
END_TEST(testFunctionProperties)
