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
#include "thrlstst.h"
#include "msgdbvw.h"
#include "msgpane.h"
#include "msgfinfo.h"

extern "C"
{
	extern int MK_MSG_LOADING_MESSAGES;
}

MSG_ListThreadState::MSG_ListThreadState(MessageDBView *view, MSG_Pane *pane, XP_Bool getNewsMsgsAfterLoad, int32 totalHeaders)
{
	m_view = view;
	m_pane = pane;
	m_startKey = 0;
	m_totalHeaders = totalHeaders;
	m_headersListed = 0;
	m_getNewsMsgsAfterLoad = getNewsMsgsAfterLoad;
	XP_ASSERT(pane && view);
}

MSG_ListThreadState::~MSG_ListThreadState()
{
}

int		MSG_ListThreadState::DoSomeWork()
{
	const int kIdChunkSize = 200;
	int			numListed = 0;
	MessageKey	idArray[kIdChunkSize];
	int32		flagArray[kIdChunkSize];
	char		levelArray[kIdChunkSize];
	SortType sortType;
	SortOrder sortOrder;
	m_view->GetDB()->GetSortInfo(&sortType, &sortOrder);
	MsgERR err;

	err = m_view->GetDB()->ListThreadIds(&m_startKey, m_view->GetViewType() == ViewOnlyNewHeaders, idArray, 
									flagArray, levelArray, kIdChunkSize, &numListed, m_view, &m_headersListed);
	if (err == eCorruptDB)
	{
		m_view->GetDB()->SetSummaryValid(FALSE);
		err = eEXCEPTION;
	}

#ifndef XP_OS2_HACK
	int32 percent = (int32) (100 * (double)m_headersListed /
							 (double) m_totalHeaders);
#else
	int32 percent = 0; /*protect against m_totalHeaders == 0*/ 

	if (m_totalHeaders != 0) 
	{
		percent = (int32) (100 * (double)m_headersListed /
							 (double) m_totalHeaders);
	} /* endif */
#endif
	FE_SetProgressBarPercent (m_pane->GetContext(), percent);
	FE_Progress(m_pane->GetContext(), XP_GetString(MK_MSG_LOADING_MESSAGES));
	if (err == eSUCCESS)
	{
		int32 numAdded = m_view->AddKeys(idArray, flagArray, levelArray, sortType, numListed);
//		if (pCount)
//			*pCount += numAdded;
	}
	if (err != eSUCCESS || m_startKey == kIdNone)
	{
		m_view->InitSort(sortType, sortOrder);
		return MK_CONNECTED;
	}
	else
		return MK_WAITING_FOR_CONNECTION;
}

XP_Bool MSG_ListThreadState::AllDone(int /* status */)
{
	// time to list the new news headers...perhaps should be a subclass
	if (m_getNewsMsgsAfterLoad && m_pane && m_pane->GetFolder() && m_pane->GetFolder()->IsNews())
		m_pane->GetNewNewsMessages(m_pane, m_pane->GetFolder(), FALSE);
	else if (m_pane->GetFolder() && m_pane->GetFolder()->IsMail())
		FE_PaneChanged(m_pane, TRUE, MSG_PaneNotifyFolderLoaded, (uint32) m_pane->GetFolder());

	// Tell front end we're done.
	m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
	m_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
	return TRUE;
}

