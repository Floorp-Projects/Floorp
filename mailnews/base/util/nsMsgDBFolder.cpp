/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIMessage.h"
#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIPref.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsLocalFolderSummarySpec.h"

#if defined(XP_OS2)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 8
#elif defined(XP_MAC)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 26
#elif defined(XP_WIN32)
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 256
#else
#define MAX_FILE_LENGTH_WITHOUT_EXTENSION 32000
#endif


static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);

nsIAtom* nsMsgDBFolder::mFolderLoadedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mDeleteOrMoveMsgCompletedAtom=nsnull;
nsrefcnt nsMsgDBFolder::mInstanceCount=0;

NS_IMPL_ISUPPORTS_INHERITED2(nsMsgDBFolder, nsMsgFolder,
                                   nsIDBChangeListener,
                                   nsIUrlListener)


nsMsgDBFolder::nsMsgDBFolder(void)
: mAddListener(PR_TRUE), mNewMessages(PR_FALSE), mGettingNewMessages(PR_FALSE)
{
  if (mInstanceCount++ <=0) {
    mFolderLoadedAtom = NS_NewAtom("FolderLoaded");
    mDeleteOrMoveMsgCompletedAtom = NS_NewAtom("DeleteOrMoveMsgCompleted");
  }
}

nsMsgDBFolder::~nsMsgDBFolder(void)
{
  if (--mInstanceCount == 0) {
    NS_IF_RELEASE(mFolderLoadedAtom);
    NS_IF_RELEASE(mDeleteOrMoveMsgCompletedAtom);
  }
	//shutdown but don't shutdown children.
	Shutdown(PR_FALSE);
}

NS_IMETHODIMP nsMsgDBFolder::Shutdown(PRBool shutdownChildren)
{
	if(mDatabase)
	{
		mDatabase->RemoveListener(this);
		mDatabase->Close(PR_TRUE);
		mDatabase = null_nsCOMPtr();

  }

	if(shutdownChildren)
	{
		PRUint32 count;
	    nsresult rv = mSubFolders->Count(&count);
	    if(NS_SUCCEEDED(rv))
		{
			for (PRUint32 i = 0; i < count; i++)
			{
				nsCOMPtr<nsISupports> childFolderSupports = getter_AddRefs(mSubFolders->ElementAt(i));
				if(childFolderSupports)
				{
					nsCOMPtr<nsIFolder> childFolder = do_QueryInterface(childFolderSupports);
					if(childFolder)
						childFolder->Shutdown(PR_TRUE);
				}
			}
		}
	}
	return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::ForceDBClosed ()
{
    PRUint32 cnt = 0, i;
    if (mSubFolders)
    {
        nsCOMPtr<nsISupports> aSupport;
        nsCOMPtr<nsIMsgFolder> child;
        mSubFolders->Count(&cnt);
        if (cnt > 0)
            for (i = 0; i < cnt; i++)
            {
                aSupport = getter_AddRefs(mSubFolders->ElementAt(i));
                child = do_QueryInterface(aSupport);
                if (child)
                    child->ForceDBClosed();
            }
    }
    if (mDatabase)
    {
        mDatabase->ForceClosed();
        mDatabase = null_nsCOMPtr();
    }
    return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::StartFolderLoading(void)
{
	if(mDatabase)
		mDatabase->RemoveListener(this);
	mAddListener = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::EndFolderLoading(void)
{
	if(mDatabase)
		mDatabase->AddListener(this);
	mAddListener = PR_TRUE;
	UpdateSummaryTotals(PR_TRUE);

	//GGGG			 check for new mail here and call SetNewMessages...?? -- ONE OF THE 2 PLACES
	if(mDatabase)
	{
	    nsresult rv;
		PRBool hasNewMessages;

		rv = mDatabase->HasNew(&hasNewMessages);
		SetHasNewMessages(hasNewMessages);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetThreads(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator** threadEnumerator)
{
	nsresult rv = GetDatabase(aMsgWindow);
	
	if(NS_SUCCEEDED(rv))
		return mDatabase->EnumerateThreads(threadEnumerator);
	else
		return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread)
{
	nsresult rv = GetDatabase(nsnull);
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
		nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(message, &rv));
		if(NS_SUCCEEDED(rv))
			rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));
		if(NS_SUCCEEDED(rv))
		{
			rv = mDatabase->GetThreadContainingMsgHdr(msgDBHdr, thread);
		}
	}
	return rv;

}


NS_IMETHODIMP
nsMsgDBFolder::GetExpungedBytes(PRUint32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

  if (mDatabase)
  {
    nsresult rv;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    rv = mDatabase->GetDBFolderInfo(getter_AddRefs(folderInfo));
    if (NS_FAILED(rv)) return rv;
    rv = folderInfo->GetExpungedBytes((PRInt32 *) count);
    if (NS_SUCCEEDED(rv))
      mExpungedBytes = *count; // sync up with the database
    return rv;
  }
  else
  {
    ReadDBFolderInfo(PR_FALSE);
    *count = mExpungedBytes;
  }
	return NS_OK;
}


NS_IMETHODIMP
nsMsgDBFolder::HasMessage(nsIMessage *message, PRBool *hasMessage)
{
	if(!hasMessage)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = GetDatabase(nsnull);

	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgDBHdr> msgDBHdr, msgDBHdrForKey;
		nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(message, &rv));
		nsMsgKey key;
		if(NS_SUCCEEDED(rv))
			rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));
		if(NS_SUCCEEDED(rv))
			rv = msgDBHdr->GetMessageKey(&key);
		if(NS_SUCCEEDED(rv))
			rv = mDatabase->ContainsKey(key, hasMessage);
		
	}
	return rv;

}

NS_IMETHODIMP nsMsgDBFolder::GetCharset(PRUnichar * *aCharset)
{
	nsresult rv = NS_OK;
	if(!aCharset)
		return NS_ERROR_NULL_POINTER;

	if(mCharset.IsEmpty())
	{
		NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);

		PRUnichar *prefCharset = nsnull;
		if (NS_SUCCEEDED(rv))
		{
			rv = prefs->GetLocalizedUnicharPref("mailnews.view_default_charset", &prefCharset);
		}
  
		nsAutoString prefCharsetStr;
		if(prefCharset)
		{
			prefCharsetStr.Assign(prefCharset);
			PR_Free(prefCharset);
		}
		else
		{
			prefCharsetStr.AssignWithConversion("us-ascii");
		}
		*aCharset = prefCharsetStr.ToNewUnicode();
	}
	else
	{
		*aCharset = mCharset.ToNewUnicode();
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetCharset(const PRUnichar * aCharset)
{
	nsresult rv;

	nsCOMPtr<nsIDBFolderInfo> folderInfo;
	nsCOMPtr<nsIMsgDatabase> db; 
	rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
	if(NS_SUCCEEDED(rv))
	{
		nsAutoString charset(aCharset);
		rv = folderInfo->SetCharacterSet(&charset);
		db->Commit(nsMsgDBCommitType::kLargeCommit);
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetHasNewMessages(PRBool *hasNewMessages)
{
	if(!hasNewMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	*hasNewMessages = mNewMessages;

	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetHasNewMessages(PRBool curNewMessages)
{
	if (curNewMessages != mNewMessages) 
	{
		/** @params
		  * nsIAtom* property, PRBool oldValue, PRBool newValue
		  */
		PRBool oldNewMessages = mNewMessages;
		mNewMessages = curNewMessages;
		NotifyBoolPropertyChanged(kNewMessagesAtom, oldNewMessages, curNewMessages);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetGettingNewMessages(PRBool *gettingNewMessages)
{
	if(!gettingNewMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	*gettingNewMessages = mGettingNewMessages;

	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetGettingNewMessages(PRBool gettingNewMessages)
{
	mGettingNewMessages = gettingNewMessages;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetFirstNewMessage(nsIMessage **firstNewMessage)
{
	//If there's not a db then there can't be new messages.  Return failure since you
	//should use HasNewMessages first.
	if(!mDatabase)
		return NS_ERROR_FAILURE;

	nsresult rv;
	nsMsgKey key;
	rv = mDatabase->GetFirstNew(&key);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMsgDBHdr> hdr;
	rv = mDatabase->GetMsgHdrForKey(key, getter_AddRefs(hdr));
	if(NS_FAILED(rv))
		return rv;

  if (!hdr)
  {
    *firstNewMessage = nsnull;
    return NS_ERROR_FAILURE;
  }
	rv = CreateMessageFromMsgDBHdr(hdr, firstNewMessage);
	if(NS_FAILED(rv))
		return rv;

	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::ClearNewMessages()
{
	nsresult rv = NS_OK;
	//If there's no db then there's nothing to clear.
	if(mDatabase)
	{
		rv = mDatabase->ClearNewList(PR_TRUE);
	}
	return rv;
}

// helper function that gets the cache element that corresponds to the passed in file spec.
// This could be static, or could live in another class - it's not specific to the current
// nsMsgDBFolder. If it lived at a higher level, we could cache the account manager and folder cache.
nsresult nsMsgDBFolder::GetFolderCacheElemFromFileSpec(nsIFileSpec *fileSpec, nsIMsgFolderCacheElement **cacheElement)
{
	nsresult result;
	if (!fileSpec || !cacheElement)
		return NS_ERROR_NULL_POINTER;
	nsCOMPtr <nsIMsgFolderCache> folderCache;
#ifdef DEBUG_bienvenu1
	PRBool exists;
	NS_ASSERTION(NS_SUCCEEDED(fileSpec->Exists(&exists)) && exists, "whoops, file doesn't exist, mac will break");
#endif
	NS_WITH_SERVICE(nsIMsgAccountManager, accountMgr, kMsgAccountManagerCID, &result); 
	if(NS_SUCCEEDED(result))
	{
		result = accountMgr->GetFolderCache(getter_AddRefs(folderCache));
		if (NS_SUCCEEDED(result) && folderCache)
		{
			nsXPIDLCString persistentPath;
			fileSpec->GetPersistentDescriptorString(getter_Copies(persistentPath));
			result = folderCache->GetCacheElement(persistentPath, PR_FALSE, cacheElement);
		}
	}
	return result;
}

nsresult nsMsgDBFolder::ReadDBFolderInfo(PRBool force)
{
	// Since it turns out to be pretty expensive to open and close
	// the DBs all the time, if we have to open it once, get everything
	// we might need while we're here

	nsresult result=NS_ERROR_FAILURE;

	// don't need to reload from cache if we've already read from cache,
	// and, we might get stale info, so don't do it.
	if (!mInitializedFromCache)
	{
		nsCOMPtr <nsIFileSpec> dbPath;

		result = GetFolderCacheKey(getter_AddRefs(dbPath));

		if (dbPath)
		{
			nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
			result = GetFolderCacheElemFromFileSpec(dbPath, getter_AddRefs(cacheElement));
			if (NS_SUCCEEDED(result) && cacheElement)
			{
				result = ReadFromFolderCacheElem(cacheElement);
			}
		}
	}
//	if (m_master->InitFolderFromCache (this))
//		return err;

	if (force || !mInitializedFromCache)
    {
        nsCOMPtr<nsIDBFolderInfo> folderInfo;
        nsCOMPtr<nsIMsgDatabase> db; 
        result = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
        if(NS_SUCCEEDED(result))
        {
			mIsCachable = PR_TRUE;
            if (folderInfo)
            {
              if (!mInitializedFromCache)
              {
                folderInfo->GetFlags((PRInt32 *)&mFlags);
#ifdef DEBUG_bienvenu
                nsXPIDLString name;
                GetName(getter_Copies(name));
                NS_ASSERTION(nsCRT::strcmp(name, "Trash") || (mFlags & MSG_FOLDER_FLAG_TRASH), "lost trash flag");
#endif
                mInitializedFromCache = PR_TRUE;
              }

				folderInfo->GetNumMessages(&mNumTotalMessages);
				folderInfo->GetNumNewMessages(&mNumUnreadMessages);
        folderInfo->GetExpungedBytes((PRInt32 *)&mExpungedBytes);

				//These should be put in IMAP folder only.
				//folderInfo->GetImapTotalPendingMessages(&mNumPendingTotalMessages);
				//folderInfo->GetImapUnreadPendingMessages(&mNumPendingUnreadMessages);

				PRBool defaultUsed;
				folderInfo->GetCharacterSet2(&mCharset, &defaultUsed);
				if (defaultUsed)
					mCharset.AssignWithConversion("");
        
				if (db) {
					PRBool hasnew;
					nsresult rv;
					rv = db->HasNew(&hasnew);
					if (NS_FAILED(rv)) return rv;
					if (!hasnew && mNumPendingUnreadMessages <= 0) {
						ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
					}
				}
            }

        }
		folderInfo = null_nsCOMPtr();
        if (db)
	        db->Close(PR_FALSE);
    }

	return result;
	
}

nsresult nsMsgDBFolder::SendFlagNotifications(nsISupports *item, PRUint32 oldFlags, PRUint32 newFlags)
{
	nsresult rv = NS_OK;

	PRUint32 changedFlags = oldFlags ^ newFlags;
	if((changedFlags & MSG_FLAG_READ) || (changedFlags & MSG_FLAG_REPLIED)
		|| (changedFlags & MSG_FLAG_FORWARDED) || (changedFlags & MSG_FLAG_IMAP_DELETED)
		|| (changedFlags & MSG_FLAG_NEW))
	{
		rv = NotifyPropertyFlagChanged(item, kStatusAtom, oldFlags, newFlags);
	}
	else if((changedFlags & MSG_FLAG_MARKED))
	{
		rv = NotifyPropertyFlagChanged(item, kFlaggedAtom, oldFlags, newFlags);
	}
		return rv;
}

// path coming in is the root path without the leaf name,
// on the way out, it's the whole path.
nsresult nsMsgDBFolder::CreatePlatformLeafNameForDisk(const char *userLeafName, nsFileSpec &path, char **resultName)
{
#if 0
	const int charLimit = MAX_FILE_LENGTH_WITHOUT_EXTENSION;	// set on platform specific basis
#endif
#if defined(XP_MAC)
	nsCAutoString illegalChars(":");
#elif defined(XP_OS2) 
	nsCAutoString illegalChars("\"/\\[]:;=,|?<>*$. ");
#elif defined(XP_WIN32)
	nsCAutoString illegalChars("\"/\\[]:;=,|?<>*$");
#else		// UNIX	(what about beos?)
	nsCAutoString illegalChars;
#endif

	if (!resultName || !userLeafName)
		return NS_ERROR_NULL_POINTER;
	*resultName = nsnull;

	// mangledLeaf is the new leaf name.
	// If userLeafName	(a) contains all legal characters
	//					(b) is within the valid length for the given platform
	//					(c) does not already exist on the disk
	// then we simply return nsCRT::strdup(userLeafName)
	// Otherwise we mangle it

	// mangledPath is the entire path to the newly mangled leaf name
	nsCAutoString mangledLeaf(userLeafName);

	PRInt32 illegalCharacterIndex = mangledLeaf.FindCharInSet(illegalChars);

	if (illegalCharacterIndex == kNotFound)
	{
		path += (const char *) mangledLeaf;
		if (!path.Exists())
		{
		// if there are no illegal characters
		// and the file doesn't already exist, then don't do anything to the string
		// Note that this might be truncated to charLength, but if so, the file still
		// does not exist, so we are OK.
			*resultName = mangledLeaf.ToNewCString();
			return (*resultName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
		}
	}
	else
	{

		// First, replace all illegal characters with '_'
		mangledLeaf.ReplaceChar(illegalChars, '_');

		path += (const char *) mangledLeaf;
	}
	// if we are here, then any of the following may apply:
	//		(a) there were illegal characters
	//		(b) the file already existed

	// Now, we have to loop until we find a filename that doesn't already
	// exist on the disk
	path.SetLeafName(mangledLeaf.GetBuffer());
  path.MakeUnique();
  *resultName = path.GetLeafName();
	return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetMsgDatabase(nsIMsgWindow *aMsgWindow,
                              nsIMsgDatabase** aMsgDatabase)
{
    GetDatabase(aMsgWindow);
    if (!aMsgDatabase || !mDatabase)
        return NS_ERROR_NULL_POINTER;
    *aMsgDatabase = mDatabase;
    NS_ADDREF(*aMsgDatabase);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::OnReadChanged(nsIDBChangeListener * aInstigator)
{
    /* do nothing.  if you care about this, over ride it.  see nsNewsFolder.cpp */
    return NS_OK;
}

// 1.  When the status of a message changes.
NS_IMETHODIMP nsMsgDBFolder::OnKeyChange(nsMsgKey aKeyChanged, PRUint32 aOldFlags, PRUint32 aNewFlags, 
                         nsIDBChangeListener * aInstigator)
{
	nsCOMPtr<nsIMsgDBHdr> pMsgDBHdr;
	nsresult rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(pMsgDBHdr));
	if(NS_SUCCEEDED(rv) && pMsgDBHdr)
	{
		nsCOMPtr<nsIMessage> message;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, getter_AddRefs(message));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(message, &rv));
			if(NS_SUCCEEDED(rv))
			{
				SendFlagNotifications(msgSupports, aOldFlags, aNewFlags);
			}
		  UpdateSummaryTotals(PR_TRUE);
		}
	}

	// The old state was new message state
	// We check and see if this state has changed
	if(aOldFlags & MSG_FLAG_NEW) 
	{
		// state changing from new to something else
		if (!(aNewFlags  & MSG_FLAG_NEW)) 
		{
			CheckWithNewMessagesStatus(PR_FALSE);
		}
	}

	return NS_OK;
}

nsresult nsMsgDBFolder::CheckWithNewMessagesStatus(PRBool messageAdded)
{
	nsresult rv;

	PRBool hasNewMessages;

	if (messageAdded)
	{
		SetHasNewMessages(PR_TRUE);
	}
	else // message modified or deleted
	{
		if(mDatabase)
		{
			rv = mDatabase->HasNew(&hasNewMessages);
			SetHasNewMessages(hasNewMessages);
		}
	}

	return NS_OK;
}

// 3.  When a message gets deleted, we need to see if it was new
//     When we lose a new message we need to check if there are still new messages 
NS_IMETHODIMP nsMsgDBFolder::OnKeyDeleted(nsMsgKey aKeyChanged, nsMsgKey  aParentKey, PRInt32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
    // check to see if a new message is being deleted
    // as in this case, if there is only one new message and it's being deleted
    // the folder newness has to be cleared.
    CheckWithNewMessagesStatus(PR_FALSE);

    //Do both flat and thread notifications
    return OnKeyAddedOrDeleted(aKeyChanged, aParentKey, aFlags, aInstigator, PR_FALSE, PR_TRUE, PR_TRUE);
}

// 2.  When a new messages gets added, we need to see if it's new.
NS_IMETHODIMP nsMsgDBFolder::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	if(aFlags & MSG_FLAG_NEW) {
		CheckWithNewMessagesStatus(PR_TRUE);
	}

	//Do both flat and thread notifications
	return OnKeyAddedOrDeleted(aKeyChanged, aParentKey, aFlags, aInstigator, PR_TRUE, PR_TRUE, PR_TRUE);
}

nsresult nsMsgDBFolder::OnKeyAddedOrDeleted(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator, PRBool added, PRBool doFlat, PRBool doThread)
{
	nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
	nsCOMPtr<nsIMsgDBHdr> parentDBHdr;
	nsresult rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(msgDBHdr));
	if(NS_FAILED(rv))
		return rv;

	rv = mDatabase->GetMsgHdrForKey(aParentKey, getter_AddRefs(parentDBHdr));
	if(NS_FAILED(rv))
		return rv;

	if(msgDBHdr)
	{
		nsCOMPtr<nsIMessage> message;
		rv = CreateMessageFromMsgDBHdr(msgDBHdr, getter_AddRefs(message));
		if(NS_FAILED(rv))
			return rv;

		nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(message));
		nsCOMPtr<nsISupports> folderSupports;
		rv = QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(folderSupports));
		if(msgSupports && NS_SUCCEEDED(rv) && doFlat)
		{
			if(added)
				NotifyItemAdded(folderSupports, msgSupports, "flatMessageView");
			else
				NotifyItemDeleted(folderSupports, msgSupports, "flatMessageView");
		}
		if(doThread)
		{
			if(parentDBHdr)
			{
				nsCOMPtr<nsIMessage> parentMessage;
				rv = CreateMessageFromMsgDBHdr(parentDBHdr, getter_AddRefs(parentMessage));
				if(NS_FAILED(rv))
					return rv;

				nsCOMPtr<nsISupports> parentSupports(do_QueryInterface(parentMessage));
				if(msgSupports && NS_SUCCEEDED(rv))
				{
					if(added)
						NotifyItemAdded(parentSupports, msgSupports, "threadMessageView");
					else
						NotifyItemDeleted(parentSupports, msgSupports, "threadMessageView");

				}
			}
			//if there's not a header then in threaded view the folder is the parent.
			else
			{
				if(msgSupports && folderSupports)
				{
					if(added)
						NotifyItemAdded(folderSupports, msgSupports, "threadMessageView");
					else
						NotifyItemDeleted(folderSupports, msgSupports, "threadMessageView");
				}
			}
		}
		UpdateSummaryTotals(PR_TRUE);
	}
	return NS_OK;

}


NS_IMETHODIMP nsMsgDBFolder::OnParentChanged(nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, 
						nsIDBChangeListener * aInstigator)
{
	//In reality we probably want to just change the parent because otherwise we will lose things like
	//selection.

	//First delete the child from the old threadParent
	OnKeyAddedOrDeleted(aKeyChanged, oldParent, 0, aInstigator, PR_FALSE, PR_FALSE, PR_TRUE);
	//Then add it to the new threadParent
	OnKeyAddedOrDeleted(aKeyChanged, newParent, 0, aInstigator, PR_TRUE, PR_FALSE, PR_TRUE);
	return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *
													 instigator)
{
    if (mDatabase)
    {
        mDatabase->RemoveListener(this);
        mDatabase = null_nsCOMPtr();
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ManyHeadersToDownload(PRBool *retval)
{
	PRInt32 numTotalMessages;

	if (!retval)
		return NS_ERROR_NULL_POINTER;
  *retval = PR_TRUE;

  // is there any reason to return false?
//	if (!mDatabase)
//		*retval = PR_TRUE;
//	else if (NS_SUCCEEDED(GetTotalMessages(PR_FALSE, &numTotalMessages)) && numTotalMessages <= 0)
//		*retval = PR_TRUE;
//	else
//		*retval = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetFlags(PRUint32 *_retval)
{
  ReadDBFolderInfo(PR_FALSE);
	*_retval = mFlags;
	return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element)
{
	nsresult rv = NS_OK;
	char *charset;

	element->GetInt32Property("flags", (PRInt32 *) &mFlags);
	element->GetInt32Property("totalMsgs", &mNumTotalMessages);
	element->GetInt32Property("totalUnreadMsgs", &mNumUnreadMessages);
  element->GetInt32Property("pendingUnreadMsgs", &mNumPendingUnreadMessages);
  element->GetInt32Property("pendingMsgs", &mNumPendingTotalMessages);
  element->GetInt32Property("expungedBytes", (PRInt32 *) &mExpungedBytes);

	element->GetStringProperty("charset", &charset);

#ifdef DEBUG_bienvenu1
	char *uri;

	GetURI(&uri);
	printf("read total %ld for %s\n", mNumTotalMessages, uri);
	PR_Free(uri);
#endif
	mCharset.AssignWithConversion(charset);
	PR_FREEIF(charset);

  mInitializedFromCache = PR_TRUE;
	return rv;
}

nsresult nsMsgDBFolder::GetFolderCacheKey(nsIFileSpec **aFileSpec)
{
	nsresult rv;
	nsCOMPtr <nsIFileSpec> path;
	rv = GetPath(getter_AddRefs(path));

	// now we put a new file spec in aFileSpec, because we're going to change it.
	rv = NS_NewFileSpec(aFileSpec);

	if (NS_SUCCEEDED(rv) && *aFileSpec)
	{
		nsIFileSpec *dbPath = *aFileSpec;
		dbPath->FromFileSpec(path);
		// if not a server, we need to convert to a db Path with .msf on the end
		PRBool isServer = PR_FALSE;
		GetIsServer(&isServer);

		// if it's a server, we don't need the .msf appended to the name
		if (!isServer)
		{
			nsFileSpec		folderName;
			dbPath->GetFileSpec(&folderName);
			nsLocalFolderSummarySpec	summarySpec(folderName);

			dbPath->SetFromFileSpec(summarySpec);
		}
	}
	return rv;
}

nsresult nsMsgDBFolder::FlushToFolderCache()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && accountManager)
  {
    nsCOMPtr<nsIMsgFolderCache> folderCache;

    rv = accountManager->GetFolderCache(getter_AddRefs(folderCache));
    if (NS_SUCCEEDED(rv) && folderCache)
      rv = WriteToFolderCache(folderCache, PR_FALSE);
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::WriteToFolderCache(nsIMsgFolderCache *folderCache, PRBool deep)
{
	nsCOMPtr <nsIEnumerator> aEnumerator;
  nsresult rv;

	if (folderCache)
	{
		nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
		nsCOMPtr <nsIFileSpec> dbPath;

		rv = GetFolderCacheKey(getter_AddRefs(dbPath));
#ifdef DEBUG_bienvenu1
		PRBool exists;
		NS_ASSERTION(NS_SUCCEEDED(dbPath->Exists(&exists)) && exists, "file spec we're adding to cache should exist");
#endif
		if (NS_SUCCEEDED(rv) && dbPath)
		{
			nsXPIDLCString persistentPath;
			dbPath->GetPersistentDescriptorString(getter_Copies(persistentPath));
			rv = folderCache->GetCacheElement(persistentPath, PR_TRUE, getter_AddRefs(cacheElement));
			if (NS_SUCCEEDED(rv) && cacheElement)
				rv = WriteToFolderCacheElem(cacheElement);
		}
	}

  if (!deep)
    return rv;

	rv = GetSubFolders(getter_AddRefs(aEnumerator));
	if(NS_FAILED(rv)) 
		return rv;

	nsCOMPtr<nsISupports> aItem;

	rv = aEnumerator->First();
	if (!NS_SUCCEEDED(rv))
		return NS_OK;	// it's OK, there are no sub-folders.

	while(NS_SUCCEEDED(rv))
	{
		rv = aEnumerator->CurrentItem(getter_AddRefs(aItem));
		if (NS_FAILED(rv)) break;
		nsCOMPtr<nsIMsgFolder> aMsgFolder(do_QueryInterface(aItem, &rv));
		if (NS_SUCCEEDED(rv))
		{
			if (folderCache)
			{
				rv = aMsgFolder->WriteToFolderCache(folderCache, PR_TRUE);
				if (!NS_SUCCEEDED(rv))
					break;
			}
		}
		rv = aEnumerator->Next();
		if (!NS_SUCCEEDED(rv))
		{
			rv = NS_OK;
			break;
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::WriteToFolderCacheElem(nsIMsgFolderCacheElement *element)
{
	nsresult rv = NS_OK;

	element->SetInt32Property("flags", (PRInt32) mFlags);
	element->SetInt32Property("totalMsgs", mNumTotalMessages);
	element->SetInt32Property("totalUnreadMsgs", mNumUnreadMessages);
  element->SetInt32Property("pendingUnreadMsgs", mNumPendingUnreadMessages);
  element->SetInt32Property("pendingMsgs", mNumPendingTotalMessages);
  element->SetInt32Property("expungedBytes", mExpungedBytes);

  nsCAutoString mcharsetC;
  mcharsetC.AssignWithConversion(mCharset);
	element->SetStringProperty("charset", (const char *) mcharsetC);

#ifdef DEBUG_bienvenu1
	char *uri;

	GetURI(&uri);
	printf("writing total %ld for %s\n", mNumTotalMessages, uri);
	PR_Free(uri);
#endif
	return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::SetFlag(PRUint32 flag)
{
  ReadDBFolderInfo(PR_FALSE);
  return nsMsgFolder::SetFlag(flag);
}

NS_IMETHODIMP
nsMsgDBFolder::MarkAllMessagesRead(void)
{
	// ### fix me need nsIMsgWindow
	nsresult rv = GetDatabase(nsnull);
	
	if(NS_SUCCEEDED(rv))
  {
    EnableNotifications(allMessageCountNotifications, PR_FALSE);
		rv = mDatabase->MarkAllRead(nsnull);
    EnableNotifications(allMessageCountNotifications, PR_TRUE);
  }
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::MarkThreadRead(nsIMsgThread *thread)
{

	nsresult rv = GetDatabase(nsnull);
	if(NS_SUCCEEDED(rv))
		return mDatabase->MarkThreadRead(thread, nsnull, nsnull);

	return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::OnStartRunningUrl(nsIURI *aUrl)
{
	NS_PRECONDITION(aUrl, "just a sanity check");
    return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::OnStopRunningUrl(nsIURI *aUrl, nsresult aExitCode)
{
	NS_PRECONDITION(aUrl, "just a sanity check");
	nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
	if (mailUrl)
	{
		PRBool updatingFolder = PR_FALSE;
		if (NS_SUCCEEDED(mailUrl->GetUpdatingFolder(&updatingFolder)) && updatingFolder)
		{
      NotifyFolderEvent(mFolderLoadedAtom);

//GGGG			 check for new mail here and call SetNewMessages...?? -- ONE OF THE 2 PLACES
			if(mDatabase)
			{
				nsresult rv;
				PRBool hasNewMessages;

				rv = mDatabase->HasNew(&hasNewMessages);
				SetHasNewMessages(hasNewMessages);
			}
		}

    // be sure to remove ourselves as a url listener
    mailUrl->UnRegisterListener(this);
	}
    return NS_OK;
}

