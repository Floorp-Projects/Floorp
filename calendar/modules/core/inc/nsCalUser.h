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
#ifndef __NS_CAL_USER__
#define __NS_CAL_USER__
#include "nscalexport.h"

class NS_CALENDAR nsCalUser
{
private:
  JulianString    m_sUserName;      /* example: sman */
  nsString        m_sDisplayName;   /* example: Steve Mansour */
  JulianPtrArray  m_CalAddrList;    /* a list of JulianString CalURL strings */
  NSCalendar*     m_pCal;           /* the preferred calendar account (the only one for now) */
  JulianPtrArray  m_CalList;        /* an array of calendars owned by this user */
  void            InitMembers();

public:
                  nsCalUser();
                  nsCalUser(JulianString& sUserName, nsString& sDisplayName);
                  nsCalUser(char* sUserName, char* sDisplayName);
  virtual         ~nsCalUser();

  JulianString&   GetUserName()                 {return m_sUserName;}
  void            SetUserName(char* psName)     {m_sUserName = psName; }

  nsString&       GetDisplayName()              {return m_sDisplayName;}
  void            SetDisplayName(char* psName)  { m_sDisplayName = psName; }

  JulianPtrArray* GetCalAddrList()              {return &m_CalAddrList;}
  JulianPtrArray* GetCalList()                  {return &m_CalList;}

  /**
   * Add a calendar address to the list of calendar addresses for this user
   * @param  psAddr the CURL pointing to the calendar store.
   * @return 0 = success
   *         1 = failure
   */
  nsresult        AddCalAddr(JulianString* psAddr);
  JulianString*   GetPreferredCalAddr();
  
  NSCalendar*     GetNSCal()                    {return m_pCal;}

};

#endif  // __NS_CAL_USER__
