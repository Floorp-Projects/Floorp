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

#ifndef nsMsgFolderCache_H
#define nsMsgFolderCache_H

#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"

#include "nsCOMPtr.h"
#include "mdb.h"
#include "nsHashtable.h"
#include "nsFileSpec.h"

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
