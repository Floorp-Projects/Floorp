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
#ifndef imapoff_H
#define imapoff_H

class MailDB;
class MailMessageHdr;
class MSG_IMAPFolderInfoMail;
class DBOfflineImapOperation;
class MSG_UrlQueue;
class MsgDocument;
class DownloadOfflineImapState;

#include "net.h"
#include "msgcom.h"
#include "msgimap.h"
#include "msgzap.h"
#include "msgdbtyp.h"

class XPStringObj;

class OfflineImapGoOnlineState : public MSG_ZapIt {
public:												// set to one folder to playback one folder only
	OfflineImapGoOnlineState(MSG_Pane *workerPane, MSG_IMAPFolderInfoMail *singleFolderOnly = NULL);
	virtual ~OfflineImapGoOnlineState();

	void		ProcessNextOperation();
	
	uint32		GetCurrentUIDValidity() { return mCurrentUIDValidity; }
	void		SetCurrentUIDValidity(uint32 uidvalidity) { mCurrentUIDValidity = uidvalidity; }
	
	void		SetPseudoOffline(XP_Bool pseudoOffline) {m_pseudoOffline = pseudoOffline;}
	XP_Bool		ProcessingStaleFolderUpdate() { return m_singleFolderToUpdate != NULL; }

	MSG_UrlQueue *GetUrlQueue(XP_Bool *alreadyRunning);
	DownloadOfflineImapState *GetDownloadOfflineImapState() {return m_downloadOfflineImapState;}
	void		SetDownloadOfflineImapState(DownloadOfflineImapState *state) {m_downloadOfflineImapState = state;}
	XP_Bool		CreateOfflineFolder(MSG_FolderInfo *folder);
private:
	XP_Bool		CreateOfflineFolders();
	void 		AdvanceToNextFolder();
	void 		DeleteAllOfflineOpsForCurrentDB();
	
	void		ProcessFlagOperation(DBOfflineImapOperation *currentOp);
	void		ProcessMoveOperation(DBOfflineImapOperation *currentOp);
	void		ProcessCopyOperation(DBOfflineImapOperation *currentOp);
	void		ProcessEmptyTrash(DBOfflineImapOperation *currentOp);
	void		ProcessAppendMsgOperation(DBOfflineImapOperation *currentOp,
										  int32 opType);
	static void PostAppendMsgExitFunction(URL_Struct *URL_s, int status,
										  MWContext *window_id);
	
	MSG_Pane			*m_workerPane;
	MSG_FolderInfoMail	*m_currentFolder;
	MSG_IMAPFolderInfoMail *m_singleFolderToUpdate;
	MSG_FolderIterator	*m_folderIterator;
	IDArray				m_CurrentKeys;
	int					m_KeyIndex;
	MailDB				*m_currentDB;
	uint32				mCurrentUIDValidity;
	int32				mCurrentPlaybackOpType;	// kFlagsChanged -> kMsgCopy -> kMsgMoved
	XP_Bool				m_mailboxupdatesStarted;
	XP_Bool				m_pseudoOffline;		// for queueing online events in offline db
	XP_Bool				m_createdOfflineFolders;
	MSG_UrlQueue		*m_urlQueue;	
	DownloadOfflineImapState *m_downloadOfflineImapState;

};


class OfflineImapState : public MSG_ZapIt 
{
public:
	OfflineImapState(MSG_IMAPFolderInfoMail *folder);  
	virtual ~OfflineImapState();
protected:
	void		SetFolderInfo(MSG_IMAPFolderInfoMail *folder);
	MailDB			*m_maildb;
	MailMessageHdr	*m_msgHdr;
	MSG_IMAPFolderInfoMail *m_folderInfo;
};


// used for streaming msg bodies into the db
class DownloadOfflineImapState : public OfflineImapState
{
public:
	DownloadOfflineImapState(MSG_IMAPFolderInfoMail *folder, NET_StreamClass *displayStream);  // only called by CreateOfflineImapStream
	virtual ~DownloadOfflineImapState();
	
	static void CreateOfflineImapStream(NET_StreamClass *theStream, MSG_IMAPFolderInfoMail *folder, NET_StreamClass *displayStream, DownloadOfflineImapState *closureData = NULL);

	static unsigned int OfflineImapStreamWriteReady (NET_StreamClass *stream) { return MAX_WRITE_READY; }
	static void OfflineImapStreamComplete(NET_StreamClass *stream);
	static void OfflineImapStreamAbort (NET_StreamClass *stream, int status);
	static int OfflineImapStreamWrite (NET_StreamClass *stream, const char *block, int32 messageKey);
	
	int		WriteStream(const char *block, uint32 messageKey);
	void	CompleteStream();
	void	AbortStream(int status);
	MSG_OfflineMsgDocumentHandle	m_dbWriteDocument;
	void	SetDeleteOnComplete(XP_Bool deleteOnComplete) {m_deleteOnComplete = deleteOnComplete;}
private:
	NET_StreamClass *m_displayStream;
	XP_Bool m_offlineWriteFailure;
	XP_Bool	m_deleteOnComplete;
};


// used for streaming bodies from the db into the display
class DisplayOfflineImapState : public OfflineImapState
{
public:
	DisplayOfflineImapState(MSG_IMAPFolderInfoMail *folder, MessageKey key);  
	virtual ~DisplayOfflineImapState();
	
	uint32 GetMsgSize();
	
	int ProcessDisplay(char *socketBuffer, uint32 read_size);
protected:
	uint32 	m_bytesDisplayedSoFar;
	XP_Bool	m_wasUnread;
};


// used for append msg operation
class AppendMsgOfflineImapState : public OfflineImapState
{
public:
	AppendMsgOfflineImapState(MSG_IMAPFolderInfoMail *folder, MessageKey key,
							  const char *msg_file_name);
	virtual ~AppendMsgOfflineImapState();
	void RemoveHdrFromDB();
	void RemoveMsgFile();
protected:
	char *m_msg_file_name;
};


// new access type of stored imap operations

const int32 kFlagsChanged		= 0x1;
const int32 kMsgMoved			= 0x2;
const int32 kMsgCopy			= 0x4;
const int32 kMoveResult			= 0x8;
const int32 kAppendDraft        = 0x10;
const int32 kAddedHeader		= 0x20;
const int32 kDeletedMsg			= 0x40;
const int32 kMsgMarkedDeleted	= 0x80;
const int32 kAppendTemplate     = 0x100;
const int32 kDeleteAllMsgs		= 0x200;

class DBOfflineImapOperation 
{
public:
						/** Instance Methods **/
						DBOfflineImapOperation();
	virtual				~DBOfflineImapOperation();

	void				SetMessageKey(MessageId key);
	void				SetInitialImapFlags(imapMessageFlagsType flags); // the flags we start with
	
	void				SetImapFlagOperation(imapMessageFlagsType flags);
	void				ClearImapFlagOperation();
	
		// you can only move (copy,delete) a msg once
	void				SetMessageMoveOperation(const char *destinationBox);
	void				ClearMoveOperation(MSG_DBHandle dbHandle);
	
		// you can copy a message more than once
	void				AddMessageCopyOperation(const char *destinationBox);
	void				ClearCopyOperations();
	
	void				SetSourceMailbox(const char *sourceMailBox, MessageKey sourceKey);
	
	void				SetDeleteAllMsgs();
	void				ClearDeleteAllMsgs();

	MessageKey							GetMessageKey();
	uint32								GetOperationFlags() ;
	imapMessageFlagsType				GetNewMessageFlags() ;
	
	XP_Bool				GetMoveDestination(MSG_DBHandle dbHandle, XPStringObj &boxName);

	uint32				GetNumberOfCopyOps();
	XP_Bool				GetIndexedCopyDestination(MSG_DBHandle dbHandle, uint32 index, XPStringObj &boxName);
	void				ClearFirstCopyOperation(MSG_DBHandle dbHandle);
	
	XP_Bool				GetSourceInfo(XPStringObj &sourceBoxName, MessageKey &sourceKey);

	void                SetAppendMsgOperation(const char *destinationBox,
											  int32 opType);
	void                ClearAppendMsgOperation(MSG_DBHandle dbHandle, int32 opType);
	MSG_OfflineIMAPOperationHandle GetHandle() {return m_offlineIMAPOperationHandle;}
	void				SetHandle(MSG_OfflineIMAPOperationHandle handle) {m_offlineIMAPOperationHandle = handle;}
	void				SetDBHandle(MSG_DBHandle dbHandle) {m_dbHandle = dbHandle;}

protected:
	MSG_OfflineIMAPOperationHandle m_offlineIMAPOperationHandle;
	MSG_DBHandle			m_dbHandle;	// which db is this in?
	
};
	



#endif
