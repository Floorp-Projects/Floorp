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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMAPHostSessionList_H_
#define _nsIMAPHostSessionList_H_

#include "nsImapCore.h"
#include "nsIIMAPHostSessionList.h"
#include "nspr.h"

class nsIMAPNamespaceList;

class nsIMAPHostInfo
{
public:
	friend class nsIMAPHostSessionList;

	nsIMAPHostInfo(const char *hostName, const char *userName);
	~nsIMAPHostInfo();
protected:
	char			*fHostName;
	char			*fUserName;
	char			*fCachedPassword;
	char			*fOnlineDir;
	nsIMAPHostInfo	*fNextHost;
	PRUint32		fCapabilityFlags;
	char			*fHierarchyDelimiters;	// string of top-level hierarchy delimiters
	PRBool			fHaveWeEverDiscoveredFolders;
	char			*fCanonicalOnlineSubDir;
	nsIMAPNamespaceList			*fNamespaceList, *fTempNamespaceList;
	PRBool			fNamespacesOverridable;
	PRBool			fUsingSubscription;
	PRBool			fOnlineTrashFolderExists;
	PRBool			fShouldAlwaysListInbox;
	PRBool			fHaveAdminURL;
	PRBool			fPasswordVerifiedOnline;
	PRBool			fDeleteIsMoveToTrash;
	PRBool			fGotNamespaces;
	nsIMAPBodyShellCache	*fShellCache;
};

// this is an interface to a linked list of host info's    
class nsIMAPHostSessionList : public nsIImapHostSessionList
{
public:

	NS_DECL_ISUPPORTS

	nsIMAPHostSessionList();
	virtual ~nsIMAPHostSessionList();
	// Host List
	 NS_IMETHOD	AddHostToList(const char *hostName, const char *userName);
	 NS_IMETHOD ResetAll();

	// Capabilities
	 NS_IMETHOD	GetCapabilityForHost(const char *hostName, const char *userName, PRUint32 &result);
	 NS_IMETHOD	SetCapabilityForHost(const char *hostName, const char *userName, PRUint32 capability);
	 NS_IMETHOD	GetHostHasAdminURL(const char *hostName, const char *userName, PRBool &result);
	 NS_IMETHOD	SetHostHasAdminURL(const char *hostName, const char *userName, PRBool hasAdminUrl);
	// Subscription
	 NS_IMETHOD	GetHostIsUsingSubscription(const char *hostName, const char *userName, PRBool &result);
	 NS_IMETHOD	SetHostIsUsingSubscription(const char *hostName, const char *userName, PRBool usingSubscription);

	// Passwords
	 NS_IMETHOD	GetPasswordForHost(const char *hostName, const char *userName, nsString &result);
	 NS_IMETHOD	SetPasswordForHost(const char *hostName, const char *userName, const char *password);
	 NS_IMETHOD GetPasswordVerifiedOnline(const char *hostName, const char *userName, PRBool &result);
	 NS_IMETHOD	SetPasswordVerifiedOnline(const char *hostName, const char *userName);

    // OnlineDir
    NS_IMETHOD GetOnlineDirForHost(const char *hostName, const char *userName,
                                   nsString &result);
    NS_IMETHOD SetOnlineDirForHost(const char *hostName, const char *userName,
                                   const char *onlineDir);

    // Delete is move to trash folder
    NS_IMETHOD GetDeleteIsMoveToTrashForHost(const char *hostName, const char
                                             *userName, PRBool &result);
    NS_IMETHOD SetDeleteIsMoveToTrashForHost(const char *hostName, const char
                                             *userName, PRBool isMoveToTrash);

    // Get namespaces
    NS_IMETHOD GetGotNamespacesForHost(const char *hostName, const char
                                       *userName, PRBool &result);
    NS_IMETHOD SetGotNamespacesForHost(const char *hostName, const char
                                       *userName, PRBool gotNamespaces);
	// Folders
	 NS_IMETHOD	SetHaveWeEverDiscoveredFoldersForHost(const char *hostName, const char *userName, PRBool discovered);
	 NS_IMETHOD	GetHaveWeEverDiscoveredFoldersForHost(const char *hostName, const char *userName, PRBool &result);

	// Trash Folder
	 NS_IMETHOD	SetOnlineTrashFolderExistsForHost(const char *hostName, const char *userName, PRBool exists);
	 NS_IMETHOD	GetOnlineTrashFolderExistsForHost(const char *hostName, const char *userName, PRBool &result);
	
	// INBOX
	 NS_IMETHOD		GetOnlineInboxPathForHost(const char *hostName, const char *userName, nsString &result);
	 NS_IMETHOD		GetShouldAlwaysListInboxForHost(const char *hostName, const char *userName, PRBool &result);
	 NS_IMETHOD		SetShouldAlwaysListInboxForHost(const char *hostName, const char *userName, PRBool shouldList);

	// Namespaces
	 NS_IMETHOD		GetNamespaceForMailboxForHost(const char *hostName, const char *userName, const char *mailbox_name, nsIMAPNamespace *&result);
	 NS_IMETHOD		AddNewNamespaceForHost(const char *hostName, const char *userName, nsIMAPNamespace *ns);
	 NS_IMETHOD		ClearServerAdvertisedNamespacesForHost(const char *hostName, const char *userName);
	 NS_IMETHOD		ClearPrefsNamespacesForHost(const char *hostName, const char *userName);
	 NS_IMETHOD		GetDefaultNamespaceOfTypeForHost(const char *hostName, const char *userName, EIMAPNamespaceType type, nsIMAPNamespace *&result);
	 NS_IMETHOD		SetNamespacesOverridableForHost(const char *hostName, const char *userName, PRBool overridable);
	 NS_IMETHOD		GetNamespacesOverridableForHost(const char *hostName, const char *userName,PRBool &result);
	 NS_IMETHOD		GetNumberOfNamespacesForHost(const char *hostName, const char *userName, PRUint32 &result);
	 NS_IMETHOD		GetNamespaceNumberForHost(const char *hostName, const char *userName, PRInt32 n, nsIMAPNamespace * &result);
	 // ### dmb hoo boy, how are we going to do this?
	 NS_IMETHOD		CommitNamespacesForHost(const char *hostName, const char *userName);
	 NS_IMETHOD		FlushUncommittedNamespacesForHost(const char *hostName, const char *userName, PRBool &result);

	// Hierarchy Delimiters
	 NS_IMETHOD		AddHierarchyDelimiter(const char *hostName, const char *userName, char delimiter);
	 NS_IMETHOD		GetHierarchyDelimiterStringForHost(const char *hostName, const char *userName, nsString &result);
	 NS_IMETHOD		SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *hostName, const char *userName, const char *boxName, char delimiter);

	// Message Body Shells
	 NS_IMETHOD		AddShellToCacheForHost(const char *hostName, const char *userName, nsIMAPBodyShell *shell);
	 NS_IMETHOD		FindShellInCacheForHost(const char *hostName, const char *userName, const char *mailboxName, const char *UID, nsIMAPBodyShell	&result);

	PRMonitor		*gCachedHostInfoMonitor;
	nsIMAPHostInfo	*fHostInfoList;
protected:
	nsIMAPHostInfo *FindHost(const char *hostName, const char *userName);
};

#endif
