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
#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/Duration.h"
#include "builtin/temporal/Instant.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainDateTime.h"
#include "builtin/temporal/PlainTime.h"
#include "builtin/temporal/Temporal.h"
#include "builtin/temporal/TemporalParser.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TemporalUnit.h"
#include "builtin/temporal/Wrapped.h"
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
#include "js/TracingAPI.h"
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
#include "vm/PIC.h"
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

void js::temporal::TimeZoneValue::trace(JSTracer* trc) {
  if (object_) {
    TraceRoot(trc, &object_, "TimeZoneValue::object");
  }
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
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone) {
  // Obtain a cached mozilla::intl::TimeZone object.
  if (auto* tz = timeZone->getTimeZone()) {
    return tz;
  }

  auto* tz = CreateIntlTimeZone(cx, timeZone->identifier()).release();
  if (!tz) {
    return nullptr;
  }
  timeZone->setTimeZone(tz);

  intl::AddICUCellMemory(timeZone,
                         TimeZoneObjectMaybeBuiltin::EstimatedMemoryUse);
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

class EpochInstantList final {
  // GetNamedTimeZoneEpochNanoseconds can return up-to two elements.
  static constexpr size_t MaxLength = 2;

  mozilla::Array<Instant, MaxLength> array_ = {};
  size_t length_ = 0;

 public:
  EpochInstantList() = default;

  size_t length() const { return length_; }

  void append(const Instant& instant) { array_[length_++] = instant; }

  auto& operator[](size_t i) { return array_[i]; }
  const auto& operator[](size_t i) const { return array_[i]; }

  auto begin() const { return array_.begin(); }
  auto end() const { return array_.begin() + length_; }
};

/**
 * GetNamedTimeZoneEpochNanoseconds ( timeZoneIdentifier, year, month, day,
 * hour, minute, second, millisecond, microsecond, nanosecond )
 */
static bool GetNamedTimeZoneEpochNanoseconds(
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone,
    const PlainDateTime& dateTime, EpochInstantList& instants) {
  MOZ_ASSERT(timeZone->offsetNanoseconds().isUndefined());
  MOZ_ASSERT(IsValidISODateTime(dateTime));
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));
  MOZ_ASSERT(instants.length() == 0);

  // FIXME: spec issue - assert ISODateTimeWithinLimits instead of
  // IsValidISODate

  int64_t ms = MakeDate(dateTime);

  auto* tz = GetOrCreateIntlTimeZone(cx, timeZone);
  if (!tz) {
    return false;
  }

  auto getOffset = [&](mozilla::intl::TimeZone::LocalOption skippedTime,
                       mozilla::intl::TimeZone::LocalOption repeatedTime,
                       int32_t* offset) {
    auto result = tz->GetUTCOffsetMs(ms, skippedTime, repeatedTime);
    if (result.isErr()) {
      intl::ReportInternalError(cx, result.unwrapErr());
      return false;
    }

    *offset = result.unwrap();
    MOZ_ASSERT(std::abs(*offset) < UnitsPerDay(TemporalUnit::Millisecond));

    return true;
  };

  constexpr auto formerTime = mozilla::intl::TimeZone::LocalOption::Former;
  constexpr auto latterTime = mozilla::intl::TimeZone::LocalOption::Latter;

  int32_t formerOffset;
  if (!getOffset(formerTime, formerTime, &formerOffset)) {
    return false;
  }

  int32_t latterOffset;
  if (!getOffset(latterTime, latterTime, &latterOffset)) {
    return false;
  }

  if (formerOffset == latterOffset) {
    auto epochInstant = GetUTCEpochNanoseconds(dateTime) -
                        InstantSpan::fromMilliseconds(formerOffset);
    instants.append(epochInstant);
    return true;
  }

  int32_t disambiguationOffset;
  if (!getOffset(formerTime, latterTime, &disambiguationOffset)) {
    return false;
  }

  // Skipped time.
  if (disambiguationOffset == formerOffset) {
    return true;
  }

  // Repeated time.
  for (auto offset : {formerOffset, latterOffset}) {
    auto epochInstant = GetUTCEpochNanoseconds(dateTime) -
                        InstantSpan::fromMilliseconds(offset);
    instants.append(epochInstant);
  }

  MOZ_ASSERT(instants.length() == 2);

  // Ensure the returned instants are sorted in numerical order.
  if (instants[0] > instants[1]) {
    std::swap(instants[0], instants[1]);
  }

  return true;
}

/**
 * GetNamedTimeZoneOffsetNanoseconds ( timeZoneIdentifier, epochNanoseconds )
 */
static bool GetNamedTimeZoneOffsetNanoseconds(
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone,
    const Instant& epochInstant, int64_t* offset) {
  MOZ_ASSERT(timeZone->offsetNanoseconds().isUndefined());

  // Round down (floor) to the previous full milliseconds.
  int64_t millis = epochInstant.floorToMilliseconds();

  auto* tz = GetOrCreateIntlTimeZone(cx, timeZone);
  if (!tz) {
    return false;
  }

  auto result = tz->GetOffsetMs(millis);
  if (result.isErr()) {
    intl::ReportInternalError(cx, result.unwrapErr());
    return false;
  }

  // FIXME: spec issue - should constrain the range to not exceed 24-hours.
  // https://github.com/tc39/ecma262/issues/3101

  int64_t nanoPerMs = 1'000'000;
  *offset = result.unwrap() * nanoPerMs;
  return true;
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

// Returns |RoundNumberToIncrement(offsetNanoseconds, 60 × 10^9, "halfExpand")|
// divided by |60 × 10^9|.
static int32_t RoundNanosecondsToMinutes(int64_t offsetNanoseconds) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  constexpr int64_t increment = ToNanoseconds(TemporalUnit::Minute);

  int64_t quotient = offsetNanoseconds / increment;
  int64_t remainder = offsetNanoseconds % increment;
  if (std::abs(remainder * 2) >= increment) {
    quotient += (offsetNanoseconds > 0 ? 1 : -1);
  }
  return quotient;
}

/**
 * FormatISOTimeZoneOffsetString ( offsetNanoseconds )
 */
JSString* js::temporal::FormatISOTimeZoneOffsetString(
    JSContext* cx, int64_t offsetNanoseconds) {
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  int32_t offsetMinutes = RoundNanosecondsToMinutes(offsetNanoseconds);

  // Step 3.
  int32_t sign = offsetMinutes < 0 ? -1 : 1;

  // Steps 4-6.
  auto hm = std::div(std::abs(offsetMinutes), 60);
  int32_t hours = hm.quot;
  int32_t minutes = hm.rem;

  MOZ_ASSERT(hours < 24, "time zone offset mustn't exceed 24-hours");

  // Format: "sign hour{2} : minute{2}"
  constexpr size_t maxLength = 1 + 2 + 1 + 2;
  char result[maxLength];

  // Steps 7-9.
  size_t n = 0;
  result[n++] = sign < 0 ? '-' : '+';
  result[n++] = '0' + (hours / 10);
  result[n++] = '0' + (hours % 10);
  result[n++] = ':';
  result[n++] = '0' + (minutes / 10);
  result[n++] = '0' + (minutes % 10);

  MOZ_ASSERT(n == maxLength);

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

  // Step 4.a. (Not applicable in our implementation.)

  // Steps 3.a or 4.b.
  timeZone->setFixedSlot(TimeZoneObject::IDENTIFIER_SLOT,
                         StringValue(identifier));

  // Step 3.b or 4.c.
  timeZone->setFixedSlot(TimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                         offsetNanoseconds);

  // Step 5.
  return timeZone;
}

static BuiltinTimeZoneObject* CreateBuiltinTimeZone(
    JSContext* cx, Handle<JSString*> identifier) {
  // TODO: Implement a built-in time zone object cache.

  auto* object = NewObjectWithGivenProto<BuiltinTimeZoneObject>(cx, nullptr);
  if (!object) {
    return nullptr;
  }

  object->setFixedSlot(BuiltinTimeZoneObject::IDENTIFIER_SLOT,
                       StringValue(identifier));

  object->setFixedSlot(BuiltinTimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                       UndefinedValue());

  return object;
}

static BuiltinTimeZoneObject* CreateBuiltinTimeZone(JSContext* cx,
                                                    int64_t offsetNanoseconds) {
  // TODO: It's unclear if offset time zones should also be cached. Real world
  // experience will tell if a cache should be added.

  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  Rooted<JSString*> identifier(
      cx, FormatTimeZoneOffsetString(cx, offsetNanoseconds));
  if (!identifier) {
    return nullptr;
  }

  auto* object = NewObjectWithGivenProto<BuiltinTimeZoneObject>(cx, nullptr);
  if (!object) {
    return nullptr;
  }

  object->setFixedSlot(BuiltinTimeZoneObject::IDENTIFIER_SLOT,
                       StringValue(identifier));

  object->setFixedSlot(BuiltinTimeZoneObject::OFFSET_NANOSECONDS_SLOT,
                       NumberValue(offsetNanoseconds));

  return object;
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
static TimeZoneObject* CreateTemporalTimeZone(
    JSContext* cx, Handle<BuiltinTimeZoneObject*> timeZone) {
  // Steps 1-2.
  auto* object = NewBuiltinClassInstance<TimeZoneObject>(cx);
  if (!object) {
    return nullptr;
  }

  // Step 4.a. (Not applicable in our implementation.)

  // Steps 3.a or 4.b.
  object->setFixedSlot(
      TimeZoneObject::IDENTIFIER_SLOT,
      timeZone->getFixedSlot(BuiltinTimeZoneObject::IDENTIFIER_SLOT));

  // Step 3.b or 4.c.
  object->setFixedSlot(
      TimeZoneObject::OFFSET_NANOSECONDS_SLOT,
      timeZone->getFixedSlot(BuiltinTimeZoneObject::OFFSET_NANOSECONDS_SLOT));

  // Step 5.
  return object;
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
BuiltinTimeZoneObject* js::temporal::CreateTemporalTimeZone(
    JSContext* cx, Handle<JSString*> identifier) {
  return ::CreateBuiltinTimeZone(cx, identifier);
}

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 *
 * When CreateTemporalTimeZone is called with `identifier="UTC"`.
 */
BuiltinTimeZoneObject* js::temporal::CreateTemporalTimeZoneUTC(JSContext* cx) {
  Handle<JSString*> identifier = cx->names().UTC.toHandle();
  return ::CreateBuiltinTimeZone(cx, identifier);
}

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool js::temporal::ToTemporalTimeZone(JSContext* cx, Handle<JSString*> string,
                                      MutableHandle<TimeZoneValue> result) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  Rooted<JSString*> timeZoneName(cx);
  int64_t offsetNanoseconds = 0;
  if (!ParseTemporalTimeZoneString(cx, string, &timeZoneName,
                                   &offsetNanoseconds)) {
    return false;
  }

  // Steps 4-5.
  if (timeZoneName) {
    // Steps 4.a-b. (Not applicable in our implementation.)

    // Steps 4.c-d.
    timeZoneName = ValidateAndCanonicalizeTimeZoneName(cx, timeZoneName);
    if (!timeZoneName) {
      return false;
    }

    // Steps 4.e and 5.
    auto* obj = ::CreateBuiltinTimeZone(cx, timeZoneName);
    if (!obj) {
      return false;
    }

    result.set(TimeZoneValue(obj));
    return true;
  }

  // Step 6.
  auto* obj = ::CreateBuiltinTimeZone(cx, offsetNanoseconds);
  if (!obj) {
    return false;
  }

  result.set(TimeZoneValue(obj));
  return true;
}

/**
 * ObjectImplementsTemporalTimeZoneProtocol ( object )
 */
static bool ObjectImplementsTemporalTimeZoneProtocol(JSContext* cx,
                                                     Handle<JSObject*> object,
                                                     bool* result) {
  // Step 1. (Not applicable in our implementation.)
  MOZ_ASSERT(!object->canUnwrapAs<TimeZoneObject>(),
             "TimeZone objects handled in the caller");

  // Step 2.
  for (auto key : {
           &JSAtomState::getOffsetNanosecondsFor,
           &JSAtomState::getPossibleInstantsFor,
           &JSAtomState::id,
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

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool js::temporal::ToTemporalTimeZone(JSContext* cx,
                                      Handle<Value> temporalTimeZoneLike,
                                      MutableHandle<TimeZoneValue> result) {
  // Step 1.
  Rooted<Value> timeZoneLike(cx, temporalTimeZoneLike);
  if (timeZoneLike.isObject()) {
    Rooted<JSObject*> obj(cx, &timeZoneLike.toObject());

    // Step 1.b. (Partial)
    if (obj->canUnwrapAs<TimeZoneObject>()) {
      result.set(TimeZoneValue(obj));
      return true;
    }

    // Step 1.a.
    if (auto* zonedDateTime = obj->maybeUnwrapIf<ZonedDateTimeObject>()) {
      result.set(zonedDateTime->timeZone());
      return result.wrap(cx);
    }

    // Step 1.b.
    bool implementsTimeZoneProtocol;
    if (!ObjectImplementsTemporalTimeZoneProtocol(
            cx, obj, &implementsTimeZoneProtocol)) {
      return false;
    }
    if (!implementsTimeZoneProtocol) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_TEMPORAL_INVALID_OBJECT,
                               "Temporal.TimeZone", obj->getClass()->name);
      return false;
    }

    // Step 1.c.
    result.set(TimeZoneValue(obj));
    return true;
  }

  // Step 2.
  Rooted<JSString*> identifier(cx, JS::ToString(cx, timeZoneLike));
  if (!identifier) {
    return false;
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
  if (!timeZoneLike.isString() && !timeZoneLike.isNumeric()) {
    ReportValueError(cx, JSMSG_TEMPORAL_TIMEZONE_PARSE_BAD_TYPE,
                     JSDVG_IGNORE_STACK, timeZoneLike, nullptr);
    return false;
  }

  // Steps 3-6.
  return ToTemporalTimeZone(cx, identifier, result);
}

/**
 * ToTemporalTimeZoneObject ( timeZoneSlotValue )
 */
JSObject* js::temporal::ToTemporalTimeZoneObject(
    JSContext* cx, Handle<TimeZoneValue> timeZone) {
  // Step 1.
  if (timeZone.isObject()) {
    return timeZone.toObject();
  }

  // Step 2.
  return CreateTemporalTimeZone(cx, timeZone.toString());
}

bool js::temporal::WrapTimeZoneValueObject(JSContext* cx,
                                           MutableHandle<JSObject*> timeZone) {
  // First handle the common case when |timeZone| is TimeZoneObjectMaybeBuiltin
  // from the current compartment.
  if (MOZ_LIKELY(timeZone->is<TimeZoneObjectMaybeBuiltin>() &&
                 timeZone->compartment() == cx->compartment())) {
    return true;
  }

  // If it's not a built-in time zone, simply wrap the object into the current
  // compartment.
  auto* unwrappedTimeZone = timeZone->maybeUnwrapIf<BuiltinTimeZoneObject>();
  if (!unwrappedTimeZone) {
    return cx->compartment()->wrap(cx, timeZone);
  }

  // If this is a built-in time zone from a different compartment, create a
  // fresh copy using the current compartment.
  //
  // We create a fresh copy, so we don't have to support the cross-compartment
  // case, which makes detection of "string" time zones easier.

  const auto& offsetNanoseconds = unwrappedTimeZone->offsetNanoseconds();
  if (offsetNanoseconds.isNumber()) {
    auto* obj =
        CreateBuiltinTimeZone(cx, int64_t(offsetNanoseconds.toNumber()));
    if (!obj) {
      return false;
    }

    timeZone.set(obj);
    return true;
  }

  Rooted<JSString*> identifier(cx, unwrappedTimeZone->identifier());
  if (!cx->compartment()->wrap(cx, &identifier)) {
    return false;
  }

  auto* obj = ::CreateBuiltinTimeZone(cx, identifier);
  if (!obj) {
    return false;
  }

  timeZone.set(obj);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getOffsetNanosecondsFor ( instant )
 */
static bool BuiltinGetOffsetNanosecondsFor(
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone,
    const Instant& instant, int64_t* offsetNanoseconds) {
  // Steps 1-3. (Not applicable.)

  // Step 4.
  if (timeZone->offsetNanoseconds().isNumber()) {
    double offset = timeZone->offsetNanoseconds().toNumber();
    MOZ_ASSERT(IsInteger(offset));
    MOZ_ASSERT(std::abs(offset) < ToNanoseconds(TemporalUnit::Day));

    *offsetNanoseconds = int64_t(offset);
    return true;
  }
  MOZ_ASSERT(timeZone->offsetNanoseconds().isUndefined());

  // Step 5.
  int64_t offset;
  if (!GetNamedTimeZoneOffsetNanoseconds(cx, timeZone, instant, &offset)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offset) < ToNanoseconds(TemporalUnit::Day));

  *offsetNanoseconds = offset;
  return true;
}

/**
 * GetOffsetNanosecondsFor ( timeZone, instant [ , getOffsetNanosecondsFor ] )
 */
static bool GetOffsetNanosecondsFor(JSContext* cx, Handle<JSObject*> timeZone,
                                    Handle<Wrapped<InstantObject*>> instant,
                                    Handle<Value> getOffsetNanosecondsFor,
                                    int64_t* offsetNanoseconds) {
  // Steps 1-2. (Not applicable)

  // Step 3.
  Rooted<Value> instantVal(cx, ObjectValue(*instant));
  Rooted<Value> rval(cx);
  if (!Call(cx, getOffsetNanosecondsFor, timeZone, instantVal, &rval)) {
    return false;
  }

  // Step 4.
  if (!rval.isNumber()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, rval,
                     nullptr, "not a number");
    return false;
  }

  // Steps 5-7.
  double num = rval.toNumber();
  if (!IsInteger(num) || std::abs(num) >= ToNanoseconds(TemporalUnit::Day)) {
    ToCStringBuf cbuf;
    const char* numStr = NumberToCString(&cbuf, num);

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_TIMEZONE_NANOS_RANGE, numStr);
    return false;
  }

  // Step 8.
  *offsetNanoseconds = int64_t(num);
  return true;
}

static bool TimeZone_getOffsetNanosecondsFor(JSContext* cx, unsigned argc,
                                             Value* vp);

/**
 * GetOffsetNanosecondsFor ( timeZone, instant [ , getOffsetNanosecondsFor ] )
 */
bool js::temporal::GetOffsetNanosecondsFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<InstantObject*>> instant, int64_t* offsetNanoseconds) {
  // Step 1.
  if (timeZone.isString()) {
    auto* unwrapped = instant.unwrap(cx);
    if (!unwrapped) {
      return false;
    }

    return BuiltinGetOffsetNanosecondsFor(
        cx, timeZone.toString(), ToInstant(unwrapped), offsetNanoseconds);
  }

  // Step 2.
  Rooted<JSObject*> timeZoneObj(cx, timeZone.toObject());
  Rooted<Value> getOffsetNanosecondsFor(cx);
  if (!GetMethodForCall(cx, timeZoneObj, cx->names().getOffsetNanosecondsFor,
                        &getOffsetNanosecondsFor)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (timeZoneObj->is<TimeZoneObject>() &&
      IsNativeFunction(getOffsetNanosecondsFor,
                       TimeZone_getOffsetNanosecondsFor)) {
    auto* unwrapped = instant.unwrap(cx);
    if (!unwrapped) {
      return false;
    }

    return BuiltinGetOffsetNanosecondsFor(cx, timeZoneObj.as<TimeZoneObject>(),
                                          ToInstant(unwrapped),
                                          offsetNanoseconds);
  }

  // Steps 3-8.
  return ::GetOffsetNanosecondsFor(cx, timeZoneObj, instant,
                                   getOffsetNanosecondsFor, offsetNanoseconds);
}

/**
 * GetOffsetNanosecondsFor ( timeZone, instant [ , getOffsetNanosecondsFor ] )
 */
bool js::temporal::GetOffsetNanosecondsFor(JSContext* cx,
                                           Handle<TimeZoneValue> timeZone,
                                           const Instant& instant,
                                           int64_t* offsetNanoseconds) {
  // Step 1.
  if (timeZone.isString()) {
    return BuiltinGetOffsetNanosecondsFor(cx, timeZone.toString(), instant,
                                          offsetNanoseconds);
  }

  // Step 2.
  Rooted<JSObject*> timeZoneObj(cx, timeZone.toObject());
  Rooted<Value> getOffsetNanosecondsFor(cx);
  if (!GetMethodForCall(cx, timeZoneObj, cx->names().getOffsetNanosecondsFor,
                        &getOffsetNanosecondsFor)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (timeZoneObj->is<TimeZoneObject>() &&
      IsNativeFunction(getOffsetNanosecondsFor,
                       TimeZone_getOffsetNanosecondsFor)) {
    return BuiltinGetOffsetNanosecondsFor(cx, timeZoneObj.as<TimeZoneObject>(),
                                          instant, offsetNanoseconds);
  }

  // Steps 3-8.
  Rooted<InstantObject*> obj(cx, CreateTemporalInstant(cx, instant));
  if (!obj) {
    return false;
  }
  return ::GetOffsetNanosecondsFor(cx, timeZoneObj, obj,
                                   getOffsetNanosecondsFor, offsetNanoseconds);
}

/**
 * GetOffsetStringFor ( timeZone, instant )
 */
JSString* js::temporal::GetOffsetStringFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<InstantObject*>> instant) {
  // Step 1.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNanoseconds)) {
    return nullptr;
  }
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // Step 2.
  return FormatTimeZoneOffsetString(cx, offsetNanoseconds);
}

// ES2019 draft rev 0ceb728a1adbffe42b26972a6541fd7f398b1557
// 5.2.5 Mathematical Operations
static inline double PositiveModulo(double dividend, double divisor) {
  MOZ_ASSERT(divisor > 0);
  MOZ_ASSERT(std::isfinite(divisor));

  double result = std::fmod(dividend, divisor);
  if (result < 0) {
    result += divisor;
  }
  return result + (+0.0);
}

/* ES5 15.9.1.10. */
static double HourFromTime(double t) {
  return PositiveModulo(std::floor(t / msPerHour), HoursPerDay);
}

static double MinFromTime(double t) {
  return PositiveModulo(std::floor(t / msPerMinute), MinutesPerHour);
}

static double SecFromTime(double t) {
  return PositiveModulo(std::floor(t / msPerSecond), SecondsPerMinute);
}

static double msFromTime(double t) { return PositiveModulo(t, msPerSecond); }

/**
 * GetISOPartsFromEpoch ( epochNanoseconds )
 */
static PlainDateTime GetISOPartsFromEpoch(const Instant& instant) {
  // TODO: YearFromTime/MonthFromTime/DayFromTime recompute the same values
  // multiple times. Consider adding a new function avoids this.

  // Step 1.
  MOZ_ASSERT(IsValidEpochInstant(instant));

  // Step 2.
  int32_t remainderNs = instant.nanoseconds % 1'000'000;

  // Step 3.
  int64_t epochMilliseconds = instant.floorToMilliseconds();

  // Step 4.
  int32_t year = JS::YearFromTime(epochMilliseconds);

  // Step 5.
  int32_t month = JS::MonthFromTime(epochMilliseconds) + 1;

  // Step 6.
  int32_t day = JS::DayFromTime(epochMilliseconds);

  // Step 7.
  int32_t hour = HourFromTime(epochMilliseconds);

  // Step 8.
  int32_t minute = MinFromTime(epochMilliseconds);

  // Step 9.
  int32_t second = SecFromTime(epochMilliseconds);

  // Step 10.
  int32_t millisecond = msFromTime(epochMilliseconds);

  // Step 11.
  int32_t microsecond = remainderNs / 1000;

  // Step 12.
  int32_t nanosecond = remainderNs % 1000;

  // Step 13.
  PlainDateTime result = {
      {year, month, day},
      {hour, minute, second, millisecond, microsecond, nanosecond}};

  // Always valid when the epoch nanoseconds are within the representable limit.
  MOZ_ASSERT(IsValidISODateTime(result));
  MOZ_ASSERT(ISODateTimeWithinLimits(result));

  return result;
}

/**
 * BalanceISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
static PlainDateTime BalanceISODateTime(const PlainDateTime& dateTime,
                                        int64_t nanoseconds) {
  MOZ_ASSERT(IsValidISODateTime(dateTime));
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));
  MOZ_ASSERT(std::abs(nanoseconds) < ToNanoseconds(TemporalUnit::Day));

  auto& [date, time] = dateTime;

  // Step 1. (Not applicable in our implementation.)

  // Step 2.
  auto balancedTime = BalanceTime(time, nanoseconds);
  MOZ_ASSERT(-1 <= balancedTime.days && balancedTime.days <= 1);

  // Step 3.
  auto balancedDate =
      BalanceISODate(date.year, date.month, date.day + balancedTime.days);

  // Step 4.
  return {balancedDate, balancedTime.time};
}

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
static bool GetPlainDateTimeFor(JSContext* cx, Handle<TimeZoneValue> timeZone,
                                Handle<Wrapped<InstantObject*>> instant,
                                PlainDateTime* result) {
  // Step 1.
  int64_t offsetNanoseconds;
  if (!GetOffsetNanosecondsFor(cx, timeZone, instant, &offsetNanoseconds)) {
    return false;
  }
  MOZ_ASSERT(std::abs(offsetNanoseconds) < ToNanoseconds(TemporalUnit::Day));

  // TODO: Steps 2-3 can be combined into a single operation to improve perf.

  // Step 2.
  auto* unwrappedInstant = instant.unwrap(cx);
  if (!unwrappedInstant) {
    return false;
  }
  PlainDateTime dateTime = GetISOPartsFromEpoch(ToInstant(unwrappedInstant));

  // Step 3.
  auto balanced = BalanceISODateTime(dateTime, offsetNanoseconds);

  // FIXME: spec issue - CreateTemporalDateTime is infallible
  // https://github.com/tc39/proposal-temporal/issues/2523
  MOZ_ASSERT(ISODateTimeWithinLimits(balanced));

  // Step 4.
  *result = balanced;
  return true;
}

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
static PlainDateTimeObject* GetPlainDateTimeFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<InstantObject*>> instant, Handle<CalendarValue> calendar) {
  PlainDateTime dateTime;
  if (!GetPlainDateTimeFor(cx, timeZone, instant, &dateTime)) {
    return nullptr;
  }

  // FIXME: spec issue - CreateTemporalDateTime is infallible
  // https://github.com/tc39/proposal-temporal/issues/2523
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  return CreateTemporalDateTime(cx, dateTime, calendar);
}

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
PlainDateTimeObject* js::temporal::GetPlainDateTimeFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone, const Instant& instant,
    Handle<CalendarValue> calendar) {
  MOZ_ASSERT(IsValidEpochInstant(instant));

  Rooted<InstantObject*> obj(cx, CreateTemporalInstant(cx, instant));
  if (!obj) {
    return nullptr;
  }
  return ::GetPlainDateTimeFor(cx, timeZone, obj, calendar);
}

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
bool js::temporal::GetPlainDateTimeFor(JSContext* cx,
                                       Handle<TimeZoneValue> timeZone,
                                       Handle<InstantObject*> instant,
                                       PlainDateTime* result) {
  return ::GetPlainDateTimeFor(cx, timeZone, instant, result);
}

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
bool js::temporal::GetPlainDateTimeFor(JSContext* cx,
                                       Handle<TimeZoneValue> timeZone,
                                       const Instant& instant,
                                       PlainDateTime* result) {
  MOZ_ASSERT(IsValidEpochInstant(instant));

  Rooted<InstantObject*> obj(cx, CreateTemporalInstant(cx, instant));
  if (!obj) {
    return false;
  }
  return ::GetPlainDateTimeFor(cx, timeZone, obj, result);
}

/**
 * Temporal.TimeZone.prototype.getPossibleInstantsFor ( dateTime )
 */
static bool BuiltinGetPossibleInstantsFor(
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone,
    const PlainDateTime& dateTime, EpochInstantList& possibleInstants) {
  MOZ_ASSERT(possibleInstants.length() == 0);

  // Steps 1-3. (Not applicable)

  // Step 4.
  if (timeZone->offsetNanoseconds().isNumber()) {
    double offsetNs = timeZone->offsetNanoseconds().toNumber();
    MOZ_ASSERT(IsInteger(offsetNs));
    MOZ_ASSERT(std::abs(offsetNs) < ToNanoseconds(TemporalUnit::Day));

    // Step 4.a.
    auto epochInstant = GetUTCEpochNanoseconds(dateTime);

    // Step 4.b.
    possibleInstants.append(epochInstant -
                            InstantSpan::fromNanoseconds(offsetNs));
  } else {
    // Step 5.
    if (!GetNamedTimeZoneEpochNanoseconds(cx, timeZone, dateTime,
                                          possibleInstants)) {
      return false;
    }
  }

  MOZ_ASSERT(possibleInstants.length() <= 2);

  // Step 7.b.
  for (const auto& epochInstant : possibleInstants) {
    if (!IsValidEpochInstant(epochInstant)) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_INSTANT_INVALID);
      return false;
    }
  }

  // Steps 6-8. (Handled in the caller).
  return true;
}

static bool BuiltinGetPossibleInstantsFor(
    JSContext* cx, Handle<TimeZoneObjectMaybeBuiltin*> timeZone,
    Handle<Wrapped<PlainDateTimeObject*>> dateTime,
    MutableHandle<InstantVector> list) {
  auto* unwrapped = dateTime.unwrap(cx);
  if (!unwrapped) {
    return false;
  }

  // Temporal.TimeZone.prototype.getInstantFor, step 4.
  EpochInstantList possibleInstants;
  if (!BuiltinGetPossibleInstantsFor(cx, timeZone, ToPlainDateTime(unwrapped),
                                     possibleInstants)) {
    return false;
  }

  // Temporal.TimeZone.prototype.getInstantFor, step 7.
  for (const auto& possibleInstant : possibleInstants) {
    auto* instant = CreateTemporalInstant(cx, possibleInstant);
    if (!instant) {
      return false;
    }

    if (!list.append(instant)) {
      return false;
    }
  }
  return true;
}

static bool TimeZone_getPossibleInstantsFor(JSContext* cx, unsigned argc,
                                            Value* vp);

/**
 * GetPossibleInstantsFor ( timeZone, dateTime [ , getPossibleInstantsFor ] )
 */
bool js::temporal::GetPossibleInstantsFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<PlainDateTimeObject*>> dateTime,
    MutableHandle<InstantVector> list) {
  // Step 1.
  if (timeZone.isString()) {
    return BuiltinGetPossibleInstantsFor(cx, timeZone.toString(), dateTime,
                                         list);
  }

  // Step 2.
  Rooted<JSObject*> timeZoneObj(cx, timeZone.toObject());
  Rooted<Value> getPossibleInstantsFor(cx);
  if (!GetMethodForCall(cx, timeZoneObj, cx->names().getPossibleInstantsFor,
                        &getPossibleInstantsFor)) {
    return false;
  }

  // Fast-path for the default implementation.
  if (timeZoneObj->is<TimeZoneObject>() &&
      IsNativeFunction(getPossibleInstantsFor,
                       TimeZone_getPossibleInstantsFor)) {
    ForOfPIC::Chain* stubChain = ForOfPIC::getOrCreate(cx);
    if (!stubChain) {
      return false;
    }

    bool arrayIterationSane;
    if (!stubChain->tryOptimizeArray(cx, &arrayIterationSane)) {
      return false;
    }

    if (arrayIterationSane) {
      return BuiltinGetPossibleInstantsFor(cx, timeZoneObj.as<TimeZoneObject>(),
                                           dateTime, list);
    }
  }

  // FIXME: spec issue - should there be a requirement that the list is sorted?
  // FIXME: spec issue - are duplicates allowed?
  // https://github.com/tc39/proposal-temporal/issues/2532

  // Step 3.
  Rooted<Value> thisv(cx, ObjectValue(*timeZoneObj));
  Rooted<Value> arg(cx, ObjectValue(*dateTime));
  Rooted<Value> rval(cx);
  if (!Call(cx, getPossibleInstantsFor, thisv, arg, &rval)) {
    return false;
  }

  // Step 4.
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(rval)) {
    return false;
  }

  // Step 5. (Not applicable in our implementation.)

  // Steps 6-7.
  Rooted<Value> nextValue(cx);
  while (true) {
    bool done;
    if (!iterator.next(&nextValue, &done)) {
      return false;
    }
    if (done) {
      break;
    }

    if (nextValue.isObject()) {
      JSObject* obj = &nextValue.toObject();
      if (obj->canUnwrapAs<InstantObject>()) {
        if (!list.append(obj)) {
          return false;
        }
        continue;
      }
    }

    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_IGNORE_STACK, nextValue,
                     nullptr, "not an instant");

    iterator.closeThrow();
    return false;
  }

  // Step 8.
  return true;
}

/**
 * AddDateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond, calendar, years, months, weeks, days, hours,
 * minutes, seconds, milliseconds, microseconds, nanoseconds, options )
 */
static bool AddDateTime(JSContext* cx, const PlainDateTime& dateTime,
                        int64_t nanoseconds, Handle<CalendarValue> calendar,
                        PlainDateTime* result) {
  MOZ_ASSERT(std::abs(nanoseconds) <= 2 * ToNanoseconds(TemporalUnit::Day));

  auto& [date, time] = dateTime;

  // Step 1.
  MOZ_ASSERT(IsValidISODateTime(dateTime));
  MOZ_ASSERT(ISODateTimeWithinLimits(dateTime));

  // Step 2. (Inlined call to AddTime)
  auto timeResult = BalanceTime(time, nanoseconds);

  // Step 3.
  const auto& datePart = date;

  // Step 4.
  Duration dateDuration = {0, 0, 0, double(timeResult.days)};

  // Step 5.
  PlainDate addedDate;
  if (!CalendarDateAdd(cx, calendar, datePart, dateDuration, &addedDate)) {
    return false;
  }

  // Step 6.
  *result = {addedDate, timeResult.time};
  return true;
}

/**
 * DisambiguatePossibleInstants ( possibleInstants, timeZone, dateTime,
 * disambiguation )
 */
Wrapped<InstantObject*> js::temporal::DisambiguatePossibleInstants(
    JSContext* cx, Handle<InstantVector> possibleInstants,
    Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<PlainDateTimeObject*>> dateTimeObj,
    TemporalDisambiguation disambiguation) {
  // Step 1. (Not applicable)

  // Steps 2-3.
  if (possibleInstants.length() == 1) {
    return possibleInstants[0];
  }

  // Steps 4-5.
  if (!possibleInstants.empty()) {
    // Step 4.a.
    if (disambiguation == TemporalDisambiguation::Earlier ||
        disambiguation == TemporalDisambiguation::Compatible) {
      return possibleInstants[0];
    }

    // Step 4.b.
    if (disambiguation == TemporalDisambiguation::Later) {
      size_t last = possibleInstants.length() - 1;
      return possibleInstants[last];
    }

    // Step 4.c.
    MOZ_ASSERT(disambiguation == TemporalDisambiguation::Reject);

    // Step 4.d.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_TIMEZONE_INSTANT_AMBIGUOUS);
    return nullptr;
  }

  // Step 6.
  if (disambiguation == TemporalDisambiguation::Reject) {
    // TODO: Improve error message to say the date was skipped.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_TIMEZONE_INSTANT_AMBIGUOUS);
    return nullptr;
  }

  constexpr auto oneDay =
      InstantSpan::fromNanoseconds(ToNanoseconds(TemporalUnit::Day));

  auto* unwrappedDateTime = dateTimeObj.unwrap(cx);
  if (!unwrappedDateTime) {
    return nullptr;
  }

  auto dateTime = ToPlainDateTime(unwrappedDateTime);
  Rooted<CalendarValue> calendar(cx, unwrappedDateTime->calendar());
  if (!calendar.wrap(cx)) {
    return nullptr;
  }

  // Step 7.
  auto epochNanoseconds = GetUTCEpochNanoseconds(dateTime);

  // Steps 8 and 10.
  auto dayBefore = epochNanoseconds - oneDay;

  // Step 9.
  if (!IsValidEpochInstant(dayBefore)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INSTANT_INVALID);
    return nullptr;
  }

  // Step 11 and 13.
  auto dayAfter = epochNanoseconds + oneDay;

  // Step 12.
  if (!IsValidEpochInstant(dayAfter)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_INSTANT_INVALID);
    return nullptr;
  }

  // Step 14.
  int64_t offsetBefore;
  if (!GetOffsetNanosecondsFor(cx, timeZone, dayBefore, &offsetBefore)) {
    return nullptr;
  }
  MOZ_ASSERT(std::abs(offsetBefore) < ToNanoseconds(TemporalUnit::Day));

  // Step 15.
  int64_t offsetAfter;
  if (!GetOffsetNanosecondsFor(cx, timeZone, dayAfter, &offsetAfter)) {
    return nullptr;
  }
  MOZ_ASSERT(std::abs(offsetAfter) < ToNanoseconds(TemporalUnit::Day));

  // Step 16.
  int64_t nanoseconds = offsetAfter - offsetBefore;

  // Step 17.
  if (disambiguation == TemporalDisambiguation::Earlier) {
    // Step 17.a.
    PlainDateTime earlier;
    if (!AddDateTime(cx, dateTime, -nanoseconds, calendar, &earlier)) {
      return nullptr;
    }

    // Step 17.b.
    Rooted<PlainDateTimeObject*> earlierDateTime(
        cx, CreateTemporalDateTime(cx, earlier, calendar));
    if (!earlierDateTime) {
      return nullptr;
    }

    // Step 17.c.
    Rooted<InstantVector> earlierInstants(cx, InstantVector(cx));
    if (!GetPossibleInstantsFor(cx, timeZone, earlierDateTime,
                                &earlierInstants)) {
      return nullptr;
    }

    // Step 17.d.
    if (earlierInstants.empty()) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_TEMPORAL_TIMEZONE_INSTANT_AMBIGUOUS);
      return nullptr;
    }

    // Step 17.d.
    return earlierInstants[0];
  }

  // Step 18.
  MOZ_ASSERT(disambiguation == TemporalDisambiguation::Compatible ||
             disambiguation == TemporalDisambiguation::Later);

  // Step 19.
  PlainDateTime later;
  if (!AddDateTime(cx, dateTime, nanoseconds, calendar, &later)) {
    return nullptr;
  }

  // Step 20.
  Rooted<PlainDateTimeObject*> laterDateTime(
      cx, CreateTemporalDateTime(cx, later, calendar));
  if (!laterDateTime) {
    return nullptr;
  }

  // Step 21.
  Rooted<InstantVector> laterInstants(cx, InstantVector(cx));
  if (!GetPossibleInstantsFor(cx, timeZone, laterDateTime, &laterInstants)) {
    return nullptr;
  }

  // Steps 22-23.
  if (laterInstants.empty()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TEMPORAL_TIMEZONE_INSTANT_AMBIGUOUS);
    return nullptr;
  }

  // Step 24.
  size_t last = laterInstants.length() - 1;
  return laterInstants[last];
}

/**
 * GetInstantFor ( timeZone, dateTime, disambiguation )
 */
static Wrapped<InstantObject*> GetInstantFor(
    JSContext* cx, Handle<TimeZoneValue> timeZone,
    Handle<Wrapped<PlainDateTimeObject*>> dateTime,
    TemporalDisambiguation disambiguation) {
  // Step 1.
  Rooted<InstantVector> possibleInstants(cx, InstantVector(cx));
  if (!GetPossibleInstantsFor(cx, timeZone, dateTime, &possibleInstants)) {
    return nullptr;
  }

  // Step 2.
  return DisambiguatePossibleInstants(cx, possibleInstants, timeZone, dateTime,
                                      disambiguation);
}

/**
 * GetInstantFor ( timeZone, dateTime, disambiguation )
 */
bool js::temporal::GetInstantFor(JSContext* cx, Handle<TimeZoneValue> timeZone,
                                 Handle<Wrapped<PlainDateTimeObject*>> dateTime,
                                 TemporalDisambiguation disambiguation,
                                 Instant* result) {
  auto instant = ::GetInstantFor(cx, timeZone, dateTime, disambiguation);
  if (!instant) {
    return false;
  }
  *result = ToInstant(&instant.unwrap());
  return true;
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
  Rooted<TimeZoneValue> timeZone(cx);
  if (!ToTemporalTimeZone(cx, args.get(0), &timeZone)) {
    return false;
  }

  // Step 2.
  auto* obj = ToTemporalTimeZoneObject(cx, timeZone);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getOffsetNanosecondsFor ( instant )
 */
static bool TimeZone_getOffsetNanosecondsFor(JSContext* cx,
                                             const CallArgs& args) {
  Rooted<TimeZoneObject*> timeZone(
      cx, &args.thisv().toObject().as<TimeZoneObject>());

  // Step 3.
  Instant instant;
  if (!ToTemporalInstantEpochInstant(cx, args.get(0), &instant)) {
    return false;
  }

  // Steps 4-5.
  int64_t offset;
  if (!BuiltinGetOffsetNanosecondsFor(cx, timeZone, instant, &offset)) {
    return false;
  }

  args.rval().setNumber(offset);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getOffsetNanosecondsFor ( instant )
 */
static bool TimeZone_getOffsetNanosecondsFor(JSContext* cx, unsigned argc,
                                             Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getOffsetNanosecondsFor>(
      cx, args);
}

/**
 * Temporal.TimeZone.prototype.getOffsetStringFor ( instant )
 */
static bool TimeZone_getOffsetStringFor(JSContext* cx, const CallArgs& args) {
  Rooted<TimeZoneValue> timeZone(cx, &args.thisv().toObject());

  // Step 3.
  Rooted<Wrapped<InstantObject*>> instant(cx,
                                          ToTemporalInstant(cx, args.get(0)));
  if (!instant) {
    return false;
  }

  // Step 4.
  JSString* str = GetOffsetStringFor(cx, timeZone, instant);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getOffsetStringFor ( instant )
 */
static bool TimeZone_getOffsetStringFor(JSContext* cx, unsigned argc,
                                        Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getOffsetStringFor>(cx,
                                                                       args);
}

/**
 * Temporal.TimeZone.prototype.getPlainDateTimeFor ( instant [, calendarLike ] )
 */
static bool TimeZone_getPlainDateTimeFor(JSContext* cx, const CallArgs& args) {
  Rooted<TimeZoneValue> timeZone(cx, &args.thisv().toObject());

  // Step 3.
  Rooted<Wrapped<InstantObject*>> instant(cx,
                                          ToTemporalInstant(cx, args.get(0)));
  if (!instant) {
    return false;
  }

  // Step 4.
  Rooted<CalendarValue> calendar(cx);
  if (!ToTemporalCalendarWithISODefault(cx, args.get(1), &calendar)) {
    return false;
  }

  // Step 5.
  auto* result = GetPlainDateTimeFor(cx, timeZone, instant, calendar);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getPlainDateTimeFor ( instant [, calendarLike ] )
 */
static bool TimeZone_getPlainDateTimeFor(JSContext* cx, unsigned argc,
                                         Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getPlainDateTimeFor>(cx,
                                                                        args);
}

/**
 * Temporal.TimeZone.prototype.getInstantFor ( dateTime [ , options ] )
 */
static bool TimeZone_getInstantFor(JSContext* cx, const CallArgs& args) {
  Rooted<TimeZoneValue> timeZone(cx, &args.thisv().toObject());

  // Step 3.
  Rooted<Wrapped<PlainDateTimeObject*>> dateTime(
      cx, ToTemporalDateTime(cx, args.get(0)));
  if (!dateTime) {
    return false;
  }

  auto disambiguation = TemporalDisambiguation::Compatible;
  if (args.hasDefined(1)) {
    // Step 4.
    Rooted<JSObject*> options(
        cx, RequireObjectArg(cx, "options", "getInstantFor", args[1]));
    if (!options) {
      return false;
    }

    // Step 5.
    if (!ToTemporalDisambiguation(cx, options, &disambiguation)) {
      return false;
    }
  }

  auto result = GetInstantFor(cx, timeZone, dateTime, disambiguation);
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getInstantFor ( dateTime [ , options ] )
 */
static bool TimeZone_getInstantFor(JSContext* cx, unsigned argc, Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getInstantFor>(cx, args);
}

/**
 * Temporal.TimeZone.prototype.getPossibleInstantsFor ( dateTime )
 */
static bool TimeZone_getPossibleInstantsFor(JSContext* cx,
                                            const CallArgs& args) {
  Rooted<TimeZoneObject*> timeZone(
      cx, &args.thisv().toObject().as<TimeZoneObject>());

  // Step 3.
  PlainDateTime dateTime;
  if (!ToTemporalDateTime(cx, args.get(0), &dateTime)) {
    return false;
  }

  // Steps 4-5.
  EpochInstantList possibleInstants;
  if (!BuiltinGetPossibleInstantsFor(cx, timeZone, dateTime,
                                     possibleInstants)) {
    return false;
  }

  // Step 6.
  size_t length = possibleInstants.length();
  Rooted<ArrayObject*> result(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!result) {
    return false;
  }
  result->ensureDenseInitializedLength(0, length);

  // Step 7.
  for (size_t i = 0; i < length; i++) {
    // Step 7.a. (Already performed in step 4 in our implementation.)
    MOZ_ASSERT(IsValidEpochInstant(possibleInstants[i]));

    // Step 7.b.
    auto* instant = CreateTemporalInstant(cx, possibleInstants[i]);
    if (!instant) {
      return false;
    }

    // Step 7.c.
    result->initDenseElement(i, ObjectValue(*instant));
  }

  // Step 8.
  args.rval().setObject(*result);
  return true;
}

/**
 * Temporal.TimeZone.prototype.getPossibleInstantsFor ( dateTime )
 */
static bool TimeZone_getPossibleInstantsFor(JSContext* cx, unsigned argc,
                                            Value* vp) {
  // Steps 1-2.
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTimeZone, TimeZone_getPossibleInstantsFor>(
      cx, args);
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

  // Steps 3-4.
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
  auto* timeZone = &args.thisv().toObject().as<TimeZoneObject>();

  // Steps 3-4.
  args.rval().setString(timeZone->identifier());
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

  // Steps 3-4.
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

void js::temporal::TimeZoneObjectMaybeBuiltin::finalize(JS::GCContext* gcx,
                                                        JSObject* obj) {
  MOZ_ASSERT(gcx->onMainThread());

  if (auto* timeZone = obj->as<TimeZoneObjectMaybeBuiltin>().getTimeZone()) {
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
    JS_FN("getOffsetNanosecondsFor", TimeZone_getOffsetNanosecondsFor, 1, 0),
    JS_FN("getOffsetStringFor", TimeZone_getOffsetStringFor, 1, 0),
    JS_FN("getPlainDateTimeFor", TimeZone_getPlainDateTimeFor, 1, 0),
    JS_FN("getInstantFor", TimeZone_getInstantFor, 1, 0),
    JS_FN("getPossibleInstantsFor", TimeZone_getPossibleInstantsFor, 1, 0),
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

const JSClassOps BuiltinTimeZoneObject::classOps_ = {
    nullptr,                          // addProperty
    nullptr,                          // delProperty
    nullptr,                          // enumerate
    nullptr,                          // newEnumerate
    nullptr,                          // resolve
    nullptr,                          // mayResolve
    BuiltinTimeZoneObject::finalize,  // finalize
    nullptr,                          // call
    nullptr,                          // construct
    nullptr,                          // trace
};

const JSClass BuiltinTimeZoneObject::class_ = {
    "Temporal.BuiltinTimeZone",
    JSCLASS_HAS_RESERVED_SLOTS(BuiltinTimeZoneObject::SLOT_COUNT) |
        JSCLASS_FOREGROUND_FINALIZE,
    &BuiltinTimeZoneObject::classOps_,
};
