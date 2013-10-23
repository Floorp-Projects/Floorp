/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testNullRoot)
{
    JSObject *obj = nullptr;
    CHECK(JS_AddObjectRoot(cx, &obj));

    JSString *str = nullptr;
    CHECK(JS_AddStringRoot(cx, &str));

    JSScript *scr = nullptr;
    CHECK(JS_AddNamedScriptRoot(cx, &scr, "testNullRoot's scr"));

    // This used to crash because obj was nullptr.
    JS_GC(cx->runtime());

    JS_RemoveObjectRoot(cx, &obj);
    JS_RemoveStringRoot(cx, &str);
    JS_RemoveScriptRoot(cx, &scr);
    return true;
}
END_TEST(testNullRoot)
