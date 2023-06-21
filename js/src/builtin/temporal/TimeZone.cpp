/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TimeZone.h"

#include "mozilla/Array.h"
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/intl/TimeZone.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"

#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "jstypes.h"
#include "NamespaceImports.h"

#include "builtin/Array.h"
#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/FormatBuffer.h"
#include "builtin/intl/SharedIntlData.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/ZonedDateTime.h"
#include "gc/Allocator.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "gc/GCContext.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/Date.h"
#include "js/ErrorReport.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"
#include "js/Printer.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/StableStringChars.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "threading/ProtectedData.h"
#include "vm/ArrayObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compartment.h"
#include "vm/DateTime.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"
#include "vm/Runtime.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static inline bool IsTimeZone(Handle<Value> v) {
  return v.isObject() && v.toObject().is<TimeZoneObject>();
}

static mozilla::UniquePtr<mozilla::intl::TimeZone> CreateIntlTimeZone(
    JSContext* cx, JSString* identifier) {
  JS::AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, identifier)) {
    return nullptr;
  }

  auto result = mozilla::intl::TimeZone::TryCreate(
      mozilla::Some(stableChars.twoByteRange()));
  if (result.isErr()) {
    intl::ReportInternalError(cx, result.unwrapErr());
    return nullptr;
  }
  return result.unwrap();
}

static mozilla::intl::TimeZone* GetOrCreateIntlTimeZone(
    JSContext* cx, Handle<TimeZoneObject*> timeZone) {
  // Obtain a cached mozilla::intl::TimeZone object.
  if (auto* tz = timeZone->getTimeZone()) {
    return tz;
  }

  auto* tz = CreateIntlTimeZone(cx, timeZone->identifier()).release();
  if (!tz) {
    return nullptr;
  }
  timeZone->setTimeZone(tz);

  intl::AddICUCellMemory(timeZone, TimeZoneObject::EstimatedMemoryUse);
  return tz;
}

/**
 * IsValidTimeZoneName ( timeZone )
 * IsAvailableTimeZoneName ( timeZone )
 */
bool js::temporal::IsValidTimeZoneName(
    JSContext* cx, Handle<JSString*> timeZone,
    MutableHandle<JSAtom*> validatedTimeZone) {
  intl::SharedIntlData& sharedIntlData = cx->runtime()->sharedIntlData.ref();

  if (!sharedIntlData.validateTimeZoneName(cx, timeZone, validatedTimeZone)) {
    return false;
  }

  if (validatedTimeZone) {
    cx->markAtom(validatedTimeZone);
  }
  return true;
}

/**
 * 6.5.2 CanonicalizeTimeZoneName ( timeZone )
 *
 * Canonicalizes the given IANA time zone name.
 *
 * ES2024 Intl draft rev 74ca7099f103d143431b2ea422ae640c6f43e3e6
 */
JSString* js::temporal::CanonicalizeTimeZoneName(
    JSContext* cx, Handle<JSLinearString*> timeZone) {
  // Step 1. (Not applicable, the input is already a valid IANA time zone.)
#ifdef DEBUG
  MOZ_ASSERT(!StringEqualsLiteral(timeZone, "Etc/Unknown"),
             "Invalid time zone");

  Rooted<JSAtom*> checkTimeZone(cx);
  if (!IsValidTimeZoneName(cx, timeZone, &checkTimeZone)) {
    return nullptr;
  }
  MOZ_ASSERT(EqualStrings(timeZone, checkTimeZone),
             "Time zone name not normalized");
#endif

  // Step 2.
  Rooted<JSLinearString*> ianaTimeZone(cx);
  do {
    intl::SharedIntlData& sharedIntlData = cx->runtime()->sharedIntlData.ref();

    // Some time zone names are canonicalized differently by ICU -- handle
    // those first:
    Rooted<JSAtom*> canonicalTimeZone(cx);
    if (!sharedIntlData.tryCanonicalizeTimeZoneConsistentWithIANA(
            cx, timeZone, &canonicalTimeZone)) {
      return nullptr;
    }

    if (canonicalTimeZone) {
      cx->markAtom(canonicalTimeZone);
      ianaTimeZone = canonicalTimeZone;
      break;
    }

    JS::AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, timeZone)) {
      return nullptr;
    }

    intl::FormatBuffer<char16_t, intl::INITIAL_CHAR_BUFFER_SIZE> buffer(cx);
    auto result = mozilla::intl::TimeZone::GetCanonicalTimeZoneID(
        stableChars.twoByteRange(), buffer);
    if (result.isErr()) {
      intl::ReportInternalError(cx, result.unwrapErr());
      return nullptr;
    }

    ianaTimeZone = buffer.toString(cx);
    if (!ianaTimeZone) {
      return nullptr;
    }
  } while (false);

#ifdef DEBUG
  MOZ_ASSERT(!StringEqualsLiteral(ianaTimeZone, "Etc/Unknown"),
             "Invalid canonical time zone");

  if (!IsValidTimeZoneName(cx, ianaTimeZone, &checkTimeZone)) {
    return nullptr;
  }
  MOZ_ASSERT(EqualStrings(ianaTimeZone, checkTimeZone),
             "Unsupported canonical time zone");
#endif

  // Step 3.
  if (StringEqualsLiteral(ianaTimeZone, "Etc/UTC") ||
      StringEqualsLiteral(ianaTimeZone, "Etc/GMT")) {
    return cx->names().UTC;
  }

  // We don't need to check against "GMT", because ICU uses the tzdata rearguard
  // format, where "GMT" is a link to "Etc/GMT".
  MOZ_ASSERT(!StringEqualsLiteral(ianaTimeZone, "GMT"));

  // Step 4.
  return ianaTimeZone;
}

/**
 * IsValidTimeZoneName ( timeZone )
 * IsAvailableTimeZoneName ( timeZone )
 * CanonicalizeTimeZoneName ( timeZone )
 */
JSString* js::temporal::ValidateAndCanonicalizeTimeZoneName(
    JSContext* cx, Handle<JSString*> timeZone) {
  Rooted<JSAtom*> validatedTimeZone(cx);
  if (!IsValidTimeZoneName(cx, timeZone, &validatedTimeZone)) {
    return nullptr;
  }

  if (!validatedTimeZone) {
    if (auto chars = QuoteString(cx, timeZone)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_TIMEZONE_INVALID_IDENTIFIER,
                               chars.get());
    }
    return nullptr;
  }

  return CanonicalizeTimeZoneName(cx, validatedTimeZone);
}

static bool TimeZone_toString(JSContext* cx, unsigned argc, Value* vp);

JSString* js::temporal::TimeZoneToString(JSContext* cx,
                                         Handle<JSObject*> timeZone) {
  if (timeZone->is<TimeZoneObject>() &&
      HasNoToPrimitiveMethodPure(timeZone, cx) &&
      HasNativeMethodPure(timeZone, cx->names().toString, TimeZone_toString,
                          cx)) {
    JSString* id = timeZone->as<TimeZoneObject>().identifier();
    MOZ_ASSERT(id);
    return id;
  }

  Rooted<Value> timeZoneValue(cx, ObjectValue(*timeZone));
  return JS::ToString(cx, timeZoneValue);
}

/**
 * GetNamedTimeZoneNextTransition ( timeZoneIdentifier, epochNanoseconds )
 */
static bool GetNamedTimeZoneNextTransition(JSContext* cx,
                                           Handle<TimeZoneObject*> timeZone,
                                           const Instant& epochInstant,
                                           mozilla::Maybe<Instant>* result) {
  MOZ_ASSERT(timeZone->offsetNanoseconds().isUndefined());

  // Round down (floor) to the previous full millisecond.
  //
  // IANA has experimental support for transitions at sub-second precision, but
  // the default configuration doesn't enable it, therefore it's safe to round
  // to milliseconds here. In addition to that, ICU also only supports
  // transitions at millisecond precision.
  int64_t millis = epochInstant.floorToMilliseconds();

  auto* tz = GetOrCreateIntlTimeZone(cx, timeZone);
  if (!tz) {
    return false;
  }

  auto next = tz->GetNextTransition(millis);
  if (next.isErr()) {
    intl::ReportInternalError(cx, next.unwrapErr());
    return false;
  }

  auto transition = next.unwrap();
  if (!transition) {
    *result = mozilla::Nothing();
    return true;
  }

  auto transitionInstant = Instant::fromMilliseconds(*transition);
  if (!IsValidEpochInstant(transitionInstant)) {
    *result = mozilla::Nothing();
    return true;
  }

  *result = mozilla::Some(transitionInstant);
  return true;
}

/**
 * GetNamedTimeZonePreviousTransition ( timeZoneIdentifier, epochNanoseconds )
 */
static bool GetNamedTimeZonePreviousTransition(
    JSContext* cx, Handle<TimeZoneObject*> timeZone,
    const Instant& epochInstant, mozilla::Maybe<Instant>* result) {
  MOZ_ASSERT(timeZone->offsetNanoseconds().isUndefined());

  // Round up (ceil) to the next full millisecond.
  //
  // IANA has experimental support for transitions at sub-second precision, but
  // the default configuration doesn't enable it, therefore it's safe to round
  // to milliseconds here. In addition to that, ICU also only supports
  // transitions at millisecond precision.
  int64_t millis = epochInstant.ceilToMilliseconds();

  auto* tz = GetOrCreateIntlTimeZone(cx, timeZone);
  if (!tz) {
    return false;
  }

  auto previous = tz->GetPreviousTransition(millis);
  if (previous.isErr()) {
    intl::ReportInternalError(cx, previous.unwrapErr());
    return false;
  }

  auto transition = previous.unwrap();
  if (!transition) {
    *result = mozilla::Nothing();
    return true;
  }

  auto transitionInstant = Instant::fromMilliseconds(*transition);
  if (!IsValidEpochInstant(transitionInstant)) {
    *result = mozilla::Nothing();
    return true;
  }

  *result = mozilla::Some(transitionInstant);
  return true;
}

/**
 * FormatTimeZoneOffsetString ( offsetNanoseconds )
 */
JSString* js::temporal::FormatTimeZoneOffsetString(JSContext* cx,
                                                   int64_t offsetNanoseconds) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  char sign = offsetNanoseconds >= 0 ? '+' : '-';

  // Step 3.
  offsetNanoseconds = std::abs(offsetNanoseconds);

  // Step 4.
  int32_t nanoseconds = int32_t(offsetNanoseconds % 1'000'000'000);

  // Step 5.
  int32_t quotient = int32_t(offsetNanoseconds / 1'000'000'000);
  int32_t seconds = quotient % 60;

  // Step 6.
  quotient /= 60;
  int32_t minutes = quotient % 60;

  // Step 7.
  int32_t hours = quotient / 60;
  MOZ_ASSERT(hours < 24, "time zone offset mustn't exceed 24-hours");

  // Format: "sign hour{2} : minute{2} : second{2} . fractional{9}"
  constexpr size_t maxLength = 1 + 2 + 1 + 2 + 1 + 2 + 1 + 9;
  char result[maxLength];

  size_t n = 0;

  // Steps 8-9 and 13-14.
  result[n++] = sign;
  result[n++] = '0' + (hours / 10);
  result[n++] = '0' + (hours % 10);
  result[n++] = ':';
  result[n++] = '0' + (minutes / 10);
  result[n++] = '0' + (minutes % 10);

  // Steps 10-12.
  if (seconds != 0 || nanoseconds != 0) {
    // Steps 10, 11.a, 12.a.
    result[n++] = ':';
    result[n++] = '0' + (seconds / 10);
    result[n++] = '0' + (seconds % 10);

    // Steps 11.a-b.
    if (uint32_t fractional = nanoseconds) {
      result[n++] = '.';

      uint32_t k = 100'000'000;
      do {
        result[n++] = '0' + (fractional / k);
        fractional %= k;
        k /= 10;
      } while (fractional);
    }
  }

  MOZ_ASSERT(n <= maxLength);

  return NewStringCopyN<CanGC>(cx, result, n);
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
static TimeZoneObject* CreateTemporalTimeZone(JSContext* cx,
                                              const CallArgs& args,
                                              Handle<JSString*> identifier,
                                              Handle<Value> offsetNanoseconds) {
  MOZ_ASSERT(offsetNanoseconds.isUndefined() || offsetNanoseconds.isNumber());
  MOZ_ASSERT_IF(offsetNanoseconds.isNumber(),
                std::abs(offsetNanoseconds.toNumber()) <
                    ToNanoseconds(TemporalUnit::Day));

  // Steps 1-2.
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_TimeZone, &proto)) {
    return nullptr;
  }

  auto* timeZone = NewObjectWithClassProto<TimeZoneObject>(cx, proto);
  if (!timeZone) {
    return nullptr;
  }

  // Steps 3.a and 4.a. (Not applicable in our implementation.)

  // Steps 3.b and 4.b.
  timeZone->setFixedSlot(TimeZoneObject::IDENTIFIER_SLOT,
                         StringValue(identifier));

  // Steps 3.c and 4.c.
  timeZone->setFixedSlot(TimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                         offsetNanoseconds);

  // Step 5.
  return timeZone;
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
static TimeZoneObject* CreateTemporalTimeZone(JSContext* cx,
                                              Handle<JSString*> identifier) {
  // Steps 1-2.
  auto* object = NewBuiltinClassInstance<TimeZoneObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 3. (Not applicable)

  // Step 4.a. (Checked in caller)

  // Step 4.b.
  object->setFixedSlot(TimeZoneObject::IDENTIFIER_SLOT,
                       StringValue(identifier));

  // Step 4.c.
  object->setFixedSlot(TimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                       UndefinedValue());
  // Step 6.
  return object;
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
static TimeZoneObject* CreateTemporalTimeZone(JSContext* cx,
                                              int64_t offsetNanoseconds) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  Rooted<JSString*> identifier(
      cx, FormatTimeZoneOffsetString(cx, offsetNanoseconds));
  if (!identifier) {
    return nullptr;
  }

  // Steps 1-2.
  auto* object = NewBuiltinClassInstance<TimeZoneObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 3.a. (Not applicable in our implementation.)

  // Step 3.b.
  object->setFixedSlot(TimeZoneObject::IDENTIFIER_SLOT,
                       StringValue(identifier));

  // Step 3.c.
  object->setFixedSlot(TimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                       NumberValue(offsetNanoseconds));

  // Step 4. (Not applicable)

  // Step 5.
  return object;
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
TimeZoneObject* js::temporal::CreateTemporalTimeZone(
    JSContext* cx, Handle<JSString*> identifier) {
  return ::CreateTemporalTimeZone(cx, identifier);
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 *
 * When CreateTemporalTimeZone is called with `identifier="UTC"`.
 */
TimeZoneObject* js::temporal::CreateTemporalTimeZoneUTC(JSContext* cx) {
  Handle<JSString*> identifier = cx->names().UTC.toHandle();
  return ::CreateTemporalTimeZone(cx, identifier);
}

/**
 * ToTemporalTimeZone ( temporalTimeZoneLike )
 */
TimeZoneObject* js::temporal::ToTemporalTimeZone(JSContext* cx,
                                                 Handle<JSString*> string) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  Rooted<JSString*> timeZoneName(cx);
  int64_t offsetNanoseconds = 0;
  if (!ParseTemporalTimeZoneString(cx, string, &timeZoneName,
                                   &offsetNanoseconds)) {
    return nullptr;
  }

  // Steps 4-5.
  if (timeZoneName) {
    // Step 4.a. (Implemented in ParseTemporalTimeZoneString)

    // Step 4.b.
    timeZoneName = ValidateAndCanonicalizeTimeZoneName(cx, timeZoneName);
    if (!timeZoneName) {
      return nullptr;
    }

    // Steps 4.c and 5.
    return ::CreateTemporalTimeZone(cx, timeZoneName);
  }

  // Step 6.
  return ::CreateTemporalTimeZone(cx, offsetNanoseconds);
}

/**
 * ToTemporalTimeZone ( temporalTimeZoneLike )
 */
JSObject* js::temporal::ToTemporalTimeZone(JSContext* cx,
                                           Handle<Value> temporalTimeZoneLike) {
  // Step 1.
  Rooted<Value> timeZoneLike(cx, temporalTimeZoneLike);
  if (timeZoneLike.isObject()) {
    Rooted<JSObject*> obj(cx, &timeZoneLike.toObject());

    // Step 1.a.
    if (obj->canUnwrapAs<TimeZoneObject>()) {
      return obj;
    }

    // Step 1.b.
    if (auto* zonedDateTime = obj->maybeUnwrapIf<ZonedDateTimeObject>()) {
      Rooted<JSObject*> timeZone(cx, zonedDateTime->timeZone());
      if (!cx->compartment()->wrap(cx, &timeZone)) {
        return nullptr;
      }
      return timeZone;
    }

    // Step 1.c.
    if (obj->canUnwrapAs<CalendarObject>()) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_INVALID_OBJECT,
                               "Temporal.TimeZone", "Temporal.Calendar");
      return nullptr;
    }

    // Step 1.d.
    bool hasTimeZone;
    if (!HasProperty(cx, obj, cx->names().timeZone, &hasTimeZone)) {
      return nullptr;
    }
    if (!hasTimeZone) {
      return obj;
    }

    // Step 1.e.
    if (!GetProperty(cx, obj, obj, cx->names().timeZone, &timeZoneLike)) {
      return nullptr;
    }

    // Step 1.f.
    if (timeZoneLike.isObject()) {
      obj = &timeZoneLike.toObject();

      // FIXME: spec issue - does this check is actually useful? In which case
      // will have a "timeZone" property be a CalendarObject?

      // Step 1.f.i.
      if (obj->canUnwrapAs<CalendarObject>()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_TEMPORAL_INVALID_OBJECT,
                                 "Temporal.TimeZone", "Temporal.Calendar");
        return nullptr;
      }

      // FIXME: spec issue - does this check is actually useful? In which case
      // will have a "timeZone" property have another "timeZone" property?

      // Step 1.f.ii.
      if (!HasProperty(cx, obj, cx->names().timeZone, &hasTimeZone)) {
        return nullptr;
      }
      if (!hasTimeZone) {
        return obj;
      }
    }
  }

  // Step 2.
  Rooted<JSString*> identifier(cx, JS::ToString(cx, timeZoneLike));
  if (!identifier) {
    return nullptr;
  }

  // The string representation of (most) other types can never be parsed as a
  // time zone, so directly throw an error here. But still perform ToString
  // first for possible side-effects.
  //
  // Numeric values are also accepted, because their ToString representation can
  // be parsed as a time zone offset value, e.g. ToString(-10) == "-10", which
  // is then interpreted as "-10:00".
  //
  // FIXME: spec issue - ToString for negative Numbers/BigInt also accepted?
  if (!timeZoneLike.isString() && !timeZoneLike.isObject() &&
      !timeZoneLike.isNumeric()) {
    ReportValueError(cx, JSMSG_TEMPORAL_TIMEZONE_PARSE_BAD_TYPE,
                     JSDVG_IGNORE_STACK, timeZoneLike, nullptr);
    return nullptr;
  }

  // Steps 3-6.
  return ToTemporalTimeZone(cx, identifier);
}

/**
 * IsTimeZoneOffsetString ( offsetString )
 *
 * Return true if |offsetString| is the prefix of a time zone offset string.
 * Time zone offset strings are be parsed through the |UTCOffset| production.
 *
 * UTCOffset :::
 *   TemporalSign Hour
 *   TemporalSign Hour HourSubcomponents[+Extended]
 *   TemporalSign Hour HourSubcomponents[~Extended]
 *
 * TemporalSign :::
 *   ASCIISign
 *   <MINUS>
 *
 * ASCIISign ::: one of + -
 *
 * NOTE: IANA time zone identifiers can't start with TemporalSign.
 */
static bool IsTimeZoneOffsetStringPrefix(JSLinearString* offsetString) {
  // Empty string can't be the prefix of |UTCOffset|.
  if (offsetString->empty()) {
    return false;
  }

  // Return true iff |offsetString| starts with |TemporalSign|.
  char16_t ch = offsetString->latin1OrTwoByteChar(0);
  return ch == '+' || ch == '-' || ch == 0x2212;
}

/**
 * Temporal.TimeZone ( identifier )
 */
static bool TimeZoneConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  if (!ThrowIfNotConstructing(cx, args, "Temporal.TimeZone")) {
    return false;
  }

  // Step 2.
  Rooted<JSString*> identifier(cx, JS::ToString(cx, args.get(0)));
  if (!identifier) {
    return false;
  }

  Rooted<JSLinearString*> linearIdentifier(cx, identifier->ensureLinear(cx));
  if (!linearIdentifier) {
    return false;
  }

  Rooted<JSString*> canonical(cx);
  Rooted<Value> offsetNanoseconds(cx);
  if (IsTimeZoneOffsetStringPrefix(linearIdentifier)) {
    // Step 3.
    int64_t nanoseconds;
    if (!ParseTimeZoneOffsetString(cx, linearIdentifier, &nanoseconds)) {
      return false;
    }
    MOZ_ASSERT(std::abs(nanoseconds) < ToNanoseconds(TemporalUnit::Day));

    canonical = FormatTimeZoneOffsetString(cx, nanoseconds);
    if (!canonical) {
      return false;
    }

    offsetNanoseconds.setNumber(nanoseconds);
  } else {
    // Step 4.
    canonical = ValidateAndCanonicalizeTimeZoneName(cx, linearIdentifier);
    if (!canonical) {
      return false;
    }

    offsetNanoseconds.setUndefined();
  }

  // Step 5.
  auto* timeZone =
      CreateTemporalTimeZone(cx, args, canonical, offsetNanoseconds);
  if (!timeZone) {
    return false;
  }

  args.rval().setObject(*timeZone);
  return true;
}

/**
 * Temporal.TimeZone.from ( item )
 */
static bool TimeZone_from(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1.
  JSObject* timeZone = ToTemporalTimeZone(cx, args.get(0));
  if (!timeZone) {
    return false;
  }

  args.rval().setObject(*timeZone);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getNextTransition ( startingPoint )
 */
static bool TimeZone_getNextTransition(JSContext* cx, const CallArgs& args) {
  Rooted<TimeZoneObject*> timeZone(
      cx, &args.thisv().toObject().as<TimeZoneObject>());

  // Step 3.
  Instant startingPoint;
  if (!ToTemporalInstantEpochInstant(cx, args.get(0), &startingPoint)) {
    return false;
  }

  // Step 4.
  if (!timeZone->offsetNanoseconds().isUndefined()) {
    args.rval().setNull();
    return true;
  }

  // Step 5.
  mozilla::Maybe<Instant> transition;
  if (!GetNamedTimeZoneNextTransition(cx, timeZone, startingPoint,
                                      &transition)) {
    return false;
  }

  // Step 6.
  if (!transition) {
    args.rval().setNull();
    return true;
  }

  // Step 7.
  auto* instant = CreateTemporalInstant(cx, *transition);
  if (!instant) {
    return false;
  }

  args.rval().setObject(*instant);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getNextTransition ( startingPoint )
 */
static bool TimeZone_getNextTransition(JSContext* cx, unsigned argc,
                                       Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getNextTransition>(cx, args);
}

/**
 * Temporal.TimeZone.prototype.getPreviousTransition ( startingPoint )
 */
static bool TimeZone_getPreviousTransition(JSContext* cx,
                                           const CallArgs& args) {
  Rooted<TimeZoneObject*> timeZone(
      cx, &args.thisv().toObject().as<TimeZoneObject>());

  // Step 3.
  Instant startingPoint;
  if (!ToTemporalInstantEpochInstant(cx, args.get(0), &startingPoint)) {
    return false;
  }

  // Step 4.
  if (!timeZone->offsetNanoseconds().isUndefined()) {
    args.rval().setNull();
    return true;
  }

  // Step 5.
  mozilla::Maybe<Instant> transition;
  if (!GetNamedTimeZonePreviousTransition(cx, timeZone, startingPoint,
                                          &transition)) {
    return false;
  }

  // Step 6.
  if (!transition) {
    args.rval().setNull();
    return true;
  }

  // Step 7.
  auto* instant = CreateTemporalInstant(cx, *transition);
  if (!instant) {
    return false;
  }

  args.rval().setObject(*instant);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getPreviousTransition ( startingPoint )
 */
static bool TimeZone_getPreviousTransition(JSContext* cx, unsigned argc,
                                           Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getPreviousTransition>(cx,
                                                                          args);
}

/**
 * Temporal.TimeZone.prototype.toString ( )
 */
static bool TimeZone_toString(JSContext* cx, const CallArgs& args) {
  auto* timeZone = &args.thisv().toObject().as<TimeZoneObject>();

  // Step 3.
  args.rval().setString(timeZone->identifier());
  return true;
}

/**
 * Temporal.TimeZone.prototype.toString ( )
 */
static bool TimeZone_toString(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_toString>(cx, args);
}

/**
 * Temporal.TimeZone.prototype.toJSON ( )
 */
static bool TimeZone_toJSON(JSContext* cx, const CallArgs& args) {
  Rooted<JSObject*> timeZone(cx, &args.thisv().toObject());

  // Step 3.
  JSString* str = TimeZoneToString(cx, timeZone);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.TimeZone.prototype.toJSON ( )
 */
static bool TimeZone_toJSON(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_toJSON>(cx, args);
}

/**
 * get Temporal.TimeZone.prototype.id
 */
static bool TimeZone_id(JSContext* cx, const CallArgs& args) {
  auto* timeZone = &args.thisv().toObject().as<TimeZoneObject>();

  // Step 3.
  args.rval().setString(timeZone->identifier());
  return true;
}

/**
 * get Temporal.TimeZone.prototype.id
 */
static bool TimeZone_id(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_id>(cx, args);
}

void js::temporal::TimeZoneObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  MOZ_ASSERT(gcx->onMainThread());

  if (auto* timeZone = obj->as<TimeZoneObject>().getTimeZone()) {
    intl::RemoveICUCellMemory(gcx, obj, TimeZoneObject::EstimatedMemoryUse);
    delete timeZone;
  }
}

const JSClassOps TimeZoneObject::classOps_ = {
    nullptr,                   // addProperty
    nullptr,                   // delProperty
    nullptr,                   // enumerate
    nullptr,                   // newEnumerate
    nullptr,                   // resolve
    nullptr,                   // mayResolve
    TimeZoneObject::finalize,  // finalize
    nullptr,                   // call
    nullptr,                   // construct
    nullptr,                   // trace
};

const JSClass TimeZoneObject::class_ = {
    "Temporal.TimeZone",
    JSCLASS_HAS_RESERVED_SLOTS(TimeZoneObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_TimeZone) |
        JSCLASS_FOREGROUND_FINALIZE,
    &TimeZoneObject::classOps_,
    &TimeZoneObject::classSpec_,
};

const JSClass& TimeZoneObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec TimeZone_methods[] = {
    JS_FN("from", TimeZone_from, 1, 0),
    JS_FS_END,
};

static const JSFunctionSpec TimeZone_prototype_methods[] = {
    JS_FN("getNextTransition", TimeZone_getNextTransition, 1, 0),
    JS_FN("getPreviousTransition", TimeZone_getPreviousTransition, 1, 0),
    JS_FN("toString", TimeZone_toString, 0, 0),
    JS_FN("toJSON", TimeZone_toJSON, 0, 0),
    JS_FS_END,
};

static const JSPropertySpec TimeZone_prototype_properties[] = {
    JS_PSG("id", TimeZone_id, 0),
    JS_STRING_SYM_PS(toStringTag, "Temporal.TimeZone", JSPROP_READONLY),
    JS_PS_END,
};

const ClassSpec TimeZoneObject::classSpec_ = {
    GenericCreateConstructor<TimeZoneConstructor, 1, gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<TimeZoneObject>,
    TimeZone_methods,
    nullptr,
    TimeZone_prototype_methods,
    TimeZone_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
