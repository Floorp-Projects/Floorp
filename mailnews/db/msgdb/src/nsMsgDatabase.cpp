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

// this file implements the nsMsgDatabase interface using the MDB Interface.

#include "nsMsgDatabase.h"

#ifdef WE_HAVE_MDBINTERFACES
static NS_DEFINE_IID(kIMDBIID, NS_IMDB_IID);
static NS_DEFINE_IID(kIMBBCID, NS_IMBB_IID);



	nsresult rv = nsRepository::CreateInstance(kMDBCID, nsnull, kIMDBIID, (void **)&gMDBInterface);

	if (nsnull != devSpec){
	  if (NS_OK == ((nsDeviceContextSpecMac *)devSpec)->Init(aQuiet)){
	    aNewSpec = devSpec;
	    rv = NS_OK;
	  }
	}
#endif

nsMsgDatabaseArray *nsMsgDatabase::m_dbCache = NULL;

nsMsgDatabaseArray::nsMsgDatabaseArray()
{
}

//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------
nsMsgDatabaseArray *
nsMsgDatabase::GetDBCache()
{
	if (!m_dbCache)
	{
		m_dbCache = new nsMsgDatabaseArray();
	}
	return m_dbCache;
	
}

void
nsMsgDatabase::CleanupCache()
{
	if (m_dbCache) // clean up memory leak
	{
		for (int i = 0; i < GetDBCache()->GetSize(); i++)
		{
			nsMsgDatabase* pMessageDB = GetDBCache()->GetAt(i);
			if (pMessageDB)
			{
#ifdef DEBUG_bienvenu
				XP_Trace("closing %s\n", pMessageDB->m_dbName);
#endif
				pMessageDB->ForceClosed();
				i--;	// back up array index, since closing removes db from cache.
			}
		}
		XP_ASSERT(GetNumInCache() == 0);	// better not be any open db's.
		delete m_dbCache;
	}
	m_dbCache = NULL; // Need to reset to NULL since it's a
			  // static global ptr and maybe referenced 
			  // again in other places.
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
nsMsgDatabase* nsMsgDatabase::FindInCache(nsFilePath &dbName)
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
		nsMsgDatabase* pMessageDB = GetDBCache()->GetAt(i);
		if (pMessageDB->MatchDbName(dbName))
		{
			return(pMessageDB);
		}
	}
	return(NULL);
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
int nsMsgDatabase::FindInCache(nsMsgDatabase* pMessageDB)
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
		if (GetDBCache()->GetAt(i) == pMessageDB)
		{
			return(i);
		}
	}
	return(-1);
}

PRBool nsMsgDatabase::MatchDbName(nsFilePath &dbName)	// returns TRUE if they match
{
	// ### we need equality operators for nsFileSpec...
	return strcmp(dbName, m_dbName); 
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void nsMsgDatabase::RemoveFromCache(nsMsgDatabase* pMessageDB)
{
	int i = FindInCache(pMessageDB);
	if (i != -1)
	{
		GetDBCache()->RemoveAt(i);
	}
}


#ifdef DEBUG
void nsMsgDatabase::DumpCache()
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
#ifdef DEBUG_bienvenu
		nsMsgDatabase* pMessageDB = 
#endif
        GetDBCache()->GetAt(i);
#ifdef DEBUG_bienvenu
		XP_Trace("db %s in cache use count = %d\n", pMessageDB->m_dbName, pMessageDB->mRefCnt);
#endif
	}
}
#endif /* DEBUG */

nsMsgDatabase::nsMsgDatabase() : m_dbName("")
{
	m_mdbEnv = NULL;
	m_mdbStore = NULL;
}

nsMsgDatabase::~nsMsgDatabase()
{
}

// ref counting methods - if we inherit from nsISupports, we won't need these,
// and we can take advantage of the nsISupports ref-counting tracing methods
nsrefcnt nsMsgDatabase::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsMsgDatabase::Release(void)
{
	NS_PRECONDITION(0 != mRefCnt, "dup release");
	if (--mRefCnt == 0) 
	{
		delete this;
		return 0;
	}
	return mRefCnt;
}

/* static */ mdbFactory *nsMsgDatabase::GetMDBFactory()
{
	static mdbFactory *gMDBFactory = NULL;
	if (!gMDBFactory)
	{
		// ### hook up class factory code when it's working
//		gMDBFactory = new mdbFactory;
	}
	return gMDBFactory;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
nsresult nsMsgDatabase::OpenMDB(const char *dbName, PRBool create)
{
	nsresult ret = NS_OK;
	mdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(&m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			mdbThumb *thumb;
			ret = myMDBFactory->OpenFileStore(m_mdbEnv, dbName, NULL, /* const mdbOpenPolicy* inOpenPolicy */
				&thumb); 
			if (NS_SUCCEEDED(ret))
			{
				mdb_count outTotal;    // total somethings to do in operation
				mdb_count outCurrent;  // subportion of total completed so far
				mdb_bool outDone = PR_FALSE;      // is operation finished?
				mdb_bool outBroken;     // is operation irreparably dead and broken?
				do
				{
					ret = thumb->DoMore(m_mdbEnv, &outTotal, &outCurrent, &outDone, &outBroken);
				}
				while (NS_SUCCEEDED(ret) && !outBroken && !outDone);
				if (NS_SUCCEEDED(ret) && outDone)
				{
					ret = myMDBFactory->ThumbToOpenStore(m_mdbEnv, thumb, &m_mdbStore);
				}
			}
		}
	}
	return ret;
}

nsresult		nsMsgDatabase::CloseMDB(PRBool commit /* = TRUE */)
{
	--mRefCnt;
	PR_ASSERT(mRefCnt >= 0);
	if (mRefCnt == 0)
	{
		RemoveFromCache(this);
#ifdef DEBUG_bienvenu1
		if (GetNumInCache() != 0)
		{
			XP_Trace("closing %s\n", m_dbName);
			DumpCache();
		}
		XP_Trace("On close - cache used = %lx", CNeoPersist::FCacheUsed);
#endif
		// if this terrifies you, we can make it a static method
		delete this;	
		return(NS_OK);
	}
	else
	{
		return(NS_OK);
	}
}

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
// This is evil in the com world, but there are times we need to delete the file.
nsresult nsMsgDatabase::ForceClosed()
{
	nsresult	err = NS_OK;

	while (mRefCnt > 0 && NS_SUCCEEDED(err))
	{
		int32 saveUseCount = mRefCnt;
		err = CloseMDB();
		if (saveUseCount == 1)
			break;
	}
	return err;
}

