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

#ifndef nsIMailboxService_h___
#define nsIMailboxService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "MailNewsTypes.h"
#include "nsFileSpec.h"

/* EEF82460-CB69-11d2-8065-006008128C4E */

#define NS_IMAILBOXSERVICE_IID                         \
{ 0xeef82460, 0xcb69, 0x11d2,                  \
    { 0x80, 0x65, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#define NS_MAILBOXSERVICE_CID						  \
{ /* EEF82462-CB69-11d2-8065-006008128C4E */      \
 0xeef82462, 0xcb69, 0x11d2,                      \
 {0x80, 0x65, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

////////////////////////////////////////////////////////////////////////////////////////
// The Mailbox Service is an interface designed to make building and running mailbox urls
// easier. I'm not sure if this service will go away when the new networking model comes
// on line (as part of the N2 project). So I reserve the right to change my mind and take
// this service away =).
////////////////////////////////////////////////////////////////////////////////////////
class nsIURL;
class nsIStreamListener;
class nsIUrlListener;

class nsIMailboxService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMAILBOXSERVICE_IID; return iid; }

	/////////////////////////////////////////////////////////////////////////////////////////////
	// All of these functions build mailbox urls and run them. If you want a handle 
	// on the running task, pass in a valid nsIURL ptr. You can later interrupt this action by 
	// asking the netlib service manager to interrupt the url you are given back. Remember to release 
	// aURL when you are done with it. Pass nsnull in for aURL if you don't care about 
	// the returned URL.
	/////////////////////////////////////////////////////////////////////////////////////////////
	
	/////////////////////////////////////////////////////////////////////////////////////////////
	// Pass in a file path for the mailbox you wish to parse. You also need to pass in a mailbox 
	// parser (the consumer). The url listener can be null if you have no interest in tracking
	// the url.
	//////////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD ParseMailbox(const nsFileSpec& aMailboxPath, nsIStreamListener * aMailboxParser, 
							nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;

	/////////////////////////////////////////////////////////////////////////////////////////////
	// Pass in the URI for the message you want to have copied. We can extract the message id
	// and the mail folder from the URI. aMailboxCopy already knows about the destination folder.
	// Set moveMessage to TRUE if you want the message to be moved. FALSE leaves it as just a copy.
	/////////////////////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopy, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL) = 0;
//	NS_IMETHOD CopyMessages(PRUnichar * aSrcMailboxURI[], PRBool moveMessage);

	/////////////////////////////////////////////////////////////////////////////////////////////
	// When you want a message from the mailbox displayed, pass in the path to the mailbox, 
	// the starting and ending byte values in the mailbox for the message. 
	// aDisplayConsumer is (for now) a nsIWebshell which we'll use to load the message into. 
	// It would be nice if we can figure this out for ourselves in the protocol but we can't do 
	// that right now. 
	//////////////////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD DisplayMessage(const nsFileSpec& aMailboxPath, nsMsgKey aMessageKey, const char * aMessageID,
		nsISupports * aDisplayConsumer, nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;

	/////////////////////////////////////////////////////////////////////////////////////////////
	// This is more of a convience function for testing purposes. We want to able to say: display 
	// message number 'n' in this mailbox without having to go out and get the key for message number 
	// 'n'. this function simply makes that possible. 
	/////////////////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD DisplayMessageNumber(const nsFileSpec& aMailboxPath, PRUint32 aMessageNumber, 
		nsISupports * aDisplayConsumer,	nsIUrlListener * aUrlListener, nsIURL ** aURL) = 0;

};

#endif /* nsIMailboxService_h___ */
