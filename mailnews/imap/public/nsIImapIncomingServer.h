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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



#ifndef __gen_nsIImapIncomingServer_h__
#define __gen_nsIImapIncomingServer_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsID.h" /* interface nsID */

/* starting interface:    nsIImapIncomingServer */

/* {3d2e7e38-f9d8-11d2-af8f-001083002da8} */
#define NS_IIMAPINCOMINGSERVER_IID_STR "3d2e7e38-f9d8-11d2-af8f-001083002da8"
#define NS_IIMAPINCOMINGSERVER_IID \
  {0x3d2e7e38, 0xf9d8, 0x11d2, \
    { 0xaf, 0x8f, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 }}

// ** forward declaration **
class nsIUrlListener;
class nsIURI;
class nsIImapUrl;
class nsIEventQueue;
class nsIImapProtocol;
class nsISupportsArray;

class nsIImapIncomingServer : public nsISupports {
public: 
    static const nsIID& GetIID() {
        static nsIID iid = NS_IIMAPINCOMINGSERVER_IID;
        return iid;
    }
    
    // ** attribute maximum cached connections **
    NS_IMETHOD GetMaximumConnectionsNumber(PRInt32* maxConnections) = 0;
    NS_IMETHOD SetMaximumConnectionsNumber(PRInt32 maxConnections) = 0;

    NS_IMETHOD GetTimeOutLimits(PRInt32* minutes) = 0;
    NS_IMETHOD SetTimeOutLimits(PRInt32 minutes) = 0;
    
    NS_IMETHOD GetImapConnectionAndLoadUrl(nsIEventQueue* aClientEventQueue,
                                           nsIImapUrl* aImapUrl,
                                           nsIUrlListener* aUrlListener,
                                           nsISupports* aConsumer,
                                           nsIURI** aURL) = 0;
    NS_IMETHOD LoadNextQueuedUrl() = 0;
    NS_IMETHOD RemoveConnection(nsIImapProtocol* aImapConnection) = 0;

	NS_IMETHOD GetUnverifiedFolders(nsISupportsArray *aFolderArray, PRInt32 *aNumUnverifiedFolders) = 0;
};

#endif /* __gen_nsIPop3IncomingServer_h__ */
