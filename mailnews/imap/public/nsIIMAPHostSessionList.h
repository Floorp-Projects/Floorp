/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsIImapHostSessionList_H_
#define _nsIImapHostSessionList_H_

#include "nsISupports.h"
#include "nsImapCore.h"

class nsIMAPBodyShellCache;
class nsIMAPBodyShell;
class nsIImapIncomingServer;

// 2a8e21fe-e3c4-11d2-a504-0060b0fc04b7

#define NS_IIMAPHOSTSESSIONLIST_IID							\
{ 0x2a8e21fe, 0xe3c4, 0x11d2, {0xa5, 0x04, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// this is an interface to a linked list of host info's    
class nsIImapHostSessionList : public nsISupports
{
public:
   static const nsIID& GetIID() { static nsIID iid = NS_IIMAPHOSTSESSIONLIST_IID; return iid; }

	// Host List
	 NS_IMETHOD	AddHostToList(const char *serverKey, nsIImapIncomingServer *server) = 0;
	 NS_IMETHOD ResetAll() = 0;

	// Capabilities
	 NS_IMETHOD	GetCapabilityForHost(const char *serverKey, PRUint32 &result) = 0;
	 NS_IMETHOD	SetCapabilityForHost(const char *serverKey, PRUint32 capability) = 0;
	 NS_IMETHOD	GetHostHasAdminURL(const char *serverKey, PRBool &result) = 0;
	 NS_IMETHOD	SetHostHasAdminURL(const char *serverKey, PRBool hasAdminUrl) = 0;
	// Subscription
	 NS_IMETHOD	GetHostIsUsingSubscription(const char *serverKey, PRBool &result) = 0;
	 NS_IMETHOD	SetHostIsUsingSubscription(const char *serverKey, PRBool usingSubscription) = 0;

	// Passwords
	 NS_IMETHOD	GetPasswordForHost(const char *serverKey, nsString &result) = 0;
	 NS_IMETHOD	SetPasswordForHost(const char *serverKey, const char *password) = 0;
	 NS_IMETHOD GetPasswordVerifiedOnline(const char *serverKey, PRBool &result) = 0;
	 NS_IMETHOD	SetPasswordVerifiedOnline(const char *serverKey) = 0;

    // OnlineDir
    NS_IMETHOD GetOnlineDirForHost(const char *serverKey,
                                   nsString &result) = 0;
    NS_IMETHOD SetOnlineDirForHost(const char *serverKey,
                                   const char *onlineDir) = 0;

    // Delete is move to trash folder
    NS_IMETHOD GetDeleteIsMoveToTrashForHost(const char *serverKey, PRBool &result) = 0;
    NS_IMETHOD SetDeleteIsMoveToTrashForHost(const char *serverKey, PRBool isMoveToTrash)
        = 0;
	NS_IMETHOD GetShowDeletedMessagesForHost(const char *serverKey, PRBool &result) = 0;

    NS_IMETHOD SetShowDeletedMessagesForHost(const char *serverKey, PRBool showDeletedMessages)
        = 0;

    // Get namespaces
    NS_IMETHOD GetGotNamespacesForHost(const char *serverKey, PRBool &result) = 0;
    NS_IMETHOD SetGotNamespacesForHost(const char *serverKey, PRBool gotNamespaces) = 0;

	// Folders
	 NS_IMETHOD	SetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool discovered) = 0;
	 NS_IMETHOD	GetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool &result) = 0;

	// Trash Folder
	 NS_IMETHOD	SetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool exists) = 0;
	 NS_IMETHOD	GetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool &result) = 0;
	
	// INBOX
	 NS_IMETHOD		GetOnlineInboxPathForHost(const char *serverKey, nsString &result) = 0;
	 NS_IMETHOD		GetShouldAlwaysListInboxForHost(const char *serverKey, PRBool &result) = 0;
	 NS_IMETHOD		SetShouldAlwaysListInboxForHost(const char *serverKey, PRBool shouldList) = 0;

	// Namespaces
	 NS_IMETHOD		GetNamespaceForMailboxForHost(const char *serverKey, const char *mailbox_name, nsIMAPNamespace * & result) = 0;
	 NS_IMETHOD		SetNamespaceFromPrefForHost(const char *serverKey, const char *namespacePref, EIMAPNamespaceType type) = 0;
	 NS_IMETHOD		AddNewNamespaceForHost(const char *serverKey, nsIMAPNamespace *ns) = 0;
	 NS_IMETHOD		ClearServerAdvertisedNamespacesForHost(const char *serverKey) = 0;
	 NS_IMETHOD		ClearPrefsNamespacesForHost(const char *serverKey) = 0;
	 NS_IMETHOD		GetDefaultNamespaceOfTypeForHost(const char *serverKey, EIMAPNamespaceType type, nsIMAPNamespace * & result) = 0;
	 NS_IMETHOD		SetNamespacesOverridableForHost(const char *serverKey, PRBool overridable) = 0;
	 NS_IMETHOD		GetNamespacesOverridableForHost(const char *serverKey,PRBool &result) = 0;
	 NS_IMETHOD		GetNumberOfNamespacesForHost(const char *serverKey, PRUint32 &result) = 0;
	 NS_IMETHOD		GetNamespaceNumberForHost(const char *serverKey, PRInt32 n, nsIMAPNamespace * &result) = 0;
	 // ### dmb hoo boy, how are we going to do this?
	 NS_IMETHOD		CommitNamespacesForHost(nsIImapIncomingServer *server) = 0;
	 NS_IMETHOD		FlushUncommittedNamespacesForHost(const char *serverKey, PRBool &result) = 0;


	// Hierarchy Delimiters
	 NS_IMETHOD		AddHierarchyDelimiter(const char *serverKey, char delimiter) = 0;
	 NS_IMETHOD		GetHierarchyDelimiterStringForHost(const char *serverKey, nsString &result) = 0;
	 NS_IMETHOD		SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *serverKey, const char *boxName, char delimiter) = 0;

	// Message Body Shells
	 NS_IMETHOD		AddShellToCacheForHost(const char *serverKey, nsIMAPBodyShell *shell) = 0;
	 NS_IMETHOD		FindShellInCacheForHost(const char *serverKey, const char *mailboxName, const char *UID, IMAP_ContentModifiedType modType, nsIMAPBodyShell	**result) = 0;

};

#endif
