/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Temporal.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <stdint.h>
#include <string_view>
#include <utility>

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Instant.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/TemporalRoundingMode.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/Barrier.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/Value.h"
#include "vm/BigIntType.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSAtomUtils.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/Realm.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

/**
 * GetOption ( options, property, type, values, default )
 *
 * GetOption specialization when `type=string`. Default value handling must
 * happen in the caller, so we don't provide the `default` parameter here.
 */
static bool GetStringOption(JSContext* cx, Handle<JSObject*> options,
                            Handle<PropertyName*> property,
                            MutableHandle<JSString*> string) {
  // Step 1.
  Rooted<Value> value(cx);
  if (!GetProperty(cx, options, options, property, &value)) {
    return false;
  }

  // Step 2. (Caller should fill in the fallback.)
  if (value.isUndefined()) {
    return true;
  }

  // Steps 3-4. (Not applicable when type=string)

  // Step 5.
  string.set(JS::ToString(cx, value));
  if (!string) {
    return false;
  }

  // Step 6. (Not applicable in our implementation)

  // Step 7.
  return true;
}

/**
 * GetOption ( options, property, type, values, default )
 */
static bool GetNumberOption(JSContext* cx, Handle<JSObject*> options,
                            Handle<PropertyName*> property, double* number) {
  // Step 1.
  Rooted<Value> value(cx);
  if (!GetProperty(cx, options, options, property, &value)) {
    return false;
  }

  // Step 2. (Caller should fill in the fallback.)
  if (value.isUndefined()) {
    return true;
  }

  // Steps 3 and 5. (Not applicable in our implementation)

  // Step 4.a.
  if (!JS::ToNumber(cx, value, number)) {
    return false;
  }

  // Step 4.b. (Caller must check for NaN values.)

  // Step 7. (Not applicable in our implementation)

  // Step 8.
  return true;
}

/**
 * ToTemporalRoundingIncrement ( normalizedOptions, dividend, inclusive )
 */
bool js::temporal::ToTemporalRoundingIncrement(JSContext* cx,
                                               Handle<JSObject*> options,
                                               Increment* increment) {
  // Step 1.
  double number = 1;
  if (!GetNumberOption(cx, options, cx->names().roundingIncrement, &number)) {
    return false;
  }

  // Step 3. (Reordered)
  number = std::trunc(number);

  // Steps 2 and 4.
  if (!std::isfinite(number) || number < 1 || number > 1'000'000'000) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, number);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_OPTION_VALUE, "roundingIncrement",
                              numStr);
    return false;
  }

  *increment = Increment{uint32_t(number)};
  return true;
}

/**
 * ValidateTemporalRoundingIncrement ( increment, dividend, inclusive )
 */
bool js::temporal::ValidateTemporalRoundingIncrement(JSContext* cx,
                                                     Increment increment,
                                                     int64_t dividend,
                                                     bool inclusive) {
  MOZ_ASSERT(dividend > 0);
  MOZ_ASSERT_IF(!inclusive, dividend > 1);

  // Steps 1-2.
  int64_t maximum = inclusive ? dividend : dividend - 1;

  // Steps 3-4.
  if (increment.value() > maximum || dividend % increment.value() != 0) {
    Int32ToCStringBuf cbuf;
    const char* numStr = Int32ToCString(&cbuf, increment.value());

    // TODO: Better error message could be helpful.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_OPTION_VALUE, "roundingIncrement",
                              numStr);
    return false;
  }

  // Step 5.
  return true;
}

PropertyName* js::temporal::TemporalUnitToString(JSContext* cx,
                                                 TemporalUnit unit) {
  switch (unit) {
    case TemporalUnit::Auto:
      break;
    case TemporalUnit::Year:
      return cx->names().year;
    case TemporalUnit::Month:
      return cx->names().month;
    case TemporalUnit::Week:
      return cx->names().week;
    case TemporalUnit::Day:
      return cx->names().day;
    case TemporalUnit::Hour:
      return cx->names().hour;
    case TemporalUnit::Minute:
      return cx->names().minute;
    case TemporalUnit::Second:
      return cx->names().second;
    case TemporalUnit::Millisecond:
      return cx->names().millisecond;
    case TemporalUnit::Microsecond:
      return cx->names().microsecond;
    case TemporalUnit::Nanosecond:
      return cx->names().nanosecond;
  }
  MOZ_CRASH("invalid temporal unit");
}

static Handle<PropertyName*> ToPropertyName(JSContext* cx,
                                            TemporalUnitKey key) {
  switch (key) {
    case TemporalUnitKey::SmallestUnit:
      return cx->names().smallestUnit;
    case TemporalUnitKey::LargestUnit:
      return cx->names().largestUnit;
    case TemporalUnitKey::Unit:
      return cx->names().unit;
  }
  MOZ_CRASH("invalid temporal unit group");
}

static const char* ToCString(TemporalUnitKey key) {
  switch (key) {
    case TemporalUnitKey::SmallestUnit:
      return "smallestUnit";
    case TemporalUnitKey::LargestUnit:
      return "largestUnit";
    case TemporalUnitKey::Unit:
      return "unit";
  }
  MOZ_CRASH("invalid temporal unit group");
}

static bool ToTemporalUnit(JSContext* cx, JSLinearString* str,
                           TemporalUnitKey key, TemporalUnit* unit) {
  struct UnitMap {
    std::string_view name;
    TemporalUnit unit;
  };

  static constexpr UnitMap mapping[] = {
      {"year", TemporalUnit::Year},
      {"years", TemporalUnit::Year},
      {"month", TemporalUnit::Month},
      {"months", TemporalUnit::Month},
      {"week", TemporalUnit::Week},
      {"weeks", TemporalUnit::Week},
      {"day", TemporalUnit::Day},
      {"days", TemporalUnit::Day},
      {"hour", TemporalUnit::Hour},
      {"hours", TemporalUnit::Hour},
      {"minute", TemporalUnit::Minute},
      {"minutes", TemporalUnit::Minute},
      {"second", TemporalUnit::Second},
      {"seconds", TemporalUnit::Second},
      {"millisecond", TemporalUnit::Millisecond},
      {"milliseconds", TemporalUnit::Millisecond},
      {"microsecond", TemporalUnit::Microsecond},
      {"microseconds", TemporalUnit::Microsecond},
      {"nanosecond", TemporalUnit::Nanosecond},
      {"nanoseconds", TemporalUnit::Nanosecond},
  };

  // Compute the length of the longest name.
  constexpr size_t maxNameLength =
      std::max_element(std::begin(mapping), std::end(mapping),
                       [](const auto& x, const auto& y) {
                         return x.name.length() < y.name.length();
                       })
          ->name.length();

  // Twenty StringEqualsLiteral calls for each possible combination seems a bit
  // expensive, so let's instead copy the input name into a char array and rely
  // on the compiler to generate optimized code for the comparisons.

  size_t length = str->length();
  if (length <= maxNameLength && StringIsAscii(str)) {
    char chars[maxNameLength] = {};
    JS::LossyCopyLinearStringChars(chars, str, length);

    for (const auto& m : mapping) {
      if (m.name == std::string_view(chars, length)) {
        *unit = m.unit;
        return true;
      }
    }
  }

  if (auto chars = QuoteString(cx, str, '"')) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_INVALID_OPTION_VALUE, ToCString(key),
                             chars.get());
  }
  return false;
}

static std::pair<TemporalUnit, TemporalUnit> AllowedValues(
    TemporalUnitGroup unitGroup) {
  switch (unitGroup) {
    case TemporalUnitGroup::Date:
      return {TemporalUnit::Year, TemporalUnit::Day};
    case TemporalUnitGroup::Time:
      return {TemporalUnit::Hour, TemporalUnit::Nanosecond};
    case TemporalUnitGroup::DateTime:
      return {TemporalUnit::Year, TemporalUnit::Nanosecond};
    case TemporalUnitGroup::DayTime:
      return {TemporalUnit::Day, TemporalUnit::Nanosecond};
  }
  MOZ_CRASH("invalid temporal unit group");
}

/**
 * GetTemporalUnit ( normalizedOptions, key, unitGroup, default [ , extraValues
 * ] )
 */
bool js::temporal::GetTemporalUnit(JSContext* cx, Handle<JSObject*> options,
                                   TemporalUnitKey key,
                                   TemporalUnitGroup unitGroup,
                                   TemporalUnit* unit) {
  // Steps 1-8. (Not applicable in our implementation.)

  // Step 9.
  Rooted<JSString*> value(cx);
  if (!GetStringOption(cx, options, ToPropertyName(cx, key), &value)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!value) {
    return true;
  }

  return GetTemporalUnit(cx, value, key, unitGroup, unit);
}

/**
 * GetTemporalUnit ( normalizedOptions, key, unitGroup, default [ , extraValues
 * ] )
 */
bool js::temporal::GetTemporalUnit(JSContext* cx, Handle<JSString*> value,
                                   TemporalUnitKey key,
                                   TemporalUnitGroup unitGroup,
                                   TemporalUnit* unit) {
  // Steps 1-9. (Not applicable in our implementation.)

  // Step 10. (Handled in caller.)

  Rooted<JSLinearString*> linear(cx, value->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  // Caller should fill in the fallback.
  if (key == TemporalUnitKey::LargestUnit) {
    if (StringEqualsLiteral(linear, "auto")) {
      return true;
    }
  }

  // Step 11.
  if (!ToTemporalUnit(cx, linear, key, unit)) {
    return false;
  }

  auto allowedValues = AllowedValues(unitGroup);
  if (*unit < allowedValues.first || *unit > allowedValues.second) {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_INVALID_OPTION_VALUE, ToCString(key),
                                chars.get());
    }
    return false;
  }

  return true;
}

/**
 * ToTemporalRoundingMode ( normalizedOptions, fallback )
 */
bool js::temporal::ToTemporalRoundingMode(JSContext* cx,
                                          Handle<JSObject*> options,
                                          TemporalRoundingMode* mode) {
  // Step 1.
  Rooted<JSString*> string(cx);
  if (!GetStringOption(cx, options, cx->names().roundingMode, &string)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!string) {
    return true;
  }

  JSLinearString* linear = string->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "ceil")) {
    *mode = TemporalRoundingMode::Ceil;
  } else if (StringEqualsLiteral(linear, "floor")) {
    *mode = TemporalRoundingMode::Floor;
  } else if (StringEqualsLiteral(linear, "expand")) {
    *mode = TemporalRoundingMode::Expand;
  } else if (StringEqualsLiteral(linear, "trunc")) {
    *mode = TemporalRoundingMode::Trunc;
  } else if (StringEqualsLiteral(linear, "halfCeil")) {
    *mode = TemporalRoundingMode::HalfCeil;
  } else if (StringEqualsLiteral(linear, "halfFloor")) {
    *mode = TemporalRoundingMode::HalfFloor;
  } else if (StringEqualsLiteral(linear, "halfExpand")) {
    *mode = TemporalRoundingMode::HalfExpand;
  } else if (StringEqualsLiteral(linear, "halfTrunc")) {
    *mode = TemporalRoundingMode::HalfTrunc;
  } else if (StringEqualsLiteral(linear, "halfEven")) {
    *mode = TemporalRoundingMode::HalfEven;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "roundingMode",
                               chars.get());
    }
    return false;
  }
  return true;
}

static BigInt* Divide(JSContext* cx, Handle<BigInt*> dividend, int64_t divisor,
                      TemporalRoundingMode roundingMode) {
  MOZ_ASSERT(divisor > 0);

  Rooted<BigInt*> div(cx, BigInt::createFromInt64(cx, divisor));
  if (!div) {
    return nullptr;
  }

  Rooted<BigInt*> quotient(cx);
  Rooted<BigInt*> remainder(cx);
  if (!BigInt::divmod(cx, dividend, div, &quotient, &remainder)) {
    return nullptr;
  }

  // No rounding needed when the remainder is zero.
  if (remainder->isZero()) {
    return quotient;
  }

  switch (roundingMode) {
    case TemporalRoundingMode::Ceil: {
      if (!remainder->isNegative()) {
        return BigInt::inc(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::Floor: {
      if (remainder->isNegative()) {
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::Trunc:
      // BigInt division truncates.
      return quotient;
    case TemporalRoundingMode::Expand: {
      if (!remainder->isNegative()) {
        return BigInt::inc(cx, quotient);
      }
      return BigInt::dec(cx, quotient);
    }
    case TemporalRoundingMode::HalfCeil: {
      int64_t rem;
      MOZ_ALWAYS_TRUE(BigInt::isInt64(remainder, &rem));

      if (!remainder->isNegative()) {
        if (uint64_t(std::abs(rem)) * 2 >= uint64_t(divisor)) {
          return BigInt::inc(cx, quotient);
        }
      } else {
        if (uint64_t(std::abs(rem)) * 2 > uint64_t(divisor)) {
          return BigInt::dec(cx, quotient);
        }
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfFloor: {
      int64_t rem;
      MOZ_ALWAYS_TRUE(BigInt::isInt64(remainder, &rem));

      if (remainder->isNegative()) {
        if (uint64_t(std::abs(rem)) * 2 >= uint64_t(divisor)) {
          return BigInt::dec(cx, quotient);
        }
      } else {
        if (uint64_t(std::abs(rem)) * 2 > uint64_t(divisor)) {
          return BigInt::inc(cx, quotient);
        }
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfExpand: {
      int64_t rem;
      MOZ_ALWAYS_TRUE(BigInt::isInt64(remainder, &rem));

      if (uint64_t(std::abs(rem)) * 2 >= uint64_t(divisor)) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfTrunc: {
      int64_t rem;
      MOZ_ALWAYS_TRUE(BigInt::isInt64(remainder, &rem));

      if (uint64_t(std::abs(rem)) * 2 > uint64_t(divisor)) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfEven: {
      int64_t rem;
      MOZ_ALWAYS_TRUE(BigInt::isInt64(remainder, &rem));

      if (uint64_t(std::abs(rem)) * 2 == uint64_t(divisor)) {
        bool isOdd = !quotient->isZero() && (quotient->digit(0) & 1) == 1;
        if (isOdd) {
          if (!dividend->isNegative()) {
            return BigInt::inc(cx, quotient);
          }
          return BigInt::dec(cx, quotient);
        }
      }
      if (uint64_t(std::abs(rem)) * 2 > uint64_t(divisor)) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
  }

  MOZ_CRASH("invalid rounding mode");
}

static BigInt* Divide(JSContext* cx, Handle<BigInt*> dividend,
                      Handle<BigInt*> divisor,
                      TemporalRoundingMode roundingMode) {
  MOZ_ASSERT(!divisor->isNegative());
  MOZ_ASSERT(!divisor->isZero());

  Rooted<BigInt*> quotient(cx);
  Rooted<BigInt*> remainder(cx);
  if (!BigInt::divmod(cx, dividend, divisor, &quotient, &remainder)) {
    return nullptr;
  }

  // No rounding needed when the remainder is zero.
  if (remainder->isZero()) {
    return quotient;
  }

  switch (roundingMode) {
    case TemporalRoundingMode::Ceil: {
      if (!remainder->isNegative()) {
        return BigInt::inc(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::Floor: {
      if (remainder->isNegative()) {
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::Trunc:
      // BigInt division truncates.
      return quotient;
    case TemporalRoundingMode::Expand: {
      if (!remainder->isNegative()) {
        return BigInt::inc(cx, quotient);
      }
      return BigInt::dec(cx, quotient);
    }
    case TemporalRoundingMode::HalfCeil: {
      BigInt* rem = BigInt::add(cx, remainder, remainder);
      if (!rem) {
        return nullptr;
      }

      if (!remainder->isNegative()) {
        if (BigInt::absoluteCompare(rem, divisor) >= 0) {
          return BigInt::inc(cx, quotient);
        }
      } else {
        if (BigInt::absoluteCompare(rem, divisor) > 0) {
          return BigInt::dec(cx, quotient);
        }
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfFloor: {
      BigInt* rem = BigInt::add(cx, remainder, remainder);
      if (!rem) {
        return nullptr;
      }

      if (remainder->isNegative()) {
        if (BigInt::absoluteCompare(rem, divisor) >= 0) {
          return BigInt::dec(cx, quotient);
        }
      } else {
        if (BigInt::absoluteCompare(rem, divisor) > 0) {
          return BigInt::inc(cx, quotient);
        }
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfExpand: {
      BigInt* rem = BigInt::add(cx, remainder, remainder);
      if (!rem) {
        return nullptr;
      }

      if (BigInt::absoluteCompare(rem, divisor) >= 0) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfTrunc: {
      BigInt* rem = BigInt::add(cx, remainder, remainder);
      if (!rem) {
        return nullptr;
      }

      if (BigInt::absoluteCompare(rem, divisor) > 0) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
    case TemporalRoundingMode::HalfEven: {
      BigInt* rem = BigInt::add(cx, remainder, remainder);
      if (!rem) {
        return nullptr;
      }

      if (BigInt::absoluteCompare(rem, divisor) == 0) {
        bool isOdd = !quotient->isZero() && (quotient->digit(0) & 1) == 1;
        if (isOdd) {
          if (!dividend->isNegative()) {
            return BigInt::inc(cx, quotient);
          }
          return BigInt::dec(cx, quotient);
        }
      }
      if (BigInt::absoluteCompare(rem, divisor) > 0) {
        if (!dividend->isNegative()) {
          return BigInt::inc(cx, quotient);
        }
        return BigInt::dec(cx, quotient);
      }
      return quotient;
    }
  }

  MOZ_CRASH("invalid rounding mode");
}

static BigInt* RoundNumberToIncrementSlow(JSContext* cx, Handle<BigInt*> x,
                                          int64_t divisor, int64_t increment,
                                          TemporalRoundingMode roundingMode) {
  // Steps 1-8.
  Rooted<BigInt*> rounded(cx, Divide(cx, x, divisor, roundingMode));
  if (!rounded) {
    return nullptr;
  }

  // We can skip the next step when |increment=1|.
  if (increment == 1) {
    return rounded;
  }

  // Step 9.
  Rooted<BigInt*> inc(cx, BigInt::createFromInt64(cx, increment));
  if (!inc) {
    return nullptr;
  }
  return BigInt::mul(cx, rounded, inc);
}

static BigInt* RoundNumberToIncrementSlow(JSContext* cx, Handle<BigInt*> x,
                                          int64_t increment,
                                          TemporalRoundingMode roundingMode) {
  return RoundNumberToIncrementSlow(cx, x, increment, increment, roundingMode);
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
bool js::temporal::RoundNumberToIncrement(JSContext* cx, const Instant& x,
                                          int64_t increment,
                                          TemporalRoundingMode roundingMode,
                                          Instant* result) {
  MOZ_ASSERT(temporal::IsValidEpochInstant(x));
  MOZ_ASSERT(increment > 0);
  MOZ_ASSERT(increment <= ToNanoseconds(TemporalUnit::Day));

  // Fast path for the default case.
  if (increment == 1) {
    *result = x;
    return true;
  }

  // Dividing zero is always zero.
  if (x == Instant{}) {
    *result = x;
    return true;
  }

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto num = x.toNanoseconds(); MOZ_LIKELY(num.isValid())) {
    // Steps 1-8.
    int64_t rounded = Divide(num.value(), increment, roundingMode);

    // Step 9.
    mozilla::CheckedInt64 checked = rounded;
    checked *= increment;
    if (MOZ_LIKELY(checked.isValid())) {
      *result = Instant::fromNanoseconds(checked.value());
      return true;
    }
  }

  Rooted<BigInt*> bi(cx, ToEpochNanoseconds(cx, x));
  if (!bi) {
    return false;
  }

  auto* rounded = RoundNumberToIncrementSlow(cx, bi, increment, roundingMode);
  if (!rounded) {
    return false;
  }

  *result = ToInstant(rounded);
  return true;
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
bool js::temporal::RoundNumberToIncrement(JSContext* cx, int64_t numerator,
                                          TemporalUnit unit,
                                          Increment increment,
                                          TemporalRoundingMode roundingMode,
                                          double* result) {
  MOZ_ASSERT(unit >= TemporalUnit::Day);
  MOZ_ASSERT(Increment::min() <= increment && increment <= Increment::max());

  // Take the slow path when the increment is too large.
  if (MOZ_UNLIKELY(increment > Increment{100'000})) {
    Rooted<BigInt*> bi(cx, BigInt::createFromInt64(cx, numerator));
    if (!bi) {
      return false;
    }

    Rooted<BigInt*> denominator(
        cx, BigInt::createFromInt64(cx, ToNanoseconds(unit)));
    if (!denominator) {
      return false;
    }

    return RoundNumberToIncrement(cx, bi, denominator, increment, roundingMode,
                                  result);
  }

  int64_t divisor = ToNanoseconds(unit) * increment.value();
  MOZ_ASSERT(divisor > 0);
  MOZ_ASSERT(divisor <= 8'640'000'000'000'000'000);

  // Division by one has no remainder.
  if (divisor == 1) {
    MOZ_ASSERT(increment == Increment{1});
    *result = double(numerator);
    return true;
  }

  // Steps 1-8.
  int64_t rounded = Divide(numerator, divisor, roundingMode);

  // Step 9.
  mozilla::CheckedInt64 checked = rounded;
  checked *= increment.value();
  if (checked.isValid()) {
    *result = double(checked.value());
    return true;
  }

  Rooted<BigInt*> bi(cx, BigInt::createFromInt64(cx, numerator));
  if (!bi) {
    return false;
  }
  return RoundNumberToIncrement(cx, bi, unit, increment, roundingMode, result);
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
bool js::temporal::RoundNumberToIncrement(
    JSContext* cx, Handle<BigInt*> numerator, TemporalUnit unit,
    Increment increment, TemporalRoundingMode roundingMode, double* result) {
  MOZ_ASSERT(unit >= TemporalUnit::Day);
  MOZ_ASSERT(Increment::min() <= increment && increment <= Increment::max());

  // Take the slow path when the increment is too large.
  if (MOZ_UNLIKELY(increment > Increment{100'000})) {
    Rooted<BigInt*> denominator(
        cx, BigInt::createFromInt64(cx, ToNanoseconds(unit)));
    if (!denominator) {
      return false;
    }

    return RoundNumberToIncrement(cx, numerator, denominator, increment,
                                  roundingMode, result);
  }

  int64_t divisor = ToNanoseconds(unit) * increment.value();
  MOZ_ASSERT(divisor > 0);
  MOZ_ASSERT(divisor <= 8'640'000'000'000'000'000);

  // Division by one has no remainder.
  if (divisor == 1) {
    MOZ_ASSERT(increment == Increment{1});
    *result = BigInt::numberValue(numerator);
    return true;
  }

  // Dividing zero is always zero.
  if (numerator->isZero()) {
    *result = 0;
    return true;
  }

  // All callers are already in the slow path, so we don't need to fast-path the
  // case when |x| can be represented by an int64 value.

  // Steps 1-9.
  auto* rounded = RoundNumberToIncrementSlow(cx, numerator, divisor,
                                             increment.value(), roundingMode);
  if (!rounded) {
    return false;
  }

  *result = BigInt::numberValue(rounded);
  return true;
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
bool js::temporal::RoundNumberToIncrement(JSContext* cx, int64_t numerator,
                                          int64_t denominator,
                                          Increment increment,
                                          TemporalRoundingMode roundingMode,
                                          double* result) {
  MOZ_ASSERT(denominator > 0);
  MOZ_ASSERT(Increment::min() <= increment && increment <= Increment::max());

  // Dividing zero is always zero.
  if (numerator == 0) {
    *result = 0;
    return true;
  }

  // We don't have to adjust the divisor when |increment=1|.
  if (increment == Increment{1}) {
    int64_t divisor = denominator;
    int64_t rounded = Divide(numerator, divisor, roundingMode);

    *result = double(rounded);
    return true;
  }

  auto divisor = mozilla::CheckedInt64(denominator) * increment.value();
  if (MOZ_LIKELY(divisor.isValid())) {
    MOZ_ASSERT(divisor.value() > 0);

    // Steps 1-8.
    int64_t rounded = Divide(numerator, divisor.value(), roundingMode);

    // Step 9.
    auto adjusted = mozilla::CheckedInt64(rounded) * increment.value();
    if (MOZ_LIKELY(adjusted.isValid())) {
      *result = double(adjusted.value());
      return true;
    }
  }

  // Slow path on overflow.

  Rooted<BigInt*> bi(cx, BigInt::createFromInt64(cx, numerator));
  if (!bi) {
    return false;
  }

  Rooted<BigInt*> denom(cx, BigInt::createFromInt64(cx, denominator));
  if (!denom) {
    return false;
  }

  return RoundNumberToIncrement(cx, bi, denom, increment, roundingMode, result);
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
bool js::temporal::RoundNumberToIncrement(
    JSContext* cx, Handle<BigInt*> numerator, Handle<BigInt*> denominator,
    Increment increment, TemporalRoundingMode roundingMode, double* result) {
  MOZ_ASSERT(!denominator->isNegative());
  MOZ_ASSERT(!denominator->isZero());
  MOZ_ASSERT(Increment::min() <= increment && increment <= Increment::max());

  // Dividing zero is always zero.
  if (numerator->isZero()) {
    *result = 0;
    return true;
  }

  // We don't have to adjust the divisor when |increment=1|.
  if (increment == Increment{1}) {
    auto divisor = denominator;

    auto* rounded = Divide(cx, numerator, divisor, roundingMode);
    if (!rounded) {
      return false;
    }

    *result = BigInt::numberValue(rounded);
    return true;
  }

  Rooted<BigInt*> inc(cx, BigInt::createFromUint64(cx, increment.value()));
  if (!inc) {
    return false;
  }

  Rooted<BigInt*> divisor(cx, BigInt::mul(cx, denominator, inc));
  if (!divisor) {
    return false;
  }
  MOZ_ASSERT(!divisor->isNegative());
  MOZ_ASSERT(!divisor->isZero());

  // Steps 1-8.
  Rooted<BigInt*> rounded(cx, Divide(cx, numerator, divisor, roundingMode));
  if (!rounded) {
    return false;
  }

  // Step 9.
  auto* adjusted = BigInt::mul(cx, rounded, inc);
  if (!adjusted) {
    return false;
  }

  *result = BigInt::numberValue(adjusted);
  return true;
}

/**
 * ToCalendarNameOption ( normalizedOptions )
 */
bool js::temporal::ToCalendarNameOption(JSContext* cx,
                                        Handle<JSObject*> options,
                                        CalendarOption* result) {
  // Step 1.
  Rooted<JSString*> calendarName(cx);
  if (!GetStringOption(cx, options, cx->names().calendarName, &calendarName)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!calendarName) {
    return true;
  }

  JSLinearString* linear = calendarName->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "auto")) {
    *result = CalendarOption::Auto;
  } else if (StringEqualsLiteral(linear, "always")) {
    *result = CalendarOption::Always;
  } else if (StringEqualsLiteral(linear, "never")) {
    *result = CalendarOption::Never;
  } else if (StringEqualsLiteral(linear, "critical")) {
    *result = CalendarOption::Critical;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "calendarName",
                               chars.get());
    }
    return false;
  }
  return true;
}

/**
 * ToFractionalSecondDigits ( normalizedOptions )
 */
bool js::temporal::ToFractionalSecondDigits(JSContext* cx,
                                            JS::Handle<JSObject*> options,
                                            Precision* precision) {
  // Step 1.
  Rooted<Value> digitsValue(cx);
  if (!GetProperty(cx, options, options, cx->names().fractionalSecondDigits,
                   &digitsValue)) {
    return false;
  }

  // Step 2.
  if (digitsValue.isUndefined()) {
    *precision = Precision::Auto();
    return true;
  }

  // Step 3.
  if (!digitsValue.isNumber()) {
    // Step 3.a.
    JSString* string = JS::ToString(cx, digitsValue);
    if (!string) {
      return false;
    }

    JSLinearString* linear = string->ensureLinear(cx);
    if (!linear) {
      return false;
    }

    if (!StringEqualsLiteral(linear, "auto")) {
      if (auto chars = QuoteString(cx, linear, '"')) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_INVALID_OPTION_VALUE,
                                 "fractionalSecondDigits", chars.get());
      }
      return false;
    }

    // Step 3.b.
    *precision = Precision::Auto();
    return true;
  }

  // Step 4.
  double digitCount = digitsValue.toNumber();
  if (!std::isfinite(digitCount)) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, digitCount);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_OPTION_VALUE,
                              "fractionalSecondDigits", numStr);
    return false;
  }

  // Step 5.
  digitCount = std::floor(digitCount);

  // Step 6.
  if (digitCount < 0 || digitCount > 9) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, digitCount);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INVALID_OPTION_VALUE,
                              "fractionalSecondDigits", numStr);
    return false;
  }

  // Step 7.
  *precision = Precision{uint8_t(digitCount)};
  return true;
}

/**
 * ToSecondsStringPrecisionRecord ( smallestUnit, fractionalDigitCount )
 */
SecondsStringPrecision js::temporal::ToSecondsStringPrecision(
    TemporalUnit smallestUnit, Precision fractionalDigitCount) {
  MOZ_ASSERT(smallestUnit == TemporalUnit::Auto ||
             smallestUnit >= TemporalUnit::Minute);
  MOZ_ASSERT(fractionalDigitCount == Precision::Auto() ||
             fractionalDigitCount.value() <= 9);

  // Steps 1-5.
  switch (smallestUnit) {
    // Step 1.
    case TemporalUnit::Minute:
      return {Precision::Minute(), TemporalUnit::Minute, Increment{1}};

    // Step 2.
    case TemporalUnit::Second:
      return {Precision{0}, TemporalUnit::Second, Increment{1}};

    // Step 3.
    case TemporalUnit::Millisecond:
      return {Precision{3}, TemporalUnit::Millisecond, Increment{1}};

    // Step 4.
    case TemporalUnit::Microsecond:
      return {Precision{6}, TemporalUnit::Microsecond, Increment{1}};

    // Step 5.
    case TemporalUnit::Nanosecond:
      return {Precision{9}, TemporalUnit::Nanosecond, Increment{1}};

    case TemporalUnit::Auto:
      break;

    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day:
    case TemporalUnit::Hour:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 6. (Not applicable in our implementation.)

  // Step 7.
  if (fractionalDigitCount == Precision::Auto()) {
    return {Precision::Auto(), TemporalUnit::Nanosecond, Increment{1}};
  }

  static constexpr Increment increments[] = {
      Increment{1},
      Increment{10},
      Increment{100},
  };

  uint8_t digitCount = fractionalDigitCount.value();

  // Step 8.
  if (digitCount == 0) {
    return {Precision{0}, TemporalUnit::Second, Increment{1}};
  }

  // Step 9.
  if (digitCount <= 3) {
    return {fractionalDigitCount, TemporalUnit::Millisecond,
            increments[3 - digitCount]};
  }

  // Step 10.
  if (digitCount <= 6) {
    return {fractionalDigitCount, TemporalUnit::Microsecond,
            increments[6 - digitCount]};
  }

  // Step 11.
  MOZ_ASSERT(digitCount <= 9);

  // Step 12.
  return {fractionalDigitCount, TemporalUnit::Nanosecond,
          increments[9 - digitCount]};
}

/**
 * ToTemporalOverflow ( normalizedOptions )
 */
bool js::temporal::ToTemporalOverflow(JSContext* cx, Handle<JSObject*> options,
                                      TemporalOverflow* result) {
  // Step 1.
  Rooted<JSString*> overflow(cx);
  if (!GetStringOption(cx, options, cx->names().overflow, &overflow)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!overflow) {
    return true;
  }

  JSLinearString* linear = overflow->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "constrain")) {
    *result = TemporalOverflow::Constrain;
  } else if (StringEqualsLiteral(linear, "reject")) {
    *result = TemporalOverflow::Reject;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "overflow",
                               chars.get());
    }
    return false;
  }
  return true;
}

/**
 * ToTemporalDisambiguation ( options )
 */
bool js::temporal::ToTemporalDisambiguation(
    JSContext* cx, Handle<JSObject*> options,
    TemporalDisambiguation* disambiguation) {
  // Step 1. (Not applicable)

  // Step 2.
  Rooted<JSString*> string(cx);
  if (!GetStringOption(cx, options, cx->names().disambiguation, &string)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!string) {
    return true;
  }

  JSLinearString* linear = string->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "compatible")) {
    *disambiguation = TemporalDisambiguation::Compatible;
  } else if (StringEqualsLiteral(linear, "earlier")) {
    *disambiguation = TemporalDisambiguation::Earlier;
  } else if (StringEqualsLiteral(linear, "later")) {
    *disambiguation = TemporalDisambiguation::Later;
  } else if (StringEqualsLiteral(linear, "reject")) {
    *disambiguation = TemporalDisambiguation::Reject;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "disambiguation",
                               chars.get());
    }
    return false;
  }
  return true;
}

/**
 * ToTemporalOffset ( options, fallback )
 */
bool js::temporal::ToTemporalOffset(JSContext* cx, Handle<JSObject*> options,
                                    TemporalOffset* offset) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<JSString*> string(cx);
  if (!GetStringOption(cx, options, cx->names().offset, &string)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!string) {
    return true;
  }

  JSLinearString* linear = string->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "prefer")) {
    *offset = TemporalOffset::Prefer;
  } else if (StringEqualsLiteral(linear, "use")) {
    *offset = TemporalOffset::Use;
  } else if (StringEqualsLiteral(linear, "ignore")) {
    *offset = TemporalOffset::Ignore;
  } else if (StringEqualsLiteral(linear, "reject")) {
    *offset = TemporalOffset::Reject;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "offset",
                               chars.get());
    }
    return false;
  }
  return true;
}

/**
 * ToTimeZoneNameOption ( normalizedOptions )
 */
bool js::temporal::ToTimeZoneNameOption(JSContext* cx,
                                        Handle<JSObject*> options,
                                        TimeZoneNameOption* result) {
  // Step 1.
  Rooted<JSString*> timeZoneName(cx);
  if (!GetStringOption(cx, options, cx->names().timeZoneName, &timeZoneName)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!timeZoneName) {
    return true;
  }

  JSLinearString* linear = timeZoneName->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "auto")) {
    *result = TimeZoneNameOption::Auto;
  } else if (StringEqualsLiteral(linear, "never")) {
    *result = TimeZoneNameOption::Never;
  } else if (StringEqualsLiteral(linear, "critical")) {
    *result = TimeZoneNameOption::Critical;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "timeZoneName",
                               chars.get());
    }
    return false;
  }
  return true;
}

/**
 * ToShowOffsetOption ( normalizedOptions )
 */
bool js::temporal::ToShowOffsetOption(JSContext* cx, Handle<JSObject*> options,
                                      ShowOffsetOption* result) {
  // FIXME: spec issue - should be renamed to ToOffsetOption to match the other
  // operations ToCalendarNameOption and ToTimeZoneNameOption.
  //
  // https://github.com/tc39/proposal-temporal/issues/2441

  // Step 1.
  Rooted<JSString*> offset(cx);
  if (!GetStringOption(cx, options, cx->names().offset, &offset)) {
    return false;
  }

  // Caller should fill in the fallback.
  if (!offset) {
    return true;
  }

  JSLinearString* linear = offset->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  if (StringEqualsLiteral(linear, "auto")) {
    *result = ShowOffsetOption::Auto;
  } else if (StringEqualsLiteral(linear, "never")) {
    *result = ShowOffsetOption::Never;
  } else {
    if (auto chars = QuoteString(cx, linear, '"')) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_INVALID_OPTION_VALUE, "offset",
                               chars.get());
    }
    return false;
  }
  return true;
}

template <typename T, typename... Ts>
static JSObject* MaybeUnwrapIf(JSObject* object) {
  if (auto* unwrapped = object->maybeUnwrapIf<T>()) {
    return unwrapped;
  }
  if constexpr (sizeof...(Ts) > 0) {
    return MaybeUnwrapIf<Ts...>(object);
  }
  return nullptr;
}

// FIXME: spec issue - "Reject" is exclusively used for Promise rejection. The
// existing `RejectPromise` abstract operation unconditionally rejects, whereas
// this operation conditionally rejects.
// https://github.com/tc39/proposal-temporal/issues/2534

/**
 * RejectTemporalLikeObject ( object )
 */
bool js::temporal::RejectTemporalLikeObject(JSContext* cx,
                                            Handle<JSObject*> object) {
  // Step 1.
  if (auto* unwrapped =
          MaybeUnwrapIf<PlainDateObject, PlainDateTimeObject,
                        PlainMonthDayObject, PlainTimeObject,
                        PlainYearMonthObject, ZonedDateTimeObject>(object)) {
    Rooted<Value> value(cx, ObjectValue(*object));
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, value,
                     nullptr, unwrapped->getClass()->name);
    return false;
  }

  Rooted<Value> property(cx);

  // Step 2.
  if (!GetProperty(cx, object, object, cx->names().calendar, &property)) {
    return false;
  }

  // Step 3.
  if (!property.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_UNEXPECTED_PROPERTY, "calendar");
    return false;
  }

  // Step 4.
  if (!GetProperty(cx, object, object, cx->names().timeZone, &property)) {
    return false;
  }

  // Step 5.
  if (!property.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_UNEXPECTED_PROPERTY, "timeZone");
    return false;
  }

  // Step 6.
  return true;
}

/**
 * ToPositiveIntegerWithTruncation ( argument )
 */
bool js::temporal::ToPositiveIntegerWithTruncation(JSContext* cx,
                                                   Handle<Value> value,
                                                   const char* name,
                                                   double* result) {
  // Step 1.
  double number;
  if (!ToIntegerWithTruncation(cx, value, name, &number)) {
    return false;
  }

  // Step 2.
  if (number <= 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, name);
    return false;
  }

  // Step 3.
  *result = number;
  return true;
}

/**
 * ToIntegerWithTruncation ( argument )
 */
bool js::temporal::ToIntegerWithTruncation(JSContext* cx, Handle<Value> value,
                                           const char* name, double* result) {
  // Step 1.
  double number;
  if (!JS::ToNumber(cx, value, &number)) {
    return false;
  }

  // Step 2.
  if (!std::isfinite(number)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_INTEGER, name);
    return false;
  }

  // Step 3.
  *result = std::trunc(number) + (+0.0);  // Add zero to convert -0 to +0.
  return true;
}

/**
 * GetMethod ( V, P )
 */
bool js::temporal::GetMethod(JSContext* cx, Handle<JSObject*> object,
                             Handle<PropertyName*> name,
                             MutableHandle<Value> result) {
  // We don't directly invoke |Call|, because |Call| tries to find the function
  // on the stack (JSDVG_SEARCH_STACK). This leads to confusing error messages
  // like:
  //
  // js> print(new Temporal.ZonedDateTime(0n, {}, {}))
  // typein:1:6 TypeError: print is not a function

  // Step 1.
  if (!GetProperty(cx, object, object, name, result)) {
    return false;
  }

  // Step 2.
  if (result.isNullOrUndefined()) {
    return true;
  }

  // Step 3.
  if (!IsCallable(result)) {
    if (auto chars = StringToNewUTF8CharsZ(cx, *name)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_PROPERTY_NOT_CALLABLE, chars.get());
    }
    return false;
  }

  // Step 4.
  return true;
}

/**
 * GetMethod ( V, P )
 */
bool js::temporal::GetMethodForCall(JSContext* cx, Handle<JSObject*> object,
                                    Handle<PropertyName*> name,
                                    MutableHandle<Value> result) {
  // We don't directly invoke |Call|, because |Call| tries to find the function
  // on the stack (JSDVG_SEARCH_STACK). This leads to confusing error messages
  // like:
  //
  // js> print(new Temporal.ZonedDateTime(0n, {}, {}))
  // typein:1:6 TypeError: print is not a function

  // Step 1.
  if (!GetProperty(cx, object, object, name, result)) {
    return false;
  }

  // Steps 2-3.
  if (!IsCallable(result)) {
    if (auto chars = StringToNewUTF8CharsZ(cx, *name)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_PROPERTY_NOT_CALLABLE, chars.get());
    }
    return false;
  }

  // Step 4.
  return true;
}

/**
 * CopyDataProperties ( target, source, excludedKeys [ , excludedValues ] )
 *
 * Implementation when |excludedKeys| and |excludedValues| are both empty lists.
 */
bool js::temporal::CopyDataProperties(JSContext* cx,
                                      Handle<PlainObject*> target,
                                      Handle<JSObject*> source) {
  // Optimization for the common case when |source| is a native object.
  if (source->is<NativeObject>()) {
    bool optimized = false;
    if (!CopyDataPropertiesNative(cx, target, source.as<NativeObject>(),
                                  nullptr, &optimized)) {
      return false;
    }
    if (optimized) {
      return true;
    }
  }

  // Step 1-2. (Not applicable)

  // Step 3.
  JS::RootedVector<PropertyKey> keys(cx);
  if (!GetPropertyKeys(
          cx, source, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys)) {
    return false;
  }

  // Step 4.
  Rooted<mozilla::Maybe<PropertyDescriptor>> desc(cx);
  Rooted<Value> propValue(cx);
  for (size_t i = 0; i < keys.length(); i++) {
    Handle<PropertyKey> key = keys[i];

    // Steps 4.a-b. (Not applicable)

    // Step 4.c.i.
    if (!GetOwnPropertyDescriptor(cx, source, key, &desc)) {
      return false;
    }

    // Step 4.c.ii.
    if (desc.isNothing() || !desc->enumerable()) {
      continue;
    }

    // Step 4.c.ii.1.
    if (!GetProperty(cx, source, source, key, &propValue)) {
      return false;
    }

    // Step 4.c.ii.2. (Not applicable)

    // Step 4.c.ii.3.
    if (!DefineDataProperty(cx, target, key, propValue)) {
      return false;
    }
  }

  // Step 5.
  return true;
}

/**
 * CopyDataProperties ( target, source, excludedKeys [ , excludedValues ] )
 *
 * Implementation when |excludedKeys| is an empty list and |excludedValues| is
 * the list «undefined».
 */
static bool CopyDataPropertiesIgnoreUndefined(JSContext* cx,
                                              Handle<PlainObject*> target,
                                              Handle<JSObject*> source) {
  // Step 1-2. (Not applicable)

  // Step 3.
  JS::RootedVector<PropertyKey> keys(cx);
  if (!GetPropertyKeys(
          cx, source, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &keys)) {
    return false;
  }

  // Step 4.
  Rooted<mozilla::Maybe<PropertyDescriptor>> desc(cx);
  Rooted<Value> propValue(cx);
  for (size_t i = 0; i < keys.length(); i++) {
    Handle<PropertyKey> key = keys[i];

    // Steps 4.a-b. (Not applicable)

    // Step 4.c.i.
    if (!GetOwnPropertyDescriptor(cx, source, key, &desc)) {
      return false;
    }

    // Step 4.c.ii.
    if (desc.isNothing() || !desc->enumerable()) {
      continue;
    }

    // Step 4.c.ii.1.
    if (!GetProperty(cx, source, source, key, &propValue)) {
      return false;
    }

    // Step 4.c.ii.2.
    if (propValue.isUndefined()) {
      continue;
    }

    // Step 4.c.ii.3.
    if (!DefineDataProperty(cx, target, key, propValue)) {
      return false;
    }
  }

  // Step 5.
  return true;
}

/**
 * SnapshotOwnProperties ( source, proto [, excludedKeys [, excludedValues ] ] )
 */
PlainObject* js::temporal::SnapshotOwnProperties(JSContext* cx,
                                                 Handle<JSObject*> source) {
  // Step 1.
  Rooted<PlainObject*> copy(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!copy) {
    return nullptr;
  }

  // Steps 2-4.
  if (!CopyDataProperties(cx, copy, source)) {
    return nullptr;
  }

  // Step 3.
  return copy;
}

/**
 * SnapshotOwnProperties ( source, proto [, excludedKeys [, excludedValues ] ] )
 *
 * Implementation when |excludedKeys| is an empty list and |excludedValues| is
 * the list «undefined».
 */
PlainObject* js::temporal::SnapshotOwnPropertiesIgnoreUndefined(
    JSContext* cx, Handle<JSObject*> source) {
  // Step 1.
  Rooted<PlainObject*> copy(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!copy) {
    return nullptr;
  }

  // Steps 2-4.
  if (!CopyDataPropertiesIgnoreUndefined(cx, copy, source)) {
    return nullptr;
  }

  // Step 3.
  return copy;
}

/**
 * GetDifferenceSettings ( operation, options, unitGroup, disallowedUnits,
 * fallbackSmallestUnit, smallestLargestDefaultUnit )
 */
bool js::temporal::GetDifferenceSettings(
    JSContext* cx, TemporalDifference operation, Handle<JSObject*> options,
    TemporalUnitGroup unitGroup, TemporalUnit smallestAllowedUnit,
    TemporalUnit fallbackSmallestUnit, TemporalUnit smallestLargestDefaultUnit,
    DifferenceSettings* result) {
  // Steps 1-2.
  auto largestUnit = TemporalUnit::Auto;
  if (!GetTemporalUnit(cx, options, TemporalUnitKey::LargestUnit, unitGroup,
                       &largestUnit)) {
    return false;
  }

  // Step 3.
  if (largestUnit > smallestAllowedUnit) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_UNIT_OPTION,
                              TemporalUnitToString(largestUnit), "largestUnit");
    return false;
  }

  // Step 4.
  auto roundingIncrement = Increment{1};
  if (!ToTemporalRoundingIncrement(cx, options, &roundingIncrement)) {
    return false;
  }

  // Step 5.
  auto roundingMode = TemporalRoundingMode::Trunc;
  if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
    return false;
  }

  // Step 6.
  if (operation == TemporalDifference::Since) {
    roundingMode = NegateTemporalRoundingMode(roundingMode);
  }

  // Step 7.
  auto smallestUnit = fallbackSmallestUnit;
  if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit, unitGroup,
                       &smallestUnit)) {
    return false;
  }

  // Step 8.
  if (smallestUnit > smallestAllowedUnit) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_TEMPORAL_INVALID_UNIT_OPTION,
        TemporalUnitToString(smallestUnit), "smallestUnit");
    return false;
  }

  // Step 9. (Inlined call to LargerOfTwoTemporalUnits)
  auto defaultLargestUnit = std::min(smallestLargestDefaultUnit, smallestUnit);

  // Step 10.
  if (largestUnit == TemporalUnit::Auto) {
    largestUnit = defaultLargestUnit;
  }

  // Step 11.
  if (largestUnit > smallestUnit) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_UNIT_RANGE);
    return false;
  }

  // Steps 12-13.
  if (smallestUnit > TemporalUnit::Day) {
    // Step 12.
    auto maximum = MaximumTemporalDurationRoundingIncrement(smallestUnit);

    // Step 13.
    if (!ValidateTemporalRoundingIncrement(cx, roundingIncrement, maximum,
                                           false)) {
      return false;
    }
  }

  // Step 14.
  *result = {smallestUnit, largestUnit, roundingMode, roundingIncrement};
  return true;
}

static JSObject* CreateTemporalObject(JSContext* cx, JSProtoKey key) {
  Rooted<JSObject*> proto(cx, &cx->global()->getObjectPrototype());

  // The |Temporal| object is just a plain object with some "static" data
  // properties and some constructor properties.
  return NewTenuredObjectWithGivenProto<TemporalObject>(cx, proto);
}

/**
 * Initializes the Temporal Object and its standard built-in properties.
 */
static bool TemporalClassFinish(JSContext* cx, Handle<JSObject*> temporal,
                                Handle<JSObject*> proto) {
  Rooted<PropertyKey> ctorId(cx);
  Rooted<Value> ctorValue(cx);
  auto defineProperty = [&](JSProtoKey protoKey, Handle<PropertyName*> name) {
    JSObject* ctor = GlobalObject::getOrCreateConstructor(cx, protoKey);
    if (!ctor) {
      return false;
    }

    ctorId = NameToId(name);
    ctorValue.setObject(*ctor);
    return DefineDataProperty(cx, temporal, ctorId, ctorValue, 0);
  };

  // Add the constructor properties.
  for (const auto& protoKey :
       {JSProto_Calendar, JSProto_Duration, JSProto_Instant, JSProto_PlainDate,
        JSProto_PlainDateTime, JSProto_PlainMonthDay, JSProto_PlainTime,
        JSProto_PlainYearMonth, JSProto_TimeZone, JSProto_ZonedDateTime}) {
    if (!defineProperty(protoKey, ClassName(protoKey, cx))) {
      return false;
    }
  }

  // ClassName(JSProto_TemporalNow) returns "TemporalNow", so we need to handle
  // it separately.
  if (!defineProperty(JSProto_TemporalNow, cx->names().Now)) {
    return false;
  }

  return true;
}

const JSClass TemporalObject::class_ = {
    "Temporal",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Temporal),
    JS_NULL_CLASS_OPS,
    &TemporalObject::classSpec_,
};

static const JSPropertySpec Temporal_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Temporal", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec TemporalObject::classSpec_ = {
    CreateTemporalObject, nullptr, nullptr,
    Temporal_properties,  nullptr, nullptr,
    TemporalClassFinish,
};
