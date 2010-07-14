/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

BEGIN_TEST(testSameValue)
{
    jsvalRoot v1(cx);
    jsvalRoot v2(cx);

    /*
     * NB: passing a double that fits in an integer jsval is API misuse.  As a
     * matter of defense in depth, however, JS_SameValue should return the
     * correct result comparing a positive-zero double to a negative-zero
     * double, and this is believed to be the only way to make such a
     * comparison possible.
     */
    CHECK(JS_NewDoubleValue(cx, 0.0, v1.addr()));
    CHECK(JSVAL_IS_DOUBLE(v1));
    CHECK(JS_NewNumberValue(cx, -0.0, v2.addr()));
    CHECK(!JS_SameValue(cx, v1, v2));
    return true;
}
END_TEST(testSameValue)
