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

NS_IMPL_ADDREF_INHERITED(nsMsgDBFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgDBFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgDBFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(NS_GET_IID(nsIDBChangeListener)))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIDBChangeListener*, this);
	}              
	else if (aIID.Equals(NS_GET_IID(nsIUrlListener)))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIUrlListener*, this);
	}              

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgFolder::QueryInterface(aIID, aInstancePtr);
}

nsMsgDBFolder::nsMsgDBFolder(void)
: mCharset(""), mAddListener(PR_TRUE)
{

}

nsMsgDBFolder::~nsMsgDBFolder(void)
{
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

		char *prefCharset = nsnull;
		if (NS_SUCCEEDED(rv))
		{
			rv = prefs->CopyCharPref("mailnews.send_default_charset", &prefCharset);
		}
  
		nsAutoString prefCharsetStr;
		if(prefCharset)
		{
			prefCharsetStr = prefCharset;
			PR_Free(prefCharset);
		}
		else
		{
			prefCharsetStr = "us-ascii";
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

NS_IMETHODIMP nsMsgDBFolder::HasNewMessages(PRBool *hasNewMessages)
{
	if(!hasNewMessages)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = GetDatabase(nsnull);

	if(NS_SUCCEEDED(rv))
	{
		rv = mDatabase->HasNew(hasNewMessages);
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetFirstNewMessage(nsIMessage **firstNewMessage)
{
	nsresult rv = GetDatabase(nsnull);

	if(NS_SUCCEEDED(rv))
	{
		nsMsgKey key;
		rv = mDatabase->GetFirstNew(&key);
		if(NS_FAILED(rv))
			return rv;

		nsCOMPtr<nsIMsgDBHdr> hdr;
		rv = mDatabase->GetMsgHdrForKey(key, getter_AddRefs(hdr));
		if(NS_FAILED(rv))
			return rv;

		rv = CreateMessageFromMsgDBHdr(hdr, firstNewMessage);
		if(NS_FAILED(rv))
			return rv;

	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::ClearNewMessages()
{
	nsresult rv = GetDatabase(nsnull);

	if(NS_SUCCEEDED(rv))
	{
		rv = mDatabase->ClearNewList(PR_FALSE);
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

	nsresult result;

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

	            folderInfo->GetFlags((PRInt32 *)&mFlags);
              mInitializedFromCache = PR_TRUE;

				folderInfo->GetNumMessages(&mNumTotalMessages);
				folderInfo->GetNumNewMessages(&mNumUnreadMessages);

				//These should be put in IMAP folder only.
				//folderInfo->GetImapTotalPendingMessages(&mNumPendingTotalMessages);
				//folderInfo->GetImapUnreadPendingMessages(&mNumPendingUnreadMessages);

				folderInfo->GetCharacterSet(&mCharset);
        
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
		|| (changedFlags & MSG_FLAG_FORWARDED)|| (changedFlags & MSG_FLAG_NEW))
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
	nsCAutoString illegalChars = ":";
#elif defined(XP_OS2) 
	nsCAutoString illegalChars = "\"/\\[]:;=,|?<>*$. ";
#elif defined(XP_WIN32)
	nsCAutoString illegalChars = "\"/\\[]:;=,|?<>*$";
#else		// UNIX	(what about beos?)
	nsCAutoString illegalChars = "";
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

	// leafLength is the length of mangledLeaf which we will return
	// if userLeafName is greater than the maximum allowed for this
	// platform, then we truncate and mangle it.  Otherwise leave it alone.
	PRInt32 leafLength;

	// mangledPath is the entire path to the newly mangled leaf name
	nsCAutoString mangledLeaf = userLeafName;
	
	PRInt32 illegalCharacterIndex = mangledLeaf.FindCharInSet(illegalChars);

	PRBool exists;

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
	PRBool nameSpaceExhausted = PR_FALSE;
	nsXPIDLCString leafName;
	
	path.SetLeafName(mangledLeaf.GetBuffer());
	exists = path.Exists();
	leafLength = mangledLeaf.Length();

	if (exists)
	{
		if (leafLength >= 2) 
			mangledLeaf.SetCharAt('A', leafLength - 2);
		mangledLeaf.SetCharAt('A', leafLength - 1);	// leafLength must be at least 1
	}

	while (!nameSpaceExhausted && path.Exists())
	{
		if (leafLength >= 2)
		{
			PRUnichar lastChar = mangledLeaf.CharAt(leafLength - 1);
			mangledLeaf.SetCharAt(++lastChar,leafLength - 1);
			if (lastChar > 'Z')
			{
				mangledLeaf.SetCharAt('A',leafLength - 1);
				PRUnichar nextToLastChar = mangledLeaf.CharAt(leafLength - 2);
				mangledLeaf.SetCharAt(nextToLastChar + 1, leafLength - 2);
				nameSpaceExhausted = (nextToLastChar == 'Z');
			}
		}
		else
		{
			PRUnichar lastChar = mangledLeaf.CharAt(leafLength - 1);
			mangledLeaf.SetCharAt(++lastChar, leafLength - 1);
			nameSpaceExhausted = (lastChar == 'Z');
		}
	}
	*resultName = mangledLeaf.ToNewCString();
	
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
	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnKeyDeleted(nsMsgKey aKeyChanged, nsMsgKey  aParentKey, PRInt32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
	//Do both flat and thread notifications
	return OnKeyAddedOrDeleted(aKeyChanged, aParentKey, aFlags, aInstigator, PR_FALSE, PR_TRUE, PR_TRUE);
}

NS_IMETHODIMP nsMsgDBFolder::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
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
	if (!mDatabase)
		*retval = PR_TRUE;
	else if (NS_SUCCEEDED(GetTotalMessages(PR_FALSE, &numTotalMessages)) && numTotalMessages <= 0)
		*retval = PR_TRUE;
	else
		*retval = PR_FALSE;
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

	element->GetStringProperty("charset", &charset);

#ifdef DEBUG_bienvenu1
	char *uri;

	GetURI(&uri);
	printf("read total %ld for %s\n", mNumTotalMessages, uri);
	PR_Free(uri);
#endif
	mCharset = charset;
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

NS_IMETHODIMP nsMsgDBFolder::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	nsCOMPtr <nsIEnumerator> aEnumerator;

	nsresult rv = GetSubFolders(getter_AddRefs(aEnumerator));
	if(NS_FAILED(rv)) 
		return rv;

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
				rv = aMsgFolder->WriteToFolderCache(folderCache);
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

	element->SetStringProperty("charset", (const char *) nsCAutoString(mCharset));

#ifdef DEBUG_bienvenu1
	char *uri;

	GetURI(&uri);
	printf("writing total %ld for %s\n", mNumTotalMessages, uri);
	PR_Free(uri);
#endif
	return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::MarkAllMessagesRead(void)
{
	// ### fix me need nsIMsgWindow
	nsresult rv = GetDatabase(nsnull);
	
	if(NS_SUCCEEDED(rv))
		return mDatabase->MarkAllRead(nsnull);

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
			NotifyFolderLoaded();
		}

    // be sure to remove ourselves as a url listener
    mailUrl->UnRegisterListener(this);
	}
    return NS_OK;
}

