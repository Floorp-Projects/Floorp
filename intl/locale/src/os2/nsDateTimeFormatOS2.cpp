/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): Henry Sobotka <sobotka@axess.com> 01/2000 review and update
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 07/05/2000       IBM Corp.       Reworked file.
 */


#include "unidef.h"
#include "time.h"
#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsDateTimeFormatOS2.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIOS2Locale.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
static NS_DEFINE_CID(kOS2LocaleFactoryCID, NS_OS2LOCALEFACTORY_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);

NS_IMPL_THREADSAFE_ISUPPORTS(nsDateTimeFormatOS2,kIDateTimeFormatIID)


nsresult nsDateTimeFormatOS2::FormatTime(nsILocale* locale, 
                               const nsDateFormatSelector  dateFormatSelector, 
                               const nsTimeFormatSelector  timeFormatSelector, 
                               const time_t                timetTime,
                               nsString                   &stringOut)
{
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, localtime( &timetTime ), stringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatOS2::FormatTMTime(nsILocale* locale, 
                               const nsDateFormatSelector  dateFormatSelector, 
                               const nsTimeFormatSelector  timeFormatSelector, 
                               const struct tm*            tmTime, 
                               nsString                   &stringOut)
{
#define NSDATETIME_FORMAT_BUFFER_LEN  80

  nsresult rc = NS_ERROR_FAILURE;
  UniChar uFmtD[NSDATETIME_FORMAT_BUFFER_LEN] = { 0 };
  UniChar uFmtT[NSDATETIME_FORMAT_BUFFER_LEN] = { 0 };
  UniChar *pString = NULL;
  LocaleObject locObj = NULL;
  int res = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"", &locObj);
  if (res != ULS_SUCCESS)
    return rc;  

  // set date format
  switch (dateFormatSelector) {
    case kDateFormatNone:
      UniStrcat( uFmtD, (UniChar*)L"");
      break; 
    case kDateFormatLong:
      UniStrcat( uFmtD, (UniChar*)L"%c");
      break; 
    case kDateFormatShort:
      UniStrcat( uFmtD, (UniChar*)L"%x");
      break; 
    case kDateFormatYearMonth:
      UniQueryLocaleItem( locObj, DATESEP, &pString);
      UniStrcat( uFmtD, (UniChar*)L"%y");
      UniStrcat( uFmtD, pString);
      UniStrcat( uFmtD, (UniChar*)L"%m");
      UniFreeMem(pString);
      break; 
    case kDateFormatWeekday:
      UniStrcat( uFmtD, (UniChar*)L"%a");
      break;
    default: 
      UniStrcat( uFmtD, (UniChar*)L"");
  }

  // set time format
  switch (timeFormatSelector) {
    case kTimeFormatNone: 
      UniStrcat( uFmtT, (UniChar*)L"");
      break;
   case kTimeFormatSeconds:
      UniStrcat( uFmtT, (UniChar*)L"%r");
      break;
    case kTimeFormatNoSeconds:
      UniStrcat( uFmtT, (UniChar*)L"%R");
      break;
    case kTimeFormatSecondsForce24Hour:
      UniStrcat( uFmtT, (UniChar*)L"%T");
      break;
    case kTimeFormatNoSecondsForce24Hour:
      UniStrcat( uFmtT, (UniChar*)L"%R");
      break;  
    default: 
      UniStrcat( uFmtT, (UniChar*)L"");
  }

 
  PRUnichar buffer[NSDATETIME_FORMAT_BUFFER_LEN] = {0};         
  UniStrcat( uFmtD, (UniChar*)L" ");
  UniStrcat( uFmtD, uFmtT);
  int length = UniStrftime(locObj, buffer, NSDATETIME_FORMAT_BUFFER_LEN, uFmtD, tmTime);
  UniFreeMem(locObj);

  if ( length != 0) {
    stringOut.Assign(buffer, length);
    rc = NS_OK;
  }
  
  return rc;
}

// performs a locale sensitive date formatting operation on the PRTime parameter
nsresult nsDateTimeFormatOS2::FormatPRTime(nsILocale* locale, 
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
nsresult nsDateTimeFormatOS2::FormatPRExplodedTime(nsILocale* locale, 
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
  nsCRT::memset( &tmTime, 0, sizeof(tmTime) );

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

