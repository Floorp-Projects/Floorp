/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Calendar.h"
#include "unicode/udat.h"
#include "unicode/utypes.h"

namespace mozilla::intl {

/* static */
Result<UniquePtr<Calendar>, ICUError> Calendar::TryCreate(
    const char* aLocale, Maybe<Span<const char16_t>> aTimeZoneOverride) {
  UErrorCode status = U_ZERO_ERROR;
  const UChar* zoneID = nullptr;
  int32_t zoneIDLen = 0;
  if (aTimeZoneOverride) {
    zoneIDLen = static_cast<int32_t>(aTimeZoneOverride->Length());
    zoneID = aTimeZoneOverride->Elements();
  }

  UCalendar* calendar =
      ucal_open(zoneID, zoneIDLen, aLocale, UCAL_DEFAULT, &status);

  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return MakeUnique<Calendar>(calendar);
}

Result<const char*, ICUError> Calendar::GetBcp47Type() {
  UErrorCode status = U_ZERO_ERROR;
  const char* oldType = ucal_getType(mCalendar, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  const char* bcp47Type = uloc_toUnicodeLocaleType("calendar", oldType);

  if (!bcp47Type) {
    return Err(ICUError::InternalError);
  }

  return bcp47Type;
}

Result<int32_t, ICUError> Calendar::GetDefaultTimeZoneOffsetMs() {
  UErrorCode status = U_ZERO_ERROR;
  int32_t offset = ucal_get(mCalendar, UCAL_ZONE_OFFSET, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return offset;
}

Result<Ok, ICUError> Calendar::SetTimeInMs(double aUnixEpoch) {
  UErrorCode status = U_ZERO_ERROR;
  ucal_setMillis(mCalendar, aUnixEpoch, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return Ok{};
}

/* static */
Result<SpanEnumeration<char>, InternalError>
Calendar::GetLegacyKeywordValuesForLocale(const char* aLocale) {
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucal_getKeywordValuesForLocale(
      "calendar", aLocale, /* commonlyUsed */ false, &status);

  if (U_SUCCESS(status)) {
    return SpanEnumeration<char>(enumeration);
  }

  return Err(InternalError{});
}

/* static */
SpanResult<char> Calendar::LegacyIdentifierToBcp47(const char* aIdentifier,
                                                   int32_t aLength) {
  if (aIdentifier == nullptr) {
    return Err(InternalError{});
  }
  // aLength is not needed here, as the ICU call uses the null terminated
  // string.
  return MakeStringSpan(uloc_toUnicodeLocaleType("ca", aIdentifier));
}

/* static */
Result<Calendar::Bcp47IdentifierEnumeration, InternalError>
Calendar::GetBcp47KeywordValuesForLocale(const char* aLocale) {
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucal_getKeywordValuesForLocale(
      "calendar", aLocale, /* commonlyUsed */ false, &status);

  if (U_SUCCESS(status)) {
    return Bcp47IdentifierEnumeration(enumeration);
  }

  return Err(InternalError{});
}

Calendar::~Calendar() {
  MOZ_ASSERT(mCalendar);
  ucal_close(mCalendar);
}

}  // namespace mozilla::intl
