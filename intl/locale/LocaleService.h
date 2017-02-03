/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_LocaleService_h__
#define mozilla_intl_LocaleService_h__

#include "mozilla/StaticPtr.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace intl {


/**
 * LocaleService is a manager of language negotiation in Gecko.
 *
 * It's intended to be the core place for collecting available and
 * requested languages and negotiating them to produce a fallback
 * chain of locales for the application.
 */
class LocaleService
{
public:
  static LocaleService* GetInstance();

  /**
   * Returns a list of locales that the application should be localized to.
   *
   * The result is a sorted list of valid locale IDs and it should be
   * used for all APIs that accept list of locales, like ECMA402 and L10n APIs.
   *
   * This API always returns at least one locale.
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * Usage:
   * nsTArray<nsCString> appLocales;
   * LocaleService::GetInstance()->GetAppLocales(appLocales);
   */
  void GetAppLocales(nsTArray<nsCString>& aRetVal);

  /**
   * Returns the best locale that the application should be localized to.
   *
   * The result is a valid locale IDs and it should be
   * used for all APIs that do not handle language negotiation.
   *
   * Where possible, GetAppLocales should be preferred over this API and
   * all callsites should handle some form of "best effort" language
   * negotiation to respect user preferences in case the use case does
   * not have data for the first locale in the list.
   *
   * Example: "zh-Hans-HK"
   *
   * Usage:
   * nsAutoCString appLocale;
   * LocaleService::GetInstance()->GetAppLocale(appLocale);
   */
  void GetAppLocale(nsACString& aRetVal);

  /**
   * Triggers a refresh of the language negotiation process.
   *
   * If the result differs from the previous list, it will additionally
   * trigger a global event "intl:app-locales-changed".
   */
  void Refresh();

protected:
  nsTArray<nsCString> mAppLocales;

private:
  static StaticAutoPtr<LocaleService> sInstance;
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_LocaleService_h__ */
