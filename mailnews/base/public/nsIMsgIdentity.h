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

#ifndef nsIMsgIdentity_h___
#define nsIMsgIdentity_h___

#include "nsISupports.h"

/* D3B4A420-D5AC-11d2-806A-006008128C4E */

#define NS_IMSGIDENTITY_IID								\
{ 0xd3b4a420, 0xd5ac, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* E7F875B0-D5AC-11d2-806A-006008128C4E */
#define NS_IMSGIDENTITY_CID								\
{ 0xe7f875b0, 0xd5ac, 0x11d2,							\
	{ 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

///////////////////////////////////////////////////////////////////////////////////
// an identity is an object designed to encapsulate all the information we need 
// to know about a user identity. I expect this interface to grow and change a lot
// as we flesh out our thoughts on multiple identities and what properties go into
// these identities.
//////////////////////////////////////////////////////////////////////////////////

class nsIMsgIdentity : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGIDENTITY_IID; return iid; }

	///////////////////////////////////////////////////////////////////////////////////
	// The user's current identity is used to abstract out information such as the
	// user name, pwd, mail server to use, etc....
	//////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD GetRootFolderPath(const char ** aRootFolderPath) = 0;

	NS_IMETHOD GetPopName(const char ** aPopName) = 0; // right now it is pop & smtp user name
	NS_IMETHOD GetSmtpName(const char ** aSmtpName) = 0;

	NS_IMETHOD GetOrganization(const char ** aOrganization) = 0;
	NS_IMETHOD GetUserFullName(const char ** aUserFullName) = 0; // User real name
	NS_IMETHOD GetUserEmail(const char ** aUserEmail) = 0;
	NS_IMETHOD GetPopPassword(const char ** aUserPassword) = 0;
	NS_IMETHOD GetPopServer(const char ** aHostName) = 0;
	NS_IMETHOD GetSmtpServer(const char ** aHostName) = 0;
	NS_IMETHOD GetReplyTo(const char ** aReplyTo) = 0;
	NS_IMETHOD GetImapServer(const char ** aHostName) = 0;
	NS_IMETHOD GetImapName(const char ** aImapName) = 0;
	NS_IMETHOD GetImapPassword(const char ** aImapPassword) = 0;

};

#endif /* nsIMsgIdentity_h___ */
