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
nsMsgDatabase* nsMsgDatabase::FindInCache(const char * pDbName)
{
	for (int i = 0; i < GetDBCache()->GetSize(); i++)
	{
		nsMsgDatabase* pMessageDB = GetDBCache()->GetAt(i);
		if (pMessageDB->MatchDbName(pDbName))
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
		XP_Trace("db %s in cache use count = %d\n", pMessageDB->m_dbName, pMessageDB->m_useCount);
#endif
	}
}
