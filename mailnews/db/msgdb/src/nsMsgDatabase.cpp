/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

// this file implements the nsMsgDatabase interface using the MDB Interface.

#include "nscore.h"
#include "msgCore.h"
#include "nsMsgDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsMsgKeySet.h"
#include "nsIEnumerator.h"
#include "nsMsgThread.h"
#include "nsFileStream.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIMsgHeaderParser.h"
#include "nsMsgBaseCID.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "xp_file.h"
#include "prprf.h"
#include "nsTime.h"
#include "nsIFileSpec.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsMsgMimeCID.h"
#include "nsILocaleService.h"

#include "nsIMsgAccountManager.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgFolderCacheElement.h"
#include "MailNewsTypes2.h"

static NS_DEFINE_CID(kCMorkFactory, NS_MORK_CID);

#if defined(XP_MAC) && defined(CompareString)
	#undef CompareString
#endif
#include "nsICollation.h"

#include "nsCollationCID.h"
#include "nsIPref.h"

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_MSGKEYSET 1
#endif

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 

#define MSG_HASH_SIZE 512

const PRInt32 kMaxHdrsInCache = 512;

// special keys
static const nsMsgKey kAllMsgHdrsTableKey = 1; 
static const nsMsgKey kTableKeyForThreadOne = 0xfffffffe;
static const nsMsgKey kFirstPseudoKey = 0xfffffffd;
static const nsMsgKey kIdStartOfFake = 0xffffff80;


// we never need to call this because we check the use cache first,
// and any hdr in this cache will be in the use cache.
nsresult nsMsgDatabase::GetHdrFromCache(nsMsgKey key, nsIMsgDBHdr* *result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_ERROR_FAILURE;

	*result = nsnull;
	if (m_bCacheHeaders && m_cachedHeaders)
	{
    PLDHashEntryHdr *entry;
    entry = PL_DHashTableOperate(m_cachedHeaders, (const void *) key, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(entry))
    {
      MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, entry);
      *result = element->mHdr;
      // need to do our own add ref because the PL_DHashTable doesn't addref.
		  if (*result)
		  {
			  NS_ADDREF(*result);
			  rv = NS_OK;
		  }
    }    

	}
	return rv;
}

nsresult nsMsgDatabase::AddHdrToCache(nsIMsgDBHdr *hdr, nsMsgKey key) // do we want key? We could get it from hdr
{
	if (m_bCacheHeaders)
	{
		if (!m_cachedHeaders)
    m_cachedHeaders = PL_NewDHashTable(&gMsgDBHashTableOps, (void *) nsnull, sizeof(struct MsgHdrHashElement), kMaxHdrsInCache );
    if (m_cachedHeaders)
    {
		  if (key == nsMsgKey_None)
			  hdr->GetMessageKey(&key);
      if (m_cachedHeaders->entryCount > kMaxHdrsInCache)
        ClearHdrCache(PR_TRUE);
      PLDHashEntryHdr *entry = PL_DHashTableOperate(m_cachedHeaders, (void *) key, PL_DHASH_ADD);
      if (!entry)
        return NS_ERROR_OUT_OF_MEMORY; // XXX out of memory

      MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, entry);
      element->mHdr = hdr;  
      element->mKey = key;
		  NS_ADDREF(hdr);     // make the cache hold onto the header
      return NS_OK;
    }
	}
	return NS_ERROR_FAILURE;
}


/* static */PLDHashOperator PR_CALLBACK nsMsgDatabase::HeaderEnumerator (PLDHashTable *table, PLDHashEntryHdr *hdr,
                               PRUint32 number, void *arg)
{

  MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, hdr);
  NS_IF_RELEASE(element->mHdr); 
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP nsMsgDatabase::ClearCachedHdrs()
{
  return ClearHdrCache(PR_FALSE); // don't re-init, hope db gets closed.
}

nsresult nsMsgDatabase::ClearHdrCache(PRBool reInit)
{
	if (m_cachedHeaders)
	{
    // save this away in case we renter this code.
    PLDHashTable  *saveCachedHeaders = m_cachedHeaders;
    m_cachedHeaders = nsnull;
    PL_DHashTableEnumerate(saveCachedHeaders, HeaderEnumerator, nsnull);

    if (reInit)
    {
      PL_DHashTableFinish(saveCachedHeaders);
      PL_DHashTableInit(saveCachedHeaders, &gMsgDBHashTableOps, nsnull, sizeof(struct MsgHdrHashElement), kMaxHdrsInCache);
      m_cachedHeaders = saveCachedHeaders;

    }
    else
    {
      PL_DHashTableDestroy(saveCachedHeaders);
    }
	}
	return NS_OK;
}

nsresult nsMsgDatabase::RemoveHdrFromCache(nsIMsgDBHdr *hdr, nsMsgKey key)
{
	if (m_cachedHeaders)
	{
		if (key == nsMsgKey_None)
			hdr->GetMessageKey(&key);

    PLDHashEntryHdr *entry = PL_DHashTableOperate(m_cachedHeaders, (const void *) key, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(entry))
    {
     PL_DHashTableOperate(m_cachedHeaders, (void *) key, PL_DHASH_REMOVE);
     NS_RELEASE(hdr); // get rid of extra ref the cache was holding.
    }

	}
	return NS_OK;
}


nsresult nsMsgDatabase::GetHdrFromUseCache(nsMsgKey key, nsIMsgDBHdr* *result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_ERROR_FAILURE;

	*result = nsnull;

	if (m_headersInUse)
	{
    PLDHashEntryHdr *entry;
    entry = PL_DHashTableOperate(m_headersInUse, (const void *) key, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(entry))
    {
      MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, entry);
      *result = element->mHdr;
    }    
		if (*result)
		{
			NS_ADDREF(*result);
			rv = NS_OK;
		}
	}
	return rv;
}

PLDHashTableOps nsMsgDatabase::gMsgDBHashTableOps =
{
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  GetKey,
  HashKey,
  MatchEntry,
  MoveEntry,
  ClearEntry,
  PL_DHashFinalizeStub,
  nsnull
};

const void* PR_CALLBACK
nsMsgDatabase::GetKey(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  MsgHdrHashElement* hdr = NS_REINTERPRET_CAST(MsgHdrHashElement*, aEntry);
//  nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, hdr->mHdr);  // closed system, so this is ok
  // ### could get the key from the hdr...
//  return (const void *) msgHdr->m_messageKey;
  return (const void *) hdr->mKey;
}

// HashKey is supposed to maximize entropy in the low order bits, and the key
// as is, should do that.
PLDHashNumber PR_CALLBACK
nsMsgDatabase::HashKey(PLDHashTable* aTable, const void* aKey)
{
  return PLDHashNumber(NS_PTR_TO_INT32(aKey));
}

PRBool PR_CALLBACK
nsMsgDatabase::MatchEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aEntry, const void* aKey)
{
  const MsgHdrHashElement* hdr = NS_REINTERPRET_CAST(const MsgHdrHashElement*, aEntry);
  return aKey == (const void *) hdr->mKey; // ### or get the key from the hdr...
}

void PR_CALLBACK
nsMsgDatabase::MoveEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aFrom, PLDHashEntryHdr* aTo)
{
  const MsgHdrHashElement* from = NS_REINTERPRET_CAST(const MsgHdrHashElement*, aFrom);
  MsgHdrHashElement* to = NS_REINTERPRET_CAST(MsgHdrHashElement*, aTo);
  // ### eh? Why is this needed? I don't think we have a copy operator?
  *to = *from;
}

void PR_CALLBACK
nsMsgDatabase::ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
  MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, aEntry);
  element->mHdr = nsnull; // eh? Need to release this or not?
  element->mKey = nsMsgKey_None; // eh?
}

extern PLDHashNumber PR_CALLBACK
PL_DHashStringKey(PLDHashTable *table, const void *key);

extern void PR_CALLBACK
PL_DHashFinalizeStub(PLDHashTable *table);


nsresult nsMsgDatabase::AddHdrToUseCache(nsIMsgDBHdr *hdr, nsMsgKey key) 
{
  if (!m_headersInUse)
  {
    mdb_count numHdrs = MSG_HASH_SIZE;
    if (m_mdbAllMsgHeadersTable)
      m_mdbAllMsgHeadersTable->GetCount(GetEnv(), &numHdrs);
    m_headersInUse = PL_NewDHashTable(&gMsgDBHashTableOps, (void *) nsnull, sizeof(struct MsgHdrHashElement), PR_MAX(MSG_HASH_SIZE, numHdrs));
  }
  if (m_headersInUse)
  {
		if (key == nsMsgKey_None)
			hdr->GetMessageKey(&key);
        PLDHashEntryHdr *entry = PL_DHashTableOperate(m_headersInUse, (void *) key, PL_DHASH_ADD);
    if (!entry)
      return NS_ERROR_OUT_OF_MEMORY; // XXX out of memory

    MsgHdrHashElement* element = NS_REINTERPRET_CAST(MsgHdrHashElement*, entry);
    element->mHdr = hdr;  
    element->mKey = key;
		// the hash table won't add ref, we'll do it ourselves
		// stand for the addref that CreateMsgHdr normally does.
		NS_ADDREF(hdr);
    return NS_OK;
  }

	return NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMsgDatabase::ClearUseHdrCache()
{
	if (m_headersInUse)
	{
    PL_DHashTableDestroy(m_headersInUse);
    m_headersInUse = nsnull;
	}
	return NS_OK;
}

nsresult nsMsgDatabase::RemoveHdrFromUseCache(nsIMsgDBHdr *hdr, nsMsgKey key)
{
	if (m_headersInUse)
	{
		if (key == nsMsgKey_None)
			hdr->GetMessageKey(&key);

   PL_DHashTableOperate(m_headersInUse, (void *) key, PL_DHASH_REMOVE);
	}
	return NS_OK;
}


nsresult
nsMsgDatabase::CreateMsgHdr(nsIMdbRow* hdrRow, nsMsgKey key, nsIMsgDBHdr* *result)
{
	nsresult rv = GetHdrFromUseCache(key, result);
	if (NS_SUCCEEDED(rv) && *result)
	{
		hdrRow->Release();
		return rv;
	}

	nsMsgHdr *msgHdr = new nsMsgHdr(this, hdrRow);
	if(!msgHdr)
		return NS_ERROR_OUT_OF_MEMORY;
    msgHdr->SetMessageKey(key);
  // don't need to addref here; GetHdrFromUseCache addrefs.
    *result = msgHdr;

	AddHdrToCache(msgHdr, key);  

	return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::AddListener(nsIDBChangeListener *listener)
{
    if (m_ChangeListeners == nsnull) 
	{
        m_ChangeListeners = new nsVoidArray();
        if (!m_ChangeListeners) 
			return NS_ERROR_OUT_OF_MEMORY;
    }
	return m_ChangeListeners->AppendElement(listener);
}

NS_IMETHODIMP nsMsgDatabase::RemoveListener(nsIDBChangeListener *listener)
{
    if (m_ChangeListeners == nsnull) 
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
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
NS_IMETHODIMP nsMsgDatabase::NotifyKeyChangeAll(nsMsgKey keyChanged, PRUint32 oldFlags, PRUint32 newFlags,
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull || oldFlags == newFlags)
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyChange(keyChanged, oldFlags, newFlags, instigator); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyReadChanged(nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull)
        return NS_OK;

    for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
    {
        nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

        nsresult rv = changeListener->OnReadChanged(instigator);
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyKeyDeletedAll(nsMsgKey keyDeleted, nsMsgKey parentKey, PRInt32 flags, 
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener = 
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyDeleted(keyDeleted, parentKey, flags, instigator); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyKeyAddedAll(nsMsgKey keyAdded, nsMsgKey parentKey, PRInt32 flags, 
	nsIDBChangeListener *instigator)
{
#ifdef DEBUG_bienvenu1
	printf("notifying add of %ld parent %ld\n", keyAdded, parentKey);
#endif
    if (m_ChangeListeners == nsnull) 
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnKeyAdded(keyAdded, parentKey, flags, instigator); 
        if (NS_FAILED(rv))
			return rv;
	}
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::NotifyParentChangedAll(nsMsgKey keyReparented, nsMsgKey oldParent, nsMsgKey newParent,
	nsIDBChangeListener *instigator)
{
    if (m_ChangeListeners == nsnull) 
		return NS_OK;
	for (PRInt32 i = 0; i < m_ChangeListeners->Count(); i++)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnParentChanged(keyReparented, oldParent, newParent, instigator); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}


NS_IMETHODIMP nsMsgDatabase::NotifyAnnouncerGoingAway(void)
{
    if (m_ChangeListeners == nsnull)
		return NS_OK;
	// run loop backwards because listeners remove themselves from the list 
	// on this notification
	for (PRInt32 i = m_ChangeListeners->Count() - 1; i >= 0 ; i--)
	{
		nsIDBChangeListener *changeListener =
            (nsIDBChangeListener *) m_ChangeListeners->ElementAt(i);

		nsresult rv = changeListener->OnAnnouncerGoingAway(this); 
        if (NS_FAILED(rv)) 
			return rv;
	}
    return NS_OK;
}



nsVoidArray *nsMsgDatabase::m_dbCache = NULL;

//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------

nsVoidArray/*<nsMsgDatabase>*/*
nsMsgDatabase::GetDBCache()
{
	if (!m_dbCache)
		m_dbCache = new nsVoidArray();

	return m_dbCache;
	
}

void
nsMsgDatabase::CleanupCache()
{
  if (m_dbCache) // clean up memory leak
  {
    for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
    {
      nsMsgDatabase* pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
      if (pMessageDB)
      {
        // hold onto the db until we're finished closing it.
        nsCOMPtr <nsIMsgDatabase> kungFuGrip = pMessageDB;
        // break cycle with folder -> parse msg state -> db
        pMessageDB->m_folder = nsnull;
        pMessageDB->ForceClosed();
        kungFuGrip = nsnull;
        // look for db in cache before deleting, 
        // in case ForceClosed caused the db to go away
        if (FindInCache(pMessageDB) != -1)
        {
          PRInt32 saveRefCnt = pMessageDB->mRefCnt;
          while (saveRefCnt-- >= 1)
          {
            nsMsgDatabase *saveDB = pMessageDB;
            NS_RELEASE(saveDB);
          }
        }
        i--;	// back up array index, since closing removes db from cache.
      }
    }
    NS_ASSERTION(GetNumInCache() == 0, "some msg dbs left open");	// better not be any open db's.
    delete m_dbCache;
  }
  m_dbCache = nsnull; // Need to reset to NULL since it's a
  // static global ptr and maybe referenced 
  // again in other places.
}

//----------------------------------------------------------------------
// FindInCache - this addrefs the db it finds.
//----------------------------------------------------------------------
nsMsgDatabase* nsMsgDatabase::FindInCache(nsFileSpec &dbName)
{
	for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
	{
		nsMsgDatabase* pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
		if (pMessageDB->MatchDbName(dbName))
		{
      if (pMessageDB->m_mdbStore)  // don't return db without store
      {
			  NS_ADDREF(pMessageDB);
			  return pMessageDB;
      }
		}
	}
	return nsnull;
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
int nsMsgDatabase::FindInCache(nsMsgDatabase* pMessageDB)
{
	for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
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
    nsMsgDatabase* pMessageDB = nsnull;
	for (PRInt32 i = 0; i < GetDBCache()->Count(); i++)
	{
		pMessageDB = NS_STATIC_CAST(nsMsgDatabase*, GetDBCache()->ElementAt(i));
	}
}
#endif /* DEBUG */

nsMsgDatabase::nsMsgDatabase()
    : m_dbFolderInfo(nsnull), 
	  m_nextPseudoMsgKey(kFirstPseudoKey),
      m_mdbEnv(nsnull), m_mdbStore(nsnull),
      m_mdbAllMsgHeadersTable(nsnull), m_dbName(""), m_newSet(nsnull),
      m_mdbTokensInitialized(PR_FALSE), m_ChangeListeners(nsnull),
      m_hdrRowScopeToken(0),
      m_hdrTableKindToken(0),
	  m_threadTableKindToken(0),
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
      m_ccListColumnToken(0),
	  m_threadFlagsColumnToken(0),
	  m_threadIdColumnToken(0),
	  m_threadChildrenColumnToken(0),
	  m_threadUnreadChildrenColumnToken(0),
	  m_messageThreadIdColumnToken(0),
	  m_threadSubjectColumnToken(0),
	  m_numReferencesColumnToken(0),
	  m_messageCharSetColumnToken(0),
	  m_threadParentColumnToken(0),
	  m_threadRootKeyColumnToken(0),
    m_offlineMsgOffsetColumnToken(0),
    m_offlineMessageSizeColumnToken(0),
	  m_HeaderParser(nsnull),
	  m_headersInUse(nsnull),
	  m_cachedHeaders(nsnull),
	  m_bCacheHeaders(PR_FALSE)

{
	NS_INIT_REFCNT();
	m_bCacheHeaders = PR_TRUE;
}

nsMsgDatabase::~nsMsgDatabase()
{
//	Close(FALSE);	// better have already been closed.
	ClearHdrCache(PR_FALSE);
#ifdef DEBUG_bienvenu1
  NS_ASSERTION(!m_headersInUse || m_headersInUse->Count() == 0, "leaking headers");
#endif
	ClearUseHdrCache();
	delete m_cachedHeaders;
	delete m_headersInUse;

	RemoveFromCache(this);
#ifdef DEBUG_bienvenu1
	if (GetNumInCache() != 0)
	{
		XP_Trace("closing %s\n", m_dbName);
		DumpCache();
	}
#endif
	NS_IF_RELEASE(m_dbFolderInfo);
	if (m_HeaderParser)
	{
		NS_RELEASE(m_HeaderParser);
		m_HeaderParser = nsnull;
	}
	if (m_mdbAllMsgHeadersTable)
		m_mdbAllMsgHeadersTable->Release();

	if (m_mdbStore)
	{
		m_mdbStore->CloseMdbObject(m_mdbEnv);
	}
	if (m_mdbEnv)
	{
		m_mdbEnv->CutStrongRef(m_mdbEnv); //??? is this right?
		m_mdbEnv = nsnull;
	}
    if (m_ChangeListeners) 
	{
        // better not be any listeners, because we're going away.
        NS_ASSERTION(m_ChangeListeners->Count() == 0, "shouldn't have any listeners");
        delete m_ChangeListeners;
    }

    if (m_newSet) {
#ifdef DEBUG_MSGKEYSET
        char *str = nsnull;
        nsresult rv = m_newSet->Output(&str);
        if (NS_SUCCEEDED(rv) && str) {
            printf("setStr = %s on destroy\n",str);
            nsMemory::Free(str);
            str = nsnull;
        }
#endif
        delete m_newSet;
        m_newSet = nsnull;
    }
}

NS_IMPL_ADDREF(nsMsgDatabase)

NS_IMPL_RELEASE(nsMsgDatabase)

NS_IMETHODIMP nsMsgDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIMsgDatabase)) ||
        aIID.Equals(NS_GET_IID(nsIDBChangeAnnouncer)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIMsgDatabase*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

/* static */ nsIMdbFactory *nsMsgDatabase::GetMDBFactory()
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
		nsCRT::free(ioPath);
		ioPath = result;
	}
}

#endif /* XP_MAC */

NS_IMETHODIMP nsMsgDatabase::OpenFolderDB(nsIMsgFolder *folder, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
  NS_ENSURE_ARG(folder);
  m_folder = folder;
  nsCOMPtr <nsIFileSpec> folderPath;
  nsresult rv = folder->GetPath(getter_AddRefs(folderPath));
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(folderPath, create, upgrading, pMessageDB);
}

NS_IMETHODIMP nsMsgDatabase::Open(nsIFileSpec *folderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
	NS_ASSERTION(PR_FALSE, "must override");
	return NS_ERROR_NOT_IMPLEMENTED;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
nsresult nsMsgDatabase::OpenMDB(const char *dbName, PRBool create)
{
	nsresult ret = NS_OK;
	nsIMdbFactory *myMDBFactory = GetMDBFactory();
	if (myMDBFactory)
	{
		ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
		if (NS_SUCCEEDED(ret))
		{
			nsIMdbThumb *thumb = nsnull;
			struct stat st;
			char	*nativeFileName = nsCRT::strdup(dbName);
			nsIMdbHeap* dbHeap = 0;
			mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable

			if (!nativeFileName)
				return NS_ERROR_OUT_OF_MEMORY;

			if (m_mdbEnv)
				m_mdbEnv->SetAutoClear(PR_TRUE);
			m_dbName = dbName;
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
        // only 0 is a non-error return.
				if (ret == 0 && outDone)
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

nsresult nsMsgDatabase::CloseMDB(PRBool commit)
{
	if (commit)
		Commit(nsMsgDBCommitType::kSessionCommit);
	return(NS_OK);
}

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
// This is evil in the com world, but there are times we need to delete the file.
NS_IMETHODIMP nsMsgDatabase::ForceClosed()
{
	nsresult	err = NS_OK;
    nsCOMPtr<nsIMsgDatabase> aDb(do_QueryInterface(this, &err));

	// make sure someone has a reference so object won't get deleted out from under us.
	AddRef();	
	NotifyAnnouncerGoingAway();
	// OK, remove from cache first and close the store.
//	RemoveFromCache(this);
	NS_IF_RELEASE(m_dbFolderInfo);
        // clear out db ptr in folder info; it might have just become invalid
        if (m_dbFolderInfo)
          m_dbFolderInfo->m_mdb = nsnull;
	m_dbFolderInfo = nsnull;

	err = CloseMDB(PR_FALSE);	// since we're about to delete it, no need to commit.
	ClearHdrCache(PR_FALSE);
#ifdef DEBUG_bienvenu
  if (m_headersInUse && m_headersInUse->entryCount > 0)
  {
//    NS_ASSERTION(PR_FALSE, "leaking headers");
    printf("leaking %d headers in %s\n", m_headersInUse->entryCount, (const char *) m_dbName);
  }
#endif
	ClearUseHdrCache();
	if (m_mdbStore)
	{
		m_mdbStore->CloseMdbObject(m_mdbEnv);
		m_mdbStore = nsnull;
	}
	Release();
	return err;
}

// caller must Release result.
NS_IMETHODIMP nsMsgDatabase::GetDBFolderInfo(nsIDBFolderInfo	**result)
{
  *result = m_dbFolderInfo;
  if (m_dbFolderInfo)
  {
    m_dbFolderInfo->AddRef();
    return NS_OK;
  }
  else
  {
    NS_ASSERTION(PR_FALSE, "db must be corrupt");
    return NS_ERROR_NULL_POINTER; // it's an error if we can't get this
  }
}

NS_IMETHODIMP nsMsgDatabase::Commit(nsMsgDBCommit commitType)
{
	nsresult	err = NS_OK;
	nsIMdbThumb	*commitThumb = NULL;

#ifdef DEBUG_seth
    printf("nsMsgDatabase::Commit(%d)\n",commitType);
#endif

	if (commitType == nsMsgDBCommitType::kLargeCommit || commitType == nsMsgDBCommitType::kSessionCommit)
	{
		mdb_percent outActualWaste = 0;
		mdb_bool outShould;
        if (m_mdbStore) {
            err = m_mdbStore->ShouldCompress(GetEnv(), 30, &outActualWaste, &outShould);
            if (NS_SUCCEEDED(err) && outShould)
                commitType = nsMsgDBCommitType::kCompressCommit;
        }
	}
//	commitType = nsMsgDBCommitType::kCompressCommit;	// ### until incremental writing works.

	if (m_mdbStore)
	{
		switch (commitType)
		{
		case nsMsgDBCommitType::kSmallCommit:
			err = m_mdbStore->SmallCommit(GetEnv());
			break;
		case nsMsgDBCommitType::kLargeCommit:
			err = m_mdbStore->LargeCommit(GetEnv(), &commitThumb);
			break;
		case nsMsgDBCommitType::kSessionCommit:
			err = m_mdbStore->SessionCommit(GetEnv(), &commitThumb);
			break;
		case nsMsgDBCommitType::kCompressCommit:
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
		if(commitThumb)
			commitThumb->CutStrongRef(m_mdbEnv);
	}
	// ### do something with error, but clear it now because mork errors out on commits.
	if (GetEnv())
		GetEnv()->ClearErrors();

  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && accountManager)
  {
    nsCOMPtr<nsIMsgFolderCache> folderCache;

    rv = accountManager->GetFolderCache(getter_AddRefs(folderCache));
    if (NS_SUCCEEDED(rv) && folderCache)
    {
      nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
      rv = folderCache->GetCacheElement(m_dbName, PR_FALSE, getter_AddRefs(cacheElement));
      if (NS_SUCCEEDED(rv) && cacheElement && m_dbFolderInfo)
      {
        PRInt32 totalMessages, unreadMessages, pendingMessages, pendingUnreadMessages;

        m_dbFolderInfo->GetNumMessages(&totalMessages);
        m_dbFolderInfo->GetNumNewMessages(&unreadMessages);
        m_dbFolderInfo->GetImapUnreadPendingMessages(&pendingUnreadMessages);
        m_dbFolderInfo->GetImapTotalPendingMessages(&pendingMessages);
	      cacheElement->SetInt32Property("totalMsgs", totalMessages);
	      cacheElement->SetInt32Property("totalUnreadMsgs", unreadMessages);
        cacheElement->SetInt32Property("pendingMsgs", pendingMessages);
        cacheElement->SetInt32Property("pendingUnreadMsgs", pendingUnreadMessages);
        folderCache->Commit(PR_FALSE);
      }
    }
  }
  
	return err;
}

NS_IMETHODIMP nsMsgDatabase::Close(PRBool forceCommit /* = TRUE */)
{
	return CloseMDB(forceCommit);
}

const char *kMsgHdrsScope = "ns:msg:db:row:scope:msgs:all";	// scope for all headers table
const char *kMsgHdrsTableKind = "ns:msg:db:table:kind:msgs";
const char *kThreadTableKind = "ns:msg:db:table:kind:thread";
const char *kThreadHdrsScope = "ns:msg:db:row:scope:threads:all"; // scope for all threads table
const char *kAllThreadsTableKind = "ns:msg:db:table:kind:allthreads"; // kind for table of all threads
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
const char *kMessageThreadIdColumnName = "msgThreadId";
const char *kNumReferencesColumnName = "numRefs";
const char *kThreadFlagsColumnName = "threadFlags";
const char *kThreadIdColumnName = "threadId";
const char *kThreadChildrenColumnName = "children";
const char *kThreadUnreadChildrenColumnName = "unreadChildren";
const char *kThreadSubjectColumnName = "threadSubject";
const char *kMessageCharSetColumnName = "msgCharSet";
const char *kThreadParentColumnName = "threadParent";
const char *kThreadRootColumnName = "threadRoot";
const char *kOfflineMsgOffsetColumnName = "msgOffset";
const char *kOfflineMsgSizeColumnName = "offlineMsgSize";
struct mdbOid gAllMsgHdrsTableOID;
struct mdbOid gAllThreadsTableOID;

// set up empty tables, dbFolderInfo, etc.
nsresult nsMsgDatabase::InitNewDB()
{
	nsresult err = NS_OK;

	err = InitMDBInfo();
	if (err == NS_OK)
	{
		// why is this bad? dbFolderInfo is tightly tightly bound to nsMsgDatabase. It will
		// never be provided by someone else. It could be strictly embedded inside nsMsgDatabase
		// but I wanted to keep nsMsgDatabase a little bit smaller
		nsDBFolderInfo *dbFolderInfo = new nsDBFolderInfo(this); // this is bad!! Should go through component manager
		if (dbFolderInfo)
		{
			NS_ADDREF(dbFolderInfo); // mscott: shouldn't have to do this...go through c. manager
			err = dbFolderInfo->AddToNewMDB();
			dbFolderInfo->SetVersion(GetCurVersion());
			nsIMdbStore *store = GetStore();
			// create the unique table for the dbFolderInfo.
			mdb_err mdberr;
	    struct mdbOid allMsgHdrsTableOID;

	    if (!store)
		    return NS_ERROR_NULL_POINTER;

	    allMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
	    allMsgHdrsTableOID.mOid_Id = kAllMsgHdrsTableKey;

	    mdberr  = GetStore()->NewTableWithOid(GetEnv(), &allMsgHdrsTableOID, m_hdrTableKindToken, 
                                         PR_FALSE, nsnull, &m_mdbAllMsgHeadersTable);

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
			{
				NS_ADDREF(m_dbFolderInfo); 
				err = m_dbFolderInfo->InitFromExistingDB();
			}
		}
    else
      err = NS_ERROR_FAILURE;

    // create new all msg hdrs table, if it doesn't exist.
    if (NS_SUCCEEDED(err) && !m_mdbAllMsgHeadersTable)
    {
			nsIMdbStore *store = GetStore();
	    struct mdbOid allMsgHdrsTableOID;
	    allMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
	    allMsgHdrsTableOID.mOid_Id = kAllMsgHdrsTableKey;

	    mdb_err mdberr  = GetStore()->NewTableWithOid(GetEnv(), &allMsgHdrsTableOID, m_hdrTableKindToken, 
                                         PR_FALSE, nsnull, &m_mdbAllMsgHeadersTable);
      if (mdberr != NS_OK || !m_mdbAllMsgHeadersTable)
        err = NS_ERROR_FAILURE;
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
			GetStore()->StringToToken(GetEnv(),  kMessageThreadIdColumnName, &m_messageThreadIdColumnToken);
			GetStore()->StringToToken(GetEnv(),  kThreadIdColumnName, &m_threadIdColumnToken);
			GetStore()->StringToToken(GetEnv(),  kThreadFlagsColumnName, &m_threadFlagsColumnToken);
			GetStore()->StringToToken(GetEnv(),  kThreadChildrenColumnName, &m_threadChildrenColumnToken);
			GetStore()->StringToToken(GetEnv(),  kThreadUnreadChildrenColumnName, &m_threadUnreadChildrenColumnToken);
			GetStore()->StringToToken(GetEnv(),  kThreadSubjectColumnName, &m_threadSubjectColumnToken);
			GetStore()->StringToToken(GetEnv(),  kNumReferencesColumnName, &m_numReferencesColumnToken);
			GetStore()->StringToToken(GetEnv(),  kMessageCharSetColumnName, &m_messageCharSetColumnToken);
			err = GetStore()->StringToToken(GetEnv(), kMsgHdrsTableKind, &m_hdrTableKindToken); 
			if (err == NS_OK)
				err = GetStore()->StringToToken(GetEnv(), kThreadTableKind, &m_threadTableKindToken);
			err = GetStore()->StringToToken(GetEnv(), kAllThreadsTableKind, &m_allThreadsTableKindToken); 
			err	= GetStore()->StringToToken(GetEnv(), kThreadHdrsScope, &m_threadRowScopeToken); 
			err	= GetStore()->StringToToken(GetEnv(), kThreadParentColumnName, &m_threadParentColumnToken);
			err	= GetStore()->StringToToken(GetEnv(), kThreadRootColumnName, &m_threadRootKeyColumnToken);
      err = GetStore()->StringToToken(GetEnv(), kOfflineMsgOffsetColumnName, &m_offlineMsgOffsetColumnToken);
      err = GetStore()->StringToToken(GetEnv(), kOfflineMsgSizeColumnName, &m_offlineMessageSizeColumnToken);

			if (err == NS_OK)
			{
				// The table of all message hdrs will have table id 1.
				gAllMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
				gAllMsgHdrsTableOID.mOid_Id = kAllMsgHdrsTableKey;
				gAllThreadsTableOID.mOid_Scope = m_threadRowScopeToken;
				gAllThreadsTableOID.mOid_Id = 1;

			}
		}
	}
	return err;
}

// Returns if the db contains this key
NS_IMETHODIMP nsMsgDatabase::ContainsKey(nsMsgKey key, PRBool *containsKey)
{

	nsresult	err = NS_OK;
	mdb_bool	hasOid;
	mdbOid		rowObjectId;

	if (!containsKey || !m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;
	*containsKey = PR_FALSE;

	rowObjectId.mOid_Id = key;
	rowObjectId.mOid_Scope = m_hdrRowScopeToken;
	err = m_mdbAllMsgHeadersTable->HasOid(GetEnv(), &rowObjectId, &hasOid);
	if(NS_SUCCEEDED(err))
		*containsKey = hasOid;

	return err;
}

// get a message header for the given key. Caller must release()!
NS_IMETHODIMP nsMsgDatabase::GetMsgHdrForKey(nsMsgKey key, nsIMsgDBHdr **pmsgHdr)
{
	nsresult	err = NS_OK;
	mdb_bool	hasOid;
	mdbOid		rowObjectId;

#ifdef DEBUG_bienvenu
  NS_ASSERTION(m_folder, "folder should be set");
#endif

	if (!pmsgHdr || !m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;

	*pmsgHdr = NULL;
	err = GetHdrFromUseCache(key, pmsgHdr);
	if (NS_SUCCEEDED(err) && *pmsgHdr)
		return err;

	rowObjectId.mOid_Id = key;
	rowObjectId.mOid_Scope = m_hdrRowScopeToken;
	err = m_mdbAllMsgHeadersTable->HasOid(GetEnv(), &rowObjectId, &hasOid);
	if (err == NS_OK && m_mdbStore /* && hasOid */)
	{
		nsIMdbRow *hdrRow;
		err = m_mdbStore->GetRow(GetEnv(), &rowObjectId, &hdrRow);

		if (err == NS_OK && hdrRow)
		{
//			NS_ASSERTION(hasOid, "we had oid, right?");
			err = CreateMsgHdr(hdrRow,  key, pmsgHdr);
		}
	}

	return err;
}

NS_IMETHODIMP nsMsgDatabase::StartBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::EndBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::DeleteMessage(nsMsgKey key, nsIDBChangeListener *instigator, PRBool commit)
{
	nsresult	err = NS_OK;
	nsIMsgDBHdr *msgHdr = NULL;

	err = GetMsgHdrForKey(key, &msgHdr);
	if (msgHdr == NULL)
		return NS_MSG_MESSAGE_NOT_FOUND;

	err = DeleteHeader(msgHdr, instigator, commit, PR_TRUE);
	NS_IF_RELEASE(msgHdr);
	return err;
}


NS_IMETHODIMP nsMsgDatabase::DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator)
{
	nsresult	err = NS_OK;

    PRUint32 kindex;
	for (kindex = 0; kindex < nsMsgKeys->GetSize(); kindex++)
	{
		nsMsgKey key = nsMsgKeys->ElementAt(kindex);
		nsIMsgDBHdr *msgHdr = NULL;
		
		PRBool hasKey;

		if (NS_SUCCEEDED(ContainsKey(key, &hasKey)) && hasKey)
		{
			err = GetMsgHdrForKey(key, &msgHdr);
			if (NS_FAILED(err)) 
			{
				err = NS_MSG_MESSAGE_NOT_FOUND;
				break;
			}
			if (msgHdr)
				err = DeleteHeader(msgHdr, instigator, kindex % 300 == 0, PR_TRUE);
			NS_IF_RELEASE(msgHdr);
			if (err != NS_OK)
				break;
		}
	}
	Commit(nsMsgDBCommitType::kSmallCommit);
	return err;
}

nsresult nsMsgDatabase::AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr)
{
  PRUint32 size = 0;
  (void)msgHdr->GetMessageSize(&size);
  return m_dbFolderInfo->ChangeExpungedBytes (size);
}


NS_IMETHODIMP nsMsgDatabase::DeleteHeader(nsIMsgDBHdr *msg, nsIDBChangeListener *instigator, PRBool commit, PRBool notify)
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
        AdjustExpungedBytesOnDelete(msg);
	}	

    PRUint32 flags;
	nsMsgKey threadParent;

	//Save off flags and threadparent since they will no longer exist after we remove the header from the db.
	if (notify)
	{

        (void)msg->GetFlags(&flags);
		msg->GetThreadParent(&threadParent);
	}

  RemoveHeaderFromThread(msgHdr);
	if (notify /* && NS_SUCCEEDED(ret)*/)
	{

		NotifyKeyDeletedAll(key, threadParent, flags, instigator); // tell listeners
    }
//	if (!onlyRemoveFromThread)	// to speed up expiration, try this. But really need to do this in RemoveHeaderFromDB
	nsresult ret = RemoveHeaderFromDB(msgHdr);
	

	if (commit)
		Commit(nsMsgDBCommitType::kLargeCommit);			// ### dmb is this a good time to commit?
	return ret;
}

NS_IMETHODIMP
nsMsgDatabase::UndoDelete(nsIMsgDBHdr *msgHdr)
{
    if (msgHdr)
    {
        SetHdrFlag(msgHdr, PR_FALSE, MSG_FLAG_EXPUNGED);
        // ** do we need to update folder info regarding the message size
    }
    return NS_OK;
}

nsresult nsMsgDatabase::RemoveHeaderFromThread(nsMsgHdr *msgHdr)
{
	if (!msgHdr)
		return NS_ERROR_NULL_POINTER;
	nsresult ret = NS_OK;
	nsCOMPtr <nsIMsgThread> thread ;
	ret = GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(thread));
	if (NS_SUCCEEDED(ret) && thread)
	{
		nsCOMPtr <nsIDBChangeAnnouncer> announcer = do_QueryInterface(this);
		ret = thread->RemoveChildHdr(msgHdr, announcer);
	}
  return ret;
}

NS_IMETHODIMP nsMsgDatabase::RemoveHeaderMdbRow(nsIMsgDBHdr *msg)
{
  NS_ENSURE_ARG_POINTER(msg);
  nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, msg);  // closed system, so this is ok
  return RemoveHeaderFromDB(msgHdr);
}

// This is a lower level routine which doesn't send notifcations or
// update folder info. One use is when a rule fires moving a header
// from one db to another, to remove it from the first db.

nsresult nsMsgDatabase::RemoveHeaderFromDB(nsMsgHdr *msgHdr)
{
	if (!msgHdr)
		return NS_ERROR_NULL_POINTER;
	nsresult ret = NS_OK;

	RemoveHdrFromCache(msgHdr, nsMsgKey_None);
	nsIMdbRow* row = msgHdr->GetMDBRow();
	ret = m_mdbAllMsgHeadersTable->CutRow(GetEnv(), row);
	row->CutAllColumns(GetEnv());
	return ret;
}

nsresult nsMsgDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
	nsresult rv;
	nsIMsgDBHdr *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv) || !msgHdr) 
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?
    rv = IsHeaderRead(msgHdr, pRead);
    NS_RELEASE(msgHdr);
    return rv;
}

PRUint32	nsMsgDatabase::GetStatusFlags(nsIMsgDBHdr *msgHdr, PRUint32 origFlags)
{
	PRUint32	statusFlags = origFlags;
	PRBool	isRead = PR_TRUE;

    nsMsgKey key;
    (void)msgHdr->GetMessageKey(&key);
	if (m_newSet && m_newSet->IsMember(key))
		statusFlags |= MSG_FLAG_NEW;
  else
    statusFlags &= ~MSG_FLAG_NEW;
	if (IsHeaderRead(msgHdr, &isRead) == NS_OK && isRead)
		statusFlags |= MSG_FLAG_READ;
	return statusFlags;
}

nsresult nsMsgDatabase::IsHeaderRead(nsIMsgDBHdr *msgHdr, PRBool *pRead)
{
	if (!msgHdr)
		return NS_MSG_MESSAGE_NOT_FOUND;

    nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, msgHdr);          // closed system, cast ok
	// can't call GetFlags, because it will be recursive.
    PRUint32 flags;
	hdr->GetRawFlags(&flags);
	*pRead = (flags & MSG_FLAG_READ) != 0;
	return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::IsMarked(nsMsgKey key, PRBool *pMarked)
{
	nsresult rv;
	nsIMsgDBHdr *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv))
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

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
	nsCOMPtr <nsIMsgThread> threadHdr;
	
	nsresult rv = GetThreadForMsgKey(key, getter_AddRefs(threadHdr));
	// This should be very surprising, but we leave that up to the caller
	// to determine for now.
	if (!threadHdr)
		return NS_MSG_MESSAGE_NOT_FOUND;

	PRUint32 threadFlags;
	threadHdr->GetFlags(&threadFlags);
	*pIgnored = (threadFlags & MSG_FLAG_IGNORED) ? PR_TRUE : PR_FALSE;
	return rv;
}

nsresult nsMsgDatabase::HasAttachments(nsMsgKey key, PRBool *pHasThem)
{
	nsresult rv;

	PR_ASSERT(pHasThem != NULL);
	if (!pHasThem)
		return NS_ERROR_NULL_POINTER;

	nsIMsgDBHdr *msgHdr;

	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv)) 
		return rv;

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pHasThem = (flags & MSG_FLAG_ATTACHMENT) ? PR_TRUE : PR_FALSE;
    NS_RELEASE(msgHdr);
	return rv;
}

PRBool nsMsgDatabase::SetHdrReadFlag(nsIMsgDBHdr *msgHdr, PRBool bRead)
{
  return SetHdrFlag(msgHdr, bRead, MSG_FLAG_READ);
}

nsresult nsMsgDatabase::MarkHdrReadInDB(nsIMsgDBHdr *msgHdr, PRBool bRead,
                                             nsIDBChangeListener *instigator)
{
  nsresult rv;
  nsMsgKey key;
  PRUint32 oldFlags;
  (void)msgHdr->GetMessageKey(&key);
  msgHdr->GetFlags(&oldFlags);
  
  if (m_newSet)
    m_newSet->Remove(key);
  
  if (m_dbFolderInfo != NULL)
  {
    if (bRead)
      m_dbFolderInfo->ChangeNumNewMessages(-1);
    else
      m_dbFolderInfo->ChangeNumNewMessages(1);
  }
  
  SetHdrReadFlag(msgHdr, bRead); // this will cause a commit, at least for local mail, so do it after we change
  // the folder counts above, so they will get committed too.
  PRUint32 flags;
  rv = msgHdr->GetFlags(&flags);
  flags &= ~MSG_FLAG_NEW;
  msgHdr->SetFlags(flags);
  if (NS_FAILED(rv)) return rv;
  
  return NotifyKeyChangeAll(key, oldFlags, flags, instigator);
}

NS_IMETHODIMP nsMsgDatabase::MarkRead(nsMsgKey key, PRBool bRead, 
						   nsIDBChangeListener *instigator)
{
	nsresult rv;
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
	
	rv = GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    if (NS_FAILED(rv) || !msgHdr) 
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

	rv = MarkHdrRead(msgHdr, bRead, instigator);
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
nsMsgDatabase::MarkThreadRead(nsIMsgThread *thread, nsIDBChangeListener *instigator, nsMsgKeyArray *thoseMarked)
{
	if (!thread)
		return NS_ERROR_NULL_POINTER;
	nsresult rv = NS_OK;

	PRUint32 numChildren;
	thread->GetNumChildren(&numChildren);
	for (PRUint32 curChildIndex = 0; curChildIndex < numChildren; curChildIndex++)
	{
		nsCOMPtr <nsIMsgDBHdr> child;

		rv = thread->GetChildAt(curChildIndex, getter_AddRefs(child));
		if (NS_SUCCEEDED(rv) && child)
		{
			PRBool isRead = PR_TRUE;
			IsHeaderRead(child, &isRead);
			if (!isRead)
			{
				if (thoseMarked)
				{
					nsMsgKey key;
					if (NS_SUCCEEDED(child->GetMessageKey(&key)))
						thoseMarked->Add(key);
				}
				MarkHdrRead(child, PR_TRUE, instigator);
			}
		}
	}

    return rv;
}

NS_IMETHODIMP
nsMsgDatabase::MarkThreadIgnored(nsIMsgThread *thread, nsMsgKey threadKey, PRBool bIgnored,
                                 nsIDBChangeListener *instigator)
{
  NS_ENSURE_ARG(thread);
  PRUint32 threadFlags;
  thread->GetFlags(&threadFlags);
  PRUint32 oldThreadFlags = threadFlags; // not quite right, since we probably want msg hdr flags.
	if (bIgnored)
	{
    threadFlags |= MSG_FLAG_IGNORED;
		threadFlags &= ~MSG_FLAG_WATCHED;	// ignore is implicit un-watch
	}
	else
		threadFlags &= ~MSG_FLAG_IGNORED;
  thread->SetFlags(threadFlags);
	NotifyKeyChangeAll(threadKey, oldThreadFlags, threadFlags, instigator);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgDatabase::MarkThreadWatched(nsIMsgThread *thread, nsMsgKey threadKey, PRBool bWatched,
                                 nsIDBChangeListener *instigator)
{
  NS_ENSURE_ARG(thread);
  PRUint32 threadFlags;
  thread->GetFlags(&threadFlags);
  PRUint32 oldThreadFlags = threadFlags; // not quite right, since we probably want msg hdr flags.
	if (bWatched)
	{
		threadFlags |= MSG_FLAG_WATCHED;
		threadFlags &= ~MSG_FLAG_IGNORED;	// watch is implicit un-ignore
	}
	else
		threadFlags &= ~MSG_FLAG_WATCHED;
	NotifyKeyChangeAll(threadKey, oldThreadFlags, threadFlags, instigator);
  thread->SetFlags(threadFlags);
  return NS_OK;
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
nsMsgDatabase::AllMsgKeysImapDeleted(nsMsgKeyArray *keys, PRBool *allKeysDeleted)
{
  if (!keys || ! allKeysDeleted)
    return NS_ERROR_NULL_POINTER;

	for (PRUint32 kindex = 0; kindex < keys->GetSize(); kindex++)
	{
		nsMsgKey key = keys->ElementAt(kindex);
		nsIMsgDBHdr *msgHdr = NULL;
		
		PRBool hasKey;

		if (NS_SUCCEEDED(ContainsKey(key, &hasKey)) && hasKey)
		{
			nsresult err = GetMsgHdrForKey(key, &msgHdr);
			if (NS_FAILED(err)) 
			{
        // ### we drop this error -probably OK.
				err = NS_MSG_MESSAGE_NOT_FOUND;
				break;
			}
			if (msgHdr)
      {
        PRUint32 flags;
        (void)msgHdr->GetFlags(&flags);
        if (! (flags & MSG_FLAG_IMAP_DELETED))
        {
          *allKeysDeleted = PR_FALSE;
          return NS_OK;
        }
      }
    }
  }
  *allKeysDeleted = PR_TRUE;
  return NS_OK;
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
	nsIMsgDBHdr *msgHdr = nsnull;
	
    rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv) || !msgHdr)
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

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
	nsIMsgDBHdr *msgHdr = nsnull;
	
	rv = GetMsgHdrForKey(key, &msgHdr);
    if (NS_FAILED(rv) || !msgHdr) 
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
    *pSent = flags & MSG_FLAG_MDN_REPORT_SENT;
    NS_RELEASE(msgHdr);
    return rv;
}


nsresult	nsMsgDatabase::SetKeyFlag(nsMsgKey key, PRBool set, PRUint32 flag,
							  nsIDBChangeListener *instigator)
{
	nsresult rv;
	nsCOMPtr <nsIMsgDBHdr> msgHdr;
		
    rv = GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
    if (NS_FAILED(rv) || !msgHdr) 
		return NS_MSG_MESSAGE_NOT_FOUND; // XXX return rv?

	PRUint32 oldFlags;
	msgHdr->GetFlags(&oldFlags);

	SetHdrFlag(msgHdr, set, flag);

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
	NotifyKeyChangeAll(key, oldFlags, flags, instigator);

	return rv;
}

nsresult nsMsgDatabase::SetMsgHdrFlag(nsIMsgDBHdr *msgHdr, PRBool set, PRUint32 flag, nsIDBChangeListener *instigator)
{
	nsresult rv;
	PRUint32 oldFlags;
	msgHdr->GetFlags(&oldFlags);

	SetHdrFlag(msgHdr, set, flag);

    PRUint32 flags;
    (void)msgHdr->GetFlags(&flags);
	PRUint32 key;
	rv = msgHdr->GetMessageKey(&key);
	if(NS_SUCCEEDED(rv))
		NotifyKeyChangeAll(key, oldFlags, flags, instigator);

	return rv;
}

// Helper routine - lowest level of flag setting - returns PR_TRUE if flags change,
// PR_FALSE otherwise.
PRBool nsMsgDatabase::SetHdrFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, MsgFlags flag)
{
//	PR_ASSERT(! (flag & kDirty));	// this won't do the right thing so don't.
	PRUint32 statusFlags;
    (void)msgHdr->GetFlags(&statusFlags);
	PRUint32 currentStatusFlags = GetStatusFlags(msgHdr, statusFlags);
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


NS_IMETHODIMP nsMsgDatabase::MarkHdrRead(nsIMsgDBHdr *msgHdr, PRBool bRead, 
                                         nsIDBChangeListener *instigator)
{
  nsresult rv = NS_OK;
  PRBool	isRead = PR_TRUE;
  PRUint32      msgFlags;

  (void) msgHdr->GetFlags(&msgFlags);
  IsHeaderRead(msgHdr, &isRead);
  // if the flag is already correct in the db, don't change it.
  // Check msg flags as well as IsHeaderRead in case it's a newsgroup
  // and the msghdr flags are out of sync with the newsrc settings.
  // (we could override this method for news db's, but it's a trivial fix here.
  if (!!isRead != !!bRead || (msgFlags & MSG_FLAG_READ != 0) != !!isRead)
  {
    nsCOMPtr <nsIMsgThread> threadHdr;
    nsMsgKey msgKey;
    msgHdr->GetMessageKey(&msgKey);
    
    rv = GetThreadForMsgKey(msgKey, getter_AddRefs(threadHdr));
    if (threadHdr)
    {
      threadHdr->MarkChildRead(bRead);
    }
    rv = MarkHdrReadInDB(msgHdr, bRead, instigator);
  }
  return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkHdrReplied(nsIMsgDBHdr *msgHdr, PRBool bReplied,
                         nsIDBChangeListener *instigator)
{
	return SetMsgHdrFlag(msgHdr, bReplied, MSG_FLAG_REPLIED, instigator);
}


NS_IMETHODIMP nsMsgDatabase::MarkHdrMarked(nsIMsgDBHdr *msgHdr, PRBool mark,
                         nsIDBChangeListener *instigator)
{
	return SetMsgHdrFlag(msgHdr, mark, MSG_FLAG_MARKED, instigator);
}


NS_IMETHODIMP nsMsgDatabase::MarkAllRead(nsMsgKeyArray *thoseMarked)
{
  nsresult		rv;
  nsMsgHdr		*pHeader;
  PRInt32			numChanged = 0;
  
  nsCOMPtr <nsISimpleEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
    return rv;
  PRBool hasMore = PR_FALSE;
  
  while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
  {
    rv = hdrs->GetNext((nsISupports**)&pHeader);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
    if (NS_FAILED(rv)) 
      break;
    
    if (thoseMarked) 
    {
      nsMsgKey key;
      (void)pHeader->GetMessageKey(&key);
      thoseMarked->Add(key);
    }
    rv = MarkHdrRead(pHeader, PR_TRUE, nsnull); 	// ### dmb - blow off error?
    numChanged++;
    NS_RELEASE(pHeader);
  }
  
  // force num new to 0.
  PRInt32 numNewMessages;
  
  rv = m_dbFolderInfo->GetNumNewMessages(&numNewMessages);
  if (rv == NS_OK)
    m_dbFolderInfo->ChangeNumNewMessages(-numNewMessages);
  // caller will Commit the db, so no need to do it here.
  return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkReadByDate (PRTime startDate, PRTime endDate, nsMsgKeyArray *markedIds)
{
	nsresult rv;
	nsMsgHdr	*pHeader;
	//ListContext		*listContext = NULL;
	PRInt32			numChanged = 0;

    nsISimpleEnumerator* hdrs;
    rv = EnumerateMessages(&hdrs);
    if (NS_FAILED(rv)) 
		return rv;
		
	nsTime t_startDate(startDate);
	nsTime t_endDate(endDate);
		
	PRBool hasMore = PR_FALSE;

	while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
	{
		rv = hdrs->GetNext((nsISupports**)&pHeader);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (NS_FAILED(rv)) break;

		PRTime headerDate;
        (void)pHeader->GetDate(&headerDate);
        nsTime t_headerDate(headerDate);
        
		if (t_headerDate > t_startDate && t_headerDate <= t_endDate)
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
		Commit(nsMsgDBCommitType::kSmallCommit);
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::MarkLater(nsMsgKey key, PRTime until)
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
    nsresult rv = NS_OK;

    if (!m_newSet) {
        m_newSet = nsMsgKeySet::Create("" /* , this */);
        if (!m_newSet) return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = m_newSet->Add(key);
    
	return rv;
}


NS_IMETHODIMP nsMsgDatabase::ClearNewList(PRBool notify /* = FALSE */)
{
	nsresult			err = NS_OK;
	if (m_newSet)
	{
		if (notify)	// need to update view
		{
			PRInt32 firstMember;
      nsMsgKeySet *saveNewSet = m_newSet;
      // set m_newSet to null so that the code that's listening to the key change
      // doesn't think we have new messages and send notifications all over
      // that we have new messages.
      m_newSet = nsnull;
			while ((firstMember = saveNewSet->GetFirstMember()) != 0)
			{
				saveNewSet->Remove(firstMember);	// this bites, since this will cause us to regen new list many times.
				nsIMsgDBHdr *msgHdr;
				err = GetMsgHdrForKey(firstMember, &msgHdr);
				if (NS_SUCCEEDED(err))
				{
                    nsMsgKey key;
                    (void)msgHdr->GetMessageKey(&key);
                    PRUint32 flags;
                    (void)msgHdr->GetFlags(&flags);
					NotifyKeyChangeAll(key, flags | MSG_FLAG_NEW, flags, NULL);
					NS_RELEASE(msgHdr);
				}
			}
      m_newSet = saveNewSet;
		}
		delete m_newSet;
		m_newSet = NULL;
	}
    else {
        NS_ASSERTION(0, "no set!\n");
    }
    return err;
}

NS_IMETHODIMP nsMsgDatabase::HasNew(PRBool *_retval)
{
	if (!_retval) return NS_ERROR_NULL_POINTER;

	if (m_newSet && m_newSet->getLength() > 0) {
		*_retval = PR_TRUE;
	}
	else {
		*_retval = PR_FALSE;
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetFirstNew(nsMsgKey *result)
{
	// even though getLength is supposedly for debugging only, it's the only
	// way I can tell if the set is empty (as opposed to having a member 0.
	PRBool hasnew;
	nsresult rv;

	rv = HasNew(&hasnew);
	if (NS_FAILED(rv)) return rv;
	if (hasnew)
		*result = m_newSet->GetFirstMember();
	else
		*result = nsMsgKey_None;
	return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////


class nsMsgDBEnumerator : public nsISimpleEnumerator {
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods:
    NS_DECL_NSISIMPLEENUMERATOR

    // nsMsgDBEnumerator methods:
    typedef nsresult (*nsMsgDBEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);

    nsMsgDBEnumerator(nsMsgDatabase* db, 
                      nsMsgDBEnumeratorFilter filter, void* closure);
    virtual ~nsMsgDBEnumerator();

protected:
	nsresult					GetRowCursor();
	nsresult					PrefetchNext();
    nsMsgDatabase*              mDB;
	nsIMdbTableRowCursor*       mRowCursor;
    nsIMsgDBHdr*                 mResultHdr;
    PRBool                      mDone;
	PRBool						mNextPrefetched;
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
	mNextPrefetched = PR_FALSE;
}

nsMsgDBEnumerator::~nsMsgDBEnumerator()
{
	if (mRowCursor)
		mRowCursor->CutStrongRef(mDB->GetEnv());
    NS_RELEASE(mDB);
	NS_IF_RELEASE(mResultHdr);
}

NS_IMPL_ISUPPORTS1(nsMsgDBEnumerator, nsISimpleEnumerator)

nsresult nsMsgDBEnumerator::GetRowCursor()
{
	nsresult rv = 0;
	mDone = PR_FALSE;

	if (!mDB || !mDB->m_mdbAllMsgHeadersTable)
		return NS_ERROR_NULL_POINTER;
		
	rv = mDB->m_mdbAllMsgHeadersTable->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
    return rv;
}

NS_IMETHODIMP nsMsgDBEnumerator::GetNext(nsISupports **aItem)
{
	if (!aItem)
		return NS_ERROR_NULL_POINTER;
	nsresult rv=NS_OK;
	if (!mNextPrefetched)
		rv = PrefetchNext();
	if (NS_SUCCEEDED(rv))
	{
		if (mResultHdr) 
		{
			*aItem = mResultHdr;
			NS_ADDREF(mResultHdr);
			mNextPrefetched = PR_FALSE;
		}
	}
	return rv;
}

nsresult nsMsgDBEnumerator::PrefetchNext()
{
	nsresult rv = NS_OK;
	nsIMdbRow* hdrRow;
	mdb_pos rowPos;
	PRUint32 flags;

	if (!mRowCursor)
	{
		rv = GetRowCursor();
		if (!NS_SUCCEEDED(rv))
			return rv;
	}

    do {
        NS_IF_RELEASE(mResultHdr);
        mResultHdr = nsnull;
        rv = mRowCursor->NextRow(mDB->GetEnv(), &hdrRow, &rowPos);
		if (!hdrRow) {
            mDone = PR_TRUE;
			return NS_ERROR_FAILURE;
        }
        if (NS_FAILED(rv)) {
            mDone = PR_TRUE;
            return rv;
        }
		//Get key from row
		mdbOid outOid;
		nsMsgKey key=0;
		if (hdrRow->GetOid(mDB->GetEnv(), &outOid) == NS_OK)
            key = outOid.mOid_Id;

		rv = mDB->GetHdrFromUseCache(key, &mResultHdr);
		if (NS_SUCCEEDED(rv) && mResultHdr)
			hdrRow->Release();
		else
			rv = mDB->CreateMsgHdr(hdrRow, key, &mResultHdr);
        if (NS_FAILED(rv))
			return rv;

		if (mResultHdr)
			mResultHdr->GetFlags(&flags);
		else
			flags = 0;
    } 
	while (mFilter && mFilter(mResultHdr, mClosure) != NS_OK && !(flags & MSG_FLAG_EXPUNGED));

    if (mResultHdr) 
	{
		mNextPrefetched = PR_TRUE;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgDBEnumerator::HasMoreElements(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;

	if (!mNextPrefetched)
		PrefetchNext();
	*aResult = !mDone;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMsgDatabase::EnumerateMessages(nsISimpleEnumerator* *result)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsnull, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}


// resulting output array is sorted by key.
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
			if (outPos < 0 || outOid.mOid_Id == (mdb_id) -1)	
				break;
			if (err == NS_OK)
				outputKeys.Add(outOid.mOid_Id);
		}
		rowCursor->Release();
	}
	outputKeys.QuickSort();
	return err;
}

class nsMsgDBThreadEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods:
    NS_DECL_NSISIMPLEENUMERATOR

    // nsMsgDBEnumerator methods:
    typedef nsresult (*nsMsgDBThreadEnumeratorFilter)(nsIMsgThread* thread);

    nsMsgDBThreadEnumerator(nsMsgDatabase* db, 
                      nsMsgDBThreadEnumeratorFilter filter);
    virtual ~nsMsgDBThreadEnumerator();

protected:
	nsresult					GetTableCursor(void);
	nsresult					PrefetchNext();
    nsMsgDatabase*              mDB;
	nsIMdbPortTableCursor*       mTableCursor;
    nsIMsgThread*                 mResultThread;
    PRBool                      mDone;
	PRBool						mNextPrefetched;
    nsMsgDBThreadEnumeratorFilter     mFilter;
};

nsMsgDBThreadEnumerator::nsMsgDBThreadEnumerator(nsMsgDatabase* db,
                                     nsMsgDBThreadEnumeratorFilter filter)
    : mDB(db), mTableCursor(nsnull), mResultThread(nsnull), mDone(PR_FALSE),
      mFilter(filter)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDB);
	mNextPrefetched = PR_FALSE;
}

nsMsgDBThreadEnumerator::~nsMsgDBThreadEnumerator()
{
	NS_IF_RELEASE(mTableCursor);
	NS_IF_RELEASE(mResultThread);
    NS_RELEASE(mDB);
}

NS_IMPL_ISUPPORTS1(nsMsgDBThreadEnumerator, nsISimpleEnumerator)

nsresult nsMsgDBThreadEnumerator::GetTableCursor(void)
{
	nsresult rv = 0;

	if (!mDB || !mDB->m_mdbStore)
		return NS_ERROR_NULL_POINTER;
		
	mDB->m_mdbStore->GetPortTableCursor(mDB->GetEnv(),   mDB->m_hdrRowScopeToken, mDB->m_threadTableKindToken,
	    &mTableCursor);

	if (NS_FAILED(rv)) 
		return rv;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDBThreadEnumerator::GetNext(nsISupports **aItem)
{
	if (!aItem)
		return NS_ERROR_NULL_POINTER;
	*aItem = nsnull;
	nsresult rv = NS_OK;
	if (!mNextPrefetched)
		rv = PrefetchNext();
	if (NS_SUCCEEDED(rv))
	{
		if (mResultThread) 
		{
			*aItem = mResultThread;
			NS_ADDREF(mResultThread);
			mNextPrefetched = PR_FALSE;
		}
	}
	return rv;
}


nsresult nsMsgDBThreadEnumerator::PrefetchNext()
{
	nsresult rv;
	nsIMdbTable *table = nsnull;

	if (!mTableCursor)
	{
		rv = GetTableCursor();
		if (!NS_SUCCEEDED(rv))
			return rv;
	}
    while (PR_TRUE) 
	{
        NS_IF_RELEASE(mResultThread);
        mResultThread = nsnull;
        rv = mTableCursor->NextTable(mDB->GetEnv(), &table);
		if (!table) 
		{
            mDone = PR_TRUE;
			return NS_ERROR_FAILURE;
        }
        if (NS_FAILED(rv)) 
		{
            mDone = PR_TRUE;
            return rv;
        }

        if (NS_FAILED(rv)) 
			return rv;

        mResultThread = new nsMsgThread(mDB, table);
		if(mResultThread)
		{
			PRUint32 numChildren = 0;
			NS_ADDREF(mResultThread);
			mResultThread->GetNumChildren(&numChildren);
			// we've got empty thread; don't tell caller about it.
			if (numChildren == 0)
				continue;
		}
		if (mFilter && mFilter(mResultThread) != NS_OK)
			continue;
		else
			break;
    }
    if (mResultThread) 
	{
		mNextPrefetched = PR_TRUE;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgDBThreadEnumerator::HasMoreElements(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	if (!mNextPrefetched)
		PrefetchNext();
	*aResult = !mDone;
    return NS_OK;
}

NS_IMETHODIMP 
nsMsgDatabase::EnumerateThreads(nsISimpleEnumerator* *result)
{
  nsMsgDBThreadEnumerator* e = new nsMsgDBThreadEnumerator(this, nsnull);
  if (e == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(e);
  *result = e;
  return NS_OK;
}

// only return headers with a particular flag set
static nsresult
nsMsgFlagSetFilter(nsIMsgDBHdr *msg, void *closure)
{
  PRUint32 msgFlags, desiredFlags;
  desiredFlags = * (PRUint32 *) closure;
  msg->GetFlags(&msgFlags);
  return (msgFlags & desiredFlags) ? NS_OK : NS_COMFALSE;
}

static nsresult
nsMsgUnreadFilter(nsIMsgDBHdr* msg, void* closure)
{
    nsMsgDatabase* db = (nsMsgDatabase*)closure;
    PRBool wasRead = PR_TRUE;
    nsresult rv = db->IsHeaderRead(msg, &wasRead);
    if (NS_FAILED(rv)) 
		return rv;
    return !wasRead ? NS_OK : NS_COMFALSE;
}

nsresult  
nsMsgDatabase::EnumerateUnreadMessages(nsISimpleEnumerator* *result)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsMsgUnreadFilter, this);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

nsresult  
nsMsgDatabase::EnumerateMessagesWithFlag(nsISimpleEnumerator* *result, PRUint32 *pFlag)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsMsgFlagSetFilter, pFlag);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

static nsresult
nsMsgReadFilter(nsIMsgDBHdr* msg, void* closure)
{
    nsMsgDatabase* db = (nsMsgDatabase*)closure;
    PRBool wasRead = PR_TRUE;
    nsresult rv = db->IsHeaderRead(msg, &wasRead);
    if (NS_FAILED(rv))
        return rv;
    return wasRead ? NS_OK : NS_COMFALSE;
}

// note that we can't just use EnumerateMessagesWithFlag(MSG_FLAG_READ) because we need
// to call IsHeaderRead.
nsresult
nsMsgDatabase::EnumerateReadMessages(nsISimpleEnumerator* *result)
{
    nsMsgDBEnumerator* e = new nsMsgDBEnumerator(this, nsMsgReadFilter, this);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::CreateNewHdr(nsMsgKey key, nsIMsgDBHdr **pnewHdr)
{
	nsresult	err = NS_OK;
	nsIMdbRow		*hdrRow;
	struct mdbOid allMsgHdrsTableOID;

	if (!pnewHdr || !m_mdbAllMsgHeadersTable || !m_mdbStore)
		return NS_ERROR_NULL_POINTER;

	allMsgHdrsTableOID.mOid_Scope = m_hdrRowScopeToken;
	allMsgHdrsTableOID.mOid_Id = key;	// presumes 0 is valid key value

	err = m_mdbStore->GetRow(GetEnv(), &allMsgHdrsTableOID, &hdrRow);
	if (!hdrRow)	
		err  = m_mdbStore->NewRowWithOid(GetEnv(), &allMsgHdrsTableOID, &hdrRow);

	if (NS_FAILED(err)) 
		return err;
    err = CreateMsgHdr(hdrRow, key, pnewHdr);
	return err;
}

NS_IMETHODIMP nsMsgDatabase::AddNewHdrToDB(nsIMsgDBHdr *newHdr, PRBool notify)
{
    nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, newHdr);          // closed system, cast ok
	PRBool newThread;

	nsresult err = ThreadNewHdr(hdr, newThread);
	// we thread header before we add it to the all headers table
	// so that subject threading will work (otherwise, when we try
	// to find the first header with the same subject, we get the
	// new header!
	if (NS_SUCCEEDED(err))
	{
		nsMsgKey key;
		PRUint32 flags;

		newHdr->GetMessageKey(&key);
		hdr->GetRawFlags(&flags);
    // use raw flags instead of GetFlags, because GetFlags will
    // pay attention to what's in m_newSet, and this new hdr isn't
    // in m_newSet yet.
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
            PRBool isRead = PR_TRUE;
            IsHeaderRead(newHdr, &isRead);
			if (!isRead) 
				m_dbFolderInfo->ChangeNumNewMessages(1);
      m_dbFolderInfo->SetHighWater(key, PR_FALSE);
		}

		err = m_mdbAllMsgHeadersTable->AddRow(GetEnv(), hdr->GetMDBRow());
		if (notify)
		{
			nsMsgKey threadParent;

			newHdr->GetThreadParent(&threadParent);
			NotifyKeyAddedAll(key, threadParent, flags, NULL);
		}
	}
	NS_ASSERTION(NS_SUCCEEDED(err), "error creating thread");
	return err;
}

NS_IMETHODIMP nsMsgDatabase::CopyHdrFromExistingHdr(nsMsgKey key, nsIMsgDBHdr *existingHdr, PRBool addHdrToDB, nsIMsgDBHdr **newHdr)
{
	nsresult	err = NS_OK;

	if (existingHdr)
	{
    if (key == nsMsgKey_None)
      return NS_MSG_MESSAGE_NOT_FOUND;

	  nsMsgHdr* sourceMsgHdr = NS_STATIC_CAST(nsMsgHdr*, existingHdr);      // closed system, cast ok
		nsMsgHdr *destMsgHdr = nsnull;
		CreateNewHdr(key, (nsIMsgDBHdr **) &destMsgHdr);
    if (!destMsgHdr)
      return NS_MSG_MESSAGE_NOT_FOUND;

		nsIMdbRow	*sourceRow = sourceMsgHdr->GetMDBRow() ;
		nsIMdbRow	*destRow = destMsgHdr->GetMDBRow();
		err = destRow->SetRow(GetEnv(), sourceRow);
		if (NS_SUCCEEDED(err))
		{
			if(addHdrToDB)
			    err = AddNewHdrToDB(destMsgHdr, PR_TRUE);
			if (NS_SUCCEEDED(err) && newHdr)
				*newHdr = destMsgHdr;
		}

	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnTonsString(nsIMdbRow *hdrRow, mdb_token columnToken, nsString &resultStr)
{
	nsresult	err = NS_OK;

	if (hdrRow)	// ### probably should be an error if hdrRow is NULL...
	{
		struct mdbYarn yarn;
		err = hdrRow->AliasCellYarn(GetEnv(), columnToken, &yarn);
		if (err == NS_OK)
			YarnTonsString(&yarn, &resultStr);
	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnTonsCString(nsIMdbRow *hdrRow, mdb_token columnToken, nsCString &resultStr)
{
	nsresult	err = NS_OK;

	if (hdrRow)	// ### probably should be an error if hdrRow is NULL...
	{
		struct mdbYarn yarn;
		err = hdrRow->AliasCellYarn(GetEnv(), columnToken, &yarn);
		if (err == NS_OK)
			YarnTonsCString(&yarn, &resultStr);
	}
	return err;
}

nsIMimeConverter *nsMsgDatabase::GetMimeConverter()
{
	if (!m_mimeConverter)
	{
		// apply mime decode
		nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                          NS_GET_IID(nsIMimeConverter), getter_AddRefs(m_mimeConverter));
	}
  return m_mimeConverter;
}

nsresult nsMsgDatabase::RowCellColumnToMime2DecodedString(nsIMdbRow *row, mdb_token columnToken, PRUnichar* *resultStr)
{
    nsresult err = NS_OK;
    nsCAutoString nakedString;
    err = RowCellColumnTonsCString(row, columnToken, nakedString);
    if (NS_SUCCEEDED(err) && nakedString.Length() > 0)
    {
        GetMimeConverter();
        if (m_mimeConverter) 
        {
            char *charset;
            nsAutoString decodedStr;
            PRBool characterSetOverride;
            m_dbFolderInfo->GetCharPtrCharacterSet(&charset);
            m_dbFolderInfo->GetCharacterSetOverride(&characterSetOverride);

            err = m_mimeConverter->DecodeMimeHeader(nakedString, resultStr, charset, characterSetOverride);
            PR_FREEIF(charset);
        }
    }
    return err;
}

nsresult nsMsgDatabase::RowCellColumnToAddressCollationKey(nsIMdbRow *row, mdb_token colToken, PRUint8 **result, PRUint32 *len)
{
    nsCAutoString cSender;
    nsXPIDLCString name;

    nsresult ret = RowCellColumnTonsCString(row, colToken, cSender);
    if (NS_SUCCEEDED(ret))
    {
        nsIMsgHeaderParser *headerParser = GetHeaderParser();
        if (headerParser)
        {
            // apply mime decode
            nsIMimeConverter *converter = GetMimeConverter();

            if (NS_SUCCEEDED(ret) && nsnull != converter) 
            {
                char *resultStr = nsnull;
                char *charset;
                PRBool characterSetOverride;
                m_dbFolderInfo->GetCharPtrCharacterSet(&charset);
                m_dbFolderInfo->GetCharacterSetOverride(&characterSetOverride);

                ret = converter->DecodeMimeHeader(cSender.get(), &resultStr,
                                                  charset, characterSetOverride);
                if (NS_SUCCEEDED(ret) && resultStr)
                {
                    ret = headerParser->ExtractHeaderAddressName ("UTF-8", resultStr, getter_Copies(name));
                }
                else {
                  ret = headerParser->ExtractHeaderAddressName ("UTF-8", cSender.get(), getter_Copies(name));
                }
                PR_FREEIF(resultStr);
                PR_FREEIF(charset);
            }

        }
    }
    if (NS_SUCCEEDED(ret))
    {
        nsAutoString nameStr;
        nameStr.AssignWithConversion(name);
        ret = CreateCollationKey(nameStr.get(), result, len);
    }

    return ret;
}

nsresult nsMsgDatabase::GetCollationKeyGenerator()
{
	nsresult err = NS_OK;
	if (!m_collationKeyGenerator)
	{
		nsCOMPtr <nsILocale> locale; 
		nsString localeName; 

		// get a locale service 
		nsCOMPtr <nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &err);
		if (NS_SUCCEEDED(err))
		{
			// do this for a new db if no UI to be provided for locale selection 
			err = localeService->GetApplicationLocale(getter_AddRefs(locale)); 

			if (locale)
			{
				// or generate a locale from a stored locale name ("en_US", "fr_FR") 
				//err = localeFactory->NewLocale(&localeName, &locale); 

				nsCOMPtr <nsICollationFactory> f;

				err = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL,
										  NS_GET_IID(nsICollationFactory), getter_AddRefs(f)); 
				if (NS_SUCCEEDED(err) && f)
				{
					// get a collation interface instance 
					err = f->CreateCollation(locale, getter_AddRefs(m_collationKeyGenerator));

				}
			}
		}
	}
	return err;
}

nsresult nsMsgDatabase::RowCellColumnToCollationKey(nsIMdbRow *row, mdb_token columnToken, PRUint8 **result, PRUint32 *len)
{
  nsXPIDLString nakedString;
	nsresult err;

	err = RowCellColumnToMime2DecodedString(row, columnToken, getter_Copies(nakedString));
	if (NS_SUCCEEDED(err))
		err = CreateCollationKey((const PRUnichar *)nakedString, result, len);

	return err;
}

NS_IMETHODIMP 
nsMsgDatabase::CompareCollationKeys(PRUint8 *key1, PRUint32 len1, PRUint8 *key2, PRUint32 len2, PRInt32 *result)
{
  nsresult rv = GetCollationKeyGenerator();
  NS_ENSURE_SUCCESS(rv,rv);
  if (!m_collationKeyGenerator) return NS_ERROR_FAILURE;

  rv = m_collationKeyGenerator->CompareRawSortKey(key1,len1,key2,len2,result);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP 
nsMsgDatabase::CreateCollationKey(const PRUnichar *sourceString, PRUint8 **result, PRUint32 *len)
{
  nsresult err = GetCollationKeyGenerator();
  NS_ENSURE_SUCCESS(err,err);
  if (!m_collationKeyGenerator) return NS_ERROR_FAILURE;

  nsAutoString sourceStr(sourceString);
  err = m_collationKeyGenerator->GetSortKeyLen(kCollationCaseInSensitive, sourceStr, len);
  NS_ENSURE_SUCCESS(err,err);

  *result = (PRUint8 *) PR_Malloc(*len);
  if (!result) return NS_ERROR_OUT_OF_MEMORY;

  err = m_collationKeyGenerator->CreateRawSortKey(kCollationCaseInSensitive, sourceStr, *result, len);
  NS_ENSURE_SUCCESS(err,err);
  return err;
}

nsIMsgHeaderParser *nsMsgDatabase::GetHeaderParser()
{

	if (!m_HeaderParser)
	{
		nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
													NULL, 
													NS_GET_IID(nsIMsgHeaderParser), 
													(void **) &m_HeaderParser);
		if (!NS_SUCCEEDED(rv))
			m_HeaderParser = nsnull;
	}
	return m_HeaderParser;
}


nsresult nsMsgDatabase::RowCellColumnToUInt32(nsIMdbRow *hdrRow, mdb_token columnToken, PRUint32 &uint32Result, PRUint32 defaultValue)
{
	return RowCellColumnToUInt32(hdrRow, columnToken, &uint32Result, defaultValue);
}

nsresult nsMsgDatabase::RowCellColumnToUInt32(nsIMdbRow *hdrRow, mdb_token columnToken, PRUint32 *uint32Result, PRUint32 defaultValue)
{
	nsresult	err = NS_OK;

	if (uint32Result)
		*uint32Result = defaultValue;
	if (hdrRow)	// ### probably should be an error if hdrRow is NULL...
	{
		struct mdbYarn yarn;
		err = hdrRow->AliasCellYarn(GetEnv(), columnToken, &yarn);
		if (err == NS_OK)
			YarnToUInt32(&yarn, uint32Result);
	}
	return err;
}

nsresult nsMsgDatabase::UInt32ToRowCellColumn(nsIMdbRow *row, mdb_token columnToken, PRUint32 value)
{
	struct mdbYarn yarn;
	char	yarnBuf[100];

	yarn.mYarn_Buf = (void *) yarnBuf;
	yarn.mYarn_Size = sizeof(yarnBuf);
	yarn.mYarn_Fill = yarn.mYarn_Size;
	yarn.mYarn_Form = 0;
	yarn.mYarn_Grow = NULL;
	return row->AddColumn(GetEnv(),  columnToken, UInt32ToYarn(&yarn, value));
}

nsresult nsMsgDatabase::CharPtrToRowCellColumn(nsIMdbRow *row, mdb_token columnToken, const char *charPtr)
{
	struct mdbYarn yarn;
	yarn.mYarn_Buf = (void *) charPtr;
	yarn.mYarn_Size = PL_strlen((const char *) yarn.mYarn_Buf) + 1;
	yarn.mYarn_Fill = yarn.mYarn_Size - 1;
	yarn.mYarn_Form = 0;	// what to do with this? we're storing csid in the msg hdr...

	return row->AddColumn(GetEnv(),  columnToken, &yarn);
}

// caller must PR_FREEIF result
nsresult nsMsgDatabase::RowCellColumnToCharPtr(nsIMdbRow *row, mdb_token columnToken, char **result)
{
	nsresult	err = NS_ERROR_NULL_POINTER;

	if (row && result)
	{
		struct mdbYarn yarn;
		err = row->AliasCellYarn(GetEnv(), columnToken, &yarn);
		if (err == NS_OK)
		{
			*result = (char *) PR_CALLOC(yarn.mYarn_Fill + 1);
			if (*result)
			{
				if (yarn.mYarn_Fill > 0)
					nsCRT::memcpy(*result, yarn.mYarn_Buf, yarn.mYarn_Fill);
				else
					**result = 0;
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;

		}
		else if (err == NS_OK)	// guarantee a non-null result
			*result = nsCRT::strdup("");
	}
	return err;
}



/* static */struct mdbYarn *nsMsgDatabase::nsStringToYarn(struct mdbYarn *yarn, nsString *str)
{
	yarn->mYarn_Buf = ToNewCString(*str);
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
	str->AssignWithConversion((const char *) yarn->mYarn_Buf, yarn->mYarn_Fill);
}

/* static */void nsMsgDatabase::YarnTonsCString(struct mdbYarn *yarn, nsCString *str)
{
	str->Assign((const char *) yarn->mYarn_Buf, yarn->mYarn_Fill);
}

// WARNING - if yarn is empty, *pResult will not be changed!!!!
// this is so we can leave default values as they were.
/* static */void nsMsgDatabase::YarnToUInt32(struct mdbYarn *yarn, PRUint32 *pResult)
{
	PRUint32 result;
	char *p = (char *) yarn->mYarn_Buf;
	PRInt32 numChars = PR_MIN(8, yarn->mYarn_Fill);
	PRInt32 i;
 
  if (numChars > 0)
  {
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
}

/* static */void nsMsgDatabase::PRTime2Seconds(PRTime prTime, PRUint32 *seconds)
{
	PRInt64 microSecondsPerSecond, intermediateResult;
	
	LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
	LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
    LL_L2UI((*seconds), intermediateResult);
}

/* static */void nsMsgDatabase::Seconds2PRTime(PRUint32 seconds, PRTime *prTime)
{
	PRInt64 microSecondsPerSecond, intermediateResult;
	
	LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
    LL_UI2L(intermediateResult, seconds);
	LL_MUL((*prTime), intermediateResult, microSecondsPerSecond);
}


nsresult nsMsgDatabase::GetProperty(nsIMdbRow *row, const char *propertyName, char **result)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	if (m_mdbStore)
		err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	else
		err = NS_ERROR_NULL_POINTER;
	if (err == NS_OK)
		err = RowCellColumnToCharPtr(row, property_token, result);

	return err;
}

nsresult nsMsgDatabase::SetProperty(nsIMdbRow *row, const char *propertyName, const char *propertyVal)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
    CharPtrToRowCellColumn(row, property_token, propertyVal);
	return err;
}

nsresult nsMsgDatabase::GetPropertyAsNSString(nsIMdbRow *row, const char *propertyName, nsString *result)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

  NS_ENSURE_ARG(result);
	err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		err = RowCellColumnTonsString(row, property_token, *result);

	return err;
}

nsresult nsMsgDatabase::SetPropertyFromNSString(nsIMdbRow *row, const char *propertyName, nsString *propertyVal)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		return SetNSStringPropertyWithToken(row, property_token, propertyVal);

	return err;
}


nsresult nsMsgDatabase::GetUint32Property(nsIMdbRow *row, const char *propertyName, PRUint32 *result, PRUint32 defaultValue)
{
	nsresult err = NS_OK;
	mdb_token	property_token;

	err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
		err = RowCellColumnToUInt32(row, property_token, result, defaultValue);

	return err;
}

nsresult nsMsgDatabase::SetUint32Property(nsIMdbRow *row, const char *propertyName, PRUint32 propertyVal)
{
	struct mdbYarn yarn;
	char	int32StrBuf[20];
	yarn.mYarn_Buf = int32StrBuf;
	yarn.mYarn_Size = sizeof(int32StrBuf);
	yarn.mYarn_Fill = sizeof(int32StrBuf);

	mdb_token	property_token;

	nsresult err = m_mdbStore->StringToToken(GetEnv(),  propertyName, &property_token);
	if (err == NS_OK)
	{
		UInt32ToYarn(&yarn, propertyVal);
		err = row->AddColumn(GetEnv(), property_token, &yarn);
	}
	return err;
}

nsresult nsMsgDatabase::SetNSStringPropertyWithToken(nsIMdbRow *row, mdb_token aProperty, nsString *propertyStr)
{
  NS_ENSURE_ARG(row);
	struct mdbYarn yarn;

	yarn.mYarn_Grow = NULL;
	nsresult err = row->AddColumn(GetEnv(), aProperty, nsStringToYarn(&yarn, propertyStr));
	nsMemory::Free((char *)yarn.mYarn_Buf);	// won't need this when we have nsCString
	return err;
}


PRUint32 nsMsgDatabase::GetCurVersion()
{
	return kMsgDBVersion;
}

NS_IMETHODIMP nsMsgDatabase::SetSummaryValid(PRBool valid /* = PR_TRUE */)
{
  // setting the version to 0 ought to make it pretty invalid.
  if (!valid)
    m_dbFolderInfo->SetVersion(0);
  
  // for default db (and news), there's no nothing to set to make it it valid
  return NS_OK;
}

// protected routines

nsresult nsMsgDatabase::CreateNewThread(nsMsgKey threadId, const char *subject, nsMsgThread **pnewThread)
{
	nsresult	err = NS_OK;
	nsIMdbTable		*threadTable;
	struct mdbOid threadTableOID;
	struct mdbOid allThreadsTableOID;

	if (!pnewThread || !m_mdbStore)
		return NS_ERROR_NULL_POINTER;

	threadTableOID.mOid_Scope = m_hdrRowScopeToken;
	threadTableOID.mOid_Id = threadId;

	err  = GetStore()->NewTableWithOid(GetEnv(), &threadTableOID, m_threadTableKindToken, 
                                     PR_FALSE, nsnull, &threadTable);
	if (NS_FAILED(err)) 
		return err;

	allThreadsTableOID.mOid_Scope = m_threadRowScopeToken;
	allThreadsTableOID.mOid_Id = threadId;	

	// add a row for this thread in the table of all threads that we'll use
	// to do our mapping between subject strings and threads.
	nsIMdbRow *threadRow = nsnull;

	err = m_mdbStore->GetRow(GetEnv(), &allThreadsTableOID, &threadRow);
	if (!threadRow)	
	{
		err  = m_mdbStore->NewRowWithOid(GetEnv(), &allThreadsTableOID, &threadRow);
		if (NS_SUCCEEDED(err) && threadRow)
		{
			err = CharPtrToRowCellColumn(threadRow, m_threadSubjectColumnToken, subject);
			threadRow->Release();
		}
	}

	*pnewThread = new nsMsgThread(this, threadTable);
	if (*pnewThread)
		(*pnewThread)->SetThreadKey(threadId);
	return err;
}


nsIMsgThread *nsMsgDatabase::GetThreadForReference(nsCString &msgID, nsIMsgDBHdr **pMsgHdr)
{
	nsIMsgDBHdr	*msgHdr = GetMsgHdrForMessageID(msgID);  
	nsIMsgThread *thread = NULL;

	if (msgHdr != NULL)
	{
		nsMsgKey threadId;
		if (NS_SUCCEEDED(msgHdr->GetThreadId(&threadId)))
		{
			// find thread header for header whose message id we matched.
			thread = GetThreadForThreadId(threadId);
		}
		if (pMsgHdr)
			*pMsgHdr = msgHdr;
		else
			msgHdr->Release();
	}
	return thread;
}

nsIMsgThread *	nsMsgDatabase::GetThreadForSubject(nsCString &subject)
{
	nsIMsgThread *thread = nsnull;

	mdbYarn	subjectYarn;

	subjectYarn.mYarn_Buf = (void*)subject.get();
	subjectYarn.mYarn_Fill = PL_strlen(subject.get());
	subjectYarn.mYarn_Form = 0;
	subjectYarn.mYarn_Size = subjectYarn.mYarn_Fill;

	nsIMdbRow	*threadRow;
	mdbOid		outRowId;
  if (m_mdbStore)
  {
	  mdb_err result = m_mdbStore->FindRow(GetEnv(), m_threadRowScopeToken,
		  m_threadSubjectColumnToken, &subjectYarn,  &outRowId, &threadRow);
	  if (NS_SUCCEEDED(result) && threadRow)
	  {
		  //Get key from row
		  mdbOid outOid;
		  nsMsgKey key = 0;
		  if (threadRow->GetOid(GetEnv(), &outOid) == NS_OK)
			  key = outOid.mOid_Id;
		  // find thread header for header whose message id we matched.
		  thread = GetThreadForThreadId(key);
	  }
  }
	return thread;
}

nsresult nsMsgDatabase::ThreadNewHdr(nsMsgHdr* newHdr, PRBool &newThread)
{
	nsresult result=NS_ERROR_UNEXPECTED;
	nsCOMPtr <nsIMsgThread> thread;
	nsCOMPtr <nsIMsgDBHdr> replyToHdr;
	nsMsgKey threadId = nsMsgKey_None;

	if (!newHdr)
		return NS_ERROR_NULL_POINTER;

	PRUint16 numReferences = 0;
	PRUint32 newHdrFlags = 0;

	newHdr->GetFlags(&newHdrFlags);
	newHdr->GetNumReferences(&numReferences);

#define SUBJ_THREADING 1// try reference threading first
	for (PRInt32 i = numReferences - 1; i >= 0;  i--)
	{
		nsCAutoString reference;

		newHdr->GetStringReference(i, reference);
		// first reference we have hdr for is best top-level hdr.
		// but we have to handle case of promoting new header to top-level
		// in case the top-level header comes after a reply.

		if (reference.Length() == 0)
			break;

		thread = getter_AddRefs(GetThreadForReference(reference, getter_AddRefs(replyToHdr))) ;
		if (thread)
		{
			thread->GetThreadKey(&threadId);
			newHdr->SetThreadId(threadId);
			result = AddToThread(newHdr, thread, replyToHdr, PR_TRUE);
			break;
		}
	}
#ifdef SUBJ_THREADING
	// try subject threading if we couldn't find a reference and the subject starts with Re:
	nsXPIDLCString subject;

	newHdr->GetSubject(getter_Copies(subject));
	if ((ThreadBySubjectWithoutRe() || (newHdrFlags & MSG_FLAG_HAS_RE)) && (!thread))
	{
		nsCAutoString cSubject(subject);
		thread = getter_AddRefs(GetThreadForSubject(cSubject));
		if(thread)
		{
			thread->GetThreadKey(&threadId);
			newHdr->SetThreadId(threadId);
			//TRACE("threading based on subject %s\n", (const char *) msgHdr->m_subject);
			// if we move this and do subject threading after, ref threading, 
			// don't thread within children, since we know it won't work. But for now, pass TRUE.
			result = AddToThread(newHdr, thread, nsnull, PR_TRUE);     
		}
	}
#endif // SUBJ_THREADING

	if (!thread)
	{
		// couldn't find any parent articles - msgHdr is top-level thread, for now
		result = AddNewThread(newHdr);
		newThread = PR_TRUE;
	}
	else
	{
		newThread = PR_FALSE;
	}
	return result;
}

nsresult nsMsgDatabase::AddToThread(nsMsgHdr *newHdr, nsIMsgThread *thread, nsIMsgDBHdr *inReplyTo, PRBool threadInThread)
{
	// don't worry about real threading yet.
	nsCOMPtr <nsIDBChangeAnnouncer> announcer = do_QueryInterface(this);

	return thread->AddChild(newHdr, inReplyTo, threadInThread, announcer);
}

nsMsgHdr	*	nsMsgDatabase::GetMsgHdrForReference(nsCString &reference)
{
	NS_ASSERTION(PR_FALSE, "not implemented yet.");
	return nsnull;
}

nsIMsgDBHdr *nsMsgDatabase::GetMsgHdrForMessageID(nsCString &msgID)
{
	nsIMsgDBHdr	*msgHdr = nsnull;
    nsresult rv = NS_OK;
	mdbYarn	messageIdYarn;

	messageIdYarn.mYarn_Buf = (void*)msgID.get();
	messageIdYarn.mYarn_Fill = PL_strlen(msgID.get());
	messageIdYarn.mYarn_Form = 0;
	messageIdYarn.mYarn_Size = messageIdYarn.mYarn_Fill;

	nsIMdbRow	*hdrRow;
	mdbOid		outRowId;
	mdb_err result = GetStore()->FindRow(GetEnv(), m_hdrRowScopeToken,
		m_messageIdColumnToken, &messageIdYarn,  &outRowId, 
		&hdrRow);
	if (NS_SUCCEEDED(result) && hdrRow)
	{
		//Get key from row
		mdbOid outOid;
		nsMsgKey key=0;
		if (hdrRow->GetOid(GetEnv(), &outOid) == NS_OK)
			key = outOid.mOid_Id;
		rv = GetHdrFromUseCache(key, &msgHdr);
		if (NS_SUCCEEDED(rv) && msgHdr)
			hdrRow->Release();
		else
			rv = CreateMsgHdr(hdrRow, key, &msgHdr);
	}
	return msgHdr;
}

nsIMsgDBHdr *nsMsgDatabase::GetMsgHdrForSubject(nsCString &subject)
{
	nsIMsgDBHdr	*msgHdr = nsnull;
    nsresult rv = NS_OK;
	mdbYarn	subjectYarn;

	subjectYarn.mYarn_Buf = (void*)subject.get();
	subjectYarn.mYarn_Fill = PL_strlen(subject.get());
	subjectYarn.mYarn_Form = 0;
	subjectYarn.mYarn_Size = subjectYarn.mYarn_Fill;

	nsIMdbRow	*hdrRow;
	mdbOid		outRowId;
	mdb_err result = GetStore()->FindRow(GetEnv(), m_hdrRowScopeToken,
		m_subjectColumnToken, &subjectYarn,  &outRowId, 
		&hdrRow);
	if (NS_SUCCEEDED(result) && hdrRow)
	{
		//Get key from row
		mdbOid outOid;
		nsMsgKey key=0;
		if (hdrRow->GetOid(GetEnv(), &outOid) == NS_OK)
			key = outOid.mOid_Id;
		rv = GetHdrFromUseCache(key, &msgHdr);
		if (NS_SUCCEEDED(rv) && msgHdr)
			hdrRow->Release();
		else
			rv = CreateMsgHdr(hdrRow, key, &msgHdr);
	}
	return msgHdr;
}

NS_IMETHODIMP nsMsgDatabase::GetThreadContainingMsgHdr(nsIMsgDBHdr *msgHdr, nsIMsgThread **result)
{
	if (!result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	nsMsgKey threadId = nsMsgKey_None;
	(void)msgHdr->GetThreadId(&threadId);
	if (threadId != nsMsgKey_None)
		*result = GetThreadForThreadId(threadId);

	return (*result) ? NS_OK : NS_ERROR_FAILURE;
}


nsresult nsMsgDatabase::GetThreadForMsgKey(nsMsgKey msgKey, nsIMsgThread **result)
{
	nsresult ret = NS_OK;
	if (!result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;

	nsIMsgDBHdr *msg = NULL;
    ret = GetMsgHdrForKey(msgKey, &msg);

	if (NS_SUCCEEDED(ret) && msg)
	{
		ret = GetThreadContainingMsgHdr(msg, result);
		NS_RELEASE(msg);
	}

	return ret;
}

// caller needs to unrefer.
nsIMsgThread *	nsMsgDatabase::GetThreadForThreadId(nsMsgKey threadId)
{

	nsMsgThread		*pThread = nsnull;
	if (m_mdbStore)
	{
		mdbOid tableId;
		tableId.mOid_Id = threadId;
		tableId.mOid_Scope = m_hdrRowScopeToken;

		nsIMdbTable *threadTable;
		mdb_err res = m_mdbStore->GetTable(GetEnv(), &tableId, &threadTable);
		
		if (NS_SUCCEEDED(res) && threadTable)
		{
			pThread = new nsMsgThread(this, threadTable);
			if(pThread)
				NS_ADDREF(pThread);
		}
	}
	return pThread;
}

// make the passed in header a thread header
nsresult nsMsgDatabase::AddNewThread(nsMsgHdr *msgHdr)
{
  
  if (!msgHdr)
    return NS_ERROR_NULL_POINTER;
  
  nsMsgThread *threadHdr = nsnull;
  
  nsXPIDLCString subject;
  nsMsgKey threadKey = msgHdr->m_messageKey;
  // can't have a thread with key 1 since that's the table id of the all msg hdr table,
  // so give it kTableKeyForThreadOne (0xfffffffe).
  if (threadKey == 1)
    threadKey = kTableKeyForThreadOne;

  nsresult err = msgHdr->GetSubject(getter_Copies(subject));
  
  err = CreateNewThread(threadKey, subject, &threadHdr);
  msgHdr->SetThreadId(threadKey);
  if (threadHdr)
  {
    //		nsCString subject;
    
    threadHdr->AddRef();
    //		err = msgHdr->GetSubject(subject);
    //		threadHdr->SetThreadKey(msgHdr->m_messageKey);
    //		threadHdr->SetSubject(subject.get());
    
    // need to add the thread table to the db.
    AddToThread(msgHdr, threadHdr, nsnull, PR_FALSE);
    
    threadHdr->Release();
  }
  return err;
}


// should we thread messages with common subjects that don't start with Re: together?
// I imagine we might have separate preferences for mail and news, so this is a virtual method.
PRBool	nsMsgDatabase::ThreadBySubjectWithoutRe()
{
	return PR_TRUE;
}

nsresult nsMsgDatabase::GetBoolPref(const char *prefName, PRBool *result)
{
	PRBool prefValue = PR_FALSE;
	nsresult rv;
	nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    if (NS_SUCCEEDED(rv) && prefs)
	{
		rv = prefs->GetBoolPref(prefName, &prefValue);
		*result = prefValue;
	}
	return rv;
}

nsresult nsMsgDatabase::ListAllThreads(nsMsgKeyArray *threadIds)
{
	nsresult		rv;
	nsMsgThread		*pThread;

    nsCOMPtr <nsISimpleEnumerator> threads;
    rv = EnumerateThreads(getter_AddRefs(threads));
    if (NS_FAILED(rv)) return rv;
	PRBool hasMore = PR_FALSE;

	while (NS_SUCCEEDED(rv = threads->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
	{
        rv = threads->GetNext((nsISupports**)&pThread);
        NS_ENSURE_SUCCESS(rv,rv);

		if (threadIds) {
            nsMsgKey key;
            (void)pThread->GetThreadKey(&key);
			threadIds->Add(key);
        }
//		NS_RELEASE(pThread);
		pThread = nsnull;
	}
	return rv;
}

NS_IMETHODIMP nsMsgDatabase::GetOfflineOpForKey(nsMsgKey msgKey, PRBool create, nsIMsgOfflineImapOperation **offlineOp)
{
  NS_ASSERTION(PR_FALSE, "overridden by nsMailDatabase");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  nsMsgDatabase::RemoveOfflineOp(nsIMsgOfflineImapOperation *op)
{
  NS_ASSERTION(PR_FALSE, "overridden by nsMailDatabase");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMsgDatabase::ListAllOfflineMsgs(nsMsgKeyArray *outputKeys)
{
  nsCOMPtr <nsISimpleEnumerator> enumerator;
  PRUint32 flag = MSG_FLAG_OFFLINE;
  // if we change this routine to return an enumerator that generates the keys
  // one by one, we'll need to somehow make a copy of flag for the enumerator
  // to own, since the enumerator will persist past the life of flag on the stack.
  nsresult rv = EnumerateMessagesWithFlag(getter_AddRefs(enumerator), &flag);
  if (NS_SUCCEEDED(rv) && enumerator)
  {
		PRBool hasMoreElements;
		while(NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements)) && hasMoreElements)
		{
			nsCOMPtr<nsISupports> childSupports;
			rv = enumerator->GetNext(getter_AddRefs(childSupports));
			if(NS_FAILED(rv))
				return rv;

      // clear out db hdr, because it won't be valid when we get rid of the .msf file
		  nsCOMPtr<nsIMsgDBHdr> dbMessage(do_QueryInterface(childSupports, &rv));
		  if(NS_SUCCEEDED(rv) && dbMessage)
      {
        nsMsgKey msgKey;
			  dbMessage->GetMessageKey(&msgKey);
        outputKeys->Add(msgKey);
      }
    }
  }
  outputKeys->QuickSort();

  return rv;
}

NS_IMETHODIMP nsMsgDatabase::EnumerateOfflineOps(nsISimpleEnumerator **enumerator)
{
  NS_ASSERTION(PR_FALSE, "overridden by nsMailDatabase");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDatabase::ListAllOfflineOpIds(nsMsgKeyArray *offlineOpIds)
{
  NS_ASSERTION(PR_FALSE, "overridden by nsMailDatabase");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDatabase::ListAllOfflineDeletes(nsMsgKeyArray *offlineDeletes)
{
	nsresult ret = NS_OK;
	if (!offlineDeletes)
		return NS_ERROR_NULL_POINTER;

	// technically, notimplemented, but no one's putting offline ops in anyway.
	return ret;
}
NS_IMETHODIMP nsMsgDatabase::GetHighWaterArticleNum(nsMsgKey *key)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsMsgDatabase::GetLowWaterArticleNum(nsMsgKey *key)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsMsgKey NextPseudoMsgKey */

NS_IMETHODIMP nsMsgDatabase::GetNextPseudoMsgKey(nsMsgKey *nextPseudoMsgKey)
{
  NS_ENSURE_ARG_POINTER(nextPseudoMsgKey);
  *nextPseudoMsgKey = m_nextPseudoMsgKey--;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::SetNextPseudoMsgKey(nsMsgKey nextPseudoMsgKey)
{
  m_nextPseudoMsgKey = nextPseudoMsgKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetNextFakeOfflineMsgKey(nsMsgKey *nextFakeOfflineMsgKey)
{
  NS_ENSURE_ARG_POINTER(nextFakeOfflineMsgKey);
  // iterate over hdrs looking for first non-existant fake offline msg key
	nsMsgKey fakeMsgKey = kIdStartOfFake;

  PRBool containsKey;
  do
  {
     ContainsKey(fakeMsgKey, &containsKey);
     if (!containsKey)
       break;
     fakeMsgKey--;
  }
  while (containsKey);

  *nextFakeOfflineMsgKey = fakeMsgKey;
  return NS_OK;
}


NS_IMETHODIMP nsMsgDatabase::HasThreads(PRBool *hasThreads)
{
    nsresult rv;

    nsCOMPtr <nsISimpleEnumerator> threads;
    rv = EnumerateThreads(getter_AddRefs(threads));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = threads->HasMoreElements(hasThreads);
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

#ifdef DEBUG
nsresult nsMsgDatabase::DumpContents()
{
    nsMsgKey key;
    PRUint32 i;

    nsMsgKeyArray keys;
    nsresult rv = ListAllKeys(keys);
    for (i = 0; i < keys.GetSize(); i++) {
        key = keys[i];
		nsIMsgDBHdr *msg = NULL;
        rv = GetMsgHdrForKey(key, &msg);
        nsMsgHdr* msgHdr = NS_STATIC_CAST(nsMsgHdr*, msg);      // closed system, cast ok
		if (NS_SUCCEEDED(rv))
		{
            nsXPIDLCString author;
            nsXPIDLCString subject;

			msgHdr->GetMessageKey(&key);
			msgHdr->GetAuthor(getter_Copies(author));
			msgHdr->GetSubject(getter_Copies(subject));
			printf("hdr key = %u, author = %s subject = %s\n", key,
                   ((const char *)author) ? (const char *)author : "",
                   ((const char*)subject) ? (const char*)subject : "");
			NS_RELEASE(msgHdr);
		}
    }
	nsMsgKeyArray threads;
    rv = ListAllThreads(&threads);
    for ( i = 0; i < threads.GetSize(); i++) 
	{
        key = threads[i];
		printf("thread key = %u\n", key);
//		DumpThread(key);
    }


    return NS_OK;
}

nsresult nsMsgDatabase::DumpMsgChildren(nsIMsgDBHdr *msgHdr)
{
	return NS_OK;
}

nsresult	nsMsgDatabase::DumpThread(nsMsgKey threadId)
{
	nsresult rv = NS_OK;
	nsIMsgThread	*thread = nsnull;

	thread = GetThreadForThreadId(threadId);
	if (thread)
	{
		nsISimpleEnumerator *enumerator = nsnull;

		rv = thread->EnumerateMessages(nsMsgKey_None, &enumerator);
		if (NS_SUCCEEDED(rv) && enumerator)
		{
			PRBool hasMore = PR_FALSE;

			while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
			{
				nsIMsgDBHdr *pMessage = nsnull;
				rv = enumerator->GetNext((nsISupports**)&pMessage);
				NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
				if (NS_FAILED(rv)) 
					break;

		//		NS_RELEASE(pMessage);
				pMessage = nsnull;
			}
			NS_RELEASE(enumerator);

		}
	}
	return rv;
}
#endif /* DEBUG */

NS_IMETHODIMP nsMsgDatabase::SetMsgRetentionSettings(nsIMsgRetentionSettings *retentionSettings)
{
  m_retentionSettings = retentionSettings;
  if (retentionSettings && m_dbFolderInfo)
  {
    nsresult rv;

    nsMsgRetainByPreference retainByPreference;
    PRUint32 daysToKeepHdrs;
    PRUint32 numHeadersToKeep;
    PRBool keepUnreadMessagesOnly;
    PRUint32 daysToKeepBodies;
    PRBool cleanupBodiesByDays;

    rv = retentionSettings->GetRetainByPreference(&retainByPreference);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = retentionSettings->GetDaysToKeepHdrs(&daysToKeepHdrs);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = retentionSettings->GetNumHeadersToKeep(&numHeadersToKeep);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = retentionSettings->GetKeepUnreadMessagesOnly(&keepUnreadMessagesOnly);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = retentionSettings->GetDaysToKeepBodies(&daysToKeepBodies);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = retentionSettings->GetCleanupBodiesByDays(&cleanupBodiesByDays);
    // need to write this to the db. We'll just use the dbfolderinfo to write properties.
    m_dbFolderInfo->SetUint32Property("retainBy", retainByPreference);
    m_dbFolderInfo->SetUint32Property("daysToKeepHdrs", daysToKeepHdrs);
    m_dbFolderInfo->SetUint32Property("numHdrsToKeep", numHeadersToKeep);
    m_dbFolderInfo->SetUint32Property("daysToKeepBodies", daysToKeepBodies);
    m_dbFolderInfo->SetUint32Property("keepUnreadOnly", (keepUnreadMessagesOnly) ? 1 : 0);
    m_dbFolderInfo->SetBooleanProperty("cleanupBodies", cleanupBodiesByDays);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetMsgRetentionSettings(nsIMsgRetentionSettings **retentionSettings)
{
  NS_ENSURE_ARG_POINTER(retentionSettings);
  if (!m_retentionSettings)
  {
    // create a new one, and initialize it from the db.
    m_retentionSettings = new nsMsgRetentionSettings;
    if (m_retentionSettings && m_dbFolderInfo)
    {
      nsresult rv;

      nsMsgRetainByPreference retainByPreference;
      PRUint32 daysToKeepHdrs = 0;
      PRUint32 numHeadersToKeep = 0;
      PRUint32 keepUnreadMessagesProp = 0;
      PRBool keepUnreadMessagesOnly = PR_FALSE;
      PRBool useServerDefaults;
      PRUint32 daysToKeepBodies = 0;
      PRBool cleanupBodiesByDays = PR_FALSE;

      rv = m_dbFolderInfo->GetUint32Property("retainBy", &retainByPreference, nsIMsgRetentionSettings::nsMsgRetainAll);
      m_dbFolderInfo->GetUint32Property("daysToKeepHdrs", &daysToKeepHdrs, 0);
      m_dbFolderInfo->GetUint32Property("numHdrsToKeep", &numHeadersToKeep, 0);
      m_dbFolderInfo->GetUint32Property("daysToKeepBodies", &daysToKeepBodies, 0);
      m_dbFolderInfo->GetUint32Property("keepUnreadOnly", &keepUnreadMessagesProp, 0);
      m_dbFolderInfo->GetBooleanProperty("useServerDefaults", &useServerDefaults, PR_TRUE);
      m_dbFolderInfo->GetBooleanProperty("cleanupBodies", &cleanupBodiesByDays, PR_FALSE);
      keepUnreadMessagesOnly = (keepUnreadMessagesProp == 1);
      m_retentionSettings->SetRetainByPreference(retainByPreference);
      m_retentionSettings->SetDaysToKeepHdrs(daysToKeepHdrs);
      m_retentionSettings->SetNumHeadersToKeep(numHeadersToKeep);
      m_retentionSettings->SetKeepUnreadMessagesOnly(keepUnreadMessagesOnly);
      m_retentionSettings->SetDaysToKeepBodies(daysToKeepBodies);
      m_retentionSettings->SetUseServerDefaults(useServerDefaults);
      m_retentionSettings->SetCleanupBodiesByDays(cleanupBodiesByDays);
    }
  }
  *retentionSettings = m_retentionSettings;
  NS_IF_ADDREF(*retentionSettings);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::SetMsgDownloadSettings(nsIMsgDownloadSettings *downloadSettings)
{
  m_downloadSettings = downloadSettings;
  if (downloadSettings && m_dbFolderInfo)
  {
    nsresult rv;

    PRBool useServerDefaults;
    PRBool downloadByDate;
    PRUint32 ageLimitOfMsgsToDownload;
    PRBool downloadUnreadOnly;

    rv = downloadSettings->GetUseServerDefaults(&useServerDefaults);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = downloadSettings->GetDownloadByDate(&downloadByDate);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = downloadSettings->GetDownloadUnreadOnly(&downloadUnreadOnly);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = downloadSettings->GetAgeLimitOfMsgsToDownload(&ageLimitOfMsgsToDownload);
    NS_ENSURE_SUCCESS(rv, rv);
    // need to write this to the db. We'll just use the dbfolderinfo to write properties.
    m_dbFolderInfo->SetBooleanProperty("useServerDefaults", useServerDefaults);
    m_dbFolderInfo->SetBooleanProperty("downloadByDate", downloadByDate);
    m_dbFolderInfo->SetBooleanProperty("downloadUnreadOnly", downloadUnreadOnly);
    m_dbFolderInfo->SetUint32Property("ageLimit", ageLimitOfMsgsToDownload);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetMsgDownloadSettings(nsIMsgDownloadSettings **downloadSettings)
{
  NS_ENSURE_ARG_POINTER(downloadSettings);
  if (!m_downloadSettings)
  {
    // create a new one, and initialize it from the db.
    m_downloadSettings = new nsMsgDownloadSettings;
    if (m_downloadSettings && m_dbFolderInfo)
    {
      PRBool useServerDefaults;
      PRBool downloadByDate;
      PRUint32 ageLimitOfMsgsToDownload;
      PRBool downloadUnreadOnly;

      m_dbFolderInfo->GetBooleanProperty("useServerDefaults", &useServerDefaults, PR_TRUE);
      m_dbFolderInfo->GetBooleanProperty("downloadByDate", &downloadByDate, PR_FALSE);
      m_dbFolderInfo->GetBooleanProperty("downloadUnreadOnly", &downloadUnreadOnly, PR_FALSE);
      m_dbFolderInfo->GetUint32Property("ageLimit", &ageLimitOfMsgsToDownload, 0);

      m_downloadSettings->SetUseServerDefaults(useServerDefaults);
      m_downloadSettings->SetDownloadByDate(downloadByDate);
      m_downloadSettings->SetDownloadUnreadOnly(downloadUnreadOnly);
      m_downloadSettings->SetAgeLimitOfMsgsToDownload(ageLimitOfMsgsToDownload);
    }
  }
  *downloadSettings = m_downloadSettings;
  NS_IF_ADDREF(*downloadSettings);
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::ApplyRetentionSettings(nsIMsgRetentionSettings *msgRetentionSettings)
{
  NS_ENSURE_ARG_POINTER(msgRetentionSettings);
  nsresult rv = NS_OK;

  nsMsgRetainByPreference retainByPreference;
  PRUint32 daysToKeepHdrs = 0;
  PRUint32 numHeadersToKeep = 0;
  PRBool keepUnreadMessagesOnly = PR_FALSE;
  msgRetentionSettings->GetRetainByPreference(&retainByPreference);
  msgRetentionSettings->GetKeepUnreadMessagesOnly(&keepUnreadMessagesOnly);

  switch (retainByPreference)
  {
  case nsIMsgRetentionSettings::nsMsgRetainAll:
    break;
  case nsIMsgRetentionSettings::nsMsgRetainByAge:
    msgRetentionSettings->GetDaysToKeepHdrs(&daysToKeepHdrs);
    return PurgeMessagesOlderThan(daysToKeepHdrs, keepUnreadMessagesOnly);
  case nsIMsgRetentionSettings::nsMsgRetainByNumHeaders:
    msgRetentionSettings->GetNumHeadersToKeep(&numHeadersToKeep);
    return PurgeExcessMessages(numHeadersToKeep, keepUnreadMessagesOnly);
  }
  return rv;
}

nsresult nsMsgDatabase::PurgeMessagesOlderThan(PRUint32 daysToKeepHdrs, PRBool keepUnreadMessagesOnly)
{
  nsresult rv = NS_OK;
  PRInt32 numPurged = 0;
	nsMsgHdr		*pHeader;
  nsCOMPtr <nsISimpleEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
  	return rv;
	PRBool hasMore = PR_FALSE;

	PRTime now = PR_Now();
	PRTime cutOffDay;

	PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInDay;
	
	LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(secondsInDays, 60 * 60 * 24 * daysToKeepHdrs);
	LL_MUL(microSecondsInDay, secondsInDays, microSecondsPerSecond);

	LL_SUB(cutOffDay, now, microSecondsInDay); // = now - term->m_value.u.age * 60 * 60 * 24; 
  // so now cutOffDay is the PRTime cut-off point. Any msg with a date less than that will get purged.
	while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
	{
    PRBool purgeHdr = PR_FALSE;

    rv = hdrs->GetNext((nsISupports**)&pHeader);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
    if (NS_FAILED(rv)) 
      break;

    if (keepUnreadMessagesOnly)
    {
      PRBool isRead;
      IsHeaderRead(pHeader, &isRead);
      if (isRead)
        purgeHdr = PR_TRUE;

    }
    if (!purgeHdr)
    {
      PRTime date;
      pHeader->GetDate(&date);
      if (LL_CMP(date, <, cutOffDay))
        purgeHdr = PR_TRUE;
    }
    if (purgeHdr)
    {
      DeleteHeader(pHeader, nsnull, PR_FALSE, PR_TRUE);
		  numPurged++;
    }
		NS_RELEASE(pHeader);
	}

	if (numPurged > 10)	// commit every once in a while
		Commit(nsMsgDBCommitType::kCompressCommit);
  else if (numPurged > 0)
    Commit(nsMsgDBCommitType::kLargeCommit);
  return rv;
}

nsresult nsMsgDatabase::PurgeExcessMessages(PRUint32 numHeadersToKeep, PRBool keepUnreadMessagesOnly)
{
  nsresult rv = NS_OK;
  PRInt32 numPurged = 0;
	nsMsgHdr		*pHeader;
  nsCOMPtr <nsISimpleEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
  	return rv;
	PRBool hasMore = PR_FALSE;

  mdb_count numHdrs = 0;
  if (m_mdbAllMsgHeadersTable)
    m_mdbAllMsgHeadersTable->GetCount(GetEnv(), &numHdrs);
  else
    return NS_ERROR_NULL_POINTER;

	while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
	{
    PRBool purgeHdr = PR_FALSE;
    rv = hdrs->GetNext((nsISupports**)&pHeader);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
    if (NS_FAILED(rv)) 
      break;

    if (keepUnreadMessagesOnly)
    {
      PRBool isRead;
      IsHeaderRead(pHeader, &isRead);
      if (isRead)
        purgeHdr = PR_TRUE;

    }
    // this isn't quite right - we want to prefer unread messages (keep all of those we can)
    if (numHdrs > numHeadersToKeep)
      purgeHdr = PR_TRUE;

    if (purgeHdr)
    {
      DeleteHeader(pHeader, nsnull, PR_FALSE, PR_TRUE);
      numHdrs--;
		  numPurged++;
    }
		NS_RELEASE(pHeader);
	}

	if (numPurged > 10)	// commit every once in a while
		Commit(nsMsgDBCommitType::kCompressCommit);
  else if (numPurged > 0)
    Commit(nsMsgDBCommitType::kLargeCommit);
  return rv;

  return rv;
}

NS_IMPL_ISUPPORTS1(nsMsgRetentionSettings, nsIMsgRetentionSettings)

nsMsgRetentionSettings::nsMsgRetentionSettings()
{
  NS_INIT_ISUPPORTS();
}

nsMsgRetentionSettings::~nsMsgRetentionSettings()
{
}

/* attribute unsigned long retainByPreference */

NS_IMETHODIMP nsMsgRetentionSettings::GetRetainByPreference(nsMsgRetainByPreference *retainByPreference)
{
  NS_ENSURE_ARG_POINTER(retainByPreference);
  *retainByPreference = m_retainByPreference;
  return NS_OK;
}

NS_IMETHODIMP nsMsgRetentionSettings::SetRetainByPreference(nsMsgRetainByPreference retainByPreference)
{
  m_retainByPreference = retainByPreference;
  return NS_OK;
}

/* attribute long daysToKeepHdrs; */
NS_IMETHODIMP nsMsgRetentionSettings::GetDaysToKeepHdrs(PRUint32 *aDaysToKeepHdrs)
{
  NS_ENSURE_ARG_POINTER(aDaysToKeepHdrs);
  *aDaysToKeepHdrs = m_daysToKeepHdrs;
  return NS_OK;
}

NS_IMETHODIMP nsMsgRetentionSettings::SetDaysToKeepHdrs(PRUint32 aDaysToKeepHdrs)
{
  m_daysToKeepHdrs = aDaysToKeepHdrs;
  return NS_OK;
}

/* attribute long numHeadersToKeep; */
NS_IMETHODIMP nsMsgRetentionSettings::GetNumHeadersToKeep(PRUint32 *aNumHeadersToKeep)
{
  NS_ENSURE_ARG_POINTER(aNumHeadersToKeep);
  *aNumHeadersToKeep = m_numHeadersToKeep;
  return NS_OK;
}
NS_IMETHODIMP nsMsgRetentionSettings::SetNumHeadersToKeep(PRUint32 aNumHeadersToKeep)
{
  m_numHeadersToKeep = aNumHeadersToKeep;
  return NS_OK;
}
/* attribute boolean useServerDefaults; */
NS_IMETHODIMP nsMsgRetentionSettings::GetUseServerDefaults(PRBool *aUseServerDefaults)
{
  NS_ENSURE_ARG_POINTER(aUseServerDefaults);
  *aUseServerDefaults = m_useServerDefaults;
  return NS_OK;
}
NS_IMETHODIMP nsMsgRetentionSettings::SetUseServerDefaults(PRBool aUseServerDefaults)
{
  m_useServerDefaults = aUseServerDefaults;
  return NS_OK;
}

/* attribute boolean keepUnreadMessagesOnly; */
NS_IMETHODIMP nsMsgRetentionSettings::GetKeepUnreadMessagesOnly(PRBool *aKeepUnreadMessagesOnly)
{
  NS_ENSURE_ARG_POINTER(aKeepUnreadMessagesOnly);
  *aKeepUnreadMessagesOnly = m_keepUnreadMessagesOnly;
  return NS_OK;
}
NS_IMETHODIMP nsMsgRetentionSettings::SetKeepUnreadMessagesOnly(PRBool aKeepUnreadMessagesOnly)
{
  m_keepUnreadMessagesOnly = aKeepUnreadMessagesOnly;
  return NS_OK;
}

/* attribute boolean cleanupBodiesByDays; */
NS_IMETHODIMP nsMsgRetentionSettings::GetCleanupBodiesByDays(PRBool *aCleanupBodiesByDays)
{
  NS_ENSURE_ARG_POINTER(aCleanupBodiesByDays);
  *aCleanupBodiesByDays = m_cleanupBodiesByDays;
  return NS_OK;
}
NS_IMETHODIMP nsMsgRetentionSettings::SetCleanupBodiesByDays(PRBool aCleanupBodiesByDays)
{
  m_cleanupBodiesByDays = aCleanupBodiesByDays;
  return NS_OK;
}


/* attribute long daysToKeepBodies; */
NS_IMETHODIMP nsMsgRetentionSettings::GetDaysToKeepBodies(PRUint32 *aDaysToKeepBodies)
{
  NS_ENSURE_ARG_POINTER(aDaysToKeepBodies);
  *aDaysToKeepBodies = m_daysToKeepBodies;
  return NS_OK;
}
NS_IMETHODIMP nsMsgRetentionSettings::SetDaysToKeepBodies(PRUint32 aDaysToKeepBodies)
{
  m_daysToKeepBodies = aDaysToKeepBodies;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMsgDownloadSettings, nsIMsgDownloadSettings)

nsMsgDownloadSettings::nsMsgDownloadSettings()
{
  NS_INIT_ISUPPORTS();
  m_useServerDefaults = PR_FALSE;
	m_downloadUnreadOnly = PR_FALSE;
	m_downloadByDate = PR_FALSE;
	m_ageLimitOfMsgsToDownload = 0;
}

nsMsgDownloadSettings::~nsMsgDownloadSettings()
{
}

/* attribute boolean useServerDefaults; */
NS_IMETHODIMP nsMsgDownloadSettings::GetUseServerDefaults(PRBool *aUseServerDefaults)
{
  NS_ENSURE_ARG_POINTER(aUseServerDefaults);
  *aUseServerDefaults = m_useServerDefaults;
  return NS_OK;
}
NS_IMETHODIMP nsMsgDownloadSettings::SetUseServerDefaults(PRBool aUseServerDefaults)
{
  m_useServerDefaults = aUseServerDefaults;
  return NS_OK;
}


/* attribute boolean keepUnreadMessagesOnly; */
NS_IMETHODIMP nsMsgDownloadSettings::GetDownloadUnreadOnly(PRBool *aDownloadUnreadOnly)
{
  NS_ENSURE_ARG_POINTER(aDownloadUnreadOnly);
  *aDownloadUnreadOnly = m_downloadUnreadOnly;
  return NS_OK;
}
NS_IMETHODIMP nsMsgDownloadSettings::SetDownloadUnreadOnly(PRBool aDownloadUnreadOnly)
{
  m_downloadUnreadOnly = aDownloadUnreadOnly;
  return NS_OK;
}

/* attribute boolean keepUnreadMessagesOnly; */
NS_IMETHODIMP nsMsgDownloadSettings::GetDownloadByDate(PRBool *aDownloadByDate)
{
  NS_ENSURE_ARG_POINTER(aDownloadByDate);
  *aDownloadByDate = m_downloadByDate;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDownloadSettings::SetDownloadByDate(PRBool aDownloadByDate)
{
  m_downloadByDate = aDownloadByDate;
  return NS_OK;
}


/* attribute long ageLimitOfMsgsToDownload; */
NS_IMETHODIMP nsMsgDownloadSettings::GetAgeLimitOfMsgsToDownload(PRUint32 *ageLimitOfMsgsToDownload)
{
  NS_ENSURE_ARG_POINTER(ageLimitOfMsgsToDownload);
  *ageLimitOfMsgsToDownload = m_ageLimitOfMsgsToDownload;
  return NS_OK;
}
NS_IMETHODIMP nsMsgDownloadSettings::SetAgeLimitOfMsgsToDownload(PRUint32 ageLimitOfMsgsToDownload)
{
  m_ageLimitOfMsgsToDownload = ageLimitOfMsgsToDownload;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetDefaultViewFlags(nsMsgViewFlagsTypeValue *aDefaultViewFlags)
{ 
  NS_ENSURE_ARG_POINTER(aDefaultViewFlags);
  *aDefaultViewFlags = nsMsgViewFlagsType::kNone;
  return NS_OK;
}

NS_IMETHODIMP nsMsgDatabase::GetDefaultSortType(nsMsgViewSortTypeValue *aDefaultSortType)
{
  NS_ENSURE_ARG_POINTER(aDefaultSortType);
  *aDefaultSortType = nsMsgViewSortType::byDate;
  return NS_OK;
}
