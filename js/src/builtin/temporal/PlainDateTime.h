/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_PlainDateTime_h
#define builtin_temporal_PlainDateTime_h

#include <stdint.h>

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/TemporalTypes.h"
#include "builtin/temporal/Wrapped.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
class PlainObject;
}  // namespace js

namespace js::temporal {

class PlainDateTimeObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  // TODO: Consider compacting fields to reduce object size.
  //
  // See also PlainDateObject and PlainTimeObject.

  static constexpr uint32_t ISO_YEAR_SLOT = 0;
  static constexpr uint32_t ISO_MONTH_SLOT = 1;
  static constexpr uint32_t ISO_DAY_SLOT = 2;
  static constexpr uint32_t ISO_HOUR_SLOT = 3;
  static constexpr uint32_t ISO_MINUTE_SLOT = 4;
  static constexpr uint32_t ISO_SECOND_SLOT = 5;
  static constexpr uint32_t ISO_MILLISECOND_SLOT = 6;
  static constexpr uint32_t ISO_MICROSECOND_SLOT = 7;
  static constexpr uint32_t ISO_NANOSECOND_SLOT = 8;
  static constexpr uint32_t CALENDAR_SLOT = 9;
  static constexpr uint32_t SLOT_COUNT = 10;

  int32_t isoYear() const { return getFixedSlot(ISO_YEAR_SLOT).toInt32(); }

  int32_t isoMonth() const { return getFixedSlot(ISO_MONTH_SLOT).toInt32(); }

  int32_t isoDay() const { return getFixedSlot(ISO_DAY_SLOT).toInt32(); }

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

  CalendarValue calendar() const {
    return CalendarValue(getFixedSlot(CALENDAR_SLOT));
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * Extract the date fields from the PlainDateTime object.
 */
inline PlainDate ToPlainDate(const PlainDateTimeObject* dateTime) {
  return {dateTime->isoYear(), dateTime->isoMonth(), dateTime->isoDay()};
}

/**
 * Extract the time fields from the PlainDateTime object.
 */
inline PlainTime ToPlainTime(const PlainDateTimeObject* dateTime) {
  return {dateTime->isoHour(),        dateTime->isoMinute(),
          dateTime->isoSecond(),      dateTime->isoMillisecond(),
          dateTime->isoMicrosecond(), dateTime->isoNanosecond()};
}

/**
 * Extract the date-time fields from the PlainDateTime object.
 */
inline PlainDateTime ToPlainDateTime(const PlainDateTimeObject* dateTime) {
  return {ToPlainDate(dateTime), ToPlainTime(dateTime)};
}

enum class TemporalUnit;

#ifdef DEBUG
/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
bool IsValidISODateTime(const PlainDateTime& dateTime);
#endif

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool ISODateTimeWithinLimits(const PlainDateTime& dateTime);

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool ISODateTimeWithinLimits(const PlainDate& date);

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool ISODateTimeWithinLimits(double year, double month, double day);

/**
 * CreateTemporalDateTime ( isoYear, isoMonth, isoDay, hour, minute, second,
 * millisecond, microsecond, nanosecond, calendar [ , newTarget ] )
 */
PlainDateTimeObject* CreateTemporalDateTime(JSContext* cx,
                                            const PlainDateTime& dateTime,
                                            JS::Handle<CalendarValue> calendar);

/**
 * ToTemporalDateTime ( item [ , options ] )
 */
Wrapped<PlainDateTimeObject*> ToTemporalDateTime(JSContext* cx,
                                                 JS::Handle<JS::Value> item);

/**
 * ToTemporalDateTime ( item [ , options ] )
 */
bool ToTemporalDateTime(JSContext* cx, JS::Handle<JS::Value> item,
                        PlainDateTime* result);

/**
 * InterpretTemporalDateTimeFields ( calendar, fields, options )
 */
bool InterpretTemporalDateTimeFields(JSContext* cx,
                                     JS::Handle<CalendarValue> calendar,
                                     JS::Handle<PlainObject*> fields,
                                     JS::Handle<PlainObject*> options,
                                     PlainDateTime* result);

/**
 * InterpretTemporalDateTimeFields ( calendar, fields, options )
 */
bool InterpretTemporalDateTimeFields(JSContext* cx,
                                     JS::Handle<CalendarValue> calendar,
                                     JS::Handle<PlainObject*> fields,
                                     PlainDateTime* result);

/**
 * DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2,
 * d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit, options )
 */
bool DifferenceISODateTime(JSContext* cx, const PlainDateTime& one,
                           const PlainDateTime& two,
                           JS::Handle<CalendarValue> calendar,
                           TemporalUnit largestUnit, Duration* result);

/**
 * DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2,
 * d2, h2, min2, s2, ms2, mus2, ns2, calendar, largestUnit, options )
 */
bool DifferenceISODateTime(JSContext* cx, const PlainDateTime& one,
                           const PlainDateTime& two,
                           JS::Handle<CalendarValue> calendar,
                           TemporalUnit largestUnit,
                           JS::Handle<PlainObject*> options, Duration* result);

} /* namespace js::temporal */

#endif /* builtin_temporal_PlainDateTime_h */
