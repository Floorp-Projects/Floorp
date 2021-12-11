/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Span.h"

#include <string_view>

#include "builtin/intl/DecimalNumber.h"
#include "jsapi-tests/tests.h"

using namespace js::intl;

static mozilla::Maybe<js::intl::DecimalNumber> DecimalFrom(
    std::string_view sv) {
  auto span = mozilla::Span(reinterpret_cast<const JS::Latin1Char*>(sv.data()),
                            sv.length());
  return DecimalNumber::from(span);
}

BEGIN_TEST(test_DecimalNumberParse) {
  struct TestData {
    std::string_view string;
    bool success;
    bool zero;
    bool negative;
    bool exponentTooLarge;
    int32_t exponent;
    size_t significandStart;
    size_t significandEnd;
  } tests[] = {
      // "Infinity" and "NaN" aren't accepted.
      {"Infinity", false},
      {"+Infinity", false},
      {"-Infinity", false},
      {"NaN", false},
      {"+NaN", false},
      {"-NaN", false},

      // Non-decimal strings aren't accepted.
      {"0xbad", false},
      {"0Xbad", false},
      {"0o0", false},
      {"0O0", false},
      {"0b0", false},
      {"0B0", false},

      // Doesn't start with sign, digit, or decimal point.
      {"e0", false},
      {"Hello", false},

      // Incomplete number strings.

      // 1. Missing digit after sign.
      {"+", false},
      {"+ ", false},
      {"+e0", false},
      {"-", false},
      {"- ", false},
      {"-e0", false},

      // 2. Missing digit when starting with decimal point.
      {".", false},
      {". ", false},
      {".e0", false},
      {"+.", false},
      {"+. ", false},
      {"+.e0", false},
      {"-.", false},
      {"-. ", false},
      {"-.e0", false},

      // 3. Missing digit after exponent.
      {"1e", false},
      {"1e ", false},
      {"1e+", false},
      {"1e+ ", false},
      {"1e-", false},
      {"1e- ", false},

      // Empty and whitespace-only strings parse as zero.
      {"", true, true, false, false, 0, 0, 0},
      {"    ", true, true, false, false, 0, 0, 0},
      {"  \t  ", true, true, false, false, 0, 0, 0},

      // Positive zero.
      {"0", true, true, false, false, 0, 0, 0},
      {"0000", true, true, false, false, 0, 0, 0},
      {"0.0", true, true, false, false, 0, 0, 0},
      {"0.", true, true, false, false, 0, 0, 0},
      {".0", true, true, false, false, 0, 0, 0},
      {"0e0", true, true, false, false, 0, 0, 0},
      {"0e+1", true, true, false, false, 0, 0, 0},
      {"0e-1", true, true, false, false, 0, 0, 0},

      {"+0", true, true, false, false, 0, 0, 0},
      {"+0000", true, true, false, false, 0, 0, 0},
      {"+0.0", true, true, false, false, 0, 0, 0},
      {"+0.", true, true, false, false, 0, 0, 0},
      {"+.0", true, true, false, false, 0, 0, 0},
      {"+0e0", true, true, false, false, 0, 0, 0},
      {"+0e+1", true, true, false, false, 0, 0, 0},
      {"+0e-1", true, true, false, false, 0, 0, 0},

      // Negative zero.
      {"-0", true, true, true, false, 0, 0, 0},
      {"-0000", true, true, true, false, 0, 0, 0},
      {"-0.0", true, true, true, false, 0, 0, 0},
      {"-0.", true, true, true, false, 0, 0, 0},
      {"-.0", true, true, true, false, 0, 0, 0},
      {"-0e0", true, true, true, false, 0, 0, 0},
      {"-0e+1", true, true, true, false, 0, 0, 0},
      {"-0e-1", true, true, true, false, 0, 0, 0},

      // Integer numbers.
      {"1", true, false, false, false, 1, 0, 1},
      {"10", true, false, false, false, 2, 0, 2},
      {"123", true, false, false, false, 3, 0, 3},
      {"123456789012345678901234567890", true, false, false, false, 30, 0, 30},
      {"1e100", true, false, false, false, 101, 0, 1},
      {"123e39", true, false, false, false, 42, 0, 3},

      {"+1", true, false, false, false, 1, 1, 2},
      {"+10", true, false, false, false, 2, 1, 3},
      {"+123", true, false, false, false, 3, 1, 4},
      {"+123456789012345678901234567890", true, false, false, false, 30, 1, 31},
      {"+1e100", true, false, false, false, 101, 1, 2},
      {"+123e39", true, false, false, false, 42, 1, 4},

      {"-1", true, false, true, false, 1, 1, 2},
      {"-10", true, false, true, false, 2, 1, 3},
      {"-123", true, false, true, false, 3, 1, 4},
      {"-123456789012345678901234567890", true, false, true, false, 30, 1, 31},
      {"-1e100", true, false, true, false, 101, 1, 2},
      {"-123e39", true, false, true, false, 42, 1, 4},

      // Fractional numbers.
      {"0.1", true, false, false, false, 0, 2, 3},
      {"0.01", true, false, false, false, -1, 3, 4},
      {"0.001", true, false, false, false, -2, 4, 5},
      {"1.001", true, false, false, false, 1, 0, 5},

      {".1", true, false, false, false, 0, 1, 2},
      {".01", true, false, false, false, -1, 2, 3},
      {".001", true, false, false, false, -2, 3, 4},

      {"+.1", true, false, false, false, 0, 2, 3},
      {"+.01", true, false, false, false, -1, 3, 4},
      {"+.001", true, false, false, false, -2, 4, 5},

      {"-.1", true, false, true, false, 0, 2, 3},
      {"-.01", true, false, true, false, -1, 3, 4},
      {"-.001", true, false, true, false, -2, 4, 5},

      // Fractional number with exponent part.
      {".1e0", true, false, false, false, 0, 1, 2},
      {".01e0", true, false, false, false, -1, 2, 3},
      {".001e0", true, false, false, false, -2, 3, 4},

      {".1e1", true, false, false, false, 1, 1, 2},
      {".01e1", true, false, false, false, 0, 2, 3},
      {".001e1", true, false, false, false, -1, 3, 4},

      {".1e-1", true, false, false, false, -1, 1, 2},
      {".01e-1", true, false, false, false, -2, 2, 3},
      {".001e-1", true, false, false, false, -3, 3, 4},

      {"1.1e0", true, false, false, false, 1, 0, 3},
      {"1.01e0", true, false, false, false, 1, 0, 4},
      {"1.001e0", true, false, false, false, 1, 0, 5},

      {"1.1e1", true, false, false, false, 2, 0, 3},
      {"1.01e1", true, false, false, false, 2, 0, 4},
      {"1.001e1", true, false, false, false, 2, 0, 5},

      {"1.1e-1", true, false, false, false, 0, 0, 3},
      {"1.01e-1", true, false, false, false, 0, 0, 4},
      {"1.001e-1", true, false, false, false, 0, 0, 5},

      // Exponent too large.
      //
      // The exponent is limited to: |abs(exp) < 2147483648|.
      {".1e2147483647", true, false, false, false, 2147483647, 1, 2},
      {".1e-2147483647", true, false, false, false, -2147483647, 1, 2},
      {"1e2147483646", true, false, false, false, 2147483647, 0, 1},
      {".01e-2147483646", true, false, false, false, -2147483647, 2, 3},
      {"1e2147483647", true, false, false, true, 0, 0, 1},
      {".1e2147483648", true, false, false, true, 0, 1, 2},
      {".1e-2147483648", true, false, false, true, 0, 1, 2},
      {".01e-2147483647", true, false, false, true, 0, 2, 3},
  };

  for (const auto& test : tests) {
    if (auto decimal = DecimalFrom(test.string)) {
      CHECK(test.success);
      CHECK_EQUAL(decimal->isZero(), test.zero);
      CHECK_EQUAL(decimal->isNegative(), test.negative);
      CHECK_EQUAL(decimal->exponentTooLarge(), test.exponentTooLarge);
      CHECK_EQUAL(decimal->exponent(), test.exponent);
      CHECK_EQUAL(decimal->significandStart(), test.significandStart);
      CHECK_EQUAL(decimal->significandEnd(), test.significandEnd);
    } else {
      CHECK(!test.success);
    }
  }

  return true;
}
END_TEST(test_DecimalNumberParse)

static int32_t NormalizeResult(int32_t r) { return r < 0 ? -1 : r > 0 ? 1 : 0; }

BEGIN_TEST(test_DecimalNumberCompareTo) {
  struct TestData {
    std::string_view left;
    std::string_view right;
    int32_t result;
  } tests[] = {
      // Compare zeros.
      {"0", "0", 0},
      {"-0", "-0", 0},
      {"-0", "+0", -1},
      {"+0", "-0", 1},

      // Compare with different signs.
      {"-1", "+1", -1},
      {"+1", "-1", 1},

      // Compare zero against non-zero.
      {"0", "+1", -1},
      {"0", "-1", 1},
      {"+1", "0", 1},
      {"-1", "0", -1},

      // Compare with different exponent.
      {"1", "0.1", 1},
      {"1e2", "1000", -1},
      {"-1", "-0.1", -1},
      {"-1e2", "-1000", 1},

      // Compare with same exponent.
      {"1", "2", -1},
      {"1", "1", 0},
      {"2", "1", 1},

      // ToString(1e10) is "10000000000".
      {"1e10", "10000000000", 0},
      {"1e10", "10000000001", -1},
      {"-1e10", "-10000000001", 1},

      // ToString(1e30) is "1e+30".
      {"1e30", "1000000000000000000000000000000", 0},
      {"1e30", "1000000000000000000000000000001", -1},
      {"-1e30", "-1000000000000000000000000000001", 1},

      // ToString(1e-5) is "0.00001".
      {"1e-5", "0.00001", 0},
      {"1e-5", "0.00002", -1},
      {"-1e-5", "-0.00002", 1},

      // ToString(1e-10) is "1e-10".
      {"1e-10", "0.0000000001", 0},
      {"1e-10", "0.0000000002", -1},
      {"-1e-10", "-0.0000000002", 1},

      // Additional tests with exponent forms.
      {"1.23e5", "123000", 0},
      {".123e-3", "0.000123", 0},
      {".123e+5", "12300", 0},
      {".00123e5", "123", 0},
  };

  for (const auto& test : tests) {
    auto left = DecimalFrom(test.left);
    CHECK(left);

    auto right = DecimalFrom(test.right);
    CHECK(right);

    int32_t result = left->compareTo(*right);
    CHECK_EQUAL(NormalizeResult(result), test.result);
  }

  return true;
}
END_TEST(test_DecimalNumberCompareTo)
