/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/**
 *  nsCalScheduler
 *     Wraps all the capi and parsing code. A more convenient
 *     interface for performing scheduling tasks.
 *
 *  sman
 */


#include "nsCalScheduler.h"

nsCalScheduler::nsCalScheduler()
{

}

nsCalScheduler::~nsCalScheduler()
{

}

/**
 * Fetch events from the supplied curl that overlap the
 * supplied date range.
 * @param sCurl       the curl describing the data source
 * @param u           the user who is asking for the data
 * @param psPassword  the password needed to login to this url
 * @param dStart      range start time
 * @param dEnd        range end time
 * @param ppsPropList array of property names to load
 * @param iPropCount  number of items in ppsPropList
 * @param pCal        the calendar into which these events should be loaded
 * @return 0 on success
 */
nsresult FetchEventsByRange( 
  const JulianString& sCurl,
  const nsCalUser& u,
  const char* psPassword,
  DateTime dStart,
  DateTime dEnd,
  char** ppsPropList,
  int iPropCount,
  nsCalendar* pCal)
{
  User * pFromUser;
  User * pToUser;
  nsCurlParser Curl(sCurl);

  JulianPtrArray CompList;
  JulianPtrArray RecipList;
  JulianPtrArray ModifierList;
  
  UnicodeString name = "John Sun";
  UnicodeString email = "jsun@netscape.com";
  UnicodeString capaddr = "jsunCAP";
  t_int32 xitemid = 32432;
  UnicodeString iripaddr = "jsunIRIP";
  UnicodeString subject = "a subject";
  UnicodeString password = "z8j20bwm";
  UnicodeString hostname = "calendar-1";
  UnicodeString node = "10000";
  pFromUser = new User(name, email, capaddr, xitemid, iripaddr);

  // TODO: set pFromUser's CS&T info for now
  pFromUser->setCAPIInfo("", password, hostname, node);

  assert(pFromUser != 0);

  pToUser = new User(name, email, capaddr, xitemid, iripaddr);
  assert(pToUser != 0);

  recipients->Add(pToUser);

  ModifierList->Add(new UnicodeString("19980220T112233Z"));
  ModifierList->Add(new UnicodeString("19980228T112233Z"));

  /*
  TransactionObject *
TransactionObjectFactory::Make(NSCalendar & cal, 
                               JulianPtrArray & CompList, 
                               User & user, 
                               JulianPtrArray & recipients,
                               UnicodeString & subject, 
                               JulianPtrArray & modifiers, 
                               JulianForm * jf,
                               MWContext * context,
                               UnicodeString & attendeeName,
                               TransactionObject::EFetchType fetchType)
*/
  TransactionObject * gto;
  gto = TransactionObjectFactory::Make(
      *pCal,          /* ical components are loaded into this calendar */
      CompList,       /* a list of pointers to the components loaded */
      *pFromUser, 
      *recipients, 
      subject,
      *ModifierList, 
      NULL, 
      NULL, 
      m_Name, 
      GetTransactionObject::EFetchType_DateRange);
  assert(gto != 0);
  JulianPtrArray  * out = new JulianPtrArray(); assert(out != 0);
  
  TransactionObject::ETxnErrorCode txnStatus;
  gto->executeCAPI(out, txnStatus);
  
  NSCalendar * aCal;
  char sBuf[5000];
  t_int32 size = out->GetSize();
  t_int32 eventSize = 0;

  char * acc;
  for (i = 0; i < size; i++)
  {
    aCal = (NSCalendar *) out->GetAt(i);
    if (aCal->getEvents() != 0)
        eventSize = aCal->getEvents()->GetSize();
    
    pMsg->SendReply(sBuf);
    acc = aCal->toICALString().toCString("")
    pMsg->SendReply(acc);
    delete [] acc;
  }
  return NS_OK;
}
