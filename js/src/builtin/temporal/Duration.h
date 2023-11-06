/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Duration_h
#define builtin_temporal_Duration_h

#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/Wrapped.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class DurationObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t YEARS_SLOT = 0;
  static constexpr uint32_t MONTHS_SLOT = 1;
  static constexpr uint32_t WEEKS_SLOT = 2;
  static constexpr uint32_t DAYS_SLOT = 3;
  static constexpr uint32_t HOURS_SLOT = 4;
  static constexpr uint32_t MINUTES_SLOT = 5;
  static constexpr uint32_t SECONDS_SLOT = 6;
  static constexpr uint32_t MILLISECONDS_SLOT = 7;
  static constexpr uint32_t MICROSECONDS_SLOT = 8;
  static constexpr uint32_t NANOSECONDS_SLOT = 9;
  static constexpr uint32_t SLOT_COUNT = 10;

  double years() const { return getFixedSlot(YEARS_SLOT).toNumber(); }
  double months() const { return getFixedSlot(MONTHS_SLOT).toNumber(); }
  double weeks() const { return getFixedSlot(WEEKS_SLOT).toNumber(); }
  double days() const { return getFixedSlot(DAYS_SLOT).toNumber(); }
  double hours() const { return getFixedSlot(HOURS_SLOT).toNumber(); }
  double minutes() const { return getFixedSlot(MINUTES_SLOT).toNumber(); }
  double seconds() const { return getFixedSlot(SECONDS_SLOT).toNumber(); }
  double milliseconds() const {
    return getFixedSlot(MILLISECONDS_SLOT).toNumber();
  }
  double microseconds() const {
    return getFixedSlot(MICROSECONDS_SLOT).toNumber();
  }
  double nanoseconds() const {
    return getFixedSlot(NANOSECONDS_SLOT).toNumber();
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the duration fields from the Duration object.
 */
inline Duration ToDuration(const DurationObject* duration) {
  return {
      duration->years(),        duration->months(),
      duration->weeks(),        duration->days(),
      duration->hours(),        duration->minutes(),
      duration->seconds(),      duration->milliseconds(),
      duration->microseconds(), duration->nanoseconds(),
  };
}

class Increment;
class PlainDateObject;
class ZonedDateTimeObject;
enum class TemporalRoundingMode;
enum class TemporalUnit;

/**
 * DurationSign ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
int32_t DurationSign(const Duration& duration);

/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
bool IsValidDuration(const Duration& duration);

/**
 * IsValidDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds )
 */
bool ThrowIfInvalidDuration(JSContext* cx, const Duration& duration);

/**
 * CreateTemporalDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds [ , newTarget ] )
 */
DurationObject* CreateTemporalDuration(JSContext* cx, const Duration& duration);

/**
 * ToTemporalDuration ( item )
 */
Wrapped<DurationObject*> ToTemporalDuration(JSContext* cx,
                                            JS::Handle<JS::Value> item);

/**
 * ToTemporalDuration ( item )
 */
bool ToTemporalDuration(JSContext* cx, JS::Handle<JS::Value> item,
                        Duration* result);

/**
 * ToTemporalDurationRecord ( temporalDurationLike )
 */
bool ToTemporalDurationRecord(JSContext* cx,
                              JS::Handle<JS::Value> temporalDurationLike,
                              Duration* result);

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
bool BalanceTimeDuration(JSContext* cx, const Duration& duration,
                         TemporalUnit largestUnit, TimeDuration* result);

/**
 * BalanceTimeDuration ( days, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds, largestUnit [ , relativeTo ] )
 */
bool BalanceTimeDuration(JSContext* cx, const InstantSpan& nanoseconds,
                         TemporalUnit largestUnit, TimeDuration* result);

/**
 * AdjustRoundedDurationDays ( years, months, weeks, days, hours, minutes,
 * seconds, milliseconds, microseconds, nanoseconds, increment, unit,
 * roundingMode, zonedRelativeTo )
 */
bool AdjustRoundedDurationDays(
    JSContext* cx, const Duration& duration, Increment increment,
    TemporalUnit unit, TemporalRoundingMode roundingMode,
    JS::Handle<Wrapped<ZonedDateTimeObject*>> relativeTo, Duration* result);

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool RoundDuration(JSContext* cx, const Duration& duration, Increment increment,
                   TemporalUnit unit, TemporalRoundingMode roundingMode,
                   Duration* result);

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool RoundDuration(JSContext* cx, const Duration& duration, Increment increment,
                   TemporalUnit unit, TemporalRoundingMode roundingMode,
                   JS::Handle<Wrapped<PlainDateObject*>> plainRelativeTo,
                   Duration* result);

/**
 * RoundDuration ( years, months, weeks, days, hours, minutes, seconds,
 * milliseconds, microseconds, nanoseconds, increment, unit, roundingMode [ ,
 * plainRelativeTo [ , zonedRelativeTo ] ] )
 */
bool RoundDuration(JSContext* cx, const Duration& duration, Increment increment,
                   TemporalUnit unit, TemporalRoundingMode roundingMode,
                   JS::Handle<ZonedDateTimeObject*> zonedRelativeTo,
                   Duration* result);

} /* namespace js::temporal */

#endif /* builtin_temporal_Duration_h */
