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
/* msgundmg.cpp --- Undo Manager implementation
 */

#include "msg.h"
#include "msgundmg.h"
#include "maildb.h"
#include "msgdbvw.h"
#include "msgpane.h"
#include "msgmpane.h"
#include "msgtpane.h"
#include "mailhdr.h"
#include "msgfinfo.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
}

///////////////////////////////////////////////////////////////////////////
// Implementation of UndoObject
///////////////////////////////////////////////////////////////////////////

UndoObject::UndoObject()
{
	m_owner = NULL;
	m_preExitFunc_hook = NULL;
	m_refCount = 1;
}

UndoObject::~UndoObject()
{
}

int
UndoObject::Release()
{
	--m_refCount;
	if (!m_refCount)
	{
		delete this;
		return 0;
	}
	return m_refCount;
}

UndoStatus
UndoObject::DoUndo()
{
	return UndoComplete;
}

UndoStatus
UndoObject::StopUndo( int /*status*/ )
{
	return UndoFailed;
}

// This is a default implementation. You should overwrite this
// method.
void
UndoObject::FinalizeUndo()
{
	// This is a default implementation. Client should
	// overwrite.
	XP_ASSERT(m_owner);

	AddRefCount();
	
	m_owner->AddUndoAction (this);

	// Inform UndoManager that we have done with
	// current undo object. 
	m_owner->DoUndoComplete(this);

}

void
UndoObject::UndoURLHook(URL_Struct *url)
{
	XP_ASSERT (url);
	if ( !url ) return;
	MSG_Pane *pane = url->msg_pane; 

	XP_ASSERT (pane);
	if (!pane) return;

	pane->GetUndoManager()->AddInProgress(this);

	if (url->pre_exit_fn)
		m_preExitFunc_hook = url->pre_exit_fn;
	
	url->pre_exit_fn = UndoObject::UndoURLPreExitFunc;
}

void
UndoObject::UndoURLPreExitFunc (URL_Struct *url,
								int status,
								MWContext *context)
{
	XP_ASSERT (url);
	if ( !url ) return;

	if (url->msg_pane)
	{
		UndoManager *undoManager = url->msg_pane->GetUndoManager();

		XP_ASSERT (undoManager);
		if (!undoManager) return;

		UndoObject *undoObject = undoManager->RemoveInProgress();

		XP_ASSERT (undoObject);
		if (!undoObject) return;

		if (undoObject->m_preExitFunc_hook)
			undoObject->m_preExitFunc_hook (url, status, context);

		if (status < 0)
		{
			undoObject->StopUndo (status);
			undoManager->Reset();
			return;
		}

		UndoStatus undoStatus;

		undoStatus = undoObject->UndoPreExit();

		if (undoStatus == UndoComplete)
			undoObject->FinalizeUndo();
		else
		{
			undoObject->StopUndo (status);
			undoManager->Reset();
		}
	}
}

/////////////////////////////////////////////////////////////////////
// Implementation of UndoMarker
/////////////////////////////////////////////////////////////////////
void
UndoMarker::FinalizeUndo()
{
	XP_ASSERT (IsMarker());

	if (IsStart())
		m_owner->StartBatch();
	else
		m_owner->EndBatch();

	m_owner->DoUndoComplete(this);
}


/////////////////////////////////////////////////////////////////
// Implementation of UndoManager
/////////////////////////////////////////////////////////////////

UndoManager::UndoManager (MSG_Pane *pane, int maxActions)
{
	m_maxUndoActions = maxActions;
	m_depth = 0;
	m_state = UndoIdle;
	m_count = 0;
	m_status = UndoComplete;
	m_pane = pane;
	m_refCount = 1;
}

UndoManager::~UndoManager ()
{
	FlushActions();
}

int
UndoManager::Init()
{
	return 0;
}

int
UndoManager::Release()
{
	--m_refCount;
	if (!m_refCount)
	{
		delete this;
		return 0;
	}
	return m_refCount;
}


int
UndoManager::StartBatch()
{
	int status = 0;
	UndoMarker *marker = new UndoMarker (FALSE);

	if ( !marker ) return MK_OUT_OF_MEMORY;

	++m_depth;

	if (m_state == UndoIdle || m_state == UndoRedoing)
		AddAtomicUndo(marker);
	else
		AddAtomicRedo(marker);

	return status;
}

int
UndoManager::EndBatch()
{
	int status = 0;
	UndoMarker *marker = new UndoMarker (TRUE);

	if (!marker) 
	{
		Reset();
		return MK_OUT_OF_MEMORY;
	}

	--m_depth;

	if (m_state == UndoIdle || m_state == UndoRedoing)
		AddAtomicUndo(marker);
	else
		AddAtomicRedo(marker);

	if (m_state == UndoIdle)
		FinalizeAtomicActions();

	return status;
}

int
UndoManager::AbortBatch()
{
	XP_ASSERT (m_state == UndoIdle);

	int size;
	int status = 0;

	// **** There definitely much more we can do. For now
	// let's just free some memory resource. Revisit later!!!!!!
    while (1)
	{
		size = m_atomicUndoList.GetSize();
		if (size == 0)
			break;
		((UndoObject *) m_atomicUndoList.GetAt(--size))->Release();
		m_atomicUndoList.RemoveAt(size);
	}
	return status;
}

int
UndoManager::AddUndoAction( UndoObject *undoObject )
{
	int status = 0;

	XP_ASSERT (undoObject);
	if (!undoObject) return -1;

	undoObject->SetOwner(this);

	if (m_state == UndoIdle || m_state == UndoRedoing)
	{
		if (m_depth) 
		{
			AddAtomicUndo(undoObject);
		}
		else
		{
			if (m_undoList.GetSize() >= m_maxUndoActions)
				FlushAtomic (&m_undoList);
			m_undoList.Add(undoObject);
		}
	}
	else
	{
		if (m_depth)
		{
			AddAtomicRedo (undoObject);
		}
		else
		{
			if (m_redoList.GetSize() >= m_maxUndoActions)
				FlushAtomic (&m_redoList);
			m_redoList.Add(undoObject);
		}
	}
	return status;
}

XP_Bool
UndoManager::CanUndo()
{
	return (m_undoList.GetSize() > 0);
}

XP_Bool
UndoManager::CanRedo()
{
	return (m_redoList.GetSize() > 0);
}

UndoStatus
UndoManager::Undo()
{
	UndoStatus status = UndoError;
	if (CanUndo())
	{
		m_status = UndoComplete;
		m_state = UndoUndoing;
		status = FireAtomic( &m_undoList );
	}
	if (status != UndoInProgress)
		m_state = UndoIdle;
	return status;
}

UndoStatus
UndoManager::Redo()
{
	UndoStatus status = UndoError;
	if (CanRedo())
	{
		m_status = UndoComplete;
		m_state = UndoRedoing;
		status = FireAtomic( &m_redoList );
	}
	if (status != UndoInProgress)
		m_state = UndoIdle;
	return status;
}

void
UndoManager::FlushActions()
{
	int size = 0;
	while (1)
	{
		size = m_undoList.GetSize();
		if (size == 0)
			break;
		((UndoObject*)m_undoList.GetAt(--size))->Release();
		m_undoList.RemoveAt(size);
	}
	while (1)
	{
		size = m_redoList.GetSize();
		if (size == 0)
			break;
		((UndoObject*)m_redoList.GetAt(--size))->Release();
		m_redoList.RemoveAt(size);
	}

	m_inProgressList.RemoveAll();
	m_atomicUndoList.RemoveAll();
	m_atomicRedoList.RemoveAll();

	m_depth = 0;
	m_count = 0;
	m_state = UndoIdle;
	m_status = UndoComplete;
}

void
UndoManager::RemoveActions(MSG_FolderInfo *folder)
{
	if (!folder) return;

	int size;
	
	size = m_undoList.GetSize();
	while (size)
	{
		if (((UndoObject*)m_undoList.GetAt(--size))->HasFolder(folder))
			m_undoList.RemoveAt(size);
	}

	size = m_redoList.GetSize();
	while(size)
	{
		if (((UndoObject*)m_undoList.GetAt(--size))->HasFolder(folder))
			m_redoList.RemoveAt(size);
	}
}

void
UndoManager::Reset()
{
	int size = 0;

	if (m_state == UndoUndoing)
	{
		while (1)
		{
			size = m_atomicRedoList.GetSize();
			if (size == 0)
				break;
			((UndoObject*)m_atomicRedoList.GetAt(--size))->Release();
			m_atomicRedoList.RemoveAt(size);
		}
	}
	else if (m_state == UndoRedoing)
	{
		while (1)
		{
			size = m_atomicUndoList.GetSize();
			if (size == 0)
				break;
			((UndoObject*)m_atomicUndoList.GetAt(--size))->Release();
			m_atomicUndoList.RemoveAt(size);
		}
	}
	m_state = UndoIdle;
	m_depth = 0;
	m_count = 0;
	m_status = UndoComplete;
}

int
UndoManager::AddInProgress (UndoObject *undoObject)
{
	XP_ASSERT (undoObject);
	if (!undoObject) return -1;

	m_inProgressList.Add((void *)undoObject);
	return 0;
}

UndoObject*
UndoManager::GetInProgress()
{
	int size = m_inProgressList.GetSize();
	XP_ASSERT(size > 0);
	if (size <= 0) return NULL;

	UndoObject *undoObject = (UndoObject *) m_inProgressList.GetAt(size-1);
	return undoObject;
}

UndoObject*
UndoManager::RemoveInProgress()
{
	UndoObject *undoObject = GetInProgress();
	m_inProgressList.RemoveAt(m_inProgressList.GetSize()-1);

	return (undoObject);
}

void
UndoManager::FlushAtomic (XPPtrArray *list)
{
	int size = list->GetSize();
	int count = 0;

	--size; // real index;

	UndoObject *undoObject = (UndoObject*) list->GetAt(size);
	XP_ASSERT (undoObject);

	if (!undoObject) return;

	if (m_depth)
	{
		// we are flush while we are adding a batch operation
		int depth = m_depth;
		while (depth && size >= 0)
		{
			if (undoObject->IsMarker() && undoObject->IsEnd())
				depth--;
			undoObject = (UndoObject*) list->GetAt(--size);
		}
	}

	if (undoObject->IsMarker())
	{
		int depth = 0;

		XP_ASSERT(undoObject->IsStart());
		while (undoObject && size >= 0)
		{
			if (undoObject->IsStart())
				depth++;
			else if (undoObject->IsEnd())
				depth--;
			undoObject->Release();
			++count;
			list->RemoveAt(size--);
			if (!depth) return;
			undoObject = (UndoObject*) list->GetAt(size);
		}
	}
	else
	{
		++count;
		undoObject->Release();
		list->RemoveAt(size);
	}
}


// This is an extremely overloaded routine. I am not sure what else
// I can do. Sigh.....
UndoStatus
UndoManager::FireAtomic (XPPtrArray *list)
{
	if (m_status!= UndoComplete) return m_status;

	int size = list->GetSize();

	if (!size) return UndoComplete;

	UndoObject *undoObject;

	if (!m_depth)
		m_count = 0;

	undoObject = (UndoObject*) list->GetAt(size-m_count-1);

	XP_ASSERT(undoObject);
	if (!undoObject) return UndoError;

	if (undoObject->IsMarker() || m_depth)
	{
		while (undoObject)
		{
			m_status = undoObject->DoUndo();
			if (m_status == UndoInProgress)
			{
				return m_status;
			}
			else if (m_status == UndoComplete)
			{
				// What happen if something went wrong in the middle of an
				// atomic operation
				undoObject->FinalizeUndo();
			}
			else
			{
				// Don't know what to do yet
				XP_ASSERT(0);
			}
			if (!m_depth) 
			{
				// We are done with this batch. Reset undo state
				// to undo idle.
				m_state = UndoIdle;
				return m_status;
			}
			else if (m_status != UndoComplete)
			{
				return m_status;
			}
			undoObject = (UndoObject*)list->GetAt(size-m_count-1);
		}
	}
	else
	{
		m_status = undoObject->DoUndo();
		if ( m_status == UndoComplete )
		{
			undoObject->FinalizeUndo();
			// FinalizeUndo() calls DoUndoComplete() if this is a single
			// atomic undo operation m_state is reset to UndoIdle in
			// DoUndoComplete().
			return UndoComplete;
		}
		else if (m_status == UndoInProgress)
		{
			return UndoInProgress;
		}
		else
		{
			// don't know what to do yet
			return m_status;
		}
	}
	return m_status;
}

void
UndoManager::DoUndoComplete(UndoObject* undoObject)
{
	m_status = UndoComplete;

	++m_count;
	if (m_depth) {
		if (m_state == UndoUndoing)
			FireAtomic(&m_undoList);
		else if (m_state == UndoRedoing)
			FireAtomic(&m_redoList);
		else
			XP_ASSERT(0);
		// **** Post Error message if error occured
	}
	else
	{
		if (undoObject->IsMarker() &&
			undoObject->IsEnd())
		{
			FinalizeAtomicActions();
		}
		else
		{
			if (m_state == UndoUndoing)
				m_undoList.Remove((void *) undoObject);
			else if (m_state == UndoRedoing)
				m_redoList.Remove((void *) undoObject);
			undoObject->Release();
		}
		m_state = UndoIdle;
	}
}


void
UndoManager::AddAtomicUndo(UndoObject *undoObject)
{
	m_atomicUndoList.Add(undoObject);
}

void
UndoManager::AddAtomicRedo(UndoObject *undoObject)
{
	m_atomicRedoList.Add(undoObject);
}

void
UndoManager::FinalizeAtomicActions()
{
	XPPtrArray *list = (m_state == UndoUndoing ?
		&m_atomicRedoList :
		&m_atomicUndoList );

	if (!m_depth)
	{
		UndoObject *undoObject;
		int size = list->GetSize();
		int count = m_count;

		for (int i=0; i<size; i++)
		{
			undoObject = (UndoObject*) list->GetAt(i);
			AddUndoAction(undoObject);
		}
		list->RemoveAll();

		if (m_state == UndoIdle)
			return;

		list = (m_state == UndoUndoing ?
			&m_undoList : &m_redoList );

		while (count)
		{
			undoObject = (UndoObject *) list->GetAt(list->GetSize()-1);
			undoObject->Release();
			list->RemoveAt(list->GetSize()-1);
			count--;
		}
	}
	
}

int 
UndoManager::AddMsgKey(MessageKey key)
{
	int status = 0;

	m_msgKeyArray.Add(key);
	return status;
}

MessageKey
UndoManager::GetAndRemoveMsgKey()
{
	if (!m_msgKeyArray.GetSize())
		return MSG_MESSAGEKEYNONE;

	MessageKey key = m_msgKeyArray.GetAt(0);
	m_msgKeyArray.RemoveAt(0);

	return key;
}


void UndoManager::SetUndoMsgFolder( MSG_FolderInfo *folder )
{
	m_undoMsgFolder = folder;
}

MSG_FolderInfo * UndoManager::GetUndoMsgFolder()
{
	return m_undoMsgFolder;
}

//////////////////////////////////////////////////////////////////////////
// BacktrackManager
//////////////////////////////////////////////////////////////////////////

BacktrackManager::BacktrackManager(MSG_Pane *pane)
{
	m_pane = pane;
	m_cursor = -1;
	m_state = MSG_BacktrackIdle;
	m_curKey = MSG_MESSAGEKEYNONE;
	m_curFolder = NULL;
}

BacktrackManager::~BacktrackManager()
{
	Reset();
}

int
BacktrackManager::AddEntry(MSG_FolderInfo *folder, MessageKey key)
{
	int status = 0;
	BacktrackEntry *entry = NULL;

	if (m_state != MSG_BacktrackIdle)
	{
		return status;
	}

	if (m_cursor >= 0)
	{		
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(m_cursor);
		if (entry->folder == folder && entry->key == key)
			goto done;
	}
	if (m_cursor-1 >= 0)
	{
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(m_cursor-1);
		if (entry->folder == folder && entry->key == key)
			goto done;
	}
	if ((m_cursor+1) < m_backtrackArray.GetSize())
	{
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(m_cursor+1);
		if (entry->folder == folder && entry->key == key)
			goto done;
	}

	entry = new BacktrackEntry;
	if (entry)
	{
		m_cursor++;
		entry->folder = folder;
		entry->key = key;
		m_backtrackArray.InsertAt(m_cursor, (void*)entry, 1);
		goto done;
	}
	else
		return MK_OUT_OF_MEMORY;
done:
	if (entry)
		SetCurMessageKey (entry->folder, entry->key);
	return status;
}

XP_Bool
BacktrackManager::CanGoBack()
{
	if (m_cursor <=0) return FALSE;
	BacktrackEntry *entry = NULL;

	for (int i=m_cursor-1; i >= 0; i--)
	{
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(i);
		XP_ASSERT (entry->key != MSG_MESSAGEKEYNONE);
		if (entry->key != MSG_MESSAGEKEYNONE) 
			return TRUE;
	}
	return FALSE;
}

XP_Bool
BacktrackManager::CanGoForward()
{
	int size = m_backtrackArray.GetSize();
	BacktrackEntry *entry = NULL;

	if (m_cursor >= size-1)
		return FALSE;
	
	for (int i=m_cursor+1; i < size; i++)
	{
		entry = (BacktrackEntry*) m_backtrackArray.GetAt(i);
		XP_ASSERT (entry->key != MSG_MESSAGEKEYNONE);
		if (entry->key != MSG_MESSAGEKEYNONE)
			return TRUE;
	}
	return FALSE;
}

MessageKey
BacktrackManager::GoBack(MSG_FolderInfo **pFolderInfo)
{
	XP_ASSERT (m_cursor > 0);
	int i;
	DBMessageHdr *msgHdr = NULL;
	BacktrackEntry *entry = NULL;
	MailDB *mailDb;

	m_state = MSG_BacktrackBackward;

	for (i=m_cursor-1; i >= 0; i--)
	{
		entry = (BacktrackEntry*) m_backtrackArray.GetAt(i);
		if (entry->folder != m_curFolder)
		{
			// *** if the folder is a newsgroup folder we done.
			if (entry->folder->IsNews()) break;

			// if this is a mail folder does this message get deleted?
			MsgERR err = eUNKNOWN;
			mailDb= NULL;
			err = MailDB::Open(entry->folder->GetMailFolderInfo()->GetPathname(), 
							   FALSE, &mailDb);
			if (err == eSUCCESS)
			{
				msgHdr = mailDb->GetDBHdrForKey(entry->key);
				if (msgHdr)
				{
					mailDb->Close();
					break;
				}
			}
			if (mailDb) mailDb->Close();
		}
		else if (entry->key != m_curKey)
		{
			msgHdr = m_pane->GetMsgView()->GetDB()->GetDBHdrForKey(entry->key);
			if (msgHdr) break;
		}
	}

	delete msgHdr;

	if (entry)
	{
		if (pFolderInfo && m_curFolder != entry->folder)
			*pFolderInfo = entry->folder;
		m_cursor = i;
		SetCurMessageKey(entry->folder, entry->key);
		return entry->key;
	}
	return MSG_MESSAGEKEYNONE;
}


MessageKey
BacktrackManager::GoForward(MSG_FolderInfo **pFolderInfo)
{
	int size = m_backtrackArray.GetSize();

	XP_ASSERT (m_cursor < size -1);
	int i;
	DBMessageHdr *msgHdr = NULL;
	BacktrackEntry *entry = NULL;
	MailDB *mailDb;

	m_state = MSG_BacktrackForward;

	for (i=m_cursor+1; i < size; i++)
	{
		entry = (BacktrackEntry*) m_backtrackArray.GetAt(i);
		if (entry->folder != m_curFolder)
		{
			// *** if the folder is a newsgroup folder we done.
			if (entry->folder->IsNews()) break;

			// if this is a mail folder does this message get deleted?
			MsgERR err = eUNKNOWN;
			mailDb= NULL;
			err = MailDB::Open(entry->folder->GetMailFolderInfo()->GetPathname(), 
							   FALSE, &mailDb);
			if (err == eSUCCESS)
			{
				msgHdr = mailDb->GetDBHdrForKey(entry->key);
				if (msgHdr)
				{
					mailDb->Close();
					break;
				}
			}
			if (mailDb) mailDb->Close();
		}
		else if (entry->key != m_curKey)
		{
			msgHdr = m_pane->GetMsgView()->GetDB()->GetDBHdrForKey(entry->key);
			if (msgHdr) break;
		}
	}

	delete msgHdr;

	if (entry)
	{
		if (pFolderInfo && m_curFolder != entry->folder)
			*pFolderInfo = entry->folder;
		m_cursor = i;
		SetCurMessageKey(entry->folder, entry->key);
		return entry->key;
	}
	return MSG_MESSAGEKEYNONE;
}

void
BacktrackManager::Reset()
{
	BacktrackEntry *entry = NULL;
	int size = m_backtrackArray.GetSize();

	for (int i=0; i < size; i++)
	{
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(0);
		m_backtrackArray.RemoveAt(0);
		delete entry;
	}

	m_cursor = -1;
	m_curKey = MSG_MESSAGEKEYNONE;
	m_curFolder = NULL;

}


void
BacktrackManager::SetCurMessageKey(MSG_FolderInfo *folder,
								   MessageKey key)
{
	m_curFolder = folder;
	m_curKey = key;
}

void
BacktrackManager::RemoveEntries(MSG_FolderInfo *folder)
{
	int size = m_backtrackArray.GetSize();
	BacktrackEntry *entry = NULL;

	if (!folder || size <= 0)
		return;

	while (size)
	{
		entry = (BacktrackEntry *) m_backtrackArray.GetAt(--size);
		if (entry->folder == folder)
		{
			m_backtrackArray.RemoveAt(size);
			delete entry;
			if (m_cursor == size)
			{
				m_cursor = -1;
				m_curKey = MSG_MESSAGEKEYNONE;
				m_curFolder = NULL;
			}
			else if (m_cursor > size)
			{
				m_cursor--;
			}
		}
	}
}

