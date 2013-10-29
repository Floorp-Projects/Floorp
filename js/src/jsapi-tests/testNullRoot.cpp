/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testNullRoot)
{
    JS::RootedObject obj(cx);
    CHECK(JS_AddObjectRoot(cx, obj.address()));

    JS::RootedString str(cx);
    CHECK(JS_AddStringRoot(cx, str.address()));

    JS::RootedScript script(cx);
    CHECK(JS_AddNamedScriptRoot(cx, script.address(), "testNullRoot's script"));

    // This used to crash because obj was nullptr.
    JS_GC(cx->runtime());

    JS_RemoveObjectRoot(cx, obj.address());
    JS_RemoveStringRoot(cx, str.address());
    JS_RemoveScriptRoot(cx, script.address());
    return true;
}
END_TEST(testNullRoot)
