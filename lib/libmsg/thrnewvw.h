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
#ifndef _THREADS_WITH_NEW_H
#define _THREADS_WITH_NEW_H

#include "thrdbvw.h"

class ThreadsWithNewView : public ThreadDBView
{
public:
	ThreadsWithNewView(ViewType viewType);
	virtual ~ThreadsWithNewView();
	const char * GetViewName(void) {return "ThreadsWithNewView"; }
	virtual MsgERR Open(MessageDB *messageDB, ViewType viewType, /*WORD viewFlags,*/ uint32* pCount, XP_Bool runInForeground = TRUE);
	virtual XP_Bool		WantsThisThread(DBThreadMessageHdr *threadHdr);
protected:

};

// This view will initially be used for cacheless IMAP.
class CachelessView : public MessageDBView
{
public:
						CachelessView(ViewType viewType);
	virtual 			~CachelessView();
	const char * 		GetViewName(void) {return "CachelessView"; }
	virtual MsgERR		Open(MessageDB *messageDB, ViewType viewType, /*WORD viewFlags,*/ uint32* pCount, XP_Bool runInForeground = TRUE);
	MsgERR				ListShortMsgHdrByIndex(MSG_ViewIndex startIndex, int numToList, MSG_MessageLine *pOutput, int *pNumListed);
	MsgERR				SetViewSize(int32 setSize); // Override
	virtual MsgERR		AddNewMessages() ;
	virtual MsgERR		AddHdr(DBMessageHdr *msgHdr);
	// for news, xover line, potentially, for IMAP, imap line...
	virtual MsgERR		AddHdrFromServerLine(char *line, MessageKey *msgId) ;
	virtual void		SetInitialSortState(void);
	virtual	MsgERR		Init(uint32 *pCount, XP_Bool runInForeground /* = TRUE */);
protected:
	static void			ExitFunction (URL_Struct *URL_s, int status, MWContext *window_id);
	void				ClearPendingIds();

	MSG_FolderInfo		*m_folder;
	MSG_Master			*m_master;
	MSG_ViewIndex		m_curStartSeq;
	MSG_ViewIndex		m_curEndSeq;
	XP_Bool				m_sizeInitialized;
};

#endif
