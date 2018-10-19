/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "jsapi-tests/tests.h"

using mozilla::ArrayLength;

// Need to account for string literals including the \0 at the end.
#define STR(x) x, (ArrayLength(x)-1)

static const struct TestTuple {
    /* The string being tested. */
    const char* string;
    /* The number of characters from the string to use. */
    size_t length;
    /* Whether this string is an index. */
    bool isindex;
    /* If it's an index, what index it is.  Ignored if not an index. */
    uint32_t index;
} tests[] = {
    { STR("0"), true, 0 },
    { STR("1"), true, 1 },
    { STR("2"), true, 2 },
    { STR("9"), true, 9 },
    { STR("10"), true, 10 },
    { STR("15"), true, 15 },
    { STR("16"), true, 16 },
    { STR("17"), true, 17 },
    { STR("99"), true, 99 },
    { STR("100"), true, 100 },
    { STR("255"), true, 255 },
    { STR("256"), true, 256 },
    { STR("257"), true, 257 },
    { STR("999"), true, 999 },
    { STR("1000"), true, 1000 },
    { STR("4095"), true, 4095 },
    { STR("4096"), true, 4096 },
    { STR("9999"), true, 9999 },
    { STR("1073741823"), true, 1073741823 },
    { STR("1073741824"), true, 1073741824 },
    { STR("1073741825"), true, 1073741825 },
    { STR("2147483647"), true, 2147483647 },
    { STR("2147483648"), true, 2147483648u },
    { STR("2147483649"), true, 2147483649u },
    { STR("4294967294"), true, 4294967294u },
    { STR("4294967295"), false, 0 },  // Not an array index because need to be able to represent length
    { STR("-1"), false, 0 },
    { STR("abc"), false, 0 },
    { STR(" 0"), false, 0 },
    { STR("0 "), false, 0 },
    // Tests to make sure the passed-in length is taken into account
    { "0 ", 1, true, 0 },
    { "123abc", 3, true, 123 },
    { "123abc", 2, true, 12 },
};

BEGIN_TEST(testStringIsArrayIndex)
{
    for (size_t i = 0, sz = ArrayLength(tests); i < sz; i++) {
        uint32_t index;
        bool isindex = js::StringIsArrayIndex(tests[i].string,
                                              tests[i].length,
                                              &index);
        CHECK_EQUAL(isindex, tests[i].isindex);
        if (isindex) {
            CHECK_EQUAL(index, tests[i].index);
        }
    }

    return true;
}
END_TEST(testStringIsArrayIndex)
