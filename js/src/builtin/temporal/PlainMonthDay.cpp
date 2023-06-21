/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainMonthDay.h"

#include "mozilla/Assertions.h"

#include <cstdlib>
#include <initializer_list>
#include <stddef.h>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalTypes.h"
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
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"
#include "vm/Realm-inl.h"

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
    double isoDay, Handle<JSObject*> calendar) {
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
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, ObjectValue(*calendar));

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
    JSContext* cx, const PlainDate& date, Handle<JSObject*> calendar) {
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
  obj->setFixedSlot(PlainMonthDayObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 8.
  obj->setFixedSlot(PlainMonthDayObject::ISO_YEAR_SLOT, Int32Value(isoYear));

  // Step 9.
  return obj;
}

/**
 * TemporalMonthDayToString ( monthDay, showCalendar )
 */
static JSString* TemporalMonthDayToString(JSContext* cx,
                                          Handle<PlainMonthDayObject*> monthDay,
                                          CalendarOption showCalendar) {
  // Steps 1-2. (Not applicable in our implementation.)

  // Note: This doesn't reserve too much space, because the string builder
  // already internally reserves space for 64 characters.
  constexpr size_t datePart = 1 + 6 + 1 + 2 + 1 + 2;  // 13
  constexpr size_t calendarPart = 30;

  JSStringBuilder result(cx);

  if (!result.reserve(datePart + calendarPart)) {
    return nullptr;
  }

  // Step 6. (Reordered)
  Rooted<JSObject*> calendar(cx, monthDay->calendar());
  JSString* str = CalendarToString(cx, calendar);
  if (!str) {
    return nullptr;
  }

  Rooted<JSLinearString*> calendarID(cx, str->ensureLinear(cx));
  if (!calendarID) {
    return nullptr;
  }

  // Step 7. (Reordered)
  if (showCalendar == CalendarOption::Always ||
      showCalendar == CalendarOption::Critical ||
      !StringEqualsLiteral(calendarID, "iso8601")) {
    int32_t year = monthDay->isoYear();
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
    result.infallibleAppend('-');
  }

  // Step 3.
  int32_t month = monthDay->isoMonth();
  result.infallibleAppend(char('0' + (month / 10)));
  result.infallibleAppend(char('0' + (month % 10)));

  // Steps 4-5.
  int32_t day = monthDay->isoDay();
  result.infallibleAppend('-');
  result.infallibleAppend(char('0' + (day / 10)));
  result.infallibleAppend(char('0' + (day % 10)));

  // Steps 8-9.
  if (!FormatCalendarAnnotation(cx, result, calendarID, showCalendar)) {
    return nullptr;
  }

  // Step 10.
  return result.finishString();
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
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(2)));
  if (!calendar) {
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
 * Temporal.PlainMonthDay.prototype.getISOFields ( )
 */
static bool PlainMonthDay_getISOFields(JSContext* cx, const CallArgs& args) {
  Rooted<PlainMonthDayObject*> monthDay(
      cx, &args.thisv().toObject().as<PlainMonthDayObject>());

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*monthDay->calendar()))) {
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

const JSClass PlainMonthDayObject::class_ = {
    "Temporal.PlainMonthDay",
    JSCLASS_HAS_RESERVED_SLOTS(PlainMonthDayObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainMonthDay),
    JS_NULL_CLASS_OPS,
    &PlainMonthDayObject::classSpec_,
};

const JSClass& PlainMonthDayObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainMonthDay_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainMonthDay_prototype_methods[] = {
    JS_FN("toString", PlainMonthDay_toString, 0, 0),
    JS_FN("toLocaleString", PlainMonthDay_toLocaleString, 0, 0),
    JS_FN("toJSON", PlainMonthDay_toJSON, 0, 0),
    JS_FN("valueOf", PlainMonthDay_valueOf, 0, 0),
    JS_FN("getISOFields", PlainMonthDay_getISOFields, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainMonthDay_prototype_properties[] = {
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
