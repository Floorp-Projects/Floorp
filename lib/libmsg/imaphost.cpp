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
#include "imaphost.h"
#include "prefapi.h"
#include "msgmast.h"
#include "msgpane.h"
#include "msgimap.h"
#include "rosetta.h"
#include HG77677
#include "msgprefs.h"

#define FE_DOES_DELETE_MODEL			// all frontends do this now.

// I18N - this is a preference string and should not be localized
static const char *kPrefTemplate = "mail.imap.server.%s.%s";

MSG_IMAPHost::MSG_IMAPHost(const char* name, 
										XP_Bool xxx,
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespaces,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir)
{
	m_name = XP_STRDUP(name);
	HG83737
	m_userName = XP_STRDUP(userName);
	m_userPassword = NULL;
	m_adminURL = NULL;
	m_manageListsURL = NULL;
	m_manageFiltersURL = NULL;
	m_imapLocalDirectory = NULL;
	m_checkNewMail = checkNewMail;
	m_biffInterval = biffInterval;
	m_rememberPassword = rememberPassword;
	m_usingSubscription = usingSubscription;
	m_overrideNamespaces = overrideNamespaces;
	m_personalNamespacePrefix = (personalOnlineDir && *personalOnlineDir) ? XP_STRDUP(personalOnlineDir) : 0;
	m_publicNamespacePrefixes = (publicOnlineDir && *publicOnlineDir) ? XP_STRDUP(publicOnlineDir) : 0;
	m_otherUsersNamespacePrefixes = (otherUsersOnlineDir && *otherUsersOnlineDir) ? XP_STRDUP(otherUsersOnlineDir) : 0;
	if (!m_personalNamespacePrefix && !m_publicNamespacePrefixes && !m_otherUsersNamespacePrefixes)
		m_personalNamespacePrefix = PR_smprintf("\"\"");
	m_rootNamespacePrefix = PR_smprintf("");
	m_writingOutPrefs = FALSE;
	m_capability = 0;
	m_expungeInboxOnExit = TRUE;
	m_emptyTrashOnExit = FALSE;
	m_emptyTrashThreshhold = 0;
	m_passwordWritten = FALSE;
	m_folderOfflineDefault = FALSE;
	// set up m_imapLocalDirectory - taken from prefs code.
	char *machinePathName = 0;
	// see if there's a :port in the server name.  If so, strip it off when
	// creating the server directory.
	char *server = XP_STRDUP(m_name);
	char *port = 0;
	if (server)
	{
		port = XP_STRCHR(server,':');
		if (port)
		{
			*port = 0;
		}
		machinePathName = WH_FileName(server, xpImapServerDirectory);
		XP_FREE(server);
	}	
	if (machinePathName)
	{
		char *imapUrl = XP_PlatformFileToURL (machinePathName);
		if (imapUrl)
		{
			m_imapLocalDirectory = XP_STRDUP(imapUrl + XP_STRLEN("file://"));
			XP_FREE(imapUrl);
		}
		XP_FREE (machinePathName);
	}
	m_hasExtendedAdminURLs = FALSE;
	m_deleteIsMoveToTrash = FALSE;	// default is false.
	m_imapDeleteModel = MSG_IMAPDeleteIsIMAPDelete;
}

MSG_IMAPHost::~MSG_IMAPHost()
{
	FREEIF(m_name);
	FREEIF(m_userName);
	FREEIF(m_userPassword);
	FREEIF(m_adminURL);
	FREEIF(m_manageListsURL);
	FREEIF(m_manageFiltersURL);
	FREEIF(m_personalNamespacePrefix);
	FREEIF(m_publicNamespacePrefixes);
	FREEIF(m_otherUsersNamespacePrefixes);
	FREEIF(m_imapLocalDirectory);
	FREEIF(m_rootNamespacePrefix);
}

MSG_IMAPHostTable::MSG_IMAPHostTable(MSG_Master *master)
{
	m_master = master;
}

MSG_IMAPHostTable::~MSG_IMAPHostTable()
{
	int32 n = GetSize();
	for (int32 i=0 ; i<n ; i++) {
		delete GetHost(i);
	}
}

MSG_IMAPHost* MSG_IMAPHostTable::GetDefaultHost(XP_Bool /* createIfMissing */)
{
	return (GetSize() > 0) ? GetHost(0) : 0;
}

void MSG_IMAPHostTable::UpdatePrefs(const char *prefName)
{
	if (!XP_STRNCMP(prefName, "mail.imap.server.", 17) && *(prefName + 17))
	{
		char *serverName = XP_STRDUP(prefName + 17);
		char *endServerName = XP_STRCHR(serverName, '.');
		if (endServerName)
		{
			*endServerName = '\0';
			MSG_IMAPHost *imapHost = FindIMAPHost (serverName);
			if (imapHost)
				imapHost->InitFromPrefs();
		}
		FREEIF(serverName);
	}
}

/* do we need this */
/* I think so, for subscribe */
int32 MSG_IMAPHostTable::GetHostList(MSG_IMAPHost** result, int32 resultsize)
{
	int32 n = GetSize();
	if (n > resultsize) n = resultsize;
	for (int32 i=0 ; i<n ; i++) {
		result[i] = GetHost(i);
	}
	return GetSize();
}

MSG_IMAPHost *MSG_IMAPHostTable::AddIMAPHost(const char* name, XP_Bool xxx, 
									const char *userName,
									XP_Bool checkNewMail,
									int	biffInterval,
									XP_Bool rememberPassword,
									XP_Bool usingSubscription,
									XP_Bool overrideNamespace,
									const char *personalOnlineDir,
									const char *publicOnlineDir,
									const char *otherUsersOnlineDir,
									XP_Bool writePrefs)
{
	MSG_IMAPHost *host = new MSG_IMAPHost(name, xxx, userName, checkNewMail, biffInterval, rememberPassword, usingSubscription,
		overrideNamespace, personalOnlineDir, publicOnlineDir, otherUsersOnlineDir);
	if (host)
	{
		Add(host);
		WriteServerList(); // flush server list to prefs.
		if (writePrefs)	   // if we're adding this server from existing prefs, don't rewrite prefs, especially those that haven't been set yet
			host->WriteHostPrefs();
	}
	return host;
}

MSG_IMAPHost *MSG_IMAPHostTable::FindIMAPHost (const char* name)
{
	if (name)
	{
		for (int i = 0; i < GetSize(); i++)
		{
			MSG_IMAPHost *host = GetHost(i);
			if (!XP_STRCMP(name, host->m_name))
				return host;
		}
	}
	return NULL;
}

MSG_IMAPHost *MSG_IMAPHostTable::FindIMAPHost (MSG_FolderInfo *container)
{
	for (int i = 0; i < GetSize(); i++)
	{
		MSG_IMAPHost *host = GetHost(i);
		if (host->m_hostinfo == container)
			return host;
	}
	return NULL;
}

// if afterHost is NULL, put this host first.
void	MSG_IMAPHostTable::ReorderIMAPHost(MSG_IMAPHost *newHost, MSG_IMAPHost *afterHost)
{
	XP_ASSERT(afterHost != newHost);
	if (afterHost == newHost)
		return;

	for (int i = 0; i < GetSize(); i++)
	{
		MSG_IMAPHost *curHost = GetHost(i);
		if (newHost == curHost)
		{
			RemoveAt(i);
			i--;
		}
		else if (curHost == afterHost)
		{
			InsertAt(i + 1, newHost);
			i++;
		}
	}

	if (!afterHost)
		InsertAt(0, newHost);

	WriteServerList();
}

int MSG_IMAPHostTable::DeleteIMAPHost(MSG_IMAPHost* hostToRemove)
{
	for (int i = 0; i < GetSize(); i++)
	{
		MSG_IMAPHost *host = GetHost(i);
		if (host == hostToRemove)
		{
			char prefName[200];

			PR_snprintf(prefName, sizeof(prefName), kPrefTemplate, hostToRemove->m_name, "");
			// take off trailing '.'
			*( prefName + XP_STRLEN(prefName) - 1) = '\0';

			RemoveAt(i);
			PREF_DeleteBranch(prefName);

			XP_RemoveDirectoryRecursive(hostToRemove->m_name, xpImapServerDirectory);
			delete hostToRemove;

			break;
		}
	}
	WriteServerList();
	return 0;
}

void MSG_IMAPHostTable::WriteServerList()
{
	int serverListLen = 0;
	int i;

	for (i = 0; i < GetSize(); i++)
	{
		MSG_IMAPHost *host = GetHost(i);
		serverListLen += XP_STRLEN(host->m_name) + 1;
	}
	char *serverList = serverListLen ? (char *) XP_ALLOC(serverListLen) : 0;
	for (i = 0; i < GetSize(); i++)
	{
		MSG_IMAPHost *host = GetHost(i);
		if (i == 0)
			XP_STRCPY(serverList, host->m_name);
		else
		{
			XP_STRCAT(serverList, ",");
			XP_STRCAT(serverList, host->m_name);
		}
	}

	PREF_SetCharPref("network.hosts.imap_servers", (serverList) ? serverList : "");
	FREEIF(serverList);
}

/* static */
void MSG_IMAPHost::UpgradeDefaultServerPrefs(MSG_Master *mailMaster)
{
	const char *hostFromPrefs = mailMaster->GetPrefs()->GetPopHost();
	char *hostName = hostFromPrefs ? XP_STRDUP(hostFromPrefs) : (char *)NULL;
	const char	*userNameFromPrefs = NET_GetPopUsername();	// default to the default pop user name
	char *userName = userNameFromPrefs ? XP_STRDUP(userNameFromPrefs) : (char *)NULL;
	XP_Bool deleteIsMoveToTrash;
	int32 biffInterval = 0;
	XP_Bool checkNewMail;
	XP_Bool rememberPassword;
	XP_Bool defaultOfflineDownload;
	char *serverList = NULL;
	char *onlineDir = NULL;
	HG77678
	
	HG72266
	PREF_GetIntPref("mail.check_time", &biffInterval);
	PREF_GetBoolPref("mail.remember_password", &rememberPassword);
	PREF_GetBoolPref("mail.check_new_mail", &checkNewMail);
	PREF_GetBoolPref("mail.imap.delete_is_move_to_trash", &deleteIsMoveToTrash);
	PREF_CopyCharPref("mail.imap.server_sub_directory", &onlineDir);
	PREF_GetBoolPref("mail.imap.local_copies", &defaultOfflineDownload);
	PREF_CopyCharPref("network.hosts.imap_servers", &serverList);
	if (!serverList)
		PREF_SetCharPref("network.hosts.imap_servers", hostName);

	MSG_IMAPHost *imapHost = new MSG_IMAPHost(hostName, HG63531,	userName, checkNewMail, biffInterval, rememberPassword, FALSE, TRUE,
		onlineDir, NULL, NULL);

										
	if (imapHost)
	{
		imapHost->SetUserPassword(HG72267(NET_GetPopPassword()));
		imapHost->SetDeleteIsMoveToTrash(deleteIsMoveToTrash);
		imapHost->SetDefaultOfflineDownload(defaultOfflineDownload);
		imapHost->WriteHostPrefs();
		delete imapHost;
	}
	HG29872
	FREEIF(serverList);	  
	FREEIF(onlineDir);
	FREEIF(hostName);
	FREEIF(userName);
}

/* static */
MSG_IMAPHost *
MSG_IMAPHost::AddHostFromPrefs(const char *hostName, MSG_Master *mailMaster)
{
	char	*userName;
	char	*adminURL = NULL;
	int		prefSize = XP_STRLEN(hostName) + 60;
	char	*prefName = (char *) XP_ALLOC(prefSize);
	int32	biffInterval = 0;
	int32	deleteModel = -1;
	XP_Bool	biff = FALSE;
	XP_Bool isXXX = FALSE;
	XP_Bool	rememberPassword = FALSE;
	XP_Bool usingSubscription = FALSE;
	XP_Bool overrideNamespaces = TRUE;	// set to true if the NAMESPACE command can override user's manual prefs
	XP_Bool	deleteIsMoveToTrash = TRUE;
	XP_Bool checkNewMail = FALSE;
	char	*personalOnlineDir = NULL;
	char	*publicOnlineDir = NULL;
	char	*otherUsersOnlineDir = NULL;

	if (!prefName)
		return NULL;

	
	HG52442

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "userName");
	if (PREF_OK != PREF_CopyCharPref(prefName, &userName))
		userName = XP_STRDUP(NET_GetPopUsername());	// default to the default pop user name

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "check_time");
	PREF_GetIntPref(prefName, &biffInterval);

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "check_new_mail");
	PREF_GetBoolPref(prefName, &checkNewMail);

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "remember_password");
	PREF_GetBoolPref(prefName, &rememberPassword);

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "using_subscription");
	PREF_GetBoolPref(prefName, &usingSubscription);

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "override_namespaces");
	PREF_GetBoolPref(prefName, &overrideNamespaces);

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "delete_is_move_to_trash");
	PREF_GetBoolPref(prefName, &deleteIsMoveToTrash);

#ifdef FE_DOES_DELETE_MODEL
	int32	tempDeleteModel;
	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "delete_model");
	if (PREF_OK == PREF_GetIntPref(prefName, &tempDeleteModel))
		deleteModel = tempDeleteModel;
#endif

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "namespace.personal");
	PREF_CopyCharPref(prefName, &personalOnlineDir);
	if (personalOnlineDir && !(*personalOnlineDir))
	{
		XP_FREE(personalOnlineDir);
		personalOnlineDir = 0;
	}

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "namespace.public");
	PREF_CopyCharPref(prefName, &publicOnlineDir);
	if (publicOnlineDir && !(*publicOnlineDir))
	{
		XP_FREE(publicOnlineDir);
		publicOnlineDir = 0;
	}

	PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "namespace.other_users");
	PREF_CopyCharPref(prefName, &otherUsersOnlineDir);
	if (otherUsersOnlineDir && !(*otherUsersOnlineDir))
	{
		XP_FREE(otherUsersOnlineDir);
		otherUsersOnlineDir = 0;
	}


	// this adds the host to the folder tree, if the host doesn't already exist.
	MSG_IMAPHost *newHost = mailMaster->AddIMAPHost(hostName, isXXX, userName, checkNewMail, biffInterval,
		rememberPassword, usingSubscription, overrideNamespaces, personalOnlineDir, publicOnlineDir, otherUsersOnlineDir, FALSE);

	if (newHost)
	{
		XP_Bool	expungeInboxOnExit = TRUE;
		XP_Bool emptyTrashOnExit = FALSE;
		int32	emptyTrashThreshhold = 0;
		XP_Bool	offlineDownload = FALSE;

		if (rememberPassword)
		{
			char *password = NULL; 

			PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "password");
			PREF_CopyCharPref(prefName, &password);
			newHost->SetUserPassword(password);
			FREEIF(password);
		}

		if (deleteModel != -1)
			newHost->SetIMAPDeleteModel((MSG_IMAPDeleteModel) deleteModel);
		else
			newHost->SetDeleteIsMoveToTrash(deleteIsMoveToTrash);

		PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "cleanup_inbox_on_exit");
		PREF_GetBoolPref(prefName, &expungeInboxOnExit);

		newHost->SetExpungeInboxOnExit(expungeInboxOnExit);

		PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "empty_trash_on_exit");
		PREF_GetBoolPref(prefName, &emptyTrashOnExit);

		newHost->SetEmptyTrashOnExit(emptyTrashOnExit);

		PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "offline_download");
		PREF_GetBoolPref(prefName, &offlineDownload);

		newHost->SetDefaultOfflineDownload(offlineDownload);

		PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "empty_trash_threshhold");
		PREF_GetIntPref(prefName, &emptyTrashThreshhold);

		newHost->SetEmptyTrashThreshhold(emptyTrashThreshhold);

		PR_snprintf(prefName, prefSize, kPrefTemplate, hostName, "admin_url");
		PREF_CopyCharPref(prefName, &adminURL);

		newHost->SetAdminURL(adminURL);

		// Creates a new host object for the IMAP thread to use for runtime data
		IMAP_AddIMAPHost(hostName, usingSubscription, overrideNamespaces, newHost->GetPersonalNamespacePrefixPref(), 
			newHost->GetPublicNamespacePrefixPref(), newHost->GetOtherUsersNamespacePrefixPref(), 
			adminURL && XP_STRLEN(adminURL) > 0);

	}


	FREEIF(userName);
	FREEIF(adminURL);
	FREEIF(personalOnlineDir);
	FREEIF(publicOnlineDir);
	FREEIF(otherUsersOnlineDir);
	XP_FREE(prefName);
	return newHost;
}

void MSG_IMAPHost::InitFromPrefs()
{
	if (m_writingOutPrefs)
		return; // Don't do this if we are just trying to write them out (in WriteHostPrefs).
	
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*prefName = (char *) XP_ALLOC(prefSize);

	if (!prefName)
		return;

	HG42322

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "userName");
	FREEIF(m_userName);
	PREF_CopyCharPref(prefName, &m_userName);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "check_time");
	PREF_GetIntPref(prefName, &m_biffInterval);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "remember_password");
	PREF_GetBoolPref(prefName, &m_rememberPassword);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "using_subscription");
	PREF_GetBoolPref(prefName, &m_usingSubscription);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "override_namespaces");
	PREF_GetBoolPref(prefName, &m_overrideNamespaces);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.personal");
	FREEIF(m_personalNamespacePrefix);
	PREF_CopyCharPref(prefName, &m_personalNamespacePrefix);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.public");
	FREEIF(m_publicNamespacePrefixes);
	PREF_CopyCharPref(prefName, &m_publicNamespacePrefixes);
	
	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.other_users");
	FREEIF(m_otherUsersNamespacePrefixes);
	PREF_CopyCharPref(prefName, &m_otherUsersNamespacePrefixes);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "delete_model");

	int32 tempDeleteModel;
	int deleteModelStatus = PREF_GetIntPref(prefName, &tempDeleteModel);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "delete_is_move_to_trash");
	PREF_GetBoolPref(prefName, &m_deleteIsMoveToTrash);

#ifdef FE_DOES_DELETE_MODEL
	if (PREF_OK == deleteModelStatus)
	{
//		PREF_ClearUserPref(prefName); want to turn this on soon...
		m_imapDeleteModel = (MSG_IMAPDeleteModel) tempDeleteModel;
		m_deleteIsMoveToTrash = (m_imapDeleteModel == MSG_IMAPDeleteIsMoveToTrash);
	}
	else
#endif
	{
		if (m_deleteIsMoveToTrash)
			m_imapDeleteModel = MSG_IMAPDeleteIsMoveToTrash;
		else
			m_imapDeleteModel = MSG_IMAPDeleteIsIMAPDelete;
	}

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "cleanup_inbox_on_exit");
	PREF_GetBoolPref(prefName, &m_expungeInboxOnExit);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "empty_trash_on_exit");
	PREF_GetBoolPref(prefName, &m_emptyTrashOnExit);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "offline_download");
	PREF_GetBoolPref(prefName, &m_folderOfflineDefault);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "empty_trash_threshhold");
	PREF_GetIntPref(prefName, &m_emptyTrashThreshhold);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "admin_url");
	FREEIF(m_adminURL);
	PREF_CopyCharPref(prefName, &m_adminURL);

	XP_FREE(prefName);

	HG72530
}

void MSG_IMAPHost::WriteHostPrefs()
{
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*prefName = (char *) XP_ALLOC(prefSize);

	if (!prefName)
		return;

	// Disable clobbering via pref callbacks.
	m_writingOutPrefs = TRUE;
	
	HG52421

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "userName");
	PREF_SetCharPref(prefName, m_userName);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "check_time");
	PREF_SetIntPref(prefName, m_biffInterval);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "remember_password");
	PREF_SetBoolPref(prefName, m_rememberPassword);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "using_subscription");
	PREF_SetBoolPref(prefName, m_usingSubscription);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "override_namespaces");
	PREF_SetBoolPref(prefName, m_overrideNamespaces);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.personal");
	PREF_SetCharPref(prefName, (m_personalNamespacePrefix) ? m_personalNamespacePrefix : "");

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.public");
	PREF_SetCharPref(prefName, (m_publicNamespacePrefixes) ? m_publicNamespacePrefixes : "");
	
	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.other_users");
	PREF_SetCharPref(prefName, (m_otherUsersNamespacePrefixes) ? m_otherUsersNamespacePrefixes : "");

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "admin_url");
	PREF_SetCharPref(prefName, (m_adminURL) ? m_adminURL : "");

	if (m_rememberPassword && m_userPassword)
	{
		PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "password");
		PREF_SetCharPref(prefName, (m_userPassword) ? m_userPassword : "");
	}
	// don't write this out, which will punt on backward compatability... 
//	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "delete_is_move_to_trash");
//	PREF_SetBoolPref(prefName, m_deleteIsMoveToTrash);

#ifdef FE_DOES_DELETE_MODEL
	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "delete_model");
	PREF_SetIntPref(prefName, m_imapDeleteModel);
#endif
	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "cleanup_inbox_on_exit");
	PREF_SetBoolPref(prefName, m_expungeInboxOnExit);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "empty_trash_on_exit");
	PREF_SetBoolPref(prefName, m_emptyTrashOnExit);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "empty_trash_threshhold");
	PREF_SetIntPref(prefName, m_emptyTrashThreshhold);

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "offline_download");
	PREF_SetBoolPref(prefName, m_folderOfflineDefault);

	// Re-enable clobbering via pref callbacks.
	m_writingOutPrefs = FALSE;

	XP_FREE(prefName);
}


char *MSG_IMAPHost::GetPrettyName()
{
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*prefName = (char *) XP_ALLOC(prefSize);
	char	*rv = NULL;

	if (!prefName)
		return NULL;

	PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "displayName");
	PREF_CopyCharPref(prefName, &rv);

	XP_FREE(prefName);
	return rv;
}


void MSG_IMAPHost::SetNamespacePrefix(EIMAPNamespaceType type, const char *prefixes)
{
	if (m_overrideNamespaces)	// if the server can override us
	{

		int		prefSize = XP_STRLEN(m_name) + 60;
		char	*prefName = (char *) XP_ALLOC(prefSize);

		switch (type)
		{
		case kPersonalNamespace:
			FREEIF(m_personalNamespacePrefix);
			m_personalNamespacePrefix = prefixes ? XP_STRDUP(prefixes) : (char *)NULL;

			// update the prefs now.
			if (prefName)
			{
				PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.personal");
				m_writingOutPrefs = TRUE;
				if (m_personalNamespacePrefix)
					PREF_SetCharPref(prefName, m_personalNamespacePrefix);
				else
					PREF_SetCharPref(prefName, "");
				m_writingOutPrefs = FALSE;
			}
			break;
		case kPublicNamespace:
			FREEIF(m_publicNamespacePrefixes);
			m_publicNamespacePrefixes = prefixes ? XP_STRDUP(prefixes) : (char *)NULL;

			// update the prefs now.
			if (prefName)
			{
				PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.public");
				m_writingOutPrefs = TRUE;
				if (m_publicNamespacePrefixes)
					PREF_SetCharPref(prefName, m_publicNamespacePrefixes);
				else
					PREF_SetCharPref(prefName, "");
				m_writingOutPrefs = FALSE;
			}
			break;
		case kOtherUsersNamespace:
			FREEIF(m_otherUsersNamespacePrefixes);
			m_otherUsersNamespacePrefixes = prefixes ? XP_STRDUP(prefixes) : (char *)NULL;

			// update the prefs now.
			if (prefName)
			{
				PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "namespace.other_users");
				m_writingOutPrefs = TRUE;
				if (prefixes)
					PREF_SetCharPref(prefName, m_otherUsersNamespacePrefixes);
				else
					PREF_SetCharPref(prefName, "");
				m_writingOutPrefs = FALSE;
			}
			break;
		default:
			XP_ASSERT(FALSE);
			break;
		}

		FREEIF(prefName);
		HG52987
		WriteHostPrefs();
	}
}

const char *MSG_IMAPHost::GetUserPassword()
{
	if (m_hostinfo && m_hostinfo->GetMaster()->IsCachePasswordProtected())
	{
		MSG_FolderInfo *folderToUpdate = NULL;
		m_hostinfo->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &folderToUpdate, 1);
		if (folderToUpdate)
		{
			const char *password = folderToUpdate->GetRememberedPassword();
		}
	}
	return m_userPassword;
}

void MSG_IMAPHost::SetUserPassword(const char *password)
{
	if (!m_passwordWritten && m_hostinfo && m_hostinfo->GetMaster()->IsCachePasswordProtected())
	{
		MSG_FolderInfo *folderToUpdate = NULL;
		m_hostinfo->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &folderToUpdate, 1);
		if (folderToUpdate)
		{
			folderToUpdate->RememberPassword(password);
		}
	}
	if (password)
	{
		FREEIF(m_userPassword);
		m_userPassword = XP_STRDUP(password);
		WriteHostPrefs();
	}
	m_passwordWritten = TRUE;
}

void MSG_IMAPHost::SetAdminURL(const char *adminURL)
{
	if (adminURL)
	{
		FREEIF(m_adminURL);
		m_adminURL = XP_STRDUP(adminURL);
		WriteHostPrefs();
	}
}

void		MSG_IMAPHost::SetManageFiltersURL(const char *manageFiltersURL)
{
	// by definition, if we have this url, we have extended urls
	m_hasExtendedAdminURLs = TRUE;
	if (manageFiltersURL)
	{
		FREEIF(m_manageFiltersURL);
		m_manageFiltersURL = XP_STRDUP(manageFiltersURL);
		WriteHostPrefs();
	}
}

void		MSG_IMAPHost::SetManageListsURL(const char *manageListsURL)
{
	m_hasExtendedAdminURLs = TRUE;
	if (manageListsURL)
	{
		FREEIF(m_manageListsURL);
		m_manageListsURL = XP_STRDUP(manageListsURL);
		WriteHostPrefs();
	}
}

/*static*/ void MSG_IMAPHost::RefreshUrlCallback (URL_Struct *, int status, MWContext *window_id)
{
	if (status >= 0)
	{
		if (window_id->currentIMAPfolder && window_id->currentIMAPfolder->AdminUrl() != NULL)
			window_id->currentIMAPfolder->GetAdminUrl(window_id, MSG_AdminFolder);
	}
}


int32 MSG_IMAPHost::GetBiffInterval()
{
	return m_biffInterval;
}


XP_Bool	MSG_IMAPHost::RunAdminURL(MWContext *context, MSG_FolderInfo *folder, MSG_AdminURLType type)
{
	char *url = NULL;
	Net_GetUrlExitFunc *preExitFunc = NULL;

	if (HaveAdminURL(type))
	{
		switch (type)
		{
		case MSG_AdminServer:
			url = XP_STRDUP(m_adminURL);
			break;
		case MSG_AdminFolder:
		{
			MSG_IMAPFolderInfoMail *imapFolder = (folder) ? folder->GetIMAPFolderInfoMail() : 0;
			if (imapFolder)
			{
				const char *adminUrl = imapFolder->AdminUrl();
				if (!adminUrl)
				{
					url = CreateIMAPRefreshFolderURLs(m_name, imapFolder->GetOnlineName());
					context->mailMaster = folder->GetMaster();
					context->currentIMAPfolder = imapFolder;
					preExitFunc = RefreshUrlCallback;
				}
				else
				{
					url = XP_STRDUP(adminUrl);
				}
			}
			break;
		}
		case MSG_AdminServerSideFilters:
			if (m_manageFiltersURL && XP_STRLEN(m_manageFiltersURL) > 0)
				url = XP_STRDUP(m_manageFiltersURL);
			break;
		case MSG_AdminServerLists:
			if (m_manageListsURL && XP_STRLEN(m_manageListsURL) > 0)
				url = XP_STRDUP(m_manageListsURL);
			break;
		default:
			break;
		}
	}
	if (url)
	{
		URL_Struct *urlStruct = NET_CreateURLStruct(url, NET_NORMAL_RELOAD);
		urlStruct->pre_exit_fn = preExitFunc;

		FE_GetURL (context, urlStruct);
		XP_FREE(url);
		return TRUE;
	}
	return FALSE;
}

XP_Bool	MSG_IMAPHost::HaveAdminURL(MSG_AdminURLType type)
{
	if (m_adminURL && XP_STRLEN(m_adminURL))
	{
		switch (type)
		{
		case MSG_AdminServer:
			return TRUE;
		case MSG_AdminServerSideFilters:
		case MSG_AdminFolder:
		case MSG_AdminServerLists:
			return m_hasExtendedAdminURLs;
		default:
			break;
		}
	}
	return FALSE;
}

void MSG_IMAPHost::SetDeleteIsMoveToTrash(XP_Bool deleteIsMoveToTrash)
{
	m_deleteIsMoveToTrash = deleteIsMoveToTrash;
	m_imapDeleteModel = (deleteIsMoveToTrash) ? MSG_IMAPDeleteIsMoveToTrash : MSG_IMAPDeleteIsIMAPDelete;
}

void MSG_IMAPHost::SetIMAPDeleteModel(MSG_IMAPDeleteModel deleteModel)
{
	m_imapDeleteModel = deleteModel;
	m_deleteIsMoveToTrash = (deleteModel == MSG_IMAPDeleteIsMoveToTrash);
}

int MSG_IMAPHost::RemoveHost()
{
	// ### dmb - need to delete all the associated files...
	MSG_Master *master = m_hostinfo->GetMaster();
	master->BroadcastFolderDeleted(m_hostinfo);
	master->GetFolderTree()->RemoveSubFolder(m_hostinfo);
	master->GetIMAPHostTable()->DeleteIMAPHost(this);
	return 0;
}


void MSG_IMAPHost::SetHostNeedsFolderUpdate(XP_Bool needsUpdate)
{
	MSG_IMAPFolderInfoContainer *hostFolderInfo = GetHostFolderInfo();
	if (hostFolderInfo)
	{
		hostFolderInfo->SetHostNeedsFolderUpdate(needsUpdate);
	}
}

// This should really only be set in the one-time subscription upgrade process
XP_Bool	MSG_IMAPHost::SetIsHostUsingSubscription(XP_Bool usingSubscription)
{
	m_usingSubscription = usingSubscription;
	WriteHostPrefs();
	IMAP_SetHostIsUsingSubscription(m_name, usingSubscription);
	return m_usingSubscription;
}

// later: Fix this to return the real port.
int32 MSG_IMAPHost::getPort()
{
	XP_ASSERT(FALSE);
	return 143;
}

MSG_FolderInfo *MSG_IMAPHost::GetTrashFolderForHost()
{
	if (!m_deleteIsMoveToTrash)	// If delete is not move to trash, then there is no
		return NULL;			// magic trash folder

	MSG_FolderInfo *trashFolder = NULL;
	GetHostFolderInfo()->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, &trashFolder, 1);
	return trashFolder;
}

int MSG_IMAPHost::FolderMatchesNamespace(const char *folder, const char *nsprefix)
{
	if (!folder) return -1;

	// If the namespace is part of the folder
	if (XP_STRSTR(folder, nsprefix) == folder)
		return XP_STRLEN(nsprefix);

	// If the folder is part of the prefix
	// (Used for matching Personal folder with Personal/ namespace, etc.)
	if (XP_STRSTR(nsprefix, folder) == nsprefix)
		return XP_STRLEN(folder);

	return -1;
}

const char *MSG_IMAPHost::GetNamespacePrefixForFolder(const char *folder)
{

	int lengthMatched = -1, currentMatchedLength = -1;
	char *rv = NULL, *nspref = m_personalNamespacePrefix;
	EIMAPNamespaceType type = kPersonalNamespace;

	for (int i = 1; i <= 3; i++)
	{
		switch (i)
		{
		case 1:
			type = kPersonalNamespace;
			nspref = m_personalNamespacePrefix;
			break;
		case 2:
			type = kPublicNamespace;
			nspref = m_publicNamespacePrefixes;
			break;
		case 3:
			type = kOtherUsersNamespace;
			nspref = m_otherUsersNamespacePrefixes;
			break;
		}


		int numNamespaces = IMAP_UnserializeNamespaces(nspref, NULL, 0);
		char **ns = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (ns)
		{
			int len = IMAP_UnserializeNamespaces(nspref, ns, numNamespaces);
			for (int k = 0; k < len; k++)
			{
				currentMatchedLength = FolderMatchesNamespace(folder, ns[k]);
				if (currentMatchedLength > lengthMatched)
				{
					rv = ns[k];
					lengthMatched = currentMatchedLength;
				}
				else
				{
					FREEIF(ns[k]);
				}
			}
			XP_FREE(ns);
		}
	}

	return rv;
}


EIMAPNamespaceType MSG_IMAPHost::GetNamespaceTypeForFolder(const char *folder)
{

	int lengthMatched = -1, currentMatchedLength = -1;
	char *nspref = m_personalNamespacePrefix;
	EIMAPNamespaceType type = kPersonalNamespace, rv = kUnknownNamespace;

	for (int i = 1; i <= 3; i++)
	{
		switch (i)
		{
		case 1:
			type = kPersonalNamespace;
			nspref = m_personalNamespacePrefix;
			break;
		case 2:
			type = kPublicNamespace;
			nspref = m_publicNamespacePrefixes;
			break;
		case 3:
			type = kOtherUsersNamespace;
			nspref = m_otherUsersNamespacePrefixes;
			break;
		}


		int numNamespaces = IMAP_UnserializeNamespaces(nspref, NULL, 0);
		char **ns = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (ns)
		{
			int len = IMAP_UnserializeNamespaces(nspref, ns, numNamespaces);
			for (int k = 0; k < len; k++)
			{
				currentMatchedLength = FolderMatchesNamespace(folder, ns[k]);
				if (currentMatchedLength > lengthMatched)
				{
					rv = type;
					lengthMatched = currentMatchedLength;
				}
				else
				{
					FREEIF(ns[k]);
				}
			}
			XP_FREE(ns);
		}
	}

	return rv;
}

const char *MSG_IMAPHost::GetDefaultNamespacePrefixOfType(EIMAPNamespaceType type)
{
	char *str = m_personalNamespacePrefix;
	switch (type)
	{
	case kPersonalNamespace:
		str = m_personalNamespacePrefix;
		break;
	case kPublicNamespace:
		str = m_publicNamespacePrefixes;
		break;
	case kOtherUsersNamespace:
		str = m_otherUsersNamespacePrefixes;
		break;
	}

	char *rv = NULL, *firstFound = NULL;;

	int numNamespaces = IMAP_UnserializeNamespaces(str, NULL, 0);
	char **ns = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
	if (ns)
	{
		int len = IMAP_UnserializeNamespaces(str, ns, numNamespaces);
		for (int k = 0; k < len; k++)
		{
			if (!(*(ns[k])))
			{
				// If this is the empty namespace of this type, it is the default
				rv = ns[k];
			}

			if (k ==0)
			{
				firstFound = ns[k];
			}
			else if (rv != ns[k])
			{
				FREEIF(ns[k]);
			}
		}
		XP_FREE(ns);
	}
	if (!rv)	// If we didn't find an empty ("") namespace, use the first one in the list
		rv = firstFound;
	if (firstFound != rv)	// If the empty namespace wasn't the first one found, free the first one found
		FREEIF(firstFound);
	return rv;
}

XP_Bool MSG_IMAPHost::GetAreAnyNamespacesRoot()
{
	XP_Bool rootFound = FALSE;
	char *rv = NULL, *nspref = m_personalNamespacePrefix;
	EIMAPNamespaceType type = kPersonalNamespace;

	for (int i = 1; i <= 3 && !rootFound; i++)
	{
		switch (i)
		{
		case 1:
			type = kPersonalNamespace;
			nspref = m_personalNamespacePrefix;
			break;
		case 2:
			type = kPublicNamespace;
			nspref = m_publicNamespacePrefixes;
			break;
		case 3:
			type = kOtherUsersNamespace;
			nspref = m_otherUsersNamespacePrefixes;
			break;
		}


		int numNamespaces = IMAP_UnserializeNamespaces(nspref, NULL, 0);
		char **ns = (char**) XP_CALLOC(numNamespaces, sizeof(char*));
		if (ns)
		{
			int len = IMAP_UnserializeNamespaces(nspref, ns, numNamespaces);
			for (int k = 0; k < len && !rootFound; k++)
			{
				rootFound = (!(*(ns[k])));
				FREEIF(ns[k]);
			}
			XP_FREE(ns);
		}
	}

	return rootFound;
}

XP_Bool MSG_IMAPHost::GetShouldStripThisNamespacePrefix(const char *prefix)
{
	// If this namespace is "" then we don't need to strip it
	if (!(*prefix))
		return FALSE;

	// If any other namespaces are "" then we can't strip this one
	if (GetAreAnyNamespacesRoot())
		return FALSE;

	// If this is the only personal namespace, then we can strip it off.
	EIMAPNamespaceType type = GetNamespaceTypeForFolder(prefix);
	if (type == kPersonalNamespace)
	{
		char *str = m_personalNamespacePrefix;
		switch (type)
		{
		case kPersonalNamespace:
			str = m_personalNamespacePrefix;
			break;
		case kPublicNamespace:
			str = m_publicNamespacePrefixes;
			break;
		case kOtherUsersNamespace:
			str = m_personalNamespacePrefix;
			break;
		}
		int numNamespaces = IMAP_UnserializeNamespaces(str, NULL, 0);
		return (numNamespaces <= 1);
	}
	else
		return FALSE;
}

// This returns the namespace prefix of the namespace which
// was stripped to the root
const char *MSG_IMAPHost::GetRootNamespacePrefix()
{
	const char *def = GetDefaultNamespacePrefixOfType(kPersonalNamespace);
	if (def)
	{
		if (GetShouldStripThisNamespacePrefix(def))
			return def;	// This is the one that was stripped to be the root
		else
			return m_rootNamespacePrefix;	// the default personal namespace wasn't stripped;  so, there must really be "" at the root
	}
	else
		return m_rootNamespacePrefix;
}

XP_Bool MSG_IMAPHost::GetHostSupportsSharing()
{
	return (GetCapabilityForHost() & kACLCapability);
}

// If there is a runtime host and we have its capabilities,
// get them from there.  If it has changed since the last session,
// write it out to the preferences.
// If we don't have a runtime capability, get them from the cached
// info in the prefs.
uint32 MSG_IMAPHost::GetCapabilityForHost()
{

	uint32 runtimeCapability = IMAP_GetCapabilityForHost(m_name);
	if (runtimeCapability)	// capabilities defined
	{
		if (runtimeCapability != m_capability)	// capabilities different than those in the prefs, or we haven't read from prefs yet
		{
			m_capability = runtimeCapability;
			int		prefSize = XP_STRLEN(m_name) + 60;
			char	*prefName = (char *) XP_ALLOC(prefSize);
			if (prefName)
			{
				// update the prefs
				PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "capability");
				m_writingOutPrefs = TRUE;
				PREF_SetIntPref(prefName, m_capability);
				m_writingOutPrefs = FALSE;
				XP_FREE(prefName);
			}
		}
	}
	else
	{
		// haven't connected yet, try getting it from the prefs.
		int		prefSize = XP_STRLEN(m_name) + 60;
		char	*prefName = (char *) XP_ALLOC(prefSize);
		if (prefName)
		{
			PR_snprintf(prefName, prefSize, kPrefTemplate, m_name, "capability");
			int32 capability = 0;
			PREF_GetIntPref(prefName, &capability);
			m_capability = (uint32) capability;
			XP_FREE(prefName);
		}
	}
	return m_capability;
}

extern "C" const char *MSG_GetIMAPHostUsername(MSG_Master *master, const char *hostName)
{
	MSG_IMAPHost *host = NULL;
	if (master && master->GetIMAPHostTable())
	{
		host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
		if (host)
			return host->GetUserName();
	}
	// we could look up the preference for the host, if we were nice.
	return NULL;
}

extern "C" const char *MSG_GetIMAPHostPassword(MSG_Master *master, const char *hostName)
{
	MSG_IMAPHost *host = NULL;
	if (master && master->GetIMAPHostTable())
	{
		host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
		if (host)
			return HG72224(host->GetUserPassword());
	}
	// we could look up the preference for the host, if we were nice.
	return NULL;
}

extern "C" void MSG_SetIMAPHostPassword(MSG_Master *master, const char *hostName, const char *password)
{
	MSG_IMAPHost *host = NULL;
	if (master && master->GetIMAPHostTable())
	{
		host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
		if (host)
		{
			HG52286
			host->SetUserPassword(HG62294(password));
			if (host->GetRememberPassword())
			{
				int		prefSize = XP_STRLEN(host->GetHostName()) + 60;
				char	*prefName = (char *) XP_ALLOC(prefSize);
				PR_snprintf(prefName, prefSize, "mail.imap.server.%s.password", host->GetHostName());
				host->SetWritingOutPrefs(TRUE);
				PREF_SetCharPref(prefName, host->GetUserPassword());
				host->SetWritingOutPrefs(FALSE);
				XP_FREE(prefName);
			}
		}
	}
}

extern "C" void MSG_SetNamespacePrefixes(MSG_Master *master, const char *hostName,
										 EIMAPNamespaceType type, const char *prefixes)
{
	MSG_IMAPHost *host = NULL;
	if (master && master->GetIMAPHostTable())
	{
		host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
		if (host)
			host->SetNamespacePrefix(type, prefixes);
	}
}

extern "C" void MSG_CommitCapabilityForHost(const char *hostName, MSG_Master *master)
{
	MSG_IMAPHost *host = NULL;
	if (master && master->GetIMAPHostTable())
	{
		host = master->GetIMAPHostTable()->FindIMAPHost(hostName);
		if (host)
			host->GetCapabilityForHost();	/* causes a refresh */
	}
}

void MSG_IMAPHost::WriteStrPref(const char *prefName, const char *strValue)
{
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*wholePrefName = (char *) XP_ALLOC(prefSize);

	if (!wholePrefName)
		return;

	// Disable clobbering via pref callbacks.
	m_writingOutPrefs = TRUE;
	HG64384
	PR_snprintf(wholePrefName, prefSize, kPrefTemplate, m_name, prefName);
	PREF_SetCharPref(wholePrefName, strValue);
	m_writingOutPrefs = FALSE;
	XP_FREE(wholePrefName);
}

void MSG_IMAPHost::WriteBoolPref(const char *prefName, XP_Bool boolValue)
{
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*wholePrefName = (char *) XP_ALLOC(prefSize);

	if (!wholePrefName)
		return;

	// Disable clobbering via pref callbacks.
	m_writingOutPrefs = TRUE;
	
	PR_snprintf(wholePrefName, prefSize, kPrefTemplate, m_name, prefName);
	PREF_SetBoolPref(wholePrefName, boolValue);
	m_writingOutPrefs = FALSE;
	XP_FREE(wholePrefName);
}

void MSG_IMAPHost::WriteIntPref(const char *prefName, int32 intValue)
{
	int		prefSize = XP_STRLEN(m_name) + 60;
	char	*wholePrefName = (char *) XP_ALLOC(prefSize);

	if (!wholePrefName)
		return;

	// Disable clobbering via pref callbacks.
	m_writingOutPrefs = TRUE;
	
	PR_snprintf(wholePrefName, prefSize, kPrefTemplate, m_name, prefName);
	PREF_SetIntPref(wholePrefName, intValue);
	m_writingOutPrefs = FALSE;
	XP_FREE(wholePrefName);
}

