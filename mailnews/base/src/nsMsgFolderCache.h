/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef nsMsgFolderCache_H
#define nsMsgFolderCache_H

#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"

#include "nsCOMPtr.h"
#include "mdb.h"
#include "nsHashtable.h"

class nsMsgFolderCache : public nsIMsgFolderCache
{

public:
	friend class nsMsgFolderCacheElement;

	nsMsgFolderCache();
	virtual ~nsMsgFolderCache();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGFOLDERCACHE

protected:
	static nsIMdbFactory *GetMDBFactory();

	nsresult AddCacheElement(const char *key, nsIMdbRow *row, nsIMsgFolderCacheElement **result);

	nsresult RowCellColumnToCharPtr(nsIMdbRow *hdrRow, mdb_token columnToken, char **resultPtr);
	nsresult InitMDBInfo();
	nsresult InitNewDB();
	nsresult InitExistingDB();
	nsresult OpenMDB(const char *dbName, PRBool create);
	nsIMdbEnv				*GetEnv() {return m_mdbEnv;}
	nsIMdbStore				*GetStore() {return m_mdbStore;}

	nsFileSpec		m_dbFileSpec;
	nsSupportsHashtable	*m_cacheElements;
	// mdb stuff
	nsIMdbEnv		    *m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore	 	    *m_mdbStore;
	nsIMdbTable		    *m_mdbAllFoldersTable;
	mdb_token			m_folderRowScopeToken;
	mdb_token			m_folderTableKindToken;

	struct mdbOid		m_allFoldersTableOID;

};


#endif
