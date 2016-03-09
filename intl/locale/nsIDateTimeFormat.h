
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDateTimeFormat_h__
#define nsIDateTimeFormat_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsStringGlue.h"
#include "nsILocale.h"
#include "nsIScriptableDateFormat.h"
#include "prtime.h"
#include <time.h>


// {2BBAA0B0-A591-11d2-9119-006008A6EDF6}
#define NS_IDATETIMEFORMAT_IID \
{ 0x2bbaa0b0, 0xa591, 0x11d2, \
{ 0x91, 0x19, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }


// Locale sensitive date and time format interface
// 
class nsIDateTimeFormat : public nsISupports {
protected:
  nsIDateTimeFormat() {}
  virtual ~nsIDateTimeFormat() {}

public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDATETIMEFORMAT_IID)

  static already_AddRefed<nsIDateTimeFormat> Create();

  // performs a locale sensitive date formatting operation on the time_t parameter
  NS_IMETHOD FormatTime(nsILocale* locale, 
                        const nsDateFormatSelector  dateFormatSelector, 
                        const nsTimeFormatSelector timeFormatSelector, 
                        const time_t  timetTime,
                        nsAString& stringOut) = 0; 

  // performs a locale sensitive date formatting operation on the struct tm parameter
  NS_IMETHOD FormatTMTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const struct tm*  tmTime, 
                          nsAString& stringOut) = 0; 

  // performs a locale sensitive date formatting operation on the PRTime parameter
  NS_IMETHOD FormatPRTime(nsILocale* locale, 
                          const nsDateFormatSelector  dateFormatSelector, 
                          const nsTimeFormatSelector timeFormatSelector, 
                          const PRTime  prTime, 
                          nsAString& stringOut) = 0;

  // performs a locale sensitive date formatting operation on the PRExplodedTime parameter
  NS_IMETHOD FormatPRExplodedTime(nsILocale* locale, 
                                  const nsDateFormatSelector  dateFormatSelector, 
                                  const nsTimeFormatSelector timeFormatSelector, 
                                  const PRExplodedTime*  explodedTime, 
                                  nsAString& stringOut) = 0; 
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDateTimeFormat, NS_IDATETIMEFORMAT_IID)

#endif  /* nsIDateTimeFormat_h__ */
