/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "msg.h"
#include "msgoffnw.h"
#include "msgpane.h"
#include "msgfinfo.h"
#include "newsdb.h"
#include "newshdr.h"

extern "C"
{
	extern int MK_NEWS_ITEM_UNAVAILABLE;
}

MSG_OfflineNewsArtState::MSG_OfflineNewsArtState(MSG_Pane *pane,
												 const char * /*group*/,
												 uint32 messageNumber)
{
	m_pane = pane;
	m_bytesReadSoFar = 0;
	if (!pane->GetFolder() || !pane->GetFolder()->IsNews())
		return ;

	MessageDBView *view = pane->GetMsgView();
	if (!view)
		return ;
	m_newsDB = view->GetDB()->GetNewsDB();
	if (m_newsDB)
	{
		m_newsHeader = m_newsDB->GetNewsHdrForKey(messageNumber);
		m_articleLength = (m_newsHeader) ? m_newsHeader->GetOfflineMessageLength(m_newsDB->GetDB()) : 0;
	}
	else
		m_newsHeader = NULL;
}

MSG_OfflineNewsArtState::~MSG_OfflineNewsArtState()
{
	delete m_newsHeader;
}

// Returns negative error codes for errors, 0 for done, positive number
// for number of bytes read.
int	MSG_OfflineNewsArtState::Process(char *outputBuffer, int outputBufSize)
{
	int32 bytesRead;

	if (!m_newsHeader)
		return MK_NEWS_ITEM_UNAVAILABLE;
	if (m_bytesReadSoFar >= m_articleLength)
	{
//		m_newsHeader->OrFlags(kOffline);  // eh? we're pulling it out of the db - no need to change flag
		return 0;
	}
	int bytesToCopy = MIN(outputBufSize, m_articleLength - m_bytesReadSoFar);
	bytesRead = m_newsHeader->ReadFromArticle(outputBuffer, bytesToCopy, m_bytesReadSoFar, m_newsDB->GetDB());

	m_bytesReadSoFar += bytesRead;
	return bytesRead;
}

int MSG_OfflineNewsArtState::Interrupt()
{
	if (m_newsHeader != NULL)
	{
		delete m_newsHeader;
		m_newsHeader = NULL;
	}
	return 0;
}
