/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZILLA_INTERNAL_API

#  include "mozilla/Assertions.h"
#  include "mozilla/UniquePtr.h"

#  include "ICUUtils.h"
#  include "mozilla/StaticPrefs_dom.h"
#  include "mozilla/intl/LocaleService.h"
#  include "mozilla/intl/FormatBuffer.h"
#  include "mozilla/intl/NumberFormat.h"
#  include "mozilla/intl/NumberParser.h"
#  include "nsIContent.h"
#  include "mozilla/dom/Document.h"
#  include "nsString.h"

using namespace mozilla;
using mozilla::intl::LocaleService;

void ICUUtils::LanguageTagIterForContent::GetNext(nsACString& aBCP47LangTag) {
  if (mCurrentFallbackIndex < 0) {
    mCurrentFallbackIndex = 0;
    // Try the language specified by a 'lang'/'xml:lang' attribute on mContent
    // or any ancestor, if such an attribute is specified:
    nsAutoString lang;
    mContent->GetLang(lang);
    if (!lang.IsEmpty()) {
      CopyUTF16toUTF8(lang, aBCP47LangTag);
      return;
    }
  }

  if (mCurrentFallbackIndex < 1) {
    mCurrentFallbackIndex = 1;
    // Else try the language specified by any Content-Language HTTP header or
    // pragma directive:
    nsAutoString lang;
    mContent->OwnerDoc()->GetContentLanguage(lang);
    if (!lang.IsEmpty()) {
      CopyUTF16toUTF8(lang, aBCP47LangTag);
      return;
    }
  }

  if (mCurrentFallbackIndex < 2) {
    mCurrentFallbackIndex = 2;
    // Else take the app's locale:

    nsAutoCString appLocale;
    LocaleService::GetInstance()->GetAppLocaleAsBCP47(aBCP47LangTag);
    return;
  }

  // TODO: Probably not worth it, but maybe have a fourth fallback to using
  // the OS locale?

  aBCP47LangTag.Truncate();  // Signal iterator exhausted
}

/* static */
bool ICUUtils::LocalizeNumber(double aValue,
                              LanguageTagIterForContent& aLangTags,
                              nsAString& aLocalizedValue) {
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");

  nsAutoCString langTag;
  aLangTags.GetNext(langTag);

  intl::NumberFormatOptions options;
  if (StaticPrefs::dom_forms_number_grouping()) {
    options.mGrouping = intl::NumberFormatOptions::Grouping::Always;
  } else {
    options.mGrouping = intl::NumberFormatOptions::Grouping::Never;
  }

  // ICU default is a maximum of 3 significant fractional digits. We don't
  // want that limit, so we set it to the maximum that a double can represent
  // (14-16 decimal fractional digits).
  options.mFractionDigits = Some(std::make_pair(0, 16));

  while (!langTag.IsEmpty()) {
    auto result = intl::NumberFormat::TryCreate(langTag.get(), options);
    if (result.isErr()) {
      aLangTags.GetNext(langTag);
      continue;
    }
    UniquePtr<intl::NumberFormat> nf = result.unwrap();
    intl::nsTStringToBufferAdapter adapter(aLocalizedValue);
    if (nf->format(aValue, adapter).isOk()) {
      return true;
    }

    aLangTags.GetNext(langTag);
  }
  return false;
}

/* static */
double ICUUtils::ParseNumber(const nsAString& aValue,
                             LanguageTagIterForContent& aLangTags) {
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");

  if (aValue.IsEmpty()) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  const Span<const char16_t> value(aValue.BeginReading(), aValue.Length());

  nsAutoCString langTag;
  aLangTags.GetNext(langTag);
  while (!langTag.IsEmpty()) {
    auto createResult = intl::NumberParser::TryCreate(
        langTag.get(), StaticPrefs::dom_forms_number_grouping());
    if (createResult.isErr()) {
      aLangTags.GetNext(langTag);
      continue;
    }
    UniquePtr<intl::NumberParser> np = createResult.unwrap();

    static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                  "Unexpected character size - the following cast is unsafe");
    auto parseResult = np->ParseDouble(value);
    if (parseResult.isOk()) {
      std::pair<double, int32_t> parsed = parseResult.unwrap();
      if (parsed.second == static_cast<int32_t>(value.Length())) {
        return parsed.first;
      }
    }
    aLangTags.GetNext(langTag);
  }
  return std::numeric_limits<float>::quiet_NaN();
}

/* static */
void ICUUtils::AssignUCharArrayToString(UChar* aICUString, int32_t aLength,
                                        nsAString& aMozString) {
  // Both ICU's UnicodeString and Mozilla's nsAString use UTF-16, so we can
  // cast here.

  static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                "Unexpected character size - the following cast is unsafe");

  aMozString.Assign((const nsAString::char_type*)aICUString, aLength);

  NS_ASSERTION((int32_t)aMozString.Length() == aLength, "Conversion failed");
}

/* static */
nsresult ICUUtils::ICUErrorToNsResult(const intl::ICUError aError) {
  switch (aError) {
    case intl::ICUError::OutOfMemory:
      return NS_ERROR_OUT_OF_MEMORY;

    default:
      return NS_ERROR_FAILURE;
  }
}

#endif /* MOZILLA_INTERNAL_API */
