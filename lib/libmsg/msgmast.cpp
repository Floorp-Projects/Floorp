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

#define FORCE_PR_LOG /* Allow logging in the release build */

#include "rosetta.h"
#include "msg.h"
#include "errcode.h"

#include "msgmast.h"
#include "msgprefs.h" 
#include "msgfinfo.h"
#include "msgfpane.h"
#include "msgtpane.h"
#include "hosttbl.h"
#include "newshost.h"
#include "prsembst.h"
#include "listngst.h"
#include "newsdb.h"
#include "maildb.h"
#include "prtime.h"
#include "msgbgcln.h"
#include "prefapi.h"
#include "rosetta.h"
#include HG99877
#include "msgdwnof.h"
#include "imapoff.h"
#include "msgurlq.h"
#include "xpgetstr.h"
#include "msgfcach.h"
#include "prlog.h"
#include "pmsgsrch.h"
#include "msgccach.h"
#include "imaphost.h"
#include "msgimap.h"
#include "nslocks.h"
#include "pw_public.h"

extern "C" {
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_SET_HTML_NEWSGROUP_HEIRARCHY_CONFIRM;
	extern int MK_MSG_FOLDER_ALREADY_EXISTS;
	extern int MK_MSG_INBOX_L10N_NAME;
	extern int MK_IMAP_UPGRADE_WAIT_WHILE_UPGRADE;
	extern int MK_IMAP_UPGRADE_PROMPT_QUESTION;
	extern int MK_IMAP_UPGRADE_CUSTOM;
	extern int MK_POP3_USERNAME_UNDEFINED;
	extern int XP_PASSWORD_FOR_POP3_USER;
	extern int XP_MSG_CACHED_PASSWORD_NOT_MATCHED;
	extern int XP_MSG_PASSWORD_FAILED;
	extern int MK_POP3_PASSWORD_UNDEFINED;
}

#ifndef NSPR20
PR_LOG_DEFINE(IMAP);
#else
PRLogModuleInfo *IMAP;
#endif

MSG_Master::MSG_Master(MSG_Prefs* prefs)
{
	XP_Bool purgeBodiesByAge;
	int32		purgeMethod;
	int32		daysToKeepHdrs;
	int32		headersToKeep;
	int32		daysToKeepBodies;

	PREF_GetBoolPref("news.enabled",&m_collabraEnabled);

	m_prefs = prefs;
	m_folderTree = NULL;
	m_IMAPfoldersBuilt = FALSE;
	m_upgradingToIMAPSubscription = FALSE;
	if (m_collabraEnabled)
		m_hosttable = msg_HostTable::Create(this);
	else
		m_hosttable = NULL;
	m_rulesSemaphoreHolder = NULL;
	m_editHeadersSemaphoreHolder = NULL;
	m_mailfolders = NULL;
	m_imapfolders = NULL;
	m_parseMailboxState = NULL;
	m_userAuthenticated = FALSE;
	m_localFoldersAuthenticated = FALSE;
	m_prefs->AddNotify(this);
	m_purgeHdrPref = new MSG_PurgeInfo;
	m_purgeArtPref = new MSG_PurgeInfo;
	PREF_GetIntPref("mail.purge_threshhold", &m_purgeThreshhold);
	PREF_GetIntPref("news.keep.method", &purgeMethod);
	PREF_GetIntPref("news.keep.days",                 &daysToKeepHdrs);    // days
	PREF_GetIntPref("news.keep.count",       &headersToKeep); //keep x newest messages
	PREF_GetBoolPref("news.keep.only_unread",          &m_purgeHdrPref->m_unreadOnly);
	PREF_GetBoolPref("news.remove_bodies.by_age",	    &purgeBodiesByAge);
	PREF_GetIntPref("news.remove_bodies.days",		&daysToKeepBodies);
	PREF_GetBoolPref("mail.password_protect_local_cache" , &m_passwordProtectCache);
	
	m_purgeHdrPref->m_purgeBy = (MSG_PurgeByPreferences) purgeMethod;
	m_purgeHdrPref->m_daysToKeep = daysToKeepHdrs;
	m_purgeHdrPref->m_numHeadersToKeep = headersToKeep;
	m_purgeArtPref->m_purgeBy = (purgeBodiesByAge) ? MSG_PurgeByAge : MSG_PurgeNone;
	m_mailAccountUrl = NULL;
	m_cachedConnectionList = NULL;
	m_playingBackOfflineOps = FALSE;
	m_progressWindow = NULL;
	m_imapUpgradeBeginPane = NULL;
	m_upgradingToIMAPSubscription = FALSE;

#ifndef NSPR20
	PR_LogInit(&IMAPLog);
#else
	IMAP = PR_NewLogModule("IMAP");
#endif
	// on the mac, use this java script preference
	// as an alternate to setenv
	XP_Bool imapIOlogging;
	PREF_GetBoolPref("imap.io.mac.logging",	    &imapIOlogging); 
	if (imapIOlogging)
		IMAP->level = PR_LOG_ALWAYS;

}

MSG_Master::~MSG_Master()
{
	m_prefs->RemoveNotify(this);

	MSG_FolderCache cache;
	cache.WriteToDisk (m_folderTree);

	delete m_hosttable;
	delete m_folderTree;
	delete m_purgeHdrPref;
	delete m_purgeArtPref;
	XP_FREEIF(m_mailAccountUrl);
	delete m_cachedConnectionList;
	delete m_imapHostTable;
}


// See if there is a previous connection cached for us, if so, then return it.
// If there is no connection, allow them to make
// a connection since it will be the first one, somebody has got to start one.

TNavigatorImapConnection* MSG_Master::UnCacheImapConnection(const char *host, const char *folderName)
{
	if (m_cachedConnectionList)		// Someone has made a connection at least once
	{
		if (folderName)
		{
			MSG_IMAPFolderInfoMail *folder = (folderName) ? FindImapMailFolder(host, folderName, NULL, FALSE) : 0;
			if (folder)
				return (m_cachedConnectionList->RemoveConnection(folder, folder->GetIMAPHost()));
		}
		else
			return m_cachedConnectionList->RemoveConnection(NULL, GetIMAPHost(host));
	}
	else 
	{
		m_cachedConnectionList = new MSG_CachedConnectionList;	// mark a connection has been created
	}
	
	return NULL;		// No connections have been made, you are the first dude
}

XP_Bool MSG_Master::HasCachedConnection(MSG_IMAPFolderInfoMail *folder)
{
	int cacheIndex;

	if (m_cachedConnectionList)		// Someone has made a connection at least once
		return (m_cachedConnectionList->HasConnection(folder, folder->GetIMAPHost(), cacheIndex));
	else
		return FALSE;
}

void MSG_Master::CloseCachedImapConnections()
{
	if (m_cachedConnectionList)
		m_cachedConnectionList->CloseAllConnections();
}

XP_Bool MSG_Master::TryToCacheImapConnection(TNavigatorImapConnection *connection, const char *host, const char *folderName)
{
	XP_Bool wasCached = FALSE;
	
	
	if (!m_cachedConnectionList)
		m_cachedConnectionList = new MSG_CachedConnectionList;

	if (m_cachedConnectionList)
	{

		MSG_FolderInfo *folder = (folderName) ? FindImapMailFolder(host, folderName, NULL, FALSE) : 0;
		wasCached = m_cachedConnectionList->AddConnection(connection, folder, GetIMAPHost(host));
	}
#ifdef DEBUG_bienvenu
	XP_Trace("trying to uncache %s result = %s\n", folderName, (wasCached) ? "TRUE" : "FALSE");
#endif
	return wasCached;
}

void MSG_Master::ImapFolderClosed(MSG_FolderInfo *folder)
{
	if (m_cachedConnectionList)
		m_cachedConnectionList->FolderClosed(folder);
}

void
MSG_Master::SetFirstPane(MSG_Pane* pane)
{
	m_firstpane = pane;
}


MSG_Pane*
MSG_Master::GetFirstPane()
{
	return m_firstpane;
}

void MSG_Master::NotifyPrefsChange(NotifyCode code) 
{
	// km - There is no way to get a notification from anybody but the MSG_Prefs
	// object.  Therefore hacking NET_Setpopusername (name wrong) cannot work without
	// also adding to MSG_Prefs
	
	if (HG24326 (m_prefs->GetMailServerIsIMAP4()))
	{
		// if we're using IMAP
		// then we should clear the connection cache and info here
		IMAP_ResetAnyCachedConnectionInfo();
		// if there is any existing cached imap connection, tell it to die
		CloseCachedImapConnections();
	}
	else if (m_firstpane && ((code == MailServerType) || (code == PopHost)))
	{
		// biff state is unknown now
		FE_UpdateBiff(MSG_BIFF_Unknown);
		
		// if the new mail server is IMAP, then delete any previous
		// imap thread and message panes
		if (m_prefs->GetMailServerIsIMAP4())
		{
			// mwelch will add code here for pane deletion?
		}
		
		// remove any existing imap folders from the folders from the folder pane.
		if (m_imapfolders)
			BroadcastFolderDeleted (m_imapfolders);
		
		// leak m_imapfolders.  Until the closure of thread panes and
		// message panes works, there will be dangling references
		// in the front ends.
		if (m_imapfolders && m_folderTree)
		{
			m_folderTree->RemoveSubFolder(m_imapfolders);
			m_imapfolders = NULL;
		}

		// if the new server is imap, populate the folder pane.
		if (m_prefs->GetMailServerIsIMAP4())
		{
			// BuildIMAPFolderTree will flag the IMAP inbox with
			// MSG_FOLDER_FLAG_INBOX.  Remove this flag from any pop
			// mailbox that has it.
			MSG_FolderInfo *offlineInbox = NULL;
			if (GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &offlineInbox, 1))
			{
				XP_ASSERT(offlineInbox);
				offlineInbox->ClearFlag(MSG_FOLDER_FLAG_INBOX);
				BroadcastFolderChanged(offlineInbox);
			}
			
			// start fresh
			IMAP_ResetAnyCachedConnectionInfo();
			
			// build the imap folder tree
			m_imapfolders =
			MSG_IMAPFolderInfoMail::BuildIMAPFolderTree(
				this, 				// master is used by libnet
				1,					
				m_folderTree->GetSubFolders()); // parent of imap folders
				
			// add them to the folder pane
			if (m_imapfolders)
			{
				m_IMAPfoldersBuilt = TRUE;
				BroadcastFolderAdded (m_imapfolders);
			}
		}
		else
		{
			if (m_IMAPfoldersBuilt)
			{
				// this was a switch from imap to pop.  Flag the pop inbox
				MSG_FolderInfoMail *offlineInbox = FindMagicMailFolder(MSG_FOLDER_FLAG_INBOX, TRUE);
				if (offlineInbox)
				{
					offlineInbox->SetFlag(MSG_FOLDER_FLAG_INBOX);
					BroadcastFolderChanged (offlineInbox);
				}
			}
			m_IMAPfoldersBuilt = FALSE;
			m_imapfolders = NULL;
		}

		// if there is any existing cached imap connection, tell it to die
		CloseCachedImapConnections();
	}
	else if (code == ChangeIMAPDeleteModel)
	{
		IMAP_ResetAnyCachedConnectionInfo();
		CloseCachedImapConnections();
	}
}

int32
MSG_Master::GetFolderChildren(MSG_FolderInfo* folder,
							  MSG_FolderInfo** result,
							  int32 resultsize)
{
	if (folder == NULL) 
		folder = GetFolderTree();
	int32 num = folder->GetNumSubFolders();
	int32 numResults = 0;

	if (result && resultsize > num) 
		resultsize = num;

	for (int i=0 ; i<num ; i++) 
	{
		MSG_FolderInfo *child = folder->GetSubFolder(i);
		if (child->CanBeInFolderPane())
		{
			if (result)
				result[numResults] = child;
			numResults++;
		}
	}

	return numResults;
}


int32
MSG_Master::GetFoldersWithFlag(uint32 flags, MSG_FolderInfo** result,
							   int32 resultsize)
{
  return GetFolderTree()->GetFoldersWithFlag(flags, result, resultsize);
}

XP_Bool
MSG_Master::GetFolderLineById(MSG_FolderInfo* folder,
								MSG_FolderLine* data) {
	XP_BZERO (data, sizeof(*data));
	XP_ASSERT(folder);

	if (!folder) return FALSE;
	data->level = folder->GetDepth();

	data->name = folder->GetName();
	data->prettyName = folder->GetPrettyName();
	data->flags = folder->GetFlags();
	data->prefFlags = folder->GetFolderPrefFlags();
	data->unseen = folder->GetNumUnread() + folder->GetNumPendingUnread();
	data->total = folder->GetTotalMessages() + folder->GetNumPendingTotalMessages();
	data->deepUnseen = folder->GetNumUnread(TRUE /*deep?*/) +
					   folder->GetNumPendingUnread(TRUE /*deep?*/);
	data->deepTotal = folder->GetTotalMessages(TRUE /*deep?*/) +
					  folder->GetNumPendingTotalMessages(TRUE /*deep?*/);
	data->id = folder; 
	data->numChildren = folder->GetNumSubFoldersToDisplay();
	data->deletedBytes = folder->GetExpungedBytesCount();
	return TRUE;
}  


void MSG_Master::BroadcastFolderChanged(MSG_FolderInfo *folderInfo)
{
	MSG_Pane *pane;

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane()) 
	{
		if (pane->GetPaneType() == MSG_FOLDERPANE) 
		{
			MSG_FolderPane* folderPane = (MSG_FolderPane*) pane;

			MSG_FolderInfo *parent = folderInfo;
			while (parent)
			{
				// since we're not rolling up container counts, don't invalidate, since it slows down the mac.
				if (parent->GetType() != FOLDER_CONTAINERONLY)
					folderPane->OnFolderChanged (parent);
				parent = m_folderTree->FindParentOf(parent);
			}
		}
		else if (pane->GetPaneType() == MSG_THREADPANE)
		{
			pane->OnFolderChanged (folderInfo);
		}
	}
}

void MSG_Master::BroadcastFolderDeleted (MSG_FolderInfo *folderInfo)
{
	MSG_Pane *pane;
	XP_ASSERT (folderInfo);
	if (!folderInfo) 
		return;

	MSG_IMAPFolderInfoMail *imapFolder = folderInfo->GetIMAPFolderInfoMail();
	if (imapFolder && m_cachedConnectionList)
			m_cachedConnectionList->FolderDeleted(folderInfo);

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane())
		pane->OnFolderDeleted (folderInfo);
}


void MSG_Master::BroadcastFolderAdded (MSG_FolderInfo *folder, MSG_Pane *instigator)
{
	MSG_Pane *pane;
	XP_ASSERT(folder);
	if (!folder)
		return;

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane())
		pane->OnFolderAdded (folder, instigator);
}


void MSG_Master::BroadcastFolderKeysAreInvalid (MSG_FolderInfo *folder)
{
	MSG_Pane *pane;
	XP_ASSERT(folder);
	if (!folder)
		return;

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane())
		pane->OnFolderKeysAreInvalid (folder);
}


MSG_Pane *MSG_Master::FindFirstPaneOfType(MSG_PaneType type)
{
	return FindNextPaneOfType(GetFirstPane(), type);
}

MSG_Pane *MSG_Master::FindNextPaneOfType(MSG_Pane *startHere, MSG_PaneType type)
{
	MSG_Pane *pane;

	for (pane = startHere; pane != NULL; pane = pane->GetNextPane()) 
	{
		if (pane->GetPaneType() == type) 
			return pane;
	}
	return NULL;
}


MSG_Pane *MSG_Master::FindPaneOfType(MSG_FolderInfo *id, MSG_PaneType type)
{
	MSG_Pane *pane;

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane()) 
	{
		if (pane->GetPaneType() == type || type == MSG_ANYPANE) 
		{
			MSG_FolderInfo *paneFolder = pane->GetFolder();
			if (paneFolder == id)
				return pane;
		}
	}
	return NULL;
}


MSG_Pane *MSG_Master::FindPaneOfType (const char *url, MSG_PaneType type)
{
	char *path = NET_ParseURL (url, GET_PATH_PART);
	if (path)
	{
		MSG_FolderInfo *folder = FindMailFolder (path, FALSE /*createIfMissing*/);
		FREEIF(path);
		if (folder)
			return FindPaneOfType (folder, type);
	}
	return NULL;
}


void MSG_Master::ClearParseMailboxState()
{
	if (m_parseMailboxState != NULL)
	{
		delete m_parseMailboxState;
		m_parseMailboxState  = NULL;
	}
}

void MSG_Master::ClearParseMailboxState(const char *folderName)
{
	MSG_FolderInfoMail *mailFolder = FindMailFolder(folderName, FALSE);
	ParseMailboxState *parser = (mailFolder) ? mailFolder->GetParseMailboxState() : 0;
	if (parser != NULL)
	{
		delete parser;
		mailFolder->SetParseMailboxState(NULL);
	}
}

void MSG_Master::ClearListNewsGroupState(MSG_NewsHost* host,
										 const char *newsGroupName)
  {
	MSG_FolderInfoNews *newsFolder = FindNewsFolder(host, newsGroupName);
	ListNewsGroupState *state = (newsFolder) ? newsFolder->GetListNewsGroupState() : 0;
	if (state != NULL)
	{
		delete state;
		newsFolder->SetListNewsGroupState(NULL);
	}
}


XP_Bool MSG_Master::FolderTreeExists ()
{
	return m_folderTree != NULL;
}


MSG_FolderInfo *MSG_Master::GetFolderTree()
{
	if (!m_folderTree)
	{
		char* dir = XP_STRDUP(GetPrefs()->GetFolderDirectory());
		m_folderTree = new MSG_FolderInfoContainer("Parent of mail and news", 0, NULL);
		
		MSG_FolderArray *parentFolder = m_folderTree->GetSubFolders(); 

		m_folderCache = new MSG_FolderCache ();
		if (m_folderCache)
			m_folderCache->ReadFromDisk ();

		if (GetPrefs()->GetMailServerIsIMAP4() &&
	    	!m_IMAPfoldersBuilt)
		{
			// ###tw This probably needs revisiting now that news folders are
			// crammed in here too.
			m_imapfolders =
			MSG_IMAPFolderInfoMail::BuildIMAPFolderTree(
				this, 				// master is used by libnet
				1,					
				parentFolder);
				
			m_IMAPfoldersBuilt = TRUE;
		}

		
		m_mailfolders =	MSG_FolderInfoMail::BuildFolderTree(dir, 1, parentFolder, this);


		// Create the magic folders on startup

		///// DRAFTS //////
		XP_Bool imapDrafts = FALSE;
		char *draftsLocation = NULL;
		PREF_CopyCharPref("mail.default_drafts", &draftsLocation);
		imapDrafts = (draftsLocation ? 
				(NET_URL_Type(draftsLocation) == IMAP_TYPE_URL) :
				FALSE);

		if (imapDrafts)
			FindMagicMailFolder(MSG_FOLDER_FLAG_DRAFTS, FALSE);
		else
			FindMagicMailFolder(MSG_FOLDER_FLAG_DRAFTS, TRUE);

		///// TEMPLATES //////
		XP_Bool imapTemplates = FALSE;
		char *templatesLocation = NULL;
		PREF_CopyCharPref("mail.default_templates", &templatesLocation);
		imapTemplates = (templatesLocation ? (NET_URL_Type(templatesLocation) == IMAP_TYPE_URL) : FALSE);
		if (imapTemplates)
			FindMagicMailFolder(MSG_FOLDER_FLAG_TEMPLATES, FALSE);
		else
			FindMagicMailFolder(MSG_FOLDER_FLAG_TEMPLATES, TRUE);

		///// TRASH //////
		FindMagicMailFolder(MSG_FOLDER_FLAG_TRASH, TRUE);

		///// SENT //////
		FindMagicMailFolder(MSG_FOLDER_FLAG_SENTMAIL, TRUE);

		///// UNSENT (OUTBOX) //////
		MSG_SetQueueFolderName(QUEUE_FOLDER_NAME);
		MSG_FolderInfo *q = FindMagicMailFolder(MSG_FOLDER_FLAG_QUEUE, FALSE);	// see if we have "Unsent Messages"
		if (!q) {
			MSG_SetQueueFolderName(QUEUE_FOLDER_NAME_OLD);
			q = FindMagicMailFolder(MSG_FOLDER_FLAG_QUEUE, FALSE);	// if not, see if we have "Outbox"
			if (!q)
			{
				MSG_SetQueueFolderName(QUEUE_FOLDER_NAME);
				FindMagicMailFolder(MSG_FOLDER_FLAG_QUEUE,TRUE);	// if not, fresh install: create "Unsent Messages"
			}
			else {
				q->SetFlag(MSG_FOLDER_FLAG_QUEUE);	// make sure we know this is the queue folder.
			}
		}

		// It is kept sorted, but we must resort after promoting the magic folder.
		// jrm asks: probably faster to remove q, set the flag, then add it back.
		m_mailfolders->GetSubFolders()->QuickSort(m_mailfolders->CompareFolders);

		
		// for POP create the inbox here. Imap does it after it
		// discovers all of its mailboxes
		if (GetPrefs()->GetMailServerIsIMAP4())
			m_mailfolders->SetFlag(MSG_FOLDER_FLAG_ELIDED);	// collapse offline mail
		else
		{
			FindMagicMailFolder(MSG_FOLDER_FLAG_INBOX, TRUE);
			m_mailfolders->ClearFlag(MSG_FOLDER_FLAG_ELIDED);
		}

		// m_hosttable can be NULL if we failed to build the hosts DB
		int n = m_hosttable ? m_hosttable->getNumHosts() : 0;
		for (int i=0 ; i<n ; i++) {
			MSG_NewsHost* host = m_hosttable->getHost(i);
			XP_ASSERT(host);
			if (!host) continue;
			MSG_NewsFolderInfoContainer* info =
				new MSG_NewsFolderInfoContainer(host, 1);
			info->SetFlag(MSG_FOLDER_FLAG_ELIDED);
			info->SetFlag(MSG_FOLDER_FLAG_NEWS_HOST);
			parentFolder->Add(info);
			host->LoadNewsrc(info);	// ###tw  Sigh.  This is expensive.
								// Probably should at least put it off until
								// the user opens the host...
		}

		delete m_folderCache;
		m_folderCache = NULL;
		XP_FREE(dir);
	}
	
	return m_folderTree;
}

MSG_FolderInfoMail*
MSG_Master::GetLocalMailFolderTree()
{
	if (!m_mailfolders) {
		(void) GetFolderTree();	// Has a side effect of creating folders.
	}
	return m_mailfolders;
}

MSG_IMAPFolderInfoContainer*
MSG_Master::GetImapMailFolderTree()
{
	if (!m_imapfolders) {
		(void) GetFolderTree();	// Has a side effect of creating folders.
	}
	return m_imapfolders;
}

// Find a folder, but do not create the folder trees

MSG_FolderInfoMail* MSG_Master::FindFolderForNikiBiff(const char *path, const char *host, const char *owner)
{
	MSG_FolderInfoMail *folder = NULL;
	
	if (m_mailfolders)
		folder = m_mailfolders->FindPathname((const char*) path);
	if (!folder && m_imapfolders)
		folder = FindImapMailFolder(host, path, owner, FALSE);
	return folder;
}


MSG_NewsHost *MSG_Master::AddNewsHost(const char* hostname,
									  XP_Bool isxxx, int32 port)
{
	if (GetHostTable() == NULL) return NULL;
	int32 defaultport = HG19826 NEWS_PORT;
	if (port == 0) port = defaultport;
	MSG_NewsHost* newsHost = FindHost(hostname, isxxx, port);
	if (newsHost) return newsHost;
	char* hostAndPort = NULL;
	if (port != defaultport) {
		hostAndPort = PR_smprintf("%s:%ld", hostname, long(port));
		if (!hostAndPort) return NULL; // Out of memory.
	}
	newsHost =
		GetHostTable()->AddNewsHost(hostname,
									isxxx, port,
									hostAndPort ? hostAndPort : hostname);
	FREEIF(hostAndPort);
	if (newsHost)
	{
		XPPtrArray *parentFolder =
			(XPPtrArray *) GetFolderTree()->GetSubFolders();
		MSG_NewsFolderInfoContainer* info =
			new MSG_NewsFolderInfoContainer(newsHost, 1);
		info->SetFlag(MSG_FOLDER_FLAG_ELIDED);
		info->SetFlag(MSG_FOLDER_FLAG_NEWS_HOST);
		parentFolder->Add(info);
		// need to diddle the fat file, I guess - I'm lost.
		newsHost->SetNewsrcFileName(newsHost->getNameAndPort());

		newsHost->LoadNewsrc(info);	// ###tw  Sigh.  This is expensive.
							// Probably should at least put it off until
							// the user opens the host...

		newsHost->MarkDirty();	// Make sure the newsrc will get created if it
								// isn't already out there.  This makes sure
								// that adding the newshost will stick to the
								// next session, even if the user never
								// subscribes to any groups in it.  See bug
								// 39791.

		MSG_Pane* pane;
		for (pane = GetFirstPane() ; pane ; pane = pane->GetNextPane()) {
			if (pane->GetPaneType() == MSG_FOLDERPANE) {
				((MSG_FolderPane*) pane)->RedisplayAll();
			}
		}
	}
	return newsHost;
}

MSG_IMAPHostTable	*MSG_Master::GetIMAPHostTable() 
{
	if (!m_imapHostTable)
	{
		m_imapHostTable = new MSG_IMAPHostTable(this);
		GetFolderTree();	// make sure we've loaded the hosts
	}
	return m_imapHostTable;
}

MSG_IMAPHost *MSG_Master::AddIMAPHost(const char* hostname,
									  XP_Bool isxxx, 
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespace,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir,
							  			XP_Bool	writePrefs /* = TRUE */)
{
	MSG_IMAPHost* imapHost = GetIMAPHostTable()->FindIMAPHost(hostname);
	if (imapHost) 
		return imapHost;

	imapHost =
		GetIMAPHostTable()->AddIMAPHost(hostname, isxxx, userName, checkNewMail, biffInterval, rememberPassword,
			usingSubscription, overrideNamespace, personalOnlineDir, publicOnlineDir, otherUsersOnlineDir, writePrefs);
	return imapHost;
}

MSG_ThreadPane* MSG_Master::FindThreadPaneNamed(const char *pathname)
{
	MSG_Pane *pane;

	for (pane = GetFirstPane(); pane != NULL; pane = pane->GetNextPane()) {
		if (pane->GetPaneType() == MSG_THREADPANE) {
			MSG_ThreadPane* threadPane = (MSG_ThreadPane*) pane;
			MSG_FolderInfo *folderInfo = threadPane->GetFolder();
			MSG_FolderInfoMail *mailFolderInfo = (folderInfo) ? folderInfo->GetMailFolderInfo() : 0;
			if (mailFolderInfo && !XP_FILENAMECMP(mailFolderInfo->GetPathname(), pathname)) {
				return threadPane;
			}
		}
	}
	return NULL;
}


void MSG_Master::FindPanesReferringToFolder (MSG_FolderInfo *folder, XPPtrArray *outArray)
{
	for (MSG_Pane *pane = GetFirstPane() ; pane != NULL; pane = pane->GetNextPane())
	{
		if (folder == pane->GetFolder())
			outArray->Add (pane);
	}
}

MSG_IMAPFolderInfoMail*
MSG_Master::FindImapMailFolder(const char *hostName, const char* onlineServerName, const char *owner, XP_Bool createIfMissing)
{
	if (!hostName)
		return FindImapMailFolder(onlineServerName);

	MSG_IMAPHostTable *imapHostTable = GetIMAPHostTable();
	if (imapHostTable)
	{
		MSG_IMAPHost *host = imapHostTable->FindIMAPHost (hostName);

		if (host && host->GetHostFolderInfo())
			return  host->GetHostFolderInfo()->FindImapMailOnline(onlineServerName, owner, createIfMissing);
	}
	return NULL;
}

MSG_IMAPFolderInfoMail *
MSG_Master::FindImapMailFolderFromPath(const char *pathName)
{
	if (!m_imapHostTable) return NULL;

	for (int i = 0; i < m_imapHostTable->GetSize(); i++)
	{
		MSG_IMAPHost *host = (MSG_IMAPHost *) m_imapHostTable->GetAt(i);

		if (host && host->GetHostFolderInfo())
		{
			MSG_FolderInfo *folderInfo = host->GetHostFolderInfo()->FindMailPathname(pathName);
			if (folderInfo)
				return folderInfo->GetIMAPFolderInfoMail();
		}
	}
	return NULL;
}

// ### dmb - what about port????
MSG_FolderInfoContainer *MSG_Master::GetImapMailFolderTreeForHost(const char *hostName)
{
	if (!m_imapHostTable) return NULL;
	MSG_IMAPHost *host = m_imapHostTable->FindIMAPHost (hostName);

	if (host && host->GetHostFolderInfo())
		return  host->GetHostFolderInfo();
	return NULL;
}

MSG_IMAPHost *MSG_Master::GetIMAPHost(const char *hostName)
{
	return (m_imapHostTable) ? m_imapHostTable->FindIMAPHost (hostName) : 0;
}

int32 MSG_Master::GetSubscribingIMAPHosts(MSG_IMAPHost** result, int32 resultsize)
{
	if (result == NULL)
	{
		// just checking for size
		int32 numSubscribingIMAPHosts = 0;
		MSG_IMAPHostTable *imapHostTable = GetIMAPHostTable();
		if (imapHostTable)
		{
			int32 numIMAPHostsToFillIn = imapHostTable->GetHostList(NULL, 0);
			MSG_IMAPHost **imapHosts = new MSG_IMAPHost* [numIMAPHostsToFillIn];
			int32 numIMAPHosts = GetIMAPHostTable()->GetHostList(imapHosts, numIMAPHostsToFillIn);

			for (int i = 0; i < numIMAPHosts; i++)
			{
				if (imapHosts[i]->GetIsHostUsingSubscription())
					numSubscribingIMAPHosts++;
			}
			delete imapHosts;
		}
		return numSubscribingIMAPHosts;
	}
	else
	{
		// actually filling in the list
		int32 numIMAPHostsToFillIn = resultsize, numSubscribingIMAPHosts = 0;
		if (numIMAPHostsToFillIn > 0 && (GetIMAPHostTable() != NULL))
		{
			// get the IMAP Host list
			int32 numIMAPHosts = GetIMAPHostTable()->GetSize();
			MSG_IMAPHost **imapHosts = new MSG_IMAPHost* [numIMAPHosts];
			int32 numIMAPHostsListed = GetIMAPHostTable()->GetHostList(imapHosts, numIMAPHosts);

			// copy them from the IMAP host list to the main result list
			for (int i = 0; (i < numIMAPHostsToFillIn) && (i < numIMAPHosts); i++)
			{
				if (imapHosts[i]->GetIsHostUsingSubscription())
				{
					result[numSubscribingIMAPHosts] = imapHosts[i];
					numSubscribingIMAPHosts++;
				}
			}
			delete imapHosts;
		}

		return numSubscribingIMAPHosts;
	}
}

// This is being changed to include all IMAP hosts, regardless of whether or not we are
// showing only subscribed folders.
// If later we want to change it back, just remove the two "TRUE"'s in the if statements.
int32 MSG_Master::GetSubscribingHosts(MSG_Host** result, int32 resultsize)
{
	if (result == NULL)
	{
		// just checking for size
		int32 numNewsHosts = GetHostTable()->GetHostList(NULL, resultsize);

		int32 numSubscribingIMAPHosts = 0;
		MSG_IMAPHostTable *imapHostTable = GetIMAPHostTable();
		if (imapHostTable)
		{
			numSubscribingIMAPHosts = imapHostTable->GetSize();
		}
		int32 total = numNewsHosts + numSubscribingIMAPHosts;
		return total;
	}
	else
	{
		// actually filling in the list
		int32 numNewsHosts = GetHostTable()->GetHostList((MSG_NewsHost **)result,resultsize);
		int32 total = numNewsHosts;
		int32 numIMAPHostsToFillIn = resultsize - numNewsHosts;
		XP_ASSERT(numIMAPHostsToFillIn >= 0);
		if (numIMAPHostsToFillIn > 0 && GetIMAPHostTable() != NULL)
		{
			// get the IMAP Host list
			int32 numIMAPHosts = GetIMAPHostTable()->GetSize();
			MSG_Host **imapHosts = new MSG_Host* [numIMAPHosts];
			int32 numIMAPHostsListed = GetIMAPHostTable()->GetHostList((MSG_IMAPHost **)imapHosts, numIMAPHosts);
			int32 numSubscribingIMAPHosts = 0;

			// copy them from the IMAP host list to the main result list
			for (int i = 0; i < numIMAPHosts && numNewsHosts + numSubscribingIMAPHosts < resultsize; i++)
			{
				result[numNewsHosts + numSubscribingIMAPHosts] = imapHosts[i];
				numSubscribingIMAPHosts++;
			}

			total += numSubscribingIMAPHosts;
			delete imapHosts;
		}

		return total;
	}
}


MSG_IMAPFolderInfoMail*
MSG_Master::FindImapMailFolder(const char* onlineServerName)
{
	if (!m_imapfolders) return NULL;
	
	return m_imapfolders->FindImapMailOnline(onlineServerName, NULL, FALSE);
}

MSG_FolderInfoMail*
MSG_Master::FindMagicMailFolder(int flag, XP_Bool createIfMissing)
{
	MSG_FolderInfoMail	*retFolder = NULL;
	char *magicFolderName = m_prefs->MagicFolderName(flag);
	if (magicFolderName)
	{
		retFolder = FindMailFolder(magicFolderName, createIfMissing);
#ifdef XP_OS2
                if (retFolder)
                {
                   if (flag & MSG_FOLDER_FLAG_INBOX)
                       retFolder->SetName(INBOX_FOLDER_PRETTY_NAME);
                   if (flag & MSG_FOLDER_FLAG_TRASH)
                       retFolder->SetName(TRASH_FOLDER_PRETTY_NAME);
                   if (flag & MSG_FOLDER_FLAG_QUEUE)
                       retFolder->SetName(QUEUE_FOLDER_PRETTY_NAME);
                   if (flag & MSG_FOLDER_FLAG_DRAFTS)
                       retFolder->SetName(DRAFTS_FOLDER_PRETTY_NAME);
                   if (flag & MSG_FOLDER_FLAG_TEMPLATES)
                       retFolder->SetName(TEMPLATES_FOLDER_PRETTY_NAME);
                }
#endif

		XP_FREE(magicFolderName);
	}
	return retFolder;
}

MSG_FolderInfoMail*
MSG_Master::FindMailFolder(const char* pathURL, XP_Bool createIfMissing)
{
	if (!m_mailfolders)
		GetFolderTree();	// at least try to build the folder tree before failing...

	if (!m_mailfolders || !pathURL) return NULL;

	if (NET_URL_Type(pathURL) == IMAP_TYPE_URL)
	{
		return GetFolderInfo(pathURL, createIfMissing) ?
			GetFolderInfo(pathURL, createIfMissing)->GetMailFolderInfo():
			0 ;
	}

	char* folderPath = NET_ParseURL(pathURL, GET_PATH_PART);
	if (!*folderPath)
		StrAllocCat(folderPath, pathURL); // it was already a path?
	MSG_FolderInfo* result = m_mailfolders->FindPathname(folderPath);
	if (!result) 
	{
		if (createIfMissing) 
		{ 
			XP_StatStruct stat;

			/* removing orphaned summary file for nonexistent mail folder
			 * otherwise the creation of the missing mail folder will fail.
			 */
			if (0 == XP_Stat (folderPath, &stat, xpMailFolderSummary))
				XP_FileRemove(folderPath, xpMailFolderSummary);

			MSG_FolderInfoMail *mailRoot = GetLocalMailFolderTree();
			// try to strip off the folder from the pathname, since
			// that's what CreateSubfolder wants.
			const char *folderDir = GetPrefs()->GetFolderDirectory();
			const char *folderName = folderPath + strlen(folderDir);
			if (strncmp(folderDir, folderPath, strlen(folderDir)))
				goto Cleanup;
			if (folderName[0] == '/')
				folderName++;
			int32 newFolderPos = 0;
			if (mailRoot->CreateSubfolder(folderName, &result, &newFolderPos)!= eSUCCESS)
				result = NULL;
			else
			{
				MSG_FolderPane *folderPane = (MSG_FolderPane*) FindPaneOfType ((MSG_FolderInfo*)NULL, MSG_FOLDERPANE);
				if (folderPane)
				{
					MSG_FolderInfo *parent = m_folderTree->FindParentOf (result);
					if (parent)
						folderPane->AddFolderToView (result, parent, newFolderPos);
				}
			}
		}
	}
Cleanup:
	XP_FREEIF(folderPath);
	return (MSG_FolderInfoMail *) result;
}

// createIfMissing is currently only used for IMAPFolderInfoMail's.  Also note that this
// only creates the folder info, not the actual folder on the server.  This is used
// for automatically adding folders to the list before we've discovered them, as in
// a URL click on a link to a folder.
MSG_FolderInfo*		MSG_Master::GetFolderInfo(const char *url, XP_Bool createIfMissing)
{
	int urlType = NET_URL_Type(url);
	MSG_FolderInfo	*folder = NULL;
	char *folderName = NULL;
	char *host_and_port = NULL;
	char *owner = NULL;

	switch (urlType)
	{
	case NEWS_TYPE_URL:
	{
		folderName = NewsGroupDB::GetGroupNameFromURL(url);
		host_and_port = NET_ParseURL (url, GET_HOST_PART);

		folder = FindNewsFolder(host_and_port, folderName, *url == 's');
	}
		break;
	case MAILBOX_TYPE_URL:
	{
		folderName = NET_ParseURL(url, GET_PATH_PART);
		if (folderName && folderName[0] != '/')
		{
			char *saveFolderName = folderName;
			folderName = PR_smprintf("%s/%s", m_prefs->GetFolderDirectory(), saveFolderName);
			FREEIF(saveFolderName);
		}
		folder = FindMailFolder(folderName, FALSE);
		break;
	}
	case IMAP_TYPE_URL:
	{
		owner = NET_ParseURL (url, GET_USERNAME_PART);
		host_and_port = NET_ParseURL (url, GET_HOST_PART);
		folderName = NET_ParseURL(url, GET_PATH_PART);	// folderName will have leading '/'
		if (folderName && *folderName)
			folder = FindImapMailFolder(host_and_port, folderName + 1, owner, createIfMissing);
		else 
			folder = GetImapMailFolderTreeForHost(host_and_port);
	}
	default:
		break;
	}

	FREEIF(folderName);
	FREEIF(host_and_port);
	FREEIF(owner);
	return folder;
}


int	MSG_Master::GetMessageLineForURL(const char *url, MSG_MessageLine *msgLine)
{
	int		urlType = NET_URL_Type(url);
	MSG_FolderInfo	*folder = NULL;
	char	*folderName = NULL;
	char	*hostName = NULL;
	int		ret = -1;	// ### dmb what should error be?
	MessageKey key = MSG_MESSAGEKEYNONE;

	switch (urlType)
	{
	case NEWS_TYPE_URL:
	{
		// given a url of the form news://news/<message-id>, GetGroupNameFromURL
		// will return the <message-id>, so go with it.
		char *idPart = NewsGroupDB::GetGroupNameFromURL(url);
		if (!idPart)
			break;

		char *msgId = XP_STRDUP (NET_UnEscape (idPart));

		// need to iterate through open thread and message panes trying to
		// find this messageid.
		MSG_Pane *pane = FindFirstPaneOfType(MSG_MESSAGEPANE);
		while (pane != NULL)
		{
			if (MSG_GetKeyFromMessageId(pane, msgId, &key) == 0)
				break;
			pane = FindNextPaneOfType(pane->GetNextPane(), MSG_MESSAGEPANE);
		}
		if (pane == NULL)
		{
			pane = FindFirstPaneOfType(MSG_THREADPANE);
			while (pane != NULL)
			{
				if (MSG_GetKeyFromMessageId(pane, msgId, &key) == 0)
					break;
				pane = FindNextPaneOfType(pane->GetNextPane(), MSG_THREADPANE);
			}		
		}
		if (key != MSG_MESSAGEKEYNONE)
			ret = MSG_GetThreadLineById(pane, key, msgLine);

	}
		break;
	case MAILBOX_TYPE_URL:
	{
		folderName = NET_ParseURL(url, GET_PATH_PART);
		if (folderName && folderName[0] != '/')
		{
			char *saveFolderName = folderName;
			folderName = PR_smprintf("%s/%s", m_prefs->GetFolderDirectory(), saveFolderName);
			FREEIF(saveFolderName);
		}
		folder = FindMailFolder(folderName, FALSE);
		if (folder)
		{
			MSG_Pane *pane = FindPaneOfType(folder, MSG_ANYPANE);
			if (!pane)
				break;
			char *path = NET_ParseURL(url, GET_SEARCH_PART); // is this right?
			char *idPart = XP_STRSTR(path, "?id=");
			char *msgId = NULL;
			if (idPart)
			{
				char *amp = XP_STRCHR(idPart+1, '&');
				if (amp)
					*amp = '\0';	// get rid of next ? if any 
				msgId = XP_STRDUP (NET_UnEscape (idPart+4));

				ret = MSG_GetKeyFromMessageId (pane, msgId, &key);
				if (ret >= 0)
					ret = MSG_GetThreadLineById(pane, key, msgLine);
			}
			FREEIF(path);
			FREEIF(msgId);
		}
		break;
	}
	case IMAP_TYPE_URL:
		{
		}
	default:
		break;
	}
	FREEIF(folderName);
	FREEIF(hostName);
	return ret;
}


MSG_FolderInfoNews *MSG_Master::FindNewsFolder(const char *host_and_port,
											   const char *newsGroupName,
											   XP_Bool xxxP,
											   XP_Bool autoSubscribe /*= TRUE*/)
{
	const char *atSign = XP_STRCHR(host_and_port, '@');
	const char *hostName = atSign;

	// if Collabra has been disabled through the prefs/mission control
	if (!m_collabraEnabled)
		return NULL;

	// in case this is a x newsgroup and has username/password tagged along
	if (hostName)
		hostName++;
	else
		hostName = host_and_port;

	int32 port = 0;
	char* colon = XP_STRCHR(host_and_port, ':');
	if (colon) {
		*colon = '\0';
		port = XP_ATOI(colon + 1);
	}

	MSG_NewsHost *host = FindHost(hostName, xxxP, port);
	MSG_FolderInfoNews *info = NULL;

	if (!host && strlen(hostName) == 0)
		host = m_hosttable->GetDefaultHost(TRUE);
	if (!host && autoSubscribe)
		host = AddNewsHost(hostName, xxxP, port);
	if (host)
	{
		GetFolderTree();	// this has the side effect of loading the
							// news rc - it is great!
		info = host->FindGroup(newsGroupName);

		// maybe we should add the group but not subscribe to it - though this
		// will grow newsrc file
		if ((!info || !info->IsSubscribed()) && autoSubscribe && XP_STRLEN(newsGroupName) > 0) {
			// let us try autosubscribe...
			info = host->AddGroup(newsGroupName);
			if (info)
				info->SetAutoSubscribed(TRUE);
		}
		if (info && hostName != host_and_port) {
			// we have a pair of username and password here
			// parse it and save it along with the newsgroup
			const char *usercolon = XP_STRCHR(host_and_port, ':');
			if (usercolon) {
				char *username = (char*)
					XP_ALLOC(usercolon - host_and_port + 1);
				if (username) {
					XP_MEMSET(username, 0, usercolon - host_and_port + 1);
					XP_MEMCPY(username, host_and_port,
							  usercolon - host_and_port);
					char *munged = HG28265(username);
					info->SetNewsgroupUsername(munged);
					FREEIF(munged);
					HG24239
				}
				usercolon++;
				if (usercolon < atSign) {
					char *password = (char*) XP_ALLOC(atSign - usercolon + 1);
					if (password) {
						XP_MEMSET(password, 0, atSign - usercolon + 1);
						XP_MEMCPY(password, usercolon, atSign - usercolon);
						char *munged = HG23258(password);
						info->SetNewsgroupPassword(munged);
						FREEIF(munged);
						HG56307
					}
				}
			}
			// else we do not have a legitimate username/password
			// forget about the whole thing
		}
	//	if (info && !info->IsSubscribed())
	//		info->Subscribe(TRUE, this);
	}
	if (colon) *colon = ':';
	return info;
}



MSG_FolderInfoNews*
MSG_Master::FindNewsFolder(MSG_NewsHost* host,
						   const char *newsGroupName,
						   XP_Bool autoSubscribe /*= TRUE*/)
{
	MSG_FolderInfoNews *info = NULL;

	// if Collabra has been disabled through the prefs/mission control
	if (!m_collabraEnabled)
		return NULL;

	XP_ASSERT(host);
	if (!host) return NULL;
	GetFolderTree();	// this has the side effect of loading the
						// news rc - it is great!
	info = host->FindGroup(newsGroupName);
	// maybe we should add the group but not subscribe to it - though this
	// will grow newsrc file
	if (!info && autoSubscribe && XP_STRLEN(newsGroupName) > 0) {
		// let us try autosubscribe...
		info = host->AddGroup(newsGroupName);
	}
	return info;
}


MSG_FolderInfo *MSG_Master::FindNextFolder(MSG_FolderInfo *startFolder)
{
	MSG_FolderIterator folderIterator(GetFolderTree());
	MSG_FolderInfo		*curFolder;

	curFolder = folderIterator.AdvanceToFolder(startFolder);
	curFolder = folderIterator.Next();
	while (curFolder != NULL && (curFolder->GetType() == FOLDER_CONTAINERONLY || curFolder->GetType() == FOLDER_IMAPSERVERCONTAINER))
		curFolder = folderIterator.Next();
	return curFolder;
}

MSG_FolderInfo	*MSG_Master::FindNextFolderWithUnread(MSG_FolderInfo *startFolder)
{
	MSG_FolderIterator folderIterator(GetFolderTree());
	MSG_FolderInfo		*curFolder;
	MSG_FolderInfo      *retFolder = NULL;

	curFolder = folderIterator.AdvanceToFolder(startFolder);
	while (curFolder != NULL)
	{
		curFolder = folderIterator.Next();
		// ignore trash folder while looking for next folder with unread
		// This will still hit child folders of trash with unread.
		if (curFolder && (curFolder->GetNumUnread() + curFolder->GetNumPendingUnread()) > 0 && !(curFolder->GetFlags() & MSG_FOLDER_FLAG_TRASH))
		{
			retFolder = curFolder;
			break;
		}
	}

	return retFolder;
}

ParseMailboxState *
MSG_Master::GetParseMailboxState(const char *folderName)
{
	MSG_FolderInfoMail *mailFolder = FindMailFolder(folderName, FALSE);
	return (mailFolder) ? mailFolder->GetParseMailboxState() : 0;
}

ListNewsGroupState *
MSG_Master::GetListNewsGroupState(MSG_NewsHost* host,
								  const char *newsGroupName)
{
	MSG_FolderInfoNews *newsFolder = FindNewsFolder(host, newsGroupName);
	return (newsFolder) ? newsFolder->GetListNewsGroupState() : 0;
}

MSG_NewsHost*
MSG_Master::FindHost(const char* name, XP_Bool isxxx, int32 port)
{
	if (!name) return NULL;		// Can happen, especially if we do not have a
								// default newshost set.
	const char* atSign = XP_STRCHR(name, '@');
	if (atSign) {
		name = atSign + 1;		// In case there is a username/password tagged
								// along with this host.
	}
	char* dupname = NULL;
	MSG_NewsHost* result = NULL;
	if (port < 0) {
		port = 0;
		if (XP_STRCHR(name, ':')) {
			dupname = XP_STRDUP(name);
			if (!dupname) return NULL;
			char* ptr = XP_STRCHR(dupname, ':');
			XP_ASSERT(ptr);
			if (ptr) {
				*ptr++ = '\0';
				port = XP_ATOI(ptr);
			}
			name = dupname;
		}
	}
	if (port == 0) port = HG82266 NEWS_PORT;
	XP_ASSERT(XP_STRCHR(name, ':') == NULL);
	// m_hosttable can be NULL if we failed to build the hosts DB
	int n = m_hosttable ? m_hosttable->getNumHosts() : 0;
	for (int i=0 ; i<n ; i++) {
		MSG_NewsHost* host = m_hosttable->getHost(i);
		if (XP_STRCMP(host->getStr(), name) == 0 &&
			HG18262
		    host->getPort() == port) {
			result = host;
			break;
		}
	}
	FREEIF(dupname);
	return result;
}


MSG_NewsHost*
MSG_Master::GetDefaultNewsHost()
{
	if (m_hosttable) return m_hosttable->GetDefaultHost(TRUE);
	return NULL;
}


// The intent is to allow the user to type in a group name
// or a group url (including news server) and we would add the
// group to the newsrc file.
MSG_FolderInfoNews *MSG_Master::AddNewsGroup(const char *groupURL)
{
	char *hostName = NET_ParseURL (groupURL, GET_HOST_PART);
	char *groupName = NET_ParseURL(groupURL, GET_PATH_PART);
	char	*origGroupName = groupName;
	char	*origHostName = hostName;
	MSG_FolderInfoNews	*newsInfo = NULL;
	HG12765

	if (groupName[0] == '/')
		groupName++;
	else if (groupName[0] == '\0')
		groupName = (char *) groupURL;
	if (hostName[0] == '\0')
	{
		FREEIF(origHostName);
		PREF_CopyCharPref("network.hosts.nntp_server", &origHostName);
		if (origHostName && *origHostName == '\0') {
			XP_FREE(origHostName);
			origHostName = NULL;
		}
		if (!origHostName) return NULL;
		hostName = origHostName;
		int32 defport = 0;
		PREF_GetIntPref("news.server_port", &defport);
		if (defport && defport != (HG16236 NEWS_PORT)) {
			char* tmp = PR_smprintf("%s:%ld", origHostName, long(defport));
			XP_FREE(origHostName);
			origHostName = tmp;
			if (!origHostName) return NULL;
		}
		hostName = origHostName;
	}

	MSG_NewsHost *newsHost = FindHost(hostName, *groupURL == 's', -1);
	if (newsHost)
		newsInfo = newsHost->AddGroup(groupName);

	if (newsInfo)	// if group already exists, AddGroup will return NULL.
	{
		XP_File newsrcFile = XP_FileOpen(hostName,
										 HG16215 xpNewsRC,
										 XP_FILE_APPEND_BIN);
		if (newsrcFile != NULL)
		{
			XP_FileWrite(groupName, strlen(groupName), newsrcFile);
			XP_FileWrite(":\n", 2, newsrcFile);
			XP_FileClose(newsrcFile);	
		}
	}
	FREEIF(origHostName);
	FREEIF(origGroupName);

	return newsInfo;
}

MSG_FolderInfoNews *MSG_Master::AddProfileNewsgroup (MSG_NewsHost* host,
													 const char *groupName)
{
	char *groupUrl = PR_smprintf ("%s/%s", host->GetURLBase(), groupName);
	if (groupUrl)
	{
		MSG_FolderInfoNews *newsFolder = AddNewsGroup (groupUrl);
		host->SetIsProfile (groupName, TRUE);
		XP_FREE(groupUrl);

		if (newsFolder)
		{
			newsFolder->SetFlag (MSG_FOLDER_FLAG_PROFILE_GROUP);
			BroadcastFolderChanged (newsFolder);
		}

		return newsFolder;
	}
	return NULL;
}

XP_Bool MSG_Master::AcquireRulesSemaphore (void *semRequester)
{
	if (m_rulesSemaphoreHolder)
		return FALSE;

	m_rulesSemaphoreHolder = semRequester;
	return TRUE;
}

XP_Bool MSG_Master::ReleaseRulesSemaphore (void *semHolder)
{
	if (m_rulesSemaphoreHolder == semHolder)
	{
		m_rulesSemaphoreHolder = NULL;
		return TRUE;
	}
	return FALSE;
}

XP_Bool MSG_Master::AcquireEditHeadersSemaphore (void * semRequester)
{
	if (m_editHeadersSemaphoreHolder)
		return FALSE;
	m_editHeadersSemaphoreHolder = semRequester;
	return TRUE;
}

XP_Bool MSG_Master::ReleaseEditHeadersSemaphore (void *semHolder)
{
	if (m_editHeadersSemaphoreHolder == semHolder)
	{
		m_editHeadersSemaphoreHolder = NULL;
		return TRUE;
	}
	return FALSE;
}


void MSG_Master::PostCreateImapFolderUrlExitFunc (URL_Struct *URL_s, int status, MWContext * /* window_id */)
{
	// Not sure why this happens (pane is deleted), but it's a guaranteed crash.
	if (!MSG_Pane::PaneInMasterList(URL_s->msg_pane))
		URL_s->msg_pane = NULL;
	if (URL_s->msg_pane && (0 == status))
	{
		MSG_Master *master = URL_s->msg_pane->GetMaster();
		char *host_and_port = NET_ParseURL (URL_s->address, GET_HOST_PART);
		
		// find the folder we created
		char *folderName = IMAP_CreateOnlineSourceFolderNameFromUrl(URL_s->address);
		MSG_IMAPFolderInfoMail *newImapFolder = master->FindImapMailFolder(host_and_port, folderName, NULL, FALSE);
		FREEIF(host_and_port);
		
		if (MSG_FOLDERPANE == URL_s->msg_pane->GetPaneType())
			((MSG_FolderPane*) URL_s->msg_pane)->PostProcessRemoteCopyAction();

		master->BroadcastFolderAdded (newImapFolder, URL_s->msg_pane);

		FREEIF(folderName);
		
	}
	else if (URL_s->msg_pane)	// this may be wrong - the url could have been interrupted after the folder was created.
	{
		MSG_Master *master = URL_s->msg_pane->GetMaster();
		for (MSG_Pane *pane = master->GetFirstPane(); pane; pane = pane->GetNextPane())
		{
			MSG_PaneType type = pane->GetPaneType();
			if (type == MSG_PANE || type == MSG_FOLDERPANE)
				FE_PaneChanged (pane, FALSE, MSG_PaneNotifyNewFolderFailed, 0);
		}
	}
}


// This function is in the Master because we want to be able to create mail
// folders without having any panes, and certainly without a folder pane.
//
MsgERR MSG_Master::CreateMailFolder (MSG_Pane *invokingPane, MSG_FolderInfo *parent, const char *childName) 
{
	MsgERR status = 0;
	MSG_FolderPane *folderPane = (MSG_FolderPane*) FindFirstPaneOfType (MSG_FOLDERPANE);

	XP_Bool folderIsIMAPContainer = parent && (parent->GetType() == FOLDER_IMAPSERVERCONTAINER);

	if (parent && !folderIsIMAPContainer && (parent->GetDepth() == 1)) {
		// trying to create a folder in the root
		parent = GetLocalMailFolderTree();
	}


	if (parent && parent->ContainsChildNamed(childName))
	{
		status = MK_MSG_FOLDER_ALREADY_EXISTS;
		if (invokingPane)
		{
			FE_Alert(invokingPane->GetContext(),XP_GetString(status));
			FE_PaneChanged (folderPane, FALSE, MSG_PaneNotifyNewFolderFailed, status);	// let the FE know creation failed,
		}
		return status;
	}

	if ((parent->GetType() == FOLDER_IMAPMAIL) || folderIsIMAPContainer)
	{

		// this code will only allow creation of subfolders for existing
		// online folders.
		
		const char *parentOnlineName;
		const char *hostName;
		MSG_IMAPHost *host = NULL;
		char newFolderOnlineHierarchySeparator = kOnlineHierarchySeparatorUnknown;
		if (folderIsIMAPContainer)
		{
			// creating a new IMAP folder, and the parent is the IMAP container.
			// This means that we might actually need to use the personal namespace
			// as the parent folder, since we will sometimes strip it off.
			hostName = ((MSG_IMAPFolderInfoContainer *) parent)->GetName();
			host = ((MSG_IMAPFolderInfoContainer *) parent)->GetIMAPHost();
			parentOnlineName = host->GetRootNamespacePrefix();
		}
		else
		{				
			parentOnlineName = 	((MSG_IMAPFolderInfoMail *) parent)->GetOnlineName();
			host = ((MSG_IMAPFolderInfoMail *) parent)->GetIMAPHost();
			newFolderOnlineHierarchySeparator = ((MSG_IMAPFolderInfoMail *) parent)->GetOnlineHierarchySeparator();
		}
						
		char *newFolderOnlineName = (char *) XP_ALLOC(XP_STRLEN(childName) + XP_STRLEN(parentOnlineName) + 2); // 2 == '/' and '\0'
		XP_STRCPY(newFolderOnlineName, parentOnlineName);

		const char *namespacePrefix = host->GetNamespacePrefixForFolder(parentOnlineName);
		XP_Bool parentIsNamespace = FALSE;
		if (namespacePrefix &&
			!XP_STRCMP(namespacePrefix, parentOnlineName))
		{
			parentIsNamespace = TRUE;	// the folder we are discovering is actually a namespace prefix.
		}

		// If the parent is a namespace itself, we shouldn't append any delimiters between the namespace
		// and the top-level folder
		if (!parentIsNamespace)
		{
			if (*newFolderOnlineName && *(newFolderOnlineName + XP_STRLEN(newFolderOnlineName) - 1) != '/')
				XP_STRCAT(newFolderOnlineName, "/");
		}
		XP_STRCAT(newFolderOnlineName, childName);
		
		if (NET_IsOffline())
		{
			MSG_IMAPFolderInfoMail *newFolder = MSG_IMAPFolderInfoMail::CreateNewIMAPFolder(parent->GetIMAPFolderInfoMail(), invokingPane->GetContext(), 
					newFolderOnlineName, childName, kPersonalMailbox, host);
			if (newFolder)
				newFolder->SetFolderPrefFlags(MSG_FOLDER_PREF_OFFLINEEVENTS | MSG_FOLDER_PREF_CREATED_OFFLINE | newFolder->GetFolderPrefFlags());

			// tell FE about new folder, but don't make it select it - I hate that.
			FE_PaneChanged (invokingPane, FALSE, MSG_PaneNotifySelectNewFolder, MSG_VIEWINDEXNONE);
			// what about name validation? What about other folder types besides kPersonalMailbox?
			return 0;
		}

		char *createMailboxURL = CreateImapMailboxCreateUrl(host->GetHostName(), newFolderOnlineName,newFolderOnlineHierarchySeparator);
		if (createMailboxURL)
		{
			URL_Struct *url_struct = NET_CreateURLStruct(createMailboxURL, NET_SUPER_RELOAD);
			if (url_struct && (invokingPane || folderPane))
			{
				if (invokingPane)
				{
					MSG_UrlQueue::AddUrlToPane(url_struct, MSG_Master::PostCreateImapFolderUrlExitFunc, invokingPane);
				}
				else
				{
					// should not get here, but just for backward compatibility until XFE and MacFE call the right glue function
					if (folderPane)
						MSG_UrlQueue::AddUrlToPane(url_struct, MSG_Master::PostCreateImapFolderUrlExitFunc, folderPane);
				}
			}
			
			XP_FREE(createMailboxURL);
		}
		FREEIF(newFolderOnlineName);
		
	}
	else
	{
		MSG_FolderInfo *newFolder = NULL;
		int32 newFolderPos = 0;
		status = parent->CreateSubfolder (childName, &newFolder, &newFolderPos);
		if (status == 0)
		{
			BroadcastFolderAdded (newFolder, invokingPane);
		}
		else {  // status != 0;  folder creation failed
			if (folderPane) {
				FE_Alert (folderPane->GetContext(), XP_GetString (status));	// put up the message to the user
				FE_PaneChanged (folderPane, FALSE, MSG_PaneNotifyNewFolderFailed, status);	// let the FE know creation failed,
			}
																						// in case it wants to do anything
		}	
	}

	return status;
}

XP_Bool MSG_Master::CleanupNeeded()
{
	MSG_FolderIterator  folderIterator(GetFolderTree());
	MSG_FolderInfo		*curFolder;

	curFolder = folderIterator.First();

	while(curFolder)
	{
		if (curFolder->RequiresCleanup())
			return TRUE;
		curFolder = folderIterator.Next();
	}
	return FALSE;
}

void MSG_Master::CleanupFolders(MSG_Pane *pane)
{
	MSG_FolderIterator  folderIterator(GetFolderTree());
	MSG_FolderInfo		*curFolder;
	XPPtrArray			foldersToCleanup;

	curFolder = folderIterator.First();

	// build up list of folders that need compaction
	while(curFolder)
	{
		if (curFolder->RequiresCleanup())
			foldersToCleanup.Add(curFolder);
		curFolder = folderIterator.Next();
	}
	if (foldersToCleanup.GetSize() > 0)
	{
		MSG_BackgroundCleanup *bgCleanupHandler = new MSG_BackgroundCleanup(foldersToCleanup);
		if (bgCleanupHandler)
			bgCleanupHandler->Begin(pane);
	}
}

int MSG_Master::DownloadForOffline(MSG_Pane *pane, MSG_FolderInfo *folder /*=NULL*/)
{
	int returnValue = 0;
	if (folder && !folder->IsNews())
	{
		FE_AllConnectionsComplete(pane->GetContext());	
		return -1;	
	}

	MSG_DownloadOfflineFoldersState *downloadOfflineFoldersState = new MSG_DownloadOfflineFoldersState(pane);
	if (downloadOfflineFoldersState)
	{
		if (folder)
			returnValue = downloadOfflineFoldersState->DownloadOneFolder(folder);
		else
			returnValue = downloadOfflineFoldersState->DoIt();
	}
	
	return returnValue;
}

class MSG_GoOfflineState : public MSG_PaneURLChain
{
public:
	MSG_GoOfflineState(MSG_Pane *pane, XP_Bool goOffline);
	virtual int GetNextURL();
	XP_Bool	m_bReplicateDirectories;
	XP_Bool	m_bDownloadDiscussions;
	XP_Bool m_bGetNewMail;
	XP_Bool m_bSendOutbox;
	XP_Bool m_bOnline;
	XP_Bool m_bDownloadRet;
	XP_Bool m_bLaunchedURL;
	XP_Bool m_bGoOffline;
};

MSG_GoOfflineState::MSG_GoOfflineState(MSG_Pane *pane, XP_Bool goOffline) : MSG_PaneURLChain(pane)
{
	m_bDownloadDiscussions = FALSE;
	m_bGetNewMail = FALSE;
	m_bSendOutbox = FALSE;
	m_bDownloadRet = 0;
	m_bLaunchedURL = FALSE;
	m_bGoOffline = goOffline;
	PREF_GetBoolPref("network.online", &m_bOnline);
	if (!m_bOnline)
		PREF_SetBoolPref("network.online", TRUE);
}

int MSG_GoOfflineState::GetNextURL()
{
	int ret = 0;
	if (m_bDownloadDiscussions)
	{
		m_bDownloadDiscussions = FALSE;
		m_bDownloadRet = m_pane->GetMaster()->DownloadForOffline(m_pane);
		m_bLaunchedURL = (m_bLaunchedURL) ? TRUE : (ret >= 0);
		if (m_bLaunchedURL)
			return 0;
	}
	if (m_bSendOutbox)
	{
		m_bSendOutbox = FALSE;
		m_bLaunchedURL = TRUE;
		m_pane->DeliverQueuedMessages();
		return 0;
	}
	if (m_bGetNewMail)
	{
		m_bGetNewMail = FALSE;
		m_bLaunchedURL = TRUE;
		if (m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
		{
			m_pane->GetMaster()->ImapGoOnline(m_pane);	// uses MSG_UrlQueue chaining
		}
		else
			return m_pane->GetNewMail(m_pane, NULL);
	}
	if (m_bReplicateDirectories)
	{
		m_bReplicateDirectories = FALSE;
		if (NET_ReplicateDirectory(m_pane, NULL))
			m_bLaunchedURL = TRUE;
	}
	else
	{
		if (m_bOnline && m_bGoOffline)
		{
			// this will need to terminate all imap connections, not just the cached one,
			m_pane->GetMaster()->CloseCachedImapConnections();
			PREF_SetBoolPref("network.online", FALSE);
		}
		if (!m_bLaunchedURL)
			FE_PaneChanged(m_pane, TRUE, MSG_PaneProgressDone, 0);
		else
			m_pane->ClearURLChain();	// will delete this, so don't do anything after this with this!!!
	}
	return 0;
}

int	MSG_Master::SynchronizeOffline(MSG_Pane *pane, XP_Bool bDownloadDiscussions, XP_Bool bGetNewMail, XP_Bool bSendOutbox, XP_Bool bReplicateDirectories, XP_Bool goOffline)
{
	MSG_GoOfflineState *queue = new MSG_GoOfflineState (pane, goOffline);
	if (queue)
	{
		queue->m_bReplicateDirectories = bReplicateDirectories;
		queue->m_bDownloadDiscussions = bDownloadDiscussions;
		queue->m_bGetNewMail = bGetNewMail;
		queue->m_bSendOutbox = bSendOutbox;
		pane->SetURLChain(queue);
		queue->GetNextURL();
	}
	return 0;
}

MSG_PurgeInfo	*MSG_Master::GetPurgeHdrInfo()
{
	return m_purgeHdrPref;
}

MSG_PurgeInfo	*MSG_Master::GetPurgeArtInfo()
{
	return m_purgeArtPref;
}

void MSG_Master::ImapGoOnline(MSG_Pane *paneForUrls)
{
	if (GetPrefs()->GetMailServerIsIMAP4())
	{
		XP_ASSERT(paneForUrls);
		
		OfflineImapGoOnlineState *goState = new OfflineImapGoOnlineState(paneForUrls);
		if (goState)
			goState->ProcessNextOperation();
	}
}

XP_Bool		MSG_Master::IsCachePasswordProtected()
{
	return m_passwordProtectCache;
}

XP_Bool
MSG_Master::IsHTMLOK(MSG_FolderInfo* info)
{
	MSG_FolderInfoNews* group = info->GetNewsFolderInfo();
	XP_ASSERT(group && group->GetHost() && group->GetNewsgroupName());
	if (!group || !group->GetHost() || !group->GetNewsgroupName()) return -1;
	return group->GetHost()->IsHTMLOk(group->GetNewsgroupName());
}

int
MSG_Master::SetIsHTMLOK(MSG_FolderInfo* info, MWContext* context,
						XP_Bool value)
{
	MSG_FolderInfoNews* group = info->GetNewsFolderInfo();
	XP_ASSERT(group && group->GetHost() && group->GetNewsgroupName());
	if (!group || !group->GetHost() || !group->GetNewsgroupName()) return -1;
	MSG_NewsHost* host = group->GetHost();
	const char* name = group->GetNewsgroupName();
	if (value == host->IsHTMLOk(name)) return 0;
	if (value) {
		return host->SetIsHTMLOKGroup(name, value);
	}
	char* tmp = XP_STRDUP(name);
	if (!tmp) return MK_OUT_OF_MEMORY;
	char* ancestor = NULL;
	while (*tmp) {
		if (host->IsHTMLOKTree(tmp)) {
			FREEIF(ancestor);
			ancestor = XP_STRDUP(tmp);
			if (!ancestor) return MK_OUT_OF_MEMORY;
		}
		char* ptr = XP_STRRCHR(tmp, '.');
		if (ptr) *ptr = '\0';
		else break;
	}
	XP_FREE(tmp);
	tmp = NULL;
	if (!ancestor) {
		return host->SetIsHTMLOKGroup(name, value);
	}
	XP_ASSERT(!value);			// If value is TRUE, and we found an ancestor,
								// then host->IsHTMLOk() should have returned
								// TRUE as well.
	if (value) return -1;
	tmp =
		PR_smprintf(XP_GetString(MK_MSG_SET_HTML_NEWSGROUP_HEIRARCHY_CONFIRM),
					name, ancestor, ancestor);
	XP_FREE(ancestor);
	ancestor = NULL;
	if (!tmp) return MK_OUT_OF_MEMORY;
	XP_Bool doit = FE_Confirm(context, tmp);
	XP_FREE(tmp);
	tmp = NULL;
	if (!doit) return -1;		// ### Need a better return value.
	tmp = XP_STRDUP(name);
	if (!tmp) return MK_OUT_OF_MEMORY;
	while (*tmp) {
		host->SetIsHTMLOKTree(tmp, value);
		char* ptr = XP_STRRCHR(tmp, '.');
		if (ptr) *ptr = '\0';
		else break;
	}
	return host->SetIsHTMLOKGroup(name, value);	
}

void MSG_Master::ClearFolderGotNewBit()
{
	MSG_FolderIterator  folderIterator(GetFolderTree());
	MSG_FolderInfo *curFolder = folderIterator.First();

	// clear new bit on all folders - should stop at mail...
	while(curFolder)
	{
		curFolder->ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
		curFolder = folderIterator.Next();
	}
}

void MSG_Master::SetMailAccountURL(const char* urlString)
{
	if (!urlString)
	{
		FREEIF(m_mailAccountUrl);
	}
	else 
	{
		if (urlString && XP_STRLEN(urlString))
			StrAllocCopy(m_mailAccountUrl, urlString);
	}
}

XP_Bool MSG_Master::InitFolderFromCache (MSG_FolderInfo *folder)
{
	if (m_folderCache)
		return m_folderCache->InitializeFolder (folder);
	return FALSE;
}

// Returns TRUE if we are running the auto-upgrade process now, FALSE otherwise
XP_Bool MSG_Master::TryUpgradeToIMAPSubscription(MWContext *context, XP_Bool forceBringUpSubscribeUI,
												 MSG_Pane *paneToAssociate, MSG_IMAPHost *hostBeingOpened)
{
	if (forceBringUpSubscribeUI ||
		(!NET_IsOffline() && ShouldUpgradeToIMAPSubscription()))
	{
		XP_Bool usingIMAP = FALSE;

		char *oldHostName = NULL;
		MSG_IMAPHost *upgradeHost = NULL;
		PREF_CopyCharPref("network.hosts.pop_server", &oldHostName);
		if (oldHostName)
		{
			upgradeHost = GetIMAPHost(oldHostName);
			usingIMAP = (upgradeHost != NULL);
		}

		if (paneToAssociate->GetPaneType() == MSG_THREADPANE)
			m_imapUpgradeBeginPane = (MSG_ThreadPane *)paneToAssociate;

		if (hostBeingOpened && (upgradeHost != hostBeingOpened))
		{
			// This case should be extremely rare, and there is no really good way to handle it.
			// But we will try our best...
			// It occurs when the user has added a second IMAP server before running
			// the upgrade, and has made that second server their default host.
			// If that's the case, we should run the upgrade on the host for
			// the folder being opened (hostBeingOpened), but ONLY IF the
			// host's using_subscription bit is set to false.  If it is set to
			// true, then we should mark the upgrade as done.
			upgradeHost = hostBeingOpened;
			XP_Bool alreadyUsingSubscription = FALSE;
			char *prefString = PR_smprintf("mail.imap.server.%s.using_subscription", hostBeingOpened->GetHostName());
			if (prefString)
			{
				PREF_GetBoolPref(prefString, &alreadyUsingSubscription);
				XP_FREE(prefString);
			}
			if (alreadyUsingSubscription)
			{
				m_prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG);
				m_imapUpgradeBeginPane = 0;
				return FALSE;
			}
		}
		
		if (usingIMAP)
		{
			XP_Bool leaveSubscriptionAlone = FALSE;
			PREF_GetBoolPref("mail.imap.upgrade.leave_subscriptions_alone", &leaveSubscriptionAlone);
			if (leaveSubscriptionAlone && !forceBringUpSubscribeUI)
			{
				// set the upgraded flag, and don't turn on using_subscription
				m_prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG);	
				m_imapUpgradeBeginPane = 0;
				return FALSE;
			}
			else
			{
				// ok, it's time to upgrade
				XP_Bool subscribeToAll = FALSE;
				MSG_IMAPUpgradeType upgradePath = MSG_IMAPUpgradeAutomatic;	
				PREF_GetBoolPref("mail.imap.upgrade.auto_subscribe_to_all", &subscribeToAll);
				if (subscribeToAll && !forceBringUpSubscribeUI)
				{
					FE_Alert(context, XP_GetString(MK_IMAP_UPGRADE_WAIT_WHILE_UPGRADE));
				}
				else if (!forceBringUpSubscribeUI)
				{
#ifdef FE_IMPLEMENTS_IMAP_SUBSCRIBE_UPGRADE
					upgradePath = FE_PromptIMAPSubscriptionUpgrade (context, oldHostName);
					if (upgradePath == MSG_IMAPUpgradeDont)
					{
						m_imapUpgradeBeginPane = 0;
						return TRUE;	// don't let them continue
					}
#else	// FE_IMPLEMENTS_IMAP_SUBSCRIBE_UPGRADE
					XP_Bool upgradePathBool = FE_Confirm(context, XP_GetString(MK_IMAP_UPGRADE_PROMPT_QUESTION));
					if (upgradePathBool)
						upgradePath = MSG_IMAPUpgradeAutomatic;
					else
						upgradePath = MSG_IMAPUpgradeCustom;
#endif
				}
				else if (forceBringUpSubscribeUI)
				{
					// forceBringUpSubscribeUI
					upgradePath = MSG_IMAPUpgradeCustom;
				}

				if (upgradePath == MSG_IMAPUpgradeCustom)
				{
					// the user said "Manual", so bring up the subscribe UI.
					//if (!forceBringUpSubscribeUI)	// don't alert them if we're forcing it - we already alterted them
					//	FE_Alert(context, XP_GetString(MK_IMAP_UPGRADE_CUSTOM));
					// Set some flags so we know the callback should write out 
					// the upgrade state if successful.
					SetUpgradingToIMAPSubscription(TRUE);	// It is the subscribe pane's responsibility
													// to clear this flag;  it is not critical if it
													// is not immediately cleared, however, since its only
													// purpose is to instruct the subscribe pane to write
													// out the fact that we've upgraded.
					FE_CreateSubscribePaneOnHost(this, context, upgradeHost);
					// return TRUE, since we want to stop the current folder load URL
					return TRUE;
				}
				else
				{
					// either "subscribe to all" or "automatic," so switch to the new URL
					char *upgradeUrl = CreateIMAPUpgradeToSubscriptionURL(oldHostName, subscribeToAll);
					URL_Struct *URL_s = NET_CreateURLStruct(upgradeUrl, NET_DONT_RELOAD);
					if(!URL_s)	// out of memory?
					{
						FE_Alert(context, XP_GetString(MK_OUT_OF_MEMORY));
						m_imapUpgradeBeginPane = 0;
						return FALSE;
					}
					URL_s->internal_url = TRUE;
					/*pw_ptr */ m_progressWindow = PW_Create(context, pwApplicationModal);
					if (m_progressWindow)
					{
						MWContext *progressContext = PW_CreateProgressContext();
						if (progressContext)
						{
							PW_SetCancelCallback(m_progressWindow, CancelIMAPProgressCallback, progressContext);
							progressContext->mailMaster = this;
							PW_AssociateWindowWithContext(progressContext, m_progressWindow);
							PW_SetWindowTitle(m_progressWindow, "IMAP Folder Upgrade");
							PW_SetLine1(m_progressWindow, "Upgrading to IMAP Subscription...");
							PW_SetLine2(m_progressWindow, NULL);
							//PW_Show(m_progressWindow);
							paneToAssociate->AdoptProgressContext(progressContext);
							m_upgradingToIMAPSubscription = TRUE;
							NET_GetURL(URL_s, FO_PRESENT, progressContext, MSG_Master::PostImapSubscriptionUpgradeExitFunc);
						}
						else
						{
							FE_Alert(context, XP_GetString(MK_OUT_OF_MEMORY));
							m_imapUpgradeBeginPane = 0;
							return FALSE;
						}
					}
					else
					{
						FE_Alert(context, XP_GetString(MK_OUT_OF_MEMORY));
						m_imapUpgradeBeginPane = 0;
						return FALSE;
					}
					return TRUE;
				}

			}
		}
		else
		{
			// Not Using IMAP - set the pref flag, and don't try again
			m_prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG);
			m_imapUpgradeBeginPane = 0;
			return FALSE;
		}
	}
	else
	{
		// Offline, or shouldn't upgrade
		return FALSE;
	}
}

void MSG_Master::PostIMAPUpgradeFolderLoad(int status)
{
	if (m_imapUpgradeBeginPane)
	{
		m_imapUpgradeBeginPane->FinishLoadingIMAPUpgradeFolder(status != MK_INTERRUPTED);
		m_imapUpgradeBeginPane = 0;
	}
}

XP_Bool MSG_Master::ShouldUpgradeToIMAPSubscription()
{
	int32 profileAge = m_prefs->GetStartingMailNewsProfileAge();
	if (profileAge & MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG)	// we've upgraded to subscription already
		return FALSE;
	else
		return TRUE;
}

void MSG_Master::SetIMAPSubscriptionUpgradedPrefs()
{
	char *oldHostName = NULL;
	PREF_CopyCharPref("network.hosts.pop_server", &oldHostName);
	if (oldHostName)
	{
		MSG_IMAPHost *upgradedHost = GetIMAPHost(oldHostName);
		XP_ASSERT(upgradedHost);
		if (upgradedHost)
		{
			upgradedHost->SetIsHostUsingSubscription(TRUE);
		}
		XP_FREE(oldHostName);
	}

	m_prefs->SetMailNewsProfileAgeFlag(MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG);
}


XP_Bool MSG_Master::ShowIMAPProgressWindow()
{
	if (m_progressWindow)
	{
		PW_Show(m_progressWindow);
		PW_SetProgressRange(m_progressWindow, 1, 0);
		return TRUE;
	}
	else
		return FALSE;
}

/* static */ void MSG_Master::PostImapSubscriptionUpgradeExitFunc (URL_Struct *, int status, MWContext *context)
{
	MSG_Master *master = context->mailMaster;
	if (context->type == MWContextProgressModule)
		PW_AssociateWindowWithContext(context, NULL);

	XP_ASSERT(master);
	if (master)
	{
		if (master->m_progressWindow)
		{
			PW_Hide(master->m_progressWindow);
			PW_Destroy(master->m_progressWindow);
			master->m_progressWindow = NULL;
		}
		master->m_upgradingToIMAPSubscription = FALSE;
		master->PostIMAPUpgradeFolderLoad(status);
	}
}


/* static */ void MSG_Master::CancelIMAPProgressCallback(void *closure)
{
	MWContext *context = (MWContext *)closure;
	if (context)
	{
		XP_InterruptContext(context);
	}
}

// Returns a newly-allocated space-delimited list of the arbitrary headers needed to be downloaded for filters
// for a given host, when using IMAP.  Returns NULL if there are none, or the host could not be found.
// This list should not include standard filter headers, such as "To" or "CC"
char *MSG_Master::GetArbitraryHeadersForHostFromFilters(const char* /*hostName*/)
{
	// eventually we'll want to provide just the ones used in filters...
	// short term fix for now to test the system where we return a space delimited list of 
	// all registered custom headers from the prefs. Return string is null terminated.
	
	int numHeaders = 0;
	int headerBufSize = 0;

	MSG_Prefs * prefs =  GetPrefs();
	if (prefs)
	{
		numHeaders = prefs->GetNumCustomHeaders();
		if (numHeaders > 0)
		{
			char ** arrayOfHeaders = (char **) XP_ALLOC(sizeof(char *) * numHeaders);
			if (arrayOfHeaders)
			{
				for (int index = 0; index < numHeaders; index++)
				{
					arrayOfHeaders[index] = prefs->GetNthCustomHeader(index);
					if (arrayOfHeaders[index])
						headerBufSize += XP_STRLEN(arrayOfHeaders[index])+1; // + 1 for space delimter or null terminator
				}

				char * headerBuffer = (char *) XP_ALLOC(sizeof(char) * headerBufSize);
				if (headerBuffer)
				{
					XP_STRCPY(headerBuffer, "");
					int i = 0;
					for (i = 0; i < numHeaders; i++)
						if (arrayOfHeaders[i])
						{
							XP_STRCAT(headerBuffer, arrayOfHeaders[i]);
							XP_FREE(arrayOfHeaders[i]);    // free memory for the arbitrary header string

							// However, on this day NJ wins...
							if (i < numHeaders-1)  // only add the space if it is the last character...
								XP_STRCAT(headerBuffer, " ");  // space delimited
						}
				}

				XP_FREE(arrayOfHeaders);
				return headerBuffer;
			}
		}	// NJ > TX
	}  // TX >>> nj

	return NULL;
}


// Returns a newly-allocated space-delimited list of the arbitrary headers needed to be downloaded for MDN
// for a given host, when using IMAP.  Returns NULL if there are none, or the host could not be found.
// For instance, if MDN is not enabled, it should return NULL.
// An example of something it might return would look like: "From To CC Resent-From"
char *MSG_Master::GetArbitraryHeadersForHostFromMDN(const char* /*hostName*/)
{
	XP_Bool bMdnEnabled = FALSE;
	int32 intPref = 0;
	char *hdrString = NULL;

	PREF_GetIntPref("mail.incorporate.return_receipt", &intPref);
	if (intPref == 1)
		StrAllocCopy(hdrString, "Content-Type");

	PREF_GetBoolPref("mail.mdn.report.enabled", &bMdnEnabled);
	if (bMdnEnabled)
		if (hdrString)
			StrAllocCat(hdrString, " Return-Receipt-To Disposition-Notification-To");
		else
			StrAllocCopy(hdrString, "Return-Receipt-To Disposition-Notification-To");

	return hdrString;
}

int MSG_Master::PromptForHostPassword(MWContext *context, MSG_FolderInfo *folder)
// prompt for the password and match against saved password until user gets it
// right or cancels. Returns 0 if passed, non-zero for failure.
// For local folders, we want to use the default imap host password if there's an imap host,
// otherwise, we want to use the pop password, which we'll store in the local Inbox summary file.
// This routine also needs to set the appropriate internal state so the user won't be challenged
// again for the password when they make a real network connection.
{
	const char *userName = folder->GetUserName();

	int ret = -1;
	const int kNumPasswordTries = 3;

	char *userNameDup = NULL;

	if (userName)
		userNameDup = XP_STRDUP(userName);
	else
		return MK_POP3_USERNAME_UNDEFINED;

	const char	*host = folder->GetHostName();
	char *alert;

	const char *savedPassword = folder->GetRememberedPassword();

	// well, if we don't have a remembered password, just let them in.
	if (!savedPassword || !XP_STRLEN(savedPassword))
	{
		FREEIF(userNameDup);
		return 0;
	}

	const char *fmt1 = XP_GetString( XP_PASSWORD_FOR_POP3_USER );

	if (fmt1)	/* We need to read a password. */
	{

		char *password;
		size_t len = (XP_STRLEN(fmt1) + (host ? XP_STRLEN(host) : 0) + 300) * sizeof(char);
		char *prompt = (char *) XP_ALLOC (len);
		if (!prompt) 
			return MK_OUT_OF_MEMORY;

		PR_snprintf (prompt, len, fmt1, userNameDup, host);

		FREEIF(userNameDup);
		char *unmungedPassword = NULL;
		if (savedPassword)
			unmungedPassword = HG32686(savedPassword);

		if (unmungedPassword == NULL || !XP_STRLEN(unmungedPassword))
			return MK_POP3_PASSWORD_UNDEFINED;

		int i;
		for (i = 0; i < kNumPasswordTries; i++)
		{
			password = FE_PromptPassword(context, prompt);

			if (!password || !XP_STRLEN(password))
			{
				ret = -1;
				break;
			}
			if (!XP_STRCMP(password, unmungedPassword))
			{
				ret = 0;
				if (GetPrefs()->GetMailServerIsIMAP4())
				{
					MSG_IMAPHost *defaultIMAPHost = GetIMAPHostTable()->GetDefaultHost();
					if (defaultIMAPHost)
						IMAP_SetPasswordForHost(defaultIMAPHost->GetHostName(), password);
				}
				else
				{
					HG53961
					NET_SetPopPassword(savedPassword);
				}
				SetLocalFoldersAuthenticated();
				break;
			}
			else
			{
				char *alert = XP_GetString(XP_MSG_CACHED_PASSWORD_NOT_MATCHED);
				if (alert)
					FE_Alert(context, alert);
			}
			FREEIF(password);
		}
		if (ret != 0)
		{
			alert = XP_GetString(XP_MSG_PASSWORD_FAILED);
			FE_Alert(context, alert);
		}

		XP_FREE(prompt);
		FREEIF(unmungedPassword);
	}
	return ret;
}
