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

#include "nsDateTimeFormatWin.h"


#define NSDATETIMEFORMAT_BUFFER_LEN  80

NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

NS_IMPL_ISUPPORTS(nsDateTimeFormatWin, kIDateTimeFormatIID);

nsresult nsDateTimeFormatWin::FormatTime(const nsString& locale, 
                                      const nsDateFormatSelector  dateFormatSelector, 
                                      const nsTimeFormatSelector timeFormatSelector, 
                                      const time_t  timetTime, 
                                      PRUnichar *stringOut, 
                                      PRUint32 *outLen)
{
  return FormatTMTime(locale, dateFormatSelector, timeFormatSelector, localtime( &timetTime ), stringOut, outLen);
}

// performs a locale sensitive date formatting operation on the struct tm parameter
// locale RFC1766 (e.g. "en-US"), caller should allocate the buffer, outLen is in/out
nsresult nsDateTimeFormatWin::FormatTMTime(const nsString& locale, 
                                        const nsDateFormatSelector  dateFormatSelector, 
                                        const nsTimeFormatSelector timeFormatSelector, 
                                        const struct tm*  tmTime, 
                                        PRUnichar *stringOut, 
                                        PRUint32 *outLen)
{
	SYSTEMTIME system_time;
  DWORD dwFlags_Date = 0, dwFlags_Time = 0;
  int dateLen, timeLen;
  PRUnichar dateBuffer[NSDATETIMEFORMAT_BUFFER_LEN], timeBuffer[NSDATETIMEFORMAT_BUFFER_LEN];

  // Map tm to SYSTEMTIME
	system_time.wYear = 1900 + tmTime->tm_year;
	system_time.wMonth = tmTime->tm_mon + 1;
	system_time.wDayOfWeek = tmTime->tm_wday;
	system_time.wDay = tmTime->tm_mday;
	system_time.wHour = tmTime->tm_hour;
	system_time.wMinute = tmTime->tm_min;
	system_time.wSecond = tmTime->tm_sec;
	system_time.wMilliseconds = 0;

  // Map to WinAPI date format
  switch (dateFormatSelector) {
  case kDateFormatLong:
    dwFlags_Date = DATE_LONGDATE;
    break;
  case kDateFormatShort:
    dwFlags_Date = DATE_SHORTDATE;
    break;
  case kDateFormatWeekday:
    dwFlags_Date = 0;
    break;
  case kDateFormatYearMonth:
    dwFlags_Date = 0;     // TODO:only availabe NT5
    break;
  }

  // Map to WinAPI time format
  switch (timeFormatSelector) {
  case kTimeFormatSeconds:
    dwFlags_Time = 0;
    break;
  case kTimeFormatNoSeconds:
    dwFlags_Time = TIME_NOSECONDS;
    break;
  case kTimeFormatSecondsForce24Hour:
    dwFlags_Time = TIME_FORCE24HOURFORMAT;
    break;
  case kTimeFormatNoSecondsForce24Hour:
    dwFlags_Time = TIME_NOSECONDS + TIME_FORCE24HOURFORMAT;
    break;
  }

  // Call GetDateFormatW
  if (dateFormatSelector == kDateFormatNone) {
    dateLen = 0;
  }
  else {
    if (dateFormatSelector == kDateFormatYearMonth) {
      dateLen = GetDateFormatW(GetUserDefaultLCID(), 0, &system_time, L"yy/MM", 
                               dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    else if (dateFormatSelector == kDateFormatWeekday) {
      dateLen = GetDateFormatW(GetUserDefaultLCID(), 0, &system_time, L"ddd", 
                               dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    else {
      dateLen = GetDateFormatW(GetUserDefaultLCID(), dwFlags_Date, &system_time, NULL, 
                               dateBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    }
    if (dateLen != 0) {
      dateLen--;  // Since the count includes the terminating null.
    }
  }

  // Call GetTimeFormatW
  if (timeFormatSelector == kTimeFormatNone) {
    timeLen = 0;
  }
  else {
    timeLen = GetTimeFormatW(GetUserDefaultLCID(), dwFlags_Time, &system_time, NULL, 
                        timeBuffer, NSDATETIMEFORMAT_BUFFER_LEN);
    if (timeLen != 0) {
      timeLen--;  // Since the count includes the terminating null.
    }
  }

  NS_ASSERTION(NSDATETIMEFORMAT_BUFFER_LEN >= (PRUint32) (dateLen + 1), "internal date buffer is not large enough");
  NS_ASSERTION(NSDATETIMEFORMAT_BUFFER_LEN >= (PRUint32) (timeLen + 1), "internal time buffer is not large enough");
  NS_ASSERTION(*outLen >= (PRUint32) (dateLen + timeLen + 1 + 1), "input buffer is not large enough");

  // Copy the result
  stringOut[0] = 0;
  if (dateLen != 0 && timeLen != 0) {
    (void) wcscpy((wchar_t *) stringOut, (const wchar_t *) dateBuffer);
    (void) wcscat((wchar_t *) stringOut, (const wchar_t *) L" ");
    (void) wcscat((wchar_t *) stringOut, (const wchar_t *) timeBuffer);
    *outLen = dateLen + timeLen + 1;
  }
  else if (timeLen == 0) {
    (void) wcscpy((wchar_t *) stringOut, (const wchar_t *) dateBuffer);
    *outLen = dateLen;
  }
  else if (dateLen == 0) {
    (void) wcscpy((wchar_t *) stringOut, (const wchar_t *) timeBuffer);
    *outLen = timeLen;
  }
  else {
    *outLen = 0;
  }

  return NS_OK;
}
