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

#ifndef nsIImapService_h___
#define nsIImapService_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsFileSpec.h"

/* 9E3233E1-EBE2-11d2-95AD-000064657374 */

#define NS_IIMAPSERVICE_IID                         \
{ 0x9e3233e1, 0xebe2, 0x11d2,                 \
    { 0x95, 0xad, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

////////////////////////////////////////////////////////////////////////////////////////
// The IMAP Service is an interfaced designed to make building and running imap urls
// easier. Clients typically go to the imap service and ask it do things such as:
// get new mail, etc....
//
// Oh and in case you couldn't tell by the name, the imap service is a service! and you
// should go through the service manager to obtain an instance of it.
////////////////////////////////////////////////////////////////////////////////////////

class nsIImapProtocol;
class nsIImapMessageSink;
class nsIUrlListener;
class nsIURI;
class nsIImapUrl;
class nsIEventQueue;
class nsIMsgFolder;
class nsIMsgStatusFeedback;

class nsIImapService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IIMAPSERVICE_IID; return iid; }

	// As always, you can pass in null for the url listener and the url if you don't require either.....
	// aClientEventQueue is the event queue of the event sinks. We post events into this queue.
	// mscott -- eventually this function will take in the account (identity/incoming server) associated with 
	// the request
	NS_IMETHOD SelectFolder(nsIEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIUrlListener * aUrlListener, 
							nsIMsgStatusFeedback *aMsgStatusFeedback,
                            nsIURI ** aURL) = 0;
	NS_IMETHOD LiteSelectFolder(nsIEventQueue * aClientEventQueue, 
                                nsIMsgFolder * aImapMailFolder,  
                                nsIUrlListener * aUrlListener, 
                                nsIURI ** aURL) = 0;

	NS_IMETHOD FetchMessage(nsIEventQueue * aClientEventQueue, 
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIUrlListener * aUrlListener, nsIURI ** aURL,
							nsISupports *aConsumer,
                            const char *messageIdentifierList,
                            PRBool messageIdsAreUID) = 0;
	NS_IMETHOD Noop(nsIEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener, nsIURI ** aURL) = 0;
	NS_IMETHOD GetHeaders(nsIEventQueue * aClientEventQueue, 
                          nsIMsgFolder * aImapMailFolder, 
                          nsIUrlListener * aUrlListener, nsIURI ** aURL,
                          const char *messageIdentifierList,
                          PRBool messageIdsAreUID) = 0;
	NS_IMETHOD Expunge(nsIEventQueue * aClientEventQueue, 
                       nsIMsgFolder * aImapMailFolder,
                       nsIUrlListener * aUrlListener, nsIURI ** aURL) = 0;
	NS_IMETHOD Biff(nsIEventQueue * aClientEventQueue, 
                    nsIMsgFolder * aImapMailFolder,
                    nsIUrlListener * aUrlListener, nsIURI ** aURL,
                    PRUint32 uidHighWater) = 0;
	NS_IMETHOD DeleteMessages(nsIEventQueue * aClientEventQueue, 
                              nsIMsgFolder * aImapMailFolder, 
                              nsIUrlListener * aUrlListener, nsIURI ** aURL,
                              const char *messageIdentifierList,
                              PRBool messageIdsAreUID) = 0;
	NS_IMETHOD DeleteAllMessages(nsIEventQueue * aClientEventQueue, 
                                 nsIMsgFolder * aImapMailFolder,
                                 nsIUrlListener * aUrlListener, 
                                 nsIURI ** aURL) = 0;
	NS_IMETHOD AddMessageFlags(nsIEventQueue * aClientEventQueue, 
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, nsIURI ** aURL,
                               const char *messageIdentifierList,
                               imapMessageFlagsType flags,
                               PRBool messageIdsAreUID) = 0;
	NS_IMETHOD SubtractMessageFlags(nsIEventQueue * aClientEventQueue, 
                                    nsIMsgFolder * aImapMailFolder, 
                                    nsIUrlListener * aUrlListener, 
                                    nsIURI ** aURL,
                                    const char *messageIdentifierList,
                                    imapMessageFlagsType flags,
                                    PRBool messageIdsAreUID) = 0;
	NS_IMETHOD SetMessageFlags(nsIEventQueue * aClientEventQueue, 
                               nsIMsgFolder * aImapMailFolder, 
                               nsIUrlListener * aUrlListener, nsIURI ** aURL,
                               const char *messageIdentifierList,
                               imapMessageFlagsType flags,
                               PRBool messageIdsAreUID) = 0;

    NS_IMETHOD DiscoverAllFolders(nsIEventQueue* aClientEventQueue,
                                  nsIMsgFolder* aImapMailFolder,
                                  nsIUrlListener* aUrlListener,
                                  nsIURI** aURL) = 0;
    NS_IMETHOD DiscoverAllAndSubscribedFolders(nsIEventQueue*
                                               aClientEventQueue,
                                               nsIMsgFolder* aImapMailFolder,
                                               nsIUrlListener* aUrlListener,
                                               nsIURI** aURL) = 0;
    NS_IMETHOD DiscoverChildren(nsIEventQueue* aClientEventQueue,
                                nsIMsgFolder* aImapMailFolder,
                                nsIUrlListener* aUrlListener,
                                nsIURI** aURL) = 0;
    NS_IMETHOD DiscoverLevelChildren(nsIEventQueue* aClientEventQueue,
                                     nsIMsgFolder* aImapMailFolder,
                                     nsIUrlListener* aUrlListener,
                                     PRInt32 level,
                                     nsIURI** aURL) = 0;
    NS_IMETHOD OnlineMessageCopy(nsIEventQueue* aClientEventQueue,
                                 nsIMsgFolder* aSrcFolder,
                                 const char* messageIds,
                                 nsIMsgFolder* aDstFolder,
                                 PRBool idsAreUids,
                                 PRBool isMove,
                                 nsIUrlListener* aUrlListener,
                                 nsIURI** aURL,
                                 nsISupports* copyState) = 0;
    NS_IMETHOD AppendMessageFromFile(nsIEventQueue* aClientEventQ,
                                     nsIFileSpec* aFileSpec,
                                     nsIMsgFolder* aDstFolder,
                                     const char* messageId, // to replace with
                                     PRBool idsAreUids,
                                     PRBool inSelectedState, // needs to be in
                                     nsIUrlListener* aUrlListener,
                                     nsIURI** aURL,
                                     nsISupports* copyState) = 0;
    NS_IMETHOD MoveFolder(nsIEventQueue* aClientEventQ,
                          nsIMsgFolder* srcFolder,
                          nsIMsgFolder* dstFolder,
                          nsIUrlListener* urlListener,
                          nsIURI** aUrl) = 0;
    NS_IMETHOD RenameLeaf(nsIEventQueue* aClientEventQ,
                          nsIMsgFolder* srcFolder,
                          const char* leafName,
                          nsIUrlListener* urlListener,
                          nsIURI** url) = 0;
    NS_IMETHOD DeleteFolder(nsIEventQueue* aClientEventQ,
                            nsIMsgFolder* aFolder,
                            nsIUrlListener* urlListener,
                            nsIURI** url) = 0;
    NS_IMETHOD CreateFolder(nsIEventQueue* eventQueue,
                            nsIMsgFolder* parent,
                            const char* leafName,
                            nsIUrlListener* urlListener,
                            nsIURI** url) = 0;
};


#endif /* nsIImapService_h___ */
