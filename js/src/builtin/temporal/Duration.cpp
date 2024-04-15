/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Duration.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/Int128.h"
#include "builtin/temporal/Int96.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalRoundingMode.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/Wrapped.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "gc/GCEnum.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
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
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/BigIntType.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsDuration(Handle<Value> v) {
  return v.isObject() && v.toObject().is<DurationObject>();
}

#ifdef DEBUG
static bool IsIntegerOrInfinity(double d) {
  return IsInteger(d) || std::isinf(d);
}

static bool IsIntegerOrInfinityDuration(const Duration& duration) {
  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  // Integers exceeding the Number range are represented as infinity.

  return IsIntegerOrInfinity(years) && IsIntegerOrInfinity(months) &&
         IsIntegerOrInfinity(weeks) && IsIntegerOrInfinity(days) &&
         IsIntegerOrInfinity(hours) && IsIntegerOrInfinity(minutes) &&
         IsIntegerOrInfinity(seconds) && IsIntegerOrInfinity(milliseconds) &&
         IsIntegerOrInfinity(microseconds) && IsIntegerOrInfinity(nanoseconds);
}

static bool IsIntegerDuration(const Duration& duration) {
  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  return IsInteger(years) && IsInteger(months) && IsInteger(weeks) &&
         IsInteger(days) && IsInteger(hours) && IsInteger(minutes) &&
         IsInteger(seconds) && IsInteger(milliseconds) &&
         IsInteger(microseconds) && IsInteger(nanoseconds);
}
#endif

/**
 * DurationSign ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
int32_t js::temporal::DurationSign(const Duration& duration) {
  MOZ_ASSERT(IsIntegerOrInfinityDuration(duration));

  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  // Step 1.
  for (auto v : {years, months, weeks, days, hours, minutes, seconds,
                 milliseconds, microseconds, nanoseconds}) {
    // Step 1.a.
    if (v < 0) {
      return -1;
    }

    // Step 1.b.
    if (v > 0) {
      return 1;
    }
  }

  // Step 2.
  return 0;
}

/**
 * Normalize a nanoseconds amount into a time duration.
 */
static NormalizedTimeDuration NormalizeNanoseconds(const Int96& nanoseconds) {
  // Split into seconds and nanoseconds.
  auto [seconds, nanos] = nanoseconds / ToNanoseconds(TemporalUnit::Second);

  return {seconds, nanos};
}

/**
 * Normalize a nanoseconds amount into a time duration. Return Nothing if the
 * value is too large.
 */
static mozilla::Maybe<NormalizedTimeDuration> NormalizeNanoseconds(
    double nanoseconds) {
  MOZ_ASSERT(IsInteger(nanoseconds));

  if (auto int96 = Int96::fromInteger(nanoseconds)) {
    // The number of normalized seconds must not exceed `2**53 - 1`.
    constexpr auto limit =
        Int96{uint64_t(1) << 53} * ToNanoseconds(TemporalUnit::Second);

    if (int96->abs() < limit) {
      return mozilla::Some(NormalizeNanoseconds(*int96));
    }
  }
  return mozilla::Nothing();
}

/**
 * Normalize a microseconds amount into a time duration.
 */
static NormalizedTimeDuration NormalizeMicroseconds(const Int96& microseconds) {
  // Split into seconds and microseconds.
  auto [seconds, micros] = microseconds / ToMicroseconds(TemporalUnit::Second);

  // Scale microseconds to nanoseconds.
  int32_t nanos = micros * ToNanoseconds(TemporalUnit::Microsecond);

  return {seconds, nanos};
}

/**
 * Normalize a microseconds amount into a time duration. Return Nothing if the
 * value is too large.
 */
static mozilla::Maybe<NormalizedTimeDuration> NormalizeMicroseconds(
    double microseconds) {
  MOZ_ASSERT(IsInteger(microseconds));

  if (auto int96 = Int96::fromInteger(microseconds)) {
    // The number of normalized seconds must not exceed `2**53 - 1`.
    constexpr auto limit =
        Int96{uint64_t(1) << 53} * ToMicroseconds(TemporalUnit::Second);

    if (int96->abs() < limit) {
      return mozilla::Some(NormalizeMicroseconds(*int96));
    }
  }
  return mozilla::Nothing();
}

/**
 * Normalize a duration into a time duration. Return Nothing if any duration
 * value is too large.
 */
static mozilla::Maybe<NormalizedTimeDuration> NormalizeSeconds(
    const Duration& duration) {
  do {
    auto nanoseconds = NormalizeNanoseconds(duration.nanoseconds);
    if (!nanoseconds) {
      break;
    }
    MOZ_ASSERT(IsValidNormalizedTimeDuration(*nanoseconds));

    auto microseconds = NormalizeMicroseconds(duration.microseconds);
    if (!microseconds) {
      break;
    }
    MOZ_ASSERT(IsValidNormalizedTimeDuration(*microseconds));

    // Overflows for millis/seconds/minutes/hours/days always result in an
    // invalid normalized time duration.

    int64_t milliseconds;
    if (!mozilla::NumberEqualsInt64(duration.milliseconds, &milliseconds)) {
      break;
    }

    int64_t seconds;
    if (!mozilla::NumberEqualsInt64(duration.seconds, &seconds)) {
      break;
    }

    int64_t minutes;
    if (!mozilla::NumberEqualsInt64(duration.minutes, &minutes)) {
      break;
    }

    int64_t hours;
    if (!mozilla::NumberEqualsInt64(duration.hours, &hours)) {
      break;
    }

    int64_t days;
    if (!mozilla::NumberEqualsInt64(duration.days, &days)) {
      break;
    }

    // Compute the overall amount of milliseconds.
    mozilla::CheckedInt64 millis = days;
    millis *= 24;
    millis += hours;
    millis *= 60;
    millis += minutes;
    millis *= 60;
    millis += seconds;
    millis *= 1000;
    millis += milliseconds;
    if (!millis.isValid()) {
      break;
    }

    auto milli = NormalizedTimeDuration::fromMilliseconds(millis.value());
    if (!IsValidNormalizedTimeDuration(milli)) {
      break;
    }

    // Compute the overall time duration.
    auto result = milli + *microseconds + *nanoseconds;
    if (!IsValidNormalizedTimeDuration(result)) {
      break;
    }

    return mozilla::Some(result);
  } while (false);

  return mozilla::Nothing();
}

/**
 * Normalize a days amount into a time duration. Return Nothing if the value is
 * too large.
 */
static mozilla::Maybe<NormalizedTimeDuration> NormalizeDays(double days) {
  MOZ_ASSERT(IsInteger(days));

  do {
    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    // Compute the overall amount of milliseconds.
    auto millis =
        mozilla::CheckedInt64(intDays) * ToMilliseconds(TemporalUnit::Day);
    if (!millis.isValid()) {
      break;
    }

    auto result = NormalizedTimeDuration::fromMilliseconds(millis.value());
    if (!IsValidNormalizedTimeDuration(result)) {
      break;
    }

    return mozilla::Some(result);
  } while (false);

  return mozilla::Nothing();
}

/**
 * NormalizeTimeDuration ( hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds )
 */
static NormalizedTimeDuration NormalizeTimeDuration(
    double hours, double minutes, double seconds, double milliseconds,
    double microseconds, double nanoseconds) {
  MOZ_ASSERT(IsInteger(hours));
  MOZ_ASSERT(IsInteger(minutes));
  MOZ_ASSERT(IsInteger(seconds));
  MOZ_ASSERT(IsInteger(milliseconds));
  MOZ_ASSERT(IsInteger(microseconds));
  MOZ_ASSERT(IsInteger(nanoseconds));

  // Steps 1-3.
  mozilla::CheckedInt64 millis = int64_t(hours);
  millis *= 60;
  millis += int64_t(minutes);
  millis *= 60;
  millis += int64_t(seconds);
  millis *= 1000;
  millis += int64_t(milliseconds);
  MOZ_ASSERT(millis.isValid());

  auto normalized = NormalizedTimeDuration::fromMilliseconds(millis.value());

  // Step 4.
  auto micros = Int96::fromInteger(microseconds);
  MOZ_ASSERT(micros);

  normalized += NormalizeMicroseconds(*micros);

  // Step 5.
  auto nanos = Int96::fromInteger(nanoseconds);
  MOZ_ASSERT(nanos);

  normalized += NormalizeNanoseconds(*nanos);

  // Step 6.
  MOZ_ASSERT(IsValidNormalizedTimeDuration(normalized));

  // Step 7.
  return normalized;
}

/**
 * NormalizeTimeDuration ( hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds )
 */
NormalizedTimeDuration js::temporal::NormalizeTimeDuration(
    int32_t hours, int32_t minutes, int32_t seconds, int32_t milliseconds,
    int32_t microseconds, int32_t nanoseconds) {
  // Steps 1-3.
  mozilla::CheckedInt64 millis = int64_t(hours);
  millis *= 60;
  millis += int64_t(minutes);
  millis *= 60;
  millis += int64_t(seconds);
  millis *= 1000;
  millis += int64_t(milliseconds);
  MOZ_ASSERT(millis.isValid());

  auto normalized = NormalizedTimeDuration::fromMilliseconds(millis.value());

  // Step 4.
  normalized += NormalizeMicroseconds(Int96{microseconds});

  // Step 5.
  normalized += NormalizeNanoseconds(Int96{nanoseconds});

  // Step 6.
  MOZ_ASSERT(IsValidNormalizedTimeDuration(normalized));

  // Step 7.
  return normalized;
}

/**
 * NormalizeTimeDuration ( hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds )
 */
NormalizedTimeDuration js::temporal::NormalizeTimeDuration(
    const Duration& duration) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::NormalizeTimeDuration(duration.hours, duration.minutes,
                                 duration.seconds, duration.milliseconds,
                                 duration.microseconds, duration.nanoseconds);
}

/**
 * AddNormalizedTimeDuration ( one, two )
 */
static bool AddNormalizedTimeDuration(JSContext* cx,
                                      const NormalizedTimeDuration& one,
                                      const NormalizedTimeDuration& two,
                                      NormalizedTimeDuration* result) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(one));
  MOZ_ASSERT(IsValidNormalizedTimeDuration(two));

  // Step 1.
  auto sum = one + two;

  // Step 2.
  if (!IsValidNormalizedTimeDuration(sum)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  // Step 3.
  *result = sum;
  return true;
}

/**
 * SubtractNormalizedTimeDuration ( one, two )
 */
static bool SubtractNormalizedTimeDuration(JSContext* cx,
                                           const NormalizedTimeDuration& one,
                                           const NormalizedTimeDuration& two,
                                           NormalizedTimeDuration* result) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(one));
  MOZ_ASSERT(IsValidNormalizedTimeDuration(two));

  // Step 1.
  auto sum = one - two;

  // Step 2.
  if (!IsValidNormalizedTimeDuration(sum)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  // Step 3.
  *result = sum;
  return true;
}

/**
 * Add24HourDaysToNormalizedTimeDuration ( d, days )
 */
bool js::temporal::Add24HourDaysToNormalizedTimeDuration(
    JSContext* cx, const NormalizedTimeDuration& d, double days,
    NormalizedTimeDuration* result) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(d));
  MOZ_ASSERT(IsInteger(days));

  // Step 1.
  auto normalizedDays = NormalizeDays(days);
  if (!normalizedDays) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  // Step 2.
  auto sum = d + *normalizedDays;
  if (!IsValidNormalizedTimeDuration(sum)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  // Step 3.
  *result = sum;
  return true;
}

/**
 * CombineDateAndNormalizedTimeDuration ( dateDurationRecord, norm )
 */
bool js::temporal::CombineDateAndNormalizedTimeDuration(
    JSContext* cx, const DateDuration& date, const NormalizedTimeDuration& time,
    NormalizedDuration* result) {
  MOZ_ASSERT(IsValidDuration(date.toDuration()));
  MOZ_ASSERT(IsValidNormalizedTimeDuration(time));

  // Step 1.
  int32_t dateSign = ::DurationSign(date.toDuration());

  // Step 2.
  int32_t timeSign = NormalizedTimeDurationSign(time);

  // Step 3
  if ((dateSign * timeSign) < 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_COMBINE_INVALID_SIGN);
    return false;
  }

  // Step 4.
  *result = {date, time};
  return true;
}

/**
 * NormalizedTimeDurationFromEpochNanosecondsDifference ( one, two )
 */
NormalizedTimeDuration
js::temporal::NormalizedTimeDurationFromEpochNanosecondsDifference(
    const Instant& one, const Instant& two) {
  MOZ_ASSERT(IsValidEpochInstant(one));
  MOZ_ASSERT(IsValidEpochInstant(two));

  // Step 1.
  auto result = one - two;

  // Step 2.
  MOZ_ASSERT(IsValidInstantSpan(result));

  // Step 3.
  return result.to<NormalizedTimeDuration>();
}

/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
bool js::temporal::IsValidDuration(const Duration& duration) {
  MOZ_ASSERT(IsIntegerOrInfinityDuration(duration));

  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  // Step 1.
  int32_t sign = DurationSign(duration);

  // Step 2.
  for (auto v : {years, months, weeks, days, hours, minutes, seconds,
                 milliseconds, microseconds, nanoseconds}) {
    // Step 2.a.
    if (!std::isfinite(v)) {
      return false;
    }

    // Step 2.b.
    if (v < 0 && sign > 0) {
      return false;
    }

    // Step 2.c.
    if (v > 0 && sign < 0) {
      return false;
    }
  }

  // Steps 3-4.
  if (!NormalizeSeconds(duration)) {
    return false;
  }

  // Step 5.
  return true;
}

#ifdef DEBUG
/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
bool js::temporal::IsValidDuration(const NormalizedDuration& duration) {
  auto date = duration.date.toDuration();
  return IsValidDuration(date) &&
         IsValidNormalizedTimeDuration(duration.time) &&
         (DurationSign(date) * NormalizedTimeDurationSign(duration.time) >= 0);
}
#endif

/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
bool js::temporal::ThrowIfInvalidDuration(JSContext* cx,
                                          const Duration& duration) {
  MOZ_ASSERT(IsIntegerOrInfinityDuration(duration));

  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  // Step 1.
  int32_t sign = DurationSign(duration);

  auto report = [&](double v, const char* name, unsigned errorNumber) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, v);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, errorNumber, name,
                              numStr);
  };

  auto throwIfInvalid = [&](double v, const char* name) {
    // Step 2.a.
    if (!std::isfinite(v)) {
      report(v, name, JSMSG_TEMPORAL_DURATION_INVALID_NON_FINITE);
      return false;
    }

    // Steps 2.b-c.
    if ((v < 0 && sign > 0) || (v > 0 && sign < 0)) {
      report(v, name, JSMSG_TEMPORAL_DURATION_INVALID_SIGN);
      return false;
    }

    return true;
  };

  // Step 2.
  if (!throwIfInvalid(years, "years")) {
    return false;
  }
  if (!throwIfInvalid(months, "months")) {
    return false;
  }
  if (!throwIfInvalid(weeks, "weeks")) {
    return false;
  }
  if (!throwIfInvalid(days, "days")) {
    return false;
  }
  if (!throwIfInvalid(hours, "hours")) {
    return false;
  }
  if (!throwIfInvalid(minutes, "minutes")) {
    return false;
  }
  if (!throwIfInvalid(seconds, "seconds")) {
    return false;
  }
  if (!throwIfInvalid(milliseconds, "milliseconds")) {
    return false;
  }
  if (!throwIfInvalid(microseconds, "microseconds")) {
    return false;
  }
  if (!throwIfInvalid(nanoseconds, "nanoseconds")) {
    return false;
  }

  // Steps 3-4.
  if (!NormalizeSeconds(duration)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  MOZ_ASSERT(IsValidDuration(duration));

  // Step 5.
  return true;
}

/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
static bool ThrowIfInvalidDuration(JSContext* cx,
                                   const DateDuration& duration) {
  MOZ_ASSERT(IsIntegerOrInfinityDuration(duration.toDuration()));

  auto& [years, months, weeks, days] = duration;

  // Step 1.
  int32_t sign = DurationSign(duration.toDuration());

  auto report = [&](double v, const char* name, unsigned errorNumber) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, v);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, errorNumber, name,
                              numStr);
  };

  auto throwIfInvalid = [&](double v, const char* name) {
    // Step 2.a.
    if (!std::isfinite(v)) {
      report(v, name, JSMSG_TEMPORAL_DURATION_INVALID_NON_FINITE);
      return false;
    }

    // Steps 2.b-c.
    if ((v < 0 && sign > 0) || (v > 0 && sign < 0)) {
      report(v, name, JSMSG_TEMPORAL_DURATION_INVALID_SIGN);
      return false;
    }

    return true;
  };

  // Step 2.
  if (!throwIfInvalid(years, "years")) {
    return false;
  }
  if (!throwIfInvalid(months, "months")) {
    return false;
  }
  if (!throwIfInvalid(weeks, "weeks")) {
    return false;
  }
  if (!throwIfInvalid(days, "days")) {
    return false;
  }

  // Steps 3-4.
  if (std::abs(days) > ((int64_t(1) << 53) / 86400)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_INVALID_NORMALIZED_TIME);
    return false;
  }

  MOZ_ASSERT(IsValidDuration(duration.toDuration()));

  // Step 5.
  return true;
}

/**
 * DefaultTemporalLargestUnit ( years, months, weeks, days, hours, minutes,
 * seconds, milliseconds, microseconds )
 */
static TemporalUnit DefaultTemporalLargestUnit(const Duration& duration) {
  MOZ_ASSERT(IsIntegerDuration(duration));

  // Step 1.
  if (duration.years != 0) {
    return TemporalUnit::Year;
  }

  // Step 2.
  if (duration.months != 0) {
    return TemporalUnit::Month;
  }

  // Step 3.
  if (duration.weeks != 0) {
    return TemporalUnit::Week;
  }

  // Step 4.
  if (duration.days != 0) {
    return TemporalUnit::Day;
  }

  // Step 5.
  if (duration.hours != 0) {
    return TemporalUnit::Hour;
  }

  // Step 6.
  if (duration.minutes != 0) {
    return TemporalUnit::Minute;
  }

  // Step 7.
  if (duration.seconds != 0) {
    return TemporalUnit::Second;
  }

  // Step 8.
  if (duration.milliseconds != 0) {
    return TemporalUnit::Millisecond;
  }

  // Step 9.
  if (duration.microseconds != 0) {
    return TemporalUnit::Microsecond;
  }

  // Step 10.
  return TemporalUnit::Nanosecond;
}

/**
 * CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds [ , newTarget ] )
 */
static DurationObject* CreateTemporalDuration(JSContext* cx,
                                              const CallArgs& args,
                                              const Duration& duration) {
  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  // Step 1.
  if (!ThrowIfInvalidDuration(cx, duration)) {
    return nullptr;
  }

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Duration, &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<DurationObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Steps 4-13.
  // Add zero to convert -0 to +0.
  object->setFixedSlot(DurationObject::YEARS_SLOT, NumberValue(years + (+0.0)));
  object->setFixedSlot(DurationObject::MONTHS_SLOT,
                       NumberValue(months + (+0.0)));
  object->setFixedSlot(DurationObject::WEEKS_SLOT, NumberValue(weeks + (+0.0)));
  object->setFixedSlot(DurationObject::DAYS_SLOT, NumberValue(days + (+0.0)));
  object->setFixedSlot(DurationObject::HOURS_SLOT, NumberValue(hours + (+0.0)));
  object->setFixedSlot(DurationObject::MINUTES_SLOT,
                       NumberValue(minutes + (+0.0)));
  object->setFixedSlot(DurationObject::SECONDS_SLOT,
                       NumberValue(seconds + (+0.0)));
  object->setFixedSlot(DurationObject::MILLISECONDS_SLOT,
                       NumberValue(milliseconds + (+0.0)));
  object->setFixedSlot(DurationObject::MICROSECONDS_SLOT,
                       NumberValue(microseconds + (+0.0)));
  object->setFixedSlot(DurationObject::NANOSECONDS_SLOT,
                       NumberValue(nanoseconds + (+0.0)));

  // Step 14.
  return object;
}

/**
 * CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds [ , newTarget ] )
 */
DurationObject* js::temporal::CreateTemporalDuration(JSContext* cx,
                                                     const Duration& duration) {
  auto& [years, months, weeks, days, hours, minutes, seconds, milliseconds,
         microseconds, nanoseconds] = duration;

  MOZ_ASSERT(IsInteger(years));
  MOZ_ASSERT(IsInteger(months));
  MOZ_ASSERT(IsInteger(weeks));
  MOZ_ASSERT(IsInteger(days));
  MOZ_ASSERT(IsInteger(hours));
  MOZ_ASSERT(IsInteger(minutes));
  MOZ_ASSERT(IsInteger(seconds));
  MOZ_ASSERT(IsInteger(milliseconds));
  MOZ_ASSERT(IsInteger(microseconds));
  MOZ_ASSERT(IsInteger(nanoseconds));

  // Step 1.
  if (!ThrowIfInvalidDuration(cx, duration)) {
    return nullptr;
  }

  // Steps 2-3.
  auto* object = NewBuiltinClassInstance<DurationObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Steps 4-13.
  // Add zero to convert -0 to +0.
  object->setFixedSlot(DurationObject::YEARS_SLOT, NumberValue(years + (+0.0)));
  object->setFixedSlot(DurationObject::MONTHS_SLOT,
                       NumberValue(months + (+0.0)));
  object->setFixedSlot(DurationObject::WEEKS_SLOT, NumberValue(weeks + (+0.0)));
  object->setFixedSlot(DurationObject::DAYS_SLOT, NumberValue(days + (+0.0)));
  object->setFixedSlot(DurationObject::HOURS_SLOT, NumberValue(hours + (+0.0)));
  object->setFixedSlot(DurationObject::MINUTES_SLOT,
                       NumberValue(minutes + (+0.0)));
  object->setFixedSlot(DurationObject::SECONDS_SLOT,
                       NumberValue(seconds + (+0.0)));
  object->setFixedSlot(DurationObject::MILLISECONDS_SLOT,
                       NumberValue(milliseconds + (+0.0)));
  object->setFixedSlot(DurationObject::MICROSECONDS_SLOT,
                       NumberValue(microseconds + (+0.0)));
  object->setFixedSlot(DurationObject::NANOSECONDS_SLOT,
                       NumberValue(nanoseconds + (+0.0)));

  // Step 14.
  return object;
}

/**
 * ToIntegerIfIntegral ( argument )
 */
static bool ToIntegerIfIntegral(JSContext* cx, const char* name,
                                Handle<Value> argument, double* num) {
  // Step 1.
  double d;
  if (!JS::ToNumber(cx, argument, &d)) {
    return false;
  }

  // Step 2.
  if (!js::IsInteger(d)) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, d);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_NOT_INTEGER, numStr,
                              name);
    return false;
  }

  // Step 3.
  *num = d;
  return true;
}

/**
 * ToIntegerIfIntegral ( argument )
 */
static bool ToIntegerIfIntegral(JSContext* cx, Handle<PropertyName*> name,
                                Handle<Value> argument, double* result) {
  // Step 1.
  double d;
  if (!JS::ToNumber(cx, argument, &d)) {
    return false;
  }

  // Step 2.
  if (!js::IsInteger(d)) {
    if (auto nameStr = js::QuoteString(cx, name)) {
      ToCStringBuf cbuf;
      const char* numStr = NumberToCString(&cbuf, d);

      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_DURATION_NOT_INTEGER, numStr,
                                nameStr.get());
    }
    return false;
  }

  // Step 3.
  *result = d;
  return true;
}

/**
 * ToTemporalPartialDurationRecord ( temporalDurationLike )
 */
static bool ToTemporalPartialDurationRecord(
    JSContext* cx, Handle<JSObject*> temporalDurationLike, Duration* result) {
  // Steps 1-3. (Not applicable in our implementation.)

  Rooted<Value> value(cx);
  bool any = false;

  auto getDurationProperty = [&](Handle<PropertyName*> name, double* num) {
    if (!GetProperty(cx, temporalDurationLike, temporalDurationLike, name,
                     &value)) {
      return false;
    }

    if (!value.isUndefined()) {
      any = true;

      if (!ToIntegerIfIntegral(cx, name, value, num)) {
        return false;
      }
    }
    return true;
  };

  // Steps 4-23.
  if (!getDurationProperty(cx->names().days, &result->days)) {
    return false;
  }
  if (!getDurationProperty(cx->names().hours, &result->hours)) {
    return false;
  }
  if (!getDurationProperty(cx->names().microseconds, &result->microseconds)) {
    return false;
  }
  if (!getDurationProperty(cx->names().milliseconds, &result->milliseconds)) {
    return false;
  }
  if (!getDurationProperty(cx->names().minutes, &result->minutes)) {
    return false;
  }
  if (!getDurationProperty(cx->names().months, &result->months)) {
    return false;
  }
  if (!getDurationProperty(cx->names().nanoseconds, &result->nanoseconds)) {
    return false;
  }
  if (!getDurationProperty(cx->names().seconds, &result->seconds)) {
    return false;
  }
  if (!getDurationProperty(cx->names().weeks, &result->weeks)) {
    return false;
  }
  if (!getDurationProperty(cx->names().years, &result->years)) {
    return false;
  }

  // Step 24.
  if (!any) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_MISSING_UNIT);
    return false;
  }

  // Step 25.
  return true;
}

/**
 * ToTemporalDurationRecord ( temporalDurationLike )
 */
bool js::temporal::ToTemporalDurationRecord(JSContext* cx,
                                            Handle<Value> temporalDurationLike,
                                            Duration* result) {
  // Step 1.
  if (!temporalDurationLike.isObject()) {
    // Step 1.a.
    if (!temporalDurationLike.isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK,
                       temporalDurationLike, nullptr, "not a string");
      return false;
    }
    Rooted<JSString*> string(cx, temporalDurationLike.toString());

    // Step 1.b.
    return ParseTemporalDurationString(cx, string, result);
  }

  Rooted<JSObject*> durationLike(cx, &temporalDurationLike.toObject());

  // Step 2.
  if (auto* duration = durationLike->maybeUnwrapIf<DurationObject>()) {
    *result = ToDuration(duration);
    return true;
  }

  // Step 3.
  Duration duration = {};

  // Steps 4-14.
  if (!ToTemporalPartialDurationRecord(cx, durationLike, &duration)) {
    return false;
  }

  // Step 15.
  if (!ThrowIfInvalidDuration(cx, duration)) {
    return false;
  }

  // Step 16.
  *result = duration;
  return true;
}

/**
 * ToTemporalDuration ( item )
 */
Wrapped<DurationObject*> js::temporal::ToTemporalDuration(JSContext* cx,
                                                          Handle<Value> item) {
  // Step 1.
  if (item.isObject()) {
    JSObject* itemObj = &item.toObject();
    if (itemObj->canUnwrapAs<DurationObject>()) {
      return itemObj;
    }
  }

  // Step 2.
  Duration result;
  if (!ToTemporalDurationRecord(cx, item, &result)) {
    return nullptr;
  }

  // Step 3.
  return CreateTemporalDuration(cx, result);
}

/**
 * ToTemporalDuration ( item )
 */
bool js::temporal::ToTemporalDuration(JSContext* cx, Handle<Value> item,
                                      Duration* result) {
  auto obj = ToTemporalDuration(cx, item);
  if (!obj) {
    return false;
  }

  *result = ToDuration(&obj.unwrap());
  return true;
}

/**
 * DaysUntil ( earlier, later )
 */
int32_t js::temporal::DaysUntil(const PlainDate& earlier,
                                const PlainDate& later) {
  MOZ_ASSERT(ISODateTimeWithinLimits(earlier));
  MOZ_ASSERT(ISODateTimeWithinLimits(later));

  // Steps 1-2.
  int32_t epochDaysEarlier = MakeDay(earlier);
  MOZ_ASSERT(std::abs(epochDaysEarlier) <= 100'000'000);

  // Steps 3-4.
  int32_t epochDaysLater = MakeDay(later);
  MOZ_ASSERT(std::abs(epochDaysLater) <= 100'000'000);

  // Step 5.
  return epochDaysLater - epochDaysEarlier;
}

/**
 * MoveRelativeDate ( calendarRec, relativeTo, duration )
 */
static bool MoveRelativeDate(
    JSContext* cx, Handle<CalendarRecord> calendar,
    Handle<Wrapped<PlainDateObject*>> relativeTo, const Duration& duration,
    MutableHandle<Wrapped<PlainDateObject*>> relativeToResult,
    int32_t* daysResult) {
  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto relativeToDate = ToPlainDate(unwrappedRelativeTo);

  // Step 1.
  auto newDate = AddDate(cx, calendar, relativeTo, duration);
  if (!newDate) {
    return false;
  }
  auto later = ToPlainDate(&newDate.unwrap());
  relativeToResult.set(newDate);

  // Step 2.
  *daysResult = DaysUntil(relativeToDate, later);
  MOZ_ASSERT(std::abs(*daysResult) <= 200'000'000);

  // Step 3.
  return true;
}

/**
 * MoveRelativeZonedDateTime ( zonedDateTime, calendarRec, timeZoneRec, years,
 * months, weeks, days, precalculatedPlainDateTime )
 */
static bool MoveRelativeZonedDateTime(
    JSContext* cx, Handle<ZonedDateTime> zonedDateTime,
    Handle<CalendarRecord> calendar, Handle<TimeZoneRecord> timeZone,
    const DateDuration& duration,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    MutableHandle<ZonedDateTime> result) {
  // Step 1.
  MOZ_ASSERT(TimeZoneMethodsRecordHasLookedUp(
      timeZone, TimeZoneMethod::GetOffsetNanosecondsFor));

  // Step 2.
  MOZ_ASSERT(TimeZoneMethodsRecordHasLookedUp(
      timeZone, TimeZoneMethod::GetPossibleInstantsFor));

  // Step 3.
  Instant intermediateNs;
  if (precalculatedPlainDateTime) {
    if (!AddZonedDateTime(cx, zonedDateTime.instant(), timeZone, calendar,
                          duration, *precalculatedPlainDateTime,
                          &intermediateNs)) {
      return false;
    }
  } else {
    if (!AddZonedDateTime(cx, zonedDateTime.instant(), timeZone, calendar,
                          duration, &intermediateNs)) {
      return false;
    }
  }
  MOZ_ASSERT(IsValidEpochInstant(intermediateNs));

  // Step 4.
  result.set(ZonedDateTime{intermediateNs, zonedDateTime.timeZone(),
                           zonedDateTime.calendar()});
  return true;
}

/**
 * Split duration into full days and remainding nanoseconds.
 */
static NormalizedTimeAndDays NormalizedTimeDurationToDays(
    const NormalizedTimeDuration& duration) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration));

  auto [seconds, nanoseconds] = duration;
  if (seconds < 0 && nanoseconds > 0) {
    seconds += 1;
    nanoseconds -= 1'000'000'000;
  }

  int64_t days = seconds / ToSeconds(TemporalUnit::Day);
  seconds = seconds % ToSeconds(TemporalUnit::Day);

  int64_t time = seconds * ToNanoseconds(TemporalUnit::Second) + nanoseconds;

  constexpr int64_t dayLength = ToNanoseconds(TemporalUnit::Day);
  MOZ_ASSERT(std::abs(time) < dayLength);

  return {days, time, dayLength};
}

/**
 * CreateTimeDurationRecord ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static TimeDuration CreateTimeDurationRecord(int64_t days, int64_t hours,
                                             int64_t minutes, int64_t seconds,
                                             int64_t milliseconds,
                                             int64_t microseconds,
                                             int64_t nanoseconds) {
  // Step 1.
  MOZ_ASSERT(IsValidDuration(
      {0, 0, 0, double(days), double(hours), double(minutes), double(seconds),
       double(milliseconds), double(microseconds), double(nanoseconds)}));

  // Step 2.
  return {
      double(days),        double(hours),        double(minutes),
      double(seconds),     double(milliseconds), double(microseconds),
      double(nanoseconds),
  };
}

/**
 * CreateTimeDurationRecord ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static TimeDuration CreateTimeDurationRecord(double days, double hours,
                                             double minutes, double seconds,
                                             double milliseconds,
                                             double microseconds,
                                             double nanoseconds) {
  // Step 1.
  MOZ_ASSERT(IsValidDuration({0, 0, 0, days, hours, minutes, seconds,
                              milliseconds, microseconds, nanoseconds}));

  // Step 2.
  // NB: Adds +0.0 to correctly handle negative zero.
  return {
      days + (+0.0),        hours + (+0.0),        minutes + (+0.0),
      seconds + (+0.0),     milliseconds + (+0.0), microseconds + (+0.0),
      nanoseconds + (+0.0),
  };
}

/**
 * BalanceTimeDuration ( norm, largestUnit )
 */
TimeDuration js::temporal::BalanceTimeDuration(
    const NormalizedTimeDuration& duration, TemporalUnit largestUnit) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration));

  auto [seconds, nanoseconds] = duration;

  // Negative nanoseconds are represented as the difference to 1'000'000'000.
  // Convert these back to their absolute value and adjust the seconds part
  // accordingly.
  //
  // For example the nanoseconds duration |-1n| is represented as the
  // duration {seconds: -1, nanoseconds: 999'999'999}.
  if (seconds < 0 && nanoseconds > 0) {
    seconds += 1;
    nanoseconds -= ToNanoseconds(TemporalUnit::Second);
  }

  // Step 1.
  int64_t days = 0;
  int64_t hours = 0;
  int64_t minutes = 0;
  int64_t milliseconds = 0;
  int64_t microseconds = 0;

  // Steps 2-3. (Not applicable in our implementation.)
  //
  // We don't need to convert to positive numbers, because integer division
  // truncates and the %-operator has modulo semantics.

  // Steps 4-10.
  switch (largestUnit) {
    // Step 4.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day: {
      // Step 4.a.
      microseconds = nanoseconds / 1000;

      // Step 4.b.
      nanoseconds = nanoseconds % 1000;

      // Step 4.c.
      milliseconds = microseconds / 1000;

      // Step 4.d.
      microseconds = microseconds % 1000;

      // Steps 4.e-f. (Not applicable)
      MOZ_ASSERT(std::abs(milliseconds) <= 999);

      // Step 4.g.
      minutes = seconds / 60;

      // Step 4.h.
      seconds = seconds % 60;

      // Step 4.i.
      hours = minutes / 60;

      // Step 4.j.
      minutes = minutes % 60;

      // Step 4.k.
      days = hours / 24;

      // Step 4.l.
      hours = hours % 24;

      break;
    }

      // Step 5.
    case TemporalUnit::Hour: {
      // Step 5.a.
      microseconds = nanoseconds / 1000;

      // Step 5.b.
      nanoseconds = nanoseconds % 1000;

      // Step 5.c.
      milliseconds = microseconds / 1000;

      // Step 5.d.
      microseconds = microseconds % 1000;

      // Steps 5.e-f. (Not applicable)
      MOZ_ASSERT(std::abs(milliseconds) <= 999);

      // Step 5.g.
      minutes = seconds / 60;

      // Step 5.h.
      seconds = seconds % 60;

      // Step 5.i.
      hours = minutes / 60;

      // Step 5.j.
      minutes = minutes % 60;

      break;
    }

    case TemporalUnit::Minute: {
      // Step 6.a.
      microseconds = nanoseconds / 1000;

      // Step 6.b.
      nanoseconds = nanoseconds % 1000;

      // Step 6.c.
      milliseconds = microseconds / 1000;

      // Step 6.d.
      microseconds = microseconds % 1000;

      // Steps 6.e-f. (Not applicable)
      MOZ_ASSERT(std::abs(milliseconds) <= 999);

      // Step 6.g.
      minutes = seconds / 60;

      // Step 6.h.
      seconds = seconds % 60;

      break;
    }

    // Step 7.
    case TemporalUnit::Second: {
      // Step 7.a.
      microseconds = nanoseconds / 1000;

      // Step 7.b.
      nanoseconds = nanoseconds % 1000;

      // Step 7.c.
      milliseconds = microseconds / 1000;

      // Step 7.d.
      microseconds = microseconds % 1000;

      // Steps 7.e-f. (Not applicable)
      MOZ_ASSERT(std::abs(milliseconds) <= 999);

      break;
    }

      // Step 8.
    case TemporalUnit::Millisecond: {
      static_assert((NormalizedTimeDuration::max().seconds + 1) *
                            ToMilliseconds(TemporalUnit::Second) <=
                        INT64_MAX,
                    "total number duration milliseconds fits into int64");

      int64_t millis = seconds * ToMilliseconds(TemporalUnit::Second);

      // Set to zero per step 1.
      seconds = 0;

      // Step 8.a.
      microseconds = nanoseconds / 1000;

      // Step 8.b.
      nanoseconds = nanoseconds % 1000;

      // Step 8.c.
      milliseconds = microseconds / 1000;

      // Step 8.d.
      microseconds = microseconds % 1000;

      MOZ_ASSERT(std::abs(milliseconds) <= 999);
      milliseconds += millis;

      break;
    }

      // Step 9.
    case TemporalUnit::Microsecond: {
      // Step 9.a.
      int64_t microseconds = nanoseconds / 1000;

      // Step 9.b.
      nanoseconds = nanoseconds % 1000;

      MOZ_ASSERT(std::abs(microseconds) <= 999'999);
      double micros =
          std::fma(double(seconds), ToMicroseconds(TemporalUnit::Second),
                   double(microseconds));

      // Step 11.
      return CreateTimeDurationRecord(0, 0, 0, 0, 0, micros,
                                      double(nanoseconds));
    }

      // Step 10.
    case TemporalUnit::Nanosecond: {
      MOZ_ASSERT(std::abs(nanoseconds) <= 999'999'999);
      double nanos =
          std::fma(double(seconds), ToNanoseconds(TemporalUnit::Second),
                   double(nanoseconds));

      // Step 11.
      return CreateTimeDurationRecord(0, 0, 0, 0, 0, 0, nanos);
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 11.
  return CreateTimeDurationRecord(days, hours, minutes, seconds, milliseconds,
                                  microseconds, nanoseconds);
}

/**
 * BalanceTimeDurationRelative ( days, norm, largestUnit, zonedRelativeTo,
 * timeZoneRec, precalculatedPlainDateTime )
 */
static bool BalanceTimeDurationRelative(
    JSContext* cx, const NormalizedDuration& duration, TemporalUnit largestUnit,
    Handle<ZonedDateTime> relativeTo, Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    TimeDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 1.
  const auto& startNs = relativeTo.instant();

  // Step 2.
  const auto& startInstant = startNs;

  // Step 3.
  auto intermediateNs = startNs;

  // Step 4.
  PlainDateTime startDateTime;
  if (duration.date.days != 0) {
    // Step 4.a.
    if (!precalculatedPlainDateTime) {
      if (!GetPlainDateTimeFor(cx, timeZone, startInstant, &startDateTime)) {
        return false;
      }
      precalculatedPlainDateTime =
          mozilla::SomeRef<const PlainDateTime>(startDateTime);
    }

    // Steps 4.b-c.
    Rooted<CalendarValue> isoCalendar(cx, CalendarValue(cx->names().iso8601));
    if (!AddDaysToZonedDateTime(cx, startInstant, *precalculatedPlainDateTime,
                                timeZone, isoCalendar, duration.date.days,
                                &intermediateNs)) {
      return false;
    }
  }

  // Step 5.
  Instant endNs;
  if (!AddInstant(cx, intermediateNs, duration.time, &endNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(endNs));

  // Step 6.
  auto normalized =
      NormalizedTimeDurationFromEpochNanosecondsDifference(endNs, startInstant);

  // Step 7.
  if (normalized == NormalizedTimeDuration{}) {
    *result = {};
    return true;
  }

  // Steps 8-9.
  int64_t days = 0;
  if (TemporalUnit::Year <= largestUnit && largestUnit <= TemporalUnit::Day) {
    // Step 8.a.
    if (!precalculatedPlainDateTime) {
      if (!GetPlainDateTimeFor(cx, timeZone, startInstant, &startDateTime)) {
        return false;
      }
      precalculatedPlainDateTime =
          mozilla::SomeRef<const PlainDateTime>(startDateTime);
    }

    // Step 8.b.
    NormalizedTimeAndDays timeAndDays;
    if (!NormalizedTimeDurationToDays(cx, normalized, relativeTo, timeZone,
                                      *precalculatedPlainDateTime,
                                      &timeAndDays)) {
      return false;
    }

    // Step 8.c.
    days = timeAndDays.days;

    // Step 8.d.
    normalized = NormalizedTimeDuration::fromNanoseconds(timeAndDays.time);
    MOZ_ASSERT_IF(days > 0, normalized >= NormalizedTimeDuration{});
    MOZ_ASSERT_IF(days < 0, normalized <= NormalizedTimeDuration{});

    // Step 8.e.
    largestUnit = TemporalUnit::Hour;
  }

  // Step 10.
  auto balanceResult = BalanceTimeDuration(normalized, largestUnit);

  // Step 11.
  *result = {
      double(days),
      balanceResult.hours,
      balanceResult.minutes,
      balanceResult.seconds,
      balanceResult.milliseconds,
      balanceResult.microseconds,
      balanceResult.nanoseconds,
  };
  MOZ_ASSERT(IsValidDuration(result->toDuration()));
  return true;
}

/**
 * CreateDateDurationRecord ( years, months, weeks, days )
 */
static DateDuration CreateDateDurationRecord(double years, double months,
                                             double weeks, double days) {
  MOZ_ASSERT(IsValidDuration(Duration{years, months, weeks, days}));
  return {years, months, weeks, days};
}

/**
 * CreateDateDurationRecord ( years, months, weeks, days )
 */
static bool CreateDateDurationRecord(JSContext* cx, double years, double months,
                                     double weeks, double days,
                                     DateDuration* result) {
  auto duration = DateDuration{years, months, weeks, days};
  if (!ThrowIfInvalidDuration(cx, duration)) {
    return false;
  }

  *result = duration;
  return true;
}

static bool UnbalanceDateDurationRelativeHasEffect(const DateDuration& duration,
                                                   TemporalUnit largestUnit) {
  MOZ_ASSERT(largestUnit != TemporalUnit::Auto);

  // Steps 2, 3.a-b, 4.a-b, 6-7.
  return (largestUnit > TemporalUnit::Year && duration.years != 0) ||
         (largestUnit > TemporalUnit::Month && duration.months != 0) ||
         (largestUnit > TemporalUnit::Week && duration.weeks != 0);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo, calendarRec )
 */
static bool UnbalanceDateDurationRelative(
    JSContext* cx, const DateDuration& duration, TemporalUnit largestUnit,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration.toDuration()));

  auto [years, months, weeks, days] = duration;

  // Step 1. (Not applicable in our implementation.)

  // Steps 2, 3.a, 4.a, and 6.
  if (!UnbalanceDateDurationRelativeHasEffect(duration, largestUnit)) {
    // Steps 2.a, 3.a, 4.a, and 6.
    *result = duration;
    return true;
  }

  // Step 3.
  if (largestUnit == TemporalUnit::Month) {
    // Step 3.a. (Handled above)
    MOZ_ASSERT(years != 0);

    // Step 3.b. (Not applicable in our implementation.)

    // Step 3.c.
    MOZ_ASSERT(
        CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateAdd));

    // Step 3.d.
    MOZ_ASSERT(
        CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateUntil));

    // Step 3.e.
    auto yearsDuration = Duration{years};

    // Step 3.f.
    Rooted<Wrapped<PlainDateObject*>> later(
        cx, CalendarDateAdd(cx, calendar, plainRelativeTo, yearsDuration));
    if (!later) {
      return false;
    }

    // Steps 3.g-i.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, plainRelativeTo, later,
                           TemporalUnit::Month, &untilResult)) {
      return false;
    }

    // Step 3.j.
    double yearsInMonths = untilResult.months;

    // Step 3.k.
    //
    // The addition |months + yearsInMonths| can be imprecise, but this is
    // safe to ignore, because all values are passed to
    // CreateDateDurationRecord, which converts the values to Numbers.
    return CreateDateDurationRecord(cx, 0, months + yearsInMonths, weeks, days,
                                    result);
  }

  // Step 4.
  if (largestUnit == TemporalUnit::Week) {
    // Step 4.a. (Handled above)
    MOZ_ASSERT(years != 0 || months != 0);

    // Step 4.b. (Not applicable in our implementation.)

    // Step 4.c.
    MOZ_ASSERT(
        CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateAdd));

    // Step 4.d.
    auto yearsMonthsDuration = Duration{years, months};

    // Step 4.e.
    auto later =
        CalendarDateAdd(cx, calendar, plainRelativeTo, yearsMonthsDuration);
    if (!later) {
      return false;
    }
    auto laterDate = ToPlainDate(&later.unwrap());

    auto* unwrappedRelativeTo = plainRelativeTo.unwrap(cx);
    if (!unwrappedRelativeTo) {
      return false;
    }
    auto relativeToDate = ToPlainDate(unwrappedRelativeTo);

    // Step 4.f.
    int32_t yearsMonthsInDays = DaysUntil(relativeToDate, laterDate);

    // Step 4.g.
    //
    // The addition |days + yearsMonthsInDays| can be imprecise, but this is
    // safe to ignore, because all values are passed to
    // CreateDateDurationRecord, which converts the values to Numbers.
    return CreateDateDurationRecord(cx, 0, 0, weeks, days + yearsMonthsInDays,
                                    result);
  }

  // Step 5. (Not applicable in our implementation.)

  // Step 6. (Handled above)
  MOZ_ASSERT(years != 0 || months != 0 || weeks != 0);

  // FIXME: why don't we unconditionally throw an error for missing calendars?

  // Step 7. (Not applicable in our implementation.)

  // Step 8.
  MOZ_ASSERT(
      CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateAdd));

  // Step 9.
  auto yearsMonthsWeeksDuration = Duration{years, months, weeks};

  // Step 10.
  auto later =
      CalendarDateAdd(cx, calendar, plainRelativeTo, yearsMonthsWeeksDuration);
  if (!later) {
    return false;
  }
  auto laterDate = ToPlainDate(&later.unwrap());

  auto* unwrappedRelativeTo = plainRelativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto relativeToDate = ToPlainDate(unwrappedRelativeTo);

  // Step 11.
  int32_t yearsMonthsWeeksInDay = DaysUntil(relativeToDate, laterDate);

  // Step 12.
  //
  // The addition |days + yearsMonthsWeeksInDay| can be imprecise, but this is
  // safe to ignore, because all values are passed to CreateDateDurationRecord,
  // which converts the values to Numbers.
  return CreateDateDurationRecord(cx, 0, 0, 0, days + yearsMonthsWeeksInDay,
                                  result);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo, calendarRec )
 */
static bool UnbalanceDateDurationRelative(JSContext* cx,
                                          const DateDuration& duration,
                                          TemporalUnit largestUnit,
                                          DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration.toDuration()));

  // Step 1. (Not applicable.)

  // Steps 2, 3.a, 4.a, and 6.
  if (!UnbalanceDateDurationRelativeHasEffect(duration, largestUnit)) {
    // Steps 2.a, 3.a, 4.a, and 6.
    *result = duration;
    return true;
  }

  // Step 5. (Not applicable.)

  // Steps 3.b, 4.b, and 7.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
  return false;
}

/**
 * BalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * smallestUnit, plainRelativeTo, calendarRec )
 */
static bool BalanceDateDurationRelative(
    JSContext* cx, const DateDuration& duration, TemporalUnit largestUnit,
    TemporalUnit smallestUnit,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration.toDuration()));
  MOZ_ASSERT(largestUnit <= smallestUnit);

  auto [years, months, weeks, days] = duration;

  // FIXME: spec issue - effectful code paths should be more fine-grained
  // similar to UnbalanceDateDurationRelative. For example:
  // 1. If largestUnit = "year" and days = 0 and months = 0, then no-op.
  // 2. Else if largestUnit = "month" and days = 0, then no-op.
  // 3. Else if days = 0, then no-op.
  //
  // Also note that |weeks| is never balanced, even when non-zero.

  // Step 1. (Not applicable in our implementation.)

  // Steps 2-4.
  if (largestUnit > TemporalUnit::Week ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 4.a.
    *result = duration;
    return true;
  }

  // Step 5.
  if (!plainRelativeTo) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // Step 6.
  MOZ_ASSERT(
      CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateAdd));

  // Step 7.
  MOZ_ASSERT(
      CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateUntil));

  // Steps 8-9. (Not applicable in our implementation.)

  auto untilAddedDate = [&](const Duration& duration, Duration* untilResult) {
    Rooted<Wrapped<PlainDateObject*>> later(
        cx, AddDate(cx, calendar, plainRelativeTo, duration));
    if (!later) {
      return false;
    }

    return CalendarDateUntil(cx, calendar, plainRelativeTo, later, largestUnit,
                             untilResult);
  };

  // Step 10.
  if (largestUnit == TemporalUnit::Year) {
    // Step 10.a.
    if (smallestUnit == TemporalUnit::Week) {
      // Step 10.a.i.
      MOZ_ASSERT(days == 0);

      // Step 10.a.ii.
      auto yearsMonthsDuration = Duration{years, months};

      // Steps 10.a.iii-iv.
      Duration untilResult;
      if (!untilAddedDate(yearsMonthsDuration, &untilResult)) {
        return false;
      }

      // FIXME: spec bug - CreateDateDurationRecord is infallible

      // Step 10.a.v.
      *result = CreateDateDurationRecord(untilResult.years, untilResult.months,
                                         weeks, 0);
      return true;
    }

    // Step 10.b.
    auto yearsMonthsWeeksDaysDuration = Duration{years, months, weeks, days};

    // Steps 10.c-d.
    Duration untilResult;
    if (!untilAddedDate(yearsMonthsWeeksDaysDuration, &untilResult)) {
      return false;
    }

    // FIXME: spec bug - CreateDateDurationRecord is infallible
    // https://github.com/tc39/proposal-temporal/issues/2750

    // Step 10.e.
    *result = CreateDateDurationRecord(untilResult.years, untilResult.months,
                                       untilResult.weeks, untilResult.days);
    return true;
  }

  // Step 11.
  if (largestUnit == TemporalUnit::Month) {
    // Step 11.a.
    MOZ_ASSERT(years == 0);

    // Step 11.b.
    if (smallestUnit == TemporalUnit::Week) {
      // Step 10.b.i.
      MOZ_ASSERT(days == 0);

      // Step 10.b.ii.
      *result = CreateDateDurationRecord(0, months, weeks, 0);
      return true;
    }

    // Step 11.c.
    auto monthsWeeksDaysDuration = Duration{0, months, weeks, days};

    // Steps 11.d-e.
    Duration untilResult;
    if (!untilAddedDate(monthsWeeksDaysDuration, &untilResult)) {
      return false;
    }

    // FIXME: spec bug - CreateDateDurationRecord is infallible
    // https://github.com/tc39/proposal-temporal/issues/2750

    // Step 11.f.
    *result = CreateDateDurationRecord(0, untilResult.months, untilResult.weeks,
                                       untilResult.days);
    return true;
  }

  // Step 12.
  MOZ_ASSERT(largestUnit == TemporalUnit::Week);

  // Step 13.
  MOZ_ASSERT(years == 0);

  // Step 14.
  MOZ_ASSERT(months == 0);

  // Step 15.
  auto weeksDaysDuration = Duration{0, 0, weeks, days};

  // Steps 16-17.
  Duration untilResult;
  if (!untilAddedDate(weeksDaysDuration, &untilResult)) {
    return false;
  }

  // FIXME: spec bug - CreateDateDurationRecord is infallible
  // https://github.com/tc39/proposal-temporal/issues/2750

  // Step 18.
  *result = CreateDateDurationRecord(0, 0, untilResult.weeks, untilResult.days);
  return true;
}

/**
 * BalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * smallestUnit, plainRelativeTo, calendarRec )
 */
bool js::temporal::BalanceDateDurationRelative(
    JSContext* cx, const DateDuration& duration, TemporalUnit largestUnit,
    TemporalUnit smallestUnit,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, DateDuration* result) {
  MOZ_ASSERT(plainRelativeTo);
  MOZ_ASSERT(calendar.receiver());

  return ::BalanceDateDurationRelative(cx, duration, largestUnit, smallestUnit,
                                       plainRelativeTo, calendar, result);
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, calendarRec,
 * zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Duration* result) {
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));

  // Steps 1-2. (Not applicable)

  // Step 3.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 4.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 5.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Step 6.a.
  if (largestUnit <= TemporalUnit::Week) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // Step 6.b.
  auto normalized1 = NormalizeTimeDuration(one);

  // Step 6.c.
  auto normalized2 = NormalizeTimeDuration(two);

  // Step 6.d.
  NormalizedTimeDuration normalized;
  if (!AddNormalizedTimeDuration(cx, normalized1, normalized2, &normalized)) {
    return false;
  }

  // Step 6.e.
  if (!Add24HourDaysToNormalizedTimeDuration(
          cx, normalized, one.days + two.days, &normalized)) {
    return false;
  }

  // Step 6.f.
  auto balanced = temporal::BalanceTimeDuration(normalized, largestUnit);

  // Steps 6.g.
  *result = balanced.toDuration();
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, calendarRec,
 * zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
                        Handle<CalendarRecord> calendar, Duration* result) {
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));

  // Steps 1-2. (Not applicable)

  // FIXME: spec issue - calendarRec is not undefined when plainRelativeTo is
  // not undefined.

  // Step 3.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 4.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 5.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Step 6. (Not applicable)

  // Step 7.a. (Not applicable in our implementation.)

  // Step 7.b.
  auto dateDuration1 = Duration{one.years, one.months, one.weeks, one.days};

  // Step 7.c.
  auto dateDuration2 = Duration{two.years, two.months, two.weeks, two.days};

  // FIXME: spec issue - calendarUnitsPresent is unused.

  // Step 7.d.
  [[maybe_unused]] bool calendarUnitsPresent = true;

  // Step 7.e.
  if (dateDuration1.years == 0 && dateDuration1.months == 0 &&
      dateDuration1.weeks == 0 && dateDuration2.years == 0 &&
      dateDuration2.months == 0 && dateDuration2.weeks == 0) {
    calendarUnitsPresent = false;
  }

  // Step 7.f.
  Rooted<Wrapped<PlainDateObject*>> intermediate(
      cx, AddDate(cx, calendar, plainRelativeTo, dateDuration1));
  if (!intermediate) {
    return false;
  }

  // Step 7.g.
  Rooted<Wrapped<PlainDateObject*>> end(
      cx, AddDate(cx, calendar, intermediate, dateDuration2));
  if (!end) {
    return false;
  }

  // Step 7.h.
  auto dateLargestUnit = std::min(TemporalUnit::Day, largestUnit);

  // Steps 7.i-k.
  Duration dateDifference;
  if (!DifferenceDate(cx, calendar, plainRelativeTo, end, dateLargestUnit,
                      &dateDifference)) {
    return false;
  }

  // Step 7.l.
  auto normalized1 = NormalizeTimeDuration(one);

  // Step 7.m.
  auto normalized2 = NormalizeTimeDuration(two);

  // Step 7.n.
  NormalizedTimeDuration normalized1WithDays;
  if (!Add24HourDaysToNormalizedTimeDuration(
          cx, normalized1, dateDifference.days, &normalized1WithDays)) {
    return false;
  }

  // Step 7.o.
  NormalizedTimeDuration normalized;
  if (!AddNormalizedTimeDuration(cx, normalized1WithDays, normalized2,
                                 &normalized)) {
    return false;
  }

  // Step 7.p.
  auto balanced = temporal::BalanceTimeDuration(normalized, largestUnit);

  // Steps 7.q.
  *result = {
      dateDifference.years, dateDifference.months, dateDifference.weeks,
      balanced.days,        balanced.hours,        balanced.minutes,
      balanced.seconds,     balanced.milliseconds, balanced.microseconds,
      balanced.nanoseconds,
  };
  MOZ_ASSERT(IsValidDuration(*result));
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, calendarRec,
 * zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
 */
static bool AddDuration(
    JSContext* cx, const Duration& one, const Duration& two,
    Handle<ZonedDateTime> zonedRelativeTo, Handle<CalendarRecord> calendar,
    Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    Duration* result) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 4.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 5.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Steps 6-7. (Not applicable)

  // Steps 8-9. (Not applicable in our implementation.)

  // FIXME: spec issue - GetPlainDateTimeFor called unnecessarily
  //
  // clang-format off
  //
  // 10. If largestUnit is one of "year", "month", "week", or "day", then
  //   a. If precalculatedPlainDateTime is undefined, then
  //     i. Let startDateTime be ? GetPlainDateTimeFor(timeZone, zonedRelativeTo.[[Nanoseconds]], calendar).
  //   b. Else,
  //     i. Let startDateTime be precalculatedPlainDateTime.
  //   c. Let intermediateNs be ? AddZonedDateTime(zonedRelativeTo.[[Nanoseconds]], timeZone, calendar, y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, startDateTime).
  //   d. Let endNs be ? AddZonedDateTime(intermediateNs, timeZone, calendar, y2, mon2, w2, d2, h2, min2, s2, ms2, mus2, ns2).
  //   e. Return ? DifferenceZonedDateTime(zonedRelativeTo.[[Nanoseconds]], endNs, timeZone, calendar, largestUnit, OrdinaryObjectCreate(null), startDateTime).
  // 11. Let intermediateNs be ? AddInstant(zonedRelativeTo.[[Nanoseconds]], h1, min1, s1, ms1, mus1, ns1).
  // 12. Let endNs be ? AddInstant(intermediateNs, h2, min2, s2, ms2, mus2, ns2).
  // 13. Let result be DifferenceInstant(zonedRelativeTo.[[Nanoseconds]], endNs, 1, "nanosecond", largestUnit, "halfExpand").
  // 14. Return ! CreateDurationRecord(0, 0, 0, 0, result.[[Hours]], result.[[Minutes]], result.[[Seconds]], result.[[Milliseconds]], result.[[Microseconds]], result.[[Nanoseconds]]).
  //
  // clang-format on

  // Step 10.
  bool startDateTimeNeeded = largestUnit <= TemporalUnit::Day;

  // Steps 11-17.
  if (!startDateTimeNeeded) {
    // Steps 11-12. (Not applicable)

    // Step 13.
    auto normalized1 = NormalizeTimeDuration(one);

    // Step 14.
    auto normalized2 = NormalizeTimeDuration(two);

    // Step 15. (Inlined AddZonedDateTime, step 6.)
    Instant intermediateNs;
    if (!AddInstant(cx, zonedRelativeTo.instant(), normalized1,
                    &intermediateNs)) {
      return false;
    }
    MOZ_ASSERT(IsValidEpochInstant(intermediateNs));

    // Step 16. (Inlined AddZonedDateTime, step 6.)
    Instant endNs;
    if (!AddInstant(cx, intermediateNs, normalized2, &endNs)) {
      return false;
    }
    MOZ_ASSERT(IsValidEpochInstant(endNs));

    // Step 17.a.
    auto normalized = NormalizedTimeDurationFromEpochNanosecondsDifference(
        endNs, zonedRelativeTo.instant());

    // Step 17.b.
    auto balanced = BalanceTimeDuration(normalized, largestUnit);

    // Step 17.c.
    *result = balanced.toDuration();
    return true;
  }

  // Steps 11-12.
  PlainDateTime startDateTime;
  if (!precalculatedPlainDateTime) {
    if (!GetPlainDateTimeFor(cx, timeZone, zonedRelativeTo.instant(),
                             &startDateTime)) {
      return false;
    }
  } else {
    startDateTime = *precalculatedPlainDateTime;
  }

  // Step 13.
  auto normalized1 = CreateNormalizedDurationRecord(one);

  // Step 14.
  auto normalized2 = CreateNormalizedDurationRecord(two);

  // Step 15.
  Instant intermediateNs;
  if (!AddZonedDateTime(cx, zonedRelativeTo.instant(), timeZone, calendar,
                        normalized1, startDateTime, &intermediateNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(intermediateNs));

  // Step 16.
  Instant endNs;
  if (!AddZonedDateTime(cx, intermediateNs, timeZone, calendar, normalized2,
                        &endNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(endNs));

  // Step 17. (Not applicable)

  // Step 18.
  NormalizedDuration difference;
  if (!DifferenceZonedDateTime(cx, zonedRelativeTo.instant(), endNs, timeZone,
                               calendar, largestUnit, startDateTime,
                               &difference)) {
    return false;
  }

  // Step 19.
  auto balanced = BalanceTimeDuration(difference.time, TemporalUnit::Hour);

  // Step 20.
  *result = {
      difference.date.years, difference.date.months, difference.date.weeks,
      difference.date.days,  balanced.hours,         balanced.minutes,
      balanced.seconds,      balanced.milliseconds,  balanced.microseconds,
      balanced.nanoseconds,
  };
  MOZ_ASSERT(IsValidDuration(*result));
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, calendarRec,
 * zonedRelativeTo, timeZoneRec [ , precalculatedPlainDateTime ] )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<ZonedDateTime> zonedRelativeTo,
                        Handle<CalendarRecord> calendar,
                        Handle<TimeZoneRecord> timeZone, Duration* result) {
  return AddDuration(cx, one, two, zonedRelativeTo, calendar, timeZone,
                     mozilla::Nothing(), result);
}

/**
 * AdjustRoundedDurationDays ( years, months, weeks, days, norm, increment,
 * unit, roundingMode, zonedRelativeTo, calendarRec, timeZoneRec,
 * precalculatedPlainDateTime )
 */
static bool AdjustRoundedDurationDays(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<ZonedDateTime> zonedRelativeTo, Handle<CalendarRecord> calendar,
    Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    NormalizedDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 1.
  if ((TemporalUnit::Year <= unit && unit <= TemporalUnit::Day) ||
      (unit == TemporalUnit::Nanosecond && increment == Increment{1})) {
    *result = duration;
    return true;
  }

  // The increment is limited for all smaller temporal units.
  MOZ_ASSERT(increment < MaximumTemporalDurationRoundingIncrement(unit));

  // Step 2.
  MOZ_ASSERT(precalculatedPlainDateTime);

  // Step 3.
  int32_t direction = NormalizedTimeDurationSign(duration.time);

  // Steps 4-5.
  Instant dayStart;
  if (!AddZonedDateTime(cx, zonedRelativeTo.instant(), timeZone, calendar,
                        duration.date, *precalculatedPlainDateTime,
                        &dayStart)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(dayStart));

  // Step 6.
  PlainDateTime dayStartDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, dayStart, &dayStartDateTime)) {
    return false;
  }

  // Step 7.
  Instant dayEnd;
  if (!AddDaysToZonedDateTime(cx, dayStart, dayStartDateTime, timeZone,
                              zonedRelativeTo.calendar(), direction, &dayEnd)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(dayEnd));

  // Step 8.
  auto dayLengthNs =
      NormalizedTimeDurationFromEpochNanosecondsDifference(dayEnd, dayStart);
  MOZ_ASSERT(IsValidInstantSpan(dayLengthNs.to<InstantSpan>()));

  // Step 9.
  NormalizedTimeDuration oneDayLess;
  if (!SubtractNormalizedTimeDuration(cx, duration.time, dayLengthNs,
                                      &oneDayLess)) {
    return false;
  }

  // Step 10.
  int32_t oneDayLessSign = NormalizedTimeDurationSign(oneDayLess);
  if ((direction > 0 && oneDayLessSign < 0) ||
      (direction < 0 && oneDayLessSign > 0)) {
    *result = duration;
    return true;
  }

  // Step 11.
  Duration adjustedDateDuration;
  if (!AddDuration(cx, duration.date.toDuration(), {0, 0, 0, double(direction)},
                   zonedRelativeTo, calendar, timeZone,
                   precalculatedPlainDateTime, &adjustedDateDuration)) {
    return false;
  }

  // Step 12.
  auto roundedTime = RoundDuration(oneDayLess, increment, unit, roundingMode);

  // Step 13.
  return CombineDateAndNormalizedTimeDuration(
      cx, adjustedDateDuration.toDateDuration(), roundedTime, result);
}

/**
 * AdjustRoundedDurationDays ( years, months, weeks, days, norm, increment,
 * unit, roundingMode, zonedRelativeTo, calendarRec, timeZoneRec,
 * precalculatedPlainDateTime )
 */
bool js::temporal::AdjustRoundedDurationDays(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<ZonedDateTime> zonedRelativeTo, Handle<CalendarRecord> calendar,
    Handle<TimeZoneRecord> timeZone,
    const PlainDateTime& precalculatedPlainDateTime,
    NormalizedDuration* result) {
  return ::AdjustRoundedDurationDays(
      cx, duration, increment, unit, roundingMode, zonedRelativeTo, calendar,
      timeZone, mozilla::SomeRef(precalculatedPlainDateTime), result);
}

static bool BigIntToStringBuilder(JSContext* cx, Handle<BigInt*> num,
                                  JSStringBuilder& sb) {
  MOZ_ASSERT(!num->isNegative());

  JSLinearString* str = BigInt::toString<CanGC>(cx, num, 10);
  if (!str) {
    return false;
  }
  return sb.append(str);
}

static bool NumberToStringBuilder(JSContext* cx, int64_t num,
                                  JSStringBuilder& sb) {
  MOZ_ASSERT(num >= 0);
  MOZ_ASSERT(num < int64_t(DOUBLE_INTEGRAL_PRECISION_LIMIT));

  ToCStringBuf cbuf;
  size_t length;
  const char* numStr = NumberToCString(&cbuf, num, &length);

  return sb.append(numStr, length);
}

static bool NumberToStringBuilder(JSContext* cx, double num,
                                  JSStringBuilder& sb) {
  MOZ_ASSERT(IsInteger(num));
  MOZ_ASSERT(num >= 0);

  if (num < DOUBLE_INTEGRAL_PRECISION_LIMIT) {
    ToCStringBuf cbuf;
    size_t length;
    const char* numStr = NumberToCString(&cbuf, num, &length);

    return sb.append(numStr, length);
  }

  Rooted<BigInt*> bi(cx, BigInt::createFromDouble(cx, num));
  if (!bi) {
    return false;
  }
  return BigIntToStringBuilder(cx, bi, sb);
}

static Duration AbsoluteDuration(const Duration& duration) {
  return {
      std::abs(duration.years),        std::abs(duration.months),
      std::abs(duration.weeks),        std::abs(duration.days),
      std::abs(duration.hours),        std::abs(duration.minutes),
      std::abs(duration.seconds),      std::abs(duration.milliseconds),
      std::abs(duration.microseconds), std::abs(duration.nanoseconds),
  };
}

/**
 * FormatFractionalSeconds ( subSecondNanoseconds, precision )
 */
[[nodiscard]] static bool FormatFractionalSeconds(JSStringBuilder& result,
                                                  int32_t subSecondNanoseconds,
                                                  Precision precision) {
  MOZ_ASSERT(0 <= subSecondNanoseconds && subSecondNanoseconds < 1'000'000'000);
  MOZ_ASSERT(precision != Precision::Minute());

  // Steps 1-2.
  if (precision == Precision::Auto()) {
    // Step 1.a.
    if (subSecondNanoseconds == 0) {
      return true;
    }

    // Step 3. (Reordered)
    if (!result.append('.')) {
      return false;
    }

    // Steps 1.b-c.
    uint32_t k = 100'000'000;
    do {
      if (!result.append(char('0' + (subSecondNanoseconds / k)))) {
        return false;
      }
      subSecondNanoseconds %= k;
      k /= 10;
    } while (subSecondNanoseconds);
  } else {
    // Step 2.a.
    uint8_t p = precision.value();
    if (p == 0) {
      return true;
    }

    // Step 3. (Reordered)
    if (!result.append('.')) {
      return false;
    }

    // Steps 2.b-c.
    uint32_t k = 100'000'000;
    for (uint8_t i = 0; i < precision.value(); i++) {
      if (!result.append(char('0' + (subSecondNanoseconds / k)))) {
        return false;
      }
      subSecondNanoseconds %= k;
      k /= 10;
    }
  }

  return true;
}

/**
 * TemporalDurationToString ( years, months, weeks, days, hours, minutes,
 * normSeconds, precision )
 */
static JSString* TemporalDurationToString(JSContext* cx,
                                          const Duration& duration,
                                          Precision precision) {
  MOZ_ASSERT(IsValidDuration(duration));
  MOZ_ASSERT(precision != Precision::Minute());

  // Convert to absolute values up front. This is okay to do, because when the
  // duration is valid, all components have the same sign.
  const auto& [years, months, weeks, days, hours, minutes, seconds,
               milliseconds, microseconds, nanoseconds] =
      AbsoluteDuration(duration);

  // Fast path for zero durations.
  if (years == 0 && months == 0 && weeks == 0 && days == 0 && hours == 0 &&
      minutes == 0 && seconds == 0 && milliseconds == 0 && microseconds == 0 &&
      nanoseconds == 0 &&
      (precision == Precision::Auto() || precision.value() == 0)) {
    return NewStringCopyZ<CanGC>(cx, "PT0S");
  }

  auto secondsDuration = NormalizeTimeDuration(0.0, 0.0, seconds, milliseconds,
                                               microseconds, nanoseconds);

  // Step 1.
  int32_t sign = DurationSign(duration);

  // Steps 2 and 7.
  JSStringBuilder result(cx);

  // Step 13. (Reordered)
  if (sign < 0) {
    if (!result.append('-')) {
      return nullptr;
    }
  }

  // Step 14. (Reordered)
  if (!result.append('P')) {
    return nullptr;
  }

  // Step 3.
  if (years != 0) {
    if (!NumberToStringBuilder(cx, years, result)) {
      return nullptr;
    }
    if (!result.append('Y')) {
      return nullptr;
    }
  }

  // Step 4.
  if (months != 0) {
    if (!NumberToStringBuilder(cx, months, result)) {
      return nullptr;
    }
    if (!result.append('M')) {
      return nullptr;
    }
  }

  // Step 5.
  if (weeks != 0) {
    if (!NumberToStringBuilder(cx, weeks, result)) {
      return nullptr;
    }
    if (!result.append('W')) {
      return nullptr;
    }
  }

  // Step 6.
  if (days != 0) {
    if (!NumberToStringBuilder(cx, days, result)) {
      return nullptr;
    }
    if (!result.append('D')) {
      return nullptr;
    }
  }

  // Step 7. (Moved above)

  // Steps 10-11. (Reordered)
  bool zeroMinutesAndHigher = years == 0 && months == 0 && weeks == 0 &&
                              days == 0 && hours == 0 && minutes == 0;

  // Steps 8-9, 12, and 15.
  bool hasSecondsPart = (secondsDuration != NormalizedTimeDuration{}) ||
                        zeroMinutesAndHigher || precision != Precision::Auto();
  if (hours != 0 || minutes != 0 || hasSecondsPart) {
    // Step 15. (Reordered)
    if (!result.append('T')) {
      return nullptr;
    }

    // Step 8.
    if (hours != 0) {
      if (!NumberToStringBuilder(cx, hours, result)) {
        return nullptr;
      }
      if (!result.append('H')) {
        return nullptr;
      }
    }

    // Step 9.
    if (minutes != 0) {
      if (!NumberToStringBuilder(cx, minutes, result)) {
        return nullptr;
      }
      if (!result.append('M')) {
        return nullptr;
      }
    }

    // Step 12.
    if (hasSecondsPart) {
      // Step 12.a.
      if (!NumberToStringBuilder(cx, secondsDuration.seconds, result)) {
        return nullptr;
      }

      // Step 12.b.
      if (!FormatFractionalSeconds(result, secondsDuration.nanoseconds,
                                   precision)) {
        return nullptr;
      }

      // Step 12.c.
      if (!result.append('S')) {
        return nullptr;
      }
    }
  }

  // Steps 13-15. (Moved above)

  // Step 16.
  return result.finishString();
}

/**
 * ToRelativeTemporalObject ( options )
 */
static bool ToRelativeTemporalObject(
    JSContext* cx, Handle<JSObject*> options,
    MutableHandle<Wrapped<PlainDateObject*>> plainRelativeTo,
    MutableHandle<ZonedDateTime> zonedRelativeTo,
    MutableHandle<TimeZoneRecord> timeZoneRecord) {
  // Step 1.
  Rooted<Value> value(cx);
  if (!GetProperty(cx, options, options, cx->names().relativeTo, &value)) {
    return false;
  }

  // Step 2.
  if (value.isUndefined()) {
    // FIXME: spec issue - switch return record fields for consistency.
    // FIXME: spec bug - [[TimeZoneRec]] field not created

    plainRelativeTo.set(nullptr);
    zonedRelativeTo.set(ZonedDateTime{});
    timeZoneRecord.set(TimeZoneRecord{});
    return true;
  }

  // Step 3.
  auto offsetBehaviour = OffsetBehaviour::Option;

  // Step 4.
  auto matchBehaviour = MatchBehaviour::MatchExactly;

  // Steps 5-6.
  PlainDateTime dateTime;
  Rooted<CalendarValue> calendar(cx);
  Rooted<TimeZoneValue> timeZone(cx);
  int64_t offsetNs;
  if (value.isObject()) {
    Rooted<JSObject*> obj(cx, &value.toObject());

    // Step 5.a.
    if (auto* zonedDateTime = obj->maybeUnwrapIf<ZonedDateTimeObject>()) {
      auto instant = ToInstant(zonedDateTime);
      Rooted<TimeZoneValue> timeZone(cx, zonedDateTime->timeZone());
      Rooted<CalendarValue> calendar(cx, zonedDateTime->calendar());

      if (!timeZone.wrap(cx)) {
        return false;
      }
      if (!calendar.wrap(cx)) {
        return false;
      }

      // Step 5.a.i.
      Rooted<TimeZoneRecord> timeZoneRec(cx);
      if (!CreateTimeZoneMethodsRecord(
              cx, timeZone,
              {
                  TimeZoneMethod::GetOffsetNanosecondsFor,
                  TimeZoneMethod::GetPossibleInstantsFor,
              },
              &timeZoneRec)) {
        return false;
      }

      // Step 5.a.ii.
      plainRelativeTo.set(nullptr);
      zonedRelativeTo.set(ZonedDateTime{instant, timeZone, calendar});
      timeZoneRecord.set(timeZoneRec);
      return true;
    }

    // Step 5.b.
    if (obj->canUnwrapAs<PlainDateObject>()) {
      plainRelativeTo.set(obj);
      zonedRelativeTo.set(ZonedDateTime{});
      timeZoneRecord.set(TimeZoneRecord{});
      return true;
    }

    // Step 5.c.
    if (auto* dateTime = obj->maybeUnwrapIf<PlainDateTimeObject>()) {
      auto plainDateTime = ToPlainDate(dateTime);

      Rooted<CalendarValue> calendar(cx, dateTime->calendar());
      if (!calendar.wrap(cx)) {
        return false;
      }

      // Step 5.c.i.
      auto* plainDate = CreateTemporalDate(cx, plainDateTime, calendar);
      if (!plainDate) {
        return false;
      }

      // Step 5.c.ii.
      plainRelativeTo.set(plainDate);
      zonedRelativeTo.set(ZonedDateTime{});
      timeZoneRecord.set(TimeZoneRecord{});
      return true;
    }

    // Step 5.d.
    if (!GetTemporalCalendarWithISODefault(cx, obj, &calendar)) {
      return false;
    }

    // Step 5.e.
    Rooted<CalendarRecord> calendarRec(cx);
    if (!CreateCalendarMethodsRecord(cx, calendar,
                                     {
                                         CalendarMethod::DateFromFields,
                                         CalendarMethod::Fields,
                                     },
                                     &calendarRec)) {
      return false;
    }

    // Step 5.f.
    JS::RootedVector<PropertyKey> fieldNames(cx);
    if (!CalendarFields(cx, calendarRec,
                        {CalendarField::Day, CalendarField::Month,
                         CalendarField::MonthCode, CalendarField::Year},
                        &fieldNames)) {
      return false;
    }

    // Step 5.g.
    if (!AppendSorted(cx, fieldNames.get(),
                      {
                          TemporalField::Hour,
                          TemporalField::Microsecond,
                          TemporalField::Millisecond,
                          TemporalField::Minute,
                          TemporalField::Nanosecond,
                          TemporalField::Offset,
                          TemporalField::Second,
                          TemporalField::TimeZone,
                      })) {
      return false;
    }

    // Step 5.h.
    Rooted<PlainObject*> fields(cx, PrepareTemporalFields(cx, obj, fieldNames));
    if (!fields) {
      return false;
    }

    // Step 5.i.
    Rooted<PlainObject*> dateOptions(cx, NewPlainObjectWithProto(cx, nullptr));
    if (!dateOptions) {
      return false;
    }

    // Step 5.j.
    Rooted<Value> overflow(cx, StringValue(cx->names().constrain));
    if (!DefineDataProperty(cx, dateOptions, cx->names().overflow, overflow)) {
      return false;
    }

    // Step 5.k.
    if (!InterpretTemporalDateTimeFields(cx, calendarRec, fields, dateOptions,
                                         &dateTime)) {
      return false;
    }

    // Step 5.l.
    Rooted<Value> offset(cx);
    if (!GetProperty(cx, fields, fields, cx->names().offset, &offset)) {
      return false;
    }

    // Step 5.m.
    Rooted<Value> timeZoneValue(cx);
    if (!GetProperty(cx, fields, fields, cx->names().timeZone,
                     &timeZoneValue)) {
      return false;
    }

    // Step 5.n.
    if (!timeZoneValue.isUndefined()) {
      if (!ToTemporalTimeZone(cx, timeZoneValue, &timeZone)) {
        return false;
      }
    }

    // Step 5.o.
    if (offset.isUndefined()) {
      offsetBehaviour = OffsetBehaviour::Wall;
    }

    // Steps 8-9.
    if (timeZone) {
      if (offsetBehaviour == OffsetBehaviour::Option) {
        MOZ_ASSERT(!offset.isUndefined());
        MOZ_ASSERT(offset.isString());

        // Step 8.a.
        Rooted<JSString*> offsetString(cx, offset.toString());
        if (!offsetString) {
          return false;
        }

        // Step 8.b.
        if (!ParseDateTimeUTCOffset(cx, offsetString, &offsetNs)) {
          return false;
        }
      } else {
        // Step 9.
        offsetNs = 0;
      }
    }
  } else {
    // Step 6.a.
    if (!value.isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, value,
                       nullptr, "not a string");
      return false;
    }
    Rooted<JSString*> string(cx, value.toString());

    // Step 6.b.
    bool isUTC;
    bool hasOffset;
    int64_t timeZoneOffset;
    Rooted<ParsedTimeZone> timeZoneName(cx);
    Rooted<JSString*> calendarString(cx);
    if (!ParseTemporalRelativeToString(cx, string, &dateTime, &isUTC,
                                       &hasOffset, &timeZoneOffset,
                                       &timeZoneName, &calendarString)) {
      return false;
    }

    // Step 6.c. (Not applicable in our implementation.)

    // Steps 6.e-f.
    if (timeZoneName) {
      // Step 6.f.i.
      if (!ToTemporalTimeZone(cx, timeZoneName, &timeZone)) {
        return false;
      }

      // Steps 6.f.ii-iii.
      if (isUTC) {
        offsetBehaviour = OffsetBehaviour::Exact;
      } else if (!hasOffset) {
        offsetBehaviour = OffsetBehaviour::Wall;
      }

      // Step 6.f.iv.
      matchBehaviour = MatchBehaviour::MatchMinutes;
    } else {
      MOZ_ASSERT(!timeZone);
    }

    // Steps 6.g-j.
    if (calendarString) {
      if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
        return false;
      }
    } else {
      calendar.set(CalendarValue(cx->names().iso8601));
    }

    // Steps 8-9.
    if (timeZone) {
      if (offsetBehaviour == OffsetBehaviour::Option) {
        MOZ_ASSERT(hasOffset);

        // Step 8.a.
        offsetNs = timeZoneOffset;
      } else {
        // Step 9.
        offsetNs = 0;
      }
    }
  }

  // Step 7.
  if (!timeZone) {
    // Step 7.a.
    auto* plainDate = CreateTemporalDate(cx, dateTime.date, calendar);
    if (!plainDate) {
      return false;
    }

    plainRelativeTo.set(plainDate);
    zonedRelativeTo.set(ZonedDateTime{});
    timeZoneRecord.set(TimeZoneRecord{});
    return true;
  }

  // Steps 8-9. (Moved above)

  // Step 10.
  Rooted<TimeZoneRecord> timeZoneRec(cx);
  if (!CreateTimeZoneMethodsRecord(cx, timeZone,
                                   {
                                       TimeZoneMethod::GetOffsetNanosecondsFor,
                                       TimeZoneMethod::GetPossibleInstantsFor,
                                   },
                                   &timeZoneRec)) {
    return false;
  }

  // Step 11.
  Instant epochNanoseconds;
  if (!InterpretISODateTimeOffset(
          cx, dateTime, offsetBehaviour, offsetNs, timeZoneRec,
          TemporalDisambiguation::Compatible, TemporalOffset::Reject,
          matchBehaviour, &epochNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(epochNanoseconds));

  // Step 12.
  plainRelativeTo.set(nullptr);
  zonedRelativeTo.set(ZonedDateTime{epochNanoseconds, timeZone, calendar});
  timeZoneRecord.set(timeZoneRec);
  return true;
}

/**
 * CreateCalendarMethodsRecordFromRelativeTo ( plainRelativeTo, zonedRelativeTo,
 * methods )
 */
static bool CreateCalendarMethodsRecordFromRelativeTo(
    JSContext* cx, Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<ZonedDateTime> zonedRelativeTo,
    mozilla::EnumSet<CalendarMethod> methods,
    MutableHandle<CalendarRecord> result) {
  // Step 1.
  if (zonedRelativeTo) {
    return CreateCalendarMethodsRecord(cx, zonedRelativeTo.calendar(), methods,
                                       result);
  }

  // Step 2.
  if (plainRelativeTo) {
    auto* unwrapped = plainRelativeTo.unwrap(cx);
    if (!unwrapped) {
      return false;
    }

    Rooted<CalendarValue> calendar(cx, unwrapped->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    return CreateCalendarMethodsRecord(cx, calendar, methods, result);
  }

  // Step 3.
  return true;
}

static constexpr bool IsSafeInteger(int64_t x) {
  constexpr int64_t MaxSafeInteger = int64_t(1) << 53;
  constexpr int64_t MinSafeInteger = -MaxSafeInteger;
  return MinSafeInteger < x && x < MaxSafeInteger;
}

static constexpr bool IsSafeInteger(const Int128& x) {
  constexpr Int128 MaxSafeInteger = Int128{int64_t(1) << 53};
  constexpr Int128 MinSafeInteger = -MaxSafeInteger;
  return MinSafeInteger < x && x < MaxSafeInteger;
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static void TruncateNumber(int64_t numerator, int64_t denominator,
                           double* quotient, double* total) {
  // Computes the quotient and real number value of the rational number
  // |numerator / denominator|.

  // Int64 division truncates.
  int64_t q = numerator / denominator;
  int64_t r = numerator % denominator;

  // The total value is stored as a mathematical number in the draft proposal,
  // so we can't convert it to a double without loss of precision. We use two
  // different approaches to compute the total value based on the input range.
  //
  // For example:
  //
  // When |numerator = 1000001| and |denominator = 60 * 1000|, the exact result
  // is |16.66668333...| and the best possible approximation is
  // |16.666683333333335070...𝔽|. We can this approximation when casting both
  // numerator and denominator to doubles and then performing a double division.
  //
  // When |numerator = 14400000000000001| and |denominator = 3600000000000|, we
  // can't use double division, because |14400000000000001| can't be represented
  // as an exact double value. The exact result is |4000.0000000000002777...|.
  //
  // The best possible approximation is |4000.0000000000004547...𝔽|, which can
  // be computed through |q + r / denominator|.
  if (::IsSafeInteger(numerator) && ::IsSafeInteger(denominator)) {
    *quotient = double(q);
    *total = double(numerator) / double(denominator);
  } else {
    *quotient = double(q);
    *total = double(q) + double(r) / double(denominator);
  }
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static bool TruncateNumber(JSContext* cx, Handle<BigInt*> numerator,
                           Handle<BigInt*> denominator, double* quotient,
                           double* total) {
  MOZ_ASSERT(!denominator->isNegative());
  MOZ_ASSERT(!denominator->isZero());

  // Dividing zero is always zero.
  if (numerator->isZero()) {
    *quotient = 0;
    *total = 0;
    return true;
  }

  int64_t num, denom;
  if (BigInt::isInt64(numerator, &num) &&
      BigInt::isInt64(denominator, &denom)) {
    TruncateNumber(num, denom, quotient, total);
    return true;
  }

  // BigInt division truncates.
  Rooted<BigInt*> quot(cx);
  Rooted<BigInt*> rem(cx);
  if (!BigInt::divmod(cx, numerator, denominator, &quot, &rem)) {
    return false;
  }

  double q = BigInt::numberValue(quot);
  *quotient = q;
  *total = q + BigInt::numberValue(rem) / BigInt::numberValue(denominator);
  return true;
}

struct RoundedDuration final {
  NormalizedDuration duration;
  double total = 0;
};

enum class ComputeRemainder : bool { No, Yes };

/**
 * RoundNormalizedTimeDurationToIncrement ( d, increment, roundingMode )
 */
static NormalizedTimeDuration RoundNormalizedTimeDurationToIncrement(
    const NormalizedTimeDuration& duration, const TemporalUnit unit,
    Increment increment, TemporalRoundingMode roundingMode) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration));
  MOZ_ASSERT(unit > TemporalUnit::Day);
  MOZ_ASSERT(increment <= MaximumTemporalDurationRoundingIncrement(unit));

  int64_t divisor = ToNanoseconds(unit) * increment.value();
  MOZ_ASSERT(divisor > 0);
  MOZ_ASSERT(divisor <= ToNanoseconds(TemporalUnit::Day));

  auto totalNanoseconds = duration.toTotalNanoseconds();
  auto rounded =
      RoundNumberToIncrement(totalNanoseconds, Int128{divisor}, roundingMode);
  return NormalizedTimeDuration::fromNanoseconds(rounded);
}

/**
 * DivideNormalizedTimeDuration ( d, divisor )
 */
static double TotalNormalizedTimeDuration(
    const NormalizedTimeDuration& duration, const TemporalUnit unit) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration));
  MOZ_ASSERT(unit > TemporalUnit::Day);

  // Compute real number value of the rational number |numerator / denominator|.

  auto numerator = duration.toTotalNanoseconds();
  auto denominator = ToNanoseconds(unit);
  MOZ_ASSERT(::IsSafeInteger(denominator));

  // The total value is stored as a mathematical number in the draft proposal,
  // so we can't convert it to a double without loss of precision. We use two
  // different approaches to compute the total value based on the input range.
  //
  // For example:
  //
  // When |numerator = 1000001| and |denominator = 60 * 1000|, the exact result
  // is |16.66668333...| and the best possible approximation is
  // |16.666683333333335070...𝔽|. We can this approximation when casting both
  // numerator and denominator to doubles and then performing a double division.
  //
  // When |numerator = 14400000000000001| and |denominator = 3600000000000|, we
  // can't use double division, because |14400000000000001| can't be represented
  // as an exact double value. The exact result is |4000.0000000000002777...|.
  //
  // The best possible approximation is |4000.0000000000004547...𝔽|, which can
  // be computed through |q + r / denominator|.
  if (::IsSafeInteger(numerator)) {
    return double(numerator) / double(denominator);
  }

  auto [q, r] = numerator.divrem(Int128{denominator});
  return double(q) + double(r) / double(denominator);
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
NormalizedTimeDuration js::temporal::RoundDuration(
    const NormalizedTimeDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode) {
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration));
  MOZ_ASSERT(unit > TemporalUnit::Day);

  // Steps 1-13. (Not applicable)

  // Steps 14-19.
  auto rounded = RoundNormalizedTimeDurationToIncrement(
      duration, unit, increment, roundingMode);
  MOZ_ASSERT(IsValidNormalizedTimeDuration(rounded));

  // Step 20.
  return rounded;
}

static bool TruncateDays(JSContext* cx,
                         const NormalizedTimeAndDays& timeAndDays, double days,
                         int32_t daysToAdd, double* result) {
  do {
    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    auto totalDays = mozilla::CheckedInt64(intDays);
    totalDays += timeAndDays.days;
    totalDays += daysToAdd;
    if (!totalDays.isValid()) {
      break;
    }

    int64_t truncatedDays = totalDays.value();
    if (timeAndDays.time > 0) {
      // Round toward positive infinity when the integer days are negative and
      // the fractional part is positive.
      if (truncatedDays < 0) {
        truncatedDays += 1;
      }
    } else if (timeAndDays.time < 0) {
      // Round toward negative infinity when the integer days are positive and
      // the fractional part is negative.
      if (truncatedDays > 0) {
        truncatedDays -= 1;
      }
    }

    *result = double(truncatedDays);
    return true;
  } while (false);

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biNanoDays(cx, BigInt::createFromInt64(cx, timeAndDays.days));
  if (!biNanoDays) {
    return false;
  }

  Rooted<BigInt*> biDaysToAdd(cx, BigInt::createFromInt64(cx, daysToAdd));
  if (!biDaysToAdd) {
    return false;
  }

  Rooted<BigInt*> truncatedDays(cx, BigInt::add(cx, biDays, biNanoDays));
  if (!truncatedDays) {
    return false;
  }

  truncatedDays = BigInt::add(cx, truncatedDays, biDaysToAdd);
  if (!truncatedDays) {
    return false;
  }

  if (timeAndDays.time > 0) {
    // Round toward positive infinity when the integer days are negative and
    // the fractional part is positive.
    if (truncatedDays->isNegative()) {
      truncatedDays = BigInt::inc(cx, truncatedDays);
      if (!truncatedDays) {
        return false;
      }
    }
  } else if (timeAndDays.time < 0) {
    // Round toward negative infinity when the integer days are positive and
    // the fractional part is negative.
    if (!truncatedDays->isNegative() && !truncatedDays->isZero()) {
      truncatedDays = BigInt::dec(cx, truncatedDays);
      if (!truncatedDays) {
        return false;
      }
    }
  }

  *result = BigInt::numberValue(truncatedDays);
  return true;
}

static bool DaysIsNegative(double days,
                           const NormalizedTimeAndDays& timeAndDays,
                           int32_t daysToAdd) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  MOZ_ASSERT(std::abs(daysToAdd) <= epochDays * 2);

  int64_t timeDays = timeAndDays.days;

  // When non-zero |days| and |timeDays| have oppositive signs, the absolute
  // value of |days| is less-or-equal to |epochDays|. That means when adding
  // |days + timeDays| we don't have to worry about a case like:
  //
  // days = 9007199254740991 and
  // timeDays = 𝔽(-9007199254740993) = -9007199254740992
  //
  // ℝ(𝔽(days) + 𝔽(timeDays)) is -1, whereas the correct result is -2.
  MOZ_ASSERT((days <= 0 && timeDays <= 0) || (days >= 0 && timeDays >= 0) ||
             std::abs(days) <= epochDays);

  // This addition can be imprecise, so |daysApproximation| is only an
  // approximation of the actual value.
  double daysApproximation = days + timeDays;

  if (std::abs(daysApproximation) <= epochDays * 2) {
    int32_t intDays = int32_t(daysApproximation) + daysToAdd;
    return intDays < 0 || (intDays == 0 && timeAndDays.time < 0);
  }

  // |daysApproximation| is too large, adding |daysToAdd| doesn't change the
  // sign.
  return daysApproximation < 0;
}

struct RoundedNumber {
  double rounded;
  double total;
};

static bool RoundNumberToIncrementSlow(JSContext* cx, double durationAmount,
                                       double amountPassed, double durationDays,
                                       int32_t daysToAdd,
                                       const NormalizedTimeAndDays& timeAndDays,
                                       int32_t oneUnitDays, Increment increment,
                                       TemporalRoundingMode roundingMode,
                                       ComputeRemainder computeRemainder,
                                       RoundedNumber* result) {
  MOZ_ASSERT(timeAndDays.dayLength > 0);
  MOZ_ASSERT(std::abs(timeAndDays.time) < timeAndDays.dayLength);
  MOZ_ASSERT(oneUnitDays != 0);

  Rooted<BigInt*> biAmount(cx, BigInt::createFromDouble(cx, durationAmount));
  if (!biAmount) {
    return false;
  }

  Rooted<BigInt*> biAmountPassed(cx,
                                 BigInt::createFromDouble(cx, amountPassed));
  if (!biAmountPassed) {
    return false;
  }

  biAmount = BigInt::add(cx, biAmount, biAmountPassed);
  if (!biAmount) {
    return false;
  }

  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, durationDays));
  if (!days) {
    return false;
  }

  Rooted<BigInt*> nanoDays(cx, BigInt::createFromInt64(cx, timeAndDays.days));
  if (!nanoDays) {
    return false;
  }

  Rooted<BigInt*> biDaysToAdd(cx, BigInt::createFromInt64(cx, daysToAdd));
  if (!biDaysToAdd) {
    return false;
  }

  days = BigInt::add(cx, days, nanoDays);
  if (!days) {
    return false;
  }

  days = BigInt::add(cx, days, biDaysToAdd);
  if (!days) {
    return false;
  }

  Rooted<BigInt*> nanoseconds(cx,
                              BigInt::createFromInt64(cx, timeAndDays.time));
  if (!nanoseconds) {
    return false;
  }

  Rooted<BigInt*> dayLength(cx,
                            BigInt::createFromInt64(cx, timeAndDays.dayLength));
  if (!dayLength) {
    return false;
  }

  Rooted<BigInt*> denominator(
      cx, BigInt::createFromInt64(cx, std::abs(oneUnitDays)));
  if (!denominator) {
    return false;
  }

  denominator = BigInt::mul(cx, denominator, dayLength);
  if (!denominator) {
    return false;
  }

  Rooted<BigInt*> totalNanoseconds(cx, BigInt::mul(cx, days, dayLength));
  if (!totalNanoseconds) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, nanoseconds);
  if (!totalNanoseconds) {
    return false;
  }

  Rooted<BigInt*> amountNanos(cx, BigInt::mul(cx, biAmount, denominator));
  if (!amountNanos) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, amountNanos);
  if (!totalNanoseconds) {
    return false;
  }

  double rounded;
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode, &rounded)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &rounded,
                          &total)) {
      return false;
    }
  }

  *result = {rounded, total};
  return true;
}

static bool RoundNumberToIncrement(JSContext* cx, double durationAmount,
                                   double amountPassed, double durationDays,
                                   int32_t daysToAdd,
                                   const NormalizedTimeAndDays& timeAndDays,
                                   int32_t oneUnitDays, Increment increment,
                                   TemporalRoundingMode roundingMode,
                                   ComputeRemainder computeRemainder,
                                   RoundedNumber* result) {
  MOZ_ASSERT(timeAndDays.dayLength > 0);
  MOZ_ASSERT(std::abs(timeAndDays.time) < timeAndDays.dayLength);
  MOZ_ASSERT(oneUnitDays != 0);
  MOZ_ASSERT(std::abs(oneUnitDays) <= 200'000'000);

  // TODO(anba): Rename variables.

  // clang-format off
  //
  // Change the representation of |fractionalWeeks| from a real number to a
  // rational number, because we don't support arbitrary precision real
  // numbers.
  //
  // |fractionalWeeks| is defined as:
  //
  //   fractionalWeeks
  // = weeks + days' / abs(oneWeekDays)
  //
  // where days' = days + nanoseconds / dayLength.
  //
  // The fractional part |nanoseconds / dayLength| is from step 4.
  //
  // The denominator for |fractionalWeeks| is |dayLength * abs(oneWeekDays)|.
  //
  //   fractionalWeeks
  // = weeks + (days + nanoseconds / dayLength) / abs(oneWeekDays)
  // = weeks + days / abs(oneWeekDays) + nanoseconds / (dayLength * abs(oneWeekDays))
  // = (weeks * dayLength * abs(oneWeekDays) + days * dayLength + nanoseconds) / (dayLength * abs(oneWeekDays))
  //
  // clang-format on

  do {
    auto dayLength = mozilla::CheckedInt64(timeAndDays.dayLength);

    auto denominator = dayLength * std::abs(oneUnitDays);
    if (!denominator.isValid()) {
      break;
    }

    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(durationDays, &intDays)) {
      break;
    }

    auto totalDays = mozilla::CheckedInt64(intDays);
    totalDays += timeAndDays.days;
    totalDays += daysToAdd;
    if (!totalDays.isValid()) {
      break;
    }

    auto totalNanoseconds = dayLength * totalDays;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    totalNanoseconds += timeAndDays.time;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    int64_t intAmount;
    if (!mozilla::NumberEqualsInt64(durationAmount, &intAmount)) {
      break;
    }

    int64_t intAmountPassed;
    if (!mozilla::NumberEqualsInt64(amountPassed, &intAmountPassed)) {
      break;
    }

    auto totalAmount = mozilla::CheckedInt64(intAmount) + intAmountPassed;
    if (!totalAmount.isValid()) {
      break;
    }

    auto amountNanos = denominator * totalAmount;
    if (!amountNanos.isValid()) {
      break;
    }

    totalNanoseconds += amountNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double rounded;
    double total = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &rounded)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &rounded,
                     &total);
    }

    *result = {rounded, total};
    return true;
  } while (false);

  return RoundNumberToIncrementSlow(
      cx, durationAmount, amountPassed, durationDays, daysToAdd, timeAndDays,
      oneUnitDays, increment, roundingMode, computeRemainder, result);
}

static bool RoundNumberToIncrement(JSContext* cx, double durationDays,
                                   const NormalizedTimeAndDays& timeAndDays,
                                   Increment increment,
                                   TemporalRoundingMode roundingMode,
                                   ComputeRemainder computeRemainder,
                                   RoundedNumber* result) {
  constexpr double daysAmount = 0;
  constexpr double daysPassed = 0;
  constexpr int32_t oneDayDays = 1;
  constexpr int32_t daysToAdd = 0;

  return RoundNumberToIncrement(cx, daysAmount, daysPassed, durationDays,
                                daysToAdd, timeAndDays, oneDayDays, increment,
                                roundingMode, computeRemainder, result);
}

static bool RoundDurationYear(JSContext* cx, const NormalizedDuration& duration,
                              const NormalizedTimeAndDays& timeAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              Handle<CalendarRecord> calendar,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  auto [years, months, weeks, days] = duration.date;

  // Step 10.a.
  Duration yearsDuration = {years};

  // Step 10.b.
  auto yearsLater = AddDate(cx, calendar, dateRelativeTo, yearsDuration);
  if (!yearsLater) {
    return false;
  }
  auto yearsLaterDate = ToPlainDate(&yearsLater.unwrap());

  // Step 10.f. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsLater);

  // Step 10.c.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 10.d.
  PlainDate yearsMonthsWeeksLater;
  if (!AddDate(cx, calendar, dateRelativeTo, yearsMonthsWeeks,
               &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 10.e.
  int32_t monthsWeeksInDays = DaysUntil(yearsLaterDate, yearsMonthsWeeksLater);
  MOZ_ASSERT(std::abs(monthsWeeksInDays) <= epochDays);

  // Step 10.f. (Moved up)

  // Step 10.g.
  // Our implementation keeps |days| and |monthsWeeksInDays| separate.

  // FIXME: spec issue - truncation doesn't match the spec polyfill.
  // https://github.com/tc39/proposal-temporal/issues/2540

  // Step 10.h.
  double truncatedDays;
  if (!TruncateDays(cx, timeAndDays, days, monthsWeeksInDays, &truncatedDays)) {
    return false;
  }
  MOZ_ASSERT(IsInteger(truncatedDays));

  PlainDate isoResult;
  if (!AddISODate(cx, yearsLaterDate, {0, 0, 0, truncatedDays},
                  TemporalOverflow::Constrain, &isoResult)) {
    return false;
  }

  // Step 10.i.
  Rooted<PlainDateObject*> wholeDaysLater(
      cx, CreateTemporalDate(cx, isoResult, calendar.receiver()));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 10.j-l.
  Duration timePassed;
  if (!DifferenceDate(cx, calendar, newRelativeTo, wholeDaysLater,
                      TemporalUnit::Year, &timePassed)) {
    return false;
  }

  // Step 10.m.
  double yearsPassed = timePassed.years;

  // Step 10.n.
  // Our implementation keeps |years| and |yearsPassed| separate.

  // Step 10.o.
  Duration yearsPassedDuration = {yearsPassed};

  // Steps 10.p-r.
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, yearsPassedDuration,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }
  MOZ_ASSERT(std::abs(daysPassed) <= epochDays);

  // Step 10.s.
  //
  // Our implementation keeps |days| and |daysPassed| separate.
  int32_t daysToAdd = monthsWeeksInDays - daysPassed;
  MOZ_ASSERT(std::abs(daysToAdd) <= epochDays * 2);

  // Steps 10.t.
  double sign = DaysIsNegative(days, timeAndDays, daysToAdd) ? -1 : 1;

  // Step 10.u.
  Duration oneYear = {sign};

  // Steps 10.v-w.
  Rooted<Wrapped<PlainDateObject*>> moveResultIgnored(cx);
  int32_t oneYearDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneYear,
                        &moveResultIgnored, &oneYearDays)) {
    return false;
  }

  // Step 10.x.
  if (oneYearDays == 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, "days");
    return false;
  }

  // Steps 10.y-aa.
  RoundedNumber rounded;
  if (!RoundNumberToIncrement(cx, years, yearsPassed, days, daysToAdd,
                              timeAndDays, oneYearDays, increment, roundingMode,
                              computeRemainder, &rounded)) {
    return false;
  }
  auto [numYears, total] = rounded;

  // Step 10.ab.
  double numMonths = 0;
  double numWeeks = 0;

  // Step 10.ac.
  constexpr auto time = NormalizedTimeDuration{};

  // Step 20.
  auto resultDuration = DateDuration{numYears, numMonths, numWeeks};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  *result = {{resultDuration, time}, total};
  return true;
}

static bool RoundDurationMonth(JSContext* cx,
                               const NormalizedDuration& duration,
                               const NormalizedTimeAndDays& timeAndDays,
                               Increment increment,
                               TemporalRoundingMode roundingMode,
                               Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                               Handle<CalendarRecord> calendar,
                               ComputeRemainder computeRemainder,
                               RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  auto [years, months, weeks, days] = duration.date;

  // Step 11.a.
  Duration yearsMonths = {years, months};

  // Step 11.b.
  auto yearsMonthsLater = AddDate(cx, calendar, dateRelativeTo, yearsMonths);
  if (!yearsMonthsLater) {
    return false;
  }
  auto yearsMonthsLaterDate = ToPlainDate(&yearsMonthsLater.unwrap());

  // Step 11.f. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsMonthsLater);

  // Step 11.c.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 11.d.
  PlainDate yearsMonthsWeeksLater;
  if (!AddDate(cx, calendar, dateRelativeTo, yearsMonthsWeeks,
               &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 11.e.
  int32_t weeksInDays = DaysUntil(yearsMonthsLaterDate, yearsMonthsWeeksLater);
  MOZ_ASSERT(std::abs(weeksInDays) <= epochDays);

  // Step 11.f. (Moved up)

  // Step 11.g.
  // Our implementation keeps |days| and |weeksInDays| separate.

  // FIXME: spec issue - truncation doesn't match the spec polyfill.
  // https://github.com/tc39/proposal-temporal/issues/2540

  // Step 11.h.
  double truncatedDays;
  if (!TruncateDays(cx, timeAndDays, days, weeksInDays, &truncatedDays)) {
    return false;
  }
  MOZ_ASSERT(IsInteger(truncatedDays));

  PlainDate isoResult;
  if (!AddISODate(cx, yearsMonthsLaterDate, {0, 0, 0, truncatedDays},
                  TemporalOverflow::Constrain, &isoResult)) {
    return false;
  }

  // Step 11.i.
  Rooted<PlainDateObject*> wholeDaysLater(
      cx, CreateTemporalDate(cx, isoResult, calendar.receiver()));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 11.j-l.
  Duration timePassed;
  if (!DifferenceDate(cx, calendar, newRelativeTo, wholeDaysLater,
                      TemporalUnit::Month, &timePassed)) {
    return false;
  }

  // Step 11.m.
  double monthsPassed = timePassed.months;

  // Step 11.n.
  // Our implementation keeps |months| and |monthsPassed| separate.

  // Step 11.o.
  Duration monthsPassedDuration = {0, monthsPassed};

  // Steps 11.p-r.
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, monthsPassedDuration,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }
  MOZ_ASSERT(std::abs(daysPassed) <= epochDays);

  // Step 11.s.
  //
  // Our implementation keeps |days| and |daysPassed| separate.
  int32_t daysToAdd = weeksInDays - daysPassed;
  MOZ_ASSERT(std::abs(daysToAdd) <= epochDays * 2);

  // Steps 11.t.
  double sign = DaysIsNegative(days, timeAndDays, daysToAdd) ? -1 : 1;

  // Step 11.u.
  Duration oneMonth = {0, sign};

  // Steps 11.v-w.
  Rooted<Wrapped<PlainDateObject*>> moveResultIgnored(cx);
  int32_t oneMonthDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth,
                        &moveResultIgnored, &oneMonthDays)) {
    return false;
  }

  // Step 11.x.
  if (oneMonthDays == 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, "days");
    return false;
  }

  // Steps 11.y-aa.
  RoundedNumber rounded;
  if (!RoundNumberToIncrement(cx, months, monthsPassed, days, daysToAdd,
                              timeAndDays, oneMonthDays, increment,
                              roundingMode, computeRemainder, &rounded)) {
    return false;
  }
  auto [numMonths, total] = rounded;

  // Step 11.ab.
  double numWeeks = 0;

  // Step 11.ac.
  constexpr auto time = NormalizedTimeDuration{};

  // Step 21.
  auto resultDuration = DateDuration{years, numMonths, numWeeks};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  *result = {{resultDuration, time}, total};
  return true;
}

static bool RoundDurationWeek(JSContext* cx, const NormalizedDuration& duration,
                              const NormalizedTimeAndDays& timeAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              Handle<CalendarRecord> calendar,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  auto [years, months, weeks, days] = duration.date;

  auto* unwrappedRelativeTo = dateRelativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto relativeToDate = ToPlainDate(unwrappedRelativeTo);

  // Step 12.a
  double truncatedDays;
  if (!TruncateDays(cx, timeAndDays, days, 0, &truncatedDays)) {
    return false;
  }
  MOZ_ASSERT(IsInteger(truncatedDays));

  PlainDate isoResult;
  if (!AddISODate(cx, relativeToDate, {0, 0, 0, truncatedDays},
                  TemporalOverflow::Constrain, &isoResult)) {
    return false;
  }

  // Step 12.b.
  Rooted<PlainDateObject*> wholeDaysLater(
      cx, CreateTemporalDate(cx, isoResult, calendar.receiver()));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 12.c-e.
  Duration timePassed;
  if (!DifferenceDate(cx, calendar, dateRelativeTo, wholeDaysLater,
                      TemporalUnit::Week, &timePassed)) {
    return false;
  }

  // Step 12.f.
  double weeksPassed = timePassed.weeks;

  // Step 12.g.
  // Our implementation keeps |weeks| and |weeksPassed| separate.

  // Step 12.h.
  Duration weeksPassedDuration = {0, 0, weeksPassed};

  // Steps 12.i-k.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, weeksPassedDuration,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }
  MOZ_ASSERT(std::abs(daysPassed) <= epochDays);

  // Step 12.l.
  //
  // Our implementation keeps |days| and |daysPassed| separate.
  int32_t daysToAdd = -daysPassed;
  MOZ_ASSERT(std::abs(daysToAdd) <= epochDays);

  // Steps 12.m.
  double sign = DaysIsNegative(days, timeAndDays, daysToAdd) ? -1 : 1;

  // Step 12.n.
  Duration oneWeek = {0, 0, sign};

  // Steps 12.o-p.
  Rooted<Wrapped<PlainDateObject*>> moveResultIgnored(cx);
  int32_t oneWeekDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneWeek,
                        &moveResultIgnored, &oneWeekDays)) {
    return false;
  }

  // Step 12.q.
  if (oneWeekDays == 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, "days");
    return false;
  }

  // Steps 12.r-t.
  RoundedNumber rounded;
  if (!RoundNumberToIncrement(cx, weeks, weeksPassed, days, daysToAdd,
                              timeAndDays, oneWeekDays, increment, roundingMode,
                              computeRemainder, &rounded)) {
    return false;
  }
  auto [numWeeks, total] = rounded;

  // Step 12.u.
  constexpr auto time = NormalizedTimeDuration{};

  // Step 20.
  auto resultDuration = DateDuration{years, months, numWeeks};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  *result = {{resultDuration, time}, total};
  return true;
}

static bool RoundDurationDay(JSContext* cx, const NormalizedDuration& duration,
                             const NormalizedTimeAndDays& timeAndDays,
                             Increment increment,
                             TemporalRoundingMode roundingMode,
                             ComputeRemainder computeRemainder,
                             RoundedDuration* result) {
  auto [years, months, weeks, days] = duration.date;

  // Steps 13.a-b.
  RoundedNumber rounded;
  if (!RoundNumberToIncrement(cx, days, timeAndDays, increment, roundingMode,
                              computeRemainder, &rounded)) {
    return false;
  }
  auto [numDays, total] = rounded;

  // Step 13.c.
  constexpr auto time = NormalizedTimeDuration{};

  // Step 20.
  auto resultDuration = DateDuration{years, months, weeks, numDays};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  *result = {{resultDuration, time}, total};
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(JSContext* cx, const NormalizedDuration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          ComputeRemainder computeRemainder,
                          RoundedDuration* result) {
  // The remainder is only needed when called from |Duration_total|. And `total`
  // always passes |increment=1| and |roundingMode=trunc|.
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                increment == Increment{1});
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                roundingMode == TemporalRoundingMode::Trunc);

  // Steps 1-5. (Not applicable.)

  // Step 6.
  if (unit <= TemporalUnit::Week) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // TODO: We could directly return here if unit=nanoseconds and increment=1,
  // because in that case this operation is a no-op. This case happens for
  // example when calling Temporal.PlainTime.prototype.{since,until} without an
  // options object.
  //
  // But maybe this can be even more efficiently handled in the callers. For
  // example when Temporal.PlainTime.prototype.{since,until} is called without
  // an options object, we can not only skip the RoundDuration call, but also
  // the following BalanceTimeDuration call.

  // Step 7. (Moved below.)

  // Steps 8-9. (Not applicable.)

  // Steps 10-12. (Not applicable.)

  // Step 13.
  if (unit == TemporalUnit::Day) {
    // Step 7.
    auto timeAndDays = NormalizedTimeDurationToDays(duration.time);

    return RoundDurationDay(cx, duration, timeAndDays, increment, roundingMode,
                            computeRemainder, result);
  }

  MOZ_ASSERT(TemporalUnit::Hour <= unit && unit <= TemporalUnit::Nanosecond);

  // Steps 14-19.
  auto time = duration.time;
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    time = RoundNormalizedTimeDurationToIncrement(time, unit, increment,
                                                  roundingMode);
  } else {
    MOZ_ASSERT(increment == Increment{1});
    MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

    total = TotalNormalizedTimeDuration(duration.time, unit);
  }
  MOZ_ASSERT(IsValidNormalizedTimeDuration(time));

  // Step 20.
  MOZ_ASSERT(IsValidDuration(duration.date.toDuration()));
  *result = {{duration.date, time}, total};
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, Handle<ZonedDateTime> zonedRelativeTo,
    Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  // Note: |duration.days| can have a different sign than the other date
  // components. The date and time components can have different signs, too.
  MOZ_ASSERT(IsValidDuration(Duration{duration.date.years, duration.date.months,
                                      duration.date.weeks}));
  MOZ_ASSERT(IsValidNormalizedTimeDuration(duration.time));

  MOZ_ASSERT(plainRelativeTo || zonedRelativeTo,
             "Use RoundDuration without relativeTo when plainRelativeTo and "
             "zonedRelativeTo are both undefined");

  // The remainder is only needed when called from |Duration_total|. And `total`
  // always passes |increment=1| and |roundingMode=trunc|.
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                increment == Increment{1});
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                roundingMode == TemporalRoundingMode::Trunc);

  // Steps 1-5. (Not applicable in our implementation.)

  // Step 6.a. (Not applicable in our implementation.)
  MOZ_ASSERT_IF(unit <= TemporalUnit::Week, plainRelativeTo);

  // Step 6.b.
  MOZ_ASSERT_IF(
      unit <= TemporalUnit::Week,
      CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateAdd));

  // Step 6.c.
  MOZ_ASSERT_IF(
      unit <= TemporalUnit::Week,
      CalendarMethodsRecordHasLookedUp(calendar, CalendarMethod::DateUntil));

  switch (unit) {
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
      break;
    case TemporalUnit::Day:
      // We can't take the faster code path when |zonedRelativeTo| is present.
      if (zonedRelativeTo) {
        break;
      }
      [[fallthrough]];
    case TemporalUnit::Hour:
    case TemporalUnit::Minute:
    case TemporalUnit::Second:
    case TemporalUnit::Millisecond:
    case TemporalUnit::Microsecond:
    case TemporalUnit::Nanosecond:
      // Steps 7-9 and 13-21.
      return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                             computeRemainder, result);
    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 7.
  MOZ_ASSERT(TemporalUnit::Year <= unit && unit <= TemporalUnit::Day);

  // Steps 7.a-c.
  NormalizedTimeAndDays timeAndDays;
  if (zonedRelativeTo) {
    // Step 7.a.i.
    Rooted<ZonedDateTime> intermediate(cx);
    if (!MoveRelativeZonedDateTime(cx, zonedRelativeTo, calendar, timeZone,
                                   duration.date, precalculatedPlainDateTime,
                                   &intermediate)) {
      return false;
    }

    // Steps 7.a.ii.
    if (!NormalizedTimeDurationToDays(cx, duration.time, intermediate, timeZone,
                                      &timeAndDays)) {
      return false;
    }

    // Step 7.a.iii. (Not applicable in our implementation.)
  } else {
    // Step 7.b. (Partial)
    timeAndDays = ::NormalizedTimeDurationToDays(duration.time);
  }

  // NormalizedTimeDurationToDays guarantees that |abs(timeAndDays.time)| is
  // less than |timeAndDays.dayLength|.
  MOZ_ASSERT(std::abs(timeAndDays.time) < timeAndDays.dayLength);

  // Step 7.c. (Moved below)

  // Step 8. (Not applicable)

  // Step 9.
  // FIXME: spec issue - `total` doesn't need be initialised.

  // Steps 10-20.
  switch (unit) {
    // Steps 10 and 20.
    case TemporalUnit::Year:
      return RoundDurationYear(cx, duration, timeAndDays, increment,
                               roundingMode, plainRelativeTo, calendar,
                               computeRemainder, result);

    // Steps 11 and 20.
    case TemporalUnit::Month:
      return RoundDurationMonth(cx, duration, timeAndDays, increment,
                                roundingMode, plainRelativeTo, calendar,
                                computeRemainder, result);

    // Steps 12 and 20.
    case TemporalUnit::Week:
      return RoundDurationWeek(cx, duration, timeAndDays, increment,
                               roundingMode, plainRelativeTo, calendar,
                               computeRemainder, result);

    // Steps 13 and 20.
    case TemporalUnit::Day:
      return RoundDurationDay(cx, duration, timeAndDays, increment,
                              roundingMode, computeRemainder, result);

    // Steps 14-19. (Handled elsewhere)
    case TemporalUnit::Auto:
    case TemporalUnit::Hour:
    case TemporalUnit::Minute:
    case TemporalUnit::Second:
    case TemporalUnit::Millisecond:
    case TemporalUnit::Microsecond:
    case TemporalUnit::Nanosecond:
      break;
  }

  MOZ_CRASH("Unexpected temporal unit");
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, Handle<ZonedDateTime> zonedRelativeTo,
    Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    double* result) {
  // Only called from |Duration_total|, which always passes |increment=1| and
  // |roundingMode=trunc|.
  MOZ_ASSERT(increment == Increment{1});
  MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       plainRelativeTo, calendar, zonedRelativeTo, timeZone,
                       precalculatedPlainDateTime, ComputeRemainder::Yes,
                       &rounded)) {
    return false;
  }

  *result = rounded.total;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, Handle<ZonedDateTime> zonedRelativeTo,
    Handle<TimeZoneRecord> timeZone,
    mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime,
    NormalizedDuration* result) {
  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       plainRelativeTo, calendar, zonedRelativeTo, timeZone,
                       precalculatedPlainDateTime, ComputeRemainder::No,
                       &rounded)) {
    return false;
  }

  *result = rounded.duration;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(JSContext* cx, const NormalizedDuration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode, double* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Only called from |Duration_total|, which always passes |increment=1| and
  // |roundingMode=trunc|.
  MOZ_ASSERT(increment == Increment{1});
  MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       ComputeRemainder::Yes, &rounded)) {
    return false;
  }

  *result = rounded.total;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
static bool RoundDuration(JSContext* cx, const NormalizedDuration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          NormalizedDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       ComputeRemainder::No, &rounded)) {
    return false;
  }

  *result = rounded.duration;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
bool js::temporal::RoundDuration(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarRecord> calendar, NormalizedDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  Rooted<ZonedDateTime> zonedRelativeTo(cx, ZonedDateTime{});
  Rooted<TimeZoneRecord> timeZone(cx, TimeZoneRecord{});
  mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime{};
  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         plainRelativeTo, calendar, zonedRelativeTo, timeZone,
                         precalculatedPlainDateTime, result);
}

/**
 * RoundDuration ( years, months, weeks, days, norm, increment, unit,
 * roundingMode [ , plainRelativeTo [ , calendarRec [ , zonedRelativeTo [ ,
 * timeZoneRec [ , precalculatedPlainDateTime ] ] ] ] ] )
 */
bool js::temporal::RoundDuration(
    JSContext* cx, const NormalizedDuration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<PlainDateObject*> plainRelativeTo, Handle<CalendarRecord> calendar,
    Handle<ZonedDateTime> zonedRelativeTo, Handle<TimeZoneRecord> timeZone,
    const PlainDateTime& precalculatedPlainDateTime,
    NormalizedDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         plainRelativeTo, calendar, zonedRelativeTo, timeZone,
                         mozilla::SomeRef(precalculatedPlainDateTime), result);
}

enum class DurationOperation { Add, Subtract };

/**
 * AddDurationToOrSubtractDurationFromDuration ( operation, duration, other,
 * options )
 */
static bool AddDurationToOrSubtractDurationFromDuration(
    JSContext* cx, DurationOperation operation, const CallArgs& args) {
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();
  auto duration = ToDuration(durationObj);

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Duration other;
  if (!ToTemporalDurationRecord(cx, args.get(0), &other)) {
    return false;
  }

  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<ZonedDateTime> zonedRelativeTo(cx);
  Rooted<TimeZoneRecord> timeZone(cx);
  if (args.hasDefined(1)) {
    const char* name = operation == DurationOperation::Add ? "add" : "subtract";

    // Step 3.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", name, args[1]));
    if (!options) {
      return false;
    }

    // Steps 4-7.
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo, &timeZone)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
    MOZ_ASSERT_IF(zonedRelativeTo, timeZone.receiver());
  }

  // Step 8.
  Rooted<CalendarRecord> calendar(cx);
  if (!CreateCalendarMethodsRecordFromRelativeTo(cx, plainRelativeTo,
                                                 zonedRelativeTo,
                                                 {
                                                     CalendarMethod::DateAdd,
                                                     CalendarMethod::DateUntil,
                                                 },
                                                 &calendar)) {
    return false;
  }

  // Step 9.
  if (operation == DurationOperation::Subtract) {
    other = other.negate();
  }

  Duration result;
  if (plainRelativeTo) {
    if (!AddDuration(cx, duration, other, plainRelativeTo, calendar, &result)) {
      return false;
    }
  } else if (zonedRelativeTo) {
    if (!AddDuration(cx, duration, other, zonedRelativeTo, calendar, timeZone,
                     &result)) {
      return false;
    }
  } else {
    if (!AddDuration(cx, duration, other, &result)) {
      return false;
    }
  }

  // Step 10.
  auto* obj = CreateTemporalDuration(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Duration ( [ years [ , months [ , weeks [ , days [ , hours [ ,
 * minutes [ , seconds [ , milliseconds [ , microseconds [ , nanoseconds ] ] ] ]
 * ] ] ] ] ] ] )
 */
static bool DurationConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.Duration")) {
    return false;
  }

  // Step 2.
  double years = 0;
  if (args.hasDefined(0) &&
      !ToIntegerIfIntegral(cx, "years", args[0], &years)) {
    return false;
  }

  // Step 3.
  double months = 0;
  if (args.hasDefined(1) &&
      !ToIntegerIfIntegral(cx, "months", args[1], &months)) {
    return false;
  }

  // Step 4.
  double weeks = 0;
  if (args.hasDefined(2) &&
      !ToIntegerIfIntegral(cx, "weeks", args[2], &weeks)) {
    return false;
  }

  // Step 5.
  double days = 0;
  if (args.hasDefined(3) && !ToIntegerIfIntegral(cx, "days", args[3], &days)) {
    return false;
  }

  // Step 6.
  double hours = 0;
  if (args.hasDefined(4) &&
      !ToIntegerIfIntegral(cx, "hours", args[4], &hours)) {
    return false;
  }

  // Step 7.
  double minutes = 0;
  if (args.hasDefined(5) &&
      !ToIntegerIfIntegral(cx, "minutes", args[5], &minutes)) {
    return false;
  }

  // Step 8.
  double seconds = 0;
  if (args.hasDefined(6) &&
      !ToIntegerIfIntegral(cx, "seconds", args[6], &seconds)) {
    return false;
  }

  // Step 9.
  double milliseconds = 0;
  if (args.hasDefined(7) &&
      !ToIntegerIfIntegral(cx, "milliseconds", args[7], &milliseconds)) {
    return false;
  }

  // Step 10.
  double microseconds = 0;
  if (args.hasDefined(8) &&
      !ToIntegerIfIntegral(cx, "microseconds", args[8], &microseconds)) {
    return false;
  }

  // Step 11.
  double nanoseconds = 0;
  if (args.hasDefined(9) &&
      !ToIntegerIfIntegral(cx, "nanoseconds", args[9], &nanoseconds)) {
    return false;
  }

  // Step 12.
  auto* duration = CreateTemporalDuration(
      cx, args,
      {years, months, weeks, days, hours, minutes, seconds, milliseconds,
       microseconds, nanoseconds});
  if (!duration) {
    return false;
  }

  args.rval().setObject(*duration);
  return true;
}

/**
 * Temporal.Duration.from ( item )
 */
static bool Duration_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Handle<Value> item = args.get(0);

  // Step 1.
  if (item.isObject()) {
    if (auto* duration = item.toObject().maybeUnwrapIf<DurationObject>()) {
      auto* result = CreateTemporalDuration(cx, ToDuration(duration));
      if (!result) {
        return false;
      }

      args.rval().setObject(*result);
      return true;
    }
  }

  // Step 2.
  auto result = ToTemporalDuration(cx, item);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Duration.compare ( one, two [ , options ] )
 */
static bool Duration_compare(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Duration one;
  if (!ToTemporalDuration(cx, args.get(0), &one)) {
    return false;
  }

  // Step 2.
  Duration two;
  if (!ToTemporalDuration(cx, args.get(1), &two)) {
    return false;
  }

  // Step 3.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(2)) {
    options = RequireObjectArg(cx, "options", "compare", args[2]);
    if (!options) {
      return false;
    }
  }

  // Step 4.
  if (one == two) {
    args.rval().setInt32(0);
    return true;
  }

  // Steps 5-8.
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<ZonedDateTime> zonedRelativeTo(cx);
  Rooted<TimeZoneRecord> timeZone(cx);
  if (options) {
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo, &timeZone)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
    MOZ_ASSERT_IF(zonedRelativeTo, timeZone.receiver());
  }

  // Steps 9-10.
  auto hasCalendarUnit = [](const auto& d) {
    return d.years != 0 || d.months != 0 || d.weeks != 0;
  };
  bool calendarUnitsPresent = hasCalendarUnit(one) || hasCalendarUnit(two);

  // Step 11.
  Rooted<CalendarRecord> calendar(cx);
  if (!CreateCalendarMethodsRecordFromRelativeTo(cx, plainRelativeTo,
                                                 zonedRelativeTo,
                                                 {
                                                     CalendarMethod::DateAdd,
                                                 },
                                                 &calendar)) {
    return false;
  }

  // Step 12.
  if (zonedRelativeTo &&
      (calendarUnitsPresent || one.days != 0 || two.days != 0)) {
    // Step 12.a.
    const auto& instant = zonedRelativeTo.instant();

    // Step 12.b.
    PlainDateTime dateTime;
    if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
      return false;
    }

    // Step 12.c.
    auto normalized1 = CreateNormalizedDurationRecord(one);

    // Step 12.d.
    auto normalized2 = CreateNormalizedDurationRecord(two);

    // Step 12.e.
    Instant after1;
    if (!AddZonedDateTime(cx, instant, timeZone, calendar, normalized1,
                          dateTime, &after1)) {
      return false;
    }

    // Step 12.f.
    Instant after2;
    if (!AddZonedDateTime(cx, instant, timeZone, calendar, normalized2,
                          dateTime, &after2)) {
      return false;
    }

    // Steps 12.g-i.
    args.rval().setInt32(after1 < after2 ? -1 : after1 > after2 ? 1 : 0);
    return true;
  }

  // Steps 13-14.
  double days1, days2;
  if (calendarUnitsPresent) {
    // FIXME: spec issue - directly throw an error if plainRelativeTo is undef.

    // Step 13.a.
    DateDuration unbalanceResult1;
    if (plainRelativeTo) {
      if (!UnbalanceDateDurationRelative(cx, one.toDateDuration(),
                                         TemporalUnit::Day, plainRelativeTo,
                                         calendar, &unbalanceResult1)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(
              cx, one.toDateDuration(), TemporalUnit::Day, &unbalanceResult1)) {
        return false;
      }
      MOZ_ASSERT(one.toDateDuration() == unbalanceResult1);
    }

    // Step 13.b.
    DateDuration unbalanceResult2;
    if (plainRelativeTo) {
      if (!UnbalanceDateDurationRelative(cx, two.toDateDuration(),
                                         TemporalUnit::Day, plainRelativeTo,
                                         calendar, &unbalanceResult2)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(
              cx, two.toDateDuration(), TemporalUnit::Day, &unbalanceResult2)) {
        return false;
      }
      MOZ_ASSERT(two.toDateDuration() == unbalanceResult2);
    }

    // Step 13.c.
    days1 = unbalanceResult1.days;

    // Step 13.d.
    days2 = unbalanceResult2.days;
  } else {
    // Step 14.a.
    days1 = one.days;

    // Step 14.b.
    days2 = two.days;
  }

  // Step 15.
  auto normalized1 = NormalizeTimeDuration(one);

  // Step 16.
  if (!Add24HourDaysToNormalizedTimeDuration(cx, normalized1, days1,
                                             &normalized1)) {
    return false;
  }

  // Step 17.
  auto normalized2 = NormalizeTimeDuration(two);

  // Step 18.
  if (!Add24HourDaysToNormalizedTimeDuration(cx, normalized2, days2,
                                             &normalized2)) {
    return false;
  }

  // Step 19.
  args.rval().setInt32(CompareNormalizedTimeDuration(normalized1, normalized2));
  return true;
}

/**
 * get Temporal.Duration.prototype.years
 */
static bool Duration_years(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->years());
  return true;
}

/**
 * get Temporal.Duration.prototype.years
 */
static bool Duration_years(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_years>(cx, args);
}

/**
 * get Temporal.Duration.prototype.months
 */
static bool Duration_months(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->months());
  return true;
}

/**
 * get Temporal.Duration.prototype.months
 */
static bool Duration_months(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_months>(cx, args);
}

/**
 * get Temporal.Duration.prototype.weeks
 */
static bool Duration_weeks(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->weeks());
  return true;
}

/**
 * get Temporal.Duration.prototype.weeks
 */
static bool Duration_weeks(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_weeks>(cx, args);
}

/**
 * get Temporal.Duration.prototype.days
 */
static bool Duration_days(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->days());
  return true;
}

/**
 * get Temporal.Duration.prototype.days
 */
static bool Duration_days(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_days>(cx, args);
}

/**
 * get Temporal.Duration.prototype.hours
 */
static bool Duration_hours(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->hours());
  return true;
}

/**
 * get Temporal.Duration.prototype.hours
 */
static bool Duration_hours(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_hours>(cx, args);
}

/**
 * get Temporal.Duration.prototype.minutes
 */
static bool Duration_minutes(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->minutes());
  return true;
}

/**
 * get Temporal.Duration.prototype.minutes
 */
static bool Duration_minutes(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_minutes>(cx, args);
}

/**
 * get Temporal.Duration.prototype.seconds
 */
static bool Duration_seconds(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->seconds());
  return true;
}

/**
 * get Temporal.Duration.prototype.seconds
 */
static bool Duration_seconds(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_seconds>(cx, args);
}

/**
 * get Temporal.Duration.prototype.milliseconds
 */
static bool Duration_milliseconds(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->milliseconds());
  return true;
}

/**
 * get Temporal.Duration.prototype.milliseconds
 */
static bool Duration_milliseconds(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_milliseconds>(cx, args);
}

/**
 * get Temporal.Duration.prototype.microseconds
 */
static bool Duration_microseconds(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->microseconds());
  return true;
}

/**
 * get Temporal.Duration.prototype.microseconds
 */
static bool Duration_microseconds(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_microseconds>(cx, args);
}

/**
 * get Temporal.Duration.prototype.nanoseconds
 */
static bool Duration_nanoseconds(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  args.rval().setNumber(duration->nanoseconds());
  return true;
}

/**
 * get Temporal.Duration.prototype.nanoseconds
 */
static bool Duration_nanoseconds(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_nanoseconds>(cx, args);
}

/**
 * get Temporal.Duration.prototype.sign
 */
static bool Duration_sign(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Step 3.
  args.rval().setInt32(DurationSign(duration));
  return true;
}

/**
 * get Temporal.Duration.prototype.sign
 */
static bool Duration_sign(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_sign>(cx, args);
}

/**
 * get Temporal.Duration.prototype.blank
 */
static bool Duration_blank(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Steps 3-5.
  args.rval().setBoolean(duration == Duration{});
  return true;
}

/**
 * get Temporal.Duration.prototype.blank
 */
static bool Duration_blank(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_blank>(cx, args);
}

/**
 * Temporal.Duration.prototype.with ( temporalDurationLike )
 *
 * ToPartialDuration ( temporalDurationLike )
 */
static bool Duration_with(JSContext* cx, const CallArgs& args) {
  // Absent values default to the corresponding values of |this| object.
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Steps 3-23.
  Rooted<JSObject*> temporalDurationLike(
      cx, RequireObjectArg(cx, "temporalDurationLike", "with", args.get(0)));
  if (!temporalDurationLike) {
    return false;
  }
  if (!ToTemporalPartialDurationRecord(cx, temporalDurationLike, &duration)) {
    return false;
  }

  // Step 24.
  auto* result = CreateTemporalDuration(cx, duration);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Duration.prototype.with ( temporalDurationLike )
 */
static bool Duration_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_with>(cx, args);
}

/**
 * Temporal.Duration.prototype.negated ( )
 */
static bool Duration_negated(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Step 3.
  auto* result = CreateTemporalDuration(cx, duration.negate());
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Duration.prototype.negated ( )
 */
static bool Duration_negated(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_negated>(cx, args);
}

/**
 * Temporal.Duration.prototype.abs ( )
 */
static bool Duration_abs(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Step 3.
  auto* result = CreateTemporalDuration(cx, AbsoluteDuration(duration));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Duration.prototype.abs ( )
 */
static bool Duration_abs(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_abs>(cx, args);
}

/**
 * Temporal.Duration.prototype.add ( other [ , options ] )
 */
static bool Duration_add(JSContext* cx, const CallArgs& args) {
  return AddDurationToOrSubtractDurationFromDuration(cx, DurationOperation::Add,
                                                     args);
}

/**
 * Temporal.Duration.prototype.add ( other [ , options ] )
 */
static bool Duration_add(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_add>(cx, args);
}

/**
 * Temporal.Duration.prototype.subtract ( other [ , options ] )
 */
static bool Duration_subtract(JSContext* cx, const CallArgs& args) {
  return AddDurationToOrSubtractDurationFromDuration(
      cx, DurationOperation::Subtract, args);
}

/**
 * Temporal.Duration.prototype.subtract ( other [ , options ] )
 */
static bool Duration_subtract(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_subtract>(cx, args);
}

/**
 * Temporal.Duration.prototype.round ( roundTo )
 */
static bool Duration_round(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Step 18. (Reordered)
  auto existingLargestUnit = DefaultTemporalLargestUnit(duration);

  // Steps 3-25.
  auto smallestUnit = TemporalUnit::Auto;
  TemporalUnit largestUnit;
  auto roundingMode = TemporalRoundingMode::HalfExpand;
  auto roundingIncrement = Increment{1};
  Rooted<JSObject*> relativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<ZonedDateTime> zonedRelativeTo(cx);
  Rooted<TimeZoneRecord> timeZone(cx);
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Steps 6-15. (Not applicable)

    // Step 16.
    Rooted<JSString*> paramString(cx, args[0].toString());
    if (!GetTemporalUnit(cx, paramString, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 17. (Not applicable)

    // Step 18. (Moved above)

    // Step 19.
    auto defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    // Step 20. (Not applicable)

    // Step 20.a. (Not applicable)

    // Step 20.b.
    largestUnit = defaultLargestUnit;

    // Steps 21-25. (Not applicable)
  } else {
    // Steps 3 and 5.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "roundTo", "round", args.get(0)));
    if (!options) {
      return false;
    }

    // Step 6.
    bool smallestUnitPresent = true;

    // Step 7.
    bool largestUnitPresent = true;

    // Steps 8-9.
    //
    // Inlined GetTemporalUnit and GetOption so we can more easily detect an
    // absent "largestUnit" value.
    Rooted<Value> largestUnitValue(cx);
    if (!GetProperty(cx, options, options, cx->names().largestUnit,
                     &largestUnitValue)) {
      return false;
    }

    if (!largestUnitValue.isUndefined()) {
      Rooted<JSString*> largestUnitStr(cx, JS::ToString(cx, largestUnitValue));
      if (!largestUnitStr) {
        return false;
      }

      largestUnit = TemporalUnit::Auto;
      if (!GetTemporalUnit(cx, largestUnitStr, TemporalUnitKey::LargestUnit,
                           TemporalUnitGroup::DateTime, &largestUnit)) {
        return false;
      }
    }

    // Steps 10-13.
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo, &timeZone)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
    MOZ_ASSERT_IF(zonedRelativeTo, timeZone.receiver());

    // Step 14.
    if (!ToTemporalRoundingIncrement(cx, options, &roundingIncrement)) {
      return false;
    }

    // Step 15.
    if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
      return false;
    }

    // Step 16.
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 17.
    if (smallestUnit == TemporalUnit::Auto) {
      // Step 17.a.
      smallestUnitPresent = false;

      // Step 17.b.
      smallestUnit = TemporalUnit::Nanosecond;
    }

    // Step 18. (Moved above)

    // Step 19.
    auto defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    // Steps 20-21.
    if (largestUnitValue.isUndefined()) {
      // Step 20.a.
      largestUnitPresent = false;

      // Step 20.b.
      largestUnit = defaultLargestUnit;
    } else if (largestUnit == TemporalUnit::Auto) {
      // Step 21.a
      largestUnit = defaultLargestUnit;
    }

    // Step 22.
    if (!smallestUnitPresent && !largestUnitPresent) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_DURATION_MISSING_UNIT_SPECIFIER);
      return false;
    }

    // Step 23.
    if (largestUnit > smallestUnit) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_RANGE);
      return false;
    }

    // Steps 24-25.
    if (smallestUnit > TemporalUnit::Day) {
      // Step 24.
      auto maximum = MaximumTemporalDurationRoundingIncrement(smallestUnit);

      // Step 25.
      if (!ValidateTemporalRoundingIncrement(cx, roundingIncrement, maximum,
                                             false)) {
        return false;
      }
    }
  }

  // Step 26.
  bool hoursToDaysConversionMayOccur = false;

  // Step 27.
  if (duration.days != 0 && zonedRelativeTo) {
    hoursToDaysConversionMayOccur = true;
  }

  // Step 28.
  else if (std::abs(duration.hours) >= 24) {
    hoursToDaysConversionMayOccur = true;
  }

  // Step 29.
  bool roundingGranularityIsNoop = smallestUnit == TemporalUnit::Nanosecond &&
                                   roundingIncrement == Increment{1};

  // Step 30.
  bool calendarUnitsPresent =
      duration.years != 0 || duration.months != 0 || duration.weeks != 0;

  // Step 31.
  if (roundingGranularityIsNoop && largestUnit == existingLargestUnit &&
      !calendarUnitsPresent && !hoursToDaysConversionMayOccur &&
      std::abs(duration.minutes) < 60 && std::abs(duration.seconds) < 60 &&
      std::abs(duration.milliseconds) < 1000 &&
      std::abs(duration.microseconds) < 1000 &&
      std::abs(duration.nanoseconds) < 1000) {
    // Steps 31.a-b.
    auto* obj = CreateTemporalDuration(cx, duration);
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // Step 32.
  mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime{};

  // Step 33.
  bool plainDateTimeOrRelativeToWillBeUsed =
      !roundingGranularityIsNoop || largestUnit <= TemporalUnit::Day ||
      calendarUnitsPresent || duration.days != 0;

  // Step 34.
  PlainDateTime relativeToDateTime;
  if (zonedRelativeTo && plainDateTimeOrRelativeToWillBeUsed) {
    // Steps 34.a-b.
    const auto& instant = zonedRelativeTo.instant();

    // Step 34.c.
    if (!GetPlainDateTimeFor(cx, timeZone, instant, &relativeToDateTime)) {
      return false;
    }
    precalculatedPlainDateTime =
        mozilla::SomeRef<const PlainDateTime>(relativeToDateTime);

    // Step 34.d.
    plainRelativeTo = CreateTemporalDate(cx, relativeToDateTime.date,
                                         zonedRelativeTo.calendar());
    if (!plainRelativeTo) {
      return false;
    }
  }

  // Step 35.
  Rooted<CalendarRecord> calendar(cx);
  if (!CreateCalendarMethodsRecordFromRelativeTo(cx, plainRelativeTo,
                                                 zonedRelativeTo,
                                                 {
                                                     CalendarMethod::DateAdd,
                                                     CalendarMethod::DateUntil,
                                                 },
                                                 &calendar)) {
    return false;
  }

  // Step 36.
  DateDuration unbalanceResult;
  if (plainRelativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration.toDateDuration(),
                                       largestUnit, plainRelativeTo, calendar,
                                       &unbalanceResult)) {
      return false;
    }
  } else {
    if (!UnbalanceDateDurationRelative(cx, duration.toDateDuration(),
                                       largestUnit, &unbalanceResult)) {
      return false;
    }
    MOZ_ASSERT(duration.toDateDuration() == unbalanceResult);
  }

  // Steps 37-39.
  auto roundInput =
      NormalizedDuration{unbalanceResult, NormalizeTimeDuration(duration)};
  NormalizedDuration roundResult;
  if (plainRelativeTo || zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, plainRelativeTo, calendar,
                         zonedRelativeTo, timeZone, precalculatedPlainDateTime,
                         &roundResult)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, &roundResult)) {
      return false;
    }
  }

  // Steps 40-41.
  TimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 40.a.
    NormalizedDuration adjustResult;
    if (!AdjustRoundedDurationDays(cx, roundResult, roundingIncrement,
                                   smallestUnit, roundingMode, zonedRelativeTo,
                                   calendar, timeZone,
                                   precalculatedPlainDateTime, &adjustResult)) {
      return false;
    }
    roundResult = adjustResult;

    // Step 40.b.
    if (!BalanceTimeDurationRelative(
            cx, roundResult, largestUnit, zonedRelativeTo, timeZone,
            precalculatedPlainDateTime, &balanceResult)) {
      return false;
    }
  } else {
    // Step 41.a.
    NormalizedTimeDuration withDays;
    if (!Add24HourDaysToNormalizedTimeDuration(
            cx, roundResult.time, roundResult.date.days, &withDays)) {
      return false;
    }

    // Step 41.b.
    balanceResult = temporal::BalanceTimeDuration(withDays, largestUnit);
  }

  // Step 41.
  DateDuration balanceInput = {
      roundResult.date.years,
      roundResult.date.months,
      roundResult.date.weeks,
      balanceResult.days,
  };
  DateDuration dateResult;
  if (!::BalanceDateDurationRelative(cx, balanceInput, largestUnit,
                                     smallestUnit, plainRelativeTo, calendar,
                                     &dateResult)) {
    return false;
  }

  // Step 42.
  auto result = Duration{
      dateResult.years,           dateResult.months,
      dateResult.weeks,           dateResult.days,
      balanceResult.hours,        balanceResult.minutes,
      balanceResult.seconds,      balanceResult.milliseconds,
      balanceResult.microseconds, balanceResult.nanoseconds,
  };
  MOZ_ASSERT(IsValidDuration(result));
  auto* obj = CreateTemporalDuration(cx, result);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Duration.prototype.round ( options )
 */
static bool Duration_round(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_round>(cx, args);
}

/**
 * Temporal.Duration.prototype.total ( totalOf )
 */
static bool Duration_total(JSContext* cx, const CallArgs& args) {
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();
  auto duration = ToDuration(durationObj);

  // Steps 3-11.
  Rooted<JSObject*> relativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<ZonedDateTime> zonedRelativeTo(cx);
  Rooted<TimeZoneRecord> timeZone(cx);
  auto unit = TemporalUnit::Auto;
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Steps 6-10. (Implicit)
    MOZ_ASSERT(!plainRelativeTo && !zonedRelativeTo);

    // Step 11.
    Rooted<JSString*> paramString(cx, args[0].toString());
    if (!GetTemporalUnit(cx, paramString, TemporalUnitKey::Unit,
                         TemporalUnitGroup::DateTime, &unit)) {
      return false;
    }
  } else {
    // Steps 3 and 5.
    Rooted<JSObject*> totalOf(
        cx, RequireObjectArg(cx, "totalOf", "total", args.get(0)));
    if (!totalOf) {
      return false;
    }

    // Steps 6-10.
    if (!ToRelativeTemporalObject(cx, totalOf, &plainRelativeTo,
                                  &zonedRelativeTo, &timeZone)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
    MOZ_ASSERT_IF(zonedRelativeTo, timeZone.receiver());

    // Step 11.
    if (!GetTemporalUnit(cx, totalOf, TemporalUnitKey::Unit,
                         TemporalUnitGroup::DateTime, &unit)) {
      return false;
    }

    if (unit == TemporalUnit::Auto) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_MISSING_OPTION, "unit");
      return false;
    }
  }

  // Step 12.
  mozilla::Maybe<const PlainDateTime&> precalculatedPlainDateTime{};

  // Step 13.
  bool plainDateTimeOrRelativeToWillBeUsed =
      unit <= TemporalUnit::Day || duration.toDateDuration() != DateDuration{};

  // Step 14.
  PlainDateTime relativeToDateTime;
  if (zonedRelativeTo && plainDateTimeOrRelativeToWillBeUsed) {
    // Steps 14.a-b.
    const auto& instant = zonedRelativeTo.instant();

    // Step 14.c.
    if (!GetPlainDateTimeFor(cx, timeZone, instant, &relativeToDateTime)) {
      return false;
    }
    precalculatedPlainDateTime =
        mozilla::SomeRef<const PlainDateTime>(relativeToDateTime);

    // Step 14.d
    plainRelativeTo = CreateTemporalDate(cx, relativeToDateTime.date,
                                         zonedRelativeTo.calendar());
    if (!plainRelativeTo) {
      return false;
    }
  }

  // Step 15.
  Rooted<CalendarRecord> calendar(cx);
  if (!CreateCalendarMethodsRecordFromRelativeTo(cx, plainRelativeTo,
                                                 zonedRelativeTo,
                                                 {
                                                     CalendarMethod::DateAdd,
                                                     CalendarMethod::DateUntil,
                                                 },
                                                 &calendar)) {
    return false;
  }

  // Step 16.
  DateDuration unbalanceResult;
  if (plainRelativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration.toDateDuration(), unit,
                                       plainRelativeTo, calendar,
                                       &unbalanceResult)) {
      return false;
    }
  } else {
    if (!UnbalanceDateDurationRelative(cx, duration.toDateDuration(), unit,
                                       &unbalanceResult)) {
      return false;
    }
    MOZ_ASSERT(duration.toDateDuration() == unbalanceResult);
  }

  // Step 17.
  double unbalancedDays = unbalanceResult.days;

  // Steps 18-19.
  int64_t days;
  NormalizedTimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 18.a
    Rooted<ZonedDateTime> intermediate(cx);
    if (!MoveRelativeZonedDateTime(
            cx, zonedRelativeTo, calendar, timeZone,
            {unbalanceResult.years, unbalanceResult.months,
             unbalanceResult.weeks, 0},
            precalculatedPlainDateTime, &intermediate)) {
      return false;
    }

    // Step 18.b.
    auto timeDuration = NormalizeTimeDuration(duration);

    // Step 18.c
    const auto& startNs = intermediate.instant();

    // Step 18.d.
    const auto& startInstant = startNs;

    // Step 18.e.
    mozilla::Maybe<PlainDateTime> startDateTime{};

    // Steps 18.f-g.
    Instant intermediateNs;
    if (unbalancedDays != 0) {
      // Step 18.f.i.
      PlainDateTime dateTime;
      if (!GetPlainDateTimeFor(cx, timeZone, startInstant, &dateTime)) {
        return false;
      }
      startDateTime = mozilla::Some(dateTime);

      // Step 18.f.ii.
      Rooted<CalendarValue> isoCalendar(cx, CalendarValue(cx->names().iso8601));
      Instant addResult;
      if (!AddDaysToZonedDateTime(cx, startInstant, dateTime, timeZone,
                                  isoCalendar, unbalancedDays, &addResult)) {
        return false;
      }

      // Step 18.f.iii.
      intermediateNs = addResult;
    } else {
      // Step 18.g.
      intermediateNs = startNs;
    }

    // Step 18.h.
    Instant endNs;
    if (!AddInstant(cx, intermediateNs, timeDuration, &endNs)) {
      return false;
    }

    // Step 18.i.
    auto difference =
        NormalizedTimeDurationFromEpochNanosecondsDifference(endNs, startNs);

    // Steps 18.j-k.
    //
    // Avoid calling NormalizedTimeDurationToDays for a zero time difference.
    if (TemporalUnit::Year <= unit && unit <= TemporalUnit::Day &&
        difference != NormalizedTimeDuration{}) {
      // Step 18.j.i.
      if (!startDateTime) {
        PlainDateTime dateTime;
        if (!GetPlainDateTimeFor(cx, timeZone, startInstant, &dateTime)) {
          return false;
        }
        startDateTime = mozilla::Some(dateTime);
      }

      // Step 18.j.ii.
      NormalizedTimeAndDays timeAndDays;
      if (!NormalizedTimeDurationToDays(cx, difference, intermediate, timeZone,
                                        *startDateTime, &timeAndDays)) {
        return false;
      }

      // Step 18.j.iii.
      balanceResult = NormalizedTimeDuration::fromNanoseconds(timeAndDays.time);

      // Step 18.j.iv.
      days = timeAndDays.days;
    } else {
      // Step 18.k.i.
      balanceResult = difference;
      days = 0;
    }
  } else {
    // Step 19.a.
    auto timeDuration = NormalizeTimeDuration(duration);

    // Step 19.b.
    if (!Add24HourDaysToNormalizedTimeDuration(cx, timeDuration, unbalancedDays,
                                               &balanceResult)) {
      return false;
    }

    // Step 19.c.
    days = 0;
  }
  MOZ_ASSERT(IsValidNormalizedTimeDuration(balanceResult));

  // Step 20.
  auto roundInput = NormalizedDuration{
      {
          unbalanceResult.years,
          unbalanceResult.months,
          unbalanceResult.weeks,
          double(days),
      },
      balanceResult,
  };
  double total;
  if (plainRelativeTo || zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, plainRelativeTo, calendar,
                         zonedRelativeTo, timeZone, precalculatedPlainDateTime,
                         &total)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, &total)) {
      return false;
    }
  }

  // Step 21.
  args.rval().setNumber(total);
  return true;
}

/**
 * Temporal.Duration.prototype.total ( totalOf )
 */
static bool Duration_total(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_total>(cx, args);
}

/**
 * Temporal.Duration.prototype.toString ( [ options ] )
 */
static bool Duration_toString(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Steps 3-9.
  SecondsStringPrecision precision = {Precision::Auto(),
                                      TemporalUnit::Nanosecond, Increment{1}};
  auto roundingMode = TemporalRoundingMode::Trunc;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Steps 4-5.
    auto digits = Precision::Auto();
    if (!ToFractionalSecondDigits(cx, options, &digits)) {
      return false;
    }

    // Step 6.
    if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
      return false;
    }

    // Step 7.
    auto smallestUnit = TemporalUnit::Auto;
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::Time, &smallestUnit)) {
      return false;
    }

    // Step 8.
    if (smallestUnit == TemporalUnit::Hour ||
        smallestUnit == TemporalUnit::Minute) {
      const char* smallestUnitStr =
          smallestUnit == TemporalUnit::Hour ? "hour" : "minute";
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_OPTION,
                                smallestUnitStr, "smallestUnit");
      return false;
    }

    // Step 9.
    precision = ToSecondsStringPrecision(smallestUnit, digits);
  }

  // Steps 10-11.
  Duration result;
  if (precision.unit != TemporalUnit::Nanosecond ||
      precision.increment != Increment{1}) {
    // Step 10.a.
    auto timeDuration = NormalizeTimeDuration(duration);

    // Step 10.b.
    auto largestUnit = DefaultTemporalLargestUnit(duration);

    // Steps 10.c-d.
    auto rounded = RoundDuration(timeDuration, precision.increment,
                                 precision.unit, roundingMode);

    // Step 10.e.
    auto balanced = BalanceTimeDuration(
        rounded, std::min(largestUnit, TemporalUnit::Second));

    // Step 10.f.
    result = {
        duration.years,        duration.months,
        duration.weeks,        duration.days + balanced.days,
        balanced.hours,        balanced.minutes,
        balanced.seconds,      balanced.milliseconds,
        balanced.microseconds, balanced.nanoseconds,
    };
    MOZ_ASSERT(IsValidDuration(duration));
  } else {
    // Step 11.
    result = duration;
  }

  // Steps 12-13.
  JSString* str = TemporalDurationToString(cx, result, precision.precision);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.Duration.prototype.toString ( [ options ] )
 */
static bool Duration_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_toString>(cx, args);
}

/**
 * Temporal.Duration.prototype.toJSON ( )
 */
static bool Duration_toJSON(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Steps 3-4.
  JSString* str = TemporalDurationToString(cx, duration, Precision::Auto());
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.Duration.prototype.toJSON ( )
 */
static bool Duration_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_toJSON>(cx, args);
}

/**
 * Temporal.Duration.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool Duration_toLocaleString(JSContext* cx, const CallArgs& args) {
  auto duration = ToDuration(&args.thisv().toObject().as<DurationObject>());

  // Steps 3-4.
  JSString* str = TemporalDurationToString(cx, duration, Precision::Auto());
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.Duration.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool Duration_toLocaleString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsDuration, Duration_toLocaleString>(cx, args);
}

/**
 * Temporal.Duration.prototype.valueOf ( )
 */
static bool Duration_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "Duration", "primitive type");
  return false;
}

const JSClass DurationObject::class_ = {
    "Temporal.Duration",
    JSCLASS_HAS_RESERVED_SLOTS(DurationObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Duration),
    JS_NULL_CLASS_OPS,
    &DurationObject::classSpec_,
};

const JSClass& DurationObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec Duration_methods[] = {
    JS_FN("from", Duration_from, 1, 0),
    JS_FN("compare", Duration_compare, 2, 0),
    JS_FS_END,
};

static const JSFunctionSpec Duration_prototype_methods[] = {
    JS_FN("with", Duration_with, 1, 0),
    JS_FN("negated", Duration_negated, 0, 0),
    JS_FN("abs", Duration_abs, 0, 0),
    JS_FN("add", Duration_add, 1, 0),
    JS_FN("subtract", Duration_subtract, 1, 0),
    JS_FN("round", Duration_round, 1, 0),
    JS_FN("total", Duration_total, 1, 0),
    JS_FN("toString", Duration_toString, 0, 0),
    JS_FN("toJSON", Duration_toJSON, 0, 0),
    JS_FN("toLocaleString", Duration_toLocaleString, 0, 0),
    JS_FN("valueOf", Duration_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec Duration_prototype_properties[] = {
    JS_PSG("years", Duration_years, 0),
    JS_PSG("months", Duration_months, 0),
    JS_PSG("weeks", Duration_weeks, 0),
    JS_PSG("days", Duration_days, 0),
    JS_PSG("hours", Duration_hours, 0),
    JS_PSG("minutes", Duration_minutes, 0),
    JS_PSG("seconds", Duration_seconds, 0),
    JS_PSG("milliseconds", Duration_milliseconds, 0),
    JS_PSG("microseconds", Duration_microseconds, 0),
    JS_PSG("nanoseconds", Duration_nanoseconds, 0),
    JS_PSG("sign", Duration_sign, 0),
    JS_PSG("blank", Duration_blank, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.Duration", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec DurationObject::classSpec_ = {
    GenericCreateConstructor<DurationConstructor, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<DurationObject>,
    Duration_methods,
    nullptr,
    Duration_prototype_methods,
    Duration_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
