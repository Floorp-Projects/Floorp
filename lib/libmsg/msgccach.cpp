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

#include "msg.h"
#include "msgccach.h"
#include "imap.h"
#include "prtime.h"
#include "msgimap.h"
#include "prefapi.h"

const int32 kImapCacheTimeLimitInSeconds = 1740;	// 29 minutes, RFC says server must not
// drop before 30 minutes


MSG_CachedConnectionList::MSG_CachedConnectionList()
{
	m_numNullFolders = 0;
}

MSG_CachedConnectionList::~MSG_CachedConnectionList()
{
}

XP_Bool MSG_CachedConnectionList::AddConnection(TNavigatorImapConnection *connection, MSG_FolderInfo *folder, MSG_IMAPHost *host)
{
	int32		maxCachedConnections = 10;
	PREF_GetIntPref("mail.imap.max_cached_connections", &maxCachedConnections);
#ifdef DEBUG_bienvenu
	if (GetSize() >= maxCachedConnections)
	{
		XP_Trace("connection cache full!\n");
		for (int32 i = 0; i < GetSize(); i++)
		{
			struct MSG_CachedConnection *curConn = GetAt(i);
			if (curConn)
				XP_Trace("in connection cache - %s\n", curConn->m_folder  ? curConn->m_folder->GetName() : "");
		}
	}
#endif
	if (GetSize() >= maxCachedConnections)
	{	// try to find a connection to remove
		XPPtrArray referringPanes;

		for (int32 i = 0; i < GetSize(); i++)
		{
			struct MSG_CachedConnection *curConn = GetAt(i);
			MSG_FolderInfo *connFolder = curConn ? curConn->m_folder : 0;

			// remove the first cached connection we can find that is not for an inbox,
			// and which isn't being displayed in any window
			if (connFolder && !(connFolder->GetFlags() & MSG_FOLDER_FLAG_INBOX))
			{
				connFolder->GetMaster()->FindPanesReferringToFolder(connFolder, &referringPanes);
				if (referringPanes.GetSize() == 0)
				{
					IMAP_TerminateConnection (curConn->m_connection);
					RemoveAt(i);
					delete curConn;
					break;
				}
			}
		}
	}
	if (GetSize() >= maxCachedConnections || (folder == NULL && m_numNullFolders > 1))
		return FALSE;

	struct MSG_CachedConnection *cachedConnection = new MSG_CachedConnection; 

	if (cachedConnection)
	{
		if (!folder)
			m_numNullFolders++;
		cachedConnection->m_connection = connection;
		cachedConnection->m_folder = folder;
		cachedConnection->m_host = (folder) ? ((MSG_IMAPFolderInfoMail *) folder)->GetIMAPHost() : host;
#ifndef NSPR20
		cachedConnection->m_ImapCacheTimeStamp = PR_NowS();	// time in seconds
#else
		{
			int64 nowS;
			LL_I2L(nowS, PR_IntervalToSeconds(PR_IntervalNow()));
			cachedConnection->m_ImapCacheTimeStamp = nowS;	// time in seconds
		}
#endif /* NSPR20 */
		Add(cachedConnection);
		return TRUE;
	}
	return FALSE;
}

TNavigatorImapConnection * MSG_CachedConnectionList::RemoveConnection(MSG_FolderInfo *folder, MSG_IMAPHost *host)
{
	int cacheIndex;
	TNavigatorImapConnection *returnConnection = NULL;
	XP_Bool hasConnection = HasConnection(folder, host, cacheIndex);
	if (hasConnection)
	{
		struct MSG_CachedConnection *curConn = GetAt(cacheIndex);
		returnConnection = curConn->m_connection;
		delete curConn;
		RemoveAt(cacheIndex);
	}
	// if we didn't find one, try finding an unused (by an open folder) connection
	if (!returnConnection && folder)
		return RemoveConnection(NULL, host);

	return returnConnection;
}

XP_Bool	MSG_CachedConnectionList::HasConnection(MSG_FolderInfo *folder, MSG_IMAPHost *host, int &cacheIndex)
{
	TNavigatorImapConnection *returnConnection = NULL;
	int32		timeoutLimitInSeconds = kImapCacheTimeLimitInSeconds;

	int i;
	for (i = 0; i < GetSize(); i++)
	{
		struct MSG_CachedConnection *curConn = GetAt(i);
		// if curConn->m_folder is NULL
		if (curConn && curConn->m_folder == folder && curConn->m_host == host)
		{
			returnConnection = curConn->m_connection;

			if (curConn->m_folder == NULL)
				m_numNullFolders--;
			if (returnConnection)
			{
				// check to see if time limit expired
				int64 cacheTimeLimit;
				int64 diffFromLimit;

				int32 overriddenTimeLimit;
				if (PREF_GetIntPref("mail.imap.connection_timeout", &overriddenTimeLimit) == PREF_OK)
					timeoutLimitInSeconds = overriddenTimeLimit;

				LL_I2L(cacheTimeLimit, timeoutLimitInSeconds);
#ifndef NSPR20
				LL_SUB(diffFromLimit, PR_NowS(), curConn->m_ImapCacheTimeStamp); // r = a - b
#else
				{
					int64 nowS;
					LL_I2L(nowS, PR_IntervalToSeconds(PR_IntervalNow()));
					LL_SUB(diffFromLimit, nowS, curConn->m_ImapCacheTimeStamp); // r = a - b
				}
#endif /* NSPR20 */
				LL_SUB(diffFromLimit, diffFromLimit, cacheTimeLimit); // r = a - b
				if (LL_GE_ZERO(diffFromLimit))
				{
					// the time limit was exceeded, kill this connection
					IMAP_TerminateConnection(returnConnection);
					delete curConn;
					RemoveAt(i);
					returnConnection = NULL;
				}
			}
			break;
		}
	}
	// if we didn't find one, try finding an unused (by an open folder) connection

	cacheIndex = i;
	return (returnConnection != NULL);
}

struct MSG_CachedConnection *MSG_CachedConnectionList::GetAt (int i) const
{
	return (struct MSG_CachedConnection *) XPPtrArray::GetAt(i);
}

void MSG_CachedConnectionList::CloseAllConnections()
{
	for (int i = 0; i < GetSize(); i++)
	{
		struct MSG_CachedConnection *cachedConn = GetAt(i);
		if (cachedConn)
		{
			IMAP_TerminateConnection (cachedConn->m_connection);
			delete cachedConn;
		}
	}
	RemoveAll();
}

void MSG_CachedConnectionList::FolderClosed(MSG_FolderInfo *folder)
{
	if (!(folder->GetFlags() & MSG_FOLDER_FLAG_INBOX))
	{
		// if one of the connections in the connection cache is for
		// this folder, close it and remove it. If we are the only 
		// connection, leave it, but clear the folder so we will
		// re-use the connection for another folder.
		for (int i = 0; i < GetSize(); i++)
		{
			struct MSG_CachedConnection *cachedConn = GetAt(i);
			if (cachedConn && cachedConn->m_folder == folder)
			{
				// We want to account for the fact that we will leave a
				// connection for the inbox open at all times. It's possible
				// that we could have two connections that aren't the inbox
				if (ShouldRemoveConnection())
				{
					IMAP_TerminateConnection (cachedConn->m_connection);
					RemoveAt(i);
					delete cachedConn;
				}
				else
				{
					cachedConn->m_folder = NULL;
				}
			}
		}
	}
}

void MSG_CachedConnectionList::FolderDeleted(MSG_FolderInfo *folder)
{
	{
		// if one of the connections in the connection cache is for
		// this folder, close it and remove it.
		for (int i = 0; i < GetSize(); i++)
		{
			struct MSG_CachedConnection *cachedConn = GetAt(i);
			if (cachedConn && cachedConn->m_folder == folder)
			{
				IMAP_TerminateConnection (cachedConn->m_connection);
				RemoveAt(i);
				delete cachedConn;
				break;
			}
		}
	}
}


// check if we already have a connection cached that is not for the inbox.
XP_Bool MSG_CachedConnectionList::ShouldRemoveConnection()
{
	for (int i = 0; i < GetSize(); i++)
	{
		struct MSG_CachedConnection *cachedConn = GetAt(i);
		if (cachedConn && (!cachedConn->m_folder || !(cachedConn->m_folder->GetFlags() & MSG_FOLDER_FLAG_INBOX)))
			return TRUE;
	}
	return FALSE;
}

