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
#include "thrhead.h"
#include "newsdb.h"
#include "msgdbapi.h"
#include "msgstrob.h"

#ifdef DEBUG_bienvenu
int32 DBThreadMessageHdr::m_numThreadHeaders = 0;
#endif
DBThreadMessageHdr::DBThreadMessageHdr() 
{
	m_numChildren = 0;
	m_flags = 0;
	m_numNewChildren = 0;
#ifdef DEBUG_bienvenu
	m_numThreadHeaders++;
#endif
}

DBThreadMessageHdr::DBThreadMessageHdr(MSG_ThreadHandle handle)
{
	MSG_DBThreadExchange threadInfo;

	m_dbThreadHandle = handle;
	MSG_ThreadHandle_GetThreadInfo(handle, &threadInfo);
	m_threadKey = threadInfo.m_threadKey;
	m_numChildren = threadInfo.m_numChildren;
	m_numNewChildren = threadInfo.m_numNewChildren;
	m_flags = threadInfo.m_flags;
#ifdef DEBUG_bienvenu
	m_numThreadHeaders++;
#endif
}

DBThreadMessageHdr::~DBThreadMessageHdr()
{
#ifdef DEBUG_bienvenu
	m_numThreadHeaders--;
#endif
	if (m_dbThreadHandle)
		MSG_ThreadHandle_RemoveReference(m_dbThreadHandle);
}

// newHeader comes out with an extra reference added.
void		DBThreadMessageHdr::AddChild(DBMessageHdr *newHeader, MessageDB *messageDB, XP_Bool threadInThread)
{
	MSG_ThreadHandle_AddChild(GetHandle(), newHeader->GetHandle(), messageDB->GetDB(), threadInThread);

	if (m_flags & kIgnored)	// if thread ignored, mark child read.
		messageDB->MarkHdrRead(newHeader, TRUE, NULL);
}

MessageKey	DBThreadMessageHdr::GetChildAt(int index)
{
	return MSG_ThreadHandle_GetChildAt(GetHandle(), index);
}

DBMessageHdr *DBThreadMessageHdr::GetChildHdrAt(int index)
{
	DBMessageHdr *returnHdr = NULL;
	MSG_HeaderHandle childHandle = MSG_ThreadHandle_GetChildHdrAt(GetHandle(), index);
	if (childHandle)
		returnHdr = new DBMessageHdr(childHandle);	// will it matter that this is not the appropriate sub-class?
	return returnHdr;
}

DBMessageHdr* DBThreadMessageHdr::GetChild(MessageKey msgId)
{
	MSG_HeaderHandle childHandle = MSG_ThreadHandle_GetChildForKey(GetHandle(), msgId);
	DBMessageHdr *returnHdr = NULL;

	if (childHandle)
		returnHdr = new DBMessageHdr(childHandle);	// will it matter that this is not the appropriate sub-class?
	return returnHdr;
}

MessageKey DBThreadMessageHdr::GetFirstUnreadKey(MessageDB *messageDB)
{
	DBMessageHdr	*hdr = GetFirstUnreadChild(messageDB);
	MessageKey	retKey = (hdr) ? hdr->GetMessageKey() : kIdNone;
	return retKey;
}

// caller must refer to msgHdr if they want to hold on to it.
DBMessageHdr *DBThreadMessageHdr::GetFirstUnreadChild(MessageDB *messageDB)
{
	DBMessageHdr	*retHdr = NULL;
	uint16 numChildren = GetNumChildren();

	for (uint16 childIndex = 0; childIndex < numChildren; childIndex++)
	{
		MessageKey msgId = GetChildAt(childIndex);
		if (msgId != 0)
		{
			XP_Bool isRead = FALSE;
			if (messageDB->IsRead(msgId, &isRead) == eSUCCESS && !isRead)
			{
				retHdr = messageDB->GetDBHdrForKey(msgId);
				break;
			}
		}
	}
	return retHdr;
}

void DBThreadMessageHdr::RemoveChild(MessageKey msgKey, MSG_DBHandle db)
{
	MSG_ThreadHandle_RemoveChildByKey(GetHandle(), msgKey, db);
}


void DBThreadMessageHdr::MarkChildRead(XP_Bool bRead, MSG_DBHandle db)
{
	MSG_ThreadHandle_MarkChildRead(GetHandle(), bRead, db);
}

// copy the subject back into the passed buffer.
void DBThreadMessageHdr::GetSubject(char *outSubject, int subjectLen)
{
	char		*subjectStr = NULL;
	XPStringObj subject;

	MSG_ThreadHandle_GetSubject(GetHandle(), &subjectStr, NULL);
	subject.SetStrPtr(subjectStr);

	if (subjectStr != NULL)
		XP_STRNCPY_SAFE(outSubject, subjectStr, subjectLen - 1);
}

void DBThreadMessageHdr::SetNumChildren(uint16 numChildren)
{
	m_numChildren = numChildren;
}

void DBThreadMessageHdr::SetNumNewChildren(uint16 numNewChildren)
{
	m_numNewChildren = numNewChildren;
}

uint16 DBThreadMessageHdr::GetNumChildren()
{
	return MSG_ThreadHandle_GetNumChildren(GetHandle());
}

uint16 DBThreadMessageHdr::GetNumNewChildren()
{
	return m_numNewChildren;
}

uint32		DBThreadMessageHdr::GetFlags()
{
	return m_flags;
}

void		DBThreadMessageHdr::SetFlags(uint32 flags)
{
	m_flags = flags;
}

uint32		DBThreadMessageHdr::OrFlags(uint32 flags)
{
	uint32 retflags = m_flags |= flags;
	return retflags;
}

uint32		DBThreadMessageHdr::AndFlags(uint32 flags)
{
	uint32 retflags = (m_flags &= flags);
	return retflags;
}

