/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"
#include "jsutil.h"

static const jschar arr[] = {
    'h', 'i', ',', 'd', 'o', 'n', '\'', 't', ' ', 'd', 'e', 'l', 'e', 't', 'e', ' ', 'm', 'e', '\0'
};
static const size_t arrlen = sizeof(arr) / sizeof(arr[0]) - 1;

static int finalized1 = 0;
static int finalized2 = 0;

static void
finalize_str(const JSStringFinalizer *fin, jschar *chars);

static const JSStringFinalizer finalizer1 = { finalize_str };
static const JSStringFinalizer finalizer2 = { finalize_str };

static void
finalize_str(const JSStringFinalizer *fin, jschar *chars)
{
    if (chars && js::PodEqual(const_cast<const jschar *>(chars), arr, arrlen)) {
        if (fin == &finalizer1) {
            ++finalized1;
        } else if (fin == &finalizer2) {
            ++finalized2;
        }
    }
}

BEGIN_TEST(testExternalStrings)
{
    const unsigned N = 1000;

    for (unsigned i = 0; i < N; ++i) {
        CHECK(JS_NewExternalString(cx, arr, arrlen, &finalizer1));
        CHECK(JS_NewExternalString(cx, arr, arrlen, &finalizer2));
    }

    // clear that newborn root
    JS_NewUCStringCopyN(cx, arr, arrlen);

    JS_GC(rt);

    // a generous fudge factor to account for strings rooted by conservative gc
    const unsigned epsilon = 10;

    CHECK((N - finalized1) < epsilon);
    CHECK((N - finalized2) < epsilon);

    return true;
}
END_TEST(testExternalStrings)
