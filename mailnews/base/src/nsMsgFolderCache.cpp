/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIMsgAccountManager.h"
#include "nsMsgFolderCacheElement.h"
#include "nsMsgFolderCache.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "nsFileStream.h"
#include "nsIFileSpec.h"

#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

const char *kFoldersScope = "ns:msg:db:row:scope:folders:all";	// scope for all folders table
const char *kFoldersTableKind = "ns:msg:db:table:kind:folders";


nsMsgFolderCache::nsMsgFolderCache()
{
	m_mdbEnv = nsnull;
	m_mdbStore = nsnull;
	NS_INIT_REFCNT();
	m_mdbAllFoldersTable = nsnull;
}

// should this, could this be an nsCOMPtr ?
static nsIMdbFactory *gMDBFactory = nsnull;

nsMsgFolderCache::~nsMsgFolderCache()
{
	delete m_cacheElements;
	if (m_mdbStore)
		m_mdbStore->Release();
	if (gMDBFactory)
	{
		gMDBFactory->CloseMdbObject(m_mdbEnv);
//		gMDBFactory->CutStrongRef(GetEnv());
	}
	gMDBFactory = nsnull;
	if (m_mdbEnv)
	{
		m_mdbEnv->CloseMdbObject(m_mdbEnv);
//		m_mdbEnv->CutStrongRef(m_mdbEnv); //??? is this right?
	}
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
	if (iid.Equals(NS_GET_IID(nsIMsgFolderCache)) ||
	      iid.Equals(NS_GET_IID(nsISupports)))
		res = NS_STATIC_CAST(nsIMsgFolderCache*, this);
	if (res) 
	{
		NS_ADDREF(this);
		*result = res;
		rv = NS_OK;
	}
	return rv;
}

/* static */ nsIMdbFactory *nsMsgFolderCache::GetMDBFactory()
{
	if (!gMDBFactory)
	{
		nsresult rv;
		nsCOMPtr <nsIMdbFactoryFactory> factoryfactory;
		rv = nsComponentManager::CreateInstance(kMorkCID,
												  nsnull,
												  NS_GET_IID(nsIMdbFactoryFactory),
												  (void **) getter_AddRefs(factoryfactory));
		if (NS_SUCCEEDED(rv) && factoryfactory)
		  rv = factoryfactory->GetMdbFactory(&gMDBFactory);
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
				while (PR_TRUE)
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
    else
      err = NS_ERROR_FAILURE;
	}
	return err;
}

nsresult nsMsgFolderCache::OpenMDB(const char *dbName, PRBool exists)
{
	nsresult ret=NS_OK;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(nsnull, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb = nsnull;
			char	*nativeFileName = nsCRT::strdup(dbName);
			nsIMdbHeap* dbHeap = 0;;
			mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable

			if (!nativeFileName)
				return NS_ERROR_OUT_OF_MEMORY;

			if (m_mdbEnv)
				m_mdbEnv->SetAutoClear(PR_TRUE);
#if defined(XP_PC) || defined(XP_MAC)
//			UnixToNative(nativeFileName);
#endif
			if (exists)
			{
				mdbOpenPolicy inOpenPolicy;
				mdb_bool	canOpen;
				mdbYarn		outFormatVersion;
				
				nsIMdbFile* oldFile = 0;
				ret = myMDBFactory->OpenOldFile(m_mdbEnv, dbHeap, nativeFileName,
					 dbFrozen, &oldFile);
				if ( oldFile )
				{
					if ( ret == NS_OK )
					{
						ret = myMDBFactory->CanOpenFilePort(m_mdbEnv, oldFile, // file to investigate
							&canOpen, &outFormatVersion);
						if (ret == 0 && canOpen)
						{
							inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
							inOpenPolicy.mOpenPolicy_MinMemory = 0;
							inOpenPolicy.mOpenPolicy_MaxLazy = 0;

							ret = myMDBFactory->OpenFileStore(m_mdbEnv, NULL, oldFile, &inOpenPolicy, 
											&thumb); 
						}
						else
							ret = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
					}
					oldFile->CutStrongRef(m_mdbEnv); // always release our file ref, store has own
				}
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
			else // ### need error code saying why open file store failed
			{
				nsIMdbFile* newFile = 0;
				ret = myMDBFactory->CreateNewFile(m_mdbEnv, dbHeap, dbName, &newFile);
				if ( newFile )
				{
					if (ret == NS_OK)
					{
						mdbOpenPolicy inOpenPolicy;

						inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
						inOpenPolicy.mOpenPolicy_MinMemory = 0;
						inOpenPolicy.mOpenPolicy_MaxLazy = 0;

						ret = myMDBFactory->CreateNewFileStore(m_mdbEnv, dbHeap,
							 newFile, &inOpenPolicy, &m_mdbStore);
						if (ret == NS_OK)
							ret = InitNewDB();
					}
					newFile->CutStrongRef(m_mdbEnv); // always release our file ref, store has own
				}

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

	nsresult rv = NS_ERROR_OUT_OF_MEMORY;

	m_cacheElements = new nsSupportsHashtable;

	if (m_cacheElements)
	{
		rv = dbFileSpec->GetFileSpec(&m_dbFileSpec);

		if (NS_SUCCEEDED(rv))
		{
			PRBool exists = m_dbFileSpec.Exists();
			// ### evil cast until MDB supports file streams.
			rv = OpenMDB((const char *) m_dbFileSpec, exists);
      // if this fails and panacea.dat exists, try blowing away the db and recreating it
      if (!NS_SUCCEEDED(rv) && exists)
      {
	      if (m_mdbStore)
		      m_mdbStore->Release();
        m_dbFileSpec.Delete(PR_FALSE);
  			rv = OpenMDB((const char *) m_dbFileSpec, PR_FALSE);
      }
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgFolderCache::GetCacheElement(const char *pathKey, PRBool createIfMissing, 
							nsIMsgFolderCacheElement **result)
{

	if (!result || !pathKey)
		return NS_ERROR_NULL_POINTER;
		
	if (nsCRT::strlen(pathKey) == 0) {
		return NS_ERROR_FAILURE;
	}
	
	nsCStringKey hashKey(pathKey);

	*result = (nsIMsgFolderCacheElement *) m_cacheElements->Get(&hashKey);

	// nsHashTable already does an address on *result
	if (*result)
	{
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
				nsresult ret = AddCacheElement(pathKey, hdrRow, result);
				if (*result)
					(*result)->SetStringProperty("key", pathKey);
				return ret;
			}
		}
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgFolderCache::Close()
{
  return Commit(PR_TRUE);
}

NS_IMETHODIMP nsMsgFolderCache::Commit(PRBool compress)
{
	nsresult ret = NS_OK;

	nsIMdbThumb	*commitThumb = nsnull;
	if (m_mdbStore)
    if (compress)
		  ret = m_mdbStore->CompressCommit(GetEnv(), &commitThumb);
    else
      ret = m_mdbStore->LargeCommit(GetEnv(), &commitThumb);

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

nsresult nsMsgFolderCache::AddCacheElement(const char *key, nsIMdbRow *row, nsIMsgFolderCacheElement **result)
{
	nsMsgFolderCacheElement *cacheElement = new nsMsgFolderCacheElement;

	if (cacheElement)
	{
		cacheElement->SetMDBRow(row);
		cacheElement->SetOwningCache(this);
		nsCAutoString hashStrKey(key);
		// if caller didn't pass in key, try to get it from row.
		if (!key)
		{
			char *existingKey = nsnull;
			cacheElement->GetStringProperty("key", &existingKey);	
			cacheElement->SetKey(existingKey);
			hashStrKey = existingKey;
			PR_Free(existingKey);
		}
		else
			cacheElement->SetKey((char *) key);
		nsCOMPtr<nsISupports> supports(do_QueryInterface(cacheElement));
		if(supports)
		{
			nsCStringKey hashKey(hashStrKey);
			m_cacheElements->Put(&hashKey, supports);
		}
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
