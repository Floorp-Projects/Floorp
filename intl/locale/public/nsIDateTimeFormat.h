
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
#ifndef nsIDateTimeFormat_h__
#define nsIDateTimeFormat_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include <time.h>


// {2BBAA0B0-A591-11d2-9119-006008A6EDF6}
#define NS_IDATETIMEFORMAT_IID \
{ 0x2bbaa0b0, 0xa591, 0x11d2, \
{ 0x91, 0x19, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

typedef enum {
    kDateFormatNone,                // do not include the date  in the format string
    kDateFormatLong,                // provides the long date format for the given locale
    kDateFormatShort,               // provides the short date format for the given locale
    kDateFormatYearMonth,           // formats using only the year and month 
    kDateFormatWeekday              // week day (e.g. Mon, Tue)
} nsDateFormatSelector;

typedef enum {
    kTimeFormatNone,                // don't include the time in the format string
    kTimeFormatSeconds,             // provides the time format with seconds in the  given locale 
    kTimeFormatNoSeconds,           // provides the time format without seconds in the given locale 
    kTimeFormatSecondsForce24Hour,  // forces the time format to use the 24 clock, regardless of the locale conventions
    kTimeFormatNoSecondsForce24Hour // forces the time format to use the 24 clock, regardless of the locale conventions
} nsTimeFormatSelector;


// Locale sensitive date and time format interface
// 
class nsIDateTimeFormat : public nsISupports {

public: 

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(const nsString& locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime,
                        nsString& stringOut) = 0; 

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(const nsString& locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const struct tm*  tmTime, 
                          nsString& stringOut) = 0; 
};

#endif  /* nsIDateTimeFormat_h__ */
