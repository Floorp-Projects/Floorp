/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/RelativeTimeFormat.h"
#include "mozilla/FloatingPoint.h"

#include "unicode/unum.h"

#include "NumberFormatFields.h"
#include "ICU4CGlue.h"
#include "ScopedICUObject.h"

namespace mozilla::intl {

/*static*/ Result<UniquePtr<RelativeTimeFormat>, ICUError>
RelativeTimeFormat::TryCreate(const char* aLocale,
                              const RelativeTimeFormatOptions& aOptions) {
  UErrorCode status = U_ZERO_ERROR;

  UFormattedRelativeDateTime* formattedRelativeDateTime =
      ureldatefmt_openResult(&status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  ScopedICUObject<UFormattedRelativeDateTime, ureldatefmt_closeResult>
      closeFormattedRelativeDate(formattedRelativeDateTime);

  UNumberFormat* nf =
      unum_open(UNUM_DECIMAL, nullptr, 0, IcuLocale(aLocale), nullptr, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  ScopedICUObject<UNumberFormat, unum_close> closeNumberFormatter(nf);

  // Use the default values as if a new Intl.NumberFormat had been constructed.
  unum_setAttribute(nf, UNUM_MIN_INTEGER_DIGITS, 1);
  unum_setAttribute(nf, UNUM_MIN_FRACTION_DIGITS, 0);
  unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, 3);
  unum_setAttribute(nf, UNUM_GROUPING_USED, true);
  unum_setAttribute(nf, UNUM_MINIMUM_GROUPING_DIGITS,
                    UNUM_MINIMUM_GROUPING_DIGITS_AUTO);

  UDateRelativeDateTimeFormatterStyle relDateTimeStyle;
  switch (aOptions.style) {
    case RelativeTimeFormatOptions::Style::Short:
      relDateTimeStyle = UDAT_STYLE_SHORT;
      break;
    case RelativeTimeFormatOptions::Style::Narrow:
      relDateTimeStyle = UDAT_STYLE_NARROW;
      break;
    case RelativeTimeFormatOptions::Style::Long:
      relDateTimeStyle = UDAT_STYLE_LONG;
      break;
  }

  URelativeDateTimeFormatter* formatter =
      ureldatefmt_open(IcuLocale(aLocale), nf, relDateTimeStyle,
                       UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);

  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  // Ownership was transferred to mFormatter.
  closeNumberFormatter.forget();

  UniquePtr<RelativeTimeFormat> rtf = MakeUnique<RelativeTimeFormat>(
      aOptions.numeric, formatter, formattedRelativeDateTime);

  // Ownership was transferred to rtf.
  closeFormattedRelativeDate.forget();
  return rtf;
}

RelativeTimeFormat::RelativeTimeFormat(
    RelativeTimeFormatOptions::Numeric aNumeric,
    URelativeDateTimeFormatter* aFormatter,
    UFormattedRelativeDateTime* aFormattedRelativeDateTime)
    : mNumeric(aNumeric),
      mFormatter(aFormatter),
      mFormattedRelativeDateTime(aFormattedRelativeDateTime) {}

RelativeTimeFormat::~RelativeTimeFormat() {
  if (mFormattedRelativeDateTime) {
    ureldatefmt_closeResult(mFormattedRelativeDateTime);
    mFormattedRelativeDateTime = nullptr;
  }

  if (mFormatter) {
    ureldatefmt_close(mFormatter);
    mFormatter = nullptr;
  }
}

URelativeDateTimeUnit RelativeTimeFormat::ToURelativeDateTimeUnit(
    FormatUnit unit) const {
  switch (unit) {
    case FormatUnit::Second:
      return UDAT_REL_UNIT_SECOND;
    case FormatUnit::Minute:
      return UDAT_REL_UNIT_MINUTE;
    case FormatUnit::Hour:
      return UDAT_REL_UNIT_HOUR;
    case FormatUnit::Day:
      return UDAT_REL_UNIT_DAY;
    case FormatUnit::Week:
      return UDAT_REL_UNIT_WEEK;
    case FormatUnit::Month:
      return UDAT_REL_UNIT_MONTH;
    case FormatUnit::Quarter:
      return UDAT_REL_UNIT_QUARTER;
    case FormatUnit::Year:
      return UDAT_REL_UNIT_YEAR;
  };
  MOZ_ASSERT_UNREACHABLE();
  return UDAT_REL_UNIT_SECOND;
}

Result<Span<const char16_t>, ICUError> RelativeTimeFormat::formatToParts(
    double aNumber, FormatUnit aUnit, NumberPartVector& aParts) const {
  UErrorCode status = U_ZERO_ERROR;

  if (mNumeric == RelativeTimeFormatOptions::Numeric::Auto) {
    ureldatefmt_formatToResult(mFormatter, aNumber,
                               ToURelativeDateTimeUnit(aUnit),
                               mFormattedRelativeDateTime, &status);
  } else {
    ureldatefmt_formatNumericToResult(mFormatter, aNumber,
                                      ToURelativeDateTimeUnit(aUnit),
                                      mFormattedRelativeDateTime, &status);
  }
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  const UFormattedValue* formattedValue =
      ureldatefmt_resultAsValue(mFormattedRelativeDateTime, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  bool isNegative = !std::isnan(aNumber) && IsNegative(aNumber);

  // Necessary until all of intl is using Span (Bug 1709880)
  return FormatResultToParts(formattedValue, Nothing(), isNegative,
                             false /*formatForUnit*/, aParts)
      .andThen([](std::u16string_view result)
                   -> Result<Span<const char16_t>, ICUError> {
        return Span<const char16_t>(result.data(), result.length());
      });
}

}  // namespace mozilla::intl
