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
/* msgundmg.h --- internal defs for undo
 */

#ifndef UndoManager_H
#define UndoManager_H

#include "xp.h"
#include "ptrarray.h"
#include "idarray.h"
#include "msgzap.h"
#include "chngntfy.h"

class UndoManager;
class BacktrackManager;
class MessageDBView;


/*	Because undo can be asynchronous (e.g., undoing an IMAP action),
	the UndoManager and UndoObjects need to be able be asynchronous
	as well. Operations that are synchronous should be undone synchronously, 
	though.
*/


// -*- Forward class declaration -*-
class UndoManager;

// Generic undo object - includes UndoMarkers (start and end blocks), and UndoActions.
// Clients subclass UndoActions to implement their action specific operations.
class UndoObject : public MSG_ZapIt
{
	friend class UndoManager;
public:
	
	UndoObject();
	virtual		~UndoObject();
	
	// A synchronous undo returns an UndoComplete status.
	// An asynchronous undo kicks off an URL and returns a UndoInProgress staus.
	// Upon receiving UndoComplete status from DoUndo(), FinalizeUndo() is 
	// call by UndoManager to finalize the undo action.
	// When an asynchronous undo URL exit, FinalizeUndo() needs to be invoked
	// through url_structs->pre_exit_fn. 
	// What should we do if DoUndo() returns UndoError/UndoFailed should we
	// remove the undo object from the Undo/Redo queue?
	virtual UndoStatus	DoUndo();

	// StopUndo is called with a status from libnet.
	virtual UndoStatus StopUndo(int status);
	
	// FinalizeUndo() needs to do two things:
	// (1) call UndoManager::AddUndoAtion() to add an inverse undo operation
	// (2) call UndoManager::DoUndoComplete() to inform the undo manager
	// that the undo action has been completed. UndoObject will then be removed
	// and deleted from the queue.
	virtual void		FinalizeUndo();

	virtual void		SetOwner(UndoManager *owner) {m_owner = owner;}
	
	virtual const char *GetUndoTypeString() { return NULL; }
	virtual XP_Bool		IsAction() {return FALSE;}
	virtual XP_Bool		IsStart() {return FALSE;}
	virtual XP_Bool		IsEnd() {return FALSE;}
	virtual XP_Bool		IsMarker() {return FALSE;}

	// Is this undo object associate with the folder? Client should overwrite
	// this method. A typical use of this method is when we need to remove an 
	// undo action from the undo/redo queue due to a compress action.
	virtual XP_Bool		HasFolder(MSG_FolderInfo * /*folder*/) { return TRUE; }

	virtual void AddRefCount() { ++m_refCount; }
	virtual int Release ();

	// This ties the undo action to the url. This is based on the assumption
	// that we have msg_pane set to url->msg_pane and there is an undomanager
	// attached to the msg_pane. And the current undo action is temprary added
	// to the UndoManager's m_inProgressList. Until we have a better mechanism
	// to chain the URL. This will be the way we attach an undo action to an
	// URL.
	virtual void		UndoURLHook (URL_Struct *url);

	// If UndoURLPreExitFunc gets called with a negative status
	// StopUndo() will be called to allow client to interpret the 
	// failure and perform certain undo actions accordingly.
	static  void		UndoURLPreExitFunc (URL_Struct *url,
											int status,
											MWContext *context);

	virtual UndoStatus UndoPreExit() { return UndoComplete; }


protected:
	int m_refCount;
	UndoManager	*m_owner;	
	// chaining the url->pre_exit_fn if needed.
	Net_GetUrlExitFunc *m_preExitFunc_hook;
};

// UndoMarkers are the start and end blocks for wrapping multiple undo objects
// into a single user-undoable action. .
class UndoMarker : public UndoObject
{
public:
	UndoMarker(XP_Bool isStart) { m_isStart = isStart;}
	virtual ~UndoMarker() {}
	
	void			SetIsStart(XP_Bool isStart) {m_isStart = isStart;}

	virtual void	FinalizeUndo();
	virtual XP_Bool	IsStart() {return m_isStart;}
	virtual XP_Bool IsEnd() {return !m_isStart;}
	virtual XP_Bool IsMarker() {return TRUE;}
protected:
	XP_Bool		m_isStart;
};

// You might want to have undo actions which are mirrors of each other
// for undo/redo. They usually need the same information. The object
// could keep track of which direction it's going, and copy itself
// to the redo stack when an undo is done, etc.

// Undo actions may need to protect themselves from things they refer
// to going away out from under them. This may involve adding a reference
// to a db or a view, or making themselves listeners.

// Override this to implement your undo actions.
class UndoAction : public UndoObject
{
public:
	UndoAction() {}
	virtual ~UndoAction() {}
	virtual XP_Bool		IsAction() {return TRUE;}
};

// The state machine for undo is encapsulated here. The UndoManager has two
// primary functions - Keeping track of undoable/redoable operations, and
// executing undo/redo commands.  There should be one UndoManager for every
// top-level window which has Undo/Redo menu items. 

class UndoManager : public MSG_ZapIt
{
public:
	UndoManager(MSG_Pane *pane, int maxActions = 30);
	int				Init();
	MSG_Pane		*GetOwner() {return m_pane;}

	// Starting a batch of undo actions
	int				StartBatch();
	// Adding an UndoAction to the undo/redo queue
	// UndoManager internally keep of where to add the
	// undo action.
	virtual int		AddUndoAction(UndoObject *action);
	// Ending a batch of undo actions
	int				EndBatch();
	// Client can call AbortBatch() to abort creating an atomic
	// undo operation.
	int				AbortBatch();

	// Remove undo actions associate with this folder
	void RemoveActions(MSG_FolderInfo *folder);

	virtual XP_Bool	CanUndo();
	virtual XP_Bool	CanRedo();
	UndoStatus		Undo();
	UndoStatus		Redo();

	// clear out the undo/redo queues.
	void			FlushActions();
	// Reset() the undo state and do some clean up
	void			Reset();

	UndoMgrState	GetState() {return m_state;}
	UndoStatus		GetStatus() {return m_status;}
	int				GetDepth() {return m_depth;}

	// **** DoUndoComplete() has to be called from UndoObject::FinalizeUndo()
	// to inform the undo manager that the undo action has been completed. It
	// is time to remove from the undo/redo queue.
	void			DoUndoComplete(UndoObject *theObject);

	// Methods use to temprary attach undo object to the
	// in progress list. The undo object is not an official
	// undo/redo object yet.
	int				AddInProgress (UndoObject *theObject);
	UndoObject		*GetInProgress ();
	UndoObject		*RemoveInProgress ();

	// Methods for temprary storing/removing destination folder message key
	// in a first in first out fashion
	int				AddMsgKey (MessageKey key);
	MessageKey		GetAndRemoveMsgKey ();

	void			SetUndoMsgFolder(MSG_FolderInfo *folder);
	MSG_FolderInfo* GetUndoMsgFolder();

	virtual void AddRefCount() { ++m_refCount; }
	virtual int Release ();

protected:
	virtual			~UndoManager();	// must use Release() instead of delete.

	// Flush an atomic Undo/Redo operation at the bottom of the
	// queue;
	void			FlushAtomic(XPPtrArray *list);
	// Fire an atomic Undo/Redo operation from the top of the 
	// queue
	UndoStatus		FireAtomic(XPPtrArray *list);

	// Methods to handle atomic Undo Actions.
	void			AddAtomicUndo(UndoObject *theObject);
	void			AddAtomicRedo(UndoObject *theObject);
	void			FinalizeAtomicActions();

	XPPtrArray		m_undoList;
	XPPtrArray		m_redoList;

	UndoMgrState	m_state;
	int				m_maxUndoActions;	// max top-level actions 
	int				m_depth;			// Undo batch depth
	int				m_count;			// undo object count of an atomic operation
										// including markers
	
	UndoStatus		m_status;			// current undo action status

	// temp place holder for in progress undo actions
	XPPtrArray		m_inProgressList;

	// temp place holder for atomic operation in case we need to undo/redo
	// partial completed operations.
	XPPtrArray		m_atomicUndoList;
	XPPtrArray		m_atomicRedoList;

	// temp place holder for move copy messages to store the destination
	// folder message key; this can be viewed as a hack
	IDArray		m_msgKeyArray;
	MSG_FolderInfo  *m_undoMsgFolder;

	MSG_Pane		*m_pane; // owner of this undomanager
	int				m_refCount;
};

typedef struct {
	MSG_FolderInfo *folder;
	MessageKey	    key;
} BacktrackEntry;

class BacktrackManager : public MSG_ZapIt
{
public:

	BacktrackManager (MSG_Pane *pane);
	virtual ~BacktrackManager();

	MSG_BacktrackState GetState() { return m_state; }
	void SetState(MSG_BacktrackState state) {m_state = state;}

	int AddEntry(MSG_FolderInfo *folder, MessageKey key);
	void SetCurMessageKey(MSG_FolderInfo *folder, MessageKey key);
	// Remove entries associate with this folder
	void RemoveEntries(MSG_FolderInfo *folder);
	
	void Reset();

	XP_Bool CanGoBack();
	XP_Bool CanGoForward();
	MessageKey GoBack(MSG_FolderInfo **pFolderInfo);
	MessageKey GoForward(MSG_FolderInfo **pFolderInfo);

protected:
	MSG_Pane *m_pane;
	XPPtrArray m_backtrackArray;
	int32 m_cursor;
	MSG_BacktrackState m_state;
	MSG_FolderInfo *m_curFolder;
	MessageKey m_curKey;
};


#endif /* UndoManager_H */

