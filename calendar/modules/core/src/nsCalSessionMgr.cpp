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
 * nsCalSessionMgr.cpp: implementation of the nsCalSessionMgr class.
 * This class manages the list of sessions to calendar stores. When
 * a session is needed, this class should be called to grant it.
 */

#include "jdefines.h"
#include "julnstr.h"
#include "nsString.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nspr.h"
#include "nsCurlParser.h"
#include "nsCalSession.h"
#include "nsCalSessionMgr.h"

nsCalSessionMgr::nsCalSessionMgr()
{
}

nsCalSessionMgr::~nsCalSessionMgr()
{
}

/**
 * Shut down all sessions.
 * @return 0 on success
 *         otherwise the number of sessions that had problems shutting
 *         down.
 */
nsresult nsCalSessionMgr::Shutdown()
{
  nsCalSession* pSession;
  int j, iCount;
  int iErrors = 0;

  /*
   * Shut down any open sessions...
   */
	for (int i = 0; i < m_List.GetSize(); i++)
	{
		pSession = (nsCalSession*) m_List.GetAt(i);
    iCount = pSession->GetUsageCount();
    if (0 == iCount)
      ++iCount;
    for (j = 0; j < iCount; ++j)
    {
      if (0 != pSession->ReleaseSession())
        ++iErrors;
    }
    delete pSession;
	}
  return (nsresult) iErrors;
}

/**
 * Get a session to the supplied curl.
 * @param psCurl     the curl to the calendar store
 * @param psPassword the password needed for logging in
 * @param s          the session
 * @return 0 on success
 *         1 problems pulling the session from the list
 *         problems from nsCalSession::GetSession.
 */
nsresult nsCalSessionMgr::GetSession(const char* psCurl, long lFlags, const char* psPassword, CAPISession& s)
{
  /*
   * First, search the current sessions and see if we already have a
   * session to the requested server...
   */
  nsCurlParser Curl(psCurl);
  int iIndex;

  if (0 == Find(Curl.GetCurl().GetBuffer(), 0, &iIndex))
  {
    /*
     * Found it
     */
    nsCalSession* pSession = GetAt(iIndex);
    if (0 == pSession)
      return 1;
    return pSession->GetSession(s,psPassword);
  }
  else
  {
    /*
     * nothing found, have to create it.
     */
    nsCalSession* pSes = new nsCalSession(psCurl,lFlags);
    m_List.Add(pSes);
    return pSes->GetSession(s,psPassword ? psPassword : "");
  }
}

/**
 * Get a session to the supplied curl.
 * @param sCurl       the curl to the calendar store
 * @param psPassword  the password needed for logging in
 * @param pCalSession the session object that was found or created
 * @return 0 on success (that is, the session was established with the
 *           capi server) or the error that occurred while trying to
 *           establish the session.
 */
nsresult nsCalSessionMgr::GetSession(const JulianString sCurl, long lFlags, const char* psPassword, nsCalSession* &pCalSession)
{
  /*
   * First, search the current sessions and see if we already have a
   * session to the requested server...
   */
  nsCurlParser Curl(sCurl);
  int iIndex;

  pCalSession = 0;

  if (0 == Find(Curl.GetCurl().GetBuffer(), 0, &iIndex))
  {
    /*
     * Found it
     */
    pCalSession = GetAt(iIndex);
    if (0 == pCalSession)
      return 1;
    return pCalSession->EstablishSession(psPassword);
  }
  else
  {
    /*
     * nothing found, have to create it.
     */
    pCalSession = new nsCalSession(sCurl,lFlags);
    m_List.Add(pCalSession);
    return pCalSession->EstablishSession(psPassword ? psPassword : "");
  }
}


/**
 * Release a session
 * @param pUser pointer to the calendar to add
 * @return 0 on success
 *         1 if not found
 */
nsresult nsCalSessionMgr::ReleaseSession(CAPISession& s)
{
  int iIndex;
  nsresult res;
  if (0 != (res = Find(s,0,&iIndex)))
    return res;
  
  nsCalSession* pSes = (nsCalSession*) GetAt(iIndex);
  if (0 == pSes)
    return 1;

  pSes->ReleaseSession();

  return NS_OK;
}

/**
 * Search for a calendar based on its curl.
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
nsresult nsCalSessionMgr::Find(char* p, int iStart, int* piFound)
{
  nsCalSession* pSession;

  *piFound = -1;
  if (m_List.GetSize() > 0)
  {
    if (iStart >= m_List.GetSize())
      iStart = m_List.GetSize() - 1;
	  for (int i = iStart; i < m_List.GetSize(); i++)
	  {
		  pSession = (nsCalSession*) m_List.GetAt(i);
      if ( (pSession->GetCurl()) == p)
      {
        *piFound = i;
        return NS_OK;
      }
	  }
  }

  return 1;
}

/**
 * Search for a calendar based on its CAPISession.
 * @param s      the capi session to search for.
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
nsresult nsCalSessionMgr::Find(CAPISession s, int iStart, int* piFound)
{
  nsCalSession* pSession;

  *piFound = -1;
  if (m_List.GetSize() > 0)
  {
    if (iStart >= m_List.GetSize())
      iStart = m_List.GetSize() - 1;
	  for (int i = iStart; i < m_List.GetSize(); i++)
	  {
		  pSession = (nsCalSession*) m_List.GetAt(i);
      if ( (pSession->m_Session) == s)
      {
        *piFound = i;
        return NS_OK;
      }
	  }
  }
  return 1;
}

/**
 * Get the calendar session definition at the supplied index.
 * @param i the index to fetch
 * @return a pointer to the calendar session at the supplied index.
 */ 
nsCalSession* nsCalSessionMgr::GetAt(int i)
{
  return (nsCalSession*) m_List.GetAt(i);
}

