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
 * nsCalList.cpp: implementation of the nsCalList class.
 * This class manages the list of calendars currently in memory.
 * All calendars that are to be displayed should be registered
 * in this list.
 */

#include "jdefines.h"
#include "julnstr.h"
#include "nsString.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nspr.h"
#include "nsCalList.h"

nsCalList::nsCalList()
{
}

nsCalList::~nsCalList()
{
}

/**
 * Add a calendar to the list
 * @param pCal pointer to the calendar to add
 * @return 0 on success
 *         1 problems adding the calendar
 *         2 already in the list
 */
nsresult nsCalList::Add(NSCalendar* pCal)
{
	int index = m_List.FindIndex(0, pCal);
	if (index != -1)
	{
    if (0 > m_List.Add(pCal))
      return 1;
    return 0;
	}
	return 2;
}

/**
 * Delete the calendar matching the supplied pointer.
 * @param pCal pointer to the calendar to add
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalList::Delete(NSCalendar* pCal)
{
  return (1 ==  m_List.Remove(pCal)) ? 0 : 1;

}

/**
 * Delete all calendars having the supplied cal url
 * @param pCurl pointer to the curl of this calendar store
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalList::Delete(char* psCurl)
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
nsresult nsCalList::Find(char* psCurl, int iStart, int* piFound)
{
  NSCalendar* pCal;
  *piFound = -1;
  if (m_List.GetSize() > 0)
  {
    if (iStart >= m_List.GetSize())
      iStart = m_List.GetSize() - 1;
	  for (int i = 0; i < m_List.GetSize(); i++)
	  {
		  pCal = (NSCalendar*) m_List.GetAt(i);
      if (0 != pCal)
      {
  		  if ( pCal->getCurl() == psCurl )
        {
          *piFound = i;
          return NS_OK;
        }
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
NSCalendar* nsCalList::GetAt(int i)
{
  return (NSCalendar*) m_List.GetAt(i);
}

