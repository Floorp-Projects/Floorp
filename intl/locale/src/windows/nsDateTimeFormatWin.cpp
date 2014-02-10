/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDateTimeFormatWin.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsILocaleService.h"
#include "nsWin32Locale.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"


#define NSDATETIMEFORMAT_BUFFER_LEN  80

NS_IMPL_ISUPPORTS1(nsDateTimeFormatWin, nsIDateTimeFormat)


// init this interface to a specified locale
nsresult nsDateTimeFormatWin::Initialize(nsILocale* locale)
{
  nsAutoString localeStr;
  nsresult res = NS_OK;

  // use cached info if match with stored locale
  if (!locale) {
    if (!mLocale.IsEmpty() && 
        mLocale.Equals(mAppLocale, nsCaseInsensitiveStringComparator())) {
      return NS_OK;
    }
  }
  else {
    res = locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME"), localeStr);
    if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
      if (!mLocale.IsEmpty() && 
          mLocale.Equals(localeStr, nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }
    }
  }

  // default LCID (en-US)
  mLCID = 1033;

  // get locale string, use app default if no locale specified
  if (!locale) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID);
    if (localeService) {
      nsCOMPtr<nsILocale> appLocale;
      res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME"), 
			             localeStr);
        if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
          mAppLocale.Assign(localeStr); // cache app locale name
        }
      }
    }
  }
  else {
    res = locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME"), localeStr);
  }

  // Get LCID and charset name from locale, if available
  if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
    mLocale.Assign(localeStr); // cache locale name
    res = nsWin32Locale::GetPlatformLocale(mLocale, (LCID *) &mLCID);
  }

  return res;
}

// performs a locale sensitive date formatting operation on the time_t parameter
nsresult nsDateTimeFormatWin::FormatTime(nsILocale* locale, 
                                         const nsDateFormatSelector  dateFormatSelector, 
                                         const nsTimeFormatSelector timeFormatSelector, 
                                         const time_t  timetTime, 
                                         nsAString& stringOut)
{
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, localtime( &timetTime ), stringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatWin::FormatTMTime(nsILocale* locale, 
                                           const nsDateFormatSelector  dateFormatSelector, 
                                           const nsTimeFormatSelector timeFormatSelector, 
                                           const struct tm*  tmTime, 
                                           nsAString& stringOut)
{
  SYSTEMTIME system_time;
  DWORD dwFlags_Date = 0, dwFlags_Time = 0;
  int dateLen, timeLen;
  char16_t dateBuffer[NSDATETIMEFORMAT_BUFFER_LEN], timeBuffer[NSDATETIMEFORMAT_BUFFER_LEN];

  // set up locale data
  (void) Initialize(locale);

  // Map tm to SYSTEMTIME
  system_time.wYear = 1900 + tmTime->tm_year;
  system_time.wMonth = tmTime->tm_mon + 1;
  system_time.wDayOfWeek = tmTime->tm_wday;
  system_time.wDay = tmTime->tm_mday;
  system_time.wHour = tmTime->tm_hour;
  system_time.wMinute = tmTime->tm_min;
  system_time.wSecond = tmTime->tm_sec;
  system_time.wMilliseconds = 0;

  // Map to WinAPI date format
  switch (dateFormatSelector) {
  case kDateFormatLong:
    dwFlags_Date = DATE_LONGDATE;
    break;
  case kDateFormatShort:
    dwFlags_Date = DATE_SHORTDATE;
    break;
  case kDateFormatWeekday:
    dwFlags_Date = 0;
    break;
  case kDateFormatYearMonth:
    dwFlags_Date = 0;     // TODO:only availabe NT5
    break;
  }

  // Map to WinAPI time format
  switch (timeFormatSelector) {
  case kTimeFormatSeconds:
    dwFlags_Time = 0;
    break;
  case kTimeFormatNoSeconds:
    dwFlags_Time = TIME_NOSECONDS;
    break;
  case kTimeFormatSecondsForce24Hour:
    dwFlags_Time = TIME_FORCE24HOURFORMAT;
    break;
  case kTimeFormatNoSecondsForce24Hour:
    dwFlags_Time = TIME_NOSECONDS + TIME_FORCE24HOURFORMAT;
    break;
  }

  // Call GetDateFormatW
  if (dateFormatSelector == kDateFormatNone) {
    dateLen = 0;
  }
  else {
    if (dateFormatSelector == kDateFormatYearMonth) {
      dateLen = nsGetDateFormatW(0, &system_time, "yyyy/MM", 
                                 dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    else if (dateFormatSelector == kDateFormatWeekday) {
      dateLen = nsGetDateFormatW(0, &system_time, "ddd", 
                                 dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    else {
      dateLen = nsGetDateFormatW(dwFlags_Date, &system_time, nullptr,
                                 dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    if (dateLen != 0) {
      dateLen--;  // Since the count includes the terminating null.
    }
  }

  // Call GetTimeFormatW
  if (timeFormatSelector == kTimeFormatNone) {
    timeLen = 0;
  }
  else {
    timeLen = nsGetTimeFormatW(dwFlags_Time, &system_time, nullptr, 
                               timeBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    if (timeLen != 0) {
      timeLen--;  // Since the count includes the terminating null.
    }
  }

  NS_ASSERTION(NSDATETIMEFORMAT_BUFFER_LEN >= (uint32_t) (dateLen + 1), "internal date buffer is not large enough");
  NS_ASSERTION(NSDATETIMEFORMAT_BUFFER_LEN >= (uint32_t) (timeLen + 1), "internal time buffer is not large enough");

  // Copy the result
  stringOut.Truncate();
  if (dateLen != 0 && timeLen != 0) {
    stringOut.Assign(dateBuffer, dateLen);
    stringOut.Append((char16_t *)(L" "), 1);
    stringOut.Append(timeBuffer, timeLen);
  }
  else if (dateLen != 0 && timeLen == 0) {
    stringOut.Assign(dateBuffer, dateLen);
  }
  else if (dateLen == 0 && timeLen != 0) {
    stringOut.Assign(timeBuffer, timeLen);
  }

  return NS_OK;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
nsresult nsDateTimeFormatWin::FormatPRTime(nsILocale* locale, 
                                           const nsDateFormatSelector  dateFormatSelector, 
                                           const nsTimeFormatSelector timeFormatSelector, 
                                           const PRTime  prTime, 
                                           nsAString& stringOut)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(prTime, PR_LocalTimeParameters, &explodedTime);

  return FormatPRExplodedTime(locale, dateFormatSelector, timeFormatSelector, &explodedTime, stringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime parameter
nsresult nsDateTimeFormatWin::FormatPRExplodedTime(nsILocale* locale, 
                                                   const nsDateFormatSelector  dateFormatSelector, 
                                                   const nsTimeFormatSelector timeFormatSelector, 
                                                   const PRExplodedTime*  explodedTime, 
                                                   nsAString& stringOut)
{
  struct tm  tmTime;
  memset( &tmTime, 0, sizeof(tmTime) );

  tmTime.tm_yday = explodedTime->tm_yday;
  tmTime.tm_wday = explodedTime->tm_wday;
  tmTime.tm_year = explodedTime->tm_year;
  tmTime.tm_year -= 1900;
  tmTime.tm_mon = explodedTime->tm_month;
  tmTime.tm_mday = explodedTime->tm_mday;
  tmTime.tm_hour = explodedTime->tm_hour;
  tmTime.tm_min = explodedTime->tm_min;
  tmTime.tm_sec = explodedTime->tm_sec;

  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, &tmTime, stringOut);
}

int nsDateTimeFormatWin::nsGetTimeFormatW(DWORD dwFlags, const SYSTEMTIME *lpTime,
                                          const char* format, char16_t *timeStr, int cchTime)
{
  int len = 0;
  len = GetTimeFormatW(mLCID, dwFlags, lpTime, 
                       format ?
                       NS_ConvertASCIItoUTF16(format).get() :
                       nullptr,
                       (LPWSTR) timeStr, cchTime);
  return len;
}

int nsDateTimeFormatWin::nsGetDateFormatW(DWORD dwFlags, const SYSTEMTIME *lpDate,
                                          const char* format, char16_t *dateStr, int cchDate)
{
  int len = 0;
  len = GetDateFormatW(mLCID, dwFlags, lpDate, 
                       format ? NS_ConvertASCIItoUTF16(format).get() : nullptr,
                       (LPWSTR) dateStr, cchDate);
  return len;
}
