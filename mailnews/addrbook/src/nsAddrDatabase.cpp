/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// this file implements the nsAddrDatabase interface using the MDB Interface.

#include "nsAddrDatabase.h"
#include "nsIEnumerator.h"
#include "nsFileStream.h"
#include "nsString.h"
#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsIAbDirectory.h"
#include "nsAbCard.h"
#include "nsIAddrBookSession.h"

#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"
#include "nsRDFCID.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsCollationCID.h"


#include "nsMorkCID.h"
#include "nsIPref.h"
#include "nsIMdbFactoryFactory.h"
#include "nsXPIDLString.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

static NS_DEFINE_CID(kCMorkFactory, NS_MORK_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

/* The definition is nsAddressBook.cpp */
extern const char *kCardDataSourceRoot;
extern const char *kDirectoryDataSourceRoot;

/* The definition is nsAddrDatabase.cpp */
extern const char *kMainPersonalAddressBook;

#define ID_PAB_TABLE			1
#define ID_ANONYMOUS_TABLE		2

const PRInt32 kAddressBookDBVersion = 1;

const char *kAnonymousTableKind = "ns:addrbk:db:table:kind:anonymous";
const char *kAnonymousRowScope = "ns:addrbk:db:row:scope:anonymous:all";

const char *kPabTableKind = "ns:addrbk:db:table:kind:pab";

const char *kCardRowScope = "ns:addrbk:db:row:scope:card:all";
const char *kListRowScope = "ns:addrbk:db:row:scope:list:all";
const char *kDataRowScope = "ns:addrbk:db:row:scope:data:all";

#define DATAROW_ROWID 1

const char *kFirstNameColumn = "FirstName";
const char *kLastNameColumn = "LastName";
const char *kDisplayNameColumn = "DisplayName";
const char *kNicknameColumn = "NickName";
const char *kPriEmailColumn = "PrimaryEmail";
const char *k2ndEmailColumn = "SecondEmail";
const char *kPlainTextColumn = "SendPlainText";
const char *kWorkPhoneColumn = "WorkPhone";
const char *kHomePhoneColumn = "HomePhone";
const char *kFaxColumn = "FaxNumber";
const char *kPagerColumn = "PagerNumber";
const char *kCellularColumn = "CellularNumber";
const char *kHomeAddressColumn = "HomeAddress";
const char *kHomeAddress2Column = "HomeAddress2";
const char *kHomeCityColumn = "HomeCity";
const char *kHomeStateColumn = "HomeState";
const char *kHomeZipCodeColumn = "HomeZipCode";
const char *kHomeCountryColumn = "HomeCountry";
const char *kWorkAddressColumn = "WorkAddress";
const char *kWorkAddress2Column = "WorkAddress2";
const char *kWorkCityColumn = "WorkCity";
const char *kWorkStateColumn = "WorkState";
const char *kWorkZipCodeColumn = "WorkZipCode";
const char *kWorkCountryColumn = "WorkCountry";
const char *kJobTitleColumn = "JobTitle";
const char *kDepartmentColumn = "Department";
const char *kCompanyColumn = "Company";
const char *kWebPage1Column = "WebPage1";
const char *kWebPage2Column = "WebPage2";
const char *kBirthYearColumn = "BirthYear";
const char *kBirthMonthColumn = "BirthMonth";
const char *kBirthDayColumn = "BirthDay";
const char *kCustom1Column = "Custom1";
const char *kCustom2Column = "Custom2";
const char *kCustom3Column = "Custom3";
const char *kCustom4Column = "Custom4";
const char *kNotesColumn = "Notes";
const char *kLastModifiedDateColumn = "LastModifiedDate";
const char *kRecordKeyColumn = "RecordKey";
const char *kLowerPriEmailColumn = "LowercasePrimaryEmail";

const char *kAddressCharSetColumn = "AddrCharSet";
const char *kLastRecordKeyColumn = "LastRecordKey";

const char *kMailListName = "ListName";
const char *kMailListNickName = "ListNickName";
const char *kMailListDescription = "ListDescription";
const char *kMailListTotalAddresses = "ListTotalAddresses"; // total number of email addresses in a mailing list
const char *kMailListTotalLists = "ListTotalLists";	// total number of mail list in a mailing list
const char *kLowerListNameColumn = "LowercaseListName";

struct mdbOid gAddressBookTableOID;
struct mdbOid gAnonymousTableOID;

static const char *kMailListAddressFormat = "Address%d";
static const char *kMailListListFormat = "List%d";

nsAddrDatabase::nsAddrDatabase()
    : m_mdbEnv(nsnull), m_mdbStore(nsnull),
      m_mdbPabTable(nsnull), m_mdbRow(nsnull),
	  m_dbName(""), m_mdbTokensInitialized(PR_FALSE), 
	  m_ChangeListeners(nsnull), m_mdbAnonymousTable(nsnull), 
	  m_AnonymousTableKind(0), m_pAnonymousStrAttributes(nsnull), 
	  m_pAnonymousStrValues(nsnull), m_pAnonymousIntAttributes(nsnull),
	  m_pAnonymousIntValues(nsnull), m_pAnonymousBoolAttributes(nsnull),
	  m_pAnonymousBoolValues(nsnull),
      m_PabTableKind(0),
      m_MailListTableKind(0),
      m_CardRowScopeToken(0),
      m_FirstNameColumnToken(0),
      m_LastNameColumnToken(0),
      m_DisplayNameColumnToken(0),
      m_NickNameColumnToken(0),
      m_PriEmailColumnToken(0),
      m_2ndEmailColumnToken(0),
      m_WorkPhoneColumnToken(0),
      m_HomePhoneColumnToken(0),
      m_FaxColumnToken(0),
      m_PagerColumnToken(0),
      m_CellularColumnToken(0),
      m_HomeAddressColumnToken(0),
      m_HomeAddress2ColumnToken(0),
      m_HomeCityColumnToken(0),
      m_HomeStateColumnToken(0),
      m_HomeZipCodeColumnToken(0),
      m_HomeCountryColumnToken(0),
      m_WorkAddressColumnToken(0),
      m_WorkAddress2ColumnToken(0),
      m_WorkCityColumnToken(0),
      m_WorkStateColumnToken(0),
      m_WorkZipCodeColumnToken(0),
      m_WorkCountryColumnToken(0),
      m_WebPage1ColumnToken(0),
      m_WebPage2ColumnToken(0),
      m_BirthYearColumnToken(0),
      m_BirthMonthColumnToken(0),
      m_BirthDayColumnToken(0),
      m_Custom1ColumnToken(0),
      m_Custom2ColumnToken(0),
      m_Custom3ColumnToken(0),
      m_Custom4ColumnToken(0),
      m_NotesColumnToken(0),
	  m_LastModDateColumnToken(0),
      m_PlainTextColumnToken(0),
      m_AddressCharSetColumnToken(0),
	  m_LastRecordKey(0),
	  m_dbDirectory(nsnull)
{
	NS_INIT_REFCNT();
}

nsAddrDatabase::~nsAddrDatabase()
{
	Close(PR_FALSE);	// better have already been closed.
    if (m_ChangeListeners)
	{
        // better not be any listeners, because we're going away.
        NS_ASSERTION(m_ChangeListeners->Count() == 0, "shouldn't have any listeners");
        delete m_ChangeListeners;
    }

	RemoveFromCache(this);

	if (m_pAnonymousStrAttributes)
		RemoveAnonymousList(m_pAnonymousStrAttributes);
	if (m_pAnonymousIntAttributes)
		RemoveAnonymousList(m_pAnonymousIntAttributes);
	if (m_pAnonymousBoolAttributes)
		RemoveAnonymousList(m_pAnonymousBoolAttributes);

	if (m_pAnonymousStrValues)
		RemoveAnonymousList(m_pAnonymousStrValues);
	if (m_pAnonymousIntValues)
		RemoveAnonymousList(m_pAnonymousIntValues);
	if (m_pAnonymousBoolValues)
		RemoveAnonymousList(m_pAnonymousBoolValues);
}

nsresult nsAddrDatabase::RemoveAnonymousList(nsVoidArray* pArray)
{
	if (pArray)
	{
		PRUint32 count = pArray->Count();
		for (int i = count - 1; i >= 0; i--)
		{
			void* pPtr = pArray->ElementAt(i);
			PR_FREEIF(pPtr);
			pArray->RemoveElementAt(i);
		}
		delete pArray;
	}
	return NS_OK;
}

NS_IMPL_THREADSAFE_ADDREF(nsAddrDatabase)

NS_IMETHODIMP_(nsrefcnt) nsAddrDatabase::Release(void)                    
{                                                      
	NS_PRECONDITION(0 != mRefCnt, "dup release");    	
	nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
	NS_LOG_RELEASE(this, count,"nsAddrDatabase"); 
	if (count == 0)	// OK, the cache is no longer holding onto this, so we really want to delete it, 
	{				// after removing it from the cache.
		mRefCnt = 1; /* stabilize */
		RemoveFromCache(this);
		if (m_mdbStore)
			m_mdbStore->CloseMdbObject(m_mdbEnv);
		NS_DELETEXPCOM(this);                              
		return 0;                                          
	}
	return count;                                      
}

NS_IMETHODIMP nsAddrDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIAddrDatabase)) ||
        aIID.Equals(NS_GET_IID(nsIAddrDBAnnouncer)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
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
	PRInt32 count = m_ChangeListeners->Count();
	PRInt32 i;
	for (i = 0; i < count; i++)
	{
		nsIAddrDBListener *dbListener = (nsIAddrDBListener *)m_ChangeListeners->ElementAt(i);
		if (dbListener == listener)
			return NS_OK;
	}
	return m_ChangeListeners->AppendElement(listener);
}

NS_IMETHODIMP nsAddrDatabase::RemoveListener(nsIAddrDBListener *listener)
{
    if (m_ChangeListeners == nsnull) 
		return NS_OK;

	PRInt32 count = m_ChangeListeners->Count();
	PRInt32 i;
	for (i = 0; i < count; i++)
	{
		nsIAddrDBListener *dbListener = (nsIAddrDBListener *)m_ChangeListeners->ElementAt(i);
		if (dbListener == listener)
		{
			m_ChangeListeners->RemoveElementAt(i);
			return NS_OK;
		}
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	PRInt32 i;
	for (i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIAddrDBListener *changeListener =
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnCardAttribChange(abCode, instigator); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	PRInt32 i;
	PRInt32 count = m_ChangeListeners->Count();
	for (i = 0; i < count; i++)
	{
		nsIAddrDBListener *changeListener = 
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnCardEntryChange(abCode, card, instigator); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}

nsresult nsAddrDatabase::NotifyListEntryChange(PRUint32 abCode, nsIAbDirectory *dir, nsIAddrDBListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	PRInt32 i;
	PRInt32 count = m_ChangeListeners->Count();
	for (i = 0; i < count; i++)
	{
		nsIAddrDBListener *changeListener = 
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnListEntryChange(abCode, dir, instigator); 
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
	PRInt32 i;
	for (i = m_ChangeListeners->Count() - 1; i >= 0 ; i--)
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

nsVoidArray/*<nsAddrDatabase>*/ *
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
		PRInt32 i;
		for (i = 0; i < GetDBCache()->Count(); i++)
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
nsAddrDatabase* nsAddrDatabase::FindInCache(nsFileSpec *dbName)
{
	PRInt32 i;
	for (i = 0; i < GetDBCache()->Count(); i++)
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
	PRInt32 i;
	for (i = 0; i < GetDBCache()->Count(); i++)
	{
		if (GetDBCache()->ElementAt(i) == pAddrDB)
		{
			return(i);
		}
	}
	return(-1);
}

PRBool nsAddrDatabase::MatchDbName(nsFileSpec* dbName)	// returns PR_TRUE if they match
{
	return (m_dbName == (*dbName)); 
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
	static nsIMdbFactory *gMDBFactory = nsnull;
	if (!gMDBFactory)
	{
		nsCOMPtr <nsIMdbFactoryFactory> factoryfactory;
		nsresult rv = nsComponentManager::CreateInstance(kCMorkFactory,
												  nsnull,
												  NS_GET_IID(nsIMdbFactoryFactory),
												  (void **) getter_AddRefs(factoryfactory));
		if (NS_SUCCEEDED(rv) && factoryfactory)
		  rv = factoryfactory->GetMdbFactory(&gMDBFactory);
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
		else if (PL_strchr(src, '/'))	// * partial path, and not just a leaf name
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
#endif /* XP_MAC */

/* caller need to delete *aDbPath */
NS_IMETHODIMP nsAddrDatabase::GetDbPath(nsFileSpec * *aDbPath)
{
	if (aDbPath)
	{
		nsFileSpec* pFilePath = new nsFileSpec();
		*pFilePath = m_dbName;
		*aDbPath = pFilePath;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAddrDatabase::SetDbPath(nsFileSpec * aDbPath)
{
	m_dbName = (*aDbPath);
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::Open
(nsFileSpec* pabName, PRBool create, nsIAddrDatabase** pAddrDB, PRBool upgrading)
{
	nsAddrDatabase	        *pAddressBookDB;
	nsresult                err = NS_OK;

	*pAddrDB = nsnull;

	pAddressBookDB = (nsAddrDatabase *) FindInCache(pabName);
	if (pAddressBookDB) {
		*pAddrDB = pAddressBookDB;
		return(NS_OK);
	}

	pAddressBookDB = new nsAddrDatabase();
	if (!pAddressBookDB) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	NS_ADDREF(pAddressBookDB);

	err = pAddressBookDB->OpenMDB(pabName, create);
	if (NS_SUCCEEDED(err)) 
	{
		pAddressBookDB->SetDbPath(pabName);
		GetDBCache()->AppendElement(pAddressBookDB);
		*pAddrDB = pAddressBookDB;
		NS_IF_ADDREF(*pAddrDB);
	}
	else 
	{
		*pAddrDB = nsnull;
		NS_IF_RELEASE(pAddressBookDB);
		pAddressBookDB = nsnull;
	}

	return err;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
NS_IMETHODIMP nsAddrDatabase::OpenMDB(nsFileSpec *dbName, PRBool create)
{
	nsresult ret = NS_OK;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb = nsnull;
			const char *pFilename = dbName->GetCString(); /* do not free */
			char	*nativeFileName = PL_strdup(pFilename);
			nsIMdbHeap* dbHeap = 0;
			mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable

			if (!nativeFileName)
				return NS_ERROR_OUT_OF_MEMORY;

			if (m_mdbEnv)
				m_mdbEnv->SetAutoClear(PR_TRUE);

#if defined(XP_PC) || defined(XP_MAC)
			UnixToNative(nativeFileName);
#endif
			if (!dbName->Exists()) 
				ret = NS_ERROR_FAILURE;  // check: use the right error code later
			else
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
						ret = myMDBFactory->CanOpenFilePort(m_mdbEnv, oldFile, // the file to investigate
							&canOpen, &outFormatVersion);
						if (ret == 0 && canOpen)
						{
							inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
							inOpenPolicy.mOpenPolicy_MinMemory = 0;
							inOpenPolicy.mOpenPolicy_MaxLazy = 0;

							ret = myMDBFactory->OpenFileStore(m_mdbEnv, dbHeap,
								oldFile, &inOpenPolicy, &thumb); 
						}
						else
							ret = NS_ERROR_FAILURE;  //check: use the right error code
					}
					oldFile->CutStrongRef(m_mdbEnv); // always release our file ref, store has own
				}
			}

			PR_FREEIF(nativeFileName);

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
					{ 
						outDone = PR_TRUE;
						break;
					}
				}
				while (NS_SUCCEEDED(ret) && !outBroken && !outDone);
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
				nsIMdbFile* newFile = 0;
				ret = myMDBFactory->CreateNewFile(m_mdbEnv, dbHeap, dbName->GetCString(), &newFile);
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
		}
	}
	//Convert the DB error to a valid nsresult error.
	if (ret == 1)
	  ret = NS_ERROR_FAILURE;
	return ret;
}

NS_IMETHODIMP nsAddrDatabase::CloseMDB(PRBool commit)
{
	if (commit)
		Commit(kSessionCommit);
//???    RemoveFromCache(this);  // if we've closed it, better not leave it in the cache.
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::OpenAnonymousDB(nsIAddrDatabase **pCardDB)
{
	nsresult rv = NS_OK;
    nsCOMPtr<nsIAddrDatabase> database;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if (NS_SUCCEEDED(rv))
	{
		nsFileSpec* dbPath;
		abSession->GetUserProfileDirectory(&dbPath);

		(*dbPath) += kMainPersonalAddressBook;

		Open(dbPath, PR_TRUE, getter_AddRefs(database), PR_TRUE);

        *pCardDB = database;
		NS_IF_ADDREF(*pCardDB);
	}
	return rv;
}

NS_IMETHODIMP nsAddrDatabase::CloseAnonymousDB(PRBool forceCommit)
{
	return CloseMDB(forceCommit);
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
	if (NS_SUCCEEDED(err))
	{
		err = InitPabTable();
		err = InitLastRecorKey();
		Commit(kLargeCommit);
	}
	return err;
}

nsresult nsAddrDatabase::InitPabTable()
{
	nsIMdbStore *store = GetStore();

    mdb_err mdberr = (nsresult) store->NewTableWithOid(GetEnv(), &gAddressBookTableOID, 
		m_PabTableKind, PR_FALSE, (const mdbOid*)nsnull, &m_mdbPabTable);

	return mdberr;
}

//save the last record number, store in m_DataRowScopeToken, row 1
nsresult nsAddrDatabase::InitLastRecorKey()
{
	if (!m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	nsIMdbRow *pDataRow = nsnull;
	mdbOid dataRowOid;
	dataRowOid.mOid_Scope = m_DataRowScopeToken;
	dataRowOid.mOid_Id = DATAROW_ROWID;
	err  = GetStore()->NewRowWithOid(GetEnv(), &dataRowOid, &pDataRow);

	if (NS_SUCCEEDED(err) && pDataRow)
	{
		m_LastRecordKey = 0;
		err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, 0);
		err = m_mdbPabTable->AddRow(GetEnv(), pDataRow);
		pDataRow->CutStrongRef(GetEnv());
	}
	return err;
}

nsresult nsAddrDatabase::GetDataRow(nsIMdbRow **pDataRow)
{
	nsIMdbRow *pRow = nsnull;
	mdbOid dataRowOid;
	dataRowOid.mOid_Scope = m_DataRowScopeToken;
	dataRowOid.mOid_Id = DATAROW_ROWID;
	GetStore()->GetRow(GetEnv(), &dataRowOid, &pRow);
	*pDataRow = pRow;
	if (pRow)
		return NS_OK;
	else
		return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetLastRecorKey()
{
	if (!m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	nsIMdbRow *pDataRow = nsnull;
	err = GetDataRow(&pDataRow);

	if (NS_SUCCEEDED(err) && pDataRow)
	{
		m_LastRecordKey = 0;
		err = GetIntColumn(pDataRow, m_LastRecordKeyColumnToken, &m_LastRecordKey, 0);
		if (NS_FAILED(err))
			err = NS_ERROR_NOT_AVAILABLE;
		pDataRow->CutStrongRef(GetEnv());
		return NS_OK;
	}
	else
		return NS_ERROR_NOT_AVAILABLE;
	return err;
}

nsresult nsAddrDatabase::UpdateLastRecordKey()
{
	if (!m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	nsIMdbRow *pDataRow = nsnull;
	err = GetDataRow(&pDataRow);

	if (NS_SUCCEEDED(err) && pDataRow)
	{
		err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, m_LastRecordKey);
		err = m_mdbPabTable->AddRow(GetEnv(), pDataRow);
		pDataRow->CutStrongRef(GetEnv());
		return NS_OK;
	}
	else if (!pDataRow)
		err = InitLastRecorKey();
	else
		return NS_ERROR_NOT_AVAILABLE;
	return err;
}


nsresult nsAddrDatabase::InitAnonymousTable()
{
	nsIMdbStore *store = GetStore();

	nsresult err = store->StringToToken(GetEnv(), kAnonymousTableKind, &m_AnonymousTableKind); 
    err = (nsresult) store->NewTableWithOid(GetEnv(), &gAnonymousTableOID, 
		m_AnonymousTableKind, PR_FALSE, (const mdbOid*)nsnull, &m_mdbAnonymousTable);
	return err;
}

nsresult nsAddrDatabase::InitExistingDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		err = GetStore()->GetTable(GetEnv(), &gAddressBookTableOID, &m_mdbPabTable);
		err = GetStore()->GetTable(GetEnv(), &gAnonymousTableOID, &m_mdbAnonymousTable);

		err = GetLastRecorKey();
		if (err == NS_ERROR_NOT_AVAILABLE)
			CheckAndUpdateRecordKey();
		UpdateLowercaseEmailListName();
	}
	return err;
}

nsresult nsAddrDatabase::CheckAndUpdateRecordKey()
{
	nsresult err = NS_OK;
	nsIMdbTableRowCursor* rowCursor = nsnull;
	nsIMdbRow* findRow = nsnull;
 	mdb_pos	rowPos = 0;

	err = m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

	if (NS_FAILED(err) || !rowCursor)
		return NS_ERROR_FAILURE;

	mdb_count total = 0;
	err = rowCursor->GetCount(GetEnv(), &total);

	nsIMdbRow *pDataRow = nsnull;
	err = GetDataRow(&pDataRow);
	if (NS_FAILED(err))
		InitLastRecorKey();

	if (total == 0)
		return NS_OK;

	do
	{  //add key to each card and mailing list row
		err = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
		if (NS_SUCCEEDED(err) && findRow)
		{
			mdbOid rowOid;

			if (findRow->GetOid(GetEnv(), &rowOid) == NS_OK)
			{
				if (!IsDataRowScopeToken(rowOid.mOid_Scope))
				{
					m_LastRecordKey++;
					err = AddIntColumn(findRow, m_RecordKeyColumnToken, m_LastRecordKey);
				}
			}
		}
	} while (findRow);

	UpdateLastRecordKey();
	Commit(kLargeCommit);
	return NS_OK;
}

nsresult nsAddrDatabase::UpdateLowercaseEmailListName()
{
	nsresult err = NS_OK;
	nsIMdbTableRowCursor* rowCursor = nsnull;
	nsIMdbRow* findRow = nsnull;
 	mdb_pos	rowPos = 0;

	err = m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

	if (NS_FAILED(err) || !rowCursor)
		return NS_ERROR_FAILURE;

	mdb_count total = 0;
	err = rowCursor->GetCount(GetEnv(), &total);

	if (total == 0)
		return NS_OK;

	do
	{   //add lowercase primary emial to each card and mailing list row
		err = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
		if (NS_SUCCEEDED(err) && findRow)
		{
			mdbOid rowOid;

			if (findRow->GetOid(GetEnv(), &rowOid) == NS_OK)
			{
				nsAutoString tempString;
				if (IsCardRowScopeToken(rowOid.mOid_Scope))
				{
					err = GetStringColumn(findRow, m_LowerPriEmailColumnToken, tempString);
					if (NS_SUCCEEDED(err))
						return NS_OK;

					err = ConvertAndAddLowercaseColumn(findRow, m_PriEmailColumnToken, 
												m_LowerPriEmailColumnToken);
				}
				else if (IsListRowScopeToken(rowOid.mOid_Scope))
				{
					err = GetStringColumn(findRow, m_LowerListNameColumnToken, tempString);
					if (NS_SUCCEEDED(err))
						return NS_OK;

					err = ConvertAndAddLowercaseColumn(findRow, m_ListNameColumnToken, 
												m_LowerListNameColumnToken);
				}
			}
		}
	} while (findRow);

	Commit(kLargeCommit);
	return NS_OK;
}

/*  
We store UTF8 strings in the database.  We need to convert the UTF8 
string into unicode string, then convert to lower case.  Before storing 
back into the database,  we need to convert the lowercase unicode string 
into UTF8 string.
*/
nsresult nsAddrDatabase::ConvertAndAddLowercaseColumn
(nsIMdbRow * row, mdb_token fromCol, mdb_token toCol)
{
	nsresult err = NS_OK;
	nsAutoString colUtf8String;

	err = GetStringColumn(row, fromCol, colUtf8String);
	if (colUtf8String.Length())
	{
		char* pUTF8String = colUtf8String.ToNewCString();
		err = AddLowercaseColumn(row, toCol, pUTF8String);
		nsCRT::free(pUTF8String);
	}
	return err;
}

/*  
Chnage the unicode string to lowercase, then convert to UTF8 string to store in db
*/
nsresult nsAddrDatabase::AddUnicodeToColumn(nsIMdbRow * row, mdb_token colToken, PRUnichar* pUnicodeStr)
{
	nsresult err = NS_OK;
	nsAutoString displayString(pUnicodeStr);
    char* pDisplayUTF8Str = displayString.ToNewUTF8String();
	nsAutoString newUnicodeString(pUnicodeStr);
	newUnicodeString.ToLowerCase();
	char* pUTF8Str = newUnicodeString.ToNewUTF8String();
	if (pUTF8Str && pDisplayUTF8Str)
	{
		if (colToken == m_PriEmailColumnToken)
		{
			err = AddCharStringColumn(row, m_PriEmailColumnToken, pDisplayUTF8Str);
			err = AddLowercaseColumn(row, m_LowerPriEmailColumnToken, pUTF8Str);
		}
		else if (colToken == m_ListNameColumnToken)
		{
			err = AddCharStringColumn(row, m_ListNameColumnToken, pDisplayUTF8Str);
			err = AddLowercaseColumn(row, m_LowerListNameColumnToken, pUTF8Str);
		}
	}
    else
        err = NS_ERROR_OUT_OF_MEMORY;

	if (pDisplayUTF8Str)
		Recycle(pDisplayUTF8Str);
	if (pUTF8Str)
		Recycle(pUTF8Str);
	return err;
}

// initialize the various tokens and tables in our db's env
nsresult nsAddrDatabase::InitMDBInfo()
{
	nsresult err = NS_OK;

	if (!m_mdbTokensInitialized && GetStore())
	{
		m_mdbTokensInitialized = PR_TRUE;
		err	= GetStore()->StringToToken(GetEnv(), kCardRowScope, &m_CardRowScopeToken); 
		err	= GetStore()->StringToToken(GetEnv(), kListRowScope, &m_ListRowScopeToken); 
		err	= GetStore()->StringToToken(GetEnv(), kDataRowScope, &m_DataRowScopeToken); 
		gAddressBookTableOID.mOid_Scope = m_CardRowScopeToken;
		gAddressBookTableOID.mOid_Id = ID_PAB_TABLE;
		gAnonymousTableOID.mOid_Scope = m_CardRowScopeToken;
		gAnonymousTableOID.mOid_Id = ID_ANONYMOUS_TABLE;
		if (NS_SUCCEEDED(err))
		{ 
			GetStore()->StringToToken(GetEnv(),  kFirstNameColumn, &m_FirstNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLastNameColumn, &m_LastNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kDisplayNameColumn, &m_DisplayNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kNicknameColumn, &m_NickNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPriEmailColumn, &m_PriEmailColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLowerPriEmailColumn, &m_LowerPriEmailColumnToken);
			GetStore()->StringToToken(GetEnv(),  k2ndEmailColumn, &m_2ndEmailColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPlainTextColumn, &m_PlainTextColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkPhoneColumn, &m_WorkPhoneColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomePhoneColumn, &m_HomePhoneColumnToken);
			GetStore()->StringToToken(GetEnv(),  kFaxColumn, &m_FaxColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPagerColumn, &m_PagerColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCellularColumn, &m_CellularColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeAddressColumn, &m_HomeAddressColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeAddress2Column, &m_HomeAddress2ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeCityColumn, &m_HomeCityColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeStateColumn, &m_HomeStateColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeZipCodeColumn, &m_HomeZipCodeColumnToken);
			GetStore()->StringToToken(GetEnv(),  kHomeCountryColumn, &m_HomeCountryColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkAddressColumn, &m_WorkAddressColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkAddress2Column, &m_WorkAddress2ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkCityColumn, &m_WorkCityColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkStateColumn, &m_WorkStateColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkZipCodeColumn, &m_WorkZipCodeColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWorkCountryColumn, &m_WorkCountryColumnToken);
			GetStore()->StringToToken(GetEnv(),  kJobTitleColumn, &m_JobTitleColumnToken);
			GetStore()->StringToToken(GetEnv(),  kDepartmentColumn, &m_DepartmentColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCompanyColumn, &m_CompanyColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWebPage1Column, &m_WebPage1ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kWebPage2Column, &m_WebPage2ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kBirthYearColumn, &m_BirthYearColumnToken);
			GetStore()->StringToToken(GetEnv(),  kBirthMonthColumn, &m_BirthMonthColumnToken);
			GetStore()->StringToToken(GetEnv(),  kBirthDayColumn, &m_BirthDayColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCustom1Column, &m_Custom1ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCustom2Column, &m_Custom2ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCustom3Column, &m_Custom3ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCustom4Column, &m_Custom4ColumnToken);
			GetStore()->StringToToken(GetEnv(),  kNotesColumn, &m_NotesColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLastModifiedDateColumn, &m_LastModDateColumnToken);
			GetStore()->StringToToken(GetEnv(),  kRecordKeyColumn, &m_RecordKeyColumnToken);

			GetStore()->StringToToken(GetEnv(),  kAddressCharSetColumn, &m_AddressCharSetColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLastRecordKeyColumn, &m_LastRecordKeyColumnToken);

			err = GetStore()->StringToToken(GetEnv(), kPabTableKind, &m_PabTableKind); 
			err = GetStore()->StringToToken(GetEnv(), kAnonymousTableKind, &m_AnonymousTableKind); 

			GetStore()->StringToToken(GetEnv(),  kMailListName, &m_ListNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMailListNickName, &m_ListNickNameColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMailListDescription, &m_ListDescriptionColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMailListTotalAddresses, &m_ListTotalColumnToken);
			GetStore()->StringToToken(GetEnv(),  kLowerListNameColumn, &m_LowerListNameColumnToken);
		}
	}
	return err;
}

////////////////////////////////////////////////////////////////////////////////

nsresult nsAddrDatabase::AddRecordKeyColumnToRow(nsIMdbRow *pRow)
{
	if (pRow)
	{
		m_LastRecordKey++;
		nsresult err = AddIntColumn(pRow, m_RecordKeyColumnToken, m_LastRecordKey);
		err = m_mdbPabTable->AddRow(GetEnv(), pRow);
		UpdateLastRecordKey();
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsAddrDatabase::AddAttributeColumnsToRow(nsIAbCard *card, nsIMdbRow *cardRow)
{
	nsresult	err = NS_OK;

	if (!card && !cardRow )
		return NS_ERROR_NULL_POINTER;

	mdbOid rowOid, tableOid;
	m_mdbPabTable->GetOid(GetEnv(), &tableOid);
	cardRow->GetOid(GetEnv(), &rowOid);

	card->SetDbTableID(tableOid.mOid_Id);
	card->SetDbRowID(rowOid.mOid_Id);

	// add the row to the singleton table.
	if (NS_SUCCEEDED(err) && cardRow)
	{
		PRUnichar* pUnicodeStr = nsnull;
		PRInt32 unicharLength = 0;
		char* pUTF8Str = nsnull;
		card->GetFirstName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddFirstName(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetLastName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddLastName(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetDisplayName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddDisplayName(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetNickName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddNickName(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);

		card->GetPrimaryEmail(&pUnicodeStr);
		if (pUnicodeStr)
			AddUnicodeToColumn(cardRow, m_PriEmailColumnToken, pUnicodeStr);
		PR_FREEIF(pUnicodeStr);

		card->GetSecondEmail(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				Add2ndEmail(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);

		PRBool bValue = PR_FALSE;
		card->GetSendPlainText(&bValue);
		AddSendPlainText(cardRow, bValue);

		card->GetWorkPhone(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkPhone(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomePhone(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomePhone(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetFaxNumber(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddFaxNumber(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetPagerNumber(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddPagerNumber(cardRow,pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCellularNumber(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCellularNumber(cardRow,pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeAddress(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeAddress(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeAddress2(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeAddress2(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeCity(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeCity(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeState(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeState(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeZipCode(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeZipCode(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetHomeCountry(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddHomeCountry(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkAddress(&pUnicodeStr);  
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkAddress(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkAddress2(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkAddress2(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkCity(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkCity(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkState(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkState(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkZipCode(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkZipCode(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWorkCountry(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWorkCountry(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetJobTitle(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddJobTitle(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetDepartment(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddDepartment(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCompany(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCompany(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWebPage1(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWebPage1(cardRow,pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetWebPage2(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddWebPage2(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetBirthYear(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddBirthYear(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetBirthMonth(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddBirthMonth(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetBirthDay(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddBirthDay(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCustom1(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCustom1(cardRow,pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCustom2(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCustom2(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCustom3(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCustom3(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetCustom4(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddCustom4(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		card->GetNotes(&pUnicodeStr); 
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddNotes(cardRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDB(nsIAbCard *newCard, PRBool notify /* = FALSE */)
{
	nsresult	err = NS_OK;
	nsIMdbRow	*cardRow;

	if (!newCard || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	err  = GetNewRow(&cardRow);

	if (NS_SUCCEEDED(err) && cardRow)
	{
		AddAttributeColumnsToRow(newCard, cardRow);
		AddRecordKeyColumnToRow(cardRow);
		err = m_mdbPabTable->AddRow(GetEnv(), cardRow);
		cardRow->CutStrongRef(GetEnv());
	}
	if (NS_FAILED(err)) return err;

	//  do notification
	if (notify)
	{
		NotifyCardEntryChange(AB_NotifyInserted, newCard, NULL);
	}
	return err;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDBWithKey(nsIAbCard *newCard, PRBool notify /* = FALSE */, PRUint32 *key)
{
  nsresult	err = NS_OK;
  *key = 0;

  err = CreateNewCardAndAddToDB(newCard, notify);
  if (NS_SUCCEEDED(err))
    *key = m_LastRecordKey;

  return err;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewListCardAndAddToDB(PRUint32 listRowID, nsIAbCard *newCard, PRBool notify /* = FALSE */)
{
	if (!newCard || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsIMdbRow* pListRow = nsnull;
	mdbOid listRowOid;
	listRowOid.mOid_Scope = m_ListRowScopeToken;
	listRowOid.mOid_Id = listRowID;
	nsresult err = GetStore()->GetRow(GetEnv(), &listRowOid, &pListRow);

	if (pListRow)
	{
		PRUint32 totalAddress = GetListAddressTotal(pListRow) + 1;
		SetListAddressTotal(pListRow, totalAddress);
		nsCOMPtr<nsIAbCard> pNewCard;
		err = AddListCardColumnsToRow(newCard, pListRow, totalAddress, getter_AddRefs(pNewCard));
		//  do notification
		if (notify)
		{
			NotifyCardEntryChange(AB_NotifyInserted, newCard, NULL);
		}
	}
	return err;

}

nsresult nsAddrDatabase::AddListCardColumnsToRow
(nsIAbCard *pCard, nsIMdbRow *pListRow, PRUint32 pos, nsIAbCard** pNewCard)
{
	if (!pCard && !pListRow )
		return NS_ERROR_NULL_POINTER;

	nsresult	err = NS_OK;
	PRUnichar *email = nsnull;
	pCard->GetPrimaryEmail(&email);
	PRInt32 emailLength = nsCRT::strlen(email);
	if (email)
	{
		char* pUTF8Email = nsnull;
		INTL_ConvertFromUnicode(email, emailLength, (char**)&pUTF8Email);
		if (pUTF8Email)
		{
			nsIMdbRow	*pCardRow = nsnull;
			err = GetRowForEmailAddress(pUTF8Email, &pCardRow);
			if (NS_FAILED(err) || !pCardRow)
			{
				//New Email, then add a new row with this email

				err  = GetNewRow(&pCardRow);

				if (NS_SUCCEEDED(err) && pCardRow)
				{
					AddPrimaryEmail(pCardRow, pUTF8Email);
					err = m_mdbPabTable->AddRow(GetEnv(), pCardRow);
				}

				//notify RDF a new card row
				nsCOMPtr<nsIAbCard>	newCard;
				CreateABCard(pCardRow, getter_AddRefs(newCard));
				*pNewCard = newCard;
				NS_IF_ADDREF(*pNewCard);

				NotifyCardEntryChange(AB_NotifyInserted, newCard, NULL);
			}
			else if (pCardRow)
			{
				//existing card, get the card ptr
				mdbOid cardOid;
				char* cardURI = nsnull;
				char* file = nsnull;

				if (NS_SUCCEEDED(pCardRow->GetOid(GetEnv(), &cardOid)))
				{
					file = m_dbName.GetLeafName();
					if (file)
					{
						cardURI = PR_smprintf("%s%s/Card%ld", kCardDataSourceRoot, file, cardOid.mOid_Id);
						NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &err);
						if (NS_SUCCEEDED(err) && cardURI)
						{
							nsCOMPtr<nsIRDFResource> cardResource;
							err = rdfService->GetResource(cardURI, getter_AddRefs(cardResource));
							if (NS_SUCCEEDED(err))
							{
								nsCOMPtr<nsIAbCard> personCard = do_QueryInterface(cardResource);
								if (personCard)
								{
									*pNewCard = personCard;
									NS_IF_ADDREF(*pNewCard);
								}
							}
							PR_smprintf_free(cardURI);
						}
						nsCRT::free(file);
					}
				}
			}
			PR_FREEIF(pUTF8Email);

			if (!pCardRow)
				return NS_ERROR_NULL_POINTER;

			//add a column with address row id to the list row
			mdb_token listAddressColumnToken;

			char columnStr[16];
			sprintf(columnStr, kMailListAddressFormat, pos); //let it starts from 1
			GetStore()->StringToToken(GetEnv(),  columnStr, &listAddressColumnToken);

			mdbOid outOid;

			if (pCardRow->GetOid(GetEnv(), &outOid) == NS_OK)
			{
				//save address row ID to the list row
				err = AddIntColumn(pListRow, listAddressColumnToken, outOid.mOid_Id);
			}
			pCardRow->CutStrongRef(GetEnv());
		}
	}
	PR_FREEIF(email);

	return NS_OK;
}

nsresult nsAddrDatabase::AddListAttributeColumnsToRow(nsIAbDirectory *list, nsIMdbRow *listRow)
{
	nsresult	err = NS_OK;

	if (!list && !listRow )
		return NS_ERROR_NULL_POINTER;

	mdbOid rowOid, tableOid;
	m_mdbPabTable->GetOid(GetEnv(), &tableOid);
	listRow->GetOid(GetEnv(), &rowOid);

	list->SetDbRowID(rowOid.mOid_Id);

	// add the row to the singleton table.
	if (NS_SUCCEEDED(err) && listRow)
	{
		PRUnichar* pUnicodeStr = nsnull;
		PRInt32 unicharLength = 0;
		char* pUTF8Str = nsnull;

		list->GetListName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
			AddUnicodeToColumn(listRow, m_ListNameColumnToken, pUnicodeStr);
		PR_FREEIF(pUnicodeStr);

		list->GetListNickName(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddListNickName(listRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);
		list->GetDescription(&pUnicodeStr);
		unicharLength = nsCRT::strlen(pUnicodeStr);
		if (pUnicodeStr)
		{
			INTL_ConvertFromUnicode(pUnicodeStr, unicharLength, (char**)&pUTF8Str);
			if (pUTF8Str)
			{
				AddListDescription(listRow, pUTF8Str);
				PR_FREEIF(pUTF8Str);
			}
		}
		PR_FREEIF(pUnicodeStr);

		nsISupportsArray* pAddressLists;
		list->GetAddressLists(&pAddressLists);
		NS_IF_ADDREF(pAddressLists);
		PRUint32 count;
		pAddressLists->Count(&count);

		PRUint32 i, total;
		total = 0;
		for (i = 0; i < count; i++)
		{
			nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(i));
			nsCOMPtr<nsIAbCard> pCard(do_QueryInterface(pSupport, &err));
			
			if (NS_FAILED(err))
				continue;

			PRUnichar *email = nsnull;
			pCard->GetPrimaryEmail(&email);
			PRInt32 emailLength = nsCRT::strlen(email);
			if (email && emailLength)
				total++;
	                PR_FREEIF(email);
		}
		SetListAddressTotal(listRow, total);

		PRUint32 pos;
		for (i = 0; i < count; i++)
		{
			nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(i));
			nsCOMPtr<nsIAbCard> pCard(do_QueryInterface(pSupport, &err));
			
			if (NS_FAILED(err))
				continue;
			pos = i + 1;
			PRUnichar *email = nsnull;
			pCard->GetPrimaryEmail(&email);
			PRInt32 emailLength = nsCRT::strlen(email);
			if (email && emailLength)
			{
				nsCOMPtr<nsIAbCard> pNewCard;
				err = AddListCardColumnsToRow(pCard, listRow, pos, getter_AddRefs(pNewCard));
				if (pNewCard)
					pAddressLists->ReplaceElementAt(pNewCard, i);
			}
	                PR_FREEIF(email);
		}
	}
	return NS_OK;
}

PRUint32 nsAddrDatabase::GetListAddressTotal(nsIMdbRow* listRow)
{
	PRUint32 count = 0;
	GetIntColumn(listRow, m_ListTotalColumnToken, &count, 0);
	return count;
}

nsresult nsAddrDatabase::SetListAddressTotal(nsIMdbRow* listRow, PRUint32 total)
{
	return AddIntColumn(listRow, m_ListTotalColumnToken, total);
}


nsresult nsAddrDatabase::GetAddressRowByPos(nsIMdbRow* listRow, PRUint16 pos, nsIMdbRow** cardRow)
{
	mdb_token listAddressColumnToken;

	char columnStr[16];
	sprintf(columnStr, kMailListAddressFormat, pos);
	GetStore()->StringToToken(GetEnv(),  columnStr, &listAddressColumnToken);

    nsAutoString tempString;
	mdb_id rowID;
	nsresult err = GetIntColumn(listRow, listAddressColumnToken, (PRUint32*)&rowID, 0);
	if (NS_SUCCEEDED(err))
	{
		err = GetCardRowByRowID(rowID, cardRow);
		return err;
	}
	else
	{
		return NS_ERROR_FAILURE;
	}
}

NS_IMETHODIMP nsAddrDatabase::CreateMailListAndAddToDB(nsIAbDirectory *newList, PRBool notify /* = FALSE */)
{
	nsresult	err = NS_OK;
	nsIMdbRow	*listRow;

	if (!newList || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;
	
	err  = GetNewListRow(&listRow);

	if (NS_SUCCEEDED(err) && listRow)
	{
		AddListAttributeColumnsToRow(newList, listRow);
		AddRecordKeyColumnToRow(listRow);
		err = m_mdbPabTable->AddRow(GetEnv(), listRow);

		nsCOMPtr<nsIAbCard> listCard;
		CreateABListCard(listRow, getter_AddRefs(listCard));
		NotifyCardEntryChange(AB_NotifyInserted, listCard, NULL);

		listRow->CutStrongRef(GetEnv());
	}
	if (NS_FAILED(err)) return err;

	return err;
}

nsresult nsAddrDatabase::FindAttributeRow(nsIMdbTable* pTable, mdb_token columnToken, nsIMdbRow** row)
{
	nsIMdbTableRowCursor* rowCursor = nsnull;
	nsIMdbRow* findRow = nsnull;
	nsIMdbCell* valueCell = nsnull;
 	mdb_pos	rowPos = 0;
	nsresult err = NS_ERROR_FAILURE;

	err = pTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

	if (NS_FAILED(err) || !rowCursor)
		return NS_ERROR_FAILURE;
	do
	{
		err = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
		if (NS_SUCCEEDED(err) && findRow)
		{
			err = findRow->GetCell(GetEnv(), columnToken, &valueCell);
			if (NS_SUCCEEDED(err) && valueCell)
			{
				*row = findRow;
				return NS_OK;
			}
		}
	} while (findRow);

	return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::DoStringAnonymousTransaction
(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code)
{
	nsresult err = NS_OK;

	if (pAttributes && pValues)
	{
		PRUint32 count, i;
		count = pAttributes->Count();
		for (i = 0; i < count; i++)
		{
			char* pAttrStr = (char*)pAttributes->ElementAt(i);
			mdb_token anonymousColumnToken;
			GetStore()->StringToToken(GetEnv(),  pAttrStr, &anonymousColumnToken);
			char* pValueStr = (char*)pValues->ElementAt(i);

			nsIMdbRow	*anonymousRow = nsnull;
			if (code == AB_NotifyInserted)
			{
				err  = GetNewRow(&anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddCharStringColumn(anonymousRow, anonymousColumnToken, pValueStr);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else if (code == AB_NotifyDeleted)
			{
				struct mdbYarn yarn;
				mdbOid rowOid;

				GetCharStringYarn(pValueStr, &yarn);
				err = GetStore()->FindRow(GetEnv(), m_CardRowScopeToken, anonymousColumnToken,
										&yarn, &rowOid, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					err = DeleteRow(m_mdbAnonymousTable, anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else /* Edit */
			{
				err = FindAttributeRow(m_mdbAnonymousTable, anonymousColumnToken, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddCharStringColumn(anonymousRow, anonymousColumnToken, pValueStr);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
 		}
	}
	return err;
}

nsresult nsAddrDatabase::DoIntAnonymousTransaction
(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code)
{
	nsresult err = NS_OK;
	if (pAttributes && pValues)
	{
		PRUint32 count, i;
		count = pAttributes->Count();
		for (i = 0; i < count; i++)
		{
			char* pAttrStr = (char*)pAttributes->ElementAt(i);
			mdb_token anonymousColumnToken;
			GetStore()->StringToToken(GetEnv(),  pAttrStr, &anonymousColumnToken);
			PRUint32* pValue = (PRUint32*)pValues->ElementAt(i);
			PRUint32 value = *pValue;

			nsIMdbRow	*anonymousRow = nsnull;
			if (code == AB_NotifyInserted)
			{
				err  = GetNewRow(&anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddIntColumn(anonymousRow, anonymousColumnToken, value);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else if (code == AB_NotifyDeleted)
			{
				struct mdbYarn yarn;
				mdbOid rowOid;
				char yarnBuf[100];

				yarn.mYarn_Buf = (void *) yarnBuf;
				GetIntYarn(value, &yarn);
				err = GetStore()->FindRow(GetEnv(), m_CardRowScopeToken, anonymousColumnToken,
										&yarn, &rowOid, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					err = DeleteRow(m_mdbAnonymousTable, anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else
			{
				err = FindAttributeRow(m_mdbAnonymousTable, anonymousColumnToken, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddIntColumn(anonymousRow, anonymousColumnToken, value);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
 		}
	}
	return err;
}

nsresult nsAddrDatabase::DoBoolAnonymousTransaction
(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code)
{
	nsresult err = NS_OK;
	if (pAttributes && pValues)
	{
		PRUint32 count, i;
		count = m_pAnonymousBoolAttributes->Count();
		for (i = 0; i < count; i++)
		{
			char* pAttrStr = (char*)pAttributes->ElementAt(i);
			mdb_token anonymousColumnToken;
			GetStore()->StringToToken(GetEnv(),  pAttrStr, &anonymousColumnToken);
			PRBool* pValue = (PRBool*)pValues->ElementAt(i);
			PRBool value = *pValue;
			PRUint32 nBoolValue = 0;
			if (value)
				nBoolValue = 1;
			else
				nBoolValue = 0;

			nsIMdbRow	*anonymousRow = nsnull;
			if (code == AB_NotifyInserted)
			{
				err  = GetNewRow(&anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddIntColumn(anonymousRow, anonymousColumnToken, nBoolValue);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else if (code == AB_NotifyDeleted)
			{
				struct mdbYarn yarn;
				mdbOid rowOid;
				char yarnBuf[100];

				yarn.mYarn_Buf = (void *) yarnBuf;
				GetIntYarn(nBoolValue, &yarn);
				err = GetStore()->FindRow(GetEnv(), m_CardRowScopeToken, anonymousColumnToken,
										&yarn, &rowOid, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					err = DeleteRow(m_mdbAnonymousTable, anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
			else
			{
				err = FindAttributeRow(m_mdbAnonymousTable, anonymousColumnToken, &anonymousRow);
				if (NS_SUCCEEDED(err) && anonymousRow)
				{
					AddIntColumn(anonymousRow, anonymousColumnToken, nBoolValue);
					err = m_mdbAnonymousTable->AddRow(GetEnv(), anonymousRow);
					anonymousRow->CutStrongRef(GetEnv());
				}
			}
 		}
	}
	return err;
}

nsresult nsAddrDatabase::DoAnonymousAttributesTransaction(AB_NOTIFY_CODE code)
{
	nsresult err = NS_OK;
		  
	if (!m_mdbAnonymousTable)
		err = InitAnonymousTable();

	if (NS_FAILED(err) || !m_mdbAnonymousTable)
		return NS_ERROR_FAILURE;

	DoStringAnonymousTransaction(m_pAnonymousStrAttributes, m_pAnonymousStrValues, code);
	DoIntAnonymousTransaction(m_pAnonymousIntAttributes, m_pAnonymousIntValues, code);
	DoBoolAnonymousTransaction(m_pAnonymousBoolAttributes, m_pAnonymousBoolValues, code);

	Commit(kSessionCommit);
	return err;
}

void nsAddrDatabase::GetAnonymousAttributesFromCard(nsIAbCard* card)
{
	nsresult err = NS_OK;
	RemoveAnonymousList(m_pAnonymousStrAttributes);
	RemoveAnonymousList(m_pAnonymousStrValues);
	RemoveAnonymousList(m_pAnonymousIntAttributes);
	RemoveAnonymousList(m_pAnonymousIntValues);
	RemoveAnonymousList(m_pAnonymousBoolAttributes);
	RemoveAnonymousList(m_pAnonymousBoolValues);
	err = card->GetAnonymousStrAttrubutesList(&m_pAnonymousStrAttributes);
	err = card->GetAnonymousStrValuesList(&m_pAnonymousStrValues);
	err = card->GetAnonymousIntAttrubutesList(&m_pAnonymousIntAttributes);
	err = card->GetAnonymousIntValuesList(&m_pAnonymousIntValues);
	err = card->GetAnonymousBoolAttrubutesList(&m_pAnonymousBoolAttributes);
	err = card->GetAnonymousBoolValuesList(&m_pAnonymousBoolValues);
}

NS_IMETHODIMP nsAddrDatabase::AddAnonymousAttributesFromCard(nsIAbCard* card)
{
	GetAnonymousAttributesFromCard(card);
	return DoAnonymousAttributesTransaction(AB_NotifyInserted);
}

NS_IMETHODIMP nsAddrDatabase::AddAnonymousAttributesToDB()
{
	return DoAnonymousAttributesTransaction(AB_NotifyInserted);
}

NS_IMETHODIMP nsAddrDatabase::RemoveAnonymousAttributesFromCard(nsIAbCard *card)
{
	GetAnonymousAttributesFromCard(card);
	return DoAnonymousAttributesTransaction(AB_NotifyDeleted);
}

NS_IMETHODIMP nsAddrDatabase::RemoveAnonymousAttributesFromDB()
{
	return DoAnonymousAttributesTransaction(AB_NotifyDeleted);
}

NS_IMETHODIMP nsAddrDatabase::EditAnonymousAttributesFromCard(nsIAbCard* card)
{
	GetAnonymousAttributesFromCard(card);
	return DoAnonymousAttributesTransaction(AB_NotifyPropertyChanged);
}

NS_IMETHODIMP nsAddrDatabase::EditAnonymousAttributesInDB()
{
	return DoAnonymousAttributesTransaction(AB_NotifyPropertyChanged);
}

void nsAddrDatabase::DeleteCardFromAllMailLists(mdb_id cardRowID)
{
	nsIMdbTableRowCursor* rowCursor;
	m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

	if (rowCursor)
	{
		nsIMdbRow* pListRow = nsnull;
		mdb_pos rowPos;
		do 
		{
			mdb_err err = rowCursor->NextRow(GetEnv(), &pListRow, &rowPos);

			if (NS_SUCCEEDED(err) && pListRow)
			{
				mdbOid rowOid;

				if (pListRow->GetOid(GetEnv(), &rowOid) == NS_OK)
				{
					if (IsListRowScopeToken(rowOid.mOid_Scope))
						DeleteCardFromListRow(pListRow, cardRowID);
				}
				pListRow->CutStrongRef(GetEnv());
			}
		} while (pListRow);

		rowCursor->CutStrongRef(GetEnv());
	}
}

NS_IMETHODIMP nsAddrDatabase::DeleteCard(nsIAbCard *card, PRBool notify)
{
	if (!card || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	PRBool bIsMailList = PR_FALSE;
	card->GetIsMailList(&bIsMailList);

	// get the right row
	nsIMdbRow* pCardRow = nsnull;
	mdbOid rowOid;
	if (bIsMailList)
		rowOid.mOid_Scope = m_ListRowScopeToken;
	else
		rowOid.mOid_Scope = m_CardRowScopeToken;
	card->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = GetStore()->GetRow(GetEnv(), &rowOid, &pCardRow);
	if (pCardRow)
	{
		err = DeleteRow(m_mdbPabTable, pCardRow);

		//delete the person card from all mailing list
		if (!bIsMailList)
			DeleteCardFromAllMailLists(rowOid.mOid_Id);

		if (NS_SUCCEEDED(err))
		{
			
			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(card, &err));
			if (NS_FAILED(err)) 
				return NS_ERROR_NULL_POINTER;
			RemoveListener(listener);

			if (notify) 
				NotifyCardEntryChange(AB_NotifyDeleted, card, NULL);
		}
		pCardRow->CutStrongRef(GetEnv());
	}
	return NS_OK;
}

nsresult nsAddrDatabase::DeleteCardFromListRow(nsIMdbRow* pListRow, mdb_id cardRowID)
{
	if (!pListRow)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

//	PRUint32 cardRowID;
//	card->GetDbRowID(&cardRowID);

	//todo: cut the card column and total column
	PRUint32 totalAddress = GetListAddressTotal(pListRow);
	
	PRUint32 pos;
	for (pos = 1; pos <= totalAddress; pos++)
	{
		mdb_token listAddressColumnToken;
		mdb_id rowID;

		char columnStr[16];
		sprintf(columnStr, kMailListAddressFormat, pos);
		GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);

		err = GetIntColumn(pListRow, listAddressColumnToken, (PRUint32*)&rowID, 0);

		if (cardRowID == rowID)
		{
			if (pos == totalAddress)
				err = pListRow->CutColumn(GetEnv(), listAddressColumnToken);
			else
			{
				//replace the deleted one with the last one and delete the last one
				mdb_id lastRowID;
				mdb_token lastAddressColumnToken;
				sprintf(columnStr, kMailListAddressFormat, totalAddress);
				GetStore()->StringToToken(GetEnv(), columnStr, &lastAddressColumnToken);

				err = GetIntColumn(pListRow, lastAddressColumnToken, (PRUint32*)&lastRowID, 0);
				err = AddIntColumn(pListRow, listAddressColumnToken, lastRowID);
				err = pListRow->CutColumn(GetEnv(), lastAddressColumnToken);
			}
			break;
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::DeleteCardFromMailList(nsIAbDirectory *mailList, nsIAbCard *card, PRBool beNotify)
{
	if (!card || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

	// get the right row
	nsIMdbRow* pListRow = nsnull;
	mdbOid listRowOid;
	listRowOid.mOid_Scope = m_ListRowScopeToken;
	mailList->GetDbRowID((PRUint32*)&listRowOid.mOid_Id);
	err = GetStore()->GetRow(GetEnv(), &listRowOid, &pListRow);

	if (pListRow)
	{
		PRUint32 cardRowID;
		card->GetDbRowID(&cardRowID);
		err = DeleteCardFromListRow(pListRow, cardRowID);

		if (NS_SUCCEEDED(err))
		{			
			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(card, &err));
			if (NS_FAILED(err)) 
				return NS_ERROR_NULL_POINTER;
			RemoveListener(listener);

			if (beNotify) 
				NotifyCardEntryChange(AB_NotifyDeleted, card, NULL);
		}
		pListRow->CutStrongRef(GetEnv());
	}
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EditCard(nsIAbCard *card, PRBool notify)
{
	if (!card || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

	nsIMdbRow* pCardRow = nsnull;
	mdbOid rowOid;
	rowOid.mOid_Scope = m_CardRowScopeToken;
	card->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = GetStore()->GetRow(GetEnv(), &rowOid, &pCardRow);
	if (pCardRow)
		err = AddAttributeColumnsToRow(card, pCardRow);
	if (NS_FAILED(err)) return err;

	if (notify) 
		NotifyCardEntryChange(AB_NotifyPropertyChanged, card, NULL);

	if (pCardRow)
		pCardRow->CutStrongRef(GetEnv());
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsCard(nsIAbCard *card, PRBool *hasCard)
{
	if (!card || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	mdb_bool hasOid;
	mdbOid rowOid;

	rowOid.mOid_Scope = m_CardRowScopeToken;
	card->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = m_mdbPabTable->HasOid(GetEnv(), &rowOid, &hasOid);
	if (NS_SUCCEEDED(err))
		*hasCard = hasOid;

	return err;
}

NS_IMETHODIMP nsAddrDatabase::DeleteMailList(nsIAbDirectory *mailList, PRBool notify)
{
	if (!mailList || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

	// get the row
	nsIMdbRow* pListRow = nsnull;
	mdbOid rowOid;
	rowOid.mOid_Scope = m_ListRowScopeToken;
	mailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = GetStore()->GetRow(GetEnv(), &rowOid, &pListRow);
	if (pListRow)
	{
		err = DeleteRow(m_mdbPabTable, pListRow);

		if (NS_SUCCEEDED(err))
		{
			
			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(mailList, &err));
			if (NS_FAILED(err)) 
				return NS_ERROR_NULL_POINTER;
			RemoveListener(listener);
		}
		pListRow->CutStrongRef(GetEnv());
	}
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EditMailList(nsIAbDirectory *mailList, PRBool notify)
{
	if (!mailList || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;

	nsIMdbRow* pListRow = nsnull;
	mdbOid rowOid;
	rowOid.mOid_Scope = m_ListRowScopeToken;
	mailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = GetStore()->GetRow(GetEnv(), &rowOid, &pListRow);
	if (pListRow)
	{
		err = AddListAttributeColumnsToRow(mailList, pListRow);
	}
	if (NS_FAILED(err)) return err;

	if (notify)
	{
		NotifyListEntryChange(AB_NotifyPropertyChanged, mailList, nsnull);

		char* listCardURI = nsnull;
		char* file = nsnull;
		file = m_dbName.GetLeafName();
		listCardURI = PR_smprintf("%s%s/ListCard%ld", kCardDataSourceRoot, file, rowOid.mOid_Id);
		NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &err);
		if(NS_FAILED(err))
			return err;
		nsCOMPtr<nsIRDFResource> listCardResource;
		err = rdfService->GetResource(listCardURI, getter_AddRefs(listCardResource));
		nsCOMPtr<nsIAbCard> listCard = do_QueryInterface(listCardResource);
		if (listCard)
		{
			GetListCardFromDB(listCard, pListRow);
			NotifyCardEntryChange(AB_NotifyPropertyChanged, listCard, nsnull);
		}
		if (file)
			nsCRT::free(file);
		if (listCardURI)
			PR_smprintf_free(listCardURI);
	}

	if (pListRow)
		pListRow->CutStrongRef(GetEnv());
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsMailList(nsIAbDirectory *mailList, PRBool *hasList)
{
	if (!mailList || !m_mdbPabTable)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	mdb_bool hasOid;
	mdbOid rowOid;

	rowOid.mOid_Scope = m_ListRowScopeToken;
	mailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
	err = m_mdbPabTable->HasOid(GetEnv(), &rowOid, &hasOid);
	if (NS_SUCCEEDED(err))
		*hasList = hasOid;

	return err;
}

NS_IMETHODIMP nsAddrDatabase::GetNewRow(nsIMdbRow * *newRow)
{
	nsresult err = NS_OK;
	nsIMdbRow *row = nsnull;
	err  = GetStore()->NewRow(GetEnv(), m_CardRowScopeToken, &row);
	*newRow = row;
	return err;
}

NS_IMETHODIMP nsAddrDatabase::GetNewListRow(nsIMdbRow * *newRow)
{
	nsresult err = NS_OK;
	nsIMdbRow *row = nsnull;
	err  = GetStore()->NewRow(GetEnv(), m_ListRowScopeToken, &row);
	*newRow = row;
	return err;
}

NS_IMETHODIMP nsAddrDatabase::AddCardRowToDB(nsIMdbRow *newRow)
{
	if (m_mdbPabTable)
	{
		nsresult err = NS_OK;
		err = m_mdbPabTable->AddRow(GetEnv(), newRow);
		if (NS_SUCCEEDED(err))
			AddRecordKeyColumnToRow(newRow);
		return err;
	}
	else
		return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP nsAddrDatabase::AddLdifListMember(nsIMdbRow* listRow, const char* value)
{
	PRUint32 total = GetListAddressTotal(listRow);
	//add member
	nsCAutoString valueString(value);
	nsCAutoString email;
	PRInt32 emailPos = valueString.Find("mail=");
	emailPos += nsCRT::strlen("mail=");
	valueString.Right(email, valueString.Length() - emailPos);
	char* emailAddress = (char*)email.ToNewCString();
	nsIMdbRow	*cardRow = nsnull;	
	mdb_err result = GetRowForEmailAddress(emailAddress, &cardRow);
	if (cardRow)
	{
		mdbOid outOid;
		mdb_id rowID = 0;
		if (cardRow->GetOid(GetEnv(), &outOid) == NS_OK)
			rowID = outOid.mOid_Id;
		total += 1;

		mdb_token listAddressColumnToken;
		char columnStr[16];
		sprintf(columnStr, kMailListAddressFormat, total); //let it starts from 1
		GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);
		result = AddIntColumn(listRow, listAddressColumnToken, rowID);

		SetListAddressTotal(listRow, total);
		cardRow->CutStrongRef(GetEnv());
	}
	if (emailAddress)
		Recycle(emailAddress);
	return NS_OK;
}
 

void nsAddrDatabase::GetCharStringYarn(char* str, struct mdbYarn* strYarn)
{
	strYarn->mYarn_Grow = NULL;
	strYarn->mYarn_Buf = str;
	strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
	strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
	strYarn->mYarn_Form = 0;
}

void nsAddrDatabase::GetStringYarn(nsString* str, struct mdbYarn* strYarn)
{
	strYarn->mYarn_Buf = str->ToNewCString();
	strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
	strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
	strYarn->mYarn_Form = 0;	 
}

void nsAddrDatabase::GetIntYarn(PRUint32 nValue, struct mdbYarn* intYarn)
{
	intYarn->mYarn_Size = sizeof(intYarn->mYarn_Buf);
	intYarn->mYarn_Fill = intYarn->mYarn_Size;
	intYarn->mYarn_Form = 0;
	intYarn->mYarn_Grow = NULL;

	PR_snprintf((char*)intYarn->mYarn_Buf, intYarn->mYarn_Size, "%lx", nValue);
	intYarn->mYarn_Fill = PL_strlen((const char *) intYarn->mYarn_Buf);
}

mdb_err nsAddrDatabase::AddCharStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, const char* str)
{
	struct mdbYarn yarn;

	GetCharStringYarn((char *) str, &yarn);
	mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);

	return err;
}

mdb_err nsAddrDatabase::AddStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, nsString* str)
{
	struct mdbYarn yarn;

	GetStringYarn(str, &yarn);
	mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);

	return err;
}

mdb_err nsAddrDatabase::AddIntColumn(nsIMdbRow* cardRow, mdb_column inColumn, PRUint32 nValue)
{
	struct mdbYarn yarn;
	char	yarnBuf[100];

	yarn.mYarn_Buf = (void *) yarnBuf;
	GetIntYarn(nValue, &yarn);
	return cardRow->AddColumn(GetEnv(),  inColumn, &yarn);
}

mdb_err nsAddrDatabase::AddBoolColumn(nsIMdbRow* cardRow, mdb_column inColumn, PRBool bValue)
{
	struct mdbYarn yarn;
	char	yarnBuf[100];

	yarn.mYarn_Buf = (void *) yarnBuf;
	if (bValue)
		GetIntYarn(1, &yarn);
	else
		GetIntYarn(0, &yarn);
	return cardRow->AddColumn(GetEnv(), inColumn, &yarn);
}

nsresult nsAddrDatabase::GetStringColumn(nsIMdbRow *cardRow, mdb_token outToken, nsString& str)
{
	nsresult	err = NS_ERROR_FAILURE;
	nsIMdbCell	*cardCell;

	if (cardRow)	
	{
		err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
		if (err == NS_OK && cardCell)
		{
			struct mdbYarn yarn;
			cardCell->AliasYarn(GetEnv(), &yarn);
            NS_ConvertUTF8toUCS2 uniStr((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);
            if (uniStr.Length() > 0)
                str.Assign(uniStr);
            else
                err = NS_ERROR_FAILURE;
			cardCell->CutStrongRef(GetEnv()); // always release ref
		}
		else
			err = NS_ERROR_FAILURE;
	}
	return err;
}

void nsAddrDatabase::YarnToUInt32(struct mdbYarn *yarn, PRUint32 *pResult)
{
	PRUint32 i, result, numChars;
	char *p = (char *) yarn->mYarn_Buf;
	if (yarn->mYarn_Fill > 8)
		numChars = 8;
	else
		numChars = yarn->mYarn_Fill;
	for (i=0, result = 0; i < numChars; i++, p++)
	{
		char C = *p;

		PRInt8 unhex = ((C >= '0' && C <= '9') ? C - '0' :
			((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
			 ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : -1)));
		if (unhex < 0)
			break;
		result = (result << 4) | unhex;
	}
    
	*pResult = result;
}

nsresult nsAddrDatabase::GetIntColumn
(nsIMdbRow *cardRow, mdb_token outToken, PRUint32* pValue, PRUint32 defaultValue)
{
	nsresult	err = NS_ERROR_FAILURE;
	nsIMdbCell	*cardCell;

	if (pValue)
		*pValue = defaultValue;
	if (cardRow)
	{
		err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
		if (err == NS_OK && cardCell)
		{
			struct mdbYarn yarn;
			cardCell->AliasYarn(GetEnv(), &yarn);
			YarnToUInt32(&yarn, pValue);
			cardCell->CutStrongRef(GetEnv());
		}
		else
			err = NS_ERROR_FAILURE;
	}
	return err;
}

nsresult nsAddrDatabase::GetBoolColumn(nsIMdbRow *cardRow, mdb_token outToken, PRBool* pValue)
{
	nsresult	err = NS_ERROR_FAILURE;
	nsIMdbCell	*cardCell;
	PRUint32 nValue = 0;

	if (cardRow)
	{
		err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
		if (err == NS_OK && cardCell)
		{
			struct mdbYarn yarn;
			cardCell->AliasYarn(GetEnv(), &yarn);
			YarnToUInt32(&yarn, &nValue);
			cardCell->CutStrongRef(GetEnv());
		}
		else
			err = NS_ERROR_FAILURE;
	}
	if (nValue == 0)
		*pValue = PR_FALSE;
	else
		*pValue = PR_TRUE;
	return err;
}


nsresult nsAddrDatabase::SetAnonymousAttribute
(nsVoidArray** pAttrAray, nsVoidArray** pValueArray, void *attrname, void *value)
{
	nsresult rv = NS_OK;
	nsVoidArray* pAttributes = *pAttrAray;
	nsVoidArray* pValues = *pValueArray; 

	if (!pAttributes && !pValues)
	{
		pAttributes = new nsVoidArray();
		pValues = new nsVoidArray();
	}
	if (pAttributes && pValues)
	{
		if (attrname && value)
		{
			pAttributes->AppendElement(attrname);
			pValues->AppendElement(value);
			*pAttrAray = pAttributes;
			*pValueArray = pValues;
		}
		else
		{
			delete pAttributes;
			delete pValues;
		}
	}
	else
	{ 
		rv = NS_ERROR_FAILURE;
	}

	return rv;
}	


NS_IMETHODIMP nsAddrDatabase::SetAnonymousStringAttribute
(const char *attrname, const char *value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	char* pValue = PL_strdup(value);
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousStrAttributes, 
			&m_pAnonymousStrValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}	

NS_IMETHODIMP nsAddrDatabase::SetAnonymousIntAttribute
(const char *attrname, PRUint32 value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	PRUint32* pValue = (PRUint32 *)PR_Calloc(1, sizeof(PRUint32));
	*pValue = value;
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousIntAttributes, 
			&m_pAnonymousIntValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}	

NS_IMETHODIMP nsAddrDatabase::SetAnonymousBoolAttribute
(const char *attrname, PRBool value)
{
	nsresult rv = NS_OK;

	char* pAttribute = PL_strdup(attrname);
	PRBool* pValue = (PRBool *)PR_Calloc(1, sizeof(PRBool));
	*pValue = value;
	if (pAttribute && pValue)
	{
		rv = SetAnonymousAttribute(&m_pAnonymousBoolAttributes, 
			&m_pAnonymousBoolValues, pAttribute, pValue);
	}
	else
	{
		PR_FREEIF(pAttribute);
		PR_FREEIF(pValue);
		rv = NS_ERROR_NULL_POINTER;
	}
	return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetAnonymousStringAttribute(const char *attrname, char** value)
{
	if (m_mdbAnonymousTable)
	{
		nsIMdbRow* cardRow;
		nsIMdbTableRowCursor* rowCursor;
		mdb_pos rowPos;
		nsAutoString tempString;
		char *tempCString = nsnull;

		mdb_token anonymousColumnToken;
		GetStore()->StringToToken(GetEnv(), attrname, &anonymousColumnToken);


		m_mdbAnonymousTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		do 
		{
			mdb_err err = rowCursor->NextRow(GetEnv(), &cardRow, &rowPos);

			if (NS_SUCCEEDED(err) && cardRow)
			{
				err = GetStringColumn(cardRow, anonymousColumnToken, tempString);
				if (NS_SUCCEEDED(err) && tempString.Length())
				{
                    tempCString = tempString.ToNewUTF8String();
					*value = PL_strdup(tempCString);
					Recycle(tempCString);
					return NS_OK;
				}
				cardRow->CutStrongRef(GetEnv());
			}
		} while (cardRow);
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::GetAnonymousIntAttribute(const char *attrname, PRUint32* value)
{
	if (m_mdbAnonymousTable)
	{
		nsIMdbRow* cardRow;
		nsIMdbTableRowCursor* rowCursor;
		mdb_pos rowPos;
		PRUint32 nValue;

		mdb_token anonymousColumnToken;
		GetStore()->StringToToken(GetEnv(), attrname, &anonymousColumnToken);


		m_mdbAnonymousTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		do 
		{
			mdb_err err = rowCursor->NextRow(GetEnv(), &cardRow, &rowPos);

			if (NS_SUCCEEDED(err) && cardRow)
			{
				err = GetIntColumn(cardRow, anonymousColumnToken, &nValue, 0);
				if (NS_SUCCEEDED(err))
				{
					*value = nValue;
					return err;
				}
				cardRow->CutStrongRef(GetEnv());
			}
		} while (cardRow);
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::GetAnonymousBoolAttribute(const char *attrname, PRBool* value)
{
	if (m_mdbAnonymousTable)
	{
		nsIMdbRow* cardRow;
		nsIMdbTableRowCursor* rowCursor;
		mdb_pos rowPos;
		PRUint32 nValue;

		mdb_token anonymousColumnToken;
		GetStore()->StringToToken(GetEnv(), attrname, &anonymousColumnToken);


		m_mdbAnonymousTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		do 
		{
			mdb_err err = rowCursor->NextRow(GetEnv(), &cardRow, &rowPos);

			if (NS_SUCCEEDED(err) && cardRow)
			{
				err = GetIntColumn(cardRow, anonymousColumnToken, &nValue, 0);
				if (NS_SUCCEEDED(err))
				{
					if (nValue)
						*value = PR_TRUE;
					else
						*value = PR_FALSE;
					return err;
				}
				cardRow->CutStrongRef(GetEnv());
			}
		} while (cardRow);
	}
	return NS_ERROR_FAILURE;
}

/*  value if UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddPrimaryEmail(nsIMdbRow * row, const char * value)
{
	if (!value)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	err = AddCharStringColumn(row, m_PriEmailColumnToken, value);

	if (NS_SUCCEEDED(err))
	{
		err = AddLowercaseColumn(row, m_LowerPriEmailColumnToken, value);
	}
	return err;
}

/*  value if UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddListName(nsIMdbRow * row, const char * value)
{
	if (!value)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	err = AddCharStringColumn(row, m_ListNameColumnToken, value);
	if (NS_SUCCEEDED(err))
	{
		err = AddLowercaseColumn(row, m_LowerListNameColumnToken, value);
	}
	return err;
}

/* 
value is UTF8 string, need to convert back to lowercase unicode then 
back to UTF8 string
*/
nsresult nsAddrDatabase::AddLowercaseColumn
(nsIMdbRow * row, mdb_token columnToken, const char* utf8String)
{
	nsresult err = NS_OK;
	if (utf8String)
	{
		PRUnichar *unicodeStr = nsnull;
		INTL_ConvertToUnicode((const char *)utf8String, nsCRT::strlen(utf8String), (void**)&unicodeStr);
		if (unicodeStr)
		{
			nsAutoString newUnicodeString(unicodeStr);
			newUnicodeString.ToLowerCase();
			char * pUTF8Str = newUnicodeString.ToNewUTF8String();
			if (pUTF8Str)
			{
				err = AddCharStringColumn(row, columnToken, pUTF8Str);
				Recycle(pUTF8Str);
			}
			PR_FREEIF(unicodeStr);
		}
	}
	return err;
}

nsresult nsAddrDatabase::GetCardFromDB(nsIAbCard *newCard, nsIMdbRow* cardRow)
{
	nsresult	err = NS_OK;
	if (!newCard || !cardRow)
		return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

	err = GetStringColumn(cardRow, m_FirstNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetFirstName(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_LastNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetLastName(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_DisplayNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetDisplayName(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_NickNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetNickName(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_PriEmailColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetPrimaryEmail(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_2ndEmailColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetSecondEmail(tempString.GetUnicode());
	}

	PRBool bValue = PR_FALSE;
	err = GetBoolColumn(cardRow, m_PlainTextColumnToken, &bValue);
	if (NS_SUCCEEDED(err))
		newCard->SetSendPlainText(bValue);

	err = GetStringColumn(cardRow, m_WorkPhoneColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkPhone(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomePhoneColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomePhone(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_FaxColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetFaxNumber(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_PagerColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetPagerNumber(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_CellularColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCellularNumber(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeAddressColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeAddress(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeAddress2ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeAddress2(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeCityColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeCity(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeStateColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeState(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeZipCodeColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeZipCode(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_HomeCountryColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetHomeCountry(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkAddressColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkAddress(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkAddress2ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkAddress2(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkCityColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkCity(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkStateColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkState(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkZipCodeColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkZipCode(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WorkCountryColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWorkCountry(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_JobTitleColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetJobTitle(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_DepartmentColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetDepartment(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_CompanyColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCompany(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WebPage1ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWebPage1(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_WebPage2ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetWebPage2(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_BirthYearColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetBirthYear(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_BirthMonthColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetBirthMonth(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_BirthDayColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetBirthDay(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_Custom1ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCustom1(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_Custom2ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCustom2(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_Custom3ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCustom3(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_Custom4ColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetCustom4(tempString.GetUnicode());
	}

	err = GetStringColumn(cardRow, m_NotesColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newCard->SetNotes(tempString.GetUnicode());
	}
	PRUint32 key = 0;
	err = GetIntColumn(cardRow, m_RecordKeyColumnToken, &key, 0);
	if (NS_SUCCEEDED(err))
		newCard->SetRecordKey(key);

	return err;
}

nsresult nsAddrDatabase::GetListCardFromDB(nsIAbCard *listCard, nsIMdbRow* listRow)
{
	nsresult	err = NS_OK;
	if (!listCard || !listRow)
		return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

	err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        listCard->SetDisplayName(tempString.GetUnicode());
        listCard->SetLastName(tempString.GetUnicode());
	}
	err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        listCard->SetNickName(tempString.GetUnicode());
	}
	err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        listCard->SetNotes(tempString.GetUnicode());
	}
	PRUint32 key = 0;
	err = GetIntColumn(listRow, m_RecordKeyColumnToken, &key, 0);
	if (NS_SUCCEEDED(err))
		listCard->SetRecordKey(key);
	return err;
}

nsresult nsAddrDatabase::GetListFromDB(nsIAbDirectory *newList, nsIMdbRow* listRow)
{
	nsresult	err = NS_OK;
	if (!newList || !listRow)
		return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

	err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newList->SetListName(tempString.GetUnicode());
	}
	err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newList->SetListNickName(tempString.GetUnicode());
	}
	err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
	if (NS_SUCCEEDED(err) && tempString.Length())
	{
        newList->SetDescription(tempString.GetUnicode());
	}

	PRUint32 totalAddress = GetListAddressTotal(listRow);
	PRUint32 pos;
	for (pos = 1; pos <= totalAddress; pos++)
	{
		mdb_token listAddressColumnToken;
		mdb_id rowID;

		char columnStr[16];
		sprintf(columnStr, kMailListAddressFormat, pos);
		GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);

		nsIMdbRow* cardRow;
		err = GetIntColumn(listRow, listAddressColumnToken, (PRUint32*)&rowID, 0);
		err = GetCardRowByRowID(rowID, &cardRow);
		
		if (cardRow)
		{
			nsCOMPtr<nsIAbCard> card;
			err = CreateABCard(cardRow, getter_AddRefs(card));
			newList->AddAddressToList(card);
		}
//		NS_IF_ADDREF(card);
	}

	return err;
}

class nsAddrDBEnumerator : public nsIEnumerator 
{
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_DECL_NSIENUMERATOR

    // nsAddrDBEnumerator methods:

    nsAddrDBEnumerator(nsAddrDatabase* db);
    virtual ~nsAddrDBEnumerator();

protected:
    nsCOMPtr<nsAddrDatabase>    mDB;
	nsCOMPtr<nsIAbDirectory>	mResultList;
	nsCOMPtr<nsIAbCard>			mResultCard;
	nsIMdbTable*				mDbTable;
	nsIMdbTableRowCursor*       mRowCursor;
	nsIMdbRow*					mCurrentRow;
 	mdb_pos						mRowPos;
    PRBool                      mDone;
    PRBool                      mCurrentRowIsList;
};

nsAddrDBEnumerator::nsAddrDBEnumerator(nsAddrDatabase* db)
    : mDB(db), mRowCursor(nsnull), mCurrentRow(nsnull), mDone(PR_FALSE)
{
    NS_INIT_REFCNT();
	mDbTable = mDB->GetPabTable();
	mCurrentRowIsList = PR_FALSE;
}

nsAddrDBEnumerator::~nsAddrDBEnumerator()
{
	if (mRowCursor)
		mRowCursor->CutStrongRef(mDB->GetEnv());
}

NS_IMPL_ISUPPORTS(nsAddrDBEnumerator, NS_GET_IID(nsIEnumerator))

NS_IMETHODIMP nsAddrDBEnumerator::First(void)
{
	nsresult rv = 0;
	mDone = PR_FALSE;

	if (!mDB || !mDbTable || !mDB->GetEnv())
		return NS_ERROR_NULL_POINTER;
		
	mDbTable->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
	if (NS_FAILED(rv)) return rv;
    return Next();
}

NS_IMETHODIMP nsAddrDBEnumerator::Next(void)
{
	if (!mRowCursor)
	{
		mDone = PR_TRUE;
		return NS_ERROR_FAILURE;
	}
	if (mCurrentRow)
		mCurrentRow->CutStrongRef(mDB->GetEnv());
	nsresult rv = mRowCursor->NextRow(mDB->GetEnv(), &mCurrentRow, &mRowPos);
    if (mCurrentRow && NS_SUCCEEDED(rv))
	{
		mdbOid rowOid;

		if (mCurrentRow->GetOid(mDB->GetEnv(), &rowOid) == NS_OK)
		{
			if (mDB->IsListRowScopeToken(rowOid.mOid_Scope))
			{
				mCurrentRowIsList = PR_TRUE;
				return NS_OK;
			}
			else if (mDB->IsCardRowScopeToken(rowOid.mOid_Scope))
			{
				mCurrentRowIsList = PR_FALSE;
				return NS_OK;
			}
			else if (mDB->IsDataRowScopeToken(rowOid.mOid_Scope))
			{
				return Next();
			}
			else
				return NS_ERROR_FAILURE;
		}
	}
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
        nsresult rv;
		if (mCurrentRowIsList)
		{
			rv = mDB->CreateABListCard(mCurrentRow, getter_AddRefs(mResultCard));
			*aItem = mResultCard;
		}
		else
		{
			rv = mDB->CreateABCard(mCurrentRow, getter_AddRefs(mResultCard));
			*aItem = mResultCard;
		}
		NS_IF_ADDREF(*aItem);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_ERROR_FAILURE;
}

class nsListAddressEnumerator : public nsIEnumerator 
{
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_DECL_NSIENUMERATOR

    // nsAddrDBEnumerator methods:

    nsListAddressEnumerator(nsAddrDatabase* db, mdb_id rowID);
    virtual ~nsListAddressEnumerator();

protected:
    nsCOMPtr<nsAddrDatabase>    mDB;
	nsCOMPtr<nsIAbCard>			mResultCard;
	nsIMdbTable*				mDbTable;
	nsIMdbRow*					mListRow;
	nsIMdbRow*					mCurrentRow;
	mdb_id						mListRowID;
    PRBool                      mDone;
	PRUint32					mAddressTotal;
	PRUint16					mAddressPos;
};

nsListAddressEnumerator::nsListAddressEnumerator(nsAddrDatabase* db, mdb_id rowID)
    : mDB(db), mCurrentRow(nsnull), mListRowID(rowID), mDone(PR_FALSE)
{
    NS_INIT_REFCNT();
	mDbTable = mDB->GetPabTable();
	mDB->GetListRowByRowID(mListRowID, &mListRow);
	mAddressTotal = mDB->GetListAddressTotal(mListRow);
	mAddressPos = 0;
}

nsListAddressEnumerator::~nsListAddressEnumerator()
{
	if (mListRow)
		mListRow->CutStrongRef(mDB->GetEnv());
}

NS_IMPL_ISUPPORTS(nsListAddressEnumerator, NS_GET_IID(nsIEnumerator))

NS_IMETHODIMP nsListAddressEnumerator::First(void)
{
	mDone = PR_FALSE;

	if (!mDB || !mDbTable || !mDB->GetEnv())
		return NS_ERROR_NULL_POINTER;
	
	//got total address count and start
	if (mAddressTotal)
		return Next();
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsListAddressEnumerator::Next(void)
{
	//go to the next address column
	if (mCurrentRow)
	{
		mCurrentRow->CutStrongRef(mDB->GetEnv());
		mCurrentRow = nsnull;
	}
	mAddressPos++;
	if (mAddressPos <= mAddressTotal)
	{
		nsresult err;
		err = mDB->GetAddressRowByPos(mListRow, mAddressPos, &mCurrentRow);
		if (mCurrentRow)
			return NS_OK;
		else
		{
			mDone = PR_TRUE;
			return NS_ERROR_FAILURE;
		}
	}
	else
	{
        mDone = PR_TRUE;
		return NS_ERROR_FAILURE;
	}
}

NS_IMETHODIMP nsListAddressEnumerator::CurrentItem(nsISupports **aItem)
{
    if (mCurrentRow) 
	{
        nsresult rv;
		rv = mDB->CreateABCardInList(mCurrentRow, getter_AddRefs(mResultCard), mListRowID);
		*aItem = mResultCard;
		NS_IF_ADDREF(*aItem);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsListAddressEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAddrDatabase::EnumerateCards(nsIAbDirectory *directory, nsIEnumerator **result)
{
    nsAddrDBEnumerator* e = new nsAddrDBEnumerator(this);
	m_dbDirectory = directory;
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetMailingListsFromDB(nsIAbDirectory *parentDir)
{
	nsCOMPtr<nsIAbDirectory>	resultList;
	nsIMdbTable*				dbTable = nsnull;
	nsIMdbTableRowCursor*       rowCursor = nsnull;
	nsIMdbRow*					currentRow = nsnull;
 	mdb_pos						rowPos;
    PRBool                      done = PR_FALSE;

	m_dbDirectory = parentDir;
	dbTable = GetPabTable();

	if (!dbTable)
		return NS_ERROR_FAILURE;

	dbTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
	if (!rowCursor)
		return NS_ERROR_FAILURE;

	while (!done)
	{
		if (currentRow)
			currentRow->CutStrongRef(GetEnv());
		nsresult rv = rowCursor->NextRow(GetEnv(), &currentRow, &rowPos);
		if (currentRow && NS_SUCCEEDED(rv))
		{
			mdbOid rowOid;

			if (currentRow->GetOid(GetEnv(), &rowOid) == NS_OK)
			{
				if (IsListRowScopeToken(rowOid.mOid_Scope))
					rv = CreateABList(currentRow, getter_AddRefs(resultList));
			}
		}
		else
			done = PR_TRUE;
	}
	if (rowCursor)
		rowCursor->CutStrongRef(GetEnv());
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EnumerateListAddresses(nsIAbDirectory *directory, nsIEnumerator **result)
{
	mdb_id rowID;
	directory->GetDbRowID((PRUint32*)&rowID);
    nsListAddressEnumerator* e = new nsListAddressEnumerator(this, rowID);
	m_dbDirectory = directory;
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

nsresult nsAddrDatabase::CreateCard(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
    nsresult rv = NS_OK; 

	mdbOid outOid;
	mdb_id rowID=0;

	if (cardRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

	if(NS_SUCCEEDED(rv))
	{
		char* cardURI = nsnull;
		char* file = nsnull;

		file = m_dbName.GetLeafName();
		if (listRowID > 0)
			cardURI = PR_smprintf("%s%s/MailList%ld/Card%ld", kCardDataSourceRoot, file, listRowID, rowID);
		else
			cardURI = PR_smprintf("%s%s/Card%ld", kCardDataSourceRoot, file, rowID);
		nsCOMPtr<nsIAbCard> personCard;
		rv = m_dbDirectory->AddChildCards(cardURI, getter_AddRefs(personCard));
		if (personCard)
		{
			GetCardFromDB(personCard, cardRow);
			mdbOid tableOid;
			m_mdbPabTable->GetOid(GetEnv(), &tableOid);
			personCard->SetDbTableID(tableOid.mOid_Id);
			personCard->SetDbRowID(rowID);
			personCard->SetAbDatabase(this);

			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(personCard, &rv));
			if (NS_FAILED(rv)) 
				return NS_ERROR_NULL_POINTER;

			AddListener(listener);
		}
		*result = personCard;
		NS_IF_ADDREF(*result);

		if (file)
			nsCRT::free(file);
		if (cardURI)
			PR_smprintf_free(cardURI);
	}

	return rv;
}

/* create a card for a person in the address book */
nsresult nsAddrDatabase::CreateABCard(nsIMdbRow* cardRow, nsIAbCard **result)
{
    return CreateCard(cardRow, 0, result);
}

/* create a card for a person in the mailing list, when clicking on a mailing */
/* list on the left pane, show this card on the result pane.                  */
/* The uri is different from a person card in the address book.               */

nsresult nsAddrDatabase::CreateABCardInList(nsIMdbRow* cardRow, nsIAbCard **result, mdb_id listRowID)
{
    return CreateCard(cardRow, listRowID, result);
}

/* create a card for mailing list in the address book */
nsresult nsAddrDatabase::CreateABListCard(nsIMdbRow* listRow, nsIAbCard **result)
{
    nsresult rv = NS_OK; 

	mdbOid outOid;
	mdb_id rowID=0;

	if (listRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

	if(NS_SUCCEEDED(rv))
	{
		char* cardURI = nsnull;
		char* file = nsnull;

		file = m_dbName.GetLeafName();
		cardURI = PR_smprintf("%s%s/ListCard%ld", kCardDataSourceRoot, file, rowID);
		nsCOMPtr<nsIAbCard> personCard;
		rv = m_dbDirectory->AddChildCards(cardURI, getter_AddRefs(personCard));
		if (personCard)
		{
			GetListCardFromDB(personCard, listRow);
			mdbOid tableOid;
			m_mdbPabTable->GetOid(GetEnv(), &tableOid);
			personCard->SetDbTableID(tableOid.mOid_Id);
			personCard->SetDbRowID(rowID);
			personCard->SetAbDatabase(this);
			personCard->SetIsMailList(PR_TRUE);

			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(personCard, &rv));
			if (NS_FAILED(rv)) 
				return NS_ERROR_NULL_POINTER;

			AddListener(listener);
		}
		*result = personCard;
		NS_IF_ADDREF(*result);

		if (file)
			nsCRT::free(file);
		if (cardURI)
			PR_smprintf_free(cardURI);
	}

	return rv;
}

/* create a sub directory for mailing list in the address book left pane */
nsresult nsAddrDatabase::CreateABList(nsIMdbRow* listRow, nsIAbDirectory **result)
{
    nsresult rv = NS_OK; 

	if (!listRow)
		return NS_ERROR_NULL_POINTER;

	mdbOid outOid;
	mdb_id rowID=0;

	if (listRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

	if(NS_SUCCEEDED(rv))
	{
		char* listURI = nsnull;
		char* file = nsnull;

		file = m_dbName.GetLeafName();
		listURI = PR_smprintf("%s%s/MailList%ld", kDirectoryDataSourceRoot, file, rowID);
		nsCOMPtr<nsIAbDirectory> mailList;
		rv = m_dbDirectory->AddDirectory(listURI, getter_AddRefs(mailList));
		if (mailList)
		{
			GetListFromDB(mailList, listRow);
			mailList->SetDbRowID(rowID);
			mailList->SetIsMailList(PR_TRUE);

			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(mailList, &rv));
			if (NS_FAILED(rv)) 
				return NS_ERROR_NULL_POINTER;
			AddListener(listener);

			m_dbDirectory->AddMailListToDirectory(mailList);

			*result = mailList;
			NS_IF_ADDREF(*result);
		}

		if (file)
			nsCRT::free(file);
		if (listURI)
			PR_smprintf_free(listURI);
	}

	return rv;
}

nsresult nsAddrDatabase::GetCardRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
	mdbOid rowOid;
	rowOid.mOid_Scope = m_CardRowScopeToken;
	rowOid.mOid_Id = rowID;

	return GetStore()->GetRow(GetEnv(), &rowOid, dbRow);
}

nsresult nsAddrDatabase::GetListRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
	mdbOid rowOid;
	rowOid.mOid_Scope = m_ListRowScopeToken;
	rowOid.mOid_Id = rowID;

	return GetStore()->GetRow(GetEnv(), &rowOid, dbRow);
}

/*
  "emailAddress" is a UTF8 string, need to convert to lowercase unicode string, then UTF8 string
  for comparison.
*/
nsresult nsAddrDatabase::GetRowForEmailAddress(const char *emailAddress, nsIMdbRow	**cardRow)
{
	nsresult result = NS_ERROR_FAILURE;
	PRUnichar *unicodeStr = nsnull;
	INTL_ConvertToUnicode((const char *)emailAddress, nsCRT::strlen(emailAddress), (void**)&unicodeStr);
	if (unicodeStr)
	{
		nsAutoString newUnicodeString(unicodeStr);
		newUnicodeString.ToLowerCase();
		char * pUTF8Str = newUnicodeString.ToNewUTF8String();
		if (pUTF8Str)
		{
			result = GetRowForCharColumn(pUTF8Str, m_LowerPriEmailColumnToken, PR_TRUE, cardRow);
			Recycle(pUTF8Str);
		}	
		PR_Free(unicodeStr);
	}
	return result;
}

NS_IMETHODIMP nsAddrDatabase::GetCardForEmailAddress(nsIAbDirectory *directory, const char *emailAddress, nsIAbCard **cardResult)
{
	nsresult rv = NS_OK;
	if (!cardResult)
		return NS_ERROR_NULL_POINTER;

	m_dbDirectory = directory;
	nsIMdbRow	*cardRow = nsnull;
	
	mdb_err result = GetRowForEmailAddress(emailAddress, &cardRow);

	if (NS_SUCCEEDED(result) && cardRow)
	{
		rv = CreateABCard(cardRow, cardResult);
	}
	else
		*cardResult = nsnull;
	return rv;
}

nsresult nsAddrDatabase::GetCollationKeyGenerator()
{
	nsresult rv = NS_OK;
	if (!m_collationKeyGenerator)
	{
		nsCOMPtr<nsILocale> locale; 

		NS_WITH_SERVICE(nsILocaleService, localeSvc, kLocaleServiceCID, &rv);
		if (NS_FAILED(rv)) return rv;

		rv = localeSvc->GetApplicationLocale(getter_AddRefs(locale));
		if (NS_SUCCEEDED(rv) && locale)
		{
			nsCOMPtr <nsICollationFactory> factory;

			rv = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL,
									  kICollationFactoryIID, getter_AddRefs(factory)); 
			if (NS_SUCCEEDED(rv) && factory)
			{
				rv = factory->CreateCollation(locale, getter_AddRefs(m_collationKeyGenerator));
			}
		}
	}
	return rv;
}

  NS_IMETHODIMP nsAddrDatabase::CreateCollationKey(const PRUnichar* sourceStr, nsString& resultStr)
  {
        nsresult rv = GetCollationKeyGenerator();
        if (NS_SUCCEEDED(rv) && m_collationKeyGenerator)
       {
               nsAutoString sourceString(sourceStr);
               PRUint32 aLength;
               PRUint8 *aKey;
               rv = m_collationKeyGenerator->GetSortKeyLen(kCollationCaseInSensitive, sourceString, &aLength);
               if (NS_SUCCEEDED(rv))
               {
                       aKey = (PRUint8 *) PR_Malloc(aLength + 3);    // plus three for null termination
                       if (aKey) 
                       {
                               rv = m_collationKeyGenerator->CreateRawSortKey(kCollationCaseInSensitive, sourceString, aKey, &aLength);
                               if (NS_SUCCEEDED(rv))
                               {
                                       // Generate a null terminated unicode string.
                                       // Note using PRUnichar* to store collation key is not recommented since the key may contains 0x0000.
                                       aKey[aLength] = 0;
                                       aKey[aLength+1] = 0;
                                       aKey[aLength+2] = 0;
                                       resultStr.Assign((PRUnichar *) aKey);
                               }
                               else
                                       PR_Free(aKey);
                       }
               }
       }
        return rv;
  }

NS_IMETHODIMP nsAddrDatabase::GetDirectoryName(PRUnichar **name)
{
	if (m_dbDirectory && name)
	{
		return m_dbDirectory->GetDirName(name);
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::AddListDirNode(nsIMdbRow * listRow)
{
	nsresult rv = NS_OK;

	NS_WITH_SERVICE( nsIProxyObjectManager, proxyMgr, kProxyObjectManagerCID, &rv);
	if (NS_FAILED(rv)) {
		return rv;
	}
	NS_WITH_PROXIED_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, NS_UI_THREAD_EVENTQ, &rv);
	if (NS_SUCCEEDED(rv)) 
	{
		nsCOMPtr<nsIRDFResource>	parentResource;

		char* file = m_dbName.GetLeafName();
		char *parentUri = PR_smprintf("%s%s", kDirectoryDataSourceRoot, file);
		rv = rdfService->GetResource( parentUri, getter_AddRefs(parentResource));
		nsCOMPtr<nsIAbDirectory> parentDir;
		rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID( nsIAbDirectory),
									parentResource, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs( parentDir));
		if (parentDir)
		{
			m_dbDirectory = parentDir;
			nsCOMPtr<nsIAbDirectory> mailList;
			rv = CreateABList(listRow, getter_AddRefs(mailList));
			if (mailList)
				parentDir->NotifyDirItemAdded(mailList);
		}
		if (parentUri)
			PR_smprintf_free(parentUri);
		if (file)
			nsCRT::free(file);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::FindMailListbyUnicodeName(const PRUnichar *listName, PRBool *exist)
{
	nsresult rv = NS_ERROR_FAILURE;
	nsAutoString unicodeString(listName);
	unicodeString.ToLowerCase();
	char* pUTF8Str = unicodeString.ToNewUTF8String();
	if (pUTF8Str)
	{
		nsIMdbRow	*pListRow = nsnull;
		rv = GetRowForCharColumn(pUTF8Str, m_LowerListNameColumnToken, PR_FALSE, &pListRow);
		if (pListRow)
		{
			*exist = PR_TRUE;
			pListRow->CutStrongRef(GetEnv());
		}
		else
			*exist = PR_FALSE;
		Recycle(pUTF8Str);
	}
	return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetCardCount(PRUint32 *count)
{	
	mdb_err rv;
	mdb_count c;
	rv = m_mdbPabTable->GetCount(m_mdbEnv, &c);
	if(NS_SUCCEEDED(rv))
		*count = c;

	return rv;
}

/*
	"sourceString" should be a lowercase UTF8 string
*/
nsresult nsAddrDatabase::GetRowForCharColumn
(const char *lowerUTF8String, mdb_column findColumn, PRBool bIsCard, nsIMdbRow **findRow)
{
	if (lowerUTF8String)
	{
		mdbYarn	sourceYarn;

		sourceYarn.mYarn_Buf = (void *) lowerUTF8String;
		sourceYarn.mYarn_Fill = PL_strlen(lowerUTF8String);
		sourceYarn.mYarn_Form = 0;
		sourceYarn.mYarn_Size = sourceYarn.mYarn_Fill;

		mdbOid		outRowId;
		nsIMdbStore* store = GetStore();
		nsIMdbEnv* env = GetEnv();
		
		if (bIsCard)
			store->FindRow(env, m_CardRowScopeToken,
						   findColumn, &sourceYarn,  &outRowId, findRow);
		else
			store->FindRow(env, m_ListRowScopeToken,
						findColumn, &sourceYarn,  &outRowId, findRow);
		
		if (*findRow)
			return NS_OK;
	}	
	return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetRowForCharColumn
(const PRUnichar *unicodeStr, mdb_column findColumn, PRBool bIsCard, nsIMdbRow **findRow)
{
	nsresult rv = NS_ERROR_FAILURE;
	nsAutoString unicodeString(unicodeStr);
	unicodeString.ToLowerCase();
	char* pUTF8Str = unicodeString.ToNewUTF8String();
	if (pUTF8Str)
	{
		rv = GetRowForCharColumn(pUTF8Str, findColumn, bIsCard, findRow);
		Recycle(pUTF8Str);
	}
	return rv;
}

nsresult nsAddrDatabase::DeleteRow(nsIMdbTable* dbTable, nsIMdbRow* dbRow)
{
	nsresult err = NS_OK;

	err = dbRow->CutAllColumns(GetEnv());
	err = dbTable->CutRow(GetEnv(), dbRow);

	return err;
}
