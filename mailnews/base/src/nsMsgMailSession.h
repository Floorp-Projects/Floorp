/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsMsgMailSession_h___
#define nsMsgMailSession_h___

#include "nsIMsgMailSession.h"
#include "nsISupports.h"

///////////////////////////////////////////////////////////////////////////////////
// The mail session is a replacement for the old 4.x MSG_Master object. It contains
// mail session generic information such as the user's current mail identity, ....
// I'm starting this off as an empty interface and as people feel they need to
// add more information to it, they can. I think this is a better approach than 
// trying to port over the old MSG_Master in its entirety as that had a lot of 
// cruft in it....
//////////////////////////////////////////////////////////////////////////////////

#include "nsIMsgAccountManager.h"

class nsMsgMailSession : public nsIMsgMailSession
{
public:
	nsMsgMailSession();
	virtual ~nsMsgMailSession();

	NS_DECL_ISUPPORTS
	
	// nsIMsgMailSession support
	NS_IMETHOD GetCurrentIdentity(nsIMsgIdentity ** aIdentity);
    NS_IMETHOD GetCurrentServer(nsIMsgIncomingServer **aServer);
  
protected:
  nsIMsgAccountManager *m_accountManager;
};

#endif /* nsMsgMailSession_h__ */
