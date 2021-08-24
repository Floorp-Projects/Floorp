/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/NumberRangeFormat.h"

#include "mozilla/intl/NumberFormat.h"
#include "NumberFormatterSkeleton.h"
#include "ScopedICUObject.h"

#include "unicode/uformattedvalue.h"
#include "unicode/unumberrangeformatter.h"
#include "unicode/upluralrules.h"

namespace mozilla::intl {

#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT

/*static*/ Result<UniquePtr<NumberRangeFormat>, NumberRangeFormat::FormatError>
NumberRangeFormat::TryCreate(std::string_view aLocale,
                             const NumberFormatOptions& aOptions) {
  UniquePtr<NumberRangeFormat> nrf = MakeUnique<NumberRangeFormat>();
  MOZ_TRY(nrf->initialize(aLocale, aOptions));
  return nrf;
}

NumberRangeFormat::~NumberRangeFormat() {
  if (mFormattedNumberRange) {
    unumrf_closeResult(mFormattedNumberRange);
  }
  if (mNumberRangeFormatter) {
    unumrf_close(mNumberRangeFormatter);
  }
}

Result<Ok, NumberRangeFormat::FormatError> NumberRangeFormat::initialize(
    std::string_view aLocale, const NumberFormatOptions& aOptions) {
  NumberFormatterSkeleton skeleton(aOptions);
  mNumberRangeFormatter = skeleton.toRangeFormatter(aLocale);
  if (mNumberRangeFormatter) {
    UErrorCode status = U_ZERO_ERROR;
    mFormattedNumberRange = unumrf_openResult(&status);
    if (U_SUCCESS(status)) {
      return Ok();
    }
  }
  return Err(FormatError::InternalError);
}

Result<int32_t, NumberRangeFormat::FormatError>
NumberRangeFormat::selectForRange(double start, double end, char16_t* keyword,
                                  int32_t keywordSize,
                                  const UPluralRules* pluralRules) const {
  MOZ_ASSERT(keyword);
  MOZ_ASSERT(pluralRules);

  MOZ_TRY(format(start, end));

  UErrorCode status = U_ZERO_ERROR;
  int32_t utf16KeywordLength = uplrules_selectForRange(
      pluralRules, mFormattedNumberRange, keyword, keywordSize, &status);
  if (U_FAILURE(status)) {
    return Err(FormatError::InternalError);
  }

  return utf16KeywordLength;
}

bool NumberRangeFormat::formatInternal(double start, double end) const {
  // ICU incorrectly formats NaN values with the sign bit set, as if they
  // were negative.  Replace all NaNs with a single pattern with sign bit
  // unset ("positive", that is) until ICU is fixed.
  if (MOZ_UNLIKELY(IsNaN(start))) {
    start = SpecificNaN<double>(0, 1);
  }
  if (MOZ_UNLIKELY(IsNaN(end))) {
    end = SpecificNaN<double>(0, 1);
  }

  UErrorCode status = U_ZERO_ERROR;
  unumrf_formatDoubleRange(mNumberRangeFormatter, start, end,
                           mFormattedNumberRange, &status);
  return U_SUCCESS(status);
}

Result<std::u16string_view, NumberRangeFormat::FormatError>
NumberRangeFormat::formatResult() const {
  UErrorCode status = U_ZERO_ERROR;

  const UFormattedValue* formattedValue =
      unumrf_resultAsValue(mFormattedNumberRange, &status);
  if (U_FAILURE(status)) {
    return Err(FormatError::InternalError);
  }

  int32_t utf16Length;
  const char16_t* utf16Str =
      ufmtval_getString(formattedValue, &utf16Length, &status);
  if (U_FAILURE(status)) {
    return Err(FormatError::InternalError);
  }

  return std::u16string_view(utf16Str, static_cast<size_t>(utf16Length));
}

#else

/*static*/ Result<UniquePtr<NumberRangeFormat>, NumberRangeFormat::FormatError>
NumberRangeFormat::TryCreate(std::string_view aLocale,
                             const NumberFormatOptions& aOptions) {
  return MakeUnique<NumberRangeFormat>();
}

#endif

}  // namespace mozilla::intl
