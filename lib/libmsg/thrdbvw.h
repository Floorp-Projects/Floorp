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
#ifndef THREAD_VIEW_H
#define THREAD_VIEW_H

#include "msgdbvw.h"

class ThreadDBView : public MessageDBView
{
public:
	ThreadDBView(ViewType viewType);
	~ThreadDBView();
	const char * GetViewName(void) {return "ThreadedView"; }
	virtual MsgERR Open(MessageDB *messageDB, ViewType viewType,uint32* pCount, XP_Bool runInForeground = TRUE);
	// thread and add m_newHeaders to db and view.
	virtual MsgERR		AddNewMessages();
	virtual MsgERR		AddHdrFromServerLine(char *line, 
											MessageId *msgId);
	virtual	MsgERR		OnNewHeader(MessageKey newKey, XP_Bool ensureListed);
	virtual MsgERR		AddHdr(DBMessageHdr *msgHdr);
	virtual MsgERR		Sort(SortType sortType, SortOrder sortOrder);
	virtual void		SetInitialSortState(void);
	virtual int32		AddKeys(MessageKey *pOutput, int32 *pFlags, char *pLevels, SortType sortType, int numListed);
	virtual MsgERR		InitSort(SortType sortType, SortOrder sortOrder);
	virtual	MsgERR		Init(uint32 *pCount, XP_Bool runInForeground = TRUE);
protected:

	virtual MsgERR	ExpandAll();
	virtual void	OnHeaderAddedOrDeleted();	
	virtual void	OnExtraFlagChanged(MSG_ViewIndex index, char extraFlag);
	void			ClearPrevIdArray();
	virtual MSG_ViewIndex GetInsertInfoForNewHdr(MessageKey newKey, MSG_ViewIndex startTheadIndex, int8 *pLevel);
	virtual MsgERR	RemoveByIndex(MSG_ViewIndex index);

	XP_Bool		m_havePrevView;
	IDArray		m_prevIdArray;
	XPByteArray m_prevFlags;
	XPByteArray m_prevLevels;
	XPPtrArray	m_newHeaders;	 // array of new DBMessageHdr *, prior to threading
	int			m_headerIndex;	// index to add next header at.
};

#endif
