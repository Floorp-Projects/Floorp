/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_IntlOSPreferences_h__
#define mozilla_intl_IntlOSPreferences_h__

#include "mozilla/StaticPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "unicode/uloc.h"

#include "mozIOSPreferences.h"

namespace mozilla {
namespace intl {

/**
 * OSPreferences API provides a set of methods for retrieving information from
 * the host environment on topics such as:
 *   - Internationalization
 *   - Localization
 *   - Regional preferences
 *
 * The API is meant to remain as simple as possible, relaying information from
 * the host environment to the user without too much logic.
 *
 * Saying that, there are two exceptions to that paradigm.
 *
 * First one is normalization. We do intend to translate host environment
 * concepts to unified Intl/L10n vocabulary used by Mozilla.
 * That means that we will format locale IDs, timezone names, currencies etc.
 * into a chosen format.
 *
 * Second is caching. This API does cache values and where possible will
 * hook into the environment for some event-driven cache invalidation.
 *
 * This means that on platforms that do not support a mechanism to
 * notify apps about changes, new OS-level settings may not be reflected
 * in the app until it is relaunched.
 */
class OSPreferences: public mozIOSPreferences
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIOSPREFERENCES

  enum class DateTimeFormatStyle {
    Invalid = -1,
    None,
    Short,   // e.g. time: HH:mm, date: Y/m/d
    Medium,  // likely same as Short
    Long,    // e.g. time: including seconds, date: including weekday
    Full     // e.g. time: with timezone, date: with long weekday, month
  };

  /**
   * Create (if necessary) and return a raw pointer to the singleton instance.
   * Use this accessor in C++ code that just wants to call a method on the
   * instance, but does not need to hold a reference, as in
   *    nsAutoCString str;
   *    OSPreferences::GetInstance()->GetSystemLocale(str);
   */
  static OSPreferences* GetInstance();

  /**
   * Return an addRef'd pointer to the singleton instance. This is used by the
   * XPCOM constructor that exists to support usage from JS.
   */
  static already_AddRefed<OSPreferences> GetInstanceAddRefed()
  {
    return RefPtr<OSPreferences>(GetInstance()).forget();
  }


  /**
   * Returns a list of locales used by the host environment for UI
   * localization.
   *
   * The result is a sorted list and we expect that the OS attempts to
   * use the top locale from the list for which it has data.
   *
   * Each element of the list is a valid locale ID that can be passed to ICU
   * and ECMA402 Intl APIs,
   * At the same time each element is a valid BCP47 language tag that can be
   * used for language negotiation.
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * The return bool value indicates whether the function successfully
   * resolved at least one locale.
   *
   * Usage:
   *   nsTArray<nsCString> systemLocales;
   *   OSPreferences::GetInstance()->GetSystemLocales(systemLocales);
   *
   * (See mozIOSPreferences.idl for a JS-callable version of this.)
   */
  bool GetSystemLocales(nsTArray<nsCString>& aRetVal);

  /**
   * Returns a list of locales used by host environment for regional
   * preferences internationalization.
   *
   * The result is a sorted list and we expect that the OS attempts to
   * use the top locale from the list for which it has data.
   *
   * Each element of the list is a valid locale ID that can be passed to ICU
   * and ECMA402 Intl APIs,
   *
   * Example: ["en-US", "de", "pl", "sr-Cyrl", "zh-Hans-HK"]
   *
   * The return bool value indicates whether the function successfully
   * resolved at least one locale.
   *
   * Usage:
   *   nsTArray<nsCString> systemLocales;
   *   OSPreferences::GetInstance()->GetRegionalPrefsLocales(regionalPrefsLocales);
   *
   * (See mozIOSPreferences.idl for a JS-callable version of this.)
   */
  bool GetRegionalPrefsLocales(nsTArray<nsCString>& aRetVal);

  static bool GetDateTimeConnectorPattern(const nsACString& aLocale,
                                          nsAString& aRetVal);

  /**
   * Triggers a refresh of retrieving data from host environment.
   *
   * If the result differs from the previous list, it will additionally
   * trigger global events for changed values:
   *
   *  * SystemLocales: "intl:system-locales-changed"
   *
   * This method should not be called from anywhere except of per-platform
   * hooks into OS events.
   */
  void Refresh();

protected:
  nsTArray<nsCString> mSystemLocales;
  nsTArray<nsCString> mRegionalPrefsLocales;

private:
  virtual ~OSPreferences() {};

  static StaticRefPtr<OSPreferences> sInstance;

  static bool CanonicalizeLanguageTag(nsCString& aLoc);

  /**
   * Helper methods to get formats from ICU; these will return false
   * in case of error, in which case the caller cannot rely on aRetVal.
   */
  bool GetDateTimePatternForStyle(DateTimeFormatStyle aDateStyle,
                                  DateTimeFormatStyle aTimeStyle,
                                  const nsACString& aLocale,
                                  nsAString& aRetVal);

  bool GetDateTimeSkeletonForStyle(DateTimeFormatStyle aDateStyle,
                                   DateTimeFormatStyle aTimeStyle,
                                   const nsACString& aLocale,
                                   nsAString& aRetVal);

  bool GetPatternForSkeleton(const nsAString& aSkeleton,
                             const nsACString& aLocale,
                             nsAString& aRetVal);

  /**
   * This is a host environment specific method that will be implemented
   * separately for each platform.
   *
   * It is only called when the cache is empty or invalidated.
   *
   * The return value indicates whether the function successfully
   * resolved at least one locale.
   */
  bool ReadSystemLocales(nsTArray<nsCString>& aRetVal);

  bool ReadRegionalPrefsLocales(nsTArray<nsCString>& aRetVal);

  /**
   * This is a host environment specific method that will be implemented
   * separately for each platform.
   *
   * It is `best-effort` kind of API that attempts to construct the best
   * possible date/time pattern for the given styles and locales.
   *
   * In case we fail to, or don't know how to retrieve the pattern in a
   * given environment this function will return false.
   * Callers should always be prepared to handle that scenario.
   *
   * The heuristic may depend on the OS API and HIG guidelines.
   */
  bool ReadDateTimePattern(DateTimeFormatStyle aDateFormatStyle,
                           DateTimeFormatStyle aTimeFormatStyle,
                           const nsACString& aLocale,
                           nsAString& aRetVal);
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_IntlOSPreferences_h__ */
