/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/NumberRangeFormat.h"

#include "mozilla/Try.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/NumberFormat.h"
#include "NumberFormatFields.h"
#include "NumberFormatterSkeleton.h"
#include "ScopedICUObject.h"

#include "unicode/uformattedvalue.h"
#include "unicode/unumberrangeformatter.h"
#include "unicode/upluralrules.h"

namespace mozilla::intl {

/*static*/ Result<UniquePtr<NumberRangeFormat>, ICUError>
NumberRangeFormat::TryCreate(std::string_view aLocale,
                             const NumberRangeFormatOptions& aOptions) {
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

Result<Ok, ICUError> NumberRangeFormat::initialize(
    std::string_view aLocale, const NumberRangeFormatOptions& aOptions) {
  mFormatForUnit = aOptions.mUnit.isSome();

  NumberFormatterSkeleton skeleton(aOptions);
  mNumberRangeFormatter = skeleton.toRangeFormatter(
      aLocale, aOptions.mRangeCollapse, aOptions.mRangeIdentityFallback);
  if (mNumberRangeFormatter) {
    UErrorCode status = U_ZERO_ERROR;
    mFormattedNumberRange = unumrf_openResult(&status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }
    return Ok();
  }
  return Err(ICUError::InternalError);
}

Result<int32_t, ICUError> NumberRangeFormat::selectForRange(
    double start, double end, char16_t* keyword, int32_t keywordSize,
    const UPluralRules* pluralRules) const {
  MOZ_ASSERT(keyword);
  MOZ_ASSERT(pluralRules);

  MOZ_TRY(format(start, end));

  UErrorCode status = U_ZERO_ERROR;
  int32_t utf16KeywordLength = uplrules_selectForRange(
      pluralRules, mFormattedNumberRange, keyword, keywordSize, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return utf16KeywordLength;
}

bool NumberRangeFormat::formatInternal(double start, double end) const {
  // ICU incorrectly formats NaN values with the sign bit set, as if they
  // were negative.  Replace all NaNs with a single pattern with sign bit
  // unset ("positive", that is) until ICU is fixed.
  if (MOZ_UNLIKELY(std::isnan(start))) {
    start = SpecificNaN<double>(0, 1);
  }
  if (MOZ_UNLIKELY(std::isnan(end))) {
    end = SpecificNaN<double>(0, 1);
  }

  UErrorCode status = U_ZERO_ERROR;
  unumrf_formatDoubleRange(mNumberRangeFormatter, start, end,
                           mFormattedNumberRange, &status);
  return U_SUCCESS(status);
}

bool NumberRangeFormat::formatInternal(std::string_view start,
                                       std::string_view end) const {
  UErrorCode status = U_ZERO_ERROR;
  unumrf_formatDecimalRange(mNumberRangeFormatter, start.data(), start.size(),
                            end.data(), end.size(), mFormattedNumberRange,
                            &status);
  return U_SUCCESS(status);
}

Result<std::u16string_view, ICUError> NumberRangeFormat::formatResult() const {
  UErrorCode status = U_ZERO_ERROR;

  const UFormattedValue* formattedValue =
      unumrf_resultAsValue(mFormattedNumberRange, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t utf16Length;
  const char16_t* utf16Str =
      ufmtval_getString(formattedValue, &utf16Length, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return std::u16string_view(utf16Str, static_cast<size_t>(utf16Length));
}

Result<std::u16string_view, ICUError> NumberRangeFormat::formatResultToParts(
    Maybe<double> start, bool startIsNegative, Maybe<double> end,
    bool endIsNegative, NumberPartVector& parts) const {
  UErrorCode status = U_ZERO_ERROR;

  const UFormattedValue* formattedValue =
      unumrf_resultAsValue(mFormattedNumberRange, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  int32_t utf16Length;
  const char16_t* utf16Str =
      ufmtval_getString(formattedValue, &utf16Length, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  UConstrainedFieldPosition* fpos = ucfpos_open(&status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  ScopedICUObject<UConstrainedFieldPosition, ucfpos_close> toCloseFpos(fpos);

  Maybe<double> number = start;
  bool isNegative = startIsNegative;

  NumberPartSourceMap sourceMap;

  // Vacuum up fields in the overall formatted string.
  NumberFormatFields fields;

  while (true) {
    bool hasMore = ufmtval_nextPosition(formattedValue, fpos, &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }
    if (!hasMore) {
      break;
    }

    int32_t category = ucfpos_getCategory(fpos, &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    int32_t fieldName = ucfpos_getField(fpos, &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    int32_t beginIndex, endIndex;
    ucfpos_getIndexes(fpos, &beginIndex, &endIndex, &status);
    if (U_FAILURE(status)) {
      return Err(ToICUError(status));
    }

    if (category == UFIELD_CATEGORY_NUMBER_RANGE_SPAN) {
      // The special field category UFIELD_CATEGORY_NUMBER_RANGE_SPAN has only
      // two allowed values (0 or 1), indicating the begin of the start resp.
      // end number.
      MOZ_ASSERT(fieldName == 0 || fieldName == 1,
                 "span category has unexpected value");

      if (fieldName == 0) {
        number = start;
        isNegative = startIsNegative;

        sourceMap.start = {uint32_t(beginIndex), uint32_t(endIndex)};
      } else {
        number = end;
        isNegative = endIsNegative;

        sourceMap.end = {uint32_t(beginIndex), uint32_t(endIndex)};
      }

      continue;
    }

    // Ignore categories other than UFIELD_CATEGORY_NUMBER.
    if (category != UFIELD_CATEGORY_NUMBER) {
      continue;
    }

    Maybe<NumberPartType> partType = GetPartTypeForNumberField(
        UNumberFormatFields(fieldName), number, isNegative, mFormatForUnit);
    if (!partType || !fields.append(*partType, beginIndex, endIndex)) {
      return Err(ToICUError(status));
    }
  }

  if (!fields.toPartsVector(utf16Length, sourceMap, parts)) {
    return Err(ToICUError(status));
  }

  return std::u16string_view(utf16Str, static_cast<size_t>(utf16Length));
}

}  // namespace mozilla::intl
