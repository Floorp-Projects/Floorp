/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocaleService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIToolkitChromeRegistry.h"

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
