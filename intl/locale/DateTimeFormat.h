/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DateTimeFormat_h
#define mozilla_DateTimeFormat_h

#include <time.h>
#include "gtest/MozGtestFriend.h"
#include "nsString.h"
#include "prtime.h"
#include "unicode/udat.h"

namespace mozilla {

enum nsDateFormatSelector : long
{
  // Do not change the order of the values below (see bug 1225696).
  kDateFormatNone = 0,            // do not include the date  in the format string
  kDateFormatLong,                // provides the long date format for the given locale
  kDateFormatShort,               // provides the short date format for the given locale
  kDateFormatYearMonth,           // formats using only the year and month
  kDateFormatWeekday,             // week day (e.g. Mon, Tue)
  kDateFormatYearMonthLong,       // long version of kDateFormatYearMonth
  kDateFormatMonthLong            // long format of month name only
};

enum nsTimeFormatSelector : long
{
  kTimeFormatNone = 0,            // don't include the time in the format string
  kTimeFormatSeconds,             // provides the time format with seconds in the  given locale 
  kTimeFormatNoSeconds            // provides the time format without seconds in the given locale 
};

class DateTimeFormat {
public:
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

  FRIEND_TEST(DateTimeFormat, FormatPRExplodedTime);
  FRIEND_TEST(DateTimeFormat, DateFormatSelectors);
  FRIEND_TEST(DateTimeFormat, FormatPRExplodedTimeForeign);
  FRIEND_TEST(DateTimeFormat, DateFormatSelectorsForeign);

  // performs a locale sensitive date formatting operation on the UDate parameter
  static nsresult FormatUDateTime(const nsDateFormatSelector aDateFormatSelector,
                                  const nsTimeFormatSelector aTimeFormatSelector,
                                  const UDate aUDateTime,
                                  const PRTimeParameters* aTimeParameters,
                                  nsAString& aStringOut);

  static nsCString* mLocale;
};

}

#endif  /* mozilla_DateTimeFormat_h */
