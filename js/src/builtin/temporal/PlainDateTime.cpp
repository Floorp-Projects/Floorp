/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDateTime.h"

#include "mozilla/Assertions.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <stddef.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
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
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsPlainDateTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainDateTimeObject>();
}

#ifdef DEBUG
/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
bool js::temporal::IsValidISODateTime(const PlainDateTime& dateTime) {
  return IsValidISODate(dateTime.date) && IsValidTime(dateTime.time);
}
#endif

/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
static bool ThrowIfInvalidISODateTime(JSContext* cx,
                                      const PlainDateTime& dateTime) {
  return ThrowIfInvalidISODate(cx, dateTime.date) &&
         ThrowIfInvalidTime(cx, dateTime.time);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day, T hour, T minute,
                                    T second, T millisecond, T microsecond,
                                    T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  MOZ_ASSERT(IsValidISODate(year, month, day));
  MOZ_ASSERT(
      IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day != (20 - 1)) {
      return day > (20 - 1);
    }
    // Needs to be past midnight on April, 19.
    return !(hour == 0 && minute == 0 && second == 0 && millisecond == 0 &&
             microsecond == 0 && nanosecond == 0);
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  MOZ_ASSERT(IsValidISODate(year, month, day));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // ISODateTimeWithinLimits is called with hour=12 and the remaining time
  // components set to zero. That means the maximum value is exclusive, whereas
  // the minimum value is inclusive.

  // FIXME: spec bug - GetUTCEpochNanoseconds when called with large |year| may
  // cause MakeDay to return NaN, which makes MakeDate return NaN, which is
  // unexpected in GetUTCEpochNanoseconds, step 4.
  // https://github.com/tc39/proposal-temporal/issues/2315

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day < (20 - 1)) {
      return false;
    }
    return true;
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(double year, double month,
                                           double day) {
  return ::ISODateTimeWithinLimits(year, month, day);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDateTime& dateTime) {
  auto& [date, time] = dateTime;
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day, time.hour,
                                   time.minute, time.second, time.millisecond,
                                   time.microsecond, time.nanosecond);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDate& date) {
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day);
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
static PlainDateTimeObject* CreateTemporalDateTime(
    JSContext* cx, const CallArgs& args, double isoYear, double isoMonth,
    double isoDay, double hour, double minute, double second,
    double millisecond, double microsecond, double nanosecond,
    Handle<JSObject*> calendar) {
  MOZ_ASSERT(IsInteger(isoYear));
  MOZ_ASSERT(IsInteger(isoMonth));
  MOZ_ASSERT(IsInteger(isoDay));
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, isoYear, isoMonth, isoDay)) {
    return nullptr;
  }

  // Step 2.
  if (!ThrowIfInvalidTime(cx, hour, minute, second, millisecond, microsecond,
                          nanosecond)) {
    return nullptr;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(isoYear, isoMonth, isoDay, hour, minute, second,
                               millisecond, microsecond, nanosecond)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return nullptr;
  }

  // Steps 4-5.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainDateTime,
                                          &proto)) {
    return nullptr;
  }

  auto* dateTime = NewObjectWithClassProto<PlainDateTimeObject>(cx, proto);
  if (!dateTime) {
    return nullptr;
  }

  // Step 6.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_YEAR_SLOT,
                         Int32Value(isoYear));

  // Step 7.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MONTH_SLOT,
                         Int32Value(isoMonth));

  // Step 8.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 9.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 10.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MINUTE_SLOT,
                         Int32Value(minute));

  // Step 11.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_SECOND_SLOT,
                         Int32Value(second));

  // Step 12.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MILLISECOND_SLOT,
                         Int32Value(millisecond));

  // Step 13.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_MICROSECOND_SLOT,
                         Int32Value(microsecond));

  // Step 14.
  dateTime->setFixedSlot(PlainDateTimeObject::ISO_NANOSECOND_SLOT,
                         Int32Value(nanosecond));

  // Step 15.
  dateTime->setFixedSlot(PlainDateTimeObject::CALENDAR_SLOT,
                         ObjectValue(*calendar));

  // Step 16.
  return dateTime;
}

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
PlainDateTimeObject* js::temporal::CreateTemporalDateTime(
    JSContext* cx, const PlainDateTime& dateTime, Handle<JSObject*> calendar) {
  auto& [date, time] = dateTime;
  auto& [isoYear, isoMonth, isoDay] = date;
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;

  // Steps 1-2.
  if (!ThrowIfInvalidISODateTime(cx, dateTime)) {
    return nullptr;
  }

  // Step 3.
  if (!ISODateTimeWithinLimits(dateTime)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_DATE_TIME_INVALID);
    return nullptr;
  }

  // Steps 4-5.
  auto* object = NewBuiltinClassInstance<PlainDateTimeObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 6.
  object->setFixedSlot(PlainDateTimeObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 7.
  object->setFixedSlot(PlainDateTimeObject::ISO_MONTH_SLOT,
                       Int32Value(isoMonth));

  // Step 8.
  object->setFixedSlot(PlainDateTimeObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 9.
  object->setFixedSlot(PlainDateTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 10.
  object->setFixedSlot(PlainDateTimeObject::ISO_MINUTE_SLOT,
                       Int32Value(minute));

  // Step 11.
  object->setFixedSlot(PlainDateTimeObject::ISO_SECOND_SLOT,
                       Int32Value(second));

  // Step 12.
  object->setFixedSlot(PlainDateTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 13.
  object->setFixedSlot(PlainDateTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 14.
  object->setFixedSlot(PlainDateTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 15.
  object->setFixedSlot(PlainDateTimeObject::CALENDAR_SLOT,
                       ObjectValue(*calendar));

  // Step 16.
  return object;
}

/**
 * InterpretTemporalDateTimeFields ( calendar, fields, options )
 */
bool js::temporal::InterpretTemporalDateTimeFields(JSContext* cx,
                                                   Handle<JSObject*> calendar,
                                                   Handle<PlainObject*> fields,
                                                   Handle<JSObject*> options,
                                                   PlainDateTime* result) {
  // Step 1.
  TimeRecord timeResult;
  if (!ToTemporalTimeRecord(cx, fields, &timeResult)) {
    return false;
  }

  // Step 2.
  auto overflow = TemporalOverflow::Constrain;
  if (!ToTemporalOverflow(cx, options, &overflow)) {
    return false;
  }

  // Step 3.
  auto temporalDate =
      js::temporal::CalendarDateFromFields(cx, calendar, fields, options);
  if (!temporalDate) {
    return false;
  }
  auto date = ToPlainDate(&temporalDate.unwrap());

  // Step 4.
  PlainTime time;
  if (!RegulateTime(cx, timeResult, overflow, &time)) {
    return false;
  }

  // Step 5.
  *result = {date, time};
  return true;
}

/**
 * InterpretTemporalDateTimeFields ( calendar, fields, options )
 */
bool js::temporal::InterpretTemporalDateTimeFields(JSContext* cx,
                                                   Handle<JSObject*> calendar,
                                                   Handle<PlainObject*> fields,
                                                   PlainDateTime* result) {
  // Step 1.
  TimeRecord timeResult;
  if (!ToTemporalTimeRecord(cx, fields, &timeResult)) {
    return false;
  }

  // Step 2.
  auto overflow = TemporalOverflow::Constrain;

  // Step 3.
  auto temporalDate = CalendarDateFromFields(cx, calendar, fields);
  if (!temporalDate) {
    return false;
  }
  auto date = ToPlainDate(&temporalDate.unwrap());

  // Step 4.
  PlainTime time;
  if (!RegulateTime(cx, timeResult, overflow, &time)) {
    return false;
  }

  // Step 5.
  *result = {date, time};
  return true;
}

/**
 * ToTemporalDateTime ( item [ , options ] )
 */
static Wrapped<PlainDateTimeObject*> ToTemporalDateTime(
    JSContext* cx, Handle<Value> item, Handle<JSObject*> maybeOptions) {
  // Steps 1-2. (Not applicable)

  // Steps 3-4.
  Rooted<JSObject*> calendar(cx);
  PlainDateTime result;
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());

    // Step 3.a.
    if (itemObj->canUnwrapAs<PlainDateTimeObject>()) {
      return itemObj;
    }

    // Step 3.b.
    if (auto* zonedDateTime = itemObj->maybeUnwrapIf<ZonedDateTimeObject>()) {
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
      return GetPlainDateTimeFor(cx, timeZone, epochInstant, calendar);
    }

    // Step 3.c.
    if (auto* date = itemObj->maybeUnwrapIf<PlainDateObject>()) {
      PlainDateTime dateTime = {ToPlainDate(date), {}};
      Rooted<JSObject*> calendar(cx, date->calendar());
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
      return CreateTemporalDateTime(cx, dateTime, calendar);
    }

    // Step 3.d.
    calendar = GetTemporalCalendarWithISODefault(cx, itemObj);
    if (!calendar) {
      return nullptr;
    }

    // Step 3.e.
    JS::RootedVector<PropertyKey> fieldNames(cx);
    if (!CalendarFields(cx, calendar,
                        {CalendarField::Day, CalendarField::Hour,
                         CalendarField::Microsecond, CalendarField::Millisecond,
                         CalendarField::Minute, CalendarField::Month,
                         CalendarField::MonthCode, CalendarField::Nanosecond,
                         CalendarField::Second, CalendarField::Year},
                        &fieldNames)) {
      return nullptr;
    }

    // Step 3.f.
    Rooted<PlainObject*> fields(cx,
                                PrepareTemporalFields(cx, itemObj, fieldNames));
    if (!fields) {
      return nullptr;
    }

    // Step 3.g.
    if (maybeOptions) {
      if (!InterpretTemporalDateTimeFields(cx, calendar, fields, maybeOptions,
                                           &result)) {
        return nullptr;
      }
    } else {
      if (!InterpretTemporalDateTimeFields(cx, calendar, fields, &result)) {
        return nullptr;
      }
    }
  } else {
    // Step 4.a.
    if (maybeOptions) {
      TemporalOverflow ignored;
      if (!ToTemporalOverflow(cx, maybeOptions, &ignored)) {
        return nullptr;
      }
    }

    // Step 4.b.
    Rooted<JSString*> string(cx, JS::ToString(cx, item));
    if (!string) {
      return nullptr;
    }

    // Step 4.c.
    Rooted<JSString*> calendarString(cx);
    if (!ParseTemporalDateTimeString(cx, string, &result, &calendarString)) {
      return nullptr;
    }

    // Step 4.d.
    MOZ_ASSERT(IsValidISODate(result.date));

    // Step 4.e.
    MOZ_ASSERT(IsValidTime(result.time));

    // Step 4.f.
    Rooted<Value> calendarValue(cx);
    if (calendarString) {
      calendarValue.setString(calendarString);
    }

    calendar = ToTemporalCalendarWithISODefault(cx, calendarValue);
    if (!calendar) {
      return nullptr;
    }
  }

  // Step 5.
  return CreateTemporalDateTime(cx, result, calendar);
}

/**
 * ToTemporalDateTime ( item [ , options ] )
 */
Wrapped<PlainDateTimeObject*> js::temporal::ToTemporalDateTime(
    JSContext* cx, Handle<Value> item) {
  return ::ToTemporalDateTime(cx, item, nullptr);
}

/**
 * ToTemporalDateTime ( item [ , options ] )
 */
bool js::temporal::ToTemporalDateTime(JSContext* cx, Handle<Value> item,
                                      PlainDateTime* result) {
  auto obj = ::ToTemporalDateTime(cx, item, nullptr);
  if (!obj) {
    return false;
  }

  *result = ToPlainDateTime(&obj.unwrap());
  return true;
}

/**
 * Temporal.PlainDateTime ( isoYear, isoMonth, isoDay [ , hour [ , minute [ ,
 * second [ , millisecond [ , microsecond [ , nanosecond [ , calendarLike ] ] ]
 * ] ] ] ] )
 */
static bool PlainDateTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainDateTime")) {
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
  double hour = 0;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "hour", &hour)) {
      return false;
    }
  }

  // Step 6.
  double minute = 0;
  if (args.hasDefined(4)) {
    if (!ToIntegerWithTruncation(cx, args[4], "minute", &minute)) {
      return false;
    }
  }

  // Step 7.
  double second = 0;
  if (args.hasDefined(5)) {
    if (!ToIntegerWithTruncation(cx, args[5], "second", &second)) {
      return false;
    }
  }

  // Step 8.
  double millisecond = 0;
  if (args.hasDefined(6)) {
    if (!ToIntegerWithTruncation(cx, args[6], "millisecond", &millisecond)) {
      return false;
    }
  }

  // Step 9.
  double microsecond = 0;
  if (args.hasDefined(7)) {
    if (!ToIntegerWithTruncation(cx, args[7], "microsecond", &microsecond)) {
      return false;
    }
  }

  // Step 10.
  double nanosecond = 0;
  if (args.hasDefined(8)) {
    if (!ToIntegerWithTruncation(cx, args[8], "nanosecond", &nanosecond)) {
      return false;
    }
  }

  // Step 11.
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(9)));
  if (!calendar) {
    return false;
  }

  // Step 12.
  auto* temporalDateTime = CreateTemporalDateTime(
      cx, args, isoYear, isoMonth, isoDay, hour, minute, second, millisecond,
      microsecond, nanosecond, calendar);
  if (!temporalDateTime) {
    return false;
  }

  args.rval().setObject(*temporalDateTime);
  return true;
}

/**
 * Temporal.PlainDateTime.from ( item [ , options ] )
 */
static bool PlainDateTime_from(JSContext* cx, unsigned argc, Value* vp) {
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
    if (auto* temporalDateTime = item->maybeUnwrapIf<PlainDateTimeObject>()) {
      auto dateTime = ToPlainDateTime(temporalDateTime);

      Rooted<JSObject*> calendar(cx, temporalDateTime->calendar());
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
      auto* result = CreateTemporalDateTime(cx, dateTime, calendar);
      if (!result) {
        return false;
      }

      args.rval().setObject(*result);
      return true;
    }
  }

  // Step 3.
  auto result = ToTemporalDateTime(cx, args.get(0), options);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.calendar
 */
static bool PlainDateTime_calendar(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setObject(*dateTime->calendar());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.calendar
 */
static bool PlainDateTime_calendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_calendar>(cx,
                                                                       args);
}

/**
 * get Temporal.PlainDateTime.prototype.year
 */
static bool PlainDateTime_year(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.year
 */
static bool PlainDateTime_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_year>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.month
 */
static bool PlainDateTime_month(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarMonth(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.month
 */
static bool PlainDateTime_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_month>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.monthCode
 */
static bool PlainDateTime_monthCode(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarMonthCode(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.monthCode
 */
static bool PlainDateTime_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.day
 */
static bool PlainDateTime_day(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDay(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.day
 */
static bool PlainDateTime_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_day>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.hour
 */
static bool PlainDateTime_hour(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoHour());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.hour
 */
static bool PlainDateTime_hour(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_hour>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.minute
 */
static bool PlainDateTime_minute(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMinute());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.minute
 */
static bool PlainDateTime_minute(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_minute>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.second
 */
static bool PlainDateTime_second(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoSecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.second
 */
static bool PlainDateTime_second(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_second>(cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.millisecond
 */
static bool PlainDateTime_millisecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMillisecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.millisecond
 */
static bool PlainDateTime_millisecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_millisecond>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.microsecond
 */
static bool PlainDateTime_microsecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoMicrosecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.microsecond
 */
static bool PlainDateTime_microsecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_microsecond>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.nanosecond
 */
static bool PlainDateTime_nanosecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  args.rval().setInt32(dateTime->isoNanosecond());
  return true;
}

/**
 * get Temporal.PlainDateTime.prototype.nanosecond
 */
static bool PlainDateTime_nanosecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_nanosecond>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfWeek
 */
static bool PlainDateTime_dayOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDayOfWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfWeek
 */
static bool PlainDateTime_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_dayOfWeek>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfYear
 */
static bool PlainDateTime_dayOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDayOfYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.dayOfYear
 */
static bool PlainDateTime_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_dayOfYear>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainDateTime.prototype.weekOfYear
 */
static bool PlainDateTime_weekOfYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarWeekOfYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.weekOfYear
 */
static bool PlainDateTime_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_weekOfYear>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.yearOfWeek
 */
static bool PlainDateTime_yearOfWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarYearOfWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.yearOfWeek
 */
static bool PlainDateTime_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_yearOfWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInWeek
 */
static bool PlainDateTime_daysInWeek(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDaysInWeek(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInWeek
 */
static bool PlainDateTime_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInMonth
 */
static bool PlainDateTime_daysInMonth(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDaysInMonth(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInMonth
 */
static bool PlainDateTime_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInMonth>(cx,
                                                                          args);
}

/**
 * get Temporal.PlainDateTime.prototype.daysInYear
 */
static bool PlainDateTime_daysInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarDaysInYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.daysInYear
 */
static bool PlainDateTime_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_daysInYear>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainDateTime.prototype.monthsInYear
 */
static bool PlainDateTime_monthsInYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarMonthsInYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.monthsInYear
 */
static bool PlainDateTime_monthsInYear(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_monthsInYear>(
      cx, args);
}

/**
 * get Temporal.PlainDateTime.prototype.inLeapYear
 */
static bool PlainDateTime_inLeapYear(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  return CalendarInLeapYear(cx, calendar, args.thisv(), args.rval());
}

/**
 * get Temporal.PlainDateTime.prototype.inLeapYear
 */
static bool PlainDateTime_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_inLeapYear>(cx,
                                                                         args);
}

/**
 * Temporal.PlainDateTime.prototype.valueOf ( )
 */
static bool PlainDateTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainDateTime", "primitive type");
  return false;
}

/**
 * Temporal.PlainDateTime.prototype.getISOFields ( )
 */
static bool PlainDateTime_getISOFields(JSContext* cx, const CallArgs& args) {
  auto* temporalDateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  auto dateTime = ToPlainDateTime(temporalDateTime);
  JSObject* calendar = temporalDateTime->calendar();

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoDay),
                          Int32Value(dateTime.date.day))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoHour),
                          Int32Value(dateTime.time.hour))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoMicrosecond),
                          Int32Value(dateTime.time.microsecond))) {
    return false;
  }

  // Step 8.
  if (!fields.emplaceBack(NameToId(cx->names().isoMillisecond),
                          Int32Value(dateTime.time.millisecond))) {
    return false;
  }

  // Step 9.
  if (!fields.emplaceBack(NameToId(cx->names().isoMinute),
                          Int32Value(dateTime.time.minute))) {
    return false;
  }

  // Step 10.
  if (!fields.emplaceBack(NameToId(cx->names().isoMonth),
                          Int32Value(dateTime.date.month))) {
    return false;
  }

  // Step 11.
  if (!fields.emplaceBack(NameToId(cx->names().isoNanosecond),
                          Int32Value(dateTime.time.nanosecond))) {
    return false;
  }

  // Step 12.
  if (!fields.emplaceBack(NameToId(cx->names().isoSecond),
                          Int32Value(dateTime.time.second))) {
    return false;
  }

  // Step 13.
  if (!fields.emplaceBack(NameToId(cx->names().isoYear),
                          Int32Value(dateTime.date.year))) {
    return false;
  }

  // Step 14.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.getISOFields ( )
 */
static bool PlainDateTime_getISOFields(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_getISOFields>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainDate ( )
 */
static bool PlainDateTime_toPlainDate(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 3.
  auto* obj = CreateTemporalDate(cx, ToPlainDate(dateTime), calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainDate ( )
 */
static bool PlainDateTime_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainDate>(cx,
                                                                          args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainYearMonth ( )
 */
static bool PlainDateTime_toPlainYearMonth(JSContext* cx,
                                           const CallArgs& args) {
  Rooted<PlainDateTimeObject*> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());

  // Step 3.
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::MonthCode, CalendarField::Year},
                      &fieldNames)) {
    return false;
  }

  // Step 4.
  Rooted<PlainObject*> fields(cx,
                              PrepareTemporalFields(cx, dateTime, fieldNames));
  if (!fields) {
    return false;
  }

  // Step 5.
  auto obj = CalendarYearMonthFromFields(cx, calendar, fields);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainYearMonth ( )
 */
static bool PlainDateTime_toPlainYearMonth(JSContext* cx, unsigned argc,
                                           Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainYearMonth>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainMonthDay ( )
 */
static bool PlainDateTime_toPlainMonthDay(JSContext* cx, const CallArgs& args) {
  Rooted<PlainDateTimeObject*> dateTime(
      cx, &args.thisv().toObject().as<PlainDateTimeObject>());

  // Step 3.
  Rooted<JSObject*> calendar(cx, dateTime->calendar());

  // Step 4.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::MonthCode},
                      &fieldNames)) {
    return false;
  }

  // Step 4.
  Rooted<PlainObject*> fields(cx,
                              PrepareTemporalFields(cx, dateTime, fieldNames));
  if (!fields) {
    return false;
  }

  // Step 5.
  auto obj = CalendarMonthDayFromFields(cx, calendar, fields);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainMonthDay ( )
 */
static bool PlainDateTime_toPlainMonthDay(JSContext* cx, unsigned argc,
                                          Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainMonthDay>(
      cx, args);
}

/**
 * Temporal.PlainDateTime.prototype.toPlainTime ( )
 */
static bool PlainDateTime_toPlainTime(JSContext* cx, const CallArgs& args) {
  auto* dateTime = &args.thisv().toObject().as<PlainDateTimeObject>();

  // Step 3.
  auto* obj = CreateTemporalTime(cx, ToPlainTime(dateTime));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainDateTime.prototype.toPlainTime ( )
 */
static bool PlainDateTime_toPlainTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainDateTime, PlainDateTime_toPlainTime>(cx,
                                                                          args);
}

const JSClass PlainDateTimeObject::class_ = {
    "Temporal.PlainDateTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDateTime),
    JS_NULL_CLASS_OPS,
    &PlainDateTimeObject::classSpec_,
};

const JSClass& PlainDateTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDateTime_methods[] = {
    JS_FN("from", PlainDateTime_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainDateTime_prototype_methods[] = {
    JS_FN("valueOf", PlainDateTime_valueOf, 0, 0),
    JS_FN("toPlainDate", PlainDateTime_toPlainDate, 0, 0),
    JS_FN("toPlainYearMonth", PlainDateTime_toPlainYearMonth, 0, 0),
    JS_FN("toPlainMonthDay", PlainDateTime_toPlainMonthDay, 0, 0),
    JS_FN("toPlainTime", PlainDateTime_toPlainTime, 0, 0),
    JS_FN("getISOFields", PlainDateTime_getISOFields, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainDateTime_prototype_properties[] = {
    JS_PSG("calendar", PlainDateTime_calendar, 0),
    JS_PSG("year", PlainDateTime_year, 0),
    JS_PSG("month", PlainDateTime_month, 0),
    JS_PSG("monthCode", PlainDateTime_monthCode, 0),
    JS_PSG("day", PlainDateTime_day, 0),
    JS_PSG("hour", PlainDateTime_hour, 0),
    JS_PSG("minute", PlainDateTime_minute, 0),
    JS_PSG("second", PlainDateTime_second, 0),
    JS_PSG("millisecond", PlainDateTime_millisecond, 0),
    JS_PSG("microsecond", PlainDateTime_microsecond, 0),
    JS_PSG("nanosecond", PlainDateTime_nanosecond, 0),
    JS_PSG("dayOfWeek", PlainDateTime_dayOfWeek, 0),
    JS_PSG("dayOfYear", PlainDateTime_dayOfYear, 0),
    JS_PSG("weekOfYear", PlainDateTime_weekOfYear, 0),
    JS_PSG("yearOfWeek", PlainDateTime_yearOfWeek, 0),
    JS_PSG("daysInWeek", PlainDateTime_daysInWeek, 0),
    JS_PSG("daysInMonth", PlainDateTime_daysInMonth, 0),
    JS_PSG("daysInYear", PlainDateTime_daysInYear, 0),
    JS_PSG("monthsInYear", PlainDateTime_monthsInYear, 0),
    JS_PSG("inLeapYear", PlainDateTime_inLeapYear, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainDateTime", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainDateTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainDateTimeConstructor, 3,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateTimeObject>,
    PlainDateTime_methods,
    nullptr,
    PlainDateTime_prototype_methods,
    PlainDateTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
