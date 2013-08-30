/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <CoreFoundation/CoreFoundation.h>
#include "nsIServiceManager.h"
#include "nsDateTimeFormatMac.h"
#include <CoreFoundation/CFDateFormatter.h>
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsUnicharUtils.h"
#include "nsTArray.h"


NS_IMPL_ISUPPORTS1(nsDateTimeFormatMac, nsIDateTimeFormat)

nsresult nsDateTimeFormatMac::Initialize(nsILocale* locale)
{
  nsAutoString localeStr;
  nsAutoString category(NS_LITERAL_STRING("NSILOCALE_TIME"));
  nsresult res;

  // use cached info if match with stored locale
  if (nullptr == locale) {
    if (!mLocale.IsEmpty() &&
        mLocale.Equals(mAppLocale, nsCaseInsensitiveStringComparator())) {
      return NS_OK;
    }
  }
  else {
    res = locale->GetCategory(category, localeStr);
    if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
      if (!mLocale.IsEmpty() &&
          mLocale.Equals(localeStr,
                         nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }
    }
  }

  // get application locale
  nsCOMPtr<nsILocaleService> localeService = 
           do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
  if (NS_SUCCEEDED(res)) {
    nsCOMPtr<nsILocale> appLocale;
    res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
    if (NS_SUCCEEDED(res)) {
      res = appLocale->GetCategory(category, localeStr);
      if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
        mAppLocale = localeStr; // cache app locale name
      }
    }
  }
  
  // use app default if no locale specified
  if (nullptr == locale) {
    mUseDefaultLocale = true;
  }
  else {
    mUseDefaultLocale = false;
    res = locale->GetCategory(category, localeStr);
  }
    
  if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
    mLocale.Assign(localeStr); // cache locale name
  }

  return res;
}

// performs a locale sensitive date formatting operation on the time_t parameter
nsresult nsDateTimeFormatMac::FormatTime(nsILocale* locale, 
                                      const nsDateFormatSelector  dateFormatSelector, 
                                      const nsTimeFormatSelector timeFormatSelector, 
                                      const time_t  timetTime, 
                                      nsAString& stringOut)
{
  struct tm tmTime;
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, localtime_r(&timetTime, &tmTime), stringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatMac::FormatTMTime(nsILocale* locale, 
                                           const nsDateFormatSelector  dateFormatSelector, 
                                           const nsTimeFormatSelector timeFormatSelector, 
                                           const struct tm*  tmTime, 
                                           nsAString& stringOut)
{
  nsresult res = NS_OK;

  // set up locale data
  (void) Initialize(locale);
  
  // return, nothing to format
  if (dateFormatSelector == kDateFormatNone && timeFormatSelector == kTimeFormatNone) {
    stringOut.Truncate();
    return NS_OK;
  }

  NS_ASSERTION(tmTime->tm_mon >= 0, "tm is not set correctly");
  NS_ASSERTION(tmTime->tm_mday >= 1, "tm is not set correctly");
  NS_ASSERTION(tmTime->tm_hour >= 0, "tm is not set correctly");
  NS_ASSERTION(tmTime->tm_min >= 0, "tm is not set correctly");
  NS_ASSERTION(tmTime->tm_sec >= 0, "tm is not set correctly");
  NS_ASSERTION(tmTime->tm_wday >= 0, "tm is not set correctly");

  // Got the locale for the formatter:
  CFLocaleRef formatterLocale;
  if (!locale) {
    formatterLocale = CFLocaleCopyCurrent();
  } else {
    CFStringRef localeStr = CFStringCreateWithCharacters(nullptr,
                                                         mLocale.get(),
                                                         mLocale.Length());
    formatterLocale = CFLocaleCreate(nullptr, localeStr);
    CFRelease(localeStr);
  }

  // Get the date style for the formatter:  
  CFDateFormatterStyle dateStyle;
  switch (dateFormatSelector) {
    case kDateFormatLong:
      dateStyle = kCFDateFormatterLongStyle;
      break;
    case kDateFormatShort:
      dateStyle = kCFDateFormatterShortStyle;
      break;
    case kDateFormatYearMonth:
    case kDateFormatWeekday:
      dateStyle = kCFDateFormatterNoStyle; // formats handled below
      break;
    case kDateFormatNone:
      dateStyle = kCFDateFormatterNoStyle;
      break;
    default:
      NS_ERROR("Unknown nsDateFormatSelector");
      res = NS_ERROR_FAILURE;
      dateStyle = kCFDateFormatterNoStyle;
  }
  
  // Get the time style for the formatter:
  CFDateFormatterStyle timeStyle;
  switch (timeFormatSelector) {
    case kTimeFormatSeconds:
    case kTimeFormatSecondsForce24Hour: // 24 hour part fixed below
      timeStyle = kCFDateFormatterMediumStyle;
      break;
    case kTimeFormatNoSeconds:
    case kTimeFormatNoSecondsForce24Hour: // 24 hour part fixed below
      timeStyle = kCFDateFormatterShortStyle;
      break;
    case kTimeFormatNone:
      timeStyle = kCFDateFormatterNoStyle;
      break;
    default:
      NS_ERROR("Unknown nsTimeFormatSelector");
      res = NS_ERROR_FAILURE;
      timeStyle = kCFDateFormatterNoStyle;
  }
  
  // Create the formatter and fix up its formatting as necessary:
  CFDateFormatterRef formatter =
    CFDateFormatterCreate(nullptr, formatterLocale, dateStyle, timeStyle);
  
  CFRelease(formatterLocale);
  
  if (dateFormatSelector == kDateFormatYearMonth ||
      dateFormatSelector == kDateFormatWeekday) {
    CFStringRef dateFormat =
      dateFormatSelector == kDateFormatYearMonth ? CFSTR("yyyy/MM ") : CFSTR("EEE ");
    
    CFStringRef oldFormat = CFDateFormatterGetFormat(formatter);
    CFMutableStringRef newFormat = CFStringCreateMutableCopy(nullptr, 0, oldFormat);
    CFStringInsert(newFormat, 0, dateFormat);
    CFDateFormatterSetFormat(formatter, newFormat);
    CFRelease(newFormat); // note we don't own oldFormat
  }
  
  if (timeFormatSelector == kTimeFormatSecondsForce24Hour ||
      timeFormatSelector == kTimeFormatNoSecondsForce24Hour) {
    // Replace "h" with "H", and remove "a":
    CFStringRef oldFormat = CFDateFormatterGetFormat(formatter);
    CFMutableStringRef newFormat = CFStringCreateMutableCopy(nullptr, 0, oldFormat);
    CFIndex replaceCount = CFStringFindAndReplace(newFormat,
                                                  CFSTR("h"), CFSTR("H"),
                                                  CFRangeMake(0, CFStringGetLength(newFormat)),	
                                                  0);
    NS_ASSERTION(replaceCount == 1, "Unexpected number of \"h\" occurrences");
    replaceCount = CFStringFindAndReplace(newFormat,
                                          CFSTR("a"), CFSTR(""),
                                          CFRangeMake(0, CFStringGetLength(newFormat)),	
                                          0);
    NS_ASSERTION(replaceCount == 1, "Unexpected number of \"a\" occurrences");
    CFDateFormatterSetFormat(formatter, newFormat);
    CFRelease(newFormat); // note we don't own oldFormat
  }
  
  // Now get the formatted date:
  CFGregorianDate date;
  date.second = tmTime->tm_sec;
  date.minute = tmTime->tm_min;
  date.hour = tmTime->tm_hour;
  date.day = tmTime->tm_mday;      // Mac is 1-based, tm is 1-based
  date.month = tmTime->tm_mon + 1; // Mac is 1-based, tm is 0-based
  date.year = tmTime->tm_year + 1900;

  CFTimeZoneRef timeZone = CFTimeZoneCopySystem(); // tmTime is in local time
  CFAbsoluteTime absTime = CFGregorianDateGetAbsoluteTime(date, timeZone);
  CFRelease(timeZone);

  CFStringRef formattedDate = CFDateFormatterCreateStringWithAbsoluteTime(nullptr,
                                                                          formatter,
                                                                          absTime);
  
  CFIndex stringLen = CFStringGetLength(formattedDate);
  
  nsAutoTArray<UniChar, 256> stringBuffer;
  if (stringBuffer.SetLength(stringLen + 1)) {
    CFStringGetCharacters(formattedDate, CFRangeMake(0, stringLen), stringBuffer.Elements());
    stringOut.Assign(stringBuffer.Elements(), stringLen);
  }
  
  CFRelease(formattedDate);
  CFRelease(formatter);
  
  return res;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
nsresult nsDateTimeFormatMac::FormatPRTime(nsILocale* locale, 
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
nsresult nsDateTimeFormatMac::FormatPRExplodedTime(nsILocale* locale, 
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

