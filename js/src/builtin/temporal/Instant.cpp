/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Instant.h"

#include "mozilla/Assertions.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "gc/AllocKind.h"
#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/TypeDecls.h"
#include "vm/BigIntType.h"
#include "vm/GlobalObject.h"
#include "vm/PlainObject.h"

using namespace js;
using namespace js::temporal;

/**
 * Check if the absolute value is less-or-equal to the given limit.
 */
template <const auto& digits>
static bool AbsoluteValueIsLessOrEqual(const BigInt* bigInt) {
  size_t length = bigInt->digitLength();

  // Fewer digits than the limit, so definitely in range.
  if (length < std::size(digits)) {
    return true;
  }

  // More digits than the limit, so definitely out of range.
  if (length > std::size(digits)) {
    return false;
  }

  // Compare each digit when the input has the same number of digits.
  size_t index = std::size(digits);
  for (auto digit : digits) {
    auto d = bigInt->digit(--index);
    if (d < digit) {
      return true;
    }
    if (d > digit) {
      return false;
    }
  }
  return true;
}

static constexpr auto NanosecondsMaxInstant() {
  static_assert(BigInt::DigitBits == 64 || BigInt::DigitBits == 32);

  // ±8.64 × 10^21 is the nanoseconds from epoch limit.
  // 8.64 × 10^21 is 86_40000_00000_00000_00000 or 0x1d4_60162f51_6f000000.
  // Return the BigInt digits of that number for fast BigInt comparisons.
  if constexpr (BigInt::DigitBits == 64) {
    return std::array{
        BigInt::Digit(0x1d4),
        BigInt::Digit(0x6016'2f51'6f00'0000),
    };
  } else {
    return std::array{
        BigInt::Digit(0x1d4),
        BigInt::Digit(0x6016'2f51),
        BigInt::Digit(0x6f00'0000),
    };
  }
}

// The epoch limit is 8.64 × 10^21 nanoseconds, which is 8.64 × 10^12 seconds.
static constexpr int64_t SecondsMaxInstant = 8'640'000'000'000;

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool js::temporal::IsValidEpochNanoseconds(const BigInt* epochNanoseconds) {
  // Steps 1-3.
  static constexpr auto epochLimit = NanosecondsMaxInstant();
  return AbsoluteValueIsLessOrEqual<epochLimit>(epochNanoseconds);
}

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool js::temporal::IsValidEpochInstant(const Instant& instant) {
  MOZ_ASSERT(0 <= instant.nanoseconds && instant.nanoseconds <= 999'999'999);

  // Steps 1-3.
  if (instant.seconds < SecondsMaxInstant) {
    return instant.seconds >= -SecondsMaxInstant;
  }
  return instant.seconds == SecondsMaxInstant && instant.nanoseconds == 0;
}

static constexpr auto NanosecondsMaxInstantDifference() {
  static_assert(BigInt::DigitBits == 64 || BigInt::DigitBits == 32);

  // ±8.64 × 10^21 is the nanoseconds from epoch limit.
  // 2 × 8.64 × 10^21 is 172_80000_00000_00000_00000 or 0x3a8_c02c5ea2_de000000.
  // Return the BigInt digits of that number for fast BigInt comparisons.
  if constexpr (BigInt::DigitBits == 64) {
    return std::array{
        BigInt::Digit(0x3a8),
        BigInt::Digit(0xc02c'5ea2'de00'0000),
    };
  } else {
    return std::array{
        BigInt::Digit(0x3a8),
        BigInt::Digit(0xc02c'5ea2),
        BigInt::Digit(0xde00'0000),
    };
  }
}

/**
 * Validates a nanoseconds amount is at most as large as the difference
 * between two valid nanoseconds from the epoch instants.
 *
 * Useful when we want to ensure a BigInt doesn't exceed a certain limit.
 */
bool js::temporal::IsValidInstantDifference(const BigInt* ns) {
  static constexpr auto differenceLimit = NanosecondsMaxInstantDifference();
  return AbsoluteValueIsLessOrEqual<differenceLimit>(ns);
}

bool js::temporal::IsValidInstantDifference(const Instant& instant) {
  MOZ_ASSERT(0 <= instant.nanoseconds && instant.nanoseconds <= 999'999'999);

  constexpr int64_t differenceLimit = SecondsMaxInstant * 2;

  // Steps 1-3.
  if (instant.seconds < differenceLimit) {
    return instant.seconds >= -differenceLimit;
  }
  return instant.seconds == differenceLimit && instant.nanoseconds == 0;
}

/**
 * Return the BigInt digits of the input as uint32_t values. The BigInt digits
 * mustn't consist of more than three uint32_t values.
 */
static std::array<uint32_t, 3> BigIntDigits(const BigInt* ns) {
  static_assert(BigInt::DigitBits == 64 || BigInt::DigitBits == 32);

  auto digits = ns->digits();
  if constexpr (BigInt::DigitBits == 64) {
    BigInt::Digit x = 0, y = 0;
    switch (digits.size()) {
      case 2:
        y = digits[1];
        [[fallthrough]];
      case 1:
        x = digits[0];
        [[fallthrough]];
      case 0:
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected digit length");
    }
    return {uint32_t(x), uint32_t(x >> 32), uint32_t(y)};
  } else {
    BigInt::Digit x = 0, y = 0, z = 0;
    switch (digits.size()) {
      case 3:
        z = digits[2];
        [[fallthrough]];
      case 2:
        y = digits[1];
        [[fallthrough]];
      case 1:
        x = digits[0];
        [[fallthrough]];
      case 0:
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected digit length");
    }
    return {uint32_t(x), uint32_t(y), uint32_t(z)};
  }
}

/**
 * Return the Instant from the input digits. The least significant digit of the
 * input is stored at index 0. The most significant digit of the input must be
 * less than 1'000'000'000.
 */
static Instant ToInstant(std::array<uint32_t, 3> digits, bool isNegative) {
  constexpr uint32_t divisor = ToNanoseconds(TemporalUnit::Second);

  MOZ_ASSERT(digits[2] < divisor);

  uint32_t quotient[2] = {};
  uint32_t remainder = digits[2];
  for (int32_t i = 1; i >= 0; i--) {
    uint64_t n = (uint64_t(remainder) << 32) | digits[i];
    quotient[i] = n / divisor;
    remainder = n % divisor;
  }

  int64_t seconds = (uint64_t(quotient[1]) << 32) | quotient[0];
  if (isNegative) {
    seconds *= -1;
    if (remainder != 0) {
      seconds -= 1;
      remainder = divisor - remainder;
    }
  }
  return {seconds, int32_t(remainder)};
}

Instant js::temporal::ToInstant(const BigInt* epochNanoseconds) {
  MOZ_ASSERT(IsValidEpochNanoseconds(epochNanoseconds));

  auto digits = BigIntDigits(epochNanoseconds);
  return ::ToInstant(digits, epochNanoseconds->isNegative());
}

Instant js::temporal::ToInstantDifference(const BigInt* epochNanoseconds) {
  MOZ_ASSERT(IsValidInstantDifference(epochNanoseconds));

  auto digits = BigIntDigits(epochNanoseconds);
  return ::ToInstant(digits, epochNanoseconds->isNegative());
}

static BigInt* CreateBigInt(JSContext* cx,
                            const std::array<uint32_t, 3>& digits,
                            bool negative) {
  static_assert(BigInt::DigitBits == 64 || BigInt::DigitBits == 32);

  if constexpr (BigInt::DigitBits == 64) {
    uint64_t x = (uint64_t(digits[1]) << 32) | digits[0];
    uint64_t y = digits[2];

    size_t length = y ? 2 : x ? 1 : 0;
    auto* result = BigInt::createUninitialized(cx, length, negative);
    if (!result) {
      return nullptr;
    }
    if (y) {
      result->setDigit(1, y);
    }
    if (x) {
      result->setDigit(0, x);
    }
    return result;
  } else {
    size_t length = digits[2] ? 3 : digits[1] ? 2 : digits[0] ? 1 : 0;
    auto* result = BigInt::createUninitialized(cx, length, negative);
    if (!result) {
      return nullptr;
    }
    while (length--) {
      result->setDigit(length, digits[length]);
    }
    return result;
  }
}

static BigInt* ToEpochBigInt(JSContext* cx, const Instant& instant) {
  MOZ_ASSERT(IsValidInstantDifference(instant));

  // Multiplies two uint32_t values and returns the lower 32-bits. The higher
  // 32-bits are stored in |high|.
  auto digitMul = [](uint32_t a, uint32_t b, uint32_t* high) {
    uint64_t result = static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
    *high = result >> 32;
    return static_cast<uint32_t>(result);
  };

  // Adds two uint32_t values and returns the result. Overflow is added to the
  // out-param |carry|.
  auto digitAdd = [](uint32_t a, uint32_t b, uint32_t* carry) {
    uint32_t result = a + b;
    *carry += static_cast<uint32_t>(result < a);
    return result;
  };

  constexpr uint32_t secToNanos = ToNanoseconds(TemporalUnit::Second);

  uint64_t seconds = std::abs(instant.seconds);
  uint32_t nanoseconds = instant.nanoseconds;

  // Negative nanoseconds are represented as the difference to 1'000'000'000.
  // Convert these back to their absolute value and adjust the seconds part
  // accordingly.
  //
  // For example the nanoseconds from the epoch value |-1n| is represented as
  // the instant {seconds: -1, nanoseconds: 999'999'999}.
  if (instant.seconds < 0 && nanoseconds != 0) {
    nanoseconds = secToNanos - nanoseconds;
    seconds -= 1;
  }

  // uint32_t digits stored in the same order as BigInt digits, i.e. the least
  // significant digit is stored at index zero.
  std::array<uint32_t, 2> multiplicand = {uint32_t(seconds),
                                          uint32_t(seconds >> 32)};
  std::array<uint32_t, 3> accumulator = {nanoseconds, 0, 0};

  // This code follows the implementation of |BigInt::multiplyAccumulate()|.

  uint32_t carry = 0;
  {
    uint32_t high = 0;
    uint32_t low = digitMul(secToNanos, multiplicand[0], &high);

    uint32_t newCarry = 0;
    accumulator[0] = digitAdd(accumulator[0], low, &newCarry);
    accumulator[1] = digitAdd(high, newCarry, &carry);
  }
  {
    uint32_t high = 0;
    uint32_t low = digitMul(secToNanos, multiplicand[1], &high);

    uint32_t newCarry = 0;
    accumulator[1] = digitAdd(accumulator[1], low, &carry);
    accumulator[2] = digitAdd(high, carry, &newCarry);
    MOZ_ASSERT(newCarry == 0);
  }

  return CreateBigInt(cx, accumulator, instant.seconds < 0);
}

BigInt* js::temporal::ToEpochNanoseconds(JSContext* cx,
                                         const Instant& instant) {
  MOZ_ASSERT(IsValidEpochInstant(instant));
  return ::ToEpochBigInt(cx, instant);
}

BigInt* js::temporal::ToEpochDifferenceNanoseconds(JSContext* cx,
                                                   const Instant& instant) {
  MOZ_ASSERT(IsValidInstantDifference(instant));
  return ::ToEpochBigInt(cx, instant);
}

static bool InstantConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

const JSClass InstantObject::class_ = {
    "Temporal.Instant",
    JSCLASS_HAS_RESERVED_SLOTS(InstantObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Instant),
    JS_NULL_CLASS_OPS,
    &InstantObject::classSpec_,
};

const JSClass& InstantObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec Instant_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec Instant_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec Instant_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec InstantObject::classSpec_ = {
    GenericCreateConstructor<InstantConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<InstantObject>,
    Instant_methods,
    nullptr,
    Instant_prototype_methods,
    Instant_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
