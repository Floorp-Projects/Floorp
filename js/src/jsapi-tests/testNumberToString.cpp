/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"  // mozilla::{PositiveInfinity,UnspecifiedNaN}

#include <stddef.h>  // size_t
#include <string.h>  // memcmp, memset

#include "js/Conversions.h"  // JS::NumberToString, JS::MaximumNumberToStringLength
#include "jsapi-tests/tests.h"  // BEGIN_TEST, CHECK_EQUAL, END_TEST
#include "util/Text.h"          // js_strlen

#define REST(x) x, (js_strlen(x)), __LINE__

static const struct NumberToStringTest {
  double number;
  const char* expected;
  size_t expectedLength;
  size_t lineno;
} numberToStringTests[] = {
    {5e-324, REST("5e-324")},                          // 2**-1074
    {9.5367431640625e-7, REST("9.5367431640625e-7")},  // 2**-20
    {0.0000010984284297360395, REST("0.0000010984284297360395")},
    {0.0000019073486328125, REST("0.0000019073486328125")},  // 2**-19
    {0.000003814697265625, REST("0.000003814697265625")},    // 2**-18
    {0.0000057220458984375, REST("0.0000057220458984375")},  // 2**-18 + 2**-19
    {0.000244140625, REST("0.000244140625")},                // 2**-12
    {0.125, REST("0.125")},
    {0.25, REST("0.25")},
    {0.5, REST("0.5")},
    {1, REST("1")},
    {1.5, REST("1.5")},
    {2, REST("2")},
    {9, REST("9")},
    {10, REST("10")},
    {15, REST("15")},
    {16, REST("16")},
    {389427, REST("389427")},
    {1073741823, REST("1073741823")},
    {1073741824, REST("1073741824")},
    {1073741825, REST("1073741825")},
    {2147483647, REST("2147483647")},
    {2147483648, REST("2147483648")},
    {2147483649, REST("2147483649")},
    {4294967294, REST("4294967294")},
    {4294967295, REST("4294967295")},
    {4294967296, REST("4294967296")},
    {999999999999999900000.0, REST("999999999999999900000")},
    {999999999999999900000.0 + 65535, REST("999999999999999900000")},
    {999999999999999900000.0 + 65536, REST("1e+21")},
    {1.7976931348623157e+308, REST("1.7976931348623157e+308")},  // MAX_VALUE
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

  NumberToStringTest zeroTest = {0.0, REST("0")};
  if (!testOne(zeroTest, false, storage)) {
    return false;
  }
  NumberToStringTest negativeZeroTest = {-0.0, REST("0")};
  if (!testOne(negativeZeroTest, false, storage)) {
    return false;
  }

  NumberToStringTest infTest = {mozilla::PositiveInfinity<double>(),
                                REST("Infinity")};
  if (!testOne(infTest, false, storage)) {
    return false;
  }
  if (!testOne(infTest, true, storage)) {
    return false;
  }

  NumberToStringTest nanTest = {mozilla::UnspecifiedNaN<double>(), REST("NaN")};
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

  if (!checkEqual(memcmp(start, test.expected, test.expectedLength), 0, start,
                  test.expected, __FILE__, test.lineno)) {
    return false;
  }

  char actualTerminator[] = {start[test.expectedLength], '\0'};
  return checkEqual(actualTerminator[0], '\0', actualTerminator, "'\\0'",
                    __FILE__, test.lineno);
}
END_TEST(testNumberToString)

#undef REST
