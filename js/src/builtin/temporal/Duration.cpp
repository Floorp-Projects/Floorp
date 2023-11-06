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
#include "proxy/DeadObjectProxy.h"
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
    Handle<Wrapped<PlainDateObject*>> relativeTo, const Duration& duration,
    Handle<Value> dateAdd,
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

static bool MoveRelativeDateLoop(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    Handle<DurationObject*> duration, Handle<Value> dateAdd) {
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, dateRelativeTo.get());
  while (true) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    int32_t ignored;
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, duration, dateAdd,
                          &newRelativeTo, &ignored)) {
      return false;
    }
  }
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
 * TotalDurationNanoseconds ( hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static mozilla::Maybe<int64_t> TotalDurationNanoseconds(
    const Duration& duration) {
  // Our implementation supports |duration.days| to avoid computing |days * 24|
  // in the caller, which may not be representable as a double value.
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

  // Step 1.
  int64_t minutes;
  if (!mozilla::NumberEqualsInt64(duration.minutes, &minutes)) {
    return mozilla::Nothing();
  }
  result *= 60;
  result += minutes;

  // Step 2.
  int64_t seconds;
  if (!mozilla::NumberEqualsInt64(duration.seconds, &seconds)) {
    return mozilla::Nothing();
  }
  result *= 60;
  result += seconds;

  // Step 3.
  int64_t milliseconds;
  if (!mozilla::NumberEqualsInt64(duration.milliseconds, &milliseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += milliseconds;

  // Step 4.
  int64_t microseconds;
  if (!mozilla::NumberEqualsInt64(duration.microseconds, &microseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += microseconds;

  // Step 5.
  int64_t nanoseconds;
  if (!mozilla::NumberEqualsInt64(duration.nanoseconds, &nanoseconds)) {
    return mozilla::Nothing();
  }
  result *= 1000;
  result += nanoseconds;

  // Step 5 (Return).
  if (!result.isValid()) {
    return mozilla::Nothing();
  }
  return mozilla::Some(result.value());
}

/**
 * TotalDurationNanoseconds ( hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
static BigInt* TotalDurationNanosecondsSlow(JSContext* cx,
                                            const Duration& duration) {
  // Our implementation supports |duration.days| to avoid computing |days * 24|
  // in the caller, which may not be representable as a double value.
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

  if (!multiplyAdd(24, duration.hours)) {
    return nullptr;
  }

  // Step 1.
  if (!multiplyAdd(60, duration.minutes)) {
    return nullptr;
  }

  // Step 2.
  if (!multiplyAdd(60, duration.seconds)) {
    return nullptr;
  }

  // Step 3.
  if (!multiplyAdd(1000, duration.milliseconds)) {
    return nullptr;
  }

  // Step 4.
  if (!multiplyAdd(1000, duration.microseconds)) {
    return nullptr;
  }

  // Step 5.
  if (!multiplyAdd(1000, duration.nanoseconds)) {
    return nullptr;
  }

  // Step 5 (Return).
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

  result.set(temporal::NanosecondsAndDays::from(
      days, ToInstantSpan(nanos), InstantSpan::fromNanoseconds(dayLengthNs)));
  return true;
}

/**
 * Split duration into full days and remainding nanoseconds.
 */
static bool NanosecondsToDays(
    JSContext* cx, const Duration& duration,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  if (auto total = TotalDurationNanoseconds(duration.time())) {
    auto nanosAndDays = ::NanosecondsToDays(*total);

    result.set(temporal::NanosecondsAndDays::from(
        nanosAndDays.days,
        InstantSpan::fromNanoseconds(nanosAndDays.nanoseconds),
        InstantSpan::fromNanoseconds(ToNanoseconds(TemporalUnit::Day))));
    return true;
  }

  Rooted<BigInt*> nanoseconds(
      cx, TotalDurationNanosecondsSlow(cx, duration.time()));
  if (!nanoseconds) {
    return false;
  }

  return ::NanosecondsToDaysSlow(cx, nanoseconds, result);
}

/**
 * NanosecondsToDays ( nanoseconds, zonedRelativeTo )
 */
static bool NanosecondsToDays(
    JSContext* cx, const Duration& duration,
    Handle<ZonedDateTimeObject*> zonedRelativeTo,
    MutableHandle<temporal::NanosecondsAndDays> result) {
  if (auto total = TotalDurationNanoseconds(duration.time())) {
    auto nanoseconds = InstantSpan::fromNanoseconds(*total);
    MOZ_ASSERT(IsValidInstantSpan(nanoseconds));

    return NanosecondsToDays(cx, nanoseconds, zonedRelativeTo, result);
  }

  auto* nanoseconds = TotalDurationNanosecondsSlow(cx, duration.time());
  if (!nanoseconds) {
    return false;
  }

  // NanosecondsToDays, step 6.
  if (!IsValidInstantSpan(nanoseconds)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INSTANT_INVALID);
    return false;
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
  // Steps 1-2. (Handled in caller.)

  BigInt* zero = BigInt::zero(cx);
  if (!zero) {
    return false;
  }

  // Step 3.
  Rooted<BigInt*> days(cx, zero);
  Rooted<BigInt*> hours(cx, zero);
  Rooted<BigInt*> minutes(cx, zero);
  Rooted<BigInt*> seconds(cx, zero);
  Rooted<BigInt*> milliseconds(cx, zero);
  Rooted<BigInt*> microseconds(cx, zero);
  Rooted<BigInt*> nanoseconds(cx, nanos);

  // Steps 4-5.
  //
  // We don't need to convert to positive numbers, because BigInt division
  // truncates and BigInt modulo has modulo semantics.

  // Steps 6-12.
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
    // Step 6.
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day: {
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

      // Steps 6.k-l.
      if (!BigInt::divmod(cx, hours, twentyfour, &days, &hours)) {
        return false;
      }

      break;
    }

    // Step 7.
    case TemporalUnit::Hour: {
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

      // Steps 7.i-j.
      if (!BigInt::divmod(cx, minutes, sixty, &hours, &minutes)) {
        return false;
      }

      break;
    }

    // Step 8.
    case TemporalUnit::Minute: {
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

      // Steps 8.g-h.
      if (!BigInt::divmod(cx, seconds, sixty, &minutes, &seconds)) {
        return false;
      }

      break;
    }

    // Step 9.
    case TemporalUnit::Second: {
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

      break;
    }

    // Step 10.
    case TemporalUnit::Millisecond: {
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

      break;
    }

    // Step 11.
    case TemporalUnit::Microsecond: {
      // Steps 11.a-b.
      if (!BigInt::divmod(cx, nanoseconds, thousand, &microseconds,
                          &nanoseconds)) {
        return false;
      }

      break;
    }

    // Step 12.
    case TemporalUnit::Nanosecond: {
      // Nothing to do.
      break;
    }

    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  double daysNumber = BigInt::numberValue(days);
  double hoursNumber = BigInt::numberValue(hours);
  double minutesNumber = BigInt::numberValue(minutes);
  double secondsNumber = BigInt::numberValue(seconds);
  double millisecondsNumber = BigInt::numberValue(milliseconds);
  double microsecondsNumber = BigInt::numberValue(microseconds);
  double nanosecondsNumber = BigInt::numberValue(nanoseconds);

  // Step 13.
  for (double v : {daysNumber, hoursNumber, minutesNumber, secondsNumber,
                   millisecondsNumber, microsecondsNumber, nanosecondsNumber}) {
    if (std::isinf(v)) {
      *result = {
          daysNumber,        hoursNumber,        minutesNumber,
          secondsNumber,     millisecondsNumber, microsecondsNumber,
          nanosecondsNumber,
      };
      return true;
    }
  }

  // Step 14.
  *result = CreateTimeDurationRecord(daysNumber, hoursNumber, minutesNumber,
                                     secondsNumber, millisecondsNumber,
                                     microsecondsNumber, nanosecondsNumber);
  return true;
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
  if (auto oneNanoseconds = TotalDurationNanoseconds(one)) {
    if (auto twoNanoseconds = TotalDurationNanoseconds(two)) {
      mozilla::CheckedInt64 nanoseconds = *oneNanoseconds;
      nanoseconds += *twoNanoseconds;
      if (nanoseconds.isValid()) {
        *result = ::BalanceTimeDuration(nanoseconds.value(), largestUnit);
        return true;
      }
    }
  }

  Rooted<BigInt*> oneNanoseconds(cx, TotalDurationNanosecondsSlow(cx, one));
  if (!oneNanoseconds) {
    return false;
  }

  Rooted<BigInt*> twoNanoseconds(cx, TotalDurationNanosecondsSlow(cx, two));
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
  if (auto oneNanoseconds = TotalDurationNanoseconds(one)) {
    if (auto twoNanoseconds = TotalDurationNanoseconds(two)) {
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

  Rooted<BigInt*> oneNanoseconds(cx, TotalDurationNanosecondsSlow(cx, one));
  if (!oneNanoseconds) {
    return false;
  }

  Rooted<BigInt*> twoNanoseconds(cx, TotalDurationNanosecondsSlow(cx, two));
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
        cx, TotalDurationNanosecondsSlow(cx, {0, 0, 0, days}));
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

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto nanoseconds = TotalDurationNanoseconds(duration)) {
    *result = ::BalanceTimeDuration(*nanoseconds, largestUnit);
    return true;
  }

  // Steps 1-2.
  Rooted<BigInt*> nanoseconds(cx, TotalDurationNanosecondsSlow(cx, duration));
  if (!nanoseconds) {
    return false;
  }

  // Steps 3-14.
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

  // Step 5. (Not applicable in our implementation.)

  // Steps 6-7.
  TimeDuration balanceResult;
  if (auto nanos = nanoseconds.toNanoseconds(); nanos.isValid()) {
    // Step 6.
    balanceResult = ::BalanceTimeDuration(nanos.value(), largestUnit);

    // Step 7.
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

    // Step 7.
    if (!IsValidDuration(balanceResult.toDuration())) {
      *result = balanceResult;
      return true;
    }
  }

  // Step 8.
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

static int64_t ClampToInt64(double num) {
  MOZ_ASSERT(js::IsInteger(num));

  return int64_t(std::clamp(num, double(INT64_MIN), double(INT64_MAX)));
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo )
 */
static bool UnbalanceDateDurationRelativeMonthSlow(
    JSContext* cx, const Duration& duration, double oneYearMonthsToAdd,
    int32_t sign, Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
    Handle<CalendarValue> calendar, Handle<DurationObject*> oneYear,
    Handle<Value> dateAdd, Handle<Value> dateUntil, DateDuration* result) {
  MOZ_ASSERT(sign == -1 || sign == 1);
  MOZ_ASSERT(plainRelativeTo);
  MOZ_ASSERT(calendar);

  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, plainRelativeTo.get());

  Rooted<BigInt*> years(cx, BigInt::createFromDouble(cx, duration.years));
  if (!years) {
    return false;
  }

  Rooted<BigInt*> months(cx, BigInt::createFromDouble(cx, duration.months));
  if (!months) {
    return false;
  }

  // Steps 11.a-d. (Not applicable)

  if (oneYearMonthsToAdd) {
    Rooted<BigInt*> toAdd(cx, BigInt::createFromDouble(cx, oneYearMonthsToAdd));
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

  // Step 11.e.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  Rooted<BigInt*> oneYearMonths(cx);
  while (!years->isZero()) {
    // Step 11.e.i.
    newRelativeTo =
        CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
    if (!newRelativeTo) {
      return false;
    }

    // Steps 11.e.ii-iv.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                           TemporalUnit::Month, dateUntil, &untilResult)) {
      return false;
    }

    // Step 11.e.v.
    oneYearMonths = BigInt::createFromDouble(cx, untilResult.months);
    if (!oneYearMonths) {
      return false;
    }

    // Step 11.e.vi.
    dateRelativeTo = newRelativeTo;

    // Step 11.e.vii.
    if (sign < 0) {
      years = BigInt::inc(cx, years);
    } else {
      years = BigInt::dec(cx, years);
    }
    if (!years) {
      return false;
    }

    // Step 11.e.viii.
    months = BigInt::add(cx, months, oneYearMonths);
    if (!months) {
      return false;
    }
  }

  // Step 11.f.
  return CreateDateDurationRecord(cx, 0, BigInt::numberValue(months),
                                  duration.weeks, duration.days, result);
}

static bool UnbalanceDateDurationRelativeHasEffect(const Duration& duration,
                                                   TemporalUnit largestUnit) {
  MOZ_ASSERT(largestUnit != TemporalUnit::Auto);

  // Steps 1-3, 11.a, 12.a, and 14.
  return (largestUnit > TemporalUnit::Year && duration.years != 0) ||
         (largestUnit > TemporalUnit::Month && duration.months != 0) ||
         (largestUnit > TemporalUnit::Week && duration.weeks != 0);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo )
 */
static bool UnbalanceDateDurationRelative(
    JSContext* cx, const Duration& duration, TemporalUnit largestUnit,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo, DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // FIXME: spec issue - any |days| value is actually okay and doesn't require
  // a calendar to be present.

  // Steps 1-3, 11.a, 12.a, and 14.
  if (!UnbalanceDateDurationRelativeHasEffect(duration, largestUnit)) {
    // Steps 3.a, 11.a, 12.a, and 14.
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
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, plainRelativeTo.get());
  auto* date = dateRelativeTo.unwrap(cx);
  if (!date) {
    return false;
  }

  Rooted<CalendarValue> calendar(cx, date->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 10. (Not applicable)

  // Step 11.
  if (largestUnit == TemporalUnit::Month) {
    // Step 11.a. (Handled above)
    MOZ_ASSERT(years != 0);

    // Step 11.b. (Not applicable in our implementation.)

    // Steps 11.c-d.
    Rooted<Value> dateAdd(cx);
    Rooted<Value> dateUntil(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());

      // Step 11.c.
      if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }

      // Step 11.d.
      if (!GetMethod(cx, calendarObj, cx->names().dateUntil, &dateUntil)) {
        return false;
      }
    }

    // Go to the slow path when the result is inexact.
    if (MOZ_UNLIKELY(!IsSafeInteger(months))) {
      return UnbalanceDateDurationRelativeMonthSlow(
          cx, {years, months, weeks, days}, 0, sign, dateRelativeTo, calendar,
          oneYear, dateAdd, dateUntil, result);
    }

    // Step 11.e.
    //
    // Clamp to int64 because too large values will trigger timeouts anyway.
    int64_t intYears = ClampToInt64(years);
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    while (intYears != 0) {
      // Step 11.e.i.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 11.e.ii-iv.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 11.e.v.
      double oneYearMonths = untilResult.months;

      // Step 11.e.vi.
      dateRelativeTo = newRelativeTo;

      // Go to the slow path when the result is inexact.
      if (MOZ_UNLIKELY(!IsSafeInteger(months + oneYearMonths))) {
        return UnbalanceDateDurationRelativeMonthSlow(
            cx, {double(intYears), months, weeks, days}, oneYearMonths, sign,
            dateRelativeTo, calendar, oneYear, dateAdd, dateUntil, result);
      }

      // Step 11.e.vii.
      intYears -= sign;

      // Step 11.e.viii.
      months += oneYearMonths;
    }

    // Step 11.f.
    return CreateDateDurationRecord(cx, 0, months, weeks, days, result);
  }

  // Step 12.
  if (largestUnit == TemporalUnit::Week) {
    // Step 12.a. (Handled above)
    MOZ_ASSERT(years != 0 || months != 0);

    // Step 12.b. (Not applicable in our implementation.)

    // Steps 12.c-d.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // Sum up all days to add to avoid imprecise floating-point arithmetic.
    int32_t daysToAdd = 0;

    // Step 12.e.
    //
    // Clamp to int64 because too large values will trigger timeouts anyway.
    int64_t intYears = ClampToInt64(years);
    while (intYears != 0) {
      // Steps 12.e.i-ii.
      int32_t oneYearDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &dateRelativeTo, &oneYearDays)) {
        return false;
      }

      // Step 12.e.iii.
      daysToAdd += oneYearDays;
      MOZ_ASSERT(std::abs(daysToAdd) <= 200'000'000);

      // Step 12.e.iv.
      intYears -= sign;
    }

    // Step 12.f.
    //
    // Clamp to int64 because too large values will trigger timeouts anyway.
    int64_t intMonths = ClampToInt64(months);
    while (intMonths != 0) {
      // Steps 12.f.i-ii.
      int32_t oneMonthDays;
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &dateRelativeTo, &oneMonthDays)) {
        return false;
      }

      // Step 12.f.iii.
      daysToAdd += oneMonthDays;
      MOZ_ASSERT(std::abs(daysToAdd) <= 200'000'000);

      // Step 12.f.iv.
      intMonths -= sign;
    }

    // Step 12.g.
    //
    // The addition |days + daysToAdd| can be imprecise, but this is safe to
    // ignore, because all values are passed to CreateDateDurationRecord, which
    // converts the values to Numbers.
    return CreateDateDurationRecord(cx, 0, 0, weeks, days + double(daysToAdd),
                                    result);
  }

  // FIXME: spec bug - incorrect assertion
  // https://github.com/tc39/proposal-temporal/issues/2695

  // Step 13.
  // MOZ_ASSERT(largestUnit == TemporalUnit::Day);

  // Step 14. (Handled above)
  MOZ_ASSERT(years != 0 || months != 0 || weeks != 0);

  // FIXME: why don't we unconditionally throw an error for missing calendars?

  // Step 15. (Not applicable in our implementation.)

  // Steps 16-17.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethod(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Sum up all days to add to avoid imprecise floating-point arithmetic.
  int32_t daysToAdd = 0;

  // Step 18.
  //
  // Clamp to int64 because too large values will trigger timeouts anyway.
  int64_t intYears = ClampToInt64(years);
  while (intYears != 0) {
    // Steps 18.a-b.
    int32_t oneYearDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                          &dateRelativeTo, &oneYearDays)) {
      return false;
    }

    // Step 18.c.
    daysToAdd += oneYearDays;
    MOZ_ASSERT(std::abs(daysToAdd) <= 200'000'000);

    // Step 18.d.
    intYears -= sign;
  }

  // Step 19.
  //
  // Clamp to int64 because too large values will trigger timeouts anyway.
  int64_t intMonths = ClampToInt64(months);
  while (intMonths != 0) {
    // Steps 19.a-b.
    int32_t oneMonthDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                          &dateRelativeTo, &oneMonthDays)) {
      return false;
    }

    // Step 19.c.
    daysToAdd += oneMonthDays;
    MOZ_ASSERT(std::abs(daysToAdd) <= 200'000'000);

    // Step 19.d.
    intMonths -= sign;
  }

  // Step 20.
  //
  // Clamp to int64 because too large values will trigger timeouts anyway.
  int64_t intWeeks = ClampToInt64(weeks);
  while (intWeeks != 0) {
    // Steps 20.a-b.
    int32_t oneWeekDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                          &dateRelativeTo, &oneWeekDays)) {
      return false;
    }

    // Step 20.c.
    daysToAdd += oneWeekDays;
    MOZ_ASSERT(std::abs(daysToAdd) <= 200'000'000);

    // Step 20.d.
    intWeeks -= sign;
  }

  // Step 21.
  //
  // The addition |days + daysToAdd| can be imprecise, but this is safe to
  // ignore, because all values are passed to CreateDateDurationRecord, which
  // converts the values to Numbers.
  return CreateDateDurationRecord(cx, 0, 0, 0, days + double(daysToAdd),
                                  result);
}

/**
 * UnbalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo )
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

  // Steps 1-3, 11.a, 12.a, and 14.
  if (!UnbalanceDateDurationRelativeHasEffect(duration, largestUnit)) {
    // Steps 3.a, 11.a, 12.a, and 14.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Steps 4-10. (Not applicable in our implementation.)

  // Steps 11.b, 12.b, and 15.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_DURATION_UNCOMPARABLE, "calendar");
  return false;
}

static bool BalanceDateDurationRelativeYearSlow(
    JSContext* cx, const Duration& duration, int32_t sign,
    Handle<Wrapped<PlainDateObject*>> relativeTo,
    Handle<CalendarValue> calendar, Handle<DurationObject*> oneYear,
    Handle<Value> dateAdd, Handle<Value> dateUntil, uint32_t yearsToAdd,
    uint32_t monthsToAdd, double oneYearMonthsNumber, DateDuration* result) {
  MOZ_ASSERT(sign == -1 || sign == 1);

  Rooted<BigInt*> months(cx, BigInt::createFromDouble(cx, duration.months));
  if (!months) {
    return false;
  }

  if (monthsToAdd) {
    Rooted<BigInt*> toAdd(cx, BigInt::createFromInt64(cx, monthsToAdd));
    if (!toAdd) {
      return false;
    }

    if (sign < 0) {
      toAdd = BigInt::neg(cx, toAdd);
      if (!toAdd) {
        return false;
      }
    }

    months = BigInt::add(cx, months, toAdd);
    if (!months) {
      return false;
    }
  }

  Rooted<BigInt*> oneYearMonths(
      cx, BigInt::createFromDouble(cx, oneYearMonthsNumber));
  if (!oneYearMonths) {
    return false;
  }

  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, relativeTo.get());
  while (BigInt::absoluteCompare(months, oneYearMonths) >= 0) {
    // Step 11.r.i.
    months = BigInt::sub(cx, months, oneYearMonths);
    if (!months) {
      return false;
    }

    // Step 11.r.ii. (Partial)
    yearsToAdd += 1;

    // Step 11.r.iii.
    dateRelativeTo = newRelativeTo;

    // Step 11.r.iv.
    newRelativeTo =
        CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
    if (!newRelativeTo) {
      return false;
    }

    // Steps 11.r.v-vii.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                           TemporalUnit::Month, dateUntil, &untilResult)) {
      return false;
    }

    // Step 11.r.viii.
    oneYearMonths = BigInt::createFromDouble(cx, untilResult.months);
    if (!oneYearMonths) {
      return false;
    }
  }

  // Step 11.f.ii and 11.r.ii.
  double years = duration.years + double(yearsToAdd) * sign;

  // Step 14.
  *result = CreateDateDurationRecord(years, BigInt::numberValue(months),
                                     duration.weeks, duration.days);
  return true;
}

/**
 * BalanceDateDurationRelative ( years, months, weeks, days, largestUnit,
 * plainRelativeTo )
 */
static bool BalanceDateDurationRelative(
    JSContext* cx, const Duration& duration, TemporalUnit largestUnit,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo, DateDuration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;
  double days = duration.days;

  // FIXME: spec issue - effectful code paths should be more fine-grained
  // similar to UnbalanceDateDurationRelative. For example:
  // 1. If largestUnit = "year" and days = 0 and months = 0, then no-op.
  // 2. Else if largestUnit = "month" and days = 0, then no-op.
  // 3. Else if days = 0, then no-op.
  //
  // Also note that |weeks| is never balanced, even when non-zero.

  // Steps 1-3.
  if (largestUnit > TemporalUnit::Week ||
      (years == 0 && months == 0 && weeks == 0 && days == 0)) {
    // Step 3.a.
    *result = CreateDateDurationRecord(years, months, weeks, days);
    return true;
  }

  // Step 4.
  if (!plainRelativeTo) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_DURATION_UNCOMPARABLE,
                              "relativeTo");
    return false;
  }

  // Step 5.
  int32_t sign = DurationSign({years, months, weeks, days});

  // Step 6.
  MOZ_ASSERT(sign != 0);

  // Step 7.
  Rooted<DurationObject*> oneYear(cx,
                                  CreateTemporalDuration(cx, {double(sign)}));
  if (!oneYear) {
    return false;
  }

  // Step 8.
  Rooted<DurationObject*> oneMonth(
      cx, CreateTemporalDuration(cx, {0, double(sign)}));
  if (!oneMonth) {
    return false;
  }

  // Step 9.
  Rooted<DurationObject*> oneWeek(
      cx, CreateTemporalDuration(cx, {0, 0, double(sign)}));
  if (!oneWeek) {
    return false;
  }

  // Step 10.
  Rooted<Wrapped<PlainDateObject*>> dateRelativeTo(cx, plainRelativeTo.get());
  auto* date = plainRelativeTo.unwrap(cx);
  if (!date) {
    return false;
  }

  Rooted<CalendarValue> calendar(cx, date->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Steps 11-13.
  if (largestUnit == TemporalUnit::Year) {
    // Steps 11.a-b.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // The loop condition is always true for too large |days| values.
    if (MOZ_UNLIKELY(std::abs(days) >= epochDays * 2)) {
      // Steps 11.c-e and 11.f.iv-vi.
      return MoveRelativeDateLoop(cx, calendar, dateRelativeTo, oneYear,
                                  dateAdd);
    }

    // Otherwise |days| is representable as an int32 value.
    int32_t intDays = int32_t(days);

    // Steps 11.c-e.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneYearDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                          &newRelativeTo, &oneYearDays)) {
      return false;
    }

    // Sum up all days to subtract.
    int32_t daysToSubtract = 0;

    // Sum up all years to add to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t yearsToAdd = 0;

    // Step 11.f.
    while (std::abs(intDays - daysToSubtract) >= std::abs(oneYearDays)) {
      // Step 11.f.i.
      daysToSubtract += oneYearDays;
      MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

      // Step 11.f.ii. (Partial)
      yearsToAdd += 1;

      // Step 11.f.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 11.f.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneYear, dateAdd,
                            &newRelativeTo, &oneYearDays)) {
        return false;
      }
    }

    // Steps 11.g-i.
    int32_t oneMonthDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }

    // Sum up all months to add to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t monthsToAdd = 0;

    // Step 11.j.
    while (std::abs(intDays - daysToSubtract) >= std::abs(oneMonthDays)) {
      // Step 11.j.i.
      daysToSubtract += oneMonthDays;
      MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

      // Step 11.j.ii.
      monthsToAdd += 1;

      // Step 11.j.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 11.j.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &newRelativeTo, &oneMonthDays)) {
        return false;
      }
    }

    // Adjust |days| by |daysToSubtract|.
    days = double(intDays - daysToSubtract);

    // Step 11.k.
    newRelativeTo =
        CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
    if (!newRelativeTo) {
      return false;
    }

    // Steps 11.l-m.
    Rooted<Value> dateUntil(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateUntil,
                            &dateUntil)) {
        return false;
      }
    }

    // Steps 11.n-p.
    Duration untilResult;
    if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                           TemporalUnit::Month, dateUntil, &untilResult)) {
      return false;
    }

    // Step 11.q.
    double oneYearMonths = untilResult.months;

    if (MOZ_UNLIKELY(!IsSafeInteger(months + double(monthsToAdd) * sign))) {
      return BalanceDateDurationRelativeYearSlow(
          cx, {years, months, weeks, days}, sign, newRelativeTo, calendar,
          oneYear, dateAdd, dateUntil, yearsToAdd, monthsToAdd, oneYearMonths,
          result);
    }

    months += double(monthsToAdd) * sign;

    // Step 11.r.
    while (std::abs(months) >= std::abs(oneYearMonths)) {
      if (MOZ_UNLIKELY(!IsSafeInteger(months - oneYearMonths))) {
        // |monthsToAdd| was already handled above, so pass zero here.
        constexpr uint32_t zeroMonthsToAdd = 0;

        return BalanceDateDurationRelativeYearSlow(
            cx, {years, months, weeks, days}, sign, newRelativeTo, calendar,
            oneYear, dateAdd, dateUntil, yearsToAdd, zeroMonthsToAdd,
            oneYearMonths, result);
      }

      // Step 11.r.i.
      months -= oneYearMonths;

      // Step 11.r.ii. (Partial)
      yearsToAdd += 1;

      // Step 11.r.iii.
      dateRelativeTo = newRelativeTo;

      // Step 11.r.iv.
      newRelativeTo =
          CalendarDateAdd(cx, calendar, dateRelativeTo, oneYear, dateAdd);
      if (!newRelativeTo) {
        return false;
      }

      // Steps 11.r.v-vii.
      Duration untilResult;
      if (!CalendarDateUntil(cx, calendar, dateRelativeTo, newRelativeTo,
                             TemporalUnit::Month, dateUntil, &untilResult)) {
        return false;
      }

      // Step 11.r.viii.
      oneYearMonths = untilResult.months;
    }

    // Step 11.f.ii and 11.r.ii.
    years += double(yearsToAdd) * sign;
  } else if (largestUnit == TemporalUnit::Month) {
    // Steps 12.a-b.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // The loop condition is always true for too large |days| values.
    if (MOZ_UNLIKELY(std::abs(days) >= epochDays * 2)) {
      // Steps 12.c-e and 12.f.iv-vi.
      return MoveRelativeDateLoop(cx, calendar, dateRelativeTo, oneMonth,
                                  dateAdd);
    }

    // Otherwise |days| is representable as an int32 value.
    int32_t intDays = int32_t(days);

    // Steps 12.c-e.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneMonthDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }

    // Sum up all days to subtract.
    int32_t daysToSubtract = 0;

    // Sum up all months to add to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t monthsToAdd = 0;

    // Step 12.f.
    while (std::abs(intDays - daysToSubtract) >= std::abs(oneMonthDays)) {
      // Step 12.f.i.
      daysToSubtract += oneMonthDays;
      MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

      // Step 12.f.ii. (Partial)
      monthsToAdd += 1;

      // Step 12.f.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 12.f.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneMonth, dateAdd,
                            &newRelativeTo, &oneMonthDays)) {
        return false;
      }
    }

    // Adjust |days| by |daysToSubtract|.
    days = double(intDays - daysToSubtract);

    // Step 12.f.ii.
    months += double(monthsToAdd) * sign;
  } else {
    // Step 13.a.
    MOZ_ASSERT(largestUnit == TemporalUnit::Week);

    // Steps 13.b-c.
    Rooted<Value> dateAdd(cx);
    if (calendar.isObject()) {
      Rooted<JSObject*> calendarObj(cx, calendar.toObject());
      if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
        return false;
      }
    }

    // The loop condition is always true for too large |days| values.
    if (MOZ_UNLIKELY(std::abs(days) >= epochDays * 2)) {
      // Steps 13.d-f and 13.g.iv-vi.
      return MoveRelativeDateLoop(cx, calendar, dateRelativeTo, oneWeek,
                                  dateAdd);
    }

    // Otherwise |days| is representable as an int32 value.
    int32_t intDays = int32_t(days);

    // Steps 13.d-f.
    Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
    int32_t oneWeekDays;
    if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                          &newRelativeTo, &oneWeekDays)) {
      return false;
    }

    // Sum up all days to subtract.
    int32_t daysToSubtract = 0;

    // Sum up all weeks to add to avoid imprecise floating-point arithmetic.
    // Uint32 overflows can be safely ignored, because they take too long to
    // happen in practice.
    uint32_t weeksToAdd = 0;

    // Step 13.g.
    while (std::abs(intDays - daysToSubtract) >= std::abs(oneWeekDays)) {
      // Step 13.g.i.
      daysToSubtract += oneWeekDays;
      MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

      // Step 13.g.ii. (Partial)
      weeksToAdd += 1;

      // Step 13.g.iii.
      dateRelativeTo = newRelativeTo;

      // Steps 13.g.iv-vi.
      if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                            &newRelativeTo, &oneWeekDays)) {
        return false;
      }
    }

    // Adjust |days| by |daysToSubtract|.
    days = double(intDays - daysToSubtract);

    // Step 13.g.ii.
    weeks += double(weeksToAdd) * sign;
  }

  // Step 14.
  *result = CreateDateDurationRecord(years, months, weeks, days);
  return true;
}

/**
 * AddDuration ( y1, mon1, w1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, w2,
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, zonedRelativeTo )
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
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, zonedRelativeTo )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
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

  // Step 5.a. (Not applicable in our implementation.)

  // Step 5.b.
  auto* unwrappedRelativeTo = plainRelativeTo.unwrap(cx);
  if (!unwrappedRelativeTo) {
    return false;
  }

  Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 5.c.
  auto dateDuration1 = one.date();

  // Step 5.d.
  auto dateDuration2 = two.date();

  // Steps 5.e-f.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 5.g.
  Rooted<Wrapped<PlainDateObject*>> intermediate(
      cx,
      CalendarDateAdd(cx, calendar, plainRelativeTo, dateDuration1, dateAdd));
  if (!intermediate) {
    return false;
  }

  // Step 5.h.
  Rooted<Wrapped<PlainDateObject*>> end(
      cx, CalendarDateAdd(cx, calendar, intermediate, dateDuration2, dateAdd));
  if (!end) {
    return false;
  }

  // Step 5.i.
  auto dateLargestUnit = std::min(TemporalUnit::Day, largestUnit);

  // Steps 5.j-l.
  Duration dateDifference;
  if (!CalendarDateUntil(cx, calendar, plainRelativeTo, end, dateLargestUnit,
                         &dateDifference)) {
    return false;
  }

  // Step 5.m.
  TimeDuration result;
  if (!BalanceTimeDuration(cx, dateDifference.days, one.time(), two.time(),
                           largestUnit, &result)) {
    return false;
  }

  // Steps 5.n.
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
 * d2, h2, min2, s2, ms2, mus2, ns2, plainRelativeTo, zonedRelativeTo )
 */
static bool AddDuration(JSContext* cx, const Duration& one, const Duration& two,
                        Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
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
  auto* unwrappedRelativeTo = zonedRelativeTo.unwrap(cx);
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
 * roundingMode, zonedRelativeTo )
 */
static bool AdjustRoundedDurationDaysSlow(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
    InstantSpan dayLength, Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));
  MOZ_ASSERT(IsValidInstantSpan(dayLength));

  // Step 2.
  Rooted<BigInt*> timeRemainderNs(
      cx, TotalDurationNanosecondsSlow(cx, duration.time()));
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
                   {0, 0, 0, double(direction)}, zonedRelativeTo,
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
 * roundingMode, zonedRelativeTo )
 */
bool js::temporal::AdjustRoundedDurationDays(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo, Duration* result) {
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

  auto* unwrappedRelativeTo = zonedRelativeTo.unwrap(cx);
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
  auto timeRemainderNs = TotalDurationNanoseconds(duration.time());
  if (!timeRemainderNs) {
    return AdjustRoundedDurationDaysSlow(cx, duration, increment, unit,
                                         roundingMode, zonedRelativeTo,
                                         dayLength, result);
  }

  // Step 9.
  auto checkedOneDayLess = *timeRemainderNs - dayLength.toNanoseconds();
  if (!checkedOneDayLess.isValid()) {
    return AdjustRoundedDurationDaysSlow(cx, duration, increment, unit,
                                         roundingMode, zonedRelativeTo,
                                         dayLength, result);
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
                   {0, 0, 0, double(direction)}, zonedRelativeTo,
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
 * seconds, milliseconds, microseconds, nanoseconds, precision )
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

      // Step 20.b. (Reordered)
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

      // Step 20.b. (Reordered)
      fraction = milli * 1'000'000 + micro * 1'000 + nano;
      MOZ_ASSERT(0 <= fraction && fraction < 1'000'000'000);
    }
  }

  // Steps 8 and 13.
  JSStringBuilder result(cx);

  // Step 1. (Reordered)
  int32_t sign = DurationSign(duration);

  // Step 21. (Reordered)
  if (sign < 0) {
    if (!result.append('-')) {
      return nullptr;
    }
  }

  // Step 22. (Reordered)
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

  // Steps 16-17.
  bool nonzeroSecondsAndLower = seconds != 0 || milliseconds != 0 ||
                                microseconds != 0 || nanoseconds != 0;
  MOZ_ASSERT(nonzeroSecondsAndLower ==
             (totalSeconds != 0 ||
              (totalSecondsBigInt && !totalSecondsBigInt->isZero()) ||
              fraction != 0));

  // Steps 18-19.
  bool zeroMinutesAndHigher = years == 0 && months == 0 && weeks == 0 &&
                              days == 0 && hours == 0 && minutes == 0;

  // Step 20. (if-condition)
  bool hasSecondsPart = nonzeroSecondsAndLower || zeroMinutesAndHigher ||
                        precision != Precision::Auto();

  if (hours != 0 || minutes != 0 || hasSecondsPart) {
    // Step 23. (Reordered)
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

    // Step 20.
    if (hasSecondsPart) {
      // Step 20.a.
      if (totalSecondsBigInt) {
        if (!BigIntToStringBuilder(cx, totalSecondsBigInt, result)) {
          return nullptr;
        }
      } else {
        if (!NumberToStringBuilder(cx, totalSeconds, result)) {
          return nullptr;
        }
      }

      // Step 20.b. (Moved above)

      // Step 20.c.
      if (!FormatFractionalSeconds(result, fraction, precision)) {
        return nullptr;
      }

      // Step 20.d.
      if (!result.append('S')) {
        return nullptr;
      }
    }
  }

  // Step 24.
  return result.finishString();
}

/**
 * ToRelativeTemporalObject ( options )
 */
static bool ToRelativeTemporalObject(
    JSContext* cx, Handle<JSObject*> options,
    MutableHandle<Wrapped<PlainDateObject*>> plainRelativeTo,
    MutableHandle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo) {
  // Step 1.
  Rooted<Value> value(cx);
  if (!GetProperty(cx, options, options, cx->names().relativeTo, &value)) {
    return false;
  }

  // Step 2.
  if (value.isUndefined()) {
    // FIXME: spec issue - switch return record fields for consistency.

    plainRelativeTo.set(nullptr);
    zonedRelativeTo.set(nullptr);
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
    if (obj->canUnwrapAs<ZonedDateTimeObject>()) {
      plainRelativeTo.set(nullptr);
      zonedRelativeTo.set(obj);
      return true;
    }

    // Step 5.b.
    if (obj->canUnwrapAs<PlainDateObject>()) {
      plainRelativeTo.set(obj);
      zonedRelativeTo.set(nullptr);
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
      zonedRelativeTo.set(nullptr);
      return true;
    }

    // Step 5.d.
    if (!GetTemporalCalendarWithISODefault(cx, obj, &calendar)) {
      return false;
    }

    // Step 5.e.
    JS::RootedVector<PropertyKey> fieldNames(cx);
    if (!CalendarFields(cx, calendar,
                        {CalendarField::Day, CalendarField::Month,
                         CalendarField::MonthCode, CalendarField::Year},
                        &fieldNames)) {
      return false;
    }

    // Step 5.f.
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

    // Step 5.g.
    Rooted<PlainObject*> fields(cx, PrepareTemporalFields(cx, obj, fieldNames));
    if (!fields) {
      return false;
    }

    // Step 5.h.
    Rooted<PlainObject*> dateOptions(cx, NewPlainObjectWithProto(cx, nullptr));
    if (!dateOptions) {
      return false;
    }

    // Step 5.i.
    Rooted<Value> overflow(cx, StringValue(cx->names().constrain));
    if (!DefineDataProperty(cx, dateOptions, cx->names().overflow, overflow)) {
      return false;
    }

    // Step 5.j.
    if (!InterpretTemporalDateTimeFields(cx, calendar, fields, dateOptions,
                                         &dateTime)) {
      return false;
    }

    // Step 5.k.
    Rooted<Value> offset(cx);
    if (!GetProperty(cx, fields, fields, cx->names().offset, &offset)) {
      return false;
    }

    // Step 5.l.
    Rooted<Value> timeZoneValue(cx);
    if (!GetProperty(cx, fields, fields, cx->names().timeZone,
                     &timeZoneValue)) {
      return false;
    }

    // Step 5.m.
    if (!timeZoneValue.isUndefined()) {
      if (!ToTemporalTimeZone(cx, timeZoneValue, &timeZone)) {
        return false;
      }
    }

    // Step 5.n.
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
    zonedRelativeTo.set(nullptr);
    return true;
  }

  // Steps 8-9. (Moved above)

  // Step 10.
  Instant epochNanoseconds;
  if (!InterpretISODateTimeOffset(cx, dateTime, offsetBehaviour, offsetNs,
                                  timeZone, TemporalDisambiguation::Compatible,
                                  TemporalOffset::Reject, matchBehaviour,
                                  &epochNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(IsValidEpochInstant(epochNanoseconds));

  // Step 11.
  auto* zonedDateTime =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!zonedDateTime) {
    return false;
  }

  plainRelativeTo.set(nullptr);
  zonedRelativeTo.set(zonedDateTime);
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

/**
 * RoundNumberToIncrement ( x, increment, roundingMode )
 */
static bool TruncateNumber(JSContext* cx, const Duration& toRound,
                           TemporalUnit unit, double* quotient, double* total) {
  MOZ_ASSERT(unit >= TemporalUnit::Day);

  int64_t denominator = ToNanoseconds(unit);
  MOZ_ASSERT(denominator > 0);
  MOZ_ASSERT(denominator <= 86'400'000'000'000);

  // Fast-path when we can perform the whole computation with int64 values.
  if (auto numerator = TotalDurationNanoseconds(toRound)) {
    TruncateNumber(*numerator, denominator, quotient, total);
    return true;
  }

  Rooted<BigInt*> numerator(cx, TotalDurationNanosecondsSlow(cx, toRound));
  if (!numerator) {
    return false;
  }

  // Division by one has no remainder.
  if (denominator == 1) {
    double q = BigInt::numberValue(numerator);
    *quotient = q;
    *total = q;
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
  *total = q + BigInt::numberValue(rem) / double(denominator);
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
  if (auto total = TotalDurationNanoseconds(toRound)) {
    return RoundNumberToIncrement(cx, *total, unit, increment, roundingMode,
                                  result);
  }

  Rooted<BigInt*> totalNs(cx, TotalDurationNanosecondsSlow(cx, toRound));
  if (!totalNs) {
    return false;
  }

  return RoundNumberToIncrement(cx, totalNs, unit, increment, roundingMode,
                                result);
}

struct RoundedDuration final {
  Duration duration;
  double total = 0;
};

enum class ComputeRemainder : bool { No, Yes };

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
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

  // Steps 1-2. (Not applicable.)

  // Step 3.
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

  // Step 4. (Not applicable.)

  // Step 5. (Moved below.)

  // Step 6. (Not applicable.)

  // Steps 7-16.
  Duration toRound;
  double* roundedTime;
  switch (unit) {
    case TemporalUnit::Auto:
    case TemporalUnit::Year:
    case TemporalUnit::Week:
    case TemporalUnit::Month:
      // Steps 7-9. (Not applicable.)
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

      // Steps 4.a, 4.c, and 10.a-b.
      toRound = duration;
      roundedTime = &days;

      // Step 4.b. (Not applicable)

      // Steps 4.d-e.
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

      // Steps 5 and 11.a-c.
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

      // Step 11.d.
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

      // Steps 5 and 12.a-c.
      toRound = {
          0,           0, 0, 0, 0, minutes, seconds, milliseconds, microseconds,
          nanoseconds,
      };
      roundedTime = &minutes;

      // Step 12.d.
      seconds = 0;
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Second: {
      MOZ_ASSERT(increment <= Increment{60},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 5 and 13.a-b.
      toRound = {
          0, 0, 0, 0, 0, 0, seconds, milliseconds, microseconds, nanoseconds,
      };
      roundedTime = &seconds;

      // Step 13.c.
      milliseconds = 0;
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Millisecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 14.a-c.
      toRound = {0, 0, 0, 0, 0, 0, 0, milliseconds, microseconds, nanoseconds};
      roundedTime = &milliseconds;

      // Step 14.d.
      microseconds = 0;
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Microsecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Steps 15.a-c.
      toRound = {0, 0, 0, 0, 0, 0, 0, 0, microseconds, nanoseconds};
      roundedTime = &microseconds;

      // Step 15.d.
      nanoseconds = 0;
      break;
    }

    case TemporalUnit::Nanosecond: {
      MOZ_ASSERT(increment <= Increment{1000},
                 "limited by MaximumTemporalDurationRoundingIncrement");

      // Step 16.a. (Implicit)

      // Steps 16.b-c.
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

  double total = 0;
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

    if (!TruncateNumber(cx, toRound, unit, roundedTime, &total)) {
      return false;
    }
  }

  MOZ_ASSERT(years == duration.years);
  MOZ_ASSERT(months == duration.months);
  MOZ_ASSERT(weeks == duration.weeks);
  MOZ_ASSERT(IsIntegerOrInfinity(days));

  // Step 17.
  Duration resultDuration = {years,        months,     weeks,   days,
                             hours,        minutes,    seconds, milliseconds,
                             microseconds, nanoseconds};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 18.
  *result = {resultDuration, total};
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
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

  *result = rounded.total;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
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
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool js::temporal::RoundDuration(JSContext* cx, const Duration& duration,
                                 Increment increment, TemporalUnit unit,
                                 TemporalRoundingMode roundingMode,
                                 Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  return ::RoundDuration(cx, duration, increment, unit, roundingMode, result);
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

static bool TruncateDays(JSContext* cx,
                         Handle<temporal::NanosecondsAndDays> nanosAndDays,
                         double days, int32_t monthsWeeksInDays,
                         double* result) {
  double extraDays = nanosAndDays.daysNumber();

  do {
    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    int64_t intExtraDays;
    if (!mozilla::NumberEqualsInt64(extraDays, &intExtraDays)) {
      break;
    }

    auto totalDays = mozilla::CheckedInt64(intDays);
    totalDays += intExtraDays;
    totalDays += monthsWeeksInDays;
    if (!totalDays.isValid()) {
      break;
    }

    int64_t truncatedDays = totalDays.value();
    if (nanosAndDays.nanoseconds() > InstantSpan{}) {
      // Round toward positive infinity when the integer days are negative and
      // the fractional part is positive.
      if (truncatedDays < 0) {
        truncatedDays += 1;
      }
    } else if (nanosAndDays.nanoseconds() < InstantSpan{}) {
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

  Rooted<BigInt*> biExtraDays(cx, BigInt::createFromDouble(cx, extraDays));
  if (!biExtraDays) {
    return false;
  }

  Rooted<BigInt*> biMonthsWeeksInDays(
      cx, BigInt::createFromInt64(cx, monthsWeeksInDays));
  if (!biMonthsWeeksInDays) {
    return false;
  }

  Rooted<BigInt*> truncatedDays(cx, BigInt::add(cx, biDays, biExtraDays));
  if (!truncatedDays) {
    return false;
  }

  truncatedDays = BigInt::add(cx, truncatedDays, biMonthsWeeksInDays);
  if (!truncatedDays) {
    return false;
  }

  if (nanosAndDays.nanoseconds() > InstantSpan{}) {
    // Round toward positive infinity when the integer days are negative and
    // the fractional part is positive.
    if (truncatedDays->isNegative()) {
      truncatedDays = BigInt::inc(cx, truncatedDays);
      if (!truncatedDays) {
        return false;
      }
    }
  } else if (nanosAndDays.nanoseconds() < InstantSpan{}) {
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

static bool RoundDurationYearSlow(
    JSContext* cx, const Duration& duration, double yearsPassed,
    int32_t monthsWeeksInDays, int32_t daysPassed,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t oneYearDays,
    Increment increment, TemporalRoundingMode roundingMode,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());

  Rooted<BigInt*> years(cx, BigInt::createFromDouble(cx, duration.years));
  if (!years) {
    return false;
  }

  Rooted<BigInt*> biYearsPassed(cx, BigInt::createFromDouble(cx, yearsPassed));
  if (!biYearsPassed) {
    return false;
  }

  years = BigInt::add(cx, years, biYearsPassed);
  if (!years) {
    return false;
  }

  Rooted<BigInt*> days(cx, BigInt::createFromDouble(cx, duration.days));
  if (!days) {
    return false;
  }

  Rooted<BigInt*> extraDays(
      cx, BigInt::createFromDouble(cx, nanosAndDays.daysNumber()));
  if (!extraDays) {
    return false;
  }

  Rooted<BigInt*> biMonthsWeeksInDays(
      cx, BigInt::createFromInt64(cx, monthsWeeksInDays));
  if (!biMonthsWeeksInDays) {
    return false;
  }

  Rooted<BigInt*> biDaysPassed(cx, BigInt::createFromInt64(cx, daysPassed));
  if (!biDaysPassed) {
    return false;
  }

  days = BigInt::add(cx, days, extraDays);
  if (!days) {
    return false;
  }

  days = BigInt::add(cx, days, biMonthsWeeksInDays);
  if (!days) {
    return false;
  }

  days = BigInt::sub(cx, days, biDaysPassed);
  if (!days) {
    return false;
  }

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

  // Steps 7.aa-ac.
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
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode, &numYears)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numYears,
                          &total)) {
      return false;
    }
  }

  // Step 7.ad.
  double numMonths = 0;
  double numWeeks = 0;

  // Step 17.
  Duration resultDuration = {numYears, numMonths, numWeeks};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 18.
  *result = {resultDuration, total};
  return true;
}

static bool RoundDurationYear(JSContext* cx, const Duration& duration,
                              Handle<temporal::NanosecondsAndDays> nanosAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;

  auto* date = dateRelativeTo.unwrap(cx);
  if (!date) {
    return false;
  }

  // Step 7.a.
  Rooted<CalendarValue> calendar(cx, date->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 7.b.
  Duration yearsDuration = {years};

  // Steps 7.c-d.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 7.e.
  auto yearsLater =
      CalendarDateAdd(cx, calendar, dateRelativeTo, yearsDuration, dateAdd);
  if (!yearsLater) {
    return false;
  }
  auto yearsLaterDate = ToPlainDate(&yearsLater.unwrap());

  // Step 7.i. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsLater);

  // Step 7.f.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 7.g.
  PlainDate yearsMonthsWeeksLater;
  if (!CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonthsWeeks, dateAdd,
                       &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 7.h.
  int32_t monthsWeeksInDays = DaysUntil(yearsLaterDate, yearsMonthsWeeksLater);
  MOZ_ASSERT(std::abs(monthsWeeksInDays) <= epochDays);

  // Step 7.i. (Moved up)

  // Step 4.b.iii. (Reordered)
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

  // This addition can be imprecise, so |daysApproximation| is only an
  // approximation of the actual value.
  double daysApproximation = days + extraDays;

  // Step 7.j.
  // Our implementation keeps |days| and |monthsWeeksInDays| separate.

  // FIXME: spec issue - truncation doesn't match the spec polyfill.
  // https://github.com/tc39/proposal-temporal/issues/2540

  // FIXME: spec issue - `truncate(days)` should be `truncate(fractionalDays)`,
  // because `days` has been set to zero in step 4.d.

  double truncatedDays;
  if (!TruncateDays(cx, nanosAndDays, days, monthsWeeksInDays,
                    &truncatedDays)) {
    return false;
  }

  // Step 7.k.
  Rooted<DurationObject*> wholeDaysDuration(
      cx, CreateTemporalDuration(cx, {0, 0, 0, truncatedDays}));
  if (!wholeDaysDuration) {
    return false;
  }

  // Step 7.l.
  Rooted<Wrapped<PlainDateObject*>> wholeDaysLater(
      cx,
      CalendarDateAdd(cx, calendar, newRelativeTo, wholeDaysDuration, dateAdd));
  if (!wholeDaysLater) {
    return false;
  }

  // Steps 7.m-o.
  Duration timePassed;
  if (!CalendarDateUntil(cx, calendar, newRelativeTo, wholeDaysLater,
                         TemporalUnit::Year, &timePassed)) {
    return false;
  }

  // Step 7.p.
  double yearsPassed = timePassed.years;

  // Step 7.q.
  // Our implementation keeps |years| and |yearsPassed| separate.

  // Step 7.r.
  yearsDuration = {yearsPassed};

  // Steps 7.s-u.
  int32_t daysPassed;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, yearsDuration, dateAdd,
                        &newRelativeTo, &daysPassed)) {
    return false;
  }
  MOZ_ASSERT(std::abs(daysPassed) <= epochDays);

  // Step 7.v.
  // Our implementation keeps |days| and |daysPassed| separate.

  // Steps 7.w.
  bool daysIsNegative;
  if (std::abs(daysApproximation) <= epochDays * 2) {
    int32_t intDays =
        int32_t(daysApproximation) + monthsWeeksInDays - daysPassed;
    daysIsNegative =
        intDays < 0 ||
        (intDays == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  } else {
    // |daysApproximation| is too large, adding |monthsWeeksInDays| and
    // |daysPassed| doesn't change the sign.
    daysIsNegative = daysApproximation < 0;
  }
  double sign = daysIsNegative ? -1 : 1;

  // Step 7.x.
  Duration oneYear = {sign};

  // Steps 7.y-z.
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

    // Steps 7.aa-ac.
    auto denominator = dayLength * std::abs(oneYearDays);
    if (!denominator.isValid()) {
      break;
    }

    int64_t intDays;
    if (!mozilla::NumberEqualsInt64(days, &intDays)) {
      break;
    }

    int64_t intExtraDays;
    if (!mozilla::NumberEqualsInt64(extraDays, &intExtraDays)) {
      break;
    }

    auto totalDays = mozilla::CheckedInt64(intDays);
    totalDays += intExtraDays;
    totalDays += monthsWeeksInDays;
    totalDays -= daysPassed;
    if (!totalDays.isValid()) {
      break;
    }

    auto totalNanoseconds = dayLength * totalDays;
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

    int64_t intYearsPassed;
    if (!mozilla::NumberEqualsInt64(yearsPassed, &intYearsPassed)) {
      break;
    }

    auto totalYears = mozilla::CheckedInt64(intYears) + intYearsPassed;
    if (!totalYears.isValid()) {
      break;
    }

    auto yearNanos = denominator * totalYears;
    if (!yearNanos.isValid()) {
      break;
    }

    totalNanoseconds += yearNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numYears;
    double total = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numYears)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numYears,
                     &total);
    }

    // Step 7.ad.
    double numMonths = 0;
    double numWeeks = 0;

    // Step 17.
    Duration resultDuration = {numYears, numMonths, numWeeks};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 18.
    *result = {resultDuration, total};
    return true;
  } while (false);

  // Steps 7.aa-ad and 17-18.
  return RoundDurationYearSlow(cx, duration, yearsPassed, monthsWeeksInDays,
                               daysPassed, nanosAndDays, oneYearDays, increment,
                               roundingMode, computeRemainder, result);
}

static bool RoundDurationMonthSlow(
    JSContext* cx, const Duration& duration, int64_t monthsToAdd, int32_t days,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t oneMonthDays,
    Increment increment, TemporalRoundingMode roundingMode,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());
  MOZ_ASSERT(oneMonthDays != 0);

  Rooted<BigInt*> months(cx, BigInt::createFromDouble(cx, duration.months));
  if (!months) {
    return false;
  }

  Rooted<BigInt*> biMonthsToAdd(cx, BigInt::createFromInt64(cx, monthsToAdd));
  if (!biMonthsToAdd) {
    return false;
  }

  months = BigInt::add(cx, months, biMonthsToAdd);
  if (!months) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromInt64(cx, days));
  if (!biDays) {
    return false;
  }

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

  Rooted<BigInt*> biOneMonthDays(
      cx, BigInt::createFromInt64(cx, std::abs(oneMonthDays)));
  if (!biOneMonthDays) {
    return false;
  }

  // Steps 8.q-s.
  Rooted<BigInt*> denominator(cx, BigInt::mul(cx, biOneMonthDays, dayLength));
  if (!denominator) {
    return false;
  }

  Rooted<BigInt*> totalNanoseconds(cx, BigInt::mul(cx, biDays, dayLength));
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
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode,
                                          &numMonths)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numMonths,
                          &total)) {
      return false;
    }
  }

  // Step 8.t.
  double numWeeks = 0;

  // Step 4.d.
  double numDays = 0;

  // Step 17.
  Duration resultDuration = {duration.years, numMonths, numWeeks, numDays};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 18.
  *result = {resultDuration, total};
  return true;
}

static bool RoundDurationMonth(
    JSContext* cx, const Duration& duration,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, Increment increment,
    TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  double years = duration.years;
  double months = duration.months;
  double weeks = duration.weeks;

  auto* date = dateRelativeTo.unwrap(cx);
  if (!date) {
    return false;
  }

  // Step 8.a.
  Rooted<CalendarValue> calendar(cx, date->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 8.b.
  Rooted<DurationObject*> yearsMonths(
      cx, CreateTemporalDuration(cx, {years, months}));
  if (!yearsMonths) {
    return false;
  }

  // Steps 8.c-d.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // Step 8.e.
  auto yearsMonthsLater =
      CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonths, dateAdd);
  if (!yearsMonthsLater) {
    return false;
  }
  auto yearsMonthsLaterDate = ToPlainDate(&yearsMonthsLater.unwrap());

  // Step 8.i. (Reordered)
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx, yearsMonthsLater);

  // Step 8.f.
  Duration yearsMonthsWeeks = {years, months, weeks};

  // Step 8.g.
  PlainDate yearsMonthsWeeksLater;
  if (!CalendarDateAdd(cx, calendar, dateRelativeTo, yearsMonthsWeeks, dateAdd,
                       &yearsMonthsWeeksLater)) {
    return false;
  }

  // Step 8.h.
  int32_t weeksInDays = DaysUntil(yearsMonthsLaterDate, yearsMonthsWeeksLater);
  MOZ_ASSERT(std::abs(weeksInDays) <= epochDays);

  // Step 8.i. (Moved up)

  // Steps 4.b. (Reordered)
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

  // This addition can be imprecise, so |daysApproximation| is only an
  // approximation of the actual value.
  double daysApproximation = days + extraDays;

  // clang-format off
  //
  // When |daysApproximation| is too large, the loop condition in step 8.p is
  // always true.
  //
  // Pre-condition:
  // The absolute values of |weeksInDays|, |daysToSubtract|, and |oneMonthDays|
  // are all smaller or equals to |epochDays|.
  //
  // The loop condition is:
  // `abs(daysApproximation + weeksInDays - daysToSubtract) ≥ abs(oneMonthDays)`
  //
  // When `abs(daysApproximation)` is larger or equals to than `3 × epochDays`,
  // the left-hand side expression is trivially always greater or equals to
  // `abs(oneMonthDays)` and the loop will never exit.
  //
  // Furthermore `abs(weeksInDays + oneMonthDays) ≤ epochDays` holds, so the
  // overall limit is `2 × epochDays`.
  //
  // clang-format on
  if (MOZ_UNLIKELY(std::abs(daysApproximation) >= epochDays * 2)) {
    // Step 8.k.
    double sign = daysApproximation < 0 ? -1 : 1;

    // Step 8.l.
    Rooted<DurationObject*> oneMonth(cx, CreateTemporalDuration(cx, {0, sign}));
    if (!oneMonth) {
      return false;
    }

    // Steps 8.m-o and 8.p.iii-v.
    return MoveRelativeDateLoop(cx, calendar, newRelativeTo, oneMonth, dateAdd);
  }

  // Otherwise |daysApproximation| is representable as an int32 value.
  int32_t intDays = int32_t(daysApproximation);

  // Step 8.j.
  intDays += weeksInDays;

  // Step 8.k.
  bool daysIsNegative =
      intDays < 0 ||
      (intDays == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  int32_t sign = daysIsNegative ? -1 : 1;

  // Step 8.l.
  Rooted<DurationObject*> oneMonth(
      cx, CreateTemporalDuration(cx, {0, double(sign)}));
  if (!oneMonth) {
    return false;
  }

  // Steps 8.m-o.
  int32_t oneMonthDays;
  if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth, dateAdd,
                        &newRelativeTo, &oneMonthDays)) {
    return false;
  }
  MOZ_ASSERT(std::abs(weeksInDays + oneMonthDays) <= epochDays);

  // FIXME: spec issue - can this loop be unbounded with a user-controlled
  // calendar?

  // Sum up all days to subtract.
  int32_t daysToSubtract = 0;

  // Sum up all months to add to avoid imprecise floating-point arithmetic.
  int64_t monthsToAdd = 0;

  // Add the fractional part of `fractionalDays`, rounded away from zero, when
  // the fractional part and |intDays| have different signs.
  int32_t roundedDays;
  if (intDays > 0 && nanosAndDays.nanoseconds() < InstantSpan{}) {
    // The loop condition is: `abs(fractionalDays) >= abs(oneMonthDays)`
    //
    // With `fractionalDays = truncatedDays + fractionalPart`.
    //
    // When `truncatedDays = oneMonthDays = 30` and `fractionalPart = -0.5`,
    // then
    //   abs(truncatedDays) >= abs(oneMonthDays)
    // = 30 >= 30
    // = ⊤
    //
    // But the correct result is:
    //   abs(truncatedDays + fractionalPart) >= abs(oneMonthDays)
    // = 29.5 >= 30
    // = ⊥
    //
    // Subtracting by one ensures the correct result.
    roundedDays = intDays - 1;
  } else if (intDays < 0 && nanosAndDays.nanoseconds() > InstantSpan{}) {
    // Same case as above, but with reversed signs.
    roundedDays = intDays + 1;
  } else {
    // No adjustments needed when the integer and fractional part have the same
    // sign or when either the integer or the fractional part is zero.
    roundedDays = intDays;
  }

  // Step 8.p.
  while (std::abs(roundedDays - daysToSubtract) >= std::abs(oneMonthDays)) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    // Step 8.p.i.
    monthsToAdd += sign;

    // Adding `oneMonthDays` to `daysToSubtract` doesn't switch the sign of the
    // expression `(roundedDays - daysToSubtract)`.
    MOZ_ASSERT_IF((roundedDays - daysToSubtract) >= 0,
                  (roundedDays - (daysToSubtract + oneMonthDays)) >= 0);
    MOZ_ASSERT_IF((roundedDays - daysToSubtract) <= 0,
                  (roundedDays - (daysToSubtract + oneMonthDays)) <= 0);

    // Step 8.p.ii.
    daysToSubtract += oneMonthDays;
    MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

    // Steps 8.p.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneMonth, dateAdd,
                          &newRelativeTo, &oneMonthDays)) {
      return false;
    }
    MOZ_ASSERT(std::abs(weeksInDays + oneMonthDays) <= epochDays);
  }

  // The truncated days, excluding any (rounded) fractional parts.
  int32_t truncatedDays = intDays - daysToSubtract;

  do {
    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    if (!nanoseconds.isValid()) {
      break;
    }

    auto dayLength = nanosAndDays.dayLength().toNanoseconds();
    if (!dayLength.isValid()) {
      break;
    }

    // Steps 8.q-s.
    auto denominator = dayLength * std::abs(oneMonthDays);
    if (!denominator.isValid()) {
      break;
    }

    auto totalNanoseconds = dayLength * truncatedDays;
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

    auto totalMonths = mozilla::CheckedInt64(intMonths) + monthsToAdd;
    if (!totalMonths.isValid()) {
      break;
    }

    auto monthNanos = denominator * totalMonths;
    if (!monthNanos.isValid()) {
      break;
    }

    totalNanoseconds += monthNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numMonths;
    double total = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numMonths)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numMonths,
                     &total);
    }

    // Step 8.t.
    double numWeeks = 0;

    // Step 4.d.
    double numDays = 0;

    // Step 17.
    Duration resultDuration = {duration.years, numMonths, numWeeks, numDays};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 18.
    *result = {resultDuration, total};
    return true;
  } while (false);

  // Steps 8.q-t and 17-18.
  return RoundDurationMonthSlow(cx, duration, monthsToAdd, truncatedDays,
                                nanosAndDays, oneMonthDays, increment,
                                roundingMode, computeRemainder, result);
}

static bool RoundDurationWeekSlow(
    JSContext* cx, const Duration& duration, int64_t weeksToAdd, int32_t days,
    Handle<temporal::NanosecondsAndDays> nanosAndDays, int32_t oneWeekDays,
    Increment increment, TemporalRoundingMode roundingMode,
    ComputeRemainder computeRemainder, RoundedDuration* result) {
  MOZ_ASSERT(nanosAndDays.dayLength() > InstantSpan{});
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength().abs());
  MOZ_ASSERT(oneWeekDays != 0);

  Rooted<BigInt*> weeks(cx, BigInt::createFromDouble(cx, duration.weeks));
  if (!weeks) {
    return false;
  }

  Rooted<BigInt*> biWeeksToAdd(cx, BigInt::createFromInt64(cx, weeksToAdd));
  if (!biWeeksToAdd) {
    return false;
  }

  weeks = BigInt::add(cx, weeks, biWeeksToAdd);
  if (!weeks) {
    return false;
  }

  Rooted<BigInt*> biDays(cx, BigInt::createFromInt64(cx, days));
  if (!biDays) {
    return false;
  }

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

  Rooted<BigInt*> biOneWeekDays(
      cx, BigInt::createFromInt64(cx, std::abs(oneWeekDays)));
  if (!biOneWeekDays) {
    return false;
  }

  // Steps 9.h-j.
  Rooted<BigInt*> denominator(cx, BigInt::mul(cx, biOneWeekDays, dayLength));
  if (!denominator) {
    return false;
  }

  Rooted<BigInt*> totalNanoseconds(cx, BigInt::mul(cx, biDays, dayLength));
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
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, denominator,
                                          increment, roundingMode, &numWeeks)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, denominator, &numWeeks,
                          &total)) {
      return false;
    }
  }

  // Step 4.d.
  double numDays = 0;

  // Step 17.
  Duration resultDuration = {duration.years, duration.months, numWeeks,
                             numDays};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 18.
  *result = {resultDuration, total};
  return true;
}

static bool RoundDurationWeek(JSContext* cx, const Duration& duration,
                              Handle<temporal::NanosecondsAndDays> nanosAndDays,
                              Increment increment,
                              TemporalRoundingMode roundingMode,
                              Handle<Wrapped<PlainDateObject*>> dateRelativeTo,
                              ComputeRemainder computeRemainder,
                              RoundedDuration* result) {
  // Numbers of days between nsMinInstant and nsMaxInstant.
  static constexpr int32_t epochDays = 200'000'000;

  auto* date = dateRelativeTo.unwrap(cx);
  if (!date) {
    return false;
  }

  // Step 9.a.
  Rooted<CalendarValue> calendar(cx, date->calendar());
  if (!calendar.wrap(cx)) {
    return false;
  }

  // Step 4.b.
  double days = duration.days;
  double extraDays = nanosAndDays.daysNumber();

  // When non-zero |days| and |extraDays| have oppositive signs, the absolute
  // value of |days| is less-or-equal to |epochDays|. That means when adding
  // |days + extraDays| we don't have to worry about a case like:
  //
  // days = 9007199254740991 and
  // extraDays = 𝔽(-9007199254740993) = -9007199254740992
  //
  // ℝ(𝔽(days) + 𝔽(extraDays)) is -1, whereas the correct result is -2.
  MOZ_ASSERT((days <= 0 && extraDays <= 0) || (days >= 0 && extraDays >= 0) ||
             std::abs(days) <= epochDays);

  // This addition can be imprecise, so |daysApproximation| is only an
  // approximation of the actual value.
  double daysApproximation = days + extraDays;

  // Step 9.b.
  bool daysIsNegative =
      daysApproximation < 0 ||
      (daysApproximation == 0 && nanosAndDays.nanoseconds() < InstantSpan{});
  int32_t sign = daysIsNegative ? -1 : 1;

  // Step 9.c.
  Rooted<DurationObject*> oneWeek(
      cx, CreateTemporalDuration(cx, {0, 0, double(sign)}));
  if (!oneWeek) {
    return false;
  }

  // Steps 9.d-e.
  Rooted<Value> dateAdd(cx);
  if (calendar.isObject()) {
    Rooted<JSObject*> calendarObj(cx, calendar.toObject());
    if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
      return false;
    }
  }

  // clang-format off
  //
  // When |daysApproximation| is too large, the loop condition in step 9.i is
  // always true.
  //
  // Pre-condition:
  // The absolute values of |daysToSubtract| and |oneWeekDays| are both smaller
  // or equals to |epochDays|.
  //
  // The loop condition is:
  // `abs(daysApproximation - daysToSubtract) ≥ abs(oneWeekDays)`
  //
  // When `abs(daysApproximation)` is larger or equals to `2 × epochDays`, the
  // left-hand side expression is always greater or equals to `abs(oneWeekDays)`
  // and the loop will never exit.
  //
  // clang-format on
  if (MOZ_UNLIKELY(std::abs(daysApproximation) >= epochDays * 2)) {
    // Steps 9.f-h and 9.i.iii-v.
    return MoveRelativeDateLoop(cx, calendar, dateRelativeTo, oneWeek, dateAdd);
  }

  // Otherwise |daysApproximation| is representable as an int32 value.
  int32_t intDays = int32_t(daysApproximation);

  // Steps 9.f-h.
  Rooted<Wrapped<PlainDateObject*>> newRelativeTo(cx);
  int32_t oneWeekDays;
  if (!MoveRelativeDate(cx, calendar, dateRelativeTo, oneWeek, dateAdd,
                        &newRelativeTo, &oneWeekDays)) {
    return false;
  }

  // FIXME: spec issue - can this loop be unbounded with a user-controlled
  // calendar?

  // Sum up all days to subtract.
  int32_t daysToSubtract = 0;

  // Sum up all weeks to add to avoid imprecise floating-point arithmetic.
  int64_t weeksToAdd = 0;

  // Add the fractional part of `fractionalDays`, rounded away from zero, when
  // the fractional part and |intDays| have different signs.
  int32_t roundedDays;
  if (intDays > 0 && nanosAndDays.nanoseconds() < InstantSpan{}) {
    // The loop condition is: `abs(fractionalDays) >= abs(oneWeekDays)`
    //
    // With `fractionalDays = truncatedDays + fractionalPart`.
    //
    // When `truncatedDays = oneWeekDays = 7` and `fractionalPart = -0.5`,
    // then
    //   abs(truncatedDays) >= abs(oneWeekDays)
    // = 7 >= 7
    // = ⊤
    //
    // But the correct result is:
    //   abs(truncatedDays + fractionalPart) >= abs(oneWeekDays)
    // = 6.5 >= 7
    // = ⊥
    //
    // Subtracting by one ensures the correct result.
    roundedDays = intDays - 1;
  } else if (intDays < 0 && nanosAndDays.nanoseconds() > InstantSpan{}) {
    // Same case as above, but with reversed signs.
    roundedDays = intDays + 1;
  } else {
    // No adjustments needed when the integer and fractional part have the same
    // sign or when either the integer or the fractional part is zero.
    roundedDays = intDays;
  }

  // Step 9.i.
  while (std::abs(roundedDays - daysToSubtract) >= std::abs(oneWeekDays)) {
    // This loop can iterate indefinitely when given a specially crafted
    // calendar object, so we need to check for interrupts.
    if (!CheckForInterrupt(cx)) {
      return false;
    }

    // Step 9.i.i.
    weeksToAdd += sign;

    // Adding `oneWeekDays` to `daysToSubtract` doesn't switch the sign of the
    // expression `(intDays - daysToSubtract)`.
    MOZ_ASSERT_IF((intDays - daysToSubtract) >= 0,
                  (intDays - (daysToSubtract + oneWeekDays)) >= 0);
    MOZ_ASSERT_IF((intDays - daysToSubtract) <= 0,
                  (intDays - (daysToSubtract + oneWeekDays)) <= 0);

    // Step 9.i.ii.
    daysToSubtract += oneWeekDays;
    MOZ_ASSERT(std::abs(daysToSubtract) <= epochDays);

    // Steps 9.i.iii-v.
    if (!MoveRelativeDate(cx, calendar, newRelativeTo, oneWeek, dateAdd,
                          &newRelativeTo, &oneWeekDays)) {
      return false;
    }
  }

  // The truncated days, excluding any (rounded) fractional parts.
  int32_t truncatedDays = intDays - daysToSubtract;

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

    auto nanoseconds = nanosAndDays.nanoseconds().toNanoseconds();
    if (!nanoseconds.isValid()) {
      break;
    }

    auto dayLength = nanosAndDays.dayLength().toNanoseconds();
    if (!dayLength.isValid()) {
      break;
    }

    // Steps 9.j-l.
    auto denominator = dayLength * std::abs(oneWeekDays);
    if (!denominator.isValid()) {
      break;
    }

    auto totalNanoseconds = dayLength * truncatedDays;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    totalNanoseconds += nanoseconds;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    int64_t intWeeks;
    if (!mozilla::NumberEqualsInt64(duration.weeks, &intWeeks)) {
      break;
    }

    auto totalWeeks = mozilla::CheckedInt64(intWeeks) + weeksToAdd;
    if (!totalWeeks.isValid()) {
      break;
    }

    auto weekNanos = denominator * totalWeeks;
    if (!weekNanos.isValid()) {
      break;
    }

    totalNanoseconds += weekNanos;
    if (!totalNanoseconds.isValid()) {
      break;
    }

    double numWeeks;
    double total = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            denominator.value(), increment,
                                            roundingMode, &numWeeks)) {
        return false;
      }
    } else {
      TruncateNumber(totalNanoseconds.value(), denominator.value(), &numWeeks,
                     &total);
    }

    // Step 4.d.
    double numDays = 0;

    // Step 17.
    Duration resultDuration = {duration.years, duration.months, numWeeks,
                               numDays};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 18.
    *result = {resultDuration, total};
    return true;
  } while (false);

  // Steps 9.j-l and 17-18.
  return RoundDurationWeekSlow(cx, duration, weeksToAdd, truncatedDays,
                               nanosAndDays, oneWeekDays, increment,
                               roundingMode, computeRemainder, result);
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

  // Steps 10.a-c.
  double days;
  double total = 0;
  if (computeRemainder == ComputeRemainder::No) {
    if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds, dayLength,
                                          increment, roundingMode, &days)) {
      return false;
    }
  } else {
    if (!::TruncateNumber(cx, totalNanoseconds, dayLength, &days, &total)) {
      return false;
    }
  }

  MOZ_ASSERT(IsIntegerOrInfinity(days));

  // Step 17.
  Duration resultDuration = {duration.years, duration.months, duration.weeks,
                             days};
  if (!ThrowIfInvalidDuration(cx, resultDuration)) {
    return false;
  }

  // Step 18.
  *result = {resultDuration, total};
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

    // Steps 10.a-c.
    double days;
    double total = 0;
    if (computeRemainder == ComputeRemainder::No) {
      if (!temporal::RoundNumberToIncrement(cx, totalNanoseconds.value(),
                                            dayLength.value(), increment,
                                            roundingMode, &days)) {
        return false;
      }
    } else {
      ::TruncateNumber(totalNanoseconds.value(), dayLength.value(), &days,
                       &total);
    }

    MOZ_ASSERT(IsIntegerOrInfinity(days));

    // Step 17.
    Duration resultDuration = {duration.years, duration.months, duration.weeks,
                               days};
    if (!ThrowIfInvalidDuration(cx, resultDuration)) {
      return false;
    }

    // Step 18.
    *result = {resultDuration, total};
    return true;
  } while (false);

  // Steps 10 and 17-18.
  return RoundDurationDaySlow(cx, duration, nanosAndDays, increment,
                              roundingMode, computeRemainder, result);
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
                          Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
                          ComputeRemainder computeRemainder,
                          RoundedDuration* result) {
  // Note: |duration.days| can have a different sign than the other date
  // components. The date and time components can have different signs, too.
  MOZ_ASSERT(
      IsValidDuration({duration.years, duration.months, duration.weeks}));
  MOZ_ASSERT(IsValidDuration(duration.time()));

  MOZ_ASSERT(plainRelativeTo || zonedRelativeTo,
             "Use RoundDuration without relativeTo when plainRelativeTo and "
             "zonedRelativeTo are both undefined");

  // The remainder is only needed when called from |Duration_total|. And `total`
  // always passes |increment=1| and |roundingMode=trunc|.
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                increment == Increment{1});
  MOZ_ASSERT_IF(computeRemainder == ComputeRemainder::Yes,
                roundingMode == TemporalRoundingMode::Trunc);

  // Steps 1-3. (Not applicable in our implementation.)
  MOZ_ASSERT_IF(unit <= TemporalUnit::Week, plainRelativeTo);

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
      // Steps 4-5 and 10-18.
      return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                             computeRemainder, result);
    case TemporalUnit::Auto:
      MOZ_CRASH("Unexpected temporal unit");
  }

  // Step 4.
  MOZ_ASSERT(TemporalUnit::Year <= unit && unit <= TemporalUnit::Day);

  // Steps 4.a-c.
  Rooted<temporal::NanosecondsAndDays> nanosAndDays(cx);
  if (zonedRelativeTo) {
    // Step 4.b.i. (Reordered)
    Rooted<ZonedDateTimeObject*> intermediate(
        cx, MoveRelativeZonedDateTime(cx, zonedRelativeTo, duration.date()));
    if (!intermediate) {
      return false;
    }

    // Steps 4.a and 4.b.ii.
    if (!NanosecondsToDays(cx, duration, intermediate, &nanosAndDays)) {
      return false;
    }

    // Step 4.b.iii. (Not applicable in our implementation.)
  } else {
    // Steps 4.a and 4.c.
    if (!::NanosecondsToDays(cx, duration, &nanosAndDays)) {
      return false;
    }
  }

  // NanosecondsToDays guarantees that |abs(nanosAndDays.nanoseconds)| is less
  // than |abs(nanosAndDays.dayLength)|.
  MOZ_ASSERT(nanosAndDays.nanoseconds().abs() < nanosAndDays.dayLength());

  // Step 4.d. (Moved below)

  // Step 4.e. (Implicit)

  // Step 5. (Not applicable)

  // Step 6.
  // FIXME: spec issue - `total` doesn't need be initialised.

  // Steps 7-18.
  switch (unit) {
    // Steps 7 and 17-18.
    case TemporalUnit::Year:
      return RoundDurationYear(cx, duration, nanosAndDays, increment,
                               roundingMode, plainRelativeTo, computeRemainder,
                               result);

    // Steps 8 and 17-18.
    case TemporalUnit::Month:
      return RoundDurationMonth(cx, duration, nanosAndDays, increment,
                                roundingMode, plainRelativeTo, computeRemainder,
                                result);

    // Steps 9 and 17-18.
    case TemporalUnit::Week:
      return RoundDurationWeek(cx, duration, nanosAndDays, increment,
                               roundingMode, plainRelativeTo, computeRemainder,
                               result);

    // Steps 10 and 19-21.
    case TemporalUnit::Day:
      return RoundDurationDay(cx, duration, nanosAndDays, increment,
                              roundingMode, computeRemainder, result);

    // Steps 11-16. (Handled elsewhere)
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
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
                          Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
                          double* result) {
  // Only called from |Duration_total|, which always passes |increment=1| and
  // |roundingMode=trunc|.
  MOZ_ASSERT(increment == Increment{1});
  MOZ_ASSERT(roundingMode == TemporalRoundingMode::Trunc);

  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       plainRelativeTo, zonedRelativeTo, ComputeRemainder::Yes,
                       &rounded)) {
    return false;
  }

  *result = rounded.total;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
static bool RoundDuration(JSContext* cx, const Duration& duration,
                          Increment increment, TemporalUnit unit,
                          TemporalRoundingMode roundingMode,
                          Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
                          Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
                          Duration* result) {
  RoundedDuration rounded;
  if (!::RoundDuration(cx, duration, increment, unit, roundingMode,
                       plainRelativeTo, zonedRelativeTo, ComputeRemainder::No,
                       &rounded)) {
    return false;
  }

  *result = rounded.duration;
  return true;
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool js::temporal::RoundDuration(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    Handle<Wrapped<PlainDateObject*>> plainRelativeTo, Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  Handle<ZonedDateTimeObject*> zonedRelativeTo = nullptr;
  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         plainRelativeTo, zonedRelativeTo, result);
}

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool js::temporal::RoundDuration(JSContext* cx, const Duration& duration,
                                 Increment increment, TemporalUnit unit,
                                 TemporalRoundingMode roundingMode,
                                 Handle<ZonedDateTimeObject*> zonedRelativeTo,
                                 Duration* result) {
  MOZ_ASSERT(IsValidDuration(duration));

  // DifferenceTemporalZonedDateTime, steps 11-12.
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  if (unit <= TemporalUnit::Week) {
    plainRelativeTo = ToTemporalDate(cx, zonedRelativeTo);
    if (!plainRelativeTo) {
      return false;
    }
  }

  return ::RoundDuration(cx, duration, increment, unit, roundingMode,
                         plainRelativeTo, zonedRelativeTo, result);
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

  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  if (args.hasDefined(1)) {
    const char* name = operation == DurationOperation::Add ? "add" : "subtract";

    // Step 3.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", name, args[1]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
  }

  // Step 5.
  if (operation == DurationOperation::Subtract) {
    other = other.negate();
  }

  Duration result;
  if (plainRelativeTo) {
    if (!AddDuration(cx, duration, other, plainRelativeTo, &result)) {
      return false;
    }
  } else if (zonedRelativeTo) {
    if (!AddDuration(cx, duration, other, zonedRelativeTo, &result)) {
      return false;
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

  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  if (args.hasDefined(2)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "compare", args[2]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (one == two) {
      args.rval().setInt32(0);
      return true;
    }

    // Steps 5-7.
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);
  } else {
    // Step 3. (Not applicable in our implementation.)

    // Step 4.
    if (one == two) {
      args.rval().setInt32(0);
      return true;
    }

    // Steps 5-7. (Not applicable in our implementation.)
  }

  // Steps 8-9.
  auto hasCalendarUnit = [](const auto& d) {
    return d.years != 0 || d.months != 0 || d.weeks != 0;
  };
  bool calendarUnitsPresent = hasCalendarUnit(one) || hasCalendarUnit(two);

  // Step 10.
  if (zonedRelativeTo &&
      (calendarUnitsPresent || one.days != 0 || two.days != 0)) {
    // Steps 10.a-b.
    auto* unwrappedRelativeTo = zonedRelativeTo.unwrap(cx);
    if (!unwrappedRelativeTo) {
      return false;
    }

    auto instant = ToInstant(unwrappedRelativeTo);
    Rooted<TimeZoneValue> timeZone(cx, unwrappedRelativeTo->timeZone());
    Rooted<CalendarValue> calendar(cx, unwrappedRelativeTo->calendar());

    // Wrap into the current compartment.
    if (!timeZone.wrap(cx)) {
      return false;
    }
    if (!calendar.wrap(cx)) {
      return false;
    }

    // Steps 10.c-d.
    PlainDateTime dateTime;
    if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
      return false;
    }

    // Step 10.e.
    Instant after1;
    if (!AddZonedDateTime(cx, instant, timeZone, calendar, one, dateTime,
                          &after1)) {
      return false;
    }

    // Step 10.f.
    Instant after2;
    if (!AddZonedDateTime(cx, instant, timeZone, calendar, two, dateTime,
                          &after2)) {
      return false;
    }

    // Steps 10.g-i.
    args.rval().setInt32(after1 < after2 ? -1 : after1 > after2 ? 1 : 0);
    return true;
  }

  // Steps 11-12.
  double days1, days2;
  if (calendarUnitsPresent) {
    // FIXME: spec issue - directly throw an error if plainRelativeTo is undef.

    // Step 11.a.
    DateDuration unbalanceResult1;
    if (plainRelativeTo) {
      if (!UnbalanceDateDurationRelative(cx, one, TemporalUnit::Day,
                                         plainRelativeTo, &unbalanceResult1)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(cx, one, TemporalUnit::Day,
                                         &unbalanceResult1)) {
        return false;
      }
      MOZ_ASSERT(one.date() == unbalanceResult1.toDuration());
    }

    // Step 11.b.
    DateDuration unbalanceResult2;
    if (plainRelativeTo) {
      if (!UnbalanceDateDurationRelative(cx, two, TemporalUnit::Day,
                                         plainRelativeTo, &unbalanceResult2)) {
        return false;
      }
    } else {
      if (!UnbalanceDateDurationRelative(cx, two, TemporalUnit::Day,
                                         &unbalanceResult2)) {
        return false;
      }
      MOZ_ASSERT(two.date() == unbalanceResult2.toDuration());
    }

    // Step 11.c.
    days1 = unbalanceResult1.days;

    // Step 11.d.
    days2 = unbalanceResult2.days;
  } else {
    // Step 12.a.
    days1 = one.days;

    // Step 12.b.
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

  // Steps 13-19.
  //
  // Fast path when the total duration amount fits into an int64.
  if (auto ns1 = TotalDurationNanoseconds(oneTotal)) {
    if (auto ns2 = TotalDurationNanoseconds(twoTotal)) {
      args.rval().setInt32(*ns1 < *ns2 ? -1 : *ns1 > *ns2 ? 1 : 0);
      return true;
    }
  }

  // Steps 13 and 15.
  Rooted<BigInt*> ns1(cx, TotalDurationNanosecondsSlow(cx, oneTotal));
  if (!ns1) {
    return false;
  }

  // Steps 14 and 16.
  auto* ns2 = TotalDurationNanosecondsSlow(cx, twoTotal);
  if (!ns2) {
    return false;
  }

  // Step 17-19.
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

  // Step 17. (Reordered)
  auto existingLargestUnit = DefaultTemporalLargestUnit(duration);

  // Steps 3-24.
  auto smallestUnit = TemporalUnit::Auto;
  TemporalUnit largestUnit;
  auto roundingMode = TemporalRoundingMode::HalfExpand;
  auto roundingIncrement = Increment{1};
  Rooted<JSObject*> relativeTo(cx);
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Steps 6-14. (Not applicable)

    // Step 15.
    Rooted<JSString*> paramString(cx, args[0].toString());
    if (!GetTemporalUnit(cx, paramString, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 16. (Not applicable)

    // Step 17. (Moved above)

    // Step 18.
    auto defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    // Step 19. (Not applicable)

    // Step 19.a. (Not applicable)

    // Step 19.b.
    largestUnit = defaultLargestUnit;

    // Steps 20-24. (Not applicable)
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

    // Steps 10-12.
    if (!ToRelativeTemporalObject(cx, options, &plainRelativeTo,
                                  &zonedRelativeTo)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);

    // Step 13.
    if (!ToTemporalRoundingIncrement(cx, options, &roundingIncrement)) {
      return false;
    }

    // Step 14.
    if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
      return false;
    }

    // Step 15.
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::DateTime, &smallestUnit)) {
      return false;
    }

    // Step 16.
    if (smallestUnit == TemporalUnit::Auto) {
      // Step 16.a.
      smallestUnitPresent = false;

      // Step 16.b.
      smallestUnit = TemporalUnit::Nanosecond;
    }

    // Step 17. (Moved above)

    // Step 18.
    auto defaultLargestUnit = std::min(existingLargestUnit, smallestUnit);

    // Steps 19-20.
    if (largestUnitValue.isUndefined()) {
      // Step 19.a.
      largestUnitPresent = false;

      // Step 19.b.
      largestUnit = defaultLargestUnit;
    } else if (largestUnit == TemporalUnit::Auto) {
      // Step 20.a
      largestUnit = defaultLargestUnit;
    }

    // Step 21.
    if (!smallestUnitPresent && !largestUnitPresent) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_DURATION_MISSING_UNIT_SPECIFIER);
      return false;
    }

    // Step 22.
    if (largestUnit > smallestUnit) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_RANGE);
      return false;
    }

    // Steps 23-24.
    if (smallestUnit > TemporalUnit::Day) {
      // Step 23.
      auto maximum = MaximumTemporalDurationRoundingIncrement(smallestUnit);

      // Step 24.
      if (!ValidateTemporalRoundingIncrement(cx, roundingIncrement, maximum,
                                             false)) {
        return false;
      }
    }
  }

  // FIXME: spec bug - negative durations shouldn't unconditionally take the
  // fast path, i.e. comparisons need to happen against the absolute values.

  // Step 25.
  bool hoursToDaysConversionMayOccur = false;

  // Step 26.
  if (duration.days != 0 && zonedRelativeTo) {
    hoursToDaysConversionMayOccur = true;
  }

  // Step 27.
  else if (duration.hours >= 24) {
    hoursToDaysConversionMayOccur = true;
  }

  // Step 28.
  if (smallestUnit == TemporalUnit::Nanosecond &&
      roundingIncrement == Increment{1} && largestUnit == existingLargestUnit &&
      duration.years == 0 && duration.months == 0 && duration.weeks == 0 &&
      !hoursToDaysConversionMayOccur && duration.minutes < 60 &&
      duration.seconds < 60 && duration.milliseconds < 1000 &&
      duration.microseconds < 1000 && duration.nanoseconds < 1000) {
    // Steps 28.a-b.
    auto* obj = CreateTemporalDuration(cx, duration);
    if (!obj) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  // FIXME: spec bug - condition incorrect:
  // - needs to take largestUnit into account for BalanceDateDurationRelative
  // - duration.[[Days]] can be non-zero and still no calendar is required
  // https://github.com/tc39/proposal-temporal/issues/2680

  // Step 29.
  bool createPlainRelativeTo = false;
  if (zonedRelativeTo) {
    if (largestUnit <= TemporalUnit::Week) {
      // Required for BalanceDateDurationRelative
      createPlainRelativeTo = true;
    } else if (smallestUnit <= TemporalUnit::Week) {
      // Required for RoundDuration
      createPlainRelativeTo = true;
    } else if (duration.years != 0 || duration.months != 0 ||
               duration.weeks != 0 || duration.days != 0) {
      // Required for UnbalanceDateDurationRelative
      createPlainRelativeTo = true;
    }
  }
  if (createPlainRelativeTo) {
    // Steps 29.a-b.
    plainRelativeTo = ToTemporalDate(cx, zonedRelativeTo);
    if (!plainRelativeTo) {
      return false;
    }
  }

  // Step 30.
  DateDuration unbalanceResult;
  if (plainRelativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration, largestUnit,
                                       plainRelativeTo, &unbalanceResult)) {
      return false;
    }
  } else {
    if (!UnbalanceDateDurationRelative(cx, duration, largestUnit,
                                       &unbalanceResult)) {
      return false;
    }
    MOZ_ASSERT(duration.date() == unbalanceResult.toDuration());
  }

  // Steps 31-32.
  Duration roundInput = {
      unbalanceResult.years, unbalanceResult.months, unbalanceResult.weeks,
      unbalanceResult.days,  duration.hours,         duration.minutes,
      duration.seconds,      duration.milliseconds,  duration.microseconds,
      duration.nanoseconds,
  };
  Duration roundResult;
  if (plainRelativeTo || zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, plainRelativeTo, zonedRelativeTo,
                         &roundResult)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, roundingIncrement, smallestUnit,
                         roundingMode, &roundResult)) {
      return false;
    }
  }

  // Steps 33-34.
  TimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 33.a.
    Duration adjustResult;
    if (!AdjustRoundedDurationDays(cx, roundResult, roundingIncrement,
                                   smallestUnit, roundingMode, zonedRelativeTo,
                                   &adjustResult)) {
      return false;
    }
    roundResult = adjustResult;

    // Step 33.b.
    if (!BalanceTimeDurationRelative(cx, roundResult, largestUnit,
                                     zonedRelativeTo, &balanceResult)) {
      return false;
    }
  } else {
    // Step 34.a.
    if (!BalanceTimeDuration(cx, roundResult, largestUnit, &balanceResult)) {
      return false;
    }
  }

  // Step 35.
  Duration balanceInput = {
      roundResult.years,
      roundResult.months,
      roundResult.weeks,
      balanceResult.days,
  };
  DateDuration result;
  if (!BalanceDateDurationRelative(cx, balanceInput, largestUnit,
                                   plainRelativeTo, &result)) {
    return false;
  }

  // Step 36.
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
  Rooted<Wrapped<PlainDateObject*>> plainRelativeTo(cx);
  Rooted<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo(cx);
  auto unit = TemporalUnit::Auto;
  if (args.get(0).isString()) {
    // Step 4. (Not applicable in our implementation.)

    // Steps 6-9. (Implicit)
    MOZ_ASSERT(!plainRelativeTo && !zonedRelativeTo);

    // Step 10.
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

    // Steps 6-9.
    if (!ToRelativeTemporalObject(cx, totalOf, &plainRelativeTo,
                                  &zonedRelativeTo)) {
      return false;
    }
    MOZ_ASSERT(!plainRelativeTo || !zonedRelativeTo);

    // Step 10.
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

  // FIXME: spec bug - wrong condition |plainRelativeTo| needs to be created
  // when |unit <= TemporalUnit::Week| OR when the calendar units are non-zero.
  // https://github.com/tc39/proposal-temporal/issues/2681

  // Step 11.
  if (zonedRelativeTo && (unit <= TemporalUnit::Week || duration.years != 0 ||
                          duration.months != 0 || duration.weeks != 0)) {
    // Steps 11.a-b.
    plainRelativeTo = ToTemporalDate(cx, zonedRelativeTo);
    if (!plainRelativeTo) {
      return false;
    }
  }

  // Step 12.
  DateDuration unbalanceResult;
  if (plainRelativeTo) {
    if (!UnbalanceDateDurationRelative(cx, duration, unit, plainRelativeTo,
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

  // Steps 13-14.
  TimeDuration balanceResult;
  if (zonedRelativeTo) {
    // Step 13.a
    Rooted<ZonedDateTimeObject*> intermediate(
        cx, MoveRelativeZonedDateTime(
                cx, zonedRelativeTo,
                {unbalanceResult.years, unbalanceResult.months,
                 unbalanceResult.weeks, 0}));
    if (!intermediate) {
      return false;
    }

    // Step 13.b.
    if (!BalancePossiblyInfiniteTimeDurationRelative(
            cx, balanceInput, unit, intermediate, &balanceResult)) {
      return false;
    }
  } else {
    // Step 14.
    if (!BalancePossiblyInfiniteTimeDuration(cx, balanceInput, unit,
                                             &balanceResult)) {
      return false;
    }
  }

  // Steps 15-16.
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

  // Step 17. (Not applicable in our implementation.)

  // Step 18.
  Duration roundInput = {
      unbalanceResult.years,      unbalanceResult.months,
      unbalanceResult.weeks,      balanceResult.days,
      balanceResult.hours,        balanceResult.minutes,
      balanceResult.seconds,      balanceResult.milliseconds,
      balanceResult.microseconds, balanceResult.nanoseconds,
  };
  double total;
  if (plainRelativeTo || zonedRelativeTo) {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, plainRelativeTo,
                         zonedRelativeTo, &total)) {
      return false;
    }
  } else {
    if (!::RoundDuration(cx, roundInput, Increment{1}, unit,
                         TemporalRoundingMode::Trunc, &total)) {
      return false;
    }
  }

  // Step 19.
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
  auto* duration = &args.thisv().toObject().as<DurationObject>();
  Duration rounded;
  if (!temporal::RoundDuration(cx, ToDuration(duration), precision.increment,
                               precision.unit, roundingMode, &rounded)) {
    return false;
  }

  // Step 12.
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
