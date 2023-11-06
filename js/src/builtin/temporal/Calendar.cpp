/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/Calendar.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "jsfriendapi.h"
#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/Array.h"
#include "builtin/String.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/Wrapped.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "gc/GCEnum.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCAPI.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Value.h"
#include "js/ValueArray.h"
#include "util/Text.h"
#include "vm/ArrayObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PIC.h"
#include "vm/PlainObject.h"
#include "vm/PropertyInfo.h"
#include "vm/PropertyKey.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/Stack.h"
#include "vm/StringType.h"

#include "vm/Compartment-inl.h"
#include "vm/JSAtomUtils-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsCalendar(Handle<Value> v) {
  return v.isObject() && v.toObject().is<CalendarObject>();
}

void js::temporal::CalendarValue::trace(JSTracer* trc) {
  TraceRoot(trc, &value_, "CalendarValue::value");
}

bool js::temporal::WrapCalendarValue(JSContext* cx,
                                     JS::MutableHandle<JS::Value> calendar) {
  MOZ_ASSERT(calendar.isString() || calendar.isObject());
  return cx->compartment()->wrap(cx, calendar);
}

/**
 * IteratorToListOfType ( iteratorRecord, elementTypes )
 *
 * With `elementTypes = « String »`.
 *
 * This implementation accepts an iterable instead of an iterator record.
 */
static bool IterableToListOfStrings(JSContext* cx, Handle<Value> items,
                                    MutableHandle<CalendarFieldNames> list) {
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(items)) {
    return false;
  }

  // Step 1. (Not applicable in our implementation.)

  // Steps 2-3.
  Rooted<Value> nextValue(cx);
  Rooted<PropertyKey> value(cx);
  while (true) {
    bool done;
    if (!iterator.next(&nextValue, &done)) {
      return false;
    }
    if (done) {
      break;
    }

    if (nextValue.isString()) {
      if (!PrimitiveValueToId<CanGC>(cx, nextValue, &value)) {
        return false;
      }
      if (!list.append(value)) {
        return false;
      }
      continue;
    }

    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, nextValue,
                     nullptr, "not a string");

    iterator.closeThrow();
    return false;
  }

  // Step 4.
  return true;
}

/**
 * IsISOLeapYear ( year )
 */
static constexpr bool IsISOLeapYear(int32_t year) {
  // Steps 1-5.
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

/**
 * IsISOLeapYear ( year )
 */
static bool IsISOLeapYear(double year) {
  // Step 1.
  MOZ_ASSERT(IsInteger(year));

  // Steps 2-5.
  return std::fmod(year, 4) == 0 &&
         (std::fmod(year, 100) != 0 || std::fmod(year, 400) == 0);
}

/**
 * ISODaysInYear ( year )
 */
int32_t js::temporal::ISODaysInYear(int32_t year) {
  // Steps 1-3.
  return IsISOLeapYear(year) ? 366 : 365;
}

/**
 * ISODaysInMonth ( year, month )
 */
static constexpr int32_t ISODaysInMonth(int32_t year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(int32_t year, int32_t month) {
  return ::ISODaysInMonth(year, month);
}

/**
 * ISODaysInMonth ( year, month )
 */
int32_t js::temporal::ISODaysInMonth(double year, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  static constexpr uint8_t daysInMonth[2][13] = {
      {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
      {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};

  // Steps 1-4.
  return daysInMonth[IsISOLeapYear(year)][month];
}

/**
 * 21.4.1.6 Week Day
 *
 * Compute the week day from |day| without first expanding |day| into a full
 * date through |MakeDate(day, 0)|:
 *
 *   WeekDay(MakeDate(day, 0))
 * = WeekDay(day × msPerDay + 0)
 * = WeekDay(day × msPerDay)
 * = 𝔽(ℝ(Day(day × msPerDay) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(floor(ℝ((day × msPerDay) / msPerDay))) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(floor(ℝ(day))) + 4𝔽) modulo 7)
 * = 𝔽(ℝ(𝔽(day) + 4𝔽) modulo 7)
 */
static int32_t WeekDay(int32_t day) {
  int32_t result = (day + 4) % 7;
  if (result < 0) {
    result += 7;
  }
  return result;
}

/**
 * ToISODayOfWeek ( year, month, day )
 */
static int32_t ToISODayOfWeek(const PlainDate& date) {
  MOZ_ASSERT(ISODateTimeWithinLimits(date));

  // Steps 1-3. (Not applicable in our implementation.)

  // TODO: Check if ES MakeDate + WeekDay is efficient enough.
  //
  // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Methods_in_computer_code

  // Step 4.
  int32_t day = MakeDay(date);

  // Step 5.
  int32_t weekday = WeekDay(day);
  return weekday != 0 ? weekday : 7;
}

static constexpr auto FirstDayOfMonth(int32_t year) {
  // The following array contains the day of year for the first day of each
  // month, where index 0 is January, and day 0 is January 1.
  std::array<int32_t, 13> days = {};
  for (int32_t month = 1; month <= 12; ++month) {
    days[month] = days[month - 1] + ::ISODaysInMonth(year, month);
  }
  return days;
}

/**
 * ToISODayOfYear ( year, month, day )
 */
static int32_t ToISODayOfYear(int32_t year, int32_t month, int32_t day) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // First day of month arrays for non-leap and leap years.
  constexpr decltype(FirstDayOfMonth(0)) firstDayOfMonth[2] = {
      FirstDayOfMonth(1), FirstDayOfMonth(0)};

  // Steps 1-3. (Not applicable in our implementation.)

  // Steps 4-5.
  //
  // Instead of first computing the date and then using DayWithinYear to map the
  // date to the day within the year, directly lookup the first day of the month
  // and then add the additional days.
  return firstDayOfMonth[IsISOLeapYear(year)][month - 1] + day;
}

/**
 * ToISODayOfYear ( year, month, day )
 */
int32_t js::temporal::ToISODayOfYear(const PlainDate& date) {
  MOZ_ASSERT(ISODateTimeWithinLimits(date));

  // Steps 1-5.
  auto& [year, month, day] = date;
  return ::ToISODayOfYear(year, month, day);
}

static int32_t FloorDiv(int32_t dividend, int32_t divisor) {
  MOZ_ASSERT(divisor > 0);

  int32_t quotient = dividend / divisor;
  int32_t remainder = dividend % divisor;
  if (remainder < 0) {
    quotient -= 1;
  }
  return quotient;
}

/**
 * 21.4.1.3 Year Number, DayFromYear
 */
static int32_t DayFromYear(int32_t year) {
  return 365 * (year - 1970) + FloorDiv(year - 1969, 4) -
         FloorDiv(year - 1901, 100) + FloorDiv(year - 1601, 400);
}

/**
 * 21.4.1.11 MakeTime ( hour, min, sec, ms )
 */
static int64_t MakeTime(const PlainTime& time) {
  MOZ_ASSERT(IsValidTime(time));

  // Step 1 (Not applicable).

  // Step 2.
  int64_t h = time.hour;

  // Step 3.
  int64_t m = time.minute;

  // Step 4.
  int64_t s = time.second;

  // Step 5.
  int64_t milli = time.millisecond;

  // Steps 6-7.
  return h * ToMilliseconds(TemporalUnit::Hour) +
         m * ToMilliseconds(TemporalUnit::Minute) +
         s * ToMilliseconds(TemporalUnit::Second) + milli;
}

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
int32_t js::temporal::MakeDay(const PlainDate& date) {
  MOZ_ASSERT(ISODateTimeWithinLimits(date));

  return DayFromYear(date.year) + ToISODayOfYear(date) - 1;
}

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t js::temporal::MakeDate(const PlainDateTime& dateTime) {
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  // Step 1 (Not applicable).

  // Steps 2-3.
  int64_t tv = MakeDay(dateTime.date) * ToMilliseconds(TemporalUnit::Day) +
               MakeTime(dateTime.time);

  // Step 4.
  return tv;
}

/**
 * 21.4.1.12 MakeDay ( year, month, date )
 */
static int32_t MakeDay(int32_t year, int32_t month, int32_t day) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // FIXME: spec issue - what should happen for invalid years/days?
  return DayFromYear(year) + ::ToISODayOfYear(year, month, day) - 1;
}

/**
 * 21.4.1.13 MakeDate ( day, time )
 */
int64_t js::temporal::MakeDate(int32_t year, int32_t month, int32_t day) {
  // NOTE: This version accepts values outside the valid date-time limits.
  MOZ_ASSERT(1 <= month && month <= 12);

  // Step 1 (Not applicable).

  // Steps 2-3.
  int64_t tv = ::MakeDay(year, month, day) * ToMilliseconds(TemporalUnit::Day);

  // Step 4.
  return tv;
}

struct YearWeek final {
  int32_t year = 0;
  int32_t week = 0;
};

/**
 * ToISOWeekOfYear ( year, month, day )
 */
static YearWeek ToISOWeekOfYear(const PlainDate& date) {
  MOZ_ASSERT(ISODateTimeWithinLimits(date));

  auto& [year, month, day] = date;

  // TODO: https://en.wikipedia.org/wiki/Week#The_ISO_week_date_system
  // TODO: https://en.wikipedia.org/wiki/ISO_week_date#Algorithms

  // Steps 1-3. (Not applicable in our implementation.)

  // Steps 4-5.
  int32_t doy = ToISODayOfYear(date);
  int32_t dow = ToISODayOfWeek(date);

  int32_t woy = (10 + doy - dow) / 7;
  MOZ_ASSERT(0 <= woy && woy <= 53);

  // An ISO year has 53 weeks if the year starts on a Thursday or if it's a
  // leap year which starts on a Wednesday.
  auto isLongYear = [](int32_t year) {
    int32_t startOfYear = ToISODayOfWeek({year, 1, 1});
    return startOfYear == 4 || (startOfYear == 3 && IsISOLeapYear(year));
  };

  // Part of last year's last week, which is either week 52 or week 53.
  if (woy == 0) {
    return {year - 1, 52 + int32_t(isLongYear(year - 1))};
  }

  // Part of next year's first week if the current year isn't a long year.
  if (woy == 53 && !isLongYear(year)) {
    return {year + 1, 1};
  }

  return {year, woy};
}

/**
 * ISOMonthCode ( month )
 */
static JSString* ISOMonthCode(JSContext* cx, int32_t month) {
  MOZ_ASSERT(1 <= month && month <= 12);

  // Steps 1-2.
  char monthCode[3] = {'M', char('0' + (month / 10)), char('0' + (month % 10))};
  return NewStringCopyN<CanGC>(cx, monthCode, std::size(monthCode));
}

template <typename T, typename... Ts>
static bool ToPlainDate(JSObject* temporalDateLike, PlainDate* result) {
  if (auto* obj = temporalDateLike->maybeUnwrapIf<T>()) {
    *result = ToPlainDate(obj);
    return true;
  }
  if constexpr (sizeof...(Ts) > 0) {
    return ToPlainDate<Ts...>(temporalDateLike, result);
  }
  return false;
}

template <typename... Ts>
static bool ToPlainDate(JSContext* cx, Handle<Value> temporalDateLike,
                        PlainDate* result) {
  if (temporalDateLike.isObject()) {
    if (ToPlainDate<Ts...>(&temporalDateLike.toObject(), result)) {
      return true;
    }
  }

  return ToTemporalDate(cx, temporalDateLike, result);
}

#ifdef DEBUG
template <typename CharT>
static bool StringIsAsciiLowerCase(mozilla::Range<CharT> str) {
  return std::all_of(str.begin().get(), str.end().get(), [](CharT ch) {
    return mozilla::IsAscii(ch) && !mozilla::IsAsciiUppercaseAlpha(ch);
  });
}

static bool StringIsAsciiLowerCase(JSLinearString* str) {
  JS::AutoCheckCannotGC nogc;
  return str->hasLatin1Chars()
             ? StringIsAsciiLowerCase(str->latin1Range(nogc))
             : StringIsAsciiLowerCase(str->twoByteRange(nogc));
}
#endif

static bool IsISO8601Calendar(JSLinearString* id) {
  return StringEqualsLiteral(id, "iso8601");
}

#ifdef DEBUG
static bool IsISO8601Calendar(CalendarObject* calendar) {
  return IsISO8601Calendar(calendar->identifier());
}
#endif

static bool IsISO8601Calendar(JSContext* cx, JSString* id, bool* result) {
  JSLinearString* linear = id->ensureLinear(cx);
  if (!linear) {
    return false;
  }
  *result = IsISO8601Calendar(linear);
  return true;
}

/**
 * IsBuiltinCalendar ( id )
 */
static bool IsBuiltinCalendar(JSLinearString* id) {
  // Callers must convert to lower case.
  MOZ_ASSERT(StringIsAsciiLowerCase(id));

  // Steps 1-3.
  return StringEqualsLiteral(id, "iso8601");
}

static JSLinearString* ThrowIfNotBuiltinCalendar(JSContext* cx,
                                                 Handle<JSLinearString*> id) {
  if (!StringIsAscii(id)) {
    if (auto chars = QuoteString(cx, id)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_ID, chars.get());
    }
    return nullptr;
  }

  JSString* lower = StringToLowerCase(cx, id);
  if (!lower) {
    return nullptr;
  }

  JSLinearString* linear = lower->ensureLinear(cx);
  if (!linear) {
    return nullptr;
  }

  if (!IsBuiltinCalendar(linear)) {
    if (auto chars = QuoteString(cx, id)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_ID, chars.get());
    }
    return nullptr;
  }

  return linear;
}

bool js::temporal::ToBuiltinCalendar(JSContext* cx, Handle<JSString*> id,
                                     MutableHandle<CalendarValue> result) {
  Rooted<JSLinearString*> linear(cx, id->ensureLinear(cx));
  if (!linear) {
    return false;
  }

  auto* identifier = ThrowIfNotBuiltinCalendar(cx, linear);
  if (!identifier) {
    return false;
  }

  result.set(CalendarValue(identifier));
  return true;
}

/**
 * CreateTemporalCalendar ( identifier [ , newTarget ] )
 */
static CalendarObject* CreateTemporalCalendar(
    JSContext* cx, const CallArgs& args, Handle<JSLinearString*> identifier) {
  // Step 1.
  MOZ_ASSERT(IsBuiltinCalendar(identifier));

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_Calendar, &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<CalendarObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  obj->setFixedSlot(CalendarObject::IDENTIFIER_SLOT, StringValue(identifier));

  // Step 5.
  return obj;
}

/**
 * CreateTemporalCalendar ( identifier [ , newTarget ] )
 */
static CalendarObject* CreateTemporalCalendar(
    JSContext* cx, Handle<JSLinearString*> identifier) {
  // Step 1.
  MOZ_ASSERT(IsBuiltinCalendar(identifier));

  // Steps 2-3.
  auto* obj = NewBuiltinClassInstance<CalendarObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  obj->setFixedSlot(CalendarObject::IDENTIFIER_SLOT, StringValue(identifier));

  // Step 5.
  return obj;
}

/**
 * ObjectImplementsTemporalCalendarProtocol ( object )
 */
static bool ObjectImplementsTemporalCalendarProtocol(JSContext* cx,
                                                     Handle<JSObject*> object,
                                                     bool* result) {
  // Step 1. (Not applicable in our implementation.)
  MOZ_ASSERT(!object->canUnwrapAs<CalendarObject>(),
             "Calendar objects handled in the caller");

  // Step 2.
  for (auto key : {
           &JSAtomState::dateAdd,      &JSAtomState::dateFromFields,
           &JSAtomState::dateUntil,    &JSAtomState::day,
           &JSAtomState::dayOfWeek,    &JSAtomState::dayOfYear,
           &JSAtomState::daysInMonth,  &JSAtomState::daysInWeek,
           &JSAtomState::daysInYear,   &JSAtomState::fields,
           &JSAtomState::id,           &JSAtomState::inLeapYear,
           &JSAtomState::mergeFields,  &JSAtomState::month,
           &JSAtomState::monthCode,    &JSAtomState::monthDayFromFields,
           &JSAtomState::monthsInYear, &JSAtomState::weekOfYear,
           &JSAtomState::year,         &JSAtomState::yearMonthFromFields,
           &JSAtomState::yearOfWeek,
       }) {
    // Step 2.a.
    bool has;
    if (!HasProperty(cx, object, cx->names().*key, &has)) {
      return false;
    }
    if (!has) {
      *result = false;
      return true;
    }
  }

  // Step 3.
  *result = true;
  return true;
}

template <typename T, typename... Ts>
static bool ToTemporalCalendar(JSContext* cx, Handle<JSObject*> object,
                               MutableHandle<CalendarValue> result) {
  if (auto* unwrapped = object->maybeUnwrapIf<T>()) {
    result.set(unwrapped->calendar());
    return result.wrap(cx);
  }

  if constexpr (sizeof...(Ts) > 0) {
    return ToTemporalCalendar<Ts...>(cx, object, result);
  }

  result.set(CalendarValue());
  return true;
}

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 */
bool js::temporal::ToTemporalCalendar(JSContext* cx,
                                      Handle<Value> temporalCalendarLike,
                                      MutableHandle<CalendarValue> result) {
  // Step 1. (Not applicable)

  // Step 2.
  Rooted<Value> calendarLike(cx, temporalCalendarLike);
  if (calendarLike.isObject()) {
    Rooted<JSObject*> obj(cx, &calendarLike.toObject());

    // Step 2.b. (Partial)
    if (obj->canUnwrapAs<CalendarObject>()) {
      result.set(CalendarValue(obj));
      return true;
    }

    // Step 2.a.
    Rooted<CalendarValue> calendar(cx);
    if (!::ToTemporalCalendar<PlainDateObject, PlainDateTimeObject,
                              PlainMonthDayObject, PlainYearMonthObject,
                              ZonedDateTimeObject>(cx, obj, &calendar)) {
      return false;
    }
    if (calendar) {
      result.set(calendar);
      return true;
    }

    // Step 2.b.
    bool implementsCalendarProtocol;
    if (!ObjectImplementsTemporalCalendarProtocol(
            cx, obj, &implementsCalendarProtocol)) {
      return false;
    }
    if (!implementsCalendarProtocol) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_INVALID_OBJECT,
                               "Temporal.Calendar", obj->getClass()->name);
      return false;
    }

    // Step 2.c.
    result.set(CalendarValue(obj));
    return true;
  }

  // Step 3.
  if (!calendarLike.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK,
                     calendarLike, nullptr, "not a string");
    return false;
  }
  Rooted<JSString*> str(cx, calendarLike.toString());

  // Step 4.
  Rooted<JSLinearString*> identifier(cx, ParseTemporalCalendarString(cx, str));
  if (!identifier) {
    return false;
  }

  // Step 5.
  identifier = ThrowIfNotBuiltinCalendar(cx, identifier);
  if (!identifier) {
    return false;
  }

  // Step 6.
  result.set(CalendarValue(identifier));
  return true;
}

/**
 * ToTemporalCalendarSlotValue ( temporalCalendarLike [ , default ] )
 *
 * When called with `default = "iso8601"`.
 */
bool js::temporal::ToTemporalCalendarWithISODefault(
    JSContext* cx, Handle<Value> temporalCalendarLike,
    MutableHandle<CalendarValue> result) {
  // Step 1.
  if (temporalCalendarLike.isUndefined()) {
    result.set(CalendarValue(cx->names().iso8601));
    return true;
  }

  // Steps 2-6.
  return ToTemporalCalendar(cx, temporalCalendarLike, result);
}

/**
 * GetTemporalCalendarSlotValueWithISODefault ( item )
 */
bool js::temporal::GetTemporalCalendarWithISODefault(
    JSContext* cx, Handle<JSObject*> item,
    MutableHandle<CalendarValue> result) {
  // Step 1.
  Rooted<CalendarValue> calendar(cx);
  if (!::ToTemporalCalendar<PlainDateObject, PlainDateTimeObject,
                            PlainMonthDayObject, PlainYearMonthObject,
                            ZonedDateTimeObject>(cx, item, &calendar)) {
    return false;
  }
  if (calendar) {
    result.set(calendar);
    return true;
  }

  // Step 2.
  Rooted<Value> calendarValue(cx);
  if (!GetProperty(cx, item, item, cx->names().calendar, &calendarValue)) {
    return false;
  }

  // Step 3.
  return ToTemporalCalendarWithISODefault(cx, calendarValue, result);
}

/**
 * ToTemporalCalendarIdentifier ( calendarSlotValue )
 */
JSString* js::temporal::ToTemporalCalendarIdentifier(
    JSContext* cx, Handle<CalendarValue> calendar) {
  // Step 1.
  if (calendar.isString()) {
    // Step 1.a.
    MOZ_ASSERT(IsBuiltinCalendar(calendar.toString()));

    // Step 1.b.
    return calendar.toString();
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> identifier(cx);
  if (!GetProperty(cx, calendarObj, calendarObj, cx->names().id, &identifier)) {
    return nullptr;
  }

  // Step 3.
  if (!identifier.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, identifier,
                     nullptr, "not a string");
    return nullptr;
  }

  // Step 4.
  return identifier.toString();
}

/**
 * ToTemporalCalendarObject ( calendarSlotValue )
 */
JSObject* js::temporal::ToTemporalCalendarObject(
    JSContext* cx, Handle<CalendarValue> calendar) {
  // Step 1.
  if (calendar.isObject()) {
    return calendar.toObject();
  }

  // Step 2.
  Rooted<JSLinearString*> calendarId(cx, calendar.toString());
  return CreateTemporalCalendar(cx, calendarId);
}

static bool ToCalendarField(JSContext* cx, JSLinearString* linear,
                            CalendarField* result) {
  if (StringEqualsLiteral(linear, "year")) {
    *result = CalendarField::Year;
    return true;
  }
  if (StringEqualsLiteral(linear, "month")) {
    *result = CalendarField::Month;
    return true;
  }
  if (StringEqualsLiteral(linear, "monthCode")) {
    *result = CalendarField::MonthCode;
    return true;
  }
  if (StringEqualsLiteral(linear, "day")) {
    *result = CalendarField::Day;
    return true;
  }
  if (auto chars = QuoteString(cx, linear, '"')) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_TEMPORAL_CALENDAR_INVALID_FIELD,
                             chars.get());
  }
  return false;
}

static PropertyName* ToPropertyName(JSContext* cx, CalendarField field) {
  switch (field) {
    case CalendarField::Year:
      return cx->names().year;
    case CalendarField::Month:
      return cx->names().month;
    case CalendarField::MonthCode:
      return cx->names().monthCode;
    case CalendarField::Day:
      return cx->names().day;
  }
  MOZ_CRASH("invalid calendar field name");
}

#ifdef DEBUG
static const char* ToCString(CalendarField field) {
  switch (field) {
    case CalendarField::Year:
      return "year";
    case CalendarField::Month:
      return "month";
    case CalendarField::MonthCode:
      return "monthCode";
    case CalendarField::Day:
      return "day";
  }
  MOZ_CRASH("invalid calendar field name");
}
#endif

/**
 * Temporal.Calendar.prototype.fields ( fields )
 */
static bool BuiltinCalendarFields(
    JSContext* cx, std::initializer_list<CalendarField> fieldNames,
    CalendarFieldNames& result) {
  MOZ_ASSERT(result.empty());

  // Steps 1-5. (Not applicable.)

  // Reserve space for the append operation.
  if (!result.reserve(fieldNames.size())) {
    return false;
  }

  // Steps 6-7.
  for (auto fieldName : fieldNames) {
    auto* name = ToPropertyName(cx, fieldName);

    // Steps 7.a and 7.b.i-iv. (Not applicable)

    // Step 7.b.v.
    result.infallibleAppend(NameToId(name));
  }

  // Step 8. (Not applicable)
  return true;
}

static bool Calendar_fields(JSContext* cx, unsigned argc, Value* vp);

#ifdef DEBUG
static bool IsSorted(std::initializer_list<CalendarField> fieldNames) {
  return std::is_sorted(fieldNames.begin(), fieldNames.end(),
                        [](auto x, auto y) {
                          auto* a = ToCString(x);
                          auto* b = ToCString(y);
                          return std::strcmp(a, b) < 0;
                        });
}
#endif

/**
 * CalendarFields ( calendar, fieldNames )
 */
bool js::temporal::CalendarFields(
    JSContext* cx, Handle<CalendarValue> calendar,
    std::initializer_list<CalendarField> fieldNames,
    MutableHandle<CalendarFieldNames> result) {
  // FIXME: spec issue - the input is already sorted, let's assert this, too.
  MOZ_ASSERT(IsSorted(fieldNames));

  // FIXME: spec issue - the input shouldn't have duplicate elements. Let's
  // assert this, too.
  MOZ_ASSERT(std::adjacent_find(fieldNames.begin(), fieldNames.end()) ==
             fieldNames.end());

  // Step 1.
  if (calendar.isString()) {
    // Step 1.a. (Not applicable in our implementation.)

    // Step 1.b.
    MOZ_ASSERT(IsISO8601Calendar(calendar.toString()));
    return BuiltinCalendarFields(cx, fieldNames, result.get());

    // Steps 1.c-d. (Not applicable in our implementation.)
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fields(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().fields, &fields)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fields, Calendar_fields)) {
    ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
    if (!stubChain) {
      return false;
    }

    bool arrayIterationSane;
    if (!stubChain->tryOptimizeArray(cx, &arrayIterationSane)) {
      return false;
    }

    if (arrayIterationSane) {
      return BuiltinCalendarFields(cx, fieldNames, result.get());
    }
  }

  auto* array = NewDenseFullyAllocatedArray(cx, fieldNames.size());
  if (!array) {
    return false;
  }
  array->setDenseInitializedLength(fieldNames.size());

  for (size_t i = 0; i < fieldNames.size(); i++) {
    auto* name = ToPropertyName(cx, fieldNames.begin()[i]);
    array->initDenseElement(i, StringValue(name));
  }

  Rooted<Value> fieldsArray(cx, ObjectValue(*array));
  if (!Call(cx, fields, calendarObj, fieldsArray, &fieldsArray)) {
    return false;
  }

  // Steps 3-4.
  if (!IterableToListOfStrings(cx, fieldsArray, result)) {
    return false;
  }

  // The spec sorts the field names in PrepareTemporalFields. Sorting is only
  // needed for user-defined calendars, so our implementation performs this step
  // here instead of in PrepareTemporalFields.
  return SortTemporalFieldNames(cx, result.get());
}

static bool ToIntegerIfIntegralNumber(JSContext* cx, Handle<Value> value,
                                      const char* name,
                                      MutableHandle<Value> result) {
  if (MOZ_LIKELY(value.isInt32())) {
    result.set(value);
    return true;
  }

  if (value.isDouble()) {
    double d = value.toDouble();
    if (js::IsInteger(d)) {
      result.setNumber(d);
      return true;
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_INTEGER, name);
    return false;
  }

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                            name, "not a number");
  return false;
}

static bool ToPositiveIntegerIfIntegralNumber(JSContext* cx,
                                              Handle<Value> value,
                                              const char* name,
                                              MutableHandle<Value> result) {
  if (!ToIntegerIfIntegralNumber(cx, value, name, result)) {
    return false;
  }

  if (result.toNumber() <= 0) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INVALID_NUMBER, name);
    return false;
  }
  return true;
}

/**
 * Temporal.Calendar.prototype.year ( temporalDateLike )
 */
static bool BuiltinCalendarYear(const PlainDate& date,
                                MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  result.setInt32(date.year);
  return true;
}

static bool Calendar_year(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarYear ( calendar, dateLike )
 */
static bool CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                         Handle<JSObject*> dateLike, const PlainDate& date,
                         MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().year, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_year)) {
    return BuiltinCalendarYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-5.
  return ToIntegerIfIntegralNumber(cx, result, "year", result);
}

/**
 * CalendarYear ( calendar, dateLike )
 */
bool js::temporal::CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                                Handle<PlainDateObject*> dateLike,
                                MutableHandle<Value> result) {
  return CalendarYear(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarYear ( calendar, dateLike )
 */
bool js::temporal::CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                                Handle<PlainDateTimeObject*> dateLike,
                                MutableHandle<Value> result) {
  return CalendarYear(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarYear ( calendar, dateLike )
 */
bool js::temporal::CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                                Handle<PlainYearMonthObject*> dateLike,
                                MutableHandle<Value> result) {
  return CalendarYear(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarYear ( calendar, dateLike )
 */
bool js::temporal::CalendarYear(JSContext* cx, Handle<CalendarValue> calendar,
                                const PlainDateTime& dateTime,
                                MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.month ( temporalDateLike )
 */
static bool BuiltinCalendarMonth(const PlainDate& date,
                                 MutableHandle<Value> result) {
  // Steps 1-5. (Not applicable.)

  // Steps 6-7.
  result.setInt32(date.month);
  return true;
}

static bool Calendar_month(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarMonth ( calendar, dateLike )
 */
static bool CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                          Handle<JSObject*> dateLike, const PlainDate& date,
                          MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarMonth(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().month, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_month)) {
    return BuiltinCalendarMonth(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "month", result);
}

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                 Handle<PlainDateObject*> dateLike,
                                 MutableHandle<Value> result) {
  return CalendarMonth(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                 Handle<PlainDateTimeObject*> dateLike,
                                 MutableHandle<Value> result) {
  return CalendarMonth(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                 Handle<PlainYearMonthObject*> dateLike,
                                 MutableHandle<Value> result) {
  return CalendarMonth(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                 const PlainDateTime& dateTime,
                                 MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarMonth(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.monthCode ( temporalDateLike )
 */
static bool BuiltinCalendarMonthCode(JSContext* cx, const PlainDate& date,
                                     MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  JSString* str = ISOMonthCode(cx, date.month);
  if (!str) {
    return false;
  }

  result.setString(str);
  return true;
}

static bool Calendar_monthCode(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
static bool CalendarMonthCode(JSContext* cx, Handle<CalendarValue> calendar,
                              Handle<JSObject*> dateLike, const PlainDate& date,
                              MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarMonthCode(cx, date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().monthCode, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_monthCode)) {
    return BuiltinCalendarMonthCode(cx, date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Step 3.
  if (!result.isString()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_UNEXPECTED_TYPE, "monthCode",
                              "not a string");
    return false;
  }

  // Step 4.
  return true;
}

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarMonthCode(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateTimeObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarMonthCode(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainMonthDayObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarMonthCode(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainYearMonthObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarMonthCode(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarMonthCode ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthCode(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDateTime& dateTime,
                                     MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarMonthCode(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.day ( temporalDateLike )
 */
static bool BuiltinCalendarDay(const PlainDate& date,
                               MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  result.setInt32(date.day);
  return true;
}

static bool Calendar_day(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDay ( calendar, dateLike )
 */
static bool CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                        Handle<JSObject*> dateLike, const PlainDate& date,
                        MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDay(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().day, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() && IsNativeFunction(fn, Calendar_day)) {
    return BuiltinCalendarDay(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "day", result);
}

/**
 * CalendarDay ( calendar, dateLike )
 */
bool js::temporal::CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<PlainDateObject*> dateLike,
                               MutableHandle<Value> result) {
  return CalendarDay(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarDay ( calendar, dateLike )
 */
bool js::temporal::CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<PlainDateTimeObject*> dateLike,
                               MutableHandle<Value> result) {
  return CalendarDay(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarDay ( calendar, dateLike )
 */
bool js::temporal::CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<PlainMonthDayObject*> dateLike,
                               MutableHandle<Value> result) {
  return CalendarDay(cx, calendar, dateLike, ToPlainDate(dateLike), result);
}

/**
 * CalendarDay ( calendar, dateLike )
 */
bool js::temporal::CalendarDayWrapped(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> dateLike, MutableHandle<Value> result) {
  auto* unwrapped = dateLike.unwrap(cx);
  if (!unwrapped) {
    return false;
  }
  return CalendarDay(cx, calendar, dateLike, ToPlainDate(unwrapped), result);
}

/**
 * CalendarDay ( calendar, dateLike )
 */
bool js::temporal::CalendarDay(JSContext* cx, Handle<CalendarValue> calendar,
                               const PlainDateTime& dateTime,
                               MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDay(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.dayOfWeek ( temporalDateLike )
 */
static bool BuiltinCalendarDayOfWeek(const PlainDate& date,
                                     MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-9.
  result.setInt32(ToISODayOfWeek(date));
  return true;
}

static bool Calendar_dayOfWeek(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
static bool CalendarDayOfWeek(JSContext* cx, Handle<CalendarValue> calendar,
                              Handle<JSObject*> dateLike, const PlainDate& date,
                              MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDayOfWeek(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dayOfWeek, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_dayOfWeek)) {
    return BuiltinCalendarDayOfWeek(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "dayOfWeek", result);
}

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfWeek(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarDayOfWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfWeek(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateTimeObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarDayOfWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarDayOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfWeek(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDateTime& dateTime,
                                     MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDayOfWeek(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.dayOfYear ( temporalDateLike )
 */
static bool BuiltinCalendarDayOfYear(const PlainDate& date,
                                     MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-7.
  result.setInt32(ToISODayOfYear(date));
  return true;
}

static bool Calendar_dayOfYear(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
static bool CalendarDayOfYear(JSContext* cx, Handle<CalendarValue> calendar,
                              Handle<JSObject*> dateLike, const PlainDate& date,
                              MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDayOfYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dayOfYear, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_dayOfYear)) {
    return BuiltinCalendarDayOfYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "dayOfYear", result);
}

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfYear(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarDayOfYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfYear(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<PlainDateTimeObject*> dateLike,
                                     MutableHandle<Value> result) {
  return CalendarDayOfYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                           result);
}

/**
 * CalendarDayOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDayOfYear(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     const PlainDateTime& dateTime,
                                     MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDayOfYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.weekOfYear ( temporalDateLike )
 */
static bool BuiltinCalendarWeekOfYear(const PlainDate& date,
                                      MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  result.setInt32(ToISOWeekOfYear(date).week);
  return true;
}

static bool Calendar_weekOfYear(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
static bool CalendarWeekOfYear(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<JSObject*> dateLike,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarWeekOfYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().weekOfYear, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_weekOfYear)) {
    return BuiltinCalendarWeekOfYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "weekOfYear", result);
}

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarWeekOfYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarWeekOfYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarWeekOfYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateTimeObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarWeekOfYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarWeekOfYear ( calendar, dateLike )
 */
bool js::temporal::CalendarWeekOfYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDateTime& dateTime,
                                      MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarWeekOfYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.yearOfWeek ( temporalDateLike )
 */
static bool BuiltinCalendarYearOfWeek(const PlainDate& date,
                                      MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  result.setInt32(ToISOWeekOfYear(date).year);
  return true;
}

static bool Calendar_yearOfWeek(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
static bool CalendarYearOfWeek(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<JSObject*> dateLike,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarYearOfWeek(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().yearOfWeek, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_yearOfWeek)) {
    return BuiltinCalendarYearOfWeek(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-5.
  return ToIntegerIfIntegralNumber(cx, result, "yearOfWeek", result);
}

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarYearOfWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarYearOfWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarYearOfWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateTimeObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarYearOfWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarYearOfWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarYearOfWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDateTime& dateTime,
                                      MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarYearOfWeek(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.daysInWeek ( temporalDateLike )
 */
static bool BuiltinCalendarDaysInWeek(const PlainDate& date,
                                      MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Step 5.
  result.setInt32(7);
  return true;
}

static bool Calendar_daysInWeek(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
static bool CalendarDaysInWeek(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<JSObject*> dateLike,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDaysInWeek(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().daysInWeek, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_daysInWeek)) {
    return BuiltinCalendarDaysInWeek(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "daysInWeek", result);
}

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarDaysInWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateTimeObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarDaysInWeek(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarDaysInWeek ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInWeek(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDateTime& dateTime,
                                      MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDaysInWeek(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.daysInMonth ( temporalDateLike )
 */
static bool BuiltinCalendarDaysInMonth(const PlainDate& date,
                                       MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Step 5.
  result.setInt32(::ISODaysInMonth(date.year, date.month));
  return true;
}

static bool Calendar_daysInMonth(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
static bool CalendarDaysInMonth(JSContext* cx, Handle<CalendarValue> calendar,
                                Handle<JSObject*> dateLike,
                                const PlainDate& date,
                                MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDaysInMonth(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().daysInMonth, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_daysInMonth)) {
    return BuiltinCalendarDaysInMonth(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "daysInMonth", result);
}

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInMonth(JSContext* cx,
                                       Handle<CalendarValue> calendar,
                                       Handle<PlainDateObject*> dateLike,
                                       MutableHandle<Value> result) {
  return CalendarDaysInMonth(cx, calendar, dateLike, ToPlainDate(dateLike),
                             result);
}

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInMonth(JSContext* cx,
                                       Handle<CalendarValue> calendar,
                                       Handle<PlainDateTimeObject*> dateLike,
                                       MutableHandle<Value> result) {
  return CalendarDaysInMonth(cx, calendar, dateLike, ToPlainDate(dateLike),
                             result);
}

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInMonth(JSContext* cx,
                                       Handle<CalendarValue> calendar,
                                       Handle<PlainYearMonthObject*> dateLike,
                                       MutableHandle<Value> result) {
  return CalendarDaysInMonth(cx, calendar, dateLike, ToPlainDate(dateLike),
                             result);
}

/**
 * CalendarDaysInMonth ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInMonth(JSContext* cx,
                                       Handle<CalendarValue> calendar,
                                       const PlainDateTime& dateTime,
                                       MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDaysInMonth(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.daysInYear ( temporalDateLike )
 */
static bool BuiltinCalendarDaysInYear(const PlainDate& date,
                                      MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Step 5.
  result.setInt32(ISODaysInYear(date.year));
  return true;
}

static bool Calendar_daysInYear(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
static bool CalendarDaysInYear(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<JSObject*> dateLike,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDaysInYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().daysInYear, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_daysInYear)) {
    return BuiltinCalendarDaysInYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "daysInYear", result);
}

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarDaysInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateTimeObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarDaysInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainYearMonthObject*> dateLike,
                                      MutableHandle<Value> result) {
  return CalendarDaysInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                            result);
}

/**
 * CalendarDaysInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarDaysInYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDateTime& dateTime,
                                      MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarDaysInYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.monthsInYear ( temporalDateLike )
 */
static bool BuiltinCalendarMonthsInYear(const PlainDate& date,
                                        MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Step 5.
  result.setInt32(12);
  return true;
}

static bool Calendar_monthsInYear(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
static bool CalendarMonthsInYear(JSContext* cx, Handle<CalendarValue> calendar,
                                 Handle<JSObject*> dateLike,
                                 const PlainDate& date,
                                 MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarMonthsInYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().monthsInYear, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_monthsInYear)) {
    return BuiltinCalendarMonthsInYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Steps 3-6.
  return ToPositiveIntegerIfIntegralNumber(cx, result, "monthsInYear", result);
}

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthsInYear(JSContext* cx,
                                        Handle<CalendarValue> calendar,
                                        Handle<PlainDateObject*> dateLike,
                                        MutableHandle<Value> result) {
  return ::CalendarMonthsInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                                result);
}

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthsInYear(JSContext* cx,
                                        Handle<CalendarValue> calendar,
                                        Handle<PlainDateTimeObject*> dateLike,
                                        MutableHandle<Value> result) {
  return ::CalendarMonthsInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                                result);
}

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthsInYear(JSContext* cx,
                                        Handle<CalendarValue> calendar,
                                        Handle<PlainYearMonthObject*> dateLike,
                                        MutableHandle<Value> result) {
  return ::CalendarMonthsInYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                                result);
}

/**
 * CalendarMonthsInYear ( calendar, dateLike )
 */
bool js::temporal::CalendarMonthsInYear(JSContext* cx,
                                        Handle<CalendarValue> calendar,
                                        const PlainDateTime& dateTime,
                                        MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarMonthsInYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * Temporal.Calendar.prototype.inLeapYear ( temporalDateLike )
 */
static bool BuiltinCalendarInLeapYear(const PlainDate& date,
                                      MutableHandle<Value> result) {
  // Steps 1-4. (Not applicable.)

  // Steps 5-6.
  result.setBoolean(IsISOLeapYear(date.year));
  return true;
}

static bool Calendar_inLeapYear(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
static bool CalendarInLeapYear(JSContext* cx, Handle<CalendarValue> calendar,
                               Handle<JSObject*> dateLike,
                               const PlainDate& date,
                               MutableHandle<Value> result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarInLeapYear(date, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> fn(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().inLeapYear, &fn)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(fn, Calendar_inLeapYear)) {
    return BuiltinCalendarInLeapYear(date, result);
  }

  Rooted<JS::Value> dateLikeValue(cx, ObjectValue(*dateLike));
  if (!Call(cx, fn, calendarObj, dateLikeValue, result)) {
    return false;
  }

  // Step 3.
  if (!result.isBoolean()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_UNEXPECTED_TYPE, "inLeapYear",
                              "not a boolean");
    return false;
  }

  // Step 4.
  return true;
}

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool js::temporal::CalendarInLeapYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateObject*> dateLike,
                                      MutableHandle<Value> result) {
  return ::CalendarInLeapYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                              result);
}

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool js::temporal::CalendarInLeapYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainDateTimeObject*> dateLike,
                                      MutableHandle<Value> result) {
  return ::CalendarInLeapYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                              result);
}

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool js::temporal::CalendarInLeapYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      Handle<PlainYearMonthObject*> dateLike,
                                      MutableHandle<Value> result) {
  return ::CalendarInLeapYear(cx, calendar, dateLike, ToPlainDate(dateLike),
                              result);
}

/**
 * CalendarInLeapYear ( calendar, dateLike )
 */
bool js::temporal::CalendarInLeapYear(JSContext* cx,
                                      Handle<CalendarValue> calendar,
                                      const PlainDateTime& dateTime,
                                      MutableHandle<Value> result) {
  Rooted<PlainDateTimeObject*> dateLike(
      cx, CreateTemporalDateTime(cx, dateTime, calendar));
  if (!dateLike) {
    return false;
  }

  return ::CalendarInLeapYear(cx, calendar, dateLike, dateTime.date, result);
}

/**
 * ISOResolveMonth ( fields )
 */
static bool ISOResolveMonth(JSContext* cx,
                            MutableHandle<TemporalFields> fields) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  double month = fields.month();

  // Step 3.
  MOZ_ASSERT((IsInteger(month) && month > 0) || std::isnan(month));

  // Step 4.
  Handle<JSString*> monthCode = fields.monthCode();

  // Step 5.
  if (!monthCode) {
    // Step 5.a.
    if (std::isnan(month)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_CALENDAR_MISSING_FIELD,
                                "monthCode");
      return false;
    }

    // Step 5.b.
    return true;
  }

  // Steps 6-7. (Not applicable in our implementation.)

  // Step 8.
  if (monthCode->length() != 3) {
    if (auto code = QuoteString(cx, monthCode)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               code.get());
    }
    return false;
  }

  JSLinearString* linear = monthCode->ensureLinear(cx);
  if (!linear) {
    return false;
  }

  char16_t chars[3] = {
      linear->latin1OrTwoByteChar(0),
      linear->latin1OrTwoByteChar(1),
      linear->latin1OrTwoByteChar(2),
  };

  // Steps 9-11. (Partial)
  if (chars[0] != 'M' || !mozilla::IsAsciiDigit(chars[1]) ||
      !mozilla::IsAsciiDigit(chars[2])) {
    if (auto code = QuoteString(cx, linear)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               code.get());
    }
    return false;
  }

  // Step 12.
  int32_t monthCodeInteger =
      AsciiDigitToNumber(chars[1]) * 10 + AsciiDigitToNumber(chars[2]);

  // Step 11. (Partial)
  if (monthCodeInteger < 1 || monthCodeInteger > 12) {
    if (auto code = QuoteString(cx, linear)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               code.get());
    }
    return false;
  }

  // Step 13. (Not applicable in our implementation.)

  // Step 14.
  if (!std::isnan(month) && month != monthCodeInteger) {
    if (auto code = QuoteString(cx, linear)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INVALID_MONTHCODE,
                               code.get());
    }
    return false;
  }

  // Step 15.
  fields.month() = monthCodeInteger;

  // Step 16.
  return true;
}

/**
 * ISODateFromFields ( fields, overflow )
 */
static bool ISODateFromFields(JSContext* cx, Handle<TemporalFields> fields,
                              TemporalOverflow overflow, PlainDate* result) {
  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  double year = fields.year();

  // Step 4.
  double month = fields.month();

  // Step 5.
  double day = fields.day();

  // Step 6.
  MOZ_ASSERT(!std::isnan(year) && !std::isnan(month) && !std::isnan(day));

  // Step 7.
  RegulatedISODate regulated;
  if (!RegulateISODate(cx, year, month, day, overflow, &regulated)) {
    return false;
  }

  // The result is used to create a new PlainDateObject, so it's okay to
  // directly throw an error for invalid years. That way we don't have to worry
  // about representing doubles in PlainDate structs.
  int32_t intYear;
  if (!mozilla::NumberEqualsInt32(regulated.year, &intYear)) {
    // CreateTemporalDate, steps 1-2.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_INVALID);
    return false;
  }

  *result = {intYear, regulated.month, regulated.day};
  return true;
}

/**
 * Temporal.Calendar.prototype.dateFromFields ( fields [ , options ] )
 */
static PlainDateObject* BuiltinCalendarDateFromFields(
    JSContext* cx, Handle<JSObject*> fields, Handle<JSObject*> maybeOptions) {
  // Steps 1-5. (Not applicable)

  // Step 6.
  Rooted<TemporalFields> dateFields(cx);
  if (!PrepareTemporalFields(cx, fields,
                             {TemporalField::Day, TemporalField::Month,
                              TemporalField::MonthCode, TemporalField::Year},
                             {TemporalField::Day, TemporalField::Year},
                             &dateFields)) {
    return nullptr;
  }

  // Step 7.
  auto overflow = TemporalOverflow::Constrain;
  if (maybeOptions) {
    if (!ToTemporalOverflow(cx, maybeOptions, &overflow)) {
      return nullptr;
    }
  }

  // Step 8.
  if (!ISOResolveMonth(cx, &dateFields)) {
    return nullptr;
  }

  // Step 9.
  PlainDate result;
  if (!ISODateFromFields(cx, dateFields, overflow, &result)) {
    return nullptr;
  }

  // Step 10.
  Rooted<CalendarValue> calendar(cx, CalendarValue(cx->names().iso8601));
  return CreateTemporalDate(cx, result, calendar);
}

static bool Calendar_dateFromFields(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDateFromFields ( calendar, fields [, options [, dateFromFields ] ] )
 */
static Wrapped<PlainDateObject*> CalendarDateFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> maybeOptions) {
  // Step 1. (Not applicable in our implemetation.)

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarDateFromFields(cx, fields, maybeOptions);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateFromFields(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateFromFields,
                        &dateFromFields)) {
    return nullptr;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(dateFromFields, Calendar_dateFromFields)) {
    return BuiltinCalendarDateFromFields(cx, fields, maybeOptions);
  }

  // Step 4.
  Rooted<Value> thisv(cx, ObjectValue(*calendarObj));
  Rooted<Value> rval(cx);

  FixedInvokeArgs<2> args(cx);
  args[0].setObject(*fields);
  if (maybeOptions) {
    args[1].setObject(*maybeOptions);
  } else {
    args[1].setUndefined();
  }

  if (!Call(cx, dateFromFields, thisv, args, &rval)) {
    return nullptr;
  }

  // Step 5.
  if (!rval.isObject() || !rval.toObject().canUnwrapAs<PlainDateObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a PlainDate object");
    return nullptr;
  }

  // Step 6.
  return &rval.toObject();
}

/**
 * CalendarDateFromFields ( calendar, fields [, options [, dateFromFields ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields) {
  // Steps 1-6.
  return ::CalendarDateFromFields(cx, calendar, fields, nullptr);
}

/**
 * CalendarDateFromFields ( calendar, fields [, options [, dateFromFields ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> options) {
  // Steps 1-6.
  return ::CalendarDateFromFields(cx, calendar, fields, options);
}

struct RegulatedISOYearMonth final {
  double year;
  int32_t month;
};

/**
 * RegulateISOYearMonth ( year, month, overflow )
 */
static bool RegulateISOYearMonth(JSContext* cx, double year, double month,
                                 TemporalOverflow overflow,
                                 RegulatedISOYearMonth* result) {
  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));

  // Step 2. (Not applicable in our implementation.)

  // Step 3.
  if (overflow == TemporalOverflow::Constrain) {
    // Step 3.a.
    month = std::clamp(month, 1.0, 12.0);

    // Step 3.b.
    *result = {year, int32_t(month)};
    return true;
  }

  // Step 4.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 4.b.
  if (month < 1 || month > 12) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_YEAR_MONTH_INVALID);
    return false;
  }

  // Step 4.c.
  *result = {year, int32_t(month)};
  return true;
}

/**
 * ISOYearMonthFromFields ( fields, overflow )
 */
static bool ISOYearMonthFromFields(JSContext* cx, Handle<TemporalFields> fields,
                                   TemporalOverflow overflow,
                                   PlainDate* result) {
  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  double year = fields.year();

  // Step 4.
  double month = fields.month();

  // Step 5.
  MOZ_ASSERT(!std::isnan(year) && !std::isnan(month));

  // Step 6.
  RegulatedISOYearMonth regulated;
  if (!RegulateISOYearMonth(cx, year, month, overflow, &regulated)) {
    return false;
  }

  // Step 7.

  // The result is used to create a new PlainYearMonthObject, so it's okay to
  // directly throw an error for invalid years. That way we don't have to worry
  // about representing doubles in PlainDate structs.
  int32_t intYear;
  if (!mozilla::NumberEqualsInt32(regulated.year, &intYear)) {
    // CreateTemporalYearMonth, steps 1-2.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_YEAR_MONTH_INVALID);
    return false;
  }

  *result = {intYear, regulated.month, 1};
  return true;
}

/**
 * Temporal.Calendar.prototype.yearMonthFromFields ( fields [ , options ] )
 */
static PlainYearMonthObject* BuiltinCalendarYearMonthFromFields(
    JSContext* cx, Handle<JSObject*> fields, Handle<JSObject*> maybeOptions) {
  // Steps 1-5. (Not applicable)

  // Step 6.
  Rooted<TemporalFields> dateFields(cx);
  if (!PrepareTemporalFields(
          cx, fields,
          {TemporalField::Month, TemporalField::MonthCode, TemporalField::Year},
          {TemporalField::Year}, &dateFields)) {
    return nullptr;
  }

  // Step 7.
  auto overflow = TemporalOverflow::Constrain;
  if (maybeOptions) {
    if (!ToTemporalOverflow(cx, maybeOptions, &overflow)) {
      return nullptr;
    }
  }

  // Step 8.
  if (!ISOResolveMonth(cx, &dateFields)) {
    return nullptr;
  }

  // Step 9.
  PlainDate result;
  if (!ISOYearMonthFromFields(cx, dateFields, overflow, &result)) {
    return nullptr;
  }

  // Step 10.
  Rooted<CalendarValue> calendar(cx, CalendarValue(cx->names().iso8601));
  return CreateTemporalYearMonth(cx, result, calendar);
}

static bool Calendar_yearMonthFromFields(JSContext* cx, unsigned argc,
                                         Value* vp);

/**
 * CalendarYearMonthFromFields ( calendar, fields [ , options ] )
 */
static Wrapped<PlainYearMonthObject*> CalendarYearMonthFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> maybeOptions) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarYearMonthFromFields(cx, fields, maybeOptions);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> yearMonthFromFields(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().yearMonthFromFields,
                        &yearMonthFromFields)) {
    return nullptr;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(yearMonthFromFields, Calendar_yearMonthFromFields)) {
    return BuiltinCalendarYearMonthFromFields(cx, fields, maybeOptions);
  }

  Rooted<Value> thisv(cx, ObjectValue(*calendarObj));
  Rooted<Value> rval(cx);

  FixedInvokeArgs<2> args(cx);
  args[0].setObject(*fields);
  if (maybeOptions) {
    args[1].setObject(*maybeOptions);
  } else {
    args[1].setUndefined();
  }

  if (!Call(cx, yearMonthFromFields, thisv, args, &rval)) {
    return nullptr;
  }

  // Step 4.
  if (!rval.isObject() ||
      !rval.toObject().canUnwrapAs<PlainYearMonthObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a PlainYearMonth object");
    return nullptr;
  }

  // Step 5.
  return &rval.toObject();
}

/**
 * CalendarYearMonthFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainYearMonthObject*> js::temporal::CalendarYearMonthFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields) {
  // Steps 1-4.
  return ::CalendarYearMonthFromFields(cx, calendar, fields, nullptr);
}

/**
 * CalendarYearMonthFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainYearMonthObject*> js::temporal::CalendarYearMonthFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> options) {
  // Steps 1-4.
  return ::CalendarYearMonthFromFields(cx, calendar, fields, options);
}

/**
 * ISOMonthDayFromFields ( fields, overflow )
 */
static bool ISOMonthDayFromFields(JSContext* cx, Handle<TemporalFields> fields,
                                  TemporalOverflow overflow,
                                  PlainDate* result) {
  // Steps 1-2. (Not applicable in our implementation.)

  // Step 3.
  double month = fields.month();

  // Step 4.
  double day = fields.day();

  // Step 5.
  MOZ_ASSERT(!std::isnan(month));
  MOZ_ASSERT(!std::isnan(day));

  // Step 6.
  double year = fields.year();

  // Step 7.
  int32_t referenceISOYear = 1972;

  // Steps 8-9.
  double y = std::isnan(year) ? referenceISOYear : year;
  RegulatedISODate regulated;
  if (!RegulateISODate(cx, y, month, day, overflow, &regulated)) {
    return false;
  }

  // Step 10.
  *result = {referenceISOYear, regulated.month, regulated.day};
  return true;
}

/**
 * Temporal.Calendar.prototype.monthDayFromFields ( fields [ , options ] )
 */
static PlainMonthDayObject* BuiltinCalendarMonthDayFromFields(
    JSContext* cx, Handle<JSObject*> fields, Handle<JSObject*> maybeOptions) {
  // Steps 1-5. (Not applicable)

  // Step 6.
  Rooted<TemporalFields> dateFields(cx);
  if (!PrepareTemporalFields(cx, fields,
                             {TemporalField::Day, TemporalField::Month,
                              TemporalField::MonthCode, TemporalField::Year},
                             {TemporalField::Day}, &dateFields)) {
    return nullptr;
  }

  // Step 7.
  auto overflow = TemporalOverflow::Constrain;
  if (maybeOptions) {
    if (!ToTemporalOverflow(cx, maybeOptions, &overflow)) {
      return nullptr;
    }
  }

  // Step 8.
  if (!ISOResolveMonth(cx, &dateFields)) {
    return nullptr;
  }

  // Step 9.
  PlainDate result;
  if (!ISOMonthDayFromFields(cx, dateFields, overflow, &result)) {
    return nullptr;
  }

  // Step 10.
  Rooted<CalendarValue> calendar(cx, CalendarValue(cx->names().iso8601));
  return CreateTemporalMonthDay(cx, result, calendar);
}

static bool Calendar_monthDayFromFields(JSContext* cx, unsigned argc,
                                        Value* vp);

/**
 * CalendarMonthDayFromFields ( calendar, fields [ , options ] )
 */
static Wrapped<PlainMonthDayObject*> CalendarMonthDayFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> maybeOptions) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarMonthDayFromFields(cx, fields, maybeOptions);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> monthDayFromFields(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().monthDayFromFields,
                        &monthDayFromFields)) {
    return nullptr;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(monthDayFromFields, Calendar_monthDayFromFields)) {
    return BuiltinCalendarMonthDayFromFields(cx, fields, maybeOptions);
  }

  Rooted<Value> thisv(cx, ObjectValue(*calendarObj));
  Rooted<Value> rval(cx);

  FixedInvokeArgs<2> args(cx);
  args[0].setObject(*fields);
  if (maybeOptions) {
    args[1].setObject(*maybeOptions);
  } else {
    args[1].setUndefined();
  }

  if (!Call(cx, monthDayFromFields, thisv, args, &rval)) {
    return nullptr;
  }

  // Step 4.
  if (!rval.isObject() || !rval.toObject().canUnwrapAs<PlainMonthDayObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a PlainMonthDay object");
    return nullptr;
  }

  // Step 5.
  return &rval.toObject();
}

/**
 * CalendarMonthDayFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainMonthDayObject*> js::temporal::CalendarMonthDayFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields) {
  // Steps 1-4.
  return ::CalendarMonthDayFromFields(cx, calendar, fields, nullptr);
}

/**
 * CalendarMonthDayFromFields ( calendar, fields [ , options ] )
 */
Wrapped<PlainMonthDayObject*> js::temporal::CalendarMonthDayFromFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<JSObject*> fields,
    Handle<JSObject*> options) {
  // Steps 1-4.
  return ::CalendarMonthDayFromFields(cx, calendar, fields, options);
}

using PropertyHashSet = JS::GCHashSet<JS::PropertyKey>;
using PropertyVector = JS::StackGCVector<JS::PropertyKey>;

/**
 * ISOFieldKeysToIgnore ( keys )
 */
static bool ISOFieldKeysToIgnore(JSContext* cx, const PropertyVector& keys,
                                 PropertyHashSet& ignoredKeys) {
  MOZ_ASSERT(ignoredKeys.empty(), "expected an empty output hashset");

  // Step 1. (Not applicable in our implementation.)

  if (!ignoredKeys.reserve(keys.length())) {
    return false;
  }

  // Step 2.
  bool seenMonthOrMonthCode = false;
  for (auto& key : keys) {
    // Reorder the substeps in order to use |putNew| instead of |put|, because
    // the former is slightly faster.

    // Steps 2.a-c.
    if (key.isAtom(cx->names().month) || key.isAtom(cx->names().monthCode)) {
      // Steps 2.b-c.
      if (!seenMonthOrMonthCode) {
        seenMonthOrMonthCode = true;

        // Add both keys at once.
        if (!ignoredKeys.putNew(NameToId(cx->names().month)) ||
            !ignoredKeys.putNew(NameToId(cx->names().monthCode))) {
          return false;
        }
      }
    } else {
      // Step 2.a.
      if (!ignoredKeys.putNew(key)) {
        return false;
      }
    }
  }

  // Step 3.
  return true;
}

/**
 * Temporal.Calendar.prototype.mergeFields ( fields, additionalFields )
 */
static PlainObject* BuiltinCalendarMergeFields(
    JSContext* cx, Handle<JSObject*> fields,
    Handle<JSObject*> additionalFields) {
  // NOTE: This function is only called from CalendarMergeFields and its
  // result is always passed to PrepareTemporalFields. PrepareTemporalFields
  // sorts the incoming property keys, so it doesn't matter in which order the
  // properties are created in this function. This allows to reorder the steps
  // to process |additionalFields| first.

  // TODO: Consider additionally passing the fieldNames from the succeeding
  // PrepareTemporalFields call, so we don't have to process unnecessary keys.

  // Steps 1-2. (Not applicable in our implementation.)

  // Step 13.
  Rooted<PlainObject*> merged(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!merged) {
    return nullptr;
  }

  // Contrary to the spec, add the properties from |additionalFields| first.

  // Step 6. (Not applicable in our implementation.)

  // Steps 7-8.
  //
  // PrepareTemporalFields ignores symbol property keys, so we don't need to
  // pass JSITER_SYMBOLS. |additionalFields| contains no non-enumerable
  // properties, therefore JSITER_HIDDEN isn't needed, too.
  JS::RootedVector<PropertyKey> keys(cx);
  if (!GetPropertyKeys(cx, additionalFields, JSITER_OWNONLY, &keys)) {
    return nullptr;
  }

  // Steps 9-11. (Not applicable in our implementation.)

  // Step 12.
  Rooted<PropertyHashSet> ignoredKeys(cx, PropertyHashSet(cx));
  if (!ignoredKeys.reserve(keys.length())) {
    return nullptr;
  }

  // Steps 7-8, 12, and 15.
  Rooted<mozilla::Maybe<PropertyDescriptor>> desc(cx);
  Rooted<Value> propValue(cx);
  bool seenMonthOrMonthCode = false;
  for (size_t i = 0; i < keys.length(); i++) {
    Handle<PropertyKey> key = keys[i];

    if (!GetOwnPropertyDescriptor(cx, additionalFields, key, &desc)) {
      return nullptr;
    }

    propValue.set(desc->value());

    // Skip |undefined| properties per step 8.
    if (propValue.isUndefined()) {
      continue;
    }

    // Step 15.
    if (!DefineDataProperty(cx, merged, key, propValue)) {
      return nullptr;
    }

    // Step 12. (Inlined ISOFieldKeysToIgnore)
    if (key.isAtom(cx->names().month) || key.isAtom(cx->names().monthCode)) {
      // ISOFieldKeysToIgnore, steps 2.b-c.
      if (!seenMonthOrMonthCode) {
        seenMonthOrMonthCode = true;

        if (!ignoredKeys.putNew(NameToId(cx->names().month)) ||
            !ignoredKeys.putNew(NameToId(cx->names().monthCode))) {
          return nullptr;
        }
      }
    } else {
      // ISOFieldKeysToIgnore, step 2.a.
      if (!ignoredKeys.putNew(key)) {
        return nullptr;
      }
    }
  }

  // Now add the properties from |field|.

  // Step 3. (Not applicable in our implementation.)

  // Reuse |keys| to avoid extra allocations.
  keys.clear();

  // Steps 4-5.
  //
  // See above why neither JSITER_SYMBOLS nor JSITER_HIDDEN is needed.
  if (!GetPropertyKeys(cx, fields, JSITER_OWNONLY, &keys)) {
    return nullptr;
  }

  // Steps 4-5 and 14.
  for (size_t i = 0; i < keys.length(); i++) {
    Handle<PropertyKey> key = keys[i];

    // Skip ignored keys per step 14.
    if (ignoredKeys.has(key)) {
      continue;
    }

    if (!GetOwnPropertyDescriptor(cx, fields, key, &desc)) {
      return nullptr;
    }

    propValue.set(desc->value());

    // Skip |undefined| properties per step 5.
    if (propValue.isUndefined()) {
      continue;
    }

    // Step 14.
    if (!DefineDataProperty(cx, merged, key, propValue)) {
      return nullptr;
    }
  }

  // Step 16.
  return merged;
}

#ifdef DEBUG
static bool IsPlainDataObject(PlainObject* obj) {
  for (ShapePropertyIter<NoGC> iter(obj->shape()); !iter.done(); iter++) {
    if (iter->flags() != PropertyFlags::defaultDataPropFlags) {
      return false;
    }
  }
  return true;
}
#endif

static bool Calendar_mergeFields(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarMergeFields ( calendar, fields, additionalFields )
 */
JSObject* js::temporal::CalendarMergeFields(
    JSContext* cx, Handle<CalendarValue> calendar, Handle<PlainObject*> fields,
    Handle<PlainObject*> additionalFields) {
  MOZ_ASSERT(IsPlainDataObject(fields));
  MOZ_ASSERT(IsPlainDataObject(additionalFields));

  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarMergeFields(cx, fields, additionalFields);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> mergeFields(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().mergeFields,
                        &mergeFields)) {
    return nullptr;
  }

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(mergeFields, Calendar_mergeFields)) {
    return BuiltinCalendarMergeFields(cx, fields, additionalFields);
  }

  Rooted<Value> thisv(cx, ObjectValue(*calendarObj));
  Rooted<Value> result(cx);

  FixedInvokeArgs<2> args(cx);
  args[0].setObject(*fields);
  args[1].setObject(*additionalFields);

  if (!Call(cx, mergeFields, thisv, args, &result)) {
    return nullptr;
  }

  // Steps 3-4.
  return RequireObject(cx, result);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static bool BuiltinCalendarAdd(JSContext* cx, const PlainDate& date,
                               const Duration& duration,
                               Handle<JSObject*> options, PlainDate* result) {
  MOZ_ASSERT(IsValidISODate(date));
  MOZ_ASSERT(IsValidDuration(duration));

  // Steps 1-6. (Not applicable)

  // Step 7.
  auto overflow = TemporalOverflow::Constrain;
  if (options) {
    if (!ToTemporalOverflow(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 8.
  TimeDuration balanceResult;
  if (!BalanceTimeDuration(cx, duration, TemporalUnit::Day, &balanceResult)) {
    return false;
  }

  // Step 9.
  Duration addDuration = {duration.years, duration.months, duration.weeks,
                          balanceResult.days};
  return AddISODate(cx, date, addDuration, overflow, result);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static PlainDateObject* BuiltinCalendarAdd(JSContext* cx, const PlainDate& date,
                                           const Duration& duration,
                                           Handle<JSObject*> options) {
  // Steps 1-6. (Not applicable)

  // Steps 7-9.
  PlainDate result;
  if (!BuiltinCalendarAdd(cx, date, duration, options, &result)) {
    return nullptr;
  }

  // Step 10.
  Rooted<CalendarValue> calendar(cx, CalendarValue(cx->names().iso8601));
  return CreateTemporalDate(cx, result, calendar);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static PlainDateObject* BuiltinCalendarAdd(
    JSContext* cx, Handle<Wrapped<PlainDateObject*>> dateObj,
    const Duration& duration, Handle<JSObject*> options) {
  auto* unwrappedDate = dateObj.unwrap(cx);
  if (!unwrappedDate) {
    return nullptr;
  }
  auto date = ToPlainDate(unwrappedDate);

  return BuiltinCalendarAdd(cx, date, duration, options);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static PlainDateObject* BuiltinCalendarAdd(
    JSContext* cx, Handle<Wrapped<PlainDateObject*>> dateObj,
    Handle<Wrapped<DurationObject*>> durationObj, Handle<JSObject*> options) {
  auto* unwrappedDate = dateObj.unwrap(cx);
  if (!unwrappedDate) {
    return nullptr;
  }
  auto date = ToPlainDate(unwrappedDate);

  auto* unwrappedDuration = durationObj.unwrap(cx);
  if (!unwrappedDuration) {
    return nullptr;
  }
  Duration duration = ToDuration(unwrappedDuration);

  return BuiltinCalendarAdd(cx, date, duration, options);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static bool BuiltinCalendarAdd(JSContext* cx,
                               Handle<Wrapped<PlainDateObject*>> dateObj,
                               const Duration& duration,
                               Handle<JSObject*> options, PlainDate* result) {
  auto* unwrappedDate = dateObj.unwrap(cx);
  if (!unwrappedDate) {
    return false;
  }
  auto date = ToPlainDate(unwrappedDate);

  return BuiltinCalendarAdd(cx, date, duration, options, result);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
static Wrapped<PlainDateObject*> CalendarDateAddSlow(
    JSContext* cx, Handle<JSObject*> calendar,
    Handle<Wrapped<PlainDateObject*>> date,
    Handle<Wrapped<DurationObject*>> duration, Handle<JSObject*> options,
    Handle<Value> dateAdd) {
  // FIXME: spec bug - structured header wrong, |dateAdd| can be undefined.
  // https://github.com/tc39/proposal-temporal/issues/2527

  if (!IsCallable(dateAdd)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_PROPERTY_NOT_CALLABLE, "dateAdd");
    return nullptr;
  }

  // Step 4.
  Rooted<Value> thisv(cx, ObjectValue(*calendar));
  Rooted<Value> rval(cx);

  FixedInvokeArgs<3> args(cx);
  args[0].setObject(*date);
  args[1].setObject(*duration);
  if (options) {
    args[2].setObject(*options);
  } else {
    args[2].setUndefined();
  }

  if (!Call(cx, dateAdd, thisv, args, &rval)) {
    return nullptr;
  }

  // Step 5.
  if (!rval.isObject() || !rval.toObject().canUnwrapAs<PlainDateObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a PlainDate object");
    return nullptr;
  }

  // Step 6.
  return &rval.toObject();
}

static bool Calendar_dateAdd(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
static Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, Handle<JSObject*> calendar,
    Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    Handle<JSObject*> options, Handle<Value> dateAdd) {
  // Fast-path for the default implementation.
  if (calendar->is<CalendarObject>() &&
      IsNativeFunction(dateAdd, Calendar_dateAdd)) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  Rooted<DurationObject*> durationObj(cx, CreateTemporalDuration(cx, duration));
  if (!durationObj) {
    return nullptr;
  }
  return CalendarDateAddSlow(cx, calendar, date, durationObj, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
static Wrapped<PlainDateObject*> CalendarDateAdd(
    JSContext* cx, Handle<JSObject*> calendar,
    Handle<Wrapped<PlainDateObject*>> date,
    Handle<Wrapped<DurationObject*>> duration, Handle<JSObject*> options,
    Handle<Value> dateAdd) {
  // Fast-path for the default implementation.
  if (calendar->is<CalendarObject>() &&
      IsNativeFunction(dateAdd, Calendar_dateAdd)) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  return CalendarDateAddSlow(cx, calendar, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
static bool CalendarDateAdd(JSContext* cx, Handle<JSObject*> calendar,
                            Handle<Wrapped<PlainDateObject*>> date,
                            const Duration& duration, Handle<JSObject*> options,
                            Handle<Value> dateAdd, PlainDate* result) {
  // Fast-path for the default implementation.
  if (calendar->is<CalendarObject>() &&
      IsNativeFunction(dateAdd, Calendar_dateAdd)) {
    return BuiltinCalendarAdd(cx, date, duration, options, result);
  }

  Rooted<DurationObject*> durationObj(cx, CreateTemporalDuration(cx, duration));
  if (!durationObj) {
    return false;
  }

  auto obj =
      CalendarDateAddSlow(cx, calendar, date, durationObj, options, dateAdd);
  if (!obj) {
    return false;
  }

  *result = ToPlainDate(&obj.unwrap());
  return true;
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
static bool CalendarDateAdd(JSContext* cx, Handle<JSObject*> calendar,
                            const PlainDate& date, const Duration& duration,
                            Handle<JSObject*> options, Handle<Value> dateAdd,
                            PlainDate* result) {
  // Fast-path for the default implementation.
  if (calendar->is<CalendarObject>() &&
      IsNativeFunction(dateAdd, Calendar_dateAdd)) {
    return BuiltinCalendarAdd(cx, date, duration, options, result);
  }

  Rooted<CalendarValue> calendarValue(cx, CalendarValue(calendar));
  Rooted<PlainDateObject*> dateObj(cx,
                                   CreateTemporalDate(cx, date, calendarValue));
  if (!dateObj) {
    return false;
  }

  Rooted<DurationObject*> durationObj(cx, CreateTemporalDuration(cx, duration));
  if (!durationObj) {
    return false;
  }

  auto obj =
      CalendarDateAddSlow(cx, calendar, dateObj, durationObj, options, dateAdd);
  if (!obj) {
    return false;
  }

  *result = ToPlainDate(&obj.unwrap());
  return true;
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    Handle<JSObject*> options) {
  // Step 1. (Not applicable).

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateAdd(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
    return nullptr;
  }

  // Steps 4-6.
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    Handle<Value> dateAdd) {
  // Step 1.
  Handle<JSObject*> options = nullptr;

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  // Steps 3-6.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> date, const Duration& duration,
    Handle<JSObject*> options, Handle<Value> dateAdd) {
  // Step 1. (Not applicable).

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  // Steps 3-6.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> date,
    Handle<Wrapped<DurationObject*>> duration, Handle<JSObject*> options) {
  // Step 1. (Not applicable).

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateAdd(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
    return nullptr;
  }

  // Steps 4-6.
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
Wrapped<PlainDateObject*> js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar,
    Handle<Wrapped<PlainDateObject*>> date,
    Handle<Wrapped<DurationObject*>> duration, Handle<Value> dateAdd) {
  // Step 1.
  Handle<JSObject*> options = nullptr;

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options);
  }

  // Steps 3-6.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool js::temporal::CalendarDateAdd(JSContext* cx,
                                   Handle<CalendarValue> calendar,
                                   const PlainDate& date,
                                   const Duration& duration,
                                   PlainDate* result) {
  // Step 1.
  Handle<JSObject*> options = nullptr;

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options, result);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateAdd(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
    return false;
  }

  // Steps 4-6.
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd,
                           result);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool js::temporal::CalendarDateAdd(
    JSContext* cx, Handle<CalendarValue> calendar, const PlainDate& date,
    const Duration& duration, Handle<JSObject*> options, PlainDate* result) {
  // Step 1. (Not applicable)

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options, result);
  }

  // Step 3.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateAdd(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateAdd, &dateAdd)) {
    return false;
  }

  // Steps 4-6.
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd,
                           result);
}

/**
 * CalendarDateAdd ( calendar, date, duration [ , options [ , dateAdd ] ] )
 */
bool js::temporal::CalendarDateAdd(JSContext* cx,
                                   Handle<CalendarValue> calendar,
                                   Handle<Wrapped<PlainDateObject*>> date,
                                   const Duration& duration,
                                   Handle<Value> dateAdd, PlainDate* result) {
  // Step 1.
  Handle<JSObject*> options = nullptr;

  // Step 2.
  if (calendar.isString()) {
    return BuiltinCalendarAdd(cx, date, duration, options, result);
  }

  // Steps 3-6.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  return ::CalendarDateAdd(cx, calendarObj, date, duration, options, dateAdd,
                           result);
}

/**
 * Temporal.Calendar.prototype.dateUntil ( one, two [ , options ] )
 */
static Duration BuiltinCalendarDateUntil(const PlainDate& one,
                                         const PlainDate& two,
                                         TemporalUnit largestUnit) {
  // Steps 1-3. (Not applicable)

  // Steps 4-8. (Not applicable)

  // Step 9.
  auto difference = DifferenceISODate(one, two, largestUnit);

  // Step 10.
  return difference.toDuration();
}

/**
 * Temporal.Calendar.prototype.dateUntil ( one, two [ , options ] )
 */
static bool BuiltinCalendarDateUntil(JSContext* cx,
                                     Handle<Wrapped<PlainDateObject*>> one,
                                     Handle<Wrapped<PlainDateObject*>> two,
                                     TemporalUnit largestUnit,
                                     Duration* result) {
  MOZ_ASSERT(largestUnit <= TemporalUnit::Day);

  auto* unwrappedOne = one.unwrap(cx);
  if (!unwrappedOne) {
    return false;
  }
  auto dateOne = ToPlainDate(unwrappedOne);

  auto* unwrappedTwo = two.unwrap(cx);
  if (!unwrappedTwo) {
    return false;
  }
  auto dateTwo = ToPlainDate(unwrappedTwo);

  // Steps 1-10.
  *result = BuiltinCalendarDateUntil(dateOne, dateTwo, largestUnit);
  return true;
}

/**
 * Temporal.Calendar.prototype.dateUntil ( one, two [ , options ] )
 */
static bool BuiltinCalendarDateUntil(JSContext* cx,
                                     Handle<Wrapped<PlainDateObject*>> one,
                                     Handle<Wrapped<PlainDateObject*>> two,
                                     Handle<JSObject*> options,
                                     Duration* result) {
  // Steps 1-6. (Not applicable)

  // Steps 7-8.
  auto largestUnit = TemporalUnit::Day;
  if (!GetTemporalUnit(cx, options, TemporalUnitKey::LargestUnit,
                       TemporalUnitGroup::Date, &largestUnit)) {
    return false;
  }

  // Steps 9-10.
  return BuiltinCalendarDateUntil(cx, one, two, largestUnit, result);
}

static bool CalendarDateUntilSlow(JSContext* cx, Handle<JSObject*> calendar,
                                  Handle<Wrapped<PlainDateObject*>> one,
                                  Handle<Wrapped<PlainDateObject*>> two,
                                  Handle<JSObject*> options,
                                  Handle<Value> dateUntil, Duration* result) {
  // FIXME: spec bug - structured header wrong, |dateUntil| can be undefined.
  // https://github.com/tc39/proposal-temporal/issues/2527

  // Step 1. (Not applicable)

  if (!IsCallable(dateUntil)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_PROPERTY_NOT_CALLABLE, "dateUntil");
    return false;
  }

  // Step 2.
  Rooted<Value> thisv(cx, ObjectValue(*calendar));
  Rooted<Value> rval(cx);

  FixedInvokeArgs<3> args(cx);
  args[0].setObject(*one);
  args[1].setObject(*two);
  args[2].setObject(*options);

  if (!Call(cx, dateUntil, thisv, args, &rval)) {
    return false;
  }

  // Step 3.
  if (!rval.isObject() || !rval.toObject().canUnwrapAs<DurationObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a Duration object");
    return false;
  }

  // Step 4.
  *result = ToDuration(&rval.toObject().unwrapAs<DurationObject>());
  return true;
}

static bool Calendar_dateUntil(JSContext* cx, unsigned argc, Value* vp);

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
static bool CalendarDateUntil(JSContext* cx, Handle<CalendarValue> calendar,
                              Handle<Wrapped<PlainDateObject*>> one,
                              Handle<Wrapped<PlainDateObject*>> two,
                              Handle<JSObject*> options,
                              Handle<Value> dateUntil, Duration* result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDateUntil(cx, one, two, options, result);
  }

  Rooted<JSObject*> calendarObj(cx, calendar.toObject());

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(dateUntil, Calendar_dateUntil)) {
    return BuiltinCalendarDateUntil(cx, one, two, options, result);
  }

  // Steps 2-5.
  return CalendarDateUntilSlow(cx, calendarObj, one, two, options, dateUntil,
                               result);
}

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool js::temporal::CalendarDateUntil(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<Wrapped<PlainDateObject*>> one,
                                     Handle<Wrapped<PlainDateObject*>> two,
                                     TemporalUnit largestUnit,
                                     Handle<Value> dateUntil,
                                     Duration* result) {
  MOZ_ASSERT(largestUnit <= TemporalUnit::Day);

  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDateUntil(cx, one, two, largestUnit, result);
  }

  Rooted<JSObject*> calendarObj(cx, calendar.toObject());

  // Fast-path for the default implementation.
  if (calendarObj->is<CalendarObject>() &&
      IsNativeFunction(dateUntil, Calendar_dateUntil)) {
    return BuiltinCalendarDateUntil(cx, one, two, largestUnit, result);
  }

  Rooted<JSObject*> untilOptions(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!untilOptions) {
    return false;
  }

  Rooted<Value> value(cx, StringValue(TemporalUnitToString(cx, largestUnit)));
  if (!DefineDataProperty(cx, untilOptions, cx->names().largestUnit, value)) {
    return false;
  }

  // Steps 2-5.
  return CalendarDateUntilSlow(cx, calendarObj, one, two, untilOptions,
                               dateUntil, result);
}

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool js::temporal::CalendarDateUntil(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<Wrapped<PlainDateObject*>> one,
                                     Handle<Wrapped<PlainDateObject*>> two,
                                     Handle<JSObject*> options,
                                     Duration* result) {
  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDateUntil(cx, one, two, options, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateUntil(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateUntil, &dateUntil)) {
    return false;
  }

  // Steps 3-5.
  return CalendarDateUntil(cx, calendar, one, two, options, dateUntil, result);
}

/**
 * CalendarDateUntil ( calendar, one, two, options [ , dateUntil ] )
 */
bool js::temporal::CalendarDateUntil(JSContext* cx,
                                     Handle<CalendarValue> calendar,
                                     Handle<Wrapped<PlainDateObject*>> one,
                                     Handle<Wrapped<PlainDateObject*>> two,
                                     TemporalUnit largestUnit,
                                     Duration* result) {
  MOZ_ASSERT(largestUnit <= TemporalUnit::Day);

  // Step 1.
  if (calendar.isString()) {
    return BuiltinCalendarDateUntil(cx, one, two, largestUnit, result);
  }

  // Step 2.
  Rooted<JSObject*> calendarObj(cx, calendar.toObject());
  Rooted<Value> dateUntil(cx);
  if (!GetMethodForCall(cx, calendarObj, cx->names().dateUntil, &dateUntil)) {
    return false;
  }

  // Steps 3-5.
  return CalendarDateUntil(cx, calendar, one, two, largestUnit, dateUntil,
                           result);
}

/**
 * CalendarEquals ( one, two )
 */
bool js::temporal::CalendarEquals(JSContext* cx, Handle<CalendarValue> one,
                                  Handle<CalendarValue> two, bool* equals) {
  // Step 1.
  if (one.isObject() && two.isObject() && one.toObject() == two.toObject()) {
    *equals = true;
    return true;
  }

  // Step 2.
  Rooted<JSString*> calendarOne(cx, ToTemporalCalendarIdentifier(cx, one));
  if (!calendarOne) {
    return false;
  }

  // Step 3.
  JSString* calendarTwo = ToTemporalCalendarIdentifier(cx, two);
  if (!calendarTwo) {
    return false;
  }

  // Steps 4-5.
  return EqualStrings(cx, calendarOne, calendarTwo, equals);
}

/**
 * CalendarEquals ( one, two )
 */
bool js::temporal::CalendarEqualsOrThrow(JSContext* cx,
                                         Handle<CalendarValue> one,
                                         Handle<CalendarValue> two) {
  // Step 1.
  if (one.isObject() && two.isObject() && one.toObject() == two.toObject()) {
    return true;
  }

  // Step 2.
  Rooted<JSString*> calendarOne(cx, ToTemporalCalendarIdentifier(cx, one));
  if (!calendarOne) {
    return false;
  }

  // Step 3.
  JSString* calendarTwo = ToTemporalCalendarIdentifier(cx, two);
  if (!calendarTwo) {
    return false;
  }

  // Steps 4-5.
  bool equals;
  if (!EqualStrings(cx, calendarOne, calendarTwo, &equals)) {
    return false;
  }
  if (equals) {
    return true;
  }

  // Throw an error when the calendar identifiers don't match. Used when unequal
  // calendars throw a RangeError.
  if (auto charsOne = QuoteString(cx, calendarOne)) {
    if (auto charsTwo = QuoteString(cx, calendarTwo)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE,
                               charsOne.get(), charsTwo.get());
    }
  }
  return false;
}

/**
 * ConsolidateCalendars ( one, two )
 */
bool js::temporal::ConsolidateCalendars(JSContext* cx,
                                        Handle<CalendarValue> one,
                                        Handle<CalendarValue> two,
                                        MutableHandle<CalendarValue> result) {
  // Step 1.
  if (one.isObject() && two.isObject() && one.toObject() == two.toObject()) {
    result.set(two);
    return true;
  }

  // Step 2.
  Rooted<JSString*> calendarOne(cx, ToTemporalCalendarIdentifier(cx, one));
  if (!calendarOne) {
    return false;
  }

  // Step 3.
  Rooted<JSString*> calendarTwo(cx, ToTemporalCalendarIdentifier(cx, two));
  if (!calendarTwo) {
    return false;
  }

  // Step 4.
  bool equals;
  if (!EqualStrings(cx, calendarOne, calendarTwo, &equals)) {
    return false;
  }
  if (equals) {
    result.set(two);
    return true;
  }

  // Step 5.
  bool isoCalendarOne;
  if (!IsISO8601Calendar(cx, calendarOne, &isoCalendarOne)) {
    return false;
  }
  if (isoCalendarOne) {
    result.set(two);
    return true;
  }

  // Step 6.
  bool isoCalendarTwo;
  if (!IsISO8601Calendar(cx, calendarTwo, &isoCalendarTwo)) {
    return false;
  }
  if (isoCalendarTwo) {
    result.set(one);
    return true;
  }

  // Step 7.
  if (auto charsOne = QuoteString(cx, calendarOne)) {
    if (auto charsTwo = QuoteString(cx, calendarTwo)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_CALENDAR_INCOMPATIBLE,
                               charsOne.get(), charsTwo.get());
    }
  }
  return false;
}

/**
 * Temporal.Calendar ( id )
 */
static bool CalendarConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.Calendar")) {
    return false;
  }

  // Step 2.
  if (!args.requireAtLeast(cx, "Temporal.Calendar", 1)) {
    return false;
  }

  if (!args[0].isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                     nullptr, "not a string");
    return false;
  }

  Rooted<JSLinearString*> identifier(cx, args[0].toString()->ensureLinear(cx));
  if (!identifier) {
    return false;
  }

  // Step 3.
  identifier = ThrowIfNotBuiltinCalendar(cx, identifier);
  if (!identifier) {
    return false;
  }

  // Step 4.
  auto* calendar = CreateTemporalCalendar(cx, args, identifier);
  if (!calendar) {
    return false;
  }

  args.rval().setObject(*calendar);
  return true;
}

/**
 * Temporal.Calendar.from ( item )
 */
static bool Calendar_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, args.get(0), &calendar)) {
    return false;
  }

  // Step 2.
  auto* obj = ToTemporalCalendarObject(cx, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * get Temporal.Calendar.prototype.id
 */
static bool Calendar_id(JSContext* cx, const CallArgs& args) {
  auto* calendar = &args.thisv().toObject().as<CalendarObject>();

  // Step 3.
  args.rval().setString(calendar->identifier());
  return true;
}

/**
 * get Temporal.Calendar.prototype.id
 */
static bool Calendar_id(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_id>(cx, args);
}

/**
 * Temporal.Calendar.prototype.dateFromFields ( fields [ , options ] )
 */
static bool Calendar_dateFromFields(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  Rooted<JSObject*> fields(
      cx, RequireObjectArg(cx, "fields", "dateFromFields", args.get(0)));
  if (!fields) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(1)) {
    options = RequireObjectArg(cx, "options", "dateFromFields", args[1]);
    if (!options) {
      return false;
    }
  }

  // Steps 6-10.
  auto* obj = BuiltinCalendarDateFromFields(cx, fields, options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Calendar.prototype.dateFromFields ( fields [ , options ] )
 */
static bool Calendar_dateFromFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_dateFromFields>(cx, args);
}

/**
 * Temporal.Calendar.prototype.yearMonthFromFields ( fields [ , options ] )
 */
static bool Calendar_yearMonthFromFields(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  Rooted<JSObject*> fields(
      cx, RequireObjectArg(cx, "fields", "yearMonthFromFields", args.get(0)));
  if (!fields) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(1)) {
    options = RequireObjectArg(cx, "options", "yearMonthFromFields", args[1]);
    if (!options) {
      return false;
    }
  }

  // Steps 6-10.
  auto* obj = BuiltinCalendarYearMonthFromFields(cx, fields, options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Calendar.prototype.yearMonthFromFields ( fields [ , options ] )
 */
static bool Calendar_yearMonthFromFields(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_yearMonthFromFields>(cx,
                                                                        args);
}

/**
 * Temporal.Calendar.prototype.monthDayFromFields ( fields [ , options ] )
 */
static bool Calendar_monthDayFromFields(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  Rooted<JSObject*> fields(
      cx, RequireObjectArg(cx, "fields", "monthDayFromFields", args.get(0)));
  if (!fields) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(1)) {
    options = RequireObjectArg(cx, "options", "monthDayFromFields", args[1]);
    if (!options) {
      return false;
    }
  }

  // Steps 6-10.
  auto* obj = BuiltinCalendarMonthDayFromFields(cx, fields, options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Calendar.prototype.monthDayFromFields ( fields [ , options ] )
 */
static bool Calendar_monthDayFromFields(JSContext* cx, unsigned argc,
                                        Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_monthDayFromFields>(cx,
                                                                       args);
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static bool Calendar_dateAdd(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Step 5.
  Duration duration;
  if (!ToTemporalDuration(cx, args.get(1), &duration)) {
    return false;
  }

  // Step 6.
  Rooted<JSObject*> options(cx);
  if (args.hasDefined(2)) {
    options = RequireObjectArg(cx, "options", "dateAdd", args[2]);
    if (!options) {
      return false;
    }
  }

  // Steps 7-10.
  auto* obj = BuiltinCalendarAdd(cx, date, duration, options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Calendar.prototype.dateAdd ( date, duration [ , options ] )
 */
static bool Calendar_dateAdd(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_dateAdd>(cx, args);
}

/**
 * Temporal.Calendar.prototype.dateUntil ( one, two [ , options ] )
 */
static bool Calendar_dateUntil(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate one;
  if (!ToTemporalDate(cx, args.get(0), &one)) {
    return false;
  }

  // Step 5.
  PlainDate two;
  if (!ToTemporalDate(cx, args.get(1), &two)) {
    return false;
  }

  // Steps 6-8.
  auto largestUnit = TemporalUnit::Day;
  if (args.hasDefined(2)) {
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "dateUntil", args[2]));
    if (!options) {
      return false;
    }

    // Steps 7-8.
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::LargestUnit,
                         TemporalUnitGroup::Date, &largestUnit)) {
      return false;
    }
  }

  // Steps 9-10.
  auto duration = BuiltinCalendarDateUntil(one, two, largestUnit);

  auto* obj = CreateTemporalDuration(cx, duration);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.Calendar.prototype.dateUntil ( one, two [ , options ] )
 */
static bool Calendar_dateUntil(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_dateUntil>(cx, args);
}

/**
 * Temporal.Calendar.prototype.year ( temporalDateLike )
 */
static bool Calendar_year(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.year ( temporalDateLike )
 */
static bool Calendar_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_year>(cx, args);
}

/**
 * Temporal.Calendar.prototype.month ( temporalDateLike )
 */
static bool Calendar_month(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  Handle<Value> temporalDateLike = args.get(0);

  // Step 4.
  if (temporalDateLike.isObject() &&
      temporalDateLike.toObject().canUnwrapAs<PlainMonthDayObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK,
                     temporalDateLike, nullptr, "a PlainMonthDay object");
    return false;
  }

  // Step 5.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, temporalDateLike, &date)) {
    return false;
  }

  // Steps 6-7.
  return BuiltinCalendarMonth(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.month ( temporalDateLike )
 */
static bool Calendar_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_month>(cx, args);
}

/**
 * Temporal.Calendar.prototype.monthCode ( temporalDateLike )
 */
static bool Calendar_monthCode(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainMonthDayObject,
                   PlainYearMonthObject>(cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarMonthCode(cx, date, args.rval());
}

/**
 * Temporal.Calendar.prototype.monthCode ( temporalDateLike )
 */
static bool Calendar_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_monthCode>(cx, args);
}

/**
 * Temporal.Calendar.prototype.day ( temporalDateLike )
 */
static bool Calendar_day(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainMonthDayObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarDay(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.day ( temporalDateLike )
 */
static bool Calendar_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_day>(cx, args);
}

/**
 * Temporal.Calendar.prototype.dayOfWeek ( temporalDateLike )
 */
static bool Calendar_dayOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-9.
  return BuiltinCalendarDayOfWeek(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.dayOfWeek ( temporalDateLike )
 */
static bool Calendar_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_dayOfWeek>(cx, args);
}

/**
 * Temporal.Calendar.prototype.dayOfYear ( temporalDateLike )
 */
static bool Calendar_dayOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-7.
  return BuiltinCalendarDayOfYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.dayOfYear ( temporalDateLike )
 */
static bool Calendar_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_dayOfYear>(cx, args);
}

/**
 * Temporal.Calendar.prototype.weekOfYear ( temporalDateLike )
 */
static bool Calendar_weekOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarWeekOfYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.weekOfYear ( temporalDateLike )
 */
static bool Calendar_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_weekOfYear>(cx, args);
}

/**
 * Temporal.Calendar.prototype.yearOfWeek ( temporalDateLike )
 */
static bool Calendar_yearOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarYearOfWeek(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.yearOfWeek ( temporalDateLike )
 */
static bool Calendar_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_yearOfWeek>(cx, args);
}

/**
 * Temporal.Calendar.prototype.daysInWeek ( temporalDateLike )
 */
static bool Calendar_daysInWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToTemporalDate(cx, args.get(0), &date)) {
    return false;
  }

  // Step 5.
  return BuiltinCalendarDaysInWeek(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.daysInWeek ( temporalDateLike )
 */
static bool Calendar_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_daysInWeek>(cx, args);
}

/**
 * Temporal.Calendar.prototype.daysInMonth ( temporalDateLike )
 */
static bool Calendar_daysInMonth(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Step 5.
  return BuiltinCalendarDaysInMonth(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.daysInMonth ( temporalDateLike )
 */
static bool Calendar_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_daysInMonth>(cx, args);
}

/**
 * Temporal.Calendar.prototype.daysInYear ( temporalDateLike )
 */
static bool Calendar_daysInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Step 5.
  return BuiltinCalendarDaysInYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.daysInYear ( temporalDateLike )
 */
static bool Calendar_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_daysInYear>(cx, args);
}

/**
 * Temporal.Calendar.prototype.monthsInYear ( temporalDateLike )
 */
static bool Calendar_monthsInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Step 5.
  return BuiltinCalendarMonthsInYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.monthsInYear ( temporalDateLike )
 */
static bool Calendar_monthsInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_monthsInYear>(cx, args);
}

/**
 * Temporal.Calendar.prototype.inLeapYear ( temporalDateLike )
 */
static bool Calendar_inLeapYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  PlainDate date;
  if (!ToPlainDate<PlainDateObject, PlainDateTimeObject, PlainYearMonthObject>(
          cx, args.get(0), &date)) {
    return false;
  }

  // Steps 5-6.
  return BuiltinCalendarInLeapYear(date, args.rval());
}

/**
 * Temporal.Calendar.prototype.inLeapYear ( temporalDateLike )
 */
static bool Calendar_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_inLeapYear>(cx, args);
}

/**
 * Temporal.Calendar.prototype.fields ( fields )
 */
static bool Calendar_fields(JSContext* cx, const CallArgs& args) {
  // Step 3.
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 4.
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(args.get(0))) {
    return false;
  }

  // Step 5.
  JS::RootedVector<Value> fieldNames(cx);
  mozilla::EnumSet<CalendarField> seen;

  // Steps 6-7.
  Rooted<Value> nextValue(cx);
  Rooted<JSLinearString*> linear(cx);
  while (true) {
    // Steps 7.a and 7.b.i.
    bool done;
    if (!iterator.next(&nextValue, &done)) {
      return false;
    }
    if (done) {
      break;
    }

    // Step 7.b.ii.
    if (!nextValue.isString()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, nextValue,
                       nullptr, "not a string");
      iterator.closeThrow();
      return false;
    }

    linear = nextValue.toString()->ensureLinear(cx);
    if (!linear) {
      return false;
    }

    // Step 7.b.iv. (Reordered)
    CalendarField field;
    if (!ToCalendarField(cx, linear, &field)) {
      iterator.closeThrow();
      return false;
    }

    // Step 7.b.iii.
    if (seen.contains(field)) {
      if (auto chars = QuoteString(cx, linear, '"')) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_CALENDAR_DUPLICATE_FIELD,
                                 chars.get());
      }
      iterator.closeThrow();
      return false;
    }

    // Step 7.b.v.
    if (!fieldNames.append(nextValue)) {
      return false;
    }
    seen += field;
  }

  // Step 8.
  auto* array =
      NewDenseCopiedArray(cx, fieldNames.length(), fieldNames.begin());
  if (!array) {
    return false;
  }

  args.rval().setObject(*array);
  return true;
}

/**
 * Temporal.Calendar.prototype.fields ( fields )
 */
static bool Calendar_fields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_fields>(cx, args);
}

/**
 * Temporal.Calendar.prototype.mergeFields ( fields, additionalFields )
 */
static bool Calendar_mergeFields(JSContext* cx, const CallArgs& args) {
  // Step 7. (Reordered)
  MOZ_ASSERT(IsISO8601Calendar(&args.thisv().toObject().as<CalendarObject>()));

  // Step 3.
  Rooted<JSObject*> fields(cx, JS::ToObject(cx, args.get(0)));
  if (!fields) {
    return false;
  }

  Rooted<PlainObject*> fieldsCopy(
      cx, SnapshotOwnPropertiesIgnoreUndefined(cx, fields));
  if (!fieldsCopy) {
    return false;
  }

  // Step 4.
  Rooted<JSObject*> additionalFields(cx, JS::ToObject(cx, args.get(1)));
  if (!additionalFields) {
    return false;
  }

  Rooted<PlainObject*> additionalFieldsCopy(
      cx, SnapshotOwnPropertiesIgnoreUndefined(cx, additionalFields));
  if (!additionalFieldsCopy) {
    return false;
  }

  // Steps 5-6.
  //
  // JSITER_HIDDEN doesn't need to be passed, because CopyDataProperties creates
  // all properties as enumerable.
  JS::RootedVector<PropertyKey> additionalKeys(cx);
  if (!GetPropertyKeys(cx, additionalFieldsCopy,
                       JSITER_OWNONLY | JSITER_SYMBOLS, &additionalKeys)) {
    return false;
  }

  // Step 8.
  Rooted<PropertyHashSet> overriddenKeys(cx, PropertyHashSet(cx));
  if (!ISOFieldKeysToIgnore(cx, additionalKeys, overriddenKeys.get())) {
    return false;
  }

  // Step 9.
  Rooted<PlainObject*> merged(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!merged) {
    return false;
  }

  // Steps 10-11.
  //
  // JSITER_HIDDEN doesn't need to be passed, because CopyDataProperties creates
  // all properties as enumerable.
  JS::RootedVector<PropertyKey> fieldsKeys(cx);
  if (!GetPropertyKeys(cx, fieldsCopy, JSITER_OWNONLY | JSITER_SYMBOLS,
                       &fieldsKeys)) {
    return false;
  }

  // Step 12.
  Rooted<Value> propValue(cx);
  for (size_t i = 0; i < fieldsKeys.length(); i++) {
    Handle<PropertyKey> key = fieldsKeys[i];

    // Step 12.a.
    // FIXME: spec issue - unnecessary initialisation
    // https://github.com/tc39/proposal-temporal/issues/2549

    // Steps 12.b-c.
    if (overriddenKeys.has(key)) {
      if (!GetProperty(cx, additionalFieldsCopy, additionalFieldsCopy, key,
                       &propValue)) {
        return false;
      }

      // Step 12.d. (Reordered)
      if (propValue.isUndefined()) {
        // The property can be undefined if the key is "month" or "monthCode".
        MOZ_ASSERT(key.isAtom(cx->names().month) ||
                   key.isAtom(cx->names().monthCode));

        continue;
      }
    } else {
      if (!GetProperty(cx, fieldsCopy, fieldsCopy, key, &propValue)) {
        return false;
      }

      // All properties of |fieldsCopy| have a non-undefined value.
      MOZ_ASSERT(!propValue.isUndefined());
    }

    // Step 12.d.
    if (!DefineDataProperty(cx, merged, key, propValue)) {
      return false;
    }
  }

  // Step 13.
  if (!CopyDataProperties(cx, merged, additionalFieldsCopy)) {
    return false;
  }

  // Step 14.
  args.rval().setObject(*merged);
  return true;
}

/**
 * Temporal.Calendar.prototype.mergeFields ( fields, additionalFields )
 */
static bool Calendar_mergeFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_mergeFields>(cx, args);
}

/**
 * Temporal.Calendar.prototype.toString ( )
 */
static bool Calendar_toString(JSContext* cx, const CallArgs& args) {
  auto* calendar = &args.thisv().toObject().as<CalendarObject>();

  // Step 3.
  args.rval().setString(calendar->identifier());
  return true;
}

/**
 * Temporal.Calendar.prototype.toString ( )
 */
static bool Calendar_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_toString>(cx, args);
}

/**
 * Temporal.Calendar.prototype.toJSON ( )
 */
static bool Calendar_toJSON(JSContext* cx, const CallArgs& args) {
  auto* calendar = &args.thisv().toObject().as<CalendarObject>();

  // Step 3.
  args.rval().setString(calendar->identifier());
  return true;
}

/**
 * Temporal.Calendar.prototype.toJSON ( )
 */
static bool Calendar_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsCalendar, Calendar_toJSON>(cx, args);
}

const JSClass CalendarObject::class_ = {
    "Temporal.Calendar",
    JSCLASS_HAS_RESERVED_SLOTS(CalendarObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_Calendar),
    JS_NULL_CLASS_OPS,
    &CalendarObject::classSpec_,
};

const JSClass& CalendarObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec Calendar_methods[] = {
    JS_FN("from", Calendar_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec Calendar_prototype_methods[] = {
    JS_FN("dateFromFields", Calendar_dateFromFields, 1, 0),
    JS_FN("yearMonthFromFields", Calendar_yearMonthFromFields, 1, 0),
    JS_FN("monthDayFromFields", Calendar_monthDayFromFields, 1, 0),
    JS_FN("dateAdd", Calendar_dateAdd, 2, 0),
    JS_FN("dateUntil", Calendar_dateUntil, 2, 0),
    JS_FN("year", Calendar_year, 1, 0),
    JS_FN("month", Calendar_month, 1, 0),
    JS_FN("monthCode", Calendar_monthCode, 1, 0),
    JS_FN("day", Calendar_day, 1, 0),
    JS_FN("dayOfWeek", Calendar_dayOfWeek, 1, 0),
    JS_FN("dayOfYear", Calendar_dayOfYear, 1, 0),
    JS_FN("weekOfYear", Calendar_weekOfYear, 1, 0),
    JS_FN("yearOfWeek", Calendar_yearOfWeek, 1, 0),
    JS_FN("daysInWeek", Calendar_daysInWeek, 1, 0),
    JS_FN("daysInMonth", Calendar_daysInMonth, 1, 0),
    JS_FN("daysInYear", Calendar_daysInYear, 1, 0),
    JS_FN("monthsInYear", Calendar_monthsInYear, 1, 0),
    JS_FN("inLeapYear", Calendar_inLeapYear, 1, 0),
    JS_FN("fields", Calendar_fields, 1, 0),
    JS_FN("mergeFields", Calendar_mergeFields, 2, 0),
    JS_FN("toString", Calendar_toString, 0, 0),
    JS_FN("toJSON", Calendar_toJSON, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec Calendar_prototype_properties[] = {
    JS_PSG("id", Calendar_id, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.Calendar", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec CalendarObject::classSpec_ = {
    GenericCreateConstructor<CalendarConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<CalendarObject>,
    Calendar_methods,
    nullptr,
    Calendar_prototype_methods,
    Calendar_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};

struct MOZ_STACK_CLASS CalendarNameAndNative final {
  PropertyName* name;
  JSNative native;
};

static CalendarNameAndNative GetCalendarNameAndNative(JSContext* cx,
                                                      CalendarField fieldName) {
  switch (fieldName) {
    case CalendarField::Year:
      return {cx->names().year, Calendar_year};
    case CalendarField::Month:
      return {cx->names().month, Calendar_month};
    case CalendarField::MonthCode:
      return {cx->names().monthCode, Calendar_monthCode};
    case CalendarField::Day:
      return {cx->names().day, Calendar_day};
  }
  MOZ_CRASH("invalid temporal field name");
}

bool js::temporal::IsBuiltinAccess(
    JSContext* cx, Handle<CalendarObject*> calendar,
    std::initializer_list<CalendarField> fieldNames) {
  // Don't optimize when the object has any own properties which may shadow the
  // built-in methods.
  if (!calendar->empty()) {
    return false;
  }

  JSObject* proto = cx->global()->maybeGetPrototype(JSProto_Calendar);

  // Don't attempt to optimize when the class isn't yet initialized.
  if (!proto) {
    return false;
  }

  // Don't optimize when the prototype isn't the built-in prototype.
  if (calendar->staticPrototype() != proto) {
    return false;
  }

  auto* nproto = &proto->as<NativeObject>();
  for (auto fieldName : fieldNames) {
    auto [name, native] = GetCalendarNameAndNative(cx, fieldName);
    auto prop = nproto->lookupPure(name);

    // Return if the property isn't a data property.
    if (!prop || !prop->isDataProperty()) {
      return false;
    }

    // Return if the property isn't the initial method.
    if (!IsNativeFunction(nproto->getSlot(prop->slot()), native)) {
      return false;
    }
  }

  // TODO: Pass accessor list from caller to avoid excessive checks.

  // Additionally check the various calendar fields operations.
  for (const auto& [name, native] : (CalendarNameAndNative[]){
           {cx->names().fields, Calendar_fields},
           {cx->names().mergeFields, Calendar_mergeFields},
           {cx->names().dateFromFields, Calendar_dateFromFields},
           {cx->names().monthDayFromFields, Calendar_monthDayFromFields},
           {cx->names().yearMonthFromFields, Calendar_yearMonthFromFields},
       }) {
    auto prop = nproto->lookupPure(name);

    // Return if the property isn't a data property.
    if (!prop || !prop->isDataProperty()) {
      return false;
    }

    // Return if the property isn't the initial method.
    if (!IsNativeFunction(nproto->getSlot(prop->slot()), native)) {
      return false;
    }
  }

  // CalendarFields observably uses array iteration.
  ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
  if (!stubChain) {
    cx->recoverFromOutOfMemory();
    return false;
  }

  bool arrayIterationSane;
  if (!stubChain->tryOptimizeArray(cx, &arrayIterationSane)) {
    cx->recoverFromOutOfMemory();
    return false;
  }
  if (!arrayIterationSane) {
    return false;
  }

  // Success! The access can be optimized.
  return true;
}

bool js::temporal::IsBuiltinAccessForStringCalendar(JSContext* cx) {
  // CalendarFields for string calendars observably uses array iteration.
  // TODO: check again after
  // https://github.com/tc39/proposal-temporal/pull/2519

  ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
  if (!stubChain) {
    cx->recoverFromOutOfMemory();
    return false;
  }

  bool arrayIterationSane;
  if (!stubChain->tryOptimizeArray(cx, &arrayIterationSane)) {
    cx->recoverFromOutOfMemory();
    return false;
  }
  if (!arrayIterationSane) {
    return false;
  }

  // Success! The access can be optimized.
  return true;
}
