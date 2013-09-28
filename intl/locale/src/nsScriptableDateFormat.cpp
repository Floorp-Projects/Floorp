/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILocaleService.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsIScriptableDateFormat.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"

static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

class nsScriptableDateFormat : public nsIScriptableDateFormat {
 public: 
  NS_DECL_ISUPPORTS 

  NS_IMETHOD FormatDateTime(const PRUnichar *locale, 
                            nsDateFormatSelector dateFormatSelector, 
                            nsTimeFormatSelector timeFormatSelector, 
                            int32_t year, 
                            int32_t month, 
                            int32_t day, 
                            int32_t hour, 
                            int32_t minute, 
                            int32_t second, 
                            PRUnichar **dateTimeString);

  NS_IMETHOD FormatDate(const PRUnichar *locale, 
                        nsDateFormatSelector dateFormatSelector, 
                        int32_t year, 
                        int32_t month, 
                        int32_t day, 
                        PRUnichar **dateString)
                        {return FormatDateTime(locale, dateFormatSelector, kTimeFormatNone, 
                                               year, month, day, 0, 0, 0, dateString);}

  NS_IMETHOD FormatTime(const PRUnichar *locale, 
                        nsTimeFormatSelector timeFormatSelector, 
                        int32_t hour, 
                        int32_t minute, 
                        int32_t second, 
                        PRUnichar **timeString)
                        {return FormatDateTime(locale, kDateFormatNone, timeFormatSelector, 
                                               1999, 1, 1, hour, minute, second, timeString);}

  nsScriptableDateFormat() {}
  virtual ~nsScriptableDateFormat() {}
private:
  nsString mStringOut;   
};

NS_IMPL_ISUPPORTS1(nsScriptableDateFormat, nsIScriptableDateFormat)

NS_IMETHODIMP nsScriptableDateFormat::FormatDateTime(
                            const PRUnichar *aLocale, 
                            nsDateFormatSelector dateFormatSelector, 
                            nsTimeFormatSelector timeFormatSelector, 
                            int32_t year, 
                            int32_t month, 
                            int32_t day, 
                            int32_t hour, 
                            int32_t minute, 
                            int32_t second, 
                            PRUnichar **dateTimeString)
{
  // We can't have a valid date with the year, month or day
  // being lower than 1.
  if (year < 1 || month < 1 || day < 1)
    return NS_ERROR_INVALID_ARG;

  nsresult rv;
  nsAutoString localeName(aLocale);
  *dateTimeString = nullptr;

  nsCOMPtr<nsILocale> locale;
  // re-initialise locale pointer only if the locale was given explicitly
  if (!localeName.IsEmpty()) {
    // get locale service
    nsCOMPtr<nsILocaleService> localeService(do_GetService(kLocaleServiceCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    // get locale
    rv = localeService->NewLocale(localeName, getter_AddRefs(locale));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIDateTimeFormat> dateTimeFormat(do_CreateInstance(kDateTimeFormatCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  tm tmTime;
  time_t timetTime;

  memset(&tmTime, 0, sizeof(tmTime));
  tmTime.tm_year = year - 1900;
  tmTime.tm_mon = month - 1;
  tmTime.tm_mday = day;
  tmTime.tm_hour = hour;
  tmTime.tm_min = minute;
  tmTime.tm_sec = second;
  tmTime.tm_yday = tmTime.tm_wday = 0;
  tmTime.tm_isdst = -1;
  timetTime = mktime(&tmTime);

  if ((time_t)-1 != timetTime) {
    rv = dateTimeFormat->FormatTime(locale, dateFormatSelector, timeFormatSelector, 
                                     timetTime, mStringOut);
  }
  else {
    // if mktime fails (e.g. year <= 1970), then try NSPR.
    PRTime prtime;
    char string[32];
    sprintf(string, "%.2d/%.2d/%d %.2d:%.2d:%.2d", month, day, year, hour, minute, second);
    if (PR_SUCCESS != PR_ParseTimeString(string, false, &prtime))
      return NS_ERROR_INVALID_ARG;

    rv = dateTimeFormat->FormatPRTime(locale, dateFormatSelector, timeFormatSelector, 
                                      prtime, mStringOut);
  }
  if (NS_SUCCEEDED(rv))
    *dateTimeString = ToNewUnicode(mStringOut);

  return rv;
}

nsresult
NS_NewScriptableDateFormat(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsScriptableDateFormat* result = new nsScriptableDateFormat();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  nsresult rv = result->QueryInterface(aIID, aResult);
  NS_RELEASE(result);

  return rv;
}
