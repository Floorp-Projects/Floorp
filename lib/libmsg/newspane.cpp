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
#include "newspane.h"
#include "pmsgsrch.h"

MSG_OfflineNewsSearchPane::MSG_OfflineNewsSearchPane(MWContext* context, MSG_Master* master) : MSG_SearchPane(context, master)
{
	m_exitFunc = 0;
	m_exitFuncData = 0;
	m_hitFunc = 0;
	m_hitFuncData = 0;
}

MSG_OfflineNewsSearchPane::~MSG_OfflineNewsSearchPane()
{
}

int		MSG_OfflineNewsSearchPane::GetURL (URL_Struct *url, XP_Bool /*isSafe*/)
{
	url->fe_data = this;
	return NET_GetURL(url, FO_CACHE_AND_PRESENT, m_context, MSG_OfflineNewsSearchPane::SearchExitFunc);
}

/* static */void MSG_OfflineNewsSearchPane::SearchExitFunc(URL_Struct *url,
														   int /*status*/,
														   MWContext *)
{
	MSG_OfflineNewsSearchPane *newsSearchPane =(MSG_OfflineNewsSearchPane *) url->fe_data;
	XP_ASSERT (newsSearchPane);
	if (!newsSearchPane) 
		return;
	newsSearchPane->CallExitFunc();
}

void	MSG_OfflineNewsSearchPane::SetExitFunc(OfflineNewsSearchExit *exitFunc, void *exitFuncData)
{
	m_exitFunc = exitFunc;
	m_exitFuncData = exitFuncData;
}

// We'd prefer to use a listener but maybe this is all that's required.
void	MSG_OfflineNewsSearchPane::SetReportHitFunction(OfflineNewsReportHit *hitFunc, void *hitData)
{
	m_hitFunc = hitFunc;
	m_hitFuncData = hitData;
}

void MSG_OfflineNewsSearchPane::CallExitFunc()
{
	if (m_exitFunc)
		(*m_exitFunc)(m_exitFuncData);
}

void	MSG_OfflineNewsSearchPane::StartingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/)
{
}

void	MSG_OfflineNewsSearchPane::EndingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex where,
							  int32 /*num*/)
{
	MSG_ResultElement	*resultElem;
	MSG_SearchValue		*searchValue;
	if (MSG_GetResultElement (this, where, &resultElem) == SearchError_Success)
	{
		if (resultElem->GetValue (attribMessageKey, &searchValue) == SearchError_Success)
		{
			m_keysFound.Add(searchValue->u.key);
			if (m_hitFunc != 0)
				(*m_hitFunc)(m_hitFuncData, searchValue->u.key);
			resultElem->DestroyValue(searchValue);
		}
		MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (this);
		if (frame)
		{
			MSG_ResultElement *res = frame->m_resultList.GetAt(where);
			if (res)
			{
				delete res;
				frame->m_resultList.RemoveAt(where);
			}
		}
	}
	// here we need to destroy the search frames' result elements... ###dmb todo
}

