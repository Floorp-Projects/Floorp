/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeFormat.h"
#include "prtime.h"

#include <time.h>

#define NSDATETIME_FORMAT_BUFFER_LEN 80

namespace mozilla {

nsCString* DateTimeFormat::mLocale = nullptr;

// performs a locale sensitive date formatting operation on the struct tm
// parameter

static nsresult
FormatTMTime(const nsDateFormatSelector aDateFormatSelector,
             const nsTimeFormatSelector aTimeFormatSelector,
             const struct tm* aTmTime,
             nsAString& aStringOut)
{
  // set date format
  nsAutoCString format;
  if (aDateFormatSelector == kDateFormatLong &&
      aTimeFormatSelector == kTimeFormatSeconds) {
    format.AssignLiteral("%c");
  } else {
    switch (aDateFormatSelector) {
      case kDateFormatLong:
      case kDateFormatShort:
        format.AssignLiteral("%x");
        break;
      case kDateFormatWeekday:
        format.AssignLiteral("%a");
        break;
      case kDateFormatMonthLong:
        format.AssignLiteral("%B");
        break;
      case kDateFormatYearMonth:
        format.AssignLiteral("%m/%Y");
        break;
      case kDateFormatYearMonthLong:
        format.AssignLiteral("%B %Y");
        break;
      case kDateFormatNone:
      default:
        break;
    }

    // set time format
    switch (aTimeFormatSelector) {
      case kTimeFormatSeconds:
        if (!format.IsEmpty()) {
          format.AppendLiteral(" ");
        }
        format.AppendLiteral("%X");
        break;
      case kTimeFormatNoSeconds:
        if (!format.IsEmpty()) {
          format.AppendLiteral(" ");
        }
        format.AppendLiteral("%H:%M");
        break;
      case kTimeFormatNone:
      default:
        break;
    }
  }

  // generate date/time string
  char strOut[NSDATETIME_FORMAT_BUFFER_LEN];
  if (!format.IsEmpty()) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, format.get(), aTmTime);
    CopyUTF8toUTF16(strOut, aStringOut);
  } else {
    aStringOut.Truncate();
  }

  return NS_OK;
}

/*static*/ nsresult
DateTimeFormat::FormatTime(const nsDateFormatSelector aDateFormatSelector,
                           const nsTimeFormatSelector aTimeFormatSelector,
                           const time_t aTimetTime,
                           nsAString& aStringOut)
{
  struct tm tmTime;
  memcpy(&tmTime, localtime(&aTimetTime), sizeof(struct tm));
  return FormatTMTime(
    aDateFormatSelector, aTimeFormatSelector, &tmTime, aStringOut);
}

// performs a locale sensitive date formatting operation on the PRTime parameter

// static
nsresult
DateTimeFormat::FormatPRTime(const nsDateFormatSelector aDateFormatSelector,
                             const nsTimeFormatSelector aTimeFormatSelector,
                             const PRTime aPrTime,
                             nsAString& aStringOut)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(aPrTime, PR_LocalTimeParameters, &explodedTime);

  return FormatPRExplodedTime(
    aDateFormatSelector, aTimeFormatSelector, &explodedTime, aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime
// parameter
// static
nsresult
DateTimeFormat::FormatPRExplodedTime(
  const nsDateFormatSelector aDateFormatSelector,
  const nsTimeFormatSelector aTimeFormatSelector,
  const PRExplodedTime* aExplodedTime,
  nsAString& aStringOut)
{
  struct tm tmTime;
  /* be safe and set all members of struct tm to zero
   *
   * there are other fields in the tm struct that we aren't setting
   * (tm_isdst, tm_gmtoff, tm_zone, should we set these?) and since
   * tmTime is on the stack, it may be filled with garbage, but
   * the garbage may vary.  (this may explain why some saw bug #10412, and
   * others did not.
   *
   * when tmTime is passed to strftime() with garbage bad things may happen.
   * see bug #10412
   */
  memset(&tmTime, 0, sizeof(tmTime));

  tmTime.tm_yday = aExplodedTime->tm_yday;
  tmTime.tm_wday = aExplodedTime->tm_wday;
  tmTime.tm_year = aExplodedTime->tm_year;
  tmTime.tm_year -= 1900;
  tmTime.tm_mon = aExplodedTime->tm_month;
  tmTime.tm_mday = aExplodedTime->tm_mday;
  tmTime.tm_hour = aExplodedTime->tm_hour;
  tmTime.tm_min = aExplodedTime->tm_min;
  tmTime.tm_sec = aExplodedTime->tm_sec;

  return FormatTMTime(
    aDateFormatSelector, aTimeFormatSelector, &tmTime, aStringOut);
}

// static
void
DateTimeFormat::Shutdown()
{
}

} // namespace mozilla
