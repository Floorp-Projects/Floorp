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

#include "msgCore.h"
#include "nsIMAPHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsIMAPNamespace.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsISupportsUtils.h"


// **************?????***********????*************????***********************
// ***** IMPORTANT **** jefft -- this is a temporary implementation for the
// testing purpose. Eventually, we will have a host service object in
// controlling the host session list.
// Remove the following when the host service object is in place.
// **************************************************************************
nsIMAPHostSessionList*gImapHostSessionList = nsnull;

nsIMAPHostInfo::nsIMAPHostInfo(const char *hostName, const char *userName)
{
	fHostName = nsCRT::strdup(hostName);
	fUserName = nsCRT::strdup(userName);
	fNextHost = NULL;
	fCachedPassword = NULL;
	fCapabilityFlags = kCapabilityUndefined;
	fHierarchyDelimiters = NULL;
	fHaveWeEverDiscoveredFolders = FALSE;
	fCanonicalOnlineSubDir = NULL;
	fNamespaceList = nsIMAPNamespaceList::CreatensIMAPNamespaceList();
	fUsingSubscription = TRUE;
	fOnlineTrashFolderExists = FALSE;
	fShouldAlwaysListInbox = TRUE;
	fShellCache = nsIMAPBodyShellCache::Create();
	fPasswordVerifiedOnline = FALSE;
}

nsIMAPHostInfo::~nsIMAPHostInfo()
{
	PR_FREEIF(fHostName);
	PR_FREEIF(fUserName);
	PR_FREEIF(fCachedPassword);
	PR_FREEIF(fHierarchyDelimiters);
	delete fNamespaceList;
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

  if (aIID.Equals(nsIImapHostSessionList::GetIID()))
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
	PR_DestroyMonitor(gCachedHostInfoMonitor);
}

nsIMAPHostInfo *nsIMAPHostSessionList::FindHost(const char *hostName, const char *userName)
{
	nsIMAPHostInfo *host;

	// ### should also check userName here, if NON NULL
	for (host = fHostInfoList; host; host = host->fNextHost)
	{
		if (!PL_strcasecmp(hostName, host->fHostName))
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

NS_IMETHODIMP nsIMAPHostSessionList::AddHostToList(const char *hostName, const char *userName)
{
	nsIMAPHostInfo *newHost=NULL;
	PR_EnterMonitor(gCachedHostInfoMonitor);
	if (!FindHost(hostName, userName))
	{
		// stick it on the front
		newHost = new nsIMAPHostInfo(hostName, userName);
		if (newHost)
		{
			newHost->fNextHost = fHostInfoList;
			fHostInfoList = newHost;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (newHost == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetCapabilityForHost(const char *hostName, const char *userName, PRUint32 &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);

	nsIMAPHostInfo *host = FindHost(hostName, userName);
	result = (host) ? host->fCapabilityFlags : 0;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetCapabilityForHost(const char *hostName, const char *userName, PRUint32 capability)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fCapabilityFlags = capability;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetPasswordForHost(const char *hostName, const char *userName, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fCachedPassword;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetPasswordForHost(const char *hostName, const char *userName, const char *password)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		PR_FREEIF(host->fCachedPassword);
		if (password)
			host->fCachedPassword = nsCRT::strdup(password);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetPasswordVerifiedOnline(const char *hostName, const char *userName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fPasswordVerifiedOnline = TRUE;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetPasswordVerifiedOnline(const char *hostName, const char *userName, PRBool &result)
{

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fPasswordVerifiedOnline;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetHierarchyDelimiterStringForHost(const char *hostName, const char *userName, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fHierarchyDelimiters;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::AddHierarchyDelimiter(const char *hostName, const char *userName, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
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

NS_IMETHODIMP nsIMAPHostSessionList::GetHostIsUsingSubscription(const char *hostName, const char *userName, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fUsingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHostIsUsingSubscription(const char *hostName, const char *userName, PRBool usingSubscription)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fUsingSubscription = usingSubscription;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetHostHasAdminURL(const char *hostName, const char *userName, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fHaveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHostHasAdminURL(const char *hostName, const char *userName, PRBool haveAdminURL)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fHaveAdminURL = haveAdminURL;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::GetHaveWeEverDiscoveredFoldersForHost(const char *hostName, const char *userName, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fHaveWeEverDiscoveredFolders;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetHaveWeEverDiscoveredFoldersForHost(const char *hostName, const char *userName, PRBool discovered)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fHaveWeEverDiscoveredFolders = discovered;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetOnlineTrashFolderExistsForHost(const char *hostName, const char *userName, PRBool exists)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fOnlineTrashFolderExists = exists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetOnlineTrashFolderExistsForHost(const char *hostName, const char *userName, PRBool &result)
{

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fOnlineTrashFolderExists;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::AddNewNamespaceForHost(const char *hostName, const char *userName, nsIMAPNamespace *ns)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		host->fNamespaceList->AddNewNamespace(ns);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::GetNamespaceForMailboxForHost(const char *hostName, const char *userName, const char *mailbox_name, nsIMAPNamespace * &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		result = host->fNamespaceList->GetNamespaceForMailbox(mailbox_name);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::ClearPrefsNamespacesForHost(const char *hostName, const char *userName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(TRUE, FALSE, TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


NS_IMETHODIMP nsIMAPHostSessionList::ClearServerAdvertisedNamespacesForHost(const char *hostName, const char *userName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		host->fNamespaceList->ClearNamespaces(FALSE, TRUE, TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetDefaultNamespaceOfTypeForHost(const char *hostName, const char *userName, EIMAPNamespaceType type, nsIMAPNamespace * &result)
{
	nsIMAPNamespace *ret = NULL;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		result = host->fNamespaceList->GetDefaultNamespaceOfType(type);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetNamespacesOverridableForHost(const char *hostName, const char *userName, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		result = host->fNamespacesOverridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetNamespacesOverridableForHost(const char *hostName, const char *userName, PRBool overridable)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fNamespacesOverridable = overridable;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetNumberOfNamespacesForHost(const char *hostName, const char *userName, PRUint32 &result)
{
	PRInt32 intResult;

	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		intResult = host->fNamespaceList->GetNumberOfNamespaces();
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	NS_ASSERTION(intResult > 0, "negative number of namespaces");
	result = (PRUint32) intResult;
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP	nsIMAPHostSessionList::GetNamespaceNumberForHost(const char *hostName, const char *userName, PRInt32 n, nsIMAPNamespace * &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		result = host->fNamespaceList->GetNamespaceNumber(n);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

// do we need this? What should we do about the master thing?
// Make sure this is running in the Mozilla thread when called
NS_IMETHODIMP nsIMAPHostSessionList::CommitNamespacesForHost(const char *hostName, const char *userName)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
#if 0
	if (host)
	{
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
				MSG_SetNamespacePrefixes(master, host->fHostName, type, NULL);
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
					MSG_SetNamespacePrefixes(master, host->fHostName, type, pref);
					PR_FREE(pref);
				}
			}
		}
		// clear, but don't delete the entries in, the temp namespace list
		host->fTempNamespaceList->ClearNamespaces(TRUE, TRUE, FALSE);
		
		// Now reset all of libmsg's namespace references.
		// Did I mention this needs to be running in the mozilla thread?
		MSG_ResetNamespaceReferences(master, host->fHostName);
	}
#endif // ### DMB must figure out what to do about this.
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::FlushUncommittedNamespacesForHost(const char *hostName, const char *userName, PRBool &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		host->fTempNamespaceList->ClearNamespaces(TRUE, TRUE, TRUE);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}


// Returns NULL if there is no personal namespace on the given host
NS_IMETHODIMP nsIMAPHostSessionList::GetOnlineInboxPathForHost(const char *hostName, const char *userName, nsString &result)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		nsIMAPNamespace *ns = NULL;
		ns = host->fNamespaceList->GetDefaultNamespaceOfType(kPersonalNamespace);
		if (ns)
		{
			result = PR_smprintf("%sINBOX",ns->GetPrefix());
		}
	}
	else
	{
		result = "";
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::GetShouldAlwaysListInboxForHost(const char* /*hostName */, const char * /*userName*/, PRBool &result)
{
	result = TRUE;

	/*
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		ret = host->fShouldAlwaysListInbox;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	*/
	return NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetShouldAlwaysListInboxForHost(const char *hostName, const char *userName, PRBool shouldList)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
		host->fShouldAlwaysListInbox = shouldList;
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::SetNamespaceHierarchyDelimiterFromMailboxForHost(const char *hostName, const char *userName, const char *boxName, char delimiter)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
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

NS_IMETHODIMP nsIMAPHostSessionList::AddShellToCacheForHost(const char *hostName, const char *userName, nsIMAPBodyShell *shell)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
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
			return FALSE;
		}
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}

NS_IMETHODIMP nsIMAPHostSessionList::FindShellInCacheForHost(const char *hostName, const char *userName, const char *mailboxName, const char *UID, nsIMAPBodyShell	&shell)
{
	PR_EnterMonitor(gCachedHostInfoMonitor);
	nsIMAPHostInfo *host = FindHost(hostName, userName);
	if (host)
	{
		if (host->fShellCache)
			shell = *host->fShellCache->FindShellForUID(UID, mailboxName);
	}
	PR_ExitMonitor(gCachedHostInfoMonitor);
	return (host == NULL) ? NS_ERROR_ILLEGAL_VALUE : NS_OK;
}



