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

/**
 * nsCalUserList.cpp: implementation of the nsCalUserList class.
 * This class manages the list of users or resources that have
 * calendars currently in memory. All calendars that are to be 
 * displayed should have their users registered in this list.
 */

#include "jdefines.h"
#include "julnstr.h"
#include "nsString.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nspr.h"
#include "nsICalendarUser.h"
#include "nsCalUserList.h"

nsCalUserList::nsCalUserList()
{
}

nsCalUserList::~nsCalUserList()
{
}

/**
 * Add a calendar to the list
 * @param pUser pointer to the calendar to add
 * @return 0 on success
 *         1 problems adding the calendar
 */
nsresult nsCalUserList::Add(nsICalendarUser* pUser)
{
  if (0 > m_List.Add(pUser))
    return 1;
  return 0;
}

/**
 * Delete the calendar matching the supplied pointer.
 * @param pUser pointer to the calendar to add
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalUserList::Delete(nsICalendarUser* pUser)
{
  return (1 ==  m_List.Remove(pUser)) ? 0 : 1;
}

/**
 * Delete all calendars having the supplied cal url
 * @param pCurl pointer to the curl of this calendar store
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalUserList::Delete(char* psCurl)
{
  int i;
  nsresult s = Find(psCurl,0,&i);
  m_List.RemoveAt(i,1);
  return NS_OK;
}

/**
 * Search for a calendar.
 * @param psCurl pointer to the curl of this calendar store
 * @param iStart start searching at this point in the list
 *               if iStart is < 0 it is snapped to 0. If it
 *               is >= list size, it is snapped to the last
 *               index.
 * @param piFound the index of the calendar of the list that
 *               matches the psCurl. This value is always
 *               >= iStart when the return value is 0. It 
 *               is returned as -1 if the curl cannot be found.
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalUserList::Find(char* p, int iStart, int* piFound)
{
  nsICalendarUser* pUser;
  JulianPtrArray* pList;
  JulianString* psCurl;
  int i,j;

  *piFound = -1;
  if (m_List.GetSize() > 0)
  {
    if (iStart >= m_List.GetSize())
      iStart = m_List.GetSize() - 1;
	  for (i = 0; i < m_List.GetSize(); i++)
	  {
		  pUser = (nsICalendarUser*) m_List.GetAt(i);
      if (0 != pUser)
      {
#if 0
        pList = pUser->GetCalAddrList();
        for (j = 0; j < pList->GetSize(); j++)
        {
          psCurl = (JulianString *)pList->GetAt(i);
          if ( (*psCurl) == p)
          {
            *piFound = i;
            return NS_OK;
           }
        }
#endif
      }
	  }
  }
  return 1;
}

/**
 * Get the calendar at the supplied index.
 * @param i the index of the calendar to fetch
 * @return a pointer to the calendar at the supplied index.
 */ 
nsICalendarUser* nsCalUserList::GetAt(int i)
{
  return (nsICalendarUser*) m_List.GetAt(i);
}

