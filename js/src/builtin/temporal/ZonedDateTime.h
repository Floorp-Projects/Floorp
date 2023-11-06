/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_ZonedDateTime_h
#define builtin_temporal_ZonedDateTime_h

#include "mozilla/Assertions.h"

#include <stdint.h>

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/TimeZone.h"
#include "builtin/temporal/Wrapped.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class ZonedDateTimeObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t SECONDS_SLOT = 0;
  static constexpr uint32_t NANOSECONDS_SLOT = 1;
  static constexpr uint32_t TIMEZONE_SLOT = 2;
  static constexpr uint32_t CALENDAR_SLOT = 3;
  static constexpr uint32_t SLOT_COUNT = 4;

  int64_t seconds() const {
    double seconds = getFixedSlot(SECONDS_SLOT).toNumber();
    MOZ_ASSERT(-8'640'000'000'000 <= seconds && seconds <= 8'640'000'000'000);
    return int64_t(seconds);
  }

  int32_t nanoseconds() const {
    int32_t nanoseconds = getFixedSlot(NANOSECONDS_SLOT).toInt32();
    MOZ_ASSERT(0 <= nanoseconds && nanoseconds <= 999'999'999);
    return nanoseconds;
  }

  TimeZoneValue timeZone() const {
    return TimeZoneValue(getFixedSlot(TIMEZONE_SLOT));
  }

  CalendarValue calendar() const {
    return CalendarValue(getFixedSlot(CALENDAR_SLOT));
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the instant fields from the ZonedDateTime object.
 */
inline Instant ToInstant(const ZonedDateTimeObject* zonedDateTime) {
  return {zonedDateTime->seconds(), zonedDateTime->nanoseconds()};
}

enum class TemporalDisambiguation;
enum class TemporalOffset;
enum class TemporalUnit;

/**
 * CreateTemporalZonedDateTime ( epochNanoseconds, timeZone, calendar [ ,
 * newTarget ] )
 */
ZonedDateTimeObject* CreateTemporalZonedDateTime(
    JSContext* cx, const Instant& instant, JS::Handle<TimeZoneValue> timeZone,
    JS::Handle<CalendarValue> calendar);

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds
 * [ , precalculatedPlainDateTime [ , options ] ] )
 */
bool AddZonedDateTime(JSContext* cx, const Instant& epochInstant,
                      JS::Handle<TimeZoneValue> timeZone,
                      JS::Handle<CalendarValue> calendar,
                      const Duration& duration, Instant* result);

/**
 * AddZonedDateTime ( epochNanoseconds, timeZone, calendar, years, months,
 * weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds
 * [ , precalculatedPlainDateTime [ , options ] ] )
 */
bool AddZonedDateTime(JSContext* cx, const Instant& epochInstant,
                      JS::Handle<TimeZoneValue> timeZone,
                      JS::Handle<CalendarValue> calendar,
                      const Duration& duration, const PlainDateTime& dateTime,
                      Instant* result);

/**
 * DifferenceZonedDateTime ( ns1, ns2, timeZone, calendar, largestUnit, options
 * )
 */
bool DifferenceZonedDateTime(JSContext* cx, const Instant& ns1,
                             const Instant& ns2,
                             JS::Handle<TimeZoneValue> timeZone,
                             JS::Handle<CalendarValue> calendar,
                             TemporalUnit largestUnit, Duration* result);

struct NanosecondsAndDays final {
  JS::BigInt* days = nullptr;
  int64_t daysInt = 0;
  InstantSpan nanoseconds;
  InstantSpan dayLength;

  double daysNumber() const;

  void trace(JSTracer* trc);

  static NanosecondsAndDays from(int64_t days, const InstantSpan& nanoseconds,
                                 const InstantSpan& dayLength) {
    return {nullptr, days, nanoseconds, dayLength};
  }

  static NanosecondsAndDays from(JS::BigInt* days,
                                 const InstantSpan& nanoseconds,
                                 const InstantSpan& dayLength) {
    return {days, 0, nanoseconds, dayLength};
  }
};

/**
 * NanosecondsToDays ( nanoseconds, zonedRelativeTo )
 */
bool NanosecondsToDays(
    JSContext* cx, const InstantSpan& nanoseconds,
    JS::Handle<Wrapped<ZonedDateTimeObject*>> zonedRelativeTo,
    JS::MutableHandle<NanosecondsAndDays> result);

enum class OffsetBehaviour { Option, Exact, Wall };

enum class MatchBehaviour { MatchExactly, MatchMinutes };

/**
 * InterpretISODateTimeOffset ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond, offsetBehaviour, offsetNanoseconds,
 * timeZone, disambiguation, offsetOption, matchBehaviour )
 */
bool InterpretISODateTimeOffset(JSContext* cx, const PlainDateTime& dateTime,
                                OffsetBehaviour offsetBehaviour,
                                int64_t offsetNanoseconds,
                                JS::Handle<TimeZoneValue> timeZone,
                                TemporalDisambiguation disambiguation,
                                TemporalOffset offsetOption,
                                MatchBehaviour matchBehaviour, Instant* result);

} /* namespace js::temporal */

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::NanosecondsAndDays, Wrapper> {
  const auto& object() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  double daysNumber() const { return object().daysNumber(); }

  JS::Handle<JS::BigInt*> days() const {
    return JS::Handle<JS::BigInt*>::fromMarkedLocation(&object().days);
  }

  int64_t daysInt() const { return object().daysInt; }

  temporal::InstantSpan nanoseconds() const { return object().nanoseconds; }

  temporal::InstantSpan dayLength() const { return object().dayLength; }
};

} /* namespace js */

#endif /* builtin_temporal_ZonedDateTime_h */
