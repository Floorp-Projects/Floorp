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

// this file implements the nsMsgDatabase interface using the MDB Interface.

#include "msgCore.h"
#include "nsMsgDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsNewsSet.h"
#include "nsIEnumerator.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsMsgThread.h"
#include "nsFileStream.h"
#include "nsString2.h"

#include "nsIMimeConverter.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleFactory.h"

#if defined(XP_MAC) && defined(CompareString)
	#undef CompareString
#endif
#include "nsICollation.h"

#include "nsCollationCID.h"
#include "nsIPref.h"


static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
  
static   NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
static NS_DEFINE_CID(kLocaleCID, NS_LOCALE_CID);
static NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);

#ifdef WE_HAVE_MDBINTERFACES
static NS_DEFINE_CID(kIMBBCID, NS_IMBB_IID);


	nsresult rv = nsComponentManager::CreateInstance(kMDBCID, nsnull, nsIMDB::GetIID(), (void **)&gMDBInterface);

	if (nsnull != devSpec){
	  if (NS_OK == ((nsDeviceContextSpecMac *)devSpec)->Init(aQuiet)){
	    aNewSpec = devSpec;
	    rv = NS_OK;
	  }
	}
#endif


const int kMsgDBVersion = 1;

#if 0
//Temporary function to create a URI so that nsMsgHdr functions with RDF.
//This will get removed.
static char* CreateURI(nsFileSpec path, nsMsgKey key)
{
	nsString uri = "mailmess://";
	uri+=path;

	char keyBuffer[50];
	PR_snprintf(keyBuffer, 50, "%u", key);

	uri+=keyBuffer;
	return uri.ToNewCString();
}
#endif

nsresult
nsMsgDatabase::CreateMsgHdr(nsIMdbRow* hdrRow, nsFileSpec& path, nsMsgKey key, nsIMessage* *result,
							PRBool getKeyFromHeader)
{
    nsresult rv;

	nsIRDFService *rdf;
	rv = nsServiceManager::GetService(kRDFServiceCID, 
										nsIRDFService::GetIID(), 
                                     (nsISupports**)&rdf);

    if (NS_FAILED(rv)) return rv;

	char* msgURI;

	//Need to remove ".msf".
	nsFileSpec folderPath = path;
	char* leafName = folderPath.GetLeafName();
	nsString folderName(leafName);
	PL_strfree(leafName);
	if(folderName.Find(".msf") != -1)
	{
		nsString realFolderName;
		folderName.Left(realFolderName, folderName.Length() - 4);
		folderPath.SetLeafName((const nsString)realFolderName);
	}

	rv = nsBuildLocalMessageURI(folderPath, key, &msgURI);
    if (NS_FAILED(rv)) return rv;


    nsIRDFResource* res;
    rv = rdf->GetResource(msgURI, &res);
    PR_smprintf_free(msgURI);
    if (NS_FAILED(rv)) return rv;
    
    nsMsgHdr* msgHdr = (nsMsgHdr*)res;
    msgHdr->Init(this, hdrRow);
    msgHdr->SetMessageKey(key);
    *result = msgHdr;
  
    nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

    return rv;
}

NS_IMETHODIMP nsMsgDatabase::AddListener(nsIDBChangeListener *listener)
{
    if (m_ChangeListeners == nsnull) {
        nsresult rv = NS_NewISupportsArray(&m_ChangeListeners);
        if (NS_FAILED(rv)) return rv;
    }
	return m_ChangeListeners->AppendElement(listener);
}

NS_IMETHODIMP nsMsgDatabase::RemoveListener(nsIDBChangeListener *listener)
{
    if (m_ChangeListeners == nsnull) return NS_OK;
	for (PRUint32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		if ((nsIDBChangeListener *) m_ChangeListeners->ElementAt(i) == listener)
		{
			m_ChangeListeners->RemoveElementAt(i);
			return NS_OK;
		}
	}
	return NS_COMFALSE;
}

	// change announcer methods - just broadcast to all listeners.
NS_IMETHODIMP nsMsgDatabase::NotifyKeyChangeAll(nsMsgKey keyChanged, PRInt32 flags, 
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull) return NS_OK;
	for (PRUint32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyChange(keyChanged, flags, instigator); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyKeyDeletedAll(nsMsgKey keyDeleted, PRInt32 flags, 
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull) return NS_OK;
	for (PRUint32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener = 
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyDeleted(keyDeleted, flags, instigator); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyKeyAddedAll(nsMsgKey keyAdded, PRInt32 flags, 
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull) return NS_OK;
	for (PRUint32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyAdded(keyAdded, flags, instigator); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyAnnouncerGoingAway(void)
{
    if (m_ChangeListeners == nsnull) return NS_OK;
	// run loop backwards because listeners remove themselves from the list 
	// on this notification
	for (int i = m_ChangeListeners->Count() - 1; i >= 0 ; i--)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnAnnouncerGoingAway(this); 
        if (NS_FAILED(rv)) return rv;
	}
    return NS_OK;
}



nsISupportsArray/*<nsMsgDatabase>*/ *nsMsgDatabase::m_dbCache = NULL;

//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------

nsISupportsArray/*<nsMsgDatabase>*/*
nsMsgDatabase::GetDBCache()
{
	if (!m_dbCache)
	{
		nsresult rv = NS_NewISupportsArray(&m_dbCache);
        NS_ASSERTION(rv == NS_OK, "fix the result type of GetDBCache");
	}
	return m_dbCache;
	
}

void
nsMsgDatabase::CleanupCache()
{
	if (m_dbCache) // clean up memory leak
	{
		for (PRUint32 i = 0; i < GetDBCache()->Count(); i++)
		{
			nsMsgDatabase* pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
			if (pMessageDB)
			{
				pMessageDB->ForceClosed();
				i--;	// back up array index, since closing removes db from cache.
			}
		}
		NS_ASSERTION(GetNumInCache() == 0, "some msg dbs left open");	// better not be any open db's.
		delete m_dbCache;
	}
	m_dbCache = NULL; // Need to reset to NULL since it's a
			  // static global ptr and maybe referenced 
			  // again in other places.
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
nsMsgDatabase* nsMsgDatabase::FindInCache(nsFileSpec &dbName)
{
	for (PRUint32 i = 0; i < GetDBCache()->Count(); i++)
	{
		nsMsgDatabase* pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
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
	for (PRUint32 i = 0; i < GetDBCache()->Count(); i++)
	{
		if (GetDBCache()->ElementAt(i) == pMessageDB)
		{
			return(i);
		}
	}
	return(-1);
}

PRBool nsMsgDatabase::MatchDbName(nsFileSpec &dbName)	// returns PR_TRUE if they match
{
	return (m_dbName == dbName); 
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void nsMsgDatabase::RemoveFromCache(nsMsgDatabase* pMessageDB)
{
	int i = FindInCache(pMessageDB);
	if (i != -1)
	{
		GetDBCache()->RemoveElementAt(i);
	}
}


#ifdef DEBUG
void nsMsgDatabase::DumpCache()
{
	for (PRUint32 i = 0; i < GetDBCache()->Count(); i++)
	{
		nsMsgDatabase* pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
	}
}
#endif /* DEBUG */

nsMsgDatabase::nsMsgDatabase()
    : m_dbFolderInfo(nsnull), m_mdbEnv(nsnull), m_mdbStore(nsnull),
      m_mdbAllMsgHeadersTable(nsnull), m_dbName(""), m_newSet(nsnull),
      m_mdbTokensInitialized(PR_FALSE), m_ChangeListeners(nsnull),
      m_hdrRowScopeToken(0),
      m_hdrTableKindToken(0),
      m_subjectColumnToken(0),
      m_senderColumnToken(0),
      m_messageIdColumnToken(0),
      m_referencesColumnToken(0),
      m_recipientsColumnToken(0),
      m_dateColumnToken(0),
      m_messageSizeColumnToken(0),
      m_flagsColumnToken(0),
      m_priorityColumnToken(0),
      m_statusOffsetColumnToken(0),
      m_numLinesColumnToken(0),
      m_ccListColumnToken(0)
{
	NS_INIT_REFCNT();
}

nsMsgDatabase::~nsMsgDatabase()
{
//	Close(FALSE);	// better have already been closed.
    if (m_ChangeListeners) {
        // better not be any listeners, because we're going away.
        NS_ASSERTION(m_ChangeListeners->Count() == 0, "shouldn't have any listeners");
        NS_RELEASE(m_ChangeListeners);
    }
}

NS_IMPL_ADDREF(nsMsgDatabase)

NS_IMETHODIMP nsMsgDatabase::Release(void)                    
{                                                      
  NS_PRECONDITION(0 != mRefCnt, "dup release");        
  if (--mRefCnt == 0) {                                
    NS_DELETEXPCOM(this);                              
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}

// NS_IMPL_RELEASE(nsMsgDatabase)

NS_IMETHODIMP nsMsgDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIMsgDatabase::GetIID()) ||
        aIID.Equals(nsIDBChangeAnnouncer::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIMsgDatabase*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

extern nsIMdbFactory *NS_NewIMdbFactory();

/* static */ nsIMdbFactory *nsMsgDatabase::GetMDBFactory()
{
	static nsIMdbFactory *gMDBFactory = NULL;
	if (!gMDBFactory)
	{
		gMDBFactory = MakeMdbFactory(); //new nsIMdbFactory;
#if 0
        nsComponentManager::CreateInstance(
#endif
	}
	return gMDBFactory;
}

#ifdef XP_PC
// this code is stolen from nsFileSpecWin. Since MDB requires a native path, for 
// the time being, we'll just take the Unix/Canonical form and munge it
void nsMsgDatabase::UnixToNative(char*& ioPath)
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
void nsMsgDatabase::UnixToNative(char*& ioPath)
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
		PR_Free(ioPath);
		ioPath = result;
	}
}

void nsMsgDatabase::NativeToUnix(char*& ioPath)
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

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
NS_IMETHODIMP nsMsgDatabase::OpenMDB(const char *dbName, PRBool create)
{
	nsresult ret = NS_OK;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb;
			struct stat st;
			char	*nativeFileName = nsCRT::strdup(dbName);

#if defined(XP_MAC)
			char * unixPath = nsCRT::strdup(dbName);
			NativeToUnix(unixPath);
			m_dbName = nsCRT::strdup(unixPath);
			delete [] unixPath;
#else
			m_dbName = nsCRT::strdup(dbName);
#endif
#if defined(XP_PC) || defined(XP_MAC)
			UnixToNative(nativeFileName);
#endif
			if (stat(nativeFileName, &st)) 
				ret = NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;
			else
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
#ifdef DEBUG_bienvenu
//				DumpContents();
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
			PR_FREEIF(nativeFileName);
		}
	}
	return ret;
}

NS_IMETHODIMP nsMsgDatabase::CloseMDB(PRBool commit)
{
	--mRefCnt; 
	PR_ASSERT(mRefCnt >= 0);
	if (mRefCnt == 0)
	{
		if (commit)
			Commit(kSessionCommit);
		RemoveFromCache(this);
#ifdef DEBUG_bienvenu1
		if (GetNumInCache() != 0)
		{
			XP_Trace("closing %s\n", m_dbName);
			DumpCache();
		}
#endif
		if (m_mdbStore)
			m_mdbStore->CloseMdbObject(m_mdbEnv);
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
NS_IMETHODIMP nsMsgDatabase::ForceClosed()
{
	nsresult	err = NS_OK;

	while (mRefCnt > 0 && NS_SUCCEEDED(err))
	{
		int32 saveUseCount = mRefCnt;
		err = CloseMDB(PR_TRUE);
		if (saveUseCount == 1)
			break;
	}
	return err;
}

NS_IMETHODIMP nsMsgDatabase::Commit(nsMsgDBCommitType commitType)
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
			err = m_mdbStore->CompressCommit(GetEnv(), &commitThumb);
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

NS_IMETHODIMP nsMsgDatabase::Close(PRBool forceCommit /* = TRUE */)
{
	return CloseMDB(forceCommit);
}

const char *kMsgHdrsScope = "ns:msg:db:row:scope:msgs:all";
const char *kMsgHdrsTableKind = "ns:msg:db:table:kind:msgs";
const char *kSubjectColumnName = "subject";
const char *kSenderColumnName = "sender";
const char *kMessageIdColumnName = "message-id";
const char *kReferencesColumnName = "references";
const char *kRecipientsColumnName = "recipients";
const char *kDateColumnName = "date";
const char *kMessageSizeColumnName = "size";
const char *kFlagsColumnName = "flags";
const char *kPriorityColumnName = "priority";
const char *kStatusOffsetColumnName = "statusOfset";
const char *kNumLinesColumnName = "numLines";
const char *kCCListColumnName = "ccList";

struct mdbOid gAllMsgHdrsTableOID;

// set up empty tables, dbFolderInfo, etc.
nsresult nsMsgDatabase::InitNewDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		nsDBFolderInfo *dbFolderInfo = new nsDBFolderInfo(this);
		if (dbFolderInfo)
		{
			err = dbFolderInfo->AddToNewMDB();
			dbFolderInfo->SetVersion(GetCurVersion());
			nsIMdbStore *store = GetStore();
			// create the unique table for the dbFolderInfo.
			mdb_err err = store->NewTable(GetEnv(), m_hdrRowScopeToken, 
				m_hdrTableKindToken, PR_FALSE, nsnull, &m_mdbAllMsgHeadersTable);
//			m_mdbAllMsgHeadersTable->BecomeContent(GetEnv(), &gAllMsgHdrsTableOID);
			m_dbFolderInfo = dbFolderInfo;

		}
		else
			err = NS_ERROR_OUT_OF_MEMORY;
	}
	return err;
}

nsresult nsMsgDatabase::InitExistingDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		err = GetStore()->GetTable(GetEnv(), &gAllMsgHdrsTableOID, &m_mdbAllMsgHeadersTable);
		if (err == NS_OK)
		{
			m_dbFolderInfo = new nsDBFolderInfo(this);
			if (m_dbFolderInfo)
				m_dbFolderInfo->InitFromExistingDB();
		}

	}
	return err;
}

// initialize the various tokens and tables in our db's env
nsresult nsMsgDatabase::InitMDBInfo()
{
	nsresult err = NS_OK;

	if (!m_mdbTokensInitialized && GetStore())
	{
		m_mdbTokensInitialized = PR_TRUE;
		err	= GetStore()->StringToToken(GetEnv(), kMsgHdrsScope, &m_hdrRowScopeToken); 
		if (err == NS_OK)
		{
			GetStore()->StringToToken(GetEnv(),  kSubjectColumnName, &m_subjectColumnToken);
			GetStore()->StringToToken(GetEnv(),  kSenderColumnName, &m_senderColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMessageIdColumnName, &m_messageIdColumnToken);
			// if we just store references as a string, we won't get any savings from the
			// fact there's a lot of duplication. So we may want to break them up into
			// multiple columns, r1, r2, etc.
			GetStore()->StringToToken(GetEnv(),  kReferencesColumnName, &m_referencesColumnToken);
			// similarly, recipients could be tokenized properties
			GetStore()->StringToToken(GetEnv(),  kRecipientsColumnName, &m_recipientsColumnToken);
			GetStore()->StringToToken(GetEnv(),  kDateColumnName, &m_dateColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMessageSizeColumnName, &m_messageSizeColumnToken);
			GetStore()->StringToToken(GetEnv(),  kFlagsColumnName, &m_flagsColumnToken);
			GetStore()->StringToToken(GetEnv(),  kPriorityColumnName, &m_priorityColumnToken);
			GetStore()->StringToToken(GetEnv(),  kStatusOffsetColumnName, &m_statusOffsetColumnToken);
			GetStore()->StringToToken(GetEnv(),  kNumLinesColumnName, &m_numLinesColumnToken);
			GetStore()->StringToToken(GetEnv(),  kCCListColumnName, &m_ccListColumnToken);
			
			err = GetStore()->StringToToken(GetEnv(), kMsgHdrsTableKind, &m_hdrTableKindToken); 
			if (err == NS_OK)
			{
				// The table of all message hdrs will have table id 1.
				gAllMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
				gAllMsgHdrsTableOID.mOid_Id = 1;
			}
		}
	}
	return err;
}

// get a message header for the given key. Caller must release()!
NS_IMETHODIMP nsMsgDatabase::GetMsgHdrForKey(nsMsgKey key, nsIMessage **pmsgHdr)
{
	nsresult	err = NS_OK;
	mdb_bool	hasOid;
	mdbOid		rowObjectId;


	if (!pmsgHdr || !m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;

	*pmsgHdr = NULL;
	rowObjectId.mOid_Id = key;
	rowObjectId.mOid_Scope = m_hdrRowScopeToken;
	err = m_mdbAllMsgHeadersTable->HasOid(GetEnv(), &rowObjectId, &hasOid);
	if (err == NS_OK && m_mdbStore /* && hasOid */)
	{
		nsIMdbRow *hdrRow;
#if 0
        nsIMdbTableRowCursor* rowCursor;                                        
        mdb_pos rowPos = -1;                                                            
                                                                                                                                             
        err = m_mdbAllMsgHeadersTable->GetTableRowCursor(GetEnv(), rowPos, &rowCursor);
                                                                                                                                             
        if (err == NS_OK && rowCursor /*rowPos >= 0*/) // ### is rowP os > 0 the right thing to check?
        {                                                                       
            do                                                                  
            {                                                                   
				nsIMdbRow   *hdrRow;                                            
				err = rowCursor->NextRow(GetEnv(), &hdrRow, &rowPos);
                if (!NS_SUCCEEDED(err) || rowPos < 0 || !hdrRow)             
					break; 
				mdbOid rowOid;
				err = hdrRow->GetOid(GetEnv(), &rowOid);
				if (NS_SUCCEEDED(err) && rowOid.mOid_Id == key)                                    
                {                                                               
                     err = CreateMsgHdr(hdrRow, m_dbName, key, pmsgHdr);         
                     break;                                                      
                 }                                                               
             }                                                                   
             while (TRUE);                                                       
             NS_RELEASE(rowCursor);                                              
		}
#else
		err = m_mdbStore->GetRow(GetEnv(), &rowObjectId, &hdrRow);

		if (err == NS_OK && hdrRow)
		{
			err = CreateMsgHdr(hdrRow, m_dbName, key, pmsgHdr);
		}
#endif
	}

	return err;
}

NS_IMETHODIMP nsMsgDatabase::DeleteMessage(nsMsgKey key, nsIDBChangeListener *instigator, PRBool commit)
{
	nsresult	err = NS_OK;
	nsIMessage *msgHdr = NULL;

	err = GetMsgHdrForKey(key, &msgHdr);
	if (msgHdr == NULL)
		return NS_MSG_MESSAGE_NOT_FOUND;

	return DeleteHeader(msgHdr, instigator, commit, PR_TRUE);
}


NS_IMETHODIMP nsMsgDatabase::DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator)
{
	nsresult	err = NS_OK;

	for (PRUint32 index = 0; index < nsMsgKeys->GetSize(); index++)
	{
		nsMsgKey key = nsMsgKeys->ElementAt(index);
		nsIMessage *msgHdr = NULL;
		
		err = GetMsgHdrForKey(key, &msgHdr);
        if (NS_FAILED(err)) 
		{
			err = NS_MSG_MESSAGE_NOT_FOUND;
			break;
		}
		err = DeleteHeader(msgHdr, instigator, index % 300 == 0, PR_TRUE);
		if (err != NS_OK)
			break;
	}
	Commit(kSmallCommit);
	return err;
}


NS_IMETHODIMP nsMsgDatabase::DeleteHeader(nsIMessage *msg, nsIDBChangeListener *instigator, PRBool commit, PRBool notify)
{
    nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, msg);  // closed system, so this is ok
	nsMsgKey key;
    (void)msg->GetMessageKey(&key);
	// only need to do this for mail - will this speed up news expiration? 
//	if (GetMailDB())
		SetHdrFlag(msg, PR_TRUE, MSG_FLAG_EXPUNGED);	// tell mailbox (mail)

	if (m_newSet)	// if it's in the new set, better get rid of it.
		m_newSet->Remove(key);

	if (m_dbFolderInfo != NULL)
	{
		PRBool isRead;
		m_dbFolderInfo->ChangeNumMessages(-1);
		m_dbFolderInfo->ChangeNumVisibleMessages(-1);
		IsRead(key, &isRead);
		if (!isRead)
			m_dbFolderInfo->ChangeNumNewMessages(-1);

        PRUint32 size;
        (void)msg->GetMessageSize(&size);
		m_dbFolderInfo->m_expungedBytes += size;

	}	

	if (notify) {
        PRUint32 flags;
        (void)msg->GetFlags(&flags);
		NotifyKeyDeletedAll(key, flags, instigator); // tell listeners
    }

//	if (!onlyRemoveFromThread)	// to speed up expiration, try this. But really need to do this in RemoveHeaderFromDB
		RemoveHeaderFromDB(msgHdr);
	if (commit)
		Commit(kSmallCommit);			// ### dmb is this a good time to commit?
	NS_RELEASE(msg);	// even though we're deleting it from the db, need to Release.
	return NS_OK;
}

NS_IMETHODIMP
nsMsgDatabase::UndoDelete(nsIMessage *msgHdr)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// This is a lower level routine which doesn't send notifcations or
// update folder info. One use is when a rule fires moving a header
// from one db to another, to remove it from the first db.

nsresult nsMsgDatabase::RemoveHeaderFromDB(nsMsgHdr *msgHdr)
{
	nsresult ret = NS_OK;
	ret = m_mdbAllMsgHeadersTable->CutRow(GetEnv(), msgHdr->GetMDBRow());
	return ret;
}

nsresult nsMsgDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
	nsresult rv;
	nsIMessage *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv) || !msgHdr) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?
    rv = IsHeaderRead(msgHdr, pRead);
    NS_RELEASE(msgHdr);
    return rv;
}

PRUint32	nsMsgDatabase::GetStatusFlags(nsIMessage *msgHdr)
{
	PRUint32	statusFlags;
    (void)msgHdr->GetFlags(&statusFlags);
	PRBool	isRead;

    nsMsgKey key;
    (void)msgHdr->GetMessageKey(&key);
	if (m_newSet && m_newSet->IsMember(key))
		statusFlags |= MSG_FLAG_NEW;
	if (IsRead(key, &isRead) == NS_OK && isRead)
		statusFlags |= MSG_FLAG_READ;
	return statusFlags;
}

NS_IMETHODIMP nsMsgDatabase::IsHeaderRead(nsIMessage *hdr, PRBool *pRead)
{
	if (!hdr)
		return NS_MSG_MESSAGE_NOT_FOUND;

    PRUint32 flags;
    (void)hdr->GetFlags(&flags);
	*pRead = (flags & MSG_FLAG_READ) != 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::IsMarked(nsMsgKey key, PRBool *pMarked)
{
	nsresult rv;
	nsIMessage *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pMarked = (flags & MSG_FLAG_MARKED) == MSG_FLAG_MARKED;
    NS_RELEASE(msgHdr);
    return rv;
}

NS_IMETHODIMP nsMsgDatabase::IsIgnored(nsMsgKey key, PRBool *pIgnored)
{
	PR_ASSERT(pIgnored != NULL);
	if (!pIgnored)
		return NS_ERROR_NULL_POINTER;
#ifdef WE_DO_THREADING_YET
	nsThreadMessageHdr *threadHdr = GetnsThreadHdrForMsgID(nsMsgKey);
	// This should be very surprising, but we leave that up to the caller
	// to determine for now.
	if (threadHdr == NULL)
		return NS_MSG_MESSAGE_NOT_FOUND;
	*pIgnored = (threadHdr->GetFlags() & MSG_FLAG_IGNORED) ? PR_TRUE : PR_FALSE;
	NS_RELEASE(threadHdr);
#endif
	return NS_OK;
}

nsresult nsMsgDatabase::HasAttachments(nsMsgKey key, PRBool *pHasThem)
{
	nsresult rv;

	PR_ASSERT(pHasThem != NULL);
	if (!pHasThem)
		return NS_ERROR_NULL_POINTER;

	nsIMessage *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return rv;

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pHasThem = (flags & MSG_FLAG_ATTACHMENT) ? PR_TRUE : PR_FALSE;
    NS_RELEASE(msgHdr);
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkHdrReadInDB(nsIMessage *msgHdr, PRBool bRead,
                                             nsIDBChangeListener *instigator)
{
    nsMsgKey key;
    (void)msgHdr->GetMessageKey(&key);
	SetHdrFlag(msgHdr, bRead, MSG_FLAG_READ);
	if (m_newSet)
		m_newSet->Remove(key);
	if (m_dbFolderInfo != NULL)
	{
		if (bRead)
			m_dbFolderInfo->ChangeNumNewMessages(-1);
		else
			m_dbFolderInfo->ChangeNumNewMessages(1);
	}

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
	return NotifyKeyChangeAll(key, flags, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkRead(nsMsgKey key, PRBool bRead, 
						   nsIDBChangeListener *instigator)
{
	nsresult rv;
	nsIMessage *msgHdr;
	
	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

	rv = MarkHdrRead(msgHdr, bRead, instigator);
	NS_RELEASE(msgHdr);
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkReplied(nsMsgKey key, PRBool bReplied, 
								nsIDBChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(key, bReplied, MSG_FLAG_REPLIED, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkForwarded(nsMsgKey key, PRBool bForwarded, 
								nsIDBChangeListener *instigator /* = NULL */) 
{
	return SetKeyFlag(key, bForwarded, MSG_FLAG_FORWARDED, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkHasAttachments(nsMsgKey key, PRBool bHasAttachments, 
								nsIDBChangeListener *instigator)
{
	return SetKeyFlag(key, bHasAttachments, MSG_FLAG_ATTACHMENT, instigator);
}

NS_IMETHODIMP
nsMsgDatabase::MarkThreadIgnored(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bIgnored,
                                 nsIDBChangeListener *instigator)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgDatabase::MarkThreadWatched(nsThreadMessageHdr *thread, nsMsgKey threadKey, PRBool bWatched,
                                 nsIDBChangeListener *instigator)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDatabase::MarkMarked(nsMsgKey key, PRBool mark,
										nsIDBChangeListener *instigator)
{
	return SetKeyFlag(key, mark, MSG_FLAG_MARKED, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkOffline(nsMsgKey key, PRBool offline,
										nsIDBChangeListener *instigator)
{
	return SetKeyFlag(key, offline, MSG_FLAG_OFFLINE, instigator);
}

NS_IMETHODIMP
nsMsgDatabase::AllMsgKeysImapDeleted(const nsMsgKeyArray *keys)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDatabase::MarkImapDeleted(nsMsgKey key, PRBool deleted,
										nsIDBChangeListener *instigator)
{
	return SetKeyFlag(key, deleted, MSG_FLAG_IMAP_DELETED, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkMDNNeeded(nsMsgKey key, PRBool bNeeded, 
								nsIDBChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(key, bNeeded, MSG_FLAG_MDN_REPORT_NEEDED, instigator);
}

NS_IMETHODIMP nsMsgDatabase::IsMDNNeeded(nsMsgKey key, PRBool *pNeeded)
{
	nsresult rv;
	nsIMessage *msgHdr;
	
    rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pNeeded = ((flags & MSG_FLAG_MDN_REPORT_NEEDED) == MSG_FLAG_MDN_REPORT_NEEDED);
    NS_RELEASE(msgHdr);
    return rv;
}


nsresult nsMsgDatabase::MarkMDNSent(nsMsgKey key, PRBool bSent, 
							  nsIDBChangeListener *instigator /* = NULL */)
{
	return SetKeyFlag(key, bSent, MSG_FLAG_MDN_REPORT_SENT, instigator);
}


nsresult nsMsgDatabase::IsMDNSent(nsMsgKey key, PRBool *pSent)
{
	nsresult rv;
	nsIMessage *msgHdr;
	
	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pSent = flags & MSG_FLAG_MDN_REPORT_SENT;
    NS_RELEASE(msgHdr);
    return rv;
}


nsresult	nsMsgDatabase::SetKeyFlag(nsMsgKey key, PRBool set, PRInt32 flag,
							  nsIDBChangeListener *instigator)
{
	nsresult rv;
	nsIMessage *msgHdr;
		
    rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

	SetHdrFlag(msgHdr, set, flag);

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
	NotifyKeyChangeAll(key, flags, instigator);

	NS_RELEASE(msgHdr);
	return rv;
}

// Helper routine - lowest level of flag setting - returns PR_TRUE if flags change,
// PR_FALSE otherwise.
PRBool nsMsgDatabase::SetHdrFlag(nsIMessage *msgHdr, PRBool bSet, MsgFlags flag)
{
//	PR_ASSERT(! (flag & kDirty));	// this won't do the right thing so don't.
	PRUint32 currentStatusFlags = GetStatusFlags(msgHdr);
	PRBool flagAlreadySet = (currentStatusFlags & flag) != 0;

	if ((flagAlreadySet && !bSet) || (!flagAlreadySet && bSet))
	{
        PRUint32 resultFlags;
		if (bSet)
		{
			msgHdr->OrFlags(flag, &resultFlags);
		}
		else
		{
			msgHdr->AndFlags(~flag, &resultFlags);
		}
		return PR_TRUE;
	}
	return PR_FALSE;
}


NS_IMETHODIMP nsMsgDatabase::MarkHdrRead(nsIMessage *msgHdr, PRBool bRead, 
                                         nsIDBChangeListener *instigator)
{
    nsresult rv = NS_OK;
	PRBool	isRead;
	IsHeaderRead(msgHdr, &isRead);
	// if the flag is already correct in the db, don't change it
	if (!!isRead != !!bRead)
	{
#ifdef WE_DO_THREADING_YET
		nsThreadMessageHdr *threadHdr = GetnsThreadHdrForMsgID(msgHdr->GetMessageKey());
		if (threadHdr != NULL)
		{
			threadHdr->MarkChildRead(bRead);
			NS_RELEASE(threadHdr);
		}
#endif
		rv = MarkHdrReadInDB(msgHdr, bRead, instigator);
	}
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkAllRead(nsMsgKeyArray *thoseMarked)
{
	nsresult		rv;
	nsMsgHdr		*pHeader;
	ListContext		*listContext = NULL;
	PRInt32			numChanged = 0;

    nsIEnumerator* hdrs;
    rv = EnumerateMessages(&hdrs);
    if (NS_FAILED(rv)) return rv;
	for (hdrs->First(); hdrs->IsDone() != NS_OK; hdrs->Next()) {
        rv = hdrs->CurrentItem((nsISupports**)&pHeader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (NS_FAILED(rv)) break;

		if (thoseMarked) {
            nsMsgKey key;
            (void)pHeader->GetMessageKey(&key);
			thoseMarked->Add(key);
        }
		rv = MarkHdrRead(pHeader, PR_TRUE, NULL); 	// ### dmb - blow off error?
		numChanged++;
		NS_RELEASE(pHeader);
	}

	if (numChanged > 0)	// commit every once in a while
		Commit(kSmallCommit);
	// force num new to 0.
	PRInt32 numNewMessages;

	rv = m_dbFolderInfo->GetNumNewMessages(&numNewMessages);
	if (rv == NS_OK)
		m_dbFolderInfo->ChangeNumNewMessages(-numNewMessages);
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkReadByDate (time_t startDate, time_t endDate, nsMsgKeyArray *markedIds)
{
	nsresult rv;
	nsMsgHdr	*pHeader;
	ListContext		*listContext = NULL;
	PRInt32			numChanged = 0;

    nsIEnumerator* hdrs;
    rv = EnumerateMessages(&hdrs);
    if (NS_FAILED(rv)) return rv;
	for (hdrs->First(); hdrs->IsDone() != NS_OK; hdrs->Next()) {
        rv = hdrs->CurrentItem((nsISupports**)&pHeader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (NS_FAILED(rv)) break;

		time_t headerDate;
        (void)pHeader->GetDate(&headerDate);
		if (headerDate > startDate && headerDate <= endDate)
		{
			PRBool isRead;
            nsMsgKey key;
            (void)pHeader->GetMessageKey(&key);
			IsRead(key, &isRead);
			if (!isRead)
			{
				numChanged++;
				if (markedIds)
					markedIds->Add(key);
				rv = MarkHdrRead(pHeader, PR_TRUE, NULL);	// ### dmb - blow off error?
			}
		}
		NS_RELEASE(pHeader);
	}
	if (numChanged > 0)
		Commit(kSmallCommit);
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkLater(nsMsgKey key, time_t *until)
{
	PR_ASSERT(m_dbFolderInfo);
	if (m_dbFolderInfo != NULL)
	{
		m_dbFolderInfo->AddLaterKey(key, until);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::AddToNewList(nsMsgKey key)
{
	if (m_newSet == NULL)
		m_newSet = nsNewsSet::Create();
	if (m_newSet)
		m_newSet->Add(key);
	return (m_newSet) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP nsMsgDatabase::ClearNewList(PRBool notify /* = FALSE */)
{
	nsresult			err = NS_OK;
	if (m_newSet)
	{
		if (notify)	// need to update view
		{
			PRInt32 firstMember;
			while ((firstMember = m_newSet->GetFirstMember()) != 0)
			{
				m_newSet->Remove(firstMember);	// this bites, since this will cause us to regen new list many times.
				nsIMessage *msgHdr;
				err = GetMsgHdrForKey(firstMember, &msgHdr);
				if (NS_SUCCEEDED(err))
				{
                    nsMsgKey key;
                    (void)msgHdr->GetMessageKey(&key);
                    PRUint32 flags;
                    (void)msgHdr->GetFlags(&flags);
					NotifyKeyChangeAll(key, flags, NULL);
					NS_RELEASE(msgHdr);
				}
			}
		}
		delete m_newSet;
		m_newSet = NULL;
	}
    return err;
}

NS_IMETHODIMP nsMsgDatabase::HasNew()
{
	return (m_newSet && m_newSet->getLength() > 0) ? NS_OK : NS_COMFALSE;
}

NS_IMETHODIMP nsMsgDatabase::GetFirstNew(nsMsgKey *result)
{
	// even though getLength is supposedly for debugging only, it's the only
	// way I can tell if the set is empty (as opposed to having a member 0.
	if (HasNew() == NS_OK)
		*result = m_newSet->GetFirstMember();
	else
		*result = nsMsgKey_None;
    return NS_OK;
}


#if 0
// header iteration methods.

class ListContext
{
public:
	ListContext();
	virtual ~ListContext();
	nsIMdbTableRowCursor *m_rowCursor;
};

ListContext::ListContext()
{
	m_rowCursor = NULL;
}

ListContext::~ListContext()
{
	if (m_rowCursor)
	{
		NS_RELEASE(m_rowCursor);
	}
}

nsresult	nsMsgDatabase::ListFirst(ListContext **ppContext, nsMsgHdr **pResultHdr)
{
	nsresult	err = NS_OK;

	if (!pResultHdr || !ppContext)
		return NS_ERROR_NULL_POINTER;

	*pResultHdr = NULL;
	*ppContext = NULL;

	ListContext *pContext = new ListContext;
	if (!pContext)
		return NS_ERROR_OUT_OF_MEMORY;

	err = m_mdbAllMsgHeadersTable->GetTableRowCursor(GetEnv(), -1, &pContext->m_rowCursor);
	if (err == NS_OK)
	{
		*ppContext = pContext;

		err = ListNext(pContext, pResultHdr);
	}
	return err;
}

nsresult	nsMsgDatabase::ListNext(ListContext *pContext, nsMsgHdr **pResultHdr)
{
	nsresult	err = NS_OK;
	nsIMdbRow	*hdrRow;
	mdb_pos		rowPos;

	if (!pResultHdr || !pContext)
		return NS_ERROR_NULL_POINTER;

	*pResultHdr = NULL;
	err = pContext->m_rowCursor->NextRow(GetEnv(), &hdrRow, &rowPos);
    if (NS_FAILED(err)) return err;

	//Get key from row
	mdbOid outOid;
	nsMsgKey key;
	if (hdrRow->GetOid(GetEnv(), &outOid) == NS_OK)
		key = outOid.mOid_Id;

    err = CreateMsgHdr(hdrRow, m_dbName, key, pResultHdr, PR_TRUE);
	return err;
}

nsresult	nsMsgDatabase::ListDone(ListContext *pContext)
{
	nsresult	err = NS_OK;
	delete pContext;
	return err;
}
#else
////////////////////////////////////////////////////////////////////////////////

class nsMsgDBEnumerator : public nsIEnumerator {
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_IMETHOD First(void);
    NS_IMETHOD Next(void);
    NS_IMETHOD CurrentItem(nsISupports **aItem);
    NS_IMETHOD IsDone(void);

    // nsMsgDBEnumerator methods:
    typedef nsresult (*nsMsgDBEnumeratorFilter)(nsIMessage* hdr, void* closure);

    nsMsgDBEnumerator(nsMsgDatabase* db, 
                      nsMsgDBEnumeratorFilter filter, void* closure);
    virtual ~nsMsgDBEnumerator();

protected:
    nsMsgDatabase*              mDB;
	nsIMdbTableRowCursor*       mRowCursor;
    nsIMessage*                 mResultHdr;
    PRBool                      mDone;
    nsMsgDBEnumeratorFilter     mFilter;
    void*                       mClosure;
};

nsMsgDBEnumerator::nsMsgDBEnumerator(nsMsgDatabase* db,
                                     nsMsgDBEnumeratorFilter filter, void* closure)
    : mDB(db), mRowCursor(nsnull), mResultHdr(nsnull), mDone(PR_FALSE),
      mFilter(filter), mClosure(closure)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDB);
}

nsMsgDBEnumerator::~nsMsgDBEnumerator()
{
    NS_RELEASE(mDB);
}

NS_IMPL_ISUPPORTS(nsMsgDBEnumerator, nsIEnumerator::GetIID())

NS_IMETHODIMP nsMsgDBEnumerator::First(void)
{
	nsresult rv = 0;

	if (!mDB || !mDB->m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;
		
	mDB->m_mdbAllMsgHeadersTable->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
	if (NS_FAILED(rv)) return rv;
    return Next();
}

NS_IMETHODIMP nsMsgDBEnumerator::Next(void)
{
	nsresult rv;
	nsIMdbRow* hdrRow;
	mdb_pos rowPos;

    do {
        NS_IF_RELEASE(mResultHdr);
        mResultHdr = nsnull;
        rv = mRowCursor->NextRow(mDB->GetEnv(), &hdrRow, &rowPos);
		if (!hdrRow) {
            mDone = PR_TRUE;
			return NS_RDF_CURSOR_EMPTY;
        }
        if (NS_FAILED(rv)) {
            mDone = PR_TRUE;
            return rv;
        }
		//Get key from row
		mdbOid outOid;
		nsMsgKey key;
		if (hdrRow->GetOid(mDB->GetEnv(), &outOid) == NS_OK)
		key = outOid.mOid_Id;

        rv = mDB->CreateMsgHdr(hdrRow, mDB->m_dbName, key, &mResultHdr, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    } while (mFilter && mFilter(mResultHdr, mClosure) != NS_OK);
	return rv;
}

NS_IMETHODIMP nsMsgDBEnumerator::CurrentItem(nsISupports **aItem)
{
    if (mResultHdr) {
        *aItem = mResultHdr;
        NS_ADDREF(mResultHdr);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgDBEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_COMFALSE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMsgDatabase::EnumerateMessages(nsIEnumerator* *result)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsnull, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}
#endif

#if HAVE_INT_ENUMERATORS
NS_IMETHODIMP nsMsgDatabase::EnumerateKeys(nsIEnumerator* *result)
{
    nsISupportsArray* keys;
    nsresult rv = NS_NewISupportsArray(&keys);
    if (NS_FAILED(rv)) return rv;

	nsIMdbTableRowCursor *rowCursor;
	rv = m_mdbAllMsgHeadersTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

	while (NS_SUCCEEDED(rv)) {
		mdbOid outOid;
		mdb_pos	outPos;

		rv = rowCursor->NextRowOid(GetEnv(), &outOid, &outPos);
        if (NS_FAILED(rv)) return rv;
		if (outPos < 0)	// is this right?
			break;
        keys->AppendElement(outOid.mOid_Id);
	}
	return keys->Enumerate(result);
}
#else
NS_IMETHODIMP nsMsgDatabase::ListAllKeys(nsMsgKeyArray &outputKeys)
{
	nsresult	err = NS_OK;
	nsIMdbTableRowCursor *rowCursor;
	if (m_mdbAllMsgHeadersTable)
	{
		err = m_mdbAllMsgHeadersTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
		while (err == NS_OK && rowCursor)
		{
			mdbOid outOid;
			mdb_pos	outPos;

			err = rowCursor->NextRowOid(GetEnv(), &outOid, &outPos);
			// is this right? Mork is returning a 0 id, but that should valid.
			if (outPos < 0 || outOid.mOid_Id == -1)	
				break;
			if (err == NS_OK)
				outputKeys.Add(outOid.mOid_Id);
		}
	}
	return err;
}
#endif

#if 0
// convenience function to iterate through a db only looking for unread messages.
// It turns out to be possible to do this more efficiently in news, since read/unread
// status is kept in a much more compact format (the newsrc format).
// So this is the base implementation, which news databases override.
nsresult nsMsgDatabase::ListNextUnread(ListContext **pContext, nsMsgHdr **pResult)
{
	nsMsgHdr	*pHeader;
	nsresult			dbErr = NS_OK;
	PRBool			lastWasRead = TRUE;
	*pResult = NULL;

	while (PR_TRUE)
	{
		if (*pContext == NULL)
			dbErr = ListFirst (pContext, &pHeader);
		else
			dbErr = ListNext(*pContext, &pHeader);

		if (dbErr != NS_OK)
		{
			ListDone(*pContext);
			break;
		}

		else if (dbErr != NS_OK)	 
			break;
		if (IsHeaderRead(pHeader, &lastWasRead) == NS_OK && !lastWasRead)
			break;
		else
			NS_RELEASE(pHeader);
	}
	if (!lastWasRead)
		*pResult = pHeader;
	return dbErr;
}
#else
static nsresult
nsMsgUnreadFilter(nsIMessage* msg, void* closure)
{
    nsMsgDatabase* db = (nsMsgDatabase*)closure;
    PRBool wasRead;
    nsresult rv = db->IsHeaderRead(msg, &wasRead);
    if (NS_FAILED(rv)) return rv;
    return !wasRead ? NS_OK : NS_COMFALSE;
}

NS_IMETHODIMP 
nsMsgDatabase::EnumerateUnreadMessages(nsIEnumerator* *result)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsMsgUnreadFilter, this);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}
#endif

NS_IMETHODIMP nsMsgDatabase::CreateNewHdr(nsMsgKey key, nsIMessage **pnewHdr)
{
	nsresult	err = NS_OK;
	nsIMdbRow		*hdrRow;
	struct mdbOid allMsgHdrsTableOID;

	if (!pnewHdr || !m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;

	allMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
	allMsgHdrsTableOID.mOid_Id = key;	// presumes 0 is valid key value

	err  = GetStore()->NewRowWithOid(GetEnv(),
                                     &allMsgHdrsTableOID, &hdrRow);
	if (NS_FAILED(err)) return err;
    err = CreateMsgHdr(hdrRow, m_dbName, key, pnewHdr);
	return err;
}

NS_IMETHODIMP nsMsgDatabase::AddNewHdrToDB(nsIMessage *newHdr, PRBool notify)
{
    nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, newHdr);          // closed system, cast ok
	nsresult err = m_mdbAllMsgHeadersTable->AddRow(GetEnv(), hdr->GetMDBRow());
	if (NS_SUCCEEDED(err))
	{
		nsMsgKey key;
		PRUint32 flags;

	    newHdr->GetMessageKey(&key);
		newHdr->GetFlags(&flags);
		if (flags & MSG_FLAG_NEW)
		{
			PRUint32 newFlags;
			newHdr->AndFlags(~MSG_FLAG_NEW, &newFlags);	// make sure not filed out
			AddToNewList(key);
		}
		if (m_dbFolderInfo != NULL)
		{
			m_dbFolderInfo->ChangeNumMessages(1);
			m_dbFolderInfo->ChangeNumVisibleMessages(1);
			if (! (flags & MSG_FLAG_READ))
				m_dbFolderInfo->ChangeNumNewMessages(1);
		}
		PRBool newThread;
#if 0
		err = ThreadNewHdr(hdr, newThread);
#endif
		if (notify)
		{
			NotifyKeyAddedAll(key, flags, NULL);
		}
	}
	return err;
}

NS_IMETHODIMP nsMsgDatabase::CreateNewHdrAndAddToDB(PRBool *newThread, nsMsgHdrStruct *hdrStruct, 
                                                    nsIMessage **pnewHdr, PRBool notify /* = FALSE */)
{
	nsresult	err = NS_OK;
	nsIMdbRow		*hdrRow;
	struct mdbOid allMsgHdrsTableOID;

	if (!pnewHdr || !m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;

	allMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
	allMsgHdrsTableOID.mOid_Id = hdrStruct->m_messageKey;

	err  = GetStore()->NewRowWithOid(GetEnv(), 
		&allMsgHdrsTableOID, &hdrRow);

	// add the row to the singleton table.
	if (NS_SUCCEEDED(err) && hdrRow)
	{
		struct mdbYarn yarn;
		char	int32StrBuf[20];

		yarn.mYarn_Grow = NULL;
		hdrRow->AddColumn(GetEnv(),  m_subjectColumnToken, nsStringToYarn(&yarn, &hdrStruct->m_subject));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString
		
		hdrRow->AddColumn(GetEnv(),  m_senderColumnToken, nsStringToYarn(&yarn, &hdrStruct->m_author));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString

		hdrRow->AddColumn(GetEnv(),  m_messageIdColumnToken, nsStringToYarn(&yarn, &hdrStruct->m_messageId));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString

		hdrRow->AddColumn(GetEnv(),  m_referencesColumnToken, nsStringToYarn(&yarn, &hdrStruct->m_references));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString

		hdrRow->AddColumn(GetEnv(),  m_recipientsColumnToken, nsStringToYarn(&yarn, &hdrStruct->m_recipients));
		delete[] yarn.mYarn_Buf;	// won't need this when we have nsCString

		yarn.mYarn_Buf = int32StrBuf;
		yarn.mYarn_Size = sizeof(int32StrBuf);
		yarn.mYarn_Fill = sizeof(int32StrBuf);

		hdrRow->AddColumn(GetEnv(),  m_dateColumnToken, UInt32ToYarn(&yarn, hdrStruct->m_date));

		hdrRow->AddColumn(GetEnv(),  m_messageSizeColumnToken, UInt32ToYarn(&yarn, hdrStruct->m_messageSize));

		hdrRow->AddColumn(GetEnv(),  m_flagsColumnToken, UInt32ToYarn(&yarn, hdrStruct->m_flags));

		hdrRow->AddColumn(GetEnv(),  m_priorityColumnToken, UInt32ToYarn(&yarn, hdrStruct->m_priority));

		err = m_mdbAllMsgHeadersTable->AddRow(GetEnv(), hdrRow);
	}
	if (NS_FAILED(err)) return err;

    err = CreateMsgHdr(hdrRow, m_dbName, hdrStruct->m_messageKey, pnewHdr);
	if (NS_SUCCEEDED(err))
		err = AddNewHdrToDB(*pnewHdr, notify);
	return err;
}

	// extract info from an nsMsgHdr into a MessageHdrStruct
NS_IMETHODIMP nsMsgDatabase::GetMsgHdrStructFromnsMsgHdr(nsIMessage *msg, nsMsgHdrStruct *hdrStruct)
{
	nsresult	err = NS_OK;
    nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, msg);          // closed system, cast ok

	if (msgHdr)
	{
		err = RowCellColumnTonsString(msgHdr->GetMDBRow(), m_subjectColumnToken, hdrStruct->m_subject);
		err = RowCellColumnTonsString(msgHdr->GetMDBRow(), m_senderColumnToken, hdrStruct->m_author);
		err = RowCellColumnTonsString(msgHdr->GetMDBRow(), m_messageIdColumnToken, hdrStruct->m_messageId);
		err = RowCellColumnTonsString(msgHdr->GetMDBRow(), m_referencesColumnToken, hdrStruct->m_references);
		err = RowCellColumnTonsString(msgHdr->GetMDBRow(), m_recipientsColumnToken, hdrStruct->m_recipients);
		err = RowCellColumnToUInt32(msgHdr->GetMDBRow(), m_messageSizeColumnToken, &hdrStruct->m_messageSize);
        nsMsgKey key;
        (void)msgHdr->GetMessageKey(&key);
		hdrStruct->m_messageKey = key;
	}
	return err;
}

NS_IMETHODIMP nsMsgDatabase::CopyHdrFromExistingHdr(nsMsgKey key, nsIMessage *existingHdr, nsIMessage **newHdr)
{
	nsresult	err = NS_OK;

	if (existingHdr)
	{
	    nsMsgHdr* sourceMsgHdr = NS_STATIC_CAST(nsMsgHdr*, existingHdr);      // closed system, cast ok
		nsMsgHdr *destMsgHdr = nsnull;
#ifdef MDB_DOES_CELL_CURSORS_OR_COPY_ROW
		CreateNewHdr(key, &destMsgHdr);
		nsIMdbRow	sourceRow = sourceMsgHdr->GetMDBRow() ;
		{
			nsIMdbRowCellCursor* cellCursor = nsnull;
			mdb_err res = sourceRow->GetRowCellCursor(GetEnv(), -1, &cellCursor) ; // acquire new cursor instance
			if (res == 0 && cellCursor)
			{
				do
				{
					nsIMdbCell ioCell;
					mdb_column outColumn;
					mdb_pos outPos;

					res = cellCursor->NextCell(GetEnv(), &ioCell, &outColumn, &outPos);
    
				}
				while (outPos >= 0 && res == 0);
			}
		}
#else
		nsMsgHdrStruct hdrStruct;
		err = GetMsgHdrStructFromnsMsgHdr(existingHdr, &hdrStruct);
		if (NS_SUCCEEDED(err))
		{
			PRBool newThread;
			hdrStruct.m_messageKey = key;
			err = CreateNewHdrAndAddToDB(&newThread, &hdrStruct, newHdr, PR_FALSE);
		}
#endif // MDB_DOES_CELL_CURSORS_OR_COPY_ROW
	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnTonsString(nsIMdbRow *hdrRow, mdb_token columnToken, nsString &resultStr)
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
			YarnTonsString(&yarn, &resultStr);
		}
	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnToMime2EncodedString(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr)
{
	nsresult err;
	nsString nakedString;
	err = RowCellColumnTonsString(row, columnToken, nakedString);
	if (NS_SUCCEEDED(err) && nakedString.Length() > 0)
	{
 
		// apply mime decode
		nsIMimeConverter *converter;
		err = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                            nsIMimeConverter::GetIID(), (void **)&converter);

		if (NS_SUCCEEDED(err) && nsnull != converter) 
		{
			nsString charset;
			nsString decodedStr;
			err = converter->DecodeMimePartIIStr(nakedString, charset, resultStr);
			NS_RELEASE(converter);
		}
	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnToCollationKey(nsIMdbRow *row, mdb_token columnToken, nsString &resultStr)
{
	nsString nakedString;
	nsresult err;

	err = RowCellColumnTonsString(row, columnToken, nakedString);
	if (NS_SUCCEEDED(err))
	{
		nsILocaleFactory* localeFactory; 
		nsILocale* locale; 
		nsString localeName; 

		// get a locale factory 
		err = nsComponentManager::FindFactory(kLocaleFactoryCID, (nsIFactory**)&localeFactory); 

		// do this for a new db if no UI to be provided for locale selection 
		err = localeFactory->GetApplicationLocale(&locale); 

		// or generate a locale from a stored locale name ("en_US", "fr_FR") 
		err = localeFactory->NewLocale(&localeName, &locale); 

		// and locale name can be taken as below, category should be one of the following 
		// probably NSILOCALE_COLLATE is appropriate 
		nsString catagory = "NSILOCALE_COLLATE"; 
		err = locale->GetCatagory(&catagory, &localeName); 

		nsICollationFactory *f;

		err = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL,
								kICollationFactoryIID, (void**) &f); 

		nsICollation *inst;

		// get a collation interface instance 
		err = f->CreateCollation(locale, &inst); 

		if (NS_SUCCEEDED(err) && inst)
		{
			err = inst->CreateSortKey( kCollationCaseSensitive, nakedString, resultStr) ;
		}
 	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnToUInt32(nsIMdbRow *hdrRow, mdb_token columnToken, PRUint32 &uint32Result)
{
	return RowCellColumnToUInt32(hdrRow, columnToken, &uint32Result);
}

nsresult nsMsgDatabase::RowCellColumnToUInt32(nsIMdbRow *hdrRow, mdb_token columnToken, PRUint32 *uint32Result)
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
			YarnToUInt32(&yarn, uint32Result);
		}
	}
	return err;
}


/* static */struct mdbYarn *nsMsgDatabase::nsStringToYarn(struct mdbYarn *yarn, nsString *str)
{
	yarn->mYarn_Buf = str->ToNewCString();
	yarn->mYarn_Size = PL_strlen((const char *) yarn->mYarn_Buf) + 1;
	yarn->mYarn_Fill = yarn->mYarn_Size - 1;
	yarn->mYarn_Form = 0;	// what to do with this? we're storing csid in the msg hdr...
	return yarn;
}

/* static */struct mdbYarn *nsMsgDatabase::UInt32ToYarn(struct mdbYarn *yarn, PRUint32 i)
{
	PR_snprintf((char *) yarn->mYarn_Buf, yarn->mYarn_Size, "%lx", i);
	yarn->mYarn_Fill = PL_strlen((const char *) yarn->mYarn_Buf);
	yarn->mYarn_Form = 0;	// what to do with this? Should be parsed out of the mime2 header?
	return yarn;
}

/* static */void nsMsgDatabase::YarnTonsString(struct mdbYarn *yarn, nsString *str)
{
	str->SetString((const char *) yarn->mYarn_Buf, yarn->mYarn_Fill);
}

/* static */void nsMsgDatabase::YarnToUInt32(struct mdbYarn *yarn, PRUint32 *pResult)
{
	PRUint32 result;
	char *p = (char *) yarn->mYarn_Buf;
	PRInt32 numChars = MIN(8, yarn->mYarn_Fill);
	PRInt32 i;
	for (i=0, result = 0; i<numChars; i++, p++)
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

PRUint32 nsMsgDatabase::GetCurVersion()
{
	return kMsgDBVersion;
}

nsresult nsMsgDatabase::SetSummaryValid(PRBool valid /* = PR_TRUE */)
{
	// setting the version to -1 ought to make it pretty invalid.
	if (!valid)
		m_dbFolderInfo->SetVersion(-1);

	// for default db (and news), there's no nothing to set to make it it valid
	return NS_OK;
}

// protected routines

nsMsgThread *nsMsgDatabase::GetThreadForReference(const char * msgID)
{
	nsMsgHdr	*msgHdr = GetMsgHdrForMessageID(msgID);  
	nsMsgThread *thread = NULL;

	if (msgHdr != NULL)
	{
		// find thread header for header whose message id we matched.
		thread = GetThreadForMsgKey(msgHdr->m_messageKey);
		msgHdr->Release();
	}
	return thread;
}

nsMsgThread *	nsMsgDatabase::GetThreadForSubject(const char * subject)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}

nsresult nsMsgDatabase::ThreadNewHdr(nsMsgHdr* newHdr, PRBool &newThread)
{
	nsresult result;
	nsMsgThread *thread = nsnull;
	nsMsgKey threadId = nsMsgKey_None;

	if (!newHdr)
		return NS_ERROR_NULL_POINTER;

	PRUint16 numReferences = 0;
	PRUint32 newHdrFlags = 0;

	newHdr->GetFlags(&newHdrFlags);
	newHdr->GetNumReferences(&numReferences);

#define SUBJ_THREADING 1// try reference threading first
	for (PRInt32 i = 0; i < numReferences; i++)
	{
		nsMsgThread *thread = nsnull;
		nsString2 reference;

		newHdr->GetStringReference(i, reference);
		// first reference we have hdr for is best top-level hdr.
		// but we have to handle case of promoting new header to top-level
		// in case the top-level header comes after a reply.

		if (reference.Length() == 0)
			break;

		if ((thread = GetThreadForReference(reference.GetBuffer())) != NULL)
		{
			thread->GetThreadKey(&threadId);
			newHdr->SetThreadId(threadId);
			result = AddToThread(newHdr, thread, TRUE);
			break;
		}
	}
#ifdef SUBJ_THREADING
	// try subject threading if we couldn't find a reference and the subject starts with Re:
	nsString subject;

	newHdr->GetSubject(subject);
	if ((ThreadBySubjectWithoutRe() || (newHdrFlags & MSG_FLAG_HAS_RE)) && thread == NULL && (thread = GetThreadForSubject(nsAutoCString(subject))) != NULL)
	{
		thread->GetThreadKey(&threadId);
		newHdr->SetThreadId(threadId);
		//TRACE("threading based on subject %s\n", (const char *) msgHdr->m_subject);
//			AddNeoHdr(neoMsgHdr);
		// if we move this and do subject threading after, ref threading, 
		// don't thread within children, since we know it won't work. But for now, pass TRUE.
		result = AddToThread(newHdr, thread, TRUE);	
	}
#endif // SUBJ_THREADING

	if (thread == NULL)
	{
		// couldn't find any parent articles - msgHdr is top-level thread, for now
		result = AddNewThread(newHdr);
		newThread = TRUE;
	}
	else
	{
		newThread = FALSE;
	}
	return result;
}

nsresult nsMsgDatabase::AddToThread(nsMsgHdr *newHdr, nsMsgThread *thread, PRBool threadInThread)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsMsgHdr	*	nsMsgDatabase::GetMsgHdrForReference(const char *reference)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}

nsMsgHdr	*	nsMsgDatabase::GetMsgHdrForMessageID(const char *msgID)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}

nsMsgThread *	nsMsgDatabase::GetThreadContainingMsgHdr(nsMsgHdr *msgHdr)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}


nsMsgThread *nsMsgDatabase::GetThreadForMsgKey(nsMsgKey msgKey)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}

// make the passed in header a thread header
nsresult nsMsgDatabase::AddNewThread(nsMsgHdr *msgHdr)
{
	nsMsgThread *threadHdr = new nsMsgThread;
	if (threadHdr)
	{
		threadHdr->AddRef();
		threadHdr->SetThreadKey(msgHdr->m_messageKey);
	//	threadHdr->SetSubject

		// need to add the thread table to the db.
		AddToThread(msgHdr, threadHdr, FALSE);

		threadHdr->Release();
	}
	return NS_OK;
}


// should we thread messages with common subjects that don't start with Re: together?
// I imagine we might have separate preferences for mail and news, so this is a virtual method.
PRBool	nsMsgDatabase::ThreadBySubjectWithoutRe()
{
	return PR_TRUE;
}

nsresult nsMsgDatabase::GetBoolPref(const char *prefName, PRBool *result)
{
	XP_Bool prefValue = PR_FALSE;
	nsIPref* prefs = nsnull;
	nsresult rv;
	rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
    if (prefs && NS_SUCCEEDED(rv))
	{
//		prefs->Startup("prefs.js");

		rv = prefs->GetBoolPref(prefName, &prefValue);
		*result = (PRBool) prefValue;
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}
	return rv;
}

#ifdef DEBUG
nsresult nsMsgDatabase::DumpContents()
{
    nsMsgKey key;

#ifdef HAVE_INT_ENUMERATORS
    nsIEnumerator* keys;
	nsresult rv = EnumerateKeys(&keys);
    if (NS_FAILED(rv)) return rv;
    for (keys->First(); keys->IsDone != NS_OK; keys->Next()) {
        rv = keys->CurrentItem((nsISupports**)&key);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (NS_FAILED(rv)) break;
#else
    nsMsgKeyArray keys;
    nsresult rv = ListAllKeys(keys);
    for (PRUint32 i = 0; i < keys.GetSize(); i++) {
        key = keys[i];
#endif
		nsIMessage *msg = NULL;
        rv = GetMsgHdrForKey(key, &msg);
        nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, msg);      // closed system, cast ok
		if (NS_SUCCEEDED(rv))
		{
            nsAutoString author;
            nsAutoString subject;
			nsMsgKey key;

			msgHdr->GetMessageKey(&key);
			msgHdr->GetAuthor(author);
			msgHdr->GetSubject(subject);
			char *authorStr = author.ToNewCString();
			char *subjectStr = subject.ToNewCString();
			printf("hdr key = %ld, author = %s subject = %s\n", key, (authorStr) ? authorStr : "", (subjectStr) ? subjectStr : "");
			delete [] authorStr;
			delete [] subjectStr;
			NS_RELEASE(msgHdr);
		}
    }
    return NS_OK;
}
#endif
