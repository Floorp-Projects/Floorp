/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"
#include "xp_core.h"
#include "nsReadableUtils.h"
#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIPref.h"
#include "nsIFileChannel.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsIFileStream.h"
#include "nsIChannel.h"
#include "nsITransport.h"
#include "nsIFileTransportService.h"
#include "nsIMsgFolderCompactor.h"
#include "nsIDocShell.h"
#include "nsIMsgWindow.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"


#define oneHour 3600000000
#include "nsMsgUtils.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static PRTime gtimeOfLastPurgeCheck = 0;    //variable to know when to check for purge_threshhold


#define PREF_MAIL_PROMPT_PURGE_THRESHOLD "mail.prompt_purge_threshhold"
#define PREF_MAIL_PURGE_THRESHOLD "mail.purge_threshhold"

nsIAtom* nsMsgDBFolder::mFolderLoadedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mDeleteOrMoveMsgCompletedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mDeleteOrMoveMsgFailedAtom=nsnull;
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
    mDeleteOrMoveMsgFailedAtom = NS_NewAtom("DeleteOrMoveMsgFailed");
  }
}

nsMsgDBFolder::~nsMsgDBFolder(void)
{
  if (--mInstanceCount == 0) {
    NS_IF_RELEASE(mFolderLoadedAtom);
    NS_IF_RELEASE(mDeleteOrMoveMsgCompletedAtom);
    NS_IF_RELEASE(mDeleteOrMoveMsgFailedAtom);
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
		mDatabase = nsnull;

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
   NotifyStoreClosedAllHeaders();

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
        mDatabase = nsnull;
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


NS_IMETHODIMP nsMsgDBFolder::GetCharset(PRUnichar * *aCharset)
{
	nsresult rv = NS_OK;
	if(!aCharset)
		return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
  {
    nsXPIDLCString  charset;
    rv = folderInfo->GetCharPtrCharacterSet(getter_Copies(charset));
    if(NS_SUCCEEDED(rv))
    {
      *aCharset = ToNewUnicode(charset);
    }
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
		mCharset.Assign(aCharset);  // synchronize member variable
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetCharsetOverride(PRBool *aCharsetOverride)
{
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
    rv = folderInfo->GetCharacterSetOverride(aCharsetOverride);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetCharsetOverride(PRBool aCharsetOverride)
{
  nsresult rv;

  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
  {
    rv = folderInfo->SetCharacterSetOverride(aCharsetOverride);
    db->Commit(nsMsgDBCommitType::kLargeCommit);
    mCharsetOverride = aCharsetOverride;  // synchronize member variable
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

NS_IMETHODIMP nsMsgDBFolder::GetFirstNewMessage(nsIMsgDBHdr **firstNewMessage)
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

	return  mDatabase->GetMsgHdrForKey(key, firstNewMessage);
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
	nsCOMPtr<nsIMsgAccountManager> accountMgr = 
	         do_GetService(kMsgAccountManagerCID, &result); 
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
				folderInfo->GetCharacterSet(&mCharset, &defaultUsed);
				if (defaultUsed)
					mCharset.AssignWithConversion("");
				folderInfo->GetCharacterSetOverride(&mCharsetOverride);
        
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
		folderInfo = nsnull;
        if (db)
	        db->Close(PR_FALSE);
    }

	return result;
	
}

nsresult nsMsgDBFolder::SendFlagNotifications(nsISupports *item, PRUint32 oldFlags, PRUint32 newFlags)
{
	nsresult rv = NS_OK;

	PRUint32 changedFlags = oldFlags ^ newFlags;
   
    if((changedFlags & MSG_FLAG_READ)  && (changedFlags & MSG_FLAG_NEW))
    {
      //..so..if the msg is read in the folder and the folder has new msgs clear the account level and status bar biffs.
      rv = NotifyPropertyFlagChanged(item, kStatusAtom, oldFlags, newFlags);
      rv = SetBiffState(newFlags);
    }
    else if(changedFlags & (MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED
      | MSG_FLAG_IMAP_DELETED | MSG_FLAG_NEW | MSG_FLAG_OFFLINE))
	{
      rv = NotifyPropertyFlagChanged(item, kStatusAtom, oldFlags, newFlags);
    }
    else if((changedFlags & MSG_FLAG_MARKED))
    {
      rv = NotifyPropertyFlagChanged(item, kFlaggedAtom, oldFlags, newFlags);
    }
    return rv;
}

NS_IMETHODIMP nsMsgDBFolder::DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *)
{
  NS_ASSERTION(PR_FALSE, "imap and news need to override this");
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow)
{
  NS_ASSERTION(PR_FALSE, "imap and news need to override this");
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetOfflineStoreInputStream(nsIInputStream **stream)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (mPath)
    rv = mPath->GetInputStream(stream);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetOfflineFileTransport(nsMsgKey msgKey, PRUint32 *offset, PRUint32 *size, nsITransport **aFileChannel)
{
  NS_ENSURE_ARG(aFileChannel);

  *offset = *size = 0;
  
  nsresult rv;

  rv = nsComponentManager::CreateInstance(NS_LOCALFILECHANNEL_CONTRACTID, nsnull, 
              NS_GET_IID(nsIFileChannel), (void **) aFileChannel);
  if (*aFileChannel)
  {
    nsXPIDLCString nativePath;
    mPath->GetNativePath(getter_Copies(nativePath));

    nsCOMPtr <nsILocalFile> localStore;
    rv = NS_NewLocalFile(nativePath, PR_TRUE, getter_AddRefs(localStore));
    if (NS_SUCCEEDED(rv) && localStore)
    {
      NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
      nsCOMPtr<nsIFileTransportService> fts = 
               do_GetService(kFileTransportServiceCID, &rv);
    
      if (NS_FAILED(rv))
        return rv;
      
      rv = fts->CreateTransport(localStore, PR_RDWR | PR_CREATE_FILE, 0664, aFileChannel);

      if (NS_SUCCEEDED(rv))
      {

	      nsCOMPtr<nsIMsgDBHdr> hdr;
	      rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(hdr));
        if (hdr && NS_SUCCEEDED(rv))
        {
          hdr->GetMessageOffset(offset);
          hdr->GetOfflineMessageSize(size);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetOfflineStoreOutputStream(nsIOutputStream **outputStream)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (mPath)
  {
    // the following code doesn't work for a host of reasons - the transfer offset
    // is ignored for output streams. The buffering used by file channels does not work
    // if transfer offsets are coerced to work, etc.
#if 0
    nsCOMPtr<nsIFileChannel> fileChannel = do_CreateInstance(NS_LOCALFILECHANNEL_CONTRACTID);
    if (fileChannel)
    {
      nsCOMPtr <nsILocalFile> localStore;
      rv = NS_NewLocalFile(nativePath, PR_TRUE, getter_AddRefs(localStore));
      if (NS_SUCCEEDED(rv) && localStore)
      {
        rv = fileChannel->Init(localStore, PR_CREATE_FILE | PR_RDWR, 0);
        if (NS_FAILED(rv)) 
          return rv; 
        rv = fileChannel->Open(outputStream);
        if (NS_FAILED(rv)) 
          return rv; 
      }
    }
#endif
    nsCOMPtr<nsISupports>  supports;
    nsFileSpec fileSpec;
    mPath->GetFileSpec(&fileSpec);
    rv = NS_NewIOFileStream(getter_AddRefs(supports), fileSpec, PR_WRONLY | PR_CREATE_FILE, 00700);
    supports->QueryInterface(NS_GET_IID(nsIOutputStream), (void **) outputStream);

    nsCOMPtr <nsIRandomAccessStore> randomStore = do_QueryInterface(supports);
    if (randomStore)
      randomStore->Seek(PR_SEEK_END, 0);
  }
  return rv;
}

// XXX todo
// move these to a common location and remove all the hard coded ".msf"
#define SUMMARY_SUFFIX ".msf"
#define SUMMARY_SUFFIX_LEN 4

// path coming in is the root path without the leaf name,
// on the way out, it's the whole path.
nsresult nsMsgDBFolder::CreateFileSpecForDB(const char *userLeafName, nsFileSpec &path, nsIFileSpec **dbFileSpec)
{
  NS_ENSURE_ARG_POINTER(dbFileSpec);
  NS_ENSURE_ARG_POINTER(userLeafName);
  nsCAutoString proposedDBName(userLeafName);
  NS_MsgHashIfNecessary(proposedDBName);

  // (note, the caller of this will be using the dbFileSpec to call db->Open() 
  // will turn the path into summary spec, and append the ".msf" extension)
  //
  // we want db->Open() to create a new summary file
  // so we have to jump through some hoops to make sure the .msf it will
  // create is unique.  now that we've got the "safe" proposedDBName,
  // we append ".msf" to see if the file exists.  if so, we make the name
  // unique and then string off the ".msf" so that we pass the right thing
  // into Open().  this isn't ideal, since this is not atomic
  // but it will make do.
  proposedDBName+= SUMMARY_SUFFIX;
  path += proposedDBName.get();
  if (path.Exists()) 
  {
    path.MakeUnique();
    proposedDBName = path.GetLeafName();
  }
  // now, take the ".msf" off
  proposedDBName.Truncate(proposedDBName.Length() - SUMMARY_SUFFIX_LEN);
  path.SetLeafName(proposedDBName);

  NS_NewFileSpecWithSpec(path, dbFileSpec);
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
nsMsgDBFolder::SetMsgDatabase(nsIMsgDatabase *aMsgDatabase)
{
  if (mDatabase)
  {
    // commit here - db might go away when all these refs are released.
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
    mDatabase->RemoveListener(this);
    mDatabase->ClearCachedHdrs();
  }
  mDatabase = aMsgDatabase;
  if (aMsgDatabase)
    aMsgDatabase->AddListener(this);
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
		nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(pMsgDBHdr, &rv));
		if(NS_SUCCEEDED(rv))
			SendFlagNotifications(msgSupports, aOldFlags, aNewFlags);
		UpdateSummaryTotals(PR_TRUE);
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
		nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(msgDBHdr));
		nsCOMPtr<nsISupports> folderSupports;
		rv = QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(folderSupports));
		if(msgSupports && NS_SUCCEEDED(rv) && doFlat)
		{
			if(added)
				NotifyItemAdded(folderSupports, msgSupports, "flatMessageView");
			else
				NotifyItemDeleted(folderSupports, msgSupports, "flatMessageView");
		}
		if(msgSupports && folderSupports)
		{
			if(added)
				NotifyItemAdded(folderSupports, msgSupports, "threadMessageView");
			else
				NotifyItemDeleted(folderSupports, msgSupports, "threadMessageView");
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
        mDatabase = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ManyHeadersToDownload(PRBool *retval)
{
	//PRInt32 numTotalMessages;

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

nsresult nsMsgDBFolder::MsgFitsDownloadCriteria(nsMsgKey msgKey, PRBool *result)
{
	if(!mDatabase)
		return NS_ERROR_FAILURE;

	nsresult rv;
	nsCOMPtr<nsIMsgDBHdr> hdr;
	rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(hdr));
	if(NS_FAILED(rv))
		return rv;

  if (hdr)
  {
    PRUint32 msgFlags = 0;

    hdr->GetFlags(&msgFlags);
    // check if we already have this message body offline
    if (! (msgFlags & MSG_FLAG_OFFLINE))
    {
      *result = PR_TRUE;
    // check against the server download size limit .
      nsCOMPtr <nsIMsgIncomingServer> incomingServer;
      rv = GetServer(getter_AddRefs(incomingServer));
      if (NS_SUCCEEDED(rv) && incomingServer)
      {
        PRBool limitDownloadSize = PR_FALSE;
        rv = incomingServer->GetLimitOfflineMessageSize(&limitDownloadSize);
        NS_ENSURE_SUCCESS(rv, rv);
        if (limitDownloadSize)
        {
          PRInt32 maxDownloadMsgSize = 0;
          PRUint32 msgSize;
          hdr->GetMessageSize(&msgSize);
          rv = incomingServer->GetMaxMessageSize(&maxDownloadMsgSize);
          NS_ENSURE_SUCCESS(rv, rv);
          maxDownloadMsgSize *= 1024;
          if (msgSize > (PRUint32) maxDownloadMsgSize)
            *result = PR_FALSE;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetSupportsOffline(PRBool *aSupportsOffline)
{  
   NS_ENSURE_ARG_POINTER(aSupportsOffline);

   nsCOMPtr<nsIMsgIncomingServer> server;
   nsresult rv = GetServer(getter_AddRefs(server));
   NS_ENSURE_SUCCESS(rv,rv);
   if (!server) return NS_ERROR_FAILURE;
  
   PRInt32 offlineSupportLevel;
   rv = server->GetOfflineSupportLevel(&offlineSupportLevel);
   NS_ENSURE_SUCCESS(rv,rv);

   *aSupportsOffline = (offlineSupportLevel >= OFFLINE_SUPPORT_LEVEL_REGULAR);
   return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ShouldStoreMsgOffline(nsMsgKey msgKey, PRBool *result)
{
  NS_ENSURE_ARG(result);
  PRUint32 flags = 0;
  *result = PR_FALSE;
  GetFlags(&flags);

  if (flags & MSG_FOLDER_FLAG_OFFLINE)
    return MsgFitsDownloadCriteria(msgKey, result);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::HasMsgOffline(nsMsgKey msgKey, PRBool *result)
{
  NS_ENSURE_ARG(result);
  *result = PR_FALSE;
	if(!mDatabase)
		return NS_ERROR_FAILURE;

	nsresult rv;
	nsCOMPtr<nsIMsgDBHdr> hdr;
	rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(hdr));
	if(NS_FAILED(rv))
		return rv;

  if (hdr)
  {
    PRUint32 msgFlags = 0;

    hdr->GetFlags(&msgFlags);
    // check if we already have this message body offline
    if ((msgFlags & MSG_FLAG_OFFLINE))
      *result = PR_TRUE;
  }
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
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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
nsMsgDBFolder::AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag)
{
  NS_ENSURE_ARG_POINTER(aMessage);

  nsresult rv = GetDatabase(nsnull);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  
  nsMsgKey msgKey;
  aMessage->GetMessageKey(&msgKey);
  
  if (aDispositionFlag == nsIMsgFolder::nsMsgDispositionState_Replied)
    mDatabase->MarkReplied(msgKey, PR_TRUE, nsnull);
  else if (aDispositionFlag == nsIMsgFolder::nsMsgDispositionState_Forwarded)
    mDatabase->MarkForwarded(msgKey, PR_TRUE, nsnull);
  return NS_OK;
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
    mDatabase->SetSummaryValid(PR_TRUE);
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
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

NS_IMETHODIMP nsMsgDBFolder::GetRetentionSettings(nsIMsgRetentionSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  nsresult rv = NS_OK;
  if (!m_retentionSettings)
  {
    GetDatabase(nsnull);
    if (mDatabase)
    {
      // get the settings from the db - if the settings from the db say the folder
      // is not overriding the incoming server settings, get the settings from the
      // server.
      rv = mDatabase->GetMsgRetentionSettings(getter_AddRefs(m_retentionSettings));
      if (NS_SUCCEEDED(rv) && m_retentionSettings)
      {
        PRBool useServerDefaults;
        m_retentionSettings->GetUseServerDefaults(&useServerDefaults);

        if (useServerDefaults)
        {
          nsCOMPtr <nsIMsgIncomingServer> incomingServer;
          rv = GetServer(getter_AddRefs(incomingServer));
          if (NS_SUCCEEDED(rv) && incomingServer)
            incomingServer->GetRetentionSettings(getter_AddRefs(m_retentionSettings));
        }

      }
    }
  }
  *settings = m_retentionSettings;
  NS_IF_ADDREF(*settings);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetRetentionSettings(nsIMsgRetentionSettings *settings)
{
  m_retentionSettings = settings;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetDownloadSettings(nsIMsgDownloadSettings **settings)
{
  NS_ENSURE_ARG_POINTER(settings);
  nsresult rv = NS_OK;
  if (!m_downloadSettings)
  {
    GetDatabase(nsnull);
    if (mDatabase)
    {
      // get the settings from the db - if the settings from the db say the folder
      // is not overriding the incoming server settings, get the settings from the
      // server.
      rv = mDatabase->GetMsgDownloadSettings(getter_AddRefs(m_downloadSettings));
      if (NS_SUCCEEDED(rv) && m_downloadSettings)
      {
        PRBool useServerDefaults;
        m_downloadSettings->GetUseServerDefaults(&useServerDefaults);

        if (useServerDefaults)
        {
          nsCOMPtr <nsIMsgIncomingServer> incomingServer;
          rv = GetServer(getter_AddRefs(incomingServer));
          if (NS_SUCCEEDED(rv) && incomingServer)
            incomingServer->GetDownloadSettings(getter_AddRefs(m_downloadSettings));
        }

      }
    }
  }
  *settings = m_downloadSettings;
  NS_IF_ADDREF(*settings);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetDownloadSettings(nsIMsgDownloadSettings *settings)
{
  m_downloadSettings = settings;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::IsCommandEnabled(const char *command, PRBool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = PR_TRUE;
  return NS_OK;
}

nsresult nsMsgDBFolder::NotifyStoreClosedAllHeaders()
{
  // don't need this anymore.
  return NS_OK;
}


nsresult nsMsgDBFolder::WriteStartOfNewLocalMessage()
{
  nsCAutoString result;
  char *ct;
  PRUint32 writeCount;
  time_t now = time ((time_t*) 0);
  ct = ctime(&now);
  ct[24] = 0;
  result = "From - ";
  result += ct;
  result += MSG_LINEBREAK;
  
  nsCOMPtr <nsIRandomAccessStore> randomStore;
  PRInt32 curStorePos;

  if (m_offlineHeader)
    randomStore = do_QueryInterface(m_tempMessageStream);

  if (randomStore)
  {
    randomStore->Tell(&curStorePos);
    m_offlineHeader->SetMessageOffset(curStorePos);
  }
  m_tempMessageStream->Write(result.get(), result.Length(),
                             &writeCount);
  if (randomStore)
  {
    m_tempMessageStream->Flush();
    randomStore->Tell(&curStorePos);
    m_offlineHeader->SetStatusOffset(curStorePos);
  }

  result = "X-Mozilla-Status: 0001";
  result += MSG_LINEBREAK;
  m_tempMessageStream->Write(result.get(), result.Length(),
                             &writeCount);
  result =  "X-Mozilla-Status2: 00000000";
  result += MSG_LINEBREAK;
  nsresult rv = m_tempMessageStream->Write(result.get(), result.Length(),
                             &writeCount);
  return rv;
}

nsresult nsMsgDBFolder::StartNewOfflineMessage()
{
  nsresult rv = GetOfflineStoreOutputStream(getter_AddRefs(m_tempMessageStream));
  WriteStartOfNewLocalMessage();
  m_numOfflineMsgLines = 0;
  return rv;
}

nsresult nsMsgDBFolder::EndNewOfflineMessage()
{
  nsCOMPtr <nsIRandomAccessStore> randomStore;
  PRInt32 curStorePos;
  PRUint32 messageOffset;
  nsMsgKey messageKey;

  m_offlineHeader->GetMessageKey(&messageKey);
  if (m_tempMessageStream)
    randomStore = do_QueryInterface(m_tempMessageStream);

  mDatabase->MarkOffline(messageKey, PR_TRUE, nsnull);
  if (randomStore)
  {
    m_tempMessageStream->Flush();

    randomStore->Tell(&curStorePos);
    m_offlineHeader->GetMessageOffset(&messageOffset);
    m_offlineHeader->SetOfflineMessageSize(curStorePos - messageOffset);
    m_offlineHeader->SetLineCount(m_numOfflineMsgLines);
  }
  m_offlineHeader = nsnull;
  return NS_OK;
}

nsresult nsMsgDBFolder::CompactOfflineStore(nsIMsgWindow *inWindow)
{
  nsresult rv;
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor =  do_CreateInstance(NS_MSGOFFLINESTORECOMPACTOR_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && folderCompactor)
  {
    GetDatabase(nsnull);
    if (NS_SUCCEEDED(rv)) 
    {
      nsCOMPtr<nsIFileSpec> pathSpec;
      rv = GetPath(getter_AddRefs(pathSpec));
      rv = folderCompactor->Init(this, mBaseMessageURI, mDatabase, pathSpec, inWindow);
      if (NS_SUCCEEDED(rv))
        rv = folderCompactor->StartCompacting();
    }
  }
  return rv;
}

nsresult
nsMsgDBFolder::AutoCompact(nsIMsgWindow *aWindow) 
{
   NS_ENSURE_ARG_POINTER(aWindow);
   nsresult rv = NS_OK;
   PRTime timeNow = PR_Now();  //time in microsec
   PRTime timeOfLastPurgeCheck = gtimeOfLastPurgeCheck;
   PRBool prompt;
   rv = GetPromptPurgeThreshold(&prompt);
   NS_ENSURE_SUCCESS(rv, rv);
   timeOfLastPurgeCheck += oneHour;   
   if ((timeOfLastPurgeCheck == oneHour || timeOfLastPurgeCheck < timeNow) && prompt)
   {
     nsCOMPtr<nsIMsgAccountManager> accountMgr = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
     if (NS_SUCCEEDED(rv))
     {
       nsCOMPtr<nsISupportsArray> allServers;
       accountMgr->GetAllServers(getter_AddRefs(allServers));
       NS_ENSURE_SUCCESS(rv,rv);
       PRUint32 numServers, serverIndex=0;
       rv = allServers->Count(&numServers);
       PRInt32 offlineSupportLevel;
       if ( numServers > 0 )
       {
         nsCOMPtr <nsISupports> serverSupports = getter_AddRefs(allServers->ElementAt(serverIndex));
         nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverSupports);
         NS_ENSURE_SUCCESS(rv,rv);
         nsCOMPtr<nsISupportsArray> folderArray;
         nsCOMPtr<nsISupportsArray> offlineFolderArray;
         NS_NewISupportsArray(getter_AddRefs(folderArray));
         NS_NewISupportsArray(getter_AddRefs(offlineFolderArray));
         PRInt32 totalExpungedBytes=0;
         PRInt32 offlineExpungedBytes =0;
         PRInt32 localExpungedBytes = 0;
         do 
         {
           nsCOMPtr<nsIFolder> rootFolder;
           rv = server->GetRootFolder(getter_AddRefs(rootFolder));
           if(NS_SUCCEEDED(rv) && rootFolder)
           {  
             rv = server->GetOfflineSupportLevel(&offlineSupportLevel);
             NS_ENSURE_SUCCESS(rv,rv);
             nsCOMPtr<nsISupportsArray> allDescendents;
             NS_NewISupportsArray(getter_AddRefs(allDescendents));
             rootFolder->ListDescendents(allDescendents);
             PRUint32 cnt=0;
             rv = allDescendents->Count(&cnt);
             NS_ENSURE_SUCCESS(rv,rv);
             PRUint32 expungedBytes=0;
         
             if (offlineSupportLevel > 0)
             {
               PRUint32 flags;
               for (PRUint32 i=0; i< cnt;i++)
               {
                 nsCOMPtr<nsISupports> supports = getter_AddRefs(allDescendents->ElementAt(i));
                 nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
                 expungedBytes = 0;
                 folder->GetFlags(&flags);
                 if (flags & MSG_FOLDER_FLAG_OFFLINE)
                   folder->GetExpungedBytes(&expungedBytes);
                 if (expungedBytes > 0 )
                 { 
                   offlineFolderArray->AppendElement(supports);
                   offlineExpungedBytes += expungedBytes;
                 }
               }
             }
             else  //pop or local
             {
               for (PRUint32 i=0; i< cnt;i++)
               {
                 nsCOMPtr<nsISupports> supports = getter_AddRefs(allDescendents->ElementAt(i));
                 nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
                 folder->GetExpungedBytes(&expungedBytes);
                 if (expungedBytes > 0 )
                 {
                   folderArray->AppendElement(supports);
                   localExpungedBytes += expungedBytes;
                 }
               }
             }
           }
           serverSupports = getter_AddRefs(allServers->ElementAt(++serverIndex));
           server = do_QueryInterface(serverSupports, &rv);
         }
         while (serverIndex < numServers);
         totalExpungedBytes = localExpungedBytes + offlineExpungedBytes;
         PRInt32 purgeThreshold;
         rv = GetPurgeThreshold(&purgeThreshold);
         NS_ENSURE_SUCCESS(rv, rv);
         if (totalExpungedBytes > (purgeThreshold*1024))
         {
           nsXPIDLString confirmString;
           PRBool okToCompact = PR_FALSE;
           rv = GetStringFromBundle("autoCompactAllFolders", getter_Copies(confirmString));
           if (NS_SUCCEEDED(rv) && confirmString)
             ThrowConfirmationPrompt(aWindow, confirmString.get(), &okToCompact);
           if (okToCompact)
           {
             if ( localExpungedBytes > 0)
             {
               nsCOMPtr <nsISupports> aSupports = getter_AddRefs(folderArray->ElementAt(0));
               nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryInterface(aSupports, &rv);
               if (msgFolder && NS_SUCCEEDED(rv))
                 if (offlineExpungedBytes > 0)
                   msgFolder->CompactAll(nsnull, aWindow, folderArray, PR_TRUE, offlineFolderArray);
                 else
                   msgFolder->CompactAll(nsnull, aWindow, folderArray, PR_FALSE, nsnull);
             }
             else if (offlineExpungedBytes > 0)
               CompactAllOfflineStores(aWindow, offlineFolderArray);
           }
         }
       }
     }  
     gtimeOfLastPurgeCheck = PR_Now();
  }
  return rv;
}
 
NS_IMETHODIMP
nsMsgDBFolder::CompactAllOfflineStores(nsIMsgWindow *aWindow, nsISupportsArray *aOfflineFolderArray)
{
  nsresult rv= NS_OK;
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor;
  folderCompactor = do_CreateInstance(NS_MSGOFFLINESTORECOMPACTOR_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv) && folderCompactor)
    rv = folderCompactor->StartCompactingAll(aOfflineFolderArray, aWindow, PR_FALSE, nsnull);
  return rv;
}

nsresult
nsMsgDBFolder::GetPromptPurgeThreshold(PRBool *aPrompt)
{
  NS_ENSURE_ARG(aPrompt);
  nsresult rv;
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefService)
  {
    rv = prefService->GetBoolPref(PREF_MAIL_PROMPT_PURGE_THRESHOLD, aPrompt);
    if (NS_FAILED(rv)) 
    {
      *aPrompt = PR_FALSE;
      rv = NS_OK;
    }
  }
  return rv;
}

nsresult
nsMsgDBFolder::GetPurgeThreshold(PRInt32 *aThreshold)
{
  NS_ENSURE_ARG(aThreshold);
  nsresult rv;
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefService)
  {
    rv = prefService->GetIntPref(PREF_MAIL_PURGE_THRESHOLD, aThreshold);
    if (NS_FAILED(rv)) 
    {
      *aThreshold = 0;
      rv = NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP //called on the folder that is renamed or about to be deleted
nsMsgDBFolder::MatchOrChangeFilterDestination(nsIMsgFolder *newFolder, PRBool caseInsensitive, PRBool *found)
{
  nsresult rv = NS_OK;
  nsXPIDLCString oldUri;
  rv = GetURI(getter_Copies(oldUri));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsXPIDLCString newUri;
  if (newFolder) //for matching uri's this will be null
  {
    rv = newFolder->GetURI(getter_Copies(newUri));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  nsCOMPtr<nsIMsgFilterList> filterList;
  nsCOMPtr<nsIMsgAccountManager> accountMgr = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupportsArray> allServers;
    rv = accountMgr->GetAllServers(getter_AddRefs(allServers));
    if (NS_SUCCEEDED(rv) && allServers)
    {
      PRUint32 numServers,
      rv = allServers->Count(&numServers);
      for (PRUint32 serverIndex=0; serverIndex < numServers; serverIndex++)
      {
        nsCOMPtr <nsISupports> serverSupports = getter_AddRefs(allServers->ElementAt(serverIndex));
        nsCOMPtr <nsIMsgIncomingServer> server = do_QueryInterface(serverSupports, &rv);
        if (server && NS_SUCCEEDED(rv))
        {
          PRBool canHaveFilters;
          rv = server->GetCanHaveFilters(&canHaveFilters);
          if (NS_SUCCEEDED(rv) && canHaveFilters) 
          {
            rv = server->GetFilterList(getter_AddRefs(filterList));
            if (filterList && NS_SUCCEEDED(rv))
            {
              rv = filterList->MatchOrChangeFilterTarget(oldUri, newUri, caseInsensitive, found);
              if (found && newFolder && newUri)
                rv = filterList->SaveToDefaultFile();
            }
          }
        }
      }
    }
  }

  return rv;
}

