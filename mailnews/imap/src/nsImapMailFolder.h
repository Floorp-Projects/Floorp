/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lorenzo Colitti <lorenzo@colitti.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIMsgFilterPlugin.h"
#include "prmon.h"
#include "nsIEventQueue.h"
#include "nsIMsgImapMailFolder.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIImapMailFolderSink.h"
#include "nsIImapServerSink.h"
#include "nsIMsgFilterPlugin.h"
class nsImapMoveCoalescer;
class nsHashtable;
class nsHashKey;
#define COPY_BUFFER_SIZE 16384

/* b64534f0-3d53-11d3-ac2a-00805f8ac968 */

#define NS_IMAPMAILCOPYSTATE_IID \
{ 0xb64534f0, 0x3d53, 0x11d3, \
    { 0xac, 0x2a, 0x00, 0x80, 0x5f, 0x8a, 0xc9, 0x68 } }

class nsImapMailCopyState: public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMAPMAILCOPYSTATE_IID)
    
    NS_DECL_ISUPPORTS

    nsImapMailCopyState();
    virtual ~nsImapMailCopyState();

    nsCOMPtr<nsISupports> m_srcSupport; // source file spec or folder
    nsCOMPtr<nsISupportsArray> m_messages; // array of source messages
    nsCOMPtr<nsMsgTxn> m_undoMsgTxn; // undo object with this copy operation
    nsCOMPtr<nsIMsgDBHdr> m_message; // current message to be copied
    nsCOMPtr<nsIMsgCopyServiceListener> m_listener; // listener of this copy
                                                    // operation 
    nsCOMPtr<nsIFileSpec> m_tmpFileSpec; // temp file spec for copy operation
    nsCOMPtr<nsIMsgWindow> m_msgWindow; // msg window for copy operation

    nsCOMPtr<nsIMsgMessageService> m_msgService; // source folder message service; can
                                        // be Nntp, Mailbox, or Imap
    PRBool m_isMove;             // is a move
    PRBool m_selectedState;      // needs to be in selected state; append msg
    PRBool m_isCrossServerOp; // are we copying between imap servers?
    PRUint32 m_curIndex; // message index to the message array which we are
                         // copying 
    PRUint32 m_totalCount;// total count of messages we have to do
    PRUint32 m_unreadCount; // num unread messages we're moving
    PRBool m_streamCopy;
    char *m_dataBuffer; // temporary buffer for this copy operation
    PRUint32 m_dataBufferSize;
    PRUint32 m_leftOver;
    PRBool m_allowUndo;
    PRBool m_eatLF;
};

// ACLs for this folder.
// Generally, we will try to always query this class when performing
// an operation on the folder.
// If the server doesn't support ACLs, none of this data will be filled in.
// Therefore, we can assume that if we look up ourselves and don't find
// any info (and also look up "anyone") then we have full rights, that is, ACLs don't exist.
class nsImapMailFolder;

#define IMAP_ACL_READ_FLAG 0x0000001 /* SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder */
#define IMAP_ACL_STORE_SEEN_FLAG 0x0000002 /* STORE SEEN flag */
#define IMAP_ACL_WRITE_FLAG 0x0000004 /* STORE flags other than SEEN and DELETED */
#define IMAP_ACL_INSERT_FLAG 0x0000008 /* APPEND, COPY into folder */
#define IMAP_ACL_POST_FLAG 0x0000010 /* Can I send mail to the submission address for folder? */
#define IMAP_ACL_CREATE_SUBFOLDER_FLAG 0x0000020 /* Can I CREATE a subfolder of this folder? */
#define IMAP_ACL_DELETE_FLAG 0x0000040 /* STORE DELETED flag, perform EXPUNGE */
#define IMAP_ACL_ADMINISTER_FLAG 0x0000080 /* perform SETACL */
#define IMAP_ACL_RETRIEVED_FLAG 0x0000100 /* ACL info for this folder has been initialized */

class nsMsgIMAPFolderACL
{
public:
  nsMsgIMAPFolderACL(nsImapMailFolder *folder);
  ~nsMsgIMAPFolderACL();
  
  PRBool SetFolderRightsForUser(const char *userName, const char *rights);
  
public:
  
  // generic for any user, although we might not use them in
  // DO NOT use these for looking up information about the currently authenticated user.
  // (There are some different checks and defaults we do).
  // Instead, use the functions below, GetICan....()
  PRBool GetCanUserLookupFolder(const char *userName);		// Is folder visible to LIST/LSUB?
  PRBool GetCanUserReadFolder(const char *userName);			// SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder?
  PRBool GetCanUserStoreSeenInFolder(const char *userName);	// STORE SEEN flag?
  PRBool GetCanUserWriteFolder(const char *userName);		// STORE flags other than SEEN and DELETED?
  PRBool GetCanUserInsertInFolder(const char *userName);		// APPEND, COPY into folder?
  PRBool GetCanUserPostToFolder(const char *userName);		// Can I send mail to the submission address for folder?
  PRBool GetCanUserCreateSubfolder(const char *userName);	// Can I CREATE a subfolder of this folder?
  PRBool GetCanUserDeleteInFolder(const char *userName);		// STORE DELETED flag, perform EXPUNGE?
  PRBool GetCanUserAdministerFolder(const char *userName);	// perform SETACL?
  
  // Functions to find out rights for the currently authenticated user.
  
  PRBool GetCanILookupFolder();		// Is folder visible to LIST/LSUB?
  PRBool GetCanIReadFolder();		// SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder?
  PRBool GetCanIStoreSeenInFolder();	// STORE SEEN flag?
  PRBool GetCanIWriteFolder();		// STORE flags other than SEEN and DELETED?
  PRBool GetCanIInsertInFolder();	// APPEND, COPY into folder?
  PRBool GetCanIPostToFolder();		// Can I send mail to the submission address for folder?
  PRBool GetCanICreateSubfolder();	// Can I CREATE a subfolder of this folder?
  PRBool GetCanIDeleteInFolder();	// STORE DELETED flag, perform EXPUNGE?
  PRBool GetCanIAdministerFolder();	// perform SETACL?
  
  PRBool GetDoIHaveFullRightsForFolder();	// Returns TRUE if I have full rights on this folder (all of the above return TRUE)
  
  PRBool GetIsFolderShared();		// We use this to see if the ACLs think a folder is shared or not.
  // We will define "Shared" in 5.0 to mean:
  // At least one user other than the currently authenticated user has at least one
  // explicitly-listed ACL right on that folder.
  
  // Returns a newly allocated string describing these rights
  nsresult CreateACLRightsString(PRUnichar **rightsString);
  
protected:
  const char *GetRightsStringForUser(const char *userName);
  PRBool GetFlagSetInRightsForUser(const char *userName, char flag, PRBool defaultIfNotFound);
  void BuildInitialACLFromCache();
  void UpdateACLCache();
  static PRBool PR_CALLBACK FreeHashRights(nsHashKey *aKey, void *aData, void *closure);
  
protected:
  nsHashtable    *m_rightsHash;	// Hash table, mapping username strings to rights strings.
  nsImapMailFolder *m_folder;
  PRInt32        m_aclCount;
  
};



class nsImapMailFolder :  public nsMsgDBFolder, 
                          public nsIMsgImapMailFolder,
                          public nsIImapMailFolderSink,
                          public nsIImapMessageSink,
                          public nsIImapExtensionSink,
                          public nsIImapMiscellaneousSink,
                          public nsICopyMessageListener,
                          public nsIMsgFilterHitNotify,
                          public nsIJunkMailClassificationListener
{
public:
  nsImapMailFolder();
  virtual ~nsImapMailFolder();
  
  NS_DECL_ISUPPORTS_INHERITED
    
  // nsICollection methods
  NS_IMETHOD Enumerate(nsIEnumerator **result);
  
  // nsIMsgFolder methods:
  NS_IMETHOD GetSubFolders(nsIEnumerator* *result);
  
  NS_IMETHOD GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result);
  NS_IMETHOD UpdateFolder(nsIMsgWindow *aWindow);
  
  NS_IMETHOD CreateSubfolder(const PRUnichar *folderName,nsIMsgWindow *msgWindow );
  NS_IMETHOD AddSubfolder(const nsAString& aName, nsIMsgFolder** aChild);
  NS_IMETHOD AddSubfolderWithPath(nsAString& name, nsIFileSpec *dbPath, nsIMsgFolder **child);
  NS_IMETHODIMP CreateStorageIfMissing(nsIUrlListener* urlListener);
  
  NS_IMETHOD Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow);
  NS_IMETHOD CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray);
  NS_IMETHOD EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener);
  NS_IMETHOD Delete ();
  NS_IMETHOD Rename (const PRUnichar *newName, nsIMsgWindow *msgWindow);
  NS_IMETHOD RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder);
  NS_IMETHOD GetNoSelect(PRBool *aResult);
  
  NS_IMETHOD GetPrettyName(PRUnichar ** prettyName);	// Override of the base, for top-level mail folder
  
  NS_IMETHOD GetFolderURL(char **url);
  
  NS_IMETHOD UpdateSummaryTotals(PRBool force) ;
  
  NS_IMETHOD GetDeletable (PRBool *deletable); 
  NS_IMETHOD GetRequiresCleanup(PRBool *requiresCleanup);
  
  NS_IMETHOD GetSizeOnDisk(PRUint32 * size);
  
  NS_IMETHOD GetCanCreateSubfolders(PRBool *aResult);
  NS_IMETHOD GetCanSubscribe(PRBool *aResult);	
  
  NS_IMETHOD AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag);
  NS_IMETHOD MarkMessagesRead(nsISupportsArray *messages, PRBool markRead);
  NS_IMETHOD MarkAllMessagesRead(void);
  NS_IMETHOD MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged);
  NS_IMETHOD MarkThreadRead(nsIMsgThread *thread);
  NS_IMETHOD SetLabelForMessages(nsISupportsArray *aMessages, nsMsgLabelValue aLabel);
  
  NS_IMETHOD DeleteSubFolders(nsISupportsArray *folders, nsIMsgWindow *msgWindow);
  NS_IMETHOD ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element);
  NS_IMETHOD WriteToFolderCacheElem(nsIMsgFolderCacheElement *element);
  
  NS_IMETHOD GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo,
                                  nsIMsgDatabase **db);
  NS_IMETHOD DeleteMessages(nsISupportsArray *messages,
                            nsIMsgWindow *msgWindow, PRBool
                            deleteStorage, PRBool isMove,
                            nsIMsgCopyServiceListener* listener, PRBool allowUndo);
  NS_IMETHOD CopyMessages(nsIMsgFolder *srcFolder, 
                          nsISupportsArray* messages,
                          PRBool isMove, nsIMsgWindow *msgWindow,
                          nsIMsgCopyServiceListener* listener, PRBool isFolder, 
                          PRBool allowUndo);
  NS_IMETHOD CopyFolder(nsIMsgFolder *srcFolder, PRBool isMove, nsIMsgWindow *msgWindow,
                        nsIMsgCopyServiceListener* listener);
  NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec, 
                              nsIMsgDBHdr* msgToReplace,
                              PRBool isDraftOrTemplate,
                              nsIMsgWindow *msgWindow,
                              nsIMsgCopyServiceListener* listener);
  NS_IMETHOD GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener);
  
  NS_IMETHOD GetPath(nsIFileSpec** aPathName);
  NS_IMETHOD SetPath(nsIFileSpec * aPath);
  
  NS_IMETHOD Shutdown(PRBool shutdownChildren);
  
  NS_IMETHOD DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *msgWindow);
  
  NS_IMETHOD DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow);
  NS_IMETHOD GetCanFileMessages(PRBool *aCanFileMessages);
  NS_IMETHOD GetCanDeleteMessages(PRBool *aCanDeleteMessages);
  // nsIMsgImapMailFolder methods
  NS_DECL_NSIMSGIMAPMAILFOLDER
          
  // nsIImapMailFolderSink methods
  NS_DECL_NSIIMAPMAILFOLDERSINK
  
  // nsIImapMessageSink methods
  NS_DECL_NSIIMAPMESSAGESINK
  
  //nsICopyMessageListener
  NS_DECL_NSICOPYMESSAGELISTENER
  
  NS_DECL_NSIREQUESTOBSERVER
  
  // nsIUrlListener methods
  NS_IMETHOD OnStartRunningUrl(nsIURI * aUrl);
  NS_IMETHOD OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);
  
  // nsIImapExtensionSink methods
  NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
                                nsIMAPACLRightsInfo* aclRights);
  
  NS_IMETHOD SetCopyResponseUid(nsIImapProtocol* aProtocol,
                                nsMsgKeyArray* keyArray,
                                const char* msgIdString,
                                nsIImapUrl * aUrl);
  NS_IMETHOD SetAppendMsgUid(nsIImapProtocol* aProtocol,
                              nsMsgKey aKey,
                              nsIImapUrl * aUrl);
  NS_IMETHOD GetMessageId(nsIImapProtocol* aProtocol,
                          nsCString* messageId,
                          nsIImapUrl * aUrl);
  
  // nsIImapMiscellaneousSink methods
  NS_IMETHOD AddSearchResult(nsIImapProtocol* aProtocol, 
                              const char* searchHitLine);
  NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol);
  // ****
  NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                    nsMsgBiffState biffState);
  NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
                            PRUint32 aMsgId, const PRUnichar *extraInfo);
  NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
                              ProgressInfo* aInfo);
  NS_IMETHOD MatchName(nsString *name, PRBool *matches);
  
  NS_DECL_NSIMSGFILTERHITNOTIFY
  NS_DECL_NSIJUNKMAILCLASSIFICATIONLISTENER
  
  NS_IMETHOD IsCommandEnabled(const char *command, PRBool *result);
  NS_IMETHOD SetFilterList(nsIMsgFilterList *aMsgFilterList);
        
  nsresult MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
                                  nsIMsgDatabase *sourceDB, 
                                  const nsACString& destFolder,
                                  nsIMsgFilter *filter,
                                  nsIMsgWindow *msgWindow);
  virtual nsresult SpamFilterClassifyMessage(const char *aURI, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin);
  virtual nsresult SpamFilterClassifyMessages(const char **aURIArray, PRUint32 aURICount, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin);
  
  static nsresult  AllocateUidStringFromKeys(nsMsgKey *keys, PRUint32 numKeys, nsCString &msgIds);

  // these might end up as an nsIImapMailFolder attribute.
  nsresult SetSupportedUserFlags(PRUint32 userFlags);
  nsresult GetSupportedUserFlags(PRUint32 *userFlags);
protected:
  // Helper methods
  
  void FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray
    &keysToFetch, nsIImapFlagAndUidState *flagState);
  void FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray
    &keysToFetch, nsIImapFlagAndUidState *flagState);
  void PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const
    nsMsgKeyArray &keysToFetch, 
    nsIMailboxSpec *boxSpec);
  void TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMsgDBHdr *tweakMe);
  
  nsresult SyncFlags(nsIImapFlagAndUidState *flagState);
  nsresult HandleCustomFlags(nsMsgKey uidOfMessage, nsIMsgDBHdr *dbHdr, nsXPIDLCString &keywords);
  nsresult NotifyMessageFlagsFromHdr(nsIMsgDBHdr *dbHdr, nsMsgKey msgKey, PRUint32 flags);
  
  nsresult SetupHeaderParseStream(PRUint32 size, const char *content_type, nsIMailboxSpec *boxSpec);
  nsresult  ParseAdoptedHeaderLine(const char *messageLine, PRUint32 msgKey);
  nsresult  NormalEndHeaderParseStream(nsIImapProtocol *aProtocol);
  
  void EndOfflineDownload();

  nsresult MarkMessagesImapDeleted(nsMsgKeyArray *keyArray, PRBool deleted, nsIMsgDatabase *db);
  
  void UpdatePendingCounts();
  void SetIMAPDeletedFlag(nsIMsgDatabase *mailDB, const nsMsgKeyArray &msgids, PRBool markDeleted);
  virtual PRBool ShowDeletedMessages();
  virtual PRBool DeleteIsMoveToTrash();
  nsresult GetFolder(const char *name, nsIMsgFolder **pFolder);
  nsresult GetTrashFolder(nsIMsgFolder **pTrashFolder);
  PRBool TrashOrDescendentOfTrash(nsIMsgFolder* folder);
  nsresult GetServerKey(char **serverKey);
  nsresult GetImapIncomingServer(nsIImapIncomingServer **aImapIncomingServer);
  
  nsresult DisplayStatusMsg(nsIImapUrl *aImapUrl, const PRUnichar *msg);
  
  //nsresult RenameLocal(const char *newName);
  nsresult AddDirectorySeparator(nsFileSpec &path);
  nsresult CreateDirectoryForFolder(nsFileSpec &path);
  nsresult CreateSubFolders(nsFileSpec &path);
  nsresult GetDatabase(nsIMsgWindow *aMsgWindow);
  virtual const char *GetIncomingServerType() {return "imap";}
  
  nsresult        GetFolderOwnerUserName(char **userName);
  nsresult        GetOwnersOnlineFolderName(char **onlineName);
  nsIMAPNamespace *GetNamespaceForFolder();
  void            SetNamespaceForFolder(nsIMAPNamespace *ns);
  
  nsresult GetServerAdminUrl(char **aAdminUrl);
  nsMsgIMAPFolderACL * GetFolderACL();
  nsresult CreateACLRightsStringForFolder(PRUnichar **rightsString);
  
  nsresult GetBodysToDownload(nsMsgKeyArray *keysOfMessagesToDownload);
  // Uber message copy service
  nsresult CopyMessagesWithStream(nsIMsgFolder* srcFolder,
                                    nsISupportsArray* messages,
                                    PRBool isMove,
                                    PRBool isCrossServerOp,
                                    nsIMsgWindow *msgWindow,
                                    nsIMsgCopyServiceListener* listener, PRBool allowUndo);
  nsresult CopyStreamMessage(nsIMsgDBHdr* message, nsIMsgFolder* dstFolder,
                            nsIMsgWindow *msgWindow, PRBool isMove);
  nsresult InitCopyState(nsISupports* srcSupport, 
                          nsISupportsArray* messages,
                          PRBool isMove,
                          PRBool selectedState,
                          PRBool acrossServers,
                          nsIMsgCopyServiceListener* listener,
                          nsIMsgWindow *msgWindow,
                          PRBool allowUndo);
  nsresult OnCopyCompleted(nsISupports *srcSupport, nsresult exitCode);
  nsresult BuildIdsAndKeyArray(nsISupportsArray* messages,
    nsCString& msgIds, nsMsgKeyArray& keyArray);
  
  nsresult GetMoveCoalescer();
  nsresult PlaybackCoalescedOperations();
  virtual nsresult CreateBaseMessageURI(const char *aURI);
  // offline-ish methods
  nsresult GetClearedOriginalOp(nsIMsgOfflineImapOperation *op, nsIMsgOfflineImapOperation **originalOp, 
    nsIMsgDatabase **originalDB);
  nsresult GetOriginalOp(nsIMsgOfflineImapOperation *op, 
    nsIMsgOfflineImapOperation **originalOp, nsIMsgDatabase **originalDB);
  nsresult CopyMessagesOffline(nsIMsgFolder* srcFolder,
                                nsISupportsArray* messages,
                                PRBool isMove,
                                nsIMsgWindow *msgWindow,
                                nsIMsgCopyServiceListener* listener);
  
  nsresult CopyOfflineMsgBody(nsIMsgFolder *srcFolder, nsIMsgDBHdr *destHdr, nsIMsgDBHdr *origHdr);
  
  void GetTrashFolderName(nsAString &aFolderName);
  
  PRBool m_initialized;
  PRBool m_haveDiscoveredAllFolders;
  PRBool m_haveReadNameFromDB;
  nsCOMPtr<nsIMsgParseMailMsgState> m_msgParser;
  nsCOMPtr<nsIMsgFilterList> m_filterList;
  nsCOMPtr<nsIMsgFilterPlugin> m_filterPlugin;  // XXX should be a list
  // used with filter plugins to know when we've finished classifying and can playback moves
  PRInt32 m_numFilterClassifyRequests;
  PRBool m_msgMovedByFilter;
  nsImapMoveCoalescer *m_moveCoalescer; // strictly owned by the nsImapMailFolder
  nsCOMPtr <nsISupportsArray> m_junkMessagesToMarkAsRead;
  nsMsgKey m_curMsgUid;
  PRUint32 m_uidValidity;
  PRInt32 m_numStatusRecentMessages; // used to store counts from Status command
  PRInt32 m_numStatusUnseenMessages;
  PRInt32  m_nextMessageByteLength;
  nsCOMPtr<nsIEventQueue> m_eventQueue;
  nsCOMPtr<nsIUrlListener> m_urlListener;
  PRBool m_urlRunning;
  
  // *** jt - undo move/copy trasaction support
  nsCOMPtr<nsMsgTxn> m_pendingUndoTxn;
  nsCOMPtr<nsImapMailCopyState> m_copyState;
  PRMonitor *m_appendMsgMonitor;
  PRUnichar m_hierarchyDelimiter;
  PRInt32 m_boxFlags;
  nsCString m_onlineFolderName;
  nsFileSpec *m_pathName;
  nsCString m_ownerUserName;  // username of the "other user," as in
  // "Other Users' Mailboxes"
  
  nsCString m_adminUrl;   // url to run to set admin privileges for this folder
  nsIMAPNamespace  *m_namespace;	
  PRPackedBool m_verifiedAsOnlineFolder;
  PRPackedBool m_explicitlyVerify; // whether or not we need to explicitly verify this through LIST
  PRPackedBool m_folderIsNamespace;
  PRPackedBool m_folderNeedsSubscribing;
  PRPackedBool m_folderNeedsAdded;
  PRPackedBool m_folderNeedsACLListed;
  PRPackedBool m_performingBiff;
  PRPackedBool m_folderQuotaCommandIssued;
  PRPackedBool m_folderQuotaDataIsValid;
  
  nsMsgIMAPFolderACL *m_folderACL;
  PRUint32     m_aclFlags;
  PRUint32     m_supportedUserFlags;

  nsCOMPtr<nsISupports> mSupportsToRelease;
  
  static nsIAtom* mImapHdrDownloadedAtom;
  
  // offline imap support
  PRBool m_downloadMessageForOfflineUse;
  PRBool m_downloadingFolderForOfflineUse;
  
  // Quota support
  nsCString m_folderQuotaRoot;
  PRUint32 m_folderQuotaUsedKB;
  PRUint32 m_folderQuotaMaxKB;
};



#endif
