/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(selfTest_NaNsAreSame)
{
    JS::RootedValue v1(cx), v2(cx);
    EVAL("0/0", &v1);  // NaN
    CHECK_SAME(v1, v1);

    EVAL("Math.sin('no')", &v2);  // also NaN
    CHECK_SAME(v1, v2);
    return true;
}
END_TEST(selfTest_NaNsAreSame)
