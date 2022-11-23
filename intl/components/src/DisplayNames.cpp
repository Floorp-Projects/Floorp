/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/intl/DisplayNames.h"
#include "ScopedICUObject.h"

namespace mozilla::intl {

DisplayNames::~DisplayNames() {
  // The mDisplayNames will not exist when the DisplayNames is being
  // moved.
  if (auto* uldn = mULocaleDisplayNames.GetMut()) {
    uldn_close(uldn);
  }
}

DisplayNamesError DisplayNames::ToError(ICUError aError) const {
  switch (aError) {
    case ICUError::InternalError:
    case ICUError::OverflowError:
      return DisplayNamesError::InternalError;
    case ICUError::OutOfMemory:
      return DisplayNamesError::OutOfMemory;
  }
  MOZ_ASSERT_UNREACHABLE();
  return DisplayNamesError::InternalError;
}

DisplayNamesError DisplayNames::ToError(
    Locale::CanonicalizationError aError) const {
  switch (aError) {
    case Locale::CanonicalizationError::DuplicateVariant:
      return DisplayNamesError::DuplicateVariantSubtag;
    case Locale::CanonicalizationError::InternalError:
      return DisplayNamesError::InternalError;
    case Locale::CanonicalizationError::OutOfMemory:
      return DisplayNamesError::OutOfMemory;
  }
  MOZ_ASSERT_UNREACHABLE();
  return DisplayNamesError::InternalError;
}

/* static */
Result<UniquePtr<DisplayNames>, ICUError> DisplayNames::TryCreate(
    const char* aLocale, Options aOptions) {
  UErrorCode status = U_ZERO_ERROR;
  UDisplayContext contexts[] = {
      // Use either standard or dialect names.
      // For example either "English (GB)" or "British English".
      aOptions.languageDisplay == DisplayNames::LanguageDisplay::Standard
          ? UDISPCTX_STANDARD_NAMES
          : UDISPCTX_DIALECT_NAMES,

      // Assume the display names are used in a stand-alone context.
      UDISPCTX_CAPITALIZATION_FOR_STANDALONE,

      // Select either the long or short form. There's no separate narrow form
      // available in ICU, therefore we equate "narrow"/"short" styles here.
      aOptions.style == DisplayNames::Style::Long ? UDISPCTX_LENGTH_FULL
                                                  : UDISPCTX_LENGTH_SHORT,

      // Don't apply substitutes, because we need to apply our own fallbacks.
      UDISPCTX_NO_SUBSTITUTE,
  };

  const char* locale = IcuLocale(aLocale);

  ULocaleDisplayNames* uLocaleDisplayNames =
      uldn_openForContext(locale, contexts, std::size(contexts), &status);

  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }
  return MakeUnique<DisplayNames>(uLocaleDisplayNames, MakeStringSpan(locale),
                                  aOptions);
};

#ifdef DEBUG
static bool IsStandaloneMonth(UDateFormatSymbolType symbolType) {
  switch (symbolType) {
    case UDAT_STANDALONE_MONTHS:
    case UDAT_STANDALONE_SHORT_MONTHS:
    case UDAT_STANDALONE_NARROW_MONTHS:
      return true;

    case UDAT_ERAS:
    case UDAT_MONTHS:
    case UDAT_SHORT_MONTHS:
    case UDAT_WEEKDAYS:
    case UDAT_SHORT_WEEKDAYS:
    case UDAT_AM_PMS:
    case UDAT_LOCALIZED_CHARS:
    case UDAT_ERA_NAMES:
    case UDAT_NARROW_MONTHS:
    case UDAT_NARROW_WEEKDAYS:
    case UDAT_STANDALONE_WEEKDAYS:
    case UDAT_STANDALONE_SHORT_WEEKDAYS:
    case UDAT_STANDALONE_NARROW_WEEKDAYS:
    case UDAT_QUARTERS:
    case UDAT_SHORT_QUARTERS:
    case UDAT_STANDALONE_QUARTERS:
    case UDAT_STANDALONE_SHORT_QUARTERS:
    case UDAT_SHORTER_WEEKDAYS:
    case UDAT_STANDALONE_SHORTER_WEEKDAYS:
    case UDAT_CYCLIC_YEARS_WIDE:
    case UDAT_CYCLIC_YEARS_ABBREVIATED:
    case UDAT_CYCLIC_YEARS_NARROW:
    case UDAT_ZODIAC_NAMES_WIDE:
    case UDAT_ZODIAC_NAMES_ABBREVIATED:
    case UDAT_ZODIAC_NAMES_NARROW:
    case UDAT_NARROW_QUARTERS:
    case UDAT_STANDALONE_NARROW_QUARTERS:
      return false;
  }

  MOZ_ASSERT_UNREACHABLE("unenumerated, undocumented symbol type");
  return false;
}
#endif

Result<Ok, DisplayNamesError> DisplayNames::ComputeDateTimeDisplayNames(
    UDateFormatSymbolType symbolType, mozilla::Span<const int32_t> indices,
    Span<const char> aCalendar) {
  if (!mDateTimeDisplayNames.empty()) {
    // No need to re-compute the display names.
    return Ok();
  }
  mozilla::intl::Locale tag;
  // Do not use mLocale.AsSpan() as it includes the null terminator inside the
  // span.
  if (LocaleParser::TryParse(Span(mLocale.Elements(), mLocale.Length() - 1),
                             tag)
          .isErr()) {
    return Err(DisplayNamesError::InvalidLanguageTag);
  }

  if (!aCalendar.empty()) {
    // Add the calendar extension to the locale. This is only available via
    // the MozExtension.
    Vector<char, 32> extension;
    Span<const char> prefix = MakeStringSpan("u-ca-");
    if (!extension.append(prefix.data(), prefix.size()) ||
        !extension.append(aCalendar.data(), aCalendar.size())) {
      return Err(DisplayNamesError::OutOfMemory);
    }
    // This overwrites any other Unicode extensions, but should be okay to do
    // here.
    if (auto result = tag.SetUnicodeExtension(extension); result.isErr()) {
      return Err(ToError(result.unwrapErr()));
    }
  }

  constexpr char16_t* timeZone = nullptr;
  constexpr int32_t timeZoneLength = 0;

  constexpr char16_t* pattern = nullptr;
  constexpr int32_t patternLength = 0;

  Vector<char, DisplayNames::LocaleVecLength> localeWithCalendar;
  VectorToBufferAdaptor buffer(localeWithCalendar);
  if (auto result = tag.ToString(buffer); result.isErr()) {
    return Err(ToError(result.unwrapErr()));
  }
  if (!localeWithCalendar.append('\0')) {
    return Err(DisplayNamesError::OutOfMemory);
  }

  UErrorCode status = U_ZERO_ERROR;
  UDateFormat* fmt = udat_open(
      UDAT_DEFAULT, UDAT_DEFAULT,
      IcuLocale(
          // IcuLocale takes a Span that does not include the null terminator.
          Span(localeWithCalendar.begin(), localeWithCalendar.length() - 1)),
      timeZone, timeZoneLength, pattern, patternLength, &status);
  if (U_FAILURE(status)) {
    return Err(DisplayNamesError::InternalError);
  }
  ScopedICUObject<UDateFormat, udat_close> datToClose(fmt);

  Vector<char16_t, DisplayNames::LocaleVecLength> name;
  for (int32_t index : indices) {
    auto result = FillBufferWithICUCall(name, [&](UChar* target, int32_t length,
                                                  UErrorCode* status) {
      return udat_getSymbols(fmt, symbolType, index, target, length, status);
    });
    if (result.isErr()) {
      return Err(ToError(result.unwrapErr()));
    }

    // Everything except Undecimber should always have a non-empty name.
    MOZ_ASSERT_IF(!IsStandaloneMonth(symbolType) || index != UCAL_UNDECIMBER,
                  !name.empty());

    if (!mDateTimeDisplayNames.emplaceBack(Span(name.begin(), name.length()))) {
      return Err(DisplayNamesError::OutOfMemory);
    }
  }
  return Ok();
}

Span<const char> DisplayNames::ToCodeString(Month aMonth) {
  switch (aMonth) {
    case Month::January:
      return MakeStringSpan("1");
    case Month::February:
      return MakeStringSpan("2");
    case Month::March:
      return MakeStringSpan("3");
    case Month::April:
      return MakeStringSpan("4");
    case Month::May:
      return MakeStringSpan("5");
    case Month::June:
      return MakeStringSpan("6");
    case Month::July:
      return MakeStringSpan("7");
    case Month::August:
      return MakeStringSpan("8");
    case Month::September:
      return MakeStringSpan("9");
    case Month::October:
      return MakeStringSpan("10");
    case Month::November:
      return MakeStringSpan("11");
    case Month::December:
      return MakeStringSpan("12");
    case Month::Undecimber:
      return MakeStringSpan("13");
  }
  MOZ_ASSERT_UNREACHABLE();
  return MakeStringSpan("1");
};

}  // namespace mozilla::intl
