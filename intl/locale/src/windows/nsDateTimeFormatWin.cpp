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

NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

NS_IMPL_ISUPPORTS(nsDateTimeFormatWin, kIDateTimeFormatIID);

nsresult nsDateTimeFormatWin::FormatTime(const nsString& locale, 
                                      const nsDateFormatSelector  dateFormatSelector, 
                                      const nsTimeFormatSelector timeFormatSelector, 
                                      const time_t  timetTime, 
                                      PRUnichar *stringOut, 
                                      PRUint32 *outLen)
{
  // Temporary implementation, real implementation to be done by FE.
  //

  struct tm *today;
  char *str;

  today = localtime( &timetTime );
  str = asctime(today);

  nsString aString(str);
  *outLen = aString.Length();
  memcpy((void *) stringOut, (void *) aString.GetUnicode(), aString.Length() * sizeof(PRUnichar));

  return NS_OK;
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
  // Temporary implementation, real implementation to be done by FE.
  //

  char *str = asctime(tmTime);

  nsString aString(str);
  *outLen = aString.Length();
  memcpy((void *) stringOut, (void *) aString.GetUnicode(), aString.Length() * sizeof(PRUnichar));

  return NS_OK;
}
