/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

BEGIN_TEST(testBug795104)
{
    JS::CompileOptions opts(cx);
    opts.setSourcePolicy(JS::CompileOptions::NO_SOURCE);
    const size_t strLen = 60002;
    char *s = static_cast<char *>(JS_malloc(cx, strLen));
    CHECK(s);
    s[0] = '"';
    memset(s + 1, 'x', strLen - 2);
    s[strLen - 1] = '"';
    CHECK(JS::Evaluate(cx, global, opts, s, strLen, NULL));
    CHECK(JS::CompileFunction(cx, global, opts, "f", 0, NULL, s, strLen));
    JS_free(cx, s);

    return true;
}
END_TEST(testBug795104)
