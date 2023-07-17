/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalNow.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/TimeZone.h"
#include "mozilla/Result.h"

#include <cmath>
#include <cstdlib>
#include <stdint.h>
#include <string_view>
#include <utility>

#include "jsdate.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/FormatBuffer.h"
#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/Allocator.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/ValueArray.h"
#include "vm/DateTime.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::temporal;

static bool SystemTimeZoneOffset(JSContext* cx, int32_t* offset) {
  auto timeZone = mozilla::intl::TimeZone::TryCreate();
  if (timeZone.isErr()) {
    intl::ReportInternalError(cx, timeZone.unwrapErr());
    return false;
  }

  auto rawOffset = timeZone.unwrap()->GetRawOffsetMs();
  if (rawOffset.isErr()) {
    intl::ReportInternalError(cx);
    return false;
  }

  *offset = rawOffset.unwrap();
  return true;
}

/**
 * 6.4.3 DefaultTimeZone ()
 *
 * Returns the IANA time zone name for the host environment's current time zone.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 */
static JSString* SystemTimeZoneIdentifier(JSContext* cx) {
  intl::FormatBuffer<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> formatBuffer(cx);
  auto result = mozilla::intl::TimeZone::GetDefaultTimeZone(formatBuffer);
  if (result.isErr()) {
    intl::ReportInternalError(cx, result.unwrapErr());
    return nullptr;
  }

  Rooted<JSString*> timeZone(cx, formatBuffer.toString(cx));
  if (!timeZone) {
    return nullptr;
  }

  Rooted<JSAtom*> validTimeZone(cx);
  if (!IsValidTimeZoneName(cx, timeZone, &validTimeZone)) {
    return nullptr;
  }
  if (validTimeZone) {
    return CanonicalizeTimeZoneName(cx, validTimeZone);
  }

  // See DateTimeFormat.js for the JS implementation.
  // TODO: Move the JS implementation into C++.

  // Before defaulting to "UTC", try to represent the system time zone using
  // the Etc/GMT + offset format. This format only accepts full hour offsets.
  int32_t offset;
  if (!SystemTimeZoneOffset(cx, &offset)) {
    return nullptr;
  }

  constexpr int32_t msPerHour = 60 * 60 * 1000;
  int32_t offsetHours = std::abs(offset / msPerHour);
  int32_t offsetHoursFraction = offset % msPerHour;
  if (offsetHoursFraction == 0 && offsetHours < 24) {
    // Etc/GMT + offset uses POSIX-style signs, i.e. a positive offset
    // means a location west of GMT.
    constexpr std::string_view etcGMT = "Etc/GMT";

    char offsetString[etcGMT.length() + 3];

    size_t n = etcGMT.copy(offsetString, etcGMT.length());
    offsetString[n++] = offset < 0 ? '+' : '-';
    if (offsetHours >= 10) {
      offsetString[n++] = '0' + (offsetHours / 10);
    }
    offsetString[n++] = '0' + (offsetHours % 10);

    MOZ_ASSERT(n == etcGMT.length() + 2 || n == etcGMT.length() + 3);

    timeZone = NewStringCopyN<CanGC>(cx, offsetString, n);
    if (!timeZone) {
      return nullptr;
    }

    // Check if the fallback is valid.
    if (!IsValidTimeZoneName(cx, timeZone, &validTimeZone)) {
      return nullptr;
    }
    if (validTimeZone) {
      return CanonicalizeTimeZoneName(cx, validTimeZone);
    }
  }

  // Fallback to "UTC" if everything else fails.
  return cx->names().UTC;
}

static BuiltinTimeZoneObject* SystemTimeZoneObject(JSContext* cx) {
  Rooted<JSString*> timeZoneIdentifier(cx, SystemTimeZoneIdentifier(cx));
  if (!timeZoneIdentifier) {
    return nullptr;
  }

  return CreateTemporalTimeZone(cx, timeZoneIdentifier);
}

/**
 * SystemUTCEpochNanoseconds ( )
 */
static bool SystemUTCEpochNanoseconds(JSContext* cx, Instant* result) {
  // Step 1.
  JS::ClippedTime nowMillis = DateNow(cx);
  MOZ_ASSERT(nowMillis.isValid());

  // Step 2.
  MOZ_ASSERT(nowMillis.toDouble() >= js::StartOfTime);
  MOZ_ASSERT(nowMillis.toDouble() <= js::EndOfTime);

  // Step 3.
  *result = Instant::fromMilliseconds(int64_t(nowMillis.toDouble()));
  return true;
}

/**
 * SystemInstant ( )
 */
static bool SystemInstant(JSContext* cx, Instant* result) {
  // Steps 1-2.
  return SystemUTCEpochNanoseconds(cx, result);
}

/**
 * SystemInstant ( )
 */
static InstantObject* SystemInstant(JSContext* cx) {
  // Step 1.
  Instant instant;
  if (!SystemUTCEpochNanoseconds(cx, &instant)) {
    return nullptr;
  }

  // Step 2.
  return CreateTemporalInstant(cx, instant);
}

/**
 * SystemDateTime ( temporalTimeZoneLike, calendarLike )
 */
static PlainDateTimeObject* SystemDateTime(JSContext* cx,
                                           Handle<Value> temporalTimeZoneLike,
                                           Handle<Value> calendarLike) {
  // Steps 1-2.
  Rooted<TimeZoneValue> timeZone(cx);
  if (temporalTimeZoneLike.isUndefined()) {
    auto* timeZoneObj = SystemTimeZoneObject(cx);
    if (!timeZoneObj) {
      return nullptr;
    }
    timeZone.set(TimeZoneValue(timeZoneObj));
  } else {
    if (!ToTemporalTimeZone(cx, temporalTimeZoneLike, &timeZone)) {
      return nullptr;
    }
  }

  // Step 3.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, calendarLike, &calendar)) {
    return nullptr;
  }

  // Step 4.
  Instant instant;
  if (!SystemInstant(cx, &instant)) {
    return nullptr;
  }

  // Step 5.
  return GetPlainDateTimeFor(cx, timeZone, instant, calendar);
}

/**
 * SystemZonedDateTime ( temporalTimeZoneLike, calendarLike )
 */
static ZonedDateTimeObject* SystemZonedDateTime(
    JSContext* cx, Handle<Value> temporalTimeZoneLike,
    Handle<Value> calendarLike) {
  // Steps 1-2.
  Rooted<TimeZoneValue> timeZone(cx);
  if (temporalTimeZoneLike.isUndefined()) {
    auto* timeZoneObj = SystemTimeZoneObject(cx);
    if (!timeZoneObj) {
      return nullptr;
    }
    timeZone.set(TimeZoneValue(timeZoneObj));
  } else {
    if (!ToTemporalTimeZone(cx, temporalTimeZoneLike, &timeZone)) {
      return nullptr;
    }
  }

  // Step 3.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendar(cx, calendarLike, &calendar)) {
    return nullptr;
  }

  // Step 4.
  Instant instant;
  if (!SystemUTCEpochNanoseconds(cx, &instant)) {
    return nullptr;
  }

  // Step 5.
  return CreateTemporalZonedDateTime(cx, instant, timeZone, calendar);
}

/**
 * Temporal.Now.timeZoneId ( )
 */
static bool Temporal_Now_timeZoneId(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* result = SystemTimeZoneIdentifier(cx);
  if (!result) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

/**
 * Temporal.Now.instant ( )
 */
static bool Temporal_Now_instant(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* result = SystemInstant(cx);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDateTime ( calendar [ , temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDateTime(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* result = SystemDateTime(cx, args.get(1), args.get(0));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDateTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDateTimeISO(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<Value> calendar(cx, StringValue(cx->names().iso8601));
  auto* result = SystemDateTime(cx, args.get(0), calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.zonedDateTime ( calendar [ , temporalTimeZoneLike ] )
 */
static bool Temporal_Now_zonedDateTime(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* result = SystemZonedDateTime(cx, args.get(1), args.get(0));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.zonedDateTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_zonedDateTimeISO(JSContext* cx, unsigned argc,
                                          Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<Value> calendar(cx, StringValue(cx->names().iso8601));
  auto* result = SystemZonedDateTime(cx, args.get(0), calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDate ( calendar [ , temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  auto* dateTime = SystemDateTime(cx, args.get(1), args.get(0));
  if (!dateTime) {
    return false;
  }
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());

  // Step 2.
  auto* result = CreateTemporalDate(cx, ToPlainDate(dateTime), calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainDateISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainDateISO(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<Value> calendarValue(cx, StringValue(cx->names().iso8601));
  auto* dateTime = SystemDateTime(cx, args.get(0), calendarValue);
  if (!dateTime) {
    return false;
  }

  // Step 2.
  Rooted<CalendarValue> calendar(cx, dateTime->calendar());
  auto* result = CreateTemporalDate(cx, ToPlainDate(dateTime), calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.Now.plainTimeISO ( [ temporalTimeZoneLike ] )
 */
static bool Temporal_Now_plainTimeISO(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  Rooted<Value> calendar(cx, StringValue(cx->names().iso8601));
  auto* dateTime = SystemDateTime(cx, args.get(0), calendar);
  if (!dateTime) {
    return false;
  }

  // Step 2.
  auto* result = CreateTemporalTime(cx, ToPlainTime(dateTime));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

const JSClass TemporalNowObject::class_ = {
    "Temporal.Now",
    JSCLASS_HAS_CACHED_PROTO(JSProto_TemporalNow),
    JS_NULL_CLASS_OPS,
    &TemporalNowObject::classSpec_,
};

static const JSFunctionSpec TemporalNow_methods[] = {
    JS_FN("timeZoneId", Temporal_Now_timeZoneId, 0, 0),
    JS_FN("instant", Temporal_Now_instant, 0, 0),
    JS_FN("plainDateTime", Temporal_Now_plainDateTime, 1, 0),
    JS_FN("plainDateTimeISO", Temporal_Now_plainDateTimeISO, 0, 0),
    JS_FN("zonedDateTime", Temporal_Now_zonedDateTime, 1, 0),
    JS_FN("zonedDateTimeISO", Temporal_Now_zonedDateTimeISO, 0, 0),
    JS_FN("plainDate", Temporal_Now_plainDate, 1, 0),
    JS_FN("plainDateISO", Temporal_Now_plainDateISO, 0, 0),
    JS_FN("plainTimeISO", Temporal_Now_plainTimeISO, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec TemporalNow_properties[] = {
    JS_STRING_SYM_PS(toStringTag, "Temporal.Now", JSPROP_READONLY),
    JS_PS_END,
};

static JSObject* CreateTemporalNowObject(JSContext* cx, JSProtoKey key) {
  Rooted<JSObject*> proto(cx, &cx->global()->getObjectPrototype());
  return NewTenuredObjectWithGivenProto(cx, &TemporalNowObject::class_, proto);
}

const ClassSpec TemporalNowObject::classSpec_ = {
    CreateTemporalNowObject,
    nullptr,
    TemporalNow_methods,
    TemporalNow_properties,
    nullptr,
    nullptr,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
