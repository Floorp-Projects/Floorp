/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "js/Conversions.h"

#include "jsapi-tests/tests.h"

using JS::ToSignedInteger;
using JS::ToUnsignedInteger;

BEGIN_TEST(testToUint8TwiceUint8Range)
{
    double d = -256;
    uint8_t expected = 0;
    do {
        CHECK(ToUnsignedInteger<uint8_t>(d) == expected);

        d++;
        expected++;
    } while (d <= 256);
    return true;
}
END_TEST(testToUint8TwiceUint8Range)

BEGIN_TEST(testToInt8)
{
    double d = -128;
    int8_t expected = -128;
    do {
        CHECK(ToSignedInteger<int8_t>(d) == expected);

        d++;
        expected++;
    } while (expected < 127);
    return true;
}
END_TEST(testToInt8)

BEGIN_TEST(testToUint32Large)
{
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 83)) == 0);
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 83) + pow(2.0, 31)) == (1U << 31));
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 83) + 2 * pow(2.0, 31)) == 0);
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 83) + 3 * pow(2.0, 31)) == (1U << 31));
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 84)) == 0);
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 84) + pow(2.0, 31)) == 0);
    CHECK(ToUnsignedInteger<uint32_t>(pow(2.0, 84) + pow(2.0, 32)) == 0);
    return true;
}
END_TEST(testToUint32Large)

BEGIN_TEST(testToUint64Large)
{
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 115)) == 0);
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 115) + pow(2.0, 63)) == (1ULL << 63));
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 115) + 2 * pow(2.0, 63)) == 0);
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 115) + 3 * pow(2.0, 63)) == (1ULL << 63));
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 116)) == 0);
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 116) + pow(2.0, 63)) == 0);
    CHECK(ToUnsignedInteger<uint64_t>(pow(2.0, 116) + pow(2.0, 64)) == 0);
    return true;
}
END_TEST(testToUint64Large)
