
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDateTimeFormatWin_h__
#define nsDateTimeFormatWin_h__


#include "nsIDateTimeFormat.h"
#include <windows.h>


// Locale sensitive date and time format interface
// 
class nsDateTimeFormatWin : public nsIDateTimeFormat {

public: 
  NS_DECL_THREADSAFE_ISUPPORTS 

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime, 
                        nsAString& stringOut); 

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const struct tm*  tmTime, 
                          nsAString& stringOut); 

  // performs a locale sensitive date formatting operation on the PRTime parameter
  NS_IMETHOD FormatPRTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const PRTime  prTime, 
                          nsAString& stringOut);

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  NS_IMETHOD FormatPRExplodedTime(nsILocale* locale, 
                                  const nsDateFormatSelector  dateFormatSelector, 
                                  const nsTimeFormatSelector timeFormatSelector, 
                                  const PRExplodedTime*  explodedTime, 
                                  nsAString& stringOut); 

  nsDateTimeFormatWin() {mLocale.SetLength(0);mAppLocale.SetLength(0);}


  virtual ~nsDateTimeFormatWin() {}

private:
  // init this interface to a specified locale
  NS_IMETHOD Initialize(nsILocale* locale);

  // call GetTimeFormatW or TimeFormatA
  int nsGetTimeFormatW(DWORD dwFlags, const SYSTEMTIME *lpTime,
                    const char* format, char16_t *timeStr, int cchTime);

  // call GetDateFormatW or GetDateFormatA
  int nsGetDateFormatW(DWORD dwFlags, const SYSTEMTIME *lpDate,
                       const char* format, char16_t *dateStr, int cchDate);

  nsString    mLocale;
  nsString    mAppLocale;
  uint32_t    mLCID;             // Windows platform locale ID
};

#endif  /* nsDateTimeFormatWin_h__ */
