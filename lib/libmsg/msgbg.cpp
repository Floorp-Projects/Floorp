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
#include "msgbg.h"
#include "msgpane.h"
#include "msgurlq.h"

extern "C" {
	extern int MK_OUT_OF_MEMORY;
}

msg_Background::msg_Background() 
{
}


msg_Background::~msg_Background()
{
	if (m_pane) Interrupt();
}


int
msg_Background::Begin(MSG_Pane* pane)
{
	XP_ASSERT(!m_pane);
	if (m_pane) return -1;
	char* url = XP_STRDUP("mailbox:?background");
	if (!url) return MK_OUT_OF_MEMORY;
	m_urlstruct = NET_CreateURLStruct(url, NET_NORMAL_RELOAD);
	if (!m_urlstruct) {
		XP_FREE(url);
		return MK_OUT_OF_MEMORY;
	}
	XP_FREE(url);
	m_urlstruct->internal_url = TRUE;
	m_pane = pane;
	msg_InterruptContext(pane->GetContext(), TRUE);
	XP_ASSERT(pane->GetCurrentBackgroundJob() == NULL);
	pane->SetCurrentBackgroundJob(this);
	MSG_UrlQueue::AddUrlToPane(m_urlstruct, msg_Background::PreExit_s, pane);
	return 0;
}



void
msg_Background::Interrupt()
{
	XP_ASSERT(m_pane);
	if (m_pane) {
		msg_InterruptContext(m_pane->GetContext(), FALSE);
		XP_ASSERT(m_pane == NULL);
	}
}


msg_Background*
msg_Background::FindBGObj(URL_Struct* urlstruct)
{
	XP_ASSERT(urlstruct && urlstruct->msg_pane);
	if (!urlstruct || !urlstruct->msg_pane) return NULL;
	msg_Background* result = urlstruct->msg_pane->GetCurrentBackgroundJob();
	// OK, I'm truly evil, but I'm using this as an empty url that goes through
	// netlib once. so result will be null...
	XP_ASSERT(!result || result->m_pane == urlstruct->msg_pane);
	return result;
}


int
msg_Background::ProcessBackground(URL_Struct* urlstruct)
{
	msg_Background* obj = FindBGObj(urlstruct);
	if (obj) {
		return obj->DoSomeWork();
	}
	return MK_CONNECTED;
}



void
msg_Background::PreExit_s(URL_Struct* urlstruct, int status,
						  MWContext* context)
{
	msg_Background* obj = FindBGObj(urlstruct);
	if (obj) {
		obj->PreExit(urlstruct, status, context);
	}
}

void
msg_Background::PreExit(URL_Struct* /*urlstruct*/, int status,
						MWContext*)
{
	XP_Bool deleteself = AllDone(status);
	XP_ASSERT(m_pane);
	if (m_pane) {
		m_pane->SetCurrentBackgroundJob(NULL);
		m_pane = NULL;
	}
	if (deleteself) delete this;
}


XP_Bool
msg_Background::AllDone(int /*status*/)
{
	return FALSE;
}

XP_Bool
msg_Background::IsRunning()
{
	return m_pane != NULL;
}
