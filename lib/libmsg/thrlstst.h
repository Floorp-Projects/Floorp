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
// Thread List State Object
#include "msgbg.h"

class MessageDBView;
class MSG_Pane;
class MSG_ListThreadState : public msg_Background
{
public:
	MSG_ListThreadState(MessageDBView *view, MSG_Pane *pane, XP_Bool getNewMsgsAfterLoad, int32 totalHeaders);
	~MSG_ListThreadState();
	virtual int DoSomeWork();
	virtual XP_Bool AllDone(int status);
protected:
	MessageDBView	*m_view;
	MSG_Pane		*m_pane;
	MessageKey		m_startKey;
	int32			m_totalHeaders;
	int32			m_headersListed;
	XP_Bool			m_getNewsMsgsAfterLoad;
};
