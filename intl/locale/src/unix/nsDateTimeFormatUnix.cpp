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

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsDateTimeFormatUnix.h"

NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

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
  char strOut[NSDATETIME_FORMAT_BUFFER_LEN];
  char fmtD[32], fmtT[32];
  
  // set date format
  switch (dateFormatSelector) {
    case kDateFormatNone:
      strcpy(fmtD, "");
      break; 
    case kDateFormatLong:
      strcpy(fmtD, "%c");
      break; 
    case kDateFormatShort:
      strcpy(fmtD, "%x");
      break; 
    case kDateFormatYearMonth:
      strcpy(fmtD, "%y/%m");
      break; 
    case kDateFormatWeekday:
      strcpy(fmtD, "%a");
      break;
    default:
      strcpy(fmtD, ""); 
  }

  // set time format
  switch (timeFormatSelector) {
    case kTimeFormatNone:
      strcpy(fmtT, ""); 
      break;
    case kTimeFormatSeconds:
      strcpy(fmtT, "%I:%M:%S %p");
      break;
    case kTimeFormatNoSeconds:
      strcpy(fmtT, "%I:%M %p");
      break;
    case kTimeFormatSecondsForce24Hour:
      strcpy(fmtT, "%H:%M:%S");
      break;
    case kTimeFormatNoSecondsForce24Hour:
      strcpy(fmtT, "%H:%M");
      break;
    default:
      strcpy(fmtT, ""); 
  }

  // generate data/time string
  if (strlen(fmtD) && strlen(fmtT)) {
    strcat(fmtD, " ");
    strcat(fmtD, fmtT);
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (strlen(fmtD) && !strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtD, tmTime);
  }
  else if (!strlen(fmtD) && strlen(fmtT)) {
    strftime(strOut, NSDATETIME_FORMAT_BUFFER_LEN, fmtT, tmTime);
  }
  else {
    strcpy(strOut, "");
  }
//  stringOut.SetString(strOut);

  // convert result to unicode
  nsresult res;
  nsString aCharset("ISO-8859-1");	//TODO: need to get this from locale
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

