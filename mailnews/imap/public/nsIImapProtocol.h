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

#ifndef nsIImapProtocol_h___
#define nsIImapProtocol_h___

#include "nsIEventQueueService.h"

#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "plevent.h"
#include "prtime.h"

/* include all of our event sink interfaces */

/* 4CCAC6F0-DCB4-11d2-806D-006008128C4E */

#define NS_IIMAPPROTOCOL_IID							\
{ 0x4ccac6f0, 0xdcb4, 0x11d2,					\
    { 0x80, 0x6d, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

class nsIURI;
class nsIImapUrl;
class nsIImapHostSessionList;
class nsIWebShell;

class nsIImapProtocol : public nsIStreamListener
{
public:
	static const nsIID& GetIID() 
	{
		static nsIID iid = NS_IIMAPPROTOCOL_IID;
		return iid;
	}

	/////////////////////////////////////////////////////////////////////////
	// LoadUrl should really be pushed into a generic protocol interface
	// I think. Imap happens to be the only case where we need to push back
	// a thread safe object to our test app. 
	/////////////////////////////////////////////////////////////////////////
	NS_IMETHOD LoadUrl(nsIURI * aUrl, nsISupports * aConsumer) = 0;

	///////////////////////////////////////////////////////////////////////// 
	// IsBusy returns true if the connection is currently processing a url
	// and false otherwise.
	/////////////////////////////////////////////////////////////////////////
	NS_IMETHOD IsBusy(PRBool & aIsConnectionBusy, 
                      PRBool & isInboxConnection) = 0;

	// Protocol instance examines the url, looking at the host name,
	// user name and folder the action would be on in order to figure out
	// if it can process this url. I decided to push the semantics about
	// whether a connection can handle a url down into the connection level
	// instead of in the connection cache.
	NS_IMETHOD CanHandleUrl(nsIImapUrl * aImapUrl, PRBool & aCanRunUrl,
                            PRBool & hasToWait) = 0;

	/////////////////////////////////////////////////////////////////////////
	// Right now, initialize requires the event queue of the UI thread, 
	// or more importantly the event queue of the consumer of the imap
	// protocol data. The protocol also needs a host session list.
	/////////////////////////////////////////////////////////////////////////
	NS_IMETHOD Initialize(nsIImapHostSessionList * aHostSessionList, nsIEventQueue * aSinkEventQueue) = 0;

    NS_IMETHOD GetThreadEventQueue(nsIEventQueue **aEventQueue) = 0;

    NS_IMETHOD NotifyFEEventCompletion() = 0;

	NS_IMETHOD NotifyHdrsToDownload(PRUint32 *keys, PRUint32 keyCount) = 0;
	NS_IMETHOD NotifyBodysToDownload(PRUint32 *keys, PRUint32 keyCount) = 0;
	// methods to get data from the imap parser flag state.
	NS_IMETHOD GetFlagsForUID(PRUint32 uid, PRBool *foundIt, imapMessageFlagsType *flags) = 0;
	NS_IMETHOD GetSupportedUserFlags(PRUint16 *flags) = 0;

	NS_IMETHOD GetRunningImapURL(nsIImapUrl **aImapUrl) = 0;

	// this is for the temp message display hack
    // ** jt - let's try it a litter more generic way
	NS_IMETHOD GetStreamConsumer (nsISupports **aSupport) = 0;
    NS_IMETHOD GetRunningUrl(nsIURI **aUrl) = 0;

    // Tell thread to die
    NS_IMETHOD TellThreadToDie(PRBool isSafeToDie) = 0;
    // Get last active time stamp
    NS_IMETHOD GetLastActiveTimeStamp(PRTime *aTimeStamp) = 0;
};

#endif /* nsIImapProtocol_h___ */
