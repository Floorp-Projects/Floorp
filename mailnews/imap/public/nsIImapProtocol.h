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

#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "plevent.h"

/* include all of our event sink interfaces */

/* 4CCAC6F0-DCB4-11d2-806D-006008128C4E */

#define NS_IIMAPPROTOCOL_IID							\
{ 0x4ccac6f0, 0xdcb4, 0x11d2,					\
    { 0x80, 0x6d, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* 8C0C40D1-E173-11d2-806E-006008128C4E */

#define NS_IMAPPROTOCOL_CID								\
{ 0x8c0c40d1, 0xe173, 0x11d2,							\
    { 0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

class nsIURL;

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
	NS_IMETHOD LoadUrl(nsIURL * aUrl, nsISupports * aConsumer) = 0;

	/////////////////////////////////////////////////////////////////////////
	// Right now, initialize requires the event queue of the UI thread, 
	// or more importantly the event queue of the consumer of the imap
	// protocol data.
	/////////////////////////////////////////////////////////////////////////
	NS_IMETHOD Initialize(PLEventQueue * aSinkEventQueue) = 0;

    NS_IMETHOD GetThreadEventQueue(PLEventQueue **aEventQueue) = 0;
    
    NS_IMETHOD SetMessageDownloadOutputStream(nsIOutputStream *aOutputStream)
        = 0;
    NS_IMETHOD NotifyFEEventCompletion() = 0;
};

#endif /* nsIImapProtocol_h___ */
