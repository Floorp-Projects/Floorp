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
#include "nsMsgDBFolder.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"
#include "nsICopyMessageListener.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"
#include "nsIImapIncomingServer.h" // we need this for its IID
#include "nsIMsgParseMailMsgState.h"
#include "nsITransactionManager.h"
#include "nsMsgTxn.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgFilterHitNotify.h"
#include "nsIMsgFilterList.h"

class nsImapMoveCoalescer;


#define FOUR_K 4096

/* b64534f0-3d53-11d3-ac2a-00805f8ac968 */

#define NS_IMAPMAILCOPYSTATE_IID \
{ 0xb64534f0, 0x3d53, 0x11d3, \
    { 0xac, 0x2a, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsImapMailCopyState: public nsISupports
{
public:
    static const nsIID& GetIID()
    {
        static nsIID iid = NS_IMAPMAILCOPYSTATE_IID;
        return iid;
    }
    
    NS_DECL_ISUPPORTS

    nsImapMailCopyState();
    virtual ~nsImapMailCopyState();

    nsCOMPtr<nsISupports> m_srcSupport; // source file spec or folder
    nsCOMPtr<nsISupportsArray> m_messages; // array of source messages
    nsCOMPtr<nsMsgTxn> m_undoMsgTxn; // undo object with this copy operation
    nsCOMPtr<nsIMessage> m_message; // current message to be copied
    nsCOMPtr<nsIMsgCopyServiceListener> m_listener; // listener of this copy
                                                    // operation 
    nsCOMPtr<nsIFileSpec> m_tmpFileSpec; // temp file spec for copy operation

    nsIMsgMessageService* m_msgService; // source folder message service; can
                                        // be Nntp, Mailbox, or Imap
    PRBool m_isMove;             // is a move
    PRBool m_selectedState;      // needs to be in selected state; append msg
    PRBool m_streamCopy;
    PRUint32 m_curIndex; // message index to the message array which we are
                         // copying 
    PRUint32 m_totalCount;// total count of messages we have to do
    char *m_dataBuffer; // temporary buffer for this copy operation
};

class nsImapMailFolder : public nsMsgDBFolder, 
                         public nsIMsgImapMailFolder,
                         public nsIImapMailFolderSink,
                         public nsIImapMessageSink,
                         public nsIImapExtensionSink,
                         public nsIImapMiscellaneousSink,
                         public nsICopyMessageListener,
                         public nsIUrlListener
#ifdef DOING_FILTERS
						 ,public nsIMsgFilterHitNotify
#endif
{
public:
	nsImapMailFolder();
	virtual ~nsImapMailFolder();

	NS_DECL_ISUPPORTS_INHERITED

    // nsICollection methods:
    NS_IMETHOD Enumerate(nsIEnumerator* *result);
    
    // nsIFolder methods:
    NS_IMETHOD GetSubFolders(nsIEnumerator* *result);
    
    // nsIMsgFolder methods:
    NS_IMETHOD AddUnique(nsISupports* element);
    NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
    NS_IMETHOD GetMessages(nsIEnumerator* *result);
    
	NS_IMETHOD CreateSubfolder(const char *folderName);
    
	NS_IMETHOD RemoveSubFolder (nsIMsgFolder *which);
    NS_IMETHOD Compact();
	NS_IMETHOD Delete ();
	NS_IMETHOD Rename (const char *newName);
	NS_IMETHOD Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos);
    
    // this override pulls the value from the db
	NS_IMETHOD GetName(PRUnichar ** name);   // Name of this folder (as presented to user).
	NS_IMETHOD GetPrettyName(PRUnichar ** prettyName);	// Override of the base, for top-level mail folder
    
    NS_IMETHOD BuildFolderURL(char **url);
    
	NS_IMETHOD UpdateSummaryTotals(PRBool force) ;
    
	NS_IMETHOD GetExpungedBytesCount(PRUint32 *count);
	NS_IMETHOD GetDeletable (PRBool *deletable); 
	NS_IMETHOD GetCanCreateChildren (PRBool *canCreateChildren) ;
	NS_IMETHOD GetCanBeRenamed (PRBool *canBeRenamed);
	NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);
    
	NS_IMETHOD GetSizeOnDisk(PRUint32 * size);
    
	NS_IMETHOD GetUsername(char** userName);
	NS_IMETHOD GetHostname(char** hostName);
	NS_IMETHOD UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate);
	NS_IMETHOD RememberPassword(const char *password);
	NS_IMETHOD GetRememberedPassword(char ** password);

	NS_IMETHOD MarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
	NS_IMETHOD MarkAllMessagesRead(void);
    
    virtual nsresult GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo,
                                          nsIMsgDatabase **db);
 	NS_IMETHOD DeleteMessages(nsISupportsArray *messages,
                              nsITransactionManager *txnMgr, PRBool
                              deleteStorage);
    NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, 
                            nsISupportsArray* messages,
                            PRBool isMove, nsITransactionManager* txnMgr,
                            nsIMsgCopyServiceListener* listener);
    NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, 
                               nsIMessage* msgToReplace,
                               PRBool isDraftOrTemplate,
                               nsITransactionManager* txnMgr,
                               nsIMsgCopyServiceListener* listener);
	NS_IMETHOD CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgHdr, nsIMessage
                                         **message);
    NS_IMETHOD GetNewMessages();

    NS_IMETHOD GetPath(nsIFileSpec** aPathName);
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

	//nsICopyMessageListener
	NS_IMETHOD BeginCopy(nsIMessage *message);
	NS_IMETHOD CopyData(nsIInputStream *aIStream, PRInt32 aLength);
	NS_IMETHOD EndCopy(PRBool copySucceeded);

    // nsIUrlListener methods
	NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);

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
    NS_IMETHOD SetCopyResponseUid(nsIImapProtocol* aProtocol,
                                  nsMsgKeyArray* keyArray,
                                  const char* msgIdString,
                                  nsISupports* copyState);
    NS_IMETHOD SetAppendMsgUid(nsIImapProtocol* aProtocol,
                               nsMsgKey aKey,
                               nsISupports* copyState);
    NS_IMETHOD GetMessageId(nsIImapProtocol* aProtocol,
                            nsString2* messageId,
                            nsISupports* copyState);
    
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
                              PRUint32 aMsgId);
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
	NS_IMETHOD LoadNextQueuedUrl(nsIImapProtocol* aProtocol,
                                 nsIImapIncomingServer *aInfo);
    NS_IMETHOD CopyNextStreamMessage(nsIImapProtocol* aProtocol,
                                     nsISupports* copyState);
    NS_IMETHOD SetUrlState(nsIImapProtocol* aProtocol,
                           nsIMsgMailNewsUrl* aUrl,
                           PRBool isRunning,
                           nsresult statusCode);

	NS_IMETHOD MatchName(nsString *name, PRBool *matches);
	// nsIMsgFilterHitNotification method(s)
	NS_IMETHOD ApplyFilterHit(nsIMsgFilter *filter, PRBool *applyMore);

    // overriding nsMsgDBFolder::GetMsgDatabase()
    NS_IMETHOD GetMsgDatabase(nsIMsgDatabase** aMsgDatabase);

	nsresult MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
									   nsIMsgDatabase *sourceDB, 
									   char *destFolder,
									   nsIMsgFilter *filter);
	nsresult StoreImapFlags(imapMessageFlagsType flags, PRBool addFlags, nsMsgKeyArray &msgKeys);
	static nsresult AllocateUidStringFromKeyArray(nsMsgKeyArray &keyArray, nsCString &msgIds);
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

	void SetIMAPDeletedFlag(nsIMsgDatabase *mailDB, const nsMsgKeyArray &msgids, PRBool markDeleted);
	virtual PRBool ShowDeletedMessages();
	virtual PRBool DeleteIsMoveToTrash();
	void ParseUidString(char *uidString, nsMsgKeyArray &keys);
	nsresult GetTrashFolder(nsIMsgFolder **pTrashFolder);

    nsresult AddDirectorySeparator(nsFileSpec &path);
    nsresult CreateDirectoryForFolder(nsFileSpec &path);
	nsresult CreateSubFolders(nsFileSpec &path);
	//Creates a subfolder with the name 'name' and adds it to the list of
    //children. Returns the child as well.
	nsresult AddSubfolder(nsAutoString name, nsIMsgFolder **child);
	nsresult GetDatabase();
	virtual const char *GetIncomingServerType() {return "imap";}

    // Uber message copy service
    nsresult CopyMessages2(nsIMsgFolder* srcFolder,
                           nsISupportsArray* messages,
                           PRBool isMove,
                           nsITransactionManager* txnMgr,
                           nsIMsgCopyServiceListener* listener);
    nsresult CopyStreamMessage(nsIMessage* message, nsIMsgFolder* dstFolder,
                               PRBool isMove);
    nsresult InitCopyState(nsISupports* srcSupport, 
                           nsISupportsArray* messages,
                           PRBool isMove,
                           PRBool selectedState,
                           nsIMsgCopyServiceListener* listener);
    void ClearCopyState(nsresult exitCode);
    nsresult SetTransactionManager(nsITransactionManager* txnMgr);
    nsresult BuildIdsAndKeyArray(nsISupportsArray* messages,
                                 nsCString& msgIds, nsMsgKeyArray& keyArray);

	nsresult GetMessageHeader(nsIMsgDBHdr ** aMsgHdr);

    nsNativeFileSpec *m_pathName;
    PRBool m_initialized;
    PRBool m_haveDiscoverAllFolders;
    PRBool m_haveReadNameFromDB;
	nsCOMPtr<nsIMsgParseMailMsgState> m_msgParser;
	nsCOMPtr<nsIMsgFilterList> m_filterList;
	PRBool				m_msgMovedByFilter;
	nsImapMoveCoalescer *m_moveCoalescer;
	nsMsgKey			m_curMsgUid;
	PRInt32			m_nextMessageByteLength;
    nsCOMPtr<nsIEventQueue> m_eventQueue;
    PRBool m_urlRunning;

	// part of temporary libmime converstion trick......these should go away once MIME uses a new stream
	// converter interface...
	nsCOMPtr<nsIOutputStream> m_tempMessageStream;
	nsFileSpec m_tempMessageFile;

    // *** jt - undo move/copy trasaction support
    nsCOMPtr<nsITransactionManager> m_transactionManager;
    nsCOMPtr<nsMsgTxn> m_pendingUndoTxn;
    nsCOMPtr<nsImapMailCopyState> m_copyState;
    PRMonitor *m_appendMsgMonitor;
};

#endif
