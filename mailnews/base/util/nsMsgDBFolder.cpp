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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsIMessage.h"
#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsIPref.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

NS_IMPL_ADDREF_INHERITED(nsMsgDBFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgDBFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgDBFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsCOMTypeInfo<nsIDBChangeListener>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIDBChangeListener*, this);
	}              

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsRDFResource::QueryInterface(aIID, aInstancePtr);
}

nsMsgDBFolder::nsMsgDBFolder(void)
: mCharset("")
{

}

nsMsgDBFolder::~nsMsgDBFolder(void)
{
	if(mDatabase)
	{
		mDatabase->RemoveListener(this);
		mDatabase->Close(PR_TRUE);
	}
}

NS_IMETHODIMP nsMsgDBFolder::GetThreads(nsIEnumerator** threadEnumerator)
{
	nsresult rv = GetDatabase();
	
	if(NS_SUCCEEDED(rv))
		return mDatabase->EnumerateThreads(threadEnumerator);
	else
		return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread)
{
	nsresult rv = GetDatabase();
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

	nsresult rv = GetDatabase();

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

	if(mCharset == "")
	{
		NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);

		char *prefCharset = nsnull;
		if (NS_SUCCEEDED(rv))
		{
			rv = prefs->CopyCharPref("intl.character_set_name", &prefCharset);
		}
  
		nsString prefCharsetStr;
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

NS_IMETHODIMP nsMsgDBFolder::SetCharset(PRUnichar * aCharset)
{
	nsresult rv;

	nsCOMPtr<nsIDBFolderInfo> folderInfo;
	nsCOMPtr<nsIMsgDatabase> db; 
	rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
	if(NS_SUCCEEDED(rv))
	{
		nsString charset(aCharset);
		rv = folderInfo->SetCharacterSet(&charset);
		db->Commit(nsMsgDBCommitType::kLargeCommit);
	}
	return rv;
}

nsresult nsMsgDBFolder::ReadDBFolderInfo(PRBool force)
{
	// Since it turns out to be pretty expensive to open and close
	// the DBs all the time, if we have to open it once, get everything
	// we might need while we're here

	nsresult result;

	nsCOMPtr <nsIMsgFolderCache> folderCache;

	NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &result); 
	if(NS_SUCCEEDED(result))
	{
		result = mailSession->GetFolderCache(getter_AddRefs(folderCache));
		if (NS_SUCCEEDED(result) && folderCache)
		{
			char *uri;

			result = GetURI(&uri);
			if (NS_SUCCEEDED(result) && uri)
			{
				nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
				result = folderCache->GetCacheElement(uri, PR_FALSE, getter_AddRefs(cacheElement));
				if (NS_SUCCEEDED(result) && cacheElement)
				{
					result = ReadFromFolderCache(cacheElement);
				}
				PR_Free(uri);
			}

		}
	}
//	if (m_master->InitFolderFromCache (this))
//		return err;

	if (force || !(mPrefFlags & MSG_FOLDER_PREF_CACHED))
    {
        nsCOMPtr<nsIDBFolderInfo> folderInfo;
        nsCOMPtr<nsIMsgDatabase> db; 
        result = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
        if(NS_SUCCEEDED(result))
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

				folderInfo->GetCharacterSet(&mCharset);
        
				if (db && !db->HasNew() && mNumPendingUnreadMessages <= 0)
					ClearFlag(MSG_FOLDER_FLAG_GOT_NEW);
            }

        }
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
		|| (changedFlags & MSG_FLAG_MARKED) || (changedFlags & MSG_FLAG_FORWARDED)
		|| (changedFlags & MSG_FLAG_NEW))
	{
		rv = NotifyPropertyFlagChanged(item, "Status", oldFlags, newFlags);
	}
	return rv;
}

NS_IMETHODIMP
nsMsgDBFolder::GetMsgDatabase(nsIMsgDatabase** aMsgDatabase)
{
    if (!aMsgDatabase || !mDatabase)
        return NS_ERROR_NULL_POINTER;
    *aMsgDatabase = mDatabase;
    NS_ADDREF(*aMsgDatabase);
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

NS_IMETHODIMP nsMsgDBFolder::OnKeyDeleted(nsMsgKey aKeyChanged, nsMsgKey /* aParentKey */, PRInt32 aFlags, 
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
				NotifyItemDeleted(msgSupports);
			}
			UpdateSummaryTotals(PR_TRUE);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey /* aParentKey */, PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	nsresult rv;
	nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
	rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(msgDBHdr));
	if(NS_SUCCEEDED(rv) && msgDBHdr)
	{
		nsCOMPtr<nsIMessage> message;
		rv = CreateMessageFromMsgDBHdr(msgDBHdr, getter_AddRefs(message));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(message));
			if(msgSupports)
			{
				NotifyItemAdded(msgSupports);
			}
			UpdateSummaryTotals(PR_TRUE);
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnParentChanged(nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, 
						nsIDBChangeListener * aInstigator)
{
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

nsresult nsMsgDBFolder::ReadFromFolderCache(nsIMsgFolderCacheElement *element)
{
	nsresult rv = NS_OK;
	char *charset;

	element->GetInt32Property("flags", &mPrefFlags);
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

    mPrefFlags |= MSG_FOLDER_PREF_CACHED;
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::WriteToFolderCache(nsIMsgFolderCache *folderCache)
{
	nsCOMPtr <nsIEnumerator> aEnumerator;

	nsresult rv = GetSubFolders(getter_AddRefs(aEnumerator));
	if(NS_FAILED(rv)) 
		return rv;

	char *uri = nsnull;
	rv = GetURI(&uri);

	if (folderCache)
	{
		nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
		rv = folderCache->GetCacheElement(uri, PR_TRUE, getter_AddRefs(cacheElement));
		if (NS_SUCCEEDED(rv) && cacheElement)
			rv = WriteToFolderCacheElem(cacheElement);
	}
	PR_FREEIF(uri);

	
	nsCOMPtr<nsISupports> aItem;

	rv = aEnumerator->First();
	while(NS_SUCCEEDED(rv))
	{
		rv = aEnumerator->CurrentItem(getter_AddRefs(aItem));
		if (NS_FAILED(rv)) break;
		nsCOMPtr<nsIMsgFolder> aMsgFolder(do_QueryInterface(aItem, &rv));
		if (NS_SUCCEEDED(rv))
		{
			if (folderCache)
				rv = aMsgFolder->WriteToFolderCache(folderCache);
		}
		rv = aEnumerator->Next();
	}
	return rv;
}

NS_IMETHODIMP nsMsgDBFolder::WriteToFolderCacheElem(nsIMsgFolderCacheElement *element)
{
	nsresult rv = NS_OK;

	element->SetInt32Property("flags", mPrefFlags);
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
	nsresult rv = GetDatabase();
	
	if(NS_SUCCEEDED(rv))
		return mDatabase->MarkAllRead(nsnull);

	return rv;
}

