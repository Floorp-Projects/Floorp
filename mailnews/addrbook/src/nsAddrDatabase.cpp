/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// this file implements the nsAddrDatabase interface using the MDB Interface.

#include "nsAddrDatabase.h"
#include "nsIEnumerator.h"
#include "nsFileStream.h"
#include "nsString.h"
#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsAbCard.h"
#include "prmem.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"


// Candice - Removed to allow it to build on Mac
//#include <sys/types.h>
//#include <sys/stat.h>

#if defined(XP_MAC) && defined(CompareString)
	#undef CompareString
#endif
#include "nsICollation.h"

#include "nsCollationCID.h"
#include "nsIPref.h"

extern nsIMdbFactory *NS_NewIMdbFactory();


const int kAddressBookDBVersion = 1;

enum nsAddrDBCommitType {
  kSmallCommit,
  kLargeCommit,
  kSessionCommit,
  kCompressCommit
};

typedef enum 
{
	AB_NotifyInserted,
	AB_NotifyDeleted,
	AB_NotifyPropertyChanged,

	AB_NotifyAll,		   /* contents of the have totally changed. Listener must totally
							  forget anything they knew about the object. */
	/* pane notifications (i.e. not tied to a particular entry */
	AB_NotifyScramble,     /* same contents, but the view indices have all changed 
						      i.e the object was sorted on a different attribute */
	AB_NotifyLDAPTotalContentChanged,
	AB_NotifyNewTopIndex,
	AB_NotifyStartSearching,
	AB_NotifyStopSearching

} AB_NOTIFY_CODE;

const char *kPabTableKind = "ns:addrbk:db:table:kind:pab";
const char *kBuddyTableKind = "ns:addrbk:db:table:kind:buddy";
const char *kHistoryTableKind = "ns:addrbk:db:table:kind:history";
const char *kMailListTableKind = "ns:addrbk:db:table:kind:maillist";
const char *kCategoryTableKind = "ns:addrbk:db:table:kind:category";

const char *kCardRowScope = "ns:addrbk:db:row:scope:card:all";

const char *kFirstNameColumnToken = "firstname";
const char *kLastNameColumnToken = "lastname";
const char *kDisplayNameColumnToken = "displayname";
const char *kPriEmailColumnToken = "primaryemail";
const char *k2ndEmailColumnToken = "secondemail";
const char *kWorkPhoneColumnToken = "workphone";
const char *kHomePhoneColumnToken = "homephone";
const char *kFaxColumnToken = "faxnumber";
const char *kPagerColumnToken = "pagernumber";
const char *kCellularColumnToken = "cellularnumber";
const char *kWorkCityColumnToken = "workcity";
const char *kOrganizationColumnToken = "organization";
const char *kNicknameColumnToken = "nickname";
const char *kAddressCharSetColumnToken = "addrCharSet";

struct mdbOid gAddressBookTableOID;
struct mdbOid gMailListTableOID;
struct mdbOid gBuddyListTableOID;
struct mdbOid gCategoryTableOID;

nsAddrDatabase::nsAddrDatabase()
    : m_mdbEnv(nsnull), m_mdbStore(nsnull),
      m_mdbPabTable(nsnull), m_dbName(""),
      m_mdbTokensInitialized(PR_FALSE), m_ChangeListeners(nsnull),
      m_cardRowScopeToken(0),
      m_pabTableKind(0),
      m_buddyTableKind(0),
      m_historyTableKind(0),
      m_mailListTableKind(0),
      m_categoryTableKind(0),
      m_firstNameColumnToken(0),
      m_lastNameColumnToken(0),
      m_displayNameColumnToken(0),
      m_priEmailColumnToken(0),
      m_2ndEmailColumnToken(0),
      m_workPhoneColumnToken(0),
      m_homePhoneColumnToken(0),
      m_faxColumnToken(0),
      m_pagerColumnToken(0),
      m_cellularColumnToken(0),
      m_workCityColumnToken(0),
      m_organizationColumnToken(0),
      m_nicknameColumnToken(0),
      m_addressCharSetColumnToken(0)
{
	NS_INIT_REFCNT();
}

nsAddrDatabase::~nsAddrDatabase()
{
//	Close(FALSE);	// better have already been closed.
    if (m_ChangeListeners) 
	{
        // better not be any listeners, because we're going away.
        NS_ASSERTION(m_ChangeListeners->Count() == 0, "shouldn't have any listeners");
        delete m_ChangeListeners;
    }
}

NS_IMPL_ADDREF(nsAddrDatabase)
NS_IMPL_RELEASE(nsAddrDatabase)

NS_IMETHODIMP nsAddrDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIAddrDatabase::GetIID()) ||
        aIID.Equals(nsIAddrDBAnnouncer::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIAddrDatabase*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

NS_IMETHODIMP nsAddrDatabase::AddListener(nsIAddrDBListener *listener)
{
    if (m_ChangeListeners == nsnull) 
	{
        m_ChangeListeners = new nsVoidArray();
        if (!m_ChangeListeners) 
			return NS_ERROR_OUT_OF_MEMORY;
    }
	return m_ChangeListeners->AppendElement(listener);
}

NS_IMETHODIMP nsAddrDatabase::RemoveListener(nsIAddrDBListener *listener)
{
    if (m_ChangeListeners == nsnull) 
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		if ((nsIAddrDBListener *) m_ChangeListeners->ElementAt(i) == listener)
		{
			m_ChangeListeners->RemoveElementAt(i);
			return NS_OK;
		}
	}
	return NS_COMFALSE;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIAddrDBListener *changeListener =
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnCardAttribChange(abCode, instigator); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardEntryChange(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIAddrDBListener *changeListener = 
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnCardEntryChange(abCode, entryID, instigator); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::NotifyAnnouncerGoingAway(void)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	// run loop backwards because listeners remove themselves from the list 
	// on this notification
	for (PRInt32 i = m_ChangeListeners->Count() - 1; i >= 0 ; i--)
	{
		nsIAddrDBListener *changeListener =
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnAnnouncerGoingAway(this); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}



nsVoidArray *nsAddrDatabase::m_dbCache = NULL;

//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------

nsVoidArray/*<nsAddrDatabase>*/*
nsAddrDatabase::GetDBCache()
{
	if (!m_dbCache)
		m_dbCache = new nsVoidArray();

	return m_dbCache;
	
}

void
nsAddrDatabase::CleanupCache()
{
	if (m_dbCache) // clean up memory leak
	{
		for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
		{
			nsAddrDatabase* pAddrDB = NS_STATIC_CAST(nsAddrDatabase*, GetDBCache()->ElementAt(i));
			if (pAddrDB)
			{
				pAddrDB->ForceClosed();
				i--;	// back up array index, since closing removes db from cache.
			}
		}
//		NS_ASSERTION(GetNumInCache() == 0, "some msg dbs left open");	// better not be any open db's.
		delete m_dbCache;
	}
	m_dbCache = nsnull; // Need to reset to NULL since it's a
			  // static global ptr and maybe referenced 
			  // again in other places.
}

//----------------------------------------------------------------------
// FindInCache - this addrefs the db it finds.
//----------------------------------------------------------------------
nsAddrDatabase* nsAddrDatabase::FindInCache(nsFileSpec &dbName)
{
	for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
	{
		nsAddrDatabase* pAddrDB = NS_STATIC_CAST(nsAddrDatabase*, GetDBCache()->ElementAt(i));
		if (pAddrDB->MatchDbName(dbName))
		{
			NS_ADDREF(pAddrDB);
			return pAddrDB;
		}
	}
	return nsnull;
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
PRInt32 nsAddrDatabase::FindInCache(nsAddrDatabase* pAddrDB)
{
	for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
	{
		if (GetDBCache()->ElementAt(i) == pAddrDB)
		{
			return(i);
		}
	}
	return(-1);
}

PRBool nsAddrDatabase::MatchDbName(nsFileSpec &dbName)	// returns PR_TRUE if they match
{
	return (m_dbName == dbName); 
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void nsAddrDatabase::RemoveFromCache(nsAddrDatabase* pAddrDB)
{
	PRInt32 i = FindInCache(pAddrDB);
	if (i != -1)
	{
		GetDBCache()->RemoveElementAt(i);
	}
}

nsIMdbFactory *nsAddrDatabase::GetMDBFactory()
{
	static nsIMdbFactory *gMDBFactory = NULL;
	if (!gMDBFactory)
	{
// Candice - Removed to allow it to build on Mac
#if 0
		gMDBFactory = MakeMdbFactory(); //new nsIMdbFactory;
#endif
	}
	return gMDBFactory;
}

#ifdef XP_PC
// this code is stolen from nsFileSpecWin. Since MDB requires a native path, for 
// the time being, we'll just take the Unix/Canonical form and munge it
void nsAddrDatabase::UnixToNative(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
	// Allow for relative or absolute.  We can do this in place, because the
	// native path is never longer.
	
	if (!ioPath || !*ioPath)
		return;
		
	char* src = ioPath;
	if (*ioPath == '/')
    {
      // Strip initial slash for an absolute path
      src++;
    }
		
	// Convert the vertical slash to a colon
	char* cp = src + 1;
	
	// If it was an absolute path, check for the drive letter
	if (*ioPath == '/' && strstr(cp, "|/") == cp)
    *cp = ':';
	
	// Convert '/' to '\'.
	while (*++cp)
    {
      if (*cp == '/')
        *cp = '\\';
    }

	if (*ioPath == '/') {
    for (cp = ioPath; *cp; ++cp)
      *cp = *(cp + 1);
  }
}
#endif /* XP_PC */

#ifdef XP_MAC
// this code is stolen from nsFileSpecMac. Since MDB requires a native path, for 
// the time being, we'll just take the Unix/Canonical form and munge it
void nsAddrDatabase::UnixToNative(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = strlen(ioPath);
	char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
	if (result)
	{
		char* dst = result;
		const char* src = ioPath;
		if (*src == '/')		 	// * full path
			src++;
		else if (strchr(src, '/'))	// * partial path, and not just a leaf name
			*dst++ = ':';
		strcpy(dst, src);

		while ( *dst != 0)
		{
			if (*dst == '/')
				*dst++ = ':';
			else
				*dst++;
		}
		nsCRT::free(ioPath);
		ioPath = result;
	}
}

void nsAddrDatabase::NativeToUnix(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
	size_t len = strlen(ioPath);
	char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
	if (result)
	{
		char* dst = result;
		const char* src = ioPath;
		if (*src == ':')		 	// * partial path, and not just a leaf name
			src++;
		else if (strchr(src, ':'))	// * full path
			*dst++ = '/';
		strcpy(dst, src);

		while ( *dst != 0)
		{
			if (*dst == ':')
				*dst++ = '/';
			else
				*dst++;
		}
		PR_Free(ioPath);
		ioPath = result;
	}
}
#endif /* XP_MAC */

NS_IMETHODIMP nsAddrDatabase::Open
(nsFileSpec &pabName, PRBool create, nsIAddrDatabase** pAddrDB, PRBool upgrading)
{
	nsAddrDatabase	        *pAddressBookDB;
	nsresult                err = NS_OK;

	*pAddrDB = nsnull;

	pAddressBookDB = (nsAddrDatabase *) FindInCache(pabName);
	if (pAddressBookDB) {
		*pAddrDB = pAddressBookDB;
		//FindInCache does the AddRef'ing
		//pAddressBookDB->AddRef();
		return(NS_OK);
	}

	pAddressBookDB = new nsAddrDatabase();
	if (!pAddressBookDB) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

//	pAddressBookDB->m_dbName = new nsFileSpec(pabName);
	pAddressBookDB->AddRef();

	err = pAddressBookDB->OpenMDB((const char *) pabName, create);
	if (NS_SUCCEEDED(err)) 
	{
		*pAddrDB = pAddressBookDB;
		if (pAddressBookDB)  
			GetDBCache()->AppendElement(pAddressBookDB);
	}
	else 
	{
		*pAddrDB = nsnull;
		if (pAddressBookDB)  
			delete pAddressBookDB;
		pAddressBookDB = nsnull;
	}

	return err;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
NS_IMETHODIMP nsAddrDatabase::OpenMDB(const char *dbName, PRBool create)
{
	nsresult ret = NS_OK;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb;
// Candice - Removed to allow it to build on Mac
//			struct stat st;
			char	*nativeFileName = nsCRT::strdup(dbName);

			if (!nativeFileName)
				return NS_ERROR_OUT_OF_MEMORY;

			if (m_mdbEnv)
				m_mdbEnv->SetAutoClear(PR_TRUE);
			m_dbName = nsCRT::strdup(dbName);
#if defined(XP_PC) || defined(XP_MAC)
			UnixToNative(nativeFileName);
#endif
// Candice - Removed to allow it to build on Mac
/*			if (stat(nativeFileName, &st)) 
				ret = NS_ERROR_FAILURE;  // check: use the right error code later
			else*/
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
					PRInt32 bytesRead = dbStream->read(bufFirst512Bytes, sizeof(bufFirst512Bytes));
					first512Bytes.mYarn_Fill = bytesRead;
					dbStream->close();
					delete dbStream;
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
					ret = NS_ERROR_FAILURE;  //check: use the right error code
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
					{
						ret = InitExistingDB();
						create = PR_FALSE;
					}
				}
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
			nsCRT::free(nativeFileName);
		}
	}
	return ret;
}

NS_IMETHODIMP nsAddrDatabase::CloseMDB(PRBool commit)
{
	if (commit)
		Commit(kSessionCommit);
	return(NS_OK);
}

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
// This is evil in the com world, but there are times we need to delete the file.
NS_IMETHODIMP nsAddrDatabase::ForceClosed()
{
	nsresult	err = NS_OK;
    nsCOMPtr<nsIAddrDatabase> aDb(do_QueryInterface(this, &err));

	// make sure someone has a reference so object won't get deleted out from under us.
	AddRef();	
	NotifyAnnouncerGoingAway();
	// OK, remove from cache first and close the store.
	RemoveFromCache(this);

	err = CloseMDB(PR_FALSE);	// since we're about to delete it, no need to commit.
	if (m_mdbStore)
	{
		m_mdbStore->CloseMdbObject(m_mdbEnv);
		m_mdbStore = nsnull;
	}
	Release();
	return err;
}

NS_IMETHODIMP nsAddrDatabase::Commit(PRUint32 commitType)
{
	nsresult	err = NS_OK;
	nsIMdbThumb	*commitThumb = NULL;

	if (m_mdbStore)
	{
		switch (commitType)
		{
		case kSmallCommit:
			err = m_mdbStore->SmallCommit(GetEnv());
			break;
		case kLargeCommit:
			err = m_mdbStore->LargeCommit(GetEnv(), &commitThumb);
			break;
		case kSessionCommit:
			// comment out until persistence works.
			err = m_mdbStore->SessionCommit(GetEnv(), &commitThumb);
			break;
		case kCompressCommit:
			err = m_mdbStore->CompressCommit(GetEnv(), &commitThumb);
			break;
		}
	}
	if (commitThumb)
	{
		mdb_count outTotal = 0;    // total somethings to do in operation
		mdb_count outCurrent = 0;  // subportion of total completed so far
		mdb_bool outDone = PR_FALSE;      // is operation finished?
		mdb_bool outBroken = PR_FALSE;     // is operation irreparably dead and broken?
		while (!outDone && !outBroken && err == NS_OK)
		{
			err = commitThumb->DoMore(GetEnv(), &outTotal, &outCurrent, &outDone, &outBroken);
		}
		NS_RELEASE(commitThumb);
	}
	// ### do something with error, but clear it now because mork errors out on commits.
	if (GetEnv())
		GetEnv()->ClearErrors();
	return err;
}

NS_IMETHODIMP nsAddrDatabase::Close(PRBool forceCommit /* = TRUE */)
{
	return CloseMDB(forceCommit);
}

// set up empty tablesetc.
nsresult nsAddrDatabase::InitNewDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		nsIMdbStore *store = GetStore();
		// create the unique table for the dbFolderInfo.
		mdb_err mdberr;

        mdberr = (nsresult) store->NewTable(GetEnv(), m_cardRowScopeToken, 
			m_pabTableKind, PR_FALSE, &gAddressBookTableOID, &m_mdbPabTable);
	}
	return err;
}

nsresult nsAddrDatabase::InitExistingDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		err = GetStore()->GetTable(GetEnv(), &gAddressBookTableOID, &m_mdbPabTable);

		nsIMdbRow* cardRow;
		nsIMdbTableRowCursor* rowCursor;
		mdb_pos rowPos;
		mdb_id rowID;

		if (m_mdbPabTable)
			m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		do 
		{
			err = rowCursor->NextRow(GetEnv(), &cardRow, &rowPos);

			if (NS_SUCCEEDED(err) && cardRow)
			{
				mdbOid outOid;
				err = cardRow->GetOid(GetEnv(), &outOid);
				if (NS_SUCCEEDED(err))
					rowID = outOid.mOid_Id;

			}
		} while (cardRow);
	}
	return err;
}

#ifdef GetCard
nsresult nsAddrDatabase::GetCardRow()
{
	if (m_mdbPabTable)
	{
		nsCOMPtr <nsIMdbTableRowCursor> rowCursor;
		rowPos = -1;
		ret= m_mdbPabTable->GetTableRowCursor(GetEnv(), rowPos, getter_addrefs(rowCursor));
		if (ret == NS_OK)
		{
			ret = rowCursor->NextRow(GetEnv(), &m_mdbRow, rowPos);
			if (ret == NS_OK && m_mdbRow)
			{
				LoadMemberVariables();
			}
		}
	}
	return ret;
}
#endif

// initialize the various tokens and tables in our db's env
nsresult nsAddrDatabase::InitMDBInfo()
{
	nsresult err = NS_OK;

	if (!m_mdbTokensInitialized && GetStore())
	{
		m_mdbTokensInitialized = PR_TRUE;
		err	= GetStore()->StringToToken(GetEnv(), kCardRowScope, &m_cardRowScopeToken); 
		if (NS_SUCCEEDED(err))
		{
			GetStore()->StringToToken(GetEnv(),  kFirstNameColumnToken, &m_firstNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLastNameColumnToken, &m_lastNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kDisplayNameColumnToken, &m_displayNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPriEmailColumnToken, &m_priEmailColumnToken);
			GetStore()->StringToToken(GetEnv(),  k2ndEmailColumnToken, &m_2ndEmailColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkPhoneColumnToken, &m_workPhoneColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomePhoneColumnToken, &m_homePhoneColumnToken);
			GetStore()->StringToToken(GetEnv(),  kFaxColumnToken, &m_faxColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPagerColumnToken, &m_pagerColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCellularColumnToken, &m_cellularColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkCityColumnToken, &m_workCityColumnToken);
			GetStore()->StringToToken(GetEnv(),  kOrganizationColumnToken, &m_organizationColumnToken);
			GetStore()->StringToToken(GetEnv(),  kNicknameColumnToken, &m_nicknameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kAddressCharSetColumnToken, &m_addressCharSetColumnToken);
			err = GetStore()->StringToToken(GetEnv(), kPabTableKind, &m_pabTableKind); 
			if (NS_SUCCEEDED(err))
			{
				GetStore()->StringToToken(GetEnv(), kBuddyTableKind, &m_buddyTableKind); 
				GetStore()->StringToToken(GetEnv(), kMailListTableKind, &m_mailListTableKind); 
				GetStore()->StringToToken(GetEnv(), kCategoryTableKind, &m_categoryTableKind); 
				// The rows have 4 mOids.  Maillist , buddylist and category tables
				// have the same oids as maillist , buddylist and category rows
				gAddressBookTableOID.mOid_Scope = m_cardRowScopeToken;
				gAddressBookTableOID.mOid_Id = 1;
				gMailListTableOID.mOid_Scope = m_mailListTableKind;
				gMailListTableOID.mOid_Id = 1;
				gBuddyListTableOID.mOid_Scope = m_buddyTableKind;
				gBuddyListTableOID.mOid_Id = 1;
				gCategoryTableOID.mOid_Scope = m_categoryTableKind;
				gCategoryTableOID.mOid_Id = 1;
			}
		}
	}
	return err;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDB(nsIAbCard *newCard, PRBool notify /* = FALSE */)
{
	nsresult	err = NS_OK;
	nsIMdbRow	*cardRow;

	if (!newCard || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	err  = GetStore()->NewRow(GetEnv(), m_cardRowScopeToken, &cardRow);

	mdbOid rowOid;
	cardRow->GetOid(GetEnv(), &rowOid);

	// add the row to the singleton table.
	if (NS_SUCCEEDED(err) && cardRow)
	{
		char* pStr = nsnull;
		newCard->GetFirstName(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_firstNameColumnToken, pStr);

		newCard->GetLastName(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_lastNameColumnToken, pStr);

		newCard->GetDisplayName(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_displayNameColumnToken, pStr);

		newCard->GetPrimaryEmail(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_priEmailColumnToken, pStr);

		newCard->GetSecondEmail(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_2ndEmailColumnToken, pStr);

		newCard->GetWorkPhone(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_workPhoneColumnToken, pStr);

		newCard->GetHomePhone(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_homePhoneColumnToken, pStr);

		newCard->GetFaxNumber(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_faxColumnToken, pStr);

		newCard->GetPagerNumber(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_pagerColumnToken, pStr);

		newCard->GetCellularNumber(&pStr);
		if (pStr)
			AddCardColumn(cardRow, m_cellularColumnToken, pStr);

		err = m_mdbPabTable->AddRow(GetEnv(), cardRow);
	}
	if (NS_FAILED(err)) return err;

	//  do notification
	if (notify)
	{
		NotifyCardEntryChange(AB_NotifyInserted, 0, NULL);
	}
	return err;
}

mdb_err nsAddrDatabase::AddCardColumn(nsIMdbRow* cardRow, mdb_column inColumn, char* str)
{
	struct mdbYarn yarn;

	yarn.mYarn_Grow = NULL;
	yarn.mYarn_Buf = str;
	yarn.mYarn_Size = PL_strlen((const char *) yarn.mYarn_Buf) + 1;
	yarn.mYarn_Fill = yarn.mYarn_Size - 1;
	yarn.mYarn_Form = 0;	// what to do with this? we're storing csid in the msg hdr...
	mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);
	PR_FREEIF(str);

	return err;
}


class nsAddrDBEnumerator : public nsIEnumerator {
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_IMETHOD First(void);
    NS_IMETHOD Next(void);
    NS_IMETHOD CurrentItem(nsISupports **aItem);
    NS_IMETHOD IsDone(void);

    // nsAddrDBEnumerator methods:

    nsAddrDBEnumerator(nsAddrDatabase* db, void* closure);
    virtual ~nsAddrDBEnumerator();

protected:
    nsCOMPtr<nsAddrDatabase>    mDB;
    nsCOMPtr<nsIAbCard>         mResultCard;
	nsIMdbTableRowCursor*       mRowCursor;
	nsIMdbRow*					mCurrentRow;
 	mdb_pos						mRowPos;
    PRBool                      mDone;
    void*                       mClosure;
};

nsAddrDBEnumerator::nsAddrDBEnumerator(nsAddrDatabase* db, void* closure)
    : mDB(db), mRowCursor(nsnull), mDone(PR_FALSE), mClosure(closure)
{
    NS_INIT_REFCNT();
}

nsAddrDBEnumerator::~nsAddrDBEnumerator()
{
}

NS_IMPL_ISUPPORTS(nsAddrDBEnumerator, nsIEnumerator::GetIID())

NS_IMETHODIMP nsAddrDBEnumerator::First(void)
{
	nsresult rv = 0;
	mDone = PR_FALSE;

	if (!mDB || !mDB->GetPabTable())
		return NS_ERROR_NULL_POINTER;
		
	mDB->GetPabTable()->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
	if (NS_FAILED(rv)) return rv;
    return Next();
}

NS_IMETHODIMP nsAddrDBEnumerator::Next(void)
{
	nsresult rv = mRowCursor->NextRow(mDB->GetEnv(), &mCurrentRow, &mRowPos);
    if (mCurrentRow && NS_SUCCEEDED(rv))
		return NS_OK;
	else if (!mCurrentRow) 
	{
        mDone = PR_TRUE;
		return NS_ERROR_NULL_POINTER;
    }
    else if (NS_FAILED(rv)) {
        mDone = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::CurrentItem(nsISupports **aItem)
{
    if (mCurrentRow) 
	{
		nsCOMPtr<nsIAbCard> temp = mResultCard;
        nsresult rv = mDB->CreateABCard(mCurrentRow, getter_AddRefs(mResultCard));
        *aItem = mResultCard;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_COMFALSE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAddrDatabase::EnumerateCards(nsIEnumerator **result)
{
    nsAddrDBEnumerator* e = new nsAddrDBEnumerator(this, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsresult nsAddrDatabase::CreateABCard(nsIMdbRow* cardRow, nsIAbCard **result)
{
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	char* cardURI = nsnull;
    nsCOMPtr<nsIRDFResource> resource;

	mdbOid outOid;
	mdb_id rowID=-1;
	mdb_id tableID = 1; /* check: temperarily set to 1 for now */
	if (cardRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;
  
	if(NS_SUCCEEDED(rv))
	{
		cardURI = PR_smprintf("abcard://Pab%d/Card%d", tableID, rowID);
	}
	if(NS_SUCCEEDED(rv))
	{
		rv = rdfService->GetResource(cardURI, getter_AddRefs(resource));
    }
	if(cardURI)
		PR_smprintf_free(cardURI);

	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIAbCard> personCard(do_QueryInterface(resource, &rv));
		if(NS_SUCCEEDED(rv))
		{
			*result = personCard;
			NS_IF_ADDREF(*result);
		}
	}
	return rv;
}


