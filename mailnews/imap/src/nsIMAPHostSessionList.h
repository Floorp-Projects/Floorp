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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsIMAPHostSessionList_H_
#define _nsIMAPHostSessionList_H_

#include "nsImapCore.h"
#include "nsIIMAPHostSessionList.h"
#include "nspr.h"

class nsIMAPNamespaceList;
class nsIImapIncomingServer;

class nsIMAPHostInfo
{
public:
	friend class nsIMAPHostSessionList;

	nsIMAPHostInfo(const char *serverKey, nsIImapIncomingServer *server);
	~nsIMAPHostInfo();
protected:
	char			*fServerKey;
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
	PRBool			fShowDeletedMessages;
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
	 NS_IMETHOD	AddHostToList(const char *serverKey, 
                              nsIImapIncomingServer *server);
	 NS_IMETHOD ResetAll();

	// Capabilities
	 NS_IMETHOD	GetCapabilityForHost(const char *serverKey, PRUint32 &result);
	 NS_IMETHOD	SetCapabilityForHost(const char *serverKey, PRUint32 capability);
	 NS_IMETHOD	GetHostHasAdminURL(const char *serverKey, PRBool &result);
	 NS_IMETHOD	SetHostHasAdminURL(const char *serverKey, PRBool hasAdminUrl);
	// Subscription
	 NS_IMETHOD	GetHostIsUsingSubscription(const char *serverKey, PRBool &result);
	 NS_IMETHOD	SetHostIsUsingSubscription(const char *serverKey, PRBool usingSubscription);

	// Passwords
	 NS_IMETHOD	GetPasswordForHost(const char *serverKey, nsString &result);
	 NS_IMETHOD	SetPasswordForHost(const char *serverKey, const char *password);
	 NS_IMETHOD GetPasswordVerifiedOnline(const char *serverKey, PRBool &result);
	 NS_IMETHOD	SetPasswordVerifiedOnline(const char *serverKey);

    // OnlineDir
    NS_IMETHOD GetOnlineDirForHost(const char *serverKey,
                                   nsString &result);
    NS_IMETHOD SetOnlineDirForHost(const char *serverKey,
                                   const char *onlineDir);

    // Delete is move to trash folder
    NS_IMETHOD GetDeleteIsMoveToTrashForHost(const char *serverKey, PRBool &result);
    NS_IMETHOD SetDeleteIsMoveToTrashForHost(const char *serverKey, PRBool isMoveToTrash);
	// imap delete model (or not)
	NS_IMETHOD GetShowDeletedMessagesForHost(const char *serverKey, PRBool &result);
    NS_IMETHOD SetShowDeletedMessagesForHost(const char *serverKey, PRBool showDeletedMessages);

    // Get namespaces
    NS_IMETHOD GetGotNamespacesForHost(const char *serverKey, PRBool &result);
    NS_IMETHOD SetGotNamespacesForHost(const char *serverKey, PRBool gotNamespaces);
	// Folders
	 NS_IMETHOD	SetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool discovered);
	 NS_IMETHOD	GetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool &result);

	// Trash Folder
	 NS_IMETHOD	SetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool exists);
	 NS_IMETHOD	GetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool &result);
	
	// INBOX
	 NS_IMETHOD		GetOnlineInboxPathForHost(const char *serverKey, nsString &result);
	 NS_IMETHOD		GetShouldAlwaysListInboxForHost(const char *serverKey, PRBool &result);
	 NS_IMETHOD		SetShouldAlwaysListInboxForHost(const char *serverKey, PRBool shouldList);

	// Namespaces
	 NS_IMETHOD		GetNamespaceForMailboxForHost(const char *serverKey, const char *mailbox_name, nsIMAPNamespace *&result);
	 NS_IMETHOD		SetNamespaceFromPrefForHost(const char *serverKey, const char *namespacePref, EIMAPNamespaceType type);
	 NS_IMETHOD		AddNewNamespaceForHost(const char *serverKey, nsIMAPNamespace *ns);
	 NS_IMETHOD		ClearServerAdvertisedNamespacesForHost(const char *serverKey);
	 NS_IMETHOD		ClearPrefsNamespacesForHost(const char *serverKey);
	 NS_IMETHOD		GetDefaultNamespaceOfTypeForHost(const char *serverKey, EIMAPNamespaceType type, nsIMAPNamespace *&result);
	 NS_IMETHOD		SetNamespacesOverridableForHost(const char *serverKey, PRBool overridable);
	 NS_IMETHOD		GetNamespacesOverridableForHost(const char *serverKey,PRBool &result);
	 NS_IMETHOD		GetNumberOfNamespacesForHost(const char *serverKey, PRUint32 &result);
	 NS_IMETHOD		GetNamespaceNumberForHost(const char *serverKey, PRInt32 n, nsIMAPNamespace * &result);
	 // ### dmb hoo boy, how are we going to do this?
	 NS_IMETHOD		CommitNamespacesForHost(nsIImapIncomingServer *host);
	 NS_IMETHOD		FlushUncommittedNamespacesForHost(const char *serverKey, PRBool &result);

	// Hierarchy Delimiters
	 NS_IMETHOD		AddHierarchyDelimiter(const char *serverKey, char delimiter);
	 NS_IMETHOD		GetHierarchyDelimiterStringForHost(const char *serverKey, nsString &result);
	 NS_IMETHOD		SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *serverKey, const char *boxName, char delimiter);

	// Message Body Shells
	 NS_IMETHOD		AddShellToCacheForHost(const char *serverKey, nsIMAPBodyShell *shell);
	 NS_IMETHOD		FindShellInCacheForHost(const char *serverKey, const char *mailboxName, const char *UID, IMAP_ContentModifiedType modType, nsIMAPBodyShell	**result);

	PRMonitor		*gCachedHostInfoMonitor;
	nsIMAPHostInfo	*fHostInfoList;
protected:
	nsresult SetNamespacesPrefForHost(nsIImapIncomingServer *aHost, EIMAPNamespaceType type, char *pref);

	nsIMAPHostInfo *FindHost(const char *serverKey);
};


#endif
