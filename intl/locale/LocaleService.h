/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_LocaleService_h__
#define mozilla_intl_LocaleService_h__

#include "mozilla/StaticPtr.h"
#include "nsString.h"
#include "nsTArray.h"

#include "mozILocaleService.h"

namespace mozilla {
namespace intl {

/**
 * LocaleService is a manager of language negotiation in Gecko.
 *
 * It's intended to be the core place for collecting available and
 * requested languages and negotiating them to produce a fallback
 * chain of locales for the application.
 */
class LocaleService : public mozILocaleService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZILOCALESERVICE

  /**
   * Create (if necessary) and return a raw pointer to the singleton instance.
   * Use this accessor in C++ code that just wants to call a method on the
   * instance, but does not need to hold a reference, as in
   *    nsAutoCString str;
   *    LocaleService::GetInstance()->GetAppLocale(str);
   */
  static LocaleService* GetInstance();

  /**
   * Return an addRef'd pointer to the singleton instance. This is used by the
   * XPCOM constructor that exists to support usage from JS.
   */
  static already_AddRefed<LocaleService> GetInstanceAddRefed()
  {
    return RefPtr<LocaleService>(GetInstance()).forget();
  }

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
   *   nsTArray<nsCString> appLocales;
   *   LocaleService::GetInstance()->GetAppLocales(appLocales);
   *
   * (See mozILocaleService.idl for a JS-callable version of this.)
   */
  void GetAppLocales(nsTArray<nsCString>& aRetVal);

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
  virtual ~LocaleService() {};

  static StaticRefPtr<LocaleService> sInstance;
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_LocaleService_h__ */
