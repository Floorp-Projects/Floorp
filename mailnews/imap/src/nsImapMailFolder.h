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
#ifndef nsImapMailFolder_h__
#define nsImapMailFolder_h__

#include "nsImapCore.h"
#include "nsMsgFolder.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"
#include "nsIDBChangeListener.h"
#include "nsICopyMessageListener.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"

/* fa32d000-f6a0-11d2-af8d-001083002da8 */
#define NS_IMAPRESOURCE_CID \
{ 0xfa32d000, 0xf6a0, 0x11d2, \
    { 0xaf, 0x8d, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

class nsParseMailMessageState;

class nsImapMailFolder : public nsMsgFolder, 
                         public nsIMsgImapMailFolder,
                         public nsIImapMailFolderSink,
                         public nsIImapMessageSink,
                         public nsIImapExtensionSink,
                         public nsIImapMiscellaneousSink,
                         public nsIDBChangeListener,
                         public nsICopyMessageListener,
                         public nsIUrlListener
{
public:
	nsImapMailFolder();
	virtual ~nsImapMailFolder();

	NS_DECL_ISUPPORTS_INHERITED
    // nsIMsgImapMailFolder methods
    NS_IMETHOD GetPathName(nsNativeFileSpec& aPathName);

    // nsICollection methods:
    NS_IMETHOD Enumerate(nsIEnumerator* *result);
    
    // nsIFolder methods:
    NS_IMETHOD GetSubFolders(nsIEnumerator* *result);
    
    // nsIMsgFolder methods:
    NS_IMETHOD AddUnique(nsISupports* element);
    NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
    NS_IMETHOD GetMessages(nsIEnumerator* *result);
	NS_IMETHOD GetThreads(nsIEnumerator** threadEnumerator);
	NS_IMETHOD GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread);

    
	NS_IMETHOD CreateSubfolder(const char *folderName);
    
	NS_IMETHOD RemoveSubFolder (nsIMsgFolder *which);
	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);
    
    NS_IMETHOD GetChildNamed(nsString& name, nsISupports ** aChild);
    
    // this override pulls the value from the db
	NS_IMETHOD GetName(char ** name);   // Name of this folder (as presented to user).
	NS_IMETHOD GetPrettyName(nsString& prettyName);	// Override of the base, for top-level mail folder
    
    NS_IMETHOD BuildFolderURL(char **url);
    
	NS_IMETHOD UpdateSummaryTotals() ;
    
	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);
    
	NS_IMETHOD GetSizeOnDisk(PRUint32 size);
    
	NS_IMETHOD GetUsersName(char** userName);
	NS_IMETHOD GetHostName(char** hostName);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);
    
    virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo,
                                          nsIMsgDatabase **db);
 	NS_IMETHOD DeleteMessage(nsIMessage *message);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgHdr, nsIMessage **message);

    // nsIImapMailFolderSink methods
    // Tell mail master about a discovered imap mailbox
    NS_IMETHOD PossibleImapMailbox(nsIImapProtocol* aProtocol,
                                   mailbox_spec* aSpec);
    NS_IMETHOD MailboxDiscoveryDone(nsIImapProtocol* aProtocol);
    // Tell mail master about the newly selected mailbox
    NS_IMETHOD UpdateImapMailboxInfo(nsIImapProtocol* aProtocol,
                                     mailbox_spec* aSpec);
    NS_IMETHOD UpdateImapMailboxStatus(nsIImapProtocol* aProtocol,
                                       mailbox_spec* aSpec);
    NS_IMETHOD ChildDiscoverySucceeded(nsIImapProtocol* aProtocol);
    NS_IMETHOD OnlineFolderDelete(nsIImapProtocol* aProtocol,
                                  const char* folderName);
    NS_IMETHOD OnlineFolderCreateFailed(nsIImapProtocol* aProtocol,
                                        const char* folderName);
    NS_IMETHOD OnlineFolderRename(nsIImapProtocol* aProtocol,
                                  folder_rename_struct* aStruct);
    NS_IMETHOD SubscribeUpgradeFinished(nsIImapProtocol* aProtocol,
                        EIMAPSubscriptionUpgradeState* aState);
    NS_IMETHOD PromptUserForSubscribeUpdatePath(nsIImapProtocol* aProtocol,
                                                PRBool* aBool);
    NS_IMETHOD FolderIsNoSelect(nsIImapProtocol* aProtocol,
                                FolderQueryInfo* aInfo);

    NS_IMETHOD SetupHeaderParseStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);

    NS_IMETHOD ParseAdoptedHeaderLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndHeaderParseStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortHeaderParseStream(nsIImapProtocol* aProtocol);
    
    // nsIImapMessageSink methods
    NS_IMETHOD SetupMsgWriteStream(nsIImapProtocol* aProtocol,
                                   StreamInfo* aStreamInfo);

    NS_IMETHOD ParseAdoptedMsgLine(nsIImapProtocol* aProtocol,
                                   msg_line_info* aMsgLineInfo);
    
    NS_IMETHOD NormalEndMsgWriteStream(nsIImapProtocol* aProtocol);
    
    NS_IMETHOD AbortMsgWriteStream(nsIImapProtocol* aProtocol);
    
    // message move/copy related methods
    NS_IMETHOD OnlineCopyReport(nsIImapProtocol* aProtocol,
                                ImapOnlineCopyState* aCopyState);
    NS_IMETHOD BeginMessageUpload(nsIImapProtocol* aProtocol);
    NS_IMETHOD UploadMessageFile(nsIImapProtocol* aProtocol,
                                 UploadMessageInfo* aMsgInfo);

    // message flags operation
    NS_IMETHOD NotifyMessageFlags(nsIImapProtocol* aProtocol,
                                  FlagsKeyStruct* aKeyStruct);

    NS_IMETHOD NotifyMessageDeleted(nsIImapProtocol* aProtocol,
                                    delete_message_struct* aStruct);
    NS_IMETHOD GetMessageSizeFromDB(nsIImapProtocol* aProtocol,
                                    MessageSizeInfo* sizeInfo);

	//nsIDBChangeListener
	NS_IMETHOD OnKeyChange(nsMsgKey aKeyChanged, int32 aFlags, 
                         nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnKeyDeleted(nsMsgKey aKeyChanged, int32 aFlags, 
                          nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnKeyAdded(nsMsgKey aKeyChanged, int32 aFlags, 
                        nsIDBChangeListener * aInstigator);
	NS_IMETHOD OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator);

	//nsICopyMessageListener
	NS_IMETHOD BeginCopy(nsIMessage *message);
	NS_IMETHOD CopyData(nsIInputStream *aIStream, PRInt32 aLength);
	NS_IMETHOD EndCopy(PRBool copySucceeded);

    // nsIUrlListener methods
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode);

    // nsIImapExtensionSink methods
    NS_IMETHOD SetUserAuthenticated(nsIImapProtocol* aProtocol,
                                    PRBool aBool);
    NS_IMETHOD SetMailServerUrls(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD SetMailAccountUrl(nsIImapProtocol* aProtocol,
                                 const char* hostName);
    NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
                                 nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
                               nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
                                   nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                         nsIMAPACLRightsInfo* aclRights);
    NS_IMETHOD SetFolderAdminURL(nsIImapProtocol* aProtocol,
                                 FolderQueryInfo* aInfo);
    
    // nsIImapMiscellaneousSink methods
	NS_IMETHOD AddSearchResult(nsIImapProtocol* aProtocol, 
                               const char* searchHitLine);
	NS_IMETHOD GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
	NS_IMETHOD GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                 GenericInfo* aInfo);
    NS_IMETHOD GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                        PRBool* aBool);
	NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol);
	NS_IMETHOD UpdateSecurityStatus(nsIImapProtocol* aProtocol);
	// ****
	NS_IMETHOD FinishImapConnection(nsIImapProtocol* aProtocol);
	NS_IMETHOD SetImapHostPassword(nsIImapProtocol* aProtocol,
                                   GenericInfo* aInfo);
	NS_IMETHOD GetPasswordForUser(nsIImapProtocol* aProtocol,
                                  const char* userName);
	NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                     nsMsgBiffState biffState);
	NS_IMETHOD GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                    uid_validity_info* aInfo);
	NS_IMETHOD LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                     PRUint32 uidValidity);
	NS_IMETHOD FEAlert(nsIImapProtocol* aProtocol,
                       const char* aString);
	NS_IMETHOD FEAlertFromServer(nsIImapProtocol* aProtocol,
                                 const char* aString);
	NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
                              const char* statusMsg);
	NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
                               ProgressInfo* aInfo);
	NS_IMETHOD PastPasswordCheck(nsIImapProtocol* aProtocol);
	NS_IMETHOD CommitNamespaces(nsIImapProtocol* aProtocol,
                                const char* hostName);
	NS_IMETHOD CommitCapabilityForHost(nsIImapProtocol* aProtocol,
                                       const char* hostName);
	NS_IMETHOD TunnelOutStream(nsIImapProtocol* aProtocol,
                               msg_line_info* aInfo);
	NS_IMETHOD ProcessTunnel(nsIImapProtocol* aProtocol,
                             TunnelInfo *aInfo);

protected:
    // Helper methods
	void FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray
                       &keysToFetch, nsImapFlagAndUidState *flagState);
	void FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray
                          &keysToFetch, nsImapFlagAndUidState *flagState);
	void PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const
                                     nsMsgKeyArray &keysToFetch, 
                                     mailbox_spec *boxSpec);
	void TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMsgDBHdr *tweakMe);
    nsresult AddDirectorySeparator(nsFileSpec &path);
    nsresult CreateDirectoryForFolder(nsFileSpec &path);
	nsresult CreateSubFolders(nsFileSpec &path);
	//Creates a subfolder with the name 'name' and adds it to the list of
    //children. Returns the child as well.
	nsresult AddSubfolder(nsAutoString name, nsIMsgFolder **child);

	nsresult GetDatabase();

    nsNativeFileSpec m_pathName;
	nsIMsgDatabase* m_mailDatabase;
    PRBool m_initialized;
    PRBool m_haveReadNameFromDB;
	nsParseMailMessageState *m_msgParser;
	nsMsgKey			m_curMsgUid;
	PRInt32			m_nextMessageByteLength;
    PLEventQueue* m_eventQueue;
    PRBool m_urlRunning;
};

#endif
