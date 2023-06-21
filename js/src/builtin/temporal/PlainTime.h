/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_PlainTime_h
#define builtin_temporal_PlainTime_h

#include <stdint.h>

#include "builtin/temporal/TemporalTypes.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class PlainTimeObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  // TODO: Consider compacting fields to reduce object size.
  //
  // ceil(log2(24)) + 2 * ceil(log2(60)) + 3 * ceil(log2(1000)) = 47 bits are
  // needed to store a time value in a single int64. 47 bits can be stored as
  // raw bits in a JS::Value.

  static constexpr uint32_t ISO_HOUR_SLOT = 0;
  static constexpr uint32_t ISO_MINUTE_SLOT = 1;
  static constexpr uint32_t ISO_SECOND_SLOT = 2;
  static constexpr uint32_t ISO_MILLISECOND_SLOT = 3;
  static constexpr uint32_t ISO_MICROSECOND_SLOT = 4;
  static constexpr uint32_t ISO_NANOSECOND_SLOT = 5;
  static constexpr uint32_t CALENDAR_SLOT = 6;
  static constexpr uint32_t SLOT_COUNT = 7;

  int32_t isoHour() const { return getFixedSlot(ISO_HOUR_SLOT).toInt32(); }

  int32_t isoMinute() const { return getFixedSlot(ISO_MINUTE_SLOT).toInt32(); }

  int32_t isoSecond() const { return getFixedSlot(ISO_SECOND_SLOT).toInt32(); }

  int32_t isoMillisecond() const {
    return getFixedSlot(ISO_MILLISECOND_SLOT).toInt32();
  }

  int32_t isoMicrosecond() const {
    return getFixedSlot(ISO_MICROSECOND_SLOT).toInt32();
  }

  int32_t isoNanosecond() const {
    return getFixedSlot(ISO_NANOSECOND_SLOT).toInt32();
  }

  JSObject* getCalendar() const {
    return getFixedSlot(CALENDAR_SLOT).toObjectOrNull();
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the time fields from the PlainTime object.
 */
inline PlainTime ToPlainTime(const PlainTimeObject* time) {
  return {time->isoHour(),        time->isoMinute(),
          time->isoSecond(),      time->isoMillisecond(),
          time->isoMicrosecond(), time->isoNanosecond()};
}

} /* namespace js::temporal */

#endif /* builtin_temporal_PlainTime_h */
