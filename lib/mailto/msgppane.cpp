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
		return MSG_Pane::DoCommand(command, indices, numIndices);
}

MSG_Pane* MSG_ProgressPane::GetParentPane()
{
	// parent pane may have been deleted w/o us knowing, so check if it's in master list.
	if (!PaneInMasterList(m_parentPane))
		m_parentPane = NULL;
	return m_parentPane;
}

