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
  Rooted<TimeZoneValue> timeZone(cx, zonedRelativeTo->timeZone());
  Rooted<CalendarValue> calendar(cx, zonedRelativeTo->calendar());

  // Wrap into the current compartment.
  if (!timeZone.wrap(cx)) {
    return false;
  }
  if (!calendar.wrap(cx)) {
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
    JSContext* cx, Handle<CalendarValue> calendar,
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
 * MoveRelativeZonedDateTime ( zonedDateTime, years, months, weeks, days )
 */
static ZonedDateTimeObject* MoveRelativeZonedDateTime(
    JSContext* cx, Handle<Wrapped<ZonedDateTimeObject*>> zonedDateTime,
    const Duration& duration) {
  auto* unwrappedZonedDateTime = zonedDateTime.unwrap(cx);
  if (!unwrappedZonedDateTime) {
    return nullptr;
  }
  auto instant = ToInstant(unwrappedZonedDateTime);
  Rooted<TimeZoneValue> timeZone(cx, unwrappedZonedDateTime->timeZone());
  Rooted<CalendarValue> calendar(cx, unwrappedZonedDateTime->calendar());

  if (!timeZone.wrap(cx)) {
    return nullptr;
  }
  if (!calendar.wrap(cx)) {
    return nullptr;
  }

  // Step 1.
  Instant intermediateNs;
  if (!AddZonedDateTime(cx, instant, timeZone, calendar, duration.date(),
                        &intermediateNs)) {
    return nullptr;
  }
  MOZ_ASSERT(IsValidEpochInstant(intermediateNs));

  // Step 2.
  return CreateTemporalZonedDateTime(cx, intermediateNs, timeZone, calendar);
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
 * Split duration into full days and remainding nanoseconds.
 */
static ::NanosecondsAndDays NanosecondsToDays(int64_t nanoseconds) {
  constexpr int64_t dayLengthNs = ToNanoseconds(TemporalUnit::Day);

  static_assert(INT64_MAX / dayLengthNs <= INT32_MAX,
                "days doesn't exceed INT32_MAX");

  return {int32_t(nanoseconds / dayLengthNs), nanoseconds % dayLengthNs};
}

/**
 * Split duration into full days and remainding nanoseconds.
 */
static bool NanosecondsToDaysSlow(
    JSContext* cx, Handle<BigInt*> nanoseconds,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  constexpr int64_t dayLengthNs = ToNanoseconds(TemporalUnit::Day);

  Rooted<BigInt*> dayLength(cx, BigInt::createFromInt64(cx, dayLengthNs));
  if (!dayLength) {
    return false;
  }

  Rooted<BigInt*> days(cx);
  Rooted<BigInt*> nanos(cx);
  if (!BigInt::divmod(cx, nanoseconds, dayLength, &days, &nanos)) {
    return false;
  }

  result.initialize(days, ToInstantSpan(nanos),
                    InstantSpan::fromNanoseconds(dayLengthNs));
  return true;
}

/**
 * Split duration into full days and remainding nanoseconds.
 */
static bool NanosecondsToDays(
    JSContext* cx, const Duration& duration,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  if (auto total = TotalDurationNanoseconds(duration.time(), 0)) {
    auto nanosAndDays = ::NanosecondsToDays(*total);

    result.initialize(
        nanosAndDays.days,
        InstantSpan::fromNanoseconds(nanosAndDays.nanoseconds),
        InstantSpan::fromNanoseconds(ToNanoseconds(TemporalUnit::Day)));
    return true;
  }

  Rooted<BigInt*> nanoseconds(
      cx, TotalDurationNanosecondsSlow(cx, duration.time(), 0));
  if (!nanoseconds) {
    return false;
  }

  return ::NanosecondsToDaysSlow(cx, nanoseconds, result);
}

/**
 * NanosecondsToDays ( nanoseconds, zonedRelativeTo )
 */
static bool NanosecondsToDaysError(
    JSContext* cx, Handle<ZonedDateTimeObject*> zonedRelativeTo) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  auto startNs = ToInstant(zonedRelativeTo);
  Rooted<TimeZoneValue> timeZone(cx, zonedRelativeTo->timeZone());

  // FIXME: spec issue - consider moving GetPlainDateTimeFor after step 9 where
  // IsValidEpochNanoseconds is checked. That way we reduce extra observable
  // behaviour.
  // https://github.com/tc39/proposal-temporal/issues/2529

  // Steps 4-5. (Executed just for possible side-effects.)
  PlainDateTime startDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, startNs, &startDateTime)) {
    return false;
  }

  // Step 6 is |startNs + nanoseconds|, but when |nanoseconds| is too large the
  // result isn't a valid epoch nanoseconds value and step 7 throws.

  // Step 7.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_INSTANT_INVALID);
  return false;
}

/**
 * NanosecondsToDays ( nanoseconds, zonedRelativeTo )
 */
static bool NanosecondsToDays(
    JSContext* cx, const Duration& duration,
    Handle<ZonedDateTimeObject*> zonedRelativeTo,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  if (auto total = TotalDurationNanoseconds(duration.time(), 0)) {
    auto nanoseconds = InstantSpan::fromNanoseconds(*total);
    MOZ_ASSERT(IsValidInstantSpan(nanoseconds));

    return NanosecondsToDays(cx, nanoseconds, zonedRelativeTo, result);
  }

  auto* nanoseconds = TotalDurationNanosecondsSlow(cx, duration.time(), 0);
  if (!nanoseconds) {
    return false;
  }

  if (!IsValidInstantSpan(nanoseconds)) {
    return NanosecondsToDaysError(cx, zonedRelativeTo);
  }
  return NanosecondsToDays(cx, ToInstantSpan(nanoseconds), zonedRelativeTo,
                           result);
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
  MOZ_ASSERT(IsValidDuration({0, 0, 0, double(days), double(hours),
                              double(minutes), double(seconds),
                              double(microseconds), double(nanoseconds)}));

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
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit )
 *
 * BalancePossiblyInfiniteTimeDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit )
 */
static TimeDuration BalanceTimeDuration(int64_t nanoseconds,
                                        TemporalUnit largestUnit) {
  // Step 1. (Handled in caller.)

  // Step 2.
  int64_t days = 0;
  int64_t hours = 0;
  int64_t minutes = 0;
  int64_t seconds = 0;
  int64_t milliseconds = 0;
  int64_t microseconds = 0;

  // Steps 3-4. (Not applicable in our implementation.)
  //
  // We don't need to convert to positive numbers, because integer division
  // truncates and the %-operator has modulo semantics.

  // Steps 5-11.
  switch (largestUnit) {
    // Step 5.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day: {
      // Step 5.a.
      microseconds = nanoseconds / 1000;

      // Step 5.b.
      nanoseconds = nanoseconds % 1000;

      // Step 5.c.
      milliseconds = microseconds / 1000;

      // Step 5.d.
      microseconds = microseconds % 1000;

      // Step 5.e.
      seconds = milliseconds / 1000;

      // Step 5.f.
      milliseconds = milliseconds % 1000;

      // Step 5.g.
      minutes = seconds / 60;

      // Step 5.h.
      seconds = seconds % 60;

      // Step 5.i.
      hours = minutes / 60;

      // Step 5.j.
      minutes = minutes % 60;

      // Step 5.k.
      days = hours / 24;

      // Step 5.l.
      hours = hours % 24;

      break;
    }

    case TemporalUnit::Hour: {
      // Step 6.a.
      microseconds = nanoseconds / 1000;

      // Step 6.b.
      nanoseconds = nanoseconds % 1000;

      // Step 6.c.
      milliseconds = microseconds / 1000;

      // Step 6.d.
      microseconds = microseconds % 1000;

      // Step 6.e.
      seconds = milliseconds / 1000;

      // Step 6.f.
      milliseconds = milliseconds % 1000;

      // Step 6.g.
      minutes = seconds / 60;

      // Step 6.h.
      seconds = seconds % 60;

      // Step 6.i.
      hours = minutes / 60;

      // Step 6.j.
      minutes = minutes % 60;

      break;
    }

    // Step 7.
    case TemporalUnit::Minute: {
      // Step 7.a.
      microseconds = nanoseconds / 1000;

      // Step 7.b.
      nanoseconds = nanoseconds % 1000;

      // Step 7.c.
      milliseconds = microseconds / 1000;

      // Step 7.d.
      microseconds = microseconds % 1000;

      // Step 7.e.
      seconds = milliseconds / 1000;

      // Step 7.f.
      milliseconds = milliseconds % 1000;

      // Step 7.g.
      minutes = seconds / 60;

      // Step 7.h.
      seconds = seconds % 60;

      break;
    }

    // Step 8.
    case TemporalUnit::Second: {
      // Step 8.a.
      microseconds = nanoseconds / 1000;

      // Step 8.b.
      nanoseconds = nanoseconds % 1000;

      // Step 8.c.
      milliseconds = microseconds / 1000;

      // Step 8.d.
      microseconds = microseconds % 1000;

      // Step 8.e.
      seconds = milliseconds / 1000;

      // Step 8.f.
      milliseconds = milliseconds % 1000;

      break;
    }

    // Step 9.
    case TemporalUnit::Millisecond: {
      // Step 9.a.
      microseconds = nanoseconds / 1000;

      // Step 9.b.
      nanoseconds = nanoseconds % 1000;

      // Step 9.c.
      milliseconds = microseconds / 1000;

      // Step 9.d.
      microseconds = microseconds % 1000;

      break;
    }

    // Step 10.
    case TemporalUnit::Microsecond: {
      // Step 10.a.
      microseconds = nanoseconds / 1000;

      // Step 10.b.
      nanoseconds = nanoseconds % 1000;

      break;
    }

    // Step 11.
    case TemporalUnit::Nanosecond: {
      // Nothing to do.
      break;
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 12. (Not applicable, all values are finite)

  // Step 13.
  return CreateTimeDurationRecord(days, hours, minutes, seconds, milliseconds,
                                  microseconds, nanoseconds);
}

/**
 * BalancePossiblyInfiniteTimeDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit )
 */
static bool BalancePossiblyInfiniteTimeDurationSlow(JSContext* cx,
                                                    Handle<BigInt*> nanos,
                                                    TemporalUnit largestUnit,
                                                    TimeDuration* result) {
  // Step 1. (Handled in caller.)

  BigInt* zero = BigInt::zero(cx);
  if (!zero) {
    return false;
  }

  // Step 2.
  Rooted<BigInt*> days(cx, zero);
  Rooted<BigInt*> hours(cx, zero);
  Rooted<BigInt*> minutes(cx, zero);
  Rooted<BigInt*> seconds(cx, zero);
  Rooted<BigInt*> milliseconds(cx, zero);
  Rooted<BigInt*> microseconds(cx, zero);
  Rooted<BigInt*> nanoseconds(cx, nanos);

  // Steps 3-4.
  //
  // We don't need to convert to positive numbers, because BigInt division
  // truncates and BigInt modulo has modulo semantics.

  // Steps 5-11.
  Rooted<BigInt*> thousand(cx, BigInt::createFromInt64(cx, 1000));
  if (!thousand) {
    return false;
  }

  Rooted<BigInt*> sixty(cx, BigInt::createFromInt64(cx, 60));
  if (!sixty) {
    return false;
  }

  Rooted<BigInt*> twentyfour(cx, BigInt::createFromInt64(cx, 24));
  if (!twentyfour) {
    return false;
  }

  switch (largestUnit) {
    // Step 5.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day: {
      // Steps 5.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 5.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 5.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      // Steps 5.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      // Steps 5.i-j.
      if (!BigInt::divmod(cx, minutes, sixty, &hours, &minutes)) {
        return false;
      }

      // Steps 5.k-l.
      if (!BigInt::divmod(cx, hours, twentyfour, &days, &hours)) {
        return false;
      }

      break;
    }

    // Step 6.
    case TemporalUnit::Hour: {
      // Steps 6.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 6.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 6.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      // Steps 6.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      // Steps 6.i-j.
      if (!BigInt::divmod(cx, minutes, sixty, &hours, &minutes)) {
        return false;
      }

      break;
    }

    // Step 7.
    case TemporalUnit::Minute: {
      // Steps 7.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 7.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 7.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      // Steps 7.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      break;
    }

    // Step 8.
    case TemporalUnit::Second: {
      // Steps 8.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      // Steps 8.c-d.
      if (!BigInt::divmod(cx, microseconds, thousand, &milliseconds,
                          &microseconds)) {
        return false;
      }

      // Steps 8.e-f.
      if (!BigInt::divmod(cx, milliseconds, thousand, &seconds,
                          &milliseconds)) {
        return false;
      }

      break;
    }

    // Step 9.
    case TemporalUnit::Millisecond: {
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

      break;
    }

    // Step 10.
    case TemporalUnit::Microsecond: {
      // Steps 10.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      break;
    }

    // Step 11.
    case TemporalUnit::Nanosecond: {
      // Nothing to do.
      break;
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Steps 12-13.
  return CreateTimeDurationRecordPossiblyInfinite(
      cx, BigInt::numberValue(days), BigInt::numberValue(hours),
      BigInt::numberValue(minutes), BigInt::numberValue(seconds),
      BigInt::numberValue(milliseconds), BigInt::numberValue(microseconds),
      BigInt::numberValue(nanoseconds), result);
}

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit )
 */
static bool BalanceTimeDurationSlow(JSContext* cx, Handle<BigInt*> nanoseconds,
                                    TemporalUnit largestUnit,
                                    TimeDuration* result) {
  // Step 1.
  if (!BalancePossiblyInfiniteTimeDurationSlow(cx, nanoseconds, largestUnit,
                                               result)) {
    return false;
  }

  // Steps 2-3.
  return ThrowIfInvalidDuration(cx, result->toDuration());
}

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit )
 */
static bool BalanceTimeDuration(JSContext* cx, const Duration& one,
                                const Duration& two, TemporalUnit largestUnit,
                                TimeDuration* result) {
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));
  MOZ_ASSERT(largestUnit >= TemporalUnit::Day);

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto oneNanoseconds = TotalDurationNanoseconds(one, 0)) {
    if (auto twoNanoseconds = TotalDurationNanoseconds(two, 0)) {
      mozilla::CheckedInt64 nanoseconds = *oneNanoseconds;
      nanoseconds += *twoNanoseconds;
      if (nanoseconds.isValid()) {
        *result = ::BalanceTimeDuration(nanoseconds.value(), largestUnit);
        return true;
      }
    }
  }

  Rooted<BigInt*> oneNanoseconds(cx, TotalDurationNanosecondsSlow(cx, one, 0));
  if (!oneNanoseconds) {
    return false;
  }

  Rooted<BigInt*> twoNanoseconds(cx, TotalDurationNanosecondsSlow(cx, two, 0));
  if (!twoNanoseconds) {
    return false;
  }

  Rooted<BigInt*> nanoseconds(cx,
                              BigInt::add(cx, oneNanoseconds, twoNanoseconds));
  if (!nanoseconds) {
    return false;
  }

  return BalanceTimeDurationSlow(cx, nanoseconds, largestUnit, result);
}

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit )
 */
static bool BalanceTimeDuration(JSContext* cx, double days, const Duration& one,
                                const Duration& two, TemporalUnit largestUnit,
                                TimeDuration* result) {
  MOZ_ASSERT(IsInteger(days));
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto oneNanoseconds = TotalDurationNanoseconds(one, 0)) {
    if (auto twoNanoseconds = TotalDurationNanoseconds(two, 0)) {
      int64_t intDays;
      if (mozilla::NumberEqualsInt64(days, &intDays)) {
        mozilla::CheckedInt64 daysNanoseconds = intDays;
        daysNanoseconds *= ToNanoseconds(TemporalUnit::Day);

        mozilla::CheckedInt64 nanoseconds = *oneNanoseconds;
        nanoseconds += *twoNanoseconds;
        nanoseconds += daysNanoseconds;

        if (nanoseconds.isValid()) {
          *result = ::BalanceTimeDuration(nanoseconds.value(), largestUnit);
          return true;
        }
      }
    }
  }

  Rooted<BigInt*> oneNanoseconds(cx, TotalDurationNanosecondsSlow(cx, one, 0));
  if (!oneNanoseconds) {
    return false;
  }

  Rooted<BigInt*> twoNanoseconds(cx, TotalDurationNanosecondsSlow(cx, two, 0));
  if (!twoNanoseconds) {
    return false;
  }

  Rooted<BigInt*> nanoseconds(cx,
                              BigInt::add(cx, oneNanoseconds, twoNanoseconds));
  if (!nanoseconds) {
    return false;
  }

  if (days) {
    Rooted<BigInt*> daysNanoseconds(
        cx, TotalDurationNanosecondsSlow(cx, {0, 0, 0, days}, 0));
    if (!daysNanoseconds) {
      return false;
    }

    nanoseconds = BigInt::add(cx, nanoseconds, daysNanoseconds);
    if (!nanoseconds) {
      return false;
    }
  }

  return BalanceTimeDurationSlow(cx, nanoseconds, largestUnit, result);
}

/**
 * BalancePossiblyInfiniteTimeDuration ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit )
 */
static bool BalancePossiblyInfiniteTimeDuration(JSContext* cx,
                                                const Duration& duration,
                                                TemporalUnit largestUnit,
                                                TimeDuration* result) {
  // NB: |duration.days| can have a different sign than the time components.
  MOZ_ASSERT(IsValidDuration(duration.time()));

  // Steps 1-2. (Not applicable)

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto nanoseconds = TotalDurationNanoseconds(duration, 0)) {
    *result = ::BalanceTimeDuration(*nanoseconds, largestUnit);
    return true;
  }

  // Step 3.
  Rooted<BigInt*> nanoseconds(cx,
                              TotalDurationNanosecondsSlow(cx, duration, 0));
  if (!nanoseconds) {
    return false;
  }

  // Steps 4-16.
  return ::BalancePossiblyInfiniteTimeDurationSlow(cx, nanoseconds, largestUnit,
                                                   result);
}

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit )
 */
bool js::temporal::BalanceTimeDuration(JSContext* cx, const Duration& duration,
                                       TemporalUnit largestUnit,
                                       TimeDuration* result) {
  if (!::BalancePossiblyInfiniteTimeDuration(cx, duration, largestUnit,
                                             result)) {
    return false;
  }
  return ThrowIfInvalidDuration(cx, result->toDuration());
}

/**
 * BalancePossiblyInfiniteTimeDurationRelative ( days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, largestUnit, zonedRelativeTo )
 */
static bool BalancePossiblyInfiniteTimeDurationRelative(
    JSContext* cx, const Duration& duration, TemporalUnit largestUnit,
    Handle<Wrapped<ZonedDateTimeObject*>> relativeTo, TimeDuration* result) {
  // Step 1.
  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto epochInstant = ToInstant(unwrappedRelativeTo);
  Rooted<TimeZoneValue> timeZone(cx, unwrappedRelativeTo->timeZone());
  Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());

  if (!timeZone.wrap(cx)) {
    return false;
  }
  if (!calendar.wrap(cx)) {
    return false;
  }

  Instant endNs;
  if (!AddZonedDateTime(cx, epochInstant, timeZone, calendar,
                        {
                            0,
                            0,
                            0,
                            duration.days,
                            duration.hours,
                            duration.minutes,
                            duration.seconds,
                            duration.milliseconds,
                            duration.microseconds,
                            duration.nanoseconds,
                        },
                        &endNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(endNs));

  // Step 2.
  auto nanoseconds = endNs - epochInstant;
  MOZ_ASSERT(IsValidInstantSpan(nanoseconds));

  // Steps 3-4.
  double days = 0;
  if (TemporalUnit::Year <= largestUnit && largestUnit <= TemporalUnit::Day) {
    // Step 3.a.
    Rooted<temporal::NanosecondsAndDays> nanosAndDays(cx);
    if (!NanosecondsToDays(cx, nanoseconds, relativeTo, &nanosAndDays)) {
      return false;
    }

    // NB: |days| is passed to CreateTimeDurationRecord, which performs
    // |ℝ(𝔽(days))|, so it's safe to convert from BigInt to double here.

    // Step 3.b.
    days = nanosAndDays.daysNumber();
    MOZ_ASSERT(IsInteger(days));

    // FIXME: spec issue - `result.[[Nanoseconds]]` not created in all branches

    // Step 3.c.
    nanoseconds = nanosAndDays.nanoseconds();
    MOZ_ASSERT_IF(days > 0, nanoseconds >= InstantSpan{});
    MOZ_ASSERT_IF(days < 0, nanoseconds <= InstantSpan{});

    // Step 3.d.
    largestUnit = TemporalUnit::Hour;
  }

  // Steps 5-6.
  TimeDuration balanceResult;
  if (auto nanos = nanoseconds.toNanoseconds(); nanos.isValid()) {
    // Step 5.
    balanceResult = ::BalanceTimeDuration(nanos.value(), largestUnit);

    // Step 6.
    MOZ_ASSERT(IsValidDuration(balanceResult.toDuration()));
  } else {
    Rooted<BigInt*> ns(cx, ToEpochNanoseconds(cx, nanoseconds));
    if (!ns) {
      return false;
    }

    // Step 5.
    if (!::BalancePossiblyInfiniteTimeDurationSlow(cx, ns, largestUnit,
                                                   &balanceResult)) {
      return false;
    }

    // Step 6.
    if (!IsValidDuration(balanceResult.toDuration())) {
      *result = balanceResult;
      return true;
    }
  }

  // Step 7.
  *result = {
      days,
      balanceResult.hours,
      balanceResult.minutes,
      balanceResult.seconds,
      balanceResult.milliseconds,
      balanceResult.microseconds,
      balanceResult.nanoseconds,
  };
  return true;
}

/**
 * BalanceTimeDurationRelative ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit, zonedRelativeTo )
 */
static bool BalanceTimeDurationRelative(
    JSContext* cx, const Duration& duration, TemporalUnit largestUnit,
    Handle<Wrapped<ZonedDateTimeObject*>> relativeTo, TimeDuration* result) {
  // Step 1.
  if (!BalancePossiblyInfiniteTimeDurationRelative(cx, duration, largestUnit,
                                                   relativeTo, result)) {
    return false;
  }

  // Steps 2-3.
  return ThrowIfInvalidDuration(cx, result->toDuration());
}

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
bool js::temporal::BalanceTimeDuration(JSContext* cx,
                                       const InstantSpan& nanoseconds,
                                       TemporalUnit largestUnit,
                                       TimeDuration* result) {
  MOZ_ASSERT(IsValidInstantSpan(nanoseconds));

  // Steps 1-3. (Not applicable)

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto nanos = nanoseconds.toNanoseconds(); nanos.isValid()) {
    *result = ::BalanceTimeDuration(nanos.value(), largestUnit);
    return true;
  }

  Rooted<BigInt*> nanos(cx, ToEpochNanoseconds(cx, nanoseconds));
  if (!nanos) {
    return false;
  }

  // Steps 4-16.
  return ::BalanceTimeDurationSlow(cx, nanos, largestUnit, result);
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
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDateDurationRelativeSlow(
    JSContext* cx, const Duration& duration, double amountToAdd,
    TemporalUnit largestUnit, int32_t sign,
    MutableHandle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<DurationObject*> oneYear,
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

  // Steps 1-3.
  MOZ_ASSERT(largestUnit != TemporalUnit::Year);
  MOZ_ASSERT(!years->isZero() || !months->isZero() || !weeks->isZero() ||
             !days->isZero());

  // Step 4. (Not applicable)

  // Step 5.
  MOZ_ASSERT(sign == -1 || sign == 1);

  // Steps 6-10. (Not applicable)

  // Steps 11-13.
  if (largestUnit == TemporalUnit::Month) {
    // Steps 11.a-c. (Not applicable)

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

    // Step 11.d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    Rooted<BigInt*> oneYearMonths(cx);
    while (!years->isZero()) {
      // Step 11.d.i.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 11.d.ii-iv.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 11.d.v.
      oneYearMonths = BigInt::createFromDouble(cx, untilResult.months);
      if (!oneYearMonths) {
        return false;
      }

      // Step 11.d.vi.
      dateRelativeTo.set(newRelativeTo);

      // Step 11.d.vii.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }

      // Step 11.d.viii.
      months = BigInt::add(cx, months, oneYearMonths);
      if (!months) {
        return false;
      }
    }
  } else if (largestUnit == TemporalUnit::Week) {
    // Steps 12.a-c. (Not applicable)

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

    // Step 12.d.
    Rooted<BigInt*> oneYearDays(cx);
    while (!years->isZero()) {
      // Steps 12.d.i-ii.
      int32_t oneYearDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            dateRelativeTo, &oneYearDaysInt)) {
        return false;
      }
      oneYearDays = BigInt::createFromInt64(cx, oneYearDaysInt);
      if (!oneYearDays) {
        return false;
      }

      // Step 12.d.iii.
      days = BigInt::add(cx, days, oneYearDays);
      if (!days) {
        return false;
      }

      // Step 12.d.iv.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }
    }

    // Step 12.e.
    Rooted<BigInt*> oneMonthDays(cx);
    while (!months->isZero()) {
      // Steps 12.e.i-ii.
      int32_t oneMonthDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            dateRelativeTo, &oneMonthDaysInt)) {
        return false;
      }
      oneMonthDays = BigInt::createFromInt64(cx, oneMonthDaysInt);
      if (!oneMonthDays) {
        return false;
      }

      // Step 12.e.iii.
      days = BigInt::add(cx, days, oneMonthDays);
      if (!days) {
        return false;
      }

      // Step 12.e.iv.
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

    // Step 13.a.

    // Steps 13.a.i-iii. (Not applicable)

    // Step 13.a.iv.
    Rooted<BigInt*> oneYearDays(cx);
    while (!years->isZero()) {
      // Steps 13.a.iv.1-2.
      int32_t oneYearDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            dateRelativeTo, &oneYearDaysInt)) {
        return false;
      }
      oneYearDays = BigInt::createFromInt64(cx, oneYearDaysInt);
      if (!oneYearDays) {
        return false;
      }

      // Step 13.a.iv.3.
      days = BigInt::add(cx, days, oneYearDays);
      if (!days) {
        return false;
      }

      // Step 13.a.iv.4.
      if (sign < 0) {
        years = BigInt::inc(cx, years);
      } else {
        years = BigInt::dec(cx, years);
      }
      if (!years) {
        return false;
      }
    }

    // Step 13.a.v.
    Rooted<BigInt*> oneMonthDays(cx);
    while (!months->isZero()) {
      // Steps 13.a.v.1-2.
      int32_t oneMonthDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            dateRelativeTo, &oneMonthDaysInt)) {
        return false;
      }
      oneMonthDays = BigInt::createFromInt64(cx, oneMonthDaysInt);
      if (!oneMonthDays) {
        return false;
      }

      // Step 13.a.v.3.
      days = BigInt::add(cx, days, oneMonthDays);
      if (!days) {
        return false;
      }

      // Step 13.a.v.4.
      if (sign < 0) {
        months = BigInt::inc(cx, months);
      } else {
        months = BigInt::dec(cx, months);
      }
      if (!months) {
        return false;
      }
    }

    // Step 13.a.vi.
    Rooted<BigInt*> oneWeekDays(cx);
    while (!weeks->isZero()) {
      // Steps 13.a.vi.1-2.
      int32_t oneWeekDaysInt;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            dateRelativeTo, &oneWeekDaysInt)) {
        return false;
      }
      oneWeekDays = BigInt::createFromInt64(cx, oneWeekDaysInt);
      if (!oneWeekDays) {
        return false;
      }

      // Step 13.a.vi.3.
      days = BigInt::add(cx, days, oneWeekDays);
      if (!days) {
        return false;
      }

      // Step 13.a.vi.4.
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

  // Step 14.
  return CreateDateDurationRecord(
      cx, BigInt::numberValue(years), BigInt::numberValue(months),
      BigInt::numberValue(weeks), BigInt::numberValue(days), result);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDateDurationRelative(JSContext* cx,
                                          const Duration& duration,
                                          TemporalUnit largestUnit,
                                          Handle<JSObject*> relativeTo,
                                          DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // Steps 1-3.
  if (largestUnit == TemporalUnit::Year ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 3.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Step 4.
  int32_t sign = DurationSign({years, months, weeks, days});

  // Step 5.
  MOZ_ASSERT(sign != 0);

  // Step 6.
  Rooted<DurationObject*> oneYear(cx,
                                  CreateTemporalDuration(cx, {double(sign)}));
  if (!oneYear) {
    return false;
  }

  // Step 7.
  Rooted<DurationObject*> oneMonth(
      cx, CreateTemporalDuration(cx, {0, double(sign)}));
  if (!oneMonth) {
    return false;
  }

  // Step 8.
  Rooted<DurationObject*> oneWeek(
      cx, CreateTemporalDuration(cx, {0, 0, double(sign)}));
  if (!oneWeek) {
    return false;
  }

  // Step 9.
  auto date = ToTemporalDate(cx, relativeTo);
  if (!date) {
    return false;
  }
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, date);

  Rooted<CalendarValue> calendar(cx, date.unwrap().calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 10. (Not applicable)

  // Steps 11-13.
  if (largestUnit == TemporalUnit::Month) {
    // Step 11.a. (Not applicable in our implementation.)

    // Steps 11.b-c.
    Rooted<Value> dateAdd(cx);
    Rooted<Value> dateUntil(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());

      // Step 11.b.
      if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }

      // Step 11.c.
      if (!GetMethod(cx, calendarObj, cx->names().dateUntil, &dateUntil)) {
        return false;
      }
    }

    // Go to the slow path when the result is inexact.
    // NB: |years -= sign| is equal to |years| for large number values.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months))) {
      return UnbalanceDateDurationRelativeSlow(
          cx, {years, months, weeks, days}, 0, largestUnit, sign,
          &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
          dateUntil, result);
    }

    // Step 11.d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    while (years != 0) {
      // Step 11.d.i.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 11.d.ii-iv.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 11.d.v.
      double oneYearMonths = untilResult.months;

      // Step 11.d.vi.
      dateRelativeTo = newRelativeTo;

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(months + oneYearMonths))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearMonths, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 11.d.vii.
      years -= sign;

      // Step 11.d.viii.
      months += oneYearMonths;
    }
  } else if (largestUnit == TemporalUnit::Week) {
    // Step 12.a. (Not applicable in our implementation.)

    // Steps 12.b-c.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Go to the slow path when the result is inexact.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months) ||
                     !IsSafeInteger(days))) {
      return UnbalanceDateDurationRelativeSlow(
          cx, {years, months, weeks, days}, 0, largestUnit, sign,
          &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
          UndefinedHandleValue, result);
    }

    // Step 12.d.
    while (years != 0) {
      // Steps 12.d.i-ii.
      int32_t oneYearDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &dateRelativeTo, &oneYearDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneYearDays))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 12.d.iii.
      days += oneYearDays;

      // Step 12.d.iv.
      years -= sign;
    }

    // Step 12.e.
    while (months != 0) {
      // Steps 12.e.i-ii.
      int32_t oneMonthDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &dateRelativeTo, &oneMonthDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneMonthDays))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneMonthDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 12.e.iii.
      days += oneMonthDays;

      // Step 12.e.iv.
      months -= sign;
    }
  } else if (years != 0 || months != 0 || weeks != 0) {
    // Step 13.a.

    // FIXME: why don't we unconditionally throw an error for missing calendars?

    // Step 13.a.i. (Not applicable in our implementation.)

    // Steps 13.a.ii-iii.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Go to the slow path when the result is inexact.
    if (MOZ_UNLIKELY(!IsSafeInteger(years) || !IsSafeInteger(months) ||
                     !IsSafeInteger(weeks) || !IsSafeInteger(days))) {
      return UnbalanceDateDurationRelativeSlow(
          cx, {years, months, weeks, days}, 0, largestUnit, sign,
          &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
          UndefinedHandleValue, result);
    }

    // Step 13.a.iv.
    while (years != 0) {
      // Steps 13.a.iv.1-2.
      int32_t oneYearDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &dateRelativeTo, &oneYearDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneYearDays))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneYearDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 13.a.iv.3.
      days += oneYearDays;

      // Step 13.a.iv.4.
      years -= sign;
    }

    // Step 13.a.v.
    while (months != 0) {
      // Steps 13.a.v.1-2.
      int32_t oneMonthDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &dateRelativeTo, &oneMonthDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneMonthDays))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneMonthDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 13.a.v.3.
      days += oneMonthDays;

      // Step 13.a.v.4.
      months -= sign;
    }

    // Step 13.a.vi.
    while (weeks != 0) {
      // Steps 13.a.vi.1-2.
      int32_t oneWeekDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            &dateRelativeTo, &oneWeekDays)) {
        return false;
      }

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(days + oneWeekDays))) {
        return UnbalanceDateDurationRelativeSlow(
            cx, {years, months, weeks, days}, oneWeekDays, largestUnit, sign,
            &dateRelativeTo, calendar, oneYear, oneMonth, oneWeek, dateAdd,
            UndefinedHandleValue, result);
      }

      // Step 13.a.vi.3.
      days += oneWeekDays;

      // Step 13.a.vi.4.
      weeks -= sign;
    }
  }

  // Step 14.
  return CreateDateDurationRecord(cx, years, months, weeks, days, result);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo )
 */
static bool UnbalanceDateDurationRelative(JSContext* cx,
                                          const Duration& duration,
                                          TemporalUnit largestUnit,
                                          DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // Steps 1-3.
  if (largestUnit == TemporalUnit::Year ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 3.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Steps 4-10. (Not applicable in our implementation.)

  // Steps 11-13.
  if (largestUnit == TemporalUnit::Month) {
    // Step 11.a.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  } else if (largestUnit == TemporalUnit::Week) {
    // Step 12.a.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  } else if (years != 0 || months != 0 || weeks != 0) {
    // Step 13.a.i.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
    return false;
  }

  // Step 14.
  *result = CreateDateDurationRecord(years, months, weeks, days);
  return true;
}

static bool BalanceDateDurationRelativeSlow(
    JSContext* cx, TemporalUnit largestUnit,
    MutableHandle<Wrapped<PlainDateObject*>> dateRelativeTo,
    MutableHandle<Wrapped<PlainDateObject*>> newRelativeTo,
    Handle<CalendarValue> calendar, Handle<DurationObject*> oneYear,
    Handle<Value> dateAdd, Handle<Value> dateUntil, double months,
    int32_t addedMonths, double oneYearMonths, uint32_t* resultAddedYears,
    double* resultMonth) {
  MOZ_ASSERT(largestUnit == TemporalUnit::Year);

  Rooted<BigInt*> bigIntMonths(cx, BigInt::createFromDouble(cx, months));
  if (!bigIntMonths) {
    return false;
  }

  if (addedMonths) {
    Rooted<BigInt*> bigIntAdded(cx, BigInt::createFromInt64(cx, addedMonths));
    if (!bigIntAdded) {
      return false;
    }

    bigIntMonths = BigInt::add(cx, bigIntMonths, bigIntAdded);
    if (!bigIntMonths) {
      return false;
    }
  }

  Rooted<BigInt*> bigIntOneYearMonths(
      cx, BigInt::createFromDouble(cx, oneYearMonths));
  if (!bigIntOneYearMonths) {
    return false;
  }

  MOZ_ASSERT(BigInt::absoluteCompare(bigIntMonths, bigIntOneYearMonths) >= 0);

  uint32_t addedYears = 0;

  while (BigInt::absoluteCompare(bigIntMonths, bigIntOneYearMonths) >= 0) {
    // Step 10.p.i.
    bigIntMonths = BigInt::sub(cx, bigIntMonths, bigIntOneYearMonths);
    if (!bigIntMonths) {
      return false;
    }

    // Step 10.p.ii. (Partial)
    addedYears += 1;

    // Step 10.p.iii.
    dateRelativeTo.set(newRelativeTo);

    // Step 10.p.iv.
    newRelativeTo.set(
        CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd));
    if (!newRelativeTo) {
      return false;
    }

    // Steps 10.p.v-vii.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                           TemporalUnit::Month, dateUntil, &untilResult)) {
      return false;
    }

    // Step 10.p.viii.
    bigIntOneYearMonths = BigInt::createFromDouble(cx, untilResult.months);
    if (!bigIntOneYearMonths) {
      return false;
    }
  }

  *resultAddedYears = addedYears;
  *resultMonth = BigInt::numberValue(bigIntMonths);
  return true;
}

/**
 * BalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * relativeTo
 * )
 */
static bool BalanceDateDurationRelative(JSContext* cx, const Duration& duration,
                                        TemporalUnit largestUnit,
                                        Handle<JSObject*> relativeTo,
                                        DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // Step 1.
  if (largestUnit > TemporalUnit::Week ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 1.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Step 2.
  if (!relativeTo) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // Step 3.
  int32_t sign = DurationSign({years, months, weeks, days});

  // Step 4.
  MOZ_ASSERT(sign != 0);

  // Step 5.
  Rooted<DurationObject*> oneYear(cx,
                                  CreateTemporalDuration(cx, {double(sign)}));
  if (!oneYear) {
    return false;
  }

  // Step 6.
  Rooted<DurationObject*> oneMonth(
      cx, CreateTemporalDuration(cx, {0, double(sign)}));
  if (!oneMonth) {
    return false;
  }

  // Step 7.
  Rooted<DurationObject*> oneWeek(
      cx, CreateTemporalDuration(cx, {0, 0, double(sign)}));
  if (!oneWeek) {
    return false;
  }

  // Step 8.
  auto date = ToTemporalDate(cx, relativeTo);
  if (!date) {
    return false;
  }
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, date);

  // Step 9.
  Rooted<CalendarValue> calendar(cx, date.unwrap().calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Steps 10-12.
  if (largestUnit == TemporalUnit::Year) {
    // Step 10.a.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Steps 10.b-d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneYearDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                          &newRelativeTo, &oneYearDays)) {
      return false;
    }

    // Sum up all added weeks to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t addedYears = 0;

    // Step 10.e.
    while (std::abs(days) >= std::abs(oneYearDays)) {
      // Step 10.e.i.
      //
      // This computation can be imprecise, but the result isn't observerable,
      // because MoveRelativeDate ensures that overly large number will be
      // rejected eventually.
      days -= oneYearDays;

      // Step 10.e.ii. (Partial)
      addedYears += 1;

      // Step 10.e.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 10.e.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &newRelativeTo, &oneYearDays)) {
        return false;
      }
    }

    // Steps 10.f-h.
    int32_t oneMonthDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }

    // Sum up all added weeks to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t addedMonths = 0;

    // Step 10.i.
    while (std::abs(days) >= std::abs(oneMonthDays)) {
      // Step 10.i.i.
      //
      // This computation can be imprecise, but the result isn't observerable,
      // because MoveRelativeDate ensures that overly large number will be
      // rejected eventually.
      days -= oneMonthDays;

      // Step 10.i.ii.
      addedMonths += 1;

      // Step 10.i.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 10.i.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &newRelativeTo, &oneMonthDays)) {
        return false;
      }
    }

    // Step 10.j.
    newRelativeTo =
        CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
    if (!newRelativeTo) {
      return false;
    }

    // Step 10.k.
    Rooted<Value> dateUntil(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateUntil,
                            &dateUntil)) {
        return false;
      }
    }

    // Steps 10.l-n.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                           TemporalUnit::Month, dateUntil, &untilResult)) {
      return false;
    }

    // Step 10.o.
    double oneYearMonths = untilResult.months;

    if (MOZ_LIKELY(IsSafeInteger(months + double(addedMonths) * sign))) {
      months += double(addedMonths) * sign;

      // Step 10.p.
      while (std::abs(months) >= std::abs(oneYearMonths)) {
        if (MOZ_UNLIKELY(!IsSafeInteger(months - oneYearMonths))) {
          // |addedMonths| was already handled above, so pass zero here.
          constexpr int32_t zeroAddedMonths = 0;

          uint32_t slowYears;
          double slowMonths;
          if (!BalanceDateDurationRelativeSlow(
                  cx, largestUnit, &dateRelativeTo, &newRelativeTo, calendar,
                  oneYear, dateAdd, dateUntil, months, zeroAddedMonths,
                  oneYearMonths, &slowYears, &slowMonths)) {
            return false;
          }

          addedYears += slowYears;
          months = slowMonths;
          break;
        }

        // Step 10.p.i.
        months -= oneYearMonths;

        // Step 10.p.ii. (Partial)
        addedYears += 1;

        // Step 10.p.iii.
        dateRelativeTo = newRelativeTo;

        // Step 10.p.iv.
        newRelativeTo =
            CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
        if (!newRelativeTo) {
          return false;
        }

        // Steps 10.p.v-vii.
        Duration untilResult;
        if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                               TemporalUnit::Month, dateUntil, &untilResult)) {
          return false;
        }

        // Step 10.p.viii.
        oneYearMonths = untilResult.months;
      }
    } else {
      uint32_t slowYears;
      double slowMonths;
      if (!BalanceDateDurationRelativeSlow(
              cx, largestUnit, &dateRelativeTo, &newRelativeTo, calendar,
              oneYear, dateAdd, dateUntil, months, int32_t(addedMonths) * sign,
              oneYearMonths, &slowYears, &slowMonths)) {
        return false;
      }

      addedYears += slowYears;
      months = slowMonths;
    }

    // Step 10.d.ii and 10.p.ii.
    years += double(addedYears) * sign;
  } else if (largestUnit == TemporalUnit::Month) {
    // Step 11.a.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Steps 11.b-d.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneMonthDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }

    // Sum up all added weeks to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t addedMonths = 0;

    // Step 11.e.
    while (std::abs(days) >= std::abs(oneMonthDays)) {
      // Step 11.e.i.
      //
      // This computation can be imprecise, but the result isn't observerable,
      // because MoveRelativeDate ensures that overly large number will be
      // rejected eventually.
      days -= oneMonthDays;

      // Step 11.e.ii. (Partial)
      addedMonths += 1;

      // Step 11.e.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 11.e.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &newRelativeTo, &oneMonthDays)) {
        return false;
      }
    }

    // Step 11.e.ii.
    months += double(addedMonths) * sign;
  } else {
    // Step 12.a.
    MOZ_ASSERT(largestUnit == TemporalUnit::Week);

    // Step 12.b.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Steps 12.c-e.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneWeekDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                          &newRelativeTo, &oneWeekDays)) {
      return false;
    }

    // Sum up all added weeks to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t addedWeeks = 0;

    // Step 12.f.
    while (std::abs(days) >= std::abs(oneWeekDays)) {
      // Step 12.f.i.
      //
      // This computation can be imprecise, but the result isn't observerable,
      // because MoveRelativeDate ensures that overly large number will be
      // rejected eventually.
      days -= oneWeekDays;

      // Step 12.f.ii. (Partial)
      addedWeeks += 1;

      // Step 12.f.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 12.f.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            &newRelativeTo, &oneWeekDays)) {
        return false;
      }
    }

    // Step 12.f.ii.
    weeks += double(addedWeeks) * sign;
  }

  // Step 13.
  *result = CreateDateDurationRecord(years, months, weeks, days);
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, relativeTo )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Duration* duration) {
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));

  // Step 1.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 2.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 3.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Step 4.a.
  if (largestUnit <= TemporalUnit::Week) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // Step 4.b.
  TimeDuration result;
  if (!BalanceTimeDuration(cx, one, two, largestUnit, &result)) {
    return false;
  }

  // Steps 4.c.
  *duration = result.toDuration();
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, relativeTo )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<Wrapped<PlainDateObject*>> relativeTo,
                        Duration* duration) {
  MOZ_ASSERT(IsValidDuration(one));
  MOZ_ASSERT(IsValidDuration(two));

  // Step 1.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 2.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 3.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Step 4. (Not applicable)

  // Step 5.a.
  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());

  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 5.b.
  auto dateDuration1 = one.date();

  // Step 5.c.
  auto dateDuration2 = two.date();

  // Step 5.d.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 5.e.
  Rooted<Wrapped<PlainDateObject*>> intermediate(
      cx, CalendarDateAdd(cx, calendar, relativeTo, dateDuration1, dateAdd));
  if (!intermediate) {
    return false;
  }

  // Step 5.f.
  Rooted<Wrapped<PlainDateObject*>> end(
      cx, CalendarDateAdd(cx, calendar, intermediate, dateDuration2, dateAdd));
  if (!end) {
    return false;
  }

  // Step 5.g.
  auto dateLargestUnit = std::min(TemporalUnit::Day, largestUnit);

  // Steps 5.h-j.
  Duration dateDifference;
  if (!CalendarDateUntil(cx, calendar, relativeTo, end, dateLargestUnit,
                         &dateDifference)) {
    return false;
  }

  // Step 5.k.
  TimeDuration result;
  if (!BalanceTimeDuration(cx, dateDifference.days, one.time(), two.time(),
                           largestUnit, &result)) {
    return false;
  }

  // Steps 5.l.
  *duration = {
      dateDifference.years, dateDifference.months, dateDifference.weeks,
      result.days,          result.hours,          result.minutes,
      result.seconds,       result.milliseconds,   result.microseconds,
      result.nanoseconds,
  };
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, relativeTo )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<Wrapped<ZonedDateTimeObject*>> relativeTo,
                        Duration* result) {
  // Step 1.
  auto largestUnit1 = DefaultTemporalLargestUnit(one);

  // Step 2.
  auto largestUnit2 = DefaultTemporalLargestUnit(two);

  // Step 3.
  auto largestUnit = std::min(largestUnit1, largestUnit2);

  // Steps 4-5. (Not applicable)

  // Step 6. (Not applicable in our implementation.)

  // Steps 7-8.
  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto epochInstant = ToInstant(unwrappedRelativeTo);
  Rooted<TimeZoneValue> timeZone(cx, unwrappedRelativeTo->timeZone());
  Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());

  if (!timeZone.wrap(cx)) {
    return false;
  }
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 9.
  Instant intermediateNs;
  if (!AddZonedDateTime(cx, epochInstant, timeZone, calendar, one,
                        &intermediateNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(intermediateNs));

  // Step 10.
  Instant endNs;
  if (!AddZonedDateTime(cx, intermediateNs, timeZone, calendar, two, &endNs)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(endNs));

  // Step 11.
  if (largestUnit > TemporalUnit::Day) {
    // Steps 11.a-b.
    return DifferenceInstant(cx, epochInstant, endNs, Increment{1},
                             TemporalUnit::Nanosecond, largestUnit,
                             TemporalRoundingMode::HalfExpand, result);
  }

  // Step 12.
  return DifferenceZonedDateTime(cx, epochInstant, endNs, timeZone, calendar,
                                 largestUnit, result);
}

static bool RoundDuration(JSContext* cx, int64_t totalNanoseconds,
                          TemporalUnit unit, Increment increment,
                          TemporalRoundingMode roundingMode, Duration* result) {
  MOZ_ASSERT(unit >= TemporalUnit::Hour);

  double rounded;
  if (!RoundNumberToIncrement(cx, totalNanoseconds, unit, increment,
                              roundingMode, &rounded)) {
    return false;
  }

  double hours = 0;
  double minutes = 0;
  double seconds = 0;
  double milliseconds = 0;
  double microseconds = 0;
  double nanoseconds = 0;

  switch (unit) {
    case TemporalUnit::Auto:
    case TemporalUnit::Year:
    case TemporalUnit::Week:
    case TemporalUnit::Month:
    case TemporalUnit::Day:
      MOZ_CRASH("Unexpected temporal unit");

    case TemporalUnit::Hour:
      hours = rounded;
      break;
    case TemporalUnit::Minute:
      minutes = rounded;
      break;
    case TemporalUnit::Second:
      seconds = rounded;
      break;
    case TemporalUnit::Millisecond:
      milliseconds = rounded;
      break;
    case TemporalUnit::Microsecond:
      microseconds = rounded;
      break;
    case TemporalUnit::Nanosecond:
      nanoseconds = rounded;
      break;
  }

  *result = {
      0,           0, 0, 0, hours, minutes, seconds, milliseconds, microseconds,
      nanoseconds,
  };
  return ThrowIfInvalidDuration(cx, *result);
}

static bool RoundDuration(JSContext* cx, Handle<BigInt*> totalNanoseconds,
                          TemporalUnit unit, Increment increment,
                          TemporalRoundingMode roundingMode, Duration* result) {
  MOZ_ASSERT(unit >= TemporalUnit::Hour);

  double rounded;
  if (!RoundNumberToIncrement(cx, totalNanoseconds, unit, increment,
                              roundingMode, &rounded)) {
    return false;
  }

  double hours = 0;
  double minutes = 0;
  double seconds = 0;
  double milliseconds = 0;
  double microseconds = 0;
  double nanoseconds = 0;

  switch (unit) {
    case TemporalUnit::Auto:
    case TemporalUnit::Year:
    case TemporalUnit::Week:
    case TemporalUnit::Month:
    case TemporalUnit::Day:
      MOZ_CRASH("Unexpected temporal unit");

    case TemporalUnit::Hour:
      hours = rounded;
      break;
    case TemporalUnit::Minute:
      minutes = rounded;
      break;
    case TemporalUnit::Second:
      seconds = rounded;
      break;
    case TemporalUnit::Millisecond:
      milliseconds = rounded;
      break;
    case TemporalUnit::Microsecond:
      microseconds = rounded;
      break;
    case TemporalUnit::Nanosecond:
      nanoseconds = rounded;
      break;
  }

  *result = {
      0,           0, 0, 0, hours, minutes, seconds, milliseconds, microseconds,
      nanoseconds,
  };
  return ThrowIfInvalidDuration(cx, *result);
}

/**
 * AdjustRoundedDurationDays ( years, months, weeks, days, hours, minutes,
 * seconds, milliseconds, microseconds, nanoseconds, increment, unit,
 * roundingMode [ , relativeTo ] )
 */
static bool AdjustRoundedDurationDaysSlow(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<ZonedDateTimeObject*>> relativeTo, InstantSpan dayLength,
    Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));
  MOZ_ASSERT(IsValidInstantSpan(dayLength));

  // Step 2.
  Rooted<BigInt*> timeRemainderNs(
      cx, TotalDurationNanosecondsSlow(cx, duration.time(), 0));
  if (!timeRemainderNs) {
    return false;
  }

  // Steps 3-5.
  int32_t direction = timeRemainderNs->sign();

  // Steps 6-7. (Computed in caller)

  // Step 8.
  Rooted<BigInt*> dayLengthNs(cx, ToEpochNanoseconds(cx, dayLength));
  if (!dayLengthNs) {
    return false;
  }
  MOZ_ASSERT(IsValidInstantSpan(dayLengthNs));

  // Step 9.
  Rooted<BigInt*> oneDayLess(cx, BigInt::sub(cx, timeRemainderNs, dayLengthNs));
  if (!oneDayLess) {
    return false;
  }

  // Step 10.
  if ((direction > 0 && oneDayLess->sign() < 0) ||
      (direction < 0 && oneDayLess->sign() > 0)) {
    *result = duration;
    return true;
  }

  // Step 11.
  Duration adjustedDateDuration;
  if (!AddDuration(cx,
                   {
                       duration.years,
                       duration.months,
                       duration.weeks,
                       duration.days,
                   },
                   {0, 0, 0, double(direction)}, relativeTo,
                   &adjustedDateDuration)) {
    return false;
  }

  // Step 12.
  Duration roundedTimeDuration;
  if (!RoundDuration(cx, oneDayLess, unit, increment, roundingMode,
                     &roundedTimeDuration)) {
    return false;
  }

  // Step 13.
  TimeDuration adjustedTimeDuration;
  if (!BalanceTimeDuration(cx, roundedTimeDuration, TemporalUnit::Hour,
                           &adjustedTimeDuration)) {
    return false;
  }

  // Step 14.
  *result = {
      adjustedDateDuration.years,        adjustedDateDuration.months,
      adjustedDateDuration.weeks,        adjustedDateDuration.days,
      adjustedTimeDuration.hours,        adjustedTimeDuration.minutes,
      adjustedTimeDuration.seconds,      adjustedTimeDuration.milliseconds,
      adjustedTimeDuration.microseconds, adjustedTimeDuration.nanoseconds,
  };
  MOZ_ASSERT(IsValidDuration(*result));
  return true;
}

/**
 * AdjustRoundedDurationDays ( years, months, weeks, days, hours, minutes,
 * seconds, milliseconds, microseconds, nanoseconds, increment, unit,
 * roundingMode [ , relativeTo ] )
 */
bool js::temporal::AdjustRoundedDurationDays(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<ZonedDateTimeObject*>> relativeTo, Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Step 1.
  if ((TemporalUnit::Year <= unit && unit <= TemporalUnit::Day) ||
      (unit == TemporalUnit::Nanosecond && increment == Increment{1})) {
    *result = duration;
    return true;
  }

  // The increment is limited for all smaller temporal units.
  MOZ_ASSERT(increment < MaximumTemporalDurationRoundingIncrement(unit));

  // Steps 3-5.
  //
  // Step 2 is moved below, so compute |direction| through DurationSign.
  int32_t direction = DurationSign(duration.time());

  auto* unwrappedRelativeTo = relativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }
  auto nanoseconds = ToInstant(unwrappedRelativeTo);
  Rooted<TimeZoneValue> timeZone(cx, unwrappedRelativeTo->timeZone());
  Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());

  if (!timeZone.wrap(cx)) {
    return false;
  }
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 6.
  Instant dayStart;
  if (!AddZonedDateTime(cx, nanoseconds, timeZone, calendar, duration.date(),
                        &dayStart)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(dayStart));

  // Step 7.
  Instant dayEnd;
  if (!AddZonedDateTime(cx, dayStart, timeZone, calendar,
                        {0, 0, 0, double(direction)}, &dayEnd)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(dayEnd));

  // Step 8.
  auto dayLength = dayEnd - dayStart;
  MOZ_ASSERT(IsValidInstantSpan(dayLength));

  // Step 2. (Reordered)
  auto timeRemainderNs = TotalDurationNanoseconds(duration.time(), 0);
  if (!timeRemainderNs) {
    return AdjustRoundedDurationDaysSlow(cx, duration, increment, unit,
                                         roundingMode, relativeTo, dayLength,
                                         result);
  }

  // Step 9.
  auto checkedOneDayLess = *timeRemainderNs - dayLength.toNanoseconds();
  if (!checkedOneDayLess.isValid()) {
    return AdjustRoundedDurationDaysSlow(cx, duration, increment, unit,
                                         roundingMode, relativeTo, dayLength,
                                         result);
  }
  auto oneDayLess = checkedOneDayLess.value();

  // Step 10.
  if ((direction > 0 && oneDayLess < 0) || (direction < 0 && oneDayLess > 0)) {
    *result = duration;
    return true;
  }

  // Step 11.
  Duration adjustedDateDuration;
  if (!AddDuration(cx,
                   {
                       duration.years,
                       duration.months,
                       duration.weeks,
                       duration.days,
                   },
                   {0, 0, 0, double(direction)}, relativeTo,
                   &adjustedDateDuration)) {
    return false;
  }

  // FIXME: spec issue - don't pass years,months,weeks,days to RoundDuration.

  // Step 12.
  Duration roundedTimeDuration;
  if (!RoundDuration(cx, oneDayLess, unit, increment, roundingMode,
                     &roundedTimeDuration)) {
    return false;
  }

  // Step 13.
  TimeDuration adjustedTimeDuration;
  if (!BalanceTimeDuration(cx, roundedTimeDuration, TemporalUnit::Hour,
                           &adjustedTimeDuration)) {
    return false;
  }

  // FIXME: spec bug - CreateDurationRecord is fallible because the adjusted
  // date and time durations can be have different signs.
  // https://github.com/tc39/proposal-temporal/issues/2536
  //
  // clang-format off
  //
  // {
  // let calendar = new class extends Temporal.Calendar {
  //   dateAdd(date, duration, options) {
  //     console.log(`dateAdd(${date}, ${duration})`);
  //     if (duration.days === 10) {
  //       return super.dateAdd(date, duration.negated(), options);
  //     }
  //     return super.dateAdd(date, duration, options);
  //   }
  // }("iso8601");
  //
  // let zdt = new Temporal.ZonedDateTime(0n, "UTC", calendar);
  //
  // let d = Temporal.Duration.from({
  //   days: 10,
  //   hours: 25,
  // });
  //
  // let r = d.round({
  //   smallestUnit: "nanoseconds",
  //   roundingIncrement: 5,
  //   relativeTo: zdt,
  // });
  // console.log(r.toString());
  // }
  //
  // clang-format on

  // Step 14.
  *result = {
      adjustedDateDuration.years,        adjustedDateDuration.months,
      adjustedDateDuration.weeks,        adjustedDateDuration.days,
      adjustedTimeDuration.hours,        adjustedTimeDuration.minutes,
      adjustedTimeDuration.seconds,      adjustedTimeDuration.milliseconds,
      adjustedTimeDuration.microseconds, adjustedTimeDuration.nanoseconds,
  };
  return ThrowIfInvalidDuration(cx, *result);
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
  Rooted<CalendarValue> calendar(cx);
  Rooted<TimeZoneValue> timeZone(cx);
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

      Rooted<CalendarValue> calendar(cx, dateTime->calendar());
      if (!calendar.wrap(cx)) {
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
    if (!GetTemporalCalendarWithISODefault(cx, obj, &calendar)) {
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
      if (!ToTemporalTimeZone(cx, timeZoneValue, &timeZone)) {
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

    // Step 7.c. (Not applicable in our implementation.)

    // Steps 7.e-f.
    if (timeZoneName) {
      // Step 7.f.i.
      if (!ToTemporalTimeZone(cx, timeZoneName, &timeZone)) {
        return false;
      }

      // Steps 7.f.ii-iii.
      if (isUTC) {
        offsetBehaviour = OffsetBehaviour::Exact;
      } else if (!hasOffset) {
        offsetBehaviour = OffsetBehaviour::Wall;
      }

      // Step 7.f.iv.
      matchBehaviour = MatchBehaviour::MatchMinutes;
    } else {
      MOZ_ASSERT(!timeZone);
    }

    // Steps 7.g-j.
    if (calendarString) {
      if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
        return false;
      }
    } else {
      calendar.set(CalendarValue(cx->names().iso8601));
    }

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
static bool TruncateNumber(JSContext* cx, Handle<BigInt*> numerator,
                           Handle<BigInt*> denominator, double* quotient,
                           double* rounded) {
  MOZ_ASSERT(!denominator->isNegative());
  MOZ_ASSERT(!denominator->isZero());

  // Dividing zero is always zero.
  if (numerator->isZero()) {
    *quotient = 0;
    *rounded = 0;
    return true;
  }

  int64_t num, denom;
  if (BigInt::isInt64(numerator, &num) &&
      BigInt::isInt64(denominator, &denom)) {
    TruncateNumber(num, denom, quotient, rounded);
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
  *rounded = q + BigInt::numberValue(rem) / BigInt::numberValue(denominator);
  return true;
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
  // the following BalanceTimeDuration call.

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

  *result = rounded.rounded;
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

static bool RoundDurationYearSlow(
    JSContext* cx, Handle<BigInt*> years, Handle<BigInt*> days,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t oneYearDays,
    Increment increment, TemporalRoundingMode roundingMode,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());

  Rooted<BigInt*> nanoseconds(
      cx, ToEpochNanoseconds(cx, nanosAndDays.nanoseconds()));
  if (!nanoseconds) {
    return false;
  }

  Rooted<BigInt*> dayLength(cx,
                            ToEpochNanoseconds(cx, nanosAndDays.dayLength()));
  if (!dayLength) {
    return false;
  }

  // FIXME: spec bug division by zero not handled
  // https://github.com/tc39/proposal-temporal/issues/2335
  if (oneYearDays == 0) {
    JS_ReportErrorASCII(cx, "division by zero");
    return false;
  }

  // Steps 9.z-ab.
  Rooted<BigInt*> denominator(
      cx, BigInt::createFromInt64(cx, std::abs(oneYearDays)));
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

  Rooted<BigInt*> yearNanos(cx, BigInt::mul(cx, years, denominator));
  if (!yearNanos) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, yearNanos);
  if (!totalNanoseconds) {
    return false;
  }

  double numYears;
  double rounded = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode, &numYears)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numYears,
                          &rounded)) {
      return false;
    }
  }

  // Step 9.ac.
  double numMonths = 0;
  double numWeeks = 0;

  // Step 19.
  Duration resultDuration = {numYears, numMonths, numWeeks};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 20.
  *result = {resultDuration, rounded};
  return true;
}

static bool RoundDurationYearSlow(
    JSContext* cx, Handle<BigInt*> inDays, Handle<BigInt*> years,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t daysPassed,
    Increment increment, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> days(cx, inDays);

  Rooted<BigInt*> biDaysPassed(cx, BigInt::createFromInt64(cx, daysPassed));
  if (!biDaysPassed) {
    return false;
  }

  // Step 9.u.
  days = BigInt::sub(cx, days, biDaysPassed);
  if (!days) {
    return false;
  }

  // Steps 9.v.
  bool daysIsNegative =
      days->isNegative() ||
      (days->isZero() && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 9.w.
  Rooted<DurationObject*> oneYear(cx, CreateTemporalDuration(cx, {sign}));
  if (!oneYear) {
    return false;
  }

  // Steps 9.v-y.
  Rooted<Wrapped<PlainDateObject*>> moveResultIgnored(cx);
  int32_t oneYearDays;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                        &moveResultIgnored, &oneYearDays)) {
    return false;
  }

  // Steps 9.x-ac and 19-20.
  return RoundDurationYearSlow(cx, years, days, nanosAndDays, oneYearDays,
                               increment, roundingMode, computeRemainder,
                               result);
}

static bool RoundDurationYearSlow(
    JSContext* cx, const Duration& duration, Handle<BigInt*> days,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, double yearsPassed,
    Increment increment, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> years(cx, BigInt::createFromDouble(cx, duration.years));
  if (!years) {
    return false;
  }

  // Step 9.p.
  Rooted<BigInt*> biYearsPassed(cx, BigInt::createFromDouble(cx, yearsPassed));
  if (!biYearsPassed) {
    return false;
  }

  years = BigInt::add(cx, years, biYearsPassed);
  if (!years) {
    return false;
  }

  // Step 9.q.
  Rooted<DurationObject*> yearsDuration(
      cx, CreateTemporalDuration(cx, {yearsPassed}));
  if (!yearsDuration) {
    return false;
  }

  // Steps 9.r-t.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, yearsDuration, dateAdd,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }

  // Steps 9.u-ac and 19-20.
  return RoundDurationYearSlow(cx, days, years, nanosAndDays, daysPassed,
                               increment, roundingMode, newRelativeTo, calendar,
                               dateAdd, computeRemainder, result);
}

static mozilla::Maybe<int64_t> DaysFrom(
    const temporal::NanosecondsAndDays& nanosAndDays) {
  if (auto* days = nanosAndDays.days) {
    int64_t daysInt;
    if (BigInt::isInt64(days, &daysInt)) {
      return mozilla::Some(daysInt);
    }
    return mozilla::Nothing();
  }
  return mozilla::Some(nanosAndDays.daysInt);
}

static BigInt* DaysFrom(JSContext* cx,
                        Handle<temporal::NanosecondsAndDays> nanosAndDays) {
  if (auto days = nanosAndDays.days()) {
    return days;
  }
  return BigInt::createFromInt64(cx, nanosAndDays.daysInt());
}

static bool RoundDurationYearSlow(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays,
    int32_t monthsWeeksInDays, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  // Step 6.b.iii.
  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, duration.days));
  if (!days) {
    return false;
  }

  Rooted<BigInt*> nanoDays(cx, DaysFrom(cx, nanosAndDays));
  if (!nanoDays) {
    return false;
  }

  days = BigInt::add(cx, days, nanoDays);
  if (!days) {
    return false;
  }

  // Step 9.i.
  Rooted<BigInt*> biMonthsWeeksInDays(
      cx, BigInt::createFromInt64(cx, monthsWeeksInDays));
  if (!biMonthsWeeksInDays) {
    return false;
  }

  days = BigInt::add(cx, days, biMonthsWeeksInDays);
  if (!days) {
    return false;
  }

  // FIXME: spec issue - truncation doesn't match the spec polyfill.
  // https://github.com/tc39/proposal-temporal/issues/2540

  Rooted<BigInt*> truncatedDays(cx, days);
  if (nanosAndDays.nanoseconds() > InstantSpan{}) {
    // Round toward positive infinity when the integer days are negative and the
    // fractional part is positive.
    if (truncatedDays->isNegative()) {
      truncatedDays = BigInt::inc(cx, truncatedDays);
      if (!truncatedDays) {
        return false;
      }
    }
  } else if (nanosAndDays.nanoseconds() < InstantSpan{}) {
    // Round toward negative infinity when the integer days are positive and the
    // fractional part is negative.
    truncatedDays = BigInt::dec(cx, truncatedDays);
    if (!truncatedDays) {
      return false;
    }
  }

  // Step 9.j.
  Rooted<DurationObject*> wholeDaysDuration(
      cx, CreateTemporalDuration(
              cx, {0, 0, 0, BigInt::numberValue(truncatedDays)}));
  if (!wholeDaysDuration) {
    return false;
  }

  // Step 9.k.
  Rooted<Wrapped<PlainDateObject*>> wholeDaysLater(
      cx, CalendarDateAdd(cx, calendar, dateRelativeTo, wholeDaysDuration,
                          dateAdd));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 9.l-n.
  Duration timePassed;
  if (!CalendarDateUntil(cx, calendar, dateRelativeTo, wholeDaysLater,
                         TemporalUnit::Year, &timePassed)) {
    return false;
  }

  // Step 9.o.
  double yearsPassed = timePassed.years;

  // Steps 9.p-ac and 19-20.
  return RoundDurationYearSlow(cx, duration, days, nanosAndDays, yearsPassed,
                               increment, roundingMode, dateRelativeTo,
                               calendar, dateAdd, computeRemainder, result);
}

static bool RoundDurationYearSlow(
    JSContext* cx, const Duration& duration, double days,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, double yearsPassed,
    Increment increment, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  // Steps 9.p-ac and 19-20.
  return RoundDurationYearSlow(cx, duration, biDays, nanosAndDays, yearsPassed,
                               increment, roundingMode, dateRelativeTo,
                               calendar, dateAdd, computeRemainder, result);
}

static bool RoundDurationYearSlow(
    JSContext* cx, double days, double years,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t daysPassed,
    Increment increment, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biYears(cx, BigInt::createFromDouble(cx, years));
  if (!biYears) {
    return false;
  }

  // Steps 9.u-ac and 19-20.
  return RoundDurationYearSlow(cx, biDays, biYears, nanosAndDays, daysPassed,
                               increment, roundingMode, dateRelativeTo,
                               calendar, dateAdd, computeRemainder, result);
}

static bool RoundDurationYear(JSContext* cx, const Duration& duration,
                              Handle<temporal::NanosecondsAndDays> nanosAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              Handle<CalendarValue> calendar,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;

  // Step 9.a.
  Rooted<DurationObject*> yearsDuration(cx,
                                        CreateTemporalDuration(cx, {years}));
  if (!yearsDuration) {
    return false;
  }

  // Steps 9.b-c.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 9.d.
  auto yearsLater =
      CalendarDateAdd(cx, calendar, dateRelativeTo, yearsDuration, dateAdd);
  if (!yearsLater) {
    return false;
  }
  auto yearsLaterDate = ToPlainDate(&yearsLater.unwrap());

  // Step 9.h. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsLater);

  // Step 9.e.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 9.f.
  PlainDate yearsMonthsWeeksLater;
  if (!CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonthsWeeks, dateAdd,
                       &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 9.g.
  int32_t monthsWeeksInDays = DaysUntil(yearsLaterDate, yearsMonthsWeeksLater);
  MOZ_ASSERT(std::abs(monthsWeeksInDays) <= 200'000'000);

  // Step 9.h. (Moved up)

  // Step 6.b.iii. (Reordered)
  double days = duration.days;
  double extraDays = nanosAndDays.daysNumber();

  // Non-zero |days| and |extraDays| can't have oppositive signs. That means
  // when adding |days + extraDays| we don't have to worry about a case like:
  //
  // days = 9007199254740991 and
  // extraDays = 𝔽(-9007199254740993) = -9007199254740992
  //
  // ℝ(𝔽(days) + 𝔽(extraDays)) is -1, whereas the correct result is -2.
  MOZ_ASSERT((days <= 0 && extraDays <= 0) || (days >= 0 && extraDays >= 0));

  if (MOZ_UNLIKELY(!IsSafeInteger(days + extraDays))) {
    return RoundDurationYearSlow(cx, duration, nanosAndDays, monthsWeeksInDays,
                                 increment, roundingMode, newRelativeTo,
                                 calendar, dateAdd, computeRemainder, result);
  }
  days += extraDays;

  // Step 9.i.
  if (MOZ_UNLIKELY(!IsSafeInteger(days + monthsWeeksInDays))) {
    return RoundDurationYearSlow(cx, duration, nanosAndDays, monthsWeeksInDays,
                                 increment, roundingMode, newRelativeTo,
                                 calendar, dateAdd, computeRemainder, result);
  }
  days += monthsWeeksInDays;

  // FIXME: spec issue - truncation doesn't match the spec polyfill.
  // https://github.com/tc39/proposal-temporal/issues/2540

  double truncatedDays = days;
  if (nanosAndDays.nanoseconds() > InstantSpan{}) {
    // Round toward positive infinity when the integer days are negative and the
    // fractional part is positive.
    if (truncatedDays < 0) {
      truncatedDays += 1;
    }
  } else if (nanosAndDays.nanoseconds() < InstantSpan{}) {
    // Round toward negative infinity when the integer days are positive and the
    // fractional part is negative.
    if (truncatedDays > 0) {
      truncatedDays -= 1;
    }
  }

  // Step 9.j.
  Rooted<DurationObject*> wholeDaysDuration(
      cx, CreateTemporalDuration(cx, {0, 0, 0, truncatedDays}));
  if (!wholeDaysDuration) {
    return false;
  }

  // Step 9.k.
  Rooted<Wrapped<PlainDateObject*>> wholeDaysLater(
      cx,
      CalendarDateAdd(cx, calendar, newRelativeTo, wholeDaysDuration, dateAdd));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 9.l-n.
  Duration timePassed;
  if (!CalendarDateUntil(cx, calendar, newRelativeTo, wholeDaysLater,
                         TemporalUnit::Year, &timePassed)) {
    return false;
  }

  // Step 9.o.
  double yearsPassed = timePassed.years;

  // Step 9.p.
  if (MOZ_UNLIKELY(!IsSafeInteger(years + yearsPassed))) {
    return RoundDurationYearSlow(cx, duration, days, nanosAndDays, yearsPassed,
                                 increment, roundingMode, newRelativeTo,
                                 calendar, dateAdd, computeRemainder, result);
  }
  years += yearsPassed;

  // Step 9.q.
  yearsDuration = CreateTemporalDuration(cx, {yearsPassed});
  if (!yearsDuration) {
    return false;
  }

  // Steps 9.r-t.
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, yearsDuration, dateAdd,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }

  // Step 9.u.
  if (MOZ_UNLIKELY(!IsSafeInteger(days - daysPassed))) {
    return RoundDurationYearSlow(cx, days, years, nanosAndDays, daysPassed,
                                 increment, roundingMode, newRelativeTo,
                                 calendar, dateAdd, computeRemainder, result);
  }
  days -= daysPassed;

  // Steps 9.v.
  bool daysIsNegative =
      days < 0 || (days == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 9.w.
  Rooted<DurationObject*> oneYear(cx, CreateTemporalDuration(cx, {sign}));
  if (!oneYear) {
    return false;
  }

  // Steps 9.x-y.
  Rooted<Wrapped<PlainDateObject*>> moveResultIgnored(cx);
  int32_t oneYearDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneYear, dateAdd,
                        &moveResultIgnored, &oneYearDays)) {
    return false;
  }

  do {
    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    if (!nanoseconds.isValid()) {
      break;
    }

    auto dayLength = nanosAndDays.dayLength().toNanoseconds();
    if (!dayLength.isValid()) {
      break;
    }

    // FIXME: spec bug division by zero not handled
    // https://github.com/tc39/proposal-temporal/issues/2335
    if (oneYearDays == 0) {
      JS_ReportErrorASCII(cx, "division by zero");
      return false;
    }

    // Steps 9.z-ab.
    auto denominator = dayLength * std::abs(oneYearDays);
    if (!denominator.isValid()) {
      break;
    }

    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    auto totalNanoseconds = dayLength * intDays;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    totalNanoseconds += nanoseconds;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    int64_t intYears;
    if (!mozilla::NumberEqualsInt64(years, &intYears)) {
      break;
    }

    auto yearNanos = denominator * intYears;
    if (!yearNanos.isValid()) {
      break;
    }

    totalNanoseconds += yearNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numYears;
    double rounded = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numYears)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numYears,
                     &rounded);
    }

    // Step 9.ac.
    double numMonths = 0;
    double numWeeks = 0;

    // Step 19.
    Duration resultDuration = {numYears, numMonths, numWeeks};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 20.
    *result = {resultDuration, rounded};
    return true;
  } while (false);

  Rooted<BigInt*> biYears(cx, BigInt::createFromDouble(cx, years));
  if (!biYears) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  // Steps 9.z-ac and 19-20.
  return RoundDurationYearSlow(cx, biYears, biDays, nanosAndDays, oneYearDays,
                               increment, roundingMode, computeRemainder,
                               result);
}

static bool RoundDurationMonthSlow(
    JSContext* cx, const Duration& duration, Handle<BigInt*> months,
    Handle<BigInt*> days, Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<BigInt*> oneMonthDays, Increment increment,
    TemporalRoundingMode roundingMode, ComputeRemainder computeRemainder,
    RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());
  MOZ_ASSERT(!oneMonthDays->isNegative());
  MOZ_ASSERT(!oneMonthDays->isZero());

  Rooted<BigInt*> nanoseconds(
      cx, ToEpochNanoseconds(cx, nanosAndDays.nanoseconds()));
  if (!nanoseconds) {
    return false;
  }

  Rooted<BigInt*> dayLength(cx,
                            ToEpochNanoseconds(cx, nanosAndDays.dayLength()));
  if (!dayLength) {
    return false;
  }

  // Steps 10.o-q.
  Rooted<BigInt*> denominator(cx, BigInt::mul(cx, oneMonthDays, dayLength));
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

  Rooted<BigInt*> monthNanos(cx, BigInt::mul(cx, months, denominator));
  if (!monthNanos) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, monthNanos);
  if (!totalNanoseconds) {
    return false;
  }

  double numMonths;
  double rounded = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode,
                                          &numMonths)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numMonths,
                          &rounded)) {
      return false;
    }
  }

  // Step 10.r.
  double numWeeks = 0;
  double numDays = 0;

  // Step 19. (Not applicable in our implementation.)

  // Step 20.
  Duration resultDuration = {duration.years, numMonths, numWeeks, numDays};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 21.
  *result = {resultDuration, rounded};
  return true;
}

static bool RoundDurationMonthSlow(
    JSContext* cx, const Duration& duration, double sign,
    Handle<BigInt*> inMonths, Handle<BigInt*> inDays,
    Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<DurationObject*> oneMonth, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> months(cx, inMonths);
  Rooted<BigInt*> days(cx, inDays);

  // Steps 10.k-m or 10.n.iii-v.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t oneMonthDays;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                        &newRelativeTo, &oneMonthDays)) {
    return false;
  }

  Rooted<BigInt*> biOneMonthDays(cx, BigInt::createFromInt64(cx, oneMonthDays));
  if (!biOneMonthDays) {
    return false;
  }

  auto daysLargerThanOrEqualToOneMonthDays = [&]() {
    auto cmp = BigInt::absoluteCompare(days, biOneMonthDays);
    if (cmp > 0) {
      return true;
    }
    if (cmp < 0) {
      return false;
    }

    // Compare the fractional part of |days|, cf. step 6.e.
    auto nanoseconds = nanosAndDays.nanoseconds();
    return nanoseconds == InstantSpan{} ||
           (days->isNegative() == (nanoseconds < InstantSpan{}));
  };

  // Step 10.n.
  while (daysLargerThanOrEqualToOneMonthDays()) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    // Step 10.n.i.
    if (sign < 0) {
      months = BigInt::dec(cx, months);
    } else {
      months = BigInt::inc(cx, months);
    }
    if (!months) {
      return false;
    }

    // Step 10.n.ii.
    days = BigInt::sub(cx, days, biOneMonthDays);
    if (!days) {
      return false;
    }

    // Steps 10.n.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }

    biOneMonthDays = BigInt::createFromInt64(cx, oneMonthDays);
    if (!biOneMonthDays) {
      return false;
    }
  }

  if (biOneMonthDays->isNegative()) {
    biOneMonthDays = BigInt::neg(cx, biOneMonthDays);
    if (!biOneMonthDays) {
      return false;
    }
  }

  // Steps 10.o-r and 19-21.
  return RoundDurationMonthSlow(cx, duration, months, days, nanosAndDays,
                                biOneMonthDays, increment, roundingMode,
                                computeRemainder, result);
}

static bool RoundDurationMonthSlow(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t weeksInDays,
    Increment increment, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> months(cx, BigInt::createFromDouble(cx, duration.months));
  if (!months) {
    return false;
  }

  // Step 6.e.
  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, duration.days));
  if (!days) {
    return false;
  }

  Rooted<BigInt*> nanoDays(cx, DaysFrom(cx, nanosAndDays));
  if (!nanoDays) {
    return false;
  }

  days = BigInt::add(cx, days, nanoDays);
  if (!days) {
    return false;
  }

  // Step 10.h.
  Rooted<BigInt*> biWeeksInDays(cx, BigInt::createFromInt64(cx, weeksInDays));
  if (!biWeeksInDays) {
    return false;
  }

  days = BigInt::add(cx, days, biWeeksInDays);
  if (!days) {
    return false;
  }

  // Step 10.i.
  bool daysIsNegative =
      days->isNegative() ||
      (days->isZero() && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 10.j.
  Rooted<DurationObject*> oneMonth(cx, CreateTemporalDuration(cx, {0, sign}));
  if (!oneMonth) {
    return false;
  }

  // Steps 10.k-r and 19-21.
  return RoundDurationMonthSlow(cx, duration, sign, months, days, nanosAndDays,
                                oneMonth, increment, roundingMode,
                                dateRelativeTo, calendar, dateAdd,
                                computeRemainder, result);
}

static bool RoundDurationMonthSlow(
    JSContext* cx, const Duration& duration, double sign, double months,
    double days, int32_t oneMonthDays,
    Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<DurationObject*> oneMonth, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> biMonths(cx, BigInt::createFromDouble(cx, months));
  if (!biMonths) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biOneMonthDays(cx, BigInt::createFromInt64(cx, oneMonthDays));
  if (!biOneMonthDays) {
    return false;
  }

  // Step 10.n.i.
  if (sign < 0) {
    biMonths = BigInt::dec(cx, biMonths);
  } else {
    biMonths = BigInt::inc(cx, biMonths);
  }
  if (!biMonths) {
    return false;
  }

  // Step 10.n.ii.
  biDays = BigInt::sub(cx, biDays, biOneMonthDays);
  if (!biDays) {
    return false;
  }

  // Steps 10.n-r and 19-21.
  return RoundDurationMonthSlow(cx, duration, sign, biMonths, biDays,
                                nanosAndDays, oneMonth, increment, roundingMode,
                                dateRelativeTo, calendar, dateAdd,
                                computeRemainder, result);
}

static bool RoundDurationMonth(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, ComputeRemainder computeRemainder,
    RoundedDuration* result) {
  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;

  // Step 10.a.
  Rooted<DurationObject*> yearsMonths(
      cx, CreateTemporalDuration(cx, {years, months}));
  if (!yearsMonths) {
    return false;
  }

  // Step 10.b.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 10.c.
  auto yearsMonthsLater =
      CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonths, dateAdd);
  if (!yearsMonthsLater) {
    return false;
  }
  auto yearsMonthsLaterDate = ToPlainDate(&yearsMonthsLater.unwrap());

  // Step 10.g. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsMonthsLater);

  // Step 10.d.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 10.e.
  PlainDate yearsMonthsWeeksLater;
  if (!CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonthsWeeks, dateAdd,
                       &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 10.f.
  int32_t weeksInDays = DaysUntil(yearsMonthsLaterDate, yearsMonthsWeeksLater);

  // Step 10.g. (Moved up)

  // Step 6.e. (Reordered)
  double days = duration.days;
  double extraDays = nanosAndDays.daysNumber();

  // Non-zero |days| and |extraDays| can't have oppositive signs. That means
  // when adding |days + extraDays| we don't have to worry about a case like:
  //
  // days = 9007199254740991 and
  // extraDays = 𝔽(-9007199254740993) = -9007199254740992
  //
  // ℝ(𝔽(days) + 𝔽(extraDays)) is -1, whereas the correct result is -2.
  MOZ_ASSERT((days <= 0 && extraDays <= 0) || (days >= 0 && extraDays >= 0));

  if (MOZ_UNLIKELY(!IsSafeInteger(days + extraDays))) {
    return RoundDurationMonthSlow(cx, duration, nanosAndDays, weeksInDays,
                                  increment, roundingMode, newRelativeTo,
                                  calendar, dateAdd, computeRemainder, result);
  }
  days += extraDays;

  // Step 10.h.
  if (MOZ_UNLIKELY(!IsSafeInteger(days + weeksInDays))) {
    return RoundDurationMonthSlow(cx, duration, nanosAndDays, weeksInDays,
                                  increment, roundingMode, newRelativeTo,
                                  calendar, dateAdd, computeRemainder, result);
  }
  days += weeksInDays;

  // Step 10.i.
  bool daysIsNegative =
      days < 0 || (days == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 10.j.
  Rooted<DurationObject*> oneMonth(cx, CreateTemporalDuration(cx, {0, sign}));
  if (!oneMonth) {
    return false;
  }

  // Steps 10.k-m.
  int32_t oneMonthDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth, dateAdd,
                        &newRelativeTo, &oneMonthDays)) {
    return false;
  }

  // FIXME: spec issue - can this loop be unbounded with a user-controlled
  // calendar?

  auto daysLargerThanOrEqualToOneMonthDays = [&]() {
    if (std::abs(days) > std::abs(oneMonthDays)) {
      return true;
    }
    if (std::abs(days) < std::abs(oneMonthDays)) {
      return false;
    }

    // Compare the fractional part of |days|, cf. step 6.e.
    auto nanoseconds = nanosAndDays.nanoseconds();
    return nanoseconds == InstantSpan{} ||
           ((days < 0) == (nanoseconds < InstantSpan{}));
  };

  // Step 10.n.
  while (daysLargerThanOrEqualToOneMonthDays()) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    if (MOZ_UNLIKELY(!IsSafeInteger(months + sign) ||
                     !IsSafeInteger(days - oneMonthDays))) {
      return RoundDurationMonthSlow(
          cx, duration, sign, months, days, oneMonthDays, nanosAndDays,
          oneMonth, increment, roundingMode, dateRelativeTo, calendar, dateAdd,
          computeRemainder, result);
    }

    // Step 10.n.i.
    months += sign;

    // Step 10.n.ii.
    days -= oneMonthDays;

    // Steps 10.n.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }
  }

  do {
    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    if (!nanoseconds.isValid()) {
      break;
    }

    auto dayLength = nanosAndDays.dayLength().toNanoseconds();
    if (!dayLength.isValid()) {
      break;
    }

    // Steps 10.o-q.
    auto denominator = dayLength * std::abs(oneMonthDays);
    if (!denominator.isValid()) {
      break;
    }

    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    auto totalNanoseconds = dayLength * intDays;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    totalNanoseconds += nanoseconds;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    int64_t intMonths;
    if (!mozilla::NumberEqualsInt64(months, &intMonths)) {
      break;
    }

    auto monthNanos = denominator * intMonths;
    if (!monthNanos.isValid()) {
      break;
    }

    totalNanoseconds += monthNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numMonths;
    double rounded = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numMonths)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numMonths,
                     &rounded);
    }

    // Step 10.r.
    double numWeeks = 0;
    double numDays = 0;

    // Step 19. (Not applicable in our implementation.)

    // Step 20.
    Duration resultDuration = {duration.years, numMonths, numWeeks, numDays};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 21.
    *result = {resultDuration, rounded};
    return true;
  } while (false);

  Rooted<BigInt*> biMonths(cx, BigInt::createFromDouble(cx, months));
  if (!biMonths) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biOneMonthDays(
      cx, BigInt::createFromInt64(cx, std::abs(oneMonthDays)));
  if (!biOneMonthDays) {
    return false;
  }

  // Steps 10.o-r and 19-21.
  return RoundDurationMonthSlow(cx, duration, biMonths, biDays, nanosAndDays,
                                biOneMonthDays, increment, roundingMode,
                                computeRemainder, result);
}

static bool RoundDurationWeekSlow(
    JSContext* cx, const Duration& duration, Handle<BigInt*> weeks,
    Handle<BigInt*> days, Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<BigInt*> oneWeekDays, Increment increment,
    TemporalRoundingMode roundingMode, ComputeRemainder computeRemainder,
    RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());
  MOZ_ASSERT(!oneWeekDays->isNegative());
  MOZ_ASSERT(!oneWeekDays->isZero());

  Rooted<BigInt*> nanoseconds(
      cx, ToEpochNanoseconds(cx, nanosAndDays.nanoseconds()));
  if (!nanoseconds) {
    return false;
  }

  Rooted<BigInt*> dayLength(cx,
                            ToEpochNanoseconds(cx, nanosAndDays.dayLength()));
  if (!dayLength) {
    return false;
  }

  // Steps 11.h-j.
  Rooted<BigInt*> denominator(cx, BigInt::mul(cx, oneWeekDays, dayLength));
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

  Rooted<BigInt*> weekNanos(cx, BigInt::mul(cx, weeks, denominator));
  if (!weekNanos) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, weekNanos);
  if (!totalNanoseconds) {
    return false;
  }

  double numWeeks;
  double rounded = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode, &numWeeks)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numWeeks,
                          &rounded)) {
      return false;
    }
  }

  // Step 11.k.
  double numDays = 0;

  // Step 19. (Not applicable in our implementation.)

  // Step 20.
  Duration resultDuration = {duration.years, duration.months, numWeeks,
                             numDays};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 21.
  *result = {resultDuration, rounded};
  return true;
}

static bool RoundDurationWeekSlow(
    JSContext* cx, const Duration& duration, double sign,
    Handle<BigInt*> inWeeks, Handle<BigInt*> inDays,
    Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<DurationObject*> oneWeek, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> weeks(cx, inWeeks);
  Rooted<BigInt*> days(cx, inDays);

  // Steps 11.d-f or 11.g.iii-v.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t oneWeekDays;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                        &newRelativeTo, &oneWeekDays)) {
    return false;
  }

  Rooted<BigInt*> biOneWeekDays(cx, BigInt::createFromInt64(cx, oneWeekDays));
  if (!biOneWeekDays) {
    return false;
  }

  auto daysLargerThanOrEqualToOneWeekDays = [&]() {
    auto cmp = BigInt::absoluteCompare(days, biOneWeekDays);
    if (cmp > 0) {
      return true;
    }
    if (cmp < 0) {
      return false;
    }

    // Compare the fractional part of |days|, cf. step 6.e.
    auto nanoseconds = nanosAndDays.nanoseconds();
    return nanoseconds == InstantSpan{} ||
           (days->isNegative() == (nanoseconds < InstantSpan{}));
  };

  // Step 11.g.
  while (daysLargerThanOrEqualToOneWeekDays()) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    // Step 11.g.i.
    if (sign < 0) {
      weeks = BigInt::dec(cx, weeks);
    } else {
      weeks = BigInt::inc(cx, weeks);
    }
    if (!weeks) {
      return false;
    }

    // Step 11.g.ii.
    days = BigInt::sub(cx, days, biOneWeekDays);
    if (!days) {
      return false;
    }

    // Steps 11.g.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneWeek, dateAdd,
                          &newRelativeTo, &oneWeekDays)) {
      return false;
    }

    biOneWeekDays = BigInt::createFromInt64(cx, oneWeekDays);
    if (!biOneWeekDays) {
      return false;
    }
  }

  if (biOneWeekDays->isNegative()) {
    biOneWeekDays = BigInt::neg(cx, biOneWeekDays);
    if (!biOneWeekDays) {
      return false;
    }
  }

  // Steps 11.h-k and 19-21.
  return RoundDurationWeekSlow(cx, duration, weeks, days, nanosAndDays,
                               biOneWeekDays, increment, roundingMode,
                               computeRemainder, result);
}

static bool RoundDurationWeekSlow(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, ComputeRemainder computeRemainder,
    RoundedDuration* result) {
  Rooted<BigInt*> weeks(cx, BigInt::createFromDouble(cx, duration.weeks));
  if (!weeks) {
    return false;
  }

  // Step 6.e.
  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, duration.days));
  if (!days) {
    return false;
  }

  Rooted<BigInt*> nanoDays(cx, DaysFrom(cx, nanosAndDays));
  if (!nanoDays) {
    return false;
  }

  days = BigInt::add(cx, days, nanoDays);
  if (!days) {
    return false;
  }

  // Step 11.a.
  bool daysIsNegative =
      days->isNegative() ||
      (days->isZero() && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 11.b.
  Rooted<DurationObject*> oneWeek(cx, CreateTemporalDuration(cx, {0, 0, sign}));
  if (!oneWeek) {
    return false;
  }

  // Step 11.c.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Steps 11.d-k and 19-21.
  return RoundDurationWeekSlow(cx, duration, sign, weeks, days, nanosAndDays,
                               oneWeek, increment, roundingMode, dateRelativeTo,
                               calendar, dateAdd, computeRemainder, result);
}

static bool RoundDurationWeekSlow(
    JSContext* cx, const Duration& duration, double sign, double weeks,
    double days, int32_t oneWeekDays,
    Handle<temporal::NanosecondsAndDays> nanosAndDays,
    Handle<DurationObject*> oneWeek, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<CalendarValue> calendar, Handle<Value> dateAdd,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  Rooted<BigInt*> biWeeks(cx, BigInt::createFromDouble(cx, weeks));
  if (!biWeeks) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biOneWeekDays(cx, BigInt::createFromInt64(cx, oneWeekDays));
  if (!biOneWeekDays) {
    return false;
  }

  // Step 11.g.i.
  if (sign < 0) {
    biWeeks = BigInt::dec(cx, biWeeks);
  } else {
    biWeeks = BigInt::inc(cx, biWeeks);
  }
  if (!biWeeks) {
    return false;
  }

  // Step 11.g.ii.
  biDays = BigInt::sub(cx, biDays, biOneWeekDays);
  if (!biDays) {
    return false;
  }

  // Steps 11.g-k and 19-21.
  return RoundDurationWeekSlow(cx, duration, sign, biWeeks, biDays,
                               nanosAndDays, oneWeek, increment, roundingMode,
                               dateRelativeTo, calendar, dateAdd,
                               computeRemainder, result);
}

static bool RoundDurationWeek(JSContext* cx, const Duration& duration,
                              Handle<temporal::NanosecondsAndDays> nanosAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              Handle<CalendarValue> calendar,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  // Step 6.e.
  double days = duration.days;
  double extraDays = nanosAndDays.daysNumber();

  // Non-zero |days| and |extraDays| can't have oppositive signs. That means
  // when adding |days + extraDays| we don't have to worry about a case like:
  //
  // days = 9007199254740991 and
  // extraDays = 𝔽(-9007199254740993) = -9007199254740992
  //
  // ℝ(𝔽(days) + 𝔽(extraDays)) is -1, whereas the correct result is -2.
  MOZ_ASSERT((days <= 0 && extraDays <= 0) || (days >= 0 && extraDays >= 0));

  if (MOZ_UNLIKELY(!IsSafeInteger(days + extraDays))) {
    return RoundDurationWeekSlow(cx, duration, nanosAndDays, increment,
                                 roundingMode, dateRelativeTo, calendar,
                                 computeRemainder, result);
  }
  days += extraDays;

  // Step 11.a.
  bool daysIsNegative =
      days < 0 || (days == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  double sign = daysIsNegative ? -1 : 1;

  // Step 11.b.
  Rooted<DurationObject*> oneWeek(cx, CreateTemporalDuration(cx, {0, 0, sign}));
  if (!oneWeek) {
    return false;
  }

  // Step 11.c.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Steps 11.d-f.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t oneWeekDays;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                        &newRelativeTo, &oneWeekDays)) {
    return false;
  }

  // FIXME: spec issue - can this loop be unbounded with a user-controlled
  // calendar?

  auto daysLargerThanOrEqualToOneWeekDays = [&]() {
    if (std::abs(days) > std::abs(oneWeekDays)) {
      return true;
    }
    if (std::abs(days) < std::abs(oneWeekDays)) {
      return false;
    }

    // Compare the fractional part of |days|, cf. step 6.e.
    auto nanoseconds = nanosAndDays.nanoseconds();
    return nanoseconds == InstantSpan{} ||
           ((days < 0) == (nanoseconds < InstantSpan{}));
  };

  // Step 11.g.
  double weeks = duration.weeks;
  while (daysLargerThanOrEqualToOneWeekDays()) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    if (MOZ_UNLIKELY(!IsSafeInteger(weeks + sign) ||
                     !IsSafeInteger(days - oneWeekDays))) {
      return RoundDurationWeekSlow(cx, duration, sign, weeks, days, oneWeekDays,
                                   nanosAndDays, oneWeek, increment,
                                   roundingMode, newRelativeTo, calendar,
                                   dateAdd, computeRemainder, result);
    }

    // Step 11.g.i.
    weeks += sign;

    // Step 11.g.ii.
    days -= oneWeekDays;

    // Steps 11.g.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneWeek, dateAdd,
                          &newRelativeTo, &oneWeekDays)) {
      return false;
    }
  }

  do {
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
    // The fractional part |nanoseconds / dayLength| is from step 6.
    //
    // The denominator for |fractionalWeeks| is |dayLength * abs(oneWeekDays)|.
    //
    //   fractionalWeeks
    // = weeks + (days + nanoseconds / dayLength) / abs(oneWeekDays)
    // = weeks + days / abs(oneWeekDays) + nanoseconds / (dayLength * abs(oneWeekDays))
    // = (weeks * dayLength * abs(oneWeekDays) + days * dayLength + nanoseconds) / (dayLength * abs(oneWeekDays))
    //
    // clang-format on

    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    if (!nanoseconds.isValid()) {
      break;
    }

    auto dayLength = nanosAndDays.dayLength().toNanoseconds();
    if (!dayLength.isValid()) {
      break;
    }

    // Steps 11.h-j.
    auto denominator = dayLength * std::abs(oneWeekDays);
    if (!denominator.isValid()) {
      break;
    }

    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    auto totalNanoseconds = dayLength * intDays;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    totalNanoseconds += nanoseconds;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    int64_t intWeeks;
    if (!mozilla::NumberEqualsInt64(weeks, &intWeeks)) {
      break;
    }

    auto weekNanos = denominator * intWeeks;
    if (!weekNanos.isValid()) {
      break;
    }

    totalNanoseconds += weekNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numWeeks;
    double rounded = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numWeeks)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numWeeks,
                     &rounded);
    }

    // Step 11.k.
    double numDays = 0;

    // Step 19. (Not applicable in our implementation.)

    // Step 20.
    Duration resultDuration = {duration.years, duration.months, numWeeks,
                               numDays};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 21.
    *result = {resultDuration, rounded};
    return true;
  } while (false);

  Rooted<BigInt*> biWeeks(cx, BigInt::createFromDouble(cx, weeks));
  if (!biWeeks) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromDouble(cx, days));
  if (!biDays) {
    return false;
  }

  Rooted<BigInt*> biOneWeekDays(
      cx, BigInt::createFromInt64(cx, std::abs(oneWeekDays)));
  if (!biOneWeekDays) {
    return false;
  }

  // Steps 11.h-k and 19-21.
  return RoundDurationWeekSlow(cx, duration, biWeeks, biDays, nanosAndDays,
                               biOneWeekDays, increment, roundingMode,
                               computeRemainder, result);
}

static bool RoundDurationDaySlow(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, Increment increment,
    TemporalRoundingMode roundingMode, ComputeRemainder computeRemainder,
    RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());

  Rooted<BigInt*> nanoseconds(
      cx, ToEpochNanoseconds(cx, nanosAndDays.nanoseconds()));
  if (!nanoseconds) {
    return false;
  }

  Rooted<BigInt*> dayLength(cx,
                            ToEpochNanoseconds(cx, nanosAndDays.dayLength()));
  if (!dayLength) {
    return false;
  }

  Rooted<BigInt*> nanoDays(cx, DaysFrom(cx, nanosAndDays));
  if (!nanoDays) {
    return false;
  }

  Rooted<BigInt*> totalNanoseconds(cx,
                                   BigInt::createFromDouble(cx, duration.days));
  if (!totalNanoseconds) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, nanoDays);
  if (!totalNanoseconds) {
    return false;
  }

  totalNanoseconds = BigInt::mul(cx, totalNanoseconds, dayLength);
  if (!totalNanoseconds) {
    return false;
  }

  totalNanoseconds = BigInt::add(cx, totalNanoseconds, nanoseconds);
  if (!totalNanoseconds) {
    return false;
  }

  // Steps 12.a-c.
  double days;
  double rounded = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, dayLength,
                                          increment, roundingMode, &days)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, dayLength, &days, &rounded)) {
      return false;
    }
  }

  // Step 19.
  MOZ_ASSERT(IsIntegerOrInfinity(days));

  // Step 20.
  Duration resultDuration = {duration.years, duration.months, duration.weeks,
                             days};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 21.
  *result = {resultDuration, rounded};
  return true;
}

static bool RoundDurationDay(JSContext* cx, const Duration& duration,
                             Handle<temporal::NanosecondsAndDays> nanosAndDays,
                             Increment increment,
                             TemporalRoundingMode roundingMode,
                             ComputeRemainder computeRemainder,
                             RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());

  do {
    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    auto dayLength = nanosAndDays.dayLength().toNanoseconds();

    auto nanoDays = DaysFrom(nanosAndDays);
    if (!nanoDays) {
      break;
    }

    int64_t durationDays;
    if (!mozilla::NumberEqualsInt64(duration.days, &durationDays)) {
      break;
    }

    auto totalNanoseconds = mozilla::CheckedInt64(durationDays) + *nanoDays;
    totalNanoseconds *= dayLength;
    totalNanoseconds += nanoseconds;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    // Steps 12.a-c.
    double days;
    double rounded = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            dayLength.value(), increment,
                                            roundingMode, &days)) {
        return false;
      }
    } else {
      ::TruncateNumber(totalNanoseconds.value(), dayLength.value(), &days,
                       &rounded);
    }

    // Step 19.
    MOZ_ASSERT(IsIntegerOrInfinity(days));

    // Step 20.
    Duration resultDuration = {duration.years, duration.months, duration.weeks,
                               days};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 21.
    *result = {resultDuration, rounded};
    return true;
  } while (false);

  // Steps 12 and 19-21.
  return RoundDurationDaySlow(cx, duration, nanosAndDays, increment,
                              roundingMode, computeRemainder, result);
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<JSObject*> relativeTo,
                          ComputeRemainder computeRemainder,
                          RoundedDuration* result) {
  // Note: |duration.days| can have a different sign than the other date
  // components. The date and time components can have different signs, too.
  MOZ_ASSERT(
      IsValidDuration({duration.years, duration.months, duration.weeks}));
  MOZ_ASSERT(IsValidDuration(duration.time()));

  // The remainder is only needed when called from |Duration_total|. And `total`
  // always passes |increment=1| and |roundingMode=trunc|.
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                increment == Increment{1});
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                roundingMode == TemporalRoundingMode::Trunc);

  // Steps 1-2. (Not applicable in our implementation.)
  MOZ_ASSERT(relativeTo);

  // Step 3.
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx);

  // FIXME: spec issue - only perform step 4 when unit is "year", "month",
  // "week"
  // https://github.com/tc39/proposal-temporal/issues/2247

  // Steps 4.a-c.
  Rooted<CalendarValue> calendar(cx);
  if (auto* unwrapped = relativeTo->maybeUnwrapIf<ZonedDateTimeObject>()) {
    // Step 4.a.i.
    zonedRelativeTo = relativeTo;

    // Step 4.c.
    calendar.set(unwrapped->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Step 4.a.ii
    dateRelativeTo = ToTemporalDate(cx, relativeTo);
    if (!dateRelativeTo) {
      return false;
    }
  } else if (auto* unwrapped = relativeTo->maybeUnwrapIf<PlainDateObject>()) {
    // Step 4.b.
    dateRelativeTo = relativeTo;

    // Step 4.c.
    calendar.set(unwrapped->calendar());
    if (!calendar.wrap(cx)) {
      return false;
    }
  } else if (IsDeadProxyObject(relativeTo)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
    return false;
  } else {
    MOZ_CRASH("expected either PlainDateObject or ZonedDateTimeObject");
  }

  // Step 5. (Not applicable)

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
      // Steps 7 and 12-21.
      return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                             computeRemainder, result);
    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 6.
  MOZ_ASSERT(TemporalUnit::Year <= unit && unit <= TemporalUnit::Day);

  // Steps 6.b-e.
  Rooted<temporal::NanosecondsAndDays> nanosAndDays(cx);
  if (zonedRelativeTo) {
    // Step 6.b.i. (Reordered)
    Rooted<ZonedDateTimeObject*> intermediate(
        cx, MoveRelativeZonedDateTime(cx, zonedRelativeTo, duration.date()));
    if (!intermediate) {
      return false;
    }

    // Steps 6.a and 6.b.ii.
    if (!NanosecondsToDays(cx, duration, intermediate, &nanosAndDays)) {
      return false;
    }

    // Step 6.b.iii. (Not applicable in our implementation.)
  } else {
    // Steps 6.a and 6.c.
    if (!::NanosecondsToDays(cx, duration, &nanosAndDays)) {
      return false;
    }
  }

  // NanosecondsToDays guarantees that |abs(nanosAndDays.nanoseconds)| is less
  // than |abs(nanosAndDays.dayLength)|.
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength());

  // Step 6.d. (Moved below)

  // Step 6.e. (Implicit)

  // Steps 7 and 13-18. (Not applicable)

  // Step 8.
  // FIXME: spec issue - `remainder` doesn't need be initialised.

  // Steps 9-21.
  switch (unit) {
    // Steps 9 and 19-21.
    case TemporalUnit::Year:
      return RoundDurationYear(cx, duration, nanosAndDays, increment,
                               roundingMode, dateRelativeTo, calendar,
                               computeRemainder, result);

    // Steps 10 and 19-21.
    case TemporalUnit::Month:
      return RoundDurationMonth(cx, duration, nanosAndDays, increment,
                                roundingMode, dateRelativeTo, calendar,
                                computeRemainder, result);

    // Steps 11 and 19-21.
    case TemporalUnit::Week:
      return RoundDurationWeek(cx, duration, nanosAndDays, increment,
                               roundingMode, dateRelativeTo, calendar,
                               computeRemainder, result);

    // Steps 12 and 19-21.
    case TemporalUnit::Day:
      return RoundDurationDay(cx, duration, nanosAndDays, increment,
                              roundingMode, computeRemainder, result);

    // Steps 13-18. (Handled elsewhere)
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
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<JSObject*> relativeTo, double* result) {
  // Only called from |Duration_total|, which always passes |increment=1| and
  // |roundingMode=trunc|.
  MOZ_ASSERT(increment == Increment{1});
  MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode, relativeTo,
                       ComputeRemainder::Yes, &rounded)) {
    return false;
  }

  *result = rounded.rounded;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<JSObject*> relativeTo, Duration* result) {
  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode, relativeTo,
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
                                 Handle<Wrapped<PlainDateObject*>> relativeTo,
                                 Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         relativeTo, result);
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * relativeTo ] )
 */
bool js::temporal::RoundDuration(JSContext* cx, const Duration& duration,
                                 Increment increment, TemporalUnit unit,
                                 TemporalRoundingMode roundingMode,
                                 Handle<ZonedDateTimeObject*> relativeTo,
                                 Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         relativeTo, result);
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

  // Step 3.
  Duration other;
  if (!ToTemporalDurationRecord(cx, args.get(0), &other)) {
    return false;
  }

  Rooted<JSObject*> relativeTo(cx);
  if (args.hasDefined(1)) {
    const char* name = operation == DurationOperation::Add ? "add" : "subtract";

    // Step 3.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", name, args[1]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!ToRelativeTemporalObject(cx, options, &relativeTo)) {
      return false;
    }
  }

  // Step 5.
  if (operation == DurationOperation::Subtract) {
    other = other.negate();
  }

  Duration result;
  if (relativeTo) {
    if (relativeTo->canUnwrapAs<PlainDateObject>()) {
      Rooted<Wrapped<PlainDateObject*>> relativeToObj(cx, relativeTo);
      if (!AddDuration(cx, duration, other, relativeToObj, &result)) {
        return false;
      }
    } else if (relativeTo->canUnwrapAs<ZonedDateTimeObject>()) {
      Rooted<Wrapped<ZonedDateTimeObject*>> relativeToObj(cx, relativeTo);
      if (!AddDuration(cx, duration, other, relativeToObj, &result)) {
        return false;
      }
    } else {
      MOZ_ASSERT(!IsDeadProxyObject(relativeTo),
                 "ToRelativeTemporalObject doesn't return dead wrappers");
      MOZ_CRASH("expected either PlainDateObject or ZonedDateTimeObject");
    }
  } else {
    if (!AddDuration(cx, duration, other, &result)) {
      return false;
    }
  }

  // Step 6.
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
      if (!UnbalanceDateDurationRelative(cx, one, TemporalUnit::Day, relativeTo,
                                         &unbalanceResult1)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(cx, one, TemporalUnit::Day,
                                         &unbalanceResult1)) {
        return false;
      }
      MOZ_ASSERT(one.date() == unbalanceResult1.toDuration());
    }

    // Step 7.b.
    DateDuration unbalanceResult2;
    if (relativeTo) {
      if (!UnbalanceDateDurationRelative(cx, two, TemporalUnit::Day, relativeTo,
                                         &unbalanceResult2)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(cx, two, TemporalUnit::Day,
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
  auto* durationObj = &args.thisv().toObject().as<DurationObject>();
  auto duration = ToDuration(durationObj);

  // Steps 3-20.
  auto smallestUnit = TemporalUnit::Auto;
  TemporalUnit largestUnit;
  auto roundingMode = TemporalRoundingMode::HalfExpand;
  auto roundingIncrement = Increment{1};
  Rooted<JSObject*> relativeTo(cx);
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Steps 6-12. (Not applicable)

    // Step 13.
    Rooted<JSString*> paramString(cx, args[0].toString());
    if (!GetTemporalUnit(cx, paramString, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 14. (Not applicable)

    // Step 15.
    auto defaultLargestUnit = DefaultTemporalLargestUnit(duration);

    // Step 16.
    defaultLargestUnit = std::min(defaultLargestUnit, smallestUnit);

    // Step 17. (Not applicable)

    // Step 17.a. (Not applicable)

    // Step 17.b.
    largestUnit = defaultLargestUnit;

    // Steps 18-23. (Not applicable)
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

    // Step 10.
    if (!ToRelativeTemporalObject(cx, options, &relativeTo)) {
      return false;
    }

    // Step 11.
    if (!ToTemporalRoundingIncrement(cx, options, &roundingIncrement)) {
      return false;
    }

    // Step 12.
    if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
      return false;
    }

    // Step 13.
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 14.
    if (smallestUnit == TemporalUnit::Auto) {
      // Step 14.a.
      smallestUnitPresent = false;

      // Step 14.b.
      smallestUnit = TemporalUnit::Nanosecond;
    }

    // Step 15.
    auto defaultLargestUnit = DefaultTemporalLargestUnit(duration);

    // Step 16.
    defaultLargestUnit = std::min(defaultLargestUnit, smallestUnit);

    // Steps 17-18.
    if (largestUnitValue.isUndefined()) {
      // Step 17.a.
      largestUnitPresent = false;

      // Step 17.b.
      largestUnit = defaultLargestUnit;
    } else if (largestUnit == TemporalUnit::Auto) {
      // Step 18.a
      largestUnit = defaultLargestUnit;
    }

    // Step 19.
    if (!smallestUnitPresent && !largestUnitPresent) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_DURATION_MISSING_UNIT_SPECIFIER);
      return false;
    }

    // Step 20.
    if (largestUnit > smallestUnit) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_RANGE);
      return false;
    }

    // Steps 21-22.
    if (smallestUnit > TemporalUnit::Day) {
      // Step 21.
      auto maximum = MaximumTemporalDurationRoundingIncrement(smallestUnit);

      // Step 22.
      if (!ValidateTemporalRoundingIncrement(cx, roundingIncrement, maximum,
                                             false)) {
        return false;
      }
    }
  }

  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  if (relativeTo) {
    if (relativeTo->canUnwrapAs<PlainDateObject>()) {
      dateRelativeTo = relativeTo;
    } else if (relativeTo->canUnwrapAs<ZonedDateTimeObject>()) {
      zonedRelativeTo = relativeTo;
    } else {
      MOZ_ASSERT(!IsDeadProxyObject(relativeTo),
                 "ToRelativeTemporalObject doesn't return dead wrappers");
      MOZ_CRASH("expected either PlainDateObject or ZonedDateTimeObject");
    }
  }

  // Step 23.
  DateDuration unbalanceResult;
  if (relativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration, largestUnit, relativeTo,
                                       &unbalanceResult)) {
      return false;
    }
  } else {
    if (!UnbalanceDateDurationRelative(cx, duration, largestUnit,
                                       &unbalanceResult)) {
      return false;
    }
    MOZ_ASSERT(duration.date() == unbalanceResult.toDuration());
  }

  // Step 24.
  Duration roundInput = {
      unbalanceResult.years, unbalanceResult.months, unbalanceResult.weeks,
      unbalanceResult.days,  duration.hours,         duration.minutes,
      duration.seconds,      duration.milliseconds,  duration.microseconds,
      duration.nanoseconds,
  };
  Duration roundResult;
  if (dateRelativeTo) {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, dateRelativeTo, &roundResult)) {
      return false;
    }
  } else if (zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, zonedRelativeTo, &roundResult)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, &roundResult)) {
      return false;
    }
  }

  // FIXME: spec issue - `relativeTo` can be undefined, in which case it's not
  // valid to test for the presence of internal slots.

  // Steps 25-26.
  TimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 25.a.
    Duration adjustResult;
    if (!AdjustRoundedDurationDays(cx, roundResult, roundingIncrement,
                                   smallestUnit, roundingMode, zonedRelativeTo,
                                   &adjustResult)) {
      return false;
    }
    roundResult = adjustResult;

    // Step 25.b.
    if (!BalanceTimeDurationRelative(cx, roundResult, largestUnit,
                                     zonedRelativeTo, &balanceResult)) {
      return false;
    }
  } else {
    // Step 26.a.
    if (!BalanceTimeDuration(cx, roundResult, largestUnit, &balanceResult)) {
      return false;
    }
  }

  // Step 27.
  Duration balanceInput = {
      roundResult.years,
      roundResult.months,
      roundResult.weeks,
      balanceResult.days,
  };
  DateDuration result;
  if (!BalanceDateDurationRelative(cx, balanceInput, largestUnit, relativeTo,
                                   &result)) {
    return false;
  }

  // Step 28.
  auto* obj = CreateTemporalDuration(cx, {
                                             result.years,
                                             result.months,
                                             result.weeks,
                                             result.days,
                                             balanceResult.hours,
                                             balanceResult.minutes,
                                             balanceResult.seconds,
                                             balanceResult.milliseconds,
                                             balanceResult.microseconds,
                                             balanceResult.nanoseconds,
                                         });
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

  // Steps 3-8.
  Rooted<JSObject*> relativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  auto unit = TemporalUnit::Auto;
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Step 7. (Implicit)
    MOZ_ASSERT(!relativeTo);

    // Step 8.
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

    // Steps 6-7.
    if (!ToRelativeTemporalObject(cx, totalOf, &relativeTo)) {
      return false;
    }

    if (relativeTo) {
      if (relativeTo->canUnwrapAs<PlainDateObject>()) {
        dateRelativeTo = relativeTo;
      } else if (relativeTo->canUnwrapAs<ZonedDateTimeObject>()) {
        zonedRelativeTo = relativeTo;
      } else {
        MOZ_ASSERT(!IsDeadProxyObject(relativeTo),
                   "ToRelativeTemporalObject doesn't return dead wrappers");
        MOZ_CRASH("expected either PlainDateObject or ZonedDateTimeObject");
      }
    }

    // Step 7.
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

  // Step 9.
  DateDuration unbalanceResult;
  if (relativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration, unit, relativeTo,
                                       &unbalanceResult)) {
      return false;
    }
  } else {
    if (!UnbalanceDateDurationRelative(cx, duration, unit, &unbalanceResult)) {
      return false;
    }
    MOZ_ASSERT(duration.date() == unbalanceResult.toDuration());
  }

  Duration balanceInput = {
      0,
      0,
      0,
      unbalanceResult.days,
      duration.hours,
      duration.minutes,
      duration.seconds,
      duration.milliseconds,
      duration.microseconds,
      duration.nanoseconds,
  };

  // Steps 10-11.
  TimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 10.a
    Rooted<ZonedDateTimeObject*> intermediate(
        cx, MoveRelativeZonedDateTime(
                cx, zonedRelativeTo,
                {unbalanceResult.years, unbalanceResult.months,
                 unbalanceResult.weeks, 0}));
    if (!intermediate) {
      return false;
    }

    // Step 10.b.
    if (!BalancePossiblyInfiniteTimeDurationRelative(
            cx, balanceInput, unit, intermediate, &balanceResult)) {
      return false;
    }
  } else {
    // Step 11.
    if (!BalancePossiblyInfiniteTimeDuration(cx, balanceInput, unit,
                                             &balanceResult)) {
      return false;
    }
  }

  // Steps 12-13.
  for (double v : {
           balanceResult.days,
           balanceResult.hours,
           balanceResult.minutes,
           balanceResult.seconds,
           balanceResult.milliseconds,
           balanceResult.microseconds,
           balanceResult.nanoseconds,
       }) {
    if (std::isinf(v)) {
      args.rval().setDouble(v);
      return true;
    }
  }
  MOZ_ASSERT(IsValidDuration(balanceResult.toDuration()));

  // Step 14. (Not applicable in our implementation.)

  // Step 15.
  Duration roundInput = {
      unbalanceResult.years,      unbalanceResult.months,
      unbalanceResult.weeks,      balanceResult.days,
      balanceResult.hours,        balanceResult.minutes,
      balanceResult.seconds,      balanceResult.milliseconds,
      balanceResult.microseconds, balanceResult.nanoseconds,
  };
  double roundResult;
  if (zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, zonedRelativeTo,
                         &roundResult)) {
      return false;
    }
  } else if (dateRelativeTo) {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, dateRelativeTo,
                         &roundResult)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, &roundResult)) {
      return false;
    }
  }

  // Steps 16-27.
  args.rval().setNumber(roundResult);
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
