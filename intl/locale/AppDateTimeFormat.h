/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_AppDateTimeFormat_h
#define mozilla_intl_AppDateTimeFormat_h

#include <time.h>
#include "gtest/MozGtestFriend.h"
#include "nsTHashMap.h"
#include "nsString.h"
#include "prtime.h"
#include "mozilla/intl/DateTimeFormat.h"

namespace mozilla::intl {

/**
 * Get a DateTimeFormat for use in Gecko. This specialized DateTimeFormat
 * respects the user's OS and app preferences, and provides caching of the
 * underlying mozilla::intl resources.
 *
 * This class is not thread-safe as it lazily initializes a cache without
 * any type of multi-threaded protections.
 */
class AppDateTimeFormat {
 public:
  /**
   * Format a DateTime using the applied app and OS-level preferences, with a
   * style bag and the PRTime.
   */
  static nsresult Format(const DateTimeFormat::StyleBag& aStyle,
                         const PRTime aPrTime, nsAString& aStringOut);

  /**
   * Format a DateTime using the applied app and OS-level preferences, with a
   * style bag and the PRExplodedTime.
   */
  static nsresult Format(const DateTimeFormat::StyleBag& aStyle,
                         const PRExplodedTime* aExplodedTime,
                         nsAString& aStringOut);

  /**
   * Format a DateTime using the applied app and OS-level preferences, with a
   * components bag and the PRExplodedTime.
   */
  static nsresult Format(const DateTimeFormat::ComponentsBag& aComponents,
                         const PRExplodedTime* aExplodedTime,
                         nsAString& aStringOut);

  /**
   * If the app locale changes, the cached locale needs to be reset.
   */
  static void ClearLocaleCache();

  static void Shutdown();

 private:
  AppDateTimeFormat() = delete;

  static nsresult Initialize();
  static void DeleteCache();
  static const size_t kMaxCachedFormats = 15;

  FRIEND_TEST(AppDateTimeFormat, FormatPRExplodedTime);
  FRIEND_TEST(AppDateTimeFormat, DateFormatSelectors);
  FRIEND_TEST(AppDateTimeFormat, FormatPRExplodedTimeForeign);
  FRIEND_TEST(AppDateTimeFormat, DateFormatSelectorsForeign);

  /**
   * Format a DateTime using the applied app and OS-level preferences, with a
   * components bag and the PRExplodedTime.
   */
  static nsresult Format(const DateTimeFormat::StyleBag& aStyle,
                         const double aUnixEpoch,
                         const PRTimeParameters* aTimeParameters,
                         nsAString& aStringOut);

  static void BuildTimeZoneString(const PRTimeParameters& aTimeParameters,
                                  nsAString& aStringOut);

  static nsCString* sLocale;
  static nsTHashMap<nsCStringHashKey, UniquePtr<DateTimeFormat>>* sFormatCache;
};

}  // namespace mozilla::intl

#endif /* mozilla_intl_AppDateTimeFormat_h */
