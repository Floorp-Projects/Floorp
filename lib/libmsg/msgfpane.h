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

#ifndef _MsgFPane_H_
#define _MsgFPane_H_

#include "msglpane.h"
#include "ptrarray.h"
#include "bytearr.h"

class MSG_ThreadPane;
class MailMessageHdr;
class MessageDB;

class MSG_Prefs;
class MSG_FolderInfoMail;
class ListNewsGroupState;
class MSG_FolderArray;
class MSG_FolderPane;
class MSG_RuleTracker;
class MSG_UrlQueue;

class MSG_RemoteFolderCopyState {
public:
	MSG_RemoteFolderCopyState(MSG_FolderInfo *sourceTree, MSG_FolderInfo *destinationTree, MSG_FolderPane *urlPane);
	virtual ~MSG_RemoteFolderCopyState();
	
	virtual MsgERR DoNextAction();
private:
	void SetNextSourceFromParent(MSG_FolderInfo *parent, MSG_FolderInfo *child);
	void SetNextSourceNode();
	
	MsgERR MakeCurrentMailbox();
	MsgERR DoCurrentCopy();
	MsgERR DoUpdateCurrentMailbox();
	
	
	MSG_FolderInfo *m_sourceTree;
	MSG_FolderInfo *m_destinationTree;
	
	MSG_FolderInfo *m_currentSourceNode;
	MSG_FolderInfo *m_currentDestinationParent;
	
	enum eWhatAmIDoing {
		kMakingMailbox,
		kUpdatingMailbox,
		kCopyingMessages
	};
	
	eWhatAmIDoing	m_currentTask;
	
	MSG_FolderPane	   *m_urlPane;
	MessageDB 		   *m_copySourceDB;
};

class MSG_FolderPane : public MSG_LinedPane 
{
public:

	MSG_FolderPane(MWContext* context, MSG_Master* master);
	virtual ~MSG_FolderPane();

	virtual MsgERR	Init();
	virtual MSG_PaneType GetPaneType();

	virtual void OnFolderDeleted (MSG_FolderInfo *);
	virtual void OnFolderChanged(MSG_FolderInfo *folderInfo);
	virtual void OnFolderAdded (MSG_FolderInfo *, MSG_Pane *instigator);

	virtual void NotifyPrefsChange(NotifyCode code);
	
	int GetFolderMaxDepth();

	virtual XP_Bool GetFolderLineByIndex(MSG_ViewIndex line, int32 numlines,
							   MSG_FolderLine* data);
	virtual int GetFolderLevelByIndex( MSG_ViewIndex line );

	char* GetFolderName(MSG_ViewIndex line);
	MSG_ViewIndex GetFolderIndex(MSG_FolderInfo* info, XP_Bool expand = FALSE);
	MSG_FolderInfo* GetFolderInfo(MSG_ViewIndex index);
	MSG_NewsHost *GetNewsHostFromIndex (MSG_ViewIndex index);
	MSG_Host *GetHostFromIndex (MSG_ViewIndex index);

	void AddFoldersToView (MSG_FolderArray &folders, MSG_FolderInfo *parent, int32 folderIdx);
	void AddFolderToView (MSG_FolderInfo *folder, MSG_FolderInfo *parent, int32 folderIdx);
	void MakeFolderVisible (MSG_FolderInfo *folder);
	
	MSG_FolderArray * GetFolderView () { return &m_folderView; }

	virtual void ToggleExpansion(MSG_ViewIndex line, int32* numchanged);
	virtual int32 ExpansionDelta(MSG_ViewIndex line);
	virtual int32 GetNumLines();

	virtual MsgERR DoCommand(MSG_CommandType command,
						   MSG_ViewIndex* indices, int32 numindices);

	virtual MsgERR GetCommandStatus(MSG_CommandType command,
								  const MSG_ViewIndex* indices, int32 numindices,
								  XP_Bool *selectable_p,
								  MSG_COMMAND_CHECK_STATE *selected_p,
								  const char **display_string,
								  XP_Bool *plural_p);

	int LineForFolder(MSG_FolderInfo* folder);

	void SetFlagForFolder (MSG_FolderInfo *folder, uint32 which);
	void ClearFlagForFolder (MSG_FolderInfo *folder, uint32 which);

	// Causes us to recreate the view from the folder tree, and tell the
	// FE to redisplay everything.  Ugly, causes flashes, but it's easy...
	void RedisplayAll();

	MsgERR OpenFolder();
	MsgERR NewFolderWithUI (MSG_ViewIndex *indices, int32 numIndices);

	MsgERR RenameFolder (MSG_FolderInfo *folder, const char *newName = NULL);
	MsgERR RenameOfflineFolder(MSG_FolderInfo *folder, const char *newName);

	// move folders to trash
	MsgERR TrashFolders(MSG_FolderArray&, XP_Bool noTrash = FALSE);
	MsgERR TrashFolders(const MSG_ViewIndex *indices, int32 numIndices, XP_Bool noTrash = FALSE);
	
	// delete the folders and associated disk storage, calls RemoveFolderFromView
	// performPreflight defaults to TRUE, called with FALSE for non-existant imap folders
	MsgERR DeleteFolder (MSG_FolderInfoMail *folder, XP_Bool performPreflight = TRUE);
		
	// remove the folders from the visible folder pane. leave disk storage intact
	MsgERR RemoveFolderFromView(MSG_FolderInfo *doomedFolder);
	
	// Move folders around the hierarchy
	MsgERR MoveFolders (MSG_FolderArray&, MSG_FolderInfo *dest, XP_Bool needExitFunc = FALSE);
	MsgERR MoveFolders (const MSG_ViewIndex *indices, int32 numIndices, MSG_FolderInfo *dest, XP_Bool needExitFunc = FALSE);
	MSG_DragEffect DragFoldersStatus (const MSG_ViewIndex *indices, int32 numIndices,
						MSG_FolderInfo *dest, MSG_DragEffect request);

	// Helpers for MoveFolders
	int PreflightMoveFolder (MSG_FolderInfo *src, MSG_FolderInfo *srcParent, MSG_FolderInfo *dest);
	int PreflightMoveFolderWithUI (MSG_FolderInfo *src, MSG_FolderInfo *srcParent, MSG_FolderInfo *dest);
	MsgERR PerformLegalFolderMove (MSG_FolderInfo *newChild, MSG_FolderInfo *newParent);
	MsgERR MakeMoveFoldersArray (const MSG_ViewIndex *indices, int32 numIndices, MSG_FolderArray *outArray);

	MsgERR CompressFolder(MSG_ViewIndex *indices, int32 numIndices);
	MsgERR AddNewsGroup();
	MsgERR DeleteVirtualNewsgroup (MSG_FolderInfo*);
	MsgERR UpdateMessageCounts(MSG_ViewIndex *indices, int32 numIndices);
	MsgERR Undo();
	MsgERR Redo();
	// Undo helper functions
	MsgERR UndoMoveFolder (MSG_FolderInfo *srcParent, MSG_FolderInfo *src, MSG_FolderInfo *dest);

	XP_Bool CanUndo();
	XP_Bool CanRedo();
	MsgERR MarkAllMessagesRead(MSG_ViewIndex* indices, int32 numIndices);

	MsgERR CheckForNew(MSG_NewsHost* host);

	void	InsertFlagsArrayAt(MSG_ViewIndex index, MSG_FolderArray &folders);
	virtual void	PostProcessRemoteCopyAction();
	static void RuleTrackCB (void *cookie, XPPtrArray& rules, XPDWordArray &actions, XP_Bool isDelete);

	static void RemoteFolderCopySelectCompleteExitFunction(URL_Struct *URL_s, int status, MWContext *context);
	static void SafeToSelectExitFunction(URL_Struct *URL_s, int status, MWContext *context);
	static void SafeToSelectInterruptFunction (MSG_UrlQueue *queue, URL_Struct *URL_s, int status, MWContext *context);

	void	RefreshUpdatedIMAPHosts();	// updates hosts after changes from the subscribe pane

protected:
	virtual void FlushUpdates();
	virtual void OnToggleExpansion(MSG_FolderInfo* toggledline,
								   MSG_ViewIndex line, int32 countVector);

	char	*RefreshIMAPHostFolders(MSG_FolderInfo *hostinfo, XP_Bool runURL);	// refreshes the folder list from the server, for a given IMAP host
	virtual void GetExpansionArray(MSG_FolderInfo *folder, MSG_FolderArray &array);
	XP_Bool ScanFolder_1();
	static void ScanFolder_s(void* closure);
	void ScanFolder();
	char *GetValidNewFolderName(MSG_FolderInfo *parent, int captionId, MSG_FolderInfo *folder);

	MsgERR PreflightRename(MSG_FolderInfo *folder, int depth);

	// remove the indexed folders from the visible folder pane. 
	MsgERR RemoveIndexedFolderFromView(MSG_ViewIndex doomedIndex, MSG_FolderArray &children, uint32 doomedFlags, MSG_FolderInfo *parentOfDoomed);

	XP_Bool IndicesAreMail (const MSG_ViewIndex*, int32);
	XP_Bool IndicesAreNews (const MSG_ViewIndex*, int32);
	XP_Bool AnyIndicesAreIMAPMail (const MSG_ViewIndex*, int32);

	static void OpenFolderCB_s(MWContext* context, char* filename,
							 void* closure);
	void OpenFolderCB(char* filename);
	virtual void UpdateNewsCountsDone(int status);
	virtual void CheckForNewDone(URL_Struct* url_struct, int status,
								 MWContext* context);

	MsgERR RenameOnlineFolder(MSG_FolderInfo *folder, const char *newName);

	XP_Bool OneFolderInArrayIsIMAP(MSG_FolderArray &theFolders);

	MSG_FolderArray m_folderView;	// Array of displayed MSG_FolderInfo*
	XP_Bool m_needToolbarUpdate;

	void* m_scanTimer;
	XP_Bool m_wantCompress;
	XP_Bool m_offeredCompress;
	XP_Bool m_confirmedDeleteFolders;

	XPByteArray	m_flagsArray;
	MSG_RemoteFolderCopyState *m_RemoteCopyState;
};


#endif /* _MsgFPane_H_ */
