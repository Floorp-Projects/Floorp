/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_DisplayNames_h_
#define intl_components_DisplayNames_h_

#include <string>
#include <string_view>
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/uldnames.h"
#include "unicode/uloc.h"
#include "unicode/ucurr.h"
#include "mozilla/intl/Calendar.h"
#include "mozilla/intl/DateTimePatternGenerator.h"
#include "mozilla/intl/ICU4CGlue.h"
#include "mozilla/intl/Locale.h"
#include "mozilla/Buffer.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::intl {
/**
 * Provide more granular errors for DisplayNames rather than use the generic
 * ICUError type. This helps with providing more actionable feedback for
 * errors with input validation.
 *
 * This type can't be nested in the DisplayNames class because it needs the
 * UnusedZero and HasFreeLSB definitions.
 */
enum class DisplayNamesError {
  // Since we claim UnusedZero<DisplayNamesError>::value and
  // HasFreeLSB<Error>::value == true below, we must only use positive,
  // even enum values.
  InternalError = 2,
  OutOfMemory = 4,
  InvalidOption = 6,
  DuplicateVariantSubtag = 8,
  InvalidLanguageTag = 10,
};
}  // namespace mozilla::intl

namespace mozilla::detail {
// Ensure the efficient packing of the error types into the result. See
// ICUError.h and the ICUError comments for more information.
template <>
struct UnusedZero<intl::DisplayNamesError>
    : UnusedZeroEnum<intl::DisplayNamesError> {};

template <>
struct HasFreeLSB<intl::DisplayNamesError> {
  static constexpr bool value = true;
};
}  // namespace mozilla::detail

namespace mozilla::intl {

// NOTE: The UTF-35 canonical "code" value for months and quarters are 1-based
// integers, so some of the following enums are 1-based for consistency with
// that. For simplicity, we make all of the following enums 1-based, but use
// `EnumToIndex` (see below) to convert to zero based if indexing into internal
// (non-ICU) tables.

/**
 * Month choices for display names.
 */
enum class Month : uint8_t {
  January = 1,
  February,
  March,
  April,
  May,
  June,
  July,
  August,
  September,
  October,
  November,
  December,
  // Some calendar systems feature a 13th month.
  // https://en.wikipedia.org/wiki/Undecimber
  Undecimber
};

/**
 * Quarter choices for display names.
 */
enum class Quarter : uint8_t {
  Q1 = 1,
  Q2,
  Q3,
  Q4,
};

/**
 * Day period choices for display names.
 */
enum class DayPeriod : uint8_t {
  AM = 1,
  PM,
};

/**
 * DateTimeField choices for display names.
 */
enum class DateTimeField : uint8_t {
  Era = 1,
  Year,
  Quarter,
  Month,
  WeekOfYear,
  Weekday,
  Day,
  DayPeriod,
  Hour,
  Minute,
  Second,
  TimeZoneName,
};

/**
 * DisplayNames provide a way to get the localized names of various types of
 * information such as the names of the day of the week, months, currency etc.
 *
 * This class backs SpiderMonkeys implementation of Intl.DisplayNames
 * https://tc39.es/ecma402/#intl-displaynames-objects
 */
class DisplayNames final {
 public:
  /**
   * The style of the display name, specified by the amount of space available
   * for displaying the text.
   */
  enum class Style {
    Narrow,
    Short,
    Long,
    // Note: Abbreviated is not part of ECMA-402, but it is available for
    // internal Mozilla usage.
    Abbreviated,
  };

  /**
   * Use either standard or dialect names for the "Language" type.
   */
  enum class LanguageDisplay {
    Standard,
    Dialect,
  };

  /**
   * Determines the fallback behavior if no match is found.
   */
  enum class Fallback {
    // The buffer will contain an empty string.
    None,
    // The buffer will contain the code, but typically in a canonicalized form.
    Code
  };

  /**
   * These options correlate to the ECMA-402 DisplayNames options. The defaults
   * values must match the default initialized values of ECMA-402. The type
   * option is omitted as the C++ API relies on directly calling the
   * DisplayNames::Get* methods.
   *
   * https://tc39.es/ecma402/#intl-displaynames-objects
   * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/DisplayNames
   */
  struct Options {
    Style style = Style::Long;
    LanguageDisplay languageDisplay = LanguageDisplay::Standard;
  };

  DisplayNames(ULocaleDisplayNames* aDisplayNames, Span<const char> aLocale,
               Options aOptions)
      : mOptions(aOptions), mULocaleDisplayNames(aDisplayNames) {
    MOZ_ASSERT(aDisplayNames);

    // Copy the span and ensure null termination.
    mLocale = Buffer<char>(aLocale.size() + 1);
    PodCopy(mLocale.begin(), aLocale.data(), aLocale.size());
    mLocale[aLocale.size()] = '\0';
  }

  /**
   * Initialize a new DisplayNames for the provided locale and using the
   * provided options.
   *
   * https://tc39.es/ecma402/#sec-Intl.DisplayNames
   */
  static Result<UniquePtr<DisplayNames>, ICUError> TryCreate(
      const char* aLocale, Options aOptions);

  // Not copyable or movable
  DisplayNames(const DisplayNames&) = delete;
  DisplayNames& operator=(const DisplayNames&) = delete;

  ~DisplayNames();

  /**
   * Easily convert to a more specific DisplayNames error.
   */
  DisplayNamesError ToError(ICUError aError) const;

  /**
   * Easily convert to a more specific DisplayNames error.
   */
  DisplayNamesError ToError(Locale::CanonicalizationError aError) const;

 private:
  /**
   * A helper function to handle the fallback behavior, where if there is a
   * fallback the buffer is filled with the "code", often in canonicalized form.
   */
  template <typename B, typename Fn>
  static Result<Ok, DisplayNamesError> HandleFallback(B& aBuffer,
                                                      Fallback aFallback,
                                                      Fn aGetFallbackSpan) {
    if (aBuffer.length() == 0 &&
        aFallback == mozilla::intl::DisplayNames::Fallback::Code) {
      if (!FillBuffer(aGetFallbackSpan(), aBuffer)) {
        return Err(DisplayNamesError::OutOfMemory);
      }
    }
    return Ok();
  }

  /**
   * This is a specialized form of the FillBufferWithICUCall for DisplayNames.
   * Different APIs report that no display name is found with different
   * statuses. This method signals no display name was found by setting the
   * buffer to 0.
   *
   * The display name APIs such as `uldn_scriptDisplayName`,
   * `uloc_getDisplayScript`, and `uldn_regionDisplayName` report
   * U_ILLEGAL_ARGUMENT_ERROR when no display name was found. In order to
   * accomodate fallbacking, return an empty string in this case.
   */
  template <typename B, typename F>
  static ICUResult FillBufferWithICUDisplayNames(
      B& aBuffer, UErrorCode aNoDisplayNameStatus, F aCallback) {
    return FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          int32_t res = aCallback(target, length, status);

          if (*status == aNoDisplayNameStatus) {
            *status = U_ZERO_ERROR;
            res = 0;
          }
          return res;
        });
  }

  /**
   * An internal helper to compute the list of display names for various
   * DateTime options.
   */
  Result<Ok, DisplayNamesError> ComputeDateTimeDisplayNames(
      UDateFormatSymbolType symbolType, mozilla::Span<const int32_t> indices,
      Span<const char> aCalendar);

  // The following are the stack-allocated sizes for various strings using the
  // mozilla::Vector. The numbers should be large enough to fit the common
  // cases, and when the strings are too large they will fall back to heap
  // allocations.

  // Fit BCP 47 locales such as "en-US", "zh-Hant". Locales can get quite long,
  // but 32 should fit most smaller locales without a lot of extensions.
  static constexpr size_t LocaleVecLength = 32;
  // Fit calendar names such as "gregory", "buddhist", "islamic-civil".
  // "islamic-umalqura" is 16 bytes + 1 for null termination, so round up to 32.
  static constexpr size_t CalendarVecLength = 32;

  /**
   * Given an ASCII alpha, convert it to upper case.
   */
  static inline char16_t AsciiAlphaToUpperCase(char16_t aCh) {
    MOZ_ASSERT(IsAsciiAlpha(aCh));
    return AsciiToUpperCase(aCh);
  };

  /**
   * Attempt to use enums to safely index into an array.
   *
   * Note: The enums we support here are all defined starting from 1.
   */
  template <typename T>
  inline int32_t EnumToIndex(size_t aSize, T aEnum) {
    size_t index = static_cast<size_t>(aEnum) - 1;
    MOZ_RELEASE_ASSERT(index < aSize,
                       "Enum indexing mismatch for display names.");
    return index;
  }

  /**
   * Convert the month to a numeric code as a string.
   */
  static Span<const char> ToCodeString(Month aMonth);

 public:
  /**
   * Get the localized name of a language. Part of ECMA-402.
   *
   * Accepts:
   *  languageCode ["-" scriptCode] ["-" regionCode ] *("-" variant )
   *  Where the language code is:
   *    1. A two letters ISO 639-1 language code
   *         https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
   *    2. A three letters ISO 639-2 language code
   *         https://en.wikipedia.org/wiki/List_of_ISO_639-2_codes
   *
   * Examples:
   *  "es-ES" => "European Spanish" (en-US), "español de España" (es-ES)
   *  "zh-Hant" => "Traditional Chinese" (en-US), "chino tradicional" (es-ES)
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetLanguage(
      B& aBuffer, Span<const char> aLanguage,
      Fallback aFallback = Fallback::None) const {
    static_assert(std::is_same<typename B::CharType, char16_t>::value);
    mozilla::intl::Locale tag;
    if (LocaleParser::TryParseBaseName(aLanguage, tag).isErr()) {
      return Err(DisplayNamesError::InvalidOption);
    }

    {
      // ICU always canonicalizes the input locale, but since we know that ICU's
      // canonicalization is incomplete, we need to perform our own
      // canonicalization to ensure consistent result.
      auto result = tag.CanonicalizeBaseName();
      if (result.isErr()) {
        return Err(ToError(result.unwrapErr()));
      }
    }

    Vector<char, DisplayNames::LocaleVecLength> tagVec;
    {
      VectorToBufferAdaptor tagBuffer(tagVec);
      auto result = tag.ToString(tagBuffer);
      if (result.isErr()) {
        return Err(ToError(result.unwrapErr()));
      }
      if (!tagVec.append('\0')) {
        // The tag should be null terminated.
        return Err(DisplayNamesError::OutOfMemory);
      }
    }

    auto result = FillBufferWithICUDisplayNames(
        aBuffer, U_ILLEGAL_ARGUMENT_ERROR,
        [&](UChar* target, int32_t length, UErrorCode* status) {
          return uldn_localeDisplayName(mULocaleDisplayNames.GetConst(),
                                        tagVec.begin(), target, length, status);
        });
    if (result.isErr()) {
      return Err(ToError(result.unwrapErr()));
    }

    return HandleFallback(aBuffer, aFallback, [&] {
      // Remove the null terminator.
      return Span(tagVec.begin(), tagVec.length() - 1);
    });
  };

  /**
   * Get the localized name of a region. Part of ECMA-402.
   *
   * Accepts:
   *  1. an ISO-3166 two letters:
   *      https://www.iso.org/iso-3166-country-codes.html
   *  2. region code, or a three digits UN M49 Geographic Regions.
   *      https://unstats.un.org/unsd/methodology/m49/
   *
   * Examples
   *  "US"  => "United States" (en-US), "Estados Unidos", (es-ES)
   *  "158" => "Taiwan" (en-US), "Taiwán", (es-ES)
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetRegion(
      B& aBuffer, Span<const char> aCode,
      Fallback aFallback = Fallback::None) const {
    static_assert(std::is_same<typename B::CharType, char16_t>::value);

    mozilla::intl::RegionSubtag region;
    if (!IsStructurallyValidRegionTag(aCode)) {
      return Err(DisplayNamesError::InvalidOption);
    }
    region.Set(aCode);

    mozilla::intl::Locale tag;
    tag.SetLanguage("und");
    tag.SetRegion(region);

    {
      // ICU always canonicalizes the input locale, but since we know that ICU's
      // canonicalization is incomplete, we need to perform our own
      // canonicalization to ensure consistent result.
      auto result = tag.CanonicalizeBaseName();
      if (result.isErr()) {
        return Err(ToError(result.unwrapErr()));
      }
    }

    MOZ_ASSERT(tag.Region().Present());

    // Note: ICU requires the region subtag to be in canonical case.
    const mozilla::intl::RegionSubtag& canonicalRegion = tag.Region();

    char regionChars[mozilla::intl::LanguageTagLimits::RegionLength + 1] = {};
    std::copy_n(canonicalRegion.Span().data(), canonicalRegion.Length(),
                regionChars);

    auto result = FillBufferWithICUDisplayNames(
        aBuffer, U_ILLEGAL_ARGUMENT_ERROR,
        [&](UChar* chars, uint32_t size, UErrorCode* status) {
          return uldn_regionDisplayName(
              mULocaleDisplayNames.GetConst(), regionChars, chars,
              AssertedCast<int32_t, uint32_t>(size), status);
        });

    if (result.isErr()) {
      return Err(ToError(result.unwrapErr()));
    }

    return HandleFallback(aBuffer, aFallback, [&] {
      region.ToUpperCase();
      return region.Span();
    });
  }

  /**
   * Get the localized name of a currency. Part of ECMA-402.
   *
   * Accepts:
   *   A 3-letter ISO 4217 currency code.
   *   https://en.wikipedia.org/wiki/ISO_4217
   *
   * Examples:
   *   "EUR" => "Euro" (en-US), "euro" (es_ES), "欧元", (zh)
   *   "JPY" => "Japanese Yen" (en-US), "yen" (es_ES), "日元", (zh)
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetCurrency(
      B& aBuffer, Span<const char> aCurrency,
      Fallback aFallback = Fallback::None) const {
    static_assert(std::is_same<typename B::CharType, char16_t>::value);
    if (aCurrency.size() != 3) {
      return Err(DisplayNamesError::InvalidOption);
    }

    if (!mozilla::IsAsciiAlpha(aCurrency[0]) ||
        !mozilla::IsAsciiAlpha(aCurrency[1]) ||
        !mozilla::IsAsciiAlpha(aCurrency[2])) {
      return Err(DisplayNamesError::InvalidOption);
    }

    // Normally this type of operation wouldn't be safe, but ASCII characters
    // all take 1 byte in UTF-8 encoding, and can be zero padded to be valid
    // UTF-16. Currency codes are all three ASCII letters.
    // Normalize to upper case so we can easily detect the fallback case.
    char16_t currency[] = {AsciiAlphaToUpperCase(aCurrency[0]),
                           AsciiAlphaToUpperCase(aCurrency[1]),
                           AsciiAlphaToUpperCase(aCurrency[2]), u'\0'};

    UCurrNameStyle style;
    switch (mOptions.style) {
      case Style::Long:
        style = UCURR_LONG_NAME;
        break;
      case Style::Abbreviated:
      case Style::Short:
        style = UCURR_SYMBOL_NAME;
        break;
      case Style::Narrow:
        style = UCURR_NARROW_SYMBOL_NAME;
        break;
    }

    int32_t length = 0;
    UErrorCode status = U_ZERO_ERROR;
    const char16_t* name = ucurr_getName(currency, IcuLocale(mLocale), style,
                                         nullptr, &length, &status);
    if (U_FAILURE(status)) {
      return Err(DisplayNamesError::InternalError);
    }

    // No localized currency name was found when the error code is
    // U_USING_DEFAULT_WARNING and the returned string is equal to the (upper
    // case transformed) currency code. When `aFallback` is `Fallback::Code`,
    // we don't have to perform any additional work, because ICU already
    // returned the currency code in its normalized, upper case form.
    if (aFallback == DisplayNames::Fallback::None &&
        status == U_USING_DEFAULT_WARNING && length == 3 &&
        std::u16string_view{name, 3} == std::u16string_view{currency, 3}) {
      if (aBuffer.length() != 0) {
        // Ensure an empty string is in the buffer when there is no fallback.
        aBuffer.written(0);
      }
      return Ok();
    }

    if (!FillBuffer(Span(name, length), aBuffer)) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    return Ok();
  }

  /**
   * Get the localized name of a script. Part of ECMA-402.
   *
   * Accepts:
   *   ECMA-402 expects the ISO-15924 four letters script code.
   *   https://unicode.org/iso15924/iso15924-codes.html
   *   e.g. "Latn"
   *
   * Examples:
   *   "Cher" => "Cherokee" (en-US), "cherokee" (es-ES)
   *   "Latn" => "Latin" (en-US), "latino" (es-ES)
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetScript(
      B& aBuffer, Span<const char> aScript,
      Fallback aFallback = Fallback::None) const {
    static_assert(std::is_same<typename B::CharType, char16_t>::value);
    mozilla::intl::ScriptSubtag script;
    if (!IsStructurallyValidScriptTag(aScript)) {
      return Err(DisplayNamesError::InvalidOption);
    }
    script.Set(aScript);

    mozilla::intl::Locale tag;
    tag.SetLanguage("und");

    tag.SetScript(script);

    {
      // ICU always canonicalizes the input locale, but since we know that ICU's
      // canonicalization is incomplete, we need to perform our own
      // canonicalization to ensure consistent result.
      auto result = tag.CanonicalizeBaseName();
      if (result.isErr()) {
        return Err(ToError(result.unwrapErr()));
      }
    }

    MOZ_ASSERT(tag.Script().Present());
    mozilla::Vector<char, DisplayNames::LocaleVecLength> tagString;
    VectorToBufferAdaptor buffer(tagString);

    switch (mOptions.style) {
      case Style::Long: {
        // |uldn_scriptDisplayName| doesn't use the stand-alone form for script
        // subtags, so we're using |uloc_getDisplayScript| instead. (This only
        // applies to the long form.)
        //
        // ICU bug: https://unicode-org.atlassian.net/browse/ICU-9301

        // |uloc_getDisplayScript| expects a full locale identifier as its
        // input.
        if (auto result = tag.ToString(buffer); result.isErr()) {
          return Err(ToError(result.unwrapErr()));
        }

        // Null terminate the tag string.
        if (!tagString.append('\0')) {
          return Err(DisplayNamesError::OutOfMemory);
        }

        auto result = FillBufferWithICUDisplayNames(
            aBuffer, U_USING_DEFAULT_WARNING,
            [&](UChar* target, int32_t length, UErrorCode* status) {
              return uloc_getDisplayScript(tagString.begin(),
                                           IcuLocale(mLocale), target, length,
                                           status);
            });

        if (result.isErr()) {
          return Err(ToError(result.unwrapErr()));
        }
        break;
      }
      case Style::Abbreviated:
      case Style::Short:
      case Style::Narrow: {
        // Note: ICU requires the script subtag to be in canonical case.
        const mozilla::intl::ScriptSubtag& canonicalScript = tag.Script();

        char scriptChars[mozilla::intl::LanguageTagLimits::ScriptLength + 1] =
            {};
        MOZ_ASSERT(canonicalScript.Length() <=
                   mozilla::intl::LanguageTagLimits::ScriptLength + 1);
        std::copy_n(canonicalScript.Span().data(), canonicalScript.Length(),
                    scriptChars);

        auto result = FillBufferWithICUDisplayNames(
            aBuffer, U_ILLEGAL_ARGUMENT_ERROR,
            [&](UChar* target, int32_t length, UErrorCode* status) {
              return uldn_scriptDisplayName(mULocaleDisplayNames.GetConst(),
                                            scriptChars, target, length,
                                            status);
            });

        if (result.isErr()) {
          return Err(ToError(result.unwrapErr()));
        }
        break;
      }
    }

    return HandleFallback(aBuffer, aFallback, [&] {
      script.ToTitleCase();
      return script.Span();
    });
  };

  /**
   * Get the localized name of a calendar.
   * Part of Intl.DisplayNames V2. https://tc39.es/intl-displaynames-v2/
   * Accepts:
   *   Unicode calendar key:
   *   https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/Locale/calendar#unicode_calendar_keys
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetCalendar(
      B& aBuffer, Span<const char> aCalendar,
      Fallback aFallback = Fallback::None) const {
    if (aCalendar.empty() || !IsAscii(aCalendar)) {
      return Err(DisplayNamesError::InvalidOption);
    }

    if (LocaleParser::CanParseUnicodeExtensionType(aCalendar).isErr()) {
      return Err(DisplayNamesError::InvalidOption);
    }

    // Convert into canonical case before searching for replacements.
    Vector<char, DisplayNames::CalendarVecLength> lowerCaseCalendar;
    for (size_t i = 0; i < aCalendar.size(); i++) {
      if (!lowerCaseCalendar.append(AsciiToLowerCase(aCalendar[i]))) {
        return Err(DisplayNamesError::OutOfMemory);
      }
    }
    if (!lowerCaseCalendar.append('\0')) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    Span<const char> canonicalCalendar = mozilla::Span(
        lowerCaseCalendar.begin(), lowerCaseCalendar.length() - 1);

    // Search if there's a replacement for the Unicode calendar keyword.
    {
      Span<const char> key = mozilla::MakeStringSpan("ca");
      Span<const char> type = canonicalCalendar;
      if (const char* replacement =
              mozilla::intl::Locale::ReplaceUnicodeExtensionType(key, type)) {
        canonicalCalendar = MakeStringSpan(replacement);
      }
    }

    // The input calendar name is user-controlled, so be extra cautious before
    // passing arbitrarily large strings to ICU.
    static constexpr size_t maximumCalendarLength = 100;

    if (canonicalCalendar.size() <= maximumCalendarLength) {
      // |uldn_keyValueDisplayName| expects old-style keyword values.
      if (const char* legacyCalendar =
              uloc_toLegacyType("calendar", canonicalCalendar.Elements())) {
        auto result = FillBufferWithICUDisplayNames(
            aBuffer, U_ILLEGAL_ARGUMENT_ERROR,
            [&](UChar* chars, uint32_t size, UErrorCode* status) {
              // |uldn_keyValueDisplayName| expects old-style keyword values.
              return uldn_keyValueDisplayName(mULocaleDisplayNames.GetConst(),
                                              "calendar", legacyCalendar, chars,
                                              size, status);
            });
        if (result.isErr()) {
          return Err(ToError(result.unwrapErr()));
        }
      } else {
        aBuffer.written(0);
      }
    } else {
      aBuffer.written(0);
    }

    return HandleFallback(aBuffer, aFallback,
                          [&] { return canonicalCalendar; });
  }

  /**
   * Get the localized name of a weekday. This is a MozExtension, and not
   * currently part of ECMA-402.
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetWeekday(
      B& aBuffer, Weekday aWeekday, Span<const char> aCalendar,
      Fallback aFallback = Fallback::None) {
    // SpiderMonkey static casts the enum, so ensure it is correctly in range.
    MOZ_ASSERT(aWeekday >= Weekday::Monday && aWeekday <= Weekday::Sunday);

    UDateFormatSymbolType symbolType;
    switch (mOptions.style) {
      case DisplayNames::Style::Long:
        symbolType = UDAT_STANDALONE_WEEKDAYS;
        break;

      case DisplayNames::Style::Abbreviated:
        // ICU "short" is CLDR "abbreviated" format.
        symbolType = UDAT_STANDALONE_SHORT_WEEKDAYS;
        break;

      case DisplayNames::Style::Short:
        // ICU "shorter" is CLDR "short" format.
        symbolType = UDAT_STANDALONE_SHORTER_WEEKDAYS;
        break;

      case DisplayNames::Style::Narrow:
        symbolType = UDAT_STANDALONE_NARROW_WEEKDAYS;
        break;
    }

    static constexpr int32_t indices[] = {
        UCAL_MONDAY, UCAL_TUESDAY,  UCAL_WEDNESDAY, UCAL_THURSDAY,
        UCAL_FRIDAY, UCAL_SATURDAY, UCAL_SUNDAY};

    if (auto result = ComputeDateTimeDisplayNames(
            symbolType, mozilla::Span(indices), aCalendar);
        result.isErr()) {
      return result.propagateErr();
    }
    MOZ_ASSERT(mDateTimeDisplayNames.length() == std::size(indices));

    auto& name =
        mDateTimeDisplayNames[EnumToIndex(std::size(indices), aWeekday)];
    if (!FillBuffer(name.AsSpan(), aBuffer)) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    // There is no need to fallback, as invalid options are
    // DisplayNamesError::InvalidOption.
    return Ok();
  }

  /**
   * Get the localized name of a month. This is a MozExtension, and not
   * currently part of ECMA-402.
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetMonth(B& aBuffer, Month aMonth,
                                         Span<const char> aCalendar,
                                         Fallback aFallback = Fallback::None) {
    // SpiderMonkey static casts the enum, so ensure it is correctly in range.
    MOZ_ASSERT(aMonth >= Month::January && aMonth <= Month::Undecimber);

    UDateFormatSymbolType symbolType;
    switch (mOptions.style) {
      case DisplayNames::Style::Long:
        symbolType = UDAT_STANDALONE_MONTHS;
        break;

      case DisplayNames::Style::Abbreviated:
      case DisplayNames::Style::Short:
        symbolType = UDAT_STANDALONE_SHORT_MONTHS;
        break;

      case DisplayNames::Style::Narrow:
        symbolType = UDAT_STANDALONE_NARROW_MONTHS;
        break;
    }

    static constexpr int32_t indices[] = {
        UCAL_JANUARY,   UCAL_FEBRUARY, UCAL_MARCH,    UCAL_APRIL,
        UCAL_MAY,       UCAL_JUNE,     UCAL_JULY,     UCAL_AUGUST,
        UCAL_SEPTEMBER, UCAL_OCTOBER,  UCAL_NOVEMBER, UCAL_DECEMBER,
        UCAL_UNDECIMBER};

    if (auto result = ComputeDateTimeDisplayNames(
            symbolType, mozilla::Span(indices), aCalendar);
        result.isErr()) {
      return result.propagateErr();
    }
    MOZ_ASSERT(mDateTimeDisplayNames.length() == std::size(indices));
    auto& name = mDateTimeDisplayNames[EnumToIndex(std::size(indices), aMonth)];
    if (!FillBuffer(Span(name.AsSpan()), aBuffer)) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    return HandleFallback(aBuffer, aFallback,
                          [&] { return ToCodeString(aMonth); });
  }

  /**
   * Get the localized name of a quarter. This is a MozExtension, and not
   * currently part of ECMA-402.
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetQuarter(
      B& aBuffer, Quarter aQuarter, Span<const char> aCalendar,
      Fallback aFallback = Fallback::None) {
    // SpiderMonkey static casts the enum, so ensure it is correctly in range.
    MOZ_ASSERT(aQuarter >= Quarter::Q1 && aQuarter <= Quarter::Q4);

    UDateFormatSymbolType symbolType;
    switch (mOptions.style) {
      case DisplayNames::Style::Long:
        symbolType = UDAT_STANDALONE_QUARTERS;
        break;

      case DisplayNames::Style::Abbreviated:
      case DisplayNames::Style::Short:
        symbolType = UDAT_STANDALONE_SHORT_QUARTERS;
        break;

      case DisplayNames::Style::Narrow:
        symbolType = UDAT_STANDALONE_NARROW_QUARTERS;
        break;
    }

    // ICU doesn't provide an enum for quarters.
    static constexpr int32_t indices[] = {0, 1, 2, 3};

    if (auto result = ComputeDateTimeDisplayNames(
            symbolType, mozilla::Span(indices), aCalendar);
        result.isErr()) {
      return result.propagateErr();
    }
    MOZ_ASSERT(mDateTimeDisplayNames.length() == std::size(indices));

    auto& name =
        mDateTimeDisplayNames[EnumToIndex(std::size(indices), aQuarter)];
    if (!FillBuffer(Span(name.AsSpan()), aBuffer)) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    // There is no need to fallback, as invalid options are
    // DisplayNamesError::InvalidOption.
    return Ok();
  }

  /**
   * Get the localized name of a day period. This is a MozExtension, and not
   * currently part of ECMA-402.
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetDayPeriod(
      B& aBuffer, DayPeriod aDayPeriod, Span<const char> aCalendar,
      Fallback aFallback = Fallback::None) {
    UDateFormatSymbolType symbolType = UDAT_AM_PMS;

    static constexpr int32_t indices[] = {UCAL_AM, UCAL_PM};

    if (auto result = ComputeDateTimeDisplayNames(
            symbolType, mozilla::Span(indices), aCalendar);
        result.isErr()) {
      return result.propagateErr();
    }
    MOZ_ASSERT(mDateTimeDisplayNames.length() == std::size(indices));

    auto& name =
        mDateTimeDisplayNames[EnumToIndex(std::size(indices), aDayPeriod)];
    if (!FillBuffer(name.AsSpan(), aBuffer)) {
      return Err(DisplayNamesError::OutOfMemory);
    }

    // There is no need to fallback, as invalid options are
    // DisplayNamesError::InvalidOption.
    return Ok();
  }

  /**
   * Get the localized name of a date time field.
   * Part of Intl.DisplayNames V2. https://tc39.es/intl-displaynames-v2/
   * Accepts:
   *    "era", "year", "quarter", "month", "weekOfYear", "weekday", "day",
   *    "dayPeriod", "hour", "minute", "second", "timeZoneName"
   * Examples:
   *   "weekday" => "day of the week"
   *   "dayPeriod" => "AM/PM"
   */
  template <typename B>
  Result<Ok, DisplayNamesError> GetDateTimeField(
      B& aBuffer, DateTimeField aField,
      DateTimePatternGenerator& aDateTimePatternGen,
      Fallback aFallback = Fallback::None) {
    UDateTimePatternField field;
    switch (aField) {
      case DateTimeField::Era:
        field = UDATPG_ERA_FIELD;
        break;
      case DateTimeField::Year:
        field = UDATPG_YEAR_FIELD;
        break;
      case DateTimeField::Quarter:
        field = UDATPG_QUARTER_FIELD;
        break;
      case DateTimeField::Month:
        field = UDATPG_MONTH_FIELD;
        break;
      case DateTimeField::WeekOfYear:
        field = UDATPG_WEEK_OF_YEAR_FIELD;
        break;
      case DateTimeField::Weekday:
        field = UDATPG_WEEKDAY_FIELD;
        break;
      case DateTimeField::Day:
        field = UDATPG_DAY_FIELD;
        break;
      case DateTimeField::DayPeriod:
        field = UDATPG_DAYPERIOD_FIELD;
        break;
      case DateTimeField::Hour:
        field = UDATPG_HOUR_FIELD;
        break;
      case DateTimeField::Minute:
        field = UDATPG_MINUTE_FIELD;
        break;
      case DateTimeField::Second:
        field = UDATPG_SECOND_FIELD;
        break;
      case DateTimeField::TimeZoneName:
        field = UDATPG_ZONE_FIELD;
        break;
    }

    UDateTimePGDisplayWidth width;
    switch (mOptions.style) {
      case DisplayNames::Style::Long:
        width = UDATPG_WIDE;
        break;
      case DisplayNames::Style::Abbreviated:
      case DisplayNames::Style::Short:
        width = UDATPG_ABBREVIATED;
        break;
      case DisplayNames::Style::Narrow:
        width = UDATPG_NARROW;
        break;
    }

    auto result = FillBufferWithICUCall(
        aBuffer, [&](UChar* target, int32_t length, UErrorCode* status) {
          return udatpg_getFieldDisplayName(
              aDateTimePatternGen.GetUDateTimePatternGenerator(), field, width,
              target, length, status);
        });

    if (result.isErr()) {
      return Err(ToError(result.unwrapErr()));
    }
    // There is no need to fallback, as invalid options are
    // DisplayNamesError::InvalidOption.
    return Ok();
  }

  Options mOptions;
  Buffer<char> mLocale;
  Vector<Buffer<char16_t>> mDateTimeDisplayNames;
  ICUPointer<ULocaleDisplayNames> mULocaleDisplayNames =
      ICUPointer<ULocaleDisplayNames>(nullptr);
};

}  // namespace mozilla::intl

#endif
