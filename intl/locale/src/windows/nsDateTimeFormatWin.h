
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsDateTimeFormatWin_h__
#define nsDateTimeFormatWin_h__


#include "nsIDateTimeFormat.h"
#include <windows.h>


// Locale sensitive date and time format interface
// 
class nsDateTimeFormatWin : public nsIDateTimeFormat {

public: 
  NS_DECL_ISUPPORTS 

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime, 
                        nsString& stringOut); 

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const struct tm*  tmTime, 
                          nsString& stringOut); 

  // performs a locale sensitive date formatting operation on the PRTime parameter
  NS_IMETHOD FormatPRTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const PRTime  prTime, 
                          nsString& stringOut);

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  NS_IMETHOD FormatPRExplodedTime(nsILocale* locale, 
                                  const nsDateFormatSelector  dateFormatSelector, 
                                  const nsTimeFormatSelector timeFormatSelector, 
                                  const PRExplodedTime*  explodedTime, 
                                  nsString& stringOut); 

  nsDateTimeFormatWin() {mLocale.SetLength(0);mAppLocale.SetLength(0);}


  virtual ~nsDateTimeFormatWin() {}

private:
  // init this interface to a specified locale
  NS_IMETHOD Initialize(nsILocale* locale);

  // call GetTimeFormatW or TimeFormatA
  int nsGetTimeFormatW(DWORD dwFlags, const SYSTEMTIME *lpTime,
                    const char* format, PRUnichar *timeStr, int cchTime);

  // call GetDateFormatW or GetDateFormatA
  int nsGetDateFormatW(DWORD dwFlags, const SYSTEMTIME *lpDate,
                       const char* format, PRUnichar *dateStr, int cchDate);

  nsString    mLocale;
  nsString    mAppLocale;
  PRBool      mW_API;            // W or A API
  PRUint32    mLCID;             // Windows platform locale ID
};

#endif  /* nsDateTimeFormatWin_h__ */
