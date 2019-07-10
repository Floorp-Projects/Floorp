/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"     // mozilla::ArrayLength
#include "mozilla/FloatingPoint.h"  // mozilla::{PositiveInfinity,UnspecifiedNaN}

#include <stddef.h>  // size_t
#include <string.h>  // memcmp, memset

#include "js/Conversions.h"  // JS::NumberToString, JS::MaximumNumberToStringLength
#include "jsapi-tests/tests.h"  // BEGIN_TEST, CHECK_EQUAL, END_TEST

// Need to account for string literals including the \0 at the end.
#define STR(x) x, (mozilla::ArrayLength(x) - 1)

static const struct NumberToStringTest {
  double number;
  const char* expected;
  size_t expectedLength;
} numberToStringTests[] = {
    {5e-324, STR("5e-324")},                                // 2**-1074
    {9.5367431640625e-7, STR("9.5367431640625e-7")},        // 2**-20
    {0.0000019073486328125, STR("0.0000019073486328125")},  // 2**-19
    {0.000003814697265625, STR("0.000003814697265625")},    // 2**-18
    {0.0000057220458984375, STR("0.0000057220458984375")},  // 2**-18 + 2**-19
    {0.000244140625, STR("0.000244140625")},                // 2**-12
    {0.125, STR("0.125")},
    {0.25, STR("0.25")},
    {0.5, STR("0.5")},
    {1, STR("1")},
    {1.5, STR("1.5")},
    {2, STR("2")},
    {9, STR("9")},
    {10, STR("10")},
    {15, STR("15")},
    {16, STR("16")},
    {389427, STR("389427")},
    {1073741823, STR("1073741823")},
    {1073741824, STR("1073741824")},
    {1073741825, STR("1073741825")},
    {2147483647, STR("2147483647")},
    {2147483648, STR("2147483648")},
    {2147483649, STR("2147483649")},
    {4294967294, STR("4294967294")},
    {4294967295, STR("4294967295")},
    {999999999999999900000.0, STR("999999999999999900000")},
    {999999999999999900000.0 + 65535, STR("999999999999999900000")},
    {999999999999999900000.0 + 65536, STR("1e+21")},
    {1.7976931348623157e+308, STR("1.7976931348623157e+308")},
};

static constexpr char PoisonChar = 0x37;

struct StorageForNumberToString {
  char out[JS::MaximumNumberToStringLength];
  char overflow;
} storage;

BEGIN_TEST(testNumberToString) {
  StorageForNumberToString storage;

  if (!testNormalValues(false, storage)) {
    return false;
  }

  if (!testNormalValues(true, storage)) {
    return false;
  }

  NumberToStringTest zeroTest = {0.0, STR("0")};
  if (!testOne(zeroTest, false, storage)) {
    return false;
  }
  NumberToStringTest negativeZeroTest = {-0.0, STR("0")};
  if (!testOne(negativeZeroTest, false, storage)) {
    return false;
  }

  NumberToStringTest infTest = {mozilla::PositiveInfinity<double>(),
                                STR("Infinity")};
  if (!testOne(infTest, false, storage)) {
    return false;
  }
  if (!testOne(infTest, true, storage)) {
    return false;
  }

  NumberToStringTest nanTest = {mozilla::UnspecifiedNaN<double>(), STR("NaN")};
  return testOne(nanTest, false, storage);
}

bool testNormalValues(bool hasMinusSign, StorageForNumberToString& storage) {
  for (const auto& test : numberToStringTests) {
    if (!testOne(test, hasMinusSign, storage)) {
      return false;
    }
  }

  return true;
}

bool testOne(const NumberToStringTest& test, bool hasMinusSign,
             StorageForNumberToString& storage) {
  memset(&storage, PoisonChar, sizeof(storage));

  JS::NumberToString(hasMinusSign ? -test.number : test.number, storage.out);

  CHECK_EQUAL(storage.overflow, PoisonChar);

  const char* start = storage.out;
  if (hasMinusSign) {
    CHECK_EQUAL(start[0], '-');
    start++;
  }

  CHECK_EQUAL(memcmp(start, test.expected, test.expectedLength), 0);
  CHECK_EQUAL(start[test.expectedLength], '\0');

  return true;
}
END_TEST(testNumberToString)

#undef STR
