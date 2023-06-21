/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_ZonedDateTime_h
#define builtin_temporal_ZonedDateTime_h

#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
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

  JSObject* timeZone() const { return &getFixedSlot(TIMEZONE_SLOT).toObject(); }

  JSObject* calendar() const { return &getFixedSlot(CALENDAR_SLOT).toObject(); }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the instant fields from the ZonedDateTime object.
 */
inline Instant ToInstant(const ZonedDateTimeObject* zonedDateTime) {
  return {zonedDateTime->seconds(), zonedDateTime->nanoseconds()};
}

} /* namespace js::temporal */

#endif /* builtin_temporal_ZonedDateTime_h */
