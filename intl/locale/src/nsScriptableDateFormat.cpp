/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleFactory.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsIScriptableDateFormat.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

class nsScriptableDateFormat : public nsIScriptableDateFormat {
 public: 
  NS_DECL_ISUPPORTS 

  NS_IMETHOD FormatDateTime(const PRUnichar *locale, 
                            nsDateFormatSelector dateFormatSelector, 
                            nsTimeFormatSelector timeFormatSelector, 
                            PRInt32 year, 
                            PRInt32 month, 
                            PRInt32 day, 
                            PRInt32 hour, 
                            PRInt32 minute, 
                            PRInt32 second, 
                            PRUnichar **dateTimeString);

  NS_IMETHOD FormatDate(const PRUnichar *locale, 
                        nsDateFormatSelector dateFormatSelector, 
                        PRInt32 year, 
                        PRInt32 month, 
                        PRInt32 day, 
                        PRUnichar **dateString)
                        {return FormatDateTime(locale, dateFormatSelector, kTimeFormatNone, 
                                               year, month, day, 0, 0, 0, dateString);}

  NS_IMETHOD FormatTime(const PRUnichar *locale, 
                        nsTimeFormatSelector timeFormatSelector, 
                        PRInt32 hour, 
                        PRInt32 minute, 
                        PRInt32 second, 
                        PRUnichar **timeString)
                        {return FormatDateTime(locale, kDateFormatNone, timeFormatSelector, 
                                               1999, 1, 1, hour, minute, second, timeString);}

  nsScriptableDateFormat() {NS_INIT_REFCNT();}
  virtual ~nsScriptableDateFormat() {}
private:
  nsString mStringOut;   
};

NS_IMPL_ISUPPORTS(nsScriptableDateFormat, nsIScriptableDateFormat::GetIID());

NS_IMETHODIMP nsScriptableDateFormat::FormatDateTime(
                            const PRUnichar *locale, 
                            nsDateFormatSelector dateFormatSelector, 
                            nsTimeFormatSelector timeFormatSelector, 
                            PRInt32 year, 
                            PRInt32 month, 
                            PRInt32 day, 
                            PRInt32 hour, 
                            PRInt32 minute, 
                            PRInt32 second, 
                            PRUnichar **dateTimeString)
{
	nsILocaleFactory* localeFactory; 
	nsILocale* aLocale; 
	nsString localeName(locale);
  nsresult rv;

  *dateTimeString = NULL;

  // get a locale factory 
	rv = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory); 
  if (NS_SUCCEEDED(rv) && localeFactory) {
    rv = localeName.Length() ? localeFactory->NewLocale(&localeName, &aLocale) :
                               localeFactory->GetApplicationLocale(&aLocale);
    if (NS_SUCCEEDED(rv) && aLocale) {
      nsIDateTimeFormat *aDateTimeFormat;
    	rv = nsComponentManager::CreateInstance(kDateTimeFormatCID, NULL,
                                              nsIDateTimeFormat::GetIID(), (void **) &aDateTimeFormat);
      if (NS_SUCCEEDED(rv) && aDateTimeFormat) {
        struct tm tmTime;
        time_t  timetTime;

        nsCRT::memset( &tmTime, 0, sizeof(tmTime) );
        tmTime.tm_year = year - 1900;
        tmTime.tm_mon = month - 1;
        tmTime.tm_mday = day;
        tmTime.tm_hour = hour;
        tmTime.tm_min = minute;
        tmTime.tm_sec = second;
        tmTime.tm_yday = tmTime.tm_wday = 0;
        tmTime.tm_isdst = -1;
        timetTime = mktime(&tmTime);
        if (-1 != timetTime) {
          rv = aDateTimeFormat->FormatTime(aLocale, dateFormatSelector, timeFormatSelector, 
                                           timetTime, mStringOut);
          if (NS_SUCCEEDED(rv)) {
            *dateTimeString = mStringOut.ToNewUnicode();
          }
        }
        else {
          // if mktime fails (e.g. year <= 1970), then try NSPR.
          PRTime prtime;
          char string[32];
          sprintf(string, "%.2d/%.2d/%d %.2d:%.2d:%.2d", month, day, year, hour, minute, second);
          if (PR_SUCCESS != PR_ParseTimeString(string, PR_FALSE, &prtime)) {
            rv = NS_ERROR_ILLEGAL_VALUE; // invalid arg value
          }
          else {
            rv = aDateTimeFormat->FormatPRTime(aLocale, dateFormatSelector, timeFormatSelector, 
                                               prtime, mStringOut);
            if (NS_SUCCEEDED(rv)) {
              *dateTimeString = mStringOut.ToNewUnicode();
            }
          }
        }
        NS_RELEASE(aDateTimeFormat);
      }
      NS_RELEASE(aLocale);
    }
    NS_RELEASE(localeFactory);
  }

  return rv;
}

nsISupports *NEW_SCRIPTABLE_DATEFORMAT(void)
{
  return (nsISupports *) new nsScriptableDateFormat;
}

