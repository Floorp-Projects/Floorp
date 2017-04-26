/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocaleService.h"

#include <algorithm>  // find_if()
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/intl/OSPreferences.h"
#include "nsIObserverService.h"
#include "nsStringEnumerator.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsXULAppAPI.h"

#ifdef ENABLE_INTL_API
#include "unicode/uloc.h"
#endif

#define MATCH_OS_LOCALE_PREF "intl.locale.matchOS"
#define SELECTED_LOCALE_PREF "general.useragent.locale"

//XXX: This pref is used only by Android and we use it to emulate
//     retrieving OS locale until we get proper hook into JNI in bug 1337078.
#define ANDROID_OS_LOCALE_PREF "intl.locale.os"

static const char* kObservedPrefs[] = {
  MATCH_OS_LOCALE_PREF,
  SELECTED_LOCALE_PREF,
  ANDROID_OS_LOCALE_PREF,
  nullptr
};

using namespace mozilla::intl;
using namespace mozilla;

NS_IMPL_ISUPPORTS(LocaleService, mozILocaleService, nsIObserver)

mozilla::StaticRefPtr<LocaleService> LocaleService::sInstance;

/**
 * This function transforms a canonical Mozilla Language Tag, into it's
 * BCP47 compilant form.
 *
 * Example: "ja-JP-mac" -> "ja-JP-x-lvariant-mac"
 *
 * The BCP47 form should be used for all calls to ICU/Intl APIs.
 * The canonical form is used for all internal operations.
 */
static void
SanitizeForBCP47(nsACString& aLocale)
{
#ifdef ENABLE_INTL_API
  // Currently, the only locale code we use that's not BCP47-conformant is
  // "ja-JP-mac" on OS X, but let's try to be more general than just
  // hard-coding that here.
  const int32_t LANG_TAG_CAPACITY = 128;
  char langTag[LANG_TAG_CAPACITY];
  nsAutoCString locale(aLocale);
  UErrorCode err = U_ZERO_ERROR;
  // This is a fail-safe method that will set langTag to "und" if it cannot
  // match any part of the input locale code.
  int32_t len = uloc_toLanguageTag(locale.get(), langTag, LANG_TAG_CAPACITY,
                                   false, &err);
  if (U_SUCCESS(err) && len > 0) {
    aLocale.Assign(langTag, len);
  }
#else
  // This is only really needed for Intl API purposes, AFAIK,
  // so probably won't be used in a non-ENABLE_INTL_API build.
  // But let's fix up the single anomalous code we actually ship,
  // just in case:
  if (aLocale.EqualsLiteral("ja-JP-mac")) {
    aLocale.AssignLiteral("ja-JP");
  }
#endif
}

static bool
ReadRequestedLocales(nsTArray<nsCString>& aRetVal)
{
  nsAutoCString locale;

  // First, we'll try to check if the user has `matchOS` pref selected
  bool matchOSLocale = Preferences::GetBool(MATCH_OS_LOCALE_PREF);

  if (matchOSLocale) {
    // If he has, we'll pick the locale from the system
    if (OSPreferences::GetInstance()->GetSystemLocales(aRetVal)) {
      // If we succeeded, return.
      return true;
    }
  }

  // Otherwise, we'll try to get the requested locale from the prefs.
  if (!NS_SUCCEEDED(Preferences::GetCString(SELECTED_LOCALE_PREF, &locale))) {
    return false;
  }

  // At the moment we just take a single locale, but in the future
  // we'll want to allow user to specify a list of requested locales.
  aRetVal.AppendElement(locale);
  return true;
}

static bool
ReadAvailableLocales(nsTArray<nsCString>& aRetVal)
{
  nsCOMPtr<nsIToolkitChromeRegistry> cr =
    mozilla::services::GetToolkitChromeRegistryService();
  if (!cr) {
    return false;
  }

  nsCOMPtr<nsIUTF8StringEnumerator> localesEnum;

  nsresult rv =
    cr->GetLocalesForPackage(NS_LITERAL_CSTRING("global"), getter_AddRefs(localesEnum));
  if (!NS_SUCCEEDED(rv)) {
    return false;
  }

  bool more;
  while (NS_SUCCEEDED(rv = localesEnum->HasMore(&more)) && more) {
    nsAutoCString localeStr;
    rv = localesEnum->GetNext(localeStr);
    if (!NS_SUCCEEDED(rv)) {
      return false;
    }

    aRetVal.AppendElement(localeStr);
  }
  return !aRetVal.IsEmpty();
}

LocaleService::LocaleService(bool aIsServer)
  :mIsServer(aIsServer)
{
}

/**
 * This function performs the actual language negotiation for the API.
 *
 * Currently it collects the locale ID used by nsChromeRegistry and
 * adds hardcoded "en-US" locale as a fallback.
 */
void
LocaleService::NegotiateAppLocales(nsTArray<nsCString>& aRetVal)
{
  nsAutoCString defaultLocale;
  GetDefaultLocale(defaultLocale);

  if (mIsServer) {
    AutoTArray<nsCString, 100> availableLocales;
    AutoTArray<nsCString, 10> requestedLocales;
    GetAvailableLocales(availableLocales);
    GetRequestedLocales(requestedLocales);

    NegotiateLanguages(requestedLocales, availableLocales, defaultLocale,
                       LangNegStrategy::Filtering, aRetVal);
  } else {
    // In content process, we will not do any language negotiation.
    // Instead, the language is set manually by SetAppLocales.
    //
    // If this method has been called, it means that we did not fire
    // SetAppLocales yet (happens during initialization).
    // In that case, all we can do is return the default locale.
    // Once SetAppLocales will be called later, it'll fire an event
    // allowing callers to update the locale.
    aRetVal.AppendElement(defaultLocale);
  }
}

LocaleService*
LocaleService::GetInstance()
{
  if (!sInstance) {
    sInstance = new LocaleService(XRE_IsParentProcess());

    if (sInstance->IsServer()) {
      // We're going to observe for requested languages changes which come
      // from prefs.
      DebugOnly<nsresult> rv = Preferences::AddStrongObservers(sInstance, kObservedPrefs);
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");
    }
    ClearOnShutdown(&sInstance);
  }
  return sInstance;
}

LocaleService::~LocaleService()
{
  if (mIsServer) {
    Preferences::RemoveObservers(sInstance, kObservedPrefs);
  }
}

void
LocaleService::GetAppLocalesAsLangTags(nsTArray<nsCString>& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales;
}

void
LocaleService::GetAppLocalesAsBCP47(nsTArray<nsCString>& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  for (uint32_t i = 0; i < mAppLocales.Length(); i++) {
    nsAutoCString locale(mAppLocales[i]);
    SanitizeForBCP47(locale);
    aRetVal.AppendElement(locale);
  }
}

void
LocaleService::AssignAppLocales(const nsTArray<nsCString>& aAppLocales)
{
  MOZ_ASSERT(!mIsServer, "This should only be called for LocaleService in client mode.");

  mAppLocales = aAppLocales;
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "intl:app-locales-changed", nullptr);
  }
}

void
LocaleService::AssignRequestedLocales(const nsTArray<nsCString>& aRequestedLocales)
{
  MOZ_ASSERT(!mIsServer, "This should only be called for LocaleService in client mode.");

  mRequestedLocales = aRequestedLocales;
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "intl:requested-locales-changed", nullptr);
  }
}

bool
LocaleService::GetRequestedLocales(nsTArray<nsCString>& aRetVal)
{
  if (mRequestedLocales.IsEmpty()) {
    ReadRequestedLocales(mRequestedLocales);
  }

  aRetVal = mRequestedLocales;
  return true;
}

bool
LocaleService::GetAvailableLocales(nsTArray<nsCString>& aRetVal)
{
  if (mAvailableLocales.IsEmpty()) {
    ReadAvailableLocales(mAvailableLocales);
  }

  aRetVal = mAvailableLocales;
  return true;
}


void
LocaleService::OnAvailableLocalesChanged()
{
  MOZ_ASSERT(mIsServer, "This should only be called in the server mode.");
  mAvailableLocales.Clear();
  // In the future we may want to trigger here intl:available-locales-changed
  OnLocalesChanged();
}

void
LocaleService::OnRequestedLocalesChanged()
{
  MOZ_ASSERT(mIsServer, "This should only be called in the server mode.");

  nsTArray<nsCString> newLocales;
  ReadRequestedLocales(newLocales);

  if (mRequestedLocales != newLocales) {
    mRequestedLocales = Move(newLocales);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "intl:requested-locales-changed", nullptr);
    }
    OnLocalesChanged();
  }
}

void
LocaleService::OnLocalesChanged()
{
  MOZ_ASSERT(mIsServer, "This should only be called in the server mode.");

  // if mAppLocales has not been initialized yet, just return
  if (mAppLocales.IsEmpty()) {
    return;
  }

  nsTArray<nsCString> newLocales;
  NegotiateAppLocales(newLocales);

  if (mAppLocales != newLocales) {
    mAppLocales = Move(newLocales);
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "intl:app-locales-changed", nullptr);

      // Deprecated, please use `intl:app-locales-changed`.
      // Kept for now for compatibility reasons
      obs->NotifyObservers(nullptr, "selected-locale-has-changed", nullptr);
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

bool
LocaleService::IsAppLocaleRTL()
{
  nsAutoCString locale;
  GetAppLocaleAsBCP47(locale);

#ifdef ENABLE_INTL_API
  int pref = Preferences::GetInt("intl.uidirection", -1);
  if (pref >= 0) {
    return (pref > 0);
  }
  return uloc_isRightToLeft(locale.get());
#else
  // first check the intl.uidirection.<locale> preference, and if that is not
  // set, check the same preference but with just the first two characters of
  // the locale. If that isn't set, default to left-to-right.
  nsAutoCString prefString = NS_LITERAL_CSTRING("intl.uidirection.") + locale;
  nsAutoCString dir;
  Preferences::GetCString(prefString.get(), &dir);
  if (dir.IsEmpty()) {
    int32_t hyphen = prefString.FindChar('-');
    if (hyphen >= 1) {
      prefString.Truncate(hyphen);
      Preferences::GetCString(prefString.get(), &dir);
    }
  }
  return dir.EqualsLiteral("rtl");
#endif
}

NS_IMETHODIMP
LocaleService::Observe(nsISupports *aSubject, const char *aTopic,
                      const char16_t *aData)
{
  MOZ_ASSERT(mIsServer, "This should only be called in the server mode.");

  // At the moment the only thing we're observing are settings indicating
  // user requested locales.
  NS_ConvertUTF16toUTF8 pref(aData);
  if (pref.EqualsLiteral(MATCH_OS_LOCALE_PREF) ||
      pref.EqualsLiteral(SELECTED_LOCALE_PREF) ||
      pref.EqualsLiteral(ANDROID_OS_LOCALE_PREF)) {
    OnRequestedLocalesChanged();
  }
  return NS_OK;
}

bool
LocaleService::LanguagesMatch(const nsCString& aRequested,
                              const nsCString& aAvailable)
{
  return Locale(aRequested, true).LanguageMatches(Locale(aAvailable, true));
}


bool
LocaleService::IsServer()
{
  return mIsServer;
}

/**
 * mozILocaleService methods
 */

static char**
CreateOutArray(const nsTArray<nsCString>& aArray)
{
  uint32_t n = aArray.Length();
  char** result = static_cast<char**>(moz_xmalloc(n * sizeof(char*)));
  for (uint32_t i = 0; i < n; i++) {
    result[i] = moz_xstrdup(aArray[i].get());
  }
  return result;
}

NS_IMETHODIMP
LocaleService::GetDefaultLocale(nsACString& aRetVal)
{
  aRetVal.AssignLiteral("en-US");
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocalesAsLangTags(uint32_t* aCount, char*** aOutArray)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }

  *aCount = mAppLocales.Length();
  *aOutArray = CreateOutArray(mAppLocales);

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocalesAsBCP47(uint32_t* aCount, char*** aOutArray)
{
  AutoTArray<nsCString, 32> locales;
  GetAppLocalesAsBCP47(locales);

  *aCount = locales.Length();
  *aOutArray = CreateOutArray(locales);

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocaleAsLangTag(nsACString& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales[0];
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocaleAsBCP47(nsACString& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales[0];

  SanitizeForBCP47(aRetVal);
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
    if (!s || !*s) {
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

static bool
SubtagMatches(const nsCString& aSubtag1, const nsCString& aSubtag2)
{
  return aSubtag1.EqualsLiteral("*") ||
         aSubtag2.EqualsLiteral("*") ||
         aSubtag1.Equals(aSubtag2, nsCaseInsensitiveCStringComparator());
}

bool
LocaleService::Locale::Matches(const LocaleService::Locale& aLocale) const
{
  return SubtagMatches(mLanguage, aLocale.mLanguage) &&
         SubtagMatches(mScript, aLocale.mScript) &&
         SubtagMatches(mRegion, aLocale.mRegion) &&
         SubtagMatches(mVariant, aLocale.mVariant);
}

bool
LocaleService::Locale::LanguageMatches(const LocaleService::Locale& aLocale) const
{
  return SubtagMatches(mLanguage, aLocale.mLanguage) &&
         SubtagMatches(mScript, aLocale.mScript);
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

NS_IMETHODIMP
LocaleService::GetRequestedLocales(uint32_t* aCount, char*** aOutArray)
{
  AutoTArray<nsCString, 16> requestedLocales;
  bool res = GetRequestedLocales(requestedLocales);

  if (!res) {
    NS_ERROR("Couldn't retrieve selected locales from prefs!");
    return NS_ERROR_FAILURE;
  }

  *aCount = requestedLocales.Length();
  *aOutArray = CreateOutArray(requestedLocales);

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetRequestedLocale(nsACString& aRetVal)
{
  AutoTArray<nsCString, 16> requestedLocales;
  bool res = GetRequestedLocales(requestedLocales);

  if (!res) {
    NS_ERROR("Couldn't retrieve selected locales from prefs!");
    return NS_ERROR_FAILURE;
  }

  if (requestedLocales.Length() > 0) {
    aRetVal = requestedLocales[0];
  }

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::SetRequestedLocales(const char** aRequested,
                                   uint32_t aRequestedCount)
{
  MOZ_ASSERT(aRequestedCount < 2, "We can only handle one requested locale");

  if (aRequestedCount == 0) {
    Preferences::ClearUser(SELECTED_LOCALE_PREF);
  } else {
    Preferences::SetCString(SELECTED_LOCALE_PREF, aRequested[0]);
  }

  Preferences::SetBool(MATCH_OS_LOCALE_PREF, aRequestedCount == 0);
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAvailableLocales(uint32_t* aCount, char*** aOutArray)
{
  AutoTArray<nsCString, 100> availableLocales;
  bool res = GetAvailableLocales(availableLocales);

  if (!res) {
    NS_ERROR("Couldn't retrieve available locales!");
    return NS_ERROR_FAILURE;
  }

  *aCount = availableLocales.Length();
  *aOutArray = CreateOutArray(availableLocales);
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetIsAppLocaleRTL(bool* aRetVal)
{
  (*aRetVal) = IsAppLocaleRTL();
  return NS_OK;
}
