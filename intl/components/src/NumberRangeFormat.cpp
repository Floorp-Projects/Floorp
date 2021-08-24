/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/NumberRangeFormat.h"

#include "mozilla/intl/NumberFormat.h"
#include "NumberFormatFields.h"
#include "NumberFormatterSkeleton.h"
#include "ScopedICUObject.h"

#include "unicode/uformattedvalue.h"
#include "unicode/unumberrangeformatter.h"
#include "unicode/upluralrules.h"

namespace mozilla::intl {

#ifdef MOZ_INTL_HAS_NUMBER_RANGE_FORMAT

/*static*/ Result<UniquePtr<NumberRangeFormat>, NumberRangeFormat::FormatError>
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

Result<Ok, NumberRangeFormat::FormatError> NumberRangeFormat::initialize(
    std::string_view aLocale, const NumberRangeFormatOptions& aOptions) {
  mFormatForUnit = aOptions.mUnit.isSome();
  switch (aOptions.mRangeIdentityFallback) {
    case NumberRangeFormatOptions::RangeIdentityFallback::
        ApproximatelyOrSingleValue:
    case NumberRangeFormatOptions::RangeIdentityFallback::Approximately:
      mFormatWithApprox = true;
      break;
    case NumberRangeFormatOptions::RangeIdentityFallback::SingleValue:
    case NumberRangeFormatOptions::RangeIdentityFallback::Range:
      mFormatWithApprox = false;
      break;
  }
  NumberFormatterSkeleton skeleton(aOptions);
  mNumberRangeFormatter = skeleton.toRangeFormatter(
      aLocale, aOptions.mRangeCollapse, aOptions.mRangeIdentityFallback);
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

bool NumberRangeFormat::formatInternal(std::string_view start,
                                       std::string_view end) const {
  UErrorCode status = U_ZERO_ERROR;
  unumrf_formatDecimalRange(mNumberRangeFormatter, start.data(), start.size(),
                            end.data(), end.size(), mFormattedNumberRange,
                            &status);
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

Result<std::u16string_view, NumberRangeFormat::FormatError>
NumberRangeFormat::formatResultToParts(Maybe<double> start,
                                       bool startIsNegative, Maybe<double> end,
                                       bool endIsNegative,
                                       NumberPartVector& parts) const {
  UErrorCode status = U_ZERO_ERROR;

  UNumberRangeIdentityResult identity =
      unumrf_resultGetIdentityResult(mFormattedNumberRange, &status);
  if (U_FAILURE(status)) {
    return Err(FormatError::InternalError);
  }

  bool isIdenticalNumber = identity != UNUM_IDENTITY_RESULT_NOT_EQUAL;

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

  UConstrainedFieldPosition* fpos = ucfpos_open(&status);
  if (U_FAILURE(status)) {
    return Err(FormatError::InternalError);
  }
  ScopedICUObject<UConstrainedFieldPosition, ucfpos_close> toCloseFpos(fpos);

  // We're only interested in UFIELD_CATEGORY_NUMBER fields when the start and
  // end range is identical.
  //
  // Constraining the category is only needed as a workaround for
  // <https://unicode-org.atlassian.net/browse/ICU-21683>.
  if (isIdenticalNumber) {
    ucfpos_constrainCategory(fpos, UFIELD_CATEGORY_NUMBER, &status);
    if (U_FAILURE(status)) {
      return Err(FormatError::InternalError);
    }
  }

  Maybe<double> number = start;
  bool isNegative = startIsNegative;

  NumberPartSourceMap sourceMap;

  // Vacuum up fields in the overall formatted string.
  NumberFormatFields fields;

  while (true) {
    bool hasMore = ufmtval_nextPosition(formattedValue, fpos, &status);
    if (U_FAILURE(status)) {
      return Err(FormatError::InternalError);
    }
    if (!hasMore) {
      break;
    }

    int32_t category = ucfpos_getCategory(fpos, &status);
    if (U_FAILURE(status)) {
      return Err(FormatError::InternalError);
    }

    int32_t fieldName = ucfpos_getField(fpos, &status);
    if (U_FAILURE(status)) {
      return Err(FormatError::InternalError);
    }

    int32_t beginIndex, endIndex;
    ucfpos_getIndexes(fpos, &beginIndex, &endIndex, &status);
    if (U_FAILURE(status)) {
      return Err(FormatError::InternalError);
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
      return Err(FormatError::InternalError);
    }
  }

  if (!fields.toPartsVector(utf16Length, sourceMap, parts)) {
    return Err(FormatError::InternalError);
  }

  // Find the approximately sign for ECMA-402.
  //
  // ICU doesn't provide a separate UNumberFormatFields for the approximately
  // sign, so we have to do some guess work to identify it. Thankfully ICU
  // doesn't currently use any sophisticated approach where to place the
  // approximately sign, but instead just places it before the first significant
  // number part as a literal string.
  //
  // We will have to revisit this code should ICU ever fix
  // <https://unicode-org.atlassian.net/browse/ICU-20163>.
  if (mFormatWithApprox && isIdenticalNumber) {
    bool foundSign = false;
    for (size_t i = 1; !foundSign && i < parts.length(); i++) {
      switch (parts[i].type) {
        // We assume the approximately sign can be placed before these types.
        case NumberPartType::Compact:
        case NumberPartType::Currency:
        case NumberPartType::Infinity:
        case NumberPartType::Integer:
        case NumberPartType::MinusSign:
        case NumberPartType::Nan:
        case NumberPartType::Percent:
        case NumberPartType::PlusSign:
        case NumberPartType::Unit: {
          auto& part = parts[i - 1];
          if (part.type == NumberPartType::Literal) {
            part.type = NumberPartType::ApproximatelySign;
            foundSign = true;
          }
          break;
        }

        // The remaining types can't start a number, so the approximately sign
        // can't be placed directly before them.
        case NumberPartType::ApproximatelySign:
        case NumberPartType::Decimal:
        case NumberPartType::ExponentInteger:
        case NumberPartType::ExponentMinusSign:
        case NumberPartType::ExponentSeparator:
        case NumberPartType::Fraction:
        case NumberPartType::Group:
        case NumberPartType::Literal:
          break;
      }
    }
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
