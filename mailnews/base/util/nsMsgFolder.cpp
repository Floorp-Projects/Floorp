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

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);


// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsMsgFolder::nsMsgFolder(void)
  : nsRDFResource(), mFlags(0),
    mNumUnreadMessages(0),	mNumTotalMessages(0),
		mListeners(nsnull),
    mCsid(0),
    mDepth(0), 
    mPrefFlags(0)
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

	//The rdf:mailnewsfolders datasource is going to be a listener to all nsIMsgFolders, so add
	//it as a listener
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFDataSource> datasource;
		rv = rdfService->GetDataSource("rdf:mailnewsfolders", getter_AddRefs(datasource));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIFolderListener> folderListener(do_QueryInterface(datasource, &rv));
			if(NS_SUCCEEDED(rv))
			{
				AddFolderListener(folderListener);
			}
		}
	}
}

nsMsgFolder::~nsMsgFolder(void)
{
	if(mSubFolders)
	{
		PRUint32 count = mSubFolders->Count();

		for (int i = count - 1; i >= 0; i--)
			mSubFolders->RemoveElementAt(i);
	}

  if (mListeners) {
      for (PRInt32 i = mListeners->Count() - 1; i >= 0; --i) {
          mListeners->RemoveElementAt(i);
      }
	delete mListeners;
  }

}

NS_IMPL_ISUPPORTS_INHERITED(nsMsgFolder, nsRDFResource, nsIMsgFolder)

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
  for (PRUint32 i = 0; i < array->Count(); i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs((*array)[i]);
    if (filter(element, data)) {
      nsresult rv = f->AppendElement(element);
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

NS_IMETHODIMP
nsMsgFolder::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgFolder::GetSubFolders(nsIEnumerator* *result)
{
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP nsMsgFolder::AddFolderListener(nsIFolderListener * listener)
{
  if (! mListeners)
	{
		mListeners = new nsVoidArray();
		if(!mListeners)
			return NS_ERROR_OUT_OF_MEMORY;
  }
  mListeners->AppendElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::RemoveFolderListener(nsIFolderListener * listener)
{
  if (! mListeners)
    return NS_OK;
  mListeners->RemoveElement(listener);
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


NS_IMETHODIMP nsMsgFolder::GetPrettyName(char ** name)
{
  char *cmName = mName.ToNewCString();
	if(cmName)
	{
		if (name) *name = PL_strdup(cmName);
		delete[] cmName;
	  return NS_OK;
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgFolder::SetPrettyName(char *name)
{
  mName = name;
  return NS_OK;
}

NS_IMETHODIMP_(PRUint32) nsMsgFolder::Count(void) const
{
  return mSubFolders->Count();
}

NS_IMETHODIMP nsMsgFolder::AppendElement(nsISupports *aElement)
{
  return mSubFolders->AppendElement(aElement);
}

NS_IMETHODIMP nsMsgFolder::RemoveElement(nsISupports *aElement)
{
  return mSubFolders->RemoveElement(aElement);
}


NS_IMETHODIMP nsMsgFolder::Enumerate(nsIEnumerator* *result)
{
  // nsMsgFolders only have subfolders, no message elements
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP nsMsgFolder::Clear(void)
{
  return mSubFolders->Clear();
}

NS_IMETHODIMP nsMsgFolder::GetName(char **name)
{
  char *cmName = mName.ToNewCString();
	if(cmName)
	{
		if (name) *name = PL_strdup(cmName);
		delete[] cmName;
		return NS_OK;
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgFolder::SetName(char * name)
{
	mName = name;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetChildNamed(const char* name, nsISupports* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::GetParent(nsIFolder* *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFolder::GetPrettiestName(char **name)
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

NS_IMETHODIMP nsMsgFolder::PropagateDelete(nsIMsgFolder **folder, PRBool deleteStorage)
{
	nsresult status = NS_OK;

	nsCOMPtr<nsIMsgFolder> child;

	// first, find the folder we're looking to delete
	for (PRUint32 i = 0; i < mSubFolders->Count() && *folder; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		child = do_QueryInterface(supports, &status);
		if(NS_SUCCEEDED(status))
			{
				if (*folder == child.get())
				{
					// maybe delete disk storage for it, and its subfolders
					status = child->RecursiveDelete(deleteStorage);	

					if (status == NS_OK) 
					{
#ifdef HAVE_MASTER
						PR_ASSERT(mMaster);
						// Send out a broadcast message that this folder is going away.
						// Many important things happen on this broadcast.
						mMaster->BroadcastFolderDeleted (child);
#endif
            mSubFolders->RemoveElement(child);
					}
				}
				else
				{
					PRUint32 folderDepth, childDepth;

					if(NS_SUCCEEDED((*folder)->GetDepth(&folderDepth)) &&
					   NS_SUCCEEDED(child->GetDepth(&childDepth)) &&
						folderDepth > childDepth)
					{ 
						status = child->PropagateDelete (folder, deleteStorage);
					}
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
	
	while (mSubFolders->Count() > 0)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(0));
		nsCOMPtr<nsIMsgFolder> child(do_QueryInterface(supports, &status));

		if(NS_SUCCEEDED(status))
		{
			status = child->RecursiveDelete(deleteStorage);  // recur

#ifdef HAVE_MASTER
			// Send out a broadcast message that this folder is going away.
			// Many important things happen on this broadcast.
			mMaster->BroadcastFolderDeleted (child);
#endif
			mSubFolders->RemoveElement(child);  // unlink it from this's child list
		}
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


NS_IMETHODIMP nsMsgFolder::Rename(const char *name)
{
    nsresult status = NS_OK;
	status = SetName((char*)name);
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

NS_IMETHODIMP nsMsgFolder::FindParentOf(nsIMsgFolder * aFolder, nsIMsgFolder ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	*aParent = nsnull;

	PRUint32 count = mSubFolders->Count();
	nsCOMPtr<nsISupports> supports;
	nsCOMPtr<nsIMsgFolder> child;

	for (PRUint32 i = 0; i < count && *aParent == NULL; i++)
	{
		supports = getter_AddRefs(mSubFolders->ElementAt(i));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			if (aFolder == child.get())
			{
				*aParent = this;
				NS_ADDREF(*aParent);
				return NS_OK;
			}
		}
	}

	for (PRUint32 j = 0; j < count && *aParent == NULL; j++)
	{

		supports = getter_AddRefs(mSubFolders->ElementAt(j));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			rv = child->FindParentOf(aFolder, aParent);
			if(NS_SUCCEEDED(rv))
				return rv;
		}

	}

	return rv;

}

NS_IMETHODIMP nsMsgFolder::IsParentOf(nsIMsgFolder *child, PRBool deep, PRBool *isParent)
{
	if(!isParent)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 count = mSubFolders->Count();

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (folder.get() == child )
				*isParent = PR_TRUE;
			else if(deep)
			{
				folder->IsParentOf(child, deep, isParent);
			}
		}
		if(*isParent)
			return NS_OK;
    }
	*isParent = PR_FALSE;
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

NS_IMETHODIMP nsMsgFolder::UpdateSummaryTotals()
{
	//We don't support this
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::SummaryChanged()
{
	UpdateSummaryTotals();
#ifdef HAVE_MASTER
    if (mMaster)
        mMaster->BroadcastFolderChanged(this);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetNumUnread(PRBool deep, PRUint32 *numUnread)
{
	if(!numUnread)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRUint32 total = mNumUnreadMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count = mSubFolders->Count();

		for (PRUint32 i = 0; i < count; i++)
		{
			nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
			folder = do_QueryInterface(supports, &rv);
			if(NS_SUCCEEDED(rv))
			{
				PRUint32 num;
				folder->GetNumUnread(deep, &num);
				if (num >= 0) // it's legal for counts to be negative if we don't know
					total += num;
			}
		}
	}
  *numUnread = total;
	return NS_OK;

}

NS_IMETHODIMP nsMsgFolder::GetTotalMessages(PRBool deep, PRUint32 *totalMessages)
{
	if(!totalMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;
	PRUint32 total = mNumTotalMessages;
	if (deep)
	{
		nsCOMPtr<nsIMsgFolder> folder;
		PRUint32 count = mSubFolders->Count();

		for (PRUint32 i = 0; i < count; i++)
		{
			nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
			folder = do_QueryInterface(supports, &rv);
			if(NS_SUCCEEDED(rv))
			{
				PRUint32 num;
				folder->GetTotalMessages (deep, &num);
				if (num >= 0) // it's legal for counts to be negative if we don't know
					total += num;
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

NS_IMETHODIMP nsMsgFolder::SetFolderCSID(PRInt16 csid)
{

}

NS_IMETHODIMP nsMsgFolder::GetFolderCSID(PRInt16 *csid)
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
		}
		num++;
	}

	nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder;
	for (PRUint32 i=0; i < (PRUint32)mSubFolders->Count(); i++) {
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
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

	*numFolders = num;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetExpansionArray(nsISupportsArray *expansionArray)
{
  // the application of flags in GetExpansionArray is subtly different
  // than in GetFoldersWithFlag 

	nsresult rv;
	for (PRUint32 i = 0; i < mSubFolders->Count(); i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			expansionArray->InsertElementAt(folder, expansionArray->Count());
			PRUint32 flags;
			folder->GetFlags(&flags);
			if (!(flags & MSG_FOLDER_FLAG_ELIDED))
				folder->GetExpansionArray(expansionArray);
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
				PRBool isParent;
				if(NS_SUCCEEDED(rv = fccFolders[i]->IsParentOf(this, PR_TRUE, &isParent)))
				{
					if (isParent)
						*displayRecipients = PR_TRUE;
				}
				NS_RELEASE(fccFolders[i]);
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::ReadDBFolderInfo(PRBool force)
{
	// Since it turns out to be pretty expensive to open and close
	// the DBs all the time, if we have to open it once, get everything
	// we might need while we're here

	nsresult result= NS_OK;
	if (force || !(mPrefFlags & MSG_FOLDER_PREF_CACHED))
    {
        nsCOMPtr<nsIDBFolderInfo> folderInfo;
        nsIMsgDatabase       *db = nsnull; //Not making an nsCOMPtr because we want to call close, not release
        result = NS_SUCCEEDED(GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), &db));
        if(result)
        {
			mIsCachable = TRUE;
            if (folderInfo)
            {

	            folderInfo->GetFlags(&mPrefFlags);
                mPrefFlags |= MSG_FOLDER_PREF_CACHED;
                folderInfo->SetFlags(mPrefFlags);

				folderInfo->GetNumMessages(&mNumTotalMessages);
				folderInfo->GetNumNewMessages(&mNumUnreadMessages);

				//These should be put in IMAP folder only.
				//folderInfo->GetImapTotalPendingMessages(&mNumPendingTotalMessages);
				//folderInfo->GetImapUnreadPendingMessages(&mNumPendingUnreadMessages);

				// folderInfo->GetCSID(&mCsid);
        
				if (db && !db->HasNew() && mNumPendingUnreadMessages <= 0)
					ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
            }

        }
        if (db)
	        db->Close(PR_FALSE);
    }

	return result;
	
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

NS_IMETHODIMP nsMsgFolder::GetUsersName(char **userName)
{
	if(!userName)
		return NS_ERROR_NULL_POINTER;

	*userName = "";
	return NS_OK;
}

NS_IMETHODIMP nsMsgFolder::GetHostName(char **hostName)
{
	if(!hostName)
		return NS_ERROR_NULL_POINTER;

	*hostName = "";
	return NS_OK;
}

nsresult nsMsgFolder::NotifyPropertyChanged(char *property, char *oldValue, char* newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(kISupportsIID, getter_AddRefs(supports))))
	{
		PRInt32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			//Folderlistener's aren't refcounted.
			nsIFolderListener* listener =(nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemPropertyChanged(supports, property, oldValue, newValue);
		}
	}

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

