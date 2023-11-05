/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <string.h>
#include "mozilla/intl/Collator.h"

namespace mozilla::intl {

Collator::Collator(UCollator* aCollator) : mCollator(aCollator) {
  MOZ_ASSERT(aCollator);
}

Collator::~Collator() {
  if (mCollator.GetMut()) {
    ucol_close(mCollator.GetMut());
  }
}

Result<UniquePtr<Collator>, ICUError> Collator::TryCreate(const char* aLocale) {
  UErrorCode status = U_ZERO_ERROR;
  UCollator* collator = ucol_open(IcuLocale(aLocale), &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return MakeUnique<Collator>(collator);
};

int32_t Collator::CompareStrings(Span<const char16_t> aSource,
                                 Span<const char16_t> aTarget) const {
  switch (ucol_strcoll(mCollator.GetConst(), aSource.data(),
                       static_cast<int32_t>(aSource.size()), aTarget.data(),
                       static_cast<int32_t>(aTarget.size()))) {
    case UCOL_LESS:
      return -1;
    case UCOL_EQUAL:
      return 0;
    case UCOL_GREATER:
      return 1;
  }
  MOZ_ASSERT_UNREACHABLE("ucol_strcoll returned bad UCollationResult");
  return 0;
}

int32_t Collator::CompareSortKeys(Span<const uint8_t> aKey1,
                                  Span<const uint8_t> aKey2) const {
  size_t minLength = std::min(aKey1.Length(), aKey2.Length());
  int32_t tmpResult = strncmp((const char*)aKey1.Elements(),
                              (const char*)aKey2.Elements(), minLength);
  if (tmpResult < 0) {
    return -1;
  }
  if (tmpResult > 0) {
    return 1;
  }
  if (aKey1.Length() > minLength) {
    // First string contains second one, so comes later, hence return > 0.
    return 1;
  }
  if (aKey2.Length() > minLength) {
    // First string is a substring of second one, so comes earlier,
    // hence return < 0.
    return -1;
  }
  return 0;
}

static UColAttributeValue CaseFirstToICU(Collator::CaseFirst caseFirst) {
  switch (caseFirst) {
    case Collator::CaseFirst::False:
      return UCOL_OFF;
    case Collator::CaseFirst::Upper:
      return UCOL_UPPER_FIRST;
    case Collator::CaseFirst::Lower:
      return UCOL_LOWER_FIRST;
  }

  MOZ_ASSERT_UNREACHABLE();
  return UCOL_DEFAULT;
}

void Collator::SetStrength(Collator::Strength aStrength) {
  UColAttributeValue strength;
  switch (aStrength) {
    case Collator::Strength::Default:
      strength = UCOL_DEFAULT_STRENGTH;
      break;
    case Collator::Strength::Primary:
      strength = UCOL_PRIMARY;
      break;
    case Collator::Strength::Secondary:
      strength = UCOL_SECONDARY;
      break;
    case Collator::Strength::Tertiary:
      strength = UCOL_TERTIARY;
      break;
    case Collator::Strength::Quaternary:
      strength = UCOL_QUATERNARY;
      break;
    case Collator::Strength::Identical:
      strength = UCOL_IDENTICAL;
      break;
  }

  ucol_setStrength(mCollator.GetMut(), strength);
}

ICUResult Collator::SetCaseLevel(Collator::Feature aFeature) {
  UErrorCode status = U_ZERO_ERROR;
  ucol_setAttribute(mCollator.GetMut(), UCOL_CASE_LEVEL,
                    ToUColAttributeValue(aFeature), &status);
  return ToICUResult(status);
}

ICUResult Collator::SetAlternateHandling(
    Collator::AlternateHandling aAlternateHandling) {
  UErrorCode status = U_ZERO_ERROR;
  UColAttributeValue handling;
  switch (aAlternateHandling) {
    case Collator::AlternateHandling::NonIgnorable:
      handling = UCOL_NON_IGNORABLE;
      break;
    case Collator::AlternateHandling::Shifted:
      handling = UCOL_SHIFTED;
      break;
    case Collator::AlternateHandling::Default:
      handling = UCOL_DEFAULT;
      break;
  }

  ucol_setAttribute(mCollator.GetMut(), UCOL_ALTERNATE_HANDLING, handling,
                    &status);
  return ToICUResult(status);
}

ICUResult Collator::SetNumericCollation(Collator::Feature aFeature) {
  UErrorCode status = U_ZERO_ERROR;
  ucol_setAttribute(mCollator.GetMut(), UCOL_NUMERIC_COLLATION,
                    ToUColAttributeValue(aFeature), &status);
  return ToICUResult(status);
}

ICUResult Collator::SetNormalizationMode(Collator::Feature aFeature) {
  UErrorCode status = U_ZERO_ERROR;
  ucol_setAttribute(mCollator.GetMut(), UCOL_NORMALIZATION_MODE,
                    ToUColAttributeValue(aFeature), &status);
  return ToICUResult(status);
}

ICUResult Collator::SetCaseFirst(Collator::CaseFirst aCaseFirst) {
  UErrorCode status = U_ZERO_ERROR;
  ucol_setAttribute(mCollator.GetMut(), UCOL_CASE_FIRST,
                    CaseFirstToICU(aCaseFirst), &status);
  return ToICUResult(status);
}

ICUResult Collator::SetOptions(const Options& aOptions,
                               const Maybe<Options&> aPrevOptions) {
  if (aPrevOptions &&
      // Check the equality of the previous options.
      aPrevOptions->sensitivity == aOptions.sensitivity &&
      aPrevOptions->caseFirst == aOptions.caseFirst &&
      aPrevOptions->ignorePunctuation == aOptions.ignorePunctuation &&
      aPrevOptions->numeric == aOptions.numeric) {
    return Ok();
  }

  Collator::Strength strength = Collator::Strength::Default;
  Collator::Feature caseLevel = Collator::Feature::Off;
  switch (aOptions.sensitivity) {
    case Collator::Sensitivity::Base:
      strength = Collator::Strength::Primary;
      break;
    case Collator::Sensitivity::Accent:
      strength = Collator::Strength::Secondary;
      break;
    case Collator::Sensitivity::Case:
      caseLevel = Collator::Feature::On;
      strength = Collator::Strength::Primary;
      break;
    case Collator::Sensitivity::Variant:
      strength = Collator::Strength::Tertiary;
      break;
  }

  SetStrength(strength);

  ICUResult result = Ok();

  // According to the ICU team, UCOL_SHIFTED causes punctuation to be
  // ignored. Looking at Unicode Technical Report 35, Unicode Locale Data
  // Markup Language, "shifted" causes whitespace and punctuation to be
  // ignored - that's a bit more than asked for, but there's no way to get
  // less.
  result = this->SetAlternateHandling(
      aOptions.ignorePunctuation ? Collator::AlternateHandling::Shifted
                                 : Collator::AlternateHandling::NonIgnorable);
  if (result.isErr()) {
    return result;
  }

  result = SetCaseLevel(caseLevel);
  if (result.isErr()) {
    return result;
  }

  result = SetNumericCollation(aOptions.numeric ? Collator::Feature::On
                                                : Collator::Feature::Off);
  if (result.isErr()) {
    return result;
  }

  // Normalization is always on to meet the canonical equivalence requirement.
  result = SetNormalizationMode(Collator::Feature::On);
  if (result.isErr()) {
    return result;
  }

  result = SetCaseFirst(aOptions.caseFirst);
  if (result.isErr()) {
    return result;
  }
  return Ok();
}

Result<Collator::CaseFirst, ICUError> Collator::GetCaseFirst() const {
  UErrorCode status = U_ZERO_ERROR;
  UColAttributeValue caseFirst =
      ucol_getAttribute(mCollator.GetConst(), UCOL_CASE_FIRST, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  if (caseFirst == UCOL_OFF) {
    return CaseFirst::False;
  }
  if (caseFirst == UCOL_UPPER_FIRST) {
    return CaseFirst::Upper;
  }
  MOZ_ASSERT(caseFirst == UCOL_LOWER_FIRST);
  return CaseFirst::Lower;
}

Result<bool, ICUError> Collator::GetIgnorePunctuation() const {
  UErrorCode status = U_ZERO_ERROR;
  UColAttributeValue alternateHandling =
      ucol_getAttribute(mCollator.GetConst(), UCOL_ALTERNATE_HANDLING, &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  MOZ_ASSERT(alternateHandling == UCOL_SHIFTED ||
             alternateHandling == UCOL_NON_IGNORABLE);
  return alternateHandling == UCOL_SHIFTED;
}

/* static */
Result<Collator::Bcp47ExtEnumeration, ICUError>
Collator::GetBcp47KeywordValuesForLocale(const char* aLocale,
                                         CommonlyUsed aCommonlyUsed) {
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucol_getKeywordValuesForLocale(
      "collation", aLocale, static_cast<bool>(aCommonlyUsed), &status);

  if (U_SUCCESS(status)) {
    return Bcp47ExtEnumeration(enumeration);
  }

  return Err(ToICUError(status));
}

/* static */
Result<Collator::Bcp47ExtEnumeration, ICUError>
Collator::GetBcp47KeywordValues() {
  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* enumeration = ucol_getKeywordValues("collation", &status);

  if (U_SUCCESS(status)) {
    return Bcp47ExtEnumeration(enumeration);
  }

  return Err(ToICUError(status));
}

/* static */
SpanResult<char> Collator::KeywordValueToBcp47Extension(const char* aKeyword,
                                                        int32_t aLength) {
  if (aKeyword == nullptr) {
    return Err(InternalError{});
  }
  return MakeStringSpan(uloc_toUnicodeLocaleType("co", aKeyword));
}

}  // namespace mozilla::intl
