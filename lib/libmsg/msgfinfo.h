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

#ifndef _MsgFInfo_H_
#define _MsgFInfo_H_

#include "ptrarray.h"
#include "imap.h"
#include "errcode.h"
#include "msgmast.h"
#include "dberror.h"
#include "idarray.h"
#include "msgundmg.h"

class MessageDB;
class NewsGroupDB;
class MailDB;
class MailMessageHdr;
class DBMessageHdr;
class MessageDBView;
class msg_NewsArtSet;
class MSG_NewsHost;
class MSG_FolderInfoContainer;
class MSG_FolderInfoCategoryContainer;
class ImapMailDB;
class ParseMailboxState;
class DBFolderInfo;
class MSG_UrlQueue;
class msg_GroupRecord;
class DBOfflineImapOperation;
class MSG_FolderArray;
class MSG_IMAPHost;

struct MSG_Filter;


/* Info about a single folder. */


enum FolderType {
    FOLDER_MAIL,
    FOLDER_IMAPMAIL,
    FOLDER_NEWSGROUP,
    FOLDER_CONTAINERONLY,       // Can't actually hold messages, just other
                                // folders.
    FOLDER_CATEGORYCONTAINER,
    FOLDER_IMAPSERVERCONTAINER
};              /* What kind of folder thingy this is. */


typedef struct msg_write_state {
    XP_Bool inhead;

    XP_File fid;
    uint32 statusPosition;
    uint32 position;
    uint32 header_bytes;
    XP_Bool update_msg;
    char *uidl;
    int numskip;
    int32 (*writefunc)(char* line, uint32 length, void* closure);
    void* writeclosure;
	uint32 flags;
} msg_write_state;



typedef struct msg_move_state {
    XP_Bool ismove;
    XP_Bool midMessage;             // in the middle of copying a large message
    uint32  messageBytesMovedSoFar; // for the current message
    
    uint32 totalSizeForProgressGraph;
    uint32 finalDownLoadMessageSize;
    XP_File infid;
    
    PRFileDesc *uploadToIMAPsocket;
    UploadCompleteFunctionPointer *uploadCompleteFunction;
    void *uploadCompleteArgument;
    
    char* buf;
    int size;  /* size of buf */
    MSG_FolderInfo* destfolder;
    MSG_Pane* sourcePane;
    TNavigatorImapConnection *imap_connection;
	XP_Bool	haveUploadedMessageSize; /* for imap uploading of msg size*/
	uint32 msgSize;
	uint32 msg_flags;

    MailDB  *destDB;
    NewsGroupDB *sourceNewsDB;
    msg_write_state writestate;
    char *ibuffer;
    uint32 ibuffer_size;
    uint32 ibuffer_fp;
    int num;
    int numunread;
    MessageKey  nextKeyToLoad;  // what to load after delete from message pane
    char *urlForNextKeyLoad;
    
    int32 msgIndex;
    XP_Bool moveCompleted;
    
    
} msg_move_state;

typedef enum
{
	PurgeNeeded,
	PurgeNotNeeded,
	PurgeMaybeNeeded
} MSG_PurgeStatus;

class MSG_FolderInfo {
public:
	MSG_FolderInfo(const char* name, uint8 depth, XPCompareFunc* comparator = NULL);
	virtual ~MSG_FolderInfo();

    virtual FolderType GetType() = 0;

    // Gets the URL that represents the given message.  Returns a newly
    // created string that must be free'd using XP_FREE().
    // If the db is NULL, then returns a URL that represents the entire
    // folder as a whole.
    virtual char *BuildUrl (MessageDB *db, MessageKey key) = 0;


    virtual void StartAsyncCopyMessagesInto (MSG_FolderInfo *dstFolder,
                                             MSG_Pane* sourcePane, 
											 MessageDB *sourceDB,
                                             IDArray *srcArray,
                                             int32 srcCount,
                                             MWContext *currentContext,
                                             MSG_UrlQueue *urlQueue,
                                             XP_Bool deleteAfterCopy,
                                             MessageKey nextKey = MSG_MESSAGEKEYNONE);

    
    virtual MsgERR BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo) = 0;


    virtual int FinishCopyingMessages (MWContext *context,
                                       MSG_FolderInfo * srcFolder, 
                                       MSG_FolderInfo *dstFolder, 
									   MessageDB *sourceDB,
                                       IDArray *srcArray, 
                                       int32 srcCount,
                                       msg_move_state *state) = 0;


    virtual void CleanupCopyMessagesInto (MessageCopyInfo **info);

    virtual MsgERR  SaveMessages(IDArray *, const char *fileName, 
                                MSG_Pane *pane, MessageDB *msgDB,
								  int (*doneCB)(void *, int status) = NULL, void *state = NULL);

    virtual const char *GetPrettyName(); // so news can override and return pretty name
	virtual const char* GetName();  // Name of this folder (as presented to user).
	virtual void SetName(const char *name);
  
	virtual const char *GetPrettiestName() ;
    XP_Bool HasSubFolders() const;
    int GetNumSubFolders() const;
    virtual int GetNumSubFoldersToDisplay() const;
    MSG_FolderInfo* GetSubFolder(int which) const;
    MSG_FolderArray *GetSubFolders ();
    virtual void RemoveSubFolder (MSG_FolderInfo *which);
	void GetVisibleSubFolders (MSG_FolderArray&);

	virtual XP_Bool GetAdminUrl(MWContext *context, MSG_AdminURLType type);
	virtual XP_Bool HaveAdminUrl(MSG_AdminURLType type);
	virtual XP_Bool DeleteIsMoveToTrash();
	virtual XP_Bool ShowDeletedMessages();
    virtual MsgERR OnCloseFolder () ;
    virtual MsgERR Delete () = 0;
    MsgERR PropagateDelete (MSG_FolderInfo **folder, XP_Bool deleteStorage = TRUE);
	MsgERR RecursiveDelete (XP_Bool deleteStorage = TRUE);  // called by PropagateDelete
    virtual MsgERR CreateSubfolder (const char *, MSG_FolderInfo**, int32*) = 0;
    virtual MsgERR Rename (const char *name);
    virtual MsgERR Adopt (MSG_FolderInfo *srcFolder, int32*) = 0;

    virtual XP_Bool ContainsChildNamed (const char *name);
    virtual MSG_FolderInfo *FindChildNamed (const char *name);
    virtual MSG_FolderInfo *FindParentOf (MSG_FolderInfo *);
    virtual XP_Bool IsParentOf (MSG_FolderInfo *, XP_Bool deep = TRUE);

	virtual const char *GenerateUniqueSubfolderName(const char *prefix, MSG_FolderInfo *otherFolder = NULL);

    uint8   GetDepth() const;
    void    SetDepth(uint8 depth);

    // updates num messages and num unread - should be pure virtual
    // when I get around to implementing in all subclasses?
    virtual XP_Bool UpdateSummaryTotals();
    void            SummaryChanged();
    int32			GetNumUnread(XP_Bool deep = FALSE) const;       // How many unread messages in this folder.
    int32			GetTotalMessages(XP_Bool deep = FALSE) const;   // Total number of messages in this folder.
	virtual int32	GetTotalMessagesInDB() const;					// How many messages in database.
	virtual void	MarkAllRead(MWContext *context, XP_Bool deep);
	
	// These functions are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	// These functions are called when MSG_Master::GetFolderLineById is populating a MSG_FolderLine
	// struct used by the FE
	int32			GetNumPendingUnread(XP_Bool deep = FALSE) const;
	int32			GetNumPendingTotalMessages(XP_Bool deep = FALSE) const;
	
	void			ChangeNumPendingUnread(int32 delta);
	void			ChangeNumPendingTotalMessages(int32 delta);

    void    SetFolderPrefFlags(int32 flags);
    int32   GetFolderPrefFlags();
	void	SetFolderCSID(int16 csid);
	int16	GetFolderCSID();
	void	SetLastMessageLoaded(MessageKey lastMessageLoaded);
	MessageKey GetLastMessageLoaded();
    uint32  GetFlags() const ;
    void    ToggleFlag (uint32 which);
    void SetFlag (uint32 which);

	void SetFlagInAllFolderPanes(uint32 which);
    void ClearFlag (uint32 which);
	XP_Bool TestFlag (uint32 which) { return (m_flags & which) != 0; } // Type conversion to char-sized XP_Bool

    int32 GetFoldersWithFlag(uint32 flags, MSG_FolderInfo** result,
                           int32 resultsize);
    // for everyone else...
    virtual void GetExpansionArray (MSG_FolderArray &array );

    char *EscapeMessageId (const char *messageId);
    
	virtual int32					GetExpungedBytesCount() const;
    virtual MSG_FolderInfoMail      *GetMailFolderInfo() ;
    virtual MSG_FolderInfoNews      *GetNewsFolderInfo() ;
	virtual MSG_IMAPFolderInfoMail	*GetIMAPFolderInfoMail() ;
	virtual MSG_IMAPFolderInfoContainer	*GetIMAPFolderInfoContainer() ;
	virtual MSG_IMAPHost			*GetIMAPHost() ;

    virtual XP_Bool IsMail () = 0;
    virtual XP_Bool IsNews () = 0;
    virtual XP_Bool IsDeletable () = 0;
    virtual XP_Bool CanCreateChildren ();
    virtual XP_Bool CanBeRenamed () ;
	virtual XP_Bool RequiresCleanup();
	virtual void	ClearRequiresCleanup();
	virtual XP_Bool CanBeInFolderPane ();
	virtual XP_Bool KnowsSearchNntpExtension();
	virtual XP_Bool AllowsPosting();
	virtual XP_Bool DisplayRecipients();

    MsgERR AcquireSemaphore (void *semHolder);
    void ReleaseSemaphore (void *semHolder);
    XP_Bool TestSemaphore (void *semHolder);
    XP_Bool IsLocked () { return m_semaphoreHolder != NULL; }
    MWContext *GetFolderPaneContext();
    MSG_Master  *GetMaster() {return m_master;}
    virtual MsgERR GetDBFolderInfoAndDB(DBFolderInfo **folderInfo, MessageDB **db) = 0;
	virtual MsgERR ReadDBFolderInfo (XP_Bool force = FALSE);

	virtual MsgERR WriteToCache (XP_File);
	virtual MsgERR ReadFromCache (char *);
	virtual XP_Bool IsCachable ();
	void SkipCacheTokens (char **ppBuf, int numTokens);
	virtual const char *GetRelativePathName ();

	virtual int32 GetSizeOnDisk();

	virtual XP_Bool	ShouldPerformOperationOffline();
	static int CompareFolderDepth (const void *, const void *);

	int		DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo, IDArray &keysToSave, MSG_FolderInfo *dstFolder, MessageDB *sourceDB);
	virtual	void RememberPassword(const char *password);
	virtual char *GetRememberedPassword();
	virtual XP_Bool UserNeedsToAuthenticateForFolder(XP_Bool displayOnly);
	virtual const char *GetUserName();
	virtual const char *GetHostName();
 
protected:

    char* m_name;
    uint32 m_flags;
    int32 m_numUnreadMessages;        /* count of unread messages   (-1 means
                                   unknown; -2 means unknown but we already
                                   tried to find out.) */
    int32 m_numTotalMessages;         /* count of existing messages. */
    MSG_FolderArray *m_subFolders;
    MSG_Master  *m_master;
	int16 m_csid;			// default csid for folder/newsgroup - maintained by fe.
    uint8 m_depth;
    int32 m_prefFlags;       // prefs like MSG_PREF_OFFLINE, MSG_PREF_ONE_PANE, etc
    void *m_semaphoreHolder; // set when the folder is being written to
	MessageKey	m_lastMessageLoaded;

	// These values are used for tricking the front end into thinking that we have more 
	// messages than are really in the DB.  This is usually after and IMAP message copy where
	// we don't want to do an expensive select until the user actually opens that folder
	int32 m_numPendingUnreadMessages;
	int32 m_numPendingTotalMessages;

	XP_Bool m_isCachable;
};


struct MessageCopyInfo {
    MSG_FolderInfo  *srcFolder;
    MSG_FolderInfo  *dstFolder;
    MessageCopyInfo *nextCopyInfo;
    
    XP_Bool         dstIMAPfolderUpdated;
    uint32          offlineFolderPositionOfMostRecentMessage;
	MessageDB		*srcDB;
    IDArray *srcArray;
    int32 srcCount;
    msg_move_state moveState;
    ImapOnlineCopyState imapOnLineCopyState;
};



class MSG_FolderInfoMail : public MSG_FolderInfo {
public:
    static MSG_FolderInfoMail *BuildFolderTree (const char *path, 
        uint8 depth, MSG_FolderArray *parentArray, MSG_Master *master, 
		XP_Bool buildingImapTree = FALSE, MSG_IMAPHost *host = NULL);

    MSG_FolderInfoMail(char* pathname, MSG_Master *master, uint8 depth, uint32 flags);
    virtual ~MSG_FolderInfoMail();

	virtual void SetPrefFolderFlag();

    virtual FolderType GetType();

    virtual char *BuildUrl (MessageDB *db, MessageKey key);
    
    virtual MsgERR BeginCopyingMessages (MSG_FolderInfo *dstFolder, 
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo);


    virtual int FinishCopyingMessages (MWContext *context,
                                       MSG_FolderInfo * srcFolder, 
                                       MSG_FolderInfo *dstFolder, 
 									   MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                       int32 srcCount,
                                       msg_move_state *state);

	virtual MsgERR CheckForLegalCopy(MSG_FolderInfo *dstFolder);

    virtual MsgERR CreateSubfolder (const char *, MSG_FolderInfo**, int32*);

    virtual void RemoveSubFolder (MSG_FolderInfo *which);
    virtual MsgERR Delete ();
    virtual MsgERR Rename (const char *newName);
    virtual MsgERR Adopt (MSG_FolderInfo *srcFolder, int32 *newPos);

    MsgERR PropagateRename (const char *newName, int startingAt);
    MsgERR PropagateAdopt (const char *parentPath, uint8 parentDepth);

		// this override pulls the value from the db
    virtual const char* GetName();   // Name of this folder (as presented to user).
	virtual const char* GetPrettyName();	// Override of the base, for top-level mail folder

	virtual const char *GenerateUniqueSubfolderName(const char *prefix, MSG_FolderInfo *otherFolder = NULL);

    const char* GetPathname() {return m_pathName;}  // Full path to this folder.
    MSG_FolderInfoMail* FindPathname(const char* pathname);
    XP_Bool             UpdateSummaryTotals() ;

    // used in copying messages 
    virtual int  CreateMessageWriteStream(msg_move_state *state, uint32 msgSize, uint32 msg_flags);
        // only makes sense if CreateMessageWriteStream worked
    virtual XP_Bool MessageWriteStreamIsReadyForWrite(msg_move_state *state);
    virtual uint32  SeekToEndOfMessageStore(msg_move_state *state);
    virtual uint32    WriteMessageBuffer(msg_move_state *state, char *buffer, uint32 bytesToWrite, MailMessageHdr *offlineHdr);
    virtual void    CloseMessageWriteStream(msg_move_state *state);
	
	// srcDB is passed in when we are dealing with a search pane & it it doesn't have an message view. everyone else
	// should ignore this extra parameter..
    virtual void    SaveMessageHeader(msg_move_state *state, MailMessageHdr *hdr, uint32 messageKey, MessageDB * srcDB = NULL);

    virtual void    CloseOfflineDestinationDbAfterMove(msg_move_state *state);

    void                SetMaster(MSG_Master *master) {m_master = master;}
    // these methods handle current folder being parsed, if any.
    ParseMailboxState   *GetParseMailboxState() {return m_parseMailboxState;}
    void                SetParseMailboxState(ParseMailboxState *state) {m_parseMailboxState = state;}

    virtual uint32 MessageCopySize(DBMessageHdr *msgHeader, msg_move_state *state);
    
    virtual MSG_FolderInfoMail      *GetMailFolderInfo() {return this;}
	virtual int32					GetExpungedBytesCount() const;
	virtual void					SetExpungedBytesCount(int32 count) ;
    virtual XP_Bool IsMail () ;
    virtual XP_Bool IsNews () ;
    virtual XP_Bool IsDeletable (); 
    virtual XP_Bool CanCreateChildren () ;
    virtual XP_Bool CanBeRenamed ();
	virtual XP_Bool RequiresCleanup();

    MsgERR      GetDBFolderInfoAndDB(DBFolderInfo **folderInfo, MessageDB **db);
	virtual MsgERR ReadDBFolderInfo (XP_Bool force = FALSE);

	virtual MsgERR WriteToCache (XP_File);
	virtual MsgERR ReadFromCache (char *);
	virtual const char *GetRelativePathName ();

	static char *CreatePlatformLeafNameForDisk(const char *userLeafName, MSG_Master *master, const char *baseDir);
	static char *CreatePlatformLeafNameForDisk(const char *userLeafName, MSG_Master *master, MSG_FolderInfoMail *parentFolder);
			IDArray	*GetImapIdsToMoveFromInbox() {return &m_imapIdsToMoveFromInbox;}
			void ClearImapIdsToMoveFromInbox() {m_imapIdsToMoveFromInbox.RemoveAll();}

	virtual int32 GetSizeOnDisk();

    static int CompareFolders (const void *, const void *);
	void 	UpdatePendingCounts(MSG_FolderInfo * dstFolder, 
								IDArray	*srcArray,
								XP_Bool countUnread = TRUE,
								XP_Bool missingAreRead = TRUE);
	void	SetGettingMail(XP_Bool flag);
	XP_Bool GetGettingMail();
	virtual const char *GetUserName();
	virtual const char *GetHostName();
	virtual void DeleteMsgsOnServer(msg_move_state *state, MessageDB *sourceDB, IDArray *srcArray, int32 srcCount);
	virtual XP_Bool UserNeedsToAuthenticateForFolder(XP_Bool displayOnly);
	virtual char *GetRememberedPassword();

protected:
		// a factory that deals with pathname to long for us to append ".snm" to. Useful when the user imports
		// a mailbox file with a long name.  If there is a new name then pathname is replaced.
	static  MSG_FolderInfoMail *CreateMailFolderInfo(char* pathname, MSG_Master *master, uint8 depth, uint32 flags);

	MsgERR BeginOfflineAppend(MSG_IMAPFolderInfoMail *dstFolder, 
												MessageDB *sourceDB,
												IDArray *srcArray, 
												int32 srcCount,
												msg_move_state *state);
	
    MsgERR CloseDatabase (const char *name, MessageDB **outDB);
    MsgERR ReopenDatabase (MessageDB *db, const char *newName);
    
    // This function ensures that each line in the buffer ends in
    // CRLF, as required by RFC822 and therefore IMAP APPEND.
    // 
    // This function assumes that there is enough room in the buffer to 
    // double its size.  The return value is the new buffer size.
    uint32 ConvertBufferToRFC822LineTermination(char *buffer, uint32 bufferSize, 
                                                XP_Bool *lastCharacterOfPrevBufferWasCR);
    
    void SetUrlForNextMessageLoad(msg_move_state *state, MessageDBView *view, MSG_ViewIndex doomedIndex);
    
    char *FindNextLineTerminatingCharacter(char *bufferToSearch);


    virtual XP_Bool ShouldIgnoreFile (const char *fileName);
    char*				m_pathName;
    ParseMailboxState   *m_parseMailboxState;
	int32				m_expungedBytes;
	XP_Bool				m_HaveReadNameFromDB;
	IDArray				m_imapIdsToMoveFromInbox;	// use to apply imap move filters with this 
													// folder as a destination.
	XP_Bool				m_gettingMail;
};


class MSG_FolderInfoNews : public MSG_FolderInfo {
public:
	MSG_FolderInfoNews(char *groupName, msg_NewsArtSet* set,
					   XP_Bool subscribed, MSG_NewsHost* host, uint8 depth);
	virtual ~MSG_FolderInfoNews();

	virtual FolderType GetType();
	virtual char *BuildUrl (MessageDB *db, MessageKey key);

	virtual MsgERR BeginCopyingMessages (MSG_FolderInfo * /*dstFolder*/, 
									  MessageDB * /*sourceDB*/,
	  							      IDArray * /*srcArray*/, 
	  							      MSG_UrlQueue * /*urlQueue*/,
	  							      int32 /*srcCount*/,
	  							      MessageCopyInfo * /* copyInfo */);


	virtual int FinishCopyingMessages (MWContext * context,
									   MSG_FolderInfo * srcFolder, 
									   MSG_FolderInfo * dstFolder, 
									   MessageDB *sourceDB,
	  							      IDArray *srcArray, 
	  							      int32 srcCount,
	  							      msg_move_state * state) ;

	virtual MsgERR CreateSubfolder (const char *, MSG_FolderInfo**, int32*);
	virtual MsgERR Delete ();
	virtual MsgERR Rename (const char *newName);
	virtual MsgERR Adopt (MSG_FolderInfo *srcFolder, int32 *newPos);

	virtual const char *GetRelativePathName ();
	const char	*GetNewsgroupName() const {return m_name;}
	const char  *GetDBFileName(); // Fully expanded filename, suitable to pass to db
	const char *GetXPDBFileName(); // filename suitable to pass to xp routines
	const char *GetXPRuleFileName(); // newsgroup rule filename suitable to pass to xp routines.

    void SetNewsgroupUsername(const char * username); // munged username
    const char *GetNewsgroupUsername(void); // return munged username; 

    void SetNewsgroupPassword(const char * password); // munged password
    const char *GetNewsgroupPassword(void); // return munged password; 

	virtual const char *GetPrettyName();
	virtual void		ClearPrettyName();
	virtual char *GetHostUrl();

	XP_Bool 	IsSubscribed();
	MsgERR		Subscribe(XP_Bool bSubscribe, MSG_Pane *invokingPane = NULL);
	msg_NewsArtSet* GetSet();
	void		ClearSet() {m_set = NULL;}
	virtual XP_Bool		UpdateSummaryTotals();
	virtual void		UpdateSummaryFromNNTPInfo(int32 oldest_message,
										  int32 youngest_message,
										  int32 total_messages);

	MSG_FolderInfoCategoryContainer *CloneIntoCategoryContainer();

	void 		SetWantNewTotals(XP_Bool value) {m_wantNewTotals = value;}
	XP_Bool		GetWantNewTotals() {return m_wantNewTotals;}

	void		SetAutoSubscribed(XP_Bool value) {m_autoSubscribed = value;}
	XP_Bool		GetAutoSubscribed() {return m_autoSubscribed;}

	virtual void MarkAllRead(MWContext *context, XP_Bool deep);

	virtual int32	GetTotalMessagesInDB() const;					// How many messages in database.
	MSG_NewsHost* GetHost() {return m_host;}

	virtual XP_Bool CanBeInFolderPane ();
	virtual XP_Bool IsMail () ;
	virtual XP_Bool IsNews ();
	virtual XP_Bool IsDeletable ();
	virtual XP_Bool	IsCategory();
	virtual XP_Bool RequiresCleanup();
	virtual void	ClearRequiresCleanup();
	virtual XP_Bool KnowsSearchNntpExtension();
	virtual XP_Bool AllowsPosting();

	virtual MSG_FolderInfoNews		*GetNewsFolderInfo();
	virtual NewsGroupDB				*OpenNewsDB(XP_Bool deleteInvalidDB = FALSE, MsgERR *pErr = NULL);
	MsgERR		GetDBFolderInfoAndDB(DBFolderInfo **folderInfo, MessageDB **db);
	// these methods handle current folder being parsed, if any.
	ListNewsGroupState	*GetListNewsGroupState() {return m_listNewsGroupState;}
	void				SetListNewsGroupState(ListNewsGroupState *state) {m_listNewsGroupState = state;}

	virtual int32 GetSizeOnDisk();

	virtual MsgERR ReadFromCache (char*);

protected:
	char *				GetUniqueFileName(const char *extension);

    MSG_NewsHost*       m_host;
    msg_NewsArtSet*     m_set;
	int32				m_nntpHighwater;
	int32				m_nntpTotalArticles;
	int32				m_totalInDB;
	int32				m_unreadInDB;
    XP_Bool             m_subscribed;
	XP_Bool				m_autoSubscribed;
    XP_Bool             m_wantNewTotals;
    char *              m_dbfilename;
    char *              m_XPDBFileName;     // name to pass to XPFile routines
    char *              m_XPRuleFileName;     // name to pass to XPFile routines
    char *              m_prettyName;
	MSG_PurgeStatus		m_purgeStatus;
    ListNewsGroupState  *m_listNewsGroupState;
    char *              m_username;	// munged username
    char *              m_password;	// munged password
};


class MSG_FolderInfoCategoryContainer : public MSG_FolderInfoNews {
public:
    MSG_FolderInfoCategoryContainer(char *groupName, msg_NewsArtSet* set,
                       XP_Bool subscribed, MSG_NewsHost* host, uint8 depth);
    virtual ~MSG_FolderInfoCategoryContainer();

    MSG_FolderInfoNews *BuildCategoryTree(MSG_FolderInfoNews *parent,
										  const char *catName, 
										  msg_GroupRecord* group,
										  uint8 depth, MSG_Master *master);
    
MSG_FolderInfoNews		*AddToCategoryTree(MSG_FolderInfoNews *parent,
												   const char *groupName,
												   msg_GroupRecord* group,
												   MSG_Master *master);

    virtual FolderType GetType();
    // these don't expand in folder pane.
	MSG_FolderInfoNews *CloneIntoNewsFolder();
	MSG_FolderInfoNews *GetRootCategory();
	virtual MSG_FolderInfoNews *GetNewsFolderInfo() ;
};


// A folder that contains other folders, but doesn't do much else.
class MSG_FolderInfoContainer : public MSG_FolderInfo {
public:
    MSG_FolderInfoContainer(const char* name, uint8 depth, XPCompareFunc* f);
    virtual ~MSG_FolderInfoContainer();
    virtual FolderType GetType();
    virtual char *BuildUrl (MessageDB *db, MessageKey key);
    virtual MsgERR BeginCopyingMessages (MSG_FolderInfo *dstFolder,
									  MessageDB *sourceDB,
                                      IDArray *srcArray, 
                                      MSG_UrlQueue *urlQueue,
                                      int32 srcCount,
                                      MessageCopyInfo *copyInfo);


    virtual int FinishCopyingMessages (MWContext *context,
                                       MSG_FolderInfo * srcFolder, 
                                       MSG_FolderInfo *dstFolder, 
  									   MessageDB *sourceDB,
                                     IDArray *srcArray, 
                                       int32 srcCount,
                                       msg_move_state *state);

    virtual MsgERR CreateSubfolder (const char *, MSG_FolderInfo**, int32*);

    virtual MsgERR  SaveMessages (IDArray*, const char *fileName, 
                                  MSG_Pane *pane, MessageDB *msgDB,
								  int (*doneCB)(void *, int status) = NULL, void *state = NULL);

    virtual MsgERR Delete();
    virtual MsgERR Adopt (MSG_FolderInfo *srcFolder, int32 *newPos);

    // returns mail folder within container
    virtual MSG_FolderInfoMail* FindMailPathname(const char* p);

	// move these into msgfinfo.cpp
    virtual XP_Bool IsMail () { return FALSE; }
    virtual XP_Bool IsNews () { return FALSE; }
    virtual XP_Bool IsDeletable () { return FALSE; }
	virtual MSG_NewsHost* GetHost() {return NULL;}
    MsgERR      GetDBFolderInfoAndDB(DBFolderInfo ** /*folderInfo*/, MessageDB ** /*db*/)
                    {return eFAILURE;}

protected:
};


class MSG_NewsFolderInfoContainer : public MSG_FolderInfoContainer
{
public:
	MSG_NewsFolderInfoContainer(MSG_NewsHost* host, uint8 depth);
	virtual ~MSG_NewsFolderInfoContainer();

	virtual XP_Bool IsDeletable();
	virtual MsgERR Delete();
	virtual MSG_NewsHost* GetHost() {return m_host;}
	virtual XP_Bool KnowsSearchNntpExtension ();
	virtual XP_Bool AllowsPosting();
    virtual int GetNumSubFoldersToDisplay() const;

protected:
	MSG_NewsHost* m_host;
	char *m_prettyName;
};


struct MSG_FolderIteratorStackElement
{
    MSG_FolderInfo *m_parent;
    int             m_currentIndex;
};


/*
  MSG_FolderIterator: A class for traversing folder lists.
  ### mw Right now, traversal is depth first. Other people can add
         their favorite traversal methods.
 */
class MSG_FolderIterator
{
public:
    MSG_FolderInfo *m_masterParent;  // Top of tree
    MSG_FolderArray m_stack;         // Stack of pointer indices

    MSG_FolderInfo *m_currentParent; // Currently iterating 
    int             m_currentIndex;
    
                    MSG_FolderIterator(MSG_FolderInfo *parent);
                    ~MSG_FolderIterator();

    XP_Bool         Push();
    XP_Bool         Pop();
    void            Init();
    void            Advance();

    MSG_FolderInfo  *First();
    MSG_FolderInfo  *Next();
    MSG_FolderInfo  *NextNoAdvance();
	MSG_FolderInfo	*AdvanceToFolder(MSG_FolderInfo *);
    XP_Bool         More();
};


#endif /* _MsgFInfo_H_ */
