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
//
// msgspane.cpp -- Pane for search results

#include "msg.h"
#include "msgspane.h"
#include "pmsgsrch.h"
#include "ptrarray.h"
#include "msgfinfo.h"


MSG_SearchPane::MSG_SearchPane (MWContext *context, MSG_Master *master) : MSG_ThreadPane (context, master)
{
}

MSG_SearchPane::~MSG_SearchPane ()
{
}


// MSG_Pane methods

MSG_PaneType MSG_SearchPane::GetPaneType()
{
	return MSG_SEARCHPANE;
}

MsgERR MSG_SearchPane::MoveMessages (const MSG_ViewIndex *indices,
									 int32 numIndices,
									 MSG_FolderInfo *folder)
{
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this);
	if (frame)
		return frame->MoveMessages(indices, numIndices, folder); 
	else
		return eFAILURE;
}

MsgERR MSG_SearchPane::CopyMessages (const MSG_ViewIndex *indices, 
									 int32 numIndices, 
									 MSG_FolderInfo *folder,
									 XP_Bool deleteAfterCopy)
{
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this);
	if (frame)
		return frame->CopyMessages(indices, numIndices, folder, deleteAfterCopy);
	else
		return eFAILURE;
}

MSG_DragEffect MSG_SearchPane::DragMessagesStatus(const MSG_ViewIndex *indices,
										   int32 numIndices,
										   MSG_FolderInfo * destFolder,
										   MSG_DragEffect request)
{
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this);
	if (frame)
		return frame->DragMessagesStatus(indices, numIndices, destFolder, request); 
	else
		return MSG_Drag_Not_Allowed;
}




MsgERR MSG_SearchPane::DoCommand(MSG_CommandType command,
								MSG_ViewIndex* indices, 
								int32 numIndices)
{
	// currently we only support delete message and delete messages no trash...
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this);
	if (!frame)
		return eFAILURE;

	switch (command)
	{
	case MSG_DeleteMessage:
		return frame->TrashMessages(indices, numIndices);
	case MSG_DeleteMessageNoTrash:
		return frame->DeleteMessages(indices, numIndices); // REALLY DELETES!! DOESN'T DELETE TO TRASH
	default:
		break;
	}

	return eFAILURE;  // we didn't recognize the command type
}


void MSG_SearchPane::CloseView (MSG_FolderInfo * folder)
{
	// whenever a search as view command is performed which required a messageDBView, 
	// this routine informs the search frame to close the view (it had been keeping it open)
	// of course we need the folder, so we can identify which view in the search frame to close.
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this); 
	if (frame)
		frame->CloseView(folder);
}

MessageDBView* MSG_SearchPane::GetView (MSG_FolderInfo * folder)
{
	MSG_SearchFrame * frame = MSG_SearchFrame::FromPane(this);
	if (frame)
		return frame->GetView(folder);
	else
		return NULL;
}


MessageKey
MSG_SearchPane::GetMessageKey(MSG_ViewIndex index)    // added by mscott in testing search as view capabilities
{
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (this);  // retrieve searchFrame from the pane/context
	if (!frame)
		return MSG_MESSAGEKEYNONE;
	MSG_ResultElement * elem = NULL;
	frame->GetResultElement(index, &elem);
	const MSG_SearchValue *keyValue = elem->GetValueRef(attribMessageKey);
	return keyValue->u.key;
}


void MSG_SearchPane::OnFolderDeleted (MSG_FolderInfo *folder)
{
	// Here we catch the broadcast message that a folder has been deleted. 
	// In the context of the search pane, this means we remove any search 
	// hits which refer to it, since those hits hold a stale pointer
	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (this);
	XP_ASSERT(frame);
	if (frame)
		frame->DeleteResultElementsInFolder (folder);

	// Call our parent in case they need to respond to this notification
	MSG_ThreadPane::OnFolderDeleted (folder);
}

// MSG_LinedPane methods

void MSG_SearchPane::ToggleExpansion (MSG_ViewIndex /*line*/, int32 * /*numChanged*/)
{
}

int32 MSG_SearchPane::ExpansionDelta (MSG_ViewIndex /*line*/)
{
	XP_ASSERT(0); // will we be able to expand mailing lists here?
	return 0;
}

int32 MSG_SearchPane::GetNumLines ()
{
	MSG_SearchFrame *frame = GetContext()->msg_searchFrame;
	XP_ASSERT(frame);
	return frame->m_resultList.GetSize();
}

// MSG_PrefsNotify methods

void MSG_SearchPane::NotifyPrefsChange (NotifyCode /*code*/)
{
}

