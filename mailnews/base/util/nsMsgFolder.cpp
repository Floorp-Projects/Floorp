/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "msgCore.h"    // precompiled header...

#include "nsMsgFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "nsIMessage.h"
#include "prprf.h"
#include "nsMsgKeyArray.h"
#include "nsMsgDatabase.h"
#include "nsIDBFolderInfo.h"
#include "nsISupportsArray.h"
#include "nsIPref.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsIAllocator.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID,					NS_MSGMAILSESSION_CID);



nsMsgFolder::nsMsgFolder(void)
  : nsRDFResource(),
    mName(""),
    mFlags(0),
    mParent(nsnull),
    mNumUnreadMessages(-1),
    mNumTotalMessages(-1),
    mDepth(0), 
    mPrefFlags(0),
    mBiffState(nsMsgBiffState_NoMail),
    mNumNewBiffMessages(0),
    mIsServer(PR_FALSE)
	{
//  NS_INIT_REFCNT(); done by superclass


  mSemaphoreHolder = NULL;

#ifdef HAVE_DB
	mLastMessageLoaded	= nsMsgKey_None;
#endif
	mNumPendingUnreadMessages = 0;
	mNumPendingTotalMessages  = 0;
	NS_NewISupportsArray(getter_AddRefs(mSubFolders));

	mIsCachable = TRUE;

	mListeners = new nsVoidArray();

	m_server = nsnull;
}

nsMsgFolder::~nsMsgFolder(void)
{
	if(mSubFolders)
	{
		PRUint32 count;
    nsresult rv = mSubFolders->Count(&count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");

		for (int i = count - 1; i >= 0; i--)
			mSubFolders->RemoveElementAt(i);
	}

    delete mListeners;

}

NS_IMPL_ADDREF_INHERITED(nsMsgFolder, nsRDFResource)
NS_IMPL_RELEASE_INHERITED(nsMsgFolder, nsRDFResource)

NS_IMETHODIMP nsMsgFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsCOMTypeInfo<nsIMsgFolder>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsIFolder>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMsgFolder*, this);
	}              

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsRDFResource::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsMsgFolder::Init(const char* aURI)
{
  nsresult rv;

  // this parsing is totally hacky. we really should generalize this,
  // but I'm not going to do this until we can eliminate the
  // nsXXX2Name/etc routines
  // -alecf
  
  // do initial parsing of the URI
  const char *cp=aURI;

  // skip to initial //
  while (*cp && (*cp != '/'))
    cp++;

  // skip past //
  while (*cp && (*cp == '/'))
    cp++;

  if (PL_strchr(cp, '/'))
    mIsServer = PR_FALSE;
  else
    mIsServer = PR_TRUE;

  return nsRDFResource::Init(aURI);
}

////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

static nsresult
nsFilterBy(nsISupportsArray* array, nsArrayFilter filter, void* data,
           nsISupportsArray* *result)
{
  nsCOMPtr<nsISupportsArray> f;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(f));
  if (NS_FAILED(rv)) return rv;
  PRUint32 cnt;
  rv = array->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs(array->ElementAt(i));
    if (filter(element, data)) {
      rv = f->AppendElement(element);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  *result = f;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMsgFolder::AddUnique(nsISupports* element)
{
  // XXX fix this
  return mSubFolders->AppendElement(element);
}

// I'm assuming this means "Replace Subfolder"?
NS_IMETHODIMP
nsMsgFolder::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PRBool success=PR_FALSE;
  PRInt32 location = mSubFolders->IndexOf(element);
  if (location>0)
    success = mSubFolders->ReplaceElementAt(newElement, location);
  
  return success ? NS_OK : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsMsgFolder::GetSubFolders(nsIEnumerator* *result)
{
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP 
nsMsgFolder::FindSubFolder(const char *subFolderName, nsIFolder **aFolder)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  
	if(NS_FAILED(rv)) 
		return rv;

	nsCAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(subFolderName);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri.GetBuffer(), getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;
	if (aFolder)
	{
		*aFolder = folder;
		NS_ADDREF(*aFolder);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsMsgFolder::GetHasSubFolders(PRBool *_retval)
{
  PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  *_retval = (cnt > 0);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::AddFolderListener(nsIFolderListener * listener)
{
	mListeners->AppendElement(listener);
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::RemoveFolderListener(nsIFolderListener * listener)
{

  mListeners->RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::SetParent(nsIFolder *parent)
{
	//Don't addref due to ownership issues.
	mParent = parent;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetParent(nsIFolder **parent)
{
	if(!parent)
		return NS_ERROR_NULL_POINTER;

	*parent = mParent;
	NS_IF_ADDREF(*parent);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::GetMessages(nsIEnumerator* *result)
{
  // XXX should this return an empty enumeration?
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgFolder::GetThreads(nsIEnumerator ** threadEnumerator)
{
  // XXX should this return an empty enumeration?
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgFolder::GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread)
{
	return NS_ERROR_FAILURE;	
}

NS_IMETHODIMP
nsMsgFolder::HasMessage(nsIMessage *message, PRBool *hasMessage)
{
	return NS_ERROR_FAILURE;	
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgFolder::BuildFolderURL(char **url)
{
	if(*url)
	{
		*url = NULL;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;	
}

#ifdef HAVE_DB
// this class doesn't have a url
NS_IMETHODIMP nsMsgFolder::BuildUrl(nsMsgDatabase *db, nsMsgKey key, char ** url)
{
	if(*url)
	{
		*url = NULL;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}
#endif

NS_IMETHODIMP nsMsgFolder::GetServer(nsIMsgIncomingServer ** aServer)
{

  /* this should really just:
     - truncate the URI
     - GetResource on the URI
     - QueryInterface to nsIMsgIncomingServer::GetIID()
  */
	nsresult rv = NS_OK; 
	if (!m_server) // if we haven't fetched the server yet....
	{
		NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv); 
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIMsgAccountManager> accountManager;
		rv = session->GetAccountManager(getter_AddRefs(accountManager));
		if(NS_FAILED(rv)) return rv;

    char * hostname = nsnull;
		rv = GetHostname(&hostname);
		if(NS_FAILED(rv)) return rv;

    char * username = nsnull;
    rv = GetUsername(&username);
    if (NS_FAILED(rv)) return rv;
    
    nsIMsgIncomingServer *server;
		rv = accountManager->FindServer(username,
                                    hostname,
                                    GetIncomingServerType(),
                                    &server);
    PR_FREEIF(username);
		PR_FREEIF(hostname);
		if (NS_FAILED(rv)) return rv;
	
		m_server = server;
		// release because we don't wan't to keep a reference.
		// the server keeps a reference to the folder, and if the
		// folder keeps one back to the server, we'd get a cycle.
		NS_IF_RELEASE(server);
	}

	if (aServer)
	{
		*aServer = m_server;
		NS_IF_ADDREF(*aServer);
	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

NS_IMETHODIMP
nsMsgFolder::GetIsServer(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mIsServer;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetPrettyName(PRUnichar ** name)
{
	if (!name)
		return NS_ERROR_NULL_POINTER;
	*name = mName.ToNewUnicode();
	return (*name) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgFolder::SetPrettyName(PRUnichar *name)
{
  mName = name;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetName(PRUnichar **name)
{
	if (!name)
		return NS_ERROR_NULL_POINTER;
  
  *name = nsnull;

  // cache the name in mName
  if (mName.IsEmpty()) {
    // return the leaf of this URI
    char *lastSlash = PL_strrchr(mURI, '/');
    if (lastSlash) {
      lastSlash++;
      mName = lastSlash;
    } else {
      // no slashes, return the whole URI
      mName = mURI;
    }
  }

  *name = mName.ToNewUnicode();
  
  if (!(*name)) return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetName(PRUnichar * name)
{
  // override the URI-generated name
	mName = name;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetChildNamed(const char *name, nsISupports ** aChild)
{
	NS_ASSERTION(aChild, "NULL child");
	nsresult rv;
	// will return nsnull if we can't find it
	*aChild = nsnull;

	nsCOMPtr<nsIMsgFolder> folder;

	PRUint32 count;
	rv = mSubFolders->Count(&count);
	if (NS_FAILED(rv)) return rv;

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			PRUnichar *folderName;

			folder->GetName(&folderName);
			// case-insensitive compare is probably LCD across OS filesystems
			if (folderName && nsCRT::strcasecmp(folderName, name)==0)
			{
				*aChild = folder;
				delete[] folderName;
				return NS_OK;
			}
			delete[] folderName;
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetChildWithURI(const char *uri, PRBool deep, nsIMsgFolder ** child)
{
	NS_ASSERTION(child, "NULL child");
	nsresult rv;
	// will return nsnull if we can't find it
	*child = nsnull;


	PRUint32 count;
	rv = mSubFolders->Count(&count);
	if (NS_FAILED(rv)) return rv;

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(supports);
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports);
		if(folderResource  && folder)
		{
			char *folderURI;
			rv = folderResource->GetValue(&folderURI);
			if(NS_FAILED(rv))
				return rv;

			// case-insensitive compare is probably LCD across OS filesystems
			PRBool equal = (folderURI && nsCRT::strcasecmp(folderURI, uri)==0);
			nsAllocator::Free(folderURI);
			if (equal)
			{
				*child = folder;
				NS_ADDREF(*child);
				return NS_OK;
			}
			else if(deep)
			{
				rv = folder->GetChildWithURI(uri, deep, child);
				if(NS_FAILED(rv))
					return rv;

				if(*child)
					return NS_OK;
			}
		}
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetPrettiestName(PRUnichar **name)
{
  if (NS_SUCCEEDED(GetPrettyName(name)))
    return NS_OK;
  return GetName(name);
}

static PRBool
nsCanBeInFolderPane(nsISupports* element, void* data)
{
#ifdef HAVE_PANE
  nsIMsgFolder* subFolder = NS_STATIC_CAST(nsIMsgFolder*, element);
  return subFolder->CanBeInFolderPane(); 
#else
  return PR_TRUE;
#endif
}

NS_IMETHODIMP
nsMsgFolder::GetVisibleSubFolders(nsIEnumerator* *result) 
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> vFolders;
  rv = nsFilterBy(mSubFolders, nsCanBeInFolderPane, nsnull, getter_AddRefs(vFolders));
  if (NS_FAILED(rv)) return rv;
  rv = vFolders->Enumerate(result);
  return rv;
}

#ifdef HAVE_ADMINURL
NS_IMETHODIMP nsMsgFolder::GetAdminUrl(MWContext *context, MSG_AdminURLType type)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::HaveAdminUrl(MSG_AdminURLType type, PRBool *haveAdminUrl)
{
	if(haveAdminUrl)
	{
		*haveAdminUrl = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}
#endif

NS_IMETHODIMP nsMsgFolder::GetDeleteIsMoveToTrash(PRBool *deleteIsMoveToTrash)
{
	if(deleteIsMoveToTrash)
	{
		*deleteIsMoveToTrash = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgFolder::GetShowDeletedMessages(PRBool *showDeletedMessages)
{
	if(showDeletedMessages)
	{
		*showDeletedMessages = PR_FALSE;
		return NS_OK;
	}
	return
		NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgFolder::OnCloseFolder ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::Delete ()
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::DeleteSubFolders(nsISupportsArray *folders)
{
	nsresult rv;

	PRUint32 count;
	rv = folders->Count(&count);
	nsCOMPtr<nsIMsgFolder> folder;
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(folders->ElementAt(i));
		folder = do_QueryInterface(supports);
		if(folder)	
			PropagateDelete(folder, PR_TRUE);
	}
	return rv;

}

NS_IMETHODIMP nsMsgFolder::PropagateDelete(nsIMsgFolder *folder, PRBool deleteStorage)
{
	nsresult status = NS_OK;

	nsCOMPtr<nsIMsgFolder> child;

	// first, find the folder we're looking to delete
	PRUint32 cnt;
	nsresult rv = mSubFolders->Count(&cnt);
	if (NS_FAILED(rv)) return rv;
	for (PRUint32 i = 0; i < cnt; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		child = do_QueryInterface(supports, &status);
		if(NS_SUCCEEDED(status))
		{
			if (folder == child.get())
			{
				// maybe delete disk storage for it, and its subfolders
				status = child->RecursiveDelete(deleteStorage);	

				if (status == NS_OK) 
				{

					//Remove from list of subfolders.
					mSubFolders->RemoveElement(child);
					//Remove self as parent
					child->SetParent(nsnull);
					nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
					if(childSupports)
						NotifyItemDeleted(childSupports);
					break;
				}
			}
			else
			{
				status = child->PropagateDelete (folder, deleteStorage);
			}
		}
	}
	
	return status;
}

NS_IMETHODIMP nsMsgFolder::RecursiveDelete(PRBool deleteStorage)
{
	// If deleteStorage is TRUE, recursively deletes disk storage for this folder
	// and all its subfolders.
	// Regardless of deleteStorage, always unlinks them from the children lists and
	// frees memory for the subfolders but NOT for _this_

	nsresult status = NS_OK;
	
	PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  while (cnt > 0)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(0));
		nsCOMPtr<nsIMsgFolder> child(do_QueryInterface(supports, &status));

		if(NS_SUCCEEDED(status))
		{
			status = child->RecursiveDelete(deleteStorage);  // recur
			mSubFolders->RemoveElement(child);  // unlink it from this's child list
			child->SetParent(nsnull);
			nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
			if(childSupports)
				NotifyItemDeleted(childSupports);
		}
		cnt--;
	}

	// now delete the disk storage for _this_
	if (deleteStorage && (status == NS_OK))
		status = Delete();

	return status;
}

NS_IMETHODIMP nsMsgFolder::CreateSubfolder(const char *folderName)
{
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::Compact()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::EmptyTrash()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::Rename(const char *name)
{
    nsresult status = NS_OK;
	nsAutoString2 unicharString(name);
	status = SetName((PRUnichar *) unicharString.GetUnicode());
	//After doing a SetName we need to make sure that broadcasting this message causes a
	//new sort to happen.
#ifdef HAVE_MASTER
	if (m_master)
		m_master->BroadcastFolderChanged(this);
#endif
	return status;

}

NS_IMETHODIMP nsMsgFolder::Adopt(nsIMsgFolder *srcFolder, PRUint32* outPos)
{
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::ContainsChildNamed(const char *name, PRBool* containsChild)
{
	nsCOMPtr<nsISupports> child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(GetChildNamed(name, getter_AddRefs(child))))
		{
			*containsChild = child != nsnull;
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}



NS_IMETHODIMP nsMsgFolder::IsAncestorOf(nsIMsgFolder *child, PRBool *isAncestor)
{
	if(!isAncestor)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 count;
  rv = mSubFolders->Count(&count);
  if (NS_FAILED(rv)) return rv;

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (folder.get() == child )
			{
				*isAncestor = PR_TRUE;
			}
			else
				folder->IsAncestorOf(child, isAncestor);

		}
		if(*isAncestor)
			return NS_OK;
    }
	*isAncestor = PR_FALSE;
	return rv;

}


NS_IMETHODIMP nsMsgFolder::GenerateUniqueSubfolderName(const char *prefix, nsIMsgFolder *otherFolder,
                                                       char **name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	/* only try 256 times */
	for (int count = 0; (count < 256); count++)
	{
		PRUint32 prefixSize = PL_strlen(prefix);

		//allocate string big enough for prefix, 256, and '\0'
		char *uniqueName = (char*)PR_MALLOC(prefixSize + 4);
		if(!uniqueName)
			return NS_ERROR_OUT_OF_MEMORY;
		PR_snprintf(uniqueName, prefixSize + 4, "%s%d",prefix,count);
		PRBool containsChild;
		PRBool otherContainsChild = PR_FALSE;

		ContainsChildNamed(uniqueName, &containsChild);
		if(otherFolder)
		{
			((nsIMsgFolder*)otherFolder)->ContainsChildNamed(uniqueName, &otherContainsChild);
		}

		if (!containsChild && !otherContainsChild)
		{
			*name = uniqueName;
			return NS_OK;
		}
		else
			PR_FREEIF(uniqueName);
	}
	*name = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetDepth(PRUint32 *depth)
{
	if(!depth)
		return NS_ERROR_NULL_POINTER;
	*depth = mDepth;
	return NS_OK;
	
}

NS_IMETHODIMP nsMsgFolder::SetDepth(PRUint32 depth)
{
	mDepth = depth;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UpdateSummaryTotals(PRBool /* force */)
{
	//We don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SummaryChanged()
{
	UpdateSummaryTotals(PR_FALSE);
#ifdef HAVE_MASTER
    if (mMaster)
        mMaster->BroadcastFolderChanged(this);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumUnread(PRBool deep, PRInt32 *numUnread)
{
	if(!numUnread)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRUint32 total = mNumUnreadMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) {

      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
        folder = do_QueryInterface(supports, &rv);
        if(NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          folder->GetNumUnread(deep, &num);
          if (num >= 0) // it's legal for counts to be negative if we don't know
            total += num;
        }
      }
    }
	}
  *numUnread = total;
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::GetTotalMessages(PRBool deep, PRInt32 *totalMessages)
{
	if(!totalMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRInt32 total = mNumTotalMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) {

      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
        folder = do_QueryInterface(supports, &rv);
        if(NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          folder->GetTotalMessages (deep, &num);
          if (num >= 0) // it's legal for counts to be negative if we don't know
            total += num;
        }
			}
		}
	}
  *totalMessages = total;
  return NS_OK;
}

#ifdef HAVE_DB
NS_IMETHOD GetTotalMessagesInDB(PRUint32 *totalMessages) const;					// How many messages in database.
#endif
	
#ifdef HAVE_PANE
virtual void	MarkAllRead(MSG_Pane *pane, PRBool deep);
#endif

#ifdef HAVE_DB	
// These functions are used for tricking the front end into thinking that we have more 
// messages than are really in the DB.  This is usually after and IMAP message copy where
// we don't want to do an expensive select until the user actually opens that folder
// These functions are called when MSG_Master::GetFolderLineById is populating a MSG_FolderLine
// struct used by the FE
int32			GetNumPendingUnread(PRBool deep = FALSE) const;
int32			GetNumPendingTotalMessages(PRBool deep = FALSE) const;
	
void			ChangeNumPendingUnread(int32 delta);
void			ChangeNumPendingTotalMessages(int32 delta);


NS_IMETHODIMP nsMsgFolder::SetFolderPrefFlags(PRUint32 flags)
{

}

NS_IMETHODIMP nsMsgFolder::GetFolderPrefFlags(PRUint32 *flags)
{

}


NS_IMETHODIMP nsMsgFolder::SetLastMessageLoaded(nsMsgKey lastMessageLoaded)
{

}

NS_IMETHODIMP nsMsgFolder::GetLastMessageLoaded()
{

}

#endif

NS_IMETHODIMP nsMsgFolder::SetFlag(PRUint32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (!flagSet)
	{
		mFlags |= flag;
		OnFlagChange(flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearFlag(PRUint32 flag)
{
	// OnFlagChange can be expensive, so don't call it if we don't need to
	PRBool flagSet;
	nsresult rv;

	if(!NS_SUCCEEDED(rv = GetFlag(flag, &flagSet)))
		return rv;

	if (!flagSet)
	{
		mFlags &= ~flag;
		OnFlagChange (flag);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlag(PRUint32 flag, PRBool *_retval)
{
	*_retval = ((mFlags & flag) != 0);
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ToggleFlag(PRUint32 flag)
{
  mFlags ^= flag;
	OnFlagChange (flag);

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::OnFlagChange(PRUint32 flag)
{
	//Still need to implement
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFlags(PRUint32 *_retval)
{
	*_retval = mFlags;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetFoldersWithFlag(PRUint32 flags, nsIMsgFolder **result,
											PRUint32 resultsize, PRUint32 *numFolders)
{
	PRUint32 num = 0;
	if ((flags & mFlags) == flags) {
		if (result && (num < resultsize)) {
			result[num] = this;
			NS_IF_ADDREF(result[num]);
		}
		num++;
	}

	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder;
	PRUint32 cnt;
  rv = mSubFolders->Count(&cnt);
  if (NS_SUCCEEDED(rv)) {
    for (PRUint32 i=0; i < cnt; i++) 
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
      if(NS_SUCCEEDED(rv) && folder)
      {
        // CAREFUL! if NULL is passed in for result then the caller
        // still wants the full count!  Otherwise, the result should be at most the
        // number that the caller asked for.
        PRUint32 numSubFolders;

        if (!result)
        {
          folder->GetFoldersWithFlag(flags, NULL, 0, &numSubFolders);
          num += numSubFolders;
        }
        else if (num < resultsize)
        {
          folder->GetFoldersWithFlag(flags, result + num, resultsize - num, &numSubFolders);
          num += numSubFolders;
        }
        else
        {
          break;
        }
      }
    }
  }
	*numFolders = num;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetExpansionArray(nsISupportsArray *expansionArray)
{
  // the application of flags in GetExpansionArray is subtly different
  // than in GetFoldersWithFlag 

	nsresult rv;
	PRUint32 cnt;
  rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			PRUint32 cnt2;
      rv = expansionArray->Count(&cnt2);
      if (NS_SUCCEEDED(rv)) {
        expansionArray->InsertElementAt(folder, cnt2);
        PRUint32 flags;
        folder->GetFlags(&flags);
        if (!(flags & MSG_FOLDER_FLAG_ELIDED))
          folder->GetExpansionArray(expansionArray);
      }
    }
  }

	return NS_OK;
}

#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::SetFlagInAllFolderPanes(PRUInt32 which)
{

}

#endif

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::EscapeMessageId(const char *messageId, const char **escapeMessageID)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetExpungedBytesCount(PRUint32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

	*count = 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetDeletable(PRBool *deletable)
{

	if(!deletable)
		return NS_ERROR_NULL_POINTER;

	*deletable = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetCanCreateChildren(PRBool *canCreateChildren)
{
	if(!canCreateChildren)
		return NS_ERROR_NULL_POINTER;

	*canCreateChildren = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetCanBeRenamed(PRBool *canBeRenamed)
{
	if(!canBeRenamed)
		return NS_ERROR_NULL_POINTER;

	*canBeRenamed = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRequiresCleanup(PRBool *requiredCleanup)
{
	if(!requiredCleanup)
		return NS_ERROR_NULL_POINTER;

	*requiredCleanup = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ClearRequiresCleanup()
{
	return NS_OK;
}

#ifdef HAVE_PANE
NS_IMETHODIMP nsMsgFolder::CanBeInFolderPane(PRBool *canBeInFolderPane)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetKnowsSearchNntpExtension(PRBool *knowsExtension)
{
	if(!knowsExtension)
		return NS_ERROR_NULL_POINTER;

	*knowsExtension = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetAllowsPosting(PRBool *allowsPosting)
{
	if(!allowsPosting)
		return NS_ERROR_NULL_POINTER;

	*allowsPosting = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::DisplayRecipients(PRBool *displayRecipients)
{
	nsresult rv;

	*displayRecipients = PR_FALSE;

	if (mFlags & MSG_FOLDER_FLAG_SENTMAIL && !(mFlags & MSG_FOLDER_FLAG_INBOX))
		*displayRecipients = PR_TRUE;
	else if (mFlags & MSG_FOLDER_FLAG_QUEUE)
		*displayRecipients = PR_TRUE;
	else 
	{
		// Only mail folders can be FCC folders
		if (mFlags & MSG_FOLDER_FLAG_MAIL || mFlags & MSG_FOLDER_FLAG_IMAPBOX) 
		{
			// There's one FCC folder for sent mail, and one for sent news
			nsIMsgFolder *fccFolders[2]; 
			int numFccFolders = 0;
#ifdef HAVE_MASTER
			m_master->GetFolderTree()->GetFoldersWithFlag (MSG_FOLDER_FLAG_SENTMAIL, fccFolders, 2, &numFccFolders);
#endif
			for (int i = 0; i < numFccFolders; i++)
			{
				PRBool isAncestor;
				if(NS_SUCCEEDED(rv = fccFolders[i]->IsAncestorOf(this, &isAncestor)))
				{
					if (isAncestor)
						*displayRecipients = PR_TRUE;
				}
				NS_RELEASE(fccFolders[i]);
			}
		}
	}
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::AcquireSemaphore(nsISupports *semHolder)
{
	nsresult rv = NS_OK;

	if (mSemaphoreHolder == NULL)
	{
		mSemaphoreHolder = semHolder;	//Don't AddRef due to ownership issues.
	}
	else
		rv = NS_MSG_FOLDER_BUSY;

	return rv;

}

NS_IMETHODIMP nsMsgFolder::ReleaseSemaphore(nsISupports *semHolder)
{
	if (!mSemaphoreHolder || mSemaphoreHolder == semHolder)
	{
		mSemaphoreHolder = NULL;
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::TestSemaphore(nsISupports *semHolder, PRBool *result)
{
	if(!result)
		return NS_ERROR_NULL_POINTER;

	*result = (mSemaphoreHolder == semHolder);

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::IsLocked(PRBool *isLocked)
{ 
	*isLocked =  mSemaphoreHolder != NULL;
	return  NS_OK;
}

#ifdef HAVE_PANE
    MWContext *GetFolderPaneContext();
#endif

#ifdef HAVE_MASTER
    MSG_Master  *GetMaster() {return m_master;}
#endif

#ifdef HAVE_CACHE
NS_IMETHODIMP nsMsgFolder::WriteToCache(XP_File)
{

}

NS_IMETHODIMP nsMsgFolder::ReadFromCache(char *)
{

}

NS_IMETHODIMP nsMsgFolder::IsCachable(PRBool *isCachable)
{

}

NS_IMETHODIMP nsMsgFolder::SkipCacheTokens(char **ppBuf, int numTokens)
{

}
#endif

NS_IMETHODIMP nsMsgFolder::GetRelativePathName(char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;

	*pathName = nsnull;
	return NS_OK;
}


NS_IMETHODIMP nsMsgFolder::GetSizeOnDisk(PRUint32 *size)
{
	if(!size)
		return NS_ERROR_NULL_POINTER;

	*size = 0;
	return NS_OK;

}

#ifdef HAVE_NET
NS_IMETHODIMP nsMsgFolder::ShouldPerformOperationOffline(PRBool *performOffline)
{

}
#endif


#ifdef DOES_FOLDEROPERATIONS
NS_IMETHODIMP nsMsgFolder::DownloadToTempFileAndUpload(MessageCopyInfo *copyInfo,
                                                       nsMsgKeyArray &keysToSave,
                                                       MSG_FolderInfo *dstFolder,
                                                       nsMsgDatabase *sourceDB)
{

}

NS_IMETHODIMP nsMsgFolder::UpdateMoveCopyStatus(MWContext *context, PRBool isMove, int32 curMsgCount, int32 totMessages)
{

}

#endif

NS_IMETHODIMP nsMsgFolder::RememberPassword(const char *password)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRememberedPassword(char ** password)
{

	if(!password)
		return NS_ERROR_NULL_POINTER;

	*password = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *needsAuthenticate)
{
	if(!needsAuthenticate)
		return NS_ERROR_NULL_POINTER;

	*needsAuthenticate = PR_FALSE;
	return NS_OK;
}

#if 0
NS_IMETHODIMP nsMsgFolder::GetUsername(char **userName)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  
  return server->GetUsername(userName);
}

NS_IMETHODIMP nsMsgFolder::GetHostname(char **hostName)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  
  return server->GetHostname(hostName);
}
#endif

NS_IMETHODIMP nsMsgFolder::GetNewMessages()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::GetBiffState(PRUint32 *aBiffState)
{
	if(!aBiffState)
		return NS_ERROR_NULL_POINTER;
	*aBiffState = mBiffState;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetBiffState(PRUint32 aBiffState)
{
	if(mBiffState != aBiffState)
	{
		PRUint32 oldBiffState = mBiffState;
		mBiffState = aBiffState;
		nsCOMPtr<nsISupports> supports;
		if(NS_SUCCEEDED(QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), getter_AddRefs(supports))))
			NotifyPropertyFlagChanged(supports, "BiffState", oldBiffState, mBiffState);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumNewMessages(PRInt32 *aNumNewMessages)
{
	if(!aNumNewMessages)
		return NS_ERROR_NULL_POINTER;

	*aNumNewMessages = mNumNewBiffMessages;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SetNumNewMessages(PRInt32 aNumNewMessages)
{
	if(aNumNewMessages != mNumNewBiffMessages)
	{
		PRInt32 oldNumMessages = mNumNewBiffMessages;
		mNumNewBiffMessages = aNumNewMessages;

		char *oldNumMessagesStr = PR_smprintf("%d", oldNumMessages);
		char *newNumMessagesStr = PR_smprintf("%d",aNumNewMessages);
		NotifyPropertyChanged("NumNewBiffMessages", oldNumMessagesStr, newNumMessagesStr);
		PR_smprintf_free(oldNumMessagesStr);
		PR_smprintf_free(newNumMessagesStr);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNewMessagesNotificationDescription(PRUnichar * *aDescription)
{
	nsresult rv;
	nsString description("");
	nsCOMPtr<nsIMsgIncomingServer> server;
	rv = GetServer(getter_AddRefs(server));
  
	if(NS_SUCCEEDED(rv))
	{
		if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
		{
			nsXPIDLString folderName;
			rv = GetPrettyName(getter_Copies(folderName));
			if (NS_SUCCEEDED(rv) && folderName)
				description = folderName;
		}

    // append the server name
    nsXPIDLString serverName;
		rv = server->GetPrettyName(getter_Copies(serverName));
		if(NS_SUCCEEDED(rv)) {
      // put this test here because we don't want to just put "folder name on"
      // in case the above failed
      if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
        description += " on ";
			description += serverName;
    }
	}
	*aDescription = description.ToNewUnicode();
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetRootFolder(nsIMsgFolder * *aRootFolder)
{
	if(!aRootFolder)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	nsCOMPtr<nsIMsgIncomingServer> server;
	rv = GetServer(getter_AddRefs(server));
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIFolder> aRoot;
	rv = server->GetRootFolder(getter_AddRefs(aRoot));

	if(NS_FAILED(rv) || !aRoot)
		return rv;

	return aRoot->QueryInterface(nsCOMTypeInfo<nsIMsgFolder>::GetIID(), (void**)aRootFolder);
}

NS_IMETHODIMP
nsMsgFolder::GetMsgDatabase(nsIMsgDatabase** aMsgDatabase)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::GetPath(nsIFileSpec * *aPath)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::MarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
	PRUint32 count;
	nsresult rv;

	rv = messages->Count(&count);
	if(NS_FAILED(rv))
		return rv;

	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
		nsCOMPtr<nsIMessage> message = do_QueryInterface(msgSupports);

		if(message)
			rv = message->MarkRead(markRead);

		if(NS_FAILED(rv))
			return rv;

	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgFolder::MarkAllMessagesRead(void)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::CopyMessages(nsIMsgFolder* srcFolder,
                          nsISupportsArray *messages,
                          PRBool isMove,
                          nsITransactionManager* txnMgr,
                          nsIMsgCopyServiceListener* listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::CopyFileMessage(nsIFileSpec* fileSpec,
                             nsIMessage* messageToReplace,
                             PRBool isDraftOrTemplate,
                             nsITransactionManager* txnMgr,
                             nsIMsgCopyServiceListener* listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::MatchName(nsString *name, PRBool *matches)
{
	if (!matches)
		return NS_ERROR_NULL_POINTER;
	*matches = mName.EqualsIgnoreCase(*name);
	return NS_OK;
}

nsresult nsMsgFolder::NotifyPropertyChanged(char *property, char *oldValue, char* newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), getter_AddRefs(supports))))
	{
		PRInt32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			//Folderlistener's aren't refcounted.
			nsIFolderListener* listener =(nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemPropertyChanged(supports, property, oldValue, newValue);
		}

		//Notify listeners who listen to every folder
		nsresult rv;
		NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
		if(NS_SUCCEEDED(rv))
			mailSession->NotifyFolderItemPropertyChanged(supports, property, oldValue, newValue);

	}

	return NS_OK;

}

nsresult nsMsgFolder::NotifyPropertyFlagChanged(nsISupports *item, char *property, PRUint32 oldValue,
												PRUint32 newValue)
{
	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
	}

	//Notify listeners who listen to every folder
	nsresult rv;
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->NotifyFolderItemPropertyFlagChanged(item, property, oldValue, newValue);

	return NS_OK;
}

nsresult nsMsgFolder::NotifyItemAdded(nsISupports *item)
{

	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemAdded(this, item);
	}

	//Notify listeners who listen to every folder
	nsresult rv;
	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->NotifyFolderItemAdded(this, item);

	return NS_OK;

}

nsresult nsMsgFolder::NotifyItemDeleted(nsISupports *item)
{

	PRInt32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		//Folderlistener's aren't refcounted.
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemRemoved(this, item);
	}
	//Notify listeners who listen to every folder
	nsresult rv;
  NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		mailSession->NotifyFolderItemDeleted(this, item);

	return NS_OK;

}


nsresult
nsGetMailFolderSeparator(nsString& result)
{
  static char* gMailFolderSep = nsnull;         // never freed

  if (gMailFolderSep == nsnull) {
    gMailFolderSep = PR_smprintf(".sbd");
    if (gMailFolderSep == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  result = gMailFolderSep;
  return NS_OK;
}

