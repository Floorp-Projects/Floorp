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

#include "rosetta.h"
#include "msg.h"
#include "hosttbl.h"
#include "newshost.h"
#include "msgmast.h"
#include "prefapi.h"

msg_HostTable* msg_HostTable::Create(MSG_Master* master)
{
	msg_HostTable *result = NULL;

	result = new msg_HostTable(master);
	XP_ASSERT(result);
	if (!result)				// Should have extra checks for memory
								// failures and such.  ###tw
	{
		delete result;
		result = NULL;
	}
	return result;
}

msg_HostTable::msg_HostTable(MSG_Master* master)
{
	char** newsrclist;
	char* defname = NULL;
	int32 defport = NEWS_PORT;
	HG73252

	PREF_CopyCharPref("network.hosts.nntp_server", &defname);
	if (defname && *defname == '\0') {
		XP_FREE(defname);
		defname = NULL;
	}
	if (defname) {
		PREF_GetIntPref("news.server_port", &defport);
		HG93733
	}
	
	char** rest;
	MSG_NewsHost *h = NULL;

	m_master = master;
	m_numhosts = 0;
	m_maxhosts = 0;
	m_hosts = NULL;

	newsrclist = XP_GetNewsRCFiles();
	char* allocatedName = NULL;
	for (rest = newsrclist; rest && *rest ; rest++) {
		FREEIF(allocatedName);
		allocatedName = XP_STRDUP(*rest);
		char* name = allocatedName;
		if (!name) continue;
		XP_Bool newsrc_name_ok = (!XP_STRCMP(name, "newsrc") ||
								  !XP_STRCMP(name, "snewsrc") ||
								  !XP_STRNCMP(name, "newsrc-", 7) ||
								  !XP_STRNCMP(name, "snewsrc-", 8));
		XP_ASSERT(newsrc_name_ok);
		if (!newsrc_name_ok) continue;
		int32 port = NEWS_PORT;
		
		HG29828
		name = XP_STRCHR(name, '-');
		if (name) name++;
		else {
			name = defname;
			port = defport;
			HG38276
		}
		if (!name || !*name) continue;
		char* ptr = XP_STRCHR(name, ':');
		if (ptr) {
			*ptr = '\0';
			port = 0;
			unsigned long p;
			char dummy;
			int n = sscanf(ptr+1, "%lu%c", &p, &dummy);
			port = p;
			XP_ASSERT(n == 1 && port != 0);
			if (n != 1 || port == 0) continue;
		}


		char* ptr2 = XP_STRCHR(*rest, '-');
		if (ptr2) ptr2++;
		else ptr2 = "";

		h = NULL;
		// Check that the newsrc file actually really exists before
		// bothering to add it to our database.
		XP_StatStruct statstr;
		if (XP_Stat(ptr2, &statstr, HG36527) >= 0) {
			h = AddNewsHost(name, HG98333, port, ptr2);
		}
	}
	FREEIF(allocatedName);

	for (rest = newsrclist; rest && *rest ; rest++) {
		XP_FREE(*rest);
	}
	
	XP_FREE(newsrclist);
	newsrclist = NULL;

	// ### let's try this
	h = GetDefaultHost(FALSE);
	if (h == NULL && defname) {
		h = AddNewsHost(defname, HG73267, defport, defname);
		if (h != NULL) {
			h->MarkDirty();		// so the default .newsrc file gets created
								// when we close the DB
		}
	}

	if (defname) XP_FREE(defname);
}


msg_HostTable::~msg_HostTable()
{
	for (int i=0 ; i<m_numhosts ; i++) {
		delete m_hosts[i];
	}
	delete [] m_hosts;
	m_hosts = NULL;
	m_numhosts = 0;
}


MSG_NewsHost *msg_HostTable::AddNewsHost(const char *name, XP_Bool xxx,
										 int32 port, const char* newsrcname)
{
	MSG_NewsHost *h = NULL;

	HG87366

	h = new MSG_NewsHost(m_master, name, xxx, port);

	XP_ASSERT(h && h->getStr() && XP_STRCMP(h->getStr(), name) == 0);
	if (!h || h->getStr() == NULL || XP_STRCMP(h->getStr(), name) != 0) {
		// Somehow ran out of memory or something.  Oh, well, skip it.
		delete h;
		return NULL;
	}
	if (m_numhosts >= m_maxhosts) {
		m_maxhosts += 10;
		MSG_NewsHost** oldlist = m_hosts;
		m_hosts = new MSG_NewsHost* [m_maxhosts];
		for (int i=0 ; i<m_numhosts ; i++) {
			m_hosts[i] = oldlist[i];
		}
		delete [] oldlist;
	}
	if (h->SetNewsrcFileName(newsrcname) != 0) {
		// bogus host, probably - better not leave it around or we will
		// die later.
		delete h;
		return NULL;
	}

	m_hosts[m_numhosts++] = h;
	return h;
}




int32
msg_HostTable::GetHostList(MSG_NewsHost** result, int32 resultsize)
{
	int32 n = getNumHosts();
	if (n > resultsize) n = resultsize;
	for (int32 i=0 ; i<n ; i++) {
		result[i] = getHost(i);
	}
	return getNumHosts();
}


MSG_NewsHost* 
msg_HostTable::GetDefaultHost(XP_Bool createIfMissing)
{
	MSG_NewsHost* host = NULL;
	char* defname = NULL;
	int32 defport = 0;
	HG83765

	PREF_CopyCharPref("network.hosts.nntp_server", &defname);
	if (defname && *defname == '\0') {
		XP_FREE(defname);
		defname = NULL;
	}
	if (defname) {
		PREF_GetIntPref("news.server_port", &defport);
		HG83178
		if (defport == 0) defport = HG37630 NEWS_PORT;
		for (int i=0 ; i<getNumHosts() ; i++) {
			host = getHost(i);
			if (host &&
				XP_STRCMP(host->getStr(), defname) == 0 &&
				host->getPort() == defport HG78636) break;
			host = NULL;
		}
		if (!host && createIfMissing) {
			host = m_master->AddNewsHost(defname, HG72688, defport);
		}
		XP_FREE(defname);
	}
	return host;
}

MSG_NewsHost *msg_HostTable::FindNewsHost(MSG_FolderInfo *container)
{
	for (int i = 0; i < getNumHosts(); i++) {
		MSG_NewsHost *host = getHost(i);
		if (host && host->GetHostInfo() == container) {
			return host;
		}
	}
	return NULL;
}


int
msg_HostTable::RemoveEntry(MSG_NewsHost* host)
{
	for (int i=0 ; i<getNumHosts() ; i++) {
		if (m_hosts[i] == host) {
			m_hosts[i] = m_hosts[--m_numhosts];
			return 0;
		}
	}
	XP_ASSERT(0);
	return -1;
}


MSG_NewsHost *msg_HostTable::InferHostFromGroups (const char *groups)
{
	MSG_NewsHost *host = NULL;
	msg_StringArray strings (TRUE /*owns memory*/);

	if (strings.ImportTokenList (groups, ", \t\r\n"))
	{
		for (int i = 0; i < strings.GetSize(); i++)
		{
			const char *group = strings.GetAt(i);
			for (int i = 0; i < getNumHosts(); i++)
			{
				MSG_NewsHost *host = getHost(i);
				if (host)
				{
					if (host->FindGroup (group))
						return host;
				}
			}

		}
	}

	return NULL;
}

