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
class OSPreferences
{

public:
  static OSPreferences* GetInstance();

  /**
   * Returns a list of locales used by the host environment.
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
   */
  bool GetSystemLocales(nsTArray<nsCString>& aRetVal);

protected:
  nsTArray<nsCString> mSystemLocales;

private:
  static StaticAutoPtr<OSPreferences> sInstance;

  static bool CanonicalizeLanguageTag(nsCString& aLoc);

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
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_IntlOSPreferences_h__ */
