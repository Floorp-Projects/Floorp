/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCExactRooting)
{
    JS::RootedObject rootCx(cx, JS_NewObject(cx, nullptr, nullptr, nullptr));
    JS::RootedObject rootRt(cx->runtime(), JS_NewObject(cx, nullptr, nullptr, nullptr));

    JS_GC(cx->runtime());

    /* Use the objects we just created to ensure that they are still alive. */
    JS_DefineProperty(cx, rootCx, "foo", JS::DoubleValue(0), nullptr, nullptr, 0);
    JS_DefineProperty(cx, rootRt, "foo", JS::DoubleValue(0), nullptr, nullptr, 0);

    return true;
}
END_TEST(testGCExactRooting)
