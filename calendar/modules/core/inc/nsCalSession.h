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
#ifndef __NS_CAL_SESSION__
#define __NS_CAL_SESSION__

#include "capi.h"
#include "nsICapi.h"
#include "nscalexport.h"

class NS_CALENDAR nsCalSession
{
public:
  nsICapi * mCapi;

protected:
  JulianString    m_sCurl;          /* example: capi://cal.mcom.com/sman */
  PRInt32         m_iCount;         /* a list of JulianString CalURL strings */
  long            m_lFlags;         /* login flags */

public:
                  nsCalSession();
                  nsCalSession(const JulianString& sCurl, long lFlags);
                  nsCalSession(const char* psCurl, long lFlags);
  virtual         ~nsCalSession();

  CAPISession     m_Session;        /* associated session */

  /**
   * @return the number of holders using this session
   */
  PRInt32         GetUsageCount() const {return m_iCount;}
  
  /**
   * @return the CURL associated with this session
   */
  JulianString    GetCurl() const   {return m_sCurl;}

  /**
   * Establish a CAPI session to the supplied curl. If a session already
   * exists, bump the reference count and return the existing session.
   * @return 0 on success
   *         CAPI errors associated with not getting a session.
   */
  nsresult        GetSession(CAPISession& s, const char* psPassword=0);

  /**
   * Establish a CAPI session to the supplied curl. If a session already
   * exists, bump the reference count and return the existing session.
   * @return 0 on success
   *         CAPI errors associated with not getting a session.
   */
  nsresult        EstablishSession(const char* psPassword=0);

  /**
   * Release the session. That is, a consumer is indicating that they
   * are finished using the session. Decrement the usage count.
   * The session should not be destroyed until the usage count is 0.
   * @return 0 on success
   */
  nsresult        ReleaseSession();

};

#endif  // __NS_CAL_SESSION__
