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
 *  nsCalSession
 *     Maintains a list of active sessions, CURLs, and reference counts.
 *
 *  sman
 */

#include "jdefines.h"
#include "julnstr.h"
#include "nsString.h"
#include "ptrarray.h"
#include "nscal.h"
#include "nspr.h"
#include "capi.h"
#include "nsCalSession.h"
#include "nsICapi.h"
#include "nsICapiLocal.h"
#include "nsCapiCIID.h"
#include "nsCurlParser.h"

static NS_DEFINE_IID(kCCapiLocalCID, NS_CAPI_LOCAL_CID);
static NS_DEFINE_IID(kCCapiCSTCID,   NS_CAPI_CST_CID);
static NS_DEFINE_IID(kICapiIID,      NS_ICAPI_IID);
static NS_DEFINE_IID(kICapiLocalIID, NS_ICAPI_LOCAL_IID);


/*
 * m_iCount  ->  0 - n  means that it is in use and that this is
 *                      this is the current number of users. A value
 *                      of 0 just means that it has been created but
 *                      no session has yet been established to the
 *                      server.
 *               -1     means that it has been used, released by
 *                      everyone who was using it, and is now ready
 *                      to be destroyed.
 */
nsCalSession::nsCalSession()
{
  m_sCurl = "";
  m_Session = 0;
  m_iCount = 0;
  m_lFlags = 0;
  mCapi = nsnull;
}

nsCalSession::nsCalSession(const JulianString& sCurl, long lFlags)
{
  m_sCurl = sCurl;
  m_Session = 0;
  m_iCount = 0;
  m_lFlags = lFlags;
  mCapi = nsnull;
}

nsCalSession::nsCalSession(const char* psCurl, long lFlags)
{
  m_sCurl = psCurl;
  m_Session = 0;
  m_iCount = 0;
  m_lFlags = lFlags;
  mCapi = nsnull;
}

nsCalSession::~nsCalSession()
{
  NS_IF_RELEASE(mCapi);
}

/**
 * Establish a CAPI session to the supplied curl. If a session already
 * exists, bump the reference count and return the existing session.
 * If the value of s is 0 on return, there was a problem getting the
 * session.
 * @return 0 on success
 *         1 general failure
 *         CAPI errors associated with not getting a session.
 */
nsresult nsCalSession::GetSession(CAPISession& Session, const char* psPassword)
{
  Session = 0;
  nsresult res = EstablishSession(psPassword);
  ++m_iCount;
  Session = m_Session;
  return res;
}

/**
 * Establish a CAPI session. If a session already
 * exists, bump the reference count and return the existing session.
 * If the value of s is 0 on return, there was a problem getting the
 * session.
 * @return 0 on success
 *         1 general failure
 *         CAPI errors associated with not getting a session.
 */
nsresult nsCalSession::EstablishSession(const char* psPassword)
{
  nsresult res = NS_OK;
  if (0 == m_Session)
  {
    CAPIStatus s;
    nsICapiLocal * capi_local = nsnull;

    if ( m_sCurl == "")
      return 1;

    /*
     * Create an nsICapi Instance
     *
     * TODO:  The CID should be based on the type of url passed in!
     *        For now, do local capi
     */
    res = 1;
    nsCurlParser cp(m_sCurl);
    switch ( cp.GetProtocol() )
    {
      case nsCurlParser::eFILE:
        res = nsRepository::CreateInstance(kCCapiLocalCID,nsnull,kICapiIID,(void**)&mCapi);
      break;

      case nsCurlParser::eCAPI:
      {
        res = nsRepository::CreateInstance(kCCapiCSTCID,nsnull, kICapiIID, (void**)&mCapi);
      }
      break;
    }

    if (NS_OK != res)
      return res;

    mCapi->Init();

    /*
     * Since we believe this to be local capi, see if the capi
     * object we created supports the nsICapiLocal interface
     */
    res = mCapi->QueryInterface(kICapiLocalIID,(void**)&capi_local);

    if (NS_OK == res)
    {
      /*
       * Do the CAPI login
       */
      s = capi_local->CAPI_LogonCurl(m_sCurl.GetBuffer(), psPassword, m_lFlags, &m_Session);
      NS_RELEASE(capi_local);
    }
    else
    {
      /*
       * use standard login...
       * Try several things for the user name
       */
      JulianString sName;
      JulianString sPW;
      JulianString sHost = cp.GetHost();
      if ( cp.GetUser() != "" )
        sName = cp.GetUser();
      else
        sName = cp.GetExtra();

      if (cp.GetPassword() != "")
        sPW = cp.GetPassword();
      else
        sPW = psPassword;

      s = mCapi->CAPI_Logon(
        sName.GetBuffer(), sPW.GetBuffer(), 
        sHost.GetBuffer(), m_lFlags, &m_Session);
    }

    if (CAPI_ERR_OK != s)
      return res;
  }

  return res;
}

/**
 * Release the session. That is, a consumer is indicating that they
 * are finished using the session. Decrement the usage count.
 * The session should not be destroyed until the usage count is 0.
 * @return 0 on success
 *         CAPI logoff errors
 */
nsresult nsCalSession::ReleaseSession()
{
  if (--m_iCount <= 0)
  {
    m_iCount = -1;
    
    /*
     * do a capi logoff here...
     */
    CAPIStatus s = mCapi->CAPI_Logoff(&m_Session, 0L);
    if (CAPI_ERR_OK != s)
        return (nsresult) s;
  }
  return NS_OK;
}
