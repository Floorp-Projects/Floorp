/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testSetProperty_InheritedGlobalSetter)
{
    // This is a JSAPI test because jsapi-test globals do not have a resolve
    // hook and therefore can use the property cache in some cases where the
    // shell can't.
    MOZ_RELEASE_ASSERT(!JS_GetClass(global)->getResolve());

    CHECK(JS_DefineProperty(cx, global, "HOTLOOP", 8, 0));
    EXEC("var n = 0;\n"
         "var global = this;\n"
         "function f() { n++; }\n"
         "Object.defineProperty(Object.prototype, 'x', {set: f});\n"
         "for (var i = 0; i < HOTLOOP; i++)\n"
         "    global.x = i;\n");
    EXEC("if (n != HOTLOOP)\n"
         "    throw 'FAIL';\n");
    return true;
}
END_TEST(testSetProperty_InheritedGlobalSetter)
