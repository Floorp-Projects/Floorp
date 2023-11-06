/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Instant_h
#define builtin_temporal_Instant_h

#include "mozilla/Assertions.h"

#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/Wrapped.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class InstantObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t SECONDS_SLOT = 0;
  static constexpr uint32_t NANOSECONDS_SLOT = 1;
  static constexpr uint32_t SLOT_COUNT = 2;

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

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the instant fields from the Instant object.
 */
inline Instant ToInstant(const InstantObject* instant) {
  return {instant->seconds(), instant->nanoseconds()};
}

class Increment;
enum class TemporalUnit;
enum class TemporalRoundingMode;

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool IsValidEpochNanoseconds(const JS::BigInt* epochNanoseconds);

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool IsValidEpochInstant(const Instant& instant);

/**
 * Return true if the input is within the valid instant span limits.
 */
bool IsValidInstantSpan(const InstantSpan& span);

/**
 * Return true if the input is within the valid instant span limits.
 */
bool IsValidInstantSpan(const JS::BigInt* nanoseconds);

/**
 * Convert a BigInt to an instant. The input must be a valid epoch nanoseconds
 * value.
 */
Instant ToInstant(const JS::BigInt* epochNanoseconds);

/**
 * Convert a BigInt to an instant span. The input must be a valid epoch
 * nanoseconds span value.
 */
InstantSpan ToInstantSpan(const JS::BigInt* nanoseconds);

/**
 * Convert an instant to a BigInt. The input must be a valid epoch instant.
 */
JS::BigInt* ToEpochNanoseconds(JSContext* cx, const Instant& instant);

/**
 * Convert an instant span to a BigInt. The input must be a valid instant span.
 */
JS::BigInt* ToEpochNanoseconds(JSContext* cx, const InstantSpan& instant);

/**
 * ToTemporalInstant ( item )
 */
Wrapped<InstantObject*> ToTemporalInstant(JSContext* cx,
                                          JS::Handle<JS::Value> item);

/**
 * ToTemporalInstant ( item )
 */
bool ToTemporalInstant(JSContext* cx, JS::Handle<JS::Value> item,
                       Instant* result);

/**
 * CreateTemporalInstant ( epochNanoseconds [ , newTarget ] )
 */
InstantObject* CreateTemporalInstant(JSContext* cx, const Instant& instant);

/**
 * GetUTCEpochNanoseconds ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond [ , offsetNanoseconds ] )
 */
Instant GetUTCEpochNanoseconds(const PlainDateTime& dateTime);

/**
 * GetUTCEpochNanoseconds ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond [ , offsetNanoseconds ] )
 */
Instant GetUTCEpochNanoseconds(const PlainDateTime& dateTime,
                               const InstantSpan& offsetNanoseconds);

/**
 * RoundTemporalInstant ( ns, increment, unit, roundingMode )
 */
bool RoundTemporalInstant(JSContext* cx, const Instant& ns, Increment increment,
                          TemporalUnit unit, TemporalRoundingMode roundingMode,
                          Instant* result);

/**
 * AddInstant ( epochNanoseconds, hours, minutes, seconds, milliseconds,
 * microseconds, nanoseconds )
 */
bool AddInstant(JSContext* cx, const Instant& instant, const Duration& duration,
                Instant* result);

/**
 * DifferenceInstant ( ns1, ns2, roundingIncrement, smallestUnit, largestUnit,
 * roundingMode )
 */
bool DifferenceInstant(JSContext* cx, const Instant& ns1, const Instant& ns2,
                       Increment roundingIncrement, TemporalUnit smallestUnit,
                       TemporalUnit largestUnit,
                       TemporalRoundingMode roundingMode, Duration* result);

} /* namespace js::temporal */

#endif /* builtin_temporal_Instant_h */
