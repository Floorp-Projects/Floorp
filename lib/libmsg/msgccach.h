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

#ifndef _MSGCCACH_H
#define _MSGCCACH_H

#include "ptrarray.h"

struct MSG_CachedConnection
{
	TNavigatorImapConnection	*m_connection;
	MSG_FolderInfo				*m_folder;
	MSG_IMAPHost				*m_host;
	int64						m_ImapCacheTimeStamp;
};

class MSG_CachedConnectionList : public XPPtrArray
{
public:
	MSG_CachedConnectionList();
	virtual ~MSG_CachedConnectionList();
	XP_Bool						AddConnection(TNavigatorImapConnection *, MSG_FolderInfo *folder, MSG_IMAPHost *host);
	TNavigatorImapConnection	*RemoveConnection(MSG_FolderInfo *folder, MSG_IMAPHost *host);
	XP_Bool						HasConnection(MSG_FolderInfo *folder, MSG_IMAPHost *host, int &cacheIndex);
	void						FolderClosed(MSG_FolderInfo *folder);
	void						FolderDeleted(MSG_FolderInfo *folder);
	void						CloseAllConnections();
	MSG_CachedConnection		*GetAt (int i) const;
protected:
	int							m_numNullFolders;
	XP_Bool						ShouldRemoveConnection();

};

#endif
