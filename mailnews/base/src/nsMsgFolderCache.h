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

#ifndef nsMsgFolderCache_H
#define nsMsgFolderCache_H

#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "mdb.h"

class nsMsgFolderCache : public nsIMsgFolderCache
{

public:
	friend class nsMsgFolderCacheElement;

	nsMsgFolderCache();
	virtual ~nsMsgFolderCache();

	NS_DECL_ISUPPORTS

	//nsMsgFolderCache
	NS_IMETHOD Init(nsIFileSpec *dbFileSpec);
	NS_IMETHOD GetCacheElement(const char *uri, PRBool createIfMissing, 
								nsIMsgFolderCacheElement **result);
	NS_IMETHOD Close();


protected:
	static PRBool FindCacheElementByURI(nsISupports *aElement, void *data);
	static nsIMdbFactory *GetMDBFactory();

	nsresult AddCacheElement(const char *uri, nsIMdbRow *row, nsIMsgFolderCacheElement **result);

	nsresult RowCellColumnToCharPtr(nsIMdbRow *hdrRow, mdb_token columnToken, char **resultPtr);
	nsresult InitMDBInfo();
	nsresult InitNewDB();
	nsresult InitExistingDB();
	nsresult OpenMDB(const char *dbName, PRBool create);
	nsIMdbEnv				*GetEnv() {return m_mdbEnv;}
	nsIMdbStore				*GetStore() {return m_mdbStore;}

	nsFileSpec		m_dbFileSpec;
	nsISupportsArray	*m_cacheElements;

	// mdb stuff
	nsIMdbEnv		    *m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore	 	    *m_mdbStore;
	nsIMdbTable		    *m_mdbAllFoldersTable;
	mdb_token			m_folderRowScopeToken;
	mdb_token			m_folderTableKindToken;

	struct mdbOid		m_allFoldersTableOID;

};


#endif
