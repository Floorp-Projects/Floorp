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

#ifndef _nsIImapHostSessionList_H_
#define _nsIImapHostSessionList_H_

#include "nsISupports.h"
#include "nsImapCore.h"

class nsIMAPBodyShellCache;
class nsIMAPBodyShell;

// 2a8e21fe-e3c4-11d2-a504-0060b0fc04b7

#define NS_IIMAPHOSTSESSIONLIST_IID							\
{ 0x2a8e21fe, 0xe3c4, 0x11d2, {0xa5, 0x04, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

//479ce8fc-e725-11d2-a505-0060b0fc04b7
#define NS_IIMAPHOSTSESSIONLIST_CID							\
{ 0x479ce8fc, 0xe725, 0x11d2, {0xa5, 0x05, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// this is an interface to a linked list of host info's    
class nsIImapHostSessionList : public nsISupports
{
public:

	// Host List
	 NS_IMETHOD	AddHostToList(const char *hostname, const char *userName) = 0;
	 NS_IMETHOD ResetAll() = 0;

	// Capabilities
	 NS_IMETHOD	GetCapabilityForHost(const char *hostName, const char *userName, PRUint32 &result) = 0;
	 NS_IMETHOD	SetCapabilityForHost(const char *hostname, const char *userName, PRUint32 capability) = 0;
	 NS_IMETHOD	GetHostHasAdminURL(const char *hostname, const char *userName, PRBool &result) = 0;
	 NS_IMETHOD	SetHostHasAdminURL(const char *hostname, const char *userName, PRBool hasAdminUrl) = 0;
	// Subscription
	 NS_IMETHOD	GetHostIsUsingSubscription(const char *hostname, const char *userName, PRBool &result) = 0;
	 NS_IMETHOD	SetHostIsUsingSubscription(const char *hostname, const char *userName, PRBool usingSubscription) = 0;

	// Passwords
	 NS_IMETHOD	GetPasswordForHost(const char *hostname, const char *userName, nsString &result) = 0;
	 NS_IMETHOD	SetPasswordForHost(const char *hostname, const char *userName, const char *password) = 0;
	 NS_IMETHOD GetPasswordVerifiedOnline(const char *hostname, const char *userName, PRBool &result) = 0;
	 NS_IMETHOD	SetPasswordVerifiedOnline(const char *hostName, const char *userName) = 0;

	// Folders
	 NS_IMETHOD	SetHaveWeEverDiscoveredFoldersForHost(const char *hostname, const char *userName, PRBool discovered) = 0;
	 NS_IMETHOD	GetHaveWeEverDiscoveredFoldersForHost(const char *hostname, const char *userName, PRBool &result) = 0;

	// Trash Folder
	 NS_IMETHOD	SetOnlineTrashFolderExistsForHost(const char *hostname, const char *userName, PRBool exists) = 0;
	 NS_IMETHOD	GetOnlineTrashFolderExistsForHost(const char *hostname, const char *userName, PRBool &result) = 0;
	
	// INBOX
	 NS_IMETHOD		GetOnlineInboxPathForHost(const char *hostname, const char *userName, nsString &result) = 0;
	 NS_IMETHOD		GetShouldAlwaysListInboxForHost(const char *hostname, const char *userName, PRBool &result) = 0;
	 NS_IMETHOD		SetShouldAlwaysListInboxForHost(const char *hostname, const char *userName, PRBool shouldList) = 0;

	// Namespaces
	 NS_IMETHOD		GetNamespaceForMailboxForHost(const char *hostname, const char *userName, const char *mailbox_name, nsIMAPNamespace * & result) = 0;
	 NS_IMETHOD		AddNewNamespaceForHost(const char *hostname, const char *userName, nsIMAPNamespace *ns) = 0;
	 NS_IMETHOD		ClearServerAdvertisedNamespacesForHost(const char *hostName, const char *userName) = 0;
	 NS_IMETHOD		ClearPrefsNamespacesForHost(const char *hostName, const char *userName) = 0;
	 NS_IMETHOD		GetDefaultNamespaceOfTypeForHost(const char *hostname, const char *userName, EIMAPNamespaceType type, nsIMAPNamespace * & result) = 0;
	 NS_IMETHOD		SetNamespacesOverridableForHost(const char *hostname, const char *userName, PRBool overridable) = 0;
	 NS_IMETHOD		GetNamespacesOverridableForHost(const char *hostname, const char *userName,PRBool &result) = 0;
	 NS_IMETHOD		GetNumberOfNamespacesForHost(const char *hostname, const char *userName, PRUint32 &result) = 0;
	 NS_IMETHOD		GetNamespaceNumberForHost(const char *hostname, const char *userName, PRInt32 n, nsIMAPNamespace * &result) = 0;
	 // ### dmb hoo boy, how are we going to do this?
	 NS_IMETHOD		CommitNamespacesForHost(const char *hostname, const char *userName) = 0;
	 NS_IMETHOD		FlushUncommittedNamespacesForHost(const char *hostName, const char *userName, PRBool &result) = 0;


	// Hierarchy Delimiters
	 NS_IMETHOD		AddHierarchyDelimiter(const char *hostname, const char *userName, char delimiter) = 0;
	 NS_IMETHOD		GetHierarchyDelimiterStringForHost(const char *hostname, const char *userName, nsString &result) = 0;
	 NS_IMETHOD		SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *hostname, const char *userName, const char *boxName, char delimiter) = 0;

	// Message Body Shells
	 NS_IMETHOD		AddShellToCacheForHost(const char *hostname, const char *userName, nsIMAPBodyShell *shell) = 0;
	 NS_IMETHOD		FindShellInCacheForHost(const char *hostname, const char *userName, const char *mailboxName, const char *UID, nsIMAPBodyShell	&result) = 0;

};

#endif
