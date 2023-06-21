/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/ZonedDateTime.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"

#include <cstdlib>
#include <initializer_list>
#include <utility>

#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainMonthDay.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/PlainYearMonth.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalFields.h"
#include "builtin/temporal/TemporalRoundingMode.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/Wrapped.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/ComparisonOperators.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/BigIntType.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsZonedDateTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<ZonedDateTimeObject>();
}

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
static ZonedDateTimeObject* CreateTemporalZonedDateTime(
    JSContext* cx, const CallArgs& args, Handle<BigInt*> epochNanoseconds,
    Handle<JSObject*> timeZone, Handle<JSObject*> calendar) {
  // Step 1.
  MOZ_ASSERT(IsValidEpochNanoseconds(epochNanoseconds));

  // Steps 3-4.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_ZonedDateTime,
                                          &proto)) {
    return nullptr;
  }

  auto* obj = NewObjectWithClassProto<ZonedDateTimeObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  auto instant = ToInstant(epochNanoseconds);
  obj->setFixedSlot(ZonedDateTimeObject::SECONDS_SLOT,
                    NumberValue(instant.seconds));
  obj->setFixedSlot(ZonedDateTimeObject::NANOSECONDS_SLOT,
                    Int32Value(instant.nanoseconds));

  // Step 5.
  obj->setFixedSlot(ZonedDateTimeObject::TIMEZONE_SLOT, ObjectValue(*timeZone));

  // Step 6.
  obj->setFixedSlot(ZonedDateTimeObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 7.
  return obj;
}

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
ZonedDateTimeObject* js::temporal::CreateTemporalZonedDateTime(
    JSContext* cx, const Instant& instant, Handle<JSObject*> timeZone,
    Handle<JSObject*> calendar) {
  // Step 1.
  MOZ_ASSERT(IsValidEpochInstant(instant));

  // Steps 2-3.
  auto* obj = NewBuiltinClassInstance<ZonedDateTimeObject>(cx);
  if (!obj) {
    return nullptr;
  }

  // Step 4.
  obj->setFixedSlot(ZonedDateTimeObject::SECONDS_SLOT,
                    NumberValue(instant.seconds));
  obj->setFixedSlot(ZonedDateTimeObject::NANOSECONDS_SLOT,
                    Int32Value(instant.nanoseconds));

  // Step 5.
  obj->setFixedSlot(ZonedDateTimeObject::TIMEZONE_SLOT, ObjectValue(*timeZone));

  // Step 6.
  obj->setFixedSlot(ZonedDateTimeObject::CALENDAR_SLOT, ObjectValue(*calendar));

  // Step 7.
  return obj;
}

/**
 * TemporalZonedDateTimeToString ( zonedDateTime, precision, showCalendar,
 * showTimeZone, showOffset [ , increment, unit, roundingMode ] )
 */
static JSString* TemporalZonedDateTimeToString(
    JSContext* cx, Handle<ZonedDateTimeObject*> zonedDateTime,
    Precision precision, CalendarOption showCalendar,
    TimeZoneNameOption showTimeZone, ShowOffsetOption showOffset,
    Increment increment = Increment{1},
    TemporalUnit unit = TemporalUnit::Nanosecond,
    TemporalRoundingMode roundingMode = TemporalRoundingMode::Trunc) {
  JSStringBuilder result(cx);

  // Steps 1-3. (Not applicable in our implementation.)

  // Step 4.
  Instant ns;
  if (!RoundTemporalInstant(cx, ToInstant(zonedDateTime), increment, unit,
                            roundingMode, &ns)) {
    return nullptr;
  }

  // Step 5.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 6.
  Rooted<InstantObject*> instant(cx, CreateTemporalInstant(cx, ns));
  if (!instant) {
    return nullptr;
  }

  // Step 7.
  Rooted<CalendarObject*> isoCalendar(cx, GetISO8601Calendar(cx));
  if (!isoCalendar) {
    return nullptr;
  }

  // Step 8.
  PlainDateTime temporalDateTime;
  if (!js::temporal::GetPlainDateTimeFor(cx, timeZone, instant,
                                         &temporalDateTime)) {
    return nullptr;
  }

  // Step 9.
  JSString* dateTimeString = TemporalDateTimeToString(
      cx, temporalDateTime, isoCalendar, precision, CalendarOption::Never);
  if (!dateTimeString) {
    return nullptr;
  }
  if (!result.append(dateTimeString)) {
    return nullptr;
  }

  // Steps 10-11.
  if (showOffset != ShowOffsetOption::Never) {
    // Step 11.a.
    int64_t offsetNs;
    if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNs)) {
      return nullptr;
    }
    MOZ_ASSERT(std::abs(offsetNs) < ToNanoseconds(TemporalUnit::Day));

    // Step 11.b.
    JSString* offsetString = FormatISOTimeZoneOffsetString(cx, offsetNs);
    if (!offsetString) {
      return nullptr;
    }
    if (!result.append(offsetString)) {
      return nullptr;
    }
  }

  // Steps 12-13.
  if (showTimeZone != TimeZoneNameOption::Never) {
    if (!result.append('[')) {
      return nullptr;
    }

    if (showTimeZone == TimeZoneNameOption::Critical) {
      if (!result.append('!')) {
        return nullptr;
      }
    }

    JSString* timeZoneString = TimeZoneToString(cx, timeZone);
    if (!timeZoneString) {
      return nullptr;
    }
    if (!result.append(timeZoneString)) {
      return nullptr;
    }

    if (!result.append(']')) {
      return nullptr;
    }
  }

  // Step 14.
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());
  if (!MaybeFormatCalendarAnnotation(cx, result, calendar, showCalendar)) {
    return nullptr;
  }

  // Step 15.
  return result.finishString();
}

double NanosecondsAndDays::daysNumber() const {
  if (days) {
    return BigInt::numberValue(days);
  }
  return double(daysInt);
}

void NanosecondsAndDays::trace(JSTracer* trc) {
  if (days) {
    TraceRoot(trc, &days, "NanosecondsAndDays::days");
  }
}

/**
 * Temporal.ZonedDateTime ( epochNanoseconds, timeZoneLike [ , calendarLike ] )
 */
static bool ZonedDateTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.ZonedDateTime")) {
    return false;
  }

  // Step 2.
  Rooted<BigInt*> epochNanoseconds(cx, js::ToBigInt(cx, args.get(0)));
  if (!epochNanoseconds) {
    return false;
  }

  // Step 3.
  if (!IsValidEpochNanoseconds(epochNanoseconds)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INSTANT_INVALID);
    return false;
  }

  // Step 4.
  Rooted<JSObject*> timeZone(cx, ToTemporalTimeZone(cx, args.get(1)));
  if (!timeZone) {
    return false;
  }

  // Step 5.
  Rooted<JSObject*> calendar(cx,
                             ToTemporalCalendarWithISODefault(cx, args.get(2)));
  if (!calendar) {
    return false;
  }

  // Step 6.
  auto* obj = CreateTemporalZonedDateTime(cx, args, epochNanoseconds, timeZone,
                                          calendar);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.calendar
 */
static bool ZonedDateTime_calendar(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  args.rval().setObject(*zonedDateTime->calendar());
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.calendar
 */
static bool ZonedDateTime_calendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_calendar>(cx,
                                                                       args);
}

/**
 * get Temporal.ZonedDateTime.prototype.timeZone
 */
static bool ZonedDateTime_timeZone(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  args.rval().setObject(*zonedDateTime->timeZone());
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.timeZone
 */
static bool ZonedDateTime_timeZone(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_timeZone>(cx,
                                                                       args);
}

/**
 * get Temporal.ZonedDateTime.prototype.year
 */
static bool ZonedDateTime_year(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.year
 */
static bool ZonedDateTime_year(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_year>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.month
 */
static bool ZonedDateTime_month(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarMonth(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.month
 */
static bool ZonedDateTime_month(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_month>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.monthCode
 */
static bool ZonedDateTime_monthCode(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarMonthCode(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.monthCode
 */
static bool ZonedDateTime_monthCode(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_monthCode>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.day
 */
static bool ZonedDateTime_day(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDay(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.day
 */
static bool ZonedDateTime_day(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_day>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.hour
 */
static bool ZonedDateTime_hour(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.hour);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.hour
 */
static bool ZonedDateTime_hour(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_hour>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.minute
 */
static bool ZonedDateTime_minute(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.minute);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.minute
 */
static bool ZonedDateTime_minute(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_minute>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.second
 */
static bool ZonedDateTime_second(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.second);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.second
 */
static bool ZonedDateTime_second(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_second>(cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.millisecond
 */
static bool ZonedDateTime_millisecond(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.millisecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.millisecond
 */
static bool ZonedDateTime_millisecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_millisecond>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.microsecond
 */
static bool ZonedDateTime_microsecond(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.microsecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.microsecond
 */
static bool ZonedDateTime_microsecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_microsecond>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.nanosecond
 */
static bool ZonedDateTime_nanosecond(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Steps 3-6.
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return false;
  }

  // Step 7.
  args.rval().setInt32(dateTime.time.nanosecond);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.nanosecond
 */
static bool ZonedDateTime_nanosecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_nanosecond>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochSeconds
 */
static bool ZonedDateTime_epochSeconds(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto instant = ToInstant(zonedDateTime);

  // Steps 4-5.
  args.rval().setNumber(instant.seconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochSeconds
 */
static bool ZonedDateTime_epochSeconds(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochSeconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMilliseconds
 */
static bool ZonedDateTime_epochMilliseconds(JSContext* cx,
                                            const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto instant = ToInstant(zonedDateTime);

  // Steps 4-5.
  args.rval().setNumber(instant.floorToMilliseconds());
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMilliseconds
 */
static bool ZonedDateTime_epochMilliseconds(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochMilliseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMicroseconds
 */
static bool ZonedDateTime_epochMicroseconds(JSContext* cx,
                                            const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto instant = ToInstant(zonedDateTime);

  // Step 4.
  auto* microseconds =
      BigInt::createFromInt64(cx, instant.floorToMicroseconds());
  if (!microseconds) {
    return false;
  }

  // Step 5.
  args.rval().setBigInt(microseconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochMicroseconds
 */
static bool ZonedDateTime_epochMicroseconds(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochMicroseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.epochNanoseconds
 */
static bool ZonedDateTime_epochNanoseconds(JSContext* cx,
                                           const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  auto* nanoseconds = ToEpochNanoseconds(cx, ToInstant(zonedDateTime));
  if (!nanoseconds) {
    return false;
  }

  args.rval().setBigInt(nanoseconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.epochNanoseconds
 */
static bool ZonedDateTime_epochNanoseconds(JSContext* cx, unsigned argc,
                                           Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_epochNanoseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfWeek
 */
static bool ZonedDateTime_dayOfWeek(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDayOfWeek(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfWeek
 */
static bool ZonedDateTime_dayOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_dayOfWeek>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfYear
 */
static bool ZonedDateTime_dayOfYear(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDayOfYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.dayOfYear
 */
static bool ZonedDateTime_dayOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_dayOfYear>(cx,
                                                                        args);
}

/**
 * get Temporal.ZonedDateTime.prototype.weekOfYear
 */
static bool ZonedDateTime_weekOfYear(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarWeekOfYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.weekOfYear
 */
static bool ZonedDateTime_weekOfYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_weekOfYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.yearOfWeek
 */
static bool ZonedDateTime_yearOfWeek(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarYearOfWeek(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.yearOfWeek
 */
static bool ZonedDateTime_yearOfWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_yearOfWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.hoursInDay
 */
static bool ZonedDateTime_hoursInDay(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto epochInstant = ToInstant(zonedDateTime);

  // Step 3.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 5.
  Rooted<CalendarObject*> isoCalendar(cx, GetISO8601Calendar(cx));
  if (!isoCalendar) {
    return false;
  }

  // Steps 4 and 6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, epochInstant, &temporalDateTime)) {
    return false;
  }

  // Steps 7-9.
  const auto& date = temporalDateTime.date;

  // Step 10.
  Rooted<PlainDateTimeObject*> today(
      cx, CreateTemporalDateTime(cx, {date, {}}, isoCalendar));
  if (!today) {
    return false;
  }

  // Step 11.
  PlainDate tomorrowFields =
      BalanceISODate(date.year, date.month, date.day + 1);

  // Step 12.
  Rooted<PlainDateTimeObject*> tomorrow(
      cx, CreateTemporalDateTime(cx, {tomorrowFields, {}}, isoCalendar));
  if (!tomorrow) {
    return false;
  }

  // Step 13.
  Instant todayInstant;
  if (!GetInstantFor(cx, timeZone, today, TemporalDisambiguation::Compatible,
                     &todayInstant)) {
    return false;
  }

  // Step 14.
  Instant tomorrowInstant;
  if (!GetInstantFor(cx, timeZone, tomorrow, TemporalDisambiguation::Compatible,
                     &tomorrowInstant)) {
    return false;
  }

  // Step 15.
  auto diffNs = tomorrowInstant - todayInstant;
  MOZ_ASSERT(IsValidInstantDifference(diffNs));

  // Step 16.
  constexpr int32_t secPerHour = 60 * 60;
  constexpr int64_t nsPerSec = ToNanoseconds(TemporalUnit::Second);
  constexpr double nsPerHour = ToNanoseconds(TemporalUnit::Hour);

  int64_t hours = diffNs.seconds / secPerHour;
  int64_t seconds = diffNs.seconds % secPerHour;
  int64_t nanoseconds = seconds * nsPerSec + diffNs.nanoseconds;

  double result = double(hours) + double(nanoseconds) / nsPerHour;
  args.rval().setNumber(result);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.hoursInDay
 */
static bool ZonedDateTime_hoursInDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_hoursInDay>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInWeek
 */
static bool ZonedDateTime_daysInWeek(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDaysInWeek(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInWeek
 */
static bool ZonedDateTime_daysInWeek(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInWeek>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInMonth
 */
static bool ZonedDateTime_daysInMonth(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDaysInMonth(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInMonth
 */
static bool ZonedDateTime_daysInMonth(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInMonth>(cx,
                                                                          args);
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInYear
 */
static bool ZonedDateTime_daysInYear(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarDaysInYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.daysInYear
 */
static bool ZonedDateTime_daysInYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_daysInYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.monthsInYear
 */
static bool ZonedDateTime_monthsInYear(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarMonthsInYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.monthsInYear
 */
static bool ZonedDateTime_monthsInYear(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_monthsInYear>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.inLeapYear
 */
static bool ZonedDateTime_inLeapYear(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-6.
  auto* dateTime = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!dateTime) {
    return false;
  }
  Rooted<Value> temporalDateTime(cx, ObjectValue(*dateTime));

  // Step 7.
  return CalendarInLeapYear(cx, calendar, temporalDateTime, args.rval());
}

/**
 * get Temporal.ZonedDateTime.prototype.inLeapYear
 */
static bool ZonedDateTime_inLeapYear(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_inLeapYear>(cx,
                                                                         args);
}

/**
 * get Temporal.ZonedDateTime.prototype.offsetNanoseconds
 */
static bool ZonedDateTime_offsetNanoseconds(JSContext* cx,
                                            const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();

  // Step 3.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 4.
  auto instant = ToInstant(zonedDateTime);

  // Step 5.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  args.rval().setNumber(offsetNanoseconds);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.offsetNanoseconds
 */
static bool ZonedDateTime_offsetNanoseconds(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_offsetNanoseconds>(
      cx, args);
}

/**
 * get Temporal.ZonedDateTime.prototype.offset
 */
static bool ZonedDateTime_offset(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto epochInstant = ToInstant(zonedDateTime);

  // Step 3.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 4.
  Rooted<InstantObject*> instant(cx, CreateTemporalInstant(cx, epochInstant));
  if (!instant) {
    return false;
  }

  // Step 5.
  JSString* str = GetOffsetStringFor(cx, timeZone, instant);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * get Temporal.ZonedDateTime.prototype.offset
 */
static bool ZonedDateTime_offset(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_offset>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.withTimeZone ( timeZoneLike )
 */
static bool ZonedDateTime_withTimeZone(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto epochNanoseconds = ToInstant(zonedDateTime);
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Step 3.
  Rooted<JSObject*> timeZone(cx, ToTemporalTimeZone(cx, args.get(0)));
  if (!timeZone) {
    return false;
  }

  // Step 4.
  auto* result =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.withTimeZone ( timeZoneLike )
 */
static bool ZonedDateTime_withTimeZone(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_withTimeZone>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.withCalendar ( calendarLike )
 */
static bool ZonedDateTime_withCalendar(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto epochNanoseconds = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 3.
  Rooted<JSObject*> calendar(cx, ToTemporalCalendar(cx, args.get(0)));
  if (!calendar) {
    return false;
  }

  // Step 4.
  auto* result =
      CreateTemporalZonedDateTime(cx, epochNanoseconds, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.withCalendar ( calendarLike )
 */
static bool ZonedDateTime_withCalendar(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_withCalendar>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toString ( [ options ] )
 */
static bool ZonedDateTime_toString(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTimeObject*> zonedDateTime(
      cx, &args.thisv().toObject().as<ZonedDateTimeObject>());

  SecondsStringPrecision precision = {Precision::Auto(),
                                      TemporalUnit::Nanosecond, Increment{1}};
  auto roundingMode = TemporalRoundingMode::Trunc;
  auto showCalendar = CalendarOption::Auto;
  auto showTimeZone = TimeZoneNameOption::Auto;
  auto showOffset = ShowOffsetOption::Auto;
  if (args.hasDefined(0)) {
    // Step 3.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "toString", args[0]));
    if (!options) {
      return false;
    }

    // Steps 4-5.
    if (!ToCalendarNameOption(cx, options, &showCalendar)) {
      return false;
    }

    // Step 6.
    auto digits = Precision::Auto();
    if (!ToFractionalSecondDigits(cx, options, &digits)) {
      return false;
    }

    // Step 7.
    if (!ToShowOffsetOption(cx, options, &showOffset)) {
      return false;
    }

    // Step 8.
    if (!ToTemporalRoundingMode(cx, options, &roundingMode)) {
      return false;
    }

    // Step 9.
    auto smallestUnit = TemporalUnit::Auto;
    if (!GetTemporalUnit(cx, options, TemporalUnitKey::SmallestUnit,
                         TemporalUnitGroup::Time, &smallestUnit)) {
      return false;
    }

    // Step 10.
    if (smallestUnit == TemporalUnit::Hour) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INVALID_UNIT_OPTION, "hour",
                                "smallestUnit");
      return false;
    }

    // Step 11.
    if (!ToTimeZoneNameOption(cx, options, &showTimeZone)) {
      return false;
    }

    // Step 12.
    precision = ToSecondsStringPrecision(smallestUnit, digits);
  }

  // Step 13.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, precision.precision, showCalendar, showTimeZone,
      showOffset, precision.increment, precision.unit, roundingMode);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toString ( [ options ] )
 */
static bool ZonedDateTime_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toString>(cx,
                                                                       args);
}

/**
 * Temporal.ZonedDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool ZonedDateTime_toLocaleString(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTimeObject*> zonedDateTime(
      cx, &args.thisv().toObject().as<ZonedDateTimeObject>());

  // Step 3.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, Precision::Auto(), CalendarOption::Auto,
      TimeZoneNameOption::Auto, ShowOffsetOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toLocaleString ( [ locales [ , options ] ] )
 */
static bool ZonedDateTime_toLocaleString(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toLocaleString>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toJSON ( )
 */
static bool ZonedDateTime_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<ZonedDateTimeObject*> zonedDateTime(
      cx, &args.thisv().toObject().as<ZonedDateTimeObject>());

  // Step 3.
  JSString* str = TemporalZonedDateTimeToString(
      cx, zonedDateTime, Precision::Auto(), CalendarOption::Auto,
      TimeZoneNameOption::Auto, ShowOffsetOption::Auto);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toJSON ( )
 */
static bool ZonedDateTime_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toJSON>(cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.valueOf ( )
 */
static bool ZonedDateTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "ZonedDateTime", "primitive type");
  return false;
}

/**
 * Temporal.ZonedDateTime.prototype.startOfDay ( )
 */
static bool ZonedDateTime_startOfDay(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);

  // Step 3.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 4.
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 5-6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &temporalDateTime)) {
    return false;
  }

  // Step 7.
  Rooted<PlainDateTimeObject*> startDateTime(
      cx, CreateTemporalDateTime(cx, {temporalDateTime.date, {}}, calendar));
  if (!startDateTime) {
    return false;
  }

  // Step 8.
  Instant startInstant;
  if (!GetInstantFor(cx, timeZone, startDateTime,
                     TemporalDisambiguation::Compatible, &startInstant)) {
    return false;
  }

  // Step 9.
  auto* result =
      CreateTemporalZonedDateTime(cx, startInstant, timeZone, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.startOfDay ( )
 */
static bool ZonedDateTime_startOfDay(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_startOfDay>(cx,
                                                                         args);
}

/**
 * Temporal.ZonedDateTime.prototype.toInstant ( )
 */
static bool ZonedDateTime_toInstant(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);

  // Step 3.
  auto* result = CreateTemporalInstant(cx, instant);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toInstant ( )
 */
static bool ZonedDateTime_toInstant(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toInstant>(cx,
                                                                        args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDate ( )
 */
static bool ZonedDateTime_toPlainDate(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 5.
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-4 and 6.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &temporalDateTime)) {
    return false;
  }

  // Step 7.
  auto* result = CreateTemporalDate(cx, temporalDateTime.date, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDate ( )
 */
static bool ZonedDateTime_toPlainDate(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainDate>(cx,
                                                                          args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainTime ( )
 */
static bool ZonedDateTime_toPlainTime(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-5.
  PlainDateTime temporalDateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &temporalDateTime)) {
    return false;
  }

  // Step 6.
  auto* result = CreateTemporalTime(cx, temporalDateTime.time);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainTime ( )
 */
static bool ZonedDateTime_toPlainTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainTime>(cx,
                                                                          args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDateTime ( )
 */
static bool ZonedDateTime_toPlainDateTime(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-5.
  auto* result = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainDateTime ( )
 */
static bool ZonedDateTime_toPlainDateTime(JSContext* cx, unsigned argc,
                                          Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainDateTime>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainYearMonth ( )
 */
static bool ZonedDateTime_toPlainYearMonth(JSContext* cx,
                                           const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 5.
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-4 and 6.
  Rooted<PlainDateTimeObject*> temporalDateTime(
      cx, GetPlainDateTimeFor(cx, timeZone, instant, calendar));
  if (!temporalDateTime) {
    return false;
  }

  // Step 7.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::MonthCode, CalendarField::Year},
                      &fieldNames)) {
    return false;
  }

  // Step 8.
  Rooted<PlainObject*> fields(
      cx, PrepareTemporalFields(cx, temporalDateTime, fieldNames));
  if (!fields) {
    return false;
  }

  // Steps 9-10.
  auto result = CalendarYearMonthFromFields(cx, calendar, fields);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainYearMonth ( )
 */
static bool ZonedDateTime_toPlainYearMonth(JSContext* cx, unsigned argc,
                                           Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainYearMonth>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainMonthDay ( )
 */
static bool ZonedDateTime_toPlainMonthDay(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto instant = ToInstant(zonedDateTime);
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 5.
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Steps 3-4 and 6.
  Rooted<PlainDateTimeObject*> temporalDateTime(
      cx, GetPlainDateTimeFor(cx, timeZone, instant, calendar));
  if (!temporalDateTime) {
    return false;
  }

  // Step 7.
  JS::RootedVector<PropertyKey> fieldNames(cx);
  if (!CalendarFields(cx, calendar,
                      {CalendarField::Day, CalendarField::MonthCode},
                      &fieldNames)) {
    return false;
  }

  // Step 8.
  Rooted<PlainObject*> fields(
      cx, PrepareTemporalFields(cx, temporalDateTime, fieldNames));
  if (!fields) {
    return false;
  }

  // Steps 9-10.
  auto result = CalendarMonthDayFromFields(cx, calendar, fields);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.toPlainMonthDay ( )
 */
static bool ZonedDateTime_toPlainMonthDay(JSContext* cx, unsigned argc,
                                          Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_toPlainMonthDay>(
      cx, args);
}

/**
 * Temporal.ZonedDateTime.prototype.getISOFields ( )
 */
static bool ZonedDateTime_getISOFields(JSContext* cx, const CallArgs& args) {
  auto* zonedDateTime = &args.thisv().toObject().as<ZonedDateTimeObject>();
  auto epochInstant = ToInstant(zonedDateTime);

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());

  // Step 6. (Reordered)
  Rooted<JSObject*> calendar(cx, zonedDateTime->calendar());

  // Step 5.
  Rooted<InstantObject*> instant(cx, CreateTemporalInstant(cx, epochInstant));
  if (!instant) {
    return false;
  }

  // Step 7.
  PlainDateTime temporalDateTime;
  if (!js::temporal::GetPlainDateTimeFor(cx, timeZone, instant,
                                         &temporalDateTime)) {
    return false;
  }

  // Step 8.
  Rooted<JSString*> offset(cx, GetOffsetStringFor(cx, timeZone, instant));
  if (!offset) {
    return false;
  }

  // Step 9.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 10.
  if (!fields.emplaceBack(NameToId(cx->names().isoDay),
                          Int32Value(temporalDateTime.date.day))) {
    return false;
  }

  // Step 11.
  if (!fields.emplaceBack(NameToId(cx->names().isoHour),
                          Int32Value(temporalDateTime.time.hour))) {
    return false;
  }

  // Step 12.
  if (!fields.emplaceBack(NameToId(cx->names().isoMicrosecond),
                          Int32Value(temporalDateTime.time.microsecond))) {
    return false;
  }

  // Step 13.
  if (!fields.emplaceBack(NameToId(cx->names().isoMillisecond),
                          Int32Value(temporalDateTime.time.millisecond))) {
    return false;
  }

  // Step 14.
  if (!fields.emplaceBack(NameToId(cx->names().isoMinute),
                          Int32Value(temporalDateTime.time.minute))) {
    return false;
  }

  // Step 15.
  if (!fields.emplaceBack(NameToId(cx->names().isoMonth),
                          Int32Value(temporalDateTime.date.month))) {
    return false;
  }

  // Step 16.
  if (!fields.emplaceBack(NameToId(cx->names().isoNanosecond),
                          Int32Value(temporalDateTime.time.nanosecond))) {
    return false;
  }

  // Step 17.
  if (!fields.emplaceBack(NameToId(cx->names().isoSecond),
                          Int32Value(temporalDateTime.time.second))) {
    return false;
  }

  // Step 18.
  if (!fields.emplaceBack(NameToId(cx->names().isoYear),
                          Int32Value(temporalDateTime.date.year))) {
    return false;
  }

  // Step 19.
  if (!fields.emplaceBack(NameToId(cx->names().offset), StringValue(offset))) {
    return false;
  }

  // Step 20.
  if (!fields.emplaceBack(NameToId(cx->names().timeZone),
                          ObjectValue(*timeZone))) {
    return false;
  }

  // Step 21.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.ZonedDateTime.prototype.getISOFields ( )
 */
static bool ZonedDateTime_getISOFields(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsZonedDateTime, ZonedDateTime_getISOFields>(
      cx, args);
}

const JSClass ZonedDateTimeObject::class_ = {
    "Temporal.ZonedDateTime",
    JSCLASS_HAS_RESERVED_SLOTS(ZonedDateTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_ZonedDateTime),
    JS_NULL_CLASS_OPS,
    &ZonedDateTimeObject::classSpec_,
};

const JSClass& ZonedDateTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec ZonedDateTime_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec ZonedDateTime_prototype_methods[] = {
    JS_FN("withTimeZone", ZonedDateTime_withTimeZone, 1, 0),
    JS_FN("withCalendar", ZonedDateTime_withCalendar, 1, 0),
    JS_FN("toString", ZonedDateTime_toString, 0, 0),
    JS_FN("toLocaleString", ZonedDateTime_toLocaleString, 0, 0),
    JS_FN("toJSON", ZonedDateTime_toJSON, 0, 0),
    JS_FN("valueOf", ZonedDateTime_valueOf, 0, 0),
    JS_FN("startOfDay", ZonedDateTime_startOfDay, 0, 0),
    JS_FN("toInstant", ZonedDateTime_toInstant, 0, 0),
    JS_FN("toPlainDate", ZonedDateTime_toPlainDate, 0, 0),
    JS_FN("toPlainTime", ZonedDateTime_toPlainTime, 0, 0),
    JS_FN("toPlainDateTime", ZonedDateTime_toPlainDateTime, 0, 0),
    JS_FN("toPlainYearMonth", ZonedDateTime_toPlainYearMonth, 0, 0),
    JS_FN("toPlainMonthDay", ZonedDateTime_toPlainMonthDay, 0, 0),
    JS_FN("getISOFields", ZonedDateTime_getISOFields, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec ZonedDateTime_prototype_properties[] = {
    JS_PSG("calendar", ZonedDateTime_calendar, 0),
    JS_PSG("timeZone", ZonedDateTime_timeZone, 0),
    JS_PSG("year", ZonedDateTime_year, 0),
    JS_PSG("month", ZonedDateTime_month, 0),
    JS_PSG("monthCode", ZonedDateTime_monthCode, 0),
    JS_PSG("day", ZonedDateTime_day, 0),
    JS_PSG("hour", ZonedDateTime_hour, 0),
    JS_PSG("minute", ZonedDateTime_minute, 0),
    JS_PSG("second", ZonedDateTime_second, 0),
    JS_PSG("millisecond", ZonedDateTime_millisecond, 0),
    JS_PSG("microsecond", ZonedDateTime_microsecond, 0),
    JS_PSG("nanosecond", ZonedDateTime_nanosecond, 0),
    JS_PSG("epochSeconds", ZonedDateTime_epochSeconds, 0),
    JS_PSG("epochMilliseconds", ZonedDateTime_epochMilliseconds, 0),
    JS_PSG("epochMicroseconds", ZonedDateTime_epochMicroseconds, 0),
    JS_PSG("epochNanoseconds", ZonedDateTime_epochNanoseconds, 0),
    JS_PSG("dayOfWeek", ZonedDateTime_dayOfWeek, 0),
    JS_PSG("dayOfYear", ZonedDateTime_dayOfYear, 0),
    JS_PSG("weekOfYear", ZonedDateTime_weekOfYear, 0),
    JS_PSG("yearOfWeek", ZonedDateTime_yearOfWeek, 0),
    JS_PSG("hoursInDay", ZonedDateTime_hoursInDay, 0),
    JS_PSG("daysInWeek", ZonedDateTime_daysInWeek, 0),
    JS_PSG("daysInMonth", ZonedDateTime_daysInMonth, 0),
    JS_PSG("daysInYear", ZonedDateTime_daysInYear, 0),
    JS_PSG("monthsInYear", ZonedDateTime_monthsInYear, 0),
    JS_PSG("inLeapYear", ZonedDateTime_inLeapYear, 0),
    JS_PSG("offsetNanoseconds", ZonedDateTime_offsetNanoseconds, 0),
    JS_PSG("offset", ZonedDateTime_offset, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.ZonedDateTime", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec ZonedDateTimeObject::classSpec_ = {
    GenericCreateConstructor<ZonedDateTimeConstructor, 2,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<ZonedDateTimeObject>,
    ZonedDateTime_methods,
    nullptr,
    ZonedDateTime_prototype_methods,
    ZonedDateTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
