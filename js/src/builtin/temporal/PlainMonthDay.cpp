/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainMonthDay.h"

#include "mozilla/Assertions.h"

#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/ToString.h"
#include "builtin/temporal/Wrapped.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/BytecodeUtil.h"
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

static inline bool IsPlainMonthDay(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainMonthDayObject>();
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
static PlainMonthDayObject* CreateTemporalMonthDay(
    JSContext* cx, const CallArgs& args, double isoYear, double isoMonth,
    double isoDay, Handle<CalendarValue> calendar) {
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
                              JSMSG_TEMPORAL_PLAIN_MONTH_DAY_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainMonthDay,
                                          &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<PlainMonthDayObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainMonthDayObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 6.
  obj->setFixedSlot(PlainMonthDayObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 7.
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, calendar.toValue());

  // Step 8.
  obj->setFixedSlot(PlainMonthDayObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 9.
  return obj;
}

/**
 * CreateTemporalMonthDay ( isoMonth, isoDay, calendar, referenceISOYear [ ,
 * newTarget ] )
 */
PlainMonthDayObject* js::temporal::CreateTemporalMonthDay(
    JSContext* cx, const PlainDate& date, Handle<CalendarValue> calendar) {
  auto& [isoYear, isoMonth, isoDay] = date;

  // Step 1.
  if (!ThrowIfInvalidISODate(cx, date)) {
    return nullptr;
  }

  // Step 2.
  if (!ISODateTimeWithinLimits(date)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_MONTH_DAY_INVALID);
    return nullptr;
  }

  // Steps 3-4.
  auto* obj = NewBuiltinClassInstance<PlainMonthDayObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 5.
  obj->setFixedSlot(PlainMonthDayObject::ISO_MONTH_SLOT, Int32Value(isoMonth));

  // Step 6.
  obj->setFixedSlot(PlainMonthDayObject::ISO_DAY_SLOT, Int32Value(isoDay));

  // Step 7.
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, calendar.toValue());

  // Step 8.
  obj->setFixedSlot(PlainMonthDayObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 9.
  return obj;
}

template <typename T, typename... Ts>
static bool ToTemporalCalendarForMonthDay(JSContext* cx,
                                          Handle<JSObject*> object,
                                          MutableHandle<CalendarValue> result) {
  if (auto* unwrapped = object->maybeUnwrapIf<T>()) {
    result.set(unwrapped->calendar());
    return result.wrap(cx);
  }

  if constexpr (sizeof...(Ts) > 0) {
    return ToTemporalCalendarForMonthDay<Ts...>(cx, object, result);
  }

  result.set(CalendarValue());
  return true;
}

/**
 * ToTemporalMonthDay ( item [ , options ] )
 */
static Wrapped<PlainMonthDayObject*> ToTemporalMonthDay(
    JSContext* cx, Handle<Value> item,
    Handle<JSObject*> maybeOptions = nullptr) {
  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  Rooted<PlainObject*> maybeResolvedOptions(cx);
  if (maybeOptions) {
    maybeResolvedOptions = SnapshotOwnProperties(cx, maybeOptions);
    if (!maybeResolvedOptions) {
      return nullptr;
    }
  }

  // Step 3.
  constexpr int32_t referenceISOYear = 1972;

  // Step 4.
  if (item.isObject()) {
    Rooted<JSObject*> itemObj(cx, &item.toObject());

    // Step 4.a.
    if (itemObj->canUnwrapAs<PlainMonthDayObject>()) {
      return itemObj;
    }

    // Steps 4.b-c.
    Rooted<CalendarValue> calendar(cx);
    bool calendarAbsent = false;
    if (!::ToTemporalCalendarForMonthDay<PlainDateObject, PlainDateTimeObject,
                                         PlainYearMonthObject,
                                         ZonedDateTimeObject>(cx, itemObj,
                                                              &calendar)) {
      return nullptr;
    }
    if (!calendar) {
      // Step 4.c.i.
      Rooted<Value> calendarLike(cx);
      if (!GetProperty(cx, itemObj, itemObj, cx->names().calendar,
                       &calendarLike)) {
        return nullptr;
      }

      // Steps 4.c.ii-iii.
      calendarAbsent = calendarLike.isUndefined();

      // Step 4.c.iv.
      if (!ToTemporalCalendarWithISODefault(cx, calendarLike, &calendar)) {
        return nullptr;
      }
    }

    // Step 4.d.
    JS::RootedVector<PropertyKey> fieldNames(cx);
    if (!CalendarFields(cx, calendar,
                        {CalendarField::Day, CalendarField::Month,
                         CalendarField::MonthCode, CalendarField::Year},
                        &fieldNames)) {
      return nullptr;
    }

    // Step 4.e.
    Rooted<PlainObject*> fields(cx,
                                PrepareTemporalFields(cx, itemObj, fieldNames));
    if (!fields) {
      return nullptr;
    }

    // Step 4.f.
    Rooted<Value> month(cx);
    if (!GetProperty(cx, fields, fields, cx->names().month, &month)) {
      return nullptr;
    }

    // Step 4.g.
    Rooted<Value> monthCode(cx);
    if (!GetProperty(cx, fields, fields, cx->names().monthCode, &monthCode)) {
      return nullptr;
    }

    // Step 4.h.
    Rooted<Value> year(cx);
    if (!GetProperty(cx, fields, fields, cx->names().year, &year)) {
      return nullptr;
    }

    // Step 4.i.
    if (calendarAbsent && !month.isUndefined() && monthCode.isUndefined() &&
        year.isUndefined()) {
      year.setInt32(referenceISOYear);
      if (!DefineDataProperty(cx, fields, cx->names().year, year)) {
        return nullptr;
      }
    }

    // Step 4.j.
    if (maybeResolvedOptions) {
      return js::temporal::CalendarMonthDayFromFields(cx, calendar, fields,
                                                      maybeResolvedOptions);
    }
    return js::temporal::CalendarMonthDayFromFields(cx, calendar, fields);
  }

  // Step 5.
  if (!item.isString()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, item,
                     nullptr, "not a string");
    return nullptr;
  }
  Rooted<JSString*> string(cx, item.toString());

  // Step 6.
  PlainDate result;
  bool hasYear;
  Rooted<JSString*> calendarString(cx);
  if (!ParseTemporalMonthDayString(cx, string, &result, &hasYear,
                                   &calendarString)) {
    return nullptr;
  }

  // Steps 7-10.
  Rooted<CalendarValue> calendar(cx, CalendarValue(cx->names().iso8601));
  if (calendarString) {
    if (!ToBuiltinCalendar(cx, calendarString, &calendar)) {
      return nullptr;
    }
  }

  // Step 11.
  if (maybeResolvedOptions) {
    TemporalOverflow ignored;
    if (!ToTemporalOverflow(cx, maybeResolvedOptions, &ignored)) {
      return nullptr;
    }
  }

  // Step 12.
  if (!hasYear) {
    // Step 12.a.
    return CreateTemporalMonthDay(
        cx, {referenceISOYear, result.month, result.day}, calendar);
  }

  // Step 13.
  Rooted<PlainMonthDayObject*> obj(
      cx, CreateTemporalMonthDay(cx, result, calendar));
  if (!obj) {
    return nullptr;
  }

  // Steps 14-15.
  return CalendarMonthDayFromFields(cx, calendar, obj);
}

/**
 * ToTemporalMonthDay ( item [ , options ] )
 */
static bool ToTemporalMonthDay(JSContext* cx, Handle<Value> item,
                               PlainDate* result,
                               MutableHandle<CalendarValue> calendar) {
  auto* obj = ToTemporalMonthDay(cx, item).unwrapOrNull();
  if (!obj) {
    return false;
  }

  *result = ToPlainDate(obj);
  calendar.set(obj->calendar());
  return calendar.wrap(cx);
}

/**
 * Temporal.PlainMonthDay ( isoMonth, isoDay [ , calendarLike [ ,
 * referenceISOYear ] ] )
 */
static bool PlainMonthDayConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainMonthDay")) {
    return false;
  }

  // Step 3.
  double isoMonth;
  if (!ToIntegerWithTruncation(cx, args.get(0), "month", &isoMonth)) {
    return false;
  }

  // Step 4.
  double isoDay;
  if (!ToIntegerWithTruncation(cx, args.get(1), "day", &isoDay)) {
    return false;
  }

  // Step 5.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendarWithISODefault(cx, args.get(2), &calendar)) {
    return false;
  }

  // Steps 2 and 6.
  double isoYear = 1972;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "year", &isoYear)) {
      return false;
    }
  }

  // Step 7.
  auto* monthDay =
      CreateTemporalMonthDay(cx, args, isoYear, isoMonth, isoDay, calendar);
  if (!monthDay) {
    return false;
  }

  args.rval().setObject(*monthDay);
  return true;
}

/**
 * Temporal.PlainMonthDay.from ( item [ , options ] )
 */
static bool PlainMonthDay_from(JSContext* cx, unsigned argc, Value* vp) {
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

    if (auto* monthDay = item->maybeUnwrapIf<PlainMonthDayObject>()) {
      auto date = ToPlainDate(monthDay);

      Rooted<CalendarValue> calendar(cx, monthDay->calendar());
      if (!calendar.wrap(cx)) {
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
      auto* obj = CreateTemporalMonthDay(cx, date, calendar);
      if (!obj) {
        return false;
      }

      args.rval().setObject(*obj);
      return true;
    }
  }

  // Step 3.
  auto obj = ToTemporalMonthDay(cx, args.get(0), options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * get Temporal.PlainMonthDay.prototype.calendarId
 */
static bool PlainMonthDay_calendarId(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();

  // Step 3.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());
  auto* calendarId = ToTemporalCalendarIdentifier(cx, calendar);
  if (!calendarId) {
    return false;
  }

  args.rval().setString(calendarId);
  return true;
}

/**
 * get Temporal.PlainMonthDay.prototype.calendarId
 */
static bool PlainMonthDay_calendarId(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_calendarId>(cx,
                                                                         args);
}

/**
 * get Temporal.PlainMonthDay.prototype.monthCode
 */
static bool PlainMonthDay_monthCode(JSContext* cx, const CallArgs& args) {
  // Step 3.
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 4.
  return CalendarMonthCode(cx, calendar, monthDay, args.rval());
}

/**
 * get Temporal.PlainMonthDay.prototype.monthCode
 */
static bool PlainMonthDay_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.PlainMonthDay.prototype.day
 */
static bool PlainMonthDay_day(JSContext* cx, const CallArgs& args) {
  // Step 3.
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 4.
  return CalendarDay(cx, calendar, monthDay, args.rval());
}

/**
 * get Temporal.PlainMonthDay.prototype.day
 */
static bool PlainMonthDay_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_day>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.with ( temporalMonthDayLike [ , options ] )
 */
static bool PlainMonthDay_with(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<JSObject*> temporalMonthDayLike(
      cx, RequireObjectArg(cx, "temporalMonthDayLike", "with", args.get(0)));
  if (!temporalMonthDayLike) {
    return false;
  }

  // Step 4.
  if (!RejectTemporalLikeObject(cx, temporalMonthDayLike)) {
    return false;
  }

  // Step 5.
  Rooted<PlainObject*> resolvedOptions(cx);
  if (args.hasDefined(1)) {
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "with", args[1]));
    if (!options) {
      return false;
    }
    resolvedOptions = SnapshotOwnProperties(cx, options);
  } else {
    resolvedOptions = NewPlainObjectWithProto(cx, nullptr);
  }
  if (!resolvedOptions) {
    return false;
  }

  // Step 6.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 7.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::Month,
                       CalendarField::MonthCode, CalendarField::Year},
                      &fieldNames)) {
    return false;
  }

  // Step 8.
  Rooted<PlainObject*> fields(cx,
                              PrepareTemporalFields(cx, monthDay, fieldNames));
  if (!fields) {
    return false;
  }

  // Step 9.
  Rooted<PlainObject*> partialMonthDay(
      cx, PreparePartialTemporalFields(cx, temporalMonthDayLike, fieldNames));
  if (!partialMonthDay) {
    return false;
  }

  // Step 10.
  Rooted<JSObject*> mergedFields(
      cx, CalendarMergeFields(cx, calendar, fields, partialMonthDay));
  if (!mergedFields) {
    return false;
  }

  // Step 11.
  fields = PrepareTemporalFields(cx, mergedFields, fieldNames);
  if (!fields) {
    return false;
  }

  // Step 12.
  auto obj = js::temporal::CalendarMonthDayFromFields(cx, calendar, fields,
                                                      resolvedOptions);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.with ( temporalMonthDayLike [ , options ] )
 */
static bool PlainMonthDay_with(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_with>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.equals ( other )
 */
static bool PlainMonthDay_equals(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();
  auto date = ToPlainDate(monthDay);
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 3.
  PlainDate other;
  Rooted<CalendarValue> otherCalendar(cx);
  if (!ToTemporalMonthDay(cx, args.get(0), &other, &otherCalendar)) {
    return false;
  }

  // Steps 4-7.
  bool equals = false;
  if (date.year == other.year && date.month == other.month &&
      date.day == other.day) {
    if (!CalendarEquals(cx, calendar, otherCalendar, &equals)) {
      return false;
    }
  }

  args.rval().setBoolean(equals);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.equals ( other )
 */
static bool PlainMonthDay_equals(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_equals>(cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.toString ( [ options ] )
 */
static bool PlainMonthDay_toString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

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
  JSString* str = TemporalMonthDayToString(cx, monthDay, showCalendar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toString ( [ options ] )
 */
static bool PlainMonthDay_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toString>(cx,
                                                                       args);
}

/**
 * Temporal.PlainMonthDay.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainMonthDay_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  JSString* str = TemporalMonthDayToString(cx, monthDay, CalendarOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool PlainMonthDay_toLocaleString(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toLocaleString>(
      cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.toJSON ( )
 */
static bool PlainMonthDay_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  JSString* str = TemporalMonthDayToString(cx, monthDay, CalendarOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toJSON ( )
 */
static bool PlainMonthDay_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toJSON>(cx, args);
}

/**
 *  Temporal.PlainMonthDay.prototype.valueOf ( )
 */
static bool PlainMonthDay_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainMonthDay", "primitive type");
  return false;
}

/**
 * Temporal.PlainMonthDay.prototype.toPlainDate ( item )
 */
static bool PlainMonthDay_toPlainDate(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<JSObject*> item(
      cx, RequireObjectArg(cx, "item", "toPlainDate", args.get(0)));
  if (!item) {
    return false;
  }

  // Step 4.
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 5.
  JS::RootedVector<PropertyKey> receiverFieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::MonthCode},
                      &receiverFieldNames)) {
    return false;
  }

  // Step 6.
  Rooted<PlainObject*> fields(
      cx, PrepareTemporalFields(cx, monthDay, receiverFieldNames));
  if (!fields) {
    return false;
  }

  // Step 7.
  JS::RootedVector<PropertyKey> inputFieldNames(cx);
  if (!CalendarFields(cx, calendar, {CalendarField::Year}, &inputFieldNames)) {
    return false;
  }

  // Step 8.
  Rooted<PlainObject*> inputFields(
      cx, PrepareTemporalFields(cx, item, inputFieldNames));
  if (!inputFields) {
    return false;
  }

  // Step 9.
  Rooted<JSObject*> mergedFields(
      cx, CalendarMergeFields(cx, calendar, fields, inputFields));
  if (!mergedFields) {
    return false;
  }

  // Step 10.
  JS::RootedVector<PropertyKey> concatenatedFieldNames(cx);
  if (!ConcatTemporalFieldNames(receiverFieldNames, inputFieldNames,
                                concatenatedFieldNames.get())) {
    return false;
  }

  // Step 11.
  mergedFields =
      PrepareTemporalFields(cx, mergedFields, concatenatedFieldNames);
  if (!mergedFields) {
    return false;
  }

  // Step 12.
  Rooted<JSObject*> options(cx, NewPlainObjectWithProto(cx, nullptr));
  if (!options) {
    return false;
  }

  // Step 13.
  Rooted<Value> overflow(cx, StringValue(cx->names().reject));
  if (!DefineDataProperty(cx, options, cx->names().overflow, overflow)) {
    return false;
  }

  // Step 14.
  auto obj =
      js::temporal::CalendarDateFromFields(cx, calendar, mergedFields, options);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.toPlainDate ( item )
 */
static bool PlainMonthDay_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_toPlainDate>(cx,
                                                                          args);
}

/**
 * Temporal.PlainMonthDay.prototype.getISOFields ( )
 */
static bool PlainMonthDay_getISOFields(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          monthDay->calendar().toValue())) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoDay),
                          Int32Value(monthDay->isoDay()))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoMonth),
                          Int32Value(monthDay->isoMonth()))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoYear),
                          Int32Value(monthDay->isoYear()))) {
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
 * Temporal.PlainMonthDay.prototype.getISOFields ( )
 */
static bool PlainMonthDay_getISOFields(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_getISOFields>(
      cx, args);
}

/**
 * Temporal.PlainMonthDay.prototype.getCalendar ( )
 */
static bool PlainMonthDay_getCalendar(JSContext* cx, const CallArgs& args) {
  auto* monthDay = &args.thisv().toObject().as<PlainMonthDayObject>();
  Rooted<CalendarValue> calendar(cx, monthDay->calendar());

  // Step 3.
  auto* obj = ToTemporalCalendarObject(cx, calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainMonthDay.prototype.getCalendar ( )
 */
static bool PlainMonthDay_getCalendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainMonthDay, PlainMonthDay_getCalendar>(cx,
                                                                          args);
}

const JSClass PlainMonthDayObject::class_ = {
    "Temporal.PlainMonthDay",
    JSCLASS_HAS_RESERVED_SLOTS(PlainMonthDayObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainMonthDay),
    JS_NULL_CLASS_OPS,
    &PlainMonthDayObject::classSpec_,
};

const JSClass& PlainMonthDayObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainMonthDay_methods[] = {
    JS_FN("from", PlainMonthDay_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainMonthDay_prototype_methods[] = {
    JS_FN("with", PlainMonthDay_with, 1, 0),
    JS_FN("equals", PlainMonthDay_equals, 1, 0),
    JS_FN("toString", PlainMonthDay_toString, 0, 0),
    JS_FN("toLocaleString", PlainMonthDay_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainMonthDay_toJSON, 0, 0),
    JS_FN("valueOf", PlainMonthDay_valueOf, 0, 0),
    JS_FN("toPlainDate", PlainMonthDay_toPlainDate, 1, 0),
    JS_FN("getISOFields", PlainMonthDay_getISOFields, 0, 0),
    JS_FN("getCalendar", PlainMonthDay_getCalendar, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainMonthDay_prototype_properties[] = {
    JS_PSG("calendarId", PlainMonthDay_calendarId, 0),
    JS_PSG("monthCode", PlainMonthDay_monthCode, 0),
    JS_PSG("day", PlainMonthDay_day, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainMonthDay", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainMonthDayObject::classSpec_ = {
    GenericCreateConstructor<PlainMonthDayConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainMonthDayObject>,
    PlainMonthDay_methods,
    nullptr,
    PlainMonthDay_prototype_methods,
    PlainMonthDay_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
