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

#ifndef nsIImapUrl_h___
#define nsIImapUrl_h___

#include "nscore.h"
#include "MailNewsTypes.h"
#include "nsIMsgMailNewsUrl.h"

#include "nsISupports.h"
#include "nsFileSpec.h"

/* include all of our event sink interfaces */
#include "nsIImapLog.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"

/* 21A89610-DC0D-11d2-806C-006008128C4E */

#define NS_IIMAPURL_IID                     \
{ 0x21a89610, 0xdc0d, 0x11d2,                  \
    { 0x80, 0x6c, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

/* 21A89611-DC0D-11d2-806C-006008128C4E */

#define NS_IMAPURL_CID                      \
{ 0x21a89611, 0xdc0d, 0x11d2,                  \
    { 0x80, 0x6c, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }

#define IMAP_PORT 143

class nsIMsgIncomingServer; 

class nsIImapUrl : public nsIMsgMailNewsUrl
{
public:
  static const nsIID& GetIID() {
    static nsIID iid = NS_IIMAPURL_IID;
    return iid;
  }

	// mscott - we have a basic set of imap url actions. These actions are nsImapActions.
    // Certain actions require us to be in the authenticated state and others require us to
    // be in the selected state. nsImapState is used to store the state the url needs to
    // be in. You'll later see us refer to the imap url state in the imap protocol when we
    // are processing the current url. Don't confuse nsImapState with the generic url state
    // used to keep track of whether the url is running or not...

	typedef enum {
		nsImapAuthenticatedState = 0,
		nsImapSelectedState
	} nsImapState;

	typedef enum {
		nsImapActionSendText = 0,      // a state used for testing purposes to send raw url text straight to the server....
		// nsImapAuthenticatedStateUrl urls
		// since the following url actions require us to be in the authenticated
		// state, the high bit is left blank....
		nsImapTest								= 0x00000001,
		nsImapExpungeFolder						= 0x00000004,
		nsImapCreateFolder						= 0x00000005,
		nsImapDeleteFolder						= 0x00000006,
		nsImapRenameFolder						= 0x00000007,
		nsImapMoveFolderHierarchy				= 0x00000008,
		nsImapLsubFolders						= 0x00000009,
		nsImapGetMailAccountUrl					= 0x0000000A,
		nsImapDiscoverChildrenUrl				= 0x0000000B,
		nsImapDiscoverLevelChildrenUrl			= 0x0000000C,
		nsImapDiscoverAllBoxesUrl				= 0x0000000D,
		nsImapDiscoverAllAndSubscribedBoxesUrl	= 0x0000000E,
		nsImapAppendMsgFromFile					= 0x0000000F,
		nsImapSubscribe							= 0x00000010,
		nsImapUnsubscribe						= 0x00000011,
		nsImapRefreshACL						= 0x00000012,
		nsImapRefreshAllACLs					= 0x00000013,
		nsImapListFolder						= 0x00000014,
		nsImapUpgradeToSubscription				= 0x00000015,
		nsImapFolderStatus						= 0x00000016,
		nsImapRefreshFolderUrls					= 0x00000017,
		// it's okay to add more imap actions that require us to 
		// be in the authenticated state here without renumbering
		// the imap selected state url actions. just make sure you don't
		// set the high bit...
        
		// nsImapSelectedState urls. Note, the high bit is always set for
		// imap actions which require us to be in the selected state
		nsImapSelectFolder						= 0x10000002,
		nsImapLiteSelectFolder					= 0x10000003,
		nsImapMsgFetch							= 0x10000018,
		nsImapMsgHeader							= 0x10000019,
		nsImapSearch							= 0x1000001A,
		nsImapDeleteMsg							= 0x1000001B,
		nsImapDeleteAllMsgs						= 0x1000001C,
		nsImapAddMsgFlags						= 0x1000001D,
		nsImapSubtractMsgFlags					= 0x1000001E,
		nsImapSetMsgFlags						= 0x1000001F,
		nsImapOnlineCopy						= 0x10000020,
		nsImapOnlineMove						= 0x10000021,
		nsImapOnlineToOfflineCopy				= 0x10000022,
		nsImapOnlineToOfflineMove				= 0x10000023,
		nsImapOfflineToOnlineMove				= 0x10000024,
		nsImapBiff								= 0x10000025,
		nsImapSelectNoopFolder					= 0x10000026
	} nsImapAction;

	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the imap specific event sinks to bind to to your url
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD SetServer(nsIMsgIncomingServer * aIncomingServer) = 0;
	NS_IMETHOD GetServer(nsIMsgIncomingServer ** aIncomingServer) = 0;

	NS_IMETHOD GetImapLog(nsIImapLog ** aImapLog) = 0;
	NS_IMETHOD SetImapLog(nsIImapLog  * aImapLog) = 0;

    NS_IMETHOD GetImapMailFolderSink(nsIImapMailFolderSink** aImapMailFolderSink) = 0;
    NS_IMETHOD SetImapMailFolderSink(nsIImapMailFolderSink* aImapMailFolderSink) = 0;

    NS_IMETHOD GetImapMessageSink(nsIImapMessageSink** aImapMessageSink) = 0;
    NS_IMETHOD SetImapMessageSink(nsIImapMessageSink* aImapMessageSink) = 0;

    NS_IMETHOD GetImapExtensionSink(nsIImapExtensionSink** aImapExtensionSink) = 0;
    NS_IMETHOD SetImapExtensionSink(nsIImapExtensionSink* aImapExtensionSink) = 0;

    NS_IMETHOD GetImapMiscellaneousSink(nsIImapMiscellaneousSink** aImapMiscellaneousSink) = 0;
    NS_IMETHOD SetImapMiscellaneousSink(nsIImapMiscellaneousSink* aImapMiscellaneousSink) = 0;
    
	/////////////////////////////////////////////////////////////////////////////// 
	// Getters and Setters for the imap url state
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD GetImapAction(nsImapAction * aImapAction) = 0;
	NS_IMETHOD SetImapAction(nsImapAction aImapAction) = 0;

	NS_IMETHOD GetRequiredImapState(nsImapState * aImapUrlState) = 0;

	NS_IMETHOD GetImapPartToFetch(char **resultPart) = 0;
	NS_IMETHOD AllocateCanonicalPath(const char *serverPath, char onlineDelimiter, char **allocatedPath ) = 0;
	NS_IMETHOD AllocateServerPath(const char * aCanonicalPath, char aOnlineDelimiter, char ** aAllocatedPath) = 0;
	NS_IMETHOD CreateServerSourceFolderPathString(char **result) = 0;
	NS_IMETHOD CreateCanonicalSourceFolderPathString(char **result) = 0;
	NS_IMETHOD CreateServerDestinationFolderPathString(char **result) = 0;

	NS_IMETHOD AddOnlineDirectoryIfNecessary(const char *onlineMailboxName, char ** directory) = 0;

	NS_IMETHOD	CreateSearchCriteriaString(nsString2 *aResult) = 0;
	NS_IMETHOD	CreateListOfMessageIdsString(nsString2 *result) = 0;
	NS_IMETHOD	MessageIdsAreUids(PRBool *result) = 0;
	NS_IMETHOD	GetMsgFlags(imapMessageFlagsType *result) = 0;	// kAddMsgFlags or kSubtractMsgFlags only

	NS_IMETHOD	SetAllowContentChange(PRBool allowContentChange) = 0;
	NS_IMETHOD  GetAllowContentChange(PRBool *results) = 0;

};

#endif /* nsIImapUrl_h___ */
