/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DateTimeFormat_h
#define mozilla_DateTimeFormat_h

#include <time.h>
#include "nsIScriptableDateFormat.h"
#include "nsStringGlue.h"
#include "prtime.h"

#ifndef ENABLE_INTL_API
#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#endif

namespace mozilla {

class DateTimeFormat {
public:
  // performs a locale sensitive date formatting operation on the time_t parameter
  static nsresult FormatTime(const nsDateFormatSelector aDateFormatSelector,
                             const nsTimeFormatSelector aTimeFormatSelector,
                             const time_t aTimetTime,
                             nsAString& aStringOut);

  // performs a locale sensitive date formatting operation on the PRTime parameter
  static nsresult FormatPRTime(const nsDateFormatSelector aDateFormatSelector,
                               const nsTimeFormatSelector aTimeFormatSelector,
                               const PRTime aPrTime,
                               nsAString& aStringOut);

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  static nsresult FormatPRExplodedTime(const nsDateFormatSelector aDateFormatSelector,
                                       const nsTimeFormatSelector aTimeFormatSelector,
                                       const PRExplodedTime* aExplodedTime,
                                       nsAString& aStringOut);

  static void Shutdown();

private:
  DateTimeFormat() = delete;

  static nsresult Initialize();

#ifdef ENABLE_INTL_API
  static nsCString* mLocale;
#else
  // performs a locale sensitive date formatting operation on the struct tm parameter
  static nsresult FormatTMTime(const nsDateFormatSelector aDateFormatSelector,
                               const nsTimeFormatSelector aTimeFormatSelector,
                               const struct tm* aTmTime,
                               nsAString& aStringOut);

  static void LocalePreferred24hour();

  static bool mLocalePreferred24hour;                       // true if 24 hour format is preferred by current locale
  static bool mLocaleAMPMfirst;                             // true if AM/PM string is preferred before the time
  static nsCOMPtr<nsIUnicodeDecoder> mDecoder;
#endif
};

}

#endif  /* mozilla_DateTimeFormat_h */
