/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsRDFCID.h"
#include "nsNetUtil.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgDatabase.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsEscape.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsIFileStream.h"
#include "nsIChannel.h"
#include "nsITransport.h"
#include "nsIMsgFolderCompactor.h"
#include "nsIDocShell.h"
#include "nsIMsgWindow.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsCollationCID.h"
#include "nsAbBaseCID.h"
#include "nsIAbMDBDirectory.h"
#include "nsISpamSettings.h"
#include "nsIMsgFilterPlugin.h"
#include "nsIMsgMailSession.h"
#include "nsIRDFService.h"
#include "nsTextFormatter.h"
#include "nsCPasswordManager.h"
#include "nsMsgDBCID.h"
#include "nsInt64.h"

#include <time.h>

#define oneHour 3600000000U
#include "nsMsgUtils.h"

static PRTime gtimeOfLastPurgeCheck;    //variable to know when to check for purge_threshhold


#define PREF_MAIL_PROMPT_PURGE_THRESHOLD "mail.prompt_purge_threshhold"
#define PREF_MAIL_PURGE_THRESHOLD "mail.purge_threshhold"
#define PREF_MAIL_WARN_FILTER_CHANGED "mail.warn_filter_changed"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

nsIAtom* nsMsgDBFolder::mFolderLoadedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mDeleteOrMoveMsgCompletedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mDeleteOrMoveMsgFailedAtom=nsnull;
nsIAtom* nsMsgDBFolder::mJunkStatusChangedAtom=nsnull;
nsIAtom* nsMsgDBFolder::kTotalMessagesAtom=nsnull;
nsIAtom* nsMsgDBFolder::kFolderSizeAtom=nsnull;
nsIAtom* nsMsgDBFolder::kBiffStateAtom=nsnull;
nsIAtom* nsMsgDBFolder::kNewMessagesAtom=nsnull;
nsIAtom* nsMsgDBFolder::kNumNewBiffMessagesAtom=nsnull;
nsIAtom* nsMsgDBFolder::kTotalUnreadMessagesAtom=nsnull;
nsIAtom* nsMsgDBFolder::kFlaggedAtom=nsnull;
nsIAtom* nsMsgDBFolder::kStatusAtom=nsnull;
nsIAtom* nsMsgDBFolder::kNameAtom=nsnull;
nsIAtom* nsMsgDBFolder::kSynchronizeAtom=nsnull;
nsIAtom* nsMsgDBFolder::kOpenAtom=nsnull;
nsIAtom* nsMsgDBFolder::kIsDeferred=nsnull;

nsICollation * nsMsgDBFolder::gCollationKeyGenerator = nsnull;

PRUnichar *nsMsgDBFolder::kLocalizedInboxName;
PRUnichar *nsMsgDBFolder::kLocalizedTrashName;
PRUnichar *nsMsgDBFolder::kLocalizedSentName;
PRUnichar *nsMsgDBFolder::kLocalizedDraftsName;
PRUnichar *nsMsgDBFolder::kLocalizedTemplatesName;
PRUnichar *nsMsgDBFolder::kLocalizedUnsentName;
PRUnichar *nsMsgDBFolder::kLocalizedJunkName;

nsrefcnt nsMsgDBFolder::mInstanceCount=0;

NS_IMPL_ISUPPORTS_INHERITED6(nsMsgDBFolder, nsRDFResource,
                                   nsISupportsWeakReference,
                                   nsIMsgFolder,
                                   nsICollection,
                                   nsISerializable,
                                   nsIDBChangeListener,
                                   nsIUrlListener)

const nsStaticAtom nsMsgDBFolder::folder_atoms[] = {
  { "FolderLoaded", &nsMsgDBFolder::mFolderLoadedAtom },
  { "DeleteOrMoveMsgCompleted", &nsMsgDBFolder::mDeleteOrMoveMsgCompletedAtom },
  { "DeleteOrMoveMsgFailed", &nsMsgDBFolder::mDeleteOrMoveMsgFailedAtom },
  { "JunkStatusChanged", &nsMsgDBFolder::mJunkStatusChangedAtom },
  { "BiffState", &nsMsgDBFolder::kBiffStateAtom },
  { "NewMessages", &nsMsgDBFolder::kNewMessagesAtom },
  { "NumNewBiffMessages", &nsMsgDBFolder::kNumNewBiffMessagesAtom },
  { "Name", &nsMsgDBFolder::kNameAtom },
  { "TotalUnreadMessages", &nsMsgDBFolder::kTotalUnreadMessagesAtom },
  { "TotalMessages", &nsMsgDBFolder::kTotalMessagesAtom },
  { "FolderSize", &nsMsgDBFolder::kFolderSizeAtom },
  { "Status", &nsMsgDBFolder::kStatusAtom },
  { "Flagged", &nsMsgDBFolder::kFlaggedAtom },
  { "Synchronize", &nsMsgDBFolder::kSynchronizeAtom },
  { "open", &nsMsgDBFolder::kOpenAtom },
  { "isDeferred", &nsMsgDBFolder::kIsDeferred }
};

nsMsgDBFolder::nsMsgDBFolder(void)
: mAddListener(PR_TRUE),
  mNewMessages(PR_FALSE),
  mGettingNewMessages(PR_FALSE),
  mLastMessageLoaded(nsMsgKey_None),
  mFlags(0),
  mNumUnreadMessages(-1),
  mNumTotalMessages(-1),
  mNotifyCountChanges(PR_TRUE),
  mExpungedBytes(0),
  mInitializedFromCache(PR_FALSE),
  mSemaphoreHolder(nsnull),
  mNumPendingUnreadMessages(0),
  mNumPendingTotalMessages(0),
  mFolderSize(0),
  mNumNewBiffMessages(0),
  mIsCachable(PR_TRUE),
  mHaveParsedURI(PR_FALSE),
  mIsServerIsValid(PR_FALSE),
  mIsServer(PR_FALSE),
  mBaseMessageURI(nsnull)
{
  NS_NewISupportsArray(getter_AddRefs(mSubFolders));
  if (mInstanceCount++ <=0) {
    NS_RegisterStaticAtoms(folder_atoms, NS_ARRAY_LENGTH(folder_atoms));
    initializeStrings();
    createCollationKeyGenerator();
#ifdef MSG_FASTER_URI_PARSING
    mParsingURL = do_CreateInstance(NS_STANDARDURL_CONTRACTID);
#endif
    LL_I2L(gtimeOfLastPurgeCheck, 0);
  }
}

nsMsgDBFolder::~nsMsgDBFolder(void)
{
  CRTFREEIF(mBaseMessageURI);

  if (--mInstanceCount == 0) {
    NS_IF_RELEASE(gCollationKeyGenerator);
    CRTFREEIF(kLocalizedInboxName);
    CRTFREEIF(kLocalizedTrashName);
    CRTFREEIF(kLocalizedSentName);
    CRTFREEIF(kLocalizedDraftsName);
    CRTFREEIF(kLocalizedTemplatesName);
    CRTFREEIF(kLocalizedUnsentName);
    CRTFREEIF(kLocalizedJunkName);
#ifdef MSG_FASTER_URI_PARSING
    mParsingURL = nsnull;
#endif
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
        nsCOMPtr<nsIMsgFolder> childFolder = do_QueryElementAt(mSubFolders, i);
        if(childFolder)
          childFolder->Shutdown(PR_TRUE);
      }
    }
    // Reset incoming server pointer and pathname.
    mServer = nsnull;
    mPath = nsnull;
    mSubFolders->Clear();
  }
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::ForceDBClosed()
{
    PRUint32 cnt = 0, i;
    if (mSubFolders)
    {
        nsCOMPtr<nsIMsgFolder> child;
        mSubFolders->Count(&cnt);
        if (cnt > 0)
            for (i = 0; i < cnt; i++)
            {
                child = do_QueryElementAt(mSubFolders, i);
                if (child)
                    child->ForceDBClosed();
            }
    }
    if (mDatabase)
    {
        mDatabase->ForceClosed();
        mDatabase = nsnull;
    }
    else
    {
      nsCOMPtr<nsIMsgDatabase> mailDBFactory;
      nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDBFactory));
      if (NS_SUCCEEDED(rv) && mailDBFactory)
        mailDBFactory->ForceFolderDBClosed(this);
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
  NS_ENSURE_ARG_POINTER(count);

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


NS_IMETHODIMP nsMsgDBFolder::GetCharset(char * *aCharset)
{
  NS_ENSURE_ARG_POINTER(aCharset);

  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
    return folderInfo->GetCharPtrCharacterSet(aCharset);

  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetCharset(const char * aCharset)
{
	nsresult rv;

	nsCOMPtr<nsIDBFolderInfo> folderInfo;
	nsCOMPtr<nsIMsgDatabase> db; 
	rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
	if(NS_SUCCEEDED(rv))
	{
		rv = folderInfo->SetCharacterSet(NS_ConvertASCIItoUTF16(aCharset).get());
		db->Commit(nsMsgDBCommitType::kLargeCommit);
		mCharset.AssignWithConversion(aCharset);  // synchronize member variable
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
  NS_ENSURE_ARG_POINTER(hasNewMessages);
  
  *hasNewMessages = mNewMessages;
  return NS_OK;
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
  NS_ENSURE_ARG_POINTER(gettingNewMessages);
  
  *gettingNewMessages = mGettingNewMessages;
  return NS_OK;
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
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &result); 
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
#ifdef DEBUG_bienvenu1
          nsXPIDLString name;
          GetName(getter_Copies(name));
          NS_ASSERTION(Compare(name, kLocalizedTrashName) || (mFlags & MSG_FOLDER_FLAG_TRASH), "lost trash flag");
#endif
          mInitializedFromCache = PR_TRUE;
        }
        
        folderInfo->GetNumMessages(&mNumTotalMessages);
        folderInfo->GetNumUnreadMessages(&mNumUnreadMessages);
        folderInfo->GetExpungedBytes((PRInt32 *)&mExpungedBytes);
        
        nsXPIDLCString utf8Name;
        folderInfo->GetFolderName(getter_Copies(utf8Name));
        if (!utf8Name.IsEmpty())
          CopyUTF8toUTF16(utf8Name, mName);
        
        //These should be put in IMAP folder only.
        //folderInfo->GetImapTotalPendingMessages(&mNumPendingTotalMessages);
        //folderInfo->GetImapUnreadPendingMessages(&mNumPendingUnreadMessages);
        
        PRBool defaultUsed;
        folderInfo->GetCharacterSet(mCharset, &defaultUsed);
        if (defaultUsed)
          mCharset.Truncate();
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
    rv = SetBiffState(nsMsgBiffState_NoMail);
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

NS_IMETHODIMP nsMsgDBFolder::GetOfflineFileStream(nsMsgKey msgKey, PRUint32 *offset, PRUint32 *size, nsIInputStream **aFileStream)
{
  NS_ENSURE_ARG(aFileStream);

  *offset = *size = 0;
  
  nsXPIDLCString nativePath;
  mPath->GetNativePath(getter_Copies(nativePath));

  nsCOMPtr <nsILocalFile> localStore;
  nsresult rv = NS_NewNativeLocalFile(nativePath, PR_TRUE, getter_AddRefs(localStore));
  if (NS_SUCCEEDED(rv) && localStore)
  {
    rv = NS_NewLocalFileInputStream(aFileStream, localStore);

    if (NS_SUCCEEDED(rv))
    {

      rv = GetDatabase(nsnull);
      NS_ENSURE_SUCCESS(rv, NS_OK);
      nsCOMPtr<nsIMsgDBHdr> hdr;
      rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(hdr));
      if (hdr && NS_SUCCEEDED(rv))
      {
        hdr->GetMessageOffset(offset);
        hdr->GetOfflineMessageSize(size);
      }
      // check if offline store really has the correct offset into the offline 
      // store by reading the first few bytes. If it doesn't, clear the offline
      // flag on the msg and return false, which will fall back to reading the message
      // from the server.
      nsCOMPtr <nsISeekableStream> seekableStream = do_QueryInterface(*aFileStream);
      if (seekableStream)
      {
        rv = seekableStream->Seek(nsISeekableStream::NS_SEEK_CUR, *offset);
        char startOfMsg[10];
        PRUint32 bytesRead;
        if (NS_SUCCEEDED(rv))
          rv = (*aFileStream)->Read(startOfMsg, sizeof(startOfMsg), &bytesRead);

        if (NS_FAILED(rv) || bytesRead != sizeof(startOfMsg) || strncmp(startOfMsg, "From ", 5))
        {
          if (mDatabase)
            mDatabase->MarkOffline(msgKey, PR_FALSE, nsnull);
          rv = NS_ERROR_FAILURE;
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
    NS_ENSURE_SUCCESS(rv, rv);
    supports->QueryInterface(NS_GET_IID(nsIOutputStream), (void **) outputStream);

    nsCOMPtr <nsIRandomAccessStore> seekable = do_QueryInterface(supports);
    if (seekable)
      seekable->Seek(nsISeekableStream::NS_SEEK_END, 0);
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
  path.SetLeafName(proposedDBName.get());

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
  
  NS_ADDREF(*aMsgDatabase = mDatabase);
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
nsMsgDBFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **database)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDBFolder::OnReadChanged(nsIDBChangeListener * aInstigator)
{
  /* do nothing.  if you care about this, override it.  see nsNewsFolder.cpp */
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::OnJunkScoreChanged(nsIDBChangeListener * aInstigator)
{
  NotifyFolderEvent(mJunkStatusChangedAtom);
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

    return OnKeyAddedOrDeleted(aKeyChanged, PR_FALSE);
}

// 2.  When a new messages gets added, we need to see if it's new.
NS_IMETHODIMP nsMsgDBFolder::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey  aParentKey , PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
  if(aFlags & MSG_FLAG_NEW) 
    CheckWithNewMessagesStatus(PR_TRUE);
  
  return OnKeyAddedOrDeleted(aKeyChanged, PR_TRUE);
}

nsresult nsMsgDBFolder::OnKeyAddedOrDeleted(nsMsgKey aKeyChanged, PRBool added)
{
  nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
  nsresult rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(msgDBHdr));
  if(NS_SUCCEEDED(rv) && msgDBHdr)
  {
    if(added)
      NotifyItemAdded(msgDBHdr);
    else
      NotifyItemRemoved(msgDBHdr);
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
  OnKeyAddedOrDeleted(aKeyChanged, PR_FALSE);
  //Then add it to the new threadParent
  OnKeyAddedOrDeleted(aKeyChanged, PR_TRUE);
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *instigator)
{
    if (mDatabase)
    {
        mDatabase->RemoveListener(this);
        mDatabase = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetManyHeadersToDownload(PRBool *retval)
{
  PRInt32 numTotalMessages;

  NS_ENSURE_ARG_POINTER(retval);

  // is there any reason to return false?
  if (!mDatabase)
    *retval = PR_TRUE;
  else if (NS_SUCCEEDED(GetTotalMessages(PR_FALSE, &numTotalMessages)) && numTotalMessages <= 0)
    *retval = PR_TRUE;
  else
    *retval = PR_FALSE;
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
  nsXPIDLCString charset;

  element->GetInt32Property("flags", (PRInt32 *) &mFlags);

  PRBool persistElided = PR_TRUE;
  rv = GetPersistElided(&persistElided);
  NS_ENSURE_SUCCESS(rv,rv);

  // we aren't persisting elided, set the folder as closed
  if (!persistElided) {
    mFlags |= MSG_FOLDER_FLAG_ELIDED;
  }

  element->GetInt32Property("totalMsgs", &mNumTotalMessages);
  element->GetInt32Property("totalUnreadMsgs", &mNumUnreadMessages);
  element->GetInt32Property("pendingUnreadMsgs", &mNumPendingUnreadMessages);
  element->GetInt32Property("pendingMsgs", &mNumPendingTotalMessages);
  element->GetInt32Property("expungedBytes", (PRInt32 *) &mExpungedBytes);
  element->GetInt32Property("folderSize", (PRInt32 *) &mFolderSize);

  element->GetStringProperty("charset", getter_Copies(charset));

#ifdef DEBUG_bienvenu1
  char *uri;

  GetURI(&uri);
  printf("read total %ld for %s\n", mNumTotalMessages, uri);
  PR_Free(uri);
#endif
  mCharset.AssignWithConversion(charset.get());

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
      nsLocalFolderSummarySpec summarySpec(folderName);
      
      dbPath->SetFromFileSpec(summarySpec);
      
      // create the .msf file
      // see bug #244217 for details
      PRBool exists;
      if (NS_SUCCEEDED(dbPath->Exists(&exists)) && !exists)
        dbPath->Touch();
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
	if (NS_FAILED(rv))
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
				if (NS_FAILED(rv))
					break;
			}
		}
		rv = aEnumerator->Next();
		if (NS_FAILED(rv))
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
  element->SetInt32Property("folderSize", mFolderSize);

  nsCAutoString mcharsetC;
  mcharsetC.AssignWithConversion(mCharset);
	element->SetStringProperty("charset", mcharsetC.get());

#ifdef DEBUG_bienvenu1
	char *uri;

	GetURI(&uri);
	printf("writing total %ld for %s\n", mNumTotalMessages, uri);
	PR_Free(uri);
#endif
	return rv;
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
    EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_TRUE /*dbBatching*/);
    rv = mDatabase->MarkAllRead(nsnull);
    EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE /*dbBatching*/);
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
      NotifyFolderEvent(mFolderLoadedAtom);
      
    // be sure to remove ourselves as a url listener
    mailUrl->UnRegisterListener(this);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgDBFolder::GetRetentionSettings(nsIMsgRetentionSettings **settings)
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
  
  nsCOMPtr <nsISeekableStream> seekable;
  nsInt64 curStorePos;

  if (m_offlineHeader)
    seekable = do_QueryInterface(m_tempMessageStream);

  if (seekable)
  {
    PRInt64 tellPos;
    seekable->Tell(&tellPos);
    curStorePos = tellPos;
    // ### todo - need to convert this to 64 bits
    m_offlineHeader->SetMessageOffset((PRUint32) curStorePos);
  }
  m_tempMessageStream->Write(result.get(), result.Length(),
                             &writeCount);
  if (seekable)
  {
    PRInt64 tellPos;
    seekable->Seek(PR_SEEK_CUR, 0); // seeking causes a flush, w/o syncing
    seekable->Tell(&tellPos);
    curStorePos = tellPos;
    m_offlineHeader->SetStatusOffset((PRUint32) curStorePos);
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
  nsresult rv = NS_OK;
  if (!m_tempMessageStream)
    rv = GetOfflineStoreOutputStream(getter_AddRefs(m_tempMessageStream));
  else
  {
    nsCOMPtr <nsISeekableStream> seekable;

    seekable = do_QueryInterface(m_tempMessageStream);

    if (seekable)
      seekable->Seek(PR_SEEK_END, 0);
  }
  if (NS_SUCCEEDED(rv))
    WriteStartOfNewLocalMessage();
  m_numOfflineMsgLines = 0;
  return rv;
}

nsresult nsMsgDBFolder::EndNewOfflineMessage()
{
  nsCOMPtr <nsIRandomAccessStore> seekable;
  nsInt64 curStorePos;
  PRUint32 messageOffset;
  nsMsgKey messageKey;

  nsresult rv = GetDatabase(nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  m_offlineHeader->GetMessageKey(&messageKey);
  if (m_tempMessageStream)
    seekable = do_QueryInterface(m_tempMessageStream);

  mDatabase->MarkOffline(messageKey, PR_TRUE, nsnull);
  if (seekable)
  {
    seekable->Seek(PR_SEEK_CUR, 0); // seeking causes a flush, w/o syncing
    PRInt64 tellPos;
    seekable->Tell(&tellPos);
    curStorePos = tellPos;
    
    m_offlineHeader->GetMessageOffset(&messageOffset);
    curStorePos -= messageOffset;
    m_offlineHeader->SetOfflineMessageSize(curStorePos);
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
      rv = folderCompactor->Compact(this, PR_TRUE, inWindow);
  return rv;
}

nsresult
nsMsgDBFolder::AutoCompact(nsIMsgWindow *aWindow) 
{
   NS_ENSURE_ARG_POINTER(aWindow);
   PRBool prompt;
   nsresult rv = GetPromptPurgeThreshold(&prompt);
   NS_ENSURE_SUCCESS(rv, rv);
   PRTime timeNow = PR_Now();   //time in microseconds
   PRTime timeAfterOneHourOfLastPurgeCheck;
   LL_ADD(timeAfterOneHourOfLastPurgeCheck, gtimeOfLastPurgeCheck, oneHour);
   if (LL_CMP(timeAfterOneHourOfLastPurgeCheck, <, timeNow) && prompt)
   {
     gtimeOfLastPurgeCheck = timeNow;
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
         nsCOMPtr<nsIMsgIncomingServer> server =
           do_QueryElementAt(allServers, serverIndex);
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
           nsCOMPtr<nsIMsgFolder> rootFolder;
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
           server = do_QueryElementAt(allServers, ++serverIndex);
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
               nsCOMPtr <nsIMsgFolder> msgFolder =
                 do_QueryElementAt(folderArray, 0, &rv);
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
    rv = folderCompactor->CompactAll(nsnull, aWindow, PR_TRUE, aOfflineFolderArray);
  return rv;
}

nsresult
nsMsgDBFolder::GetPromptPurgeThreshold(PRBool *aPrompt)
{
  NS_ENSURE_ARG(aPrompt);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    rv = prefBranch->GetBoolPref(PREF_MAIL_PROMPT_PURGE_THRESHOLD, aPrompt);
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
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    rv = prefBranch->GetIntPref(PREF_MAIL_PURGE_THRESHOLD, aThreshold);
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
      PRUint32 numServers;
      rv = allServers->Count(&numServers);
      for (PRUint32 serverIndex=0; serverIndex < numServers; serverIndex++)
      {
        nsCOMPtr <nsIMsgIncomingServer> server =
          do_QueryElementAt(allServers, serverIndex, &rv);
        if (server && NS_SUCCEEDED(rv))
        {
          PRBool canHaveFilters;
          rv = server->GetCanHaveFilters(&canHaveFilters);
          if (NS_SUCCEEDED(rv) && canHaveFilters) 
          {
            rv = server->GetFilterList(nsnull, getter_AddRefs(filterList));
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

NS_IMETHODIMP
nsMsgDBFolder::GetDBTransferInfo(nsIDBFolderInfo **aTransferInfo)
{
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  nsCOMPtr <nsIMsgDatabase> db;
  GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(db));
  if (dbFolderInfo)
    dbFolderInfo->GetTransferInfo(aTransferInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::SetDBTransferInfo(nsIDBFolderInfo *aTransferInfo)
{
  NS_ENSURE_ARG(aTransferInfo);
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  nsCOMPtr <nsIMsgDatabase> db;
  GetMsgDatabase(nsnull, getter_AddRefs(db));
  if (db)
  {
    db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
    if(dbFolderInfo)
      dbFolderInfo->InitFromTransferInfo(aTransferInfo);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetStringProperty(const char *propertyName, char **propertyValue)
{
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_ARG_POINTER(propertyValue);
  nsCOMPtr <nsIFileSpec> dbPath;
  nsresult rv = GetFolderCacheKey(getter_AddRefs(dbPath));
  if (dbPath)
  {
    nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
    rv = GetFolderCacheElemFromFileSpec(dbPath, getter_AddRefs(cacheElement));
    if (cacheElement)  //try to get from cache
      rv = cacheElement->GetStringProperty(propertyName, propertyValue);
    if (NS_FAILED(rv))  //if failed, then try to get from db
    {
      nsCOMPtr<nsIDBFolderInfo> folderInfo;
      nsCOMPtr<nsIMsgDatabase> db; 
      PRBool exists;
      rv = dbPath->Exists(&exists);
      if (NS_FAILED(rv) || !exists)
        return NS_MSG_ERROR_FOLDER_MISSING;
      rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
      if (NS_SUCCEEDED(rv))
        rv = folderInfo->GetCharPtrProperty(propertyName, propertyValue);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::SetStringProperty(const char *propertyName, const char *propertyValue)
{
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_ARG_POINTER(propertyValue);

  nsCOMPtr <nsIFileSpec> dbPath;
  GetFolderCacheKey(getter_AddRefs(dbPath));
  if (dbPath)
  {
    nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
    GetFolderCacheElemFromFileSpec(dbPath, getter_AddRefs(cacheElement));
    if (cacheElement)  //try to set in the cache
      cacheElement->SetStringProperty(propertyName, propertyValue);
  }  
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
  {
    folderInfo->SetCharPtrProperty(propertyName, propertyValue);
    db->Commit(nsMsgDBCommitType::kLargeCommit);  //commiting the db also commits the cache
  }
  return NS_OK;
}

// sub-classes need to override
nsresult
nsMsgDBFolder::SpamFilterClassifyMessage(const char *aURI, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  return aJunkMailPlugin->ClassifyMessage(aURI, aMsgWindow, nsnull);   
}

nsresult
nsMsgDBFolder::SpamFilterClassifyMessages(const char **aURIArray, PRUint32 aURICount, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  return aJunkMailPlugin->ClassifyMessages(aURICount, aURIArray, aMsgWindow, nsnull);   
}

/**
 * Call the filter plugins (XXX currently just one)
 */
NS_IMETHODIMP
nsMsgDBFolder::CallFilterPlugins(nsIMsgWindow *aMsgWindow, PRBool *aFiltersRun)
{
  NS_ENSURE_ARG_POINTER(aFiltersRun);
  *aFiltersRun = PR_FALSE;
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsISpamSettings> spamSettings;
  nsCOMPtr<nsIAbMDBDirectory> whiteListDirectory;
  nsCOMPtr<nsIMsgHeaderParser> headerParser;
  PRBool useWhiteList = PR_FALSE;
  PRInt32 spamLevel = 0;
  nsXPIDLCString whiteListAbURI;

  // if this is the junk folder, or the trash folder
  // don't analyze for spam, because we don't care
  //
  // if it's the sent, unsent, templates, or drafts, 
  // don't analyze for spam, because the user
  // created that message
  //
  // if it's a public imap folder, or another users
  // imap folder, don't analyze for spam, because
  // it's not ours to analyze
  if (mFlags & (MSG_FOLDER_FLAG_JUNK | MSG_FOLDER_FLAG_TRASH |
               MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_QUEUE |
               MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_TEMPLATES |
               MSG_FOLDER_FLAG_IMAP_PUBLIC | MSG_FOLDER_FLAG_IMAP_OTHER_USER))
    return NS_OK;

  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv); 
  rv = server->GetSpamSettings(getter_AddRefs(spamSettings));
  nsCOMPtr <nsIMsgFilterPlugin> filterPlugin;
  server->GetSpamFilterPlugin(getter_AddRefs(filterPlugin));
  if (!filterPlugin) // it's not an error not to have the filter plugin.
    return NS_OK;
  NS_ENSURE_SUCCESS(rv, rv); 

  nsCOMPtr <nsIJunkMailPlugin> junkMailPlugin = do_QueryInterface(filterPlugin);

  if (junkMailPlugin)
  {
    PRBool userHasClassified = PR_FALSE;
    // if the user has not classified any messages yet, then we shouldn't bother
    // running the junk mail controls. This creates a better first use experience.
    // See Bug #250084.
    junkMailPlugin->GetUserHasClassified(&userHasClassified);
    if (!userHasClassified)
      return NS_OK;
  }

  spamSettings->GetLevel(&spamLevel);
  if (!spamLevel)
    return NS_OK;

  nsCOMPtr<nsIMsgMailSession> mailSession = 
      do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mDatabase) 
  {
      rv = GetDatabase(nsnull);   // XXX is nsnull a reasonable arg here?
      NS_ENSURE_SUCCESS(rv, rv);
  }

  // get the list of new messages
  //
  nsMsgKeyArray *newMessageKeys;
  rv = mDatabase->GetNewList(&newMessageKeys);
  NS_ENSURE_SUCCESS(rv, rv);

  // if there weren't any, just return 
  //
  if (!newMessageKeys || !newMessageKeys->GetSize()) 
      return NS_OK;

  spamSettings->GetUseWhiteList(&useWhiteList);
  if (useWhiteList)
  {
    spamSettings->GetWhiteListAbURI(getter_Copies(whiteListAbURI));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!whiteListAbURI.IsEmpty())
    {
      nsCOMPtr <nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr <nsIRDFResource> resource;
      rv = rdfService->GetResource(whiteListAbURI, getter_AddRefs(resource));
      NS_ENSURE_SUCCESS(rv, rv);

      whiteListDirectory = do_QueryInterface(resource, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      headerParser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // if we can't get the db, we probably want to continue firing spam filters.
  }

  // build up list of keys to classify
  //
  nsXPIDLCString uri;
  nsMsgKeyArray keysToClassify;

  PRUint32 numNewMessages = newMessageKeys->GetSize();
  for ( PRUint32 i=0 ; i < numNewMessages ; ++i ) 
  {
      nsXPIDLCString junkScore;
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      nsMsgKey msgKey = newMessageKeys->GetAt(i);
      rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(msgHdr));
      if (!NS_SUCCEEDED(rv))
        continue;

      msgHdr->GetStringProperty("junkscore", getter_Copies(junkScore));
      if (!junkScore.IsEmpty()) // ignore already scored messages.
        continue;
    // check whitelist first:
      if (whiteListDirectory)
      {
        if (NS_SUCCEEDED(rv))
        {
          PRBool cardExists = PR_FALSE;
          nsXPIDLCString author;
          nsXPIDLCString authorEmailAddress;
          msgHdr->GetAuthor(getter_Copies(author));
          rv = headerParser->ExtractHeaderAddressMailboxes(nsnull, author.get(), getter_Copies(authorEmailAddress));
          // don't want to abort the rest of the scoring.
          if (NS_SUCCEEDED(rv))
            rv = whiteListDirectory->HasCardForEmailAddress(authorEmailAddress, &cardExists);
          if (NS_SUCCEEDED(rv) && cardExists)
          {
            // mark this msg as non-junk, because we whitelisted it.
            mDatabase->SetStringProperty(msgKey, "junkscore", "0");
            mDatabase->SetStringProperty(msgKey, "junkscoreorigin", "plugin");
            continue; // skip this msg since it's in the white list
          }
        }
      }

      keysToClassify.Add(newMessageKeys->GetAt(i));

  }

  if (keysToClassify.GetSize() > 0)
  {
    PRUint32 numMessagesToClassify = keysToClassify.GetSize();
    char ** messageURIs = (char **) PR_MALLOC(sizeof(const char *) * numMessagesToClassify);
    if (!messageURIs)
      return NS_ERROR_OUT_OF_MEMORY;

    for ( PRUint32 msgIndex=0 ; msgIndex < numMessagesToClassify ; ++msgIndex ) 
    {
        // generate a URI for the message
        //
        rv = GenerateMessageURI(keysToClassify.GetAt(msgIndex), &messageURIs[msgIndex]);
        if (NS_FAILED(rv)) 
            NS_WARNING("nsMsgDBFolder::CallFilterPlugins(): could not"
                       " generate URI for message");
    }
    // filterMsgs
    //
    *aFiltersRun = PR_TRUE;
    rv = SpamFilterClassifyMessages((const char **) messageURIs, numMessagesToClassify, aMsgWindow, junkMailPlugin); 

    for ( PRUint32 freeIndex=0 ; freeIndex < numMessagesToClassify ; ++freeIndex ) 
      PR_Free(messageURIs[freeIndex]);
    PR_Free(messageURIs);

  }
  NS_DELETEXPCOM(newMessageKeys);
  return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetLastMessageLoaded(nsMsgKey *aMsgKey) 
{
  NS_ENSURE_ARG_POINTER(aMsgKey);
  *aMsgKey = mLastMessageLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::SetLastMessageLoaded(nsMsgKey aMsgKey)
{
  mLastMessageLoaded = aMsgKey;
  return NS_OK;
}

nsresult nsMsgDBFolder::PromptForCachePassword(nsIMsgIncomingServer *server, nsIMsgWindow *aWindow, PRBool &passwordCorrect)
{
  PRBool userDidntCancel;
  passwordCorrect = PR_FALSE;
  nsCOMPtr <nsIStringBundle> bundle;
  nsresult rv = GetBaseStringBundle(getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString hostName;
  nsXPIDLCString userName;
  nsXPIDLString passwordTemplate;
  nsXPIDLCString password;
  nsXPIDLString passwordTitle;
  nsXPIDLString passwordPromptString;

  server->GetRealHostName(getter_Copies(hostName));
  server->GetRealUsername(getter_Copies(userName));
  bundle->GetStringFromName(NS_LITERAL_STRING("passwordTitle").get(), getter_Copies(passwordTitle));
  bundle->GetStringFromName(NS_LITERAL_STRING("passwordPrompt").get(), getter_Copies(passwordTemplate));

  NS_ConvertASCIItoUCS2 userNameStr(userName);
  NS_ConvertASCIItoUCS2 hostNameStr(hostName);

  const PRUnichar *stringParams[2] = { userNameStr.get(), hostNameStr.get() };

  rv = bundle->FormatStringFromName(
        NS_LITERAL_STRING("passwordPrompt").get(), stringParams, 2, 
        getter_Copies(passwordPromptString ));
  NS_ENSURE_SUCCESS(rv, rv);

  do
  {
    rv = server->GetPasswordWithUI(passwordPromptString,
                                   passwordTitle, 
                                   aWindow,
                                   &userDidntCancel,
                                   getter_Copies(password));
    if (rv != NS_MSG_PASSWORD_PROMPT_CANCELLED && !password.IsEmpty()) 
    {
      nsCOMPtr <nsIPasswordManagerInternal> passwordMgrInt = do_GetService(NS_PASSWORDMANAGER_CONTRACTID, &rv);
      if(passwordMgrInt) 
      {

        // Get the current server URI
        nsXPIDLCString currServerUri;
        rv = server->GetServerURI(getter_Copies(currServerUri));
        NS_ENSURE_SUCCESS(rv, rv);

        currServerUri.Insert('x', 0);
        nsCAutoString hostFound;
        nsAutoString userNameFound;
        nsAutoString passwordFound;

        const nsAFlatString& empty = EmptyString();

        // Get password entry corresponding to the host URI we are passing in.
        rv = passwordMgrInt->FindPasswordEntry(currServerUri, empty, empty,
                                               hostFound, userNameFound,
                                               passwordFound);
        if (NS_FAILED(rv)) 
          break;
        // compare the user-entered password with the saved password with
        // the munged uri.
        passwordCorrect = password.Equals(NS_ConvertUCS2toUTF8(passwordFound).get());
        if (!passwordCorrect)
          server->SetPassword("");
      }
    }
  }
  while (NS_SUCCEEDED(rv) && rv != NS_MSG_PASSWORD_PROMPT_CANCELLED && userDidntCancel && !passwordCorrect);
  return (!passwordCorrect) ? NS_ERROR_FAILURE : rv;
}


nsresult nsMsgDBFolder::PerformBiffNotifications(void)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32  numBiffMsgs = 0;
  nsCOMPtr<nsIMsgFolder> root;
  rv = GetRootFolder(getter_AddRefs(root));
  root->GetNumNewMessages(PR_TRUE, &numBiffMsgs);
  if (numBiffMsgs > 0) 
  {
    server->SetPerformingBiff(true);
    SetBiffState(nsIMsgFolder::nsMsgBiffState_NewMail);
    server->SetPerformingBiff(false);
  }
  return NS_OK;
}

nsresult
nsMsgDBFolder::initializeStrings()
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                   getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  bundle->GetStringFromName(NS_LITERAL_STRING("inboxFolderName").get(),
                            &kLocalizedInboxName);
  bundle->GetStringFromName(NS_LITERAL_STRING("trashFolderName").get(),
                            &kLocalizedTrashName);
  bundle->GetStringFromName(NS_LITERAL_STRING("sentFolderName").get(),
                            &kLocalizedSentName);
  bundle->GetStringFromName(NS_LITERAL_STRING("draftsFolderName").get(),
                            &kLocalizedDraftsName);
  bundle->GetStringFromName(NS_LITERAL_STRING("templatesFolderName").get(),
                            &kLocalizedTemplatesName);
  bundle->GetStringFromName(NS_LITERAL_STRING("junkFolderName").get(),
                            &kLocalizedJunkName);
  bundle->GetStringFromName(NS_LITERAL_STRING("unsentFolderName").get(),
                            &kLocalizedUnsentName);
  return NS_OK;
}

nsresult
nsMsgDBFolder::createCollationKeyGenerator()
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILocaleService> localeSvc = do_GetService(NS_LOCALESERVICE_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocale> locale;
  rv = localeSvc->GetApplicationLocale(getter_AddRefs(locale));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsICollationFactory> factory = do_CreateInstance(kCollationFactoryCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = factory->CreateCollation(locale, &gCollationKeyGenerator);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::Init(const char* aURI)
{
  // for now, just initialize everything during Init()

  nsresult rv;

  rv = nsRDFResource::Init(aURI);
  if (NS_FAILED(rv))
    return rv;

  rv = CreateBaseMessageURI(aURI);

  return NS_OK;
}

nsresult nsMsgDBFolder::CreateBaseMessageURI(const char *aURI)
{
  // Each folder needs to implement this.
  return NS_OK;
}

  // nsISerializable methods:
NS_IMETHODIMP
nsMsgDBFolder::Read(nsIObjectInputStream *aStream)
{
  NS_NOTREACHED("nsMsgDBFolder::Read");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDBFolder::Write(nsIObjectOutputStream *aStream)
{
  NS_NOTREACHED("nsMsgDBFolder::Write");
  return NS_ERROR_NOT_IMPLEMENTED;
}

  // nsICollection methods:
NS_IMETHODIMP
nsMsgDBFolder::Count(PRUint32 *result)
{
  return mSubFolders->Count(result);
}

NS_IMETHODIMP
nsMsgDBFolder::GetElementAt(PRUint32 i, nsISupports* *result)
{
  return mSubFolders->GetElementAt(i, result);
}

NS_IMETHODIMP
nsMsgDBFolder::QueryElementAt(PRUint32 i, const nsIID & iid, void * *result) 
{
  return mSubFolders->QueryElementAt(i, iid, result);
}

NS_IMETHODIMP
nsMsgDBFolder::SetElementAt(PRUint32 i, nsISupports* value) 
{
  return mSubFolders->SetElementAt(i, value);
}

NS_IMETHODIMP
nsMsgDBFolder::AppendElement(nsISupports *aElement) 
{
  return mSubFolders->AppendElement(aElement);
}

NS_IMETHODIMP
nsMsgDBFolder::RemoveElement(nsISupports *aElement) 
{
  return mSubFolders->RemoveElement(aElement);
}

NS_IMETHODIMP
nsMsgDBFolder::Enumerate(nsIEnumerator* *result) 
{
  // nsMsgDBFolders only have subfolders, no message elements
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP
nsMsgDBFolder::Clear(void) 
{
  return mSubFolders->Clear();
}

NS_IMETHODIMP
nsMsgDBFolder::GetURI(char* *name) 
{
  return nsRDFResource::GetValue(name);
}


////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP
nsMsgDBFolder::GetSubFolders(nsIEnumerator* *result)
{
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP
nsMsgDBFolder::FindSubFolder(const nsACString& aEscapedSubFolderName, nsIMsgFolder **aFolder)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));

  if (NS_FAILED(rv))
    return rv;

  // XXX use necko here
  nsCAutoString uri;
  uri.Append(mURI);
  uri.Append('/');
  uri.Append(aEscapedSubFolderName);

  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uri, getter_AddRefs(res));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;
  if (!aFolder)
    return NS_ERROR_UNEXPECTED;

  *aFolder = folder;
  NS_ADDREF(*aFolder);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetHasSubFolders(PRBool *_retval)
{
  PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  *_retval = (cnt > 0);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::AddFolderListener(nsIFolderListener * listener)
{
  return mListeners.AppendElement(listener) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgDBFolder::RemoveFolderListener(nsIFolderListener * listener)
{

  mListeners.RemoveElement(listener);
  return NS_OK;

}

NS_IMETHODIMP nsMsgDBFolder::SetParent(nsIMsgFolder *aParent)
{
  mParent = do_GetWeakReference(aParent);

  if (aParent) 
  {
    nsresult rv;
    nsCOMPtr<nsIMsgFolder> parentMsgFolder = do_QueryInterface(aParent, &rv);

    if (NS_SUCCEEDED(rv)) 
    {

      // servers do not have parents, so we must not be a server
      mIsServer = PR_FALSE;
      mIsServerIsValid = PR_TRUE;

      // also set the server itself while we're here.

      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = parentMsgFolder->GetServer(getter_AddRefs(server));
      if (NS_SUCCEEDED(rv) && server)
        mServer = do_GetWeakReference(server);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetParent(nsIMsgFolder **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  nsCOMPtr<nsIMsgFolder> parent = do_QueryReferent(mParent);

  *aParent = parent;
  NS_IF_ADDREF(*aParent);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetParentMsgFolder(nsIMsgFolder **aParentMsgFolder)
{
  NS_ENSURE_ARG_POINTER(aParentMsgFolder);

  nsCOMPtr<nsIMsgFolder> parent = do_QueryReferent(mParent);

  NS_IF_ADDREF(*aParentMsgFolder = parent);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  // XXX should this return an empty enumeration?
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMsgDBFolder::UpdateFolder(nsIMsgWindow *)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsMsgDBFolder::GetFolderURL(char **url)
{
  NS_ENSURE_ARG_POINTER(url);
  *url = nsnull;
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetServer(nsIMsgIncomingServer ** aServer)
{
  NS_ENSURE_ARG_POINTER(aServer);

  nsresult rv;

  // short circut the server if we have it.
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(mServer, &rv);
  if (NS_FAILED(rv) || !server) 
  {
    // try again after parsing the URI
    rv = parseURI(PR_TRUE);
    server = do_QueryReferent(mServer);
  }

  *aServer = server;
  NS_IF_ADDREF(*aServer);

  return (server) ? NS_OK : NS_ERROR_NULL_POINTER;
}

#ifdef MSG_FASTER_URI_PARSING
class nsMsgAutoBool {
public:
  nsMsgAutoBool() : mValue(nsnull) {}
  void autoReset(PRBool *aValue) { mValue = aValue; }
  ~nsMsgAutoBool() { if (mValue) *mValue = PR_FALSE; }
private:
  PRBool *mValue;
};
#endif

nsresult
nsMsgDBFolder::parseURI(PRBool needServer)
{
  nsresult rv;
  nsCOMPtr<nsIURL> url;

#ifdef MSG_FASTER_URI_PARSING
  nsMsgAutoBool parsingUrlState;
  if (mParsingURLInUse) 
  {
    url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  }
  else 
  {
    url = mParsingURL;
    mParsingURLInUse = PR_TRUE;
    parsingUrlState.autoReset(&mParsingURLInUse);
  }

#else
  url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
#endif

  rv = url->SetSpec(nsDependentCString(mURI));
  if (NS_FAILED(rv)) return rv;

  //
  // pull some info out of the URI
  //

  // empty path tells us it's a server.
  if (!mIsServerIsValid) 
  {
    nsCAutoString path;
    rv = url->GetPath(path);
    if (NS_SUCCEEDED(rv)) 
    {
      if (!strcmp(path.get(), "/"))
        mIsServer = PR_TRUE;
      else
        mIsServer = PR_FALSE;
    }
    mIsServerIsValid = PR_TRUE;
  }

  // grab the name off the leaf of the server
  if (mName.IsEmpty()) 
  {
    // mName:
    // the name is the trailing directory in the path
    nsCAutoString fileName;
    url->GetFileName(fileName);
    if (!fileName.IsEmpty()) 
    {
      // XXX conversion to unicode here? is fileName in UTF8?
      // yes, let's say it is in utf8
      NS_UnescapeURL((char *)fileName.get());
      mName = NS_ConvertUTF8toUCS2(fileName.get());
    }
  }

  // grab the server by parsing the URI and looking it up
  // in the account manager...
  // But avoid this extra work by first asking the parent, if any

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(mServer, &rv);
  if (NS_FAILED(rv) || !server) 
  {
    // first try asking the parent instead of the URI
    nsCOMPtr<nsIMsgFolder> parentMsgFolder;
    rv = GetParentMsgFolder(getter_AddRefs(parentMsgFolder));

    if (NS_SUCCEEDED(rv) && parentMsgFolder)
      rv = parentMsgFolder->GetServer(getter_AddRefs(server));

    // no parent. do the extra work of asking
    if (!server && needServer) 
    {
      // Get username and hostname so we can get the server
      nsCAutoString userPass;
      rv = url->GetUserPass(userPass);
      if (NS_SUCCEEDED(rv) && !userPass.IsEmpty())
        nsUnescape(userPass.BeginWriting());

      nsCAutoString hostName;
      rv = url->GetHost(hostName);
      if (NS_SUCCEEDED(rv) && !hostName.IsEmpty())
        nsUnescape(hostName.BeginWriting());

      // turn it back into a server:

      nsCOMPtr<nsIMsgAccountManager> accountManager =
               do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = accountManager->FindServer(userPass.get(),
                                      hostName.get(),
                                      GetIncomingServerType(),
                                      getter_AddRefs(server));

      if (NS_FAILED(rv)) return rv;

    }

    mServer = do_GetWeakReference(server);

  } /* !mServer */

  // now try to find the local path for this folder
  if (server) 
  {
    nsCAutoString newPath;

    nsCAutoString urlPath;
    url->GetFilePath(urlPath);
    if (!urlPath.IsEmpty()) 
    {
      NS_UnescapeURL((char *) urlPath.get());

      // transform the filepath from the URI, such as
      // "/folder1/folder2/foldern"
      // to
      // "folder1.sbd/folder2.sbd/foldern"
      // (remove leading / and add .sbd to first n-1 folders)
      // to be appended onto the server's path

      NS_MsgCreatePathStringFromFolderURI(urlPath.get(), newPath);
    }

    // now append munged path onto server path
    nsCOMPtr<nsIFileSpec> serverPath;
    rv = server->GetLocalPath(getter_AddRefs(serverPath));
    if (NS_FAILED(rv)) return rv;

    if (serverPath) 
    {
      rv = serverPath->AppendRelativeUnixPath(newPath.get());
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to append to the serverPath");
      if (NS_FAILED(rv)) 
      {
        mPath = nsnull;
        return rv;
      }
      mPath = serverPath;
    }

    // URI is completely parsed when we've attempted to get the server
    mHaveParsedURI=PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetIsServer(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // make sure we've parsed the URI
  if (!mIsServerIsValid) 
  {
    nsresult rv = parseURI();
    if (NS_FAILED(rv) || !mIsServerIsValid)
      return NS_ERROR_FAILURE;
  }

  *aResult = mIsServer;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetNoSelect(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetImapShared(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return GetFlag(MSG_FOLDER_FLAG_PERSONAL_SHARED, aResult);
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanSubscribe(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  // by default, you can't subscribe.
  // if otherwise, override it.
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanFileMessages(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  //varada - checking folder flag to see if it is the "Unsent Messages"
  //and if so return FALSE
  if (mFlags & (MSG_FOLDER_FLAG_QUEUE | MSG_FOLDER_FLAG_VIRTUAL))
  {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  // by default, you can't file messages into servers, only to folders
  // if otherwise, override it.
  *aResult = !isServer;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanDeleteMessages(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanCreateSubfolders(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  //Checking folder flag to see if it is the "Unsent Messages"
  //or a virtual folder, and if so return FALSE
  if (mFlags & (MSG_FOLDER_FLAG_QUEUE | MSG_FOLDER_FLAG_VIRTUAL))
  {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  // by default, you can create subfolders on server and folders
  // if otherwise, override it.
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanRename(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  if (NS_FAILED(rv)) return rv;

  // by default, you can't rename servers, only folders
  // if otherwise, override it.
  if (isServer) 
  {
    *aResult = PR_FALSE;
  }
  //
  // check if the folder is a special folder
  // (Trash, Drafts, Unsent Messages, Inbox, Sent, Templates, Junk)
  // if it is, don't allow the user to rename it
  // (which includes dnd moving it with in the same server)
  //
  // this errors on the side of caution.  we'll return false a lot
  // more often if we use flags,
  // instead of checking if the folder really is being used as a
  // special folder by looking at the "copies and folders" prefs on the
  // identities.
  //
  // one day...
  else if (mFlags & MSG_FOLDER_FLAG_TRASH ||
           mFlags & MSG_FOLDER_FLAG_DRAFTS ||
           mFlags & MSG_FOLDER_FLAG_QUEUE ||
           mFlags & MSG_FOLDER_FLAG_INBOX ||
           mFlags & MSG_FOLDER_FLAG_SENTMAIL ||
           mFlags & MSG_FOLDER_FLAG_TEMPLATES ||
           mFlags & MSG_FOLDER_FLAG_JUNK) 
  {
    *aResult = PR_FALSE;
  }
  else 
  {
    *aResult = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetCanCompact(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRBool isServer = PR_FALSE;
  nsresult rv = GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv,rv);
  *aResult = !isServer;   //servers cannot be compacted --> 4.x
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetPrettyName(PRUnichar ** name)
{
  return GetName(name);
}

NS_IMETHODIMP nsMsgDBFolder::SetPrettyName(const PRUnichar *name)
{
  nsresult rv;
  nsAutoString unicodeName(name);

  //Set pretty name only if special flag is set and if it the default folder name
  if (mFlags & MSG_FOLDER_FLAG_INBOX && unicodeName.LowerCaseEqualsLiteral("inbox"))
    rv = SetName(kLocalizedInboxName);
  else if (mFlags & MSG_FOLDER_FLAG_SENTMAIL && unicodeName.LowerCaseEqualsLiteral("sent"))
    rv = SetName(kLocalizedSentName);
  //netscape webmail uses "Draft" instead of "Drafts"
  else if (mFlags & MSG_FOLDER_FLAG_DRAFTS && (unicodeName.LowerCaseEqualsLiteral("drafts") 
                                                || unicodeName.LowerCaseEqualsLiteral("draft")))  
    rv = SetName(kLocalizedDraftsName);
  else if (mFlags & MSG_FOLDER_FLAG_TEMPLATES && unicodeName.LowerCaseEqualsLiteral("templates"))
    rv = SetName(kLocalizedTemplatesName);
  else if (mFlags & MSG_FOLDER_FLAG_TRASH && unicodeName.LowerCaseEqualsLiteral("trash"))
    rv = SetName(kLocalizedTrashName);
  else if (mFlags & MSG_FOLDER_FLAG_QUEUE && unicodeName.LowerCaseEqualsLiteral("unsent messages"))
    rv = SetName(kLocalizedUnsentName);
  else if (mFlags & MSG_FOLDER_FLAG_JUNK && unicodeName.LowerCaseEqualsLiteral("junk"))
    rv = SetName(kLocalizedJunkName);
  else
    rv = SetName(name);

  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetName(PRUnichar **name)
{
  NS_ENSURE_ARG_POINTER(name);

  nsresult rv;
  if (!mHaveParsedURI && mName.IsEmpty()) 
  {
    rv = parseURI();
    if (NS_FAILED(rv)) return rv;
  }

  // if it's a server, just forward the call
  if (mIsServer) 
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv) && server)
      return server->GetPrettyName(name);
  }

  *name = ToNewUnicode(mName);

  if (!(*name)) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::SetName(const PRUnichar * name)
{
  // override the URI-generated name
  if (!mName.Equals(name))
  {
    mName = name;

    // old/new value doesn't matter here
    NotifyUnicharPropertyChanged(kNameAtom, name, name);
  }
  return NS_OK;
}

//For default, just return name
NS_IMETHODIMP nsMsgDBFolder::GetAbbreviatedName(PRUnichar * *aAbbreviatedName)
{
  return GetName(aAbbreviatedName);
}

NS_IMETHODIMP nsMsgDBFolder::GetChildNamed(const PRUnichar *name, nsISupports ** aChild)
{
  NS_ASSERTION(aChild, "NULL child");
  nsresult rv;
  // will return nsnull if we can't find it
  *aChild = nsnull;

  PRUint32 count;
  rv = mSubFolders->Count(&count);
  if (NS_FAILED(rv)) return rv;

  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
    if (NS_SUCCEEDED(rv))
    {
      nsXPIDLString folderName;

      rv = folder->GetName(getter_Copies(folderName));
      // case-insensitive compare is probably LCD across OS filesystems
      if (NS_SUCCEEDED(rv) &&
          folderName.Equals(name, nsCaseInsensitiveStringComparator()))
      {
        NS_ADDREF(*aChild = folder);
        return NS_OK;
      }
    }
  }
  // don't return NS_OK if we didn't find the folder
  // see http://bugzilla.mozilla.org/show_bug.cgi?id=210089#c15
  // and http://bugzilla.mozilla.org/show_bug.cgi?id=210089#c17
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgDBFolder::GetChildWithURI(const char *uri, PRBool deep, PRBool caseInsensitive, nsIMsgFolder ** child)
{
  NS_ASSERTION(child, "NULL child");
  nsresult rv;
  // will return nsnull if we can't find it
  *child = nsnull;

  nsCOMPtr <nsIEnumerator> aEnumerator;

  rv = GetSubFolders(getter_AddRefs(aEnumerator));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISupports> aItem;

  rv = aEnumerator->First();
  if (NS_FAILED(rv))
    return NS_OK; // it's OK, there are no sub-folders.

  while(NS_SUCCEEDED(rv))
  {
    rv = aEnumerator->CurrentItem(getter_AddRefs(aItem));
    if (NS_FAILED(rv)) break;
    nsCOMPtr<nsIRDFResource> folderResource = do_QueryInterface(aItem);
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(aItem);
    if (folderResource  && folder)
    {
      const char *folderURI;
      rv = folderResource->GetValueConst(&folderURI);
      if (NS_FAILED(rv)) return rv;
      PRBool equal;
      equal = folderURI &&
              (caseInsensitive
               ? nsCRT::strcasecmp(folderURI, uri)
               : nsCRT::strcmp(folderURI, uri)) == 0;
      if (equal)
      {
        *child = folder;
        NS_ADDREF(*child);
        return NS_OK;
      }
      if (deep)
      {
        rv = folder->GetChildWithURI(uri, deep, caseInsensitive, child);
        if (NS_FAILED(rv))
          return rv;

        if (*child)
          return NS_OK;
      }
    }
    rv = aEnumerator->Next();
    if (NS_FAILED(rv))
    {
      rv = NS_OK;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetPrettiestName(PRUnichar **name)
{
  if (NS_SUCCEEDED(GetPrettyName(name)))
    return NS_OK;
  return GetName(name);
}


NS_IMETHODIMP nsMsgDBFolder::GetShowDeletedMessages(PRBool *showDeletedMessages)
{
  NS_ENSURE_ARG_POINTER(showDeletedMessages);

  *showDeletedMessages = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::Delete()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::DeleteSubFolders(nsISupportsArray *folders,
                                            nsIMsgWindow *msgWindow)
{
  nsresult rv;

  PRUint32 count;
  rv = folders->Count(&count);
  for(PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(folders, i, &rv));
    if (folder)
      PropagateDelete(folder, PR_TRUE, msgWindow);
  }
  return rv;

}

NS_IMETHODIMP nsMsgDBFolder::CreateStorageIfMissing(nsIUrlListener* /* urlListener */)
{
  NS_ASSERTION(PR_FALSE, "needs to be overridden");
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::PropagateDelete(nsIMsgFolder *folder, PRBool deleteStorage, nsIMsgWindow *msgWindow)
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
    if (NS_SUCCEEDED(status))
    {
      if (folder == child.get())
      {
        //Remove self as parent
        child->SetParent(nsnull);

        // maybe delete disk storage for it, and its subfolders
        status = child->RecursiveDelete(deleteStorage, msgWindow);

        if (status == NS_OK)
        {
          //Remove from list of subfolders.
          mSubFolders->RemoveElement(supports);
          NotifyItemRemoved(supports);
          break;
        }
        else
        {  // setting parent back if we failed
          child->SetParent(this);
        }
      }
      else
      {
        status = child->PropagateDelete (folder, deleteStorage, msgWindow);
      }
    }
  }

  return status;
}

NS_IMETHODIMP nsMsgDBFolder::RecursiveDelete(PRBool deleteStorage, nsIMsgWindow *msgWindow)
{
  // If deleteStorage is PR_TRUE, recursively deletes disk storage for this folder
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

    if (NS_SUCCEEDED(status))
    {
      child->SetParent(nsnull);
      status = child->RecursiveDelete(deleteStorage,msgWindow);  // recur
      if (NS_SUCCEEDED(status))
        mSubFolders->RemoveElement(supports);  // unlink it from this's child list
      else
      { // setting parent back if we failed for some reason
          child->SetParent(this);
      }
    }
    cnt--;
  }

  // now delete the disk storage for _this_
  if (deleteStorage && (status == NS_OK))
      status = Delete();
  return status;
}

NS_IMETHODIMP nsMsgDBFolder::CreateSubfolder(const PRUnichar *folderName, nsIMsgWindow *msgWindow )
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::AddSubfolder(const nsAString& name,
                                   nsIMsgFolder** child)
{
  NS_ENSURE_ARG_POINTER(child);
  
  PRInt32 flags = 0;
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCAutoString uri(mURI);
  uri.Append('/');
  
  // URI should use UTF-8
  // (see RFC2396 Uniform Resource Identifiers (URI): Generic Syntax)
  nsCAutoString escapedName;
  rv = NS_MsgEscapeEncodeURLPath(name, escapedName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // fix for #192780
  // if this is the root folder
  // make sure the the special folders
  // have the right uri.
  // on disk, host\INBOX should be a folder with the uri mailbox://user@host/Inbox"
  // as mailbox://user@host/Inbox != mailbox://user@host/INBOX
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder && (rootFolder.get() == (nsIMsgFolder *)this))
  {
    if (nsCRT::strcasecmp(escapedName.get(), "INBOX") == 0)
      uri += "Inbox";
    else if (nsCRT::strcasecmp(escapedName.get(), "UNSENT%20MESSAGES") == 0)
      uri += "Unsent%20Messages";
    else if (nsCRT::strcasecmp(escapedName.get(), "DRAFTS") == 0)
      uri += "Drafts";
    else if (nsCRT::strcasecmp(escapedName.get(), "TRASH") == 0)
      uri += "Trash";
    else if (nsCRT::strcasecmp(escapedName.get(), "SENT") == 0)
      uri += "Sent";
    else if (nsCRT::strcasecmp(escapedName.get(), "TEMPLATES") == 0)
      uri +="Templates";
    else
      uri += escapedName.get();
  }
  else
    uri += escapedName.get();
  
  nsCOMPtr <nsIMsgFolder> msgFolder;
  rv = GetChildWithURI(uri.get(), PR_FALSE/*deep*/, PR_TRUE /*case Insensitive*/, getter_AddRefs(msgFolder));  
  if (NS_SUCCEEDED(rv) && msgFolder)
    return NS_MSG_FOLDER_EXISTS;
  
  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uri, getter_AddRefs(res));
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;
  
  folder->GetFlags((PRUint32 *)&flags);
  
  flags |= MSG_FOLDER_FLAG_MAIL;
  
  folder->SetParent(this);
  
  PRBool isServer;
  rv = GetIsServer(&isServer);
  
  //Only set these is these are top level children.
  if(NS_SUCCEEDED(rv) && isServer)
  {
    if(name.LowerCaseEqualsLiteral("inbox"))
    {
      flags |= MSG_FOLDER_FLAG_INBOX;
      SetBiffState(nsIMsgFolder::nsMsgBiffState_Unknown);
    }
    else if (name.LowerCaseEqualsLiteral("trash"))
      flags |= MSG_FOLDER_FLAG_TRASH;
    else if (name.LowerCaseEqualsLiteral("unsent messages") ||
      name.LowerCaseEqualsLiteral("outbox"))
      flags |= MSG_FOLDER_FLAG_QUEUE;
#if 0
    // the logic for this has been moved into 
    // SetFlagsOnDefaultMailboxes()
    else if(name.EqualsIgnoreCase(NS_LITERAL_STRING("Sent"), nsCaseInsensitiveStringComparator()))
      folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
    else if(name.EqualsIgnoreCase(NS_LITERAL_STRING("Drafts"), nsCaseInsensitiveStringComparator()))
      folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
    else if(name.EqualsIgnoreCase(NS_LITERAL_STRING("Templates"), nsCaseInsensitiveStringComparator()))
      folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
#endif 
  }
  
  folder->SetFlags(flags);
  
  //at this point we must be ok and we don't want to return failure in case GetIsServer failed.
  rv = NS_OK;
  
  nsCOMPtr<nsISupports> supports = do_QueryInterface(folder);
  if(folder)
    mSubFolders->AppendElement(supports);
  *child = folder;
  NS_ADDREF(*child);
  
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aCompactOfflineArray)
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::EmptyTrash(nsIMsgWindow *msgWindow, nsIUrlListener *aListener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::Rename(const PRUnichar *name, nsIMsgWindow *msgWindow)
{
  nsresult status = NS_OK;
  nsAutoString unicharString(name);
  status = SetName((PRUnichar *) unicharString.get());
  //After doing a SetName we need to make sure that broadcasting this message causes a
  //new sort to happen.
  return status;

}

NS_IMETHODIMP nsMsgDBFolder::RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::ContainsChildNamed(const PRUnichar *name, PRBool* containsChild)
{
  NS_ENSURE_ARG_POINTER(containsChild);

  nsCOMPtr<nsISupports> child;
  GetChildNamed(name, getter_AddRefs(child));
  *containsChild = child != nsnull;
  return NS_OK;
}



NS_IMETHODIMP nsMsgDBFolder::IsAncestorOf(nsIMsgFolder *child, PRBool *isAncestor)
{
  NS_ENSURE_ARG_POINTER(isAncestor);

  nsresult rv = NS_OK;

  PRUint32 count;
  rv = mSubFolders->Count(&count);
  if (NS_FAILED(rv)) return rv;

  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
    if (NS_SUCCEEDED(rv))
    {
      if (folder.get() == child )
      {
        *isAncestor = PR_TRUE;
      }
      else
        folder->IsAncestorOf(child, isAncestor);

    }
    if (*isAncestor)
      return NS_OK;
  }
  *isAncestor = PR_FALSE;
  return rv;

}


NS_IMETHODIMP nsMsgDBFolder::GenerateUniqueSubfolderName(const PRUnichar *prefix,
                                                       nsIMsgFolder *otherFolder,
                                                       PRUnichar **name)
{
  NS_ENSURE_ARG_POINTER(name);

  /* only try 256 times */
  for (int count = 0; (count < 256); count++)
  {
    nsAutoString uniqueName;
    uniqueName.Assign(prefix);
    uniqueName.AppendInt(count);
    PRBool containsChild;
    PRBool otherContainsChild = PR_FALSE;

    ContainsChildNamed(uniqueName.get(), &containsChild);
    if (otherFolder)
    {
      ((nsIMsgFolder*)otherFolder)->ContainsChildNamed(uniqueName.get(), &otherContainsChild);
    }

    if (!containsChild && !otherContainsChild)
    {
      *name = nsCRT::strdup(uniqueName.get());
      return NS_OK;
    }
  }
  *name = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::UpdateSummaryTotals(PRBool /* force */)
{
  //We don't support this
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::SummaryChanged()
{
  UpdateSummaryTotals(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetNumUnread(PRBool deep, PRInt32 *numUnread)
{
  NS_ENSURE_ARG_POINTER(numUnread);

  nsresult rv;
  PRInt32 total = mNumUnreadMessages + mNumPendingUnreadMessages;
  if (deep)
  {
    if (total < 0) // deep search never returns negative counts
      total = 0;
    PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) 
    {
      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
        if (NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          PRUint32 folderFlags;
          folder->GetFlags(&folderFlags);
          if (!(folderFlags & MSG_FOLDER_FLAG_VIRTUAL))
          {
            folder->GetNumUnread(deep, &num);
            total += num;
          }
        }
      }
    }
  }
  *numUnread = total;
  return NS_OK;

}

NS_IMETHODIMP nsMsgDBFolder::GetTotalMessages(PRBool deep, PRInt32 *totalMessages)
{
  NS_ENSURE_ARG_POINTER(totalMessages);

  nsresult rv;
  PRInt32 total = mNumTotalMessages + mNumPendingTotalMessages;
  if (deep)
  {
    if (total < 0) // deep search never returns negative counts
      total = 0;
    PRUint32 count;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv)) 
    {

      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
        if (NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          PRUint32 folderFlags;
          folder->GetFlags(&folderFlags);
          if (!(folderFlags & MSG_FOLDER_FLAG_VIRTUAL))
          {
            folder->GetTotalMessages (deep, &num);
            total += num;
          }
        }
      }
    }
  }
  *totalMessages = total;
  return NS_OK;
}

PRInt32 nsMsgDBFolder::GetNumPendingUnread()
{
  return mNumPendingUnreadMessages;
}

PRInt32 nsMsgDBFolder::GetNumPendingTotalMessages()
{
  return mNumPendingTotalMessages;
}

void nsMsgDBFolder::ChangeNumPendingUnread(PRInt32 delta)
{
  if (delta)
  {
    PRInt32 oldUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
    mNumPendingUnreadMessages += delta;
    PRInt32 newUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
    NS_ASSERTION(newUnreadMessages >= 0, "shouldn't have negative unread message count");
    if (newUnreadMessages >= 0)
    {
      nsCOMPtr<nsIMsgDatabase> db;
      nsCOMPtr<nsIDBFolderInfo> folderInfo;
      nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
      if (NS_SUCCEEDED(rv) && folderInfo)
        folderInfo->SetImapUnreadPendingMessages(mNumPendingUnreadMessages);

      NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, newUnreadMessages);
    }
  }
}

void nsMsgDBFolder::ChangeNumPendingTotalMessages(PRInt32 delta)
{
  if (delta)
  {
    PRInt32 oldTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;
    mNumPendingTotalMessages += delta;
    PRInt32 newTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;

    nsCOMPtr<nsIMsgDatabase> db;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    nsresult rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
    if (NS_SUCCEEDED(rv) && folderInfo)
      folderInfo->SetImapTotalPendingMessages(mNumPendingTotalMessages);
    NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, newTotalMessages);
  }
}

NS_IMETHODIMP nsMsgDBFolder::SetPrefFlag()
{
  // *** Note: this method should only be called when we done with the folder
  // discovery. GetResource() may return a node which is not in the folder
  // tree hierarchy but in the rdf cache in case of the non-existing default
  // Sent, Drafts, and Templates folders. The resouce will be eventually
  // released when the rdf service shuts down. When we create the default
  // folders later on in the imap server, the subsequent GetResource() of the
  // same uri will get us the cached rdf resource which should have the folder
  // flag set appropriately.
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgAccountManager> accountMgr = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = accountMgr->GetFirstIdentityForServer(server, getter_AddRefs(identity));
  if (NS_SUCCEEDED(rv) && identity)
  {
    nsXPIDLCString folderUri;
    nsCOMPtr<nsIRDFResource> res;
    nsCOMPtr<nsIMsgFolder> folder;
    identity->GetFccFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
    }
    identity->GetDraftFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
    }
    identity->GetStationeryFolder(getter_Copies(folderUri));
    if (folderUri && NS_SUCCEEDED(rdf->GetResource(folderUri, getter_AddRefs(res))))
    {
      folder = do_QueryInterface(res, &rv);
      if (NS_SUCCEEDED(rv))
        rv = folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
    }
  }

  // XXX TODO
  // JUNK RELATED
  // should we be using the spam settings to set the JUNK flag?
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetFlag(PRUint32 flag)
{
  ReadDBFolderInfo(PR_FALSE);
  // OnFlagChange can be expensive, so don't call it if we don't need to
  PRBool flagSet;
  nsresult rv;

  if (NS_FAILED(rv = GetFlag(flag, &flagSet)))
    return rv;

  if (!flagSet)
  {
    mFlags |= flag;
    OnFlagChange(flag);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ClearFlag(PRUint32 flag)
{
  // OnFlagChange can be expensive, so don't call it if we don't need to
  PRBool flagSet;
  nsresult rv;

  if (NS_FAILED(rv = GetFlag(flag, &flagSet)))
    return rv;

  if (flagSet)
  {
    mFlags &= ~flag;
    OnFlagChange (flag);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetFlag(PRUint32 flag, PRBool *_retval)
{
  *_retval = ((mFlags & flag) != 0);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ToggleFlag(PRUint32 flag)
{
  mFlags ^= flag;
  OnFlagChange (flag);

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnFlagChange(PRUint32 flag)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgDatabase> db;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && folderInfo)
  {
#ifdef DEBUG_bienvenu1
       nsXPIDLString name;
       rv = GetName(getter_Copies(name));
       NS_ASSERTION(Compare(name, kLocalizedTrashName) || (mFlags & MSG_FOLDER_FLAG_TRASH), "lost trash flag");
#endif
    folderInfo->SetFlags((PRInt32) mFlags);
    if (db)
      db->Commit(nsMsgDBCommitType::kLargeCommit);

    if (flag & MSG_FOLDER_FLAG_OFFLINE) 
    {
      PRBool newValue = mFlags & MSG_FOLDER_FLAG_OFFLINE;
      rv = NotifyBoolPropertyChanged(kSynchronizeAtom, !newValue, newValue);
      NS_ENSURE_SUCCESS(rv,rv);
    }
    else if (flag & MSG_FOLDER_FLAG_ELIDED) 
    {
      PRBool newValue = mFlags & MSG_FOLDER_FLAG_ELIDED;
      rv = NotifyBoolPropertyChanged(kOpenAtom, newValue, !newValue);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  folderInfo = nsnull;
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetFlags(PRUint32 aFlags)
{
  if (mFlags != aFlags)
  {
    mFlags = aFlags;
    OnFlagChange(mFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetAllFoldersWithFlag(PRUint32 flag, nsISupportsArray **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, nsnull, 
                                           NS_GET_IID(nsISupportsArray), (void **)aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  return ListFoldersWithFlag(flag, *aResult);
}

nsresult nsMsgDBFolder::ListFoldersWithFlag(PRUint32 flag, nsISupportsArray *array)
{
  if ((flag & mFlags) == flag) 
  {
    nsCOMPtr <nsISupports> supports;
    QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports));
    array->AppendElement(supports);
  }

  nsresult rv;
  PRUint32 cnt;

  // call GetSubFolders() to ensure that mSubFolders is initialized
  nsCOMPtr <nsIEnumerator> enumerator;
  rv = GetSubFolders(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mSubFolders->Count(&cnt);
  if (NS_SUCCEEDED(rv)) 
  {
    for (PRUint32 i=0; i < cnt; i++)
    {
      nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
      if (NS_SUCCEEDED(rv) && folder)
      {
        nsIMsgFolder *msgFolder = folder.get();
        nsMsgDBFolder *dbFolder = NS_STATIC_CAST(nsMsgDBFolder *, msgFolder);
        dbFolder->ListFoldersWithFlag(flag,array);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetFoldersWithFlag(PRUint32 flags, PRUint32 resultsize, PRUint32 *numFolders, nsIMsgFolder **result)
{
  PRUint32 num = 0;
  if ((flags & mFlags) == flags) 
  {
    if (result && (num < resultsize)) 
    {
      result[num] = this;
      NS_IF_ADDREF(result[num]);
    }
    num++;
  }

  nsresult rv;
  PRUint32 cnt;

  // call GetSubFolders() to ensure that mSubFolders is initialized
  nsCOMPtr <nsIEnumerator> enumerator;
  rv = GetSubFolders(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mSubFolders->Count(&cnt);
  if (NS_SUCCEEDED(rv)) 
  {
    for (PRUint32 i=0; i < cnt; i++)
    {
      nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
      if (NS_SUCCEEDED(rv) && folder)
      {
        // CAREFUL! if NULL is passed in for result then the caller
        // still wants the full count!  Otherwise, the result should be at most the
        // number that the caller asked for.
        PRUint32 numSubFolders;

        if (!result)
        {
          folder->GetFoldersWithFlag(flags, 0, &numSubFolders, NULL);
          num += numSubFolders;
        }
        else if (num < resultsize)
        {
          folder->GetFoldersWithFlag(flags, resultsize - num, &numSubFolders, result+num);
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

NS_IMETHODIMP nsMsgDBFolder::GetExpansionArray(nsISupportsArray *expansionArray)
{
  // the application of flags in GetExpansionArray is subtly different
  // than in GetFoldersWithFlag

  nsresult rv;
  PRUint32 cnt;
  rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt; i++)
  {
    nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
    if (NS_SUCCEEDED(rv))
    {
      PRUint32 cnt2;
      rv = expansionArray->Count(&cnt2);
      if (NS_SUCCEEDED(rv)) 
      {
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


NS_IMETHODIMP nsMsgDBFolder::GetDeletable(PRBool *deletable)
{
  NS_ENSURE_ARG_POINTER(deletable);

  *deletable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetRequiresCleanup(PRBool *requiredCleanup)
{
  NS_ENSURE_ARG_POINTER(requiredCleanup);

  *requiredCleanup = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::ClearRequiresCleanup()
{
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetKnowsSearchNntpExtension(PRBool *knowsExtension)
{
  NS_ENSURE_ARG_POINTER(knowsExtension);

  *knowsExtension = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetAllowsPosting(PRBool *allowsPosting)
{
  NS_ENSURE_ARG_POINTER(allowsPosting);

  *allowsPosting = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetDisplayRecipients(PRBool *displayRecipients)
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
      for (int i = 0; i < numFccFolders; i++)
      {
        PRBool isAncestor;
        if (NS_SUCCEEDED(rv = fccFolders[i]->IsAncestorOf(this, &isAncestor)))
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


NS_IMETHODIMP nsMsgDBFolder::AcquireSemaphore(nsISupports *semHolder)
{
  nsresult rv = NS_OK;

  if (mSemaphoreHolder == NULL)
  {
    mSemaphoreHolder = semHolder; //Don't AddRef due to ownership issues.
  }
  else
    rv = NS_MSG_FOLDER_BUSY;

  return rv;

}

NS_IMETHODIMP nsMsgDBFolder::ReleaseSemaphore(nsISupports *semHolder)
{
  if (!mSemaphoreHolder || mSemaphoreHolder == semHolder)
  {
    mSemaphoreHolder = NULL;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::TestSemaphore(nsISupports *semHolder, PRBool *result)
{
  NS_ENSURE_ARG_POINTER(result);

  *result = (mSemaphoreHolder == semHolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetLocked(PRBool *isLocked)
{
  *isLocked =  mSemaphoreHolder != NULL;
  return  NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetRelativePathName(char **pathName)
{
  NS_ENSURE_ARG_POINTER(pathName);

  *pathName = nsnull;
  return NS_OK;
}


NS_IMETHODIMP nsMsgDBFolder::GetSizeOnDisk(PRUint32 *size)
{
  NS_ENSURE_ARG_POINTER(size);

  *size = 0;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::SetSizeOnDisk(PRUint32 aSizeOnDisk)
{
  NotifyIntPropertyChanged(kFolderSizeAtom, mFolderSize, aSizeOnDisk);
  mFolderSize = aSizeOnDisk;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetUsername(char **userName)
{
  NS_ENSURE_ARG_POINTER(userName);
  nsresult rv;
  nsCOMPtr <nsIMsgIncomingServer> server;

  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  if (server)
    return server->GetUsername(userName);

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsMsgDBFolder::GetHostname(char **hostName)
{
  NS_ENSURE_ARG_POINTER(hostName);

  nsresult rv;

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  if (server)
    return server->GetHostName(hostName);

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsMsgDBFolder::GetNewMessages(nsIMsgWindow *, nsIUrlListener * /* aListener */)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::GetBiffState(PRUint32 *aBiffState)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
    rv = server->GetBiffState(aBiffState);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetBiffState(PRUint32 aBiffState)
{
  PRUint32 oldBiffState;

  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
    rv = server->GetBiffState(&oldBiffState);
    // Get the server and notify it and not inbox.
  if (oldBiffState != aBiffState)
  {
    if (aBiffState == nsMsgBiffState_NoMail)
      SetNumNewMessages(0);

    // we don't distinguish between unknown and noMail for servers
    if (! (oldBiffState == nsMsgBiffState_Unknown && aBiffState == nsMsgBiffState_NoMail))
    {
      if (!mIsServer)
      {
        nsCOMPtr<nsIMsgFolder> folder;
        rv = GetRootFolder(getter_AddRefs(folder));
        if (NS_SUCCEEDED(rv) && folder)
          return folder->SetBiffState(aBiffState);
      }

      if (server)
        server->SetBiffState(aBiffState);
      NotifyPropertyFlagChanged((nsIRDFResource *)this, kBiffStateAtom, oldBiffState, aBiffState);
    }
  }
  else if (aBiffState == nsMsgBiffState_NoMail)
  {
    // even if the old biff state equals the new biff state, it is still possible that we've never
    // cleared the number of new messages for this particular folder. This happens when the new mail state
    // got cleared by viewing a new message in folder that is different from this one. Biff state is stored per server
    //  the num. of new messages is per folder. 
    SetNumNewMessages(0);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetNumNewMessages(PRBool deep, PRInt32 *aNumNewMessages)
{
  NS_ENSURE_ARG_POINTER(aNumNewMessages);

  PRInt32 numNewMessages = mNumNewBiffMessages;
  if (deep)
  {
    PRUint32 count;
    nsresult rv = NS_OK;
    rv = mSubFolders->Count(&count);
    if (NS_SUCCEEDED(rv))
    {
      for (PRUint32 i = 0; i < count; i++)
      {
        nsCOMPtr<nsIMsgFolder> folder(do_QueryElementAt(mSubFolders, i, &rv));
        if (NS_SUCCEEDED(rv))
        {
          PRInt32 num;
          folder->GetNumNewMessages(deep, &num);
          if (num > 0) // it's legal for counts to be negative if we don't know
            numNewMessages += num;
        }
      }
    }
  }
  *aNumNewMessages = numNewMessages;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::SetNumNewMessages(PRInt32 aNumNewMessages)
{
  if (aNumNewMessages != mNumNewBiffMessages)
  {
    PRInt32 oldNumMessages = mNumNewBiffMessages;
    mNumNewBiffMessages = aNumNewMessages;

    nsCAutoString oldNumMessagesStr;
    oldNumMessagesStr.AppendInt(oldNumMessages);
    nsCAutoString newNumMessagesStr;
    newNumMessagesStr.AppendInt(aNumNewMessages);
    NotifyPropertyChanged(kNumNewBiffMessagesAtom, oldNumMessagesStr.get(), newNumMessagesStr.get());
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetNewMessagesNotificationDescription(PRUnichar * *aDescription)
{
  nsresult rv;
  nsAutoString description;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));

  if (NS_SUCCEEDED(rv))
  {
    if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
    {
      nsXPIDLString folderName;
      rv = GetPrettyName(getter_Copies(folderName));
      if (NS_SUCCEEDED(rv) && folderName)
        description.Assign(folderName);
    }

    // append the server name
    nsXPIDLString serverName;
    rv = server->GetPrettyName(getter_Copies(serverName));
    if (NS_SUCCEEDED(rv)) 
    {
      // put this test here because we don't want to just put "folder name on"
      // in case the above failed
      if (!(mFlags & MSG_FOLDER_FLAG_INBOX))
        description.AppendLiteral(" on ");
      description.Append(serverName);
    }
  }
  *aDescription = ToNewUnicode(description);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetRootFolder(nsIMsgFolder * *aRootFolder)
{
  NS_ENSURE_ARG_POINTER(aRootFolder);

  nsresult rv;
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(server, "server is null");
  // if this happens, bail.
  if (!server) return NS_ERROR_NULL_POINTER;

  rv = server->GetRootMsgFolder(aRootFolder);
  if (!aRootFolder)
    return NS_ERROR_NULL_POINTER;
  else
    return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetPath(nsIFileSpec * *aPath)
{
  NS_ENSURE_ARG_POINTER(aPath);
  nsresult rv=NS_OK;

  if (!mPath)
    rv = parseURI(PR_TRUE);

  *aPath = mPath;
  NS_IF_ADDREF(*aPath);

  return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::SetPath(nsIFileSpec  *aPath)
{
  // XXX - make a local copy!
  mPath = aPath;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::MarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
  PRUint32 count;
  nsresult rv;

  rv = messages->Count(&count);
  if (NS_FAILED(rv))
    return rv;

  for(PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgDBHdr> message = do_QueryElementAt(messages, i, &rv);

    if (message)
      rv = message->MarkRead(markRead);

    if (NS_FAILED(rv))
      return rv;

  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged)
{
  PRUint32 count;
  nsresult rv;

  rv = messages->Count(&count);
  if (NS_FAILED(rv))
    return rv;

  for(PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIMsgDBHdr> message = do_QueryElementAt(messages, i, &rv);

    if (message)
      rv = message->MarkFlagged(markFlagged);

    if (NS_FAILED(rv))
      return rv;

  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::SetLabelForMessages(nsISupportsArray *aMessages, nsMsgLabelValue aLabel)
{
  GetDatabase(nsnull);
  if (mDatabase)
  {
  PRUint32 count;
  NS_ENSURE_ARG(aMessages);
  nsresult rv = aMessages->Count(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  for(PRUint32 i = 0; i < count; i++)
  {
      nsMsgKey msgKey;
    nsCOMPtr<nsIMsgDBHdr> message = do_QueryElementAt(aMessages, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
      (void) message->GetMessageKey(&msgKey);
      rv = mDatabase->SetLabel(msgKey, aLabel);
    NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::DeleteMessages(nsISupportsArray *messages,
                              nsIMsgWindow *msgWindow,
                              PRBool deleteStorage,
                              PRBool isMove,
                              nsIMsgCopyServiceListener *listener,
                              PRBool allowUndo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDBFolder::CopyMessages(nsIMsgFolder* srcFolder,
                          nsISupportsArray *messages,
                          PRBool isMove,
                          nsIMsgWindow *window,
                          nsIMsgCopyServiceListener* listener,
                          PRBool isFolder,
                          PRBool allowUndo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDBFolder::CopyFolder(nsIMsgFolder* srcFolder,
                        PRBool isMoveFolder,
                        nsIMsgWindow *window,
                        nsIMsgCopyServiceListener* listener)
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDBFolder::CopyFileMessage(nsIFileSpec* fileSpec,
                             nsIMsgDBHdr* messageToReplace,
                             PRBool isDraftOrTemplate,
                             nsIMsgWindow *window,
                             nsIMsgCopyServiceListener* listener)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::MatchName(nsString *name, PRBool *matches)
{
  NS_ENSURE_ARG_POINTER(matches);

  *matches = mName.Equals(*name, nsCaseInsensitiveStringComparator());
  return NS_OK;
}

nsresult
nsMsgDBFolder::NotifyPropertyChanged(nsIAtom *property,
                                   const char *oldValue, const char* newValue)
{
  for (PRInt32 i = 0; i < mListeners.Count(); i++)
  {
    //Folderlisteners aren't refcounted.
    nsIFolderListener* listener =(nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemPropertyChanged(this, property, oldValue, newValue);
  }

  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemPropertyChanged(this, property, oldValue, newValue);

  return NS_OK;

}

nsresult
nsMsgDBFolder::NotifyUnicharPropertyChanged(nsIAtom *property,
                                          const PRUnichar* oldValue,
                                          const PRUnichar *newValue)
{
  nsresult rv;

  for (PRInt32 i = 0; i < mListeners.Count(); i++) 
  {
    // folderlisteners aren't refcounted in the array
    nsIFolderListener* listener=(nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemUnicharPropertyChanged(this, property, oldValue, newValue);
  }

  // Notify listeners who listen to every folder
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = folderListenerManager->OnItemUnicharPropertyChanged(this,
                                                   property,
                                                   oldValue,
                                                   newValue);
  return NS_OK;
}

nsresult nsMsgDBFolder::NotifyIntPropertyChanged(nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
  //Don't send off count notifications if they are turned off.
  if (!mNotifyCountChanges && ((property == kTotalMessagesAtom) ||( property ==  kTotalUnreadMessagesAtom)))
    return NS_OK;

  for (PRInt32 i = 0; i < mListeners.Count(); i++)
  {
    //Folderlisteners aren't refcounted.
    nsIFolderListener* listener =(nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemIntPropertyChanged(this, property, oldValue, newValue);
  }

  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemIntPropertyChanged(this, property, oldValue, newValue);

  return NS_OK;

}

nsresult
nsMsgDBFolder::NotifyBoolPropertyChanged(nsIAtom* property,
                                       PRBool oldValue, PRBool newValue)
{
  for (PRInt32 i = 0; i < mListeners.Count(); i++)
  {
    //Folderlisteners aren't refcounted.
    nsIFolderListener* listener =(nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemBoolPropertyChanged(this, property, oldValue, newValue);
  }

  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemBoolPropertyChanged(this, property, oldValue, newValue);

  return NS_OK;

}

nsresult
nsMsgDBFolder::NotifyPropertyFlagChanged(nsISupports *item, nsIAtom *property,
                                       PRUint32 oldValue, PRUint32 newValue)
{
  PRInt32 i;
  for(i = 0; i < mListeners.Count(); i++)
  {
    //Folderlistener's aren't refcounted.
    nsIFolderListener *listener = (nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemPropertyFlagChanged(item, property, oldValue, newValue);
  }

  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemPropertyFlagChanged(item, property, oldValue, newValue);

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::NotifyItemAdded(nsISupports *aItem)
{
  static PRBool notify = PR_TRUE;

  if (!notify)
    return NS_OK;

  PRInt32 i;
  for(i = 0; i < mListeners.Count(); i++)
  {
    //Folderlistener's aren't refcounted.
    nsIFolderListener *listener = (nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemAdded(this, aItem);
  }

  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemAdded(this, aItem);

  return NS_OK;

}

nsresult nsMsgDBFolder::NotifyItemRemoved(nsISupports *item)
{

  PRInt32 i;
  for(i = 0; i < mListeners.Count(); i++)
  {
    //Folderlistener's aren't refcounted.
    nsIFolderListener *listener = (nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemRemoved(this, item);
  }
  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemRemoved(this, item);

  return NS_OK;

}


nsresult nsMsgDBFolder::NotifyFolderEvent(nsIAtom* aEvent)
{
  PRInt32 i;
  for(i = 0; i < mListeners.Count(); i++)
  {
    //Folderlistener's aren't refcounted.
    nsIFolderListener *listener = (nsIFolderListener*)mListeners.ElementAt(i);
    listener->OnItemEvent(this, aEvent);
  }
  //Notify listeners who listen to every folder
  nsresult rv;
  nsCOMPtr<nsIFolderListener> folderListenerManager =
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    folderListenerManager->OnItemEvent(this, aEvent);

  return NS_OK;
}

nsresult
nsGetMailFolderSeparator(nsString& result)
{
  result.AssignLiteral(".sbd");
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDBFolder::GetFilterList(nsIMsgWindow *aMsgWindow, nsIMsgFilterList **aResult)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(server, NS_ERROR_FAILURE);

  return server->GetFilterList(aMsgWindow, aResult);
}

NS_IMETHODIMP
nsMsgDBFolder::SetFilterList(nsIMsgFilterList *aFilterList)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(server, NS_ERROR_FAILURE);
  
  return server->SetFilterList(aFilterList);
}

/* void enableNotifications (in long notificationType, in boolean enable); */
NS_IMETHODIMP nsMsgDBFolder::EnableNotifications(PRInt32 notificationType, PRBool enable, PRBool dbBatching)
{
  if (notificationType == nsIMsgFolder::allMessageCountNotifications)
  {
    mNotifyCountChanges = enable;

    // start and stop db batching here. This is under the theory
    // that any time we want to enable and disable notifications,
    // we're probably doing something that should be batched.
    nsCOMPtr <nsIMsgDatabase> database;

    if (dbBatching)  //only if we do dbBatching we need to get db
      GetMsgDatabase(nsnull, getter_AddRefs(database));

    if (enable)
    {
      if (database)
        database->EndBatch();
      UpdateSummaryTotals(PR_TRUE);
    }
    else if (database)
      return database->StartBatch();

    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgDBFolder::GetMessageHeader(nsMsgKey msgKey, nsIMsgDBHdr **aMsgHdr)
{
  nsresult rv = NS_OK;
  if (aMsgHdr)
  {
    nsCOMPtr <nsIMsgDatabase> database;

    rv = GetMsgDatabase(nsnull, getter_AddRefs(database));
    if (NS_SUCCEEDED(rv) && database) // did we get a db back?
      rv = database->GetMsgHdrForKey(msgKey, aMsgHdr);
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

// this gets the deep sub-folders too, e.g., the children of the children
NS_IMETHODIMP nsMsgDBFolder::ListDescendents(nsISupportsArray *descendents)
{
  NS_ENSURE_ARG(descendents);
  PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 index = 0; index < cnt; index++)
  {
    nsresult rv;
    nsCOMPtr<nsISupports> supports(do_QueryElementAt(mSubFolders, index));
    nsCOMPtr<nsIMsgFolder> child(do_QueryInterface(supports, &rv));

    if (NS_SUCCEEDED(rv))
    {
      if (!descendents->AppendElement(supports))
        rv = NS_ERROR_OUT_OF_MEMORY;
      else
        rv = child->ListDescendents(descendents);  // recurse
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::GetBaseMessageURI(char **baseMessageURI)
{
  NS_ENSURE_ARG_POINTER(baseMessageURI);

  if (!mBaseMessageURI)
    return NS_ERROR_FAILURE;

  *baseMessageURI = nsCRT::strdup(mBaseMessageURI);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetUriForMsg(nsIMsgDBHdr *msgHdr, char **aURI)
{
  NS_ENSURE_ARG(msgHdr);
  NS_ENSURE_ARG(aURI);
  nsMsgKey msgKey;
  msgHdr->GetMessageKey(&msgKey);
  nsCAutoString uri;
  uri.Assign(mBaseMessageURI);

  // append a "#" followed by the message key.
  uri.Append('#');
  uri.AppendInt(msgKey);

  *aURI = ToNewCString(uri);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GenerateMessageURI(nsMsgKey msgKey, char **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsXPIDLCString baseURI;

  nsresult rv = GetBaseMessageURI(getter_Copies(baseURI));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCAutoString uri;
  uri.Assign(baseURI);

  // append a "#" followed by the message key.
  uri.Append('#');
  uri.AppendInt(msgKey);

  *aURI = ToNewCString(uri);
  if (! *aURI)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

nsresult
nsMsgDBFolder::GetBaseStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService =
         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                 getter_AddRefs(bundle));
  *aBundle = bundle;
  NS_IF_ADDREF(*aBundle);
  return rv;
}

nsresult //Do not use this routine if you have to call it very often because it creates a new bundle each time
nsMsgDBFolder::GetStringFromBundle(const char *msgName, PRUnichar **aResult)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aResult);
  nsCOMPtr <nsIStringBundle> bundle;
  rv = GetBaseStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
    rv=bundle->GetStringFromName(NS_ConvertASCIItoUCS2(msgName).get(), aResult);
  return rv;

}

nsresult
nsMsgDBFolder::ThrowConfirmationPrompt(nsIMsgWindow *msgWindow, const PRUnichar *confirmString, PRBool *confirmed)
{
  nsresult rv=NS_OK;
  if (msgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && confirmString)
        dialog->Confirm(nsnull, confirmString, confirmed);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetStringWithFolderNameFromBundle(const char *msgName, PRUnichar **aResult)
{
  nsCOMPtr <nsIStringBundle> bundle;
  nsresult rv = GetBaseStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
  {
    nsXPIDLString folderName;
    GetName(getter_Copies(folderName));
    const PRUnichar *formatStrings[] =
    {
      folderName
    };
    rv = bundle->FormatStringFromName(NS_ConvertASCIItoUCS2(msgName).get(),
                                      formatStrings, 1, aResult);
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::ConfirmFolderDeletionForFilter(nsIMsgWindow *msgWindow, PRBool *confirmed)
{
  nsXPIDLString confirmString;
  nsresult rv = GetStringWithFolderNameFromBundle("confirmFolderDeletionForFilter", getter_Copies(confirmString));
  if (NS_SUCCEEDED(rv) && confirmString)
    rv = ThrowConfirmationPrompt(msgWindow, confirmString.get(), confirmed);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::ThrowAlertMsg(const char*msgName, nsIMsgWindow *msgWindow)
{
  nsXPIDLString alertString;
  nsresult rv = GetStringWithFolderNameFromBundle(msgName, getter_Copies(alertString));
  if (NS_SUCCEEDED(rv) && alertString && msgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && alertString)
        dialog->Alert(nsnull, alertString);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::AlertFilterChanged(nsIMsgWindow *msgWindow)
{  //this is a different alert i.e  alert w/ checkbox.
  nsresult rv = NS_OK;
  PRBool checkBox=PR_FALSE;
  GetWarnFilterChanged(&checkBox);
  if (msgWindow && !checkBox)
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    nsXPIDLString alertString;
    rv = GetStringFromBundle("alertFilterChanged", getter_Copies(alertString));
    nsXPIDLString alertCheckbox;
    rv = GetStringFromBundle("alertFilterCheckbox", getter_Copies(alertCheckbox));
    if (alertString && alertCheckbox && docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog)
      {
        dialog->AlertCheck(nsnull, alertString, alertCheckbox, &checkBox);
        SetWarnFilterChanged(checkBox);
      }
    }
  }
  return rv;
}

nsresult
nsMsgDBFolder::GetWarnFilterChanged(PRBool *aVal)
{
  NS_ENSURE_ARG(aVal);
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
  {
    rv = prefBranch->GetBoolPref(PREF_MAIL_WARN_FILTER_CHANGED, aVal);
    if (NS_FAILED(rv))
    {
      *aVal = PR_FALSE;
      rv = NS_OK;
    }
  }
  return rv;
}

nsresult
nsMsgDBFolder::SetWarnFilterChanged(PRBool aVal)
{
  nsresult rv=NS_OK;
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch)
    rv = prefBranch->SetBoolPref(PREF_MAIL_WARN_FILTER_CHANGED, aVal);
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::NotifyCompactCompleted()
{
  NS_ASSERTION(PR_FALSE, "should be overridden by child class");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgDBFolder::CloseDBIfFolderNotOpen()
{
  nsresult rv;
  nsCOMPtr<nsIMsgMailSession> session = 
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 
  if (NS_SUCCEEDED(rv) && session) // don't use NS_ENSURE_SUCCESS here - we need to release semaphore below
  {
    PRBool folderOpen;
    session->IsFolderOpenInWindow(this, &folderOpen);
    if (!folderOpen && ! (mFlags & (MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_INBOX)))
      SetMsgDatabase(nsnull);
  }
  return rv;
}

NS_IMETHODIMP nsMsgDBFolder::SetSortOrder(PRInt32 order)
{
  NS_ASSERTION(PR_FALSE, "not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBFolder::GetSortOrder(PRInt32 *order)
{
  NS_ENSURE_ARG_POINTER(order);

  PRUint32 flags;
  nsresult rv = GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv,rv);

  if (flags & MSG_FOLDER_FLAG_INBOX)
    *order = 0;
  else if (flags & MSG_FOLDER_FLAG_QUEUE)
    *order = 1;
  else if (flags & MSG_FOLDER_FLAG_DRAFTS)
    *order = 2;
  else if (flags & MSG_FOLDER_FLAG_TEMPLATES)
    *order = 3;
  else if (flags & MSG_FOLDER_FLAG_SENTMAIL)
    *order = 4;
  else if (flags & MSG_FOLDER_FLAG_JUNK)
    *order = 5;
  else if (flags & MSG_FOLDER_FLAG_TRASH)
    *order = 6;
  else if (flags & MSG_FOLDER_FLAG_VIRTUAL)
    *order = 7;
  else
    *order = 8;

  return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::GetSortKey(PRUint8 **aKey, PRUint32 *aLength)
{
  NS_ENSURE_ARG(aKey);
  PRInt32 order;
  nsresult rv = GetSortOrder(&order);
  NS_ENSURE_SUCCESS(rv,rv);
  nsAutoString orderString;
  orderString.AppendInt(order);

  nsXPIDLString folderName;
  rv = GetName(getter_Copies(folderName));
  NS_ENSURE_SUCCESS(rv,rv);
  orderString.Append(folderName);
  return CreateCollationKey(orderString, aKey, aLength);
}

NS_IMETHODIMP nsMsgDBFolder::GetPersistElided(PRBool *aPersistElided)
{
  // by default, we should always persist the open / closed state of folders & servers
  *aPersistElided = PR_TRUE;
  return NS_OK;
}

nsresult
nsMsgDBFolder::CreateCollationKey(const nsString &aSource,  PRUint8 **aKey, PRUint32 *aLength)
{
  NS_ASSERTION(gCollationKeyGenerator, "gCollationKeyGenerator is null");
  if (!gCollationKeyGenerator)
    return NS_ERROR_NULL_POINTER;

  return gCollationKeyGenerator->AllocateRawSortKey(kCollationCaseInSensitive, aSource, aKey, aLength);
}

NS_IMETHODIMP nsMsgDBFolder::CompareSortKeys(nsIMsgFolder *aFolder, PRInt32 *sortOrder)
{
  PRUint8 *sortKey1=nsnull;
  PRUint8 *sortKey2=nsnull;
  PRUint32 sortKey1Length;
  PRUint32 sortKey2Length;
  nsresult rv = GetSortKey(&sortKey1, &sortKey1Length);
  NS_ENSURE_SUCCESS(rv,rv);
  aFolder->GetSortKey(&sortKey2, &sortKey2Length);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = gCollationKeyGenerator->CompareRawSortKey(sortKey1, sortKey1Length, sortKey2, sortKey2Length, sortOrder);
  PR_Free(sortKey1);
  PR_Free(sortKey2);
  return rv;
}

