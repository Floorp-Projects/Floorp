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


#ifndef _MsgPane_H_
#define _MsgPane_H_

#include "msgprnot.h"
#include "chngntfy.h"
#include "idarray.h"
#include "errcode.h"
#include "msgundmg.h"
#include "msg.h"
#include "abcom.h" // required for adding sender or recipients to address books.

class MSG_Master;
class MessageDBView;
class MSG_FolderInfo;
class MSG_NewsHost;
class ParseMailboxState;
class msg_Background;
class OfflineImapGoOnlineState;
class MSG_FolderInfoMail;
class MSG_PostDeliveryActionInfo;

struct tImapFilterClosure;

struct msg_incorporate_state;
struct MessageHdrStruct;



class PaneListener : public ChangeListener
{
public:
	PaneListener(MSG_Pane *pPane);
	virtual ~PaneListener();
	virtual void OnViewChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnViewStartChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnViewEndChange(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator);
	virtual void OnKeyChange(MessageKey keyChanged, int32 flags, 
		ChangeListener * instigator);
	virtual void OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
	virtual void OnAnnouncerChangingView(ChangeAnnouncer * /* instigator */, MessageDBView * /* view */) ;
	virtual void StartKeysChanging();
	virtual void EndKeysChanging();

protected:
	MSG_Pane		*m_pPane;
	XP_Bool			m_keysChanging;	// are keys changing?
	XP_Bool			m_keyChanged;	// has a key changed since StartKeysChanging called?
};

// If a MSG_Pane has its url chain ptr set to a non-null value,
// it calls the GetNextURL method whenever it finishes a url that is chainable.
// These include delivering queued mail,  get new mail, and retrieving
// messages for offline use, oddly enough - the three kinds of urls I need to queue.
// Sadly, neither the msg_Background or MSG_UrlQueue do what I want,
// because I need to chain network urls that have their own exit functions
// and indeed chain urls themselves.
class MSG_PaneURLChain
{
public:
	MSG_PaneURLChain(MSG_Pane *pane);
	virtual ~MSG_PaneURLChain();
	virtual int		GetNextURL();	// return 0 to stop chaining.
protected:
	MSG_Pane	*m_pane;
};

class MSG_Pane : public MSG_PrefsNotify {
public:

  // hack..
  // Find a pane of the given type that matches the given context.  If none,
  // find some other pane of the given type (if !contextMustMatch).
  static MSG_Pane* FindPane(MWContext* context,
							MSG_PaneType type = MSG_ANYPANE,
							XP_Bool contextMustMatch = FALSE);

  static XP_Bool PaneInMasterList(MSG_Pane *pane);


  static MSG_PaneType PaneTypeForURL(const char *url);
  XP_Bool		NavigationGoesToNextFolder(MSG_MotionType motionType);
  MSG_Pane(MWContext* context, MSG_Master* master);
  virtual ~MSG_Pane();

  void SetFEData(void*);
  void* GetFEData();

  virtual XP_Bool IsLinePane();
  virtual MSG_PaneType GetPaneType() ;
  virtual void NotifyPrefsChange(NotifyCode /*code*/) ;

  virtual MSG_Pane* GetParentPane();
  
  MSG_Pane* GetNextPane() {return m_nextPane;}

  MSG_Pane *GetFirstPaneForContext(MWContext *context);

  MSG_Pane *GetNextPaneForContext(MSG_Pane *pane, MWContext *context);

  virtual MWContext* GetContext();
  MSG_Prefs* GetPrefs();

  MSG_Master* GetMaster() {return m_master;}

  // these are here mostly to allow thread panes and message panes
  // to handle the same commands - if we had a common base class
  // for these (other than MSG_Pane), we could put these there.
  virtual MessageDBView *GetMsgView();
  virtual void SetMsgView(MessageDBView *);
  virtual void SwitchView(MessageDBView *) ;
  virtual MSG_FolderInfo *GetFolder() ;
  virtual void SetFolder(MSG_FolderInfo *info);
  virtual void StartingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/);
  virtual void EndingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/);

	// do not use this function unless you really know what you are doing.
	// Currently, it is used when a url queue for loading an imap folder
	// is interrupted
	virtual void CrushUpdateLevelToZero();

	virtual void OnFolderChanged(MSG_FolderInfo *folder);
	virtual void OnFolderDeleted (MSG_FolderInfo *folder);
	virtual void OnFolderAdded (MSG_FolderInfo *folder, MSG_Pane *instigator);
	virtual void OnFolderKeysAreInvalid (MSG_FolderInfo *folder);

	/* New address book requires a destination address book because it supports multiple ABs */
	virtual MsgERR AddToAddressBook(MSG_CommandType command, MSG_ViewIndex*indices, int32 numIndices, AB_ContainerInfo * destAB);

  virtual MsgERR DoCommand(MSG_CommandType command,
						   MSG_ViewIndex* indices, int32 numindices);
  virtual MsgERR GetMailForAFolder(MSG_FolderInfo *folder);

  virtual MsgERR GetCommandStatus(MSG_CommandType command,
								  const MSG_ViewIndex* indices, int32 numindices,
								  XP_Bool *selectable_p,
								  MSG_COMMAND_CHECK_STATE *selected_p,
								  const char **display_string,
								  XP_Bool *plural_p);

  virtual MsgERR SetToggleStatus(MSG_CommandType command,
								 MSG_ViewIndex* indices, int32 numindices,
								 MSG_COMMAND_CHECK_STATE value);

  virtual MSG_COMMAND_CHECK_STATE GetToggleStatus(MSG_CommandType command,
												  MSG_ViewIndex* indices,
												  int32 numindices);

  virtual MsgERR GetNavigateStatus(MSG_MotionType motion, MSG_ViewIndex index, 
								  XP_Bool *selectable_p,
								  const char **display_string);

  MSG_FolderInfo* FindFolderOfType(int type);


  MSG_FolderInfo* FindMailFolder(const char* pathname,
								 XP_Bool createIfMissing);

  virtual MsgERR MarkReadByDate (time_t startDate, time_t endDate);

  virtual XP_Bool SetMessagePriority(MessageKey key, MSG_PRIORITY priority);

  MsgERR ComposeNewMessage();
  //ComposeMessageToMany calls ComposeNewMessage if nothing was selected
  //otherwise it builds a string containing selected groups to post to.
  MsgERR ComposeMessageToMany(MSG_ViewIndex* indices, int32 numIndices);

  virtual MsgERR	ComposeNewsMessage(MSG_FolderInfo *folder);
  char* CreateForwardSubject(MessageHdrStruct* header);

  virtual void InterruptContext(XP_Bool safetoo);

  virtual int BeginOpenFolderSock(const char *folder_name,
								  const char *message_id, int32 msgnum,
								  void **folder_ptr);
  virtual int FinishOpenFolderSock(const char* folder_name,
								   const char* message_id,
								   int32 msgnum, void** folder_ptr);
  virtual void CloseFolderSock(const char* folder_name, const char* message_id,
							   int32 msgnum, void* folder_ptr);

  virtual int OpenMessageSock(const char *folder_name, const char *msg_id,
							  int32 msgnum, void *folder_ptr, void **message_ptr,
							  int32 *content_length);
  virtual int ReadMessageSock(const char* folder_name, void* message_ptr,
							  const char* message_id, int32 msgnum, char* buffer,
							  int32 buffer_size);
  virtual void CloseMessageSock(const char* folder_name, const char* message_id,
							    int32 msgnum, void* message_ptr);

  virtual MsgERR GetKeyFromMessageId (const char *message_id, MessageKey *outId);
  virtual int MarkMessageKeyRead(MessageKey key, const char* xref);

  virtual MSG_ViewIndex GetThreadIndexOfMsg(MessageKey key);

  virtual MsgERR ViewNavigate(MSG_MotionType /*motion*/,
						MSG_ViewIndex /*startIndex*/, 
						MessageKey * /*resultKey*/, MSG_ViewIndex * /*resultIndex*/, 
						MSG_ViewIndex * /*pThreadIndex*/,
						MSG_FolderInfo ** /*ppFolderInfo*/);


#ifdef XP_UNIX
	void IncorporateFromFile(XP_File infid, void (*donefunc)(void*, XP_Bool),
						   void* doneclosure);
#endif

	XP_Bool BeginMailDelivery();
	void	EndMailDelivery();
	void*	IncorporateBegin(FO_Present_Types format_out, char* pop3_uidl, 
							 URL_Struct* url, uint32 flags);
	MsgERR	IncorporateWrite(void* closure, const char* block, int32 length);
	MsgERR	IncorporateComplete(void* closure);
	MsgERR	IncorporateAbort(void* closure, int status);
	void	ClearSenderAuthedFlag(void *closure);
	MsgERR	GetNewNewsMessages(MSG_Pane *parentPane, MSG_FolderInfo *folder, XP_Bool getOld = FALSE);
	MsgERR GetNewMail(MSG_Pane *parentPane, MSG_FolderInfo *folder);
	static	void GetNewMailExit (URL_Struct *URL_s, int status, MWContext *window_id);

	char *GetUIDL() {return m_incUidl;}

	MsgERR UpdateNewsCounts(MSG_NewsHost* host);

	virtual int GetURL (URL_Struct *url, XP_Bool isSafe);
	// return a url_struct with a url that refers to this pane.
	virtual URL_Struct *  ConstructUrlForMessage(MessageKey key = MSG_MESSAGEKEYNONE);
	static URL_Struct *  ConstructUrl(MSG_FolderInfo *folder);
	MsgERR TrashMessages (MSG_ViewIndex* indices, int32 numindices);
	MsgERR DeleteMessages (MSG_FolderInfo *folder, MSG_ViewIndex* indices, int32 numindices);

	static void CancelDone(URL_Struct *url, int status, MWContext *context);
	int CancelMessage(MSG_ViewIndex index);

	// Move/copy operations

	// made these virtual for the search as view operations on the search pane.
	virtual MsgERR MoveMessages (const MSG_ViewIndex *indices,
							int32 numIndices,
							MSG_FolderInfo *folder);

	virtual MsgERR CopyMessages (const MSG_ViewIndex *indices, 
							int32 numIndices, 
							MSG_FolderInfo *folder,
							XP_Bool deleteAfterCopy);

	virtual MSG_DragEffect DragMessagesStatus (const MSG_ViewIndex *indices, 
							int32 numIndices, 
							MSG_FolderInfo *folder,
							MSG_DragEffect request);

	MsgERR CopyMessages (const MSG_ViewIndex *indices, 
							int32 numIndices, 
							const char *folderPath,
							XP_Bool deleteAfterCopy);

	MSG_DragEffect DragMessagesStatus (const MSG_ViewIndex *indices, 
							int32 numIndices, 
							const char *folderPath,
							MSG_DragEffect request);


	// moved from MSG_FolderPane; this enables compressing a folder
	// in a compose pane in case we need to reclaim some disk space
	// after we send/save a draft
	MsgERR CompressAllFolders();
	MsgERR EmptyTrash(MSG_FolderInfo *curFolder);
	MsgERR EmptyImapTrash(MSG_IMAPHost *host);
	
	// move some implementation from MSG_FolderPane.  Compress the one
	// specified mail folder whether it be imap or pop
	MsgERR CompressOneMailFolder(MSG_FolderInfoMail *mailFolder);

	int BeginCompressFolder(URL_Struct* url, const char* foldername,
						  void** closure);
	int FinishCompressFolder(URL_Struct* url, const char* foldername,
						   void* closure);
	int CloseCompressFolderSock(URL_Struct* url, void* closure);

	// saves the corresponding keys in m_saveKeys.
	int SaveIndicesAsKeys(MSG_ViewIndex* indices, int32 numindices);

	virtual int32 GetNewsRCCount(MSG_NewsHost* host);
	virtual char* GetNewsRCGroup(MSG_NewsHost* host);
	virtual int DisplaySubscribedGroup(MSG_NewsHost* host,
									   const char *group,
									   int32 oldest_message,
									   int32 youngest_message,
									   int32 total_messages,
									   XP_Bool nowvisiting);

	virtual int AddNewNewsGroup(MSG_NewsHost* host, const char* groupname,
						int32 oldest, int32 youngest, const char *flag, XP_Bool bXactiveFlags);

	MsgERR CheckForNew(MSG_NewsHost* host);

	virtual UndoManager *GetUndoManager ();
	virtual BacktrackManager *GetBacktrackManager();

	MsgERR DeliverQueuedMessages();
	int BeginDeliverQueued(URL_Struct* url, void** closure);
	int FinishDeliverQueued(URL_Struct* url, void* closure);
	int CloseDeliverQueuedSock(URL_Struct* url, void* closure);
	static void PostDeliverQueuedExitFunc(URL_Struct *url, int status, MWContext *context);
	
	// related to running the asynch imap filters in a pane
	virtual void StoreImapFilterClosureData( tImapFilterClosure *closureData );
	virtual void ClearImapFilterClosureData();
	virtual tImapFilterClosure *GetImapFilterClosureData();
	
	// set when loading an imap folder
	MSG_IMAPFolderInfoMail *GetLoadingImapFolder() { return m_loadingImapFolder ; }
	void SetLoadingImapFolder(MSG_IMAPFolderInfoMail *folder) { m_loadingImapFolder=folder ; }
	
	// set when the current folder load is kicking off imap filters
	void SetActiveImapFiltering(XP_Bool isFiltering) { m_ActiveImapFilters = isFiltering; }
	XP_Bool GetActiveImapFiltering() { return m_ActiveImapFilters; }
	
	
	void		SetGoOnlineState(OfflineImapGoOnlineState *state) { m_goOnlineState = state; }
	OfflineImapGoOnlineState *GetGoOnlineState() { return m_goOnlineState; }

	// Removes this pane from the main pane list.  This is so that calls to
	// MSG_Master::FindPaneOfType() won't find this one (because, for example,
	// we know we're about to delete this one.)
	void UnregisterFromPaneList();


	// These routines should be used only by the msg_Background class.
	msg_Background* GetCurrentBackgroundJob() {return m_background;}
	void SetCurrentBackgroundJob(msg_Background* b) {m_background = b;}

	void ClearURLChain() {delete m_urlChain; m_urlChain = NULL;}
	MSG_PaneURLChain *GetURLChain() {return m_urlChain;}
	void			  SetURLChain(MSG_PaneURLChain *chain) {m_urlChain = chain;}
static void				GetNextURLInChain_CB(URL_Struct* urlstruct, int status,  MWContext* context);
	void				GetNextURLInChain();

	// Open Message as Draft
  MsgERR OpenMessageAsDraft(MSG_ViewIndex* indices, int32 numIndices, 
							XP_Bool bFwdInline = FALSE);

	// News server admin stuff
	XP_Bool ModerateNewsgroupStatus (MSG_FolderInfo *folder);
	MsgERR ModerateNewsgroup (MSG_FolderInfo *folder);
	XP_Bool NewNewsgroupStatus (MSG_FolderInfo *folder);
	MsgERR NewNewsgroup (MSG_FolderInfo *folder, XP_Bool createCategory = FALSE);
	static int CompareViewIndices (const void *v1, const void *v2);
	MsgERR ApplyCommandToIndices(MSG_CommandType command, MSG_ViewIndex* indices, int32 numIndices);
	void SetShowingProgress(XP_Bool showingProgress) {m_showingProgress = showingProgress;}

	MsgERR ManageMailAccount(MSG_FolderInfo *folder);
    static void ManageMailAccountExitFunc(URL_Struct *url,
										  int status,
										  MWContext *context);
										  
	MsgERR PreflightDeleteFolder (MSG_FolderInfo *folder, XP_Bool getUserConfirmation);
		
	void SetNumberOfNewImapMailboxes(uint32 numberNew) { m_NumberOfNewImapMailboxes = numberNew; }
	uint32 GetNumberOfNewImapMailboxes() { return m_NumberOfNewImapMailboxes; }
	virtual PaneListener *GetListener();
	MsgERR	CloseView();
	MsgERR	ListThreads();
	void	GroupNotFound(MSG_NewsHost* host, const char *group, XP_Bool opening);

	XP_Bool DisplayingRecipients ();
	
	void    SetPreImapFolderVerifyUrlExitFunction(Net_GetUrlExitFunc *func) { m_PreImapFolderVerifyUrlExitFunction = func; }
	Net_GetUrlExitFunc *GetPreImapFolderVerifyUrlExitFunction() { return m_PreImapFolderVerifyUrlExitFunction; }

	void SetRequestForReturnReceipt(XP_Bool isNeeded);
	XP_Bool GetRequestForReturnReceipt();

	void SetSendingMDNInProgress(XP_Bool inProgress);
	XP_Bool GetSendingMDNInProgress();

    MSG_PostDeliveryActionInfo *GetPostDeliveryActionInfo ();
    void SetPostDeliveryActionInfo ( MSG_PostDeliveryActionInfo *actionInfo );
	virtual void SetIMAPListInProgress(XP_Bool inProgress);
	virtual XP_Bool IMAPListInProgress();
	virtual void SetIMAPListMailboxExist(XP_Bool bExist);
	virtual XP_Bool IMAPListMailboxExist();
	
	static void PostLiteSelectExitFunc( URL_Struct *url, int status, MWContext *context);
	static void PostDeleteIMAPOldDraftUID(URL_Struct* url_struct, int status, MWContext *context);
	void DeleteIMAPOldDraftUID(MSG_PostDeliveryActionInfo *actionInfo, MSG_Pane *urlPane = NULL);
	int GetNumstack() { return m_numstack; };
	void AdoptProgressContext(MWContext *context);

	char* MakeMailto(const char *to, const char *cc,
					const char *newsgroups,
					const char *subject, const char *references,
					const char *attachment, const char *host_data,
					XP_Bool xxx_p, XP_Bool sign_p);

	/* used with the new address book....all panes must register a entry property sheet call back function if 
	they want the ability to add sender/all to the address book */
	void SetShowPropSheetForEntryFunc(AB_ShowPropertySheetForEntryFunc * func) { m_entryPropSheetFunc = func; }
	AB_ShowPropertySheetForEntryFunc * GetShowPropSheetForEntryFunc() { return m_entryPropSheetFunc;}

protected:
	char* ComputeNewshostArg();
	// GetNewMail helper routines
	msg_incorporate_state *CreateIncorporateState ();
	int OpenDestFolder(msg_incorporate_state* state);
	int CloseDestFolder(msg_incorporate_state* state);
	void ResolveIndices (MessageDBView *view, const MSG_ViewIndex *indices, int32 numIndices, IDArray*);
	static void IncorporateShufflePartial_s(URL_Struct *url, int status,
											MWContext *context);
	virtual void IncorporateShufflePartial(URL_Struct *url, int status,
										   MWContext *context);

	void				ClearNewInOpenFolders(MSG_FolderInfo *folderOfGetNewMsgs);
	ParseMailboxState	*GetParseMailboxState(const char *folderName);

	// Callback for FE_PromptForFileName
	static void SaveMessagesAsCB(MWContext *context, char *file_name,
								 void *closure);

	// callback function for showing a property sheet pane. Used by address book panes, message pane and thread pane...
	AB_ShowPropertySheetForEntryFunc * m_entryPropSheetFunc;

	static void UpdateNewsCountsDone_s(URL_Struct*, int status, MWContext*);
	virtual void UpdateNewsCountsDone(int status);

	// Whether newly discovered newsgroups should have the "new" bit set on
	// them.
	virtual XP_Bool AddGroupsAsNew();

	static void CheckForNewDone_s(URL_Struct* url_struct, int status,
								  MWContext* context);
	virtual void CheckForNewDone(URL_Struct* url_struct, int status,
								 MWContext* context);

	virtual XP_Bool	ShouldDeleteInBackground();


  IDArray		m_saveKeys;		// this is used to save keys used for a
								// pane command in a callback from FE_PromptForFileName.
								// This is ugly but we only get one closure 
								// and that's our this ptr.

  static MSG_Pane* MasterList;	
  MSG_Pane* m_nextInMasterList;	

  
  MSG_Pane* m_nextPane;			// Link of panes created with the same master.
  
  
  MSG_Master* m_master;
  MWContext* m_context;
  MSG_Prefs* m_prefs;
  void* m_fedata;
  int m_numstack;				// used for DEBUG, and to tell listeners
								// if we're in an update block.
  
  // get new mail handling state
  char *m_incUidl;	// Special UIDL to inc from, if any. hack

  int32 m_numNewGroups;			// How many new newsgroups have been noticed
								// from the news server.

  UndoManager *m_undoManager;
  BacktrackManager *m_backtrackManager;
  
  tImapFilterClosure *m_ImapFilterData;
  
  OfflineImapGoOnlineState *m_goOnlineState;
  
  XP_Bool m_ActiveImapFilters;
  MSG_IMAPFolderInfoMail *m_loadingImapFolder;

  static XP_Bool m_warnedInvalidHostTable;

  msg_Background* m_background;
  MSG_PaneURLChain	*m_urlChain;
  MSG_NewsHost* m_hostCheckingForNew;
  time_t m_checkForNewStartTime;
  XP_Bool	m_showingProgress;
  XP_Bool m_requestForReturnReceipt;
  XP_Bool m_sendingMDNInProgress;

  uint32	m_NumberOfNewImapMailboxes;
  
  Net_GetUrlExitFunc *m_PreImapFolderVerifyUrlExitFunction;
  MSG_PostDeliveryActionInfo *m_actionInfo;
  XP_Bool	m_imapListInProgress;
  XP_Bool	m_imapListMailboxExist;

  msg_YesNoDontKnow m_displayRecipients;
  MWContext	*m_progressContext;
};


/* Message post delivery action info
 * Used for deleting draft message when done sending 
 * or turning on the Replied/Forwarded flag after
 * successfully sending/forwarding a message
 */

class MSG_PostDeliveryActionInfo : public MSG_ZapIt
{
public:
  MSG_PostDeliveryActionInfo(MSG_FolderInfo *folderInfo);

  MSG_FolderInfo *m_folderInfo;
  XPDWordArray m_msgKeyArray;		/* origianl message keyArray */
  uint32 m_flags;
};



#endif /* _MsgPane_H_ */
