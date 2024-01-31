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

already_AddRefed<nsAtom> ICUUtils::LanguageTagIterForContent::GetNext() {
  if (mCurrentFallbackIndex < 0) {
    mCurrentFallbackIndex = 0;
    // Try the language specified by a 'lang'/'xml:lang' attribute on mContent
    // or any ancestor, if such an attribute is specified:
    if (auto* lang = mContent->GetLang()) {
      return do_AddRef(lang);
    }
  }

  if (mCurrentFallbackIndex < 1) {
    mCurrentFallbackIndex = 1;
    // Else try the language specified by any Content-Language HTTP header or
    // pragma directive:
    if (nsAtom* lang = mContent->OwnerDoc()->GetContentLanguage()) {
      return do_AddRef(lang);
    }
  }

  if (mCurrentFallbackIndex < 2) {
    mCurrentFallbackIndex = 2;
    // Else take the app's locale:
    nsAutoCString appLocale;
    LocaleService::GetInstance()->GetAppLocaleAsBCP47(appLocale);
    return NS_Atomize(appLocale);
  }

  // TODO: Probably not worth it, but maybe have a fourth fallback to using
  // the OS locale?
  return nullptr;
}

/* static */
bool ICUUtils::LocalizeNumber(double aValue,
                              LanguageTagIterForContent& aLangTags,
                              nsAString& aLocalizedValue) {
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");
  MOZ_ASSERT(NS_IsMainThread());
  using LangToFormatterCache =
      nsTHashMap<RefPtr<nsAtom>, UniquePtr<intl::NumberFormat>>;

  static StaticAutoPtr<LangToFormatterCache> sCache;
  if (!sCache) {
    sCache = new LangToFormatterCache();
    ClearOnShutdown(&sCache);
  }

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

  while (RefPtr<nsAtom> langTag = aLangTags.GetNext()) {
    auto& formatter = sCache->LookupOrInsertWith(langTag, [&] {
      nsAutoCString tag;
      langTag->ToUTF8String(tag);
      return intl::NumberFormat::TryCreate(tag, options).unwrapOr(nullptr);
    });
    if (!formatter) {
      continue;
    }
    intl::nsTStringToBufferAdapter adapter(aLocalizedValue);
    if (formatter->format(aValue, adapter).isOk()) {
      return true;
    }
  }
  return false;
}

/* static */
double ICUUtils::ParseNumber(const nsAString& aValue,
                             LanguageTagIterForContent& aLangTags) {
  MOZ_ASSERT(aLangTags.IsAtStart(), "Don't call Next() before passing");
  using LangToParserCache =
      nsTHashMap<RefPtr<nsAtom>, UniquePtr<intl::NumberParser>>;
  static StaticAutoPtr<LangToParserCache> sCache;
  if (aValue.IsEmpty()) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  if (!sCache) {
    sCache = new LangToParserCache();
    ClearOnShutdown(&sCache);
  }

  const Span<const char16_t> value(aValue.BeginReading(), aValue.Length());

  while (RefPtr<nsAtom> langTag = aLangTags.GetNext()) {
    auto& parser = sCache->LookupOrInsertWith(langTag, [&] {
      nsAutoCString tag;
      langTag->ToUTF8String(tag);
      return intl::NumberParser::TryCreate(
                 tag.get(), StaticPrefs::dom_forms_number_grouping())
          .unwrapOr(nullptr);
    });
    if (!parser) {
      continue;
    }
    static_assert(sizeof(UChar) == 2 && sizeof(nsAString::char_type) == 2,
                  "Unexpected character size - the following cast is unsafe");
    auto parseResult = parser->ParseDouble(value);
    if (!parseResult.isOk()) {
      continue;
    }
    std::pair<double, int32_t> parsed = parseResult.unwrap();
    if (parsed.second == static_cast<int32_t>(value.Length())) {
      return parsed.first;
    }
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
