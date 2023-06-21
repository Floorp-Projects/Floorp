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
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalRoundingMode.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/TimeZone.h"
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
    JS_FN("toString", ZonedDateTime_toString, 0, 0),
    JS_FN("toLocaleString", ZonedDateTime_toLocaleString, 0, 0),
    JS_FN("toJSON", ZonedDateTime_toJSON, 0, 0),
    JS_FN("valueOf", ZonedDateTime_valueOf, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec ZonedDateTime_prototype_properties[] = {
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
