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
#include "xp.h"
#include "newshdr.h"
#include "imapoff.h"	// for offline body blob chunking
#include "msgdbapi.h"

/* ****************************************************************** */
						/** NewsMessageHdr Class **/
/* ****************************************************************** */
NewsMessageHdr::NewsMessageHdr()
{
	m_messageSize = 0;
	if (!m_dbHeaderHandle)
		m_dbHeaderHandle = GetNewNewsHeaderHandle();
}

NewsMessageHdr::NewsMessageHdr(MSG_HeaderHandle handle) : DBMessageHdr(handle)
{
	m_messageSize = 0;
}

int32 NewsMessageHdr::AddToArticle(const char *block, int32 length, MSG_DBHandle db )
{
	return MSG_HeaderHandle_AddToOfflineMessage(m_dbHeaderHandle, block, length, db);
}

int32 NewsMessageHdr::ReadFromArticle(char *block, int32 length, int32 offset, MSG_DBHandle db)
{
	return MSG_HeaderHandle_ReadFromOfflineMessage(m_dbHeaderHandle, block, length, offset, db);
}

void NewsMessageHdr::PurgeOfflineMessage(MSG_DBHandle db)
{
	MSG_HeaderHandle_PurgeOfflineMessage(m_dbHeaderHandle, db);
}

int32 NewsMessageHdr::GetOfflineMessageLength(MSG_DBHandle db) 
{
	return MSG_HeaderHandle_GetOfflineMessageLength(m_dbHeaderHandle, db);
}

uint32	NewsMessageHdr::GetByteLength()
{
// if we have the message size in bytes, we should return it.
//	return (m_flags & kNewsSizeInBytes) ? m_messageSize : 0;
	return 0;
}

