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

#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"


NS_IMPL_ADDREF_INHERITED(nsMsgDBFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgDBFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgDBFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsIDBChangeListener::GetIID()))
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
nsresult nsMsgDBFolder::ReadDBFolderInfo(PRBool force)
{
	// Since it turns out to be pretty expensive to open and close
	// the DBs all the time, if we have to open it once, get everything
	// we might need while we're here

	nsresult result= NS_OK;
	if (force || !(mPrefFlags & MSG_FOLDER_PREF_CACHED))
    {
        nsCOMPtr<nsIDBFolderInfo> folderInfo;
        nsCOMPtr<nsIMsgDatabase> db; 
        result = NS_SUCCEEDED(GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db)));
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

NS_IMETHODIMP nsMsgDBFolder::OnKeyChange(nsMsgKey aKeyChanged, PRUint32 aOldFlags, PRUint32 aNewFlags, 
                         nsIDBChangeListener * aInstigator)
{
	nsCOMPtr<nsIMsgDBHdr> pMsgDBHdr;
	nsresult rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(pMsgDBHdr));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMessage> message;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, getter_AddRefs(message));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsISupports> msgSupports(do_QueryInterface(message, &rv));
			if(NS_SUCCEEDED(rv))
			{
				NotifyPropertyFlagChanged(msgSupports, "Status", aOldFlags, aNewFlags);
			}
			UpdateSummaryTotals();
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnKeyDeleted(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
	nsCOMPtr<nsIMsgDBHdr> pMsgDBHdr;
	nsresult rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(pMsgDBHdr));
	if(NS_SUCCEEDED(rv))
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
			UpdateSummaryTotals();
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgDBFolder::OnKeyAdded(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	nsresult rv;
	nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
	rv = mDatabase->GetMsgHdrForKey(aKeyChanged, getter_AddRefs(msgDBHdr));
	if(NS_SUCCEEDED(rv))
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
			UpdateSummaryTotals();
		}
	}
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
