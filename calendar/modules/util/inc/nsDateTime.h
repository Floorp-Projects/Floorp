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

#ifndef nsDateTime_h___
#define nsDateTime_h___

#include "nsIDateTime.h"
#include "jdefines.h"
#include "datetime.h"

class nsDateTime : public nsIDateTime
{
public:
  nsDateTime();

  NS_DECL_ISUPPORTS

  NS_IMETHOD            Init() ;

  NS_IMETHOD_(PRUint32) GetYear();
  NS_IMETHOD_(PRUint32) GetMonth();
  NS_IMETHOD_(PRUint32) GetDay();
  NS_IMETHOD_(PRUint32) GetHour();
  NS_IMETHOD_(PRUint32) GetMinute();
  NS_IMETHOD_(PRUint32) GetSecond();

  NS_IMETHOD SetYear(PRUint32 aYear);
  NS_IMETHOD SetMonth(PRUint32 aMonth);
  NS_IMETHOD SetDay(PRUint32 aDay);
  NS_IMETHOD SetHour(PRUint32 aHour);
  NS_IMETHOD SetMinute(PRUint32 aMinute);
  NS_IMETHOD SetSecond(PRUint32 aSecond);

  NS_IMETHOD IncrementYear(PRUint32 aYear);
  NS_IMETHOD IncrementMonth(PRUint32 aMonth);
  NS_IMETHOD IncrementDay(PRUint32 aDay);
  NS_IMETHOD IncrementHour(PRUint32 aHour);
  NS_IMETHOD IncrementMinute(PRUint32 aMinute);
  NS_IMETHOD IncrementSecond(PRUint32 aSecond);

  NS_IMETHOD DecrementYear(PRUint32 aYear);
  NS_IMETHOD DecrementMonth(PRUint32 aMonth);
  NS_IMETHOD DecrementDay(PRUint32 aDay);
  NS_IMETHOD DecrementHour(PRUint32 aHour);
  NS_IMETHOD DecrementMinute(PRUint32 aMinute);
  NS_IMETHOD DecrementSecond(PRUint32 aSecond);

  NS_IMETHOD SetDateTime(DateTime * aDateTime);
  NS_IMETHOD_(DateTime *) GetDateTime();

  NS_IMETHOD strftime(nsString& aPattern, nsString ** aString);

  NS_IMETHOD_(nsIDateTime *) Copy() ;

protected:
  ~nsDateTime();

private:
  DateTime * mDateTime;
  UnicodeString * mUnicodeString;

};

#endif /* nsDateTime_h___ */
