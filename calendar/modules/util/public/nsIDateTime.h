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

#ifndef nsIDateTime_h___
#define nsIDateTime_h___

#include "nsISupports.h"
#include "nsString.h"

//32385f60-1cca-11d2-9246-00805f8a7ab6
#define NS_IDATETIME_IID   \
{ 0x32385f60, 0x1cca, 0x11d2,    \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIDateTime : public nsISupports
{

public:

  NS_IMETHOD                  Init() = 0 ;

  NS_IMETHOD_(PRUint32) GetYear() = 0;
  NS_IMETHOD_(PRUint32) GetMonth() = 0;
  NS_IMETHOD_(PRUint32) GetDay() = 0;
  NS_IMETHOD_(PRUint32) GetHour() = 0;
  NS_IMETHOD_(PRUint32) GetMinute() = 0;
  NS_IMETHOD_(PRUint32) GetSecond() = 0;

  NS_IMETHOD SetYear(PRUint32 aYear) = 0;
  NS_IMETHOD SetMonth(PRUint32 aMonth) = 0;
  NS_IMETHOD SetDay(PRUint32 aDay) = 0;
  NS_IMETHOD SetHour(PRUint32 aHour) = 0;
  NS_IMETHOD SetMinute(PRUint32 aMinute) = 0;
  NS_IMETHOD SetSecond(PRUint32 aSecond) = 0;

  NS_IMETHOD IncrementYear(PRUint32 aYear) = 0;
  NS_IMETHOD IncrementMonth(PRUint32 aMonth) = 0;
  NS_IMETHOD IncrementDay(PRUint32 aDay) = 0;
  NS_IMETHOD IncrementHour(PRUint32 aHour) = 0;
  NS_IMETHOD IncrementMinute(PRUint32 aMinute) = 0;
  NS_IMETHOD IncrementSecond(PRUint32 aSecond) = 0;

  NS_IMETHOD DecrementYear(PRUint32 aYear) = 0;
  NS_IMETHOD DecrementMonth(PRUint32 aMonth) = 0;
  NS_IMETHOD DecrementDay(PRUint32 aDay) = 0;
  NS_IMETHOD DecrementHour(PRUint32 aHour) = 0;
  NS_IMETHOD DecrementMinute(PRUint32 aMinute) = 0;
  NS_IMETHOD DecrementSecond(PRUint32 aSecond) = 0;

  NS_IMETHOD strftime(nsString& aPattern, nsString ** aString) = 0;

  NS_IMETHOD_(nsIDateTime *) Copy() = 0;
};

#endif /* nsIDateTime_h___ */
