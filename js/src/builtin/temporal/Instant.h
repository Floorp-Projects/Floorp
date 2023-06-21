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

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool IsValidEpochNanoseconds(const JS::BigInt* epochNanoseconds);

/**
 * IsValidEpochNanoseconds ( epochNanoseconds )
 */
bool IsValidEpochInstant(const Instant& instant);

/**
 * Return true if the input is within the valid instant difference limits.
 */
bool IsValidInstantDifference(const Instant& ns);

/**
 * Return true if the input is within the valid instant difference limits.
 */
bool IsValidInstantDifference(const JS::BigInt* ns);

/**
 * Convert a BigInt to an instant. The input must be a valid epoch nanoseconds
 * value.
 */
Instant ToInstant(const JS::BigInt* epochNanoseconds);

/**
 * Convert a BigInt to an instant difference. The input must be a valid epoch
 * nanoseconds difference value.
 */
Instant ToInstantDifference(const JS::BigInt* epochNanoseconds);

/**
 * Convert an instant to a BigInt. The input must be a valid epoch instant.
 */
JS::BigInt* ToEpochNanoseconds(JSContext* cx, const Instant& instant);

/**
 * Convert an instant difference to a BigInt. The input must be a valid epoch
 * instant difference.
 */
JS::BigInt* ToEpochDifferenceNanoseconds(JSContext* cx, const Instant& instant);

} /* namespace js::temporal */

#endif /* builtin_temporal_Instant_h */
