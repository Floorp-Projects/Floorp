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

#include "nscore.h"
#include "nsCalUtilCIID.h"
#include "nsString.h"
#include "nsDateTime.h"
#include "unistring.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCDateTimeCID, NS_DATETIME_CID);

nsDateTime :: nsDateTime()
{
  NS_INIT_REFCNT();

  mUnicodeString = nsnull;
  mDateTime = nsnull;
}

nsDateTime :: ~nsDateTime()
{
  if (mDateTime) {
    delete mDateTime;
    mDateTime = nsnull;
  }
  if (mUnicodeString) {
    delete mUnicodeString;
    mUnicodeString = nsnull;
  }
}

NS_IMPL_QUERY_INTERFACE(nsDateTime, kCDateTimeCID)
NS_IMPL_ADDREF(nsDateTime)
NS_IMPL_RELEASE(nsDateTime)


nsresult nsDateTime :: Init()
{
  if (nsnull == mDateTime)    
    mDateTime = new DateTime();

  if (nsnull == mUnicodeString)    
    mUnicodeString = new UnicodeString();

  return NS_OK ;
}

PRUint32 nsDateTime :: GetYear()
{
  return (mDateTime->getYear());
}

PRUint32 nsDateTime :: GetMonth()
{
  return (mDateTime->getMonth());
}

PRUint32 nsDateTime :: GetDay()
{
  PRUint32 day = mDateTime->getDate();
  return (day);
}

PRUint32 nsDateTime :: GetHour()
{
  return (mDateTime->getHour());
}

PRUint32 nsDateTime :: GetMinute()
{
  return (mDateTime->getMinute());
}

PRUint32 nsDateTime :: GetSecond()
{
  return (mDateTime->getSecond());
}

nsresult nsDateTime :: SetYear(PRUint32 aYear)
{
  mDateTime->set(Calendar::YEAR,aYear);
  return NS_OK;
}

nsresult nsDateTime :: SetMonth(PRUint32 aMonth)
{
  mDateTime->set(Calendar::MONTH,aMonth);
  return NS_OK;
}

nsresult nsDateTime :: SetDay(PRUint32 aDay)
{
  mDateTime->set(Calendar::DATE,aDay);
  return NS_OK;
}

nsresult nsDateTime :: SetHour(PRUint32 aHour)
{
  mDateTime->set(Calendar::HOUR_OF_DAY,aHour);
  return NS_OK;
}

nsresult nsDateTime :: SetMinute(PRUint32 aMinute)
{
  mDateTime->set(Calendar::MINUTE,aMinute);
  return NS_OK;
}

nsresult nsDateTime :: SetSecond(PRUint32 aSecond)
{
  mDateTime->set(Calendar::SECOND,aSecond);
  return NS_OK;
}

DateTime * nsDateTime :: GetDateTime()
{
  return (mDateTime);
}

nsresult nsDateTime :: SetDateTime(DateTime * aDateTime)
{
  if (mDateTime) {
    delete mDateTime;
  }
  mDateTime = aDateTime;
  return NS_OK;
}

nsresult nsDateTime :: strftime(nsString& aPattern, 
                                nsString ** aString)
{
  char szTemp[1024];
  char * string;

  UnicodeString pattern(aPattern.ToCString(szTemp,aPattern.Length()));

  UnicodeString unistring = mDateTime->strftime(pattern);

  string = unistring.toCString(nsnull);

  nsString * nsstring = new nsString(string);

  *aString = (nsString *)nsstring;

  return NS_OK;
}

nsresult nsDateTime :: IncrementYear(PRUint32 aYear)
{
  mDateTime->add(Calendar::YEAR,aYear);
  return NS_OK;
}

nsresult nsDateTime :: IncrementMonth(PRUint32 aMonth)
{
  mDateTime->add(Calendar::MONTH,aMonth);
  return NS_OK;
}

nsresult nsDateTime :: IncrementDay(PRUint32 aDay)
{
  mDateTime->add(Calendar::DATE,aDay);
  return NS_OK;
}

nsresult nsDateTime :: IncrementHour(PRUint32 aHour)
{
  mDateTime->add(Calendar::HOUR_OF_DAY,aHour);
  return NS_OK;
}

nsresult nsDateTime :: IncrementMinute(PRUint32 aMinute)
{
  mDateTime->add(Calendar::MINUTE,aMinute);
  return NS_OK;
}

nsresult nsDateTime :: IncrementSecond(PRUint32 aSecond)
{
  mDateTime->add(Calendar::SECOND,aSecond);
  return NS_OK;
}

nsresult nsDateTime :: DecrementYear(PRUint32 aYear)
{
  mDateTime->add(Calendar::YEAR,aYear * -1);
  return NS_OK;
}

nsresult nsDateTime :: DecrementMonth(PRUint32 aMonth)
{
  mDateTime->add(Calendar::MONTH,aMonth* -1);
  return NS_OK;
}

nsresult nsDateTime :: DecrementDay(PRUint32 aDay)
{
  mDateTime->add(Calendar::DATE,aDay* -1);
  return NS_OK;
}

nsresult nsDateTime :: DecrementHour(PRUint32 aHour)
{
  mDateTime->add(Calendar::HOUR_OF_DAY,aHour* -1);
  return NS_OK;
}

nsresult nsDateTime :: DecrementMinute(PRUint32 aMinute)
{
  mDateTime->add(Calendar::MINUTE,aMinute* -1);
  return NS_OK;
}

nsresult nsDateTime :: DecrementSecond(PRUint32 aSecond)
{
  mDateTime->add(Calendar::SECOND,aSecond* -1);
  return NS_OK;
}

nsIDateTime * nsDateTime :: Copy()
{
  nsDateTime * datetime = nsnull;

  static NS_DEFINE_IID(kCalDateTimeCID, NS_DATETIME_CID);
  static NS_DEFINE_IID(kCalDateTimeIID, NS_IDATETIME_IID);

  nsresult res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                              nsnull, 
                                              kCalDateTimeCID, 
                                              (void **)&datetime);

  if (NS_OK != res)
    return nsnull;
     
  datetime->mDateTime = new DateTime(*mDateTime);
  datetime->mUnicodeString = new UnicodeString(*mUnicodeString);

  return datetime ;
}
