/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JS_HAS_TEMPORAL_API

#  include "mozilla/TextUtils.h"

#  include <array>
#  include <climits>
#  include <limits>
#  include <optional>
#  include <stdint.h>
#  include <utility>

#  include "builtin/temporal/Int128.h"
#  include "jsapi-tests/tests.h"

using Int128 = js::temporal::Int128;
using Uint128 = js::temporal::Uint128;

// Simple Uint128 parser.
template <char... DIGITS>
constexpr Uint128 operator""_u128() {
  static_assert(sizeof...(DIGITS) > 0);

  constexpr auto digits = std::array{DIGITS...};

  constexpr auto isBinaryDigit = [](auto c) {
    return (c >= '0' && c <= '1') || c == '\'';
  };

  constexpr auto isOctalDigit = [](auto c) {
    return (c >= '0' && c <= '7') || c == '\'';
  };

  constexpr auto isDigit = [](auto c) {
    return mozilla::IsAsciiDigit(c) || c == '\'';
  };

  constexpr auto isHexDigit = [](auto c) {
    return mozilla::IsAsciiHexDigit(c) || c == '\'';
  };

  constexpr auto isBinary = [isBinaryDigit](auto zero, auto prefix,
                                            auto... rest) {
    return zero == '0' && (prefix == 'b' || prefix == 'B') &&
           (isBinaryDigit(rest) && ...);
  };

  constexpr auto isHex = [isHexDigit](auto zero, auto prefix, auto... rest) {
    return zero == '0' && (prefix == 'x' || prefix == 'X') &&
           (isHexDigit(rest) && ...);
  };

  constexpr auto binary = [digits]() -> std::optional<Uint128> {
    auto value = Uint128{};
    for (size_t i = 2; i < digits.size(); ++i) {
      auto digit = digits[i];
      if (digit == '\'') {
        continue;
      }

      // Detect overflow.
      if (((value << 1) >> 1) != value) {
        return std::nullopt;
      }
      value = (value << 1) | Uint128{uint64_t(digit - '0')};
    }
    return value;
  };

  constexpr auto octal = [digits]() -> std::optional<Uint128> {
    auto value = Uint128{};
    for (size_t i = 1; i < digits.size(); ++i) {
      auto digit = digits[i];
      if (digit == '\'') {
        continue;
      }

      // Detect overflow.
      if (((value << 3) >> 3) != value) {
        return std::nullopt;
      }
      value = (value << 3) | Uint128{uint64_t(digit - '0')};
    }
    return value;
  };

  constexpr auto decimal = [digits]() -> std::optional<Uint128> {
    auto value = Uint128{};
    for (size_t i = 0; i < digits.size(); ++i) {
      auto digit = digits[i];
      if (digit == '\'') {
        continue;
      }

      // NB: Overflow check not implemented.
      value = (value * Uint128{10}) + Uint128{uint64_t(digit - '0')};
    }
    return value;
  };

  constexpr auto hexadecimal = [digits]() -> std::optional<Uint128> {
    auto value = Uint128{};
    for (size_t i = 2; i < digits.size(); ++i) {
      auto digit = digits[i];
      if (digit == '\'') {
        continue;
      }

      // Detect overflow.
      if (((value << 4) >> 4) != value) {
        return std::nullopt;
      }
      value =
          (value << 4) | Uint128{uint64_t(digit >= 'a'   ? (digit - 'a') + 10
                                          : digit >= 'A' ? (digit - 'A') + 10
                                                         : digit - '0')};
    }
    return value;
  };

  if constexpr (digits.size() > 2 && digits[0] == '0' &&
                !mozilla::IsAsciiDigit(digits[1])) {
    if constexpr (isBinary(DIGITS...)) {
      if constexpr (constexpr auto value = binary()) {
        return *value;
      } else {
        static_assert(false, "binary literal too large");
      }
    } else if constexpr (isHex(DIGITS...)) {
      if constexpr (constexpr auto value = hexadecimal()) {
        return *value;
      } else {
        static_assert(false, "hexadecimal literal too large");
      }
    } else {
      static_assert(false, "invalid prefix literal");
    }
  } else if constexpr (digits.size() > 1 && digits[0] == '0') {
    if constexpr ((isOctalDigit(DIGITS) && ...)) {
      if constexpr (constexpr auto value = octal()) {
        return *value;
      } else {
        static_assert(false, "octal literal too large");
      }
    } else {
      static_assert(false, "invalid octal literal");
    }
  } else if constexpr ((isDigit(DIGITS) && ...)) {
    if constexpr (constexpr auto value = decimal()) {
      return *value;
    } else {
      static_assert(false, "decimal literal too large");
    }
  } else {
    static_assert(false, "invalid literal");
  }
}

template <char... DIGITS>
constexpr Int128 operator""_i128() {
  return Int128{operator""_u128 < DIGITS... > ()};
}

template <typename T, size_t N, size_t... ISeq>
static constexpr auto to_array_impl(const T (&elements)[N],
                                    std::index_sequence<ISeq...>) {
  return std::array<T, N>{{elements[ISeq]...}};
}

// No std::to_array because we don't yet compile with C++20.
template <typename T, size_t N>
static constexpr auto to_array(const T (&elements)[N]) {
  return to_array_impl(elements, std::make_index_sequence<N>{});
}

class ConversionFixture : public JSAPIRuntimeTest {
 public:
  virtual ~ConversionFixture() = default;

  template <typename T, typename U, size_t N>
  bool testConversion(const std::array<U, N>& values);
};

template <typename T, typename U, size_t N>
bool ConversionFixture::testConversion(const std::array<U, N>& values) {
  for (auto v : values) {
    // Conversion to signed int.
    CHECK_EQUAL(int64_t(T{v}), int64_t(v));
    CHECK_EQUAL(int32_t(T{v}), int32_t(v));
    CHECK_EQUAL(int16_t(T{v}), int16_t(v));
    CHECK_EQUAL(int8_t(T{v}), int8_t(v));

    // Conversion to unsigned int.
    CHECK_EQUAL(uint64_t(T{v}), uint64_t(v));
    CHECK_EQUAL(uint32_t(T{v}), uint32_t(v));
    CHECK_EQUAL(uint16_t(T{v}), uint16_t(v));
    CHECK_EQUAL(uint8_t(T{v}), uint8_t(v));

    // Conversion to double.
    CHECK_EQUAL(double(T{v}), double(v));

    // Conversion to bool.
    CHECK_EQUAL(bool(T{v}), bool(v));
  }
  return true;
}

BEGIN_FIXTURE_TEST(ConversionFixture, testInt128_conversion) {
  auto values = to_array<int64_t>({
      INT64_MIN,
      INT64_MIN + 1,
      int64_t(INT32_MIN) - 1,
      INT32_MIN,
      INT32_MIN + 1,
      -1,
      0,
      1,
      INT32_MAX - 1,
      INT32_MAX,
      int64_t(INT32_MAX) + 1,
      INT64_MAX - 1,
      INT64_MAX,
  });

  CHECK(testConversion<Int128>(values));

  return true;
}
END_FIXTURE_TEST(ConversionFixture, testInt128_conversion)

BEGIN_FIXTURE_TEST(ConversionFixture, testUint128_conversion) {
  auto values = to_array<uint64_t>({
      0,
      1,
      UINT32_MAX - 1,
      UINT32_MAX,
      uint64_t(UINT32_MAX) + 1,
      UINT64_MAX - 1,
      UINT64_MAX,
  });

  CHECK(testConversion<Uint128>(values));

  return true;
}
END_FIXTURE_TEST(ConversionFixture, testUint128_conversion)

class OperatorFixture : public JSAPIRuntimeTest {
 public:
  virtual ~OperatorFixture() = default;

  template <typename T, typename U, size_t N>
  bool testOperator(const std::array<U, N>& values);
};

template <typename T, typename U, size_t N>
bool OperatorFixture::testOperator(const std::array<U, N>& values) {
  // Unary operators.
  for (auto x : values) {
    // Sign operators.
    CHECK_EQUAL(U(+T{x}), +x);
    CHECK_EQUAL(U(-T{x}), -x);

    // Bitwise operators.
    CHECK_EQUAL(U(~T{x}), ~x);

    // Increment/Decrement operators.
    auto y = T{x};
    CHECK_EQUAL(U(++y), x + 1);
    CHECK_EQUAL(U(y), x + 1);

    y = T{x};
    CHECK_EQUAL(U(y++), x);
    CHECK_EQUAL(U(y), x + 1);

    y = T{x};
    CHECK_EQUAL(U(--y), x - 1);
    CHECK_EQUAL(U(y), x - 1);

    y = T{x};
    CHECK_EQUAL(U(y--), x);
    CHECK_EQUAL(U(y), x - 1);
  }

  // Binary operators.
  for (auto x : values) {
    for (auto y : values) {
      // Comparison operators.
      CHECK_EQUAL((T{x} == T{y}), (x == y));
      CHECK_EQUAL((T{x} != T{y}), (x != y));
      CHECK_EQUAL((T{x} < T{y}), (x < y));
      CHECK_EQUAL((T{x} <= T{y}), (x <= y));
      CHECK_EQUAL((T{x} > T{y}), (x > y));
      CHECK_EQUAL((T{x} >= T{y}), (x >= y));

      // Add/Sub/Mul operators.
      CHECK_EQUAL(U(T{x} + T{y}), (x + y));
      CHECK_EQUAL(U(T{x} - T{y}), (x - y));
      CHECK_EQUAL(U(T{x} * T{y}), (x * y));

      // Division operators.
      if (y != 0) {
        CHECK_EQUAL(U(T{x} / T{y}), (x / y));
        CHECK_EQUAL(U(T{x} % T{y}), (x % y));
      }

      // Shift operators.
      if (y >= 0) {
        CHECK_EQUAL(U(T{x} << y), (x << y));
        CHECK_EQUAL(U(T{x} >> y), (x >> y));
      }

      // Bitwise operators.
      CHECK_EQUAL(U(T{x} & T{y}), (x & y));
      CHECK_EQUAL(U(T{x} | T{y}), (x | y));
      CHECK_EQUAL(U(T{x} ^ T{y}), (x ^ y));
    }
  }

  // Compound assignment operators.
  for (auto x : values) {
    for (auto y : values) {
      auto z = T{x};
      z += T{y};
      CHECK_EQUAL(U(z), x + y);

      z = T{x};
      z -= T{y};
      CHECK_EQUAL(U(z), x - y);

      z = T{x};
      z *= T{y};
      CHECK_EQUAL(U(z), x * y);

      if (y != 0) {
        z = T{x};
        z /= T{y};
        CHECK_EQUAL(U(z), x / y);

        z = T{x};
        z %= T{y};
        CHECK_EQUAL(U(z), x % y);
      }

      if (y >= 0) {
        z = T{x};
        z <<= y;
        CHECK_EQUAL(U(z), x << y);

        z = T{x};
        z >>= y;
        CHECK_EQUAL(U(z), x >> y);
      }

      z = T{x};
      z &= T{y};
      CHECK_EQUAL(U(z), x & y);

      z = T{x};
      z |= T{y};
      CHECK_EQUAL(U(z), x | y);

      z = T{x};
      z ^= T{y};
      CHECK_EQUAL(U(z), x ^ y);
    }
  }
  return true;
}

BEGIN_FIXTURE_TEST(OperatorFixture, testInt128_operator) {
  auto values = to_array<int64_t>({
      -3,
      -2,
      -1,
      0,
      1,
      2,
      3,
      63,
  });

  CHECK(testOperator<Int128>(values));

  // Values larger than INT64_MAX.
  CHECK((Int128{INT64_MAX} * Int128{2}) ==
        (Int128{INT64_MAX} + Int128{INT64_MAX}));
  CHECK((Int128{INT64_MAX} * Int128{3}) ==
        (Int128{INT64_MAX} * Int128{4} - Int128{INT64_MAX}));
  CHECK((Int128{INT64_MAX} * Int128{2}) == (Int128{INT64_MAX} << 1));
  CHECK((Int128{INT64_MAX} * Int128{8}) == (Int128{INT64_MAX} << 3));
  CHECK((Int128{INT64_MAX} * Int128{8} / Int128{2}) ==
        (Int128{INT64_MAX} << 2));
  CHECK((Int128{INT64_MAX} * Int128{23} % Int128{13}) == (Int128{5}));

  // Values smaller than INT64_MIN.
  CHECK((Int128{INT64_MIN} * Int128{2}) ==
        (Int128{INT64_MIN} + Int128{INT64_MIN}));
  CHECK((Int128{INT64_MIN} * Int128{3}) ==
        (Int128{INT64_MIN} * Int128{4} - Int128{INT64_MIN}));
  CHECK((Int128{INT64_MIN} * Int128{2}) == (Int128{INT64_MIN} << 1));
  CHECK((Int128{INT64_MIN} * Int128{8}) == (Int128{INT64_MIN} << 3));
  CHECK((Int128{INT64_MIN} * Int128{8} / Int128{2}) ==
        (Int128{INT64_MIN} << 2));
  CHECK((Int128{INT64_MIN} * Int128{23} % Int128{13}) == (Int128{-2}));

  return true;
}
END_FIXTURE_TEST(OperatorFixture, testInt128_operator)

BEGIN_FIXTURE_TEST(OperatorFixture, testUint128_operator) {
  auto values = to_array<uint64_t>({
      0,
      1,
      2,
      3,
      5,
      63,
  });

  CHECK(testOperator<Uint128>(values));

  // Values larger than UINT64_MAX.
  CHECK((Uint128{UINT64_MAX} * Uint128{2}) ==
        (Uint128{UINT64_MAX} + Uint128{UINT64_MAX}));
  CHECK((Uint128{UINT64_MAX} * Uint128{3}) ==
        (Uint128{UINT64_MAX} * Uint128{4} - Uint128{UINT64_MAX}));
  CHECK((Uint128{UINT64_MAX} * Uint128{2}) == (Uint128{UINT64_MAX} << 1));
  CHECK((Uint128{UINT64_MAX} * Uint128{8}) == (Uint128{UINT64_MAX} << 3));
  CHECK((Uint128{UINT64_MAX} * Uint128{8} / Uint128{2}) ==
        (Uint128{UINT64_MAX} << 2));
  CHECK((Uint128{UINT64_MAX} * Uint128{23} % Uint128{13}) == (Uint128{7}));

  return true;
}
END_FIXTURE_TEST(OperatorFixture, testUint128_operator)

BEGIN_TEST(testInt128_literal) {
  CHECK_EQUAL(int64_t(0x7fff'ffff'ffff'ffff_i128), INT64_MAX);
  CHECK_EQUAL(int64_t(-0x8000'0000'0000'0000_i128), INT64_MIN);

  CHECK(std::numeric_limits<Int128>::max() ==
        0x7fff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_i128);
  CHECK(std::numeric_limits<Int128>::min() ==
        -0x8000'0000'0000'0000'0000'0000'0000'0000_i128);

  auto x = (Int128{INT64_MAX} + Int128{1}) * Int128{3};
  CHECK(x == 27670116110564327424_i128);
  CHECK(x == 0x1'8000'0000'0000'0000_i128);

  auto y = Int128{0} - (Int128{5} * Int128{INT64_MAX});
  CHECK(y == -46116860184273879035_i128);
  CHECK(y == -0x2'7fff'ffff'ffff'fffb_i128);

  // NB: This shift expression overflows.
  auto z = Int128{0x1122'3344} << 100;
  CHECK(z == 0x1223'3440'0000'0000'0000'0000'0000'0000_i128);
  CHECK(z == 0221063210000000000000000000000000000000000_i128);
  CHECK(z == 24108894070078995479046745700448600064_i128);
  CHECK(
      z ==
      0b10010001000110011010001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000_i128);

  z >>= 80;
  CHECK(z == 0X1223'3440'0000_i128);
  CHECK(z == 0442146420000000_i128);
  CHECK(z == 19942409764864_i128);
  CHECK(z == 0B100100010001100110100010000000000000000000000_i128);

  auto v = Int128{INT64_MAX} * Int128{INT64_MAX};
  CHECK(v == 0x3fff'ffff'ffff'ffff'0000'0000'0000'0001_i128);
  CHECK((v + v) == 0x7fff'ffff'ffff'fffe'0000'0000'0000'0002_i128);
  CHECK((v * v) == 0x7fff'ffff'ffff'fffe'0000'0000'0000'0001_i128);
  CHECK((v * -v) == -0x7fff'ffff'ffff'fffe'0000'0000'0000'0001_i128);
  CHECK((-v * v) == -0x7fff'ffff'ffff'fffe'0000'0000'0000'0001_i128);
  CHECK((-v * -v) == 0x7fff'ffff'ffff'fffe'0000'0000'0000'0001_i128);

  auto w = Int128{INT64_MIN} * Int128{INT64_MIN};
  CHECK(w == 0x4000'0000'0000'0000'0000'0000'0000'0000_i128);
  CHECK((w + w) == -0x8000'0000'0000'0000'0000'0000'0000'0000_i128);
  CHECK((w * w) == 0_i128);

  CHECK((Int128{1} << 120) == 0x100'0000'0000'0000'0000'0000'0000'0000_i128);

  return true;
}
END_TEST(testInt128_literal)

BEGIN_TEST(testUint128_literal) {
  CHECK_EQUAL(uint64_t(0xffff'ffff'ffff'ffff_u128), UINT64_MAX);

  CHECK(std::numeric_limits<Uint128>::max() ==
        0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_u128);

  auto x = (Uint128{UINT64_MAX} + Uint128{3}) * Uint128{3};
  CHECK(x == 55340232221128654854_u128);
  CHECK(x == 0x3'0000'0000'0000'0006_u128);

  // NB: This shift expression overflows.
  auto z = Uint128{0x1122'3344} << 100;
  CHECK(z == 0x1223'3440'0000'0000'0000'0000'0000'0000_u128);
  CHECK(z == 0221063210000000000000000000000000000000000_u128);
  CHECK(z == 24108894070078995479046745700448600064_u128);
  CHECK(
      z ==
      0b10010001000110011010001000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000_u128);

  z >>= 80;
  CHECK(z == 0X1223'3440'0000_u128);
  CHECK(z == 0442146420000000_u128);
  CHECK(z == 19942409764864_u128);
  CHECK(z == 0B100100010001100110100010000000000000000000000_u128);

  auto v = Uint128{UINT64_MAX} * Uint128{UINT64_MAX};
  CHECK(v == 0xffff'ffff'ffff'fffe'0000'0000'0000'0001_u128);
  CHECK((v + v) == 0xffff'ffff'ffff'fffc'0000'0000'0000'0002_u128);
  CHECK((v * v) == 0xffff'ffff'ffff'fffc'0000'0000'0000'0001_u128);
  CHECK((v * -v) == 0x3'ffff'ffff'ffff'ffff_u128);
  CHECK((-v * v) == 0x3'ffff'ffff'ffff'ffff_u128);
  CHECK((-v * -v) == 0xffff'ffff'ffff'fffc'0000'0000'0000'0001_u128);

  CHECK((Uint128{1} << 120) == 0x100'0000'0000'0000'0000'0000'0000'0000_u128);

  return true;
}
END_TEST(testUint128_literal)

BEGIN_TEST(testInt128_division) {
  auto x = Int128{INT64_MAX} * Int128{4};
  CHECK((x / Int128{2}) == 0xffff'ffff'ffff'fffe_i128);
  CHECK((x / Int128{2}) == (x >> 1));

  auto y = Int128{INT64_MAX} * Int128{16};
  CHECK((y / Int128{2}) == 0x3'ffff'ffff'ffff'fff8_i128);
  CHECK((y / Int128{2}) == (y >> 1));

  CHECK((0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128 / 7_i128) ==
        0x272'999c'0c33'35a5'cf3f'6668'db4b'be55_i128);
  CHECK((0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128 /
         0x1'2345'6789'abcd'ef11'abcd'ef11_i128) == 0xf0f0f0f_i128);
  CHECK((7_i128 / 0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128) == 0_i128);

  CHECK((0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128 % 7_i128) == 3_i128);
  CHECK((0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128 %
         0x1'2345'6789'abcd'ef11'abcd'ef11_i128) ==
        0x1122'3353'7d8e'9fb0'dc00'3357_i128);
  CHECK((7_i128 % 0x1122'3344'5566'7788'aabb'ccdd'ff12'3456_i128) == 7_i128);

  return true;
}
END_TEST(testInt128_division)

BEGIN_TEST(testInt128_abs) {
  CHECK((0_i128).abs() == 0_u128);

  CHECK((0x1122'3344_i128).abs() == 0x1122'3344_u128);
  CHECK((-0x1122'3344_i128).abs() == 0x1122'3344_u128);

  CHECK((0x1111'2222'3333'4444'5555'6666'7777'8888_i128).abs() ==
        0x1111'2222'3333'4444'5555'6666'7777'8888_u128);
  CHECK((-0x1111'2222'3333'4444'5555'6666'7777'8888_i128).abs() ==
        0x1111'2222'3333'4444'5555'6666'7777'8888_u128);

  CHECK(std::numeric_limits<Int128>::min().abs() ==
        0x8000'0000'0000'0000'0000'0000'0000'0000_u128);
  CHECK(std::numeric_limits<Int128>::max().abs() ==
        0x7fff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_u128);

  return true;
}
END_TEST(testInt128_abs)

#endif
