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
// msgspane.cpp -- Pane for search results

#ifndef _MSGSPANE_H
#define _MSGSPANE_H

#include "xp.h"
#include "errcode.h"
#include "msg.h"
#include "msgtpane.h" 
#include "msgdbvw.h" 
#include "ptrarray.h"


// MSG_FolderKeyPair is used to specify an individual message unambiguously.
// This is useful for operating on groups of messages which live in 
// different folderInfos, as is done in the searchView
//
class MSG_FolderKeyPair
{
public:
	MSG_FolderKeyPair (MSG_FolderInfo *folder, MessageKey key)
		: m_key(key), m_folder(folder) {}

	MessageKey m_key;
	MSG_FolderInfo *m_folder;
};


// MSG_PairArray is an array with type-safe accessors for MSG_FolderKeyPair
//
class MSG_PairArray : public XPPtrArray
{
public:
	MSG_FolderKeyPair *GetAt(int i) const { return (MSG_FolderKeyPair*) XPPtrArray::GetAt(i); }
};


// MSG_SearchPane is a pane which can do most of what the thread pane can
// do, in terms of copying and deleting messages. However, there are a 
// few commands we have to disable, and searchPane also contains the 
// sorting code which would normally be done by the (homogeneous) view
//
class MSG_SearchPane : public MSG_ThreadPane
{
public:
	MSG_SearchPane (MWContext *, MSG_Master *);
	~MSG_SearchPane ();

	// currently we only support delete as the only valid command on a search pane. 
	virtual MsgERR DoCommand(MSG_CommandType command,
							MSG_ViewIndex* indices, int32 numindices);

	// search as view related methods. Call this when a search as view command which
	// required a view of a DB has been completed. 
	void CloseView(MSG_FolderInfo * folder);

	// this method is NOT intended to return a view from the search frame...it works only
	// when you have a search as view command pending which requires a view of a DB. 
	// only call it when you know what you are doing....
	MessageDBView * GetView(MSG_FolderInfo * folder); 

	virtual MsgERR MoveMessages (const MSG_ViewIndex *indices,
							int32 numIndices,
							MSG_FolderInfo *folder);

	virtual MsgERR CopyMessages (const MSG_ViewIndex *indices, 
							int32 numIndices, 
							MSG_FolderInfo *folder,
							XP_Bool deleteAfterCopy);

	virtual MSG_DragEffect DragMessagesStatus(const MSG_ViewIndex *indices,
											int32 numIndices,
											MSG_FolderInfo * destFolder,
											MSG_DragEffect request);

	// MSG_Pane methods
	virtual MSG_PaneType GetPaneType();
	virtual void OnFolderDeleted (MSG_FolderInfo *folder);

	// MSG_LinedPane methods
	virtual void ToggleExpansion (MSG_ViewIndex line, int32 *numChanged);
	virtual int32 ExpansionDelta (MSG_ViewIndex line);
	virtual int32 GetNumLines ();
	
	virtual MessageKey GetMessageKey(MSG_ViewIndex index); // added by mscott

	// MSG_PrefsNotify methods
	virtual void NotifyPrefsChange (NotifyCode code);
};


#endif // _MSGSPANE_H
