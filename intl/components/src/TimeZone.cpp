/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/TimeZone.h"

#include "unicode/uenum.h"

namespace mozilla::intl {

/* static */
Result<UniquePtr<TimeZone>, ICUError> TimeZone::TryCreate(
    Maybe<Span<const char16_t>> aTimeZoneOverride) {
  // An empty string is used for the root locale. This is regarded as the base
  // locale of all locales, and is used as the language/country neutral locale
  // for locale sensitive operations.
  const char* rootLocale = "";

  UErrorCode status = U_ZERO_ERROR;
  const UChar* zoneID = nullptr;
  int32_t zoneIDLen = 0;
  if (aTimeZoneOverride) {
    zoneIDLen = static_cast<int32_t>(aTimeZoneOverride->Length());
    zoneID = aTimeZoneOverride->Elements();
  }

  UCalendar* calendar =
      ucal_open(zoneID, zoneIDLen, rootLocale, UCAL_DEFAULT, &status);

  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  // https://tc39.es/ecma262/#sec-time-values-and-time-range
  //
  // A time value supports a slightly smaller range of -8,640,000,000,000,000 to
  // 8,640,000,000,000,000 milliseconds.
  constexpr double StartOfTime = -8.64e15;

  // Ensure all computations are performed in the proleptic Gregorian calendar.
  ucal_setGregorianChange(calendar, StartOfTime, &status);

  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return MakeUnique<TimeZone>(calendar);
}

Result<int32_t, ICUError> TimeZone::GetRawOffsetMs() {
  // Reset the time in case the calendar has been modified.
  UErrorCode status = U_ZERO_ERROR;
  ucal_setMillis(mCalendar, ucal_getNow(), &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t offset = ucal_get(mCalendar, UCAL_ZONE_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return offset;
}

Result<int32_t, ICUError> TimeZone::GetDSTOffsetMs(int64_t aUTCMilliseconds) {
  UDate date = UDate(aUTCMilliseconds);

  UErrorCode status = U_ZERO_ERROR;
  ucal_setMillis(mCalendar, date, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t dstOffset = ucal_get(mCalendar, UCAL_DST_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return dstOffset;
}

Result<int32_t, ICUError> TimeZone::GetOffsetMs(int64_t aUTCMilliseconds) {
  UDate date = UDate(aUTCMilliseconds);

  UErrorCode status = U_ZERO_ERROR;
  ucal_setMillis(mCalendar, date, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t rawOffset = ucal_get(mCalendar, UCAL_ZONE_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t dstOffset = ucal_get(mCalendar, UCAL_DST_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return rawOffset + dstOffset;
}

Result<int32_t, ICUError> TimeZone::GetUTCOffsetMs(int64_t aLocalMilliseconds) {
  // https://tc39.es/ecma262/#sec-local-time-zone-adjustment
  //
  // LocalTZA ( t, isUTC )
  //
  // When t_local represents local time repeating multiple times at a negative
  // time zone transition (e.g. when the daylight saving time ends or the time
  // zone offset is decreased due to a time zone rule change) or skipped local
  // time at a positive time zone transitions (e.g. when the daylight saving
  // time starts or the time zone offset is increased due to a time zone rule
  // change), t_local must be interpreted using the time zone offset before the
  // transition.
#ifndef U_HIDE_DRAFT_API
  constexpr UTimeZoneLocalOption skippedTime = UCAL_TZ_LOCAL_FORMER;
  constexpr UTimeZoneLocalOption repeatedTime = UCAL_TZ_LOCAL_FORMER;
#else
  constexpr UTimeZoneLocalOption skippedTime = UTimeZoneLocalOption(0x4);
  constexpr UTimeZoneLocalOption repeatedTime = UTimeZoneLocalOption(0x4);
#endif

  UDate date = UDate(aLocalMilliseconds);

  UErrorCode status = U_ZERO_ERROR;
  ucal_setMillis(mCalendar, date, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t rawOffset, dstOffset;
  ucal_getTimeZoneOffsetFromLocal(mCalendar, skippedTime, repeatedTime,
                                  &rawOffset, &dstOffset, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return rawOffset + dstOffset;
}

Result<SpanEnumeration<char>, ICUError> TimeZone::GetAvailableTimeZones(
    const char* aRegion) {
  // Get the time zones that are commonly used in the given region. Uses the
  // UCAL_ZONE_TYPE_ANY filter so we have more fine-grained control over the
  // returned time zones and don't omit time zones which are considered links in
  // ICU, but are treated as proper zones in IANA.
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucal_openTimeZoneIDEnumeration(
      UCAL_ZONE_TYPE_ANY, aRegion, nullptr, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return SpanEnumeration<char>(enumeration);
}

TimeZone::~TimeZone() {
  MOZ_ASSERT(mCalendar);
  ucal_close(mCalendar);
}

}  // namespace mozilla::intl
