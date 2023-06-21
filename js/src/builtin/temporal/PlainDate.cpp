/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDate.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdint.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/Wrapped.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/Date.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainDate(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainDateObject>();
}

#ifdef DEBUG
/**
 * IsValidISODate ( year, month, day )
 */
template <typename T>
static bool IsValidISODate(T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 2.
  if (month < 1 || month > 12) {
    return false;
  }

  // Step 3.
  int32_t daysInMonth = js::temporal::ISODaysInMonth(year, int32_t(month));

  // Step 4.
  if (day < 1 || day > daysInMonth) {
    return false;
  }

  // Step 5.
  return true;
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::IsValidISODate(const PlainDate& date) {
  auto& [year, month, day] = date;
  return ::IsValidISODate(year, month, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::IsValidISODate(double year, double month, double day) {
  return ::IsValidISODate(year, month, day);
}
#endif

static void ReportInvalidDateValue(JSContext* cx, const char* name, int32_t min,
                                   int32_t max, double num) {
  Int32ToCStringBuf minCbuf;
  const char* minStr = Int32ToCString(&minCbuf, min);

  Int32ToCStringBuf maxCbuf;
  const char* maxStr = Int32ToCString(&maxCbuf, max);

  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_PLAIN_DATE_INVALID_VALUE, name,
                            minStr, maxStr, numStr);
}

template <typename T>
static inline bool ThrowIfInvalidDateValue(JSContext* cx, const char* name,
                                           int32_t min, int32_t max, T num) {
  if (min <= num && num <= max) {
    return true;
  }
  ReportInvalidDateValue(cx, name, min, max, num);
  return false;
}

/**
 * IsValidISODate ( year, month, day )
 */
template <typename T>
static bool ThrowIfInvalidISODate(JSContext* cx, T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 2.
  if (!ThrowIfInvalidDateValue(cx, "month", 1, 12, month)) {
    return false;
  }

  // Step 3.
  int32_t daysInMonth = js::temporal::ISODaysInMonth(year, int32_t(month));

  // Step 4.
  if (!ThrowIfInvalidDateValue(cx, "day", 1, daysInMonth, day)) {
    return false;
  }

  // Step 5.
  return true;
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, const PlainDate& date) {
  auto& [year, month, day] = date;
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * IsValidISODate ( year, month, day )
 */
bool js::temporal::ThrowIfInvalidISODate(JSContext* cx, double year,
                                         double month, double day) {
  return ::ThrowIfInvalidISODate(cx, year, month, day);
}

/**
 * RegulateISODate ( year, month, day, overflow )
 */
bool js::temporal::RegulateISODate(JSContext* cx, const PlainDate& date,
                                   TemporalOverflow overflow,
                                   PlainDate* result) {
  auto& [year, month, day] = date;

  // Step 1.
  if (overflow == TemporalOverflow::Constrain) {
    // Step 1.a.
    int32_t m = std::clamp(month, 1, 12);

    // Step 1.b.
    int32_t daysInMonth = ISODaysInMonth(year, m);

    // Step 1.c.
    int32_t d = std::clamp(day, 1, daysInMonth);

    // Step 1.d.
    *result = {year, m, d};
    return true;
  }

  // Step 2.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 2.b.
  if (!ThrowIfInvalidISODate(cx, year, month, day)) {
    return false;
  }

  // Step 2.b. (Inlined call to CreateISODateRecord.)
  *result = {year, month, day};
  return true;
}

/**
 * RegulateISODate ( year, month, day, overflow )
 */
bool js::temporal::RegulateISODate(JSContext* cx, double year, double month,
                                   double day, TemporalOverflow overflow,
                                   RegulatedISODate* result) {
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));

  // Step 1.
  if (overflow == TemporalOverflow::Constrain) {
    // Step 1.a.
    int32_t m = int32_t(std::clamp(month, 1.0, 12.0));

    // Step 1.b.
    double daysInMonth = double(ISODaysInMonth(year, m));

    // Step 1.c.
    int32_t d = int32_t(std::clamp(day, 1.0, daysInMonth));

    // Step 1.d.
    *result = {year, m, d};
    return true;
  }

  // Step 2.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 2.b.
  if (!ThrowIfInvalidISODate(cx, year, month, day)) {
    return false;
  }

  // Step 2.b. (Inlined call to CreateISODateRecord.)
  *result = {year, int32_t(month), int32_t(day)};
  return true;
}

/**
 * CreateTemporalDate ( isoYear, isoMonth, isoDay, calendar [ , newTarget ] )
 */
static PlainDateObject* CreateTemporalDate(JSContext* cx, const CallArgs& args,
                                           double isoYear, double isoMonth,
                                           double isoDay,
                                           Handle<JSObject*> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(isoYear, isoMonth, isoDay)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainDate,
                                          &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<PlainDateObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Step 5.
  object->setFixedSlot(PlainDateObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  object->setFixedSlot(PlainDateObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  object->setFixedSlot(PlainDateObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 8.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 9.
  return object;
}

/**
 * CreateTemporalDate ( isoYear, isoMonth, isoDay, calendar [ , newTarget ] )
 */
PlainDateObject* js::temporal::CreateTemporalDate(JSContext* cx,
                                                  const PlainDate& date,
                                                  Handle<JSObject*> calendar) {
  auto& [isoYear, isoMonth, isoDay] = date;

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(date)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  auto* object = NewBuiltinClassInstance<PlainDateObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 5.
  object->setFixedSlot(PlainDateObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 6.
  object->setFixedSlot(PlainDateObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 7.
  object->setFixedSlot(PlainDateObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 8.
  object->setFixedSlot(PlainDateObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 9.
  return object;
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
static Wrapped<PlainDateObject*> ToTemporalDate(
    JSContext* cx, Handle<JSObject*> item, Handle<JSObject*> maybeOptions) {
  // Step 1-2. (Not applicable in our implementation.)

  // Step 3.a.
  if (item->canUnwrapAs<PlainDateObject>()) {
    return item;
  }

  // Step 3.b.
  if (auto* zonedDateTime = item->maybeUnwrapIf<ZonedDateTimeObject>()) {
    auto epochInstant = ToInstant(zonedDateTime);
    Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
    Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

    if (!cx->compartment()->wrap(cx, &timeZone)) {
      return nullptr;
    }
    if (!cx->compartment()->wrap(cx, &calendar)) {
      return nullptr;
    }

    // Step 3.b.i.
    if (maybeOptions) {
      TemporalOverflow ignored;
      if (!ToTemporalOverflow(cx, maybeOptions, &ignored)) {
        return nullptr;
      }
    }

    // Steps 3.b.ii-iii.
    PlainDateTime dateTime;
    if (!GetPlainDateTimeFor(cx, timeZone, epochInstant, &dateTime)) {
      return nullptr;
    }

    // Step 3.b.iv.
    return CreateTemporalDate(cx, dateTime.date, calendar);
  }

  // Step 3.c.
  if (auto* dateTime = item->maybeUnwrapIf<PlainDateTimeObject>()) {
    auto date = ToPlainDate(dateTime);
    Rooted<JSObject*> calendar(cx, dateTime->calendar());
    if (!cx->compartment()->wrap(cx, &calendar)) {
      return nullptr;
    }

    // Step 3.c.i.
    if (maybeOptions) {
      TemporalOverflow ignored;
      if (!ToTemporalOverflow(cx, maybeOptions, &ignored)) {
        return nullptr;
      }
    }

    // Step 3.c.ii.
    return CreateTemporalDate(cx, date, calendar);
  }

  // Step 3.d.
  Rooted<JSObject*> calendar(cx, GetTemporalCalendarWithISODefault(cx, item));
  if (!calendar) {
    return nullptr;
  }

  // Step 3.e.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::Month,
                       CalendarField::MonthCode, CalendarField::Year},
                      &fieldNames)) {
    return nullptr;
  }

  // Step 3.f.
  Rooted<PlainObject*> fields(cx, PrepareTemporalFields(cx, item, fieldNames));
  if (!fields) {
    return nullptr;
  }

  // Step 3.g.
  return ::CalendarDateFromFields(cx, calendar, fields, maybeOptions);
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
static Wrapped<PlainDateObject*> ToTemporalDate(
    JSContext* cx, Handle<Value> item, Handle<JSObject*> maybeOptions) {
  // Step 1-2. (Not applicable in our implementation.)

  // Step 3.
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());
    return ::ToTemporalDate(cx, itemObj, maybeOptions);
  }

  // Step 4.
  if (maybeOptions) {
    TemporalOverflow ignored;
    if (!ToTemporalOverflow(cx, maybeOptions, &ignored)) {
      return nullptr;
    }
  }

  // Step 5.
  Rooted<JSString*> string(cx, JS::ToString(cx, item));
  if (!string) {
    return nullptr;
  }

  // Step 6.
  PlainDate result;
  Rooted<JSString*> calendarString(cx);
  if (!ParseTemporalDateString(cx, string, &result, &calendarString)) {
    return nullptr;
  }

  // Step 7.
  MOZ_ASSERT(IsValidISODate(result));

  // Step 8.
  Rooted<Value> calendarLike(cx);
  if (calendarString) {
    calendarLike.setString(calendarString);
  }

  Rooted<JSObject*> calendar(
      cx, ToTemporalCalendarWithISODefault(cx, calendarLike));
  if (!calendar) {
    return nullptr;
  }

  // Step 9.
  return CreateTemporalDate(cx, result, calendar);
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
Wrapped<PlainDateObject*> js::temporal::ToTemporalDate(JSContext* cx,
                                                       Handle<JSObject*> item) {
  return ::ToTemporalDate(cx, item, nullptr);
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
bool js::temporal::ToTemporalDate(JSContext* cx, Handle<Value> item,
                                  PlainDate* result) {
  auto obj = ::ToTemporalDate(cx, item, nullptr);
  if (!obj) {
    return false;
  }

  *result = ToPlainDate(&obj.unwrap());
  return true;
}

/**
 * ToTemporalDate ( item [ , options ] )
 */
bool js::temporal::ToTemporalDate(JSContext* cx, Handle<Value> item,
                                  PlainDate* result,
                                  MutableHandle<JSObject*> calendar) {
  auto* obj = ::ToTemporalDate(cx, item, nullptr).unwrapOrNull();
  if (!obj) {
    return false;
  }

  *result = ToPlainDate(obj);
  calendar.set(obj->calendar());
  return cx->compartment()->wrap(cx, calendar);
}

/**
 * TemporalDateToString ( temporalDate, showCalendar )
 */
static JSString* TemporalDateToString(JSContext* cx,
                                      Handle<PlainDateObject*> temporalDate,
                                      CalendarOption showCalendar) {
  auto [year, month, day] = ToPlainDate(temporalDate);

  // Steps 1-2. (Not applicable in our implementation.)

  JSStringBuilder result(cx);

  if (!result.reserve(1 + 6 + 1 + 2 + 1 + 2)) {
    return nullptr;
  }

  // Step 3.
  if (0 <= year && year <= 9999) {
    result.infallibleAppend(char('0' + (year / 1000)));
    result.infallibleAppend(char('0' + (year % 1000) / 100));
    result.infallibleAppend(char('0' + (year % 100) / 10));
    result.infallibleAppend(char('0' + (year % 10)));
  } else {
    result.infallibleAppend(year < 0 ? '-' : '+');

    year = std::abs(year);
    result.infallibleAppend(char('0' + (year / 100000)));
    result.infallibleAppend(char('0' + (year % 100000) / 10000));
    result.infallibleAppend(char('0' + (year % 10000) / 1000));
    result.infallibleAppend(char('0' + (year % 1000) / 100));
    result.infallibleAppend(char('0' + (year % 100) / 10));
    result.infallibleAppend(char('0' + (year % 10)));
  }

  // Step 4.
  result.infallibleAppend('-');
  result.infallibleAppend(char('0' + (month / 10)));
  result.infallibleAppend(char('0' + (month % 10)));

  // Step 5.
  result.infallibleAppend('-');
  result.infallibleAppend(char('0' + (day / 10)));
  result.infallibleAppend(char('0' + (day % 10)));

  // Step 6.
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());
  if (!MaybeFormatCalendarAnnotation(cx, result, calendar, showCalendar)) {
    return nullptr;
  }

  // Step 7.
  return result.finishString();
}

static bool CanBalanceISOYear(double year) {
  // TODO: Export these values somewhere.
  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // If the year is below resp. above the min-/max-year, no value of |day| will
  // make the resulting date valid.
  return minYear <= year && year <= maxYear;
}

static bool CanBalanceISODay(double day) {
  // The maximum number of seconds from the epoch is 8.64 * 10^12.
  constexpr int64_t maxInstantSeconds = 8'640'000'000'000;

  // In days that makes 10^8.
  constexpr int64_t maxInstantDays = maxInstantSeconds / 60 / 60 / 24;

  // Multiply by two to take both directions into account and add twenty to
  // account for the day number of the minimum date "-271821-02-20".
  constexpr int64_t maximumDayDifference = 2 * maxInstantDays + 20;

  // When |day| is below |maximumDayDifference|, it can be represented as int32.
  static_assert(maximumDayDifference <= INT32_MAX);

  // When the day difference exceeds the maximum valid day difference, the
  // overall result won't be a valid date. Detect this early so we don't have to
  // struggle with floating point precision issues in BalanceISODate.
  //
  // This also means BalanceISODate, step 1 doesn't apply to our implementation.
  return std::abs(day) <= maximumDayDifference;
}

/**
 * BalanceISODate ( year, month, day )
 */
PlainDate js::temporal::BalanceISODateNew(int32_t year, int32_t month,
                                          int32_t day) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // Steps 1-3.
  int64_t ms = MakeDate(year, month, day);

  // FIXME: spec issue - |ms| can be non-finite
  // https://github.com/tc39/proposal-temporal/issues/2315

  // TODO: This approach isn't efficient, because MonthFromTime and DayFromTime
  // both recompute YearFromTime.

  // Step 4.
  return {int32_t(JS::YearFromTime(ms)), int32_t(JS::MonthFromTime(ms) + 1),
          int32_t(JS::DayFromTime(ms))};
}

/**
 * BalanceISODate ( year, month, day )
 */
bool js::temporal::BalanceISODate(JSContext* cx, int32_t year, int32_t month,
                                  int64_t day, PlainDate* result) {
  if (!CanBalanceISODay(day)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  *result = BalanceISODate(year, month, int32_t(day));
  return true;
}

/**
 * BalanceISODate ( year, month, day )
 */
PlainDate js::temporal::BalanceISODate(int32_t year, int32_t month,
                                       int32_t day) {
  // Check no inputs can lead to floating point precision issues below. This
  // also ensures all loops can finish in reasonable time, so we don't need to
  // worry about interrupts here. And it ensures there won't be overflows when
  // using int32_t values.
  MOZ_ASSERT(CanBalanceISOYear(year));
  MOZ_ASSERT(1 <= month && month <= 12);
  MOZ_ASSERT(CanBalanceISODay(day));

  // TODO: BalanceISODate now works using MakeDate
  // TODO: Can't use JS::MakeDate, because it expects valid month/day values.
  // https://github.com/tc39/proposal-temporal/issues/2315

  // Step 1. (Not applicable in our implementation.)

  // Steps 3-4. (Not applicable in our implementation.)

  constexpr int32_t daysInNonLeapYear = 365;

  // Skip steps 5-11 for the common case when abs(day) doesn't exceed 365.
  if (std::abs(day) > daysInNonLeapYear) {
    // Step 5. (Note)

    // Steps 6-7.
    int32_t testYear = month > 2 ? year : year - 1;

    // Step 8.
    while (day < -ISODaysInYear(testYear)) {
      // Step 8.a.
      day += ISODaysInYear(testYear);

      // Step 8.b.
      year -= 1;

      // Step 8.c.
      testYear -= 1;
    }

    // Step 9. (Note)

    // Step 10.
    testYear += 1;

    // Step 11.
    while (day > ISODaysInYear(testYear)) {
      // Step 11.a.
      day -= ISODaysInYear(testYear);

      // Step 11.b.
      year += 1;

      // Step 11.c.
      testYear += 1;
    }
  }

  // Step 12. (Note)

  // Step 13.
  while (day < 1) {
    // Steps 13.a-b. (Inlined call to BalanceISOYearMonth.)
    if (--month == 0) {
      month = 12;
      year -= 1;
    }

    // Step 13.d
    day += ISODaysInMonth(year, month);
  }

  // Step 14. (Note)

  // Step 15.
  while (day > ISODaysInMonth(year, month)) {
    // Step 15.a.
    day -= ISODaysInMonth(year, month);

    // Steps 15.b-d. (Inlined call to BalanceISOYearMonth.)
    if (++month == 13) {
      month = 1;
      year += 1;
    }
  }

  MOZ_ASSERT(1 <= month && month <= 12);
  MOZ_ASSERT(1 <= day && day <= 31);

  // Step 16.
  return {year, month, day};
}

/**
 * Temporal.PlainDate ( isoYear, isoMonth, isoDay [ , calendarLike ] )
 */
static bool PlainDateConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainDate")) {
    return false;
  }

  // Step 2.
  double isoYear;
  if (!ToIntegerWithTruncation(cx, args.get(0), "year", &isoYear)) {
    return false;
  }

  // Step 3.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(1), "month", &isoMonth)) {
    return false;
  }

  // Step 4.
  double isoDay;
  if (!ToIntegerWithTruncation(cx, args.get(2), "day", &isoDay)) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(3)));
  if (!calendar) {
    return false;
  }

  // Step 6.
  auto* temporalDate =
      CreateTemporalDate(cx, args, isoYear, isoMonth, isoDay, calendar);
  if (!temporalDate) {
    return false;
  }

  args.rval().setObject(*temporalDate);
  return true;
}

/**
 * Temporal.PlainDate.from ( item [ , options ] )
 */
static bool PlainDate_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(1)) {
    options = RequireObjectArg(cx, "options", "from", args[1]);
    if (!options) {
      return false;
    }
  }

  // Step 2.
  if (args.get(0).isObject()) {
    JSObject* item = &args[0].toObject();
    if (auto* temporalDate = item->maybeUnwrapIf<PlainDateObject>()) {
      auto date = ToPlainDate(temporalDate);

      Rooted<JSObject*> calendar(cx, temporalDate->calendar());
      if (!cx->compartment()->wrap(cx, &calendar)) {
        return false;
      }

      if (options) {
        // Step 2.a.
        TemporalOverflow ignored;
        if (!ToTemporalOverflow(cx, options, &ignored)) {
          return false;
        }
      }

      // Step 2.b.
      auto* result = CreateTemporalDate(cx, date, calendar);
      if (!result) {
        return false;
      }

      args.rval().setObject(*result);
      return true;
    }
  }

  // Step 3.
  auto result = ToTemporalDate(cx, args.get(0), options);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * get Temporal.PlainDate.prototype.calendar
 */
static bool PlainDate_calendar(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  args.rval().setObject(*temporalDate->calendar());
  return true;
}

/**
 * get Temporal.PlainDate.prototype.calendar
 */
static bool PlainDate_calendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_calendar>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.year
 */
static bool PlainDate_year(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.year
 */
static bool PlainDate_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_year>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.month
 */
static bool PlainDate_month(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarMonth(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.month
 */
static bool PlainDate_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_month>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.monthCode
 */
static bool PlainDate_monthCode(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarMonthCode(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.monthCode
 */
static bool PlainDate_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_monthCode>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.day
 */
static bool PlainDate_day(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDay(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.day
 */
static bool PlainDate_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_day>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.dayOfWeek
 */
static bool PlainDate_dayOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDayOfWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.dayOfWeek
 */
static bool PlainDate_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_dayOfWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.dayOfYear
 */
static bool PlainDate_dayOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDayOfYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.dayOfYear
 */
static bool PlainDate_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_dayOfYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.weekOfYear
 */
static bool PlainDate_weekOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarWeekOfYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.weekOfYear
 */
static bool PlainDate_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_weekOfYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.yearOfWeek
 */
static bool PlainDate_yearOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarYearOfWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.yearOfWeek
 */
static bool PlainDate_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_yearOfWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInWeek
 */
static bool PlainDate_daysInWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDaysInWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInWeek
 */
static bool PlainDate_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInWeek>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInMonth
 */
static bool PlainDate_daysInMonth(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDaysInMonth(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInMonth
 */
static bool PlainDate_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInMonth>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.daysInYear
 */
static bool PlainDate_daysInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarDaysInYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.daysInYear
 */
static bool PlainDate_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_daysInYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.monthsInYear
 */
static bool PlainDate_monthsInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarMonthsInYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.monthsInYear
 */
static bool PlainDate_monthsInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_monthsInYear>(cx, args);
}

/**
 * get Temporal.PlainDate.prototype.inLeapYear
 */
static bool PlainDate_inLeapYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  return CalendarInLeapYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDate.prototype.inLeapYear
 */
static bool PlainDate_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_inLeapYear>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toPlainYearMonth ( )
 */
static bool PlainDate_toPlainYearMonth(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::MonthCode, CalendarField::Year},
                      &fieldNames)) {
    return false;
  }

  // Step 5.
  Rooted<PlainObject*> fields(
      cx, PrepareTemporalFields(cx, temporalDate, fieldNames));
  if (!fields) {
    return false;
  }

  // Step 6.
  auto obj = CalendarYearMonthFromFields(cx, calendar, fields);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainYearMonth ( )
 */
static bool PlainDate_toPlainYearMonth(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainYearMonth>(cx,
                                                                       args);
}

/**
 * Temporal.PlainDate.prototype.toPlainMonthDay ( )
 */
static bool PlainDate_toPlainMonthDay(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Step 4.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::MonthCode},
                      &fieldNames)) {
    return false;
  }

  // Step 5.
  Rooted<PlainObject*> fields(
      cx, PrepareTemporalFields(cx, temporalDate, fieldNames));
  if (!fields) {
    return false;
  }

  // Step 6.
  auto obj = CalendarMonthDayFromFields(cx, calendar, fields);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainMonthDay ( )
 */
static bool PlainDate_toPlainMonthDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainMonthDay>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toPlainDateTime ( [ temporalTime ] )
 */
static bool PlainDate_toPlainDateTime(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  Rooted<JSObject*> calendar(cx, temporalDate->calendar());

  // Default initialize the time component to all zero.
  PlainDateTime dateTime = {ToPlainDate(temporalDate), {}};

  // Step 4. (Reordered)
  if (args.hasDefined(0)) {
    if (!ToTemporalTime(cx, args[0], &dateTime.time)) {
      return false;
    }
  }

  // Steps 3 and 5.
  auto* obj = CreateTemporalDateTime(cx, dateTime, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toPlainDateTime ( [ temporalTime ] )
 */
static bool PlainDate_toPlainDateTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toPlainDateTime>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.getISOFields ( )
 */
static bool PlainDate_getISOFields(JSContext* cx, const CallArgs& args) {
  auto* temporalDate = &args.thisv().toObject().as<PlainDateObject>();
  auto date = ToPlainDate(temporalDate);
  JSObject* calendar = temporalDate->calendar();

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoDay), Int32Value(date.day))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoMonth),
                          Int32Value(date.month))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoYear),
                          Int32Value(date.year))) {
    return false;
  }

  // Step 8.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDate.prototype.getISOFields ( )
 */
static bool PlainDate_getISOFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_getISOFields>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toString ( [ options ] )
 */
static bool PlainDate_toString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  auto showCalendar = CalendarOption::Auto;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Step 4.
    if (!ToCalendarNameOption(cx, options, &showCalendar)) {
      return false;
    }
  }

  // Step 5.
  JSString* str = TemporalDateToString(cx, temporalDate, showCalendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toString ( [ options ] )
 */
static bool PlainDate_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toString>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDate_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  JSString* str = TemporalDateToString(cx, temporalDate, CalendarOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainDate_toLocaleString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toLocaleString>(cx, args);
}

/**
 * Temporal.PlainDate.prototype.toJSON ( )
 */
static bool PlainDate_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateObject*> temporalDate(
      cx, &args.thisv().toObject().as<PlainDateObject>());

  // Step 3.
  JSString* str = TemporalDateToString(cx, temporalDate, CalendarOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainDate.prototype.toJSON ( )
 */
static bool PlainDate_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDate, PlainDate_toJSON>(cx, args);
}

/**
 *  Temporal.PlainDate.prototype.valueOf ( )
 */
static bool PlainDate_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainDate", "primitive type");
  return false;
}

const JSClass PlainDateObject::class_ = {
    "Temporal.PlainDate",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDate),
    JS_NULL_CLASS_OPS,
    &PlainDateObject::classSpec_,
};

const JSClass& PlainDateObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDate_methods[] = {
    JS_FN("from", PlainDate_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainDate_prototype_methods[] = {
    JS_FN("toPlainMonthDay", PlainDate_toPlainMonthDay, 0, 0),
    JS_FN("toPlainYearMonth", PlainDate_toPlainYearMonth, 0, 0),
    JS_FN("toPlainDateTime", PlainDate_toPlainDateTime, 0, 0),
    JS_FN("getISOFields", PlainDate_getISOFields, 0, 0),
    JS_FN("toString", PlainDate_toString, 0, 0),
    JS_FN("toLocaleString", PlainDate_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainDate_toJSON, 0, 0),
    JS_FN("valueOf", PlainDate_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainDate_prototype_properties[] = {
    JS_PSG("calendar", PlainDate_calendar, 0),
    JS_PSG("year", PlainDate_year, 0),
    JS_PSG("month", PlainDate_month, 0),
    JS_PSG("monthCode", PlainDate_monthCode, 0),
    JS_PSG("day", PlainDate_day, 0),
    JS_PSG("dayOfWeek", PlainDate_dayOfWeek, 0),
    JS_PSG("dayOfYear", PlainDate_dayOfYear, 0),
    JS_PSG("weekOfYear", PlainDate_weekOfYear, 0),
    JS_PSG("yearOfWeek", PlainDate_yearOfWeek, 0),
    JS_PSG("daysInWeek", PlainDate_daysInWeek, 0),
    JS_PSG("daysInMonth", PlainDate_daysInMonth, 0),
    JS_PSG("daysInYear", PlainDate_daysInYear, 0),
    JS_PSG("monthsInYear", PlainDate_monthsInYear, 0),
    JS_PSG("inLeapYear", PlainDate_inLeapYear, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainDate", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainDateObject::classSpec_ = {
    GenericCreateConstructor<PlainDateConstructor, 3, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateObject>,
    PlainDate_methods,
    nullptr,
    PlainDate_prototype_methods,
    PlainDate_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
