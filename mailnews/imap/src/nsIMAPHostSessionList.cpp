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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsIMAPHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsIMAPNamespace.h"
#include "nsISupportsUtils.h"
#include "nsIImapIncomingServer.h"
#include "nsCOMPtr.h"
#include "nsIMsgIncomingServer.h"

nsIMAPHostInfo::nsIMAPHostInfo(const char *serverKey, 
                               nsIImapIncomingServer *server)
{
	fServerKey = nsCRT::strdup(serverKey);
    NS_ASSERTION(server, "*** Fatal null imap incoming server...\n");
    fOnlineDir = NULL;
    server->GetServerDirectory(&fOnlineDir);
	fNextHost = NULL;
	fCachedPassword = NULL;
	fCapabilityFlags = kCapabilityUndefined;
	fHierarchyDelimiters = NULL;
	fHaveWeEverDiscoveredFolders = PR_FALSE;
	fCanonicalOnlineSubDir = NULL;
	fNamespaceList = nsIMAPNamespaceList::CreatensIMAPNamespaceList();
	fUsingSubscription = PR_TRUE;
    server->GetUsingSubscription(&fUsingSubscription);
	fOnlineTrashFolderExists = PR_FALSE;
	fShouldAlwaysListInbox = PR_TRUE;
	fShellCache = nsIMAPBodyShellCache::Create();
	fPasswordVerifiedOnline = PR_FALSE;
    fDeleteIsMoveToTrash = PR_TRUE;
	fShowDeletedMessages = PR_FALSE;
    fGotNamespaces = PR_FALSE;
	fHaveAdminURL = PR_FALSE;
	fNamespacesOverridable = PR_TRUE;
    server->GetOverrideNamespaces(&fNamespacesOverridable);
	fTempNamespaceList = nsIMAPNamespaceList::CreatensIMAPNamespaceList();
}

nsIMAPHostInfo::~nsIMAPHostInfo()
{
	PR_FREEIF(fServerKey);
	PR_FREEIF(fCachedPassword);
	PR_FREEIF(fHierarchyDelimiters);
    PR_FREEIF(fOnlineDir);
	delete fNamespaceList;
	delete fTempNamespaceList;
	delete fShellCache;
}

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ADDREF(nsIMAPHostSessionList)
NS_IMPL_THREADSAFE_RELEASE(nsIMAPHostSessionList)

NS_IMETHODIMP nsIMAPHostSessionList::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
        
  *aInstancePtr = NULL;
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID); 

  if (aIID.Equals(NS_GET_IID(nsIImapHostSessionList)))
  {
	  *aInstancePtr = (nsIImapHostSessionList *) this;
	  NS_ADDREF_THIS();
	  return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
	  *aInstancePtr = (void*) ((nsISupports*)this);
	  NS_ADDREF_THIS();
    return NS_OK;                                                        
  }                                                                      
  
  if (aIID.Equals(kIsThreadsafeIID)) 
  {
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


nsIMAPHostSessionList::nsIMAPHostSessionList()
{
    NS_INIT_REFCNT ();
	gCachedHostInfoMonitor = PR_NewMonitor(/* "accessing-hostlist-monitor"*/);
	fHostInfoList = nsnull;
}

nsIMAPHostSessionList::~nsIMAPHostSessionList()
{
	ResetAll();
	PR_DestroyMonitor(gCachedHostInfoMonitor);
}

nsIMAPHostInfo *nsIMAPHostSessionList::FindHost(const char *serverKey)
{
	nsIMAPHostInfo *host;

	// ### should also check userName here, if NON NULL
	for (host = fHostInfoList; host; host = host->fNextHost)
	{
		if (!PL_strcasecmp(serverKey, host->fServerKey)) 
			return host;
	}
	return host;
}

// reset any cached connection info - delete the lot of 'em
NS_IMETHODIMP nsIMAPHostSessionList::ResetAll()
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *nextHost = NULL;
	for (nsIMAPHostInfo *host = fHostInfoList; host; host = nextHost)
	{
		nextHost = host->fNextHost;
		delete host;
	}
	fHostInfoList = NULL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return NS_OK;
}

NS_IMETHODIMP
nsIMAPHostSessionList::AddHostToList(const char *serverKey,
                                     nsIImapIncomingServer *server)
{
	nsIMAPHostInfo *newHost=NULL;
	PR_EnterMonitor(gCachedHostInfoMonitor);
	if (!FindHost(serverKey))
	{
		// stick it on the front
		newHost = new nsIMAPHostInfo(serverKey, server);
		if (newHost)
		{
			newHost->fNextHost = fHostInfoList;
			fHostInfoList = newHost;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (newHost == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetCapabilityForHost(const char *serverKey, PRUint32 &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);

	nsIMAPHostInfo *host = FindHost(serverKey);
	result = (host) ? host->fCapabilityFlags : 0;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetCapabilityForHost(const char *serverKey, PRUint32 capability)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fCapabilityFlags = capability;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetPasswordForHost(const char *serverKey, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result.AssignWithConversion(host->fCachedPassword);
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetPasswordForHost(const char *serverKey, const char *password)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		PR_FREEIF(host->fCachedPassword);
		if (password)
			host->fCachedPassword = nsCRT::strdup(password);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetPasswordVerifiedOnline(const char *serverKey)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fPasswordVerifiedOnline = PR_TRUE;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetPasswordVerifiedOnline(const char *serverKey, PRBool &result)
{

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fPasswordVerifiedOnline;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetOnlineDirForHost(const char *serverKey,
                                                         nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
        result.AssignWithConversion(host->fOnlineDir);
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetOnlineDirForHost(const char *serverKey,
                                                         const char *onlineDir)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		PR_FREEIF(host->fOnlineDir);
		if (onlineDir)
			host->fOnlineDir = nsCRT::strdup(onlineDir);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetDeleteIsMoveToTrashForHost(
    const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
        result = host->fDeleteIsMoveToTrash;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetShowDeletedMessagesForHost(
    const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
        result = host->fShowDeletedMessages;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetDeleteIsMoveToTrashForHost(
    const char *serverKey, PRBool isMoveToTrash)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fDeleteIsMoveToTrash = isMoveToTrash;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetShowDeletedMessagesForHost(
    const char *serverKey, PRBool showDeletedMessages)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fShowDeletedMessages = showDeletedMessages;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetGotNamespacesForHost(
    const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
        result = host->fGotNamespaces;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetGotNamespacesForHost(
    const char *serverKey, PRBool gotNamespaces)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fGotNamespaces = gotNamespaces;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetHierarchyDelimiterStringForHost(const char *serverKey, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result.AssignWithConversion(host->fHierarchyDelimiters);
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::AddHierarchyDelimiter(const char *serverKey, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		if (!host->fHierarchyDelimiters)
		{
			host->fHierarchyDelimiters = PR_smprintf("%c",delimiter);
		}
		else if (!PL_strchr(host->fHierarchyDelimiters, delimiter))
		{
			char *tmpDelimiters = PR_smprintf("%s%c",host->fHierarchyDelimiters,delimiter);
			PR_FREEIF(host->fHierarchyDelimiters);
			host->fHierarchyDelimiters = tmpDelimiters;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetHostIsUsingSubscription(const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fUsingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHostIsUsingSubscription(const char *serverKey, PRBool usingSubscription)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fUsingSubscription = usingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetHostHasAdminURL(const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fHaveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHostHasAdminURL(const char *serverKey, PRBool haveAdminURL)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fHaveAdminURL = haveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::GetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fHaveWeEverDiscoveredFolders;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHaveWeEverDiscoveredFoldersForHost(const char *serverKey, PRBool discovered)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fHaveWeEverDiscoveredFolders = discovered;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool exists)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fOnlineTrashFolderExists = exists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetOnlineTrashFolderExistsForHost(const char *serverKey, PRBool &result)
{

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fOnlineTrashFolderExists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::AddNewNamespaceForHost(const char *serverKey, nsIMAPNamespace *ns)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		host->fNamespaceList->AddNewNamespace(ns);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetNamespaceFromPrefForHost(const char *serverKey, 
																 const char *namespacePref, EIMAPNamespaceType nstype)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		if (namespacePref)
		{
			int numNamespaces = host->fNamespaceList->UnserializeNamespaces(namespacePref, nsnull, 0);
			char **prefixes = (char**) PR_CALLOC(numNamespaces * sizeof(char*));
			if (prefixes)
			{
				int len = host->fNamespaceList->UnserializeNamespaces(namespacePref, prefixes, numNamespaces);
				for (int i = 0; i < len; i++)
				{
					char *thisns = prefixes[i];
					char delimiter = '/';	// a guess
					if (PL_strlen(thisns) >= 1)
						delimiter = thisns[PL_strlen(thisns)-1];
					nsIMAPNamespace *ns = new nsIMAPNamespace(nstype, thisns, delimiter, PR_TRUE);
					if (ns)
						host->fNamespaceList->AddNewNamespace(ns);
					PR_FREEIF(thisns);
				}
				PR_Free(prefixes);
			}
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetNamespaceForMailboxForHost(const char *serverKey, const char *mailbox_name, nsIMAPNamespace * &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		result = host->fNamespaceList->GetNamespaceForMailbox(mailbox_name);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::ClearPrefsNamespacesForHost(const char *serverKey)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(PR_TRUE, PR_FALSE, PR_TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::ClearServerAdvertisedNamespacesForHost(const char *serverKey)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(PR_FALSE, PR_TRUE, PR_TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetDefaultNamespaceOfTypeForHost(const char *serverKey, EIMAPNamespaceType type, nsIMAPNamespace * &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		result = host->fNamespaceList->GetDefaultNamespaceOfType(type);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetNamespacesOverridableForHost(const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		result = host->fNamespacesOverridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetNamespacesOverridableForHost(const char *serverKey, PRBool overridable)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fNamespacesOverridable = overridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetNumberOfNamespacesForHost(const char *serverKey, PRUint32 &result)
{
	PRInt32 intResult = 0;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		intResult = host->fNamespaceList->GetNumberOfNamespaces();
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	NS_ASSERTION(intResult >= 0, "negative number of namespaces");
	result = (PRUint32) intResult;
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP	nsIMAPHostSessionList::GetNamespaceNumberForHost(const char *serverKey, PRInt32 n, nsIMAPNamespace * &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		result = host->fNamespaceList->GetNamespaceNumber(n);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

nsresult nsIMAPHostSessionList::SetNamespacesPrefForHost(nsIImapIncomingServer *aHost, EIMAPNamespaceType type, char *pref)
{
	if (type == kPersonalNamespace)
		aHost->SetPersonalNamespace(pref);
	else if (type == kPublicNamespace)
		aHost->SetPublicNamespace(pref);
	else if (type == kOtherUsersNamespace)
		aHost->SetOtherUsersNamespace(pref);
	else
		NS_ASSERTION(PR_FALSE, "bogus namespace type");
	return NS_OK;

}
// do we need this? What should we do about the master thing?
// Make sure this is running in the Mozilla thread when called
NS_IMETHODIMP nsIMAPHostSessionList::CommitNamespacesForHost(nsIImapIncomingServer *aHost)
{
	char * serverKey = nsnull;
	
	if (!aHost)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr <nsIMsgIncomingServer> incomingServer = do_QueryInterface(aHost);
	if (!incomingServer)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = incomingServer->GetKey(&serverKey);
	if (NS_FAILED(rv)) return rv;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		host->fGotNamespaces = PR_TRUE;	// so we only issue NAMESPACE once per host per session.
		EIMAPNamespaceType type = kPersonalNamespace;
		for (int i = 1; i <= 3; i++)
		{
			switch(i)
			{
			case 1:
				type = kPersonalNamespace;
				break;
			case 2:
				type = kPublicNamespace;
				break;
			case 3:
				type = kOtherUsersNamespace;
				break;
			default:
				type = kPersonalNamespace;
				break;
			}
			
			int32 numInNS = host->fNamespaceList->GetNumberOfNamespaces(type);
			if (numInNS == 0)
			{
				SetNamespacesPrefForHost(aHost, type, NULL);
			}
			else if (numInNS >= 1)
			{
				char *pref = PR_smprintf("");
				for (int count = 1; count <= numInNS; count++)
				{
					nsIMAPNamespace *ns = host->fNamespaceList->GetNamespaceNumber(count, type);
					if (ns)
					{
						if (count > 1)
						{
							// append the comma
							char *tempPref = PR_smprintf("%s,",pref);
							PR_FREEIF(pref);
							pref = tempPref;
						}
						char *tempPref = PR_smprintf("%s\"%s\"",pref,ns->GetPrefix());
						PR_FREEIF(pref);
						pref = tempPref;
					}
				}
				if (pref)
				{
					SetNamespacesPrefForHost(aHost, type, pref);
					PR_Free(pref);
				}
			}
		}
		// clear, but don't delete the entries in, the temp namespace list
		host->fTempNamespaceList->ClearNamespaces(PR_TRUE, PR_TRUE, PR_FALSE);
		
		// Now reset all of libmsg's namespace references.
		// Did I mention this needs to be running in the mozilla thread?
		aHost->ResetNamespaceReferences();
	}
	PR_FREEIF(serverKey);
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::FlushUncommittedNamespacesForHost(const char *serverKey, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		host->fTempNamespaceList->ClearNamespaces(PR_TRUE, PR_TRUE, PR_TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


// Returns NULL if there is no personal namespace on the given host
NS_IMETHODIMP nsIMAPHostSessionList::GetOnlineInboxPathForHost(const char *serverKey, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		nsIMAPNamespace *ns = NULL;
		ns = host->fNamespaceList->GetDefaultNamespaceOfType(kPersonalNamespace);
		if (ns)
		{
			result.AssignWithConversion(ns->GetPrefix());
            result.AppendWithConversion("INBOX");
		}
	}
	else
	{
		result.SetLength(0);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetShouldAlwaysListInboxForHost(const char* /*serverKey*/, PRBool &result)
{
	result = PR_TRUE;

	/*
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		ret = host->fShouldAlwaysListInbox;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	*/
	return NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetShouldAlwaysListInboxForHost(const char *serverKey, PRBool shouldList)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
		host->fShouldAlwaysListInbox = shouldList;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *serverKey, const char *boxName, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		nsIMAPNamespace *ns = host->fNamespaceList->GetNamespaceForMailbox(boxName);
		if (ns && !ns->GetIsDelimiterFilledIn())
		{
			ns->SetDelimiter(delimiter);
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::AddShellToCacheForHost(const char *serverKey, nsIMAPBodyShell *shell)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		if (host->fShellCache)
		{
			PRBool rv = host->fShellCache->AddShellToCache(shell);
			PR_ExitMonitor(gCachedHostInfoMonitor);
			return rv;
		}
		else
		{
			PR_ExitMonitor(gCachedHostInfoMonitor);
			return PR_FALSE;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::FindShellInCacheForHost(const char *serverKey, const char *mailboxName, const char *UID, 
                                                             IMAP_ContentModifiedType modType, nsIMAPBodyShell	**shell)
{
	nsCString uidString(UID);

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(serverKey);
	if (host)
	{
		if (host->fShellCache)
			*shell = host->fShellCache->FindShellForUID(uidString, mailboxName);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

