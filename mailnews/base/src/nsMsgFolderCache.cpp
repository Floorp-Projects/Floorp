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
#include "nsIMsgAccountManager.h"
#include "nsMsgFolderCacheElement.h"
#include "nsMsgFolderCache.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "nsFileStream.h"

#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kCMorkFactory, NS_MORK_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

const char *kFoldersScope = "ns:msg:db:row:scope:folders:all";	// scope for all folders table
const char *kFoldersTableKind = "ns:msg:db:table:kind:folders";

nsMsgFolderCache::nsMsgFolderCache()
{
	m_mdbEnv = nsnull;
	m_mdbStore = nsnull;
	NS_INIT_REFCNT();
	m_mdbAllFoldersTable = nsnull;
    nsresult rv = nsServiceManager::GetService(kCMsgMailSessionCID,
                                      nsCOMTypeInfo<nsIMsgMailSession>::GetIID(),
                                      (nsISupports**)&m_mailSession,
                                      this);
	NS_ASSERTION(NS_SUCCEEDED(rv), "have to get mail session service");
}

// should this, could this be an nsCOMPtr ?
static nsIMdbFactory *gMDBFactory = nsnull;

nsMsgFolderCache::~nsMsgFolderCache()
{
	if (m_mdbStore)
		m_mdbStore->Release();
	if (gMDBFactory)
		gMDBFactory->CutStrongRef(GetEnv());
	gMDBFactory = nsnull;
	NS_RELEASE(m_cacheElements);
	if (m_mdbEnv)
		m_mdbEnv->CutStrongRef(m_mdbEnv); //??? is this right?
}




NS_IMPL_ADDREF(nsMsgFolderCache)
NS_IMPL_RELEASE(nsMsgFolderCache)
  
nsresult
nsMsgFolderCache::QueryInterface(const nsIID& iid, void **result)
{
	nsresult rv = NS_NOINTERFACE;
	if (! result)
		return NS_ERROR_NULL_POINTER;

	void *res = nsnull;
	if (iid.Equals(nsCOMTypeInfo<nsIMsgFolderCache>::GetIID()) ||
	      iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
		res = NS_STATIC_CAST(nsIMsgFolderCache*, this);
	else if (iid.Equals(nsCOMTypeInfo<nsIShutdownListener>::GetIID()))
		res = NS_STATIC_CAST(nsIShutdownListener*, this);

	if (res) 
	{
		NS_ADDREF(this);
		*result = res;
		rv = NS_OK;
	}
	return rv;
}


/* called if the mail session service goes offline */
NS_IMETHODIMP
nsMsgFolderCache::OnShutdown(const nsCID& aClass, nsISupports *service)
{
	if (aClass.Equals(kCMsgMailSessionCID)) 
	{
		nsCOMPtr <nsIMsgAccountManager> accountManager;

		if (m_mailSession)
		{
			nsresult rv = m_mailSession->GetAccountManager(getter_AddRefs(accountManager));
			if (NS_SUCCEEDED(rv)  && accountManager)
				rv = accountManager->WriteToFolderCache(this);
			// releasing the service seems to be wrong, since it causes the service to
			// be deleted out from under itself while getting shut down.
//			nsServiceManager::ReleaseService(kCMsgMailSessionCID, m_mailSession /*, this */);
		}
		m_mailSession = nsnull;
	}

	return NS_OK;
}


/* static */ nsIMdbFactory *nsMsgFolderCache::GetMDBFactory()
{
	if (!gMDBFactory)
	{
		nsresult rv;
        rv = nsComponentManager::CreateInstance(kCMorkFactory, nsnull, nsIMdbFactoryFactory::GetIID(), (void **) &gMDBFactory);
	}
	return gMDBFactory;
}

// initialize the various tokens and tables in our db's env
nsresult nsMsgFolderCache::InitMDBInfo()
{
	nsresult err = NS_OK;

	if (GetStore())
	{
		err	= GetStore()->StringToToken(GetEnv(), kFoldersScope, &m_folderRowScopeToken); 
		if (err == NS_OK)
		{
			err = GetStore()->StringToToken(GetEnv(), kFoldersTableKind, &m_folderTableKindToken); 
			if (err == NS_OK)
			{
				// The table of all message hdrs will have table id 1.
				m_allFoldersTableOID.mOid_Scope = m_folderRowScopeToken;
				m_allFoldersTableOID.mOid_Id = 1;
			}
		}
	}
	return err;
}

// set up empty tables, dbFolderInfo, etc.
nsresult nsMsgFolderCache::InitNewDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		nsIMdbStore *store = GetStore();
		// create the unique table for the dbFolderInfo.
		mdb_err mdberr;

        mdberr = (nsresult) store->NewTable(GetEnv(), m_folderRowScopeToken, 
			m_folderTableKindToken, PR_FALSE, nsnull, &m_mdbAllFoldersTable);

	}
	return err;
}

nsresult nsMsgFolderCache::InitExistingDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		err = GetStore()->GetTable(GetEnv(), &m_allFoldersTableOID, &m_mdbAllFoldersTable);
		if (NS_SUCCEEDED(err) && m_mdbAllFoldersTable)
		{
			nsIMdbTableRowCursor* rowCursor = nsnull;
			err = m_mdbAllFoldersTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
			if (NS_SUCCEEDED(err) && rowCursor)
			{
				// iterate over the table rows and create nsMsgFolderCacheElements for each.
				while (TRUE)
				{
					nsresult rv;
					nsIMdbRow* hdrRow;
					mdb_pos rowPos;

					rv = rowCursor->NextRow(GetEnv(), &hdrRow, &rowPos);
					if (NS_FAILED(rv) || !hdrRow)
						break;

					rv = AddCacheElement(nsnull, hdrRow, nsnull);
//					rv = mDB->CreateMsgHdr(hdrRow, key, &mResultHdr);
					if (NS_FAILED(rv))
						return rv;
				}
				rowCursor->Release();
			}
		}
	}
	return err;
}

nsresult nsMsgFolderCache::OpenMDB(const char *dbName, PRBool create)
{
	nsresult ret;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb = nsnull;
			char	*nativeFileName = nsCRT::strdup(dbName);

			if (!nativeFileName)
				return NS_ERROR_OUT_OF_MEMORY;

			if (m_mdbEnv)
				m_mdbEnv->SetAutoClear(PR_TRUE);
#if defined(XP_PC) || defined(XP_MAC)
//			UnixToNative(nativeFileName);
#endif
			{
				mdbOpenPolicy inOpenPolicy;
				mdb_bool	canOpen;
				mdbYarn		outFormatVersion;
				char		bufFirst512Bytes[512];
				mdbYarn		first512Bytes;

				first512Bytes.mYarn_Buf = bufFirst512Bytes;
				first512Bytes.mYarn_Size = 512;
				first512Bytes.mYarn_Fill = 512;
				first512Bytes.mYarn_Form = 0;	// what to do with this? we're storing csid in the msg hdr...

				{
					nsIOFileStream *dbStream = new nsIOFileStream(nsFileSpec(dbName));
					if (dbStream)
					{
						PRInt32 bytesRead = dbStream->read(bufFirst512Bytes, sizeof(bufFirst512Bytes));
						first512Bytes.mYarn_Fill = bytesRead;
						dbStream->close();
						delete dbStream;
					}
					else
						return NS_ERROR_OUT_OF_MEMORY;
				}
				ret = myMDBFactory->CanOpenFilePort(m_mdbEnv, nativeFileName, // the file to investigate
					&first512Bytes,	&canOpen, &outFormatVersion);
				if (ret == 0 && canOpen)
				{

					inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
					inOpenPolicy.mOpenPolicy_MinMemory = 0;
					inOpenPolicy.mOpenPolicy_MaxLazy = 0;

					ret = myMDBFactory->OpenFileStore(m_mdbEnv, NULL, nativeFileName, &inOpenPolicy, 
									&thumb); 
				}
				else
					ret = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
			}
			if (NS_SUCCEEDED(ret) && thumb)
			{
				mdb_count outTotal;    // total somethings to do in operation
				mdb_count outCurrent;  // subportion of total completed so far
				mdb_bool outDone = PR_FALSE;      // is operation finished?
				mdb_bool outBroken;     // is operation irreparably dead and broken?
				do
				{
					ret = thumb->DoMore(m_mdbEnv, &outTotal, &outCurrent, &outDone, &outBroken);
					if (ret != 0)
					{// mork isn't really doing NS erorrs yet.
						outDone = PR_TRUE;
						break;
					}
				}
				while (NS_SUCCEEDED(ret) && !outBroken && !outDone);
//				m_mdbEnv->ClearErrors(); // ### temporary...
				if (NS_SUCCEEDED(ret) && outDone)
				{
					ret = myMDBFactory->ThumbToOpenStore(m_mdbEnv, thumb, &m_mdbStore);
					if (ret == NS_OK && m_mdbStore)
						ret = InitExistingDB();
				}
#ifdef DEBUG_bienvenu1
				DumpContents();
#endif
			}
			else if (create)	// ### need error code saying why open file store failed
			{
				mdbOpenPolicy inOpenPolicy;

				inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
				inOpenPolicy.mOpenPolicy_MinMemory = 0;
				inOpenPolicy.mOpenPolicy_MaxLazy = 0;

				ret = myMDBFactory->CreateNewFileStore(m_mdbEnv, NULL, dbName, &inOpenPolicy, &m_mdbStore);
				if (ret == NS_OK)
					ret = InitNewDB();
			}
			if(thumb)
			{
				thumb->CutStrongRef(m_mdbEnv);
			}
			nsCRT::free(nativeFileName);
		}
	}
	return ret;
}

NS_IMETHODIMP nsMsgFolderCache::Init(nsIFileSpec *dbFileSpec)
{
	if (!dbFileSpec)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_NewISupportsArray(&m_cacheElements);

	if (NS_SUCCEEDED(rv) && m_cacheElements)
	{
		rv = dbFileSpec->GetFileSpec(&m_dbFileSpec);

		if (NS_SUCCEEDED(rv))
		{
			// ### evil cast until MDB supports file streams.
			rv = OpenMDB((const char *) m_dbFileSpec, PR_TRUE);
		}
	}
	return rv;
}

typedef struct _findCacheElementByURIEntry {
  const char *m_uri;
  nsIMsgFolderCacheElement *m_cacheElement;
} findCacheElementByURIEntry;

NS_IMETHODIMP nsMsgFolderCache::GetCacheElement(const char *uri, PRBool createIfMissing, 
							nsIMsgFolderCacheElement **result)
{
	if (!result || !uri)
		return NS_ERROR_NULL_POINTER;

	findCacheElementByURIEntry findEntry;
	findEntry.m_uri = uri;
	findEntry.m_cacheElement = nsnull;

	m_cacheElements->EnumerateForwards(FindCacheElementByURI, (void *)&findEntry);

	if (findEntry.m_cacheElement)
	{
		*result = findEntry.m_cacheElement;
		NS_ADDREF(*result);
		return NS_OK;
	}
	else if (createIfMissing)
	{
		nsIMdbRow* hdrRow;

		if (GetStore())
		{
			mdb_err err = GetStore()->NewRow(GetEnv(), m_folderRowScopeToken,   // row scope for row ids
				&hdrRow);
			if (NS_SUCCEEDED(err) && hdrRow)
			{
				m_mdbAllFoldersTable->AddRow(GetEnv(), hdrRow);
				nsresult ret = AddCacheElement(uri, hdrRow, result);
				if (*result)
					(*result)->SetStringProperty("uri", uri);
				return ret;
			}
		}
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolderCache::Close()
{
	nsresult ret;

	nsIMdbThumb	*commitThumb = NULL;
	if (m_mdbStore)
		ret = m_mdbStore->CompressCommit(GetEnv(), &commitThumb);

	if (commitThumb)
	{
		mdb_count outTotal = 0;    // total somethings to do in operation
		mdb_count outCurrent = 0;  // subportion of total completed so far
		mdb_bool outDone = PR_FALSE;      // is operation finished?
		mdb_bool outBroken = PR_FALSE;     // is operation irreparably dead and broken?
		while (!outDone && !outBroken && ret == NS_OK)
		{
			ret = commitThumb->DoMore(GetEnv(), &outTotal, &outCurrent, &outDone, &outBroken);
		}
		if(commitThumb)
			commitThumb->CutStrongRef(m_mdbEnv);
	}
	// ### do something with error, but clear it now because mork errors out on commits.
	if (GetEnv())
		GetEnv()->ClearErrors();
	return ret;
}


PRBool
nsMsgFolderCache::FindCacheElementByURI(nsISupports *aElement, void *data)
{
	nsresult rv;
	nsCOMPtr<nsIMsgFolderCacheElement> cacheElement = do_QueryInterface(aElement, &rv);
	if (NS_FAILED(rv)) 
		return PR_TRUE;

	findCacheElementByURIEntry *entry = (findCacheElementByURIEntry *) data;

	nsXPIDLCString key;
	rv = cacheElement->GetURI(getter_Copies(key));
	if (NS_FAILED(rv)) 
		return rv;
  
	if (entry && entry->m_uri && !PL_strcmp(key, entry->m_uri ))
	{
		entry->m_cacheElement = cacheElement;
		NS_ADDREF(entry->m_cacheElement);
		return PR_FALSE;
	}

	return PR_TRUE;
}

nsresult nsMsgFolderCache::AddCacheElement(const char *uri, nsIMdbRow *row, nsIMsgFolderCacheElement **result)
{
	nsMsgFolderCacheElement *cacheElement = new nsMsgFolderCacheElement;

	if (cacheElement)
	{
		cacheElement->SetMDBRow(row);
		cacheElement->SetOwningCache(this);
		// if caller didn't pass in URI, try to get it from row.
		if (!uri)
		{
			char *existingURI = nsnull;
			cacheElement->GetStringProperty("uri", &existingURI);	
			cacheElement->SetURI(existingURI);
			PR_Free(existingURI);
		}
		else
			cacheElement->SetURI((char *) uri);
		nsCOMPtr<nsISupports> supports(do_QueryInterface(cacheElement));
		if(supports)
			m_cacheElements->AppendElement(supports);
		if (result)
		{
			*result = cacheElement;
			NS_ADDREF(*result);
		}
		return NS_OK;
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMsgFolderCache::RowCellColumnToCharPtr(nsIMdbRow *hdrRow, mdb_token columnToken, char **resultPtr)
{
	nsresult	err = NS_OK;
	nsIMdbCell	*hdrCell;

	if (hdrRow)	// ### probably should be an error if hdrRow is NULL...
	{
		err = hdrRow->GetCell(GetEnv(), columnToken, &hdrCell);
		if (err == NS_OK && hdrCell)
		{
			struct mdbYarn yarn;
			hdrCell->AliasYarn(GetEnv(), &yarn);
			char *result = (char *) PR_Malloc(yarn.mYarn_Fill + 1);
			if (result)
			{
				nsCRT::memcpy(result, yarn.mYarn_Buf, yarn.mYarn_Fill);
				result[yarn.mYarn_Fill] = '\0';
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;

			*resultPtr = result;
			hdrCell->CutStrongRef(GetEnv()); // always release ref
		}
	}
	return err;
}
