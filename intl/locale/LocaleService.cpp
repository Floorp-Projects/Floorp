/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocaleService.h"

#include <algorithm>  // find_if()
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/intl/MozLocale.h"
#include "mozilla/intl/OSPreferences.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsIToolkitChromeRegistry.h"
#include "nsStringEnumerator.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"

#include "unicode/uloc.h"

#define INTL_SYSTEM_LOCALES_CHANGED "intl:system-locales-changed"

#define REQUESTED_LOCALES_PREF "intl.locale.requested"

static const char* kObservedPrefs[] = {
  REQUESTED_LOCALES_PREF,
  nullptr
};

using namespace mozilla::intl;
using namespace mozilla;

NS_IMPL_ISUPPORTS(LocaleService, mozILocaleService, nsIObserver,
                  nsISupportsWeakReference)

mozilla::StaticRefPtr<LocaleService> LocaleService::sInstance;

/**
 * This function transforms a canonical Mozilla Language Tag, into it's
 * BCP47 compilant form.
 *
 * Example: "ja-JP-mac" -> "ja-JP-macos"
 *
 * The BCP47 form should be used for all calls to ICU/Intl APIs.
 * The canonical form is used for all internal operations.
 */
static bool
SanitizeForBCP47(nsACString& aLocale, bool strict)
{
  // Currently, the only locale code we use that's not BCP47-conformant is
  // "ja-JP-mac" on OS X, and ICU canonicalizes it into a mouthfull
  // "ja-JP-x-lvariant-mac", so instead we're hardcoding a conversion
  // of it to "ja-JP-macos".
  if (aLocale.LowerCaseEqualsASCII("ja-jp-mac")) {
    aLocale.AssignLiteral("ja-JP-macos");
    return true;
  }

  // The rest of this function will use ICU canonicalization for any other
  // tag that may come this way.
  const int32_t LANG_TAG_CAPACITY = 128;
  char langTag[LANG_TAG_CAPACITY];
  nsAutoCString locale(aLocale);
  locale.Trim(" ");
  UErrorCode err = U_ZERO_ERROR;
  // This is a fail-safe method that will set langTag to "und" if it cannot
  // match any part of the input locale code.
  int32_t len = uloc_toLanguageTag(locale.get(), langTag, LANG_TAG_CAPACITY,
                                   strict, &err);
  if (U_SUCCESS(err) && len > 0) {
    aLocale.Assign(langTag, len);
  }
  return U_SUCCESS(err);
}

/**
 * This function splits an input string by `,` delimiter, sanitizes the result
 * language tags and returns them to the caller.
 */
static void
SplitLocaleListStringIntoArray(nsACString& str, nsTArray<nsCString>& aRetVal)
{
  if (str.Length() > 0) {
    for (const nsACString& part : str.Split(',')) {
      nsAutoCString locale(part);
      if (SanitizeForBCP47(locale, true)) {
        if (!aRetVal.Contains(locale)) {
          aRetVal.AppendElement(locale);
        }
      }
    }
  }
}

static bool
ReadRequestedLocales(nsTArray<nsCString>& aRetVal)
{
  nsAutoCString str;
  nsresult rv = Preferences::GetCString(REQUESTED_LOCALES_PREF, str);

  // We handle three scenarios here:
  //
  // 1) The pref is not set - use default locale
  // 2) The pref is set to "" - use OS locales
  // 3) The pref is set to a value - parse the locale list and use it
  if (NS_SUCCEEDED(rv)) {
    if (str.Length() == 0) {
      // If the pref string is empty, we'll take requested locales
      // from the OS.
      OSPreferences::GetInstance()->GetSystemLocales(aRetVal);
    } else {
      SplitLocaleListStringIntoArray(str, aRetVal);
    }
  } else {
    nsAutoCString defaultLocale;
    LocaleService::GetInstance()->GetDefaultLocale(defaultLocale);
    aRetVal.AppendElement(defaultLocale);
  }

  // Last fallback locale is a locale for the requested locale chain.
  // In the future we'll want to make the fallback chain differ per-locale.
  //
  // Notice: This is not the same as DefaultLocale,
  // which follows the default locale the build is in.
  LocaleService::GetInstance()->GetLastFallbackLocale(str);
  if (!aRetVal.Contains(str)) {
    aRetVal.AppendElement(str);
  }
  return true;
}

LocaleService::LocaleService(bool aIsServer)
  :mIsServer(aIsServer)
{
}

/**
 * This function performs the actual language negotiation for the API.
 *
 * Currently it collects the locale ID used by nsChromeRegistry and
 * adds hardcoded default locale as a fallback.
 */
void
LocaleService::NegotiateAppLocales(nsTArray<nsCString>& aRetVal)
{
  if (mIsServer) {
    nsAutoCString defaultLocale;
    AutoTArray<nsCString, 100> availableLocales;
    AutoTArray<nsCString, 10> requestedLocales;
    GetDefaultLocale(defaultLocale);
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
    // In that case, all we can do is return the default or last fallback locale.
    //
    // We will return last fallback here, to avoid having to trigger reading
    // `update.locale` for default locale.
    //
    // Once SetAppLocales will be called later, it'll fire an event
    // allowing callers to update the locale.
    nsAutoCString lastFallbackLocale;
    GetLastFallbackLocale(lastFallbackLocale);
    aRetVal.AppendElement(lastFallbackLocale);
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
      DebugOnly<nsresult> rv = Preferences::AddWeakObservers(sInstance, kObservedPrefs);
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Adding observers failed.");

      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (obs) {
        obs->AddObserver(sInstance, INTL_SYSTEM_LOCALES_CHANGED, true);
      }
    }
    ClearOnShutdown(&sInstance, ShutdownPhase::Shutdown);
  }
  return sInstance;
}

LocaleService::~LocaleService()
{
  if (mIsServer) {
    Preferences::RemoveObservers(this, kObservedPrefs);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, INTL_SYSTEM_LOCALES_CHANGED);
    }
  }
}

void
LocaleService::GetAppLocalesAsLangTags(nsTArray<nsCString>& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  for (uint32_t i = 0; i < mAppLocales.Length(); i++) {
    nsAutoCString locale(mAppLocales[i]);
    if (locale.LowerCaseEqualsASCII("ja-jp-macos")) {
      aRetVal.AppendElement("ja-JP-mac");
    } else {
      aRetVal.AppendElement(locale);
    }
  }
}

void
LocaleService::GetAppLocalesAsBCP47(nsTArray<nsCString>& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales;
}

void
LocaleService::GetRegionalPrefsLocales(nsTArray<nsCString>& aRetVal)
{
  bool useOSLocales = Preferences::GetBool("intl.regional_prefs.use_os_locales", false);

  // If the user specified that they want to use OS Regional Preferences locales,
  // try to retrieve them and use.
  if (useOSLocales) {
    if (OSPreferences::GetInstance()->GetRegionalPrefsLocales(aRetVal)) {
      return;
    }

    // If we fail to retrieve them, return the app locales.
    GetAppLocalesAsBCP47(aRetVal);
    return;
  }

  // Otherwise, fetch OS Regional Preferences locales and compare the first one
  // to the app locale. If the language subtag matches, we can safely use
  // the OS Regional Preferences locale.
  //
  // This facilitates scenarios such as Firefox in "en-US" and User sets
  // regional prefs to "en-GB".
  nsAutoCString appLocale;
  AutoTArray<nsCString, 10> regionalPrefsLocales;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(appLocale);

  if (!OSPreferences::GetInstance()->GetRegionalPrefsLocales(regionalPrefsLocales)) {
    GetAppLocalesAsBCP47(aRetVal);
    return;
  }

  if (LocaleService::LanguagesMatch(appLocale, regionalPrefsLocales[0])) {
    aRetVal = regionalPrefsLocales;
    return;
  }

  // Otherwise use the app locales.
  GetAppLocalesAsBCP47(aRetVal);
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
    // If there are no available locales set, it means that L10nRegistry
    // did not register its locale pool yet. The best course of action
    // is to use packaged locales until that happens.
    GetPackagedLocales(mAvailableLocales);
  }

  aRetVal = mAvailableLocales;
  return true;
}

void
LocaleService::RequestedLocalesChanged()
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
    LocalesChanged();
  }
}

void
LocaleService::LocalesChanged()
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
 * The exact algorithm is custom, and consists of a 6 level strategy:
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
 * 5) Attempt to look up for a maximized version of the requested locale,
 *    stripped of the region code.
 *    Example: ['en-CA'] * ['en-ZA', 'en-US'] = ['en-US', 'en-ZA']
 *               ^^^^^
 *               |----------- look for likelySubtag of 'en': 'en-Latn-US'
 *
 *
 * 6) Attempt to look up for a different region of the same locale.
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
  // matching. We will invalidate entries in this list when they are matched
  // and the corresponding strings from aAvailable added to aRetVal, so that
  // no available locale will be found more than once.
  AutoTArray<Locale, 100> availLocales;
  for (auto& avail : aAvailable) {
    availLocales.AppendElement(Locale(avail));
  }

  for (auto& requested : aRequested) {
    if (requested.IsEmpty()) {
      MOZ_ASSERT(!requested.IsEmpty(), "Locale string cannot be empty.");
      continue;
    }

    // 1) Try to find a simple (case-insensitive) string match for the request.
    auto matchesExactly = [&](Locale& aLoc) {
      return requested.Equals(aLoc.AsString(),
                              nsCaseInsensitiveCStringComparator());
    };
    auto match = std::find_if(availLocales.begin(), availLocales.end(),
                              matchesExactly);
    if (match != availLocales.end()) {
      aRetVal.AppendElement(aAvailable[match - availLocales.begin()]);
      match->Invalidate();
    }

    if (!aRetVal.IsEmpty()) {
      HANDLE_STRATEGY;
    }

    // 2) Try to match against the available locales treated as ranges.
    auto findRangeMatches = [&](Locale& aReq, bool aAvailRange, bool aReqRange) {
      auto matchesRange = [&](Locale& aLoc) {
        return aLoc.Matches(aReq, aAvailRange, aReqRange);
      };
      bool foundMatch = false;
      auto match = availLocales.begin();
      while ((match = std::find_if(match, availLocales.end(),
                                   matchesRange)) != availLocales.end()) {
        aRetVal.AppendElement(aAvailable[match - availLocales.begin()]);
        match->Invalidate();
        foundMatch = true;
        if (aStrategy != LangNegStrategy::Filtering) {
          return true; // we only want the first match
        }
      }
      return foundMatch;
    };

    Locale requestedLocale = Locale(requested);
    if (findRangeMatches(requestedLocale, true, false)) {
      HANDLE_STRATEGY;
    }

    // 3) Try to match against a maximized version of the requested locale
    if (requestedLocale.AddLikelySubtags()) {
      if (findRangeMatches(requestedLocale, true, false)) {
        HANDLE_STRATEGY;
      }
    }

    // 4) Try to match against a variant as a range
    requestedLocale.ClearVariants();
    if (findRangeMatches(requestedLocale, true, true)) {
      HANDLE_STRATEGY;
    }

    // 5) Try to match against the likely subtag without region
    requestedLocale.ClearRegion();
    if (requestedLocale.AddLikelySubtags()) {
      if (findRangeMatches(requestedLocale, true, false)) {
        HANDLE_STRATEGY;
      }
    }

    // 6) Try to match against a region as a range
    requestedLocale.ClearRegion();
    if (findRangeMatches(requestedLocale, true, true)) {
      HANDLE_STRATEGY;
    }
  }
}

void
LocaleService::NegotiateLanguages(const nsTArray<nsCString>& aRequested,
                                  const nsTArray<nsCString>& aAvailable,
                                  const nsACString& aDefaultLocale,
                                  LangNegStrategy aStrategy,
                                  nsTArray<nsCString>& aRetVal)
{
  MOZ_ASSERT(aDefaultLocale.IsEmpty() || Locale(aDefaultLocale).IsValid(),
    "If specified, default locale must be a valid BCP47 language tag.");

  if (aStrategy == LangNegStrategy::Lookup && aDefaultLocale.IsEmpty()) {
    NS_WARNING("Default locale should be specified when using lookup strategy.");
  }

  FilterMatches(aRequested, aAvailable, aStrategy, aRetVal);

  if (aStrategy == LangNegStrategy::Lookup) {
    // If the strategy is Lookup and Filtering returned no matches, use
    // the default locale.
    if (aRetVal.Length() == 0) {
      // If the default locale is empty, we already issued a warning, so
      // now we will just pick up the LocaleService's defaultLocale.
      if (aDefaultLocale.IsEmpty()) {
        nsAutoCString defaultLocale;
        GetDefaultLocale(defaultLocale);
        aRetVal.AppendElement(defaultLocale);
      } else {
        aRetVal.AppendElement(aDefaultLocale);
      }
    }
  } else if (!aDefaultLocale.IsEmpty() && !aRetVal.Contains(aDefaultLocale)) {
    // If it's not a Lookup strategy, add the default locale only if it's
    // set and it's not in the results already.
    aRetVal.AppendElement(aDefaultLocale);
  }
}

bool
LocaleService::IsAppLocaleRTL()
{
  nsAutoCString locale;
  GetAppLocaleAsBCP47(locale);

  int pref = Preferences::GetInt("intl.uidirection", -1);
  if (pref >= 0) {
    return (pref > 0);
  }
  return uloc_isRightToLeft(locale.get());
}

NS_IMETHODIMP
LocaleService::Observe(nsISupports *aSubject, const char *aTopic,
                      const char16_t *aData)
{
  MOZ_ASSERT(mIsServer, "This should only be called in the server mode.");

  if (!strcmp(aTopic, INTL_SYSTEM_LOCALES_CHANGED)) {
    RequestedLocalesChanged();
  } else {
    NS_ConvertUTF16toUTF8 pref(aData);
    // At the moment the only thing we're observing are settings indicating
    // user requested locales.
    if (pref.EqualsLiteral(REQUESTED_LOCALES_PREF)) {
      RequestedLocalesChanged();
    }
  }

  return NS_OK;
}

bool
LocaleService::LanguagesMatch(const nsACString& aRequested,
                              const nsACString& aAvailable)
{
  Locale requested = Locale(aRequested);
  Locale available = Locale(aAvailable);
  return requested.GetLanguage().Equals(available.GetLanguage());
}


bool
LocaleService::IsServer()
{
  return mIsServer;
}

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

static bool
GetGREFileContents(const char* aFilePath, nsCString* aOutString)
{
  // Look for the requested file in omnijar.
  RefPtr<nsZipArchive> zip = Omnijar::GetReader(Omnijar::GRE);
  if (zip) {
    nsZipItemPtr<char> item(zip, aFilePath);
    if (!item) {
      return false;
    }
    aOutString->Assign(item.Buffer(), item.Length());
    return true;
  }

  // If we didn't have an omnijar (i.e. we're running a non-packaged
  // build), then look in the GRE directory.
  nsCOMPtr<nsIFile> path;
  if (NS_FAILED(nsDirectoryService::gService->Get(NS_GRE_DIR,
                                                  NS_GET_IID(nsIFile),
                                                  getter_AddRefs(path)))) {
    return false;
  }

  path->AppendRelativeNativePath(nsDependentCString(aFilePath));
  bool result;
  if (NS_FAILED(path->IsFile(&result)) || !result ||
      NS_FAILED(path->IsReadable(&result)) || !result) {
    return false;
  }

  // This is a small file, only used once, so it's not worth doing some fancy
  // off-main-thread file I/O or whatever. Just read it.
  FILE* fp;
  if (NS_FAILED(path->OpenANSIFileDesc("r", &fp)) || !fp) {
    return false;
  }

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  rewind(fp);
  aOutString->SetLength(len);
  size_t cc = fread(aOutString->BeginWriting(), 1, len, fp);

  fclose(fp);

  return cc == size_t(len);
}

void
LocaleService::InitPackagedLocales()
{
  MOZ_ASSERT(mPackagedLocales.IsEmpty());

  nsAutoCString localesString;
  if (GetGREFileContents("res/multilocale.txt", &localesString)) {
    localesString.Trim(" \t\n\r");
    // This should never be empty in a correctly-built product.
    MOZ_ASSERT(!localesString.IsEmpty());
    SplitLocaleListStringIntoArray(localesString, mPackagedLocales);
  }

  // Last resort in case of broken build
  if (mPackagedLocales.IsEmpty()) {
    nsAutoCString defaultLocale;
    GetDefaultLocale(defaultLocale);
    mPackagedLocales.AppendElement(defaultLocale);
  }
}

void
LocaleService::GetPackagedLocales(nsTArray<nsCString>& aRetVal)
{
  if (mPackagedLocales.IsEmpty()) {
    InitPackagedLocales();
  }
  aRetVal = mPackagedLocales;
}

/**
 * mozILocaleService methods
 */

NS_IMETHODIMP
LocaleService::GetDefaultLocale(nsACString& aRetVal)
{
  // We don't allow this to change during a session (it's set at build/package
  // time), so we cache the result the first time we're called.
  if (mDefaultLocale.IsEmpty()) {
    nsAutoCString locale;
    // Try to get the package locale from update.locale in omnijar. If the
    // update.locale file is not found, item.len will remain 0 and we'll
    // just use our hard-coded default below.
    GetGREFileContents("update.locale", &locale);
    locale.Trim(" \t\n\r");
    // This should never be empty.
    MOZ_ASSERT(!locale.IsEmpty());
    if (SanitizeForBCP47(locale, true)) {
      mDefaultLocale.Assign(locale);
    }

    // Hard-coded fallback to allow us to survive even if update.locale was
    // missing/broken in some way.
    if (mDefaultLocale.IsEmpty()) {
      GetLastFallbackLocale(mDefaultLocale);
    }
  }

  aRetVal = mDefaultLocale;
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetLastFallbackLocale(nsACString& aRetVal)
{
  aRetVal.AssignLiteral("en-US");
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocalesAsLangTags(uint32_t* aCount, char*** aOutArray)
{
  AutoTArray<nsCString, 32> locales;
  GetAppLocalesAsLangTags(locales);

  *aCount = locales.Length();
  *aOutArray = CreateOutArray(locales);

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocalesAsBCP47(uint32_t* aCount, char*** aOutArray)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  *aCount = mAppLocales.Length();
  *aOutArray = CreateOutArray(mAppLocales);

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocaleAsLangTag(nsACString& aRetVal)
{
  AutoTArray<nsCString, 32> locales;
  GetAppLocalesAsLangTags(locales);

  aRetVal = locales[0];
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetAppLocaleAsBCP47(nsACString& aRetVal)
{
  if (mAppLocales.IsEmpty()) {
    NegotiateAppLocales(mAppLocales);
  }
  aRetVal = mAppLocales[0];
  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetRegionalPrefsLocales(uint32_t* aCount, char*** aOutArray)
{
  AutoTArray<nsCString,10> rgLocales;

  GetRegionalPrefsLocales(rgLocales);

  *aCount = rgLocales.Length();
  *aOutArray = static_cast<char**>(moz_xmalloc(*aCount * sizeof(char*)));

  for (uint32_t i = 0; i < *aCount; i++) {
    (*aOutArray)[i] = moz_xstrdup(rgLocales[i].get());
  }

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
  NegotiateLanguages(requestedLocales, availableLocales,
                     defaultLocale, strategy, supportedLocales);

  *aRetVal =
    static_cast<char**>(moz_xmalloc(sizeof(char*) * supportedLocales.Length()));

  *aCount = 0;
  for (const auto& supported : supportedLocales) {
    (*aRetVal)[(*aCount)++] = moz_xstrdup(supported.get());
  }

  return NS_OK;
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
  nsAutoCString str;

  for (uint32_t i = 0; i < aRequestedCount; i++) {
    nsAutoCString locale(aRequested[i]);
    if (!SanitizeForBCP47(locale, true)) {
      NS_ERROR("Invalid language tag provided to SetRequestedLocales!");
      return NS_ERROR_INVALID_ARG;
    }

    if (i > 0) {
      str.AppendLiteral(",");
    }
    str.Append(locale);
  }
  Preferences::SetCString(REQUESTED_LOCALES_PREF, str);

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

NS_IMETHODIMP
LocaleService::SetAvailableLocales(const char** aAvailable,
                                   uint32_t aAvailableCount)
{
  nsTArray<nsCString> newLocales;

  for (uint32_t i = 0; i < aAvailableCount; i++) {
    nsAutoCString locale(aAvailable[i]);
    if (!SanitizeForBCP47(locale, true)) {
      NS_ERROR("Invalid language tag provided to SetAvailableLocales!");
      return NS_ERROR_INVALID_ARG;
    }
    newLocales.AppendElement(locale);
  }

  if (newLocales != mAvailableLocales) {
    mAvailableLocales = Move(newLocales);
    LocalesChanged();
  }

  return NS_OK;
}

NS_IMETHODIMP
LocaleService::GetPackagedLocales(uint32_t* aCount, char*** aOutArray)
{
  if (mPackagedLocales.IsEmpty()) {
    InitPackagedLocales();
  }

  *aCount = mPackagedLocales.Length();
  *aOutArray = CreateOutArray(mPackagedLocales);

  return NS_OK;
}
