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

#ifndef _THREAD_MSGHDR_H
#define _THREAD_MSGHDR_H

#include "msghdr.h"

class DBThreadMessageHdr 
{
public:
	DBThreadMessageHdr();
	DBThreadMessageHdr(MSG_ThreadHandle handle);
	virtual ~DBThreadMessageHdr();
	virtual void		AddChild(DBMessageHdr *child, MessageDB *db, XP_Bool threadInThread);
	virtual MessageKey	GetChildAt(int index);
	virtual DBMessageHdr* GetChild(MessageKey msgId);
	virtual DBMessageHdr *GetChildHdrAt(int index);
	virtual void		RemoveChild(MessageKey msgId, MSG_DBHandle db);
	virtual void		MarkChildRead(XP_Bool bRead, MSG_DBHandle db);
	virtual MessageKey	GetFirstUnreadKey(MessageDB *db);
			MessageKey	GetThreadID() {return m_threadKey;}
	virtual DBMessageHdr *GetFirstUnreadChild(MessageDB *db);
			void		GetSubject(char *subject, int subjectLen);
			void		SetNumChildren(uint16 numChildren);
			uint16		GetNumChildren();
			void		SetNumNewChildren(uint16 numNewChildren);
			uint16		GetNumNewChildren();
			uint32		GetFlags();
			void		SetFlags(uint32 flags);
			uint32		OrFlags(uint32 flags);
			uint32		AndFlags(uint32 flags);
			MSG_ThreadHandle GetHandle() {return m_dbThreadHandle;}

	MessageKey	m_threadKey;
#ifdef DEBUG_bienvenu
	static  int32		m_numThreadHeaders;
#endif
protected:
	MSG_ThreadHandle m_dbThreadHandle;
	uint16		m_numChildren;		
	uint16		m_numNewChildren;	
	uint32		m_flags;

};


#endif
