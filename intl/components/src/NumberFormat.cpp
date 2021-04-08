/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/NumberFormat.h"
#include "NumberFormatterSkeleton.h"

#include "unicode/unumberformatter.h"

namespace mozilla {
namespace intl {

NumberFormat::NumberFormat(std::string_view aLocale,
                           const NumberFormatOptions& aOptions) {
  NumberFormatterSkeleton skeleton(aOptions);
  mNumberFormatter = skeleton.toFormatter(aLocale);
  if (mNumberFormatter) {
    UErrorCode status = U_ZERO_ERROR;
    mFormattedNumber = unumf_openResult(&status);
    if (U_SUCCESS(status)) {
      mIsInitialized = true;
    }
  }
}

NumberFormat::~NumberFormat() {
  if (mFormattedNumber) {
    unumf_closeResult(mFormattedNumber);
  }
  if (mNumberFormatter) {
    unumf_close(mNumberFormatter);
  }
}

bool NumberFormat::formatInternal(double number) const {
  // ICU incorrectly formats NaN values with the sign bit set, as if they
  // were negative.  Replace all NaNs with a single pattern with sign bit
  // unset ("positive", that is) until ICU is fixed.
  if (MOZ_UNLIKELY(IsNaN(number))) {
    number = SpecificNaN<double>(0, 1);
  }

  UErrorCode status = U_ZERO_ERROR;
  unumf_formatDouble(mNumberFormatter, number, mFormattedNumber, &status);
  return U_SUCCESS(status);
}

bool NumberFormat::formatInternal(int64_t number) const {
  UErrorCode status = U_ZERO_ERROR;
  unumf_formatInt(mNumberFormatter, number, mFormattedNumber, &status);
  return U_SUCCESS(status);
}

bool NumberFormat::formatInternal(std::string_view number) const {
  UErrorCode status = U_ZERO_ERROR;
  unumf_formatDecimal(mNumberFormatter, number.data(), number.size(),
                      mFormattedNumber, &status);
  return U_SUCCESS(status);
}

std::u16string_view NumberFormat::formatResult() const {
  UErrorCode status = U_ZERO_ERROR;

  const UFormattedValue* formattedValue =
      unumf_resultAsValue(mFormattedNumber, &status);
  if (U_FAILURE(status)) {
    return {};
  }

  int32_t utf16Length;
  const char16_t* utf16Str =
      ufmtval_getString(formattedValue, &utf16Length, &status);
  if (U_FAILURE(status)) {
    return {};
  }

  return {utf16Str, static_cast<size_t>(utf16Length)};
}

}  // namespace intl
}  // namespace mozilla
