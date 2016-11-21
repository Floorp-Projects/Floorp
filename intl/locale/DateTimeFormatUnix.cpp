/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DateTimeFormat.h"
#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

namespace mozilla {

bool DateTimeFormat::mLocalePreferred24hour;
bool DateTimeFormat::mLocaleAMPMfirst;
nsCOMPtr<nsIUnicodeDecoder> DateTimeFormat::mDecoder;

/*static*/ nsresult
DateTimeFormat::Initialize()
{
  nsAutoString localeStr;
  nsAutoCString charset;
  nsresult rv = NS_OK;

  if (mDecoder) {
    return NS_OK;
  }

  charset.AssignLiteral("windows-1252");

  nsCOMPtr<nsILocaleService> localeService =
    do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsILocale> appLocale;
    rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
    if (NS_SUCCEEDED(rv)) {
      rv = appLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_TIME##PLATFORM"), localeStr);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get app locale info");
    }
  }

  if (NS_SUCCEEDED(rv) && !localeStr.IsEmpty()) {
    nsCOMPtr<nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString mappedCharset;
      rv = platformCharset->GetDefaultCharsetForLocale(localeStr, mappedCharset);
      if (NS_SUCCEEDED(rv)) {
        charset = mappedCharset;
      }
    }
  }

  mDecoder = EncodingUtils::DecoderForEncoding(charset);

  LocalePreferred24hour();

  return rv;
}

/*static*/ void
DateTimeFormat::LocalePreferred24hour()
{
  char str[100];
  time_t tt;
  struct tm *tmc;
  int i;

  tt = time(nullptr);
  tmc = localtime(&tt);

  tmc->tm_hour=22;    // put the test sample hour to 22:00 which is 10PM
  tmc->tm_min=0;      // set the min & sec other number than '2'
  tmc->tm_sec=0;

  strftime(str, (size_t)99, "%X", (struct tm *)tmc);

  mLocalePreferred24hour = false;
  for (i=0; str[i]; i++) {
    if (str[i] == '2') {    // if there is any '2', that locale use 0-23 time format
      mLocalePreferred24hour = true;
      break;
    }
  }

  mLocaleAMPMfirst = true;
  if (mLocalePreferred24hour == false) {
    if (str[0] && str[0] == '1') { // if the first character is '1' of 10:00,
			           // AMPM string is located after 10:00
      mLocaleAMPMfirst = false;
    }
  }
}

/*static*/ nsresult
DateTimeFormat::FormatTime(const nsDateFormatSelector aDateFormatSelector,
                           const nsTimeFormatSelector aTimeFormatSelector,
                           const time_t aTimetTime,
                           nsAString& aStringOut)
{
  struct tm tmTime;
  memcpy(&tmTime, localtime(&aTimetTime), sizeof(struct tm));
  return FormatTMTime(aDateFormatSelector, aTimeFormatSelector, &tmTime, aStringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
/*static*/ nsresult
DateTimeFormat::FormatTMTime(const nsDateFormatSelector aDateFormatSelector,
                             const nsTimeFormatSelector aTimeFormatSelector,
                             const struct tm* aTmTime,
                             nsAString& aStringOut)
{
#define NSDATETIME_FORMAT_BUFFER_LEN 80
  char strOut[NSDATETIME_FORMAT_BUFFER_LEN*2];  // buffer for date and time
  char fmtD[NSDATETIME_FORMAT_BUFFER_LEN], fmtT[NSDATETIME_FORMAT_BUFFER_LEN];
  nsresult rv;

  // set up locale data
  (void) Initialize();
  NS_ENSURE_TRUE(mDecoder, NS_ERROR_NOT_INITIALIZED);

  // set date format
  if (aDateFormatSelector == kDateFormatLong && aTimeFormatSelector == kTimeFormatSeconds) {
    PL_strncpy(fmtD, "%c", NSDATETIME_FORMAT_BUFFER_LEN);
    PL_strncpy(fmtT, "", NSDATETIME_FORMAT_BUFFER_LEN);
  } else {

    switch (aDateFormatSelector) {
      case kDateFormatNone:
        PL_strncpy(fmtD, "", NSDATETIME_FORMAT_BUFFER_LEN);
        break;
      case kDateFormatLong:
      case kDateFormatShort:
        PL_strncpy(fmtD, "%x", NSDATETIME_FORMAT_BUFFER_LEN);
        break;
      default:
        PL_strncpy(fmtD, "", NSDATETIME_FORMAT_BUFFER_LEN);
    }

    // set time format
    switch (aTimeFormatSelector) {
      case kTimeFormatNone:
        PL_strncpy(fmtT, "", NSDATETIME_FORMAT_BUFFER_LEN);
        break;
      case kTimeFormatSeconds:
        PL_strncpy(fmtT, "%X", NSDATETIME_FORMAT_BUFFER_LEN);
        break;
      case kTimeFormatNoSeconds:
        PL_strncpy(fmtT,
                   mLocalePreferred24hour ? "%H:%M" : mLocaleAMPMfirst ? "%p %I:%M" : "%I:%M %p",
                   NSDATETIME_FORMAT_BUFFER_LEN);
        break;
      default:
        PL_strncpy(fmtT, "", NSDATETIME_FORMAT_BUFFER_LEN);
    }
  }

  // generate date/time string
  if (strlen(fmtD) && strlen(fmtT)) {
    PL_strncat(fmtD, " ", NSDATETIME_FORMAT_BUFFER_LEN);
    PL_strncat(fmtD, fmtT, NSDATETIME_FORMAT_BUFFER_LEN);
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, aTmTime);
  } else if (strlen(fmtD) && !strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, aTmTime);
  } else if (!strlen(fmtD) && strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtT, aTmTime);
  } else {
    PL_strncpy(strOut, "", NSDATETIME_FORMAT_BUFFER_LEN);
  }

  // convert result to unicode
  int32_t srcLength = (int32_t) strlen(strOut);
  int32_t unicharLength = NSDATETIME_FORMAT_BUFFER_LEN*2;
  char16_t unichars[NSDATETIME_FORMAT_BUFFER_LEN*2];   // buffer for date and time

  rv = mDecoder->Convert(strOut, &srcLength, unichars, &unicharLength);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aStringOut.Assign(unichars, unicharLength);

  return rv;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
/*static*/ nsresult
DateTimeFormat::FormatPRTime(const nsDateFormatSelector aDateFormatSelector,
                             const nsTimeFormatSelector aTimeFormatSelector,
                             const PRTime aPrTime,
                             nsAString& aStringOut)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(aPrTime, PR_LocalTimeParameters, &explodedTime);

  return FormatPRExplodedTime(aDateFormatSelector, aTimeFormatSelector, &explodedTime, aStringOut);
}

// performs a locale sensitive date formatting operation on the PRExplodedTime parameter
/*static*/ nsresult
DateTimeFormat::FormatPRExplodedTime(const nsDateFormatSelector aDateFormatSelector,
                                     const nsTimeFormatSelector aTimeFormatSelector,
                                     const PRExplodedTime* aExplodedTime,
                                     nsAString& aStringOut)
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

  tmTime.tm_yday = aExplodedTime->tm_yday;
  tmTime.tm_wday = aExplodedTime->tm_wday;
  tmTime.tm_year = aExplodedTime->tm_year;
  tmTime.tm_year -= 1900;
  tmTime.tm_mon = aExplodedTime->tm_month;
  tmTime.tm_mday = aExplodedTime->tm_mday;
  tmTime.tm_hour = aExplodedTime->tm_hour;
  tmTime.tm_min = aExplodedTime->tm_min;
  tmTime.tm_sec = aExplodedTime->tm_sec;

  return FormatTMTime(aDateFormatSelector, aTimeFormatSelector, &tmTime, aStringOut);
}

/*static*/ void
DateTimeFormat::Shutdown()
{
}

}
