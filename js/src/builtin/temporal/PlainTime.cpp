/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainTime.h"

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stddef.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
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
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
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

static inline bool IsPlainTime(Handle<Value> v) {
  return v.isObject() && v.toObject().is<PlainTimeObject>();
}

#ifdef DEBUG
/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool IsValidTime(T hour, T minute, T second, T millisecond,
                        T microsecond, T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 2.
  if (hour < 0 || hour > 23) {
    return false;
  }

  // Step 3.
  if (minute < 0 || minute > 59) {
    return false;
  }

  // Step 4.
  if (second < 0 || second > 59) {
    return false;
  }

  // Step 5.
  if (millisecond < 0 || millisecond > 999) {
    return false;
  }

  // Step 6.
  if (microsecond < 0 || microsecond > 999) {
    return false;
  }

  // Step 7.
  if (nanosecond < 0 || nanosecond > 999) {
    return false;
  }

  // Step 8.
  return true;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::IsValidTime(const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;
  return ::IsValidTime(hour, minute, second, millisecond, microsecond,
                       nanosecond);
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::IsValidTime(double hour, double minute, double second,
                               double millisecond, double microsecond,
                               double nanosecond) {
  return ::IsValidTime(hour, minute, second, millisecond, microsecond,
                       nanosecond);
}
#endif

static void ReportInvalidTimeValue(JSContext* cx, const char* name, int32_t min,
                                   int32_t max, double num) {
  Int32ToCStringBuf minCbuf;
  const char* minStr = Int32ToCString(&minCbuf, min);

  Int32ToCStringBuf maxCbuf;
  const char* maxStr = Int32ToCString(&maxCbuf, max);

  ToCStringBuf numCbuf;
  const char* numStr = NumberToCString(&numCbuf, num);

  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_TEMPORAL_PLAIN_TIME_INVALID_VALUE, name,
                            minStr, maxStr, numStr);
}

template <typename T>
static inline bool ThrowIfInvalidTimeValue(JSContext* cx, const char* name,
                                           int32_t min, int32_t max, T num) {
  if (min <= num && num <= max) {
    return true;
  }
  ReportInvalidTimeValue(cx, name, min, max, num);
  return false;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ThrowIfInvalidTime(JSContext* cx, T hour, T minute, T second,
                               T millisecond, T microsecond, T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 2.
  if (!ThrowIfInvalidTimeValue(cx, "hour", 0, 23, hour)) {
    return false;
  }

  // Step 3.
  if (!ThrowIfInvalidTimeValue(cx, "minute", 0, 59, minute)) {
    return false;
  }

  // Step 4.
  if (!ThrowIfInvalidTimeValue(cx, "second", 0, 59, second)) {
    return false;
  }

  // Step 5.
  if (!ThrowIfInvalidTimeValue(cx, "millisecond", 0, 999, millisecond)) {
    return false;
  }

  // Step 6.
  if (!ThrowIfInvalidTimeValue(cx, "microsecond", 0, 999, microsecond)) {
    return false;
  }

  // Step 7.
  if (!ThrowIfInvalidTimeValue(cx, "nanosecond", 0, 999, nanosecond)) {
    return false;
  }

  // Step 8.
  return true;
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::ThrowIfInvalidTime(JSContext* cx, const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;
  return ::ThrowIfInvalidTime(cx, hour, minute, second, millisecond,
                              microsecond, nanosecond);
}

/**
 * IsValidTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
bool js::temporal::ThrowIfInvalidTime(JSContext* cx, double hour, double minute,
                                      double second, double millisecond,
                                      double microsecond, double nanosecond) {
  return ::ThrowIfInvalidTime(cx, hour, minute, second, millisecond,
                              microsecond, nanosecond);
}

/**
 * ConstrainTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
static PlainTime ConstrainTime(double hour, double minute, double second,
                               double millisecond, double microsecond,
                               double nanosecond) {
  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Steps 2-8.
  return {
      int32_t(std::clamp(hour, 0.0, 23.0)),
      int32_t(std::clamp(minute, 0.0, 59.0)),
      int32_t(std::clamp(second, 0.0, 59.0)),
      int32_t(std::clamp(millisecond, 0.0, 999.0)),
      int32_t(std::clamp(microsecond, 0.0, 999.0)),
      int32_t(std::clamp(nanosecond, 0.0, 999.0)),
  };
}

/**
 * RegulateTime ( hour, minute, second, millisecond, microsecond, nanosecond,
 * overflow )
 */
bool js::temporal::RegulateTime(JSContext* cx, const TimeRecord& time,
                                TemporalOverflow overflow, PlainTime* result) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;

  // Step 1.
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 2. (Not applicable in our implementation.)

  // Step 3.
  if (overflow == TemporalOverflow::Constrain) {
    *result = ConstrainTime(hour, minute, second, millisecond, microsecond,
                            nanosecond);
    return true;
  }

  // Step 4.a.
  MOZ_ASSERT(overflow == TemporalOverflow::Reject);

  // Step 4.b.
  if (!ThrowIfInvalidTime(cx, hour, minute, second, millisecond, microsecond,
                          nanosecond)) {
    return false;
  }

  // Step 4.c.
  *result = {
      int32_t(hour),        int32_t(minute),      int32_t(second),
      int32_t(millisecond), int32_t(microsecond), int32_t(nanosecond),
  };
  return true;
}

/**
 * CreateTemporalTime ( hour, minute, second, millisecond, microsecond,
 * nanosecond [ , newTarget ] )
 */
static PlainTimeObject* CreateTemporalTime(JSContext* cx, const CallArgs& args,
                                           double hour, double minute,
                                           double second, double millisecond,
                                           double microsecond,
                                           double nanosecond) {
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  // Step 1.
  if (!ThrowIfInvalidTime(cx, hour, minute, second, millisecond, microsecond,
                          nanosecond)) {
    return nullptr;
  }

  // Steps 2-3.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_PlainTime,
                                          &proto)) {
    return nullptr;
  }

  auto* object = NewObjectWithClassProto<PlainTimeObject>(cx, proto);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  object->setFixedSlot(PlainTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 5.
  object->setFixedSlot(PlainTimeObject::ISO_MINUTE_SLOT, Int32Value(minute));

  // Step 6.
  object->setFixedSlot(PlainTimeObject::ISO_SECOND_SLOT, Int32Value(second));

  // Step 7.
  object->setFixedSlot(PlainTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 8.
  object->setFixedSlot(PlainTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 9.
  object->setFixedSlot(PlainTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 10.
  object->setFixedSlot(PlainTimeObject::CALENDAR_SLOT, NullValue());

  // Step 11.
  return object;
}

/**
 * CreateTemporalTime ( hour, minute, second, millisecond, microsecond,
 * nanosecond [ , newTarget ] )
 */
PlainTimeObject* js::temporal::CreateTemporalTime(JSContext* cx,
                                                  const PlainTime& time) {
  auto& [hour, minute, second, millisecond, microsecond, nanosecond] = time;

  // Step 1.
  if (!ThrowIfInvalidTime(cx, time)) {
    return nullptr;
  }

  // Steps 2-3.
  auto* object = NewBuiltinClassInstance<PlainTimeObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 4.
  object->setFixedSlot(PlainTimeObject::ISO_HOUR_SLOT, Int32Value(hour));

  // Step 5.
  object->setFixedSlot(PlainTimeObject::ISO_MINUTE_SLOT, Int32Value(minute));

  // Step 6.
  object->setFixedSlot(PlainTimeObject::ISO_SECOND_SLOT, Int32Value(second));

  // Step 7.
  object->setFixedSlot(PlainTimeObject::ISO_MILLISECOND_SLOT,
                       Int32Value(millisecond));

  // Step 8.
  object->setFixedSlot(PlainTimeObject::ISO_MICROSECOND_SLOT,
                       Int32Value(microsecond));

  // Step 9.
  object->setFixedSlot(PlainTimeObject::ISO_NANOSECOND_SLOT,
                       Int32Value(nanosecond));

  // Step 10.
  object->setFixedSlot(PlainTimeObject::CALENDAR_SLOT, NullValue());

  // Step 11.
  return object;
}

/**
 * BalanceTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
template <typename IntT>
static BalancedTime BalanceTime(IntT hour, IntT minute, IntT second,
                                IntT millisecond, IntT microsecond,
                                IntT nanosecond) {
  // Step 1. (Not applicable in our implementation.)

  // Combined floor'ed division and modulo operation.
  auto divmod = [](IntT dividend, int32_t divisor, int32_t* remainder) {
    MOZ_ASSERT(divisor > 0);

    IntT quotient = dividend / divisor;
    *remainder = dividend % divisor;

    // The remainder is negative, add the divisor and simulate a floor instead
    // of trunc division.
    if (*remainder < 0) {
      *remainder += divisor;
      quotient -= 1;
    }

    return quotient;
  };

  PlainTime time = {};

  // Steps 2-3.
  microsecond += divmod(nanosecond, 1000, &time.nanosecond);

  // Steps 4-5.
  millisecond += divmod(microsecond, 1000, &time.microsecond);

  // Steps 6-7.
  second += divmod(millisecond, 1000, &time.millisecond);

  // Steps 8-9.
  minute += divmod(second, 60, &time.second);

  // Steps 10-11.
  hour += divmod(minute, 60, &time.minute);

  // Steps 12-13.
  int32_t days = divmod(hour, 24, &time.hour);

  // Step 14.
  MOZ_ASSERT(IsValidTime(time));
  return {days, time};
}

/**
 * BalanceTime ( hour, minute, second, millisecond, microsecond, nanosecond )
 */
BalancedTime js::temporal::BalanceTime(const PlainTime& time,
                                       int64_t nanoseconds) {
  MOZ_ASSERT(IsValidTime(time));
  MOZ_ASSERT(std::abs(nanoseconds) <= 2 * ToNanoseconds(TemporalUnit::Day));

  return ::BalanceTime<int64_t>(time.hour, time.minute, time.second,
                                time.millisecond, time.microsecond,
                                time.nanosecond + nanoseconds);
}

/**
 * ToTemporalTime ( item [ , overflow ] )
 */
static Wrapped<PlainTimeObject*> ToTemporalTime(JSContext* cx,
                                                Handle<Value> item,
                                                TemporalOverflow overflow) {
  // Steps 1-2. (Not applicable in our implementation.)

  // Steps 3-4.
  PlainTime result;
  if (item.isObject()) {
    // Step 3.
    Rooted<JSObject*> itemObj(cx, &item.toObject());

    // Step 3.a.
    if (itemObj->canUnwrapAs<PlainTimeObject>()) {
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

      // Steps 3.b.i-ii.
      PlainDateTime dateTime;
      if (!GetPlainDateTimeFor(cx, timeZone, epochInstant, &dateTime)) {
        return nullptr;
      }

      // Step 3.b.iii.
      return CreateTemporalTime(cx, dateTime.time);
    }

    // Step 3.c.
    if (auto* dateTime = itemObj->maybeUnwrapIf<PlainDateTimeObject>()) {
      return CreateTemporalTime(cx, ToPlainTime(dateTime));
    }

    // Step 3.d.
    Rooted<JSObject*> calendar(cx,
                               GetTemporalCalendarWithISODefault(cx, itemObj));
    if (!calendar) {
      return nullptr;
    }

    // Step 3.e.
    JSString* calendarId = CalendarToString(cx, calendar);
    if (!calendarId) {
      return nullptr;
    }

    JSLinearString* linear = calendarId->ensureLinear(cx);
    if (!linear) {
      return nullptr;
    }

    if (!StringEqualsLiteral(linear, "iso8601")) {
      if (auto chars = QuoteString(cx, linear, '"')) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_PLAIN_TIME_CALENDAR_NOT_ISO8601,
                                 chars.get());
      }
      return nullptr;
    }

    // Step 3.f.
    TimeRecord timeResult;
    if (!ToTemporalTimeRecord(cx, itemObj, &timeResult)) {
      return nullptr;
    }

    // Step 3.g.
    if (!RegulateTime(cx, timeResult, overflow, &result)) {
      return nullptr;
    }
  } else {
    // Step 4.

    // Step 4.a.
    Rooted<JSString*> string(cx, JS::ToString(cx, item));
    if (!string) {
      return nullptr;
    }

    // Step 4.b.
    Rooted<JSString*> calendar(cx);
    if (!ParseTemporalTimeString(cx, string, &result, &calendar)) {
      return nullptr;
    }

    // Step 4.c.
    MOZ_ASSERT(IsValidTime(result));

    // Step 4.d.
    if (calendar) {
      JSLinearString* linear = calendar->ensureLinear(cx);
      if (!linear) {
        return nullptr;
      }

      if (!StringEqualsAscii(linear, "iso8601")) {
        if (auto chars = QuoteString(cx, linear)) {
          JS_ReportErrorNumberUTF8(
              cx, GetErrorMessage, nullptr,
              JSMSG_TEMPORAL_PLAIN_TIME_CALENDAR_NOT_ISO8601, chars.get());
        }
        return nullptr;
      }
    }
  }

  // Step 5.
  return CreateTemporalTime(cx, result);
}

/**
 * ToTemporalTime ( item [ , overflow ] )
 */
bool js::temporal::ToTemporalTime(JSContext* cx, Handle<Value> item,
                                  PlainTime* result) {
  auto obj = ::ToTemporalTime(cx, item, TemporalOverflow::Constrain);
  if (!obj) {
    return false;
  }

  *result = ToPlainTime(&obj.unwrap());
  return true;
}

/**
 * ToTemporalTimeRecord ( temporalTimeLike [ , completeness ] )
 */
static bool ToTemporalTimeRecord(JSContext* cx,
                                 Handle<JSObject*> temporalTimeLike,
                                 TimeRecord* result) {
  // Steps 1 and 3-4. (Not applicable in our implementation.)

  // Step 2. (Inlined call to PrepareTemporalFields.)
  // PrepareTemporalFields, step 1. (Not applicable in our implementation.)

  // PrepareTemporalFields, step 2.
  bool any = false;

  // PrepareTemporalFields, steps 3-4. (Loop unrolled)
  Rooted<Value> value(cx);
  auto getTimeProperty = [&](Handle<PropertyName*> property, const char* name,
                             double* num) {
    // Step 4.a.
    if (!GetProperty(cx, temporalTimeLike, temporalTimeLike, property,
                     &value)) {
      return false;
    }

    // Step 4.b.
    if (!value.isUndefined()) {
      // Step 4.b.i.
      any = true;

      // Step 4.b.ii.2.
      if (!ToIntegerWithTruncation(cx, value, name, num)) {
        return false;
      }
    }
    return true;
  };

  if (!getTimeProperty(cx->names().hour, "hour", &result->hour)) {
    return false;
  }
  if (!getTimeProperty(cx->names().microsecond, "microsecond",
                       &result->microsecond)) {
    return false;
  }
  if (!getTimeProperty(cx->names().millisecond, "millisecond",
                       &result->millisecond)) {
    return false;
  }
  if (!getTimeProperty(cx->names().minute, "minute", &result->minute)) {
    return false;
  }
  if (!getTimeProperty(cx->names().nanosecond, "nanosecond",
                       &result->nanosecond)) {
    return false;
  }
  if (!getTimeProperty(cx->names().second, "second", &result->second)) {
    return false;
  }

  // PrepareTemporalFields, step 5.
  if (!any) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_PLAIN_TIME_MISSING_UNIT);
    return false;
  }

  // Steps 5-16. (Performed implicitly in our implementation.)

  // Step 17.
  return true;
}

/**
 * ToTemporalTimeRecord ( temporalTimeLike )
 */
bool js::temporal::ToTemporalTimeRecord(JSContext* cx,
                                        Handle<JSObject*> temporalTimeLike,
                                        TimeRecord* result) {
  // Step 3.a. (Set all fields to zero.)
  *result = {};

  // Steps 1-2 and 4-17.
  return ::ToTemporalTimeRecord(cx, temporalTimeLike, result);
}

JSObject* js::temporal::PlainTimeObject::createCalendar(
    JSContext* cx, Handle<PlainTimeObject*> obj) {
  auto* calendar = GetISO8601Calendar(cx);
  if (!calendar) {
    return nullptr;
  }

  obj->setCalendar(calendar);
  return calendar;
}

/**
 * Temporal.PlainTime ( [ hour [ , minute [ , second [ , millisecond [ ,
 * microsecond [ , nanosecond ] ] ] ] ] ] )
 */
static bool PlainTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.PlainTime")) {
    return false;
  }

  // Step 2.
  double hour = 0;
  if (args.hasDefined(0)) {
    if (!ToIntegerWithTruncation(cx, args[0], "hour", &hour)) {
      return false;
    }
  }

  // Step 3.
  double minute = 0;
  if (args.hasDefined(1)) {
    if (!ToIntegerWithTruncation(cx, args[1], "minute", &minute)) {
      return false;
    }
  }

  // Step 4.
  double second = 0;
  if (args.hasDefined(2)) {
    if (!ToIntegerWithTruncation(cx, args[2], "second", &second)) {
      return false;
    }
  }

  // Step 5.
  double millisecond = 0;
  if (args.hasDefined(3)) {
    if (!ToIntegerWithTruncation(cx, args[3], "millisecond", &millisecond)) {
      return false;
    }
  }

  // Step 6.
  double microsecond = 0;
  if (args.hasDefined(4)) {
    if (!ToIntegerWithTruncation(cx, args[4], "microsecond", &microsecond)) {
      return false;
    }
  }

  // Step 7.
  double nanosecond = 0;
  if (args.hasDefined(5)) {
    if (!ToIntegerWithTruncation(cx, args[5], "nanosecond", &nanosecond)) {
      return false;
    }
  }

  // Step 8.
  auto* temporalTime = CreateTemporalTime(cx, args, hour, minute, second,
                                          millisecond, microsecond, nanosecond);
  if (!temporalTime) {
    return false;
  }

  args.rval().setObject(*temporalTime);
  return true;
}

/**
 * Temporal.PlainTime.from ( item [ , options ] )
 */
static bool PlainTime_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1. (Not applicable)

  auto overflow = TemporalOverflow::Constrain;
  if (args.hasDefined(1)) {
    // Step 2.
    Rooted<JSObject*> options(cx,
                              RequireObjectArg(cx, "options", "from", args[1]));
    if (!options) {
      return false;
    }

    // Step 3.
    if (!ToTemporalOverflow(cx, options, &overflow)) {
      return false;
    }
  }

  // Step 4.
  if (args.get(0).isObject()) {
    JSObject* item = &args[0].toObject();
    if (auto* time = item->maybeUnwrapIf<PlainTimeObject>()) {
      auto* result = CreateTemporalTime(cx, ToPlainTime(time));
      if (!result) {
        return false;
      }

      args.rval().setObject(*result);
      return true;
    }
  }

  // Step 5.
  auto result = ToTemporalTime(cx, args.get(0), overflow);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * get Temporal.PlainTime.prototype.calendar
 */
static bool PlainTime_calendar(JSContext* cx, const CallArgs& args) {
  Rooted<PlainTimeObject*> temporalTime(
      cx, &args.thisv().toObject().as<PlainTimeObject>());

  // Step 3.
  auto* calendar = PlainTimeObject::getOrCreateCalendar(cx, temporalTime);
  if (!calendar) {
    return false;
  }

  args.rval().setObject(*calendar);
  return true;
}

/**
 * get Temporal.PlainTime.prototype.calendar
 */
static bool PlainTime_calendar(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_calendar>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.hour
 */
static bool PlainTime_hour(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoHour());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.hour
 */
static bool PlainTime_hour(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_hour>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.minute
 */
static bool PlainTime_minute(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoMinute());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.minute
 */
static bool PlainTime_minute(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_minute>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.second
 */
static bool PlainTime_second(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoSecond());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.second
 */
static bool PlainTime_second(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_second>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.millisecond
 */
static bool PlainTime_millisecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoMillisecond());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.millisecond
 */
static bool PlainTime_millisecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_millisecond>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.microsecond
 */
static bool PlainTime_microsecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoMicrosecond());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.microsecond
 */
static bool PlainTime_microsecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_microsecond>(cx, args);
}

/**
 * get Temporal.PlainTime.prototype.nanosecond
 */
static bool PlainTime_nanosecond(JSContext* cx, const CallArgs& args) {
  // Step 3.
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  args.rval().setInt32(temporalTime->isoNanosecond());
  return true;
}

/**
 * get Temporal.PlainTime.prototype.nanosecond
 */
static bool PlainTime_nanosecond(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_nanosecond>(cx, args);
}

/**
 * Temporal.PlainTime.prototype.toPlainDateTime ( temporalDate )
 */
static bool PlainTime_toPlainDateTime(JSContext* cx, const CallArgs& args) {
  auto* temporalTime = &args.thisv().toObject().as<PlainTimeObject>();
  auto time = ToPlainTime(temporalTime);

  // Step 3.
  PlainDate date;
  Rooted<JSObject*> calendar(cx);
  if (!ToTemporalDate(cx, args.get(0), &date, &calendar)) {
    return false;
  }

  // Step 4.
  auto* result = CreateTemporalDateTime(cx, {date, time}, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.PlainTime.prototype.toPlainDateTime ( temporalDate )
 */
static bool PlainTime_toPlainDateTime(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_toPlainDateTime>(cx, args);
}

/**
 * Temporal.PlainTime.prototype.getISOFields ( )
 */
static bool PlainTime_getISOFields(JSContext* cx, const CallArgs& args) {
  Rooted<PlainTimeObject*> temporalTime(
      cx, &args.thisv().toObject().as<PlainTimeObject>());
  auto time = ToPlainTime(temporalTime);

  auto* calendar = PlainTimeObject::getOrCreateCalendar(cx, temporalTime);
  if (!calendar) {
    return false;
  }

  // Step 3.
  Rooted<IdValueVector> fields(cx, IdValueVector(cx));

  // Step 4.
  if (!fields.emplaceBack(NameToId(cx->names().calendar),
                          ObjectValue(*calendar))) {
    return false;
  }

  // Step 5.
  if (!fields.emplaceBack(NameToId(cx->names().isoHour),
                          Int32Value(time.hour))) {
    return false;
  }

  // Step 6.
  if (!fields.emplaceBack(NameToId(cx->names().isoMicrosecond),
                          Int32Value(time.microsecond))) {
    return false;
  }

  // Step 7.
  if (!fields.emplaceBack(NameToId(cx->names().isoMillisecond),
                          Int32Value(time.millisecond))) {
    return false;
  }

  // Step 8.
  if (!fields.emplaceBack(NameToId(cx->names().isoMinute),
                          Int32Value(time.minute))) {
    return false;
  }

  // Step 9.
  if (!fields.emplaceBack(NameToId(cx->names().isoNanosecond),
                          Int32Value(time.nanosecond))) {
    return false;
  }

  // Step 10.
  if (!fields.emplaceBack(NameToId(cx->names().isoSecond),
                          Int32Value(time.second))) {
    return false;
  }

  // Step 11.
  auto* obj =
      NewPlainObjectWithUniqueNames(cx, fields.begin(), fields.length());
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.PlainTime.prototype.getISOFields ( )
 */
static bool PlainTime_getISOFields(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsPlainTime, PlainTime_getISOFields>(cx, args);
}

/**
 * Temporal.PlainTime.prototype.valueOf ( )
 */
static bool PlainTime_valueOf(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                            "PlainTime", "primitive type");
  return false;
}

const JSClass PlainTimeObject::class_ = {
    "Temporal.PlainTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainTime),
    JS_NULL_CLASS_OPS,
    &PlainTimeObject::classSpec_,
};

const JSClass& PlainTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainTime_methods[] = {
    JS_FN("from", PlainTime_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec PlainTime_prototype_methods[] = {
    JS_FN("toPlainDateTime", PlainTime_toPlainDateTime, 1, 0),
    JS_FN("getISOFields", PlainTime_getISOFields, 0, 0),
    JS_FN("valueOf", PlainTime_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec PlainTime_prototype_properties[] = {
    JS_PSG("calendar", PlainTime_calendar, 0),
    JS_PSG("hour", PlainTime_hour, 0),
    JS_PSG("minute", PlainTime_minute, 0),
    JS_PSG("second", PlainTime_second, 0),
    JS_PSG("millisecond", PlainTime_millisecond, 0),
    JS_PSG("microsecond", PlainTime_microsecond, 0),
    JS_PSG("nanosecond", PlainTime_nanosecond, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.PlainTime", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec PlainTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainTimeConstructor, 0, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainTimeObject>,
    PlainTime_methods,
    nullptr,
    PlainTime_prototype_methods,
    PlainTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
