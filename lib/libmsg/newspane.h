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
#ifndef MSG_NEWSSEARCH_PANE_H
#define MSG_NEWSSEARCH_PANE_H

#include "msgspane.h"
#include "idarray.h"

typedef int OfflineNewsSearchExit(void *exitData);
typedef void OfflineNewsReportHit(void *hitData, MessageKey hitKey);

// this class is used to search off-line news databases, usually for
// applying off-line retrieval options to a database, or for applying
// header/article retention policies. We are using search to get its
// background url handling for free, and its matching engine.

// We had to subclass MSG_SearchPane to override GetURL (so we can provide
// our own exit function so that we can go on to the next part of the process)
// We subclass the Starting and Ending Update functions so we can look at
// each result as it comes in.

// Searches for retrieving off-line will typically look like this:
// Date > 01/23/96 AND Date < 01/23/97 AND Size < 5000 AND Status != isRead
// AND Status != Offline

// Searches for purging articles will typically look like this:
// Date < 01/23/96 AND Status == isRead AND Status == Offline

// Searches for purging headers will typically look like this:
// Date < 01/23/96 AND Status == isRead

// Customers will either wait for the whole array of matching keys
// to be constructed before acting on them (e.g., Offline retrieval)
// or will act on each key in turn (purging articles or headers). We
// could require that customers who want notification of each add
// become view listeners. Oh sure, why not.

class MSG_OfflineNewsSearchPane : public MSG_SearchPane
{
public:
					MSG_OfflineNewsSearchPane(MWContext* context, MSG_Master* master);
	virtual			~MSG_OfflineNewsSearchPane();
	virtual int		GetURL (URL_Struct *url, XP_Bool isSafe);
	virtual void	StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num);
	virtual void	EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num);
	void			CallExitFunc();
	static void		SearchExitFunc(URL_Struct *url , int status, MWContext *context);
	IDArray			*GetKeysFound() {return &m_keysFound;}
	virtual void	SetExitFunc(OfflineNewsSearchExit *exitFunc, void *exitFuncData);
	virtual void	SetReportHitFunction(OfflineNewsReportHit *hitFunc, void *hitData);
protected:
	IDArray							m_keysFound;
	OfflineNewsSearchExit			*m_exitFunc;
	void							*m_exitFuncData;
	OfflineNewsReportHit			*m_hitFunc;
	void							*m_hitFuncData;
};
#endif
