/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIMsgMailSession_h___
#define nsIMsgMailSession_h___

#include "nsISupports.h"

/* D5124440-D59E-11d2-806A-006008128C4E */

#define NS_IMSGMAILSESSION_IID							\
{ 0xd5124440, 0xd59e, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* D5124441-D59E-11d2-806A-006008128C4E */
#define NS_MSGMAILSESSION_CID							\
{ 0xd5124441, 0xd59e, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

///////////////////////////////////////////////////////////////////////////////////
// The mail session is a replacement for the old 4.x MSG_Master object. It contains
// mail session generic information such as the user's current mail identity, ....
// I'm starting this off as an empty interface and as people feel they need to
// add more information to it, they can. I think this is a better approach than 
// trying to port over the old MSG_Master in its entirety as that had a lot of 
// cruft in it....
//////////////////////////////////////////////////////////////////////////////////

#include "nsIMsgIdentity.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgAccountManager.h"

class nsIMsgMailSession : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGMAILSESSION_IID; return iid; }

	///////////////////////////////////////////////////////////////////////////////////
	// The user's current identity is used to abstract out information such as the
	// user name, pwd, mail server to use, etc....
	//////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD GetCurrentIdentity(nsIMsgIdentity ** aIdentity) = 0;
    NS_IMETHOD GetCurrentServer(nsIMsgIncomingServer* *aServer) = 0;
    NS_IMETHOD GetAccountManager(nsIMsgAccountManager* *aAccountManager) = 0;
};

#endif /* nsIMsgMailSession_h___ */
