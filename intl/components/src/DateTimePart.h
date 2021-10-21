/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_DateTimePart_h_
#define intl_components_DateTimePart_h_

#include <cstddef>
#include <cstdint>

#include "mozilla/Vector.h"

namespace mozilla::intl {

enum class DateTimePartType : int16_t {
  Literal,
  Weekday,
  Era,
  Year,
  YearName,
  RelatedYear,
  Month,
  Day,
  DayPeriod,
  Hour,
  Minute,
  Second,
  FractionalSecondDigits,
  TimeZoneName,
  Unknown
};

enum class DateTimePartSource : int16_t { Shared, StartRange, EndRange };

/**
 * The 'Part' object defined in FormatDateTimeToParts and
 * FormatDateTimeRangeToParts
 *
 * Each part consists of three properties: ||Type||, ||Value|| and ||Source||,
 * with the ||Source|| property is set to DateTimePartSource::Shared by default.
 * (Note: From the spec, the part from FormatDateTimeToParts doesn't have the
 * ||Source|| property, so if the caller is FormatDateTimeToParts, it should
 * ignore the ||Source|| property).
 *
 * To store DateTimePart more efficiently, it doesn't store the ||Value|| of
 * type string in this struct. Instead, it stores the end index of the string
 * in the buffer(which is passed to DateTimeFormat::TryFormatToParts() or
 * can be got by calling AutoFormattedDateInterval::ToSpan()). The begin index
 * of the ||Value|| is the mEndIndex of the previous part.
 *
 *  Buffer
 *  0               i                j
 * +---------------+---------------+---------------+
 * | Part[0].Value | Part[1].Value | Part[2].Value | ....
 * +---------------+---------------+---------------+
 *
 *     Part[0].mEndIndex is i. Part[0].Value is stored in the Buffer[0..i].
 *     Part[1].mEndIndex is j. Part[1].Value is stored in the Buffer[i..j].
 *
 * See:
 * https://tc39.es/ecma402/#sec-formatdatetimetoparts
 * https://tc39.es/ecma402/#sec-formatdatetimerangetoparts
 */
struct DateTimePart {
  DateTimePart(DateTimePartType type, size_t endIndex,
               DateTimePartSource source)
      : mEndIndex(endIndex), mType(type), mSource(source) {}

  // See the above comments for details, mEndIndex is placed first for reducing
  // padding.
  size_t mEndIndex;
  DateTimePartType mType;
  DateTimePartSource mSource;
};

// The common parts are 'month', 'literal', 'day', 'literal', 'year', 'literal',
// 'hour', 'literal', 'minute', 'literal', which are 10 parts, for DateTimeRange
// the number will be doubled, so choosing 32 as the initial length to prevent
// heap allocation.
constexpr size_t INITIAL_DATETIME_PART_VECTOR_SIZE = 32;
using DateTimePartVector =
    mozilla::Vector<DateTimePart, INITIAL_DATETIME_PART_VECTOR_SIZE>;

}  // namespace mozilla::intl
#endif
