/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocaleService.h"

#include <algorithm>  // find_if()
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIToolkitChromeRegistry.h"

#ifdef ENABLE_INTL_API
#include "unicode/uloc.h"
#endif

using namespace mozilla::intl;

NS_IMPL_ISUPPORTS(LocaleService, mozILocaleService)

mozilla::StaticRefPtr<LocaleService> LocaleService::sInstance;

/**
 * This function performs the actual language negotiation for the API.
 *
 * Currently it collects the locale ID used by nsChromeRegistry and
 * adds hardcoded "en-US" locale as a fallback.
 */
static void
ReadAppLocales(nsTArray<nsCString>& aRetVal)
{
  nsAutoCString uaLangTag;
  nsCOMPtr<nsIToolkitChromeRegistry> cr =
    mozilla::services::GetToolkitChromeRegistryService();
  if (cr) {
    // We don't want to canonicalize the locale from ChromeRegistry into
    // it's BCP47 form because we will use it for direct language
    // negotiation and BCP47 changes `ja-JP-mac` into `ja-JP-x-variant-mac`.
    cr->GetSelectedLocale(NS_LITERAL_CSTRING("global"), false, uaLangTag);
  }
  if (!uaLangTag.IsEmpty()) {
    aRetVal.AppendElement(uaLangTag);
  }

  if (!uaLangTag.EqualsLiteral("en-US")) {
    aRetVal.AppendElement(NS_LITERAL_CSTRING("en-US"));
  }
}

LocaleService*
LocaleService::GetInstance()
{
  if (!sInstance) {
    sInstance = new LocaleService();
    ClearOnShutdown(&sInstance);
  }
  return sInstance;
}

void
LocaleService::GetAppLocales(nsTArray<nsCString>& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    ReadAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales;
}

void
LocaleService::Refresh()
{
  nsTArray<nsCString> newLocales;
  ReadAppLocales(newLocales);

  if (mAppLocales != newLocales) {
    mAppLocales = Move(newLocales);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "intl:app-locales-changed", nullptr);
    }
  }
}

// After trying each step of the negotiation algorithm for each requested locale,
// if a match was found we use this macro to decide whether to return immediately,
// skip to the next requested locale, or continue searching for additional matches,
// according to the desired negotiation strategy.
#define HANDLE_STRATEGY \
          switch (aStrategy) { \
            case LangNegStrategy::Lookup: \
              return; \
            case LangNegStrategy::Matching: \
              continue; \
            case LangNegStrategy::Filtering: \
              break; \
          }

/**
 * This is the raw algorithm for language negotiation based roughly
 * on RFC4647 language filtering, with changes from LDML language matching.
 *
 * The exact algorithm is custom, and consist of 5 level strategy:
 *
 * 1) Attempt to find an exact match for each requested locale in available
 *    locales.
 *    Example: ['en-US'] * ['en-US'] = ['en-US']
 *
 * 2) Attempt to match a requested locale to an available locale treated
 *    as a locale range.
 *    Example: ['en-US'] * ['en'] = ['en']
 *                           ^^
 *                           |-- becomes 'en-*-*-*'
 *
 * 3) Attempt to use the maximized version of the requested locale, to
 *    find the best match in available locales.
 *    Example: ['en'] * ['en-GB', 'en-US'] = ['en-US']
 *               ^^
 *               |-- ICU likelySubtags expands it to 'en-Latn-US'
 *
 * 4) Attempt to look up for a different variant of the same locale.
 *    Example: ['ja-JP-win'] * ['ja-JP-mac'] = ['ja-JP-mac']
 *               ^^^^^^^^^
 *               |----------- replace variant with range: 'ja-JP-*'
 *
 * 5) Attempt to look up for a different region of the same locale.
 *    Example: ['en-GB'] * ['en-AU'] = ['en-AU']
 *               ^^^^^
 *               |----- replace region with range: 'en-*'
 *
 * It uses one of the strategies described in LocaleService.h.
 */
void
LocaleService::FilterMatches(const nsTArray<nsCString>& aRequested,
                             const nsTArray<nsCString>& aAvailable,
                             LangNegStrategy aStrategy,
                             nsTArray<nsCString>& aRetVal)
{
  // Local copy of the list of available locales, in Locale form for flexible
  // matching. We will remove entries from this list as they get appended to
  // aRetVal, so that no available locale will be found more than once.
  AutoTArray<Locale, 100> availLocales;
  for (auto& avail : aAvailable) {
    availLocales.AppendElement(Locale(avail, true));
  }

  // Helper to erase an entry from availLocales once we have copied it to
  // the result list. Returns an iterator pointing to the entry that was
  // immediately after the one that was erased (or availLocales.end() if
  // the target was the last in the array).
  auto eraseFromAvail = [&](nsTArray<Locale>::iterator aIter) {
    nsTArray<Locale>::size_type index = aIter - availLocales.begin();
    availLocales.RemoveElementAt(index);
    return availLocales.begin() + index;
  };

  for (auto& requested : aRequested) {

    // 1) Try to find a simple (case-insensitive) string match for the request.
    auto matchesExactly = [&](const Locale& aLoc) {
      return requested.Equals(aLoc.AsString(),
                              nsCaseInsensitiveCStringComparator());
    };
    auto match = std::find_if(availLocales.begin(), availLocales.end(),
                              matchesExactly);
    if (match != availLocales.end()) {
      aRetVal.AppendElement(match->AsString());
      eraseFromAvail(match);
    }

    if (!aRetVal.IsEmpty()) {
      HANDLE_STRATEGY;
    }

    // 2) Try to match against the available locales treated as ranges.
    auto findRangeMatches = [&](const Locale& aReq) {
      auto matchesRange = [&](const Locale& aLoc) {
        return aReq.Matches(aLoc);
      };
      bool foundMatch = false;
      auto match = availLocales.begin();
      while ((match = std::find_if(match, availLocales.end(),
                                   matchesRange)) != availLocales.end()) {
        aRetVal.AppendElement(match->AsString());
        match = eraseFromAvail(match);
        foundMatch = true;
        if (aStrategy != LangNegStrategy::Filtering) {
          return true; // we only want the first match
        }
      }
      return foundMatch;
    };

    Locale requestedLocale = Locale(requested, false);
    if (findRangeMatches(requestedLocale)) {
      HANDLE_STRATEGY;
    }

    // 3) Try to match against a maximized version of the requested locale
    if (requestedLocale.AddLikelySubtags()) {
      if (findRangeMatches(requestedLocale)) {
        HANDLE_STRATEGY;
      }
    }

    // 4) Try to match against a variant as a range
    requestedLocale.SetVariantRange();
    if (findRangeMatches(requestedLocale)) {
      HANDLE_STRATEGY;
    }

    // 5) Try to match against a region as a range
    requestedLocale.SetRegionRange();
    if (findRangeMatches(requestedLocale)) {
      HANDLE_STRATEGY;
    }
  }
}

bool
LocaleService::NegotiateLanguages(const nsTArray<nsCString>& aRequested,
                                  const nsTArray<nsCString>& aAvailable,
                                  const nsACString& aDefaultLocale,
                                  LangNegStrategy aStrategy,
                                  nsTArray<nsCString>& aRetVal)
{
  // If the strategy is Lookup, we require the defaultLocale to be set.
  if (aStrategy == LangNegStrategy::Lookup && aDefaultLocale.IsEmpty()) {
    return false;
  }

  FilterMatches(aRequested, aAvailable, aStrategy, aRetVal);

  if (aStrategy == LangNegStrategy::Lookup) {
    if (aRetVal.Length() == 0) {
      // If the strategy is Lookup and Filtering returned no matches, use
      // the default locale.
      aRetVal.AppendElement(aDefaultLocale);
    }
  } else if (!aDefaultLocale.IsEmpty() && !aRetVal.Contains(aDefaultLocale)) {
    // If it's not a Lookup strategy, add the default locale only if it's
    // set and it's not in the results already.
    aRetVal.AppendElement(aDefaultLocale);
  }
  return true;
}


/**
 * mozILocaleService methods
 */
NS_IMETHODIMP
LocaleService::GetAppLocales(uint32_t* aCount, char*** aOutArray)
{
  if (mAppLocales.IsEmpty()) {
    ReadAppLocales(mAppLocales);
  }

  *aCount = mAppLocales.Length();
  *aOutArray = static_cast<char**>(moz_xmalloc(*aCount * sizeof(char*)));

  for (uint32_t i = 0; i < *aCount; i++) {
    (*aOutArray)[i] = moz_xstrdup(mAppLocales[i].get());
  }

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocale(nsACString& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    ReadAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales[0];
  return NS_OK;
}

static LocaleService::LangNegStrategy
ToLangNegStrategy(int32_t aStrategy)
{
  switch (aStrategy) {
    case 1:
      return LocaleService::LangNegStrategy::Matching;
    case 2:
      return LocaleService::LangNegStrategy::Lookup;
    default:
      return LocaleService::LangNegStrategy::Filtering;
  }
}

NS_IMETHODIMP
LocaleService::NegotiateLanguages(const char** aRequested,
                                  const char** aAvailable,
                                  const char*  aDefaultLocale,
                                  int32_t aStrategy,
                                  uint32_t aRequestedCount,
                                  uint32_t aAvailableCount,
                                  uint32_t* aCount, char*** aRetVal)
{
  if (aStrategy < 0 || aStrategy > 2) {
    return NS_ERROR_INVALID_ARG;
  }

  // Check that the given string contains only ASCII characters valid in tags
  // (i.e. alphanumerics, plus '-' and '_'), and is non-empty.
  auto validTagChars = [](const char* s) {
    if (!*s) {
      return false;
    }
    while (*s) {
      if (isalnum((unsigned char)*s) || *s == '-' || *s == '_' || *s == '*') {
        s++;
      } else {
        return false;
      }
    }
    return true;
  };

  AutoTArray<nsCString, 100> requestedLocales;
  for (uint32_t i = 0; i < aRequestedCount; i++) {
    if (!validTagChars(aRequested[i])) {
      continue;
    }
    requestedLocales.AppendElement(aRequested[i]);
  }

  AutoTArray<nsCString, 100> availableLocales;
  for (uint32_t i = 0; i < aAvailableCount; i++) {
    if (!validTagChars(aAvailable[i])) {
      continue;
    }
    availableLocales.AppendElement(aAvailable[i]);
  }

  nsAutoCString defaultLocale(aDefaultLocale);

  LangNegStrategy strategy = ToLangNegStrategy(aStrategy);

  AutoTArray<nsCString, 100> supportedLocales;
  bool result = NegotiateLanguages(requestedLocales, availableLocales,
                                   defaultLocale, strategy, supportedLocales);

  if (!result) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRetVal =
    static_cast<char**>(moz_xmalloc(sizeof(char*) * supportedLocales.Length()));

  *aCount = 0;
  for (const auto& supported : supportedLocales) {
    (*aRetVal)[(*aCount)++] = moz_xstrdup(supported.get());
  }

  return NS_OK;
}

LocaleService::Locale::Locale(const nsCString& aLocale, bool aRange)
  : mLocaleStr(aLocale)
{
  int32_t partNum = 0;

  nsAutoCString normLocale(aLocale);
  normLocale.ReplaceChar('_', '-');

  for (const nsCSubstring& part : normLocale.Split('-')) {
    switch (partNum) {
      case 0:
        if (part.EqualsLiteral("*") ||
            part.Length() == 2 || part.Length() == 3) {
          mLanguage.Assign(part);
        }
        break;
      case 1:
        if (part.EqualsLiteral("*") || part.Length() == 4) {
          mScript.Assign(part);
          break;
        }

        // fallover to region case
        partNum++;
        MOZ_FALLTHROUGH;
      case 2:
        if (part.EqualsLiteral("*") || part.Length() == 2) {
          mRegion.Assign(part);
        }
        break;
      case 3:
        if (part.EqualsLiteral("*") || part.Length() == 3) {
          mVariant.Assign(part);
        }
        break;
    }
    partNum++;
  }

  if (aRange) {
    if (mLanguage.IsEmpty()) {
      mLanguage.Assign(NS_LITERAL_CSTRING("*"));
    }
    if (mScript.IsEmpty()) {
      mScript.Assign(NS_LITERAL_CSTRING("*"));
    }
    if (mRegion.IsEmpty()) {
      mRegion.Assign(NS_LITERAL_CSTRING("*"));
    }
    if (mVariant.IsEmpty()) {
      mVariant.Assign(NS_LITERAL_CSTRING("*"));
    }
  }
}

bool
LocaleService::Locale::Matches(const LocaleService::Locale& aLocale) const
{
  auto subtagMatches = [](const nsCString& aSubtag1,
                          const nsCString& aSubtag2) {
    return aSubtag1.EqualsLiteral("*") ||
           aSubtag2.EqualsLiteral("*") ||
           aSubtag1.Equals(aSubtag2, nsCaseInsensitiveCStringComparator());
  };

  return subtagMatches(mLanguage, aLocale.mLanguage) &&
         subtagMatches(mScript, aLocale.mScript) &&
         subtagMatches(mRegion, aLocale.mRegion) &&
         subtagMatches(mVariant, aLocale.mVariant);
}

void
LocaleService::Locale::SetVariantRange()
{
  mVariant.AssignLiteral("*");
}

void
LocaleService::Locale::SetRegionRange()
{
  mRegion.AssignLiteral("*");
}

bool
LocaleService::Locale::AddLikelySubtags()
{
#ifdef ENABLE_INTL_API
  const int32_t kLocaleMax = 160;
  char maxLocale[kLocaleMax];

  UErrorCode status = U_ZERO_ERROR;
  uloc_addLikelySubtags(mLocaleStr.get(), maxLocale, kLocaleMax, &status);

  if (U_FAILURE(status)) {
    return false;
  }

  nsDependentCString maxLocStr(maxLocale);
  Locale loc = Locale(maxLocStr, false);

  if (loc == *this) {
    return false;
  }

  mLanguage = loc.mLanguage;
  mScript = loc.mScript;
  mRegion = loc.mRegion;
  mVariant = loc.mVariant;
  return true;
#else
  return false;
#endif
}
