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
 * This extends the nsCalUser just a bit. It includes a password for 
 * logging in and a local capi url.
 *
 * sman
 */

#ifndef __NS_CAL_LOGGED_IN_USER__
#define __NS_CAL_LOGGED_IN_USER__

#include "nscalexport.h"

class NS_CALENDAR nsCalLoggedInUser : public nsCalUser
{
private:
  JulianString    m_sPassword;      /* user's password */
  JulianString    m_sLocalCapiUrl;  /* url to local capi file */

public:
                  nsCalLoggedInUser();
  virtual         ~nsCalLoggedInUser();

  JulianString&   GetPassword() {return m_sPassword;}
  void            SetPassword(char* psName) { m_sPassword = psName; }

  JulianString&   GetLocalCapiUrl() {return m_sLocalCapiUrl;}
  void            SetLocalCapiUrl(char* psName) { m_sLocalCapiUrl = psName; }
};

#endif  // __NS_CAL_LOGGED_IN_USER__
