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
#include "msg.h"
#include "msgppane.h"
#include "msgfpane.h"
#include "msgmpane.h"
#include "msgtpane.h"
#include "msgfinfo.h"

MSG_ProgressPane::MSG_ProgressPane(MWContext* context, MSG_Master* master,
										 MSG_Pane *parentPane) 
: MSG_Pane(context, master)
{
	m_parentPane = parentPane;

	// progress panes must not look appetizing to JavaScript
	//XP_ASSERT(MWContextMailNewsProgress == context->type);
	if (context->type != MWContextMailNewsProgress)
		context->type = MWContextMailNewsProgress; 
	if (m_parentPane)
		m_parentPane->SetShowingProgress(TRUE);
}

MSG_ProgressPane::~MSG_ProgressPane()
{
	if (GetParentPane())
		m_parentPane->SetShowingProgress(FALSE);

	if (m_context->imapURLPane == this)
		m_context->imapURLPane = NULL;
}


MsgERR
MSG_ProgressPane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
					int32 numIndices)
{
	int status = 0;

	if ((command == MSG_GetNewMail || command == MSG_GetNextChunkMessages) && m_parentPane)
	{
		MSG_FolderInfo	*folder = m_parentPane->GetFolder();
		if (folder && folder->IsNews())
			status = GetNewNewsMessages(m_parentPane, folder, command == MSG_GetNextChunkMessages);
		else if (command == MSG_GetNewMail)
			status = GetNewMail(m_parentPane, folder);
		return status;
	}
	else if (command == MSG_CompressFolder)
	{
		XP_ASSERT(numIndices <= 1);
		MSG_FolderInfo *folder = NULL;
		switch (m_parentPane->GetPaneType()) {
			case MSG_FOLDERPANE:
				if (numIndices == 1)
					folder = ((MSG_FolderPane*) m_parentPane)->GetFolderInfo(indices[0]);
				break;
			case MSG_THREADPANE:
				folder = ((MSG_ThreadPane *) m_parentPane)->GetFolder();
				break;
			case MSG_MESSAGEPANE:
				folder = ((MSG_MessagePane *) m_parentPane)->GetFolder();
				break;
			default:
				XP_ASSERT(FALSE);	// msg command status should be false
				break;
		}
		
		if (folder && folder->IsMail())
		{
			MSG_FolderInfoMail *mailFolder = folder->GetMailFolderInfo();
			if (mailFolder)
				status = CompressOneMailFolder(mailFolder);
		}
		
		return status;
	}
	else
		return MSG_Pane::DoCommand(command, indices, numIndices);
}

MSG_FolderInfo	*MSG_ProgressPane::GetFolder()
{
	MSG_FolderInfo	*retFolder = NULL;
	if (GetParentPane())
		retFolder = m_parentPane->GetFolder();
	return retFolder;
}

void MSG_ProgressPane::CrushUpdateLevelToZero()
{
	if (GetParentPane())
		m_parentPane->CrushUpdateLevelToZero();
}

void MSG_ProgressPane::StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	if (GetParentPane())
		m_parentPane->StartingUpdate(code, where, num);
}
void MSG_ProgressPane::EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num)
{
	if (GetParentPane())
		m_parentPane->EndingUpdate(code, where, num);
}

MSG_Pane* MSG_ProgressPane::GetParentPane()
{
	// parent pane may have been deleted w/o us knowing, so check if it's in master list.
	if (!PaneInMasterList(m_parentPane))
		m_parentPane = NULL;
	return m_parentPane;
}

void MSG_ProgressPane::OnFolderAdded (MSG_FolderInfo *folder, MSG_Pane *instigator)
{
	// If the FE has run the IMAP New Folder command in a progress pane, they need 
	// to know when to take down the New Folder dialog box.
	if (instigator == this)
		FE_PaneChanged (this, FALSE, MSG_PaneNotifySelectNewFolder, MSG_VIEWINDEXNONE);

	MSG_Pane::OnFolderAdded (folder, instigator);
}

