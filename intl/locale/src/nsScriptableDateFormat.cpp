/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsDateTimeFormatCID.h"
#include "nsIDateTimeFormat.h"
#include "nsIScriptableDateFormat.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
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
                            PRInt32 year, 
                            PRInt32 month, 
                            PRInt32 day, 
                            PRInt32 hour, 
                            PRInt32 minute, 
                            PRInt32 second, 
                            PRUnichar **dateTimeString)
{
  nsAutoString localeName(aLocale);
  *dateTimeString = nsnull;

  // get locale service 
  nsCOMPtr<nsILocaleService> localeService = do_GetService(kLocaleServiceCID);
  if (!localeService)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsILocale> locale;
  if (localeName.IsEmpty())
    localeService->GetApplicationLocale(getter_AddRefs(locale));
  else
    localeService->NewLocale(localeName, getter_AddRefs(locale));

  if (!locale)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIDateTimeFormat> dateTimeFormat = do_CreateInstance(kDateTimeFormatCID);
  if (!dateTimeFormat)
    return NS_ERROR_NOT_AVAILABLE;
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

  nsresult rv;

  if ((time_t)-1 != timetTime) {
    rv = dateTimeFormat->FormatTime(locale, dateFormatSelector, timeFormatSelector, 
                                     timetTime, mStringOut);
  }
  else {
    // if mktime fails (e.g. year <= 1970), then try NSPR.
    PRTime prtime;
    char string[32];
    sprintf(string, "%.2d/%.2d/%d %.2d:%.2d:%.2d", month, day, year, hour, minute, second);
    if (PR_SUCCESS != PR_ParseTimeString(string, PR_FALSE, &prtime))
      return NS_ERROR_ILLEGAL_VALUE; // invalid arg value

    rv = dateTimeFormat->FormatPRTime(locale, dateFormatSelector, timeFormatSelector, 
                                      prtime, mStringOut);
  }
  if (NS_SUCCEEDED(rv))
    *dateTimeString = ToNewUnicode(mStringOut);

  return rv;
}

NS_IMETHODIMP
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
