/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <locale.h>
#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsDateTimeFormatUnix.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsIPosixLocale.h"

static NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
static NS_DEFINE_IID(kPosixLocaleFactoryCID, NS_POSIXLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);

NS_IMPL_ISUPPORTS(nsDateTimeFormatUnix, kIDateTimeFormatIID);

nsresult nsDateTimeFormatUnix::FormatTime(nsILocale* locale, 
                                      const nsDateFormatSelector  dateFormatSelector, 
                                      const nsTimeFormatSelector timeFormatSelector, 
                                      const time_t  timetTime, 
                                      nsString& stringOut) 
{
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, localtime(&timetTime), stringOut);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
nsresult nsDateTimeFormatUnix::FormatTMTime(nsILocale* locale, 
                                        const nsDateFormatSelector  dateFormatSelector, 
                                        const nsTimeFormatSelector timeFormatSelector, 
                                        const struct tm*  tmTime, 
                                        nsString& stringOut) 
{
#define NSDATETIME_FORMAT_BUFFER_LEN  80
#define kPlatformLocaleLength 64
  char strOut[NSDATETIME_FORMAT_BUFFER_LEN];
  char fmtD[32], fmtT[32];
  char platformLocale[kPlatformLocaleLength+1];
  nsString aCharset("ISO-8859-1");	//TODO: need to get this from locale
  nsresult res;
  
  PL_strcpy(platformLocale, "en_US");
  if (locale != nsnull) {
    nsString aLocale;
    nsString aCategory("NSILOCALE_TIME");

    res = locale->GetCatagory(&aCategory, &aLocale);
    if (NS_FAILED(res)) {
      return res;
    }

    nsIPosixLocale* posixLocale;
    res = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID, NULL, kIPosixLocaleIID, (void**)&posixLocale);
    if (NS_FAILED(res)) {
      return res;
    }
    res = posixLocale->GetPlatformLocale(&aCategory, platformLocale, kPlatformLocaleLength+1);

    posixLocale->Release();
  }

  // set date format
  switch (dateFormatSelector) {
    case kDateFormatNone:
      PL_strcpy(fmtD, "");
      break; 
    case kDateFormatLong:
      PL_strcpy(fmtD, "%c");
      break; 
    case kDateFormatShort:
      PL_strcpy(fmtD, "%x");
      break; 
    case kDateFormatYearMonth:
      PL_strcpy(fmtD, "%y/%m");
      break; 
    case kDateFormatWeekday:
      PL_strcpy(fmtD, "%a");
      break;
    default:
      PL_strcpy(fmtD, ""); 
  }

  // set time format
  switch (timeFormatSelector) {
    case kTimeFormatNone:
      PL_strcpy(fmtT, ""); 
      break;
    case kTimeFormatSeconds:
      PL_strcpy(fmtT, "%I:%M:%S %p");
      break;
    case kTimeFormatNoSeconds:
      PL_strcpy(fmtT, "%I:%M %p");
      break;
    case kTimeFormatSecondsForce24Hour:
      PL_strcpy(fmtT, "%H:%M:%S");
      break;
    case kTimeFormatNoSecondsForce24Hour:
      PL_strcpy(fmtT, "%H:%M");
      break;
    default:
      PL_strcpy(fmtT, ""); 
  }

  // generate data/time string
  char *old_locale = setlocale(LC_TIME, NULL);
  (void) setlocale(LC_TIME, platformLocale);
  if (strlen(fmtD) && strlen(fmtT)) {
    PL_strcat(fmtD, " ");
    PL_strcat(fmtD, fmtT);
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (strlen(fmtD) && !strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (!strlen(fmtD) && strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtT, tmTime);
  }
  else {
    PL_strcpy(strOut, "");
  }
  (void) setlocale(LC_TIME, old_locale);

  // convert result to unicode
  nsICharsetConverterManager * ccm = nsnull;

  res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                   kICharsetConverterManagerIID, 
                                   (nsISupports**)&ccm);
  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeDecoder * decoder = nsnull;
    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) {
      PRInt32 unicharLength = 0;
      PRInt32 srcLength = (PRInt32) strlen(strOut);
      res = decoder->Length(strOut, 0, srcLength, &unicharLength);
      PRUnichar *unichars = new PRUnichar [ unicharLength ];
  
      if (nsnull != unichars) {
        res = decoder->Convert(unichars, 0, &unicharLength,
                               strOut, 0, &srcLength);
        if (NS_SUCCEEDED(res)) {
          stringOut.SetString(unichars, unicharLength);
        }
      }
      delete [] unichars;
      NS_IF_RELEASE(decoder);
    }    
    nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
  }
  
  return res;
}

