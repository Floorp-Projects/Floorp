/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <locale.h>
#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsDateTimeFormatUnix.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIPosixLocale.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDateTimeFormatUnix, nsIDateTimeFormat)

// init this interface to a specified locale
nsresult nsDateTimeFormatUnix::Initialize(nsILocale* locale)
{
  nsAutoString localeStr;
  NS_NAMED_LITERAL_STRING(aCategory, "NSILOCALE_TIME");
  nsresult res = NS_OK;

  // use cached info if match with stored locale
  if (NULL == locale) {
    if (!mLocale.IsEmpty() &&
        mLocale.Equals(mAppLocale, nsCaseInsensitiveStringComparator())) {
      return NS_OK;
    }
  }
  else {
    res = locale->GetCategory(aCategory, localeStr);
    if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
      if (!mLocale.IsEmpty() &&
          mLocale.Equals(localeStr,
                         nsCaseInsensitiveStringComparator())) {
        return NS_OK;
      }
    }
  }

  mCharset.AssignLiteral("ISO-8859-1");
  mPlatformLocale.Assign("en_US");

  // get locale name string, use app default if no locale specified
  if (NULL == locale) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCOMPtr<nsILocale> appLocale;
      res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(aCategory, localeStr);
        if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
          NS_ASSERTION(NS_SUCCEEDED(res), "failed to get app locale info");
          mAppLocale = localeStr; // cache app locale name
        }
      }
    }
  }
  else {
    res = locale->GetCategory(aCategory, localeStr);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to get locale info");
  }

  if (NS_SUCCEEDED(res) && !localeStr.IsEmpty()) {
    mLocale = localeStr; // cache locale name

    nsCOMPtr <nsIPosixLocale> posixLocale = do_GetService(NS_POSIXLOCALE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      res = posixLocale->GetPlatformLocale(mLocale, mPlatformLocale);
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCAutoString mappedCharset;
      res = platformCharset->GetDefaultCharsetForLocale(mLocale, mappedCharset);
      if (NS_SUCCEEDED(res)) {
        mCharset = mappedCharset;
      }
    }
  }

  // Initialize unicode decoder
  nsCOMPtr <nsICharsetConverterManager>  charsetConverterManager;
  charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res);
  if (NS_SUCCEEDED(res)) {
    res = charsetConverterManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mDecoder));
  }

  LocalePreferred24hour();

  return res;
}

void nsDateTimeFormatUnix::LocalePreferred24hour()
{
  char str[100];
  time_t tt;
  struct tm *tmc;
  int i;

  tt = time((time_t)NULL);
  tmc = localtime(&tt);

  tmc->tm_hour=22;    // put the test sample hour to 22:00 which is 10PM
  tmc->tm_min=0;      // set the min & sec other number than '2'
  tmc->tm_sec=0;

  char *temp = setlocale(LC_TIME, mPlatformLocale.get());
  strftime(str, (size_t)99, "%X", (struct tm *)tmc);

  (void) setlocale(LC_TIME, temp);

  mLocalePreferred24hour = PR_FALSE;
  for (i=0; str[i]; i++) {
    if (str[i] == '2') {    // if there is any '2', that locale use 0-23 time format
        mLocalePreferred24hour = PR_TRUE;
        break;
    }
  }

  mLocaleAMPMfirst = PR_TRUE;
  if (mLocalePreferred24hour == PR_FALSE) {
    if (str[0] && str[0] == '1') { // if the first character is '1' of 10:00,
			           // AMPM string is located after 10:00
      mLocaleAMPMfirst = PR_FALSE;
    }
  }
}

nsresult nsDateTimeFormatUnix::FormatTime(nsILocale* locale, 
                                      const nsDateFormatSelector  dateFormatSelector, 
                                      const nsTimeFormatSelector timeFormatSelector, 
                                      const time_t  timetTime, 
                                      nsString& stringOut) 
{
  struct tm tmTime;
  memcpy(&tmTime, localtime(&timetTime), sizeof(struct tm));
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, &tmTime, stringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatUnix::FormatTMTime(nsILocale* locale, 
                                        const nsDateFormatSelector  dateFormatSelector, 
                                        const nsTimeFormatSelector timeFormatSelector, 
                                        const struct tm*  tmTime, 
                                        nsString& stringOut) 
{
#define NSDATETIME_FORMAT_BUFFER_LEN  80
  char strOut[NSDATETIME_FORMAT_BUFFER_LEN*2];  // buffer for date and time
  char fmtD[NSDATETIME_FORMAT_BUFFER_LEN], fmtT[NSDATETIME_FORMAT_BUFFER_LEN];
  nsresult rv;

  
  // set up locale data
  (void) Initialize(locale);
  NS_ENSURE_TRUE(mDecoder, NS_ERROR_NOT_INITIALIZED);

  // set date format
  switch (dateFormatSelector) {
    case kDateFormatNone:
      PL_strncpy(fmtD, "", NSDATETIME_FORMAT_BUFFER_LEN);
      break; 
    case kDateFormatLong:
    case kDateFormatShort:
      PL_strncpy(fmtD, "%x", NSDATETIME_FORMAT_BUFFER_LEN);
      break; 
    case kDateFormatYearMonth:
      PL_strncpy(fmtD, "%y/%m", NSDATETIME_FORMAT_BUFFER_LEN);
      break; 
    case kDateFormatWeekday:
      PL_strncpy(fmtD, "%a", NSDATETIME_FORMAT_BUFFER_LEN);
      break;
    default:
      PL_strncpy(fmtD, "", NSDATETIME_FORMAT_BUFFER_LEN); 
  }

  // set time format
  switch (timeFormatSelector) {
    case kTimeFormatNone:
      PL_strncpy(fmtT, "", NSDATETIME_FORMAT_BUFFER_LEN); 
      break;
    case kTimeFormatSeconds:
      PL_strncpy(fmtT, 
                 mLocalePreferred24hour ? "%H:%M:%S" : mLocaleAMPMfirst ? "%p %I:%M:%S" : "%I:%M:%S %p", 
                 NSDATETIME_FORMAT_BUFFER_LEN);
      break;
    case kTimeFormatNoSeconds:
      PL_strncpy(fmtT, 
                 mLocalePreferred24hour ? "%H:%M" : mLocaleAMPMfirst ? "%p %I:%M" : "%I:%M %p", 
                 NSDATETIME_FORMAT_BUFFER_LEN);
      break;
    case kTimeFormatSecondsForce24Hour:
      PL_strncpy(fmtT, "%H:%M:%S", NSDATETIME_FORMAT_BUFFER_LEN);
      break;
    case kTimeFormatNoSecondsForce24Hour:
      PL_strncpy(fmtT, "%H:%M", NSDATETIME_FORMAT_BUFFER_LEN);
      break;
    default:
      PL_strncpy(fmtT, "", NSDATETIME_FORMAT_BUFFER_LEN); 
  }

  // generate data/time string
  char *old_locale = setlocale(LC_TIME, NULL);
  (void) setlocale(LC_TIME, mPlatformLocale.get());
  if (PL_strlen(fmtD) && PL_strlen(fmtT)) {
    PL_strncat(fmtD, " ", NSDATETIME_FORMAT_BUFFER_LEN);
    PL_strncat(fmtD, fmtT, NSDATETIME_FORMAT_BUFFER_LEN);
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (PL_strlen(fmtD) && !PL_strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (!PL_strlen(fmtD) && PL_strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtT, tmTime);
  }
  else {
    PL_strncpy(strOut, "", NSDATETIME_FORMAT_BUFFER_LEN);
  }
  (void) setlocale(LC_TIME, old_locale);

  // convert result to unicode
  PRInt32 srcLength = (PRInt32) PL_strlen(strOut);
  PRInt32 unicharLength = NSDATETIME_FORMAT_BUFFER_LEN*2;
  PRUnichar unichars[NSDATETIME_FORMAT_BUFFER_LEN*2];   // buffer for date and time

  rv = mDecoder->Convert(strOut, &srcLength, unichars, &unicharLength);
  if (NS_FAILED(rv))
    return rv;
  stringOut.Assign(unichars, unicharLength);

  return rv;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
nsresult nsDateTimeFormatUnix::FormatPRTime(nsILocale* locale, 
                                           const nsDateFormatSelector  dateFormatSelector, 
                                           const nsTimeFormatSelector timeFormatSelector, 
                                           const PRTime  prTime, 
                                           nsString& stringOut)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(prTime, PR_LocalTimeParameters, &explodedTime);

  return FormatPRExplodedTime(locale, dateFormatSelector, timeFormatSelector, &explodedTime, stringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime parameter
nsresult nsDateTimeFormatUnix::FormatPRExplodedTime(nsILocale* locale, 
                                                   const nsDateFormatSelector  dateFormatSelector, 
                                                   const nsTimeFormatSelector timeFormatSelector, 
                                                   const PRExplodedTime*  explodedTime, 
                                                   nsString& stringOut)
{
  struct tm  tmTime;
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

