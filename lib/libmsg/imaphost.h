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

#ifndef _IMAPHOST_H
#define _IMAPHOST_H

#include "msghost.h"
#include "ptrarray.h"
#include "imap.h"
class MSG_IMAPFolderInfoContainer;

class MSG_IMAPHost : public MSG_Host {
	friend class MSG_IMAPHostTable;
public:
	MSG_IMAPHost(const char* name, 
										XP_Bool secure,
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespace,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir);
	virtual ~MSG_IMAPHost();

    virtual XP_Bool IsIMAP () { return TRUE; }
	virtual MSG_IMAPHost *GetIMAPHost() { return this; }

    // Returns a fully descriptive name for this IMAP host, possibly including the
    // ":<port>" and also possibly a trailing (and localized) " (secure)"
	// Well, in theory, anyway.
	virtual const char* getFullUIName() { return GetHostName(); }

	// Returns a newly-allocated pretty name for this host, only if set in the prefs.
	// Returns NULL otherwise
	virtual char	*GetPrettyName();
	virtual int32 getPort();

	virtual int RemoveHost();
	
	static MSG_IMAPHost *AddHostFromPrefs(const char *hostName, MSG_Master *mailMaster);
	static void UpgradeDefaultServerPrefs(MSG_Master *mailMaster);
	void	WriteHostPrefs();
	void	InitFromPrefs();
	
	// GetTrashFolderForHost() returns the MSG_FolderInfo* of the Trash
	// folder on the host if it is using the Trash delete model;  otherwise,
	// returns NULL.
	MSG_FolderInfo *GetTrashFolderForHost();

	virtual XP_Bool isSecure() {return m_secure;}
	const char *GetUserName() {return m_userName;}
	void		SetHostInfo(MSG_IMAPFolderInfoContainer *folder) {m_hostinfo = folder;}
	MSG_IMAPFolderInfoContainer *GetHostFolderInfo() {return m_hostinfo;}
	const char *GetHostName() { return m_name;}
	const char *GetUserPassword() ;
	void		SetUserPassword(const char *password) ;
	const char *GetLocalDirectory() {return m_imapLocalDirectory;}
	XP_Bool		GetRememberPassword() {return m_rememberPassword;}
	XP_Bool		GetIsHostUsingSubscription() {return m_usingSubscription;}
	XP_Bool		SetIsHostUsingSubscription(XP_Bool usingSubscription);
	void		SetAdminURL(const char *adminURL);
	const char *GetAdminURL() {return m_adminURL;}
	void		SetManageFiltersURL(const char *manageFiltersURL);
	const char *GetManageFiltersURL() {return m_manageFiltersURL;}
	void		SetManageListsURL(const char *manageListsURL);
	const char *GetManageListsURL() {return m_manageListsURL;}
	static void RefreshUrlCallback (URL_Struct *URL_s, int status, MWContext *window_id);
	XP_Bool		RunAdminURL(MWContext *context, MSG_FolderInfo *folder, MSG_AdminURLType type);
	XP_Bool		HaveAdminURL(MSG_AdminURLType type);
	void		SetHostNeedsFolderUpdate(XP_Bool needsUpdate);
	XP_Bool		GetOverrideNamespaces() { return m_overrideNamespaces; }
	void		SetDeleteIsMoveToTrash(XP_Bool deleteIsMoveToTrash);
	XP_Bool		GetDeleteIsMoveToTrash() {return (m_imapDeleteModel == MSG_IMAPDeleteIsMoveToTrash);}
	XP_Bool		GetCheckNewMail() {return m_checkNewMail;}
	MSG_IMAPDeleteModel	GetIMAPDeleteModel() {return m_imapDeleteModel;}
	void				SetIMAPDeleteModel(MSG_IMAPDeleteModel deleteModel);

	XP_Bool		GetExpungeInboxOnExit() {return m_expungeInboxOnExit;}
	XP_Bool		GetEmptyTrashOnExit() { return m_emptyTrashOnExit;}
	int32		GetEmptyTrashThreshhold() {return m_emptyTrashThreshhold;}
	void		SetEmptyTrashOnExit(XP_Bool emptyTrash) {m_emptyTrashOnExit = emptyTrash;}
	void		SetExpungeInboxOnExit(XP_Bool expungeInbox) {m_expungeInboxOnExit = expungeInbox;}
	void		SetEmptyTrashThreshhold(int32 threshhold) {m_emptyTrashThreshhold = threshhold;}
	XP_Bool		GetDefaultOfflineDownload() {return m_folderOfflineDefault;}
	void		SetDefaultOfflineDownload(XP_Bool defaultOffline) {m_folderOfflineDefault = defaultOffline;}

	//const char *GetPersonalNamespacePrefix() { return m_personalNamespacePrefix; }
	void		SetNamespacePrefix(EIMAPNamespaceType type, const char *prefix);
	const char *GetNamespacePrefixForFolder(const char *folder);
	EIMAPNamespaceType	GetNamespaceTypeForFolder(const char *folder);
	const char *GetDefaultNamespacePrefixOfType(EIMAPNamespaceType type);
	const char *GetRootNamespacePrefix();
	XP_Bool		GetShouldStripThisNamespacePrefix(const char *prefix);
	XP_Bool		GetAreAnyNamespacesRoot();
	XP_Bool		GetHostSupportsSharing();
	int32		GetBiffInterval();
	uint32		GetCapabilityForHost();
	void		SetWritingOutPrefs(XP_Bool w) { m_writingOutPrefs = w; }


protected:
	int			FolderMatchesNamespace(const char *folder, const char *nsprefix);
	char		*GetPersonalNamespacePrefixPref() { return m_personalNamespacePrefix; }
	char		*GetPublicNamespacePrefixPref() { return m_publicNamespacePrefixes; }
	char		*GetOtherUsersNamespacePrefixPref() { return m_otherUsersNamespacePrefixes; }
	void		WriteStrPref(const char *prefName, const char *strValue);
	void		WriteBoolPref(const char *prefName, XP_Bool boolValue);
	void		WriteIntPref(const char *prefName, int32 intValue);


protected:
	char		* m_name;
	XP_Bool		m_secure;
	XP_Bool		m_usingSubscription;
	XP_Bool		m_overrideNamespaces;
	XP_Bool		m_checkNewMail;
	uint32		m_capability;

	char		*m_personalNamespacePrefix;
	char		*m_publicNamespacePrefixes;
	char		*m_otherUsersNamespacePrefixes;
	char		*m_rootNamespacePrefix;
	char		*m_userName;
	char		*m_userPassword;
	char		*m_imapLocalDirectory;
	char		*m_defaultPersonalNamespacePrefix;
	char		*m_adminURL;
	char		*m_manageListsURL;
	char		*m_manageFiltersURL;
	int32		m_biffInterval;
	MSG_IMAPDeleteModel	m_imapDeleteModel;
	XP_Bool		m_rememberPassword;
	XP_Bool		m_hasExtendedAdminURLs;
	XP_Bool		m_deleteIsMoveToTrash;
	XP_Bool		m_expungeInboxOnExit;
	XP_Bool		m_emptyTrashOnExit;
	int32		m_emptyTrashThreshhold;
	XP_Bool		m_writingOutPrefs;
	MSG_IMAPFolderInfoContainer* m_hostinfo;	// Object representing entire imap host in tree
	XP_Bool		m_passwordWritten;
	XP_Bool		m_folderOfflineDefault;
};

class MSG_IMAPHostTable : public XPPtrArray{
public:
    MSG_IMAPHostTable(MSG_Master* master);
    ~MSG_IMAPHostTable();

	MSG_IMAPHost* GetHost(int i) 
	{
		return (MSG_IMAPHost *) GetAt(i);
	}

	void UpdatePrefs(const char *prefName);
	MSG_IMAPHost* GetDefaultHost(XP_Bool createIfMissing = FALSE);

	int32 GetHostList(MSG_IMAPHost** result, int32 resultsize);

	MSG_IMAPHost *AddIMAPHost(const char* name, XP_Bool secure, 
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespace,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir,
										XP_Bool writePrefs);

	MSG_IMAPHost *FindIMAPHost (MSG_FolderInfo *container);
	MSG_IMAPHost *FindIMAPHost (const char* name);

	int		DeleteIMAPHost(MSG_IMAPHost* host);
	void	ReorderIMAPHost(MSG_IMAPHost *host, MSG_IMAPHost *afterHost /* NULL = pos 0 */);
	void	WriteServerList();

protected:
	MSG_Master* m_master;
};

#endif
