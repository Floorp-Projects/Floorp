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

  return MakeUnique<TimeZone>(calendar);
}

Result<int32_t, ICUError> TimeZone::GetRawOffsetMs() {
  UErrorCode status = U_ZERO_ERROR;
  int32_t offset = ucal_get(mCalendar, UCAL_ZONE_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return offset;
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
