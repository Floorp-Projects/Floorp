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

#ifndef _MsgBgCleanup_H_
#define _MsgBgCleanup_H_

#include "msgbg.h"
#include "ptrarray.h"

class MSG_CompressFolderState;
class MSG_PurgeNewsgroupState;

enum BackgroundCleanupState
{
	BGCleanupStart = 0,
	BGCleanupNextGroup,
	BGCleanupMoreHeaders,
	BGCleanupAbort
};


class MSG_BackgroundCleanup : public msg_Background
{
public:
	MSG_BackgroundCleanup(XPPtrArray &foldersToCleanup);
	virtual ~MSG_BackgroundCleanup();

protected:
	MSG_FolderArray	m_foldersToCleanup;
	int			m_folderIndex;
	MSG_FolderInfo	*m_folder;
	MSG_CompressFolderState		*m_currentFolderCompressor;
	MSG_PurgeNewsgroupState		*m_currentNewsgroupPurger;
	virtual int DoSomeWork();
	virtual XP_Bool AllDone(int status);
	int DoNextFolder();
	int DoProcess();
	int StopCompression();
	int StopPurging();
	BackgroundCleanupState		m_state;
	int32						m_bytesCompressed;
	int32						m_bytesTotal;
};

#endif
