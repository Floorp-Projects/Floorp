/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IntegerPrintfMacros.h"

#include <cfloat>
#include <stdarg.h>

#include "jsprf.h"

#include "jsapi-tests/tests.h"

static bool
MOZ_FORMAT_PRINTF(2, 3)
print_one (const char *expect, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    JS::UniqueChars output = JS_vsmprintf (fmt, ap);
    va_end(ap);

    return output && !strcmp(output.get(), expect);
}

static const char *
zero()
{
    return nullptr;
}

BEGIN_TEST(testPrintf)
{
    CHECK(print_one("23", "%d", 23));
    CHECK(print_one("-1", "%d", -1));
    CHECK(print_one("23", "%u", 23u));
    CHECK(print_one("0x17", "0x%x", 23u));
    CHECK(print_one("0xFF", "0x%X", 255u));
    CHECK(print_one("027", "0%o", 23u));
    CHECK(print_one("-1", "%hd", (short) -1));
    // This could be expanded if need be, it's just convenient to do
    // it this way.
    if (sizeof(short) == 2) {
        CHECK(print_one("8000", "%hx", (unsigned short) 0x8000));
    }
    CHECK(print_one("0xf0f0", "0x%lx", 0xf0f0ul));
    CHECK(print_one("0xF0F0", "0x%llX", 0xf0f0ull));
    CHECK(print_one("27270", "%zu", (size_t) 27270));
    CHECK(print_one("27270", "%zu", (size_t) 27270));
    CHECK(print_one("hello", "he%so", "ll"));
    CHECK(print_one("(null)", "%s", zero()));
    CHECK(print_one("0", "%p", (char *) 0));
    CHECK(print_one("h", "%c", 'h'));
    CHECK(print_one("1.500000", "%f", 1.5f));
    CHECK(print_one("1.5", "%g", 1.5));

    // Regression test for bug#1350097.  The bug was an assertion
    // failure caused by printing a very long floating point value.
    print_one("ignore", "%lf", DBL_MAX);

    CHECK(print_one("2727", "%" PRIu32, (uint32_t) 2727));
    CHECK(print_one("aa7", "%" PRIx32, (uint32_t) 2727));
    CHECK(print_one("2727", "%" PRIu64, (uint64_t) 2727));
    CHECK(print_one("aa7", "%" PRIx64, (uint64_t) 2727));

    return true;
}
END_TEST(testPrintf)
