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
