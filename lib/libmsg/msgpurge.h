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

#ifndef MsgPurge_H
#define MsgPurge_H

#include "msg.h"
#include "msgdbtyp.h"

class MessageDB;
class NewsGroupDB;
class NewsMessageHdr;
class MSG_FolderInfo;
class DBMessageHdr;
class NewsFolderInfo;
struct ListContext;

class MSG_PurgeNewsgroupState
{
public:
	MSG_PurgeNewsgroupState(MSG_Pane *pane, MSG_FolderInfo *folder);
	virtual	~MSG_PurgeNewsgroupState();
	MsgERR			Init();
	int				PurgeSomeMore();
	int				FinishPurging();
protected:
	void			PurgeHeader(DBMessageHdr *hdr);
	void			PurgeArticle(DBMessageHdr *hdr);

	NewsGroupDB		*m_newsDB;
	MSG_FolderInfo	*m_folder;
	NewsFolderInfo	*m_groupInfo;
	int32			m_purgeCount;
	int32			m_headerIndex;
	MWContext		*m_context;
	MSG_Pane		*m_pane;
	MSG_PurgeInfo	m_headerPurgeInfo;
	MSG_PurgeInfo	m_articlePurgeInfo;
	ListContext		*m_listContext;
	DBMessageHdr	*m_pNextHeader;
};

#endif
