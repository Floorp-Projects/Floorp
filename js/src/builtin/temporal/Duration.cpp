/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Duration.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
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
#include "gc/Allocator.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
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
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "proxy/DeadObjectProxy.h"
#include "util/StringBuffer.h"
#include "vm/BigIntType.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSContext-inl.h"
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

  // Step 3.
  return true;
}

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

  MOZ_ASSERT(IsValidDuration(duration));

  // Step 3.
  return true;
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
    Rooted<JSString*> string(cx, JS::ToString(cx, temporalDurationLike));
    if (!string) {
      return false;
    }

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
 * CalculateOffsetShift ( relativeTo, y, mon, d )
 */
static bool CalculateOffsetShift(JSContext* cx, Handle<JSObject*> relativeTo,
                                 const Duration& duration, int64_t* result) {
  // Step 1.
  if (!relativeTo) {
    *result = 0;
    return true;
  }

  auto* zonedRelativeTo = relativeTo->maybeUnwrapIf<ZonedDateTimeObject>();
  if (!zonedRelativeTo) {
    *result = 0;
    return true;
  }

  auto epochInstant = ToInstant(zonedRelativeTo);
  Rooted<JSObject*> timeZone(cx, zonedRelativeTo->timeZone());
  Rooted<JSObject*> calendar(cx, zonedRelativeTo->calendar());

  // Wrap into the current compartment.
  if (!cx->compartment()->wrap(cx, &timeZone)) {
    return false;
  }
  if (!cx->compartment()->wrap(cx, &calendar)) {
    return false;
  }

  // Steps 2-3.
  int64_t offsetBefore;
  if (!GetOffsetNanosecondsFor(cx, timeZone, epochInstant, &offsetBefore)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetBefore) < ToNanoseconds(TemporalUnit::Day));

  // Step 4.
  Instant after;
  if (!AddZonedDateTime(cx, epochInstant, timeZone, calendar, duration,
                        &after)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(after));

  // Steps 5-6.
  int64_t offsetAfter;
  if (!GetOffsetNanosecondsFor(cx, timeZone, after, &offsetAfter)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetAfter) < ToNanoseconds(TemporalUnit::Day));

  // Step 7.
  *result = offsetAfter - offsetBefore;
  return true;
}

/**
 * DaysUntil ( earlier, later )
 */
static int32_t DaysUntil(const PlainDate& earlier, const PlainDate& later) {
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
 * MoveRelativeDate ( calendar, relativeTo, duration, dateAdd )
 */
static bool MoveRelativeDate(
    JSContext* cx, Handle<JSObject*> calendar,
    Handle<Wrapped<PlainDateObject*>> relativeTo,
    Handle<DurationObject*> duration, Handle<Value> dateAdd,
    MutableHandle<Wrapped<PlainDateObject*>> relativeToResult,
    int32_t* daysResult) {
  MOZ_ASSERT(IsCallable(dateAdd) || dateAdd.isUndefined());

  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto relativeToDate = ToPlainDate(unwrappedRelativeTo);

  // Step 1.
  auto newDate = CalendarDateAdd(cx, calendar, relativeTo, duration, dateAdd);
  if (!newDate) {
    return false;
  }
  auto later = ToPlainDate(&newDate.unwrap());
  relativeToResult.set(newDate);

  // Step 3.
  *daysResult = DaysUntil(relativeToDate, later);
  MOZ_ASSERT(std::abs(*daysResult) <= 200'000'000);

  // Step 4.
  return true;
}


/**
 * TotalDurationNanoseconds ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, offsetShift )
 */
static mozilla::Maybe<int64_t> TotalDurationNanoseconds(
    const Duration& duration, int64_t offsetShift) {
  MOZ_ASSERT(std::abs(offsetShift) <= 2 * ToNanoseconds(TemporalUnit::Day));

  // Step 2.
  int64_t days;
  if (!mozilla::NumberEqualsInt64(duration.days, &days)) {
    return mozilla::Nothing();
  }
  int64_t hours;
  if (!mozilla::NumberEqualsInt64(duration.hours, &hours)) {
    return mozilla::Nothing();
  }
  mozilla::CheckedInt64 result = days;
  result *= 24;
  result += hours;

  // Step 3.
  int64_t minutes;
  if (!mozilla::NumberEqualsInt64(duration.minutes, &minutes)) {
    return mozilla::Nothing();
  }
  result *= 60;
  result += minutes;

  // Step 4.
  int64_t seconds;
  if (!mozilla::NumberEqualsInt64(duration.seconds, &seconds)) {
    return mozilla::Nothing();
  }
  result *= 60;
  result += seconds;

  // Step 5.
  int64_t milliseconds;
  if (!mozilla::NumberEqualsInt64(duration.milliseconds, &milliseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += milliseconds;

  // Step 6.
  int64_t microseconds;
  if (!mozilla::NumberEqualsInt64(duration.microseconds, &microseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += microseconds;

  // Step 7.
  int64_t nanoseconds;
  if (!mozilla::NumberEqualsInt64(duration.nanoseconds, &nanoseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += nanoseconds;

  // Step 1.
  if (days != 0) {
    result -= offsetShift;
  }

  // Step 7 (Return).
  if (!result.isValid()) {
    return mozilla::Nothing();
  }
  return mozilla::Some(result.value());
}

/**
 * TotalDurationNanoseconds ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, offsetShift )
 */
static BigInt* TotalDurationNanosecondsSlow(JSContext* cx,
                                            const Duration& duration,
                                            int64_t offsetShift) {
  MOZ_ASSERT(std::abs(offsetShift) <= 2 * ToNanoseconds(TemporalUnit::Day));

  Rooted<BigInt*> result(cx, BigInt::createFromDouble(cx, duration.days));
  if (!result) {
    return nullptr;
  }

  Rooted<BigInt*> temp(cx);
  auto multiplyAdd = [&](int32_t factor, double number) {
    temp = BigInt::createFromInt64(cx, factor);
    if (!temp) {
      return false;
    }

    result = BigInt::mul(cx, result, temp);
    if (!result) {
      return false;
    }

    temp = BigInt::createFromDouble(cx, number);
    if (!temp) {
      return false;
    }

    result = BigInt::add(cx, result, temp);
    return !!result;
  };

  // Step 2.
  if (!multiplyAdd(24, duration.hours)) {
    return nullptr;
  }

  // Step 3.
  if (!multiplyAdd(60, duration.minutes)) {
    return nullptr;
  }

  // Step 4.
  if (!multiplyAdd(60, duration.seconds)) {
    return nullptr;
  }

  // Step 5.
  if (!multiplyAdd(1000, duration.milliseconds)) {
    return nullptr;
  }

  // Step 6.
  if (!multiplyAdd(1000, duration.microseconds)) {
    return nullptr;
  }

  // Step 7.
  if (!multiplyAdd(1000, duration.nanoseconds)) {
    return nullptr;
  }

  // Step 1.
  if (duration.days != 0 && offsetShift != 0) {
    temp = BigInt::createFromInt64(cx, offsetShift);
    if (!temp) {
      return nullptr;
    }

    result = BigInt::sub(cx, result, temp);
    if (!result) {
      return nullptr;
    }
  }

  // Step 7 (Return).
  return result;
}

struct NanosecondsAndDays final {
  int32_t days = 0;
  int64_t nanoseconds = 0;
};

/**
 * NanosecondsToDays ( nanoseconds, relativeTo )
 */
static ::NanosecondsAndDays NanosecondsToDays(int64_t nanoseconds) {
  // Step 1.
  constexpr int64_t dayLengthNs = ToNanoseconds(TemporalUnit::Day);

  static_assert(INT64_MAX / dayLengthNs <= INT32_MAX,
                "days doesn't exceed INT32_MAX");

  // Steps 2-4.
  return {int32_t(nanoseconds / dayLengthNs), nanoseconds % dayLengthNs};
}

/**
 * NanosecondsToDays ( nanoseconds, relativeTo )
 */
static bool NanosecondsToDaysSlow(
    JSContext* cx, Handle<BigInt*> nanoseconds,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  // Step 1.
  constexpr int64_t dayLengthNs = ToNanoseconds(TemporalUnit::Day);

  Rooted<BigInt*> dayLength(cx, BigInt::createFromInt64(cx, dayLengthNs));
  if (!dayLength) {
    return false;
  }

  // Step 2 is a fast path in the spec when |nanoseconds| is zero, but since
  // we're already in the slow path don't worry about special casing it here.

  // Steps 2-4.
  Rooted<BigInt*> days(cx);
  Rooted<BigInt*> nanos(cx);
  if (!BigInt::divmod(cx, nanoseconds, dayLength, &days, &nanos)) {
    return false;
  }

  result.initialize(days, ToInstantDifference(nanos),
                    Instant::fromNanoseconds(dayLengthNs));
  return true;
}

/**
 * CreateTimeDurationRecord ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static TimeDuration CreateTimeDurationRecord(double days, int64_t hours,
                                             int64_t minutes, int64_t seconds,
                                             int64_t milliseconds,
                                             int64_t microseconds,
                                             int64_t nanoseconds) {
  MOZ_ASSERT(IsInteger(days));
  MOZ_ASSERT(!mozilla::IsNegativeZero(days));

  // Step 1.
  MOZ_ASSERT(IsValidDuration({0, 0, 0, days, double(hours), double(minutes),
                              double(seconds), double(microseconds),
                              double(nanoseconds)}));

  // Step 2.
  return {
      days,
      double(hours),
      double(minutes),
      double(seconds),
      double(milliseconds),
      double(microseconds),
      double(nanoseconds),
  };
}

/**
 * CreateTimeDurationRecord ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static bool CreateTimeDurationRecordPossiblyInfinite(
    JSContext* cx, double days, double hours, double minutes, double seconds,
    double milliseconds, double microseconds, double nanoseconds,
    TimeDuration* result) {
  MOZ_ASSERT(!std::isnan(days) && !std::isnan(hours) && !std::isnan(minutes) &&
             !std::isnan(seconds) && !std::isnan(milliseconds) &&
             !std::isnan(microseconds) && !std::isnan(nanoseconds));

  for (double v : {days, hours, minutes, seconds, milliseconds, microseconds,
                   nanoseconds}) {
    if (std::isinf(v)) {
      *result = {
          days,         hours,        minutes,     seconds,
          milliseconds, microseconds, nanoseconds,
      };
      return true;
    }
  }

  // Step 1.
  if (!ThrowIfInvalidDuration(cx, {0, 0, 0, days, hours, minutes, seconds,
                                   milliseconds, microseconds, nanoseconds})) {
    return false;
  }

  // Step 2.
  // NB: Adds +0.0 to correctly handle negative zero.
  *result = {
      days + (+0.0),        hours + (+0.0),        minutes + (+0.0),
      seconds + (+0.0),     milliseconds + (+0.0), microseconds + (+0.0),
      nanoseconds + (+0.0),
  };
  return true;
}

/**
 * BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds, largestUnit [ , relativeTo ] )
 *
 * BalancePossiblyInfiniteDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
static TimeDuration BalanceDuration(double days, int64_t nanoseconds,
                                    TemporalUnit largestUnit) {
  MOZ_ASSERT(IsInteger(days));
  MOZ_ASSERT_IF(days < 0, nanoseconds <= 0);
  MOZ_ASSERT_IF(days > 0, nanoseconds >= 0);

  // Steps 1-5. (Handled in caller.)

  // Step 6.
  int64_t hours = 0;
  int64_t minutes = 0;
  int64_t seconds = 0;
  int64_t milliseconds = 0;
  int64_t microseconds = 0;

  // Steps 7-8. (Not applicable in our implementation.)
  //
  // We don't need to convert to positive numbers, because integer division
  // truncates and the %-operator has modulo semantics.

  // Steps 9-13.
  switch (largestUnit) {
    // Step 9.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day:
    case TemporalUnit::Hour: {
      // Step 9.a.
      microseconds = nanoseconds / 1000;

      // Step 9.b.
      nanoseconds = nanoseconds % 1000;

      // Step 9.c.
      milliseconds = microseconds / 1000;

      // Step 9.d.
      microseconds = microseconds % 1000;

      // Step 9.e.
      seconds = milliseconds / 1000;

      // Step 9.f.
      milliseconds = milliseconds % 1000;

      // Step 9.g.
      minutes = seconds / 60;

      // Step 9.h.
      seconds = seconds % 60;

      // Step 9.i.
      hours = minutes / 60;

      // Step 9.j.
      minutes = minutes % 60;

      break;
    }

    // Step 10.
    case TemporalUnit::Minute: {
      // Step 10.a.
      microseconds = nanoseconds / 1000;

      // Step 10.b.
      nanoseconds = nanoseconds % 1000;

      // Step 10.c.
      milliseconds = microseconds / 1000;

      // Step 10.d.
      microseconds = microseconds % 1000;

      // Step 10.e.
      seconds = milliseconds / 1000;

      // Step 10.f.
      milliseconds = milliseconds % 1000;

      // Step 10.g.
      minutes = seconds / 60;

      // Step 10.h.
      seconds = seconds % 60;

      break;
    }

    // Step 11.
    case TemporalUnit::Second: {
      // Step 11.a.
      microseconds = nanoseconds / 1000;

      // Step 11.b.
      nanoseconds = nanoseconds % 1000;

      // Step 11.c.
      milliseconds = microseconds / 1000;

      // Step 11.d.
      microseconds = microseconds % 1000;

      // Step 11.e.
      seconds = milliseconds / 1000;

      // Step 11.f.
      milliseconds = milliseconds % 1000;

      break;
    }

    // Step 12.
    case TemporalUnit::Millisecond: {
      // Step 12.a.
      microseconds = nanoseconds / 1000;

      // Step 12.b.
      nanoseconds = nanoseconds % 1000;

      // Step 12.c.
      milliseconds = microseconds / 1000;

      // Step 12.d.
      microseconds = microseconds % 1000;

      break;
    }

    // Step 13.
    case TemporalUnit::Microsecond: {
      // Step 13.a.
      microseconds = nanoseconds / 1000;

      // Step 13.b.
      nanoseconds = nanoseconds % 1000;

      break;
    }

    // Step 14.
    case TemporalUnit::Nanosecond: {
      // Nothing to do.
      break;
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 15. (Not applicable, all values are finite)

  // Step 16.
  return CreateTimeDurationRecord(days, hours, minutes, seconds, milliseconds,
                                  microseconds, nanoseconds);
}

/**
 * BalancePossiblyInfiniteDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
static bool BalancePossiblyInfiniteDurationSlow(JSContext* cx, double days,
                                                Handle<BigInt*> nanos,
                                                TemporalUnit largestUnit,
                                                TimeDuration* result) {
  // Steps 1-5. (Handled in caller.)

  BigInt* zero = BigInt::zero(cx);
  if (!zero) {
    return false;
  }

  // Step 6.
  Rooted<BigInt*> hours(cx, zero);
  Rooted<BigInt*> minutes(cx, zero);
  Rooted<BigInt*> seconds(cx, zero);
  Rooted<BigInt*> milliseconds(cx, zero);
  Rooted<BigInt*> microseconds(cx, zero);
  Rooted<BigInt*> nanoseconds(cx, nanos);

  // Steps 7-8.
  //
  // We don't need to convert to positive numbers, because BigInt division
  // truncates and BigInt modulo has modulo semantics.

  // Steps 9-14.
  Rooted<BigInt*> thousand(cx, BigInt::createFromInt64(cx, 1000));
  if (!thousand) {
    return false;
  }

  Rooted<BigInt*> sixty(cx, BigInt::createFromInt64(cx, 60));
  if (!sixty) {
    return false;
  }

  switch (largestUnit) {
    // Step 9.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day:
    case TemporalUnit::Hour: {
      // Steps 9.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 9.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 9.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      // Steps 9.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      // Steps 9.i-j.
      if (!BigInt::divmod(cx, minutes, sixty, &hours, &minutes)) {
        return false;
      }

      break;
    }

    // Step 10.
    case TemporalUnit::Minute: {
      // Steps 10.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 10.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 10.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      // Steps 10.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      break;
    }

    // Step 11.
    case TemporalUnit::Second: {
      // Steps 11.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 11.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 11.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      break;
    }

    // Step 12.
    case TemporalUnit::Millisecond: {
      // Steps 21.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 12.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      break;
    }

    // Step 13.
    case TemporalUnit::Microsecond: {
      // Steps 13.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      break;
    }

    // Step 14.
    case TemporalUnit::Nanosecond: {
      // Nothing to do.
      break;
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Steps 15-16.
  return CreateTimeDurationRecordPossiblyInfinite(
      cx, days, BigInt::numberValue(hours), BigInt::numberValue(minutes),
      BigInt::numberValue(seconds), BigInt::numberValue(milliseconds),
      BigInt::numberValue(microseconds), BigInt::numberValue(nanoseconds),
      result);
}

/**
 * BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds, largestUnit [ , relativeTo ] )
 *
 * BalancePossiblyInfiniteDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
static TimeDuration BalanceDuration(int64_t nanoseconds,
                                    TemporalUnit largestUnit) {
  // Steps 1-3. (Not applicable)

  // Steps 4-5.
  double days = 0;
  if (TemporalUnit::Year <= largestUnit && largestUnit <= TemporalUnit::Day) {
    // Step 4.a.
    auto nanosAndDays = ::NanosecondsToDays(nanoseconds);

    // Step 4.b.
    days = nanosAndDays.days;

    // Step 4.c.
    nanoseconds = nanosAndDays.nanoseconds;
  }

  // Steps 6-16.
  return ::BalanceDuration(days, nanoseconds, largestUnit);
}

/**
 * BalancePossiblyInfiniteDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
static bool BalancePossiblyInfiniteDurationSlow(JSContext* cx,
                                                Handle<BigInt*> nanoseconds,
                                                TemporalUnit largestUnit,
                                                TimeDuration* result) {
  // Steps 1-3. (Not applicable)

  // Step 4.
  if (TemporalUnit::Year <= largestUnit && largestUnit <= TemporalUnit::Day) {
    // Step 4.a.
    Rooted<temporal::NanosecondsAndDays> nanosAndDays(cx);
    if (!::NanosecondsToDaysSlow(cx, nanoseconds, &nanosAndDays)) {
      return false;
    }

    // NB: |days| is passed to CreateTimeDurationRecord, which performs
    // |ℝ(𝔽(days))|, so it's safe to convert from BigInt to double here.

    // Step 4.b.
    double days = nanosAndDays.daysNumber();

    // Step 4.c.
    int64_t nanos = nanosAndDays.nanoseconds().toNanoseconds().value();

    // Step 15. (Reordered)
    if (!std::isfinite(days)) {
      *result = {days};
      return true;
    }
    MOZ_ASSERT(IsInteger(days));

    // Steps 6-15.
    *result = ::BalanceDuration(days, nanos, largestUnit);
    return true;
  }

  // Step 5.a.
  double days = 0;

  // Steps 6-15.
  return ::BalancePossiblyInfiniteDurationSlow(cx, days, nanoseconds,
                                               largestUnit, result);
}

/**
 * BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds, largestUnit [ , relativeTo ] )
 */
static bool BalanceDurationSlow(JSContext* cx, Handle<BigInt*> nanoseconds,
                                TemporalUnit largestUnit,
                                TimeDuration* result) {
  // Step 1.
  if (!BalancePossiblyInfiniteDurationSlow(cx, nanoseconds, largestUnit,
                                           result)) {
    return false;
  }

  // Steps 2-3.
  return ThrowIfInvalidDuration(cx, result->toDuration());
}

/**
 * BalancePossiblyInfiniteDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
static bool BalancePossiblyInfiniteDuration(JSContext* cx,
                                            const Duration& duration,
                                            TemporalUnit largestUnit,
                                            TimeDuration* result) {
  // NB: |duration.days| can have a different sign than the time components.
  MOZ_ASSERT(IsValidDuration(duration.time()));

  // Steps 1-2. (Not applicable)

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto nanoseconds = TotalDurationNanoseconds(duration, 0)) {
    *result = ::BalanceDuration(*nanoseconds, largestUnit);
    return true;
  }

  // Step 3.
  Rooted<BigInt*> nanoseconds(cx,
                              TotalDurationNanosecondsSlow(cx, duration, 0));
  if (!nanoseconds) {
    return false;
  }

  // Steps 4-16.
  return ::BalancePossiblyInfiniteDurationSlow(cx, nanoseconds, largestUnit,
                                               result);
}

/**
 * BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds, largestUnit [ , relativeTo ] )
 */
bool js::temporal::BalanceDuration(JSContext* cx, const Duration& duration,
                                   TemporalUnit largestUnit,
                                   TimeDuration* result) {
  if (!::BalancePossiblyInfiniteDuration(cx, duration, largestUnit, result)) {
    return false;
  }
  return ThrowIfInvalidDuration(cx, result->toDuration());
}

/**
 * BalanceDuration ( days, hours, minutes, seconds, milliseconds, microseconds,
 * nanoseconds, largestUnit [ , relativeTo ] )
 */
bool js::temporal::BalanceDuration(JSContext* cx, const Instant& nanoseconds,
                                   TemporalUnit largestUnit,
                                   TimeDuration* result) {
  MOZ_ASSERT(IsValidInstantDifference(nanoseconds));

  // Steps 1-3. (Not applicable)

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto nanos = nanoseconds.toNanoseconds(); nanos.isValid()) {
    *result = ::BalanceDuration(nanos.value(), largestUnit);
    return true;
  }

  Rooted<BigInt*> nanos(cx, ToEpochDifferenceNanoseconds(cx, nanoseconds));
  if (!nanos) {
    return false;
  }

  // Steps 4-16.
  return ::BalanceDurationSlow(cx, nanos, largestUnit, result);
}

/**
 * CreateDateDurationRecord ( years, months, weeks, days )
 */
static DateDuration CreateDateDurationRecord(double years, double months,
                                             double weeks, double days) {
  MOZ_ASSERT(IsValidDuration({years, months, weeks, days}));
  return {years, months, weeks, days};
}

/**
 * CreateDateDurationRecord ( years, months, weeks, days )
 */
static bool CreateDateDurationRecord(JSContext* cx, double years, double months,
                                     double weeks, double days,
                                     DateDuration* result) {
  if (!ThrowIfInvalidDuration(cx, {years, months, weeks, days})) {
    return false;
  }

  *result = {years, months, weeks, days};
  return true;
}

static double IsSafeInteger(double num) {
  MOZ_ASSERT(js::IsInteger(num) || std::isinf(num));

  constexpr double maxSafeInteger = DOUBLE_INTEGRAL_PRECISION_LIMIT - 1;
  constexpr double minSafeInteger = -maxSafeInteger;
  return minSafeInteger <= num && num <= maxSafeInteger;
}

/**
 * UnbalanceDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDurationRelativeSlow(
    JSContext* cx, const Duration& duration, double amountToAdd,
    TemporalUnit largestUnit, int32_t sign,
    MutableHandle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<JSObject*> calendar, Handle<DurationObject*> oneYear,
    Handle<DurationObject*> oneMonth, Handle<DurationObject*> oneWeek,
    Handle<Value> dateAdd, Handle<Value> dateUntil, DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));
  MOZ_ASSERT(dateRelativeTo);
  MOZ_ASSERT(calendar);

  Rooted<BigInt*> years(cx, BigInt::createFromDouble(cx, duration.years));
  if (!years) {
    return false;
  }

  Rooted<BigInt*> months(cx, BigInt::createFromDouble(cx, duration.months));
  if (!months) {
    return false;
  }

  Rooted<BigInt*> weeks(cx, BigInt::createFromDouble(cx, duration.weeks));
  if (!weeks) {
    return false;
  }

  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, duration.days));
  if (!days) {
    return false;
  }

  // Step 1.
  MOZ_ASSERT(largestUnit != TemporalUnit::Year);
  MOZ_ASSERT(!years->isZero() || !months->isZero() || !weeks->isZero() ||
             !days->isZero());

  // Step 2. (Not applicable)

  // Step 3.
  MOZ_ASSERT(sign == -1 || sign == 1);

  // Steps 4-8. (Not applicable)

  // Steps 9-11.
  if (largestUnit == TemporalUnit::Month) {
    // Steps 9.a-c. (Not applicable)

    if (amountToAdd) {
      Rooted<BigInt*> toAdd(cx, BigInt::createFromDouble(cx, amountToAdd));
      if (!toAdd) {
        return false;
      }

      months = BigInt::add(cx, months, toAdd);
      if (!months) {
        return false;
      }

      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }
    }

    // Step 9.d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    Rooted<BigInt*> oneYearMonths(cx);
    while (!years->isZero()) {
      // Step 9.d.i.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 9.d.ii-iv.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 9.d.v.
      oneYearMonths = BigInt::createFromDouble(cx, untilResult.months);
      if (!oneYearMonths) {
        return false;
      }

      // Step 9.d.vi.
      dateRelativeTo.set(newRelativeTo);

      // Step 9.d.vii.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }

      // Step 9.d.viii.
      months = BigInt::add(cx, months, oneYearMonths);
      if (!months) {
        return false;
      }
    }
  } else if (largestUnit == TemporalUnit::Week) {
    // Steps 10.a-b. (Not applicable)

    if (amountToAdd) {
      Rooted<BigInt*> toAdd(cx, BigInt::createFromDouble(cx, amountToAdd));
      if (!toAdd) {
        return false;
      }

      days = BigInt::add(cx, days, toAdd);
      if (!days) {
        return false;
      }

      if (!years->isZero()) {
        if (sign < 0) {
          years = BigInt::inc(cx, years);
        } else {
          years = BigInt::dec(cx, years);
        }
        if (!years) {
          return false;
        }
      } else {
        MOZ_ASSERT(!months->isZero());
        if (sign < 0) {
          months = BigInt::inc(cx, months);
        } else {
          months = BigInt::dec(cx, months);
        }
        if (!months) {
          return false;
        }
      }
    }

    // Step 10.c.
    Rooted<BigInt*> oneYearDays(cx);
    while (!years->isZero()) {
      // Steps 10.c.i-ii.
      int32_t oneYearDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            dateRelativeTo, &oneYearDaysInt)) {
        return false;
      }
      oneYearDays = BigInt::createFromInt64(cx, oneYearDaysInt);
      if (!oneYearDays) {
        return false;
      }

      // Step 10.c.iii.
      days = BigInt::add(cx, days, oneYearDays);
      if (!days) {
        return false;
      }

      // Step 10.c.iv.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }
    }

    // Step 10.d.
    Rooted<BigInt*> oneMonthDays(cx);
    while (!months->isZero()) {
      // Steps 10.d.i-ii.
      int32_t oneMonthDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            dateRelativeTo, &oneMonthDaysInt)) {
        return false;
      }
      oneMonthDays = BigInt::createFromInt64(cx, oneMonthDaysInt);
      if (!oneMonthDays) {
        return false;
      }

      // Step 10.d.iii.
      days = BigInt::add(cx, days, oneMonthDays);
      if (!days) {
        return false;
      }

      // Step 10.d.iv.
      if (sign < 0) {
        months = BigInt::inc(cx, months);
      } else {
        months = BigInt::dec(cx, months);
      }
      if (!months) {
        return false;
      }
    }
  } else if (!years->isZero() || !months->isZero() || !weeks->isZero()) {
    if (amountToAdd) {
      Rooted<BigInt*> toAdd(cx, BigInt::createFromDouble(cx, amountToAdd));
      if (!toAdd) {
        return false;
      }

      days = BigInt::add(cx, days, toAdd);
      if (!days) {
        return false;
      }

      if (!years->isZero()) {
        if (sign < 0) {
          years = BigInt::inc(cx, years);
        } else {
          years = BigInt::dec(cx, years);
        }
        if (!years) {
          return false;
        }
      } else if (!months->isZero()) {
        if (sign < 0) {
          months = BigInt::inc(cx, months);
        } else {
          months = BigInt::dec(cx, months);
        }
        if (!months) {
          return false;
        }
      } else {
        MOZ_ASSERT(!weeks->isZero());

        if (sign < 0) {
          weeks = BigInt::inc(cx, weeks);
        } else {
          weeks = BigInt::dec(cx, weeks);
        }
        if (!years) {
          return false;
        }
      }
    }

    // Step 11.a.

    // Steps 11.a.i-ii. (Not applicable)

    // Step 11.a.iii.
    Rooted<BigInt*> oneYearDays(cx);
    while (!years->isZero()) {
      // Steps 11.a.iii.1-2.
      int32_t oneYearDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            dateRelativeTo, &oneYearDaysInt)) {
        return false;
      }
      oneYearDays = BigInt::createFromInt64(cx, oneYearDaysInt);
      if (!oneYearDays) {
        return false;
      }

      // Step 11.a.iii.3.
      days = BigInt::add(cx, days, oneYearDays);
      if (!days) {
        return false;
      }

      // Step 11.a.iii.4.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }
    }

    // Step 11.a.iv.
    Rooted<BigInt*> oneMonthDays(cx);
    while (!months->isZero()) {
      // Steps 11.a.iv.1-2.
      int32_t oneMonthDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            dateRelativeTo, &oneMonthDaysInt)) {
        return false;
      }
      oneMonthDays = BigInt::createFromInt64(cx, oneMonthDaysInt);
      if (!oneMonthDays) {
        return false;
      }

      // Step 11.a.iv.3.
      days = BigInt::add(cx, days, oneMonthDays);
      if (!days) {
        return false;
      }

      // Step 11.a.iv.4.
      if (sign < 0) {
        months = BigInt::inc(cx, months);
      } else {
        months = BigInt::dec(cx, months);
      }
      if (!months) {
        return false;
      }
    }

    // Step 11.a.v.
    Rooted<BigInt*> oneWeekDays(cx);
    while (!weeks->isZero()) {
      // Steps 11.a.v.1-2.
      int32_t oneWeekDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            dateRelativeTo, &oneWeekDaysInt)) {
        return false;
      }
      oneWeekDays = BigInt::createFromInt64(cx, oneWeekDaysInt);
      if (!oneWeekDays) {
        return false;
      }

      // Step 11.a.v.3.
      days = BigInt::add(cx, days, oneWeekDays);
      if (!days) {
        return false;
      }

      // Step 11.a.v.4.
      if (sign < 0) {
        weeks = BigInt::inc(cx, weeks);
      } else {
        weeks = BigInt::dec(cx, weeks);
      }
      if (!years) {
        return false;
      }
    }
  }

  // Step 12.
  return CreateDateDurationRecord(
      cx, BigInt::numberValue(years), BigInt::numberValue(months),
      BigInt::numberValue(weeks), BigInt::numberValue(days), result);
}

/**
 * UnbalanceDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDurationRelative(JSContext* cx, const Duration& duration,
                                      TemporalUnit largestUnit,
                                      Handle<JSObject*> relativeTo,
                                      DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // Step 1.
  if (largestUnit == TemporalUnit::Year ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 1.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Step 2.
  int32_t sign = DurationSign({years, months, weeks, days});

  // Step 3.
  MOZ_ASSERT(sign != 0);

  // Step 4.
  Rooted<DurationObject*> oneYear(cx,
                                  CreateTemporalDuration(cx, {double(sign)}));
  if (!oneYear) {
    return false;
  }

  // Step 5.
  Rooted<DurationObject*> oneMonth(
      cx, CreateTemporalDuration(cx, {0, double(sign)}));
  if (!oneMonth) {
    return false;
  }

  // Step 6.
  Rooted<DurationObject*> oneWeek(
      cx, CreateTemporalDuration(cx, {0, 0, double(sign)}));
  if (!oneWeek) {
    return false;
  }

  // Steps 7-8.
  auto date = ToTemporalDate(cx, relativeTo);
  if (!date) {
    return false;
  }
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, date);

  Rooted<JSObject*> calendar(cx, date.unwrap().calendar());
  if (!cx->compartment()->wrap(cx, &calendar)) {
    return false;
  }

  // Steps 9-11.
  if (largestUnit == TemporalUnit::Month) {
    // Step 9.a. (Not applicable in our implementation.)

    // Step 9.b.
    Rooted<Value> dateAdd(cx);
    if (!GetMethod(cx, calendar, cx->names().dateAdd, &dateAdd)) {
      return false;
    }

    // Step 9.c.
    Rooted<Value> dateUntil(cx);
    if (!GetMethod(cx, calendar, cx->names().dateUntil, &dateUntil)) {
      return false;
    }

    // Go to the slow path when the result is inexact.
    // NB: |years -= sign| is equal to |years| for large number values.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months))) {
      return UnbalanceDurationRelativeSlow(cx, {years, months, weeks, days}, 0,
                                           largestUnit, sign, &dateRelativeTo,
                                           calendar, oneYear, oneMonth, oneWeek,
                                           dateAdd, dateUntil, result);
    }

    // Step 9.d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    while (years != 0) {
      // Step 9.d.i.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 9.d.ii-iv.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 9.d.v.
      double oneYearMonths = untilResult.months;

      // Step 9.d.vi.
      dateRelativeTo = newRelativeTo;

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(months + oneYearMonths))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearMonths, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 9.d.vii.
      years -= sign;

      // Step 9.d.viii.
      months += oneYearMonths;
    }
  } else if (largestUnit == TemporalUnit::Week) {
    // Step 10.a. (Not applicable in our implementation.)

    // Step 10.b.
    Rooted<Value> dateAdd(cx);
    if (!GetMethod(cx, calendar, cx->names().dateAdd, &dateAdd)) {
      return false;
    }

    // Go to the slow path when the result is inexact.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months) ||
                     !IsSafeInteger(days))) {
      return UnbalanceDurationRelativeSlow(
          cx, {years, months, weeks, days}, 0, largestUnit, sign,
          &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
          UndefinedHandleValue, result);
    }

    // Step 10.c.
    while (years != 0) {
      // Steps 10.c.i-ii.
      int32_t oneYearDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &dateRelativeTo, &oneYearDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneYearDays))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 10.c.iii.
      days += oneYearDays;

      // Step 10.c.iv.
      years -= sign;
    }

    // Step 10.d.
    while (months != 0) {
      // Steps 10.d.i-ii.
      int32_t oneMonthDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &dateRelativeTo, &oneMonthDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneMonthDays))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneMonthDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 10.d.iii.
      days += oneMonthDays;

      // Step 10.d.iv.
      months -= sign;
    }
  } else if (years != 0 || months != 0 || weeks != 0) {
    // Step 11.a.

    // FIXME: why don't we unconditionally throw an error for missing calendars?

    // Step 11.a.i. (Not applicable in our implementation.)

    // Step 11.a.ii.
    Rooted<Value> dateAdd(cx);
    if (!GetMethod(cx, calendar, cx->names().dateAdd, &dateAdd)) {
      return false;
    }

    // Go to the slow path when the result is inexact.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months) ||
                     !IsSafeInteger(weeks) || !IsSafeInteger(days))) {
      return UnbalanceDurationRelativeSlow(
          cx, {years, months, weeks, days}, 0, largestUnit, sign,
          &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
          UndefinedHandleValue, result);
    }

    // Step 11.a.iii.
    while (years != 0) {
      // Steps 11.a.iii.1-2.
      int32_t oneYearDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &dateRelativeTo, &oneYearDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneYearDays))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 11.a.iii.3.
      days += oneYearDays;

      // Step 11.a.iii.4.
      years -= sign;
    }

    // Step 11.a.iv.
    while (months != 0) {
      // Steps 11.a.iv.1-2.
      int32_t oneMonthDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &dateRelativeTo, &oneMonthDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneMonthDays))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneMonthDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 11.a.iv.3.
      days += oneMonthDays;

      // Step 11.a.iv.4.
      months -= sign;
    }

    // Step 11.a.v.
    while (weeks != 0) {
      // Steps 11.a.v.1-2.
      int32_t oneWeekDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            &dateRelativeTo, &oneWeekDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneWeekDays))) {
        return UnbalanceDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneWeekDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 11.a.v.3.
      days += oneWeekDays;

      // Step 11.a.v.4.
      weeks -= sign;
    }
  }

  // Step 12.
  return CreateDateDurationRecord(cx, years, months, weeks, days, result);
}

/**
 * UnbalanceDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDurationRelative(JSContext* cx, const Duration& duration,
                                      TemporalUnit largestUnit,
                                      DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // Step 1.
  if (largestUnit == TemporalUnit::Year ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 1.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Steps 2-8. (Not applicable in our implementation.)

  // Steps 9-11.
  if (largestUnit == TemporalUnit::Month) {
    // Step 9.a.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  } else if (largestUnit == TemporalUnit::Week) {
    // Step 10.a.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  } else if (years != 0 || months != 0 || weeks != 0) {
    // Step 11.a.i.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  }

  // Step 12.
  *result = CreateDateDurationRecord(years, months, weeks, days);
  return true;
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
 * TemporalDurationToString ( years, months, weeks, days, hours, minutes,
 * seconds, milliseconds, microseconds, nanoseconds, precision )
 */
static JSString* TemporalDurationToString(JSContext* cx,
                                          const Duration& duration,
                                          Precision precision) {
  MOZ_ASSERT(IsValidDuration(duration));
  MOZ_ASSERT(!precision.isMinute());

  // Convert to absolute values up front. This is okay to do, because when the
  // duration is valid, all components have the same sign.
  const auto& [years, months, weeks, days, hours, minutes, seconds,
               milliseconds, microseconds, nanoseconds] =
      AbsoluteDuration(duration);

  // Fast path for zero durations.
  if (years == 0 && months == 0 && weeks == 0 && days == 0 && hours == 0 &&
      minutes == 0 && seconds == 0 && milliseconds == 0 && microseconds == 0 &&
      nanoseconds == 0 && (precision.isAuto() || precision.value() == 0)) {
    return NewStringCopyZ<CanGC>(cx, "PT0S");
  }

  Rooted<BigInt*> totalSecondsBigInt(cx);
  double totalSeconds = seconds;
  int32_t fraction = 0;
  if (milliseconds != 0 || microseconds != 0 || nanoseconds != 0) {
    bool imprecise = false;
    do {
      int64_t sec;
      int64_t milli;
      int64_t micro;
      int64_t nano;
      if (!mozilla::NumberEqualsInt64(seconds, &sec) ||
          !mozilla::NumberEqualsInt64(milliseconds, &milli) ||
          !mozilla::NumberEqualsInt64(microseconds, &micro) ||
          !mozilla::NumberEqualsInt64(nanoseconds, &nano)) {
        imprecise = true;
        break;
      }

      mozilla::CheckedInt64 intermediate;

      // Step 2.
      intermediate = micro;
      intermediate += (nano / 1000);
      if (!intermediate.isValid()) {
        imprecise = true;
        break;
      }
      micro = intermediate.value();

      // Step 3.
      nano %= 1000;

      // Step 4.
      intermediate = milli;
      intermediate += (micro / 1000);
      if (!intermediate.isValid()) {
        imprecise = true;
        break;
      }
      milli = intermediate.value();

      // Step 5.
      micro %= 1000;

      // Step 6.
      intermediate = sec;
      intermediate += (milli / 1000);
      if (!intermediate.isValid()) {
        imprecise = true;
        break;
      }
      sec = intermediate.value();

      // Step 7.
      milli %= 1000;

      if (sec < int64_t(DOUBLE_INTEGRAL_PRECISION_LIMIT)) {
        totalSeconds = double(sec);
      } else {
        totalSecondsBigInt = BigInt::createFromInt64(cx, sec);
        if (!totalSecondsBigInt) {
          return nullptr;
        }
      }

      // These are now all in the range [0, 999].
      MOZ_ASSERT(0 <= milli && milli <= 999);
      MOZ_ASSERT(0 <= micro && micro <= 999);
      MOZ_ASSERT(0 <= nano && nano <= 999);

      // Step 16.a. (Reordered)
      fraction = milli * 1'000'000 + micro * 1'000 + nano;
      MOZ_ASSERT(0 <= fraction && fraction < 1'000'000'000);
    } while (false);

    // If a result was imprecise, recompute with BigInt to get full precision.
    if (imprecise) {
      Rooted<BigInt*> secs(cx, BigInt::createFromDouble(cx, seconds));
      if (!secs) {
        return nullptr;
      }

      Rooted<BigInt*> millis(cx, BigInt::createFromDouble(cx, milliseconds));
      if (!millis) {
        return nullptr;
      }

      Rooted<BigInt*> micros(cx, BigInt::createFromDouble(cx, microseconds));
      if (!micros) {
        return nullptr;
      }

      Rooted<BigInt*> nanos(cx, BigInt::createFromDouble(cx, nanoseconds));
      if (!nanos) {
        return nullptr;
      }

      Rooted<BigInt*> thousand(cx, BigInt::createFromInt64(cx, 1000));
      if (!thousand) {
        return nullptr;
      }

      // Steps 2-3.
      Rooted<BigInt*> quotient(cx);
      if (!BigInt::divmod(cx, nanos, thousand, &quotient, &nanos)) {
        return nullptr;
      }

      micros = BigInt::add(cx, micros, quotient);
      if (!micros) {
        return nullptr;
      }

      // Steps 4-5.
      if (!BigInt::divmod(cx, micros, thousand, &quotient, &micros)) {
        return nullptr;
      }

      millis = BigInt::add(cx, millis, quotient);
      if (!millis) {
        return nullptr;
      }

      // Steps 6-7.
      if (!BigInt::divmod(cx, millis, thousand, &quotient, &millis)) {
        return nullptr;
      }

      totalSecondsBigInt = BigInt::add(cx, secs, quotient);
      if (!totalSecondsBigInt) {
        return nullptr;
      }

      // These are now all in the range [0, 999].
      int64_t milli = BigInt::toInt64(millis);
      int64_t micro = BigInt::toInt64(micros);
      int64_t nano = BigInt::toInt64(nanos);

      MOZ_ASSERT(0 <= milli && milli <= 999);
      MOZ_ASSERT(0 <= micro && micro <= 999);
      MOZ_ASSERT(0 <= nano && nano <= 999);

      // Step 16.a. (Reordered)
      fraction = milli * 1'000'000 + micro * 1'000 + nano;
      MOZ_ASSERT(0 <= fraction && fraction < 1'000'000'000);
    }
  }

  // Steps 8 and 13.
  JSStringBuilder result(cx);

  // Step 1. (Reordered)
  int32_t sign = DurationSign(duration);

  // Steps 17-18. (Reordered)
  if (sign < 0) {
    if (!result.append('-')) {
      return nullptr;
    }
  }

  // Step 18. (Reordered)
  if (!result.append('P')) {
    return nullptr;
  }

  // Step 9.
  if (years != 0) {
    if (!NumberToStringBuilder(cx, years, result)) {
      return nullptr;
    }
    if (!result.append('Y')) {
      return nullptr;
    }
  }

  // Step 10.
  if (months != 0) {
    if (!NumberToStringBuilder(cx, months, result)) {
      return nullptr;
    }
    if (!result.append('M')) {
      return nullptr;
    }
  }

  // Step 11.
  if (weeks != 0) {
    if (!NumberToStringBuilder(cx, weeks, result)) {
      return nullptr;
    }
    if (!result.append('W')) {
      return nullptr;
    }
  }

  // Step 12.
  if (days != 0) {
    if (!NumberToStringBuilder(cx, days, result)) {
      return nullptr;
    }
    if (!result.append('D')) {
      return nullptr;
    }
  }

  // Step 16. (if-condition)
  bool hasSecondsPart = totalSeconds != 0 ||
                        (totalSecondsBigInt && !totalSecondsBigInt->isZero()) ||
                        fraction != 0 ||
                        (years == 0 && months == 0 && weeks == 0 && days == 0 &&
                         hours == 0 && minutes == 0) ||
                        !precision.isAuto();

  if (hours != 0 || minutes != 0 || hasSecondsPart) {
    // Step 19. (Reordered)
    if (!result.append('T')) {
      return nullptr;
    }

    // Step 14.
    if (hours != 0) {
      if (!NumberToStringBuilder(cx, hours, result)) {
        return nullptr;
      }
      if (!result.append('H')) {
        return nullptr;
      }
    }

    // Step 15.
    if (minutes != 0) {
      if (!NumberToStringBuilder(cx, minutes, result)) {
        return nullptr;
      }
      if (!result.append('M')) {
        return nullptr;
      }
    }

    // Step 16.
    if (hasSecondsPart) {
      // Step 16.a. (Moved above)

      // Step 16.f.
      if (totalSecondsBigInt) {
        if (!BigIntToStringBuilder(cx, totalSecondsBigInt, result)) {
          return nullptr;
        }
      } else {
        if (!NumberToStringBuilder(cx, totalSeconds, result)) {
          return nullptr;
        }
      }

      // Steps 16.b-e and 16.g.
      if (precision.isAuto()) {
        if (fraction != 0) {
          // Steps 16.g.
          if (!result.append('.')) {
            return nullptr;
          }

          uint32_t k = 100'000'000;
          do {
            if (!result.append(char('0' + (fraction / k)))) {
              return nullptr;
            }
            fraction %= k;
            k /= 10;
          } while (fraction);
        }
      } else if (precision.value() != 0) {
        // Steps 16.g.
        if (!result.append('.')) {
          return nullptr;
        }

        uint32_t k = 100'000'000;
        for (uint8_t i = 0; i < precision.value(); i++) {
          if (!result.append(char('0' + (fraction / k)))) {
            return nullptr;
          }
          fraction %= k;
          k /= 10;
        }
      }

      // Step 16.h.
      if (!result.append('S')) {
        return nullptr;
      }
    }
  }

  // Step 20.
  return result.finishString();
}

/**
 * ToRelativeTemporalObject ( options )
 */
static bool ToRelativeTemporalObject(JSContext* cx, Handle<JSObject*> options,
                                     MutableHandle<JSObject*> result) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<Value> value(cx);
  if (!GetProperty(cx, options, options, cx->names().relativeTo, &value)) {
    return false;
  }

  // Step 3.
  if (value.isUndefined()) {
    result.set(nullptr);
    return true;
  }

  // Step 4.
  auto offsetBehaviour = OffsetBehaviour::Option;

  // Step 5.
  auto matchBehaviour = MatchBehaviour::MatchExactly;

  // Steps 6-7.
  PlainDateTime dateTime;
  Rooted<JSObject*> calendar(cx);
  Rooted<JSObject*> timeZone(cx);
  int64_t offsetNs;
  if (value.isObject()) {
    Rooted<JSObject*> obj(cx, &value.toObject());

    // Step 6.a.
    if (obj->canUnwrapAs<PlainDateObject>()) {
      result.set(obj);
      return true;
    }
    if (obj->canUnwrapAs<ZonedDateTimeObject>()) {
      result.set(obj);
      return true;
    }

    // Step 6.b.
    if (auto* dateTime = obj->maybeUnwrapIf<PlainDateTimeObject>()) {
      auto plainDateTime = ToPlainDate(dateTime);

      Rooted<JSObject*> calendar(cx, dateTime->calendar());
      if (!cx->compartment()->wrap(cx, &calendar)) {
        return false;
      }

      auto* date = CreateTemporalDate(cx, plainDateTime, calendar);
      if (!date) {
        return false;
      }

      result.set(date);
      return true;
    }

    // Step 6.c.
    calendar = GetTemporalCalendarWithISODefault(cx, obj);
    if (!calendar) {
      return false;
    }

    // Step 6.d.
    JS::RootedVector<PropertyKey> fieldNames(cx);
    if (!CalendarFields(cx, calendar,
                        {CalendarField::Day, CalendarField::Hour,
                         CalendarField::Microsecond, CalendarField::Millisecond,
                         CalendarField::Minute, CalendarField::Month,
                         CalendarField::MonthCode, CalendarField::Nanosecond,
                         CalendarField::Second, CalendarField::Year},
                        &fieldNames)) {
      return false;
    }

    // Steps 6.e-f.
    if (!AppendSorted(cx, fieldNames.get(),
                      {TemporalField::Offset, TemporalField::TimeZone})) {
      return false;
    }

    // Step 6.g.
    Rooted<PlainObject*> fields(cx, PrepareTemporalFields(cx, obj, fieldNames));
    if (!fields) {
      return false;
    }

    // Step 6.h.
    Rooted<JSObject*> dateOptions(cx, NewPlainObjectWithProto(cx, nullptr));
    if (!dateOptions) {
      return false;
    }

    // Step 6.i.
    Rooted<Value> overflow(cx, StringValue(cx->names().constrain));
    if (!DefineDataProperty(cx, dateOptions, cx->names().overflow, overflow)) {
      return false;
    }

    // Step 6.j.
    if (!InterpretTemporalDateTimeFields(cx, calendar, fields, dateOptions,
                                         &dateTime)) {
      return false;
    }

    // Step 6.k.
    Rooted<Value> offset(cx);
    if (!GetProperty(cx, fields, fields, cx->names().offset, &offset)) {
      return false;
    }

    // Step 6.l.
    Rooted<Value> timeZoneValue(cx);
    if (!GetProperty(cx, fields, fields, cx->names().timeZone,
                     &timeZoneValue)) {
      return false;
    }

    // Step 6.m.
    if (!timeZoneValue.isUndefined()) {
      timeZone = ToTemporalTimeZone(cx, timeZoneValue);
      if (!timeZone) {
        return false;
      }
    }

    // Step 6.n.
    if (offset.isUndefined()) {
      offsetBehaviour = OffsetBehaviour::Wall;
    }

    // Steps 9-10.
    if (timeZone) {
      if (offsetBehaviour == OffsetBehaviour::Option) {
        MOZ_ASSERT(!offset.isUndefined());
        MOZ_ASSERT(offset.isString());

        // Step 9.a.
        Rooted<JSString*> offsetString(cx, offset.toString());
        if (!offsetString) {
          return false;
        }

        // Step 9.b.
        if (!ParseTimeZoneOffsetString(cx, offsetString, &offsetNs)) {
          return false;
        }
      } else {
        // Step 10.
        offsetNs = 0;
      }
    }
  } else {
    // Step 7.a.
    Rooted<JSString*> string(cx, JS::ToString(cx, value));
    if (!string) {
      return false;
    }

    // Step 7.b.
    bool isUTC;
    bool hasOffset;
    int64_t timeZoneOffset;
    Rooted<JSString*> timeZoneName(cx);
    Rooted<JSString*> calendarString(cx);
    if (!ParseTemporalRelativeToString(cx, string, &dateTime, &isUTC,
                                       &hasOffset, &timeZoneOffset,
                                       &timeZoneName, &calendarString)) {
      return false;
    }

    // Step 7.c.
    Rooted<Value> calendarValue(cx);
    if (calendarString) {
      calendarValue.setString(calendarString);
    }

    calendar = ToTemporalCalendarWithISODefault(cx, calendarValue);
    if (!calendar) {
      return false;
    }

    // Step 7.d. (Not applicable in our implementation.)

    // Steps 7.e-g.
    if (timeZoneName) {
      timeZone = ToTemporalTimeZone(cx, timeZoneName);
      if (!timeZone) {
        return false;
      }
    } else {
      MOZ_ASSERT(!timeZone);
    }

    // Steps 7.h-i.
    if (isUTC) {
      offsetBehaviour = OffsetBehaviour::Exact;
    } else if (!hasOffset) {
      offsetBehaviour = OffsetBehaviour::Wall;
    }

    // Step 7.j.
    matchBehaviour = MatchBehaviour::MatchMinutes;

    // Steps 9-10.
    if (timeZone) {
      if (offsetBehaviour == OffsetBehaviour::Option) {
        MOZ_ASSERT(hasOffset);

        // Steps 9.a-b.
        offsetNs = timeZoneOffset;
      } else {
        // Step 10.
        offsetNs = 0;
      }
    }
  }

  // Step 8.
  if (!timeZone) {
    auto* obj = CreateTemporalDate(cx, dateTime.date, calendar);
    if (!obj) {
      return false;
    }

    result.set(obj);
    return true;
  }

  // Steps 9-10. (Moved above)

  // Step 11.
  Instant epochNanoseconds;
  if (!InterpretISODateTimeOffset(cx, dateTime, offsetBehaviour, offsetNs,
                                  timeZone, TemporalDisambiguation::Compatible,
                                  TemporalOffset::Reject, matchBehaviour,
                                  &epochNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(epochNanoseconds));

  // Step 12.
  auto* obj =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!obj) {
    return false;
  }

  result.set(obj);
  return true;
}

static constexpr bool IsSafeInteger(int64_t x) {
  constexpr int64_t MaxSafeInteger = int64_t(1) << 53;
  constexpr int64_t MinSafeInteger = -MaxSafeInteger;
  return MinSafeInteger < x && x < MaxSafeInteger;
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static void TruncateNumber(int64_t numerator, int64_t denominator,
                           double* quotient, double* rounded) {
  // Computes the quotient and rounded value of the rational number
  // |numerator / denominator|.
  //
  // The numerator can be represented as |numerator = a * denominator + b|.
  //
  // So we have:
  //
  //   numerator / denominator
  // = (a * denominator + b) / denominator
  // = ((a * denominator) / denominator) + (b / denominator)
  // = a + (b / denominator)
  //
  // where |quotient = a| and |remainder = b / denominator|. |a| and |b| can be
  // computed through normal int64 division.

  // Int64 division truncates.
  int64_t q = numerator / denominator;
  int64_t r = numerator % denominator;

  // The remainder is stored as a mathematical number in the draft proposal, so
  // we can't convert it to a double without loss of precision. The remainder is
  // eventually added to the quotient and if we directly perform this addition,
  // we can reduce the possible loss of precision. We still need to choose which
  // approach to take based on the input range.
  //
  // For example:
  //
  // When |numerator = 1000001| and |denominator = 60 * 1000|, then
  // |quotient = 16| and |remainder = 40001 / (60 * 1000)|. The exact result is
  // |16.66668333...|.
  //
  // When storing the remainder as a double and later adding it to the quotient,
  // we get |𝔽(16) + 𝔽(40001 / (60 * 1000)) = 16.666683333333331518...𝔽|. This
  // is wrong by 1 ULP, a better approximation is |16.666683333333335070...𝔽|.
  //
  // We can get the better approximation when casting the numerator and
  // denominator to doubles and then performing a double division.
  //
  // When |numerator = 14400000000000001| and |denominator = 3600000000000|, we
  // can't use double division, because |14400000000000001| can't be represented
  // as an exact double value. The exact result is |4000.0000000000002777...|.
  //
  // The best possible approximation is |4000.0000000000004547...𝔽|, which can
  // be computed through |q + r / denominator|.
  if (::IsSafeInteger(numerator) && ::IsSafeInteger(denominator)) {
    *quotient = double(q);
    *rounded = double(numerator) / double(denominator);
  } else {
    *quotient = double(q);
    *rounded = double(q) + double(r) / double(denominator);
  }
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static bool TruncateNumber(JSContext* cx, const Duration& toRound,
                           TemporalUnit unit, double* quotient,
                           double* rounded) {
  MOZ_ASSERT(unit >= TemporalUnit::Day);

  int64_t denominator = ToNanoseconds(unit);
  MOZ_ASSERT(denominator > 0);
  MOZ_ASSERT(denominator <= 86'400'000'000'000);

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto numerator = TotalDurationNanoseconds(toRound, 0)) {
    TruncateNumber(*numerator, denominator, quotient, rounded);
    return true;
  }

  Rooted<BigInt*> numerator(cx, TotalDurationNanosecondsSlow(cx, toRound, 0));
  if (!numerator) {
    return false;
  }

  // Division by one has no remainder.
  if (denominator == 1) {
    double q = BigInt::numberValue(numerator);
    *quotient = q;
    *rounded = q;
    return true;
  }

  Rooted<BigInt*> denom(cx, BigInt::createFromInt64(cx, denominator));
  if (!denom) {
    return false;
  }

  // BigInt division truncates.
  Rooted<BigInt*> quot(cx);
  Rooted<BigInt*> rem(cx);
  if (!BigInt::divmod(cx, numerator, denom, &quot, &rem)) {
    return false;
  }

  double q = BigInt::numberValue(quot);
  *quotient = q;
  *rounded = q + BigInt::numberValue(rem) / double(denominator);
  return true;
}

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static bool RoundNumberToIncrement(JSContext* cx, const Duration& toRound,
                                   TemporalUnit unit, Increment increment,
                                   TemporalRoundingMode roundingMode,
                                   double* result) {
  MOZ_ASSERT(unit >= TemporalUnit::Day);

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto total = TotalDurationNanoseconds(toRound, 0)) {
    return RoundNumberToIncrement(cx, *total, unit, increment, roundingMode,
                                  result);
  }

  Rooted<BigInt*> totalNs(cx, TotalDurationNanosecondsSlow(cx, toRound, 0));
  if (!totalNs) {
    return false;
  }

  return RoundNumberToIncrement(cx, totalNs, unit, increment, roundingMode,
                                result);
}

struct RoundedDuration final {
  Duration duration;
  double rounded = 0;
};

enum class ComputeRemainder : bool { No, Yes };

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
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

  auto [years, months, weeks, days, hours, minutes, seconds, milliseconds,
        microseconds, nanoseconds] = duration;

  // Step 1. (Not applicable.)

  // Step 2.
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
  // the following BalanceDuration call.

  // Steps 3-5. (Not applicable.)

  // Steps 6-7 (Moved below).

  // Step 8. (Not applicable.)

  // Steps 9-18.
  Duration toRound;
  double* roundedTime;
  switch (unit) {
    case TemporalUnit::Auto:
    case TemporalUnit::Year:
    case TemporalUnit::Week:
    case TemporalUnit::Month:
      // Steps 9-11. (Not applicable.)
      MOZ_CRASH("Unexpected temporal unit");

    case TemporalUnit::Day: {
      // clang-format off
      //
      // Relevant steps from the spec algorithm:
      //
      // 6.a Let nanoseconds be ! TotalDurationNanoseconds(0, hours, minutes, seconds, milliseconds, microseconds, nanoseconds, 0).
      // 6.d Let result be ? NanosecondsToDays(nanoseconds, intermediate).
      // 6.e Set days to days + result.[[Days]] + result.[[Nanoseconds]] / abs(result.[[DayLength]]).
      // ...
      // 12.a Let fractionalDays be days.
      // 12.b Set days to ? RoundNumberToIncrement(days, increment, roundingMode).
      // 12.c Set remainder to fractionalDays - days.
      //
      // Where `result.[[Days]]` is `the integral part of nanoseconds / dayLengthNs`
      // and `result.[[Nanoseconds]]` is `nanoseconds modulo dayLengthNs`.
      // With `dayLengthNs = 8.64 × 10^13`.
      //
      // So we have:
      //   d + r.days + (r.nanoseconds / len)
      // = d + [ns / len] + ((ns % len) / len)
      // = d + [ns / len] + ((ns - ([ns / len] × len)) / len)
      // = d + [ns / len] + (ns / len) - (([ns / len] × len) / len)
      // = d + [ns / len] + (ns / len) - [ns / len]
      // = d + (ns / len)
      // = ((d × len) / len) + (ns / len)
      // = ((d × len) + ns) / len
      //
      // `((d × len) + ns)` is the result of calling TotalDurationNanoseconds(),
      // which means we can use the same code for all time computations in this
      // function.
      //
      // clang-format on

      MOZ_ASSERT(increment <= Increment{1'000'000'000},
                 "limited by ToTemporalRoundingIncrement");

      // Steps 6.a, 6.d-e, and 12.a-c.
      toRound = duration;
      roundedTime = &days;

      // Steps 6.b-c. (Not applicable)

      // Step 6.f.
      hours = 0;
      minutes = 0;
      seconds = 0;
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Hour: {
      MOZ_ASSERT(increment <= Increment{24},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 7 and 13.a-c.
      toRound = {
          0,
          0,
          0,
          0,
          hours,
          minutes,
          seconds,
          milliseconds,
          microseconds,
          nanoseconds,
      };
      roundedTime = &hours;

      // Step 13.d.
      minutes = 0;
      seconds = 0;
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Minute: {
      MOZ_ASSERT(increment <= Increment{60},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 7 and 14.a-c.
      toRound = {
          0,           0, 0, 0, 0, minutes, seconds, milliseconds, microseconds,
          nanoseconds,
      };
      roundedTime = &minutes;

      // Step 14.d.
      seconds = 0;
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Second: {
      MOZ_ASSERT(increment <= Increment{60},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 7 and 15.a-b.
      toRound = {
          0, 0, 0, 0, 0, 0, seconds, milliseconds, microseconds, nanoseconds,
      };
      roundedTime = &seconds;

      // Step 15.c.
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Millisecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 16.a-c.
      toRound = {0, 0, 0, 0, 0, 0, 0, milliseconds, microseconds, nanoseconds};
      roundedTime = &milliseconds;

      // Step 16.d.
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Microsecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 17.a-c.
      toRound = {0, 0, 0, 0, 0, 0, 0, 0, microseconds, nanoseconds};
      roundedTime = &microseconds;

      // Step 17.d.
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Nanosecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Step 18.a. (Implicit)

      // Steps 18.b-d.
      toRound = {0, 0, 0, 0, 0, 0, 0, 0, 0, nanoseconds};
      roundedTime = &nanoseconds;
      break;
    }
  }

  // clang-format off
  //
  // The specification uses mathematical values in its computations, which
  // requires to be able to represent decimals with arbitrary precision. To
  // avoid having to struggle with decimals, we can transform the steps to work
  // on integer values, which we can conveniently represent with BigInts.
  //
  // As an example here are the transformation steps for "hours", but all other
  // units can be handled similarly.
  //
  // Relevant spec steps:
  //
  // 7.a Let fractionalSeconds be nanoseconds × 10^9 + microseconds × 10^6 + milliseconds × 10^3 + seconds.
  // ...
  // 13.a Let fractionalHours be (fractionalSeconds / 60 + minutes) / 60 + hours.
  // 13.b Set hours to ? RoundNumberToIncrement(fractionalHours, increment, roundingMode).
  //
  // And from RoundNumberToIncrement:
  //
  // 1. Let quotient be x / increment.
  // 2-7. Let rounded be op(quotient).
  // 8. Return rounded × increment.
  //
  // With `fractionalHours = (totalNs / nsPerHour)`, the rounding operation
  // computes:
  //
  //   op(fractionalHours / increment) × increment
  // = op((totalNs / nsPerHour) / increment) × increment
  // = op(totalNs / (nsPerHour × increment)) × increment
  //
  // So when we pass `totalNs` and `nsPerHour` as separate arguments to
  // RoundNumberToIncrement, we can avoid any precision losses and instead
  // compute with exact values.
  //
  // clang-format on

  double rounded = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!RoundNumberToIncrement(cx, toRound, unit, increment, roundingMode,
                                roundedTime)) {
      return false;
    }
  } else {
    // clang-format off
    //
    // The remainder is only used for Duration.prototype.total(), which calls
    // this operation with increment=1 and roundingMode=trunc.
    //
    // That means the remainder computation is actually just
    // `(totalNs % toNanos) / toNanos`, where `totalNs % toNanos` is already
    // computed in RoundNumberToIncrement():
    //
    // rounded = trunc(totalNs / toNanos)
    //         = [totalNs / toNanos]
    //
    // roundedTime = ℝ(𝔽(rounded))
    //
    // remainder = (totalNs - (rounded * toNanos)) / toNanos
    //           = (totalNs - ([totalNs / toNanos] * toNanos)) / toNanos
    //           = (totalNs % toNanos) / toNanos
    //
    // When used in Duration.prototype.total(), the overall computed value is
    // `[totalNs / toNanos] + (totalNs % toNanos) / toNanos`.
    //
    // Applying normal math rules would allow to simplify this to:
    //
    //   [totalNs / toNanos] + (totalNs % toNanos) / toNanos
    // = [totalNs / toNanos] + (totalNs - [totalNs / toNanos] * toNanos) / toNanos
    // = total / toNanos
    //
    // We can't apply this simplification because it'd introduce double
    // precision issues. Instead of that, we use a specialized version of
    // RoundNumberToIncrement which directly returns the remainder. The
    // remainder `(totalNs % toNanos) / toNanos` is a value near zero, so this
    // approach is as exact as possible. (Double numbers near zero can be
    // computed more precisely than large numbers with fractional parts.)
    //
    // clang-format on

    MOZ_ASSERT(increment == Increment{1});
    MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

    if (!TruncateNumber(cx, toRound, unit, roundedTime, &rounded)) {
      return false;
    }
  }

  MOZ_ASSERT(years == duration.years);
  MOZ_ASSERT(months == duration.months);
  MOZ_ASSERT(weeks == duration.weeks);

  // Step 19.
  MOZ_ASSERT(IsIntegerOrInfinity(days));

  // Step 20.
  Duration resultDuration = {years,        months,     weeks,   days,
                             hours,        minutes,    seconds, milliseconds,
                             microseconds, nanoseconds};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 21.
  *result = {resultDuration, rounded};
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode, Duration* result) {
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
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
bool js::temporal::RoundDuration(JSContext* cx, const Duration& duration,
                                 Increment increment, TemporalUnit unit,
                                 TemporalRoundingMode roundingMode,
                                 Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::RoundDuration(cx, duration, increment, unit, roundingMode, result);
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

  Rooted<JSObject*> relativeTo(cx);
  if (args.hasDefined(2)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "compare", args[2]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!ToRelativeTemporalObject(cx, options, &relativeTo)) {
      return false;
    }
  }

  // Step 5.
  int64_t shift1;
  if (!CalculateOffsetShift(cx, relativeTo, one.date(), &shift1)) {
    return false;
  }

  // Step 6.
  int64_t shift2;
  if (!CalculateOffsetShift(cx, relativeTo, two.date(), &shift2)) {
    return false;
  }

  // Steps 7-8.
  double days1, days2;
  if (one.years != 0 || one.months != 0 || one.weeks != 0 || two.years != 0 ||
      two.months != 0 || two.weeks != 0) {
    // Step 7.a.
    DateDuration unbalanceResult1;
    if (relativeTo) {
      if (!UnbalanceDurationRelative(cx, one, TemporalUnit::Day, relativeTo,
                                     &unbalanceResult1)) {
        return false;
      }
    } else {
      if (!UnbalanceDurationRelative(cx, one, TemporalUnit::Day,
                                     &unbalanceResult1)) {
        return false;
      }
      MOZ_ASSERT(one.date() == unbalanceResult1.toDuration());
    }

    // Step 7.b.
    DateDuration unbalanceResult2;
    if (relativeTo) {
      if (!UnbalanceDurationRelative(cx, two, TemporalUnit::Day, relativeTo,
                                     &unbalanceResult2)) {
        return false;
      }
    } else {
      if (!UnbalanceDurationRelative(cx, two, TemporalUnit::Day,
                                     &unbalanceResult2)) {
        return false;
      }
      MOZ_ASSERT(two.date() == unbalanceResult2.toDuration());
    }

    // Step 7.c.
    days1 = unbalanceResult1.days;

    // Step 7.d.
    days2 = unbalanceResult2.days;
  } else {
    // Step 8.a.
    days1 = one.days;

    // Step 8.b.
    days2 = two.days;
  }

  // Note: duration units can be arbitrary doubles, so we need to use BigInts
  // Test case:
  //
  // Temporal.Duration.compare({
  //   milliseconds: 10000000000000, microseconds: 4, nanoseconds: 95
  // }, {
  //   nanoseconds:10000000000000004000
  // })
  //
  // This must return -1, but would return 0 when |double| is used.
  //
  // Note: BigInt(10000000000000004000) is 10000000000000004096n

  Duration oneTotal = {
      0,
      0,
      0,
      days1,
      one.hours,
      one.minutes,
      one.seconds,
      one.milliseconds,
      one.microseconds,
      one.nanoseconds,
  };
  Duration twoTotal = {
      0,
      0,
      0,
      days2,
      two.hours,
      two.minutes,
      two.seconds,
      two.milliseconds,
      two.microseconds,
      two.nanoseconds,
  };

  // Steps 9-13.
  //
  // Fast path when the total duration amount fits into an int64.
  if (auto ns1 = TotalDurationNanoseconds(oneTotal, shift1)) {
    if (auto ns2 = TotalDurationNanoseconds(twoTotal, shift2)) {
      args.rval().setInt32(*ns1 < *ns2 ? -1 : *ns1 > *ns2 ? 1 : 0);
      return true;
    }
  }

  // Step 9.
  Rooted<BigInt*> ns1(cx, TotalDurationNanosecondsSlow(cx, oneTotal, shift1));
  if (!ns1) {
    return false;
  }

  // Step 10.
  auto* ns2 = TotalDurationNanosecondsSlow(cx, twoTotal, shift2);
  if (!ns2) {
    return false;
  }

  // Step 11-13.
  args.rval().setInt32(BigInt::compare(ns1, ns2));
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
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  int32_t sign = DurationSign(ToDuration(duration));
  args.rval().setInt32(sign);
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
  // Step 3.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  int32_t sign = DurationSign(ToDuration(duration));

  // Steps 4-5.
  args.rval().setBoolean(sign == 0);
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
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();

  // Absent values default to the corresponding values of |this| object.
  auto duration = ToDuration(durationObj);

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
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();
  auto duration = ToDuration(durationObj);

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
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();
  auto duration = ToDuration(durationObj);

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
 * Temporal.Duration.prototype.toString ( [ options ] )
 */
static bool Duration_toString(JSContext* cx, const CallArgs& args) {
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

  // Step 10.
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  Duration rounded;
  if (!temporal::RoundDuration(cx, ToDuration(duration), precision.increment,
                               precision.unit, roundingMode, &rounded)) {
    return false;
  }

  // Step 11.
  JSString* str = TemporalDurationToString(cx, rounded, precision.precision);
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
 *  Temporal.Duration.prototype.toJSON ( )
 */
static bool Duration_toJSON(JSContext* cx, const CallArgs& args) {
  auto* duration = &args.thisv().toObject().as<DurationObject>();

  // Step 3.
  JSString* str =
      TemporalDurationToString(cx, ToDuration(duration), Precision::Auto());
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 *  Temporal.Duration.prototype.toJSON ( )
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
  auto* duration = &args.thisv().toObject().as<DurationObject>();

  // Step 3.
  JSString* str =
      TemporalDurationToString(cx, ToDuration(duration), Precision::Auto());
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
