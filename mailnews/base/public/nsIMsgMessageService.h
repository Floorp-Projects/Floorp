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

#ifndef nsIMsgMessageService_h___
#define nsIMsgMessageService_h___

#include "nscore.h"
#include "nsISupports.h"

/* F11009C1-F697-11d2-807F-006008128C4E */

#define NS_IMSGMESSAGESERVICE_IID              \
{ 0xf11009c1, 0xf697, 0x11d2,                  \
    { 0x80, 0x7f, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

////////////////////////////////////////////////////////////////////////////////////////
// The Message Service is a pretty generic interface used to represent all of the
// different types of operations one performs on a message or article (for news). 
// Examples include copying messages, displaying messages, etc. Our current thinking is
// that the mailnews data source can take a uri and get a message service which can
// handle that uri type. i.e. given a uri, the data source can generate a progid for
// a particular protocol (imap, news, pop) which can perform the message operation. 
// All of these protocol services support this generic interface so the data source
// doesn't have to explicitly know about and call into the imap service or the pop 
// service or the news service...well you get the picture...All our msg based protocol 
// services support this interface and it makes life for the mailnews data source
// much easier!
////////////////////////////////////////////////////////////////////////////////////////

class nsIURL;
class nsIStreamListener;
class nsIUrlListener;

class nsIMsgMessageService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGMESSAGESERVICE_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////////////////////
	// If you want a handle on the running task, pass in a valid nsIURL ptr. You can later 
	// interrupt this action by asking the netlib service manager to interrupt the url you are given 
	// back. Remember to release aURL when you are done with it. Pass nsnull in for aURL if you don't 
	// care about the returned URL.
	/////////////////////////////////////////////////////////////////////////////////////////////
	
	/////////////////////////////////////////////////////////////////////////////////////////////
	// CopyMessage: Pass in the URI for the message you want to have copied. 
	// aCopyListener already knows about the destination folder. Set aMoveMessage to PR_TRUE 
	// if you want the message to be moved. PR_FALSE leaves it as just a copy.
	/////////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD CopyMessage(const char * aSrcURI, nsIStreamListener * aCopyListener, PRBool aMoveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL) = 0;
//	NS_IMETHOD CopyMessages(PRUnichar * aSrcMailboxURI[], PRBool moveMessage);

	/////////////////////////////////////////////////////////////////////////////////////////////
	// DisplayMessage: When you want a message displayed....
	// aMessageURI is a uri representing the message to display.
	// aDisplayConsumer is (for now) a nsIWebshell which we'll use to load the message into. 
	// It would be nice if we can figure this out for ourselves in the protocol but we can't do 
	// that right now. 
	//////////////////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
							  nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;
};

#endif /* nsIMessageService_h___ */
