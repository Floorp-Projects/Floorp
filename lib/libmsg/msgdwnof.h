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

#ifndef _Msgdwnof_H_
#define _Msgdwnof_H_

#include "msg.h"
class MSG_FolderIterator;
class MSG_FolderInfo;
class MSG_Pane;
class NewsGroupDB;

class MSG_DownloadOfflineFoldersState 
{
public:
	MSG_DownloadOfflineFoldersState(MSG_Pane *pane);
	~MSG_DownloadOfflineFoldersState();
	int		DownloadOneFolder(MSG_FolderInfo *folder);
	int		DoIt();
	MSG_FolderInfo		*GetCurFolder() {return m_curFolder;}
static					FolderDoneCB(void *state, int status);
static void				DownloadArticlesCB(URL_Struct *url , int status, MWContext *context);
protected:
	int					DoSomeMore();
	int					DownloadFolder(MSG_FolderInfo *curFolder);
	int					DownloadArticles(MSG_FolderInfo *curFolder);

	MSG_FolderIterator	*m_folderIterator;
	MSG_FolderInfo		*m_curFolder;
	MSG_Pane			*m_pane;
	NewsGroupDB			*m_newsDB;
	XP_Bool				m_oneFolder;
	XP_Bool				m_foundFolderToDownload;
};
#endif

