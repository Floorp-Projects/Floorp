/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_LocaleService_h__
#define mozilla_intl_LocaleService_h__

#include "nsIObserver.h"
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
 *
 * Client / Server
 *
 * LocaleService may operate in one of two modes:
 *
 *   server
 *     in the server mode, LocaleService is collecting and negotiating
 *     languages. It also subscribes to relevant observers.
 *     There should be at most one server per application instance.
 *
 *   client
 *     in the client mode, LocaleService is not responsible for collecting
 *     or reacting to any system changes. It still distributes information
 *     about locales, but internally, it gets information from the server instance
 *     instead of collecting it on its own.
 *     This prevents any data desynchronization and minimizes the cost
 *     of running the service.
 *
 *   In both modes, all get* methods should work the same way and all
 *   static methods are available.
 *
 *   In the server mode, other components may inform LocaleService about their
 *   status either via calls to set* methods or via observer events.
 *   In the client mode, only the process communication should provide data
 *   to the LocaleService.
 *
 *   At the moment desktop apps use the parent process in the server mode, and
 *   content processes in the client mode.
 *
 * Locale / Language
 *
 * The terms `Locale ID` and `Language ID` are used slightly differently
 * by different organizations. Mozilla uses the term `Language ID` to describe
 * a string that contains information about the language itself, script,
 * region and variant. For example "en-Latn-US-mac" is a correct Language ID.
 *
 * Locale ID contains a Language ID plus a number of extension tags that
 * contain information that go beyond language inforamation such as
 * preferred currency, date/time formatting etc.
 *
 * An example of a Locale ID is `en-Latn-US-x-hc-h12-ca-gregory`
 *
 * At the moment we do not support full extension tag system, but we
 * try to be specific when naming APIs, so the service is for locales,
 * but we negotiate between languages etc.
 */
class LocaleService : public mozILocaleService,
                      public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_MOZILOCALESERVICE

  /**
   * List of available language negotiation strategies.
   *
   * See the mozILocaleService.idl for detailed description of the
   * strategies.
   */
  enum class LangNegStrategy {
    Filtering,
    Matching,
    Lookup
  };

  explicit LocaleService(bool aIsServer);

  /**
   * Create (if necessary) and return a raw pointer to the singleton instance.
   * Use this accessor in C++ code that just wants to call a method on the
   * instance, but does not need to hold a reference, as in
   *    nsAutoCString str;
   *    LocaleService::GetInstance()->GetAppLocaleAsLangTag(str);
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
   * The result is a ordered list of valid locale IDs and it should be
   * used for all APIs that accept list of locales, like ECMA402 and L10n APIs.
   *
   * This API always returns at least one locale.
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * Usage:
   *   nsTArray<nsCString> appLocales;
   *   LocaleService::GetInstance()->GetAppLocalesAsLangTags(appLocales);
   *
   * (See mozILocaleService.idl for a JS-callable version of this.)
   */
  void GetAppLocalesAsLangTags(nsTArray<nsCString>& aRetVal);
  void GetAppLocalesAsBCP47(nsTArray<nsCString>& aRetVal);

  /**
   * This method should only be called in the client mode.
   *
   * It replaces all the language negotiation and is supposed to be called
   * in order to bring the client LocaleService in sync with the server
   * LocaleService.
   *
   * Currently, it's called by the IPC code.
   */
  void AssignAppLocales(const nsTArray<nsCString>& aAppLocales);
  void AssignRequestedLocales(const nsTArray<nsCString>& aRequestedLocales);

  /**
   * Returns a list of locales that the user requested the app to be
   * localized to.
   *
   * The result is a sorted list of valid locale IDs and it should be
   * used as a requestedLocales input list for languages negotiation.
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * Usage:
   *   nsTArray<nsCString> reqLocales;
   *   LocaleService::GetInstance()->GetRequestedLocales(reqLocales);
   *
   * Returns a boolean indicating if the attempt to retrieve prefs
   * was successful.
   *
   * (See mozILocaleService.idl for a JS-callable version of this.)
   */
  bool GetRequestedLocales(nsTArray<nsCString>& aRetVal);

  /**
   * Returns a list of available locales that can be used to
   * localize the app.
   *
   * The result is an unsorted list of valid locale IDs and it should be
   * used as a availableLocales input list for languages negotiation.
   *
   * Example: ["de", "en-US", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * Usage:
   *   nsTArray<nsCString> availLocales;
   *   LocaleService::GetInstance()->GetAvailableLocales(availLocales);
   *
   * Returns a boolean indicating if the attempt to retrieve at least
   * one locale was successful.
   *
   * (See mozILocaleService.idl for a JS-callable version of this.)
   */
  bool GetAvailableLocales(nsTArray<nsCString>& aRetVal);

  /**
   * Those three functions allow to trigger cache invalidation on one of the
   * three cached values.
   *
   * In most cases, the functions will be called by the observer in
   * LocaleService itself, but in a couple special cases, we have the
   * other component call this manually instead of sending a global event.
   *
   * If the result differs from the previous list, it will additionally
   * trigger a corresponding event
   *
   * This code should be called only in the server mode..
   */
  void OnAvailableLocalesChanged();
  void OnRequestedLocalesChanged();
  void OnLocalesChanged();

  /**
   * Negotiates the best locales out of an ordered list of requested locales and
   * a list of available locales.
   *
   * Internally it uses the following naming scheme:
   *
   *  Requested - locales requested by the user
   *  Available - locales for which the data is available
   *  Supported - locales negotiated by the algorithm
   *
   * Additionally, if defaultLocale is provided, it adds it to the end of the
   * result list as a "last resort" locale.
   *
   * Strategy is one of the three strategies described at the top of this file.
   *
   * The result list is ordered according to the order of the requested locales.
   *
   * (See mozILocaleService.idl for a JS-callable version of this.)
   */
  bool NegotiateLanguages(const nsTArray<nsCString>& aRequested,
                          const nsTArray<nsCString>& aAvailable,
                          const nsACString& aDefaultLocale,
                          LangNegStrategy aLangNegStrategy,
                          nsTArray<nsCString>& aRetVal);

  /**
   * Returns whether the current app locale is RTL.
   */
  bool IsAppLocaleRTL();

  static bool LanguagesMatch(const nsCString& aRequested,
                             const nsCString& aAvailable);

  bool IsServer();

private:
  /**
   * Locale object, a BCP47-style tag decomposed into subtags for
   * matching purposes.
   *
   * If constructed with aRange = true, any missing subtags will be
   * set to "*".
   */
  class Locale
  {
  public:
    Locale(const nsCString& aLocale, bool aRange);

    bool Matches(const Locale& aLocale) const;
    bool LanguageMatches(const Locale& aLocale) const;

    void SetVariantRange();
    void SetRegionRange();

    bool AddLikelySubtags(); // returns false if nothing changed

    const nsCString& AsString() const {
      return mLocaleStr;
    }

    bool operator== (const Locale& aOther) {
      const auto& cmp = nsCaseInsensitiveCStringComparator();
      return mLanguage.Equals(aOther.mLanguage, cmp) &&
             mScript.Equals(aOther.mScript, cmp) &&
             mRegion.Equals(aOther.mRegion, cmp) &&
             mVariant.Equals(aOther.mVariant, cmp);
    }

  private:
    const nsCString& mLocaleStr;
    nsCString mLanguage;
    nsCString mScript;
    nsCString mRegion;
    nsCString mVariant;
  };

  void FilterMatches(const nsTArray<nsCString>& aRequested,
                     const nsTArray<nsCString>& aAvailable,
                     LangNegStrategy aStrategy,
                     nsTArray<nsCString>& aRetVal);

  void NegotiateAppLocales(nsTArray<nsCString>& aRetVal);

  virtual ~LocaleService();

  nsTArray<nsCString> mAppLocales;
  nsTArray<nsCString> mRequestedLocales;
  nsTArray<nsCString> mAvailableLocales;
  const bool mIsServer;

  static StaticRefPtr<LocaleService> sInstance;
};
} // intl
} // namespace mozilla

#endif /* mozilla_intl_LocaleService_h__ */
