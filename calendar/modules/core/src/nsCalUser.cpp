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
 *  nsCalUser
 *     Simple abstraction for the owner of a calendar store. Applies
 *     to a resource or a person.  This implementation assumes a single
 *     NSCalendar per user.
 *
 *  sman
 */
#include "jdefines.h"
#include "julnstr.h"
#include "nsString.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nsCalUser.h"
#include "nspr.h"

nsCalUser::nsCalUser()
{
  InitMembers();
}

nsCalUser::nsCalUser(JulianString& sUserName, nsString& sDisplayName)
{
  InitMembers();
  m_sUserName = sUserName;
  m_sDisplayName = sDisplayName;
}

nsCalUser::nsCalUser(char* sUserName, char* sDisplayName)
{
  InitMembers();
  m_sUserName = sUserName;
  m_sDisplayName = sDisplayName;
}

nsCalUser::~nsCalUser()
{
  PRInt32 size = m_CalAddrList.GetSize();
  PRInt32 i ;

  for (i=0; i<size; i++)
  {
    JulianString * element = (JulianString *)m_CalAddrList.GetAt(i);
    delete element;
  }

  m_CalAddrList.RemoveAll();
}

/**
 * Initialize all data members to something
 */
void nsCalUser::InitMembers()
{
  m_sUserName = "";     /* example: sman */
  m_sDisplayName = "";  /* example: Steve Mansour */
  m_pCal = 0;           /* the preferred calendar account (the only one for now) */
}

/**
 *  If the user has multiple calendars, this routine returns the "preferred" 
 *  calendar. For now, we're just going to return the first one.
 */
JulianString* nsCalUser::GetPreferredCalAddr()
{
  return (JulianString*) m_CalAddrList.GetAt(0);
}

/**
 * Add a calendar address to the list of calendar addresses for this user...
 * @return 0 = success
 *         1 = failure
 */
nsresult nsCalUser::AddCalAddr(JulianString* psAddr)
{
  return (0 > m_CalAddrList.Add(psAddr));
}
