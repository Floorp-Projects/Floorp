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

#ifndef _MsgIMAP_H_
#define _MsgIMAP_H_

#include "msgfinfo.h"

/* url exit functions that may be called by MSG_ImapLoadFolderUrlQueue::HandleUrlQueueInterrupt */
void FilteringCompleteExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);
void ImapFolderSelectCompleteExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);


// forward declaration
struct msg_FolderParseState;
struct tIMAPUploadInfo;
class MSG_IMAPFolderACL;

typedef struct xp_HashTable *XP_HashTable;

class MSG_IMAPFolderInfoMail : public MSG_FolderInfoMail { 
public:
    static MSG_IMAPFolderInfoContainer *BuildIMAPFolderTree 
                                            (MSG_Master *mailMaster, 
                                             uint8 depth, 
                                             MSG_FolderArray *parentArray);
    
    static char *CreateMailboxNameFromDbName(const char *dbName);
	static int UploadMsgsInFolder(void *uploadInfo, int status);
	static void URLFinished(URL_Struct *URL_s, int status, MWContext *window_id);

	static MSG_IMAPFolderInfoMail *CreateNewIMAPFolder(MSG_IMAPFolderInfoMail *parentFolder, MWContext *currentContext, 
											const char *mailboxName, const char *onlineLeafName, uint32 mailboxFlags, MSG_IMAPHost *host);
    MSG_IMAPFolderInfoMail(char* pathname,
                           MSG_Master *mailMaster,
                           uint8 depth, uint32 flags, MSG_IMAPHost *host);
    virtual ~MSG_IMAPFolderInfoMail();

	virtual void SetPrefFolderFlag();

	virtual MSG_IMAPFolderInfoMail	*GetIMAPFolderInfoMail() ;
    virtual MsgERR OnCloseFolder () ;
    virtual FolderType GetType();
    virtual char *BuildUrl (MessageDB *db, MessageKey key);
    
	virtual char *SetupHeaderFetchUrl(MSG_Pane *pane, MessageDB *db, MessageKey startSeq, MessageKey endSeq);
	virtual char *SetupMessageBodyFetchUrl(MSG_Pane *pane, MessageDB *db, IDArray &keysToDownload);
	virtual int SaveFlaggedMessagesToDB(MSG_Pane *pane, MessageDB *db, IDArray &keysToSave);
	virtual int SaveMessagesToDB(MSG_Pane *pane, MessageDB *db, IDArray &keysToSave);
    virtual MsgERR BeginCopyingMessages (MSG_FolderInfo * dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo * copyInfo);


    virtual int FinishCopyingMessages (MWContext * context,
                                       MSG_FolderInfo * srcFolder, 
                                       MSG_FolderInfo * dstFolder, 
 									   MessageDB *sourceDB,
                                     IDArray *srcArray, 
                                      int32 srcCount,
                                      msg_move_state * state);


    virtual void AddToSummaryMailDB(const IDArray &keysToFetch,
                                    MSG_Pane *url_pane,
                                    mailbox_spec *boxSpec);
                                    
    virtual int  HandleBlockForDataBaseCreate(MSG_Pane *pane, const char *str, 
                                                int32 len, 
                                                int32 msgUID,
                                                uint32 msgSize);
    virtual void FinishStreamForDataBaseCreate();
    virtual void AllFolderHeadersAreDownloaded(MSG_Pane *urlPane, TNavigatorImapConnection *imapConnection);
    virtual void AbortStreamForDataBaseCreate (int status);
    
    virtual void StartUpdateOfNewlySelectedFolder(MSG_Pane *paneOfCommand, 
                                                  XP_Bool   loadingFolder,	// only set by msgtpane::loadfolder
                                                  MSG_UrlQueue *urlQueueForSelectURL,
                                                  const IDArray *keysToUndoRedo = NULL,
                                                  XP_Bool undo = FALSE,
												  XP_Bool playbackOfflineEvents = TRUE,
												  Net_GetUrlExitFunc *selectExitFunction = NULL);
                                                  
    virtual void UpdateNewlySelectedFolder(mailbox_spec *adoptedBoxSpec,
                                           MSG_Pane *url_pane);

    virtual void UpdateFolderStatus(mailbox_spec *adoptedBoxSpec,
                                           MSG_Pane *url_pane);

    virtual MsgERR Rename (const char *newName);
	virtual MsgERR ParentRenamed (const char * parentOnlineName);
    
    
    const char* GetOnlineName(); // Name of this folder online
    void SetOnlineName(const char *name);
    
    virtual const char* GetName();  // Name of this folder (as presented to user).
	virtual const char* GetHostName(); // host name to create imap urls with.    
    MSG_IMAPFolderInfoMail *FindImapMailOnline(const char* onlineServerName);
	virtual MSG_IMAPHost			*GetIMAPHost() ;
	MSG_IMAPFolderInfoContainer		*GetIMAPContainer() ;
    // hierarchy separating character on the server
    char GetOnlineHierarchySeparator();
    void SetOnlineHierarchySeparator(char separator);

    enum eMessageDownLoadState {
        kNoDownloadInProgress,
        kDownLoadingAllMessageHeaders,
        kDownLoadMessageForCopy,
        kDownLoadMessageForOfflineDB
    } ;

	void SetDownloadState(eMessageDownLoadState state) { m_DownLoadState = state; }
    // depending on the state of m_DownLoadState, create the stream that will catch
    // the upcoming message download.
    NET_StreamClass *CreateDownloadMessageStream(ImapActiveEntry *ce,uint32 size,
		const char *content_type, XP_Bool content_modified);
    
    
    // used in copying messages
    virtual int CreateMessageWriteStream(msg_move_state *state, uint32 msgSize, uint32 msg_flags);
        // only makes sense if CreateMessageWriteStream worked
    virtual XP_Bool MessageWriteStreamIsReadyForWrite(msg_move_state *state);
    virtual uint32 SeekToEndOfMessageStore(msg_move_state *state);
    virtual uint32   WriteMessageBuffer(msg_move_state *state, char *buffer, uint32 bytesToWrite, MailMessageHdr *offlineHdr);
    virtual void   CloseMessageWriteStream(msg_move_state *state);
	virtual void   SaveMessageHeader(msg_move_state *state, MailMessageHdr *hdr, uint32 messageKey, MessageDB * srcDB = NULL);

    virtual uint32 MessageCopySize(DBMessageHdr *msgHeader, msg_move_state *state);
	virtual uint32 WriteBlockToImapServer(DBMessageHdr *msgHeader, //move this header
											msg_move_state *state,	// track it in this state
											MailMessageHdr *newmsgHeader, // offline append?
											XP_Bool actuallySendToServer);	// false to use for counting

	void			UploadMessagesFromFile(MSG_FolderInfoMail *srcFolder, MSG_Pane *sourcePane, MailDB *mailDB);
	void			FinishUploadFromFile(struct MessageCopyInfo *msgCopyInfo, int status);
    virtual void SetIsOnlineVerified(XP_Bool verified);
	virtual void SetHierarchyIsOnlineVerified(XP_Bool verified);
    virtual XP_Bool GetIsOnlineVerified();
	int32	GetUnverifiedFolders(MSG_IMAPFolderInfoMail** result, int32 resultsize);
    
    MsgERR 	DeleteAllMessages(MSG_Pane *paneForDeleteUrl, XP_Bool deleteSubFoldersToo = TRUE);			// implements empty trash
    void 	DeleteSpecifiedMessages(MSG_Pane *paneForDeleteUrl,		// implements delete from trash
    								const IDArray &keysToDelete,
    								MessageKey loadKeyOnExit = MSG_MESSAGEKEYNONE);
    
    void	StoreImapFlags(MSG_Pane *paneForFlagUrl, imapMessageFlagsType flags, XP_Bool addFlags, 
							const IDArray &keysToFlag, MSG_UrlQueue	*urlQueue = NULL);
    
    void	MessagesWereDeleted(MSG_Pane *urlPane,
    							XP_Bool deleteAllMsgs, 
                    			const char *doomedKeyString);
    
	// store MSG_FLAG_IMAP_DELETED in the specified mailhdr records
	void SetIMAPDeletedFlag(MailDB *mailDB, const IDArray &msgids, XP_Bool markDeleted = TRUE);

    void	Biff(MWContext *biffContext); // set the biff indicator after comparing this db to online state

	XP_Bool MailCheck(MWContext *biffContext); // the future of biff

	// MSG_FolderInfoMail::Adopt does to much.  We already know the
	// newFolder pathname and parent sbd exist, so use this during
	// imap folder discovery
	void	LightWeightAdopt(MSG_IMAPFolderInfoMail *newChild);
	
	// create a string of uids, suitable for passing to imap url create functions
	static char		*AllocateImapUidString(const IDArray &keys);
	static void		ParseUidString(char *uidString, IDArray &keys);
	// we never compress imap folders
	virtual XP_Bool RequiresCleanup();
	virtual void	ClearRequiresCleanup();
	
	// for syncing with server
	void FindKeysToDelete(const IDArray &existingKeys, IDArray &keysToDelete, TImapFlagAndUidState *flagState);
	void FindKeysToAdd(const IDArray &existingKeys, IDArray &keysToFetch, TImapFlagAndUidState *flagState);

	// Helper function for undo MoveCopyMessages
	XP_Bool UndoMoveCopyMessagesHelper(MSG_Pane *urlPane, const IDArray &keysToUndo, UndoObject *undoObject);
	
	
    // non null if we are current loading this folder in some
    // context.  This is to prevent loading a message in the same
    // context that is synching with the server
	MWContext *GetFolderLoadingContext() { return m_LoadingInContext; }
	void SetFolderLoadingContext(MWContext *context) { m_LoadingInContext = context; }
	
	void	   NotifyFolderLoaded(MSG_Pane *urlPane, XP_Bool loadWasInterrupted = FALSE);

	virtual MsgERR ReadDBFolderInfo (XP_Bool force = FALSE);

	virtual MsgERR ReadFromCache (char *);
	virtual MsgERR WriteToCache (XP_File f);
	
	struct mailbox_spec *GetMailboxSpec() {return &m_mailboxSpec;}		
    /* used to prevent recursive listing of mailboxes during discovery */
    int64				GetTimeStampOfLastList() { return m_TimeStampOfLastList; }
    void				SetTimeStampOfLastListNow();

	/* used to determine if we should store subsequent imap urls in the offline db to avoid interrupting
		the current url.  
	*/
	void				SetRunningIMAPUrl(MSG_RunningState runningIMAPUrl);
	MSG_RunningState	GetRunningIMAPUrl() ;

	void				SetHasOfflineEvents(XP_Bool hasOfflineEvents);
	XP_Bool				GetHasOfflineEvents();

	virtual XP_Bool		CanBeInFolderPane () { return m_canBeInFolderPane; }
	void				AllowInFolderPane (XP_Bool allowed) { m_canBeInFolderPane = allowed; }

	virtual XP_Bool GetAdminUrl(MWContext *context, MSG_AdminURLType type);
	virtual XP_Bool HaveAdminUrl(MSG_AdminURLType type);
			const char *AdminUrl() {return m_adminUrl;}
	virtual XP_Bool DeleteIsMoveToTrash();
	virtual XP_Bool ShowDeletedMessages();

	virtual XP_Bool GetFolderNeedsSubscribing() { return m_folderNeedsSubscribing; }
	virtual void	SetFolderNeedsSubscribing(XP_Bool needsSubscribing) { m_folderNeedsSubscribing = needsSubscribing; }
	virtual XP_Bool GetFolderNeedsACLListed() { return m_folderNeedsACLListed; }
	virtual void	SetFolderNeedsACLListed(XP_Bool needsACL) { m_folderNeedsACLListed = needsACL; }

	virtual void	AddFolderRightsForUser(const char *userName, const char *rights);
	virtual void	ClearAllFolderACLRights();
	virtual void	RefreshFolderACLRightsView();

			void	SetAdminUrl(const char *adminUrl);
	virtual const char	*GetFolderOwnerUserName();	// gets the username of the owner of this folder
	virtual const char	*GetOwnersOnlineFolderName();	// gets the online name of this folder, relative to the personal namespace
													// of its owner;  returns the unmodified online name if there is no owner

	virtual XP_Bool GetCanIOpenThisFolder();	// returns TRUE if I can open (select) this folder
	virtual XP_Bool	GetCanDropFolderIntoThisFolder();	// returns TRUE if another folder can be dropped into this folder
	virtual XP_Bool GetCanDropMessagesIntoFolder();	// returns TRUE if messages can be dropped into this folder
	virtual XP_Bool GetCanDragMessagesFromThisFolder();	// returns TRUE if messages can be dragged from of this folder (but not deleted)
	virtual XP_Bool GetCanDeleteMessagesInFolder();	// returns TRUE if we can delete messages in this folder

	virtual char	*CreateACLRightsStringForFolder();
	virtual char	*GetTypeNameForFolder();	// Allocates and returns a string naming this folder's type
												// i.e. "Personal Folder"
												// The caller should free the returned value.
	virtual char	*GetTypeDescriptionForFolder();	// Allocates and returns a string describing this folder's type
													// i.e. "This is a mail folder shared by user 'bob'"
													// The caller should free the returned value.

	virtual XP_Bool AllowsPosting();

	static MSG_IMAPFolderInfoContainer* BuildIMAPServerTree(MSG_Master *mailMaster, const char *imapServerName, MSG_FolderArray * parentArray,
													 XP_Bool errorReturnRootContainerOnly, int depth);

	virtual	void RememberPassword(const char *password);
	virtual char *GetRememberedPassword();
	virtual XP_Bool UserNeedsToAuthenticateForFolder(XP_Bool displayOnly);
	virtual const char *GetUserName();

protected:
	// helper function for SetPrefFolderFlag() method
	void MatchPathWithPersonalNamespaceThenSetFlag(const char *path, 
												   const char *personalNameSpace, 
												   uint32 folderFlag, 
												   const char *onlineName);

	static void ReportErrorFromBuildIMAPFolderTree(MSG_Master *mailMaster, const char *fileName, XP_FileType filetype);
	void NotifyFetchAnyNeededBodies(MSG_Pane *urlPane, TNavigatorImapConnection *imapConnection, MailDB *folderDb);

    virtual MsgERR BeginOfflineCopy (MSG_FolderInfo * dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      int32 srcCount,
                                      msg_move_state * state);
    
    DBOfflineImapOperation *GetClearedOriginalOp(DBOfflineImapOperation *op, MailDB **originalDB);
    DBOfflineImapOperation *GetOriginalOp(DBOfflineImapOperation *op, MailDB **originalDB);

	MsgERR OfflineImapMsgToLocalFolder(MailMessageHdr *sourceHeader, MSG_FolderInfoMail *destFolder,MessageDB *sourceDB,msg_move_state *state);
	
	// create a fixed size array of uids.  Used for downloading bodies
	uint32	*AllocateImapUidArray(const IDArray &keys);

    // used the first time I know that my db is in sync with server
    void OpenThreadView(MailDB *mailDB,mailbox_spec *adoptedBoxSpec);
    
    void QueueUpImapFilterUrls(MSG_UrlQueue *urlQueue);
    void GetFilterSearchUrlString(MSG_Filter *currenFilter, char **imapUrlString);                 
    IDArray *CreateFilterIDArray(const char *imapSearchResultString, 
                                            int &numberOfHits);
    void LogRuleHit(MSG_Filter *filter, DBMessageHdr *msgHdr , MWContext *context);

	static  XP_Bool StaticShouldIgnoreFile(const char *fileName);
    virtual XP_Bool ShouldIgnoreFile (const char *fileName);

	char *CreateOnlineVersionOfLocalPathString(MSG_Prefs &prefs,const char *localPath) const;
	MSG_IMAPFolderACL	*GetFolderACL();

private:
	XP_Bool				m_canBeInFolderPane;
    XP_Bool             m_FolderIsRoot;
    XP_Bool             m_VerifiedAsOnlineFolder;
	MSG_RunningState	m_runningIMAPUrl;
	XP_Bool				m_requiresCleanup;
	XP_Bool				m_folderNeedsSubscribing;
	XP_Bool				m_folderNeedsACLListed;
	char				*m_ownerUserName;	// username of the "other user," as in
											// "Other Users' Mailboxes"
    
    char                *m_OnlineFolderName;
    char				m_OnlineHierSeparator;

    struct mailbox_spec m_mailboxSpec;	// most recent mailbox spec for this folder

    // non null if we are current loading this folder in some
    // context.  This is to prevent loading a message in the same
    // context that is synching with the server
    MWContext			*m_LoadingInContext;
        
    /* used to prevent recursive listing of mailboxes during discovery */
    int64				m_TimeStampOfLastList;
    
	XP_Bool					m_hasOfflineEvents;
    eMessageDownLoadState m_DownLoadState;
	struct tIMAPUploadInfo	*m_uploadInfo;	// for uploading news messages to imap folders
	MSG_IMAPHost			*m_host;
	MSG_IMAPFolderACL		*m_folderACL;
	char					*m_adminUrl;
};  


struct tImapFilterClosure {
    MSG_IMAPFolderInfoMail *sourceFolder;
    int32 filterIndex;
    MSG_UrlQueue *urlQueue;
    MSG_Pane     *filterPane;
	MessageDB	 *sourceDB;
};

class MSG_IMAPFolderInfoContainer : public MSG_FolderInfoContainer
{
public:
    MSG_IMAPFolderInfoContainer (const char *name, uint8 depth, MSG_IMAPHost *host, MSG_Master *master);
    virtual ~MSG_IMAPFolderInfoContainer ();

    virtual FolderType GetType();
	virtual MSG_IMAPFolderInfoContainer	*GetIMAPFolderInfoContainer() ;

    virtual XP_Bool CanCreateChildren () { return TRUE; }
	virtual XP_Bool IsDeletable();
    virtual const char *GetPrettyName();
    
    virtual char *BuildUrl (MessageDB *db, MessageKey key);
    MSG_IMAPFolderInfoMail *FindImapMailOnline(const char* onlineServerName, const char *owner, XP_Bool createIfMissing);

    int32 GetUnverifiedFolders(MSG_IMAPFolderInfoMail** result, int32 resultsize);


	virtual MSG_IMAPHost			*GetIMAPHost();
	virtual const char* GetHostName(); // host name to create imap urls with.    
	MsgERR Adopt (MSG_FolderInfo *srcFolder, int32 *pOutPos);
	virtual XP_Bool GetAdminUrl(MWContext *context, MSG_AdminURLType type);
	virtual XP_Bool HaveAdminUrl(MSG_AdminURLType type);
	void		SetHostNeedsFolderUpdate(XP_Bool needsUpdate) {m_foldersNeedUpdating = needsUpdate; }
	XP_Bool		GetHostNeedsFolderUpdate() { return m_foldersNeedUpdating; }

protected:
	char					*m_prettyName;
	MSG_IMAPHost			*m_host;
	XP_Bool					m_foldersNeedUpdating;
};



// ACLs for this folder.
// Generally, we will try to always query this class when performing
// an operation on the folder.
// If the server doesn't support ACLs, none of this data will be filled in.
// Therefore, we can assume that if we look up ourselves and don't find
// any info (and also look up "anyone") then we have full rights, that is, ACLs don't exist.
class MSG_IMAPFolderACL
{
public:
	MSG_IMAPFolderACL(MSG_IMAPFolderInfoMail *folder);
	~MSG_IMAPFolderACL();
	
	XP_Bool SetFolderRightsForUser(const char *userName, const char *rights);

public:

	// generic for any user, although we might not use them in
	// DO NOT use these for looking up information about the currently authenticated user.
	// (There are some different checks and defaults we do).
	// Instead, use the functions below, GetICan....()
	XP_Bool GetCanUserLookupFolder(const char *userName);		// Is folder visible to LIST/LSUB?
	XP_Bool GetCanUserReadFolder(const char *userName);			// SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder?
	XP_Bool	GetCanUserStoreSeenInFolder(const char *userName);	// STORE SEEN flag?
	XP_Bool GetCanUserWriteFolder(const char *userName);		// STORE flags other than SEEN and DELETED?
	XP_Bool	GetCanUserInsertInFolder(const char *userName);		// APPEND, COPY into folder?
	XP_Bool	GetCanUserPostToFolder(const char *userName);		// Can I send mail to the submission address for folder?
	XP_Bool	GetCanUserCreateSubfolder(const char *userName);	// Can I CREATE a subfolder of this folder?
	XP_Bool	GetCanUserDeleteInFolder(const char *userName);		// STORE DELETED flag, perform EXPUNGE?
	XP_Bool	GetCanUserAdministerFolder(const char *userName);	// perform SETACL?

	// Functions to find out rights for the currently authenticated user.

	XP_Bool GetCanILookupFolder();		// Is folder visible to LIST/LSUB?
	XP_Bool GetCanIReadFolder();		// SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder?
	XP_Bool	GetCanIStoreSeenInFolder();	// STORE SEEN flag?
	XP_Bool GetCanIWriteFolder();		// STORE flags other than SEEN and DELETED?
	XP_Bool	GetCanIInsertInFolder();	// APPEND, COPY into folder?
	XP_Bool	GetCanIPostToFolder();		// Can I send mail to the submission address for folder?
	XP_Bool	GetCanICreateSubfolder();	// Can I CREATE a subfolder of this folder?
	XP_Bool	GetCanIDeleteInFolder();	// STORE DELETED flag, perform EXPUNGE?
	XP_Bool	GetCanIAdministerFolder();	// perform SETACL?

	XP_Bool GetDoIHaveFullRightsForFolder();	// Returns TRUE if I have full rights on this folder (all of the above return TRUE)

	XP_Bool	GetIsFolderShared();		// We use this to see if the ACLs think a folder is shared or not.
										// We will define "Shared" in 5.0 to mean:
										// At least one user other than the currently authenticated user has at least one
										// explicitly-listed ACL right on that folder.

	char	*CreateACLRightsString();	// Returns a newly allocated string describing these rights
	



protected:
	const char *GetRightsStringForUser(const char *userName);
	XP_Bool GetFlagSetInRightsForUser(const char *userName, char flag, XP_Bool defaultIfNotFound);
	void BuildInitialACLFromCache();
	void UpdateACLCache();

protected:
	XP_HashTable	m_rightsHash;	// Hash table, mapping username strings to rights strings.
	MSG_IMAPFolderInfoMail *m_folder;
	uint32			m_aclCount;

};



#endif
