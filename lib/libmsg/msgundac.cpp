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
/*   msgundac.cpp --- Msg undo actions implementation
 */

#include "msg.h"
#include "errcode.h"
#include "xp_str.h"
#include "xpgetstr.h"
#include "msgdbvw.h"
#include "maildb.h"
#include "msgmpane.h"
#include "msgundac.h"
#include "msgutils.h"
#include "msgprefs.h"
#include "msgtpane.h"
#include "msgfpane.h"
#include "prsembst.h"
#include "msgimap.h"
#include "imapoff.h"

extern "C"
{
	extern int MK_MSG_ID_NOT_IN_FOLDER;
}

//////////////////////////////////////////////////////////////////////
// **** MarkMessageUndoAction
//////////////////////////////////////////////////////////////////////
MarkMessageUndoAction::MarkMessageUndoAction(MSG_Pane *pane,
											 MSG_CommandType command,
											 MSG_ViewIndex *indices,
											 int32 numIndices,
											 MSG_FolderInfo *folder)
{
	XP_ASSERT (pane && folder);
	MessageDBView	*view = pane->GetMsgView();
	if (view)
	{
		for (int i=0; i < numIndices; i++)
			m_keyArray.Add( view->GetAt(*(indices+i)));
	}
	m_pane = pane;
	m_command = command;
	m_folder = folder;
}

MarkMessageUndoAction::MarkMessageUndoAction(MSG_Pane *pane,
											 MSG_CommandType command,
											 MSG_FolderInfo *folder)
{
	XP_ASSERT (pane && folder);

	m_pane = pane;
	m_command = command;
	m_folder = folder;
}

MarkMessageUndoAction::~MarkMessageUndoAction()
{
}

void
MarkMessageUndoAction::AddKey(MessageKey key)
{
  XP_ASSERT(MSG_MESSAGEKEYNONE != key);
  if (MSG_MESSAGEKEYNONE == key) return;
  m_keyArray.Add(key);
}

MSG_CommandType
MarkMessageUndoAction::GetUndoMarkCommand()
{
	if (m_owner->GetState() == UndoRedoing)
		return m_command;
	switch (m_command)
	{
	case MSG_MarkMessages:
		return MSG_UnmarkMessages;
	case MSG_UnmarkMessages:
		return MSG_MarkMessages;
	case MSG_MarkMessagesRead:
		return MSG_MarkMessagesUnread;
	case MSG_MarkMessagesUnread:
		return MSG_MarkMessagesRead;
	case MSG_ToggleMessageRead:
		return MSG_ToggleMessageRead;
	default:
		return m_command;
	}
}

UndoStatus
MarkMessageUndoAction::DoUndo()
{
	XP_ASSERT (m_pane);
	MessageDBView *view;
	MSG_ViewIndex viewIndex = MSG_VIEWINDEXNONE;
	uint i, size = m_keyArray.GetSize();

	if (!size) return UndoFailed;

	MsgERR status = eSUCCESS;

	if (m_pane->GetFolder() != m_folder) {
		if (m_pane->GetPaneType() == MSG_MESSAGEPANE)
			((MSG_MessagePane*)m_pane)->LoadMessage(m_folder, m_keyArray.GetAt(size-1), NULL, TRUE);
		else if (m_pane->GetPaneType() == MSG_THREADPANE)
			((MSG_ThreadPane*)m_pane)->LoadFolder(m_folder);
	}

	view = m_pane->GetMsgView();

	if (view) {
		for (i=0; i < size; i++)
		{
			viewIndex = view->FindKey (m_keyArray.GetAt(i), TRUE);
			if ( viewIndex != MSG_VIEWINDEXNONE )
			{
				m_pane->StartingUpdate(MSG_NotifyNone, viewIndex, 1);
				status = m_pane->ApplyCommandToIndices(GetUndoMarkCommand(), 
														   &viewIndex, 1);
				m_pane->EndingUpdate(MSG_NotifyNone, viewIndex, 1);
			}
		}
	}

	if (status == eSUCCESS)
		return UndoComplete;
	else
		return UndoFailed;
}

XP_Bool
MarkMessageUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return (m_folder == folder);
}

//////////////////////////////////////////////////////////////////////////////
// UndoMarkChangeListener
//
UndoMarkChangeListener::UndoMarkChangeListener(MSG_Pane *pane,
											   MSG_FolderInfo *folderInfo,
											   MSG_CommandType command)
{
  XP_ASSERT(pane && folderInfo);
  m_pane = pane;
  m_folderInfo = folderInfo;
  m_undoAction = NULL;
  m_command = command;
}

UndoMarkChangeListener::~UndoMarkChangeListener()
{
  // if there is a undoAction add undo manager
  if (m_undoAction)
	m_pane->GetUndoManager()->AddUndoAction(m_undoAction);
}

void
UndoMarkChangeListener::OnKeyChange(MessageKey keyChanged,
									int32 flags,
									ChangeListener* /*instigator*/)
{
  int32 tmpFlags = 0;
  switch (m_command) 
	{
	case MSG_MarkMessagesRead:
	case MSG_ToggleMessageRead:
	  tmpFlags = kIsRead;
	  break;
	case MSG_MarkMessagesUnread:
	  tmpFlags = ~kIsRead;
	  break;
	case MSG_MarkMessages:
	  tmpFlags = kMsgMarked;
	  break;
	case MSG_UnmarkMessages:
	  tmpFlags = ~kMsgMarked;
	  break;
	default:
	  break;
	}
  if (flags & tmpFlags) {
	if (!m_undoAction)
	  m_undoAction = new MarkMessageUndoAction (m_pane, m_command, m_folderInfo);
	XP_ASSERT (m_undoAction);
	m_undoAction->AddKey(keyChanged);
  }
}

/////////////////////////////////////////////////////////////////////////////
// MoveCopyMessagesUndoAction
////////////////////////////////////////////////////////////////////////////
MoveCopyMessagesUndoAction::MoveCopyMessagesUndoAction (
			MSG_FolderInfo *srcFolder,
			MSG_FolderInfo *dstFolder,
			XP_Bool isMove,
			MSG_Pane *pane,
			MessageKey prevKeyToLoad,
			MessageKey nextKeyToLoad)
{
	XP_ASSERT (srcFolder && dstFolder && pane);
	m_srcFolder = srcFolder;
	m_dstFolder = dstFolder;
	m_isMove = isMove;
	m_pane = pane;
	m_prevKeyToLoad = prevKeyToLoad;
	m_nextKeyToLoad = nextKeyToLoad;
	m_mailDB = NULL;
}

XP_Bool
MoveCopyMessagesUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return (m_srcFolder == folder ||
			m_dstFolder == folder);
}

MoveCopyMessagesUndoAction::~MoveCopyMessagesUndoAction()
{
}

UndoStatus
MoveCopyMessagesUndoAction::DoUndo()
{
	MailDB *srcDb = NULL, *dstDb = NULL;
	MsgERR msgErr = MailDB::Open(m_dstFolder->GetMailFolderInfo()->GetPathname(), 
								 FALSE, &dstDb);
	
	if (eSUCCESS == msgErr)
	{
		msgErr = MailDB::Open(m_srcFolder->GetMailFolderInfo()->GetPathname(), 
							  FALSE, &srcDb);
		if (eSUCCESS == msgErr)
		{
			uint i, size;
			MailMessageHdr* msgHdr;
			MessageDBView *view = m_pane->GetMsgView();

			if (m_owner->GetState() == UndoUndoing)
			{
				for (i=0, size = m_dstArray.GetSize(); i < size; i++)
				{
					if (m_isMove)
					{
						msgHdr = dstDb->GetMailHdrForKey(m_dstArray.GetAt(i));
						if (!msgHdr)	// the message must be deleted by someone else
						{
							// XP_ASSERT(msgHdr);
							// *** need to post out some informative message
							continue;
						}
						MailMessageHdr *newHdr = new MailMessageHdr;
						XP_ASSERT(newHdr);
						newHdr->CopyFromMsgHdr (msgHdr, dstDb->GetDB(), srcDb->GetDB());
						newHdr->SetMessageKey(m_srcArray.GetAt(i));
						msgErr = srcDb->UndoDelete(newHdr);
						delete msgHdr;
						delete newHdr;
					}

					dstDb->DeleteMessage(m_dstArray.GetAt(i));
				}
				if (m_isMove)
				{
					m_owner->SetUndoMsgFolder(m_srcFolder);
					m_owner->AddMsgKey(m_prevKeyToLoad);
				}
			}
			else
			{
				for (i=0, size=m_srcArray.GetSize(); i < size; i++)
				{
					msgHdr = srcDb->GetMailHdrForKey(m_srcArray.GetAt(i));
					if (!msgHdr)	// the message must be deleted by someone else
					{
						// XP_ASSERT(msgHdr);
						// *** need to post out some informative message
						continue;
					}
					MailMessageHdr *newHdr = new MailMessageHdr;
					XP_ASSERT(newHdr);
					newHdr->CopyFromMsgHdr (msgHdr, srcDb->GetDB(), dstDb->GetDB());
					newHdr->SetMessageKey(m_dstArray.GetAt(i));
					msgErr = dstDb->UndoDelete(newHdr);
					delete msgHdr;
					if (m_isMove)
					{
						srcDb->DeleteMessage(m_srcArray.GetAt(i));

					}
				}

				if (m_isMove)
				{
					m_owner->SetUndoMsgFolder(m_srcFolder);
					m_owner->AddMsgKey(m_nextKeyToLoad);
				}
			}
			srcDb->Close();
			dstDb->Close();

			return UndoComplete;
		}
		else
		{
			dstDb->Close();
			if (srcDb)
				srcDb->Close();
			return ReCreateMailDB(m_srcFolder);
		}
	}
	else
	{
		if (dstDb)
			dstDb->Close();
		return ReCreateMailDB(m_dstFolder);
	}
}

void
MoveCopyMessagesUndoAction::AddDstKey(MessageKey key)
{
	m_dstArray.Add(key);
}

void
MoveCopyMessagesUndoAction::AddSrcKey(MessageKey key)
{
	m_srcArray.Add(key);
}

UndoStatus
MoveCopyMessagesUndoAction::UndoPreExit()
{
	if (m_mailDB)
	{
		m_mailDB->Close();
		m_mailDB = NULL;
	}
	return DoUndo();
}

UndoStatus
MoveCopyMessagesUndoAction::ReCreateMailDB(MSG_FolderInfo *folder)
{
	const char *pathname = folder->GetMailFolderInfo()->GetPathname();
	char * url = PR_smprintf ("mailbox:%s", pathname);
	URL_Struct *url_struct = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	MailDB	*mailDB;
	MsgERR status = eUNKNOWN;

	XP_FileRemove(pathname, xpMailFolderSummary);

	status = MailDB::Open(pathname, TRUE, &mailDB);
	if (mailDB != NULL && (status == eSUCCESS || status == eNoSummaryFile))
	{
		// mailDB->Close(); // decrement ref count.
		m_mailDB = mailDB;

		ParseMailboxState *parseMailboxState = new ParseMailboxState(pathname);
		// parseMailboxState->SetView(m_view);  
		parseMailboxState->SetIncrementalUpdate(FALSE);
		parseMailboxState->SetMaster(m_pane->GetMaster());
		parseMailboxState->SetDB(mailDB);
		parseMailboxState->SetContext(m_pane->GetContext());
		parseMailboxState->SetFolder(folder);

		folder->GetMailFolderInfo()->SetParseMailboxState(parseMailboxState);
		url_struct->msg_pane = m_pane;
		UndoURLHook(url_struct);
		m_pane->GetURL(url_struct, FALSE);
		return UndoInProgress;
	}
	else
		return UndoFailed;
}
///////////////////////////////////////////////////////////////////////////
// MoveFolderUndoAction
///////////////////////////////////////////////////////////////////////////

MoveFolderUndoAction::MoveFolderUndoAction (MSG_Pane *pane,
											MSG_FolderInfo *srcParent,
											MSG_FolderInfo *srcFolder,
											MSG_FolderInfo *dstFolder)
{
	XP_ASSERT (pane && srcFolder && dstFolder);
	m_pane = pane;
	m_srcParent = srcParent;
	m_srcFolder = srcFolder;
	m_dstFolder = dstFolder;
}

MoveFolderUndoAction::~MoveFolderUndoAction ()
{
}

XP_Bool
MoveFolderUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return (m_srcParent == folder ||
			m_srcFolder == folder ||
			m_dstFolder == folder);
}

UndoStatus
MoveFolderUndoAction::DoUndo()
{
	MSG_FolderPane *folderPane = (MSG_FolderPane*) m_pane;
	MsgERR err = eUNKNOWN;
	MSG_FolderInfo *destFolder, *srcFolder, *srcParent;

	srcFolder = m_srcFolder;
	if (m_owner->GetState() == UndoUndoing) {
		srcParent = m_dstFolder;
		destFolder = m_srcParent;
	}
	else {
		srcParent = m_srcParent;
		destFolder = m_dstFolder;
	}

	// If we're moving into an IMAP folder, start the URL here
	if ( ((destFolder->GetType() == FOLDER_IMAPMAIL) || 
		  (destFolder == m_pane->GetMaster()->GetImapMailFolderTree())) &&
		 (srcFolder->GetType() == FOLDER_IMAPMAIL)  )
	{
		const char *destinationName = "";	// covers promote to root
		char destinationHierarchySeparator = 0;	// covers promote to root
		if (destFolder->GetType() == FOLDER_IMAPMAIL)
		{
			destinationName = ((MSG_IMAPFolderInfoMail *)destFolder)->GetOnlineName();
			destinationHierarchySeparator = ((MSG_IMAPFolderInfoMail *)destFolder)->GetOnlineHierarchySeparator();
		}
		
		// the rename on the server has to happen first imap.h
		char *renameMailboxURL = CreateImapMailboxMoveFolderHierarchyUrl
												(m_pane->GetPrefs()->GetPopHost(),
												((MSG_IMAPFolderInfoMail *) srcFolder)->GetOnlineName(),
												((MSG_IMAPFolderInfoMail *) srcFolder)->GetOnlineHierarchySeparator(),
												destinationName,destinationHierarchySeparator);
		if (renameMailboxURL)
		{
			URL_Struct *url_struct = NET_CreateURLStruct(renameMailboxURL, NET_SUPER_RELOAD);
			if (url_struct) {
				url_struct->msg_pane = m_pane;
				UndoURLHook(url_struct);             
				m_pane->GetURL(url_struct, FALSE);
			}
			XP_FREE(renameMailboxURL);
		}
		return UndoInProgress; 
	}
	else 
	{
		err = folderPane->UndoMoveFolder(srcParent, srcFolder, destFolder);
	}


	if (eSUCCESS == err)
		return UndoComplete;

	return UndoFailed;
}

//////////////////////////////////////////////////////////////////////////////
// RenameFolderUndoAction
//////////////////////////////////////////////////////////////////////////////
RenameFolderUndoAction::RenameFolderUndoAction(MSG_Pane *pane,
											   MSG_FolderInfo *folder,
											   const char *oldName,
											   const char *newName)
{
	XP_ASSERT (pane && folder && oldName && newName);
	m_pane = pane;
	m_folder = folder;
	m_oldName = XP_STRDUP(oldName);
	m_newName = XP_STRDUP(newName);
}

XP_Bool
RenameFolderUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return (m_folder == folder);
}

RenameFolderUndoAction::~RenameFolderUndoAction()
{
	XP_FREE (m_oldName);
	XP_FREE (m_newName);
}

UndoStatus
RenameFolderUndoAction::DoUndo()
{
	MsgERR err = eUNKNOWN;

	if (m_owner->GetState() == UndoUndoing)
		err = ((MSG_FolderPane*)m_pane)->RenameFolder(m_folder, m_oldName);
	else
		err = ((MSG_FolderPane*)m_pane)->RenameFolder(m_folder, m_newName);

	if (err == eSUCCESS)
		return UndoComplete;
	
	return UndoFailed;
}

///////////////////////////////////////////////////////////////////////////
// IMAPMoveMessagesUndoAction
///////////////////////////////////////////////////////////////////////////
IMAPMoveCopyMessagesUndoAction::IMAPMoveCopyMessagesUndoAction(
		MSG_Pane *pane,
		MSG_FolderInfo *srcFolder,
		MSG_FolderInfo *dstFolder,
		XP_Bool isMove,
		MessageKey prevKeyToLoad,
		MessageKey nextKeyToLoad)
{
  XP_ASSERT(pane && srcFolder && dstFolder);
  m_pane = pane;
  m_srcFolder = srcFolder;
  m_dstFolder = dstFolder;  /* just for the record; cannot do anything */
  m_prevKeyToLoad = prevKeyToLoad;
  m_nextKeyToLoad = nextKeyToLoad;
  m_isMove = isMove;
}

IMAPMoveCopyMessagesUndoAction::~IMAPMoveCopyMessagesUndoAction()
{
}

XP_Bool
IMAPMoveCopyMessagesUndoAction::HasFolder(MSG_FolderInfo *folder)
{
  return (m_srcFolder == folder);
}

UndoStatus
IMAPMoveCopyMessagesUndoAction::DoUndo()
{
	if (! m_keyArray.GetSize()) return UndoComplete;

	if (((MSG_IMAPFolderInfoMail *)m_srcFolder)->UndoMoveCopyMessagesHelper(
		m_pane, m_keyArray, this))
		return UndoInProgress;
	else
		return UndoFailed;
}

UndoStatus
IMAPMoveCopyMessagesUndoAction::UndoPreExit()
{
  if (m_owner->GetState() == UndoUndoing) {
	if (m_isMove)
	  m_owner->AddMsgKey(m_prevKeyToLoad);
  }
  else {
	if (m_isMove)
	  m_owner->AddMsgKey(m_nextKeyToLoad);
  }
 return UndoComplete; 
}
	
void
IMAPMoveCopyMessagesUndoAction::AddKey(MessageKey key)
{
  m_keyArray.Add(key);
}


////////////////////////////////////////////////////////////////////
// IMAPRenameFolderUndoAction
////////////////////////////////////////////////////////////////////

IMAPRenameFolderUndoAction::IMAPRenameFolderUndoAction(
		  MSG_Pane *pane,
		  MSG_FolderInfo *folder,
		  const char *oldName,
		  const char *newName)
{
  XP_ASSERT (pane && folder && oldName && newName);
	m_pane = pane;
	m_folder = folder;
	m_oldName = XP_STRDUP(oldName);
	m_newName = XP_STRDUP(newName);
}

XP_Bool
IMAPRenameFolderUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return (m_folder == folder);
}

IMAPRenameFolderUndoAction::~IMAPRenameFolderUndoAction()
{
	XP_FREE (m_oldName);
	XP_FREE (m_newName);
}

UndoStatus
IMAPRenameFolderUndoAction::DoUndo()
{
	char *renameMailboxURL = NULL;

	if (m_owner->GetState() == UndoUndoing) {
	  renameMailboxURL = CreateImapMailboxRenameLeafUrl(
		  m_pane->GetPrefs()->GetPopHost(),
		  ((MSG_IMAPFolderInfoMail *)m_folder)->GetOnlineName(),
		  ((MSG_IMAPFolderInfoMail *)m_folder)->GetOnlineHierarchySeparator(),
		  m_oldName);
	}
	else {
	  renameMailboxURL = CreateImapMailboxRenameLeafUrl(
		  m_pane->GetPrefs()->GetPopHost(),
		  ((MSG_IMAPFolderInfoMail *)m_folder)->GetOnlineName(),
		  ((MSG_IMAPFolderInfoMail *)m_folder)->GetOnlineHierarchySeparator(),
		  m_newName);
	}

	if (renameMailboxURL) {
		URL_Struct *url_struct = 
			NET_CreateURLStruct(renameMailboxURL, NET_SUPER_RELOAD);

		XP_FREEIF(renameMailboxURL);

		if (url_struct) {
		  url_struct->msg_pane = m_pane;
		  UndoURLHook(url_struct);
		  m_pane->GetURL(url_struct, FALSE);
		  return UndoInProgress;
		}
	}
	return UndoFailed;
}

UndoStatus
IMAPRenameFolderUndoAction::UndoPreExit()
{
  // this may work
  return UndoComplete;
}

////////////////////////////////////////////////////////////
// OfflineIMAPUndoAction
////////////////////////////////////////////////////////////

UndoIMAPChangeListener::UndoIMAPChangeListener(OfflineIMAPUndoAction *action)
{
	m_undoAction = action;
}

UndoIMAPChangeListener::~UndoIMAPChangeListener()
{
}

void UndoIMAPChangeListener::OnAnnouncerGoingAway(ChangeAnnouncer * /*instigator*/) 
{
	if (m_undoAction->m_db)
	{
		m_undoAction->m_db->RemoveListener(this);
		m_undoAction->m_db = NULL;	
	}
}

OfflineIMAPUndoAction::OfflineIMAPUndoAction (MSG_Pane *pane, MSG_FolderInfo* folder,
											  MessageKey dbKey, int32 opType,
											  MSG_FolderInfo *srcFolder, MSG_FolderInfo *dstFolder,
											  imapMessageFlagsType flags, MailMessageHdr *hdr, XP_Bool addFlag)
{
	XP_Bool dbWasCreated=FALSE;
	
	m_pane = pane;
	m_dbKey = dbKey;
	m_folder = (MSG_IMAPFolderInfoMail*) folder;
	m_srcFolder = (MSG_IMAPFolderInfoMail*) srcFolder;
	m_dstFolder = (MSG_IMAPFolderInfoMail*) dstFolder;
	m_opType = opType;
	m_flags = flags;
	m_header = NULL;
	m_db = NULL;
	m_addFlags = addFlag;
	m_changeListener = NULL;
	if (!m_srcFolder)
		m_srcFolder = m_folder;
	if (hdr)
	{
		MsgERR dbStatus = ImapMailDB::Open(m_srcFolder->GetMailFolderInfo()->GetPathname(), TRUE, // create if necessary
						(MailDB**) &m_db, m_pane->GetMaster(), &dbWasCreated);
		if (!m_db)
			return;
		m_header = new MailMessageHdr;
		m_header->CopyFromMsgHdr (hdr, m_db->GetDB(), m_db->GetDB());
		m_changeListener = new UndoIMAPChangeListener(this);
		m_db->AddListener(m_changeListener);
	}
}


XP_Bool OfflineIMAPUndoAction::HasFolder(MSG_FolderInfo *folder)
{
	return ((MSG_FolderInfo*) m_folder == folder);
}

OfflineIMAPUndoAction::~OfflineIMAPUndoAction()
{
	if (m_db)
	{
		delete m_header;

		if (m_changeListener)
			m_db->RemoveListener(m_changeListener);
		m_db->Close();
		m_db = NULL;
	}
	else
		XP_ASSERT(!m_header);
}


// Open the database and find the key for the offline operation that we want to
// undo, then remove it from the database, we also hold on to this
// data for a redo operation.
// Lesson I learned the hard way, you do not RemoveReference after an Add or Delete operation on
// a header or op item.

UndoStatus OfflineIMAPUndoAction::DoUndo()
{
	IDArray keys;
	ImapMailDB *destdb = NULL;
	DBOfflineImapOperation	*op = NULL;
	XP_Bool dbWasCreated=FALSE;
	MailMessageHdr *restoreHdr = NULL;
	MsgERR dbStatus = 0;
	
	if (!m_folder || !m_pane)
		return UndoFailed;
	dbStatus = ImapMailDB::Open(m_srcFolder->GetMailFolderInfo()->GetPathname(), TRUE, // create if necessary
						(MailDB**) &m_db, m_pane->GetMaster(), &dbWasCreated);
	if (!m_db)
		return UndoFailed;
	if (m_owner->GetState() == UndoRedoing)	// REDO
	{
		switch (m_opType)
		{
			case kMsgMoved:
			case kMsgCopy:
				op = m_db->GetOfflineOpForKey(m_dbKey, TRUE);
				if (op)
				{
					if (m_opType == kMsgMoved)
						op->SetMessageMoveOperation(m_dstFolder->GetOnlineName()); // offline move
					if (m_opType == kMsgCopy)
						op->AddMessageCopyOperation(m_dstFolder->GetOnlineName()); // offline copy
					delete op;
					op = NULL;
				}
				m_dstFolder->SummaryChanged();
				break;
			case kAddedHeader:
				dbStatus = ImapMailDB::Open(m_dstFolder->GetMailFolderInfo()->GetPathname(), TRUE, // create if necessary
						(MailDB**) &destdb, m_pane->GetMaster(), &dbWasCreated);
				if (!destdb)
					return UndoFailed;
				restoreHdr = new MailMessageHdr;
				if (restoreHdr)
				{
					if (m_header)
					{
						restoreHdr->CopyFromMsgHdr (m_header, m_db->GetDB(), destdb->GetDB());
						int err = destdb->AddHdrToDB(restoreHdr, NULL, FALSE);
					}
				}
				op = destdb->GetOfflineOpForKey(m_dbKey, TRUE);
				if (op)
				{
					op->SetSourceMailbox(m_srcFolder->GetOnlineName(), m_dbKey);
					delete op;
				}
				m_dstFolder->SummaryChanged();
				destdb->Close();
				break;
			case kDeletedMsg:
				m_db->DeleteMessage(m_dbKey);
				break;
			case kMsgMarkedDeleted:
				m_db->MarkImapDeleted(m_dbKey, TRUE, NULL);
				break;
			case kFlagsChanged:
				op = m_db->GetOfflineOpForKey(m_dbKey, TRUE);
				if (op)
				{
					if (m_addFlags)
						op->SetImapFlagOperation(op->GetNewMessageFlags() | m_flags);
					else
						op->SetImapFlagOperation(op->GetNewMessageFlags() & ~m_flags);
					delete op;
				}
				break;
			default:
				break;
		}
		m_db->Close();
		m_db = NULL;
		m_srcFolder->SummaryChanged();
		return UndoComplete;
	}
	switch (m_opType)		// UNDO
	{
		case kMsgMoved:
		case kMsgCopy:
		case kAddedHeader:
		case kFlagsChanged:
			op = m_db->GetOfflineOpForKey(m_dbKey, FALSE);
			if (op)
			{
				m_db->DeleteOfflineOp(m_dbKey);
				delete op;
				op = NULL;
			}
			if (m_header && (m_opType == kAddedHeader))
			{
				MailMessageHdr *mailHdr = m_db->GetMailHdrForKey(m_header->GetMessageKey());
				if (mailHdr)
					m_db->DeleteHeader(mailHdr, NULL, FALSE);
				delete mailHdr;
			}
			break;
		case kDeletedMsg:
			restoreHdr = new MailMessageHdr;
			if (restoreHdr)
			{
				dbStatus = ImapMailDB::Open(m_dstFolder->GetMailFolderInfo()->GetPathname(), TRUE, // create if necessary
						(MailDB**) &destdb, m_pane->GetMaster(), &dbWasCreated);
				if (!destdb)
					return UndoFailed;
				if (m_header)
				{
					restoreHdr->CopyFromMsgHdr (m_header, destdb->GetDB(), m_db->GetDB());
					m_db->AddHdrToDB(restoreHdr, NULL, TRUE);
				}
				destdb->Close();
				m_dstFolder->SummaryChanged();
			}
			break;
		case kMsgMarkedDeleted:
			m_db->MarkImapDeleted(m_dbKey, FALSE, NULL);
			break;
		default:
			break;
	}
	m_db->Close();
	m_db = NULL;
	m_srcFolder->SummaryChanged();
	return UndoComplete;
}




