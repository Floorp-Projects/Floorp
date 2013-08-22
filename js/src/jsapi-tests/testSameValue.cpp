/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testSameValue)
{

    /*
     * NB: passing a double that fits in an integer jsval is API misuse.  As a
     * matter of defense in depth, however, JS_SameValue should return the
     * correct result comparing a positive-zero double to a negative-zero
     * double, and this is believed to be the only way to make such a
     * comparison possible.
     */
    jsval v1 = DOUBLE_TO_JSVAL(0.0);
    jsval v2 = DOUBLE_TO_JSVAL(-0.0);
    bool same;
    CHECK(JS_SameValue(cx, v1, v2, &same));
    CHECK(!same);
    return true;
}
END_TEST(testSameValue)
