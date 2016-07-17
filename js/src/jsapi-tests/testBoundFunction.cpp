/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testBoundFunction)
{
    EXEC("function foo() {}");
    JS::RootedValue foo(cx);
    EVAL("foo", &foo);
    JS::RootedValue bound(cx);
    EVAL("foo.bind(1)", &bound);

    JS::RootedFunction foofun(cx, JS_ValueToFunction(cx, foo));
    JS::RootedFunction boundfun(cx, JS_ValueToFunction(cx, bound));

    CHECK(!JS_IsFunctionBound(foofun));
    CHECK(JS_IsFunctionBound(boundfun));

    CHECK(!JS_GetBoundFunctionTarget(foofun));
    JSObject* target = JS_GetBoundFunctionTarget(boundfun);
    CHECK(!!target);
    CHECK(JS_ObjectIsFunction(cx, target));
    JS::RootedValue targetVal(cx, JS::ObjectValue(*target));
    CHECK_SAME(foo, targetVal);

    return true;
}
END_TEST(testBoundFunction)
