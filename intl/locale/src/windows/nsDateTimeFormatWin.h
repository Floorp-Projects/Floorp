
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

  nsDateTimeFormatWin() {NS_INIT_REFCNT();};

private:
  // util function to call unicode converter
  nsresult ConvertToUnicode(const char *inString, const PRInt32 inLen, PRUnichar *outString, PRInt32 *outLen);

  // call GetTimeFormatW or TimeFormatA
  int nsGetTimeFormatW(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpTime,
                    const char* format, PRUnichar *timeStr, int cchTime);

  // call GetDateFormatW or GetDateFormatA
  int nsGetDateFormatW(LCID Locale, DWORD dwFlags, const SYSTEMTIME *lpDate,
                       const char* format, PRUnichar *dataStr, int cchDate);

  PRBool      mW_API;     // W or A API
  nsString    mCharset;   // for A version of API, we need to convert

};

#endif  /* nsDateTimeFormatWin_h__ */
