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

#ifndef nsIPop3Service_h___
#define nsIPop3Service_h___

#include "nscore.h"
#include "nsISupports.h"
#include "MailNewsTypes.h"
#include "nsFileSpec.h"

/* 3BB459E0-D746-11d2-806A-006008128C4E */

#define NS_IPOP3SERVICE_IID								\
{ 0x3bb459e0, 0xd746, 0x11d2,							\
    { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#define NS_POP3SERVICE_CID								\
{ /* 3BB459E3-D746-11d2-806A-006008128C4E} */			\
 0x3bb459e3, 0xd746, 0x11d2,							\
  { 0x80, 0x6a, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e }}

////////////////////////////////////////////////////////////////////////////////////////
// The Pop3 Service is an interface designed to make building and running pop3 urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////
class nsIURL;
class nsIStreamListener;
class nsIUrlListener;

class nsIPop3Service : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IPOP3SERVICE_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////////////////////
	// All of these functions build pop3 urls and run them. If you want a handle 
	// on the running task, pass in a valid nsIURL ptr. You can later interrupt this action by 
	// asking the netlib service manager to interrupt the url you are given back. Remember to release 
	// aURL when you are done with it. Pass nsnull in for aURL if you don't care about 
	// the returned URL.
	/////////////////////////////////////////////////////////////////////////////////////////////
	
	/////////////////////////////////////////////////////////////////////////////////////////////
	// right now getting new mail doesn't require any user specific data. We use the default current
	// identity for this information. I suspect that we'll eventually pass in an identity to this 
	// call so you can get mail on different pop3 accounts....
	//////////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD GetNewMail(nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;

};

#endif /* nsIPop3Service_h___ */
