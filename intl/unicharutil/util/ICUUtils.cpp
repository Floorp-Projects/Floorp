/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZILLA_INTERNAL_API
#ifdef ENABLE_INTL_API

#include "ICUUtils.h"
#include "mozilla/Preferences.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsStringGlue.h"
#include "unicode/uloc.h"
#include "unicode/unum.h"

using namespace mozilla;

/**
 * This pref just controls whether we format the number with grouping separator
 * characters when the internal value is set or updated. It does not stop the
 * user from typing in a number and using grouping separators.
 */
static bool gLocaleNumberGroupingEnabled;
static const char LOCALE_NUMBER_GROUPING_PREF_STR[] = "dom.forms.number.grouping";

static bool
LocaleNumberGroupingIsEnabled()
{
  static bool sInitialized = false;

  if (!sInitialized) {
    /* check and register ourselves with the pref */
    Preferences::AddBoolVarCache(&gLocaleNumberGroupingEnabled,
                                 LOCALE_NUMBER_GROUPING_PREF_STR,
                                 false);
    sInitialized = true;
  }

  return gLocaleNumberGroupingEnabled;
}

void
ICUUtils::LanguageTagIterForContent::GetNext(nsACString& aBCP47LangTag)
{
  if (mCurrentFallbackIndex < 0) {
    mCurrentFallbackIndex = 0;
    // Try the language specified by a 'lang'/'xml:lang' attribute on mContent
    // or any ancestor, if such an attribute is specified:
    nsAutoString lang;
    mContent->GetLang(lang);
    if (!lang.IsEmpty()) {
      aBCP47LangTag = NS_ConvertUTF16toUTF8(lang);
      return;
    }
  }

  if (mCurrentFallbackIndex < 1) {
    mCurrentFallbackIndex = 1;
    // Else try the language specified by any Content-Language HTTP header or
    // pragma directive:
    nsIDocument* doc = mContent->OwnerDoc();
    nsAutoString lang;
    doc->GetContentLanguage(lang);
    if (!lang.IsEmpty()) {
      aBCP47LangTag = NS_ConvertUTF16toUTF8(lang);
      return;
    }
  }

  if (mCurrentFallbackIndex < 2) {
    mCurrentFallbackIndex = 2;
    // Else try the user-agent's locale:
    nsCOMPtr<nsIToolkitChromeRegistry> cr =
      mozilla::services::GetToolkitChromeRegistryService();
    nsAutoCString uaLangTag;
    if (cr) {
      cr->GetSelectedLocale(NS_LITERAL_CSTRING("global"), uaLangTag);
    }
    if (!uaLangTag.IsEmpty()) {
      aBCP47LangTag = uaLangTag;
      return;
    }
  }

  // TODO: Probably not worth it, but maybe have a fourth fallback to using
  // the OS locale?

  aBCP47LangTag.Truncate(); // Signal iterator exhausted
}

/* static */ bool
ICUUtils::LocalizeNumber(double aValue,
                         LanguageTagIterForContent& aLangTags,
                         nsAString& aLocalizedValue)
{
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");

  static const int32_t kBufferSize = 256;

  UChar buffer[kBufferSize];

  nsAutoCString langTag;
  aLangTags.GetNext(langTag);
  while (!langTag.IsEmpty()) {
    UErrorCode status = U_ZERO_ERROR;
    AutoCloseUNumberFormat format(unum_open(UNUM_DECIMAL, nullptr, 0,
                                            langTag.get(), nullptr, &status));
    unum_setAttribute(format, UNUM_GROUPING_USED,
                      LocaleNumberGroupingIsEnabled());
    // ICU default is a maximum of 3 significant fractional digits. We don't
    // want that limit, so we set it to the maximum that a double can represent
    // (14-16 decimal fractional digits).
    unum_setAttribute(format, UNUM_MAX_FRACTION_DIGITS, 16);
    int32_t length = unum_formatDouble(format, aValue, buffer, kBufferSize,
                                       nullptr, &status);
    NS_ASSERTION(length < kBufferSize &&
                 status != U_BUFFER_OVERFLOW_ERROR &&
                 status != U_STRING_NOT_TERMINATED_WARNING,
                 "Need a bigger buffer?!");
    if (U_SUCCESS(status)) {
      ICUUtils::AssignUCharArrayToString(buffer, length, aLocalizedValue);
      return true;
    }
    aLangTags.GetNext(langTag);
  }
  return false;
}

/* static */ double
ICUUtils::ParseNumber(nsAString& aValue,
                      LanguageTagIterForContent& aLangTags)
{
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");

  if (aValue.IsEmpty()) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  uint32_t length = aValue.Length();

  nsAutoCString langTag;
  aLangTags.GetNext(langTag);
  while (!langTag.IsEmpty()) {
    UErrorCode status = U_ZERO_ERROR;
    AutoCloseUNumberFormat format(unum_open(UNUM_DECIMAL, nullptr, 0,
                                            langTag.get(), nullptr, &status));
    int32_t parsePos = 0;
    static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                  "Unexpected character size - the following cast is unsafe");
    double val = unum_parseDouble(format,
                                  (const UChar*)PromiseFlatString(aValue).get(),
                                  length, &parsePos, &status);
    if (U_SUCCESS(status) && parsePos == (int32_t)length) {
      return val;
    }
    aLangTags.GetNext(langTag);
  }
  return std::numeric_limits<float>::quiet_NaN();
}

/* static */ void
ICUUtils::AssignUCharArrayToString(UChar* aICUString,
                                   int32_t aLength,
                                   nsAString& aMozString)
{
  // Both ICU's UnicodeString and Mozilla's nsAString use UTF-16, so we can
  // cast here.

  static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                "Unexpected character size - the following cast is unsafe");

  aMozString.Assign((const nsAString::char_type*)aICUString, aLength);

  NS_ASSERTION((int32_t)aMozString.Length() == aLength, "Conversion failed");
}

#if 0
/* static */ Locale
ICUUtils::BCP47CodeToLocale(const nsAString& aBCP47Code)
{
  MOZ_ASSERT(!aBCP47Code.IsEmpty(), "Don't pass an empty BCP 47 code");

  Locale locale;
  locale.setToBogus();

  // BCP47 codes are guaranteed to be ASCII, so lossy conversion is okay
  NS_LossyConvertUTF16toASCII bcp47code(aBCP47Code);

  UErrorCode status = U_ZERO_ERROR;
  int32_t needed;

  char localeID[256];
  needed = uloc_forLanguageTag(bcp47code.get(), localeID,
                               PR_ARRAY_SIZE(localeID) - 1, nullptr,
                               &status);
  MOZ_ASSERT(needed < int32_t(PR_ARRAY_SIZE(localeID)) - 1,
             "Need a bigger buffer");
  if (needed <= 0 || U_FAILURE(status)) {
    return locale;
  }

  char lang[64];
  needed = uloc_getLanguage(localeID, lang, PR_ARRAY_SIZE(lang) - 1,
                            &status);
  MOZ_ASSERT(needed < int32_t(PR_ARRAY_SIZE(lang)) - 1,
             "Need a bigger buffer");
  if (needed <= 0 || U_FAILURE(status)) {
    return locale;
  }

  char country[64];
  needed = uloc_getCountry(localeID, country, PR_ARRAY_SIZE(country) - 1,
                           &status);
  MOZ_ASSERT(needed < int32_t(PR_ARRAY_SIZE(country)) - 1,
             "Need a bigger buffer");
  if (needed > 0 && U_SUCCESS(status)) {
    locale = Locale(lang, country);
  }

  if (locale.isBogus()) {
    // Using the country resulted in a bogus Locale, so try with only the lang
    locale = Locale(lang);
  }

  return locale;
}

/* static */ void
ICUUtils::ToMozString(UnicodeString& aICUString, nsAString& aMozString)
{
  // Both ICU's UnicodeString and Mozilla's nsAString use UTF-16, so we can
  // cast here.

  static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                "Unexpected character size - the following cast is unsafe");

  const nsAString::char_type* buf =
    (const nsAString::char_type*)aICUString.getTerminatedBuffer();
  aMozString.Assign(buf);

  NS_ASSERTION(aMozString.Length() == (uint32_t)aICUString.length(),
               "Conversion failed");
}

/* static */ void
ICUUtils::ToICUString(nsAString& aMozString, UnicodeString& aICUString)
{
  // Both ICU's UnicodeString and Mozilla's nsAString use UTF-16, so we can
  // cast here.

  static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                "Unexpected character size - the following cast is unsafe");

  aICUString.setTo((UChar*)PromiseFlatString(aMozString).get(),
                   aMozString.Length());

  NS_ASSERTION(aMozString.Length() == (uint32_t)aICUString.length(),
               "Conversion failed");
}
#endif

#endif /* ENABLE_INTL_API */
#endif /* MOZILLA_INTERNAL_API */

